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

#include "tfx.h"
#include "tfxattributes.h"

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

double cameraAspect(ToonzScene *scene) {
  const double kDefault = 16.0 / 9.0;
  if (!scene) return kDefault;
  TXsheet *xsh = scene->getTopXsheet();
  if (!xsh) return kDefault;
  TStageObjectTree *tree = xsh->getStageObjectTree();
  TStageObject *camObj =
      tree->getStageObject(tree->getCurrentCameraId(), false);
  TCamera *cam = camObj ? camObj->getCamera() : nullptr;
  if (!cam) return kDefault;
  TDimension res = cam->getRes();
  if (res.lx <= 0 || res.ly <= 0) return kDefault;
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

}  // namespace ZtoryShotOps
