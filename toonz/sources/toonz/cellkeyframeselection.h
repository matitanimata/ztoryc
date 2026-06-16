#pragma once

#ifndef TCELLKEYFRAMESELECTION_H
#define TCELLKEYFRAMESELECTION_H

#include "toonzqt/selection.h"
#include "cellselection.h"
#include "keyframeselection.h"
#include "tgeometry.h"
#include <set>

class TXsheetHandle;

//=============================================================================
// TCellKeyframeSelection
//-----------------------------------------------------------------------------

// Inherits TCellSelection so that dynamic_cast<TCellSelection*> on the current
// selection succeeds while a combined cells+keyframes selection is active. This
// keeps EVERY cell command (drawing substitution, filters, reframe, …) working
// on the full range in keys-follow mode, instead of falling back to the single
// current cell. The object IS the cell selection (it owns the range via the
// base class) and additionally carries the selected keyframes.
class TCellKeyframeSelection final : public TCellSelection {
  TKeyframeSelection *m_keyframeSelection;

  TXsheetHandle *m_xsheetHandle;

public:
  TCellKeyframeSelection(TKeyframeSelection *keyframeSelection);
  ~TCellKeyframeSelection();

  // The wrapper itself is the cell selection now.
  TCellSelection *getCellSelection() { return this; }
  TKeyframeSelection *getKeyframeSelection() { return m_keyframeSelection; }

  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }

  void enableCommands() override;

  bool isEmpty() const override;

  void copyCellsKeyframes();
  void pasteCellsKeyframes();
  void deleteCellsKeyframes();
  void cutCellsKeyframes();

  // Increase / Decrease spacing in combined mode (keys-follow ON): add (resp.
  // remove) one frame in every gap between consecutive keys. Uses insert/
  // removeCells so BOTH cells and keyframes ripple together. NOT a cell step
  // repeat — the drawings are not duplicated, only the timing is stretched.
  void increaseCellsKeyframes();
  void decreaseCellsKeyframes();

  //! \note: puo' anche essere r0>r1 o c0>c1
  void selectCellsKeyframes(int r0, int c0, int r1, int c1);
  void selectCellKeyframe(int row, int col);
  void selectNone() override;

  /*
  void getSelectedCells(int &r0, int &c0, int &r1, int &c1) const;
  Range getSelectedCells() const;

bool isCellSelected(int r , int c) const;
bool isRowSelected(int row) const;
bool isColSelected(int col) const;

bool areAllColSelectedLocked() const;

  //commands
void reverseCells();
  void swingCells();
  void incrementCells();
void duplicateCells();
  void randomCells();
  void stepCells(int count);
  void eachCells(int count);
void step2Cells() {stepCells(2);}
void step3Cells() {stepCells(3);}
void step4Cells() {stepCells(4);}
void each2Cells() {eachCells(2);}
void each3Cells() {eachCells(3);}
void each4Cells() {eachCells(4);}
  void rollupCells();
  void rolldownCells();

  void setKeyframes();
  void cloneLevel();
void insertCells();

void openTimeStretchPopup();*/
};

#endif  // TCELLKEYFRAMESELECTION_H
