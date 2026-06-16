

#include "cellkeyframeselection.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "cellkeyframedata.h"
#include "tapp.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonzqt/tselectionhandle.h"
#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "toonz/tstageobject.h"
#include "toonz/preferences.h"

// std includes
#include <map>
#include <set>
#include <vector>

// TnzCore includes
#include "tundo.h"

// Qt includes
#include <QApplication>
#include <QClipboard>

//=============================================================================
//  TCellKeyframeSelection
//-----------------------------------------------------------------------------

TCellKeyframeSelection::TCellKeyframeSelection(
    TKeyframeSelection *keyframeSelection)
    : m_keyframeSelection(keyframeSelection), m_xsheetHandle(0) {}

//-----------------------------------------------------------------------------

TCellKeyframeSelection::~TCellKeyframeSelection() { delete m_keyframeSelection; }

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::enableCommands() {
  // Abilita l'INTERO repertorio dei comandi celle (reverse, swing, step/each,
  // insert/remove/duplicate, reframe, time stretch, …) facendoli operare sulla
  // cell-selection interna (che tiene il range corretto e usa il proprio
  // m_range, non la current selection dell'app).  Prima questa selezione
  // combinata era uno stub con solo copy/paste/cut/delete → tutti gli altri
  // comandi restavano grigi quando i keyframe erano inclusi nella selezione.
  //
  // Nota: i comandi che spostano l'exposure passando da TXsheet::insert/
  // removeCells portano già i keyframe con sé (Preference "Keyframes Follow
  // Exposure", hook centrale).  I comandi di RIORDINO (reverse/swing/step…)
  // riordinano solo le celle: l'allineamento dei keyframe a quei riordini è un
  // incremento successivo.
  //
  // We ARE a TCellSelection now, so enable the whole cell repertoire on
  // ourselves; the overrides below then refine the few commands that must also
  // touch the selected keyframes. Since this object is current in normal mode
  // too, each override falls through to the plain cell behavior when no
  // keyframes are involved (see the methods).
  TCellSelection::enableCommands();

  // Override dei 4 comandi che devono gestire ANCHE i keyframe selezionati.
  enableCommand(this, MI_Copy, &TCellKeyframeSelection::copyCellsKeyframes);
  enableCommand(this, MI_Paste, &TCellKeyframeSelection::pasteCellsKeyframes);
  enableCommand(this, MI_Cut, &TCellKeyframeSelection::cutCellsKeyframes);
  enableCommand(this, MI_Clear, &TCellKeyframeSelection::deleteCellsKeyframes);

  // Increase/Decrease Step in modalità combinata NON ripetono i disegni: aggiungono
  // o tolgono un frame negli intervalli tra le chiavi (celle + chiavi slittano
  // insieme via keys-follow).  Override del default cella (increase/decreaseStep).
  enableCommand(this, MI_IncreaseStep,
                &TCellKeyframeSelection::increaseCellsKeyframes);
  enableCommand(this, MI_DecreaseStep,
                &TCellKeyframeSelection::decreaseCellsKeyframes);
}

//-----------------------------------------------------------------------------

