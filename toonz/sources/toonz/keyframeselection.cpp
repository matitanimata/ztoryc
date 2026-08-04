

#include "keyframeselection.h"

// Tnz6 includes
#include "keyframedata.h"
#include "cellkeyframedata.h"
#include "timestretchpopup.h"
#include "tapp.h"
#include "menubarcommandids.h"
#include "xsheetviewer.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/stageobjectutil.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobject.h"
#include "toonz/txshcolumn.h"

// TnzQt includes
#include "historytypes.h"

// TnzCore includes
#include "tundo.h"

// Qt includes
#include <QApplication>
#include <QClipboard>

#include <climits>
#include <cmath>

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

struct PegbarArgument {
  TStageObject *m_stageObject;
  std::set<int> m_frames;
};

//-----------------------------------------------------------------------------

bool shiftKeyframesWithoutUndo(int r0, int r1, int c0, int c1, bool cut,
                               bool shiftFollowing) {
  int delta = cut ? -(r1 - r0 + 1) : r1 - r0 + 1;
  if (delta == 0) return false;
  TXsheet *xsh   = TApp::instance()->getCurrentXsheet()->getXsheet();
  bool isShifted = false;
  int x;
  for (x = c0; x <= c1; x++) {
    TStageObject *stObj = xsh->getStageObject(
        x >= 0 ? xsh->getColumnObjectId(x)
               : TStageObjectId::CameraId(xsh->getCameraColumnIndex()));
    std::set<int> keyToShift;
    int kr0, kr1;
    stObj->getKeyframeRange(kr0, kr1);
    int i = r0;
    while (i <= kr1) {
      if (stObj->isKeyframe(i)) {
        keyToShift.insert(i);
        if (!shiftFollowing) break;
      }
      i++;
    }
    isShifted = stObj->moveKeyframes(keyToShift, delta);
  }
  xsh->updateNonZeroDrawingNumberCellsBox(r0, c0, c1);
  return isShifted;
}

//-----------------------------------------------------------------------------

void copyKeyframesWithoutUndo(
    std::set<TKeyframeSelection::Position> *positions) {
  QClipboard *clipboard = QApplication::clipboard();
  TXsheet *xsh          = TApp::instance()->getCurrentXsheet()->getXsheet();
  TKeyframeData *data   = new TKeyframeData();
  data->setKeyframes(*positions, xsh);
  clipboard->setMimeData(data, QClipboard::Clipboard);
}

//-----------------------------------------------------------------------------

bool pasteKeyframesWithoutUndo(
    const TKeyframeData *data,
    std::set<TKeyframeSelection::Position> *positions) {
  if (!data || positions->empty()) return false;
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  if (!data->getKeyframes(*positions, xsh)) return false;
  return true;
}

//-----------------------------------------------------------------------------

bool deleteKeyframesWithoutUndo(
    std::set<TKeyframeSelection::Position> *positions) {
  TApp *app = TApp::instance();
  assert(app);
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  TStageObjectId cameraId =
      TStageObjectId::CameraId(xsh->getCameraColumnIndex());

  if (positions->empty()) return false;

  std::set<TKeyframeSelection::Position>::iterator it = positions->begin();
  bool areAllColumnLocked                             = true;
  for (; it != positions->end(); ++it) {
    int row              = it->first;
    int col              = it->second;
    TStageObject *pegbar = xsh->getStageObject(col >= 0 ? xsh->getColumnObjectId(col) : cameraId);
    if (xsh->getColumn(col) && xsh->getColumn(col)->isLocked()) continue;
    areAllColumnLocked = false;
    assert(pegbar);
    pegbar->removeKeyframeWithoutUndo(row);

    // Move frame center back to origin -- but ONLY once the object has no
    // keyframes left. Doing it after every single deleted key MOVES THE OBJECT
    // while its channel values stay exactly the same, which reads as the
    // animation silently drifting.
    //
    // setCenter(frame, center, resetOrigin) forwards to the four-argument form
    // as setCenter(frame, center, CENTER, resetOrigin), so it writes
    // m_frameCenter = center -- a different value from the one it held -- and
    // then recomputes m_offset from a placement it has just zeroed. Neither is
    // a keyframed channel, and both feed the placement directly: on a path
    // `position = splinePoint - m_frameCenter`, and everywhere `pos = m_offset
    // + position`. That is why the object shifts with a spline AND without one.
    //
    // Reported by Franco 2026-08-04 on MaggiolataZombie/sh260: deleting the
    // leading keys of a run to start from the last key's position dropped the
    // object, with identical values at the surviving key.
    //
    // The reset itself is upstream's (baecf7504, "Fix resetting center and
    // offset on key delete"): keeping it for the case it was written for --
    // the object has no animation left, so the bookkeeping should go back to
    // the origin -- and not running it while keys remain.
    pegbar->resetFrameCenterIfUnanimated(row);
  }
  if (areAllColumnLocked) return false;

  positions->clear();
  return true;
}

//=============================================================================
//  PasteKeyframesUndo
//-----------------------------------------------------------------------------

class PasteKeyframesUndo final : public TUndo {
  TKeyframeSelection *m_selection;
  QMimeData *m_newData;
  QMimeData *m_oldData;
  int m_r0, m_r1, m_c0, m_c1;

public:
  PasteKeyframesUndo(TKeyframeSelection *selection, QMimeData *newData,
                     QMimeData *oldData, int r0, int r1, int c0, int c1)
      : m_selection(selection)
      , m_newData(newData)
      , m_oldData(oldData)
      , m_r0(r0)
      , m_r1(r1)
      , m_c0(c0)
      , m_c1(c1) {}

