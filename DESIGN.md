# Ztoryc — Functional Specification & Architecture

*Version 1.0 — March 2026*

---

## 1. Overview

Ztoryc is an open-source pre-production tool for 2D animation, built as a fork of Tahoma2D 1.6.0. It integrates a complete storyboard and animatic pipeline directly into the application, eliminating the need to switch between tools during pre-production.

There is currently no open-source storyboard tool available. Ztoryc aims to fill that gap. While built on Tahoma2D, its storyboard and export workflows are designed to be useful regardless of the downstream animation software.

### 1.1 Core Concept

Ztoryc works on a standard Tahoma2D scene file (`.tnz`). The main xsheet represents the entire film/episode:

- Each **column** in the main xsheet = one **shot**
- Each shot is a **sub-scene** (subxsheet) containing the animation
- A companion `.ztoryc` file (XML) stores all Ztoryc metadata alongside the `.tnz` file
- Three dedicated **Rooms** replace the default Tahoma2D workspace for pre-production work

### 1.2 File Structure

```
myfilm.tnz          — main Tahoma2D scene (one column per shot)
myfilm.ztoryc       — Ztoryc metadata (shot numbers, dialog, action, notes, panel data)
myfilm/             — folder with sub-scene files, one per shot
```

Exported shots: standalone `.tnz` files with isolated assets.

---

## 2. Workflow Overview

The Ztoryc workflow follows the standard pre-production pipeline:

| Room | Purpose |
|------|---------|
| **BOARD** | Draw storyboard panels, write dialog/action/notes, set shot order |
| **ANIMATIC** | Set shot timing, edit audio, preview synchronization |
| **SHOTEDITOR** | Refine poses, draw layouts, animate |

The user switches between rooms using the Workflow menu. Each room shows a different set of panels optimized for that phase of work.

---

## 3. Room: BOARD (Storyboard)

The BOARD room is the main storyboard interface. It shows all shots in a scrollable grid.

### 3.1 StoryboardPanel — Shot Grid

The central panel of the BOARD room. Displays all shots as a grid of cards, each containing one or more panels.

#### 3.1.1 Shot Structure

- Each shot = one column in the main xsheet = one sub-scene
- Each shot contains one or more **panels** (`PanelData`)
- A panel represents a key composition within a shot (keyframe, camera move, drawing change)
- Panel 0 = first/main panel of the shot

#### 3.1.2 Panel Auto-Detection

Panels are detected automatically when the user is inside the sub-scene:

- From animation keyframes
- From camera keyframes
- From drawing/composition changes
- Detection triggers on **frame change** (debounce 1000ms) — **NOT** on `xsheetChanged` to avoid lag during drawing
- On return to main xsheet: full refresh of all panels

Manual panel management:
- Add panel: right-click inside the sub-scene viewer → "Add this frame as story panel"
- Remove panel: right-click on panel in the board → "Remove panel"

#### 3.1.3 Shot Card Layout

Each shot card in the grid shows:

- Shot number (editable inline, top left)
- One or more panel thumbnails with partial duration (`D:` field — read-only, derived from keyframes)
- Total shot duration (`T:` field — editable only on the first panel of the shot, read-only on others)
- Dialog text field
- Action notes field
- Edit button — enters the sub-scene in Shot Editor
- Delete button — removes shot from Ztoryc AND deletes the column from the xsheet

#### 3.1.4 Duration Fields

Two separate duration fields per panel:

- **D: (partial duration)** — read-only. Duration of this specific panel, derived dynamically from keyframes inside the sub-scene. Updates automatically when the sub-scene is edited.
- **T: (total duration)** — editable ONLY on panel 0 (first panel of the shot). Read-only on all other panels. Modifying T: calls `resequenceXsheet()` to reposition all subsequent shots in the main timeline.

#### 3.1.5 Shot Numbering

Three modes, selectable from the toolbar:

- **Auto**: shots are renumbered sequentially (01, 02, 03...) on every change
- **Keep**: existing numbers are preserved; new shots inserted with letter suffix (2A, 2B...)
- **Renumber All**: bulk renumber with popup options (range, step)

In Auto mode: a warning popup alerts the user that all numbers will be reassigned.  
In Keep mode: shot numbers are editable inline via `QLineEdit`.

#### 3.1.6 Shot Operations