bool TCellKeyframeSelection::isEmpty() const {
  return m_keyframeSelection->isEmpty() && TCellSelection::isEmpty();
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::copyCellsKeyframes() {
  // Normal mode (no keyframes selected): behave exactly like a cell copy.
  if (m_keyframeSelection->isEmpty()) {
    TCellSelection::copyCells();
    return;
  }
  TCellKeyframeData *data = new TCellKeyframeData();
  // Copy cells
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  if (!isEmpty()) {
    int colCount = c1 - c0 + 1;
    int rowCount = r1 - r0 + 1;
    if (colCount <= 0 || rowCount <= 0) return;
    TXsheet *xsh        = m_xsheetHandle->getXsheet();
    TCellData *cellData = new TCellData();
    cellData->setCells(xsh, r0, c0, r1, c1);
    data->setCellData(cellData);
  }
  // Copy keyframes
  if (!isEmpty()) {
    QClipboard *clipboard       = QApplication::clipboard();
    TXsheet *xsh                = m_xsheetHandle->getXsheet();
    TKeyframeData *keyframeData = new TKeyframeData();
    TKeyframeData::Position startPos(r0, c0);
    keyframeData->setKeyframes(m_keyframeSelection->getSelection(), xsh,
                               startPos);
    data->setKeyframeData(keyframeData);
  }
  // Set the clipboard
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setMimeData(data, QClipboard::Clipboard);
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::pasteCellsKeyframes() {
  // Normal mode: standard paste (honors the paste-behavior preference).
  if (m_keyframeSelection->isEmpty()) {
    TCellSelection::doPaste();
    return;
  }
  TCellSelection::pasteCells();
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::deleteCellsKeyframes() {
  // Normal mode: plain Clear (matches the base MI_Clear binding).
  if (m_keyframeSelection->isEmpty()) {
    TCellSelection::clearCells();
    return;
  }
  TUndoManager::manager()->beginBlock();
  TCellSelection::deleteCells();
  m_keyframeSelection->deleteKeyframes();
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::cutCellsKeyframes() {
  // Normal mode: plain cut.
  if (m_keyframeSelection->isEmpty()) {
    TCellSelection::cutCells();
    return;
  }
  copyCellsKeyframes();
  TUndoManager::manager()->beginBlock();
  // I keyframe vanno cancellati PRIMA di cutCells().  Con "Keyframes Follow
  // Exposure" ON, cutCells() -> removeCells() cancella già i keyframe nello
  // span tagliato: se deleteKeyframes() girasse dopo, lo snapshot dell'undo
  // sarebbe vuoto e l'undo non potrebbe ripristinare le chiavi (BUG-2, perdita
  // dati).  Usiamo deleteKeyframes() (senza shift): lo shift delle chiavi
  // superstiti è gestito una sola volta da removeCells() — e simmetricamente da
  // insertCells() nell'undo del cut celle.  L'ordine di undo risultante è
  // corretto: prima CutCellsUndo (ripristina celle + shift), poi
  // DeleteKeyframesUndo (ri-incolla le chiavi sulle celle ormai ripristinate).
  m_keyframeSelection->deleteKeyframes();
  TCellSelection::cutCells(true);
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::selectCellsKeyframes(int r0, int c0, int r1,
                                                  int c1) {
  TCellSelection::selectCells(r0, c0, r1, c1);
  TXsheet *xsh = m_xsheetHandle->getXsheet();
  m_xsheetHandle->getXsheet();
  if (r1 < r0) std::swap(r0, r1);
  if (c1 < c0) std::swap(c0, c1);
  m_keyframeSelection->clear();
  int r, c;
  for (c = c0; c <= c1; c++)
    for (r = r0; r <= r1; r++) {
      TStageObjectId id =
          c < 0 ? TStageObjectId::CameraId(xsh->getCameraColumnIndex())
                : xsh->getColumnObjectId(c);
      TStageObject *stObj = xsh->getStageObject(id);
      if (stObj->isKeyframe(r)) m_keyframeSelection->select(r, c);
    }
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::selectCellKeyframe(int row, int col) {
  TCellSelection::selectCell(row, col);
  TXsheet *xsh = m_xsheetHandle->getXsheet();
  TStageObjectId id =
      col < 0 ? TStageObjectId::CameraId(xsh->getCameraColumnIndex())
              : xsh->getColumnObjectId(col);
  TStageObject *stObj = xsh->getStageObject(id);
  m_keyframeSelection->clear();
  if (stObj->isKeyframe(row)) m_keyframeSelection->select(row, col);
}

//-----------------------------------------------------------------------------

void TCellKeyframeSelection::selectNone() {
  TCellSelection::selectNone();
  m_keyframeSelection->selectNone();
}

//-----------------------------------------------------------------------------
// Increase / Decrease spacing in combined mode (keys-follow): add or remove one
// frame in every gap between consecutive keys, per column. Cells AND keyframes
// ripple together. We move the keyframes explicitly (not via the keys-follow
// preference) so the result is identical whether the combined selection was
// reached with the pref ON or via Alt/Ctrl with the pref OFF.
//-----------------------------------------------------------------------------

namespace {

TStageObject *gapStageObj(TXsheet *xsh, int c) {
  return xsh->getStageObject(
      c >= 0 ? xsh->getColumnObjectId(c)
             : TStageObjectId::CameraId(xsh->getCameraColumnIndex()));
}

// Insert one frame at `row`, shifting cells AND keyframes >= row down by 1. The
// new frame repeats a drawing so no empty "hole" appears (with implicit hold
// off). Prefer the preceding cell (extend the outgoing hold); if there is none
// — e.g. inserting at the very first frame of a drawing — repeat the drawing
// that was just pushed down so the drawing grows at its head instead of leaving
// an empty leading frame.
void gapInsert(TXsheet *xsh, int col, int row) {
  TXshColumn *column = xsh->getColumn(col);
  if (!column) return;
  if (TXshCellColumn *cc = column->getCellColumn()) {
    cc->insertEmptyCells(row, 1);
    // row-1 is above the insertion point, so it still holds the original cell;
    // row+1 holds the cell that was at `row` before the shift.
    TXshCell fill = (row > 0) ? xsh->getCell(row - 1, col) : TXshCell();
    if (fill.isEmpty()) fill = xsh->getCell(row + 1, col);
    if (!fill.isEmpty()) xsh->setCell(row, col, fill);
  }
  if (TStageObject *obj = gapStageObj(xsh, col)) {
    TStageObject::KeyframeMap km;
    obj->getKeyframes(km);
    std::set<int> toShift;
    for (auto const &kv : km)
      if (kv.first >= row) toShift.insert(kv.first);
    if (!toShift.empty()) obj->moveKeyframes(toShift, 1);
  }
  xsh->updateFrameCount();
}

// Remove one cell at `row`, deleting a keyframe there (if any) and shifting
// cells AND keyframes > row up by 1.
void gapRemove(TXsheet *xsh, int col, int row) {
  TXshColumn *column = xsh->getColumn(col);
  if (!column) return;
  if (TStageObject *obj = gapStageObj(xsh, col))
    if (obj->isKeyframe(row)) obj->removeKeyframeWithoutUndo(row);
  if (TXshCellColumn *cc = column->getCellColumn()) cc->removeCells(row, 1);
  if (TStageObject *obj = gapStageObj(xsh, col)) {
    TStageObject::KeyframeMap km;
    obj->getKeyframes(km);
    std::set<int> toShift;
    for (auto const &kv : km)
      if (kv.first > row) toShift.insert(kv.first);
    if (!toShift.empty()) obj->moveKeyframes(toShift, -1);
  }
  xsh->updateFrameCount();
}

class KeyframeGapResizeUndo final : public TUndo {
  int m_r0, m_r1, m_delta, m_snapEnd;
  // per column, the rows where we insert (increase) / remove (decrease) one
  // frame: one per inter-key gap when the selection spans >=2 keys, otherwise a
  // single point at r0 (so a one-cell selection still adds/removes a frame).
  std::map<int, std::vector<int>> m_ops;
  // full per-column snapshot for an exact undo: cells [r0, snapEnd] + keys >= r0
  std::map<int, std::vector<TXshCell>> m_cells;
  std::map<int, std::map<int, TStageObject::Keyframe>> m_keys;

  static TXsheet *xsheet() {
    return TApp::instance()->getCurrentXsheet()->getXsheet();
  }

public:
  KeyframeGapResizeUndo(int r0, int r1, int delta, const std::set<int> &cols)
      : m_r0(r0), m_r1(r1), m_delta(delta) {
    TXsheet *xsh = xsheet();
    m_snapEnd    = xsh->getFrameCount();
    for (int c : cols) {
      TStageObject *obj = gapStageObj(xsh, c);
      if (!obj) continue;
      std::vector<int> keys;
      for (int r = r0; r <= r1; r++)
        if (obj->isKeyframe(r)) keys.push_back(r);

      // Build the operation rows (ascending) for this column.
      std::vector<int> ops;
      if ((int)keys.size() >= 2) {
        for (int i = 1; i < (int)keys.size(); i++) {
          if (delta > 0)
            ops.push_back(keys[i]);  // insert just before key i
          else {
            int rr = keys[i] - 1;
            if (rr <= keys[i - 1]) {  // gap already 1 → can't tighten this col
              ops.clear();
              break;
            }
            ops.push_back(rr);  // remove just before key i
          }
        }
      } else {
        // No full gap selected: act on the single selection point r0.
        if (delta > 0)
          ops.push_back(r0);
        else if (!obj->isKeyframe(r0))  // never delete a key on decrease
          ops.push_back(r0);
      }
      if (ops.empty()) continue;
      m_ops[c] = ops;

      std::vector<TXshCell> cells;
      for (int r = r0; r <= m_snapEnd; r++) cells.push_back(xsh->getCell(r, c));
      m_cells[c] = cells;
      TStageObject::KeyframeMap km;
      obj->getKeyframes(km);
      std::map<int, TStageObject::Keyframe> ks;
      for (auto const &kv : km)
        if (kv.first >= r0) ks[kv.first] = kv.second;
      m_keys[c] = ks;
    }
  }

  bool hasWork() const { return !m_ops.empty(); }

  // Largest signed row shift across columns — used by the caller to reselect the
  // resized block.
  int maxSignedDelta() const {
    int best = 0;
    for (auto const &cv : m_ops)
      if ((int)cv.second.size() > best) best = (int)cv.second.size();
    return best * m_delta;
  }

  void redo() const override {
    TXsheet *xsh = xsheet();
    for (auto const &cv : m_ops) {
      int c                       = cv.first;
      const std::vector<int> &ops = cv.second;
      // Process bottom-up so the unprocessed (higher) rows keep the original
      // indices captured in the ctor.
      for (int i = (int)ops.size() - 1; i >= 0; i--) {
        if (m_delta > 0)
          gapInsert(xsh, c, ops[i]);
        else
          gapRemove(xsh, c, ops[i]);
      }
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  void undo() const override {
    TXsheet *xsh = xsheet();
    for (auto const &cv : m_ops) {
      int c             = cv.first;
      TStageObject *obj = gapStageObj(xsh, c);
      // wipe current keys >= r0 and cells [r0, currentEnd]
      if (obj) {
        TStageObject::KeyframeMap km;
        obj->getKeyframes(km);
        std::vector<int> toDel;
        for (auto const &kv : km)
          if (kv.first >= m_r0) toDel.push_back(kv.first);
        for (int r : toDel) obj->removeKeyframeWithoutUndo(r);
      }
      int curEnd = xsh->getFrameCount();
      for (int r = m_r0; r <= curEnd; r++) xsh->clearCells(r, c);
      // restore snapshot cells
      auto itC = m_cells.find(c);
      if (itC != m_cells.end())
        for (int k = 0; k < (int)itC->second.size(); k++)
          if (!itC->second[k].isEmpty())
            xsh->setCell(m_r0 + k, c, itC->second[k]);
      // restore snapshot keys
      auto itK = m_keys.find(c);
      if (obj && itK != m_keys.end())
        for (auto const &kv : itK->second)
          obj->setKeyframeWithoutUndo(kv.first, kv.second);
    }
    xsh->updateFrameCount();
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

}  // namespace

void TCellKeyframeSelection::increaseCellsKeyframes() {
  // Normal mode (keys-follow OFF): Increase Step keeps its native meaning —
  // repeat each drawing one more time.
  if (!Preferences::instance()->isKeyframesFollowExposureEnabled()) {
    TCellSelection::increaseStepCells();
    return;
  }
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  if (r1 < r0 || c1 < c0) return;
  std::set<int> cols;
  for (int c = c0; c <= c1; c++) cols.insert(c);

  KeyframeGapResizeUndo *undo = new KeyframeGapResizeUndo(r0, r1, +1, cols);
  if (!undo->hasWork()) {
    delete undo;
    return;
  }
  int newR1 = r1 + undo->maxSignedDelta();
  TUndoManager::manager()->add(undo);
  undo->redo();

  selectCellsKeyframes(r0, c0, newR1, c1);
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
}

void TCellKeyframeSelection::decreaseCellsKeyframes() {
  // Normal mode (keys-follow OFF): native Decrease Step (un-repeat drawings).
  if (!Preferences::instance()->isKeyframesFollowExposureEnabled()) {
    TCellSelection::decreaseStepCells();
    return;
  }
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  if (r1 < r0 || c1 < c0) return;
  std::set<int> cols;
  for (int c = c0; c <= c1; c++) cols.insert(c);

  // Feasibility (gap>=2 per column, or a non-key single point) is decided inside
  // the undo ctor: infeasible columns are skipped, so hasWork() == false means
  // the whole selection had nothing to tighten → silent no-op.
  KeyframeGapResizeUndo *undo = new KeyframeGapResizeUndo(r0, r1, -1, cols);
  if (!undo->hasWork()) {
    delete undo;
    return;
  }
  int newR1 = r1 + undo->maxSignedDelta();
  TUndoManager::manager()->add(undo);
  undo->redo();

  selectCellsKeyframes(r0, c0, newR1, c1);
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
}
