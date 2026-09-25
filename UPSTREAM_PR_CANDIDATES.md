# Ztoryc → Tahoma2D / OpenToonz — Upstream Contribution Candidates

> **One file, two halves.** Part 1 is the orientation: what this list is, how sure
> we are of each item, and where it makes sense to start. Part 2 is the working
> list itself. If you are reading this to help, Part 1 is enough to decide what
> to pick up.

---

## Part 1 — Read this first

### What Ztoryc is

Ztoryc is a fork of **Tahoma2D 1.6.x** that adds an integrated storyboard and
animatic pipeline for 2D pre-production. Most of the work in the fork falls into
two very different piles, and the difference matters for anything upstream:

- work that happens to have been done *in* Ztoryc but is about **Tahoma2D's own
  code** — crashes, memory, the Plastic (mesh-deformation) tool, the timeline,
  the brush. None of it needs anything Ztoryc-specific to be useful;
- work that only exists **because** of the storyboard pipeline, and which needs
  the `.ztoryc` sidecar file to mean anything at all.

The first pile is what this document is for. The second is listed too, in
§ 2.3 — not as a proposal, but so nobody has to wonder whether it was
overlooked.

### How sure we are of each item

Every bug fix carries one of three markers. They are about **evidence**, not
about how good the fix is:

| | meaning |
|---|---|
| ✅ | reproduced on **stock** Tahoma2D and fixed there — ready to become a PR |
| ❓ | fixed in Ztoryc; the upstream code looks identical, but nobody has reproduced it on stock yet |
| ⚠️ | the defect is readable in the code, but our trigger was Ztoryc-specific — best presented as hardening, not as a bug report |

Right now the count is **4 ✅, 21 ❓, 4 ⚠️**. That ratio is the honest state of
things: most of these were found while chasing something else, fixed, and moved
on from. The code was checked against upstream (`git show <remote>/master:<file>`)
but the *symptom* was not re-triggered on a clean build.

### Where it makes sense to start

**The four ✅ items are the ready ones** — they have a stock reproduction behind
them, so they can go straight to a branch and a PR.

**The most useful help is turning ❓ into ✅.** That work is: build stock, find a
scene or a sequence of actions that triggers the symptom, confirm it, then the
fix is a small diff we already have. It is unglamorous and it is the entire
bottleneck — a fix nobody has reproduced upstream is a fix a maintainer has to
take on faith, which is not a fair thing to ask.

Two ❓ items are worth singling out because they affect **every macOS and Linux
user**, not a corner case: **#5** (`lzoCompress` deadlock — silent hang saving
`.tlv`) and **#17** (`memoryShortage()` is a stub returning `false`, so the image
cache never evicts). If only two things get verified, those two.

### How settled each feature is

Certainty markers say whether a *bug* was reproduced. Features need a different
question — **how much mileage does it have?** — so each one in § 2.2 carries one
of these:

| | meaning |
|---|---|
| 🟢 | settled — shipped a while ago and used since, no known gaps |
| 🟡 | recent — shipped in the last release or two, little mileage yet |
| 🔴 | **work in progress** — actively being developed, known gaps, shape may still change |

This matters more than it looks. A 🔴 item is not a warning about quality, it is
a warning about **timing**: proposing something upstream freezes its interface,
and freezing an interface that is still moving is how a contribution becomes a
burden for everyone. The rigging suite is the clearest case — see the note under
§ 2.2.

### The two kinds of features

Features are split in Part 2 by a single test: **does it need the `.ztoryc`
sidecar to exist?**

- **§ 2.2 — portable.** Works on a plain Tahoma2D or OpenToonz scene. The
  storyboard pipeline is not involved. These are genuinely proposable.
- **§ 2.3 — Ztoryc-exclusive.** The storyboard, the animatic, the shot database.
  These read and write `.ztoryc`, and porting them would mean porting the whole
  data model. Listed for completeness only.

Some items sit near the line and are marked where they do. The lip sync is the
clearest example: the *engine* is general (see § 2.2), but the reason it is
accurate is that the words come from the storyboard panels — and that part is
not portable.

---

## Part 2 — The list

### 2.1 — Bug fixes

#### 🔴 High-impact