- Drag & drop to reorder shots (updates main xsheet column order)
- Multiple selection: Shift+click for range, Cmd+click for individual
- **Copy**: creates a linked instance (shared sub-scene — editing one edits the other)
- **Clone**: creates an independent copy of the sub-scene
- **Paste**: inserts copied/cloned shot(s) at selected position
- **Delete**: removes shot(s) from Ztoryc and column(s) from xsheet
- **Merge**: merges two adjacent shots into one

#### 3.1.7 Dialog / Action / Notes

Text fields per panel, written directly in the board. Future feature: import from screenplay (Final Draft or compatible format — format TBD).

#### 3.1.8 Grid Configuration

- Configurable number of columns (N columns per row)
- Thumbnail size adjustable

#### 3.1.9 Export

- **Export Shots**: exports each shot as a standalone `.tnz` scene with isolated assets. Popup for range selection (All / From-To).
- **Export PDF**: storyboard PDF with thumbnails, dialog, action notes (planned).

---

## 4. Room: ANIMATIC

The ANIMATIC room replaces the native Tahoma2D timeline **for the main xsheet**. It provides an NLE-style (Non-Linear Editor) interface for timing and audio.

> **Important**: the native timeline is NOT used in this room. It is reserved for sub-scenes (shots) when in SHOTEDITOR mode.

### 4.1 Layout

- **Top**: `ZtoryAnimaticViewer` — integrated OpenGL viewer always showing the main xsheet
- **Bottom**: `ZtoryAnimaticPanel` — horizontal timeline with ruler, shot track, and audio tracks

### 4.2 ZtoryAnimaticViewer

- Inherits from `BaseViewerPanel`
- `m_alwaysMainXsheet = true` — always shows the main xsheet, independent of the global Edit In Place toggle
- Disconnected from `activeViewerChanged` — does not become the global active viewer
- Synchronized with the animatic playhead

### 4.3 ZtoryAnimaticTrack — Shot Track

Horizontal track showing one colored block per shot:

- Block width = shot duration in frames × pixels-per-frame (`m_ppf`)
- Block color = shot color (from xsheet column color)
- Shot number displayed inside the block
- Zoom: mouse wheel, range 2–64 ppf, factor 1.15 per step
- Playhead: synchronized with `TFrameHandle`, updated on `frameSwitched` when at main xsheet

#### 4.3.1 Ripple Edit

Drag the right edge of a shot block to resize it:

- Default: **Ripple** — all subsequent shots shift right/left to maintain sequence
- Alt key: only the dragged shot changes, others stay in place
- On mouse release: calls `resequenceXsheet()` to update the main xsheet

#### 4.3.2 Shot Operations from Animatic

- Copy / Clone / Paste: same behavior as in BOARD room
- Merge: merge two adjacent shots into one
- Delete: removes shot and column
- Right-click menu: **Match Subscene Duration** — reads last non-empty frame of sub-scene and sets shot duration accordingly

#### 4.3.3 Camera

**Camera editing is NOT available from the ANIMATIC room.** Camera must be edited from inside the sub-scene in SHOTEDITOR.

### 4.4 Audio Tracks

Audio tracks are displayed below the shot track, one track per `TXshSoundColumn` found in the main xsheet:

- Waveform visualization using `soundLevel->getValueAtPixel()` — same logic as native xsheet
- Track height ~40px, background dark gray, waveform green
- Column name displayed as label on the left
- Audio tracks have the **same editing capabilities as in the native timeline**: move, cut, copy, paste segments
- **Lock toggle**: when locked, audio track moves with shot timing changes; when unlocked, stays in place
- Auto-update when xsheet changes

> **Audio is managed ONLY in the main xsheet (ANIMATIC room). Sub-scenes do not contain audio.** However, when editing a shot in SHOTEDITOR, the animatic timeline is visible alongside the shot timeline so the animator can verify audio/video sync.

### 4.5 ZtoryAnimaticRuler

- Horizontal time ruler at the top of the timeline
- Synchronized with zoom level (`m_ppf`)
- Adaptive density: shows more tick marks at higher zoom levels (every 1, 5, 10, 24 frames based on ppf) — planned

---

## 5. Room: SHOTEDITOR

The SHOTEDITOR room is where the animator works on individual shots. It combines storyboard context, drawing tools, and both timelines.

### 5.1 Layout

