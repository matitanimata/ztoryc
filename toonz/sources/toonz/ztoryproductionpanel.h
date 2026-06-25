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
#include <QStringList>

class QTabWidget;
class QListWidget;
class QLineEdit;
class QComboBox;

class ZtoryProductionPanel final : public TPanel {
  Q_OBJECT

  QTabWidget   *m_tabs  = nullptr;
  // Shots tab
  QTableWidget *m_table = nullptr;
  QStringList   m_taskCols;   // task-type per column (index 0 == table column 1)
  // Team tab
  QListWidget  *m_teamList = nullptr;
  bool          m_teamLoading = false;  // guard against apply during reload
  // Project tab
  QLineEdit    *m_prodEdit = nullptr, *m_titleEdit = nullptr, *m_epEdit = nullptr;
  QComboBox    *m_techCombo = nullptr;
  bool          m_projLoading = false;

public:
  ZtoryProductionPanel(QWidget *parent = nullptr);

private:
  // Shots tab
  QWidget *buildShotsTab();
  void rebuild();                  // rebuild the shot × task matrix from ZtoryModel
  void editCell(int row, int col); // status/assignee picker for a clicked cell
  // Team tab
  QWidget *buildTeamTab();
  void reloadTeamTab();            // model → list
  void applyTeamFromList();        // list → model + persist
  // Project tab
  QWidget *buildProjectTab();
  void reloadProjectTab();         // model → fields
  void applyProjectFromFields();   // fields → model + persist

private slots:
  void onModelChanged();
  void onCellClicked(int row, int col);
};
