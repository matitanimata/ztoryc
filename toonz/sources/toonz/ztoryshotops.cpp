#include "ztoryshotops.h"

#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshchildlevel.h"
#include "toonz/levelset.h"
#include "toonz/sceneproperties.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tcamera.h"
#include "toonzqt/stageobjectsdata.h"
#include "toonz/fxdag.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"

#include "tfx.h"
#include "tfxattributes.h"
#include "tparamcontainer.h"
#include "tdoubleparam.h"
#include "tdoublekeyframe.h"

#include <string>

namespace ZtoryShotOps {

void syncChildCameraToMain(TXsheet *parentXsh, TXshChildLevel *cl) {
  if (!parentXsh || !cl) return;
  TXsheet *childXsh = cl->getXsheet();
  if (!childXsh) return;
  TStageObjectTree *parentTree = parentXsh->getStageObjectTree();
  TStageObjectTree *childTree  = childXsh->getStageObjectTree();
  int tmpCamId = 0;
  for (int cam = 0; cam < parentTree->getCameraCount();) {
    TStageObject *parentCamera =
        parentTree->getStageObject(TStageObjectId::CameraId(tmpCamId), false);
    if (!parentCamera) { tmpCamId++; continue; }
    if (parentCamera->getCamera()) {
      TStageObject *childObj =
          childTree->getStageObject(TStageObjectId::CameraId(tmpCamId));
      TCamera *childCamera = childObj ? childObj->getCamera() : nullptr;
      if (childCamera) {
        childCamera->setRes(parentCamera->getCamera()->getRes());
        childCamera->setSize(parentCamera->getCamera()->getSize());
      }
    }
    tmpCamId++; cam++;
  }
  childTree->setCurrentCameraId(parentTree->getCurrentCameraId());
}

// Copy res+size of every camera from srcTree to dstTree (matched by camera id).
static bool copyCameraResSize(TStageObjectTree *srcTree,
                              TStageObjectTree *dstTree) {
  bool changed = false;
  int tmpCamId = 0;
  for (int cam = 0; cam < srcTree->getCameraCount();) {
    TStageObject *srcCamObj =
        srcTree->getStageObject(TStageObjectId::CameraId(tmpCamId), false);
    if (!srcCamObj) { tmpCamId++; continue; }
    TCamera *srcCam = srcCamObj->getCamera();
    if (srcCam) {
      TStageObject *dstObj =
          dstTree->getStageObject(TStageObjectId::CameraId(tmpCamId));
      TCamera *dstCam = dstObj ? dstObj->getCamera() : nullptr;
      if (dstCam && (dstCam->getRes() != srcCam->getRes() ||
                     dstCam->getSize() != srcCam->getSize())) {
        dstCam->setRes(srcCam->getRes());
        dstCam->setSize(srcCam->getSize());
        changed = true;
      }
    }
    tmpCamId++; cam++;
  }
  return changed;
}

bool syncAllCamerasFrom(ToonzScene *scene, TXsheet *srcXsh) {
  if (!scene || !srcXsh) return false;
  TStageObjectTree *srcTree = srcXsh->getStageObjectTree();
  TXsheet *mainXsh          = scene->getTopXsheet();

  bool changed = false;

  // 1) Propagate to the main xsheet (unless the edit happened there).
  if (mainXsh && mainXsh != srcXsh)
    changed |= copyCameraResSize(srcTree, mainXsh->getStageObjectTree());

  // 2) Propagate to every sub-scene (skip the source xsheet itself).
  std::vector<TXshLevel *> levels;
  scene->getLevelSet()->listLevels(levels);
  for (TXshLevel *lvl : levels) {
    if (!lvl) continue;
    TXshChildLevel *cl = lvl->getChildLevel();
    if (!cl) continue;
    TXsheet *childXsh = cl->getXsheet();
    if (!childXsh || childXsh == srcXsh) continue;
    changed |= copyCameraResSize(srcTree, childXsh->getStageObjectTree());
  }
  return changed;
}

TDimension xsheetCameraRes(TXsheet *xsh) {
  const TDimension kDefault(1920, 1080);
  if (!xsh) return kDefault;
  TStageObjectTree *tree = xsh->getStageObjectTree();
  TStageObject *camObj =
      tree->getStageObject(tree->getCurrentCameraId(), false);
  TCamera *cam = camObj ? camObj->getCamera() : nullptr;
  if (!cam) return kDefault;
  TDimension res = cam->getRes();
  if (res.lx <= 0 || res.ly <= 0) return kDefault;
  return res;
}

double xsheetCameraAspect(TXsheet *xsh) {
  TDimension res = xsheetCameraRes(xsh);
  return (double)res.lx / (double)res.ly;
}

TDimension cameraRes(ToonzScene *scene) {
  const TDimension kDefault(1920, 1080);
  if (!scene) return kDefault;
  return xsheetCameraRes(scene->getTopXsheet());
}

double cameraAspect(ToonzScene *scene) {
  TDimension res = cameraRes(scene);
  return (double)res.lx / (double)res.ly;
}

void cloneChildToPosition(int srcCol, int dstCol) {
  TApp *app          = TApp::instance();
  ToonzScene *scene  = app->getCurrentScene()->getScene();
  TXsheet *xsh       = app->getCurrentXsheet()->getXsheet();
  TXshColumn *column = xsh->getColumn(srcCol);
  if (!column) return;
  TXshLevelColumn *lcolumn = column->getLevelColumn();
  if (!lcolumn) return;
  int r0 = 0, r1 = -1;
  lcolumn->getRange(r0, r1);
  if (r0 > r1) return;
  TXshCell cell = lcolumn->getCell(r0);
  if (cell.isEmpty()) return;
  TXshChildLevel *childLevel = cell.m_level->getChildLevel();
  if (!childLevel) return;
  TXsheet *childXsh = childLevel->getXsheet();

  // Inserisci colonna vuota alla posizione target
  xsh->insertColumn(dstCol);

  // Crea nuovo child level clone
  ChildStack *childStack = scene->getChildStack();
  TXshChildLevel *newChildLevel = childStack->createChild(0, dstCol);
  TXsheet *newChildXsh = newChildLevel->getXsheet();

  // Copia contenuto colonne.  NON instradiamo la camera tramite
  // StageObjectsData: ogni child xsheet creato da createChild() ha già una
  // Camera 0 di default, quindi restoreCamera() la trova "occupata" e crea una
  // Camera 1 FANTASMA con i keyframe, lasciando la Camera 0 (quella usata per il
  // rendering) senza animazione — ecco perché lo shot clonato perdeva le chiavi
  // di camera.  La camera la copiamo a mano sotto, direttamente Camera 0→Camera 0.
  std::set<int> indices;
  for (int i = 0; i < childXsh->getColumnCount(); i++) indices.insert(i);
  std::vector<TStageObjectId> ids;
  for (int i : indices) ids.push_back(TStageObjectId::ColumnId(i));
  StageObjectsData *data = new StageObjectsData();
  data->storeObjects(ids, childXsh, 0);
  data->storeColumnFxs(indices, childXsh, 0);
  std::list<int> restoredSplineIds;
  QMap<TStageObjectId, TStageObjectId> idTable;
  QMap<TFx *, TFx *> fxTable;
  data->restoreObjects(indices, restoredSplineIds, newChildXsh,
                       StageObjectsData::eDoClone, idTable, fxTable);
  delete data;

  // Stage objects non-colonna (camera, pegbar): copia params (keyframe/
  // animazione) sull'oggetto con lo STESSO id già esistente nel clone.  Le
  // colonne sono già state ricreate da restoreObjects(); qui copiamo camera e
  // pegbar — che altrimenti andrebbero persi (la camera finirebbe su una Camera
  // fantasma via restoreCamera; i pegbar non verrebbero animati).  Mirror della
  // logica stock cloneXsheetTStageObjectTree() (in namespace anonimo, non
  // richiamabile da qui).
  {
    TStageObjectTree *srcTree = childXsh->getStageObjectTree();
    for (int i = 0; i < srcTree->getStageObjectCount(); i++) {
      TStageObject *srcObj = srcTree->getStageObject(i);
      TStageObjectId id    = srcObj->getId();
      if (id.isColumn()) continue;  // colonne già gestite da restoreObjects
      TStageObject *dstObj = newChildXsh->getStageObject(id);
      if (!dstObj) continue;
      if (id.isCamera()) *(dstObj->getCamera()) = *(srcObj->getCamera());
      TStageObjectParams *p = srcObj->getParams();
      dstObj->assignParams(p, /*doParametersClone=*/true);
      delete p;
      dstObj->setParent(childXsh->getStageObjectParent(id));
    }
  }

  newChildXsh->getFxDag()->getXsheetFx()->getAttributes()->setDagNodePos(
      childXsh->getFxDag()->getXsheetFx()->getAttributes()->getDagNodePos());
  newChildXsh->updateFrameCount();

  // Rimuovi cella creata da createChild e copia celle originali
  xsh->removeCells(0, dstCol);
  for (int r = r0; r <= r1; r++) {
    TXshCell c = lcolumn->getCell(r);
    if (c.isEmpty()) continue;
    c.m_level = newChildLevel;
    xsh->setCell(r, dstCol, c);
  }

  xsh->updateFrameCount();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentXsheet()->notifyXsheetChanged();
}

void pasteSharedClip(const std::vector<ZtoryClipEntry> &clip, int insertCol,
                     TXsheet *xsh, ToonzScene *scene) {
  for (int ci = 0; ci < (int)clip.size(); ci++) {
    int pos = insertCol + ci;
    const ZtoryClipEntry &ce = clip[ci];
    if (ce.isCut && ce.srcCol == -1) {
      // Immediate cut: original already gone, re-insert via saved cutLevel.
      xsh->insertColumn(pos);
      if (ce.cutLevel) {
        for (int r = 0; r < ce.duration; r++)
          xsh->setCell(r, pos, TXshCell(ce.cutLevel, TFrameId(r + 1)));
      } else if (scene) {
        TXshLevel *xl = scene->createNewLevel(CHILD_XSHLEVEL);
        if (xl && xl->getChildLevel()) {
          TXshChildLevel *cl = xl->getChildLevel();
          syncChildCameraToMain(xsh, cl);  // new empty sub → match main camera
          for (int r = 0; r < ce.duration; r++)
            xsh->setCell(r, pos, TXshCell(cl, TFrameId(r + 1)));
        }
      }
    } else if (ce.isClone || ce.isCut) {
      // Clone or deferred cut: clone from source column.
      int srcCol = ce.srcCol;
      for (int cj = 0; cj < ci; cj++) if (insertCol + cj <= srcCol) srcCol++;
      cloneChildToPosition(srcCol, pos);
    } else {
      // Copy: share the same TXshChildLevel (shared sub-scene).
      int srcCol = ce.srcCol;
      for (int cj = 0; cj < ci; cj++) if (insertCol + cj <= srcCol) srcCol++;
      TXshColumn *srcColumn = xsh->getColumn(srcCol);
      if (srcColumn) {
        int r0 = 0, r1 = 0;
        srcColumn->getRange(r0, r1);
        xsh->insertColumn(pos);
        for (int r = r0; r <= r1; r++) {
          TXshCell cell = xsh->getCell(r, srcCol >= pos ? srcCol + 1 : srcCol);
          if (!cell.isEmpty()) xsh->setCell(r, pos, cell);
        }
      }
    }
  }
}

int colDuration(TXsheet *xsh, int col) {
  TXshColumn *c = xsh ? xsh->getColumn(col) : nullptr;
  if (!c) return 24;
  int r0 = 0, r1 = 0;
  c->getRange(r0, r1);
  return (r1 >= r0) ? r1 - r0 + 1 : 24;
}

// Row count (= T/2) of the SoundText note column named `name` inside a
// sub-scene.  Same probe as ztoryanimatic.cpp's xdNoteHalfCount(): the XD-in /
// XD-out columns are the persisted source of truth for a shot's cross-dissolve
// halves (they survive save/reload/undo, unlike the transient main frameIds).
static int xdNoteRows(TXsheet *subXsh, const char *name) {
  if (!subXsh) return 0;
  for (int c = 0; c < subXsh->getColumnCount(); c++) {
    TXshColumn *col = subXsh->getColumn(c);
    if (!col || !col->getSoundTextColumn()) continue;
    std::string colName =
        subXsh->getStageObject(subXsh->getColumnObjectId(c))->getName();
    if (colName == name) {
      int r0 = 0, r1 = 0;
      col->getRange(r0, r1);
      return (r1 >= r0) ? (r1 - r0 + 1) : 0;
    }
  }
  return 0;
}

int xdInHeadOffset(TXsheet *subXsh) { return xdNoteRows(subXsh, "XD-in"); }

int xdOutTailCount(TXsheet *subXsh) { return xdNoteRows(subXsh, "XD-out"); }

//-----------------------------------------------------------------------------
// Real cross-dissolve rendering — CELL side (see header).  We only EXPOSE the
// overlap material on the two shot columns so each has content to render during
// the window; the actual blend is injected at RENDER time in scenefx.cpp
// (FxBuilder::makePF(TXsheetFx)), NOT as a persisted fx node.  Keeping no fx in
// the dag is what makes this safe across undo / save / room-switch / chaining.
// All three functions key off the persisted XD-in / XD-out note columns, so the
// overlap is self-healing across resequence / reload / undo.
//-----------------------------------------------------------------------------

// Return the child-level sub-scene exposed in a main-xsheet column, or nullptr.
static TXsheet *columnSubXsheet(TXsheet *mainXsh, int col) {
  TXshColumn *column = mainXsh->getColumn(col);
  if (!column || column->isEmpty()) return nullptr;
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1);
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = mainXsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel())
      return cell.m_level->getChildLevel()->getXsheet();
  }
  return nullptr;
}