  ~PasteKeyframesUndo() {
    delete m_selection;
    delete m_newData;
    delete m_oldData;
  }
  // data->xsh
  void setXshFromData(QMimeData *data) const {
    const TKeyframeData *keyframeData = dynamic_cast<TKeyframeData *>(data);
    if (keyframeData) {
      TKeyframeSelection *selection =
          new TKeyframeSelection(m_selection->getSelection());
      pasteKeyframesWithoutUndo(keyframeData, &selection->getSelection());
    }
  }
  void undo() const override {
    // Delete merged data
    TKeyframeSelection *selection =
        new TKeyframeSelection(m_selection->getSelection());
    deleteKeyframesWithoutUndo(&selection->getSelection());
    if (-(m_r1 - m_r0 + 1) != 0)
      shiftKeyframesWithoutUndo(m_r0, m_r1, m_c0, m_c1, true, true);
    if (m_oldData) setXshFromData(m_oldData);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    if (m_r1 - m_r0 + 1 != 0)
      shiftKeyframesWithoutUndo(m_r0, m_r1, m_c0, m_c1, false, true);
    // Delete merged data
    setXshFromData(m_newData);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Paste Key Frames");
  }
};

//=============================================================================
//  DeleteKeyframesUndo
//-----------------------------------------------------------------------------

class DeleteKeyframesUndo final : public TUndo {
  TKeyframeSelection *m_selection;
  QMimeData *m_data;
  int m_r0, m_r1, m_c0, m_c1;

public:
  DeleteKeyframesUndo(TKeyframeSelection *selection, QMimeData *data, int r0,
                      int r1, int c0, int c1)
      : m_selection(selection)
      , m_data(data)
      , m_r0(r0)
      , m_r1(r1)
      , m_c0(c0)
      , m_c1(c1) {}

  ~DeleteKeyframesUndo() {
    delete m_selection;
    delete m_data;
  }

  void undo() const override {
    const TKeyframeData *keyframeData = dynamic_cast<TKeyframeData *>(m_data);
    if (m_r1 - m_r0 + 1 != 0)
      shiftKeyframesWithoutUndo(m_r0, m_r1, m_c0, m_c1, false, true);
    if (keyframeData)
      pasteKeyframesWithoutUndo(keyframeData, &m_selection->getSelection());
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    TKeyframeSelection *tempSelection =
        new TKeyframeSelection(m_selection->getSelection());
    deleteKeyframesWithoutUndo(&tempSelection->getSelection());
    if (m_r1 - m_r0 + 1 != 0)
      shiftKeyframesWithoutUndo(m_r0, m_r1, m_c0, m_c1, true, true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Delete Key Frames");
  }
};

//=============================================================================
//  ShiftKeyframesUndo
//-----------------------------------------------------------------------------

class ShiftKeyframesUndo final : public TUndo {
  int m_r0, m_r1, m_c0, m_c1;
  bool m_shiftFollowing;

public:
  ShiftKeyframesUndo(int r0, int r1, int c0, int c1, bool shiftFollowing)
      : m_r0(r0)
      , m_r1(r1)
      , m_c0(c0)
      , m_c1(c1)
      , m_shiftFollowing(shiftFollowing) {}