1. ❓ **Vector fill: closed shapes "unfillable" until scene reload + Maximum Gap resets on frame change** — `common/tvectorimage/tvectorimage.cpp`, `include/tvectorimage.h`, `tnztools/filltool.cpp/.h`. Incremental region recompute leaves intersection data stale; only reload rebuilt it. Long-standing, community-documented. Fix: `TVectorImage::forceRegionsRecompute()` on fill-tool activate + frame change; gap slider made sticky. `021d6886d`. *(gap part verified; fill part needs a stock repro)*
2. ❓ **`convertToExplicitHolds` turns sub-xsheets into IMPLICIT holds** — `toonzlib/txsheet.cpp` (~2732). Copy-paste bug (recurses with the inverse function). Code identical upstream — verify with a scene that has a sub-xsheet.
3. ✅ **CRASH dragging the "Drawing #" handle of the Animate tool** — `tnztools/edittool.cpp`. Single-channel tool used the two-channel API → OOB heap write. Root cause found via lldb, reproduced on stock (upstream feature PR #2124).
4. ⚠️ **`TUndoManager` use-after-free on reentrant `add()`** — `tcore/tundo.cpp`. `doAdd()`/`beginBlock()` truncate the redo branch without protecting the object whose `undo()`/`redo()` is running. Code identical upstream, but no stock repro (our trigger was Ztoryc code) → present as hardening. `c0e7c92bf`.
5. ❓ **`lzoCompress`/`lzodecompress` deadlock on macOS/Linux** — `tcodec.cpp`. `QProcess::start()` forks; a signal during `malloc_fork_prepare` deadlocks (silent hang on .tlv save). Fix: block signals around `process.start()`. Affects all Mac/Linux users — high priority. `140d790ac`.
6. ❓ **ImageManager cache leak after render** — `imagemanager.cpp`, `rendercommand.cpp`. All frames stay cached (~10 GB on 350-frame scenes). `be20f9512`.
7. ✅ **TasksViewer crash on room switch** — `tasksviewer.cpp`. Empty destructor leaves a dangling pointer in `BatchesController`. Verified and fixed — ready for PR. `1569cf2cc`.
8. ❓ **`requireColumnSoundTrack` allocates RAM proportional to audio duration** — 2h audio → ~1.3 GB per column. Cap `toFrame` to the video frame count. `69a8b9043`.
9. ❓ **Save Sub-Scene As path corruption** — `toonzscene.cpp`, `iocommand.cpp`.
10. ❓ **Wrong column-header thumbnail when sub-scenes share a name** — `icongenerator.cpp`. `XsheetIconRenderer::getId` uses a pointer instead of the name.
11. ❓ **Set Key (Z) not showing the keyframe diamond on peg columns** — `cellselectioncommand.cpp`. Used `ColumnId(col)` instead of `xsh->getColumnObjectId(col)`.
12. ❓ **Peg column width reset after delete** — `columnfan.cpp` (1 line). The column after a deleted peg inherits the reduced width. `b8ddea829`.
13. ❓ **PSD first layer lost when loaded as sub-scene** — `txshsimplelevel.cpp`, `tiio_psd.cpp`. Affinity 16-bit PSD, empty Pascal names, group mode: `##`→`#` replace corrupts the path. `5b8eeb3c1`.

#### 🟠 Medium

14. ❓ **`getPreviewButtonStates` null crash** — `viewerpane.cpp`. Crash if `m_previewButton`/`m_subcameraButton` uninitialized. `d7453d1eb`.
15. ❓ **Mesh sub-scenes saved to the wrong folder** — `meshifypopup.cpp`.
16. ❓ **New Scene missing the save dialog** — `iocommand.cpp`.
17. ❓ **`TSystem::memoryShortage()` always returns false on macOS/Linux** — `tsystempd.cpp`. A no-op (`return false`) → `TImageCache` never auto-evicts even near full RAM. Fix: `host_statistics64` (macOS) / `/proc/meminfo` (Linux). Affects all Mac/Linux users — high priority. `b79ba7d32`.
18. ❓ **macOS "Unable to create a new document" on launch** — `BundleInfo.plist.in` (`NSQuitAlwaysKeepsWindows`, `NSApplicationSupportsSecureRestorableState`). `a7a822704`.
19. ❓ **macOS CI deployment target** — needs `-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0` or the binary embeds the runner's `minos`. `940e895bc`.
20. ❓ **AutoFill undo/redo (brush)** *(⚠️ same dependency as #15 for OpenToonz.)* — the brush AutoFill wasn't undoable and refilled existing shapes; dedicated `AutoFillUndo` (before/after tiles) grouped with the stroke, and "new shapes only". `25ad78f53`, `toonzrasterbrushtool.cpp`. *(bug fix side of the AutoFill work; the color-picker part is a feature — see Part 2)*

#### 🟡 Windows / MSVC compatibility

21. ❓ **Alternative tokens `not`/`and`/`or` → `!`/`&&`/`||`** (48 sites). `105588c14`.
22. ❓ **Local variable `near` renamed** — collides with the `windef.h` macro on Windows. `8a4dbc294`.

#### 🆕 Added August 2026

23. ❓ **`renderFrame` is re-entrant and leaves framebuffer 0 bound** (macOS/Linux/FreeBSD) — `toonzlib/toonzscene.cpp`, the `renderFrame(ras, row, xsh, placedRect, worldToPlacedAff)` overload. Drawing a Plastic-deformed column goes through `texture_utils::getTextureData(const TXsheet *, int)`, which builds the sub-xsheet's texture by calling **this same function** again. The nested call binds its own FBO and ends with `fb->release()`, which binds framebuffer **0** — not the previous one. In an offscreen context framebuffer 0 is incomplete, so every subsequent draw of the outer render fails with `GL_INVALID_FRAMEBUFFER_OPERATION` (0x506) and the image comes out **blank**, not merely missing the character. `glPushAttrib(GL_ALL_ATTRIB_BITS)` does not cover this: the framebuffer binding is not attribute state. Fix: read `GL_FRAMEBUFFER_BINDING` before, restore it after (via `QOpenGLContext::currentContext()->functions()`, since `glBindFramebuffer` is not declared by the system headers on Linux). Measured, not deduced — the `0x506` was logged. To reproduce upstream: a scene with a Plastic-deformed column **inside a sub-xsheet**, rendered offscreen (scene icon / thumbnail), on macOS or Linux.
24. ❓ **`portableStatus` undeclared — Tahoma2D 1.6.2 does not compile on Linux** — `common/tapptools/tenv.cpp`, the `#elif defined(LINUX) || defined(FREEBSD)` branch of `setWorkingDirectory()`. The block that looks for the portable folder via `$APPIMAGE` uses a variable that does not exist anywhere. The branch compiles **only** on Linux/FreeBSD, so macOS and Windows never see it. Arrived with 1.6.2 ("Fix AppImage working directory"). Fix: use `TFileStatus(portableCheck)` directly, as the other branches do. *(Diagnosed on Ztoryc's Linux CI. If upstream's own Linux CI is green, they have a different build path and that should be understood before proposing.)*
25. ⚠️ **Rhubarb is found in the bundle by coincidence** — `toonzlib/thirdparty.cpp`, `autodetectRhubarb()`. Of the three external programs, Rhubarb is the only one whose search never names the bundle: `autodetectFFmpeg()` and `autodetectWhisper()` have an explicit `applicationDirPath() + "/../Resources/..."` line, Rhubarb does not. It is found anyway, but only because `TEnv::getWorkingDirectory()` happens to resolve to `Contents/Resources`. No symptom today — it is an implicit dependency between two things that do not know about each other. Present as alignment between the three functions, not as a defect.

26. ❓ **libgphoto2 plugins are shipped but never found — camera support is silently dead on user machines** (macOS) — `stopmotion/gphotocam.cpp`, the `GPhotoCam::GPhotoCam()` constructor. The packaging script copies `libgphoto2/` and `libgphoto2_port/` into `Contents/Resources`, but nothing sets `IOLIBS`/`CAMLIBS`, and libgphoto2 has its plugin paths **compiled in**, pointing at the build machine's Homebrew prefix. `strings` on the shipped `libgphoto2_port.12.dylib` gives `/usr/local/lib/libgphoto2_port/0.12.2`. On any Mac without Homebrew at that exact path, `gp_abilities_list_load()` and `gp_port_info_list_load()` load nothing: **no camera is ever detected**, with no error and no log — it just looks like no camera is connected. Fix: set both variables from `applicationDirPath()/../Resources` before the two `gp_*_list_load()` calls, discovering the version subdirectory instead of hardcoding it (hardcoding means the next libgphoto2 bump silently kills cameras again). *(Diagnosed, **not verified on Tahoma2D stock**: stock has zero occurrences of `IOLIBS`/`CAMLIBS` and the same in-bundle layout, so it should be affected identically — but Tahoma2D builds its own libgphoto2 fork, which could carry a path patch we have not read. Confirm with a real camera on a clean Mac before proposing.)*

---

#### ✨ New (2026-09-16)

- ✅ **`StyleEditor::setPaletteHandle()` is declared but has no body** —
  `toonzqt/styleeditor.cpp:4522`. The declaration is public in the header, so
  the call compiles and only the LINKER says the symbol does not exist: anyone
  trying to point a Style Editor at a palette of their own finds out the hard
  way. The commented-out body also shows why it was disabled — it swapped the
  pointer and called `onStyleSwitched()` **without moving the signal
  connections**, which `showEvent()` binds to the handle and `hideEvent()`
  drops; swapping while visible left the editor showing one palette and
  listening to another.
  **Fix:** implement it properly — disconnect from the old handle and reconnect
  to the new one *when the editor is on screen*, do nothing when it is hidden
  (showEvent will wire whatever handle is current by then), then
  `onStyleSwitched()`.
  **Why it matters upstream:** it is what lets a panel host its own Style Editor
  on its own `TPaletteHandle` instead of hijacking the application's shared one
  — no state to restore, no way to break drawing elsewhere. Ztoryc's Thumbnail
  room brush palette is the first consumer; the need is general.
  *(Written and built here; wants a stock Tahoma build to confirm nothing else
  depends on the symbol being absent.)*

- ✅ **`StyleEditor` cannot show "brush browser + brush parameters"** —
  `toonzqt/styleeditor.cpp` (`updateTabBar()`, `setPage()`, header). The tab bar
  has exactly three shapes: all five tabs (Color, Raster, Texture, Vector,
  Settings), Color only, or Color+Settings. A panel whose palette holds nothing
  but MyPaint brushes needs **Color + Raster + Settings** and cannot get it — so it has
  to show Color / Texture / Vector, where one click REPLACES a MyPaint style
  with a style of another type. The brush then quietly stops being a brush.
  **Fix:** a fourth mode (`enableBrushPagesOnly()`), plus the matching branch in
  `setPage()`. ⚠️ The tab→page mapping is **not** 1:1 and that is
  where this is easy to get wrong: Color and Raster are pages 0 and 1, but the
  Settings page is second-to-last in the stack (the last one is blank).
  **Why it matters upstream:** any panel hosting a brush-only palette wants
  this; it pairs with the `setPaletteHandle()` fix above.
  *(Written and built here; wants a stock Tahoma build to confirm the other
  three modes are untouched.)*

- ✅ **A MyPaint brush saved in a palette records an ABSOLUTE path, so the
  palette only works on the machine that made it** — `toonzlib/mypaintbrushstyle.cpp`,
  `loadBrush()`. `BrushStyleManager::loadItems()` builds each library brush from
  an absolute path (`TSystem::readDirectory` on an absolute folder), `loadBrush()`
  stores it verbatim in `m_path`, and `saveData()` writes exactly that — so a
  `.tpl`, a scene palette, or a studio palette carries something like
  `/Users/…/stuff/library/mypaint brushes/classic/pencil.myb`.
  **What the user sees: nothing.** Open that palette where the file is not at
  that path — another machine, another OS, a clean install into a different
  folder, a portable folder moved — and `loadBrush()` silently falls back to
  `fromDefaults()`. The style still paints, and the user's own saved parameters
  are reapplied on top, so it looks like a brush and behaves like the WRONG one.
  No error, no log line.
  **Fix:** when the resolved file lives inside one of `getBrushesDirs()`, store
  `m_path` relative to that directory (`m_fullpath - dir`, guarded by
  `isAncestorOf`). `decodePath()` already resolves relative paths against those
  same directories — the mechanism exists and was simply never used on the way
  in. Paths outside the library (a `.myb` kept beside a scene) stay absolute, and
  palettes already saved with absolute paths keep working, since `decodePath()`
  returns an absolute path unchanged. Backward compatible both ways.
  **Measured, not deduced:** a probe linked against the real libraries saves and
  reloads a palette, with a negative control — after the fix the stored path is
  `classic/pencil.myb`, and the reloaded brush reports `hardness` 0.10 against
  0.80 for a brush whose file is missing, `slow_tracking` 1.00 against 0.00. That
  difference is what proves the file was read: radius, opacity and colour come
  back identical either way, because they are the user's own values reapplied
  from the palette.
  **Why it matters upstream:** this is not a Ztoryc corner — it is every scene
  palette and every studio palette containing a MyPaint brush, in any studio that
  shares scenes between machines.
  *(Written and built here, verified with the probe; wants a stock Tahoma build
  and a shared-scene check before proposing.)*

- ✅ **"Overwrite Paste Cell Numbers" pastes nothing, silently, whenever the
  destination column is empty** — `toonz/cellselection.cpp`,
  `TCellSelection::overwritePasteNumbers()`. When `pasteNumbersWithoutUndo()`
  returns false the function deletes its undo data and returns with no message
  at all. The command is called Paste, the clipboard does hold cells, and the
  xsheet simply does not change — there is nothing on screen to say that a
  preference picked a different kind of paste, or that renumbering needs a level
  in the destination column to renumber. Pasting into an empty column hits this
  every single time.
  **How it was found:** a user reported "cell copy/paste stopped working, it
  only works inside the same column". It was not a defect at all —
  `pasteCellsBehavior` was set to 1 — but the absence of any feedback is what
  made it look like one, and cost a diagnosis session. The other early returns
  in the same function (locked column, type mismatch, circular reference) all
  show a `DVGui::error`; only this one is mute.
  **Fix:** a `DVGui::warning` on that branch, naming the preference, what
  "Overwrite Paste Cell Numbers" actually does, and the two ways out (Paste
  Cell Content, or Preferences > Scene > Paste Cells Behaviour).
  ⚠️ **The category is labelled "Scene", not "Xsheet".** `createXsheetLayout()`
  fills `m_categoryBoxes[7]`, whose title comes from the 8th entry of the
  `categories` list — which reads `tr("Scene")`. The function name is the only
  place the word Xsheet appears, so anyone writing user-facing text from the
  code (or a message like this one) sends people looking for a tab that does not
  exist. Worth renaming the function, or the category, in the same PR.
  **Why it matters upstream:** the behaviour, the preference and the silence are
  all stock — nothing here is Ztoryc-specific. Note that
  `TCellSelection::pasteCells()` has the same shape of problem on its
  `canChange()` branch, where `overwritePasteNumbers()` does show an error;
  worth aligning the two in the same PR.
  *(Written and built here; the fix is a message, so it needs no behavioural
  verification, but a stock build should confirm the wording matches upstream's
  menu labels.)*

- ❓ **`mypaint::Setting::all()` and `mypaint::Input::all()` re-run their whole
  initialisation on EVERY call** — `include/toonz/mypaint.h`, around lines 264
  and 317. Both declare `static bool initialized = false;`, both test it, and
  **neither ever sets it to true.** So every call rewrites the `std::string`
  members (`key`, `name`, `tooltip`) of a shared static array, and re-runs a
  gettext lookup per name and per tooltip — `mypaint_brush_setting_info_get_name`
  and `..._get_tooltip` are translated. These are called from
  `TMyPaintBrushStyle::loadData()` (once per modified parameter of every brush
  loaded) and from `findByKey()`.

  **What is CERTAIN:** the flag is dead code and the initialisation is repeated.
  That is provable by reading, and it is a defect on its own — wasted work, and
  continuous rewriting of shared static state.

  **What was RULED OUT, the same evening:** this is **not** what caused the heap
  corruption we were chasing on 2026-09-16. It looked like it — the block the
  allocator choked on had been allocated under `Input::all()`, with
  `libintl_dcigettext` right below it in the stack, and after the fix the scene
  opened. Then it crashed again. AddressSanitizer later showed the real cause,
  which was ours and elsewhere entirely (a `delete` on a ref-counted style owned
  by a palette — see ANIMATIC_TASKS). A matching allocation site and a plausible
  mechanism were not proof, and saying so at the time is the only reason this
  entry is still honest.
  **Present it upstream as a dead flag and repeated work**, which is what is
  demonstrable, and **never as a crash fix.**

  **Fix:** `initialized = true;` at the end of both loops.

  **The "corroboration" that was not one:** an older archived build (Ztoryc-SP)
  opened the same scene fine, which looked like support for "latent upstream
  defect woken by a new caller". It was the second reading that turned out
  right — the new caller was wrong by itself. Worth remembering: an observation
  compatible with two explanations supports neither.

#### ✨ New (2026-09-25)

- 🔴 **CRASH deleting a key on a channel the Function Editor's segment viewer
  once showed** — `toonzqt/functionsegmentviewer.cpp`,
  `FunctionSegmentViewer::~FunctionSegmentViewer()`. `setSegment()` and
  `setSegmentByFrame()` register the viewer as an observer of the curve
  (`m_curve->addObserver(this)`), but the destructor only does
  `m_curve->release()`: the curve keeps a dangling `TParamObserver*`. The next
  change to that curve — deleting a key, say — runs
  `TDoubleParam::Imp::notify()` over the observer set and calls `onChange()` on
  freed memory: `EXC_BAD_ACCESS` in `tdoubleparam.cpp:332`, from
  `TKeyframeSelection::deleteKeyframesWithShift` →
  `TStageObject::removeKeyframeWithoutUndo`. **Reproduced 3/3 under lldb**
  (debug build, `MallocScribble`): the offending observer is the only entry
  besides the `TStageObject` itself, its vtable pointer reads 0, and
  `malloc_history` shows its memory re-used by a widget built during a room
  rebuild. **Fix:** `m_curve->removeObserver(this)` before the release —
  exactly what `FunctionToolbar::~FunctionToolbar()` already does.
  **Identical in Tahoma2D master and OpenToonz master.** Why it barely shows
  upstream: the Function Editor is rarely destroyed while a scene lives.
  Ztoryc rebuilds the rooms on every workflow switch, so the viewer dies with
  the old room set and the crash comes at the first key edit on the channel it
  was showing. Any upstream path that destroys a Function Editor panel (closing
  a floating one, a room reload) should do the same. *(Fix built; verification
  in the running app pending. Not verified on stock — to reproduce: show a
  curve in a floating Function Editor, close the panel, delete a key on that
  channel from the xsheet.)*

- 🟡 **Hardening: nearest vertex/edge on an EMPTY mesh reads out of bounds** —
  `tnztools/plastictool_meshedit.cpp`, `closestVertex(const TTextureMesh&, …)` /
  `closestEdge(…)`: `std::min_element` on an empty range returns `end()`, then
  `mesh.vertex(end.index())` is read. Called on every mouse move in mesh edit.
  Guarded (skip empty/null meshes) + the build-mode snap in
  `plastictool_build.cpp` falls back to the mouse position. `5cbf0265c`.
  *(Hardening, NOT a proven fix: a crash there after a Cut Mesh could not be
  reproduced under lldb.)*

#### ✨ New (2026-09-24)

- 🟠 **Sound goes silent for good after an underrun** —
  `toonz/sources/common/tsound/tsound_qt.cpp`, `TSoundOutputDeviceImp`.
  Push mode: the buffer is refilled only from `QAudioOutput::notify()`, which
  Qt emits only while the output is CONSUMING audio. If the UI thread stalls
  longer than the 100 ms buffer, the output underruns, goes `IdleState` +
  `UnderrunError`, stops consuming — so `notify()` never comes again and
  nothing refills it: silent for the rest of playback while the picture goes
  on. **Measured** in Ztoryc with a probe on the output's states: two outputs
  underran in the same millisecond after 18 s of play, buffer index frozen
  until the stop 12 s later. **Fix:** on `stateChanged(IdleState)` with
  `UnderrunError` and audio still to play, call `sendBuffer()` — writing data
  resumes the output; the gap lasts as long as the stall, not the rest of the
  film. Same code on `upstream/master`.

- 🟡 **Sound keeps playing on the old output after switching device** —
  `toonz/sources/common/tsound/tsound_qt.cpp`, `TSoundOutputDeviceImp::play`.
  The `QAudioOutput` is rebuilt only when the audio FORMAT changes, and a
  `QAudioOutput` stays bound to the device that was the system default when
  it was created. Switch from speakers to headphones and any output device
  created before (scrub, a column player) keeps sounding from the speakers.
  **Fix:** remember the device name, rebuild when the default changes, and
  pass the `QAudioDeviceInfo` to the constructor explicitly —
  `defaultOutputDevice()` was already queried on every `play()`, so it costs
  nothing. **Same code on `upstream/master`** (lines 175-188, checked
  2026-09-24). Seen in Ztoryc by Franco (scrub on speakers, playback on
  headphones); not yet reproduced on stock Tahoma.

- 🟠 **The viewer steals the keyboard from multi-line text fields** —
  `toonz/sceneviewerevents.cpp`, `SceneViewer::onEnter()`. When the mouse
  enters the viewer it calls `setFocus()` unless the focused widget is a
  `QLineEdit`; `QTextEdit` and `QPlainTextEdit` were not spared. So merely
  crossing the viewer on the way elsewhere takes the keyboard away from a
  multi-line field: typing continues as viewer shortcuts, and Cmd/Ctrl+C on
  text selected in a read-only one copies the viewer's selection instead.
  **Measured, not deduced:** a probe on focus changes and Copy key events
  showed the viewer taking the focus 42 ms before the Cmd+C that failed, and
  the successful ones arriving at the text field (Ztoryc's screenplay panel,
  reported by Franco as "the first Cmd+C works, the second pastes the previous
  sentence"). **Fix:** two more `dynamic_cast`s beside the `QLineEdit` one.
  **Same code on `upstream/master`** (checked 2026-09-24, lines 493-502), so
  it applies as is. Not reproduced on stock Tahoma: stock has no read-only
  text panel, but any QTextEdit next to a viewer (e.g. a dockable text panel)
  should show it. Ztoryc commit: see CHANGELOG 2026-09-24.

#### ✨ New (2026-09-23)

- ⛔ **WITHDRAWN before it was ever proposed — "changing the camera format is
  not undoable"** — `toonz/camerasettingspopup.cpp`,
  `toonzqt/camerasettingswidget.cpp`. Logged here for half an hour on
  2026-09-23 as a gap, then withdrawn the same day. **Do not re-propose it.**

  The measurement stands and is worth keeping: `TUndo` occurrences in those
  two files are **0 and 0** on `upstream/master` (tahoma2d) and **0 and 0** on
  `opentoonz/master`. So a camera format change really is outside the undo
  chain everywhere. What changed is the reading of that fact.

  **It is not a gap, it is the right behaviour** (Franco, 2026-09-23, after
  checking it in Tahoma himself): changing the camera format is a change to
  the PROJECT SETTINGS, not to the work, and it belongs outside the undo chain
  — like the frame rate. The case that settles it: draw, change camera, keep
  drawing; if the camera change were in the chain, undoing the last few
  strokes would force you through it and leave the camera changed without
  asking.

  Consequence for Ztoryc, already applied: the Thumbnail room no longer keeps
  an undo snapshot of its grid re-layout either. Making the CONSEQUENCE
  undoable while the CAUSE was not is exactly what produced the defect Franco
  hit — the grid going back while Camera Settings stayed on the new format,
  and a 16:9 snapshot left in the stack by every scene open.

#### ✨ New (2026-09-18)

- ✅ **A scene whose file name ends with `_` (or `-`) cannot be opened at all,
  and the failure is silent** — `common/tsystem/tfilepath.cpp`
  (`getDots()`, `getSepChar()`, `getFrame()`), with the silent exit in
  `toonz/iocommand.cpp` (`IoCmd::loadScene`).
  `rfindFrameSep()` accepts `.`, `-` and `_` as the separator that may precede a
  frame number. When the separator sits **immediately before the extension**
  (`SB_.tnz`), the code reads it as an *empty frame* — a level with no frame
  number — **without checking whether the type can be a frame sequence at all**.
  `getDots()` then returns `".."`, and the comment beside it is explicit:
  *"return '..' regardless of sepChar type (either '_' or '.')"*. So the
  underscore comes back as a dot.
  The damage is done one call later, in `TSystem::readDirectory`:
  ```cpp
  if (son.getDots() == "..") son = son.withFrame();   // tsystem.cpp:556
  ```
  `SB_.tnz` is collapsed into the level `SB..tnz`. Every scene browser, the
  startup window included, now carries a path that **names no file on disk**.
  `IoCmd::loadScene` checks `doesExistFileOrLevel`, fails, and takes the one
  exit in the whole function that shows **no message at all**.
  **What the user sees:** they click their storyboard, no dialog appears, and
  they land on the untitled scene they started from — which reads exactly like
  the scene has been emptied. Reported here on 2026-09-18 as *"I find the SB
  scene empty"*; the file was intact all along (33 shots, 253 KB, loads and
  renders fine under `tcomposer`, which never round-trips the name).
  Middle underscores are safe (`lib_kiko.tnz`): there the separator is followed
  by letters, not by the extension.
  **Fix:** the empty-frame shortcut is only universal for the double dot. A `_`
  or `-` before the extension is the underscore/hyphen spelling of a level name,
  and means a sequence **only for a type that can be one**:
  ```cpp
  inline bool isEmptyFrameSep(const std::wstring &str, int sep, int dot,
                              const QString &type) {
    return sep == dot - 1 && (str[sep] == L'.' || checkForSeqNum(type));
  }
  ```
  used in `getDots()`, `getSepChar()` and `getFrame()` in place of the bare
  `sep == dot - 1`. `pippo..tif` still reads as a sequence; `pippo_.tif` still
  reads as one, because `tif` passes `checkForSeqNum`; `SB_.tnz` no longer does,
  because a scene is not a frame sequence.
  **Second, separable fix:** give that `return false` in `IoCmd::loadScene` a
  message. Every caller is user-initiated (startup window, Open Recent, Revert,
  file browser, script console, command line), and a path that does not resolve
  should never vanish without a word — the silence is what turned a
  one-character parsing bug into "I lost a day of storyboard".
  **Measured, not deduced:** the path was read out of the running application
  (`[ZDIAG] path=[…/scenes/SB..tnz] exists=0`), and a probe linked against the
  real libraries confirms the fix — `SB_.tnz` now reports `dots="."`,
  `name="SB_"`, and `TSystem::readDirectory` on the real folder returns it
  intact and existing.
  *(Written, built and verified in the running app here; wants a stock Tahoma
  build to confirm no level-naming regression — the underscore frame format is
  the part to exercise.)*

### 2.2 — Features that can go upstream as they are

- 🟡 **Alt (⌥) + middle-drag = the Rotate tool, held** — `toonz/sceneviewerevents.cpp`
  (`SceneViewer::onPress` + one condition in `onMove`), and **Alt+0 = Reset
  Rotation** (`mainwindow.cpp`, `deftahoma2d.ini`). Tried by Franco
  2026-09-24.
  Today the view can be rotated only with a pinch (touch), or by holding the
  `T_Rotate` shortcut and dragging with the LEFT button. Alt + middle press now
  enters that same temporary mode — `m_mouseRotating`, the same
  `mouseRotate()`, the same cursor — and releasing the button leaves it
  (`m_resetOnRelease`), like letting go of the key. No new rotation code: a
  keyboard shortcut cannot carry a mouse button, so the binding lives in
  `onPress`. Without Alt the middle button still pans; Shift+Ctrl+middle
  still scrubs.

  **Alt+0.** With rotation one gesture away, straightening is the common case,
  and Reset View (the old Alt+0) also throws away zoom and pan. Alt+0 becomes
  Reset Rotation (which had no shortcut) and Reset View moves to Ctrl+Alt+0 —
  not Alt+Shift+0, which depends on the keyboard layout (Shift+0 is "=" on an
  Italian keyboard, and macOS matches by character). ⚠️ This one is a change
  of DEFAULTS that existing users would notice: propose it separately from the
  gesture, as a question rather than a patch.

  **Where the idea comes from:** Ztoryc's Thumbnail room, which has its own
  tool system; Franco then wanted it everywhere — touch is on few machines, a
  modifier and a mouse are on all of them.

  ⚠️ A first version wrote its own rotation (angle of the pointer around the
  centre, sign taken from the pinch branch). Dropped on Franco's indication in
  favour of reusing the native mode — the better proposal upstream too: it
  adds a binding, not a second way of rotating.


Nothing here needs the `.ztoryc` file. They operate on ordinary scenes, levels
and xsheets.

#### 🏳️ ZtoRig / SuperPlastic — rigging suite for the Plastic tool · 🔴 WIP

The flagship. General animation features on the Plastic (mesh-deformation) tool,
useful to anyone rigging cut-out characters in stock Tahoma2D/OpenToonz. Can be
one large contribution or split.

> 🔴 **Read this before picking any of it up.** The core (1–4) shipped in
> **v0.11.0, July 2026**; joint correctives (5) are from **August 2026**.
> Development is **deliberately paused** since 2026-08-14 — not abandoned, but
> the author stopped to build an actual character with it, which is the right
> order and also means the interface is still moving. Two defects are open,
> found by using it rather than by inspection:
> - sculpting a corrective on an **arm** does not produce one (unclear yet
>   whether the corrective is not created or created and not shown);
> - **Show SO** turns on with **Order** but does not turn off with it — a paired
>   state that remembers how to switch on and not how to go back.
>
> Both are small. Neither is fixed. Anything proposed from this section should
> wait for the pause to end, or be scoped to a piece that is demonstrably still.

1. **Inverse Kinematics / pins** — keyframeable pins, foot/hand planting held per-frame (through in-betweens), free root via rigid-rig translation, multi-pin, clean bake-to-FK when leaving IK.
2. **Keyframeable joint angle limits** — min/max bounds with a draggable in-viewer gizmo, animatable.
3. **Squash & stretch controller on the skeleton** — Animate-tool-style gizmo (move/rotate/scale/shear about a keyframeable pivot) on top of the deformed skeleton, with a show/hide toggle.
4. **Cross-level / multi-column skeletons** — treat hook-connected columns as one rig: unified view + selection, cross-column posing, unified FK (child roots act as ordinary chain joints), smarter picking at coincident joints.
5. 🔴 **Joint correctives (pose-space deformation)** *(August 2026, newest of the five)* — corrective shapes driven by a joint's rotation, so an elbow or a shoulder keeps its silhouette through its range. Authored with a sculpt brush and edited as a **track in degrees** rather than a table, so you see *where* a corrective acts and *how much*, along the joint's rotation. Includes a stacking-order control.

> ⚠️ **Strategic note:** work in progress and the project's competitive differentiator (the Harmony/Moho-direction work). Whether and when to upstream it is a business decision, not a technical one.

#### 🔹 Lip sync · 🟡 recent (shipped v0.13.0–v0.13.1, August 2026)

6. 🟡 **Forced alignment from a known script** — when the words are already known, a recogniser is not asked *what* was said but only *when*, which is a much easier question. The script becomes a closed vocabulary and the recogniser runs as a forced aligner; a proper noun it has never heard no longer derails the take. Measured against the spectrogram on our own recording (the /s/ of *questo*, the /f/ of *fa* — physical events, not another model's opinion): **Vosk 10 ms** mean error (0.2 frames), **Whisper 30 ms**, **Whisper+DTW 191 ms**. Engines: Vosk (Apache 2.0) for timing, whisper.cpp (MIT) as the fallback for languages Vosk has no model for, espeak-ng (GPLv3, invoked as a separate process, never linked) for phonemes → mouth shapes.
   > **Where the line is:** the engine is portable. *Where the text comes from* is not — in Ztoryc it comes from the storyboard panels. Upstream would need a text source of its own (a per-shot field, an imported file). Tahoma2D already ships Rhubarb, which needs no script at all; this is the accuracy path for when a script exists.
7. 🟡 **Add a language without rebuilding** — a preferences entry that installs a Vosk model downloaded by the user, stored next to personal settings rather than in the cache (which can be emptied). Every pass reports **which engine did the work**, so loose timing reads as a missing model instead of a broken feature.
8. 🟡 **Mouth sets stored beside the level** — a character's viseme→drawing map lives in a file next to the level file, on one rule: *the map lives where the thing lives*. Importing a character into a scene brings its mouths along. A viseme is a **list** of targets, not one, because in cutout the mouth and the teeth change on the same frame.

#### 🔹 Animation & timeline

9. 🟢 **Keys Follow Exposure** (Harmony/Moho-style) — a visible toolbar toggle (`MI_ToggleKeyframesFollowExposure`) that makes keyframes follow cell exposure edits. Includes:
   - **Combined cell + keyframe selection** — `TCellKeyframeSelection` (inherits `TCellSelection`): select and edit cells and keys together.
   - Cell operations that carry the keyframes: **Reverse, Roll Up / Roll Down, Swing, Repeat, Time Stretch**.
   - **"Edit Cels/Keys"** context submenu on **both** cells and keyframes.
   - Level Extender (shrink) keyframe-undo fix.
   - Files: `txsheet.cpp`, `xsheetdragtool.cpp`, `xshcellviewer.cpp`, `cellselectioncommand.cpp`, `timestretchpopup.cpp`, `duplicatepopup.cpp`.
10. 🟢 **Main Audio toggle** — play the main xsheet's soundtrack while inside a sub-scene (`MI_ToggleMainAudio`).
11. 🟢 **Zoom-to-cursor in the timeline** — the frame under the cursor stays fixed while zooming. `b8ddea829`.
12. 🟢 **Per-xsheet In/Out markers** — separate play-range markers for the main xsheet vs each sub-scene.

    **How it works** *(written out for Rodney, who asked on 2026-08-18)*. Upstream, the play range is a single **global GUI state** (`XsheetGUI::get/setPlayRange`). Nothing is attached to the xsheet, so entering a sub-xsheet and coming back leaves whatever range the GUI last held: the main xsheet's range gets clobbered by the sub-scene's, and vice versa. Three pieces fix it.

    1. **Storage on the xsheet itself** — `TXsheet` gains `m_markerIn`/`m_markerOut` plus `getInOutMarkers()`, `setInOutMarkers()`, `hasInOutMarkers()` (`include/toonz/txsheet.h:179, 666-675`). The convention that makes it work is **`m_markerOut == -1` means "unset"**: it distinguishes "no range" from a real range, so a disabled range does not come back as a bogus `-1/-2` after reload — it falls back to automatic.
    2. **Serialised into the `.tnz`** — read at `toonzlib/txsheet.cpp:1893`, written at `:1965` as `os.child("inOutMarkers") << m_markerIn << m_markerOut`, and **only when `m_markerOut >= 0`**. So markers survive save+reload for every (sub-)xsheet, and scenes without the tag simply have none. Compatible in both directions.
    3. **Save/restore at the sub-scene boundary** — `subscenecommand.cpp`, in `openSubXsheet()` and `closeSubXsheet()`. On leaving an xsheet the current GUI range is written both to a session map (`s_frameRangeMap`, keyed by `TXsheet*`) and to the persistent markers; on entering, the range is restored with the **persistent markers taking priority**. They must take priority because a raw pointer is not stable across a load, so the session map alone cannot work after reopening a scene.

    **Note on "does it behave differently in storyboard mode?"** No — there is no mode branch. It is the same three-level chain everywhere: **persistent markers → session cache → automatic fallback**. Once a sub-xsheet has explicit markers they win, and they stay put in every mode. The only place where markers appear to "move by themselves" is the **third** branch, which runs only for a sub-xsheet that never had explicit markers.

    ⚠️ **What a clean port must strip — two things, both in that fallback branch.**
    1. `openSubXsheet()` extends `markOut` by the length of any sound-text column named `XD-in` / `XD-out`. That is Ztoryc animatic cross-dissolve bookkeeping, meaningless upstream.
    2. It calls `shotColPtr->getRange(r0, r1, /*ignoreLastStop=*/true)`. That flag exists because `ZtoryModel::resequenceXsheet()` parks a trailing Stop Frame Hold at `r1+1` of every shot column to block implicit-hold bleed; without the flag the duration is inflated by one and the mark-out lands one frame past the content. **Upstream has no such trailing hold**, so the flag should not be passed. The `ignoreLastStop` parameter itself is **not** a Ztoryc addition — it already exists in Tahoma2D stock (`include/toonz/txshcolumn.h:154, 240`), but **not in OpenToonz**, so an OT port must use plain `getRange()`. What is Ztoryc-specific is only the *reason* we pass `true`.

    Strip both and the fallback becomes simply "full duration of the cell block in the parent", which is the sensible upstream behaviour.

    **Do NOT bundle the storyboard duration syncing into this candidate.** In Ztoryc two things look like "the markers follow the shot", and only one of them is this feature:
    - *Lengthen a shot in the timeline → the sub-scene's mark-out follows.* This is **not** a write to the markers: it is the fallback above recomputing `markOut` from the parent column on the next `openSubXsheet()`. It therefore happens **only while that sub-xsheet has no explicit markers**. Once markers are set, priority 1 wins and lengthening the shot merely clamps them (`qBound` to the frame count).
    - *"Match shot duration" → the timeline follows the sub-scene.* The opposite direction, and it never touches the markers at all: `StoryboardPanel::onMatchDuration()` rewrites the main column's cells to `childLevel->getXsheet()->getFrameCount()`. Pure Ztoryc storyboard code, outside the scope of this candidate.

    For completeness, markers are written in exactly three places: `open/closeSubXsheet()` (the boundary sync), and `iocommand.cpp:1674` on save — the latter covers setting a range and saving **without ever entering or leaving a sub-scene**, which is otherwise the only moment they would get synced. A port needs that third one too, or ranges silently fail to persist in the simplest workflow of all.
13. 🟢 **Keyframe diamond grammar** *(v0.10.0)* — one source of truth for the diamond's colours and detection, so the xsheet, the timeline and the viewer cannot disagree about what kind of key a cell holds, plus a keyframe navigator in the viewer.
14. 🟡 **Function editor usable on its own** *(August 2026)* — it no longer needs a "current curve" to stand up, and the interpolation handling (Auto Bezier, Flat, tangents, Rove) was straightened out.

#### 🔹 Brush / painting

15. 🟢 **AutoFill fill-style picker** *(⚠️ blocked for OpenToonz: OT has no Auto Fill / Vector Auto Close yet — those must be ported from Tahoma2D first. Rodney, 2026-08-18. Can go to **Tahoma2D** directly.)* — choose the colour AutoFill uses: "Next Style (N+1)" (default) or "Current Style", plus a dynamic palette picker listing all styles as `[N] StyleName` (rebuilds on palette/level change). `toonzrasterbrushtool.h/.cpp`, `tooloptions.cpp`.
16. 🟡 **Tilt that follows the surface, not the screen** *(⚠️ same for OpenToonz: OT has no pen-tilt support yet. Rodney, 2026-08-18. Tahoma2D directly is fine.)* *(August 2026)* — with the elbow bent, a tablet's tilt axes do not line up with the screen axes, and the brush ends up shaped by how you are sitting rather than how you are drawing.

#### 🔹 File handling

17. ✅ **TAKEN UPSTREAM by Rodney Baker, 2026-08-18** — [OT-Dev PR #91](https://github.com/OpenAnimationLibrary/opentoonz-dev/pull/91), *Recognize hyphen-separated image sequences*, +50/-45 on `tfilepath.cpp` and `tfilepath.h`. **Do not re-propose.** His port is the SAME design as ours — identical `rfindFrameSep(str, i, underscoreAllowed)` helper, same legacy priority for `.`, same rightmost-of-`-`/`_` rule, same call-site rewrites; the only difference is `jh > ju ? jh : ju` instead of `std::max(ju, jh)`. So the next upstream merge of `tfilepath.cpp` should be clean — but **check that file first anyway**, because a bad conflict resolution there breaks level-name recognition everywhere and does not fail loudly.
    Original entry: 🟢 **Image-sequence recognition with `-` (hyphen) separator** — `frame-0006.jpg` (common from DaVinci, Blender and other exporters) recognised as a sequence, not a single level; guarded so `my-file.jpg` stays a single level. `common/tsystem/tfilepath.cpp`, `include/tfilepath.h`.

---

### 2.3 — Ztoryc-exclusive: **not** proposable

These need the `.ztoryc` sidecar and the shot data model behind it. Porting one
would mean porting the whole pipeline. Listed so nobody has to ask whether they
were forgotten.

- **Board room** — the shot grid: panels with dialogue, action and notes, camera-move notation drawn over the thumbnail (START/STOP rectangles, A→B letters, Pan/Tilt/Trk labels), light-direction gizmos, PDF and spreadsheet export.
- **Automatic panel detection** — where a new panel begins inside a shot, and the `Panels/s` cap that keeps a fully animated level from producing one panel per frame.
- **Animatic room** — NLE-style timeline over the main xsheet, audio-clocked playback, ripple edit, razor, real cross-dissolves rendered at render time, FCPXML export with transitions for DaVinci.
- **Shot export** — exporting each shot as its own scene, optionally pre-populated from the breakdown (characters as sub-scenes, props and backgrounds as levels) and with lip sync columns already written; export to an external OpenToonz/Tahoma2D project.
- **Production Tracker** — the shot database, the per-shot asset breakdown, Kitsu synchronisation, shot identity as *sequence + number*.
- **Script panel** — screenplay import and the dialogue that feeds the panels.
- **Scene roles** — storyboard / shot / character, with badges in the file browser.
- **Paper import** — printing a numbered storyboard sheet, then importing the scanned or photographed drawings back into panels.

---

### 2.4 — How a PR is prepared here

**Permanent stock worktrees**, so that "it works on our binary" is never said in
place of "reproducible on stock":

| worktree | branch | for |
|---|---|---|
| `tahoma-stock` | from `upstream/master` | reproducing and verifying on clean Tahoma2D |
| `opentoonz-stock` | from `opentoonz/master` | the same on OpenToonz |

They are `git worktree`s, not clones: they share the object database. The bundles
they produce (`Tahoma2D.app`, `OpenToonz.app`) differ from `Ztoryc.app`, so they
run side by side without fighting over the single-instance lock. Both remotes
have push **DISABLED**.

**Steps, in order:**

1. `git worktree add -b pr/<name> ../tahoma-stock-<name> upstream/master`.
2. **Check the bug is actually there**, and in which of the two projects: a defect
   introduced by a Tahoma commit is not in OpenToonz, and vice versa; old shared
   code is usually in both. This can be checked without compiling —
   `git show <remote>/master:<file> | grep ...`.
3. Apply **only** that change, with comments rewritten neutrally — no private
   scene names, no local paths, no references to internal conversations.
4. Build and test **there**.
5. **Automated review** on the PR branch, to catch the mechanical slips: one call
   site out of eight forgotten, a case left unhandled.
6. **Human review** — *after* the automated one, never before. Sending a
   volunteer something a machine would have caught spends their goodwill. This is
   for the questions only someone with the project's history can answer: is this
   the shape of fix they would accept? does it touch behaviour somebody relies
   on? is there a historical reason it is written that way?

   **How:** open the PR **on the fork** (`pr/<name>` → `matitanimata/ztoryc:master`).
   That gives the whole GitHub review interface — line comments, applicable
   suggestions — **without notifying the Tahoma2D maintainers**. Once it is
   agreed, it is redirected upstream.