bool shotTrueSpan(TXsheet *mainXsh, int col, int &startOut, int &durationOut) {
  if (!mainXsh) return false;
  TXshColumn *column = mainXsh->getColumn(col);
  if (!column || column->isEmpty()) return false;
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1);  // includes trailing stop + any overlap cells
  if (r1 < r0) return false;

  TXsheet *sub = columnSubXsheet(mainXsh, col);
  int headHalf = xdInHeadOffset(sub);
  int tailHalf = xdOutTailCount(sub);

  // Head-extra: the first headHalf rows are dissolve head-hold copies (frameIds
  // 1..headHalf); real content starts at frameId 1+headHalf.  Only skip them
  // when they are actually exposed (the leading cell is a head-hold copy) — if
  // the shot has an XD-in column but no exposed head-extra yet, r0 is already
  // the real start.
  int trueStart = r0;
  if (headHalf > 0) {
    TXshCell c0 = mainXsh->getCell(r0, col);
    if (!c0.isEmpty() && !c0.getFrameId().isStopFrame() &&
        c0.getFrameId().getNumber() <= headHalf)
      trueStart = r0 + headHalf;
  }

  // Tail dissolve layout is [real][tailHalf tail-extra][stop]; a plain shot is
  // [real][stop].  Exclude the tail-extra (+ its stop) or the plain stop.
  TXshCell last     = mainXsh->getCell(r1, col);
  bool lastIsStop   = !last.isEmpty() && last.getFrameId().isStopFrame();
  int trueEndInclusive;
  if (tailHalf > 0 && lastIsStop && (r1 - 1 - tailHalf) >= trueStart)
    trueEndInclusive = r1 - 1 - tailHalf;
  else
    trueEndInclusive = lastIsStop ? r1 - 1 : r1;

  startOut    = trueStart;
  durationOut = trueEndInclusive - trueStart + 1;
  return durationOut > 0;
}