- **Top strip**: StoryStrip — horizontal filmstrip of all shot thumbnails
- **Center**: ComboViewer — main drawing/animation viewer with tool panels (palette, style editor, etc.)
- **Bottom left**: ZtoryAnimaticPanel — animatic timeline (read-only audio, shot context)
- **Bottom right**: ZtoryAnimaticViewer — small animatic viewer
- **Bottom**: native Tahoma2D timeline for the current sub-scene

### 5.2 StoryStrip

Horizontal filmstrip showing thumbnails of all shots:

- Click on a thumbnail → enters that shot for editing
- Current shot highlighted
- Shot number and duration shown under each thumbnail
- Provides visual context (surrounding shots) while animating

### 5.3 Dual Timeline

The SHOTEDITOR shows two timelines simultaneously:

- **Animatic timeline (top)**: shows all shots and audio tracks. Read-only for audio. Helps animator see where the current shot falls in relation to audio.
- **Shot timeline (bottom)**: native Tahoma2D timeline for the current sub-scene. Full editing. Camera editing available here. No audio.

The two timelines are **independent** (separate `TFrameHandle` — planned). This allows playing the animatic while working on a specific shot.

### 5.4 Camera

Camera is edited from the **shot timeline (sub-scene)**. Not available in the animatic timeline. This is intentional: camera moves belong to individual shots, not to the global animatic.

---

## 6. Data Model

### 6.1 ZtoryModel (Singleton)

Central data manager shared across all Ztoryc panels:

- `std::vector<ShotData>` — ordered list of all shots
- Thread-safe access
- Preview cache for thumbnails
- CRUD operations: add, remove, move, clone shots
- Numbering logic: Auto / Keep modes
- Panel auto-detection from keyframes
- Persistence: read/write `.ztoryc` XML v2

Qt signals emitted on data changes:

| Signal | Description |
|--------|-------------|
| `modelReset` | full reload |
| `shotAdded(int index)` | shot inserted |
| `shotRemoved(int index)` | shot deleted |
| `shotMoved(int from, int to)` | shot reordered |
| `shotDataChanged(int index)` | shot data modified |
| `previewUpdated(int index)` | thumbnail refreshed |

### 6.2 ShotData

- `shotNumber` (QString) — display number (e.g. '01', '2A')
- `columnIndex` (int) — index in main xsheet
- `panels` (QVector\<PanelData\>) — list of panels

### 6.3 PanelData

- `startFrame` (int) — frame inside sub-scene
- `duration` (int) — partial duration (read-only, from keyframes)
- `dialog` (QString)
- `action` (QString)
- `notes` (QString)
- `thumbnail` (QPixmap — cached)

### 6.4 .ztoryc File Format (XML v2)

```xml
<ztoryc version="2">
  <shot index="0" number="01" column="0">
    <panel index="0" startFrame="0" duration="8">
      <dialog>Character enters.</dialog>
      <action>Wide shot, camera pans right.</action>
      <notes>Reference: scene_02_ref.jpg</notes>
    </panel>
  </shot>
</ztoryc>
```

---

## 7. Xsheet Synchronization

Ztoryc keeps the main xsheet synchronized with its internal data:

- **Column name**: updated automatically by `updateColumnName()` when `shotNumber` changes
- **Delete shot**: uses `ColumnCmd::deleteColumn(col)` to remove the column from xsheet
- **Clone shot**: uses `cloneChildToPosition()` (static function in `storyboardpanel.cpp`)
- **Resequence**: `resequenceXsheet()` repositions all shot columns in the main xsheet after any duration/order change
- **In/Out markers**: saved and restored per sub-scene via `s_frameRangeMap` (`subscenecommand.cpp`)
- **Export shot**: uses `ztoryOpenSubXsheet()` + `IoCmd::saveScene(SAVE_SUBXSHEET)` without `SILENTLY_OVERWRITE`

---

## 8. Key Architectural Decisions

