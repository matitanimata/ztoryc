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
#include "kitsuclient.h"  // KitsuTaskPush (pending-tasks member)

#include <QTableWidget>
#include <QStringList>
#include <QVector>

class QTabWidget;
class QListWidget;
class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QGroupBox;

class ZtoryProductionPanel final : public TPanel {
  Q_OBJECT

  QTabWidget   *m_tabs  = nullptr;
  // Exit bar: shown only when the tracker runs as the standalone Production
  // room (which has no File menu) so the user can leave by opening a scene.
  QWidget      *m_exitBar      = nullptr;
  QPushButton  *m_openSceneBtn = nullptr;
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
  QLineEdit    *m_epNumEdit = nullptr;          // Ztoryc: episode number, {EPNUM}
  QLineEdit    *m_assetPatternEdit = nullptr;   // Ztoryc: asset files' convention
  QLabel       *m_kitsuLabel = nullptr;    // M5: Kitsu link status
  QPushButton  *m_kitsuConnectBtn = nullptr; // Connect… / Kitsu settings…
  bool          m_projLoading = false;
  // M5 — Kitsu sync controls, gated behind the project's opt-in flag (useKitsu):
  // the whole group is hidden unless the project enables Kitsu.
  //! Turns the Kitsu integration on for this project, at any time. It used to
  //! be an opt-in taken only when the project was created, and never
  //! revisitable.
  QCheckBox    *m_useKitsuCheck   = nullptr;
  QGroupBox    *m_kitsuGroup      = nullptr;
  QPushButton  *m_kitsuUploadBtn = nullptr;
  // Ztoryc: the one Sync button (2026-09-27, Franco: «un unico tasto»). The
  // Sync itself is ZtoryKitsuSync; the tracker shows it.
  QPushButton  *m_kitsuSyncBtn = nullptr;
  void onKitsuSync();
  void maybeAutoSync();  // once per login, when the statuses are there
  // A linked project offers to reconnect, with the saved credentials, when a
  // saved scene of it is opened (not at start-up: the app may sit on another
  // project). The binding is the project's, not the dialog's.
  void maybeAutoConnect();
  // The gate in front of it: a saved scene is open and the tracker is visible.
  void maybeAutoConnectForScene();
  QString m_autoConnectTried;  // project DB path already tried this session
  // The Sync after a login runs ONLY when asked for: «Connect and sync» in
  // the reconnect question. Any other login — the Kitsu dialog connecting by
  // itself with the saved password — leaves the Sync to the button, so the
  // episode can be checked or changed first (Franco, 2026-09-29: the Sync
  // started before he could link the episode).
  bool m_syncAfterConnect = false;
  // «Production — Episode», or the production alone, for the Kitsu labels.
  static QString kitsuBindingText();
  static bool kitsuEpisodeMissing();  // linked to a series, no episode
  QCheckBox    *m_kitsuHandlesCheck = nullptr;
  QSpinBox     *m_kitsuHandlesSpin  = nullptr;
  QLabel       *m_kitsuSyncLabel = nullptr;
  // Assets tab
  QTableWidget *m_assetTable = nullptr;
  QStringList   m_assetTaskCols;
  bool          m_assetLoading = false;
  // Workflows tab
  QListWidget  *m_techList     = nullptr;
  QListWidget  *m_taskTypeList = nullptr;
  bool          m_wfLoading    = false;

