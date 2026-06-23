#pragma once

//=============================================================================
// ZtoryShotOps — shared shot/sub-scene xsheet operations
//-----------------------------------------------------------------------------
// Pure xsheet/scene logic shared by StoryboardPanel (BOARD) and
// ZtoryAnimaticPanel (ANIMATIC).  These functions operate ONLY on the
// xsheet/scene — no panel state, no selection, no UI — so both panels can
// delegate to a single implementation instead of keeping drifting copies.
//
// The panels keep their own slots (onCopyShot/onCopyShots, …) because the
// selection source (shot index vs xsheet column), the clipboard mirroring and
// the undo wrapping differ; only the underlying column manipulation is shared
// here.
//=============================================================================

#include "ztorymodel.h"  // ZtoryClipEntry
#include "tgeometry.h"   // TDimension

#include <vector>

class TXsheet;
class ToonzScene;
class TXshChildLevel;

namespace ZtoryShotOps {

// Force a freshly-created sub-scene's camera(s) to match the MAIN xsheet's
// camera (resolution + size).  Only safe for BRAND-NEW empty sub-scenes — never
// call on a clone with camera animation (changing size reframes keyframes).
void syncChildCameraToMain(TXsheet *parentXsh, TXshChildLevel *cl);

// Propagate the camera resolution + size of srcXsh to EVERY other xsheet in the
// scene (the main xsheet and all sub-scenes), matching cameras by id.  Tahoma2D
// treats the camera framing as a single scene-wide setting that applies to the
// main xsheet and all sub-scenes, so a Camera Settings change made from any
// xsheet (main OR inside a shot) must reach all the others.  Only res+size are
// copied — per-shot camera-move keyframes (transform) are left untouched.
// Returns true if any camera was actually modified.
bool syncAllCamerasFrom(ToonzScene *scene, TXsheet *srcXsh);

// Clone the sub-scene in column srcCol into a new (independent) sub-scene at
// dstCol.  Inserts a column at dstCol, deep-clones the child xsheet (columns,
// fx, camera, pegbars — preserving camera keyframes) and re-exposes the cells.
void cloneChildToPosition(int srcCol, int dstCol);

// Paste shared-clipboard entries (ZtoryClipEntry) into the xsheet starting at
// insertCol.  Handles copy (shared sub-scene), clone (independent) and cut
// (re-insert via kept-alive level, or a new empty sub-scene).
// Caller must run xsh->updateFrameCount(), resequenceXsheet(), refresh after.
void pasteSharedClip(const std::vector<ZtoryClipEntry> &clip, int insertCol,
                     TXsheet *xsh, ToonzScene *scene);

// Cell count of a column (r1 - r0 + 1), or 24 if empty/invalid.
int colDuration(TXsheet *xsh, int col);

// Aspect ratio (width/height) of the scene camera, read from the MAIN xsheet
// camera resolution.  Board previews, the PDF export and the Thumbnail room
// grid use this so a non-16:9 camera (e.g. square) is framed correctly instead
// of being letterboxed inside a fixed 16:9 box.  Falls back to 16:9.
double cameraAspect(ToonzScene *scene);

// Pixel resolution of the scene camera, read from the MAIN xsheet camera.
// Used by the Thumbnail room export to rasterize panels at the real shot
// framing.  Falls back to 1920x1080.
TDimension cameraRes(ToonzScene *scene);

}  // namespace ZtoryShotOps