// List the main-xsheet columns that carry a shot (child-level), in order.
static std::vector<int> shotColumns(TXsheet *mainXsh) {
  std::vector<int> cols;
  int n = mainXsh->getColumnCount();
  for (int c = 0; c < n; c++)
    if (columnSubXsheet(mainXsh, c)) cols.push_back(c);
  return cols;
}

void teardownCrossDissolves(TXsheet *mainXsh) {
  if (!mainXsh) return;
  // For each consecutive shot pair with a cross-dissolve, strip the overlap
  // cells so getRange() reports true durations again.  Detection is purely
  // geometric (no fx): X (colB's real start) is the first colB row whose frameId
  // exceeds the head-hold count, so head-extra = rows [r0B, X) and colA's
  // tail-extra + its stop = rows [X, X+half].  If the overlap is not currently
  // exposed the loops simply clear the (already correct) boundary stop, which
  // resequence re-lays — so this is safe to call unconditionally.
  std::vector<int> cols = shotColumns(mainXsh);
  for (size_t k = 0; k + 1 < cols.size(); k++) {
    int colA = cols[k], colB = cols[k + 1];
    int half = xdOutTailCount(columnSubXsheet(mainXsh, colA));
    if (half <= 0 || half != xdInHeadOffset(columnSubXsheet(mainXsh, colB)))
      continue;

    int r0B = 0, r1B = 0;
    mainXsh->getColumn(colB)->getRange(r0B, r1B);
    int X = r0B;
    for (int r = r0B; r <= r1B; r++) {
      TXshCell cell = mainXsh->getCell(r, colB);
      if (!cell.isEmpty() && !cell.getFrameId().isStopFrame() &&
          cell.getFrameId().getNumber() > half) {
        X = r;
        break;
      }
    }
    if (X > r0B) mainXsh->clearCells(r0B, colB, X - r0B);  // colB head-extra
    mainXsh->clearCells(X, colA, half + 1);  // colA tail-extra + its stop
  }
}

