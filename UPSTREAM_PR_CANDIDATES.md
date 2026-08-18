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
20. ❓ **AutoFill undo/redo (brush)** — the brush AutoFill wasn't undoable and refilled existing shapes; dedicated `AutoFillUndo` (before/after tiles) grouped with the stroke, and "new shapes only". `25ad78f53`, `toonzrasterbrushtool.cpp`. *(bug fix side of the AutoFill work; the color-picker part is a feature — see Part 2)*

#### 🟡 Windows / MSVC compatibility

21. ❓ **Alternative tokens `not`/`and`/`or` → `!`/`&&`/`||`** (48 sites). `105588c14`.
22. ❓ **Local variable `near` renamed** — collides with the `windef.h` macro on Windows. `8a4dbc294`.

#### 🆕 Added August 2026

23. ❓ **`renderFrame` is re-entrant and leaves framebuffer 0 bound** (macOS/Linux/FreeBSD) — `toonzlib/toonzscene.cpp`, the `renderFrame(ras, row, xsh, placedRect, worldToPlacedAff)` overload. Drawing a Plastic-deformed column goes through `texture_utils::getTextureData(const TXsheet *, int)`, which builds the sub-xsheet's texture by calling **this same function** again. The nested call binds its own FBO and ends with `fb->release()`, which binds framebuffer **0** — not the previous one. In an offscreen context framebuffer 0 is incomplete, so every subsequent draw of the outer render fails with `GL_INVALID_FRAMEBUFFER_OPERATION` (0x506) and the image comes out **blank**, not merely missing the character. `glPushAttrib(GL_ALL_ATTRIB_BITS)` does not cover this: the framebuffer binding is not attribute state. Fix: read `GL_FRAMEBUFFER_BINDING` before, restore it after (via `QOpenGLContext::currentContext()->functions()`, since `glBindFramebuffer` is not declared by the system headers on Linux). Measured, not deduced — the `0x506` was logged. To reproduce upstream: a scene with a Plastic-deformed column **inside a sub-xsheet**, rendered offscreen (scene icon / thumbnail), on macOS or Linux.
24. ❓ **`portableStatus` undeclared — Tahoma2D 1.6.2 does not compile on Linux** — `common/tapptools/tenv.cpp`, the `#elif defined(LINUX) || defined(FREEBSD)` branch of `setWorkingDirectory()`. The block that looks for the portable folder via `$APPIMAGE` uses a variable that does not exist anywhere. The branch compiles **only** on Linux/FreeBSD, so macOS and Windows never see it. Arrived with 1.6.2 ("Fix AppImage working directory"). Fix: use `TFileStatus(portableCheck)` directly, as the other branches do. *(Diagnosed on Ztoryc's Linux CI. If upstream's own Linux CI is green, they have a different build path and that should be understood before proposing.)*
25. ⚠️ **Rhubarb is found in the bundle by coincidence** — `toonzlib/thirdparty.cpp`, `autodetectRhubarb()`. Of the three external programs, Rhubarb is the only one whose search never names the bundle: `autodetectFFmpeg()` and `autodetectWhisper()` have an explicit `applicationDirPath() + "/../Resources/..."` line, Rhubarb does not. It is found anyway, but only because `TEnv::getWorkingDirectory()` happens to resolve to `Contents/Resources`. No symptom today — it is an implicit dependency between two things that do not know about each other. Present as alignment between the three functions, not as a defect.

26. ❓ **libgphoto2 plugins are shipped but never found — camera support is silently dead on user machines** (macOS) — `stopmotion/gphotocam.cpp`, the `GPhotoCam::GPhotoCam()` constructor. The packaging script copies `libgphoto2/` and `libgphoto2_port/` into `Contents/Resources`, but nothing sets `IOLIBS`/`CAMLIBS`, and libgphoto2 has its plugin paths **compiled in**, pointing at the build machine's Homebrew prefix. `strings` on the shipped `libgphoto2_port.12.dylib` gives `/usr/local/lib/libgphoto2_port/0.12.2`. On any Mac without Homebrew at that exact path, `gp_abilities_list_load()` and `gp_port_info_list_load()` load nothing: **no camera is ever detected**, with no error and no log — it just looks like no camera is connected. Fix: set both variables from `applicationDirPath()/../Resources` before the two `gp_*_list_load()` calls, discovering the version subdirectory instead of hardcoding it (hardcoding means the next libgphoto2 bump silently kills cameras again). *(Diagnosed, **not verified on Tahoma2D stock**: stock has zero occurrences of `IOLIBS`/`CAMLIBS` and the same in-bundle layout, so it should be affected identically — but Tahoma2D builds its own libgphoto2 fork, which could carry a path patch we have not read. Confirm with a real camera on a clean Mac before proposing.)*

---

### 2.2 — Features that can go upstream as they are

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
12. 🟢 **Per-xsheet In/Out markers** — separate play-range markers for the main xsheet vs each sub-scene. `subscenecommand.cpp`.
13. 🟢 **Keyframe diamond grammar** *(v0.10.0)* — one source of truth for the diamond's colours and detection, so the xsheet, the timeline and the viewer cannot disagree about what kind of key a cell holds, plus a keyframe navigator in the viewer.
14. 🟡 **Function editor usable on its own** *(August 2026)* — it no longer needs a "current curve" to stand up, and the interpolation handling (Auto Bezier, Flat, tangents, Rove) was straightened out.

#### 🔹 Brush / painting

15. 🟢 **AutoFill fill-style picker** — choose the colour AutoFill uses: "Next Style (N+1)" (default) or "Current Style", plus a dynamic palette picker listing all styles as `[N] StyleName` (rebuilds on palette/level change). `toonzrasterbrushtool.h/.cpp`, `tooloptions.cpp`.
16. 🟡 **Tilt that follows the surface, not the screen** *(August 2026)* — with the elbow bent, a tablet's tilt axes do not line up with the screen axes, and the brush ends up shaped by how you are sitting rather than how you are drawing.

#### 🔹 File handling

17. 🟢 **Image-sequence recognition with `-` (hyphen) separator** — `frame-0006.jpg` (common from DaVinci, Blender and other exporters) recognised as a sequence, not a single level; guarded so `my-file.jpg` stays a single level. `common/tsystem/tfilepath.cpp`, `include/tfilepath.h`.

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