  ~ShiftKeyframesUndo() {}
  void undo() const override {
    if (m_r0 != m_r1) {
      int r1adj  = m_r0 < m_r1 ? m_r1 - 1 : m_r0 + (m_r0 - m_r1) - 1;
      int rshift = m_r0 < m_r1 ? 0 : -(r1adj - m_r0 + 1);
      bool cut   = m_r0 < m_r1 ? true : false;

      shiftKeyframesWithoutUndo(m_r0 + rshift, r1adj + rshift, m_c0, m_c1, cut,
                                m_shiftFollowing);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    if (m_r0 != m_r1) {
      int r1adj = m_r0 < m_r1 ? m_r1 - 1 : m_r0 + (m_r0 - m_r1) - 1;
      bool cut  = m_r0 < m_r1 ? false : true;

      shiftKeyframesWithoutUndo(m_r0, r1adj, m_c0, m_c1, cut, m_shiftFollowing);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    if (m_r0 < m_r1) return QObject::tr("Shift Key Frames Down");
    return QObject::tr("Shift Key Frames Up");
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// TKeyframeSelection
//-----------------------------------------------------------------------------

void TKeyframeSelection::enableCommands() {
  enableCommand(this, MI_Copy, &TKeyframeSelection::copyKeyframes);
  enableCommand(this, MI_Paste, &TKeyframeSelection::pasteKeyframes);
  enableCommand(this, MI_Cut, &TKeyframeSelection::cutKeyframes);
  enableCommand(this, MI_Clear, &TKeyframeSelection::deleteKeyframes);
  enableCommand(this, MI_ShiftKeyframesDown,
                &TKeyframeSelection::shiftKeyframesDown);
  enableCommand(this, MI_ShiftKeyframesUp,
                &TKeyframeSelection::shiftKeyframesUp);
  // Key-only timing ops (Ztoryc)
  enableCommand(this, MI_Reverse, &TKeyframeSelection::reverseKeyframes);
  enableCommand(this, MI_Swing, &TKeyframeSelection::swingKeyframes);
  enableCommand(this, MI_Rollup, &TKeyframeSelection::rollupKeyframes);
  enableCommand(this, MI_Rolldown, &TKeyframeSelection::rolldownKeyframes);
  enableCommand(this, MI_TimeStretch, &TKeyframeSelection::openTimeStretchPopup);
  enableCommand(this, MI_IncreaseStep, &TKeyframeSelection::increaseKeyframes);
  enableCommand(this, MI_DecreaseStep, &TKeyframeSelection::decreaseKeyframes);
}

//-----------------------------------------------------------------------------

int TKeyframeSelection::getFirstRow() const {
  if (isEmpty()) return 0;
  return m_positions.begin()->first;
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::unselectLockedColumn() {
  TApp *app = TApp::instance();
  assert(app);
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  std::set<Position> positions;
  std::set<Position>::iterator it;

  for (it = m_positions.begin(); it != m_positions.end(); ++it) {
    int col = it->second;
    if (xsh->getColumn(col) && xsh->getColumn(col)->isLocked()) continue;
    positions.insert(*it);
  }
  m_positions.swap(positions);
}

//-----------------------------------------------------------------------------

bool TKeyframeSelection::select(const TSelection *s) {
  if (const TKeyframeSelection *ss =
          dynamic_cast<const TKeyframeSelection *>(s)) {
    std::set<Position> pos(ss->m_positions);
    m_positions.swap(pos);
    makeCurrent();
    return true;
  } else
    return false;
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::setKeyframes() {
  TApp *app                   = TApp::instance();
  TXsheetHandle *xsheetHandle = app->getCurrentXsheet();
  TXsheet *xsh                = xsheetHandle->getXsheet();
  TStageObjectId cameraId =
      TStageObjectId::CameraId(xsh->getCameraColumnIndex());
  if (isEmpty()) return;
  Position pos         = *m_positions.begin();
  int row              = pos.first;
  int col              = pos.second;
  TStageObjectId id    = col < 0 ? cameraId : xsh->getColumnObjectId(col);
  TStageObject *pegbar = xsh->getStageObject(id);
  if (!pegbar) return;
  if (pegbar->isKeyframe(row)) {
    TStageObject::Keyframe key = pegbar->getKeyframe(row);

    pegbar->removeKeyframeWithoutUndo(row);

    TPointD center, offset;
    pegbar->getCenterAndOffset(center, offset);
    pegbar->resetFrameCenterIfUnanimated(row);

    UndoRemoveKeyFrame *undo =
        new UndoRemoveKeyFrame(id, row, key, center, offset, xsheetHandle);
    undo->setObjectHandle(app->getCurrentObject());
    TUndoManager::manager()->add(undo);
  } else {
    pegbar->setKeyframeWithoutUndo(row);
    UndoSetKeyFrame *undo = new UndoSetKeyFrame(id, row, xsheetHandle);
    undo->setObjectHandle(app->getCurrentObject());
    TUndoManager::manager()->add(undo);
  }
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentObject()->notifyObjectIdChanged(false);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::copyKeyframes() {
  if (isEmpty()) return;
  copyKeyframesWithoutUndo(&m_positions);
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::pasteKeyframes() {
  pasteKeyframesWithShift(0, 0, 0, -1);
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::deleteKeyframes() {
  deleteKeyframesWithShift(0, -1, 0, -1);
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::cutKeyframes() {
  copyKeyframes();
  deleteKeyframesWithShift(0, -1, 0, -1);
}

//-----------------------------------------------------------------------------
// Reverse Keyframes (key-only) — mirror the selected keys in [r0,r1] about the
// range centre (r -> r0+r1-r), per column. Involution: undo == redo. Mirrors
// TCellSelection::reverseCells but on TStageObject keyframes.
//-----------------------------------------------------------------------------

namespace {

void doReverseKeyframes(int r0, int r1, const std::set<int> &cols) {
  if (r1 <= r0) return;
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObjectId cameraId =
      TStageObjectId::CameraId(xsh->getCameraColumnIndex());
  for (int c : cols) {
    TStageObjectId id = c >= 0 ? xsh->getColumnObjectId(c) : cameraId;
    TStageObject *obj = xsh->getStageObject(id);
    if (!obj) continue;
    std::map<int, TStageObject::Keyframe> keys;
    for (int r = r0; r <= r1; r++)
      if (obj->isKeyframe(r)) keys[r] = obj->getKeyframe(r);
    if (keys.empty()) continue;
    for (auto const &kv : keys) obj->removeKeyframeWithoutUndo(kv.first);
    for (auto const &kv : keys)
      obj->setKeyframeWithoutUndo(r0 + r1 - kv.first, kv.second);
  }
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

class KeyframeReverseUndo final : public TUndo {
  int m_r0, m_r1;
  std::set<int> m_cols;

public:
  KeyframeReverseUndo(int r0, int r1, const std::set<int> &cols)
      : m_r0(r0), m_r1(r1), m_cols(cols) {}

  void redo() const override { doReverseKeyframes(m_r0, m_r1, m_cols); }
  void undo() const override { redo(); }  // mirror is its own inverse

  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Reverse Keyframes");
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

}  // namespace

void TKeyframeSelection::reverseKeyframes() {
  unselectLockedColumn();
  if (isEmpty()) return;

  int r0 = INT_MAX, r1 = -1;
  std::set<int> cols;
  for (auto const &p : m_positions) {
    r0 = std::min(r0, p.first);
    r1 = std::max(r1, p.first);
    cols.insert(p.second);
  }
  if (r1 <= r0) return;  // nothing to mirror with a single row

  TUndo *undo = new KeyframeReverseUndo(r0, r1, cols);
  TUndoManager::manager()->add(undo);
  undo->redo();
}

//-----------------------------------------------------------------------------
// Swing Keyframes (key-only) — append the keys [r0,r1-1] mirrored past r1, so
// the motion ping-pongs (A B C B A). New keys land in the tail (r1+1 ..
// 2*r1-r0); the undo restores whatever was there before.
//-----------------------------------------------------------------------------

namespace {

class KeyframeSwingUndo final : public TUndo {
  int m_r0, m_r1;
  std::set<int> m_cols;
  // keys overwritten in the tail span, per column (for a faithful undo)
  std::map<int, std::map<int, TStageObject::Keyframe>> m_tailSnapshot;

  static TStageObject *stageObj(TXsheet *xsh, int c) {
    TStageObjectId id =
        c >= 0 ? xsh->getColumnObjectId(c)
               : TStageObjectId::CameraId(xsh->getCameraColumnIndex());
    return xsh->getStageObject(id);
  }

public:
  KeyframeSwingUndo(int r0, int r1, const std::set<int> &cols)
      : m_r0(r0), m_r1(r1), m_cols(cols) {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int t0 = m_r1 + 1, t1 = 2 * m_r1 - m_r0;
    for (int c : cols) {
      TStageObject *obj = stageObj(xsh, c);
      if (!obj) continue;
      std::map<int, TStageObject::Keyframe> snap;
      for (int r = t0; r <= t1; r++)
        if (obj->isKeyframe(r)) snap[r] = obj->getKeyframe(r);
      m_tailSnapshot[c] = snap;
    }
  }

  void redo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c : m_cols) {
      TStageObject *obj = stageObj(xsh, c);
      if (!obj) continue;
      for (int r = m_r0; r <= m_r1 - 1; r++)
        if (obj->isKeyframe(r))
          obj->setKeyframeWithoutUndo(2 * m_r1 - r, obj->getKeyframe(r));
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int t0 = m_r1 + 1, t1 = 2 * m_r1 - m_r0;
    for (int c : m_cols) {
      TStageObject *obj = stageObj(xsh, c);
      if (!obj) continue;
      for (int r = t0; r <= t1; r++)
        if (obj->isKeyframe(r)) obj->removeKeyframeWithoutUndo(r);
      auto it = m_tailSnapshot.find(c);
      if (it != m_tailSnapshot.end())
        for (auto const &kv : it->second)
          obj->setKeyframeWithoutUndo(kv.first, kv.second);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override { return QObject::tr("Swing Keyframes"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

}  // namespace

void TKeyframeSelection::swingKeyframes() {
  unselectLockedColumn();
  if (isEmpty()) return;

  int r0 = INT_MAX, r1 = -1;
  std::set<int> cols;
  for (auto const &p : m_positions) {
    r0 = std::min(r0, p.first);
    r1 = std::max(r1, p.first);
    cols.insert(p.second);
  }
  if (r1 <= r0) return;

  TUndo *undo = new KeyframeSwingUndo(r0, r1, cols);
  TUndoManager::manager()->add(undo);
  undo->redo();
}

//-----------------------------------------------------------------------------
// Roll Up / Down (key-only) — cyclic rotation of the keys within [r0,r1] by one
// row. Roll up: r0 wraps to r1, the rest move up one. Roll down is the inverse.
//-----------------------------------------------------------------------------

namespace {

void doRollKeyframes(int r0, int r1, const std::set<int> &cols, bool up) {
  if (r1 <= r0) return;
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObjectId cameraId =
      TStageObjectId::CameraId(xsh->getCameraColumnIndex());
  for (int c : cols) {
    TStageObjectId id = c >= 0 ? xsh->getColumnObjectId(c) : cameraId;
    TStageObject *obj = xsh->getStageObject(id);
    if (!obj) continue;
    std::map<int, TStageObject::Keyframe> keys;
    for (int r = r0; r <= r1; r++)
      if (obj->isKeyframe(r)) keys[r] = obj->getKeyframe(r);
    if (keys.empty()) continue;
    for (auto const &kv : keys) obj->removeKeyframeWithoutUndo(kv.first);
    for (auto const &kv : keys) {
      int r = kv.first;
      int nr;
      if (up)
        nr = (r == r0) ? r1 : r - 1;
      else
        nr = (r == r1) ? r0 : r + 1;
      obj->setKeyframeWithoutUndo(nr, kv.second);
    }
  }
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

class KeyframeRollUndo final : public TUndo {
  int m_r0, m_r1;
  std::set<int> m_cols;
  bool m_up;

public:
  KeyframeRollUndo(int r0, int r1, const std::set<int> &cols, bool up)
      : m_r0(r0), m_r1(r1), m_cols(cols), m_up(up) {}

  void redo() const override { doRollKeyframes(m_r0, m_r1, m_cols, m_up); }
  void undo() const override { doRollKeyframes(m_r0, m_r1, m_cols, !m_up); }

  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return m_up ? QObject::tr("Roll Up Keyframes")
                : QObject::tr("Roll Down Keyframes");
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

}  // namespace

void TKeyframeSelection::rollupKeyframes() {
  unselectLockedColumn();
  if (isEmpty()) return;
  int r0 = INT_MAX, r1 = -1;
  std::set<int> cols;
  for (auto const &p : m_positions) {
    r0 = std::min(r0, p.first);
    r1 = std::max(r1, p.first);
    cols.insert(p.second);
  }
  if (r1 <= r0) return;
  TUndo *undo = new KeyframeRollUndo(r0, r1, cols, true);
  TUndoManager::manager()->add(undo);
  undo->redo();
}

void TKeyframeSelection::rolldownKeyframes() {
  unselectLockedColumn();
  if (isEmpty()) return;
  int r0 = INT_MAX, r1 = -1;
  std::set<int> cols;
  for (auto const &p : m_positions) {
    r0 = std::min(r0, p.first);
    r1 = std::max(r1, p.first);
    cols.insert(p.second);
  }
  if (r1 <= r0) return;
  TUndo *undo = new KeyframeRollUndo(r0, r1, cols, false);
  TUndoManager::manager()->add(undo);
  undo->redo();
}

//-----------------------------------------------------------------------------
// Repeat / Duplicate Keyframes (key-only) — append the key pattern [r0,r1]
// `count` times right after it. The undo restores whatever was in the tail.
//-----------------------------------------------------------------------------

namespace {

class KeyframeDuplicateUndo final : public TUndo {
  int m_r0, m_r1, m_count, m_step;
  std::set<int> m_cols;
  std::map<int, std::map<int, TStageObject::Keyframe>> m_tailSnapshot;

  static TStageObject *stageObj(TXsheet *xsh, int c) {
    TStageObjectId id =
        c >= 0 ? xsh->getColumnObjectId(c)
               : TStageObjectId::CameraId(xsh->getCameraColumnIndex());
    return xsh->getStageObject(id);
  }
  // first / last row written by the copies (loop overlaps onto r1)
  int tail0() const { return m_r0 + m_step; }
  int tail1() const { return m_r1 + m_count * m_step; }

public:
  KeyframeDuplicateUndo(int r0, int r1, int count, int step,
                        const std::set<int> &cols)
      : m_r0(r0), m_r1(r1), m_count(count), m_step(step), m_cols(cols) {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c : cols) {
      TStageObject *obj = stageObj(xsh, c);
      if (!obj) continue;
      std::map<int, TStageObject::Keyframe> snap;
      for (int r = tail0(); r <= tail1(); r++)
        if (obj->isKeyframe(r)) snap[r] = obj->getKeyframe(r);
      m_tailSnapshot[c] = snap;
    }
  }

  void redo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c : m_cols) {
      TStageObject *obj = stageObj(xsh, c);
      if (!obj) continue;
      // copies must be read from the original block: snapshot it first
      std::map<int, TStageObject::Keyframe> src;
      for (int r = m_r0; r <= m_r1; r++)
        if (obj->isKeyframe(r)) src[r] = obj->getKeyframe(r);
      for (int i = 1; i <= m_count; i++)
        for (auto const &kv : src)
          obj->setKeyframeWithoutUndo(kv.first + i * m_step, kv.second);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c : m_cols) {
      TStageObject *obj = stageObj(xsh, c);
      if (!obj) continue;
      for (int r = tail0(); r <= tail1(); r++)
        if (obj->isKeyframe(r)) obj->removeKeyframeWithoutUndo(r);
      auto it = m_tailSnapshot.find(c);
      if (it != m_tailSnapshot.end())
        for (auto const &kv : it->second)
          obj->setKeyframeWithoutUndo(kv.first, kv.second);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Repeat Keyframes");
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

}  // namespace

void TKeyframeSelection::duplicateKeyframes(int count, bool loop) {
  unselectLockedColumn();
  if (isEmpty() || count < 1) return;

  int r0 = INT_MAX, r1 = -1;
  std::set<int> cols;
  for (auto const &p : m_positions) {
    r0 = std::min(r0, p.first);
    r1 = std::max(r1, p.first);
    cols.insert(p.second);
  }
  if (r1 < r0) return;
  // loop shares the seam key (last == first), so each copy advances by r1-r0
  int step = (r1 - r0) + (loop ? 0 : 1);
  if (step < 1) return;  // single key with loop has no cycle length

  TUndo *undo = new KeyframeDuplicateUndo(r0, r1, count, step, cols);
  TUndoManager::manager()->add(undo);
  undo->redo();

  // extend the selection to cover all the copies
  for (int c : cols)
    for (int i = 1; i <= count; i++)
      for (int r = r0; r <= r1; r++)
        if (isSelected(r, c)) select(r + i * step, c);
}

//-----------------------------------------------------------------------------
// Time Stretch Keyframes (key-only) — proportionally rescale the keys in
// [r0,r1] so the block spans newRange rows, endpoints anchored. Useful e.g. to
// slow a riggsed walk cycle from 18 to 24 frames keeping the timing proportions.
//-----------------------------------------------------------------------------

namespace {

class KeyframeStretchUndo final : public TUndo {
  int m_r0, m_r1, m_newRange;
  std::set<int> m_cols;
  // full affected span snapshot, per column (undo-safe even if it overwrites
  // foreign keys in the expanded span)
  std::map<int, std::map<int, TStageObject::Keyframe>> m_snapshot;

  static TStageObject *stageObj(TXsheet *xsh, int c) {
    TStageObjectId id =
        c >= 0 ? xsh->getColumnObjectId(c)
               : TStageObjectId::CameraId(xsh->getCameraColumnIndex());
    return xsh->getStageObject(id);
  }
  int oldRange() const { return m_r1 - m_r0 + 1; }
  int span1() const { return m_r0 + std::max(oldRange(), m_newRange) - 1; }

public:
  KeyframeStretchUndo(int r0, int r1, int newRange, const std::set<int> &cols)
      : m_r0(r0), m_r1(r1), m_newRange(newRange), m_cols(cols) {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c : cols) {
      TStageObject *obj = stageObj(xsh, c);
      if (!obj) continue;
      std::map<int, TStageObject::Keyframe> snap;
      for (int r = m_r0; r <= span1(); r++)
        if (obj->isKeyframe(r)) snap[r] = obj->getKeyframe(r);
      m_snapshot[c] = snap;
    }
  }

  void redo() const override {
    TXsheet *xsh   = TApp::instance()->getCurrentXsheet()->getXsheet();
    double oldSpan = oldRange() - 1, newSpan = m_newRange - 1;
    for (int c : m_cols) {
      TStageObject *obj = stageObj(xsh, c);
      if (!obj) continue;
      auto it = m_snapshot.find(c);
      if (it == m_snapshot.end()) continue;
      // clear the whole affected span, then place the rescaled originals
      for (int r = m_r0; r <= span1(); r++)
        if (obj->isKeyframe(r)) obj->removeKeyframeWithoutUndo(r);
      for (auto const &kv : it->second) {
        if (kv.first < m_r0 || kv.first > m_r1) continue;  // only the block
        int nr = m_r0 + (int)std::lround((kv.first - m_r0) * newSpan / oldSpan);
        obj->setKeyframeWithoutUndo(nr, kv.second);
      }
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c : m_cols) {
      TStageObject *obj = stageObj(xsh, c);
      if (!obj) continue;
      for (int r = m_r0; r <= span1(); r++)
        if (obj->isKeyframe(r)) obj->removeKeyframeWithoutUndo(r);
      auto it = m_snapshot.find(c);
      if (it != m_snapshot.end())
        for (auto const &kv : it->second)
          obj->setKeyframeWithoutUndo(kv.first, kv.second);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Time Stretch Keyframes");
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

}  // namespace

void TKeyframeSelection::timeStretchKeyframes(int newRange) {
  unselectLockedColumn();
  if (isEmpty() || newRange < 1) return;

  int r0 = INT_MAX, r1 = -1;
  std::set<int> cols;
  for (auto const &p : m_positions) {
    r0 = std::min(r0, p.first);
    r1 = std::max(r1, p.first);
    cols.insert(p.second);
  }
  int oldRange = r1 - r0 + 1;
  if (oldRange < 2 || newRange == oldRange) return;  // need a span to rescale

  TUndo *undo = new KeyframeStretchUndo(r0, r1, newRange, cols);
  TUndoManager::manager()->add(undo);
  undo->redo();

  // reselect the rescaled block
  selectNone();
  for (int c : cols)
    for (int r = r0; r <= r0 + newRange - 1; r++) select(r, c);
}

void TKeyframeSelection::openTimeStretchPopup() {
  static TimeStretchPopup *popup = nullptr;
  if (!popup) popup = new TimeStretchPopup();
  popup->show();
  popup->raise();
  popup->activateWindow();
}

//-----------------------------------------------------------------------------
// Increase / Decrease Keyframe Spacing (key-only) — widen (delta=+1) or tighten
// (delta=-1) every gap between consecutive selected keys by one frame, per
// object. The first selected key per object is anchored; the i-th key moves by
// i*delta, so the whole block grows/shrinks uniformly while keeping the timing
// proportions of unequal gaps. Decrease is rejected (whole op) if any object
// has a gap of 1, because keys can't be pushed onto each other.
//-----------------------------------------------------------------------------

namespace {

class KeyframeSpaceUndo final : public TUndo {
  int m_r0, m_r1, m_delta;
  std::set<int> m_cols;
  // original block keys per column (only [r0,r1]); foreign keys below the block
  // are handled by a uniform ripple (moveKeyframes), not snapshotted.
  std::map<int, std::map<int, TStageObject::Keyframe>> m_blockKeys;

  static TStageObject *stageObj(TXsheet *xsh, int c) {
    TStageObjectId id =
        c >= 0 ? xsh->getColumnObjectId(c)
               : TStageObjectId::CameraId(xsh->getCameraColumnIndex());
    return xsh->getStageObject(id);
  }

  // Rows below r1 ripple by this so they keep their distance from the block end:
  // the block grows/shrinks by (keys-1)*delta.
  int totalDelta(const std::map<int, TStageObject::Keyframe> &blk) const {
    return ((int)blk.size() - 1) * m_delta;
  }

  // Shift every keyframe strictly below `threshold` row by `delta`.
  static void rippleBelow(TStageObject *obj, int threshold, int delta) {
    if (delta == 0) return;
    TStageObject::KeyframeMap km;
    obj->getKeyframes(km);
    std::set<int> toShift;
    for (auto const &kv : km)
      if (kv.first > threshold) toShift.insert(kv.first);
    if (!toShift.empty()) obj->moveKeyframes(toShift, delta);
  }

public:
  KeyframeSpaceUndo(int r0, int r1, int delta, const std::set<int> &cols)
      : m_r0(r0), m_r1(r1), m_delta(delta), m_cols(cols) {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c : cols) {
      TStageObject *obj = stageObj(xsh, c);
      if (!obj) continue;
      std::map<int, TStageObject::Keyframe> blk;
      for (int r = m_r0; r <= m_r1; r++)
        if (obj->isKeyframe(r)) blk[r] = obj->getKeyframe(r);
      m_blockKeys[c] = blk;
    }
  }

  void redo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c : m_cols) {
      TStageObject *obj = stageObj(xsh, c);
      if (!obj) continue;
      auto it = m_blockKeys.find(c);
      if (it == m_blockKeys.end()) continue;
      const auto &blk = it->second;
      // 1) lift the block keys out so the ripple destination is clear
      for (auto const &kv : blk)
        if (obj->isKeyframe(kv.first)) obj->removeKeyframeWithoutUndo(kv.first);
      // 2) ripple everything below the block to make room (or close the gap)
      rippleBelow(obj, m_r1, totalDelta(blk));
      // 3) re-place the block keys with one frame added/removed per gap
      int i = 0;
      for (auto const &kv : blk)
        obj->setKeyframeWithoutUndo(kv.first + (i++) * m_delta, kv.second);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c : m_cols) {
      TStageObject *obj = stageObj(xsh, c);
      if (!obj) continue;
      auto it = m_blockKeys.find(c);
      if (it == m_blockKeys.end()) continue;
      const auto &blk = it->second;
      int td = totalDelta(blk);
      // 1) remove the respaced block keys (current positions)
      int i = 0;
      for (auto const &kv : blk) {
        int nr = kv.first + (i++) * m_delta;
        if (obj->isKeyframe(nr)) obj->removeKeyframeWithoutUndo(nr);
      }
      // 2) ripple the foreign keys back (they now sit below the new block end)
      rippleBelow(obj, m_r1 + td, -td);
      // 3) restore the original block keys
      for (auto const &kv : blk)
        obj->setKeyframeWithoutUndo(kv.first, kv.second);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return m_delta > 0 ? QObject::tr("Increase Keyframe Spacing")
                       : QObject::tr("Decrease Keyframe Spacing");
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

// Reselect the respaced block so the command can be applied repeatedly without
// having to drag-select the keys again. Computes the new key rows (orig+i*delta)
// from the block captured BEFORE the op.
void reselectRespacedBlock(TKeyframeSelection *sel,
                           const std::map<int, std::vector<int>> &origBlock,
                           int delta) {
  sel->selectNone();
  for (auto const &cv : origBlock)
    for (int i = 0; i < (int)cv.second.size(); i++)
      sel->select(cv.second[i] + i * delta, cv.first);
}

// Snapshot, per column, the rows that carry a keyframe in [r0,r1] (the block).
std::map<int, std::vector<int>> captureBlockRows(const std::set<int> &cols,
                                                 int r0, int r1) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  std::map<int, std::vector<int>> blk;
  for (int c : cols) {
    TStageObjectId id =
        c >= 0 ? xsh->getColumnObjectId(c)
               : TStageObjectId::CameraId(xsh->getCameraColumnIndex());
    TStageObject *obj = xsh->getStageObject(id);
    if (!obj) continue;
    std::vector<int> rows;
    for (int r = r0; r <= r1; r++)
      if (obj->isKeyframe(r)) rows.push_back(r);
    if (!rows.empty()) blk[c] = rows;
  }
  return blk;
}

}  // namespace

void TKeyframeSelection::increaseKeyframes() {
  unselectLockedColumn();
  if (isEmpty()) return;

  int r0 = INT_MAX, r1 = -1;
  std::set<int> cols;
  for (auto const &p : m_positions) {
    r0 = std::min(r0, p.first);
    r1 = std::max(r1, p.first);
    cols.insert(p.second);
  }
  if (r1 <= r0) return;  // need at least two rows to have a gap

  auto origBlock = captureBlockRows(cols, r0, r1);
  TUndo *undo = new KeyframeSpaceUndo(r0, r1, +1, cols);
  TUndoManager::manager()->add(undo);
  undo->redo();
  reselectRespacedBlock(this, origBlock, +1);
}

void TKeyframeSelection::decreaseKeyframes() {
  unselectLockedColumn();
  if (isEmpty()) return;

  int r0 = INT_MAX, r1 = -1;
  std::set<int> cols;
  for (auto const &p : m_positions) {
    r0 = std::min(r0, p.first);
    r1 = std::max(r1, p.first);
    cols.insert(p.second);
  }
  if (r1 <= r0) return;

  // Reject if any object already has a 1-frame gap between consecutive selected
  // keys — decreasing would collide two keys on the same row.
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  for (int c : cols) {
    TStageObjectId id =
        c >= 0 ? xsh->getColumnObjectId(c)
               : TStageObjectId::CameraId(xsh->getCameraColumnIndex());
    TStageObject *obj = xsh->getStageObject(id);
    if (!obj) continue;
    int prev = -1;
    for (int r = r0; r <= r1; r++) {
      if (!obj->isKeyframe(r)) continue;
      if (prev >= 0 && r - prev < 2) return;  // gap already 1 → abort
      prev = r;
    }
  }

  auto origBlock = captureBlockRows(cols, r0, r1);
  TUndo *undo = new KeyframeSpaceUndo(r0, r1, -1, cols);
  TUndoManager::manager()->add(undo);
  undo->redo();
  reselectRespacedBlock(this, origBlock, -1);
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::shiftKeyframes(int direction) {
  copyKeyframes();
  if (isEmpty()) return;

  std::set<Position> positions = m_positions;

  int r0 = positions.begin()->first;
  int c0 = positions.begin()->second;

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  TKeyframeData *data = new TKeyframeData();
  data->setKeyframes(positions, xsh);

  TUndoManager::manager()->beginBlock();

  XsheetViewer *viewer = TApp::instance()->getCurrentXsheetViewer();
  TKeyframeSelection *selection = viewer->getKeyframeSelection();
  selection->selectNone();

  std::set<Position>::iterator it = positions.begin(), itEnd = positions.end();
  for(; it != itEnd; ++it) {
    Position position = *it;
    int r = position.first;
    int c = position.second;

	TXshColumn *column = xsh->getColumn(c);
	if (!column || column->isLocked()) continue;

    shiftKeyframes(r, r + direction, c, c, false);
	selection->select(r + direction, c);
  }

  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::pasteKeyframesWithShift(int r0, int r1, int c0,
                                                 int c1) {
  unselectLockedColumn();

  // Retrieve keyframes to paste
  QClipboard *clipboard = QApplication::clipboard();
  const TKeyframeData *data =
      dynamic_cast<const TKeyframeData *>(clipboard->mimeData());
  if (!data) {
    const TCellKeyframeData *cellKeyframeData =
        dynamic_cast<const TCellKeyframeData *>(clipboard->mimeData());
    if (cellKeyframeData) data = cellKeyframeData->getKeyframeData();
  }
  if (!data) return;

  // Retrieve corresponding old keyframes
  std::set<TKeyframeSelection::Position> positions(m_positions);
  data->getKeyframes(positions);

  TKeyframeData *oldData = new TKeyframeData();
  TXsheet *xsh           = TApp::instance()->getCurrentXsheet()->getXsheet();
  oldData->setKeyframes(positions, xsh);

  bool hasDrawingKeys = false;
  std::map<int, std::set<double>> oldKeyRange;
  std::map<int, std::vector<TXshCell>> undoFrames;

  foreach (auto keyData, data->m_keyData) {
    int r = keyData.second.m_channels->m_frame;
    int c = keyData.first.second;
    if (c < 0 || oldKeyRange.find(c) != oldKeyRange.end()) continue;
    TStageObject *stObj = xsh->getStageObject(xsh->getColumnObjectId(c));
    if (!stObj->hasDrawingNumberKey(r)) continue;
    hasDrawingKeys = true;

    TStageObject::KeyframeMap keyframes;
    stObj->getKeyframes(keyframes);
    std::set<double> range;
    range.insert(keyframes.begin()->first);
    range.insert(keyframes.rbegin()->first);
    oldKeyRange[c] = range;

    int cr0, cr1;
    xsh->getCellRange(c, cr0, cr1);
    int n = cr1 + 1;
    std::vector<TXshCell> cells(n);
    xsh->getCells(0, c, n, &cells[0], false, false);
    undoFrames[c] = cells;
  }

  bool isShift = shiftKeyframesWithoutUndo(r0, r1, c0, c1, false, true);
  bool isPaste = pasteKeyframesWithoutUndo(data, &m_positions);
  if (!isPaste && !isShift) {
    delete oldData;
    return;
  }

  TKeyframeData *newData = new TKeyframeData();
  newData->setKeyframes(m_positions, xsh);
  TKeyframeSelection *selection = new TKeyframeSelection(m_positions);

  if (hasDrawingKeys) {
    TUndoManager::manager()->beginBlock();

    std::map<int, std::set<double>>::iterator it(oldKeyRange.begin()),
        itEnd(oldKeyRange.end());
    std::map<int, std::vector<TXshCell>>::iterator cit(undoFrames.begin());
    for (; it != itEnd; it++, cit++) {
      int c               = cit->first;
      TStageObject *stObj = xsh->getStageObject(xsh->getColumnObjectId(c));
      TStageObject::KeyframeMap keyframes;
      stObj->getKeyframes(keyframes);
      foreach (auto key, keyframes) {
        int r = key.first;
        xsh->addUndoDrawingNumberChange(r, c, it->second, cit->second);
      }
    }
  }
  TUndoManager::manager()->add(
      new PasteKeyframesUndo(selection, newData, oldData, r0, r1, c0, c1));
  if (hasDrawingKeys) TUndoManager::manager()->endBlock();

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::deleteKeyframesWithShift(int r0, int r1, int c0,
                                                  int c1) {
  unselectLockedColumn();

  TKeyframeData *data = new TKeyframeData();
  TXsheet *xsh        = TApp::instance()->getCurrentXsheet()->getXsheet();
  data->setKeyframes(m_positions, xsh);
  if (m_positions.empty()) {
    delete data;
    return;
  }
  TKeyframeSelection *selection = new TKeyframeSelection(m_positions);
  bool deleteKeyFrame           = deleteKeyframesWithoutUndo(&m_positions);
  bool isShift = shiftKeyframesWithoutUndo(r0, r1, c0, c1, true, true);
  if (!deleteKeyFrame && !isShift) {
    delete selection;
    delete data;
    return;
  }
  TUndoManager::manager()->add(
      new DeleteKeyframesUndo(selection, data, r0, r1, c0, c1));
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void TKeyframeSelection::shiftKeyframes(int r0, int r1, int c0, int c1,
                                        bool shiftFollowing) {
  unselectLockedColumn();

  int r1adj = r0 < r1 ? r1 - 1 : r0 + (r0 - r1) - 1;
  bool cut  = r0 < r1 ? false : true;

  bool isShift =
      shiftKeyframesWithoutUndo(r0, r1adj, c0, c1, cut, shiftFollowing);
  if (!isShift) return;

  TUndoManager::manager()->add(
      new ShiftKeyframesUndo(r0, r1, c0, c1, shiftFollowing));
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}