void applyCrossDissolves(TXsheet *mainXsh, const std::vector<ShotLayout> &shots) {
  if (!mainXsh) return;

  // Expose the overlap material for every consecutive shot pair that carries a
  // cross-dissolve (half = XD-out(A) == XD-in(B) > 0), WITHOUT shifting any shot
  // and WITHOUT touching the fx dag.  The blend itself is injected at render
  // time (scenefx.cpp) from these cells.
  for (size_t i = 0; i + 1 < shots.size(); i++) {
    const ShotLayout &A = shots[i];
    const ShotLayout &B = shots[i + 1];
    int half = xdOutTailCount(A.subXsh);
    if (half <= 0 || half != xdInHeadOffset(B.subXsh)) continue;

    int X = B.startFrame;                         // cut row (B start = A end)
    if (A.startFrame + A.duration != X) continue;  // not adjacent — skip

    TXshChildLevel *clA = nullptr, *clB = nullptr;
    {
      TXshCell a = mainXsh->getCell(A.startFrame, A.col);
      TXshCell b = mainXsh->getCell(B.startFrame, B.col);
      if (!a.isEmpty() && a.m_level) clA = a.m_level->getChildLevel();
      if (!b.isEmpty() && b.m_level) clB = b.m_level->getChildLevel();
    }
    if (!clA || !clB) continue;

    // colA tail-extra: A CONTINUES its own frames across the second half of the
    // window at rows [X .. X+half-1] (overwriting the boundary stop resequence
    // put at X — no 1-frame gap), then a fresh stop at X+half.  onTransitionChanged
    // appended `half` hold copies to sub-scene A, so those frameIds render.
    TXshCell aLast = mainXsh->getCell(X - 1, A.col);
    int aLastId    = aLast.isEmpty() ? 1 : aLast.getFrameId().getNumber();
    for (int j = 0; j < half; j++)
      mainXsh->setCell(X + j, A.col, TXshCell(clA, TFrameId(aLastId + 1 + j)));
    mainXsh->setCell(X + half, A.col,
                     TXshCell(clA, TFrameId(TFrameId::STOP_FRAME)));

    // colB head-extra: sub-scene head-hold copies are frames 1..half; lay them
    // BEFORE B's start, in rows [X-half .. X-1].
    for (int j = 0; j < half; j++)
      mainXsh->setCell(X - half + j, B.col, TXshCell(clB, TFrameId(1 + j)));
  }
}

}  // namespace ZtoryShotOps