  QListWidget  *m_assetTypeList     = nullptr;  // custom asset types
  QListWidget  *m_assetTaskTypeList = nullptr;  // selected type's task pipeline
  bool          m_atLoading         = false;

public:
  ZtoryProductionPanel(QWidget *parent = nullptr);

protected:
  // Load the current project's DB when shown so the tracker works standalone
  // (a production manager can view/edit it without opening a .tnz scene).
  void showEvent(QShowEvent *e) override;

private:
  // Shots tab
  QWidget *buildShotsTab();
  void rebuild();                  // rebuild the shot × task matrix from ZtoryModel
  // One rebuild when control returns to the event loop, however many model
  // signals asked for it: a paste of one shot re-syncs EVERY shot's panels,
  // and each emitted shotDataChanged rebuilt the whole table — two thirds of
  // a six-second paste (measured with `sample`, 2026-09-28).
  void scheduleRebuild();
  bool m_rebuildScheduled = false;
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
  // Kitsu sync actions (Project tab); enabled only when the project is linked.
  void onKitsuUpload();            // upload per-shot clips from a chosen folder
  void updateKitsuButtons();       // enable/disable sync buttons by link state
  // Assets tab
  QWidget *buildAssetsTab();
  void rebuildAssets();            // rebuild the asset × task matrix from ZtoryModel
  void editAssetCell(int row, int col);
  // After a change of workflow (shots) or type (assets): their tasks on Kitsu
  // that belong to the old one are listed, and the user picks what to delete
  // — only the untouched ones, or all of them. Nothing without Kitsu.
  void offerKitsuCleanup(bool isAsset, const QStringList &uuids);
  // The second half: lists the leftovers and asks. `created` = new tasks just
  // made on Kitsu, reported in the same answer.
  void askStaleTasks(bool isAsset, const QVector<KitsuStaleQuery> &queries,
                     int created);
  // Workflows tab
  QWidget *buildWorkflowsTab();
  void reloadWorkflowsTab();        // model → workflow (technique) list
  void reloadTaskTypeList();        // selected workflow → its task-type list
  void applyTaskTypesToTechnique(); // task-type list → selected workflow + persist

  QWidget *buildAssetTypesTab();
  // Breakdown: which assets each shot needs (Kitsu calls it «casting»).
  // Written here or on Kitsu; the Sync merges the two.
  QWidget *buildBreakdownTab();
  void rebuildBreakdown();
  void onBreakdownContextMenu(const QPoint &pos);
  // Chiede il file di un asset e lo lega. Condivisa fra scheda Assets e
  // scheda Breakdown: due copie divergono, e la seconda dimentica il
  // filtro .tnz dei personaggi.
  bool linkAssetFileInteractive(int assetIndex);
  // The link dot + tooltip on an asset's name cell (see the .cpp).
  void showAssetLink(QTableWidgetItem *item, const Asset &a,
                     QHash<QString, QFileInfoList> *dirCache);
  // Il PSD da riggare di un personaggio (importato alla creazione della sua scena).
  bool linkAssetRigPsdInteractive(int assetIndex);
  // Renames a nearly-matching file to the asset's exact name (asks first).
  void renameAssetFile(int row, const QString &file, const QString &newName);
  // Opzioni PSD di UN asset. Registra solo cio' che differisce dal
  // default di progetto, cosi' cambiare il default continua ad arrivare
  // qui: salvare anche i campi uguali li congelerebbe.
  bool editAssetPsdOptions(int assetIndex);
  QLineEdit *m_propsDirEdit = nullptr;
  QComboBox *m_importModeCombo = nullptr;
  QComboBox *m_psdLoadAsCombo = nullptr;
  QComboBox *m_psdLevelNameCombo = nullptr;
  QComboBox *m_psdGroupsCombo = nullptr;
  QCheckBox *m_psdSubSceneCheck = nullptr;
  QLineEdit *m_bgDirEdit = nullptr;
  QLineEdit *m_modelSheetDirEdit = nullptr;
  QTableWidget *m_breakdownTable = nullptr;
  // Scrittura del breakdown a mano: fino alla 0.13.0 lo scriveva solo Kitsu.
  void onBreakdownAdd();
  void onBreakdownRemove();
  void onBreakdownFromDialogue();
  void reloadAssetTypesTab();        // model → asset-type list
  void reloadAssetTaskTypeList();    // selected asset type → its task pipeline
  void applyAssetTaskTypesToType();  // task list → selected asset type + persist

private slots:
  void onModelChanged();
  void onCellClicked(int row, int col);
  void onShotContextMenu(const QPoint &pos);    // batch edit on selected task cells
  void onAssetCellClicked(int row, int col);
  void onAssetItemChanged(QTableWidgetItem *it);
  void onAssetContextMenu(const QPoint &pos);
  // Leave the standalone Production room: open the Startup screen so the user
  // can load or create a scene (re-applies a normal workflow's rooms).
  void onOpenScene();
};
