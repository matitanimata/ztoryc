#pragma once

//=============================================================================
// ZtoryProductionPanel
// -----------------------------------------------------------------------------
// Kitsu-style production tracker: a dockable matrix where rows are shots and
// columns are the production task types (Storyboard, Layout, Animation, …).
// Each cell shows the per-shot task status with its Kitsu colour.  This panel
// is the in-app source of truth for task statuses (the .ztoryc persists them);
// the exported spreadsheet is just a projection of this data.
//
// Phase 1 (this file): read-only matrix that mirrors the model.
// Phase 2 (planned): click-to-edit a cell's status + undo, then auto WIP→WFA
// on render completion.
//=============================================================================

#include "pane.h"

#include <QTableWidget>

class ZtoryProductionPanel final : public TPanel {
  Q_OBJECT

  QTableWidget *m_table = nullptr;
  QStringList   m_taskCols;   // task-type per column (index 0 == table column 1)

public:
  ZtoryProductionPanel(QWidget *parent = nullptr);

private:
  // Rebuild the whole matrix from ZtoryModel (cheap: small productions).
  void rebuild();
  // Pop up the status picker for a clicked cell and apply + persist + undo.
  void editCell(int row, int col);

private slots:
  void onModelChanged();
  void onCellClicked(int row, int col);
};
