# Ztoryc — Animatic Panel: Task List for Claude Code

> Generated from code review + design document (March 2026).
> Each task has: context, what to do, which files to touch, pitfalls.

---

## LEGEND

- **BUG** = existing code that is broken
- **NEW** = feature that does not exist yet
- **MOD** = existing code that needs modification

---

## 1. BUG — Animatic Viewer not visible / not independent

**Problem:** `ZtoryAnimaticViewer` is created inside `ZtoryAnimaticPanel` (line 363 of
`ztoryanimatic.cpp`) and placed in a splitter (viewer above, timeline below). However
the user reports it is not showing or not working. The viewer inherits
`BaseViewerPanel` with `m_sceneViewer->setAlwaysMainXsheet(true)` and disconnects
`activeViewerChanged`. Despite this, the viewer still shares `TFrameHandle` with the
normal viewer (documented as known bug in AGENTS.md).

**Root cause to investigate:**

1. The viewer may need an explicit `show()` call or a minimum size to appear.
2. `BaseViewerPanel` relies on being registered as the "active viewer" to receive
   paint updates. Since we disconnect `activeViewerChanged`, the viewer may never
   get repainted. Check if `onSceneChanged` / `updateFrameRange` are triggered.
3. The `TFrameHandle` sharing means that when the user navigates the animatic
   ruler, it moves the frame in whatever sub-scene is open in the normal viewer
   — this is confusing and must be fixed with a separate `TFrameHandle` instance.

**What to do:**

- Ensure the `ZtoryAnimaticViewer` widget is visible: check splitter sizes,
  add `m_animViewer->show()` in `showEvent`, verify minimum height.
- Add a dedicated `TFrameHandle *m_animaticFrameHandle` in `ZtoryAnimaticPanel`
  and wire it to `ZtoryAnimaticViewer` instead of `TApp::instance()->getCurrentFrame()`.
  This requires subclassing or patching `BaseViewerPanel` to accept an external
  frame handle (add a `setFrameHandle(TFrameHandle*)` method to
  `BaseViewerPanel` or `SceneViewer`).
- When the ruler/track changes frame, update only `m_animaticFrameHandle`.
- When TApp's frame changes from external sources, sync back to the animatic
  ruler/track only if we're at the main xsheet.

**Files:** `ztoryanimatic.h`, `ztoryanimatic.cpp`, possibly `viewerpane.h/cpp`,
`sceneviewer.h/cpp`.

**Pitfall:** Do NOT modify `TFrameHandle` globally. Create a new instance local
to the animatic panel.

---

## 2. BUG — Timing desync between Board and Animatic timeline

**Problem:** When the user modifies shot duration by dragging the border in the
animatic timeline (`onShotDurationChanged`), the Board panel does not always
reflect the updated duration. The Board's `onXsheetChanged` handler (line 1051
of `storyboardpanel.cpp`) updates `panels[0].duration` from `column->getRange()`,
but only if `getAncestorCount() == 0` (at main xsheet level). If the user is
inside a sub-scene while the animatic timeline changes the main xsheet, the
board won't update.

Also, `resequenceXsheet()` exists in BOTH `storyboardpanel.cpp` and
`ztoryanimatic.cpp` — two separate implementations that can diverge. The board
version uses `m_shots[i].data.totalDuration()` (from ZtoryModel data) while
the animatic version scans actual xsheet columns. This is a recipe for conflicts.

**What to do:**

- **Unify `resequenceXsheet()`** into `ZtoryModel` as a single source of truth.
  Both panels should call `ZtoryModel::instance()->resequenceXsheet()`.
- After any duration change in the animatic, emit a signal that the board
  can listen to. Use `ZtoryModel::shotDataChanged(int shotIdx)` for this.
- Make sure both panels connect to `ZtoryModel::modelReset` and
  `ZtoryModel::shotDataChanged` to refresh their UI.

**Files:** `ztorymodel.h/cpp`, `ztoryanimatic.cpp`, `storyboardpanel.cpp`.

**Pitfall:** Avoid infinite signal loops (panel A changes data -> model emits
signal -> panel B updates -> panel B notifies model -> model emits again).
Use a guard flag or `blockSignals()`.