| Decision | Rationale |
|----------|-----------|
| **Shot duration = cells in main xsheet (including empty/red cells)** — from `getRange()`. NOT from In/Out markers | Reflects actual timing in the animatic |
| **In/Out markers** — represent the play range inside the sub-scene. Saved/restored per sub-scene. Not used for animatic duration | Kept separate to avoid confusion |
| **Audio** — managed only in the main xsheet (animatic). Not duplicated in sub-scenes | Clean separation of concerns |
| **Copy vs Clone** — Copy = shared instance (edit one = edit both). Clone = fully independent sub-scene | Mirrors standard NLE behavior |
| **Camera** — edited inside sub-scene (SHOTEDITOR). Not accessible from ANIMATIC room | Camera moves are shot-specific, not global |
| **Thumbnail refresh** — triggered on `frameSwitched` with 1000ms debounce. Not on `xsheetChanged` (too frequent during drawing) | Avoids lag and recursion during drawing |
| **Frame handle** — animatic and shot editor will have separate `TFrameHandle` instances to allow independent playback (planned) | Required for dual timeline in SHOTEDITOR |
| **Rolling Edit** — removed from roadmap | Not needed in 2D animation workflow |

---

## 9. Current Code Architecture

```
StoryboardPanel (TPanel)
├── ShotData (from ZtoryModel)
│   └── PanelData
└── PanelWidget (QFrame) — UI for single panel

ZtoryModel (singleton)
└── std::vector<ShotData>

ZtoryAnimaticPanel (TPanel)
└── QSplitter (horizontal)
    ├── ZtoryAnimaticViewer (BaseViewerPanel)
    │   └── SceneViewer (m_alwaysMainXsheet=true)
    └── QScrollArea
        ├── ZtoryAnimaticRuler
        └── ZtoryAnimaticTrack
```

### 9.1 Target Architecture

```
ZtoryModel (singleton — master data)
└── std::vector<ShotData>

ZtoryBasePanel (TPanel)
├── StoryboardPanel
├── StoryStrip
└── OrderReview
```

---

## 10. Roadmap

### In Progress

- Connect StoryboardPanel → ZtoryModel as single master
- Shot duration from sub-scene In/Out markers (read-only in panel)
- Audio track visualization in animatic
- Restore ZtoryAnimaticViewer in panel

### Milestone 1 — Core Structure

- [ ] Keyboard shortcuts Cmd+C/D/V (CommandManager)
- [ ] Undo/Redo (TUndoManager)
- [ ] StoryStrip Panel (horizontal filmstrip)
- [ ] Order Review Panel (compact reordering grid)
- [ ] Advanced renumber popup

### Milestone 2 — Animatic

- [ ] Separate TFrameHandle for animatic viewer
- [ ] Adaptive zoom ruler
- [ ] Audio track editing (move, cut, copy, paste, lock)
- [ ] Animatic export connected to render pipeline

### Milestone 3 — Shot Editor

- [ ] StoryStrip integration in SHOTEDITOR room
- [ ] Quick-shot selector in toolbar
- [ ] PDF export with real thumbnails

### Milestone 4 — Polish

- [ ] Keep→Auto mode warning popup
- [ ] Save/restore Ztoryc room layouts
- [ ] Improved Edit/Board toggle

### Milestone 5 — Kitsu / AI Integration

- [ ] Asset tagging per panel (character/bkg/prop)
- [ ] Kitsu integration: push shots as tasks with thumbnails and metadata
- [ ] Export to AI: dialog/action/notes as prompts, panels as visual references
- [ ] Pipeline: Ztoryc → Kitsu → AI → Tahoma2D

---

## 11. Contributing & Build

- **Repository**: https://github.com/matitanimata/ztoryc
- **Base**: Tahoma2D 1.6.0 (BSD 2-Clause License)
- **Workspace**: `/Volumes/ZioSam/tahoma2d-workspace/tahoma2d`
- **Build**: `touch [file] && ninja -j4 -C [workspace]/toonz/build 2>&1 | grep 'error:' | head -10`
- **Before PR**: run `toonz/sources/beautification.sh` (clang-format)
- **Commit format**: Conventional Commits (`feat:`, `fix:`, `docs:`, `refactor:`...)

### Main Files

| File | Description |
|------|-------------|
| `toonz/sources/toonz/storyboardpanel.h/.cpp` | BOARD room — shot grid UI |
| `toonz/sources/toonz/ztorymodel.h/.cpp` | Data model singleton |
| `toonz/sources/toonz/ztoryanimatic.h/.cpp` | ANIMATIC room — timeline + viewer |
| `toonz/sources/toonz/ztorybackpanel.h/.cpp` | Back panel (to be removed) |
