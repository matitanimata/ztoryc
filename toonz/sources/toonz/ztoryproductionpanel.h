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
class QLabel;

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
  QLineEdit    *m_seasonEdit = nullptr;
  QLineEdit    *m_codeEdit = nullptr;      // short project code (Kitsu code, {CODE} token)
  QComboBox    *m_techCombo = nullptr;
  QLineEdit    *m_patternEdit = nullptr;   // B3d: naming pattern
  QLabel       *m_kitsuLabel = nullptr;    // M5: Kitsu link status
  bool          m_projLoading = false;
  // Assets tab
  QTableWidget *m_assetTable = nullptr;
  QStringList   m_assetTaskCols;
  bool          m_assetLoading = false;
  // Workflows tab
  QListWidget  *m_techList     = nullptr;
  QListWidget  *m_taskTypeList = nullptr;
  bool          m_wfLoading    = false;

public:
  ZtoryProductionPanel(QWidget *parent = nullptr);

private:
  // Shots tab
  QWidget *buildShotsTab();
  void rebuild();                  // rebuild the shot × task matrix from ZtoryModel
  void editCell(int row, int col); // status/assignee picker for a clicked cell
  // Full-project export: one XLSX with every tab (Shots across all storyboards,
  // Team, Assets, Workflows, Project) sourced from the project DB.
  void exportFullProject();
  // Team tab
  QWidget *buildTeamTab();
  void reloadTeamTab();            // model → list
  void applyTeamFromList();        // list → model + persist
  // Project tab
  QWidget *buildProjectTab();
  void reloadProjectTab();         // model → fields
  void applyProjectFromFields();   // fields → model + persist
  // Assets tab
  QWidget *buildAssetsTab();
  void rebuildAssets();            // rebuild the asset × task matrix from ZtoryModel
  void editAssetCell(int row, int col);
  // Workflows tab
  QWidget *buildWorkflowsTab();
  void reloadWorkflowsTab();        // model → workflow (technique) list
  void reloadTaskTypeList();        // selected workflow → its task-type list
  void applyTaskTypesToTechnique(); // task-type list → selected workflow + persist

private slots:
  void onModelChanged();
  void onCellClicked(int row, int col);
  void onShotContextMenu(const QPoint &pos);    // batch edit on selected task cells
  void onAssetCellClicked(int row, int col);
  void onAssetItemChanged(QTableWidgetItem *it);
  void onAssetContextMenu(const QPoint &pos);
};