---

## 3. BUG — Copy does not work in Board (Clone works)

**Problem:** The user reports "Clone funziona ma Copy no." Looking at the code:

- `onCopyShot()` (line 1131) copies shot data to `m_clipboard` with `isClone=false`.
- `onCloneShot()` (line 1148) copies shot data to `m_clipboard` with `isClone=true`.
- `onPasteShot()` (line 1221) always calls `cloneChildToPosition()` regardless
  of `isClone` flag — the `isClone` flag is never checked!

For a **copy** (shared instance), the paste should reuse the same `TXshChildLevel`
(point to the same sub-scene), not create an independent clone. The current code
always creates an independent clone via `StageObjectsData::eDoClone`.

**What to do:**

- In `onPasteShot()`, check `m_clipboard[ci].isClone`:
  - If `true` (clone): call `cloneChildToPosition(srcCol, pos)` as now.
  - If `false` (copy/shared): insert a new column at `pos`, then copy the
    original column's cells (same `TXshChildLevel` pointer) into it. Something
    like:
    ```cpp
    xsh->insertColumn(pos);
    TXshColumn *srcColumn = xsh->getColumn(srcCol);
    int r0, r1;
    srcColumn->getRange(r0, r1);
    for (int r = r0; r <= r1; r++) {
        TXshCell cell = xsh->getCell(r, srcCol);
        if (!cell.isEmpty()) xsh->setCell(r, pos, cell);
    }
    ```
    This makes the new column point to the SAME sub-scene. Edits in one will
    appear in all copies (shared child level).

**Files:** `storyboardpanel.cpp` (around line 1221-1260).

**Pitfall:** After inserting a column, all column indices shift. The existing
correction loop (lines 1230-1234) handles this for `srcCol`, but verify it
works for the copy case too.

---

## 4. BUG — Duplicate button declarations in StoryboardPanel constructor

**Problem:** In `StoryboardPanel::StoryboardPanel()` (lines 417-445), the buttons
`m_copyButton`, `m_cloneButton`, and `m_pasteButton` are each created TWICE:

```cpp
m_copyButton = new QPushButton("Copy");       // line 417
...
m_copyButton = new QPushButton("Copy");       // line 432 — overwrites!
```

This means the first three button instances leak (never deleted) and the
toolbar adds both the first and second instances (`tb->addWidget` at lines
473-479 adds them twice).

**What to do:**

- Remove the duplicate declarations (lines 432-445).
- Remove the duplicate `tb->addWidget` calls (lines 477-479).

**Files:** `storyboardpanel.cpp` (constructor, around lines 417-479).

---

## 5. NEW — Story-strip (horizontal thumbnail strip)

**What:** A thin horizontal strip showing all shots as small thumbnails (no text
fields). Two arrow buttons on the sides for scrolling, plus a scroll bar.
Clicking a thumbnail navigates to that shot. Should be placed above the
animatic timeline (between viewer and ruler).

**Design (from user document):**

- Based on the board panel concept, but much smaller thumbnails
- No dialog/action/notes fields
- Shot number overlaid on each thumbnail
- Scroll arrows on left/right edges
- Current shot highlighted

**Where to put it:**

- Create a new class `ZtoryStoryStrip : public QWidget` in `ztoryanimatic.h/cpp`
  (or in its own file if preferred).
- Add it to `ZtoryAnimaticPanel` layout, between `m_animViewer` and the scroll
  area containing ruler+track.

**Implementation notes:**

- Reuse thumbnail generation from `StoryboardPanel::updatePreview()` or
  `ZtoryModel::preview()`.
- Each thumbnail: fixed height ~60px, width proportional to aspect ratio.
- Use a `QScrollArea` with horizontal scroll.
- Connect to `ZtoryModel::shotDataChanged` and `modelReset` for updates.

**Files:** `ztoryanimatic.h/cpp` (new class + integration in panel layout).

---

## 6. NEW — Animatic Timeline Toolbar

**What:** A toolbar above the timeline ruler with these tools:

### 6a. Zoom slider
- A `QSlider` (horizontal) that controls `m_ppf` (pixels per frame).
- Range: 2.0 to 64.0. Default: 8.0.
- Syncs with wheel zoom that already exists.

### 6b. Select tool
- Default tool. Single click selects a shot; double-click enters edit mode
  (calls `MI_OpenChild` on that column — same as board's `onEditShot`).
- Shift+click: add to selection. Ctrl/Cmd+click: toggle selection.
- Currently: single click selects (`shotClicked` signal), but:
  - No double-click handling
  - No multi-selection
  - No visual indication of selection in the track

### 6c. Razor tool
- Click on a shot at position X to split it into two shots at that frame.
- Implementation: given shot at column `col` with range `[r0, r1]` and click
  at frame `splitFrame`:
  1. Create a new column at `col+1`
  2. Move cells from `splitFrame` to `r1` into the new column
  3. The new column gets a new sub-scene containing only the frames after split
  4. Or simpler: just split the cell range and create a new child level with
     `cloneChildToPosition` trimmed to the second half
- Also supports splitting an audio track at the click position (razor on audio).

### 6d. Link/Unlink tool
- Toggles link between audio and video tracks.
- When linked: moving a shot also moves the corresponding audio region.
- When unlinked: audio stays fixed, video can be repositioned independently.
- Implementation: add a `bool m_audioLinked` flag per shot or as a global
  toggle. When dragging a shot in linked mode, also shift the audio columns.

### 6e. Add / Delete / Copy / Clone / Paste buttons
- Same operations as the Board toolbar, but operating on the timeline selection.
- Can reuse the same logic from `StoryboardPanel` or (better) move the logic
  into `ZtoryModel` and call it from both panels.

### 6f. Merge button
- Merges two or more selected shots into one.
- Use case: two shots with the same framing separated by an insert shot; when
  the insert is deleted, the two matching shots should be reunited.
- Implementation: take all selected shots, combine their sub-scene content into
  the first shot's sub-scene (append frames), delete the other shots' columns.

**Files:** `ztoryanimatic.h/cpp`, potentially `ztorymodel.h/cpp` for shared logic.

---

## 7. NEW — Double-click shot to enter edit mode

**What:** In the animatic track, double-clicking a shot block should enter
that shot's sub-scene (same as the Board's "Edit" button behavior).

**What to do:**

- Add `mouseDoubleClickEvent` to `ZtoryAnimaticTrack`.
- Detect which shot block was clicked.
- Emit a new signal `shotDoubleClicked(int col)`.
- In `ZtoryAnimaticPanel`, connect to this signal and call the same logic as
  `StoryboardPanel::onEditShot()`:
  ```cpp
  void ZtoryAnimaticPanel::onShotDoubleClicked(int col) {
      TApp *app = TApp::instance();
      // First close any open sub-scene
      ToonzScene *scene = app->getCurrentScene()->getScene();
      while (scene->getChildStack()->getAncestorCount() > 0)
          CommandManager::instance()->execute("MI_CloseChild");
      // Then open the target sub-scene
      app->getCurrentColumn()->setColumnIndex(col);
      TXshCell cell = app->getCurrentXsheet()->getXsheet()->getCell(0, col);
      if (cell.m_level && cell.m_level->getChildLevel()) {
          app->getCurrentFrame()->setFrame(0);
          CommandManager::instance()->execute("MI_OpenChild");
      }
  }
  ```

**Files:** `ztoryanimatic.h/cpp`.

---

## 8. NEW — Multi-selection in Animatic Track

**What:** Allow selecting multiple shots with Shift+click (range) and
Ctrl/Cmd+click (toggle), same as the board.

**What to do:**

- Add `std::set<int> m_selectedCols` to `ZtoryAnimaticTrack`.
- Modify `mousePressEvent` to handle modifiers.
- In `paintEvent`, draw selected shots with a highlight border (orange, same
  as the board's selection style).
- Emit `selectionChanged(std::set<int> selectedCols)` signal.

**Files:** `ztoryanimatic.h/cpp`.

---

## 9. NEW — Audio export with shot (reserved column)

**What (from user document — N.B. section):** When exporting shots as individual
`.tnz` scenes, the portion of audio from the main xsheet that corresponds to
each shot's frame range must be automatically inserted into the sub-scene.
There should be a reserved, non-editable column in the sub-scene to hold this
audio.

**Implementation:**

- In `StoryboardPanel::onExportShots()`, before calling
  `IoCmd::saveScene(outPath, IoCmd::SAVE_SUBXSHEET)`:
  1. Find all audio columns in the main xsheet.
  2. For each audio column, extract the audio data in the frame range
     `[shotStartFrame, shotStartFrame + shotDuration - 1]`.
  3. Insert this audio into the sub-scene's xsheet as a new sound column.
  4. Mark this column as "reserved" (read-only). Tahoma2D doesn't have a
     built-in column lock, so this might need a metadata flag or a special
     column name prefix like `_audio_main_`.
  5. After saving, remove the temporary audio column from the sub-scene.

**Files:** `storyboardpanel.cpp` (`onExportShots`), possibly new utility function.

**Pitfall:** The audio must be trimmed to the exact shot range. Use
`TXshSoundColumn::getSoundCell()` to read cells in the range, and insert
them at frame 0 in the sub-scene.

---

## 10. MOD — X-Sheet Panel in storyboard workflow

**What (from user document):** The native X-Sheet/Timeline panel, when used in
the storyboard workflow, should:

- Show only the sub-scene content (current shot being edited)
- NOT display the main xsheet level
- NOT import audio files (audio only in main xsheet)
- Double-click on the thumbnail in the storyboard enters edit mode

**This is largely about room configuration** rather than code changes:

- The BOARD and SHOTEDITOR rooms should configure the timeline panel to show
  only the current xsheet (which is already the default behavior when inside
  a sub-scene via `MI_OpenChild`).
- To prevent audio import in sub-scenes, add a check in the audio import
  command: if `getAncestorCount() > 0`, show a warning and refuse.

**Files:** Room layout configuration, possibly `iocommand.cpp` (audio import
guard).

---

## 11. MOD — Animatic Viewer toggle with ComboViewer

**What (from user document):** The Animatic Viewer could share the same space
as the ComboViewer via a toggle button, instead of always being a separate
widget. This saves screen space.

**Implementation option:**

- Add a toggle button (e.g., "Animatic View / Shot View") in the viewer area.
- Use a `QStackedWidget` containing both `m_animViewer` and the normal
  `SceneViewerPanel`.
- Toggle switches between index 0 (animatic = main xsheet playback) and
  index 1 (shot editor = sub-scene).

**Alternative:** Keep them separate but allow the user to hide/show the
animatic viewer via the panel's context menu or a collapse button.

**Files:** `ztoryanimatic.h/cpp` (panel layout).

---

## Priority Order (suggested)

1. **Task 4** — Fix duplicate buttons (trivial, 5 min)
2. **Task 1** — Fix animatic viewer visibility + TFrameHandle (critical)
3. **Task 3** — Fix Copy vs Clone in Board (important UX)
4. **Task 2** — Unify resequenceXsheet into ZtoryModel (prevents future bugs)
5. **Task 7** — Double-click to enter edit mode (quick win)
6. **Task 8** — Multi-selection in track (prerequisite for merge/razor)
7. **Task 6a** — Zoom slider (quick)
8. **Task 6c** — Razor tool
9. **Task 6d** — Link/Unlink audio-video
10. **Task 6f** — Merge shots
11. **Task 5** — Story-strip
12. **Task 9** — Audio export with shot
13. **Task 10** — X-Sheet panel guard for audio import
14. **Task 11** — Viewer toggle

---

## Reference: Current File Structure

```
ztoryanimatic.h     — 148 lines (classes: Ruler, Track, AudioTrack, Viewer, Panel)
ztoryanimatic.cpp   — 653 lines
storyboardpanel.h   — 175 lines (classes: PanelWidget, StoryboardPanel)
storyboardpanel.cpp — 1628 lines
ztorymodel.h        — 89 lines
ztorymodel.cpp      — 326 lines
ztorybackpanel.h    — 13 lines
AGENTS.md           — 218 lines (Claude Code rules)
DESIGN.md           — functional spec
```
