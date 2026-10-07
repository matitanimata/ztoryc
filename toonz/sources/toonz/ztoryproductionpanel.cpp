#include "ztoryproductionpanel.h"

#include "ztorymodel.h"
#include "storyboardpanel.h"
#include "kitsuconnectdialog.h"
#include "startuppopup.h"
#include "toonz/preferences.h"  // getCurrentRoomChoice (exit-bar gating)

#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"
#include "ztorycharacter.h"  // declareCharacterScene
#include "ztorytaskflow.h"
#include "ztorykitsusync.h"

#include "tundo.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"

#include "xlsxdocument.h"
#include "xlsxformat.h"
#include "xlsxdatavalidation.h"
#include "xlsxconditionalformatting.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"
#include "xlsxcellreference.h"

#include <QVBoxLayout>
#include <QRegularExpression>
#include <QFileDialog>
#include <QMessageBox>
#include <QImage>
#include <QFileInfo>

#include <set>
#include <algorithm>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QColor>
#include <QMenu>
#include <QAction>
#include <QPixmap>
#include <QApplication>
#include <QCursor>
#include <QInputDialog>
#include <QLineEdit>
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QAbstractItemModel>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QProgressBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QPainter>
#include <QIcon>

#include <cassert>

namespace {

// Format a 1-based cumulative frame number as MM:SS:FR timecode (0-based, so the
// first frame reads 00:00:00). Used for the In-Out column.
QString frameToTimecode(int frame1Based, int fps) {
  if (fps <= 0) fps = 25;
  int f = frame1Based > 0 ? frame1Based - 1 : 0;  // 0-based for timecode
  const int totalSec = f / fps;
  return QString("%1:%2:%3")
      .arg(totalSec / 60, 2, 10, QChar('0'))
      .arg(totalSec % 60, 2, 10, QChar('0'))
      .arg(f % fps,       2, 10, QChar('0'));
}

// The palette lives in the model, shared with the Board's export.
QColor statusColor(TaskStatus s) { return ZtoryModel::taskStatusColor(s); }

// Light statuses read better with black text.
bool isLightStatus(TaskStatus s) {
  return s == TaskStatus::Todo || s == TaskStatus::Ready;
}

const QVector<TaskStatus> &kAllStatuses = ZtoryModel::allTaskStatuses();

// The Board alive, if any: a tracker edit marks the scene modified through it
// (step 3b — the .ztoryc is written with the scene's ⌘S, by the Board or by
// the model when no Board is alive).
StoryboardPanel *findBoard() {
  for (QWidget *w : QApplication::allWidgets())
    if (auto *b = qobject_cast<StoryboardPanel *>(w)) return b;
  return nullptr;
}

// Tracker edits change production.ztrack / the .ztoryc, never the .tnz scene.
// In standalone mode (the Production room opened without a scene) the undo
// registration and TUndoManager notifications would otherwise flag the empty
// untitled scene "modified" (Untitled*), triggering a spurious "save scene?"
// prompt when the user later loads/creates a scene to leave. Clear it after
// every tracker persist — a titled scene is left untouched.
void clearUntitledSceneDirty() {
  TApp *app = TApp::instance();
  if (app->getCurrentScene() && app->getCurrentScene()->getScene() &&
      app->getCurrentScene()->getScene()->isUntitled())
    app->getCurrentScene()->setDirtyFlag(false);
}

void persistViaBoard() {
  // Step 3b: the scene is modified; ⌘S writes the .ztoryc.
  if (auto *b = findBoard()) b->markShotDocumentChanged();
  clearUntitledSceneDirty();
}

// Project-level data (shot aggregation, assets, team, meta) lives in
// production.ztrack, not the .ztoryc.
void persistProjectDb() {
  ZtoryModel::instance()->saveProjectDb();
  clearUntitledSceneDirty();
}
// Assets are project-level: they live in production.ztrack, not the .ztoryc.
void persistAssets() { persistProjectDb(); }

// Undo for a single per-task status edit. Keyed by stable shotLabel so it
// survives shot reordering between the edit and its undo.
class StatusEditUndo final : public TUndo {
  QString    m_shotLabel, m_taskType;
  TaskStatus m_old, m_new;

public:
  StatusEditUndo(const QString &shotLabel, const QString &taskType,
                 TaskStatus oldS, TaskStatus newS)
      : m_shotLabel(shotLabel), m_taskType(taskType), m_old(oldS), m_new(newS) {}

  void undo() const override {
    ZtoryModel::instance()->setShotTaskStatusByLabel(m_shotLabel, m_taskType, m_old);
    persistViaBoard();
  }
  void redo() const override {
    ZtoryModel::instance()->setShotTaskStatusByLabel(m_shotLabel, m_taskType, m_new);
    persistViaBoard();
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Set %1 status").arg(m_taskType);
  }
};

// Undo for a per-task assignees edit. Same keying as StatusEditUndo.
class AssigneeEditUndo final : public TUndo {
  QString     m_shotLabel, m_taskType;
  QStringList m_old, m_new;

public:
  AssigneeEditUndo(const QString &shotLabel, const QString &taskType,
                   const QStringList &oldA, const QStringList &newA)
      : m_shotLabel(shotLabel), m_taskType(taskType), m_old(oldA), m_new(newA) {}

  void undo() const override {
    ZtoryModel::instance()->setShotTaskAssigneesByLabel(m_shotLabel, m_taskType, m_old);
    persistViaBoard();
  }
  void redo() const override {
    ZtoryModel::instance()->setShotTaskAssigneesByLabel(m_shotLabel, m_taskType, m_new);
    persistViaBoard();
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Set %1 assignees").arg(m_taskType);
  }
};

// Parse a comma-separated assignee string into a trimmed, non-empty list.
QStringList parseAssignees(const QString &text) {
  QStringList out;
  for (const QString &p : text.split(',', Qt::SkipEmptyParts)) {
    QString t = p.trimmed();
    if (!t.isEmpty()) out << t;
  }
  return out;
}

// Shared status/assignee picker used by both the Shots and Assets matrices.
struct TaskEditResult {
  enum Kind { None, Status, Assignees } kind = None;
  TaskStatus  status = TaskStatus::Todo;
  QStringList assignees;
};

// Assignee picker: team checkboxes (+ free text), or plain text if no team.
// Returns false if cancelled; otherwise fills `out`.
bool pickAssignees(QWidget *parent, const QString &taskType,
                   const QStringList &current, QStringList &out) {
  const QStringList team = ZtoryModel::instance()->team();
  if (team.isEmpty()) {
    bool ok = false;
    QString text = QInputDialog::getText(
        parent, QObject::tr("Assignees"),
        QObject::tr("People assigned to %1 (comma-separated):").arg(taskType),
        QLineEdit::Normal, current.join(", "), &ok);
    if (!ok) return false;
    out = parseAssignees(text);
    return true;
  }
  QDialog dlg(parent);
  dlg.setWindowTitle(QObject::tr("Assignees — %1").arg(taskType));
  QVBoxLayout *lay = new QVBoxLayout(&dlg);
  lay->addWidget(new QLabel(QObject::tr("Assign to:"), &dlg));
  QListWidget *list = new QListWidget(&dlg);
  for (const QString &p : team) {
    auto *it = new QListWidgetItem(p, list);
    it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
    it->setCheckState(current.contains(p) ? Qt::Checked : Qt::Unchecked);
  }
  lay->addWidget(list);
  QStringList extras;
  for (const QString &a : current)
    if (!team.contains(a)) extras << a;
  lay->addWidget(new QLabel(QObject::tr("Others (comma-separated):"), &dlg));
  QLineEdit *extraEdit = new QLineEdit(extras.join(", "), &dlg);
  lay->addWidget(extraEdit);
  auto *bb = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  lay->addWidget(bb);
  if (dlg.exec() != QDialog::Accepted) return false;
  out.clear();
  for (int r = 0; r < list->count(); r++)
    if (list->item(r)->checkState() == Qt::Checked)
      out << list->item(r)->text();
  out += parseAssignees(extraEdit->text());
  return true;
}

TaskEditResult pickTaskEdit(QWidget *parent, const QString &taskType,
                            TaskStatus oldStatus, const QStringList &oldAssign) {
  TaskEditResult res;
  QMenu menu(parent);
  for (TaskStatus s : kAllStatuses) {
    QPixmap pm(14, 14);
    pm.fill(statusColor(s));
    QAction *a = menu.addAction(QIcon(pm), ZtoryModel::taskStatusLabel(s));
    a->setCheckable(true);
    a->setChecked(s == oldStatus);
    a->setData(static_cast<int>(s));
  }
  menu.addSeparator();
  QAction *assignAct = menu.addAction(QObject::tr("Set assignees…"));

  QAction *chosen = menu.exec(QCursor::pos());
  if (!chosen) return res;

  if (chosen != assignAct) {
    res.kind   = TaskEditResult::Status;
    res.status = static_cast<TaskStatus>(chosen->data().toInt());
    return res;
  }
  QStringList newAssign;
  if (!pickAssignees(parent, taskType, oldAssign, newAssign)) return res;
  res.kind      = TaskEditResult::Assignees;
  res.assignees = newAssign;
  return res;
}

// Undo for project-shot task edits — keyed by stable shot uuid (survives
// reordering and cross-storyboard aggregation). Persists to project DB.
// Ztoryc: it keeps every change the edit made — a Done also readies the next
// task — and takes them all back, so ⌘Z does not leave that Ready behind.
class ProjectShotStatusUndo final : public TUndo {
  QString    m_uuid, m_taskType;
  TaskStatus m_old, m_new;
  mutable QVector<ZtoryTaskFlow::Change> m_changes;
public:
  ProjectShotStatusUndo(const QString &uuid, const QString &taskType,
                        TaskStatus o, TaskStatus n,
                        const QVector<ZtoryTaskFlow::Change> &changes)
      : m_uuid(uuid), m_taskType(taskType), m_old(o), m_new(n),
        m_changes(changes) {}
  void undo() const override {
    ZtoryTaskFlow::revert(m_changes);
    persistProjectDb();
    emit ZtoryModel::instance()->taskStatusChanged();
  }
  void redo() const override {
    m_changes.clear();
    ZtoryTaskFlow::setStatus(ZtoryTaskFlow::Entity::Shot, m_uuid, m_taskType,
                             m_new, ZtoryTaskFlow::Origin::User,
                             /*batch=*/true, &m_changes);
    persistProjectDb();
    emit ZtoryModel::instance()->taskStatusChanged();
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Set %1 status").arg(m_taskType);
  }
};

class ProjectShotAssigneeUndo final : public TUndo {
  QString     m_uuid, m_taskType;
  QStringList m_old, m_new;
public:
  ProjectShotAssigneeUndo(const QString &uuid, const QString &taskType,
                          const QStringList &o, const QStringList &n)
      : m_uuid(uuid), m_taskType(taskType), m_old(o), m_new(n) {}
  void undo() const override {
    ZtoryModel::instance()->setProjectShotAssigneesByUuid(m_uuid, m_taskType, m_old);
    persistProjectDb();
  }
  void redo() const override {
    ZtoryModel::instance()->setProjectShotAssigneesByUuid(m_uuid, m_taskType, m_new);
    persistProjectDb();
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Set %1 assignees").arg(m_taskType);
  }
};

// Undo for asset task edits — keyed by the asset's stable uuid.
// Ztoryc: like ProjectShotStatusUndo, it takes back the changes the edit caused.
class AssetStatusUndo final : public TUndo {
  QString m_uuid, m_taskType;
  TaskStatus m_old, m_new;
  mutable QVector<ZtoryTaskFlow::Change> m_changes;
public:
  AssetStatusUndo(const QString &uuid, const QString &type, TaskStatus o,
                  TaskStatus n, const QVector<ZtoryTaskFlow::Change> &changes)
      : m_uuid(uuid), m_taskType(type), m_old(o), m_new(n),
        m_changes(changes) {}
  void undo() const override {
    ZtoryTaskFlow::revert(m_changes);
    persistAssets();
    emit ZtoryModel::instance()->assetsChanged();
  }
  void redo() const override {
    m_changes.clear();
    ZtoryTaskFlow::setStatus(ZtoryTaskFlow::Entity::Asset, m_uuid, m_taskType,
                             m_new, ZtoryTaskFlow::Origin::User,
                             /*batch=*/true, &m_changes);
    persistAssets();
    emit ZtoryModel::instance()->assetsChanged();
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Set asset %1 status").arg(m_taskType);
  }
};

class AssetAssigneeUndo final : public TUndo {
  QString m_uuid, m_taskType;
  QStringList m_old, m_new;
public:
  AssetAssigneeUndo(const QString &uuid, const QString &type,
                    const QStringList &o, const QStringList &n)
      : m_uuid(uuid), m_taskType(type), m_old(o), m_new(n) {}
  void undo() const override {
    ZtoryModel::instance()->setAssetTaskAssigneesByUuid(m_uuid, m_taskType, m_old);
    persistAssets();
  }
  void redo() const override {
    ZtoryModel::instance()->setAssetTaskAssigneesByUuid(m_uuid, m_taskType, m_new);
    persistAssets();
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Set asset %1 assignees").arg(m_taskType);
  }
};

}  // namespace

//-----------------------------------------------------------------------------

ZtoryProductionPanel::ZtoryProductionPanel(QWidget *parent) : TPanel(parent) {
  m_tabs = new QTabWidget(this);
  m_tabs->setDocumentMode(true);
  m_tabs->addTab(buildProjectTab(),   QObject::tr("Project"));
  m_tabs->addTab(buildShotsTab(),     QObject::tr("Shots"));
  m_tabs->addTab(buildTeamTab(),      QObject::tr("Team"));
  m_tabs->addTab(buildAssetsTab(),    QObject::tr("Assets"));
  m_tabs->addTab(buildWorkflowsTab(), QObject::tr("Workflows"));
  m_tabs->addTab(buildAssetTypesTab(), QObject::tr("Asset Types"));
  m_tabs->addTab(buildBreakdownTab(), QObject::tr("Breakdown"));

  // Exit bar — only meaningful when the tracker IS the standalone Production
  // room (no File menu there). A button reopens the Startup screen so the user
  // can load/create a scene and thereby leave the room. Hidden when docked in a
  // normal room; toggled in showEvent() based on the current room.
  m_exitBar     = new QWidget(this);
  auto *barLay  = new QHBoxLayout(m_exitBar);
  barLay->setContentsMargins(4, 4, 4, 2);
  m_openSceneBtn = new QPushButton(QObject::tr("← Open or Create Scene…"),
                                   m_exitBar);
  m_openSceneBtn->setToolTip(
      QObject::tr("Leave the Production room: load or create a scene"));
  connect(m_openSceneBtn, &QPushButton::clicked, this,
          &ZtoryProductionPanel::onOpenScene);
  barLay->addWidget(m_openSceneBtn);
  barLay->addStretch();
  m_exitBar->hide();  // shown only in the standalone Production room

  // TPanel (a TDockWidget) mounts its content via setWidget, not setLayout.
  QWidget *container = new QWidget(this);
  auto *outer        = new QVBoxLayout(container);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);
  outer->addWidget(m_exitBar);
  outer->addWidget(m_tabs);
  setWidget(container);

  ZtoryModel *m = ZtoryModel::instance();
  connect(m, &ZtoryModel::modelReset,        this, &ZtoryProductionPanel::onModelChanged);
  connect(m, &ZtoryModel::shotAdded,         this, [this](int) { scheduleRebuild(); });
  connect(m, &ZtoryModel::shotRemovedAt,     this, [this](int) { scheduleRebuild(); });
  connect(m, &ZtoryModel::shotDataChanged,   this, [this](int) { scheduleRebuild(); });
  connect(m, &ZtoryModel::taskStatusChanged, this, [this] { rebuild(); reloadProjectTab(); });
  connect(m, &ZtoryModel::assetsChanged,     this, [this] { rebuildAssets(); });
  connect(m, &ZtoryModel::productionReloaded, this, &ZtoryProductionPanel::onModelChanged);

  // --- Kitsu (Project tab) -------------------------------------------------
  // What Kitsu sends back is applied by ZtoryKitsuSync, once for the whole
  // app; the tracker only shows progress and results.
  KitsuClient *kc = KitsuClient::instance();
  connect(kc, &KitsuClient::shotsPushProgress, this, [this](const QString &msg) {
    if (m_kitsuSyncLabel) { m_kitsuSyncLabel->setStyleSheet(QString()); m_kitsuSyncLabel->setText(msg); }
  });
  // Ztoryc: say what the automatic push did when it did NOT simply succeed —
  // above all when Kitsu had moved on and its status was kept.
  connect(kc, &KitsuClient::transitionPushed, this,
          [this](int, const QString &, const QString &, int result, int,
                 const QString &msg) {
            if (!m_kitsuSyncLabel) return;
            if (result == KitsuClient::TrPushed ||
                result == KitsuClient::TrAlreadyThere)
              return;
            m_kitsuSyncLabel->setStyleSheet(result == KitsuClient::TrConflict
                                                ? "color:#FFB000;"
                                                : "color:#FF3860;");
            m_kitsuSyncLabel->setText(msg);
          });
  connect(kc, &KitsuClient::previewsUploaded, this, [this](bool ok, int, const QString &msg) {
    if (m_kitsuSyncLabel) {
      m_kitsuSyncLabel->setStyleSheet(ok ? "color:#22D160;" : "color:#FF3860;");
      m_kitsuSyncLabel->setText(msg);
    }
  });
  // Ztoryc: the Sync and what Kitsu sends back live in ZtoryKitsuSync
  // (2026-09-27); here only what the tracker shows.
  ZtoryKitsuSync *sync = ZtoryKitsuSync::instance();
  connect(sync, &ZtoryKitsuSync::progress, this, [this](const QString &text) {
    if (!m_kitsuSyncLabel) return;
    m_kitsuSyncLabel->setStyleSheet(QString());
    // Every line carries the target, so it stays readable for the whole Sync.
    m_kitsuSyncLabel->setText(kitsuBindingText() + "\n" + text);
  });
  connect(sync, &ZtoryKitsuSync::finished, this,
          [this](bool ok, bool warn, const QString &summary) {
            if (m_kitsuSyncLabel) {
              m_kitsuSyncLabel->setStyleSheet(
                  !ok ? "color:#FF3860;" : warn ? "color:#FFB000;" : "color:#22D160;");
              m_kitsuSyncLabel->setText(kitsuBindingText() + "\n" + summary);
            }
            rebuildBreakdown();  // step 5 may have changed it
            updateKitsuButtons();
          });
  connect(sync, &ZtoryKitsuSync::assetTypesChanged, this,
          [this] { reloadAssetTypesTab(); });
  connect(sync, &ZtoryKitsuSync::teamChanged, this, [this](int added) {
    rebuild();
    if (m_kitsuSyncLabel && !ZtoryKitsuSync::instance()->isRunning()) {
      m_kitsuSyncLabel->setStyleSheet("color:#22D160;");
      m_kitsuSyncLabel->setText(tr("Team from Kitsu: %1 added.").arg(added));
    }
  });
  connect(kc, &KitsuClient::teamPulled, this,
          [this](bool ok, const QVector<KitsuPerson> &, const QString &msg) {
    // The team itself is applied by ZtoryKitsuSync; the label only outside a
    // Sync, whose summary it must not cover.
    if (!m_kitsuSyncLabel || ZtoryKitsuSync::instance()->isRunning()) return;
    m_kitsuSyncLabel->setStyleSheet(ok ? "color:#22D160;" : "color:#FF3860;");
    m_kitsuSyncLabel->setText(msg);
  });
  // As soon as we're connected, pull the project's team so the assignee picker is
  // populated from Kitsu (Kitsu is authoritative on the roster while linked).
  // The reconnect question waits for a SCENE: at start-up the app may be on
  // a project nobody is going to work on (Franco, 2026-09-29), so asking
  // there named the wrong production. A saved scene says which project it is.
  // Deferred: the question is modal and the scene is still being set up.
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, [this]() {
            QTimer::singleShot(0, this, [this]() { maybeAutoConnectForScene(); });
          });
  connect(kc, &KitsuClient::loginFinished, this, [this](bool ok, const QString &) {
    if (!ok) m_syncAfterConnect = false;  // no login, no Sync to run
    updateKitsuButtons();  // connection line + button text + Push/Pull
    ZtoryModel *mm = ZtoryModel::instance();
    if (ok && mm->isKitsuLinked())
      KitsuClient::instance()->pullTeam(mm->kitsuProjectId());
    maybeAutoSync();
  });
  // The login asks for the statuses after it answers: the auto Sync waits
  // for them (without, start() refuses — every status would read as Todo).
  connect(kc, &KitsuClient::taskStatusesFetched, this,
          [this](const QVector<KitsuTaskStatus> &) { maybeAutoSync(); });
  // Panel opened while already connected+linked (e.g. reopened room): pull now.
  if (kc->isLoggedIn() && m->isKitsuLinked())
    kc->pullTeam(m->kitsuProjectId());

  // Rebuild thumbnails when the Board finishes rendering a preview (panel 0 only —
  // panel 0 is the shot thumbnail). Debounced: one rebuild after a burst of renders.
  auto *thumbDebounce = new QTimer(this);
  thumbDebounce->setSingleShot(true);
  thumbDebounce->setInterval(400);
  connect(thumbDebounce, &QTimer::timeout, this, [this] { rebuild(); });
  connect(m, &ZtoryModel::previewUpdated, this, [thumbDebounce](int /*si*/, int pi) {
    if (pi == 0) thumbDebounce->start();  // only panel 0 is used as shot thumbnail
  });

  rebuild();
  reloadTeamTab();
  reloadProjectTab();
  rebuildAssets();
  reloadWorkflowsTab();
  reloadAssetTypesTab();
  // Il breakdown e' persistito nel .ztoryc: va ridisegnato all'apertura, non
  // solo dopo un pull, o la scheda sembra vuota finche' non si ricontatta Kitsu.
  rebuildBreakdown();
}

//-----------------------------------------------------------------------------

void ZtoryProductionPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  // Standalone use: the Board normally drives loadProjectDb() when a scene opens,
  // but a production manager may want to view/edit the tracker without opening a
  // .tnz. loadProjectDb() locates the DB from the CURRENT PROJECT (not the
  // scene) and only touches project-level data (shots list, team, assets,
  // techniques) — never the open scene's shots — so it's safe to (re)load here.
  // This also picks up a project switch made while the tracker was hidden.
  ZtoryModel::instance()->loadProjectDb();
  onModelChanged();  // rebuild every tab from the freshly loaded DB
  // Also when the tracker comes into view AFTER a scene was opened (its room
  // may be built later than the scene load, or entered by hand): the scene
  // test inside keeps it silent at start-up.
  QTimer::singleShot(0, this, [this]() { maybeAutoConnectForScene(); });

  // Show the exit bar only when this tracker IS the standalone Production room.
  // Gate on the room *choice* ("Production", the room-set folder name), not the
  // room's display name (which is "Production Tracker").
  bool standalone =
      (Preferences::instance()->getCurrentRoomChoice() == "Production");
  if (m_exitBar) m_exitBar->setVisible(standalone);
}

//-----------------------------------------------------------------------------

void ZtoryProductionPanel::onOpenScene() {
  // Reopen the Startup screen (full DefaultMode: create + load + recents,
  // including the Production Tracker tile). Loading or creating a scene from
  // here re-applies a normal workflow's rooms, leaving the Production room.
  if (StartupPopup *p = StartupPopup::visibleDefaultInstance()) {
    p->raise();
    p->activateWindow();
    return;
  }
  StartupPopup *popup = new StartupPopup(StartupPopup::DefaultMode);
  popup->show();
  popup->raise();
  popup->activateWindow();
}

//-----------------------------------------------------------------------------

void ZtoryProductionPanel::onModelChanged() {
  rebuild();
  reloadTeamTab();
  reloadProjectTab();
  rebuildAssets();
  reloadWorkflowsTab();
  reloadAssetTypesTab();
  // Il breakdown e' persistito nel .ztoryc: va ridisegnato all'apertura, non
  // solo dopo un pull, o la scheda sembra vuota finche' non si ricontatta Kitsu.
  rebuildBreakdown();
}

//-----------------------------------------------------------------------------

QWidget *ZtoryProductionPanel::buildShotsTab() {
  QWidget *w = new QWidget(this);
  m_table    = new QTableWidget(w);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
  m_table->setContextMenuPolicy(Qt::CustomContextMenu);
  m_table->verticalHeader()->setVisible(false);
  m_table->setShowGrid(true);
  m_table->setAlternatingRowColors(false);
  connect(m_table, &QTableWidget::cellClicked, this,
          &ZtoryProductionPanel::onCellClicked);
  connect(m_table, &QTableWidget::cellDoubleClicked, this,
          [this](int r, int c) { editCell(r, c); });  // single-cell quick edit
  connect(m_table, &QWidget::customContextMenuRequested, this,
          &ZtoryProductionPanel::onShotContextMenu);
  auto *lay = new QVBoxLayout(w);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->addWidget(m_table);
  // Full-project export (all storyboards + all tabs).
  auto *exportRow = new QHBoxLayout();
  exportRow->addStretch();
  auto *exportBtn =
      new QPushButton(QObject::tr("  Export Project Spreadsheet…"), w);
  exportBtn->setIcon(createQIcon("ztoryc_export_spreadsheet"));
  exportBtn->setIconSize(QSize(28, 20));  // wider than the Board's (whole project)
  exportBtn->setToolTip(QObject::tr(
      "Export one spreadsheet with every storyboard's shots plus the Project, "
      "Team, Assets and Workflows tabs."));
  connect(exportBtn, &QPushButton::clicked, this,
          &ZtoryProductionPanel::exportFullProject);
  exportRow->addWidget(exportBtn);
  lay->addLayout(exportRow);
  return w;
}

//-----------------------------------------------------------------------------
// Full-project XLSX export — every storyboard's shots + Team/Assets/Workflows/
// Project, sourced entirely from the project DB (no open scene required).
// Thumbnails come from the on-disk thumb cache keyed by shot uuid.

void ZtoryProductionPanel::exportFullProject() {
  using namespace QXlsx;
  ZtoryModel *m = ZtoryModel::instance();

  const std::vector<ProjectShot> &shots = m->projectShots();
  const auto frameRanges = m->projectShotFrameRanges();  // cumulative in/out
  if (shots.empty()) {
    QMessageBox::information(this, QObject::tr("Export Full Project"),
                            QObject::tr("The project has no shots to export."));
    return;
  }

  // Suggested filename: production[_episode]_project.xlsx
  QString base = m->production().trimmed();
  QString ep   = m->episode().trimmed();
  if (!ep.isEmpty()) base = (base.isEmpty() ? ep : base + "_" + ep);
  base = base.isEmpty() ? QString("project") : base + "_project";
  base.replace(' ', '_').replace('/', '_');
  QString startDir;
  if (!m->projectDbPath().isEmpty())
    startDir = QFileInfo(m->projectDbPath()).absolutePath();
  QString suggested = startDir.isEmpty() ? base + ".xlsx"
                                         : startDir + "/" + base + ".xlsx";
  QString path = QFileDialog::getSaveFileName(
      this, QObject::tr("Export Full Project Spreadsheet"), suggested,
      QObject::tr("Excel Spreadsheet (*.xlsx)"));
  if (path.isEmpty()) return;
  if (!path.endsWith(".xlsx", Qt::CaseInsensitive)) path += ".xlsx";

  const int fps = m->fps() > 0 ? m->fps() : 24;

  // ── Shared formats ────────────────────────────────────────────────────────
  Format titleFmt; titleFmt.setFontBold(true); titleFmt.setFontSize(14);
  Format subFmt;   subFmt.setFontBold(true);
  Format hdrFmt;
  hdrFmt.setFontBold(true);
  hdrFmt.setFontColor(Qt::white);
  hdrFmt.setPatternBackgroundColor(QColor("#2C3E50"));
  hdrFmt.setHorizontalAlignment(Format::AlignHCenter);
  hdrFmt.setVerticalAlignment(Format::AlignVCenter);
  hdrFmt.setTextWrap(true);
  Format cellFmt;   cellFmt.setVerticalAlignment(Format::AlignVCenter);
  cellFmt.setTextWrap(true);
  Format centerFmt;
  centerFmt.setHorizontalAlignment(Format::AlignHCenter);
  centerFmt.setVerticalAlignment(Format::AlignVCenter);
  Format naFmt;
  naFmt.setHorizontalAlignment(Format::AlignHCenter);
  naFmt.setVerticalAlignment(Format::AlignVCenter);
  naFmt.setFontColor(QColor("#BBBBBB"));
  naFmt.setPatternBackgroundColor(QColor("#F0F0F0"));

  const QString statusList = "\"TODO,READY,WIP,WFA,RETAKE,DONE\"";

  Document xlsx;

  // ── Helper: technique → ordered task types ────────────────────────────────
  auto taskTypesOfTech = [&](const QString &techName) -> QStringList {
    QString tn = techName.isEmpty() ? m->defaultTechnique() : techName;
    const Technique *t = m->findTechnique(tn);
    return t ? t->taskTypes : QStringList();
  };

  // ── Sort shots by source, then sequence, then label ───────────────────────
  std::vector<int> order(shots.size());
  for (int i = 0; i < (int)shots.size(); i++) order[i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    if (shots[a].source != shots[b].source) return shots[a].source < shots[b].source;
    if (shots[a].seq != shots[b].seq)       return shots[a].seq < shots[b].seq;
    return shots[a].label < shots[b].label;
  });

  // ── writeShotsSheet: one sheet for a given subset of shots + task columns ──
  const QStringList fixedCols = {
      QObject::tr("Thumbnail"), QObject::tr("Storyboard"),
      QObject::tr("Sequence"),  QObject::tr("Shot"),
      QObject::tr("Frames"),    QObject::tr("In-Out"),
      QObject::tr("Workflow")};
  const int firstTaskCol = fixedCols.size() + 1;  // 1-based
  const int headerRow    = 4;
  const int firstDataRow = 5;

  auto writeShotsSheet = [&](const QString &sheetName, const QString &subtitle,
                             const std::vector<int> &shotIdxs,
                             const QStringList &cols) {
    xlsx.write(1, 1, m->production().isEmpty() ? QObject::tr("Production")
                                               : m->production(), titleFmt);
    if (!subtitle.isEmpty()) xlsx.write(2, 1, subtitle, subFmt);

    for (int c = 0; c < fixedCols.size(); c++)
      xlsx.write(headerRow, c + 1, fixedCols[c], hdrFmt);
    for (int t = 0; t < cols.size(); t++) {
      int sc = firstTaskCol + t * 2;
      xlsx.write(headerRow, sc,     cols[t],                       hdrFmt);
      xlsx.write(headerRow, sc + 1, cols[t] + QObject::tr(" — Who"), hdrFmt);
    }
    xlsx.setRowHeight(headerRow, 28);
    xlsx.setColumnWidth(1, 23); xlsx.setColumnWidth(2, 20);
    xlsx.setColumnWidth(3, 12); xlsx.setColumnWidth(4, 10);
    xlsx.setColumnWidth(5, 8);  xlsx.setColumnWidth(6, 11);
    xlsx.setColumnWidth(7, 15);
    for (int t = 0; t < cols.size(); t++) {
      xlsx.setColumnWidth(firstTaskCol + t * 2,     11);
      xlsx.setColumnWidth(firstTaskCol + t * 2 + 1, 12);
    }

    int row = firstDataRow;
    for (int si : shotIdxs) {
      const ProjectShot &ps = shots[si];

      QPixmap px = m->thumbCache().value(ps.uuid);
      if (!px.isNull()) {
        QImage thumb = px.toImage().scaled(128, 72, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
        thumb.setDotsPerMeterX(3780);  // 96 dpi
        thumb.setDotsPerMeterY(3780);
        xlsx.insertImage(row - 1, 0, thumb);  // 0-based anchor
      }
      xlsx.setRowHeight(row, 60);

      xlsx.write(row, 2, ps.source, centerFmt);
      xlsx.write(row, 3, ps.seq,    centerFmt);
      xlsx.write(row, 4, ps.label,  centerFmt);
      xlsx.write(row, 5, ps.frames, centerFmt);
      const auto fr = (si < (int)frameRanges.size()) ? frameRanges[si]
                                                     : std::make_pair(0, 0);
      xlsx.write(row, 6,
                 frameToTimecode(fr.first, fps) + "-" + frameToTimecode(fr.second, fps),
                 centerFmt);
      QString tech = ps.technique.isEmpty() ? m->defaultTechnique() : ps.technique;
      xlsx.write(row, 7, tech, centerFmt);

      QStringList applicable = taskTypesOfTech(ps.technique);
      for (int t = 0; t < cols.size(); t++) {
        int sc = firstTaskCol + t * 2;
        const QString &tt = cols[t];
        if (!applicable.contains(tt)) {
          xlsx.write(row, sc,     QString("N/A"), naFmt);
          xlsx.write(row, sc + 1, QString(),      naFmt);
          continue;
        }
        TaskState tsk = ps.tasks.value(tt);  // missing → default Todo
        Format sf;
        sf.setHorizontalAlignment(Format::AlignHCenter);
        sf.setVerticalAlignment(Format::AlignVCenter);
        sf.setFontBold(true);
        xlsx.write(row, sc,     ZtoryModel::taskStatusLabel(tsk.status), sf);
        xlsx.write(row, sc + 1, tsk.assignees.join(", "), centerFmt);
      }
      row++;
    }

    const int lastRow = row - 1;
    if (lastRow < firstDataRow) return;

    // Status dropdown on each status column.
    for (int t = 0; t < cols.size(); t++) {
      int sc = firstTaskCol + t * 2;
      DataValidation dv(DataValidation::List);
      dv.setFormula1(statusList);
      dv.addRange(firstDataRow, sc, lastRow, sc);
      dv.setAllowBlank(true);
      xlsx.addDataValidation(dv);
    }

    // Colour-by-value over all status columns.
    if (!cols.isEmpty()) {
      ConditionalFormatting cf;
      for (TaskStatus s : kAllStatuses) {
        Format f;
        f.setPatternBackgroundColor(statusColor(s));
        f.setFontColor(isLightStatus(s) ? QColor(Qt::black) : QColor(Qt::white));
        f.setFontBold(true);
        cf.addHighlightCellsRule(ConditionalFormatting::Highlight_ContainsText,
                                 ZtoryModel::taskStatusLabel(s), f);
      }
      for (int t = 0; t < cols.size(); t++)
        cf.addRange(firstDataRow, firstTaskCol + t * 2,
                    lastRow, firstTaskCol + t * 2);
      xlsx.addConditionalFormatting(cf);
    }

    int lastCol = cols.isEmpty() ? fixedCols.size()
                                 : firstTaskCol + cols.size() * 2 - 1;
    if (QXlsx::Worksheet *ws = xlsx.currentWorksheet())
      ws->setAutoFilter(QXlsx::CellRange(headerRow, 1, lastRow, lastCol));
    QString fdb = QString("='%1'!%2:%3").arg(sheetName)
        .arg(QXlsx::CellReference(headerRow, 1).toString(true, true))
        .arg(QXlsx::CellReference(lastRow, lastCol).toString(true, true));
    xlsx.defineName("_xlnm._FilterDatabase", fdb, QString(), sheetName);
  };  // writeShotsSheet

  auto sanitizeSheet = [](QString n) -> QString {
    for (QChar c : QString("\\/?*:[]")) n.replace(c, ' ');
    return n.trimmed().left(31);
  };

  // ── Project sheet — FIRST (rename the default sheet) ──────────────────────
  {
    QStringList existing = xlsx.sheetNames();
    if (existing.isEmpty()) xlsx.addSheet(QObject::tr("Project"));
    else                    xlsx.renameSheet(existing.first(), QObject::tr("Project"));
    xlsx.write(1, 1, QObject::tr("Project"), titleFmt);
    xlsx.setColumnWidth(1, 22); xlsx.setColumnWidth(2, 40);
    int r = 3;
    auto kv = [&](const QString &k, const QString &v) {
      Format kf; kf.setFontBold(true);
      xlsx.write(r, 1, k, kf);
      xlsx.write(r, 2, v, cellFmt);
      r++;
    };
    kv(QObject::tr("Production"),        m->production());
    kv(QObject::tr("Title"),             m->title());
    kv(QObject::tr("Season"),            m->season());
    kv(QObject::tr("Episode"),           m->episode());
    kv(QObject::tr("Default technique"), m->defaultTechnique());
    {
      std::set<QString> sources;
      for (const ProjectShot &ps : shots) sources.insert(ps.source);
      kv(QObject::tr("Storyboards"), QString::number((int)sources.size()));
    }
    kv(QObject::tr("Total shots"),       QString::number((int)shots.size()));
    kv(QObject::tr("Team members"),      QString::number(m->team().size()));
    kv(QObject::tr("Assets"),            QString::number((int)m->assets().size()));
  }

  // ── Overview sheet (all shots, union of task columns) ─────────────────────
  std::set<QString> usedSet;
  for (const ProjectShot &ps : shots)
    for (const QString &tt : taskTypesOfTech(ps.technique)) usedSet.insert(tt);
  QStringList allTaskCols;
  for (const QString &tt : ZtoryModel::canonicalTaskOrder())
    if (usedSet.count(tt)) { allTaskCols << tt; usedSet.erase(tt); }
  for (const QString &tt : usedSet) allTaskCols << tt;  // custom types last

  const QString overviewName = QObject::tr("Overview");
  xlsx.addSheet(overviewName);

  QString subtitle = m->title();
  if (!m->episode().isEmpty())
    subtitle += (subtitle.isEmpty() ? QString() : QString("  ·  ")) +
                QObject::tr("Episode ") + m->episode();
  writeShotsSheet(overviewName, subtitle, order, allTaskCols);

  // ── One sheet per technique actually used ─────────────────────────────────
  QStringList usedTechs;
  for (const ProjectShot &ps : shots) {
    QString tn = ps.technique.isEmpty() ? m->defaultTechnique() : ps.technique;
    if (!usedTechs.contains(tn)) usedTechs << tn;
  }
  for (const QString &tn : usedTechs) {
    std::vector<int> idxs;
    for (int si : order) {
      QString t = shots[si].technique.isEmpty() ? m->defaultTechnique()
                                                : shots[si].technique;
      if (t == tn) idxs.push_back(si);
    }
    QString sname = sanitizeSheet(tn);
    if (sname.compare(overviewName, Qt::CaseInsensitive) == 0) sname += " (wf)";
    if (!xlsx.addSheet(sname)) continue;
    writeShotsSheet(sname, tn, idxs, taskTypesOfTech(tn));
  }

  // ── Team sheet ────────────────────────────────────────────────────────────
  if (xlsx.addSheet(QObject::tr("Team"))) {
    xlsx.write(1, 1, QObject::tr("Team"), titleFmt);
    xlsx.write(3, 1, QObject::tr("Member"), hdrFmt);
    xlsx.setColumnWidth(1, 30);
    int r = 4;
    for (const QString &name : m->team()) xlsx.write(r++, 1, name, cellFmt);
  }

  // ── Assets sheet ──────────────────────────────────────────────────────────
  if (xlsx.addSheet(QObject::tr("Assets"))) {
    const std::vector<Asset> &assets = m->assets();
    std::set<QString> assetTaskSet;
    for (const Asset &a : assets)
      for (auto it = a.tasks.begin(); it != a.tasks.end(); ++it)
        assetTaskSet.insert(it.key());
    QStringList assetTaskCols;
    for (const QString &k : assetTaskSet) assetTaskCols << k;

    xlsx.write(1, 1, QObject::tr("Assets"), titleFmt);
    const QStringList aFixed = {QObject::tr("Type"), QObject::tr("Name"),
                                QObject::tr("Tags")};
    for (int c = 0; c < aFixed.size(); c++) xlsx.write(3, c + 1, aFixed[c], hdrFmt);
    for (int t = 0; t < assetTaskCols.size(); t++)
      xlsx.write(3, aFixed.size() + 1 + t, assetTaskCols[t], hdrFmt);
    xlsx.setColumnWidth(1, 14); xlsx.setColumnWidth(2, 24);
    xlsx.setColumnWidth(3, 24);

    int r = 4;
    for (const Asset &a : assets) {
      xlsx.write(r, 1, a.type, cellFmt);
      xlsx.write(r, 2, a.name, cellFmt);
      xlsx.write(r, 3, a.tags.join(", "), cellFmt);
      for (int t = 0; t < assetTaskCols.size(); t++) {
        int col = aFixed.size() + 1 + t;
        if (!a.tasks.contains(assetTaskCols[t])) { xlsx.write(r, col, QString("N/A"), naFmt); continue; }
        TaskState tsk = a.tasks.value(assetTaskCols[t]);
        Format sf;
        sf.setHorizontalAlignment(Format::AlignHCenter);
        sf.setFontBold(true);
        sf.setPatternBackgroundColor(statusColor(tsk.status));
        sf.setFontColor(isLightStatus(tsk.status) ? QColor(Qt::black) : QColor(Qt::white));
        xlsx.write(r, col, ZtoryModel::taskStatusLabel(tsk.status), sf);
      }
      r++;
    }
  }

  // ── Workflows sheet ───────────────────────────────────────────────────────
  if (xlsx.addSheet(QObject::tr("Workflows"))) {
    xlsx.write(1, 1, QObject::tr("Workflows"), titleFmt);
    xlsx.write(3, 1, QObject::tr("Technique"),  hdrFmt);
    xlsx.write(3, 2, QObject::tr("Task types (in order)"), hdrFmt);
    xlsx.setColumnWidth(1, 18); xlsx.setColumnWidth(2, 70);
    int r = 4;
    for (const Technique &t : m->techniques()) {
      xlsx.write(r, 1, t.name, cellFmt);
      xlsx.write(r, 2, t.taskTypes.join("  ›  "), cellFmt);
      r++;
    }
  }

  if (xlsx.saveAs(path))
    QMessageBox::information(this, QObject::tr("Export Full Project"),
                            QObject::tr("Saved:\n%1").arg(path));
  else
    QMessageBox::warning(this, QObject::tr("Export Full Project"),
                         QObject::tr("Could not write the spreadsheet."));
}

//-----------------------------------------------------------------------------
// Team tab — editable roster (project-level), the single home for the team
// (moved out of Storyboard Settings: the tracker governs the whole pipeline).

QWidget *ZtoryProductionPanel::buildTeamTab() {
  QWidget *w = new QWidget(this);
  auto *lay  = new QVBoxLayout(w);
  lay->addWidget(new QLabel(QObject::tr("Project team (double-click to rename):"), w));
  m_teamList = new QListWidget(w);
  lay->addWidget(m_teamList);
  auto *btns   = new QHBoxLayout();
  auto *addBtn = new QPushButton(QObject::tr("+ Add"), w);
  auto *remBtn = new QPushButton(QObject::tr("− Remove"), w);
  btns->addWidget(addBtn);
  btns->addWidget(remBtn);
  btns->addStretch();
  lay->addLayout(btns);

  connect(addBtn, &QPushButton::clicked, this, [this] {
    auto *it = new QListWidgetItem(QObject::tr("New person"), m_teamList);
    it->setFlags(it->flags() | Qt::ItemIsEditable);
    m_teamList->setCurrentItem(it);
    m_teamList->editItem(it);
  });
  connect(remBtn, &QPushButton::clicked, this, [this] {
    delete m_teamList->currentItem();
    applyTeamFromList();
  });
  connect(m_teamList, &QListWidget::itemChanged, this,
          [this](QListWidgetItem *) { applyTeamFromList(); });
  return w;
}

void ZtoryProductionPanel::reloadTeamTab() {
  if (!m_teamList) return;
  m_teamLoading = true;
  m_teamList->clear();
  for (const QString &p : ZtoryModel::instance()->team()) {
    auto *it = new QListWidgetItem(p, m_teamList);
    it->setFlags(it->flags() | Qt::ItemIsEditable);
  }
  m_teamLoading = false;
}

void ZtoryProductionPanel::applyTeamFromList() {
  if (m_teamLoading || !m_teamList) return;
  QStringList team;
  for (int i = 0; i < m_teamList->count(); i++) {
    QString t = m_teamList->item(i)->text().trimmed();
    if (!t.isEmpty()) team << t;
  }
  ZtoryModel::instance()->setTeam(team);
  ZtoryModel::instance()->saveProjectDb();  // team lives in the project DB
}

//-----------------------------------------------------------------------------
// Project tab — production metadata + default technique (pipeline-level, lives
// here rather than in Storyboard Settings).

QWidget *ZtoryProductionPanel::buildProjectTab() {
  QWidget *w  = new QWidget(this);
  auto *form  = new QFormLayout(w);
  m_prodEdit   = new QLineEdit(w);
  m_codeEdit   = new QLineEdit(w);
  m_codeEdit->setPlaceholderText(QObject::tr("e.g. CS26 — short code used in {CODE}"));
  m_codeEdit->setToolTip(QObject::tr(
      "Short project code (Kitsu 'code'), used as the {CODE} naming token.\n"
      "Keep it brief (≈3 chars, no spaces)."));
  m_codeEdit->setMaxLength(16);
  m_seasonEdit = new QLineEdit(w);
  m_titleEdit  = new QLineEdit(w);
  m_epEdit     = new QLineEdit(w);
  m_techCombo  = new QComboBox(w);
  m_patternEdit = new QLineEdit(w);
  m_patternEdit->setPlaceholderText("{PROD}_{CODE}_{EP}_{SEQ}_{SHOT}_{TASK}_V{VER:02}");
  m_patternEdit->setToolTip(
      QObject::tr("Tokens: {PROD} {CODE} {SEASON} {EP} {SEQ} {SHOT} {TASK} {VER}\n"
                  "Format: {VER:02} = zero-padded to 2 digits\n"
                  "Task codes: LAY, ANIM, KAN, INB, CU, VFX, COMP, AMC…"));
  form->addRow(QObject::tr("Production:"),        m_prodEdit);
  form->addRow(QObject::tr("Code:"),              m_codeEdit);
  form->addRow(QObject::tr("Season:"),            m_seasonEdit);
  form->addRow(QObject::tr("Episode:"),           m_epEdit);
  form->addRow(QObject::tr("Title:"),             m_titleEdit);
  form->addRow(QObject::tr("Default technique:"), m_techCombo);
  form->addRow(QObject::tr("Naming pattern:"),    m_patternEdit);
  // Ztoryc (2026-09-27): the episode number and the asset files' convention.
  m_epNumEdit = new QLineEdit(w);
  m_epNumEdit->setMaxLength(8);
  m_epNumEdit->setToolTip(QObject::tr(
      "The episode's number, used as {EPNUM} — «06» in CS2606. Empty: read "
      "from the episode name when it starts with the code (CS26 + "
      "CS2606_MESSINA → 06)."));
  m_assetPatternEdit = new QLineEdit(w);
  m_assetPatternEdit->setToolTip(QObject::tr(
      "How the asset files are named — used to FIND them in the category "
      "folders and to RENAME them.\nTokens: {CODE} {EPNUM} {EP} {SEASON} "
      "{PROD} {TYPE} {NAME} {VER}\n{TYPE}: PS props, BG backgrounds, CH "
      "characters, FX effects. {NAME}: the asset's name in lower case, words "
      "joined by «-».\nExample: CS2606_PS_bacchetta-magica_V1.psd"));
  form->addRow(QObject::tr("Episode number:"),   m_epNumEdit);
  form->addRow(QObject::tr("Asset file names:"), m_assetPatternEdit);

  // Dove stanno i file degli asset, UNA CARTELLA PER CATEGORIA. Con 145 asset
  // un percorso per ciascuno non lo compila nessuno: qui si indica la cartella
  // e il file si risolve per convenzione dal nome dell'asset.
  // I Character non hanno cartella: nel cutout sono SCENE, e la scena si lega
  // sull'asset stesso (scheda Assets → Link file).
  auto addDirRow = [&](const QString &label, QLineEdit *&edit,
                       const QString &tip) {
    auto *row  = new QWidget(w);
    auto *hl   = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    edit       = new QLineEdit(row);
    edit->setToolTip(tip);
    auto *browse = new QPushButton(QObject::tr("…"), row);
    browse->setFixedWidth(28);
    hl->addWidget(edit);
    hl->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this, edit, label] {
      const QString d = QFileDialog::getExistingDirectory(this, label,
                                                          edit->text());
      if (!d.isEmpty()) { edit->setText(d); applyProjectFromFields(); }
    });
    connect(edit, &QLineEdit::editingFinished, this,
            [this] { applyProjectFromFields(); });
    form->addRow(label, row);
  };
  addDirRow(QObject::tr("Props folder:"), m_propsDirEdit,
            QObject::tr("Folder holding the prop files. The export looks here "
                        "for a prop that has no file of its own."));
  addDirRow(QObject::tr("Backgrounds folder:"), m_bgDirEdit,
            QObject::tr("Folder holding the background/environment files."));
  addDirRow(QObject::tr("Model sheets folder:"), m_modelSheetDirEdit,
            QObject::tr("Tradigital: folder holding the character model "
                        "sheets, imported as images.\nIn digital cutout a "
                        "character is a scene instead — link it on the asset."));

  // Come l'export porta gli asset dentro lo shot. Default di progetto: i
  // singoli asset possono scostarsene dalla scheda Assets.
  m_importModeCombo = new QComboBox(w);
  m_importModeCombo->addItem(QObject::tr("Load — the shot points at the file"),
                             int(AssetImportPolicy::Load));
  m_importModeCombo->addItem(
      QObject::tr("Import — the file is copied into the shot"),
      int(AssetImportPolicy::Import));
  m_importModeCombo->setToolTip(QObject::tr(
      "Load: one source for everyone — fix the asset once and every shot gets "
      "the fix, but a moved file breaks them all together.\n"
      "Import: the shot is self-contained (delivery, archive, someone working "
      "elsewhere), but a fix has to be redone shot by shot.\n"
      "Rule of thumb: Load while the asset is still being worked on, Import "
      "once it is frozen. Single assets can differ (Assets tab)."));
  form->addRow(QObject::tr("Assets come in as:"), m_importModeCombo);
  connect(m_importModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this] { applyProjectFromFields(); });

  // Opzioni PSD: sono quelle del popup di import, replicate qui perche' un
  // export automatico non puo' fermarsi a chiederle per ognuno dei 145 asset.
  m_psdLoadAsCombo = new QComboBox(w);
  m_psdLoadAsCombo->addItems({QObject::tr("Single Image"), QObject::tr("Frames"),
                              QObject::tr("Columns")});
  m_psdLevelNameCombo = new QComboBox(w);
  m_psdLevelNameCombo->addItems({"FileName#LayerName", "LayerName"});
  m_psdGroupsCombo = new QComboBox(w);
  m_psdGroupsCombo->addItems(
      {QObject::tr("Ignore groups"),
       QObject::tr("Group layers as columns in a sub-scene"),
       QObject::tr("Group layers as frames in a column")});
  m_psdSubSceneCheck =
      new QCheckBox(QObject::tr("Expose in a Sub-Scene"), w);
  for (QWidget *cw : QList<QWidget *>{m_psdLoadAsCombo, m_psdLevelNameCombo,
                                      m_psdGroupsCombo})
    static_cast<QComboBox *>(cw)->setToolTip(
        QObject::tr("Default for PSD assets. Same options as the PSD import "
                    "dialog."));
  form->addRow(QObject::tr("PSD — load as:"),    m_psdLoadAsCombo);
  form->addRow(QObject::tr("PSD — level name:"), m_psdLevelNameCombo);
  form->addRow(QObject::tr("PSD — groups:"),     m_psdGroupsCombo);
  form->addRow(QString(), m_psdSubSceneCheck);
  for (QComboBox *cb : {m_psdLoadAsCombo, m_psdLevelNameCombo, m_psdGroupsCombo})
    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { applyProjectFromFields(); });
  connect(m_psdSubSceneCheck, &QCheckBox::toggled, this,
          [this] { applyProjectFromFields(); });

  // Kitsu was an opt-in taken at project creation and never revisitable: the
  // whole integration stayed hidden for a project made before you knew you
  // wanted it, with no way to turn it on short of recreating the project.
  // The decision belongs where it can be changed.
  m_useKitsuCheck = new QCheckBox(QObject::tr("Use Kitsu for this project"), w);
  m_useKitsuCheck->setToolTip(
      QObject::tr("Shows the Kitsu integration below. Can be turned on at any "
                  "time -- it does not have to be chosen when the project is "
                  "created."));
  form->addRow(QString(), m_useKitsuCheck);

  // M5 — Kitsu integration, gated behind the project's opt-in flag: the whole
  // group is hidden unless the project enables Kitsu (chosen at creation).
  m_kitsuGroup = new QGroupBox(QObject::tr("Kitsu integration"), w);
  auto *kgl = new QVBoxLayout(m_kitsuGroup);
  m_kitsuLabel = new QLabel(QObject::tr("Not linked to Kitsu."), m_kitsuGroup);
  m_kitsuLabel->setWordWrap(true);
  kgl->addWidget(m_kitsuLabel);
  // Ztoryc: the automatic push of status changes (2026-09-26). Off by default;
  // a machine setting, not a project one. Each change is sent only if Kitsu
  // still has the status it started from — otherwise Kitsu's is kept.
  auto *autoPush = new QCheckBox(
      QObject::tr("Push status changes to Kitsu automatically"), m_kitsuGroup);
  autoPush->setChecked(KitsuClient::instance()->autoPushStatus());
  autoPush->setToolTip(QObject::tr(
      "Every status change made in Ztoryc is sent to Kitsu as it happens — "
      "only if the task on Kitsu still has the status it started from. If "
      "someone changed it on Kitsu meanwhile, Kitsu's status is kept and "
      "Ztoryc takes it."));
  connect(autoPush, &QCheckBox::toggled, this,
          [](bool on) { KitsuClient::instance()->setAutoPushStatus(on); });
  kgl->addWidget(autoPush);
  // ⚠️ Two different things: LINKED (the project is bound to a Kitsu
  // production — saved in production.ztrack, it survives restarts; the label
  // above) and CONNECTED (logged in — lasts until Ztoryc closes; this button).
  // The button always read «Connect to Kitsu…», so next to a green «linked»
  // nobody could tell whether they were connected (Franco, 2026-09-25): it now
  // turns into a green «Connected» — see updateKitsuButtons().
  auto *kitsuBtn = new QPushButton(QObject::tr("Connect to Kitsu…"), m_kitsuGroup);
  m_kitsuConnectBtn = kitsuBtn;
  kgl->addWidget(kitsuBtn);
  connect(kitsuBtn, &QPushButton::clicked, this, [this] {
    KitsuConnectDialog dlg(this);
    dlg.exec();
    reloadProjectTab();
    updateKitsuButtons();
  });

  m_kitsuHandlesCheck = new QCheckBox(QObject::tr("Push with handles"), m_kitsuGroup);
  m_kitsuHandlesCheck->setToolTip(QObject::tr(
      "Pad each shot's frame_in/out in Kitsu by N frames of safety margin,\n"
      "leaving Ztoryc's board timing unchanged."));
  m_kitsuHandlesSpin = new QSpinBox(m_kitsuGroup);
  m_kitsuHandlesSpin->setRange(0, 240);
  m_kitsuHandlesSpin->setValue(12);
  m_kitsuHandlesSpin->setSuffix(QObject::tr(" fr"));
  auto *handlesRow = new QHBoxLayout();
  handlesRow->addWidget(m_kitsuHandlesCheck);
  handlesRow->addWidget(m_kitsuHandlesSpin);
  handlesRow->addStretch(1);
  kgl->addLayout(handlesRow);

  m_kitsuUploadBtn = new QPushButton(QObject::tr("Upload shot previews →"), m_kitsuGroup);
  m_kitsuUploadBtn->setToolTip(QObject::tr(
      "Pick a folder of per-shot clips and upload each to its shot's task\n"
      "(matched by shot name + {TASK} code), setting it to WFA."));
  // Ztoryc: ONE Sync button instead of Push/Pull for shots and assets
  // (Franco, 2026-09-27). The old four stay as objects — updateKitsuButtons
  // and the handlers still use them — but are not shown.
  m_kitsuSyncBtn = new QPushButton(QObject::tr("⇄ Sync with Kitsu"),
                                   m_kitsuGroup);
  m_kitsuSyncBtn->setToolTip(QObject::tr(
      "Brings Ztoryc and Kitsu in line, shots and assets: new shots and assets "
      "are created on Kitsu, new assets from Kitsu come in, and statuses are "
      "merged — what changed on Kitsu comes in, what changed in Ztoryc goes "
      "out. Each task remembers its status at the last sync, so it knows who "
      "changed what; if both changed it, Kitsu's status is kept."));
  // Kept to put back once an episode is linked (updateKitsuButtons replaces
  // the tooltip with the reason while the button is off).
  m_kitsuSyncBtn->setProperty("baseTip", m_kitsuSyncBtn->toolTip());
  kgl->addWidget(m_kitsuSyncBtn);

  connect(m_kitsuSyncBtn, &QPushButton::clicked, this,
          &ZtoryProductionPanel::onKitsuSync);


  kgl->addWidget(m_kitsuUploadBtn);
  m_kitsuSyncLabel = new QLabel(QString(), m_kitsuGroup);
  m_kitsuSyncLabel->setWordWrap(true);
  kgl->addWidget(m_kitsuSyncLabel);
  form->addRow(m_kitsuGroup);

  connect(m_kitsuUploadBtn, &QPushButton::clicked, this, &ZtoryProductionPanel::onKitsuUpload);

  for (QLineEdit *e : {m_prodEdit, m_codeEdit, m_seasonEdit, m_titleEdit, m_epEdit, m_patternEdit,
                       m_epNumEdit, m_assetPatternEdit})
    connect(e, &QLineEdit::editingFinished, this,
            [this] { applyProjectFromFields(); });
  connect(m_techCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int) { applyProjectFromFields(); });
  connect(m_useKitsuCheck, &QCheckBox::toggled, this, [this](bool on) {
    if (m_projLoading) return;
    ZtoryModel::instance()->setUseKitsu(on);
    if (m_kitsuGroup) m_kitsuGroup->setVisible(on);
  });
  return w;
}

void ZtoryProductionPanel::reloadProjectTab() {
  if (!m_prodEdit) return;
  m_projLoading = true;
  ZtoryModel *m = ZtoryModel::instance();
  m_prodEdit->setText(m->production());
  if (m_useKitsuCheck) m_useKitsuCheck->setChecked(m->useKitsu());
  // The effective code, shown as a real value. Empty, the field showed grey
  // hint text and read as disabled -- the same trap as the naming pattern.
  if (m_codeEdit) m_codeEdit->setText(m->effectiveCode());
  m_seasonEdit->setText(m->season());
  m_titleEdit->setText(m->title());
  m_epEdit->setText(m->episode());
  if (m_propsDirEdit)      m_propsDirEdit->setText(m->propsDir());
  if (m_bgDirEdit)         m_bgDirEdit->setText(m->backgroundsDir());
  if (m_modelSheetDirEdit) m_modelSheetDirEdit->setText(m->modelSheetDir());
  if (m_importModeCombo && m_psdLoadAsCombo) {
    const AssetImportPolicy &p = m->defaultImportPolicy();
    m_importModeCombo->setCurrentIndex(
        p.mode == AssetImportPolicy::Import ? 1 : 0);
    const QStringList loadAs{"Single Image", "Frames", "Columns"};
    const QStringList groups{"Ignore", "SubSceneColumns", "ColumnFrames"};
    m_psdLoadAsCombo->setCurrentIndex(qMax(0, loadAs.indexOf(p.psdLoadAs)));
    m_psdGroupsCombo->setCurrentIndex(qMax(0, groups.indexOf(p.psdGroups)));
    if (!p.psdLevelName.isEmpty()) {
      int i = m_psdLevelNameCombo->findText(p.psdLevelName);
      if (i >= 0) m_psdLevelNameCombo->setCurrentIndex(i);
    }
    m_psdSubSceneCheck->setChecked(p.psdSubScene == 1);
  }
  // Bound to a Kitsu episode: the name is no longer ours to change. Renaming it
  // here would leave the name and kitsuEpisodeId pointing at different things —
  // the push would create a NEW episode under the typed name while the pull
  // kept filtering on the old id. The rename belongs on Kitsu, then re-link.
  {
    const bool epBound = m->isKitsuEpisodeLinked();
    m_epEdit->setReadOnly(epBound);
    m_epEdit->setToolTip(
        epBound ? QObject::tr("Bound to the Kitsu episode. Rename it in Kitsu, "
                              "then link again from the Kitsu dialog.")
                : QString());
  }
  m_techCombo->clear();
  for (const Technique &t : m->techniques()) m_techCombo->addItem(t.name);
  int di = m_techCombo->findText(m->defaultTechnique());
  if (di >= 0) m_techCombo->setCurrentIndex(di);
  // The EFFECTIVE pattern, not a placeholder. Left empty the field showed grey
  // hint text, which reads as "disabled" -- Franco reported it as not editable
  // when it always was. A field should show what it will do, not what you could
  // type into it.
  if (m_patternEdit) {
    const QString pat = m->namingPattern();
    m_patternEdit->setText(pat.isEmpty() ? m->defaultNamingPattern() : pat);
  }
  // Shown as what they will do: the derived number, the default convention.
  if (m_epNumEdit) m_epNumEdit->setText(m->effectiveEpisodeNumber());
  if (m_assetPatternEdit) {
    const QString ap = m->assetFilePattern();
    m_assetPatternEdit->setText(ap.isEmpty() ? ZtoryModel::defaultAssetFilePattern()
                                             : ap);
  }

  // M5 — when linked to Kitsu, that instance owns the project metadata: mirror
  // it and make the synced fields read-only ("managed in Kitsu").
  const bool linked = m->isKitsuLinked();
  if (m_kitsuLabel) {
    if (linked) {
      QString info = tr("🔗 Linked: %1").arg(kitsuBindingText());
      if (!m->productionType().isEmpty())
        info += "  ·  " + m->productionType();
      if (!m->resolution().isEmpty())
        info += "  ·  " + m->resolution() + " @ " + QString::number(m->fps()) + "fps";
      m_kitsuLabel->setText(info);
      m_kitsuLabel->setStyleSheet(kitsuEpisodeMissing() ? "color:#FFB000;"
                                                        : "color:#22D160;");
    } else {
      m_kitsuLabel->setText(tr("Not linked to Kitsu."));
      m_kitsuLabel->setStyleSheet(QString());
    }
  }
  // Production + Code are owned by Kitsu when linked; keep them read-only so the
  // local copy can't silently diverge from the server.
  m_prodEdit->setReadOnly(linked);
  if (m_codeEdit) m_codeEdit->setReadOnly(linked);
  updateKitsuButtons();
  m_projLoading = false;
}

void ZtoryProductionPanel::updateKitsuButtons() {
  // Opt-in: hide the whole Kitsu group unless the project uses Kitsu.
  if (m_kitsuGroup) m_kitsuGroup->setVisible(ZtoryModel::instance()->useKitsu());
  // Push/Pull need BOTH: a production to talk to and a session to talk with.
  // Enabled on «linked» alone they answered «Not logged in» after the click.
  const bool loggedIn = KitsuClient::instance()->isLoggedIn();
  const bool linked   = ZtoryModel::instance()->isKitsuLinked() && loggedIn;
  // Once connected the same dialog is still needed (production, episode,
  // statuses), so the button stays clickable — but it says the state instead
  // of offering to connect again.
  if (m_kitsuConnectBtn) {
    // Connected TO WHAT: the production alone was not enough to notice a
    // project bound to another episode of the same show.
    const bool bound = ZtoryModel::instance()->isKitsuLinked();
    m_kitsuConnectBtn->setText(
        !loggedIn ? tr("Connect to Kitsu…")
        : bound   ? tr("● Connected — %1").arg(kitsuBindingText())
                  : tr("● Connected"));
    m_kitsuConnectBtn->setStyleSheet(
        !loggedIn               ? QString()
        : kitsuEpisodeMissing() ? QString("color:#FFB000;")
                                : QString("color:#22D160;"));
    m_kitsuConnectBtn->setToolTip(
        loggedIn ? tr("Connected as %1.\nClick for the Kitsu settings: "
                      "production, episode, statuses.")
                       .arg(KitsuClient::instance()->email())
                 : tr("Log in to Kitsu to push or pull."));
  }
  // Not while a Sync runs: a second one would interleave its steps. And not on
  // a series without its episode: the button says WHERE it syncs, so the
  // target is read before every click (Franco, 2026-09-29).
  const bool target = linked && !kitsuEpisodeMissing();
  if (m_kitsuSyncBtn) {
    m_kitsuSyncBtn->setEnabled(target &&
                               !ZtoryKitsuSync::instance()->isRunning());
    m_kitsuSyncBtn->setText(ZtoryModel::instance()->isKitsuLinked()
                                ? tr("⇄ Sync — %1").arg(kitsuBindingText())
                                : tr("⇄ Sync with Kitsu"));
    m_kitsuSyncBtn->setToolTip(
        (linked && !target)
            ? tr("No episode linked. «Connect to Kitsu…», choose the episode "
                 "row, «Link selected» — then Sync.")
            : m_kitsuSyncBtn->property("baseTip").toString());
  }
  if (m_kitsuUploadBtn) m_kitsuUploadBtn->setEnabled(target);
  if (m_kitsuHandlesCheck) m_kitsuHandlesCheck->setEnabled(linked);
  if (m_kitsuHandlesSpin)  m_kitsuHandlesSpin->setEnabled(linked);
}

QString ZtoryProductionPanel::kitsuBindingText() {
  ZtoryModel *m = ZtoryModel::instance();
  if (m->isKitsuEpisodeLinked())
    return QString("%1 — %2").arg(m->kitsuProjectName(), m->episode());
  // A series with no episode bound reads the WHOLE show: say it, instead of
  // showing the production alone as if nothing were missing.
  if (m->productionType() == "tvshow")
    return tr("%1 — no episode linked").arg(m->kitsuProjectName());
  return m->kitsuProjectName();
}

bool ZtoryProductionPanel::kitsuEpisodeMissing() {
  ZtoryModel *m = ZtoryModel::instance();
  return m->isKitsuLinked() && m->productionType() == "tvshow" &&
         !m->isKitsuEpisodeLinked();
}

void ZtoryProductionPanel::maybeAutoConnectForScene() {
  if (!isVisible()) return;  // asked where it can be read, in the tracker
  ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
  if (!sc || sc->isUntitled()) return;  // no scene chosen yet: say nothing
  // This scene's project, not the one the panel last showed.
  ZtoryModel::instance()->loadProjectDb();
  reloadProjectTab();
  updateKitsuButtons();
  maybeAutoConnect();
}

void ZtoryProductionPanel::maybeAutoConnect() {
  ZtoryModel *m   = ZtoryModel::instance();
  KitsuClient *kc = KitsuClient::instance();
  if (!m->isKitsuLinked() || kc->isLoggedIn()) return;
  if (kc->email().isEmpty() || !kc->hasSavedPassword()) return;
  // Once per project and session: offline, a retry at every show of the
  // panel would only stack failed logins. The dialog stays the way to retry.
  const QString key = m->projectDbPath();
  if (key.isEmpty() || key == m_autoConnectTried) return;
  m_autoConnectTried = key;
  // Said before doing it, with the target in full: the reconnection must
  // never go somewhere the user did not read (Franco, 2026-09-29).
  // Plain text: the message box guesses rich text by itself.
  const QString target =
      m->isKitsuEpisodeLinked()
          ? tr("production «%1»\nepisode «%2»")
                .arg(m->kitsuProjectName(), m->episode())
          : tr("production «%1»").arg(m->kitsuProjectName());
  // A series with no episode linked cannot sync (ZtoryKitsuSync::start): the
  // question then offers the connection only, and says what is missing.
  if (kitsuEpisodeMissing()) {
    const int answer = DVGui::MsgBox(
        tr("Connecting to Kitsu:\n\n%1\n\nNo episode is linked: the Sync "
           "stays off until you link one in «Connect to Kitsu…».")
            .arg(target),
        tr("Connect"), tr("Not now"), 1);
    if (answer != 1) return;
    m_syncAfterConnect = false;
    kc->connectAndSync();
    return;
  }
  const int answer = DVGui::MsgBox(
      tr("Connecting to Kitsu:\n\n%1\n\nthen syncing.").arg(target),
      tr("Connect and sync"), tr("Connect only"), tr("Not now"), 1);
  if (answer != 1 && answer != 2) return;  // «Not now», or the box closed
  m_syncAfterConnect = (answer == 1);
  kc->connectAndSync();  // loginFinished -> maybeAutoSync()
}

// The Sync that starts by itself when the connection to Kitsu is made, so
// that Ztoryc works from Kitsu's latest. Same confirmation as the button
// when it would write a lot (onKitsuSync).
void ZtoryProductionPanel::maybeAutoSync() {
  KitsuClient *kc = KitsuClient::instance();
  if (!kc->isLoggedIn() || !kc->hasTaskStatuses()) return;
  if (!ZtoryModel::instance()->isKitsuLinked()) return;
  if (!ZtoryKitsuSync::instance()->takeAutoSync()) return;
  if (!m_syncAfterConnect) return;  // not asked for: the button does it
  m_syncAfterConnect = false;
  onKitsuSync();
}

// Ztoryc: the one Sync button (2026-09-27). The work is ZtoryKitsuSync's;
// here the confirmation of a large send, and the label.
void ZtoryProductionPanel::onKitsuSync() {
  ZtoryKitsuSync *sync = ZtoryKitsuSync::instance();
  // The first Sync of a project (no base yet) can write many statuses on
  // Kitsu: say it before, not after.
  // Asset previews too: the first Sync after the update uploads them all,
  // each replacing the asset's cover on Kitsu.
  const int toSend   = ZtoryKitsuSync::pendingSends();
  const int previews = ZtoryKitsuSync::pendingPreviews();
  if ((toSend > 10 || previews > 10) &&
      DVGui::MsgBox(tr("Sync with %3.\n\nThis Sync will send %1 statuses "
                       "from Ztoryc to Kitsu (each only where Kitsu still has "
                       "the status of the last sync) and upload %2 asset "
                       "previews (each becomes the asset's cover on Kitsu)."
                       "\n\nContinue?")
                        .arg(toSend)
                        .arg(previews)
                        .arg(kitsuBindingText()),
                    tr("Sync"), tr("Cancel"), 1) != 1)
    return;
  const int handles =
      m_kitsuHandlesCheck->isChecked() ? m_kitsuHandlesSpin->value() : 0;
  QString why;
  if (!sync->start(handles, &why)) {
    m_kitsuSyncLabel->setStyleSheet("color:#FFB000;");
    m_kitsuSyncLabel->setText(why);
    return;
  }
  // Said as it starts, with the target: the progress lines that follow do not
  // repeat it.
  m_kitsuSyncLabel->setStyleSheet(QString());
  m_kitsuSyncLabel->setText(tr("Syncing with %1…").arg(kitsuBindingText()));
  updateKitsuButtons();
}

void ZtoryProductionPanel::onKitsuUpload() {
  ZtoryModel *m = ZtoryModel::instance();
  if (!m->isKitsuLinked()) return;
  const QString dir =
      QFileDialog::getExistingDirectory(this, tr("Folder with per-shot clips"));
  if (dir.isEmpty()) return;
  int unmatched = 0, noId = 0;
  QVector<KitsuPreviewUpload> uploads =
      KitsuClient::buildUploadsFromFolder(dir, unmatched, noId);
  if (uploads.isEmpty()) {
    m_kitsuSyncLabel->setStyleSheet("color:#FF3860;");
    m_kitsuSyncLabel->setText(
        noId ? tr("Matched shots have no Kitsu id — push shots first.")
             : tr("No clips matched a shot name."));
    return;
  }
  KitsuClient::mirrorUploadedWfa(uploads);
  m_kitsuSyncLabel->setStyleSheet(QString());
  m_kitsuSyncLabel->setText(tr("Uploading %1 previews…%2")
                                .arg(uploads.size())
                                .arg(noId ? tr(" (%1 not on Kitsu yet)").arg(noId)
                                          : QString()));
  KitsuClient::instance()->uploadPreviews(m->kitsuProjectId(), uploads);
}

void ZtoryProductionPanel::applyProjectFromFields() {
  if (m_projLoading || !m_prodEdit) return;
  ZtoryModel *m = ZtoryModel::instance();
  // Production/Code are Kitsu-owned while linked — don't write them back.
  if (!m->isKitsuLinked()) {
    m->setProduction(m_prodEdit->text().trimmed());
    if (m_codeEdit) m->setCode(m_codeEdit->text().trimmed());
  }
  m->setSeason(m_seasonEdit->text().trimmed());
  m->setTitle(m_titleEdit->text().trimmed());
  // Same rule as Production/Code: while bound to a Kitsu episode the name is
  // Kitsu's, and writing the field back would desync it from kitsuEpisodeId.
  if (!m->isKitsuEpisodeLinked())
    m->setEpisode(m_epEdit->text().trimmed());
  if (!m_techCombo->currentText().isEmpty())
    m->setDefaultTechnique(m_techCombo->currentText());
  if (m_patternEdit && !m_patternEdit->text().trimmed().isEmpty())
    m->setNamingPattern(m_patternEdit->text().trimmed());
  // Saved only when it differs from what the episode name says: a derived
  // «06» written back would stay 06 when the episode becomes CS2607.
  if (m_epNumEdit) {
    const QString n = m_epNumEdit->text().trimmed();
    m->setEpisodeNumber(n == m->derivedEpisodeNumber() ? QString() : n);
  }
  if (m_assetPatternEdit) {
    const QString ap = m_assetPatternEdit->text().trimmed();
    // The default stays implicit: saving it would freeze it.
    m->setAssetFilePattern(ap == ZtoryModel::defaultAssetFilePattern() ? QString()
                                                                       : ap);
  }
  // Cartelle degli asset per categoria (export completo). Nessun trim del
  // percorso oltre agli spazi: un nome di cartella puo' contenerne.
  if (m_propsDirEdit)      m->setPropsDir(m_propsDirEdit->text().trimmed());
  if (m_bgDirEdit)         m->setBackgroundsDir(m_bgDirEdit->text().trimmed());
  if (m_modelSheetDirEdit) m->setModelSheetDir(m_modelSheetDirEdit->text().trimmed());
  if (m_importModeCombo && m_psdLoadAsCombo) {
    AssetImportPolicy p;
    p.mode = static_cast<AssetImportPolicy::Mode>(
        m_importModeCombo->currentData().toInt());
    // I valori salvati sono le chiavi NON tradotte: salvare l'etichetta
    // renderebbe il .ztoryc illeggibile da una build in un'altra lingua.
    static const char *kLoadAs[] = {"Single Image", "Frames", "Columns"};
    static const char *kGroups[] = {"Ignore", "SubSceneColumns", "ColumnFrames"};
    p.psdLoadAs    = kLoadAs[qBound(0, m_psdLoadAsCombo->currentIndex(), 2)];
    p.psdLevelName = m_psdLevelNameCombo->currentText();
    p.psdGroups    = kGroups[qBound(0, m_psdGroupsCombo->currentIndex(), 2)];
    p.psdSubScene  = m_psdSubSceneCheck->isChecked() ? 1 : 0;
    m->setDefaultImportPolicy(p);
  }
  m->saveProjectDb();  // project-meta lives in the project DB
  rebuild();  // default-technique change may alter which task columns apply
}

//-----------------------------------------------------------------------------
// Assets tab — project-level asset list with its own task pipeline.

bool ZtoryProductionPanel::editAssetPsdOptions(int assetIndex) {
  ZtoryModel *m = ZtoryModel::instance();
  if (assetIndex < 0 || assetIndex >= m->assetCount()) return false;
  const Asset &a = m->assets()[assetIndex];

  // Si parte dai valori EFFETTIVI (default di progetto + eventuali scostamenti
  // gia' presenti): il dialogo deve mostrare cosa succederebbe adesso, non
  // campi vuoti da cui dedurre.
  const AssetImportPolicy eff = m->effectiveImportPolicy(a);
  const AssetImportPolicy def = m->defaultImportPolicy();

  QDialog dlg(this);
  dlg.setWindowTitle(QObject::tr("PSD import options — %1").arg(a.name));
  auto *lay  = new QVBoxLayout(&dlg);
  auto *form = new QFormLayout();
  lay->addLayout(form);

  const QStringList kLoadAs{"Single Image", "Frames", "Columns"};
  const QStringList kGroups{"Ignore", "SubSceneColumns", "ColumnFrames"};

  auto *loadAs = new QComboBox(&dlg);
  loadAs->addItems({QObject::tr("Single Image"), QObject::tr("Frames"),
                    QObject::tr("Columns")});
  loadAs->setCurrentIndex(qMax(0, kLoadAs.indexOf(eff.psdLoadAs)));
  auto *levelName = new QComboBox(&dlg);
  levelName->addItems({"FileName#LayerName", "LayerName"});
  if (!eff.psdLevelName.isEmpty()) {
    int i = levelName->findText(eff.psdLevelName);
    if (i >= 0) levelName->setCurrentIndex(i);
  }
  auto *groups = new QComboBox(&dlg);
  groups->addItems({QObject::tr("Ignore groups"),
                    QObject::tr("Group layers as columns in a sub-scene"),
                    QObject::tr("Group layers as frames in a column")});
  groups->setCurrentIndex(qMax(0, kGroups.indexOf(eff.psdGroups)));
  auto *subScene = new QCheckBox(QObject::tr("Expose in a Sub-Scene"), &dlg);
  subScene->setChecked(eff.psdSubScene == 1);

  form->addRow(QObject::tr("Load as:"),    loadAs);
  form->addRow(QObject::tr("Level name:"), levelName);
  form->addRow(QObject::tr("Groups:"),     groups);
  form->addRow(QString(), subScene);

  auto *hint = new QLabel(
      QObject::tr("Only what differs from the project default is stored, so "
                  "changing the project default still reaches this asset."),
      &dlg);
  hint->setWordWrap(true);
  lay->addWidget(hint);

  auto *box = new QDialogButtonBox(QDialogButtonBox::Ok |
                                       QDialogButtonBox::Cancel,
                                   &dlg);
  QPushButton *resetBtn =
      box->addButton(QObject::tr("Use project defaults"),
                     QDialogButtonBox::ResetRole);
  lay->addWidget(box);
  connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  bool reset = false;
  connect(resetBtn, &QPushButton::clicked, &dlg, [&] { reset = true; dlg.accept(); });

  if (dlg.exec() != QDialog::Accepted) return false;

  AssetImportPolicy p = a.importPolicy;  // il modo Load/Import non si tocca qui
  if (reset) {
    p.psdLoadAs.clear();
    p.psdLevelName.clear();
    p.psdGroups.clear();
    p.psdSubScene = -1;
  } else {
    // SOLO gli scostamenti. Salvare anche i campi uguali al default li
    // congelerebbe: cambiare il default di progetto non arriverebbe piu' qui,
    // e nessuno capirebbe perche' proprio questo asset non segue.
    const QString newLoadAs = kLoadAs[loadAs->currentIndex()];
    const QString newGroups = kGroups[groups->currentIndex()];
    const QString newLevel  = levelName->currentText();
    const int     newSub    = subScene->isChecked() ? 1 : 0;
    p.psdLoadAs    = (newLoadAs == def.psdLoadAs) ? QString() : newLoadAs;
    p.psdGroups    = (newGroups == def.psdGroups) ? QString() : newGroups;
    p.psdLevelName = (newLevel == def.psdLevelName) ? QString() : newLevel;
    p.psdSubScene  = (newSub == def.psdSubScene) ? -1 : newSub;
  }
  m->setAssetImportPolicy(assetIndex, p);
  persistAssets();
  rebuildAssets();
  rebuildBreakdown();
  return true;
}

// Ztoryc: renames a file found by a nearly-matching name to the asset's exact
// name, so the folder convention finds it (Franco, 2026-09-27). Asks first:
// scenes that LOAD the file (point at it instead of copying it) would look
// for the old name and lose it.
void ZtoryProductionPanel::renameAssetFile(int row, const QString &file,
                                           const QString &newName) {
  ZtoryModel *m = ZtoryModel::instance();
  if (row < 0 || row >= m->assetCount()) return;
  const Asset a = m->assets()[row];
  const QFileInfo fi(file);
  if (newName.isEmpty() ||
      newName.contains(QRegularExpression("[/\\\\:*?\"<>|]"))) {
    DVGui::warning(tr("«%1» cannot be a file name: link the file instead.")
                       .arg(newName));
    return;
  }
  const QString target = fi.absolutePath() + "/" + newName;
  // On a disk that ignores case (and Unicode form) a name differing only in
  // those «exists» — as the file itself. That is a rename, not a clash; on a
  // case-sensitive disk the folder lists both names, and then it IS a clash.
  const auto same = [](const QString &x, const QString &y) {
    return x.normalized(QString::NormalizationForm_C)
               .compare(y.normalized(QString::NormalizationForm_C),
                        Qt::CaseInsensitive) == 0;
  };
  const bool sameFile =
      QFileInfo::exists(target) && same(QFileInfo(target).fileName(), fi.fileName()) &&
      !QDir(fi.absolutePath()).entryList(QDir::Files).contains(QFileInfo(target).fileName());
  if (QFileInfo::exists(target) && !sameFile) {
    DVGui::warning(tr("«%1» already exists: link the file instead.")
                       .arg(QFileInfo(target).fileName()));
    return;
  }
  QString question = tr("Rename «%1» to «%2»?")
                         .arg(fi.fileName(), QFileInfo(target).fileName());
  if (m->effectiveImportPolicy(a).mode == AssetImportPolicy::Load)
    question += "\n\n" +
                tr("%1 comes into the shots by LOAD, i.e. pointing at this file: "
                   "shots already exported would look for the old name and "
                   "lose it. Linking the file as it is avoids that.")
                    .arg(a.name);
  // «Cancel» is the default: Enter pressed without reading renames nothing.
  if (DVGui::MsgBox(question, tr("Rename"), tr("Cancel"), 1) != 1) return;
  bool renamed;
  if (sameFile) {  // two steps: through a temporary name not yet taken
    QString tmp = target + ".ztoryc-rename";
    for (int i = 1; QFileInfo::exists(tmp); ++i)
      tmp = target + QString(".ztoryc-rename%1").arg(i);
    renamed = QFile::rename(file, tmp);
    if (renamed && !QFile::rename(tmp, target)) {
      QFile::rename(tmp, file);  // back: the file must not vanish
      renamed = false;
    }
  } else {
    renamed = QFile::rename(file, target);
  }
  if (!renamed) {
    DVGui::warning(tr("Could not rename «%1».").arg(fi.fileName()));
    return;
  }
  rebuildAssets();
  rebuildBreakdown();
}

// Ztoryc: il PSD da riggare di un personaggio (Franco, 2026-09-26). Si parte
// dalla cartella dei model sheet, dove i disegni dei personaggi stanno.
bool ZtoryProductionPanel::linkAssetRigPsdInteractive(int assetIndex) {
  ZtoryModel *m = ZtoryModel::instance();
  if (assetIndex < 0 || assetIndex >= m->assetCount()) return false;
  const Asset &a      = m->assets()[assetIndex];
  const QString start = !a.rigPsdPath.isEmpty() ? m->resolveAssetRigPsd(a)
                                                : m->modelSheetDir();
  const QString f = QFileDialog::getOpenFileName(
      this, QObject::tr("PSD to rig for %1").arg(a.name), start,
      QObject::tr("Photoshop files (*.psd)"));
  if (f.isEmpty()) return false;
  m->setAssetRigPsd(assetIndex, f);
  persistAssets();
  rebuildAssets();
  rebuildBreakdown();
  // Le opzioni si chiedono adesso, come per il file dell'asset: chiederle
  // dopo vuol dire non chiederle mai, e il PSD entrerebbe con quelle di
  // progetto senza che nessuno l'abbia deciso.
  editAssetPsdOptions(assetIndex);
  return true;
}

bool ZtoryProductionPanel::linkAssetFileInteractive(int assetIndex) {
  ZtoryModel *m = ZtoryModel::instance();
  if (assetIndex < 0 || assetIndex >= m->assetCount()) return false;
  const Asset &a    = m->assets()[assetIndex];
  const bool isChar = ZtoryModel::isCharacterType(a.type);

  // Un personaggio cutout E' una scena, e nello shot entra come sotto-scena:
  // per quello serve il .tnz. Prop e sfondi sono livelli, e il legame diretto
  // e' l'eccezione alla convenzione per cartella — quindi si parte dalla
  // cartella della categoria, dove il file probabilmente e' gia'.
  QString start = a.filePath;
  if (start.isEmpty()) {
    if (isChar) {
      // I personaggi non hanno una cartella di categoria: sono scene DI QUESTO
      // progetto, quindi si parte da +scenes invece che da dove capita.
      if (ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene())
        start = sc->decodeFilePath(TFilePath("+scenes")).getQString();
    } else {
      start = m->assetDirForType(a.type);
    }
  }
  const QString f = QFileDialog::getOpenFileName(
      this, QObject::tr("Link file to %1").arg(a.name), start,
      isChar ? QObject::tr("Scenes (*.tnz)") : QObject::tr("All files (*)"));
  if (f.isEmpty()) return false;
  // ⚠️ Collegare DICHIARA la scena personaggio (sotto): su uno shot o uno
  // storyboard ne cambia il ruolo, su un altro personaggio ne ruba la scena.
  // Due nomi quasi uguali bastano a sbagliare — FATINA finì su
  // «Companion_non_chiamate,i_princess.tnz», uno shot vecchio, invece che su
  // «…chiamatemi_princess.tnz» (Franco, 2026-09-25). Si chiede, non si fa.
  if (isChar) {
    const QString role = ZtoryCharacter::roleOf(f);
    QString otherUuid, otherName;
    ZtoryCharacter::characterRef(f, &otherUuid, &otherName);
    QString problem;
    if (role == QLatin1String("shot") || role == QLatin1String("storyboard"))
      problem = QObject::tr("«%1» is a %2 scene, not a character scene.")
                    .arg(QFileInfo(f).fileName(), role);
    else if (!otherUuid.isEmpty() && otherUuid != a.uuid)
      problem = QObject::tr("«%1» is already the scene of %2.")
                    .arg(QFileInfo(f).fileName(),
                         otherName.isEmpty() ? otherUuid : otherName);
    if (!problem.isEmpty()) {
      const int answer = DVGui::MsgBox(
          problem + "\n\n" +
              QObject::tr("Link it to %1 anyway, and mark it as %1's scene?")
                  .arg(a.name),
          QObject::tr("Link"), QObject::tr("Cancel"), 1);
      if (answer != 1) return false;
    }
  }
  m->setAssetFilePath(assetIndex, f);
  // Il legame va nei DUE sensi, come quando la scena nasce dal popup: il
  // tracker sa qual e' la scena, e la scena (il suo .ztoryc) sa di essere
  // questo personaggio — e' da li' che le mappe delle bocche prendono il
  // personaggio a cui appartengono (Franco, 2026-09-25).
  if (isChar) {
    QString why;
    if (!ZtoryCharacter::declareCharacterScene(f, a.uuid, a.name, &why))
      DVGui::warning(
          QObject::tr("The scene is linked, but could not be marked as %1: %2")
              .arg(a.name, why));
  }
  persistAssets();
  rebuildAssets();
  rebuildBreakdown();
  // Un PSD entra in molti modi diversi, e il momento in cui lo si collega e'
  // l'unico in cui si ha in mente com'e' fatto. Chiederlo dopo vuol dire non
  // chiederlo mai: «Use project defaults» chiude in un clic.
  if (QFileInfo(f).suffix().compare("psd", Qt::CaseInsensitive) == 0)
    editAssetPsdOptions(assetIndex);
  return true;
}

// La colonna della tabella Breakdown che porta l'indice dell'asset (Qt::UserRole).
// In un posto solo: quando si aggiunge una colonna a sinistra, l'indice scritto
// a mano in tre punti se ne accorge in due.
static const int kBreakdownAssetCol = 2;

void ZtoryProductionPanel::onBreakdownContextMenu(const QPoint &pos) {
  if (!m_breakdownTable) return;
  QTableWidgetItem *it = m_breakdownTable->itemAt(pos);
  if (!it) return;
  // L'indice dell'asset viaggia sulla riga: la tabella e' piatta (shot × asset)
  // e lo stesso asset compare in piu' righe, quindi risalirci dal nome sarebbe
  // sbagliato appena due asset si chiamano uguale in tipi diversi.
  const int assetIndex =
      m_breakdownTable->item(it->row(), kBreakdownAssetCol)
          ->data(Qt::UserRole)
          .toInt();
  ZtoryModel *m = ZtoryModel::instance();
  if (assetIndex < 0 || assetIndex >= m->assetCount()) return;
  const Asset &a    = m->assets()[assetIndex];
  const bool isChar = ZtoryModel::isCharacterType(a.type);

  QMenu menu(this);
  QAction *linkAct = menu.addAction(isChar
                                        ? QObject::tr("Link character scene…")
                                        : QObject::tr("Link file…"));
  QAction *clearAct = a.filePath.isEmpty()
                          ? nullptr
                          : menu.addAction(QObject::tr("Clear link"));
  QAction *rigLinkAct = nullptr, *rigClearAct = nullptr;
  if (isChar) {
    menu.addSeparator();
    rigLinkAct = menu.addAction(QObject::tr("Link PSD to rig…"));
    if (!a.rigPsdPath.isEmpty())
      rigClearAct = menu.addAction(QObject::tr("Clear PSD link"));
  }
  QAction *psdAct = nullptr;
  if (QFileInfo(isChar ? m->resolveAssetRigPsd(a) : m->resolveAssetFile(a))
          .suffix()
          .compare("psd", Qt::CaseInsensitive) == 0) {
    menu.addSeparator();
    psdAct = menu.addAction(QObject::tr("PSD import options…"));
  }
  QAction *ch = menu.exec(m_breakdownTable->viewport()->mapToGlobal(pos));
  if (!ch) return;
  if (ch == rigLinkAct) {
    linkAssetRigPsdInteractive(assetIndex);
  } else if (ch == rigClearAct) {
    m->setAssetRigPsd(assetIndex, QString());
    persistAssets();
    rebuildAssets();
  } else if (ch == psdAct) {
    editAssetPsdOptions(assetIndex);
  } else if (ch == linkAct) {
    linkAssetFileInteractive(assetIndex);
  } else if (ch == clearAct) {
    m->setAssetFilePath(assetIndex, QString());
    persistAssets();
    rebuildAssets();
    rebuildBreakdown();
  }
}

//-----------------------------------------------------------------------------
// Scrivere il breakdown a mano. Il pezzo che mancava: senza, «questo shot ha
// bisogno di questo asset» si poteva dire solo passando da Kitsu.
//-----------------------------------------------------------------------------

namespace {

// L'etichetta con cui uno shot di progetto si riconosce fra gli altri: piu'
// storyboard nello stesso progetto hanno ognuno il suo SH010, e senza la
// provenienza si sceglie quello sbagliato.
QString projectShotLabel(const ProjectShot &ps) {
  QString src = ps.source;
  src.remove(QRegularExpression("\\.ztoryc$",
                                QRegularExpression::CaseInsensitiveOption));
  QString s = ps.seq.isEmpty() ? ps.label : (ps.seq + " " + ps.label);
  if (!src.isEmpty()) s += "   —   " + src;
  return s;
}

int projectShotIndexByUuid(const QString &uuid) {
  const std::vector<ProjectShot> &v = ZtoryModel::instance()->projectShots();
  for (int i = 0; i < (int)v.size(); i++)
    if (v[i].uuid == uuid) return i;
  return -1;
}

}  // namespace

void ZtoryProductionPanel::onBreakdownAdd() {
  ZtoryModel *m = ZtoryModel::instance();
  if (m->projectShots().empty() || m->assetCount() == 0) {
    DVGui::warning(QObject::tr(
        "Add some assets first, and save the storyboard so its shots reach the "
        "project."));
    return;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(QObject::tr("This shot needs…"));
  auto *lay  = new QVBoxLayout(&dlg);
  auto *form = new QFormLayout();
  lay->addLayout(form);

  auto *shotCombo = new QComboBox(&dlg);
  for (const ProjectShot &ps : m->projectShots())
    shotCombo->addItem(projectShotLabel(ps), ps.uuid);
  // Se una riga e' selezionata, si parte da quel suo shot: quasi sempre si
  // aggiunge un asset allo shot che si sta guardando.
  if (m_breakdownTable && m_breakdownTable->currentRow() >= 0)
    if (QTableWidgetItem *it =
            m_breakdownTable->item(m_breakdownTable->currentRow(), 0)) {
      int i = shotCombo->findData(it->data(Qt::UserRole).toString());
      if (i >= 0) shotCombo->setCurrentIndex(i);
    }

  auto *assetCombo = new QComboBox(&dlg);
  for (int i = 0; i < m->assetCount(); i++) {
    const Asset &a = m->assets()[i];
    assetCombo->addItem(QString("%1  (%2)").arg(a.name, a.type), a.uuid);
  }
  auto *nSpin = new QSpinBox(&dlg);
  nSpin->setRange(1, 99);

  form->addRow(QObject::tr("Shot:"), shotCombo);
  form->addRow(QObject::tr("Asset:"), assetCombo);
  form->addRow(QObject::tr("How many:"), nSpin);

  auto *box = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  lay->addWidget(box);
  connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;

  const int si = projectShotIndexByUuid(shotCombo->currentData().toString());
  if (si < 0) return;
  QVector<BreakdownEntry> b = m->projectShots()[si].breakdown;
  const QString assetUuid   = assetCombo->currentData().toString();
  for (BreakdownEntry &be : b)
    if (be.assetUuid == assetUuid) {
      // Gia' presente: si aggiorna il numero invece di aggiungere una seconda
      // riga per la stessa coppia, che al momento dell'import sarebbe lo stesso
      // file due volte.
      be.nbOccurrences = nSpin->value();
      m->setShotBreakdown(si, b);
      m->saveProjectDb();
      rebuildBreakdown();
      return;
    }
  BreakdownEntry be;
  be.assetUuid     = assetUuid;
  be.nbOccurrences = nSpin->value();
  b.push_back(be);
  m->setShotBreakdown(si, b);
  m->saveProjectDb();
  rebuildBreakdown();
}

void ZtoryProductionPanel::onBreakdownRemove() {
  if (!m_breakdownTable) return;
  const int row = m_breakdownTable->currentRow();
  if (row < 0) return;
  QTableWidgetItem *key = m_breakdownTable->item(row, 0);
  if (!key) return;
  const QString shotUuid  = key->data(Qt::UserRole).toString();
  const QString assetUuid = key->data(Qt::UserRole + 1).toString();
  const int si            = projectShotIndexByUuid(shotUuid);
  if (si < 0) return;
  ZtoryModel *m             = ZtoryModel::instance();
  QVector<BreakdownEntry> b = m->projectShots()[si].breakdown;
  for (int i = 0; i < b.size(); i++)
    if (b[i].assetUuid == assetUuid) { b.remove(i); break; }
  m->setShotBreakdown(si, b);
  m->saveProjectDb();
  rebuildBreakdown();
}

void ZtoryProductionPanel::onBreakdownFromDialogue() {
  ZtoryModel *m = ZtoryModel::instance();
  // Solo gli shot dello storyboard APERTO: i dialoghi stanno nei suoi pannelli,
  // e degli shot degli altri storyboard qui non si sa niente.
  int added = 0, shotsTouched = 0, noProjectShot = 0;
  for (int i = 0; i < m->shotCount(); i++) {
    const ShotData &sd = m->shot(i);
    if (sd.uuid.isEmpty()) continue;
    const int si = projectShotIndexByUuid(sd.uuid);
    if (si < 0) { noProjectShot++; continue; }

    // Chi parla nei pannelli di questo shot, gia' risolto in asset: e'
    // esattamente cio' che rende verdi i nomi nel Board.
    QSet<QString> speakers;
    for (const PanelData &p : sd.panels)
      for (const DialogueLine &dl : m->parseDialogue(p.dialog))
        if (dl.matched && !dl.assetUuid.isEmpty()) speakers.insert(dl.assetUuid);
    if (speakers.isEmpty()) continue;

    QVector<BreakdownEntry> b = m->projectShots()[si].breakdown;
    QSet<QString> have;
    for (const BreakdownEntry &be : b) have.insert(be.assetUuid);
    bool changed = false;
    for (const QString &u : speakers) {
      if (have.contains(u)) continue;
      // Si AGGIUNGE soltanto. Cio' che c'e' gia' — magari messo a mano o
      // arrivato da Kitsu — non si tocca: questo bottone e' un aiuto, non
      // un'autorita' sul breakdown.
      BreakdownEntry be;
      be.assetUuid = u;
      b.push_back(be);
      changed = true;
      added++;
    }
    if (changed) {
      m->setShotBreakdown(si, b);
      shotsTouched++;
    }
  }
  if (added > 0) m->saveProjectDb();
  rebuildBreakdown();

  QString msg = QObject::tr("%1 character(s) added across %2 shot(s).")
                    .arg(added)
                    .arg(shotsTouched);
  if (noProjectShot > 0)
    msg += QObject::tr(
               "\n%1 shot(s) of this storyboard are not in the project yet: "
               "save the scene to publish them.")
               .arg(noProjectShot);
  DVGui::info(msg);
}

QWidget *ZtoryProductionPanel::buildBreakdownTab() {
  QWidget *w = new QWidget(this);
  auto *lay  = new QVBoxLayout(w);

  auto *btns = new QHBoxLayout();
  // The breakdown goes to and from Kitsu with «⇄ Sync with Kitsu» (step 5,
  // merged on a base). The old «Pull breakdown from Kitsu» REPLACED each
  // shot with Kitsu's list, and would now throw away what was added here.
  // Written by hand too (+ Add, − Remove, from the dialogue): a project
  // without Kitsu needs a breakdown as well, or the asset import at the
  // export would have nothing to work on.
  auto *addBreakdownBtn = new QPushButton(QObject::tr("+ Add"), w);
  addBreakdownBtn->setToolTip(
      QObject::tr("Say by hand that a shot needs an asset."));
  auto *remBreakdownBtn = new QPushButton(QObject::tr("− Remove"), w);
  auto *fromDialogueBtn =
      new QPushButton(QObject::tr("Characters from the dialogue"), w);
  fromDialogueBtn->setToolTip(QObject::tr(
      "For every shot of the storyboard now open, add the characters that "
      "speak in its panels — the same names the Board shows in green.\n"
      "Only adds what is missing: nothing already there is touched."));
  btns->addWidget(addBreakdownBtn);
  btns->addWidget(remBreakdownBtn);
  btns->addWidget(fromDialogueBtn);
  connect(addBreakdownBtn, &QPushButton::clicked, this,
          &ZtoryProductionPanel::onBreakdownAdd);
  connect(remBreakdownBtn, &QPushButton::clicked, this,
          &ZtoryProductionPanel::onBreakdownRemove);
  connect(fromDialogueBtn, &QPushButton::clicked, this,
          &ZtoryProductionPanel::onBreakdownFromDialogue);
  btns->addStretch();
  lay->addLayout(btns);

  m_breakdownTable = new QTableWidget(w);
  m_breakdownTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_breakdownTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_breakdownTable->verticalHeader()->setVisible(false);
  m_breakdownTable->setContextMenuPolicy(Qt::CustomContextMenu);
  lay->addWidget(m_breakdownTable);
  connect(m_breakdownTable, &QWidget::customContextMenuRequested, this,
          &ZtoryProductionPanel::onBreakdownContextMenu);
  // La colonna File e' dove si VEDE cosa manca, quindi e' li' che si aggiusta:
  // doppio clic = collega, senza passare dalla scheda Assets.
  connect(m_breakdownTable, &QTableWidget::cellDoubleClicked, this,
          [this](int row, int) {
            QTableWidgetItem *it =
                m_breakdownTable->item(row, kBreakdownAssetCol);
            if (it) linkAssetFileInteractive(it->data(Qt::UserRole).toInt());
          });

  return w;
}

void ZtoryProductionPanel::rebuildBreakdown() {
  if (!m_breakdownTable) return;
  ZtoryModel *m = ZtoryModel::instance();
  QHash<QString, QFileInfoList> breakdownDirCache;  // each folder listed once

  m_breakdownTable->clear();
  // ⚠️ La colonna «Storyboard» non e' un di piu'. Un progetto ha piu' file di
  // storyboard, e ognuno ha il suo SH010: senza la provenienza, due righe
  // «SH010» di due storyboard diversi sono indistinguibili — e si finisce per
  // credere che lo shot che si sta esportando abbia il breakdown di un altro.
  // Successo davvero il 2026-08-17, con l'export che diceva giustamente «non
  // c'e' niente da importare» mentre la tabella mostrava quattro asset.
  m_breakdownTable->setColumnCount(6);
  m_breakdownTable->setHorizontalHeaderLabels(
      {QObject::tr("Storyboard"), QObject::tr("Shot"), QObject::tr("Asset"),
       QObject::tr("Type"), QObject::tr("×"), QObject::tr("File")});

  // One row per (shot, asset): the flat form is what you read when you want to
  // know «what does this shot need», and it sorts and searches naturally.
  int rows = 0;
  for (const ProjectShot &ps : m->projectShots()) rows += ps.breakdown.size();
  m_breakdownTable->setRowCount(rows);

  int r = 0;
  for (const ProjectShot &ps : m->projectShots()) {
    for (const BreakdownEntry &be : ps.breakdown) {
      // The asset is held by uuid; resolve it for display. An entry whose asset
      // is gone shows the uuid rather than an empty cell — a silent blank would
      // read as «no asset» instead of «dangling link».
      QString name = QObject::tr("⟨missing asset %1⟩").arg(be.assetUuid.left(8));
      QString type;
      const Asset *found = nullptr;
      int assetIndex = -1;
      for (int ai = 0; ai < m->assetCount(); ai++)
        if (m->assets()[ai].uuid == be.assetUuid) {
          found = &m->assets()[ai]; name = found->name; type = found->type;
          assetIndex = ai;
          break;
        }

      QString src = ps.source;
      src.remove(QRegularExpression("\\.ztoryc$", 
                                    QRegularExpression::CaseInsensitiveOption));
      auto *srcItem = new QTableWidgetItem(src);
      // Le due chiavi della riga: quale shot e quale asset. Servono a togliere
      // la voce senza dover indovinare da nome ed etichetta, che si ripetono.
      srcItem->setData(Qt::UserRole, ps.uuid);
      srcItem->setData(Qt::UserRole + 1, be.assetUuid);
      srcItem->setToolTip(ps.source);
      srcItem->setForeground(QBrush(QColor("#999999")));
      m_breakdownTable->setItem(r, 0, srcItem);
      // Sequenza + shot: due storyboard possono avere lo stesso SH010, ma
      // dentro uno stesso storyboard e' la sequenza a distinguerli.
      const QString shotText =
          ps.seq.isEmpty() ? ps.label : (ps.seq + " " + ps.label);
      m_breakdownTable->setItem(r, 1, new QTableWidgetItem(shotText));
      auto *nameItem = new QTableWidgetItem(name);
      nameItem->setData(Qt::UserRole, assetIndex);
      m_breakdownTable->setItem(r, kBreakdownAssetCol, nameItem);
      m_breakdownTable->setItem(r, 3, new QTableWidgetItem(type));
      m_breakdownTable->setItem(
          r, 4,
          new QTableWidgetItem(be.nbOccurrences > 1
                                   ? QString::number(be.nbOccurrences)
                                   : QString()));

      // Cosa troverebbe l'export, ADESSO. E' la colonna che rende il breakdown
      // utile invece che decorativo: un asset che non si risolve va visto qui,
      // non scoperto a export fatto.
      auto *fileItem = new QTableWidgetItem();
      if (found) {
        QString why;
        ZtoryModel::AssetMatch how = ZtoryModel::AssetMatch::None;
        const QString path =
            m->resolveAssetFile(*found, &why, &breakdownDirCache, &how);
        const bool isNear = how == ZtoryModel::AssetMatch::NearName ||
                            how == ZtoryModel::AssetMatch::Convention;
        if (!path.isEmpty()) {
          fileItem->setText(QFileInfo(path).fileName());
          fileItem->setToolTip(
              isNear ? path + "\n\n" + why + "\n" +
                           QObject::tr("Check it, and link it (Assets tab, "
                                       "right-click).")
                     : path);
          fileItem->setForeground(
              QBrush(QColor(isNear ? "#3273DC" : "#22D160")));
        } else if (how == ZtoryModel::AssetMatch::NoFile) {
          fileItem->setText(QObject::tr("— no file (on purpose)"));
          fileItem->setToolTip(why);
          fileItem->setForeground(QBrush(QColor("#9E9E9E")));
        } else {
          fileItem->setText(why);
          fileItem->setToolTip(
              why + QObject::tr("\n\nRight-click to link the file by hand."));
          fileItem->setForeground(QBrush(QColor("#F5A623")));
        }
      }
      m_breakdownTable->setItem(r, 5, fileItem);
      r++;
    }
  }
  m_breakdownTable->resizeColumnsToContents();
}

QWidget *ZtoryProductionPanel::buildAssetsTab() {
  QWidget *w   = new QWidget(this);
  auto *lay    = new QVBoxLayout(w);
  auto *btns   = new QHBoxLayout();
  auto *addBtn = new QPushButton(QObject::tr("+ Add asset"), w);
  auto *remBtn = new QPushButton(QObject::tr("− Remove"), w);
  btns->addWidget(addBtn);
  btns->addWidget(remBtn);
  btns->addStretch();
  lay->addLayout(btns);

  m_assetTable = new QTableWidget(w);
  m_assetTable->setSelectionBehavior(QAbstractItemView::SelectItems);
  m_assetTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_assetTable->setContextMenuPolicy(Qt::CustomContextMenu);
  m_assetTable->verticalHeader()->setVisible(false);
  lay->addWidget(m_assetTable);
  connect(m_assetTable, &QWidget::customContextMenuRequested, this,
          &ZtoryProductionPanel::onAssetContextMenu);

  connect(addBtn, &QPushButton::clicked, this, [this] {
    ZtoryModel::instance()->addAsset(ZtoryModel::kCharacterType, QObject::tr("New asset"));
    persistAssets();
  });
  connect(remBtn, &QPushButton::clicked, this, [this] {
    int row = m_assetTable->currentRow();
    if (row < 0) return;
    ZtoryModel::instance()->removeAssetAt(row);
    persistAssets();
  });
  connect(m_assetTable, &QTableWidget::cellClicked, this,
          &ZtoryProductionPanel::onAssetCellClicked);
  connect(m_assetTable, &QTableWidget::cellDoubleClicked, this,
          [this](int r, int c) { editAssetCell(r, c); });
  connect(m_assetTable, &QTableWidget::itemChanged, this,
          &ZtoryProductionPanel::onAssetItemChanged);
  return w;
}

// Ztoryc: whether an asset has its file, at a glance (Franco, 2026-09-27) —
// a dot beside the name, the details in the tooltip. Green: it has its file,
// linked by hand or found by folder + name (the tooltip says which — the
// rule is strict enough that telling them apart by colour was noise, Franco
// 2026-09-27); red: no file, the export would skip it.
// A dot and not a column: the task columns' indexes are used in three places.
static QIcon linkDot(const QColor &c) {
  QPixmap pm(12, 12);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen);
  p.setBrush(c);
  p.drawEllipse(2, 2, 8, 8);
  return QIcon(pm);
}

void ZtoryProductionPanel::showAssetLink(QTableWidgetItem *item, const Asset &a,
                                         QHash<QString, QFileInfoList> *dirCache) {
  ZtoryModel *m = ZtoryModel::instance();
  QString why;
  ZtoryModel::AssetMatch how = ZtoryModel::AssetMatch::None;
  const QString file = m->resolveAssetFile(a, &why, dirCache, &how);
  QString tip;
  if (how == ZtoryModel::AssetMatch::NoFile) {
    // Grey: no file on purpose — nothing is missing.
    item->setIcon(linkDot(QColor("#9E9E9E")));
    tip = tr("No file on purpose: it is drawn inside another asset.\n"
             "Right-click to change it.");
  } else if (file.isEmpty()) {
    item->setIcon(linkDot(QColor("#FF3860")));
    tip = tr("No file — the export skips it: %1").arg(why);
  } else if (how == ZtoryModel::AssetMatch::NearName ||
             how == ZtoryModel::AssetMatch::Convention) {
    // Blue: deduced — nearly the name, or by the naming convention.
    item->setIcon(linkDot(QColor("#3273DC")));
    tip = tr("Used, but %1.\nRight-click to link it (or rename it).").arg(why);
  } else {
    item->setIcon(linkDot(QColor("#22D160")));
    tip = a.filePath.isEmpty() ? tr("Found by folder and name: %1").arg(file)
                               : tr("Linked: %1").arg(file);
  }
  if (ZtoryModel::isCharacterType(a.type))
    tip += "\n" + (a.rigPsdPath.isEmpty()
                        ? tr("No PSD to rig linked.")
                        : tr("PSD to rig: %1").arg(m->resolveAssetRigPsd(a)));
  item->setToolTip(tip);
}

void ZtoryProductionPanel::rebuildAssets() {
  if (!m_assetTable) return;
  ZtoryModel *m   = ZtoryModel::instance();
  // Union of task types across the asset types in use, in per-type pipeline
  // order — reordering/renaming a type's tasks reflects here immediately.
  m_assetTaskCols = m->assetTaskColumns();
  m_assetLoading  = true;
  m_assetTable->clear();
  const int kFixed = 2;  // Type, Name
  m_assetTable->setColumnCount(kFixed + m_assetTaskCols.size());
  m_assetTable->setRowCount(m->assetCount());
  QStringList headers;
  headers << QObject::tr("Type") << QObject::tr("Name");
  headers += m_assetTaskCols;
  m_assetTable->setHorizontalHeaderLabels(headers);

  QHash<QString, QFileInfoList> dirCache;  // each category folder listed once
  for (int i = 0; i < m->assetCount(); i++) {
    const Asset &as = m->assets()[i];
    // «MP»: Kitsu keeps it in the Main Pack, shared by every episode of the
    // series — told apart from this episode's own assets (Franco, 2026-09-29).
    auto *typeItem = new QTableWidgetItem(
        as.kitsuMainPack ? QString("%1 · MP").arg(as.type) : as.type);
    typeItem->setFlags(Qt::ItemIsEnabled);  // edited via click menu
    typeItem->setTextAlignment(Qt::AlignCenter);
    if (as.kitsuMainPack) {
      typeItem->setForeground(QColor(0x5A, 0xB4, 0xFF));
      typeItem->setToolTip(QObject::tr(
          "Main Pack on Kitsu: shared by every episode of the series."));
    }
    m_assetTable->setItem(i, 0, typeItem);
    auto *nameItem = new QTableWidgetItem(as.name);
    nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable);
    showAssetLink(nameItem, as, &dirCache);
    m_assetTable->setItem(i, 1, nameItem);
    // Only the task types in THIS asset's type pipeline are editable cells; the
    // rest (columns belonging to other types) are blanked and disabled.
    const QStringList typeTasks = m->assetTaskTypesForType(as.type);
    for (int c = 0; c < m_assetTaskCols.size(); c++) {
      auto *it = new QTableWidgetItem();
      it->setTextAlignment(Qt::AlignCenter);
      if (!typeTasks.contains(m_assetTaskCols[c])) {
        it->setFlags(Qt::NoItemFlags);  // not part of this type's pipeline
        it->setBackground(QColor(0, 0, 0, 40));
        m_assetTable->setItem(i, kFixed + c, it);
        continue;
      }
      const TaskState ts = as.tasks.value(m_assetTaskCols[c]);
      it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      QString text = ZtoryModel::taskStatusLabel(ts.status);
      if (!ts.assignees.isEmpty()) text += "\n" + ts.assignees.join(", ");
      it->setText(text);
      it->setBackground(statusColor(ts.status));
      it->setForeground(isLightStatus(ts.status) ? QColor(Qt::black)
                                                 : QColor(Qt::white));
      m_assetTable->setItem(i, kFixed + c, it);
    }
  }
  m_assetTable->resizeColumnsToContents();
  m_assetTable->resizeRowsToContents();
  m_assetLoading = false;
}

void ZtoryProductionPanel::onAssetItemChanged(QTableWidgetItem *it) {
  if (m_assetLoading || !it) return;
  ZtoryModel *m = ZtoryModel::instance();
  int row = it->row();
  if (row < 0 || row >= m->assetCount()) return;
  if (it->column() == 1) {  // Name
    m->assets()[row].name = it->text().trimmed();
    persistAssets();
  }
}

void ZtoryProductionPanel::onAssetCellClicked(int row, int col) {
  ZtoryModel *m = ZtoryModel::instance();
  if (row < 0 || row >= m->assetCount()) return;
  if (col == 0) {  // Type picker
    QMenu menu(this);
    for (const AssetType &t : m->assetTypes()) menu.addAction(t.name);
    QAction *ch = menu.exec(QCursor::pos());
    if (!ch) return;
    m->assets()[row].type = ch->text();
    persistAssets();
    rebuildAssets();
    return;
  }
  // Task cells: left-click selects only (double-click edits, right-click = menu).
}

void ZtoryProductionPanel::onAssetContextMenu(const QPoint &pos) {
  ZtoryModel *m    = ZtoryModel::instance();
  const int kFixed = 2;
  struct Target { int row; QString uuid; QString task; };
  QList<Target> targets;
  for (QTableWidgetItem *it : m_assetTable->selectedItems()) {
    int row = it->row(), col = it->column();
    int ti  = col - kFixed;
    if (ti < 0 || ti >= m_assetTaskCols.size()) continue;
    if (row < 0 || row >= m->assetCount()) continue;
    targets.append({row, m->assets()[row].uuid, m_assetTaskCols[ti]});
  }
  if (targets.isEmpty()) {
    if (QTableWidgetItem *it = m_assetTable->itemAt(pos)) {
      int row = it->row(), ti = it->column() - kFixed;
      if (ti >= 0 && ti < m_assetTaskCols.size() && row >= 0 && row < m->assetCount())
        targets.append({row, m->assets()[row].uuid, m_assetTaskCols[ti]});
    }
  }

  // Nessun task sotto il puntatore: siamo su Type o Name, e li' la voce utile
  // e' il legame al FILE, che appartiene alla riga e non a una cella di task.
  // Senza questo ramo il comando sarebbe raggiungibile solo cliccando per caso
  // su una colonna di task.
  if (targets.isEmpty()) {
    QTableWidgetItem *it = m_assetTable->itemAt(pos);
    if (!it) return;
    const int row = it->row();
    if (row < 0 || row >= m->assetCount()) return;
    const Asset &a    = m->assets()[row];
    const bool isChar = ZtoryModel::isCharacterType(a.type);

    QMenu rowMenu(this);
    QAction *linkAct  = rowMenu.addAction(isChar
                                              ? QObject::tr("Link character scene…")
                                              : QObject::tr("Link file…"));
    // No file on purpose: drawn inside another asset (the books in the
    // library background). Stops any search or deduction of a wrong file.
    // Not for a character: it always has its own scene.
    QAction *noFileAct = nullptr;
    if (!isChar) {
      noFileAct = rowMenu.addAction(
          QObject::tr("No file — drawn inside another asset"));
      noFileAct->setCheckable(true);
      noFileAct->setChecked(a.noFile);
    }
    QAction *clearAct = a.filePath.isEmpty()
                            ? nullptr
                            : rowMenu.addAction(QObject::tr("Clear link"));
    if (!a.filePath.isEmpty()) {
      rowMenu.addSeparator();
      QAction *shown = rowMenu.addAction(a.filePath);
      shown->setEnabled(false);  // mostra il legame, non e' un comando
    }
    // Scostamento dalla politica di progetto, per QUESTO asset. «Use project
    // default» e' una voce a sé e non l'assenza di scelta: si deve poter
    // tornare indietro, e si deve vedere quale delle tre e' attiva.
    // Il PSD da riggare: per un personaggio le opzioni PSD valgono per lui.
    QAction *rigLinkAct = nullptr, *rigClearAct = nullptr;
    if (isChar) {
      rowMenu.addSeparator();
      rigLinkAct = rowMenu.addAction(QObject::tr("Link PSD to rig…"));
      if (!a.rigPsdPath.isEmpty()) {
        rigClearAct   = rowMenu.addAction(QObject::tr("Clear PSD link"));
        QAction *shown = rowMenu.addAction(a.rigPsdPath);
        shown->setEnabled(false);
      }
    }
    QAction *psdAct = nullptr;
    if (QFileInfo(isChar ? m->resolveAssetRigPsd(a) : m->resolveAssetFile(a))
            .suffix()
            .compare("psd", Qt::CaseInsensitive) == 0)
      psdAct = rowMenu.addAction(QObject::tr("PSD import options…"));
    // A file found by a name that is only nearly the asset's (the blue dot).
    QAction *renameAct = nullptr, *linkAsIsAct = nullptr;
    ZtoryModel::AssetMatch how = ZtoryModel::AssetMatch::None;
    const QString nearFile = m->resolveAssetFile(a, nullptr, nullptr, &how);
    if (how == ZtoryModel::AssetMatch::NearName ||
        how == ZtoryModel::AssetMatch::Convention) {
      rowMenu.addSeparator();
      const QFileInfo nf(nearFile);
      // Renamed by the production's convention (Franco, 2026-09-27), keeping
      // the file's version when it has one.
      QString part;
      int ver = 0;
      if (!m->parseAssetFileName(nf.completeBaseName(), a, &part, &ver) || ver < 1)
        ver = 1;
      const QString target = m->assetFileName(a, ver, nf.suffix());
      if (target.compare(nf.fileName(), Qt::CaseSensitive) != 0)
        renameAct = rowMenu.addAction(
            QObject::tr("Rename the file to «%1»").arg(target));
      linkAsIsAct = rowMenu.addAction(
          QObject::tr("Link «%1» as it is").arg(nf.fileName()));
    }

    rowMenu.addSeparator();
    const AssetImportPolicy eff = m->effectiveImportPolicy(a);
    QMenu *modeMenu = rowMenu.addMenu(QObject::tr("Comes in as"));
    struct ModeAct { QAction *act; AssetImportPolicy::Mode mode; };
    QVector<ModeAct> modeActs;
    auto addMode = [&](const QString &text, AssetImportPolicy::Mode mo) {
      QAction *act = modeMenu->addAction(text);
      act->setCheckable(true);
      act->setChecked(a.importPolicy.mode == mo);
      modeActs.push_back({act, mo});
    };
    addMode(QObject::tr("Use project default (%1)")
                .arg(eff.mode == AssetImportPolicy::Import
                         ? QObject::tr("Import")
                         : QObject::tr("Load")),
            AssetImportPolicy::Default);
    modeMenu->addSeparator();
    addMode(QObject::tr("Load — point at the file"), AssetImportPolicy::Load);
    addMode(QObject::tr("Import — copy into the shot"), AssetImportPolicy::Import);

    QAction *ch = rowMenu.exec(m_assetTable->viewport()->mapToGlobal(pos));
    if (!ch) return;
    if (ch == psdAct) { editAssetPsdOptions(row); return; }
    if (noFileAct && ch == noFileAct) {
      m->setAssetNoFile(row, !a.noFile);
      persistAssets();
      rebuildAssets();
      rebuildBreakdown();
      return;
    }
    if (ch == linkAsIsAct) {
      m->setAssetFilePath(row, nearFile);
      persistAssets();
      rebuildAssets();
      rebuildBreakdown();
      return;
    }
    if (ch == renameAct) {
      QString part;
      int ver = 0;
      const QFileInfo nf(nearFile);
      if (!m->parseAssetFileName(nf.completeBaseName(), a, &part, &ver) || ver < 1)
        ver = 1;
      renameAssetFile(row, nearFile, m->assetFileName(a, ver, nf.suffix()));
      return;
    }
    if (ch == rigLinkAct) { linkAssetRigPsdInteractive(row); return; }
    if (ch == rigClearAct) {
      m->setAssetRigPsd(row, QString());
      persistAssets();
      rebuildAssets();
      return;
    }

    for (const ModeAct &ma : modeActs)
      if (ch == ma.act) {
        AssetImportPolicy p = a.importPolicy;  // le opzioni PSD restano
        p.mode = ma.mode;
        m->setAssetImportPolicy(row, p);
        persistAssets();
        rebuildAssets();
        rebuildBreakdown();
        return;
      }

    if (ch == clearAct) {
      m->setAssetFilePath(row, QString());
    } else if (ch == linkAct) {
      linkAssetFileInteractive(row);  // fa da sé persist + rebuild
      return;
    } else {
      return;
    }
    persistAssets();
    rebuildAssets();
    rebuildBreakdown();
    return;
  }

  QMenu menu(this);
  QMenu *sm = menu.addMenu(QObject::tr("Set status (%1 tasks)").arg(targets.size()));
  for (TaskStatus s : kAllStatuses) {
    QPixmap pm(14, 14);
    pm.fill(statusColor(s));
    sm->addAction(QIcon(pm), ZtoryModel::taskStatusLabel(s))
        ->setData(static_cast<int>(s));
  }
  QAction *assignAct = menu.addAction(QObject::tr("Set assignees…"));
  QAction *chosen    = menu.exec(m_assetTable->viewport()->mapToGlobal(pos));
  if (!chosen) return;

  TUndoManager::manager()->beginBlock();
  {
    QSignalBlocker block(m);
    if (chosen == assignAct) {
      QStringList newAssign;
      const QStringList cur =
          m->assets()[targets.first().row].tasks.value(targets.first().task).assignees;
      if (pickAssignees(this, QObject::tr("selected tasks"), cur, newAssign))
        for (const Target &t : targets) {
          QStringList old = m->assets()[t.row].tasks.value(t.task).assignees;
          if (old == newAssign) continue;
          m->setAssetTaskAssigneesByUuid(t.uuid, t.task, newAssign);
          TUndoManager::manager()->add(
              new AssetAssigneeUndo(t.uuid, t.task, old, newAssign));
        }
    } else {
      TaskStatus s = static_cast<TaskStatus>(chosen->data().toInt());
      for (const Target &t : targets) {
        TaskStatus old = m->assets()[t.row].tasks.value(t.task).status;
        if (old == s) continue;
        QVector<ZtoryTaskFlow::Change> fx;
        ZtoryTaskFlow::setStatus(ZtoryTaskFlow::Entity::Asset, t.uuid, t.task,
                                 s, ZtoryTaskFlow::Origin::User,
                                 /*batch=*/true, &fx);
        TUndoManager::manager()->add(
            new AssetStatusUndo(t.uuid, t.task, old, s, fx));
      }
    }
  }
  TUndoManager::manager()->endBlock();
  persistAssets();
  rebuildAssets();
}

void ZtoryProductionPanel::editAssetCell(int row, int col) {
  const int kFixed = 2;
  const int ti     = col - kFixed;
  if (ti < 0 || ti >= m_assetTaskCols.size()) return;
  ZtoryModel *m = ZtoryModel::instance();
  if (row < 0 || row >= m->assetCount()) return;
  const QString     taskType  = m_assetTaskCols[ti];
  const Asset      &as        = m->assets()[row];
  const TaskState   cur       = as.tasks.value(taskType);
  const TaskStatus  oldStatus = cur.status;
  const QStringList oldAssign = cur.assignees;
  const QString     uuid      = as.uuid;

  TaskEditResult r = pickTaskEdit(this, taskType, oldStatus, oldAssign);
  if (r.kind == TaskEditResult::Status) {
    if (r.status == oldStatus) return;
    QVector<ZtoryTaskFlow::Change> fx;
    ZtoryTaskFlow::setStatus(ZtoryTaskFlow::Entity::Asset, uuid, taskType,
                             r.status, ZtoryTaskFlow::Origin::User,
                             /*batch=*/true, &fx);
    emit m->assetsChanged();
    persistAssets();
    TUndoManager::manager()->add(
        new AssetStatusUndo(uuid, taskType, oldStatus, r.status, fx));
  } else if (r.kind == TaskEditResult::Assignees) {
    if (r.assignees == oldAssign) return;
    m->setAssetTaskAssignees(row, taskType, r.assignees);
    persistAssets();
    TUndoManager::manager()->add(
        new AssetAssigneeUndo(uuid, taskType, oldAssign, r.assignees));
  }
  clearUntitledSceneDirty();
}

//-----------------------------------------------------------------------------
// Workflows tab — Kitsu-style: define workflows (techniques) and their custom,
// ordered task types. Drives which task columns apply to each shot.

QWidget *ZtoryProductionPanel::buildWorkflowsTab() {
  QWidget *w = new QWidget(this);
  auto *root = new QHBoxLayout(w);

  // Left: workflows (techniques).
  auto *leftCol = new QVBoxLayout();
  leftCol->addWidget(new QLabel(QObject::tr("Workflows (double-click to rename):"), w));
  m_techList = new QListWidget(w);
  leftCol->addWidget(m_techList);
  auto *lb   = new QHBoxLayout();
  auto *addT = new QPushButton(QObject::tr("+ Workflow"), w);
  auto *remT = new QPushButton(QObject::tr("− Workflow"), w);
  lb->addWidget(addT);
  lb->addWidget(remT);
  lb->addStretch();
  leftCol->addLayout(lb);
  root->addLayout(leftCol, 1);

  // Right: task types of the selected workflow.
  auto *rightCol = new QVBoxLayout();
  rightCol->addWidget(
      new QLabel(QObject::tr("Task types (double-click to rename):"), w));
  m_taskTypeList = new QListWidget(w);
  // Allow reordering task types by dragging (pipeline order matters).
  m_taskTypeList->setDragDropMode(QAbstractItemView::InternalMove);
  m_taskTypeList->setDefaultDropAction(Qt::MoveAction);
  rightCol->addWidget(m_taskTypeList);
  auto *rb    = new QHBoxLayout();
  auto *addTT = new QPushButton(QObject::tr("+ Task"), w);
  auto *remTT = new QPushButton(QObject::tr("− Task"), w);
  auto *upTT   = new QToolButton(w);
  auto *downTT = new QToolButton(w);
  upTT->setArrowType(Qt::UpArrow);
  downTT->setArrowType(Qt::DownArrow);
  upTT->setToolTip(QObject::tr("Move task earlier in the pipeline"));
  downTT->setToolTip(QObject::tr("Move task later in the pipeline"));
  upTT->setFixedWidth(32);
  downTT->setFixedWidth(32);
  rb->addWidget(addTT);
  rb->addWidget(remTT);
  rb->addStretch();
  rb->addWidget(upTT);
  rb->addWidget(downTT);
  rightCol->addLayout(rb);
  root->addLayout(rightCol, 2);

  connect(m_techList, &QListWidget::currentRowChanged, this,
          [this](int) { reloadTaskTypeList(); });
  connect(m_techList, &QListWidget::itemChanged, this, [this](QListWidgetItem *it) {
    if (m_wfLoading) return;
    int row     = m_techList->row(it);
    auto &techs = ZtoryModel::instance()->techniques();
    if (row >= 0 && row < (int)techs.size()) {
      techs[row].name = it->text().trimmed();
      ZtoryModel::instance()->saveProjectDb();
      reloadProjectTab();  // default-technique combo reflects the new name
    }
  });
  connect(addT, &QPushButton::clicked, this, [this] {
    ZtoryModel::instance()->techniques().push_back(
        Technique{QObject::tr("New workflow"), {}});
    ZtoryModel::instance()->saveProjectDb();
    reloadWorkflowsTab();
    m_techList->setCurrentRow(m_techList->count() - 1);
  });
  connect(remT, &QPushButton::clicked, this, [this] {
    int row     = m_techList->currentRow();
    auto &techs = ZtoryModel::instance()->techniques();
    if (row < 0 || row >= (int)techs.size()) return;
    techs.erase(techs.begin() + row);
    ZtoryModel::instance()->saveProjectDb();
    reloadWorkflowsTab();
    emit ZtoryModel::instance()->taskStatusChanged();  // shot columns may change
  });
  connect(addTT, &QPushButton::clicked, this, [this] {
    if (m_techList->currentRow() < 0) return;
    auto *it = new QListWidgetItem(QObject::tr("newtask"), m_taskTypeList);
    it->setFlags(it->flags() | Qt::ItemIsEditable);
    m_taskTypeList->setCurrentItem(it);
    m_taskTypeList->editItem(it);
  });
  connect(remTT, &QPushButton::clicked, this, [this] {
    delete m_taskTypeList->currentItem();
    applyTaskTypesToTechnique();
  });
  // Move current task up/down within the pipeline order.
  auto moveCurrentTask = [this](int delta) {
    int row = m_taskTypeList->currentRow();
    int dst = row + delta;
    if (row < 0 || dst < 0 || dst >= m_taskTypeList->count()) return;
    QListWidgetItem *it = m_taskTypeList->takeItem(row);
    m_taskTypeList->insertItem(dst, it);
    m_taskTypeList->setCurrentRow(dst);
    applyTaskTypesToTechnique();
  };
  connect(upTT,   &QPushButton::clicked, this, [moveCurrentTask] { moveCurrentTask(-1); });
  connect(downTT, &QPushButton::clicked, this, [moveCurrentTask] { moveCurrentTask(+1); });
  connect(m_taskTypeList, &QListWidget::itemChanged, this,
          [this](QListWidgetItem *) { applyTaskTypesToTechnique(); });
  // Persist the new order after a drag-and-drop reorder.
  connect(m_taskTypeList->model(), &QAbstractItemModel::rowsMoved, this,
          [this] { applyTaskTypesToTechnique(); });
  return w;
}

void ZtoryProductionPanel::reloadWorkflowsTab() {
  if (!m_techList) return;
  m_wfLoading = true;
  m_techList->clear();
  for (const Technique &t : ZtoryModel::instance()->techniques()) {
    auto *it = new QListWidgetItem(t.name, m_techList);
    it->setFlags(it->flags() | Qt::ItemIsEditable);
  }
  m_wfLoading = false;
  if (m_techList->count() > 0)
    m_techList->setCurrentRow(0);
  else
    reloadTaskTypeList();
}

void ZtoryProductionPanel::reloadTaskTypeList() {
  if (!m_taskTypeList) return;
  m_wfLoading = true;
  m_taskTypeList->clear();
  int row            = m_techList ? m_techList->currentRow() : -1;
  const auto &techs  = ZtoryModel::instance()->techniques();
  if (row >= 0 && row < (int)techs.size())
    for (const QString &tt : techs[row].taskTypes) {
      auto *it = new QListWidgetItem(tt, m_taskTypeList);
      it->setFlags(it->flags() | Qt::ItemIsEditable);
    }
  m_wfLoading = false;
}

void ZtoryProductionPanel::applyTaskTypesToTechnique() {
  if (m_wfLoading || !m_taskTypeList || !m_techList) return;
  int row     = m_techList->currentRow();
  auto &techs = ZtoryModel::instance()->techniques();
  if (row < 0 || row >= (int)techs.size()) return;
  QStringList tt;
  for (int i = 0; i < m_taskTypeList->count(); i++) {
    QString s = m_taskTypeList->item(i)->text().trimmed();
    if (!s.isEmpty()) tt << s;
  }
  techs[row].taskTypes = tt;
  ZtoryModel::instance()->saveProjectDb();
  emit ZtoryModel::instance()->taskStatusChanged();  // shot matrix columns refresh
}

//-----------------------------------------------------------------------------
// Asset Types tab — same two-pane editor as Workflows, but for the custom asset
// types and their per-type task pipelines (Kitsu-aligned).

QWidget *ZtoryProductionPanel::buildAssetTypesTab() {
  QWidget *w = new QWidget(this);
  auto *root = new QHBoxLayout(w);

  // Left: asset types.
  auto *leftCol = new QVBoxLayout();
  leftCol->addWidget(
      new QLabel(QObject::tr("Asset types (double-click to rename):"), w));
  m_assetTypeList = new QListWidget(w);
  leftCol->addWidget(m_assetTypeList);
  auto *lb   = new QHBoxLayout();
  auto *addT = new QPushButton(QObject::tr("+ Type"), w);
  auto *remT = new QPushButton(QObject::tr("− Type"), w);
  lb->addWidget(addT);
  lb->addWidget(remT);
  lb->addStretch();
  leftCol->addLayout(lb);
  root->addLayout(leftCol, 1);

  // Right: task pipeline of the selected asset type.
  auto *rightCol = new QVBoxLayout();
  rightCol->addWidget(
      new QLabel(QObject::tr("Task types (double-click to rename):"), w));
  m_assetTaskTypeList = new QListWidget(w);
  m_assetTaskTypeList->setDragDropMode(QAbstractItemView::InternalMove);
  m_assetTaskTypeList->setDefaultDropAction(Qt::MoveAction);
  rightCol->addWidget(m_assetTaskTypeList);
  auto *rb     = new QHBoxLayout();
  auto *addTT  = new QPushButton(QObject::tr("+ Task"), w);
  auto *remTT  = new QPushButton(QObject::tr("− Task"), w);
  auto *upTT   = new QToolButton(w);
  auto *downTT = new QToolButton(w);
  upTT->setArrowType(Qt::UpArrow);
  downTT->setArrowType(Qt::DownArrow);
  upTT->setToolTip(QObject::tr("Move task earlier in the pipeline"));
  downTT->setToolTip(QObject::tr("Move task later in the pipeline"));
  upTT->setFixedWidth(32);
  downTT->setFixedWidth(32);
  rb->addWidget(addTT);
  rb->addWidget(remTT);
  rb->addStretch();
  rb->addWidget(upTT);
  rb->addWidget(downTT);
  rightCol->addLayout(rb);
  root->addLayout(rightCol, 2);

  connect(m_assetTypeList, &QListWidget::currentRowChanged, this,
          [this](int) { reloadAssetTaskTypeList(); });
  connect(m_assetTypeList, &QListWidget::itemChanged, this,
          [this](QListWidgetItem *it) {
            if (m_atLoading) return;
            int row     = m_assetTypeList->row(it);
            auto &types = ZtoryModel::instance()->assetTypes();
            if (row >= 0 && row < (int)types.size()) {
              types[row].name = it->text().trimmed();
              ZtoryModel::instance()->saveProjectDb();
              emit ZtoryModel::instance()->assetsChanged();  // type picker + table
            }
          });
  connect(addT, &QPushButton::clicked, this, [this] {
    ZtoryModel::instance()->assetTypes().push_back(
        AssetType{QObject::tr("New type"),
                  ZtoryModel::canonicalAssetTaskOrder()});
    ZtoryModel::instance()->saveProjectDb();
    reloadAssetTypesTab();
    m_assetTypeList->setCurrentRow(m_assetTypeList->count() - 1);
  });
  connect(remT, &QPushButton::clicked, this, [this] {
    int row     = m_assetTypeList->currentRow();
    auto &types = ZtoryModel::instance()->assetTypes();
    if (row < 0 || row >= (int)types.size()) return;
    types.erase(types.begin() + row);
    ZtoryModel::instance()->saveProjectDb();
    reloadAssetTypesTab();
    emit ZtoryModel::instance()->assetsChanged();
  });
  connect(addTT, &QPushButton::clicked, this, [this] {
    if (m_assetTypeList->currentRow() < 0) return;
    auto *it = new QListWidgetItem(QObject::tr("newtask"), m_assetTaskTypeList);
    it->setFlags(it->flags() | Qt::ItemIsEditable);
    m_assetTaskTypeList->setCurrentItem(it);
    m_assetTaskTypeList->editItem(it);
  });
  connect(remTT, &QPushButton::clicked, this, [this] {
    delete m_assetTaskTypeList->currentItem();
    applyAssetTaskTypesToType();
  });
  auto moveCurrentTask = [this](int delta) {
    int row = m_assetTaskTypeList->currentRow();
    int dst = row + delta;
    if (row < 0 || dst < 0 || dst >= m_assetTaskTypeList->count()) return;
    QListWidgetItem *it = m_assetTaskTypeList->takeItem(row);
    m_assetTaskTypeList->insertItem(dst, it);
    m_assetTaskTypeList->setCurrentRow(dst);
    applyAssetTaskTypesToType();
  };
  connect(upTT,   &QPushButton::clicked, this, [moveCurrentTask] { moveCurrentTask(-1); });
  connect(downTT, &QPushButton::clicked, this, [moveCurrentTask] { moveCurrentTask(+1); });
  connect(m_assetTaskTypeList, &QListWidget::itemChanged, this,
          [this](QListWidgetItem *) { applyAssetTaskTypesToType(); });
  connect(m_assetTaskTypeList->model(), &QAbstractItemModel::rowsMoved, this,
          [this] { applyAssetTaskTypesToType(); });
  return w;
}

void ZtoryProductionPanel::reloadAssetTypesTab() {
  if (!m_assetTypeList) return;
  m_atLoading = true;
  m_assetTypeList->clear();
  for (const AssetType &t : ZtoryModel::instance()->assetTypes()) {
    auto *it = new QListWidgetItem(t.name, m_assetTypeList);
    it->setFlags(it->flags() | Qt::ItemIsEditable);
  }
  m_atLoading = false;
  if (m_assetTypeList->count() > 0)
    m_assetTypeList->setCurrentRow(0);
  else
    reloadAssetTaskTypeList();
}

void ZtoryProductionPanel::reloadAssetTaskTypeList() {
  if (!m_assetTaskTypeList) return;
  m_atLoading = true;
  m_assetTaskTypeList->clear();
  int row            = m_assetTypeList ? m_assetTypeList->currentRow() : -1;
  const auto &types  = ZtoryModel::instance()->assetTypes();
  if (row >= 0 && row < (int)types.size())
    for (const QString &tt : types[row].taskTypes) {
      auto *it = new QListWidgetItem(tt, m_assetTaskTypeList);
      it->setFlags(it->flags() | Qt::ItemIsEditable);
    }
  m_atLoading = false;
}

void ZtoryProductionPanel::applyAssetTaskTypesToType() {
  if (m_atLoading || !m_assetTaskTypeList || !m_assetTypeList) return;
  int row     = m_assetTypeList->currentRow();
  auto &types = ZtoryModel::instance()->assetTypes();
  if (row < 0 || row >= (int)types.size()) return;
  QStringList tt;
  for (int i = 0; i < m_assetTaskTypeList->count(); i++) {
    QString s = m_assetTaskTypeList->item(i)->text().trimmed();
    if (!s.isEmpty()) tt << s;
  }
  types[row].taskTypes = tt;
  ZtoryModel::instance()->saveProjectDb();
  emit ZtoryModel::instance()->assetsChanged();  // asset table columns refresh
}

//-----------------------------------------------------------------------------

void ZtoryProductionPanel::scheduleRebuild() {
  if (m_rebuildScheduled) return;
  m_rebuildScheduled = true;
  QTimer::singleShot(0, this, [this] {
    m_rebuildScheduled = false;
    rebuild();
  });
}

void ZtoryProductionPanel::rebuild() {
  ZtoryModel *m = ZtoryModel::instance();

  m_taskCols = m->spreadsheetTaskColumns();
  const QStringList &taskCols = m_taskCols;
  const int fps = m->fps() > 0 ? m->fps() : 25;

  // Prefer the project-level shots (multi-storyboard); fall back to scene shots.
  const bool useProjectShots = !m->projectShots().empty();
  // kFixed: Source column only in project mode (col 0 = Source).
  // Project mode: Source | Shot | Thumb | Frames | Sec/Fr | Workflow | Done | tasks...
  // Legacy mode:          Shot | Thumb | Frames | Sec/Fr | Workflow | Done | tasks...
  const int kFixed = useProjectShots ? 7 : 6;
  const int kSrcCol = useProjectShots ? 0 : -1;  // Source column index (or -1)

  StoryboardPanel *board = findBoard();
  const int rowCount = useProjectShots ? (int)m->projectShots().size() : m->shotCount();

  // Remove progress-bar widgets from previous build (clear() doesn't).
  const int doneCol = useProjectShots ? 6 : 5;
  for (int r = 0; r < m_table->rowCount(); r++) m_table->removeCellWidget(r, doneCol);
  m_table->clear();
  m_table->setColumnCount(kFixed + taskCols.size());
  m_table->setRowCount(rowCount);
  m_table->setIconSize(QSize(72, 40));

  QStringList headers;
  if (useProjectShots)
    headers << QObject::tr("Storyboard");
  headers << QObject::tr("Shot") << QString() << QObject::tr("Frames")
          << QObject::tr("In-Out") << QObject::tr("Workflow") << QObject::tr("Done");
  headers += taskCols;
  m_table->setHorizontalHeaderLabels(headers);

  if (useProjectShots) {
    // ── Project-shots mode: read from m_projectShots ────────────────────────
    const auto &pshots = m->projectShots();
    // Cumulative frame in/out (edit timecode) per source — aligns with Kitsu.
    const auto frameRanges = m->projectShotFrameRanges();
    // Build uuid→scene-index map for thumbnails of the open storyboard.
    QHash<QString, int> uuidToSceneIdx;
    for (int i = 0; i < m->shotCount(); i++)
      if (!m->shot(i).uuid.isEmpty()) uuidToSceneIdx[m->shot(i).uuid] = i;

    for (int i = 0; i < (int)pshots.size(); i++) {
      const ProjectShot &ps = pshots[i];

      // Source (storyboard file).
      auto *srcItem = new QTableWidgetItem(ps.source);
      srcItem->setFlags(Qt::ItemIsEnabled);
      srcItem->setForeground(QColor("#aaaaaa"));
      m_table->setItem(i, 0, srcItem);

      // Shot label.
      QString fullLabel = ps.seq.isEmpty() ? ps.label
                                           : ps.seq + "_" + ps.label;
      auto *shotItem = new QTableWidgetItem(fullLabel);
      shotItem->setFlags(Qt::ItemIsEnabled);
      // Store uuid in UserRole for editing/undo.
      shotItem->setData(Qt::UserRole, ps.uuid);
      QFont f = shotItem->font();
      f.setBold(true);
      shotItem->setFont(f);
      m_table->setItem(i, 1, shotItem);

      // Thumbnail — read from the persistent cache (keyed by uuid) so thumbs
      // remain visible even after switching to a different storyboard scene.
      auto *thumb = new QTableWidgetItem();
      thumb->setFlags(Qt::ItemIsEnabled);
      {
        QPixmap pm = m->thumbCache().value(ps.uuid);
        if (pm.isNull()) {
          // Fallback: live Board data if this shot belongs to the open scene.
          auto sceneIt = uuidToSceneIdx.find(ps.uuid);
          if (board && sceneIt != uuidToSceneIdx.end())
            pm = board->firstPanelThumbnail(sceneIt.value());
          // Warm the cache so future rebuilds don't need the Board.
          if (!pm.isNull())
            m->updateThumbCache(ps.uuid, pm);
        }
        if (!pm.isNull())
          thumb->setData(Qt::DecorationRole,
                         pm.scaled(72, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
      }
      m_table->setItem(i, 2, thumb);

      const int frames = ps.frames;
      auto *frItem = new QTableWidgetItem(QString::number(frames));
      frItem->setFlags(Qt::ItemIsEnabled);
      frItem->setTextAlignment(Qt::AlignCenter);
      m_table->setItem(i, 3, frItem);
      const auto fr = (i < (int)frameRanges.size()) ? frameRanges[i]
                                                    : std::make_pair(0, 0);
      auto *tcItem = new QTableWidgetItem(
          frameToTimecode(fr.first, fps) + "-" + frameToTimecode(fr.second, fps));
      tcItem->setFlags(Qt::ItemIsEnabled);
      tcItem->setTextAlignment(Qt::AlignCenter);
      m_table->setItem(i, 4, tcItem);

      auto *wfItem = new QTableWidgetItem(m->techniqueForProjectShot(ps));
      wfItem->setFlags(Qt::ItemIsEnabled);
      wfItem->setTextAlignment(Qt::AlignCenter);
      wfItem->setToolTip(QObject::tr("Click to set this shot's workflow"));
      m_table->setItem(i, 5, wfItem);

      const QStringList shotTasks = m->taskTypesForProjectShot(ps);
      int done = 0;
      for (const QString &tt : shotTasks)
        if (ps.tasks.value(tt).status == TaskStatus::Done) done++;
      if (!shotTasks.isEmpty()) {
        auto *bar = new QProgressBar();
        bar->setRange(0, shotTasks.size());
        bar->setValue(done);
        bar->setFormat(QString("%1/%2").arg(done).arg(shotTasks.size()));
        bar->setAlignment(Qt::AlignCenter);
        bar->setMaximumHeight(18);
        bar->setStyleSheet(
            "QProgressBar{border:1px solid #555;border-radius:3px;background:#333;"
            "color:#fff;font-size:10px;}"
            "QProgressBar::chunk{background:#22D160;border-radius:2px;}");
        m_table->setCellWidget(i, 6, bar);
      } else {
        auto *empty = new QTableWidgetItem();
        empty->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(i, 6, empty);
      }

      for (int c = 0; c < taskCols.size(); c++) {
        const QString &tt = taskCols[c];
        auto *it = new QTableWidgetItem();
        it->setTextAlignment(Qt::AlignCenter);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        if (!shotTasks.contains(tt)) {
          it->setText(QObject::tr("N/A"));
          it->setForeground(QColor("#777777"));
          it->setBackground(QColor("#3a3a3a"));
        } else {
          const TaskState ts = ps.tasks.value(tt);
          QString text = ZtoryModel::taskStatusLabel(ts.status);
          if (!ts.assignees.isEmpty()) text += "\n" + ts.assignees.join(", ");
          it->setText(text);
          if (!ts.assignees.isEmpty())
            it->setToolTip(QObject::tr("Assignees: %1").arg(ts.assignees.join(", ")));
          it->setBackground(statusColor(ts.status));
          it->setForeground(isLightStatus(ts.status) ? QColor(Qt::black)
                                                      : QColor(Qt::white));
        }
        m_table->setItem(i, kFixed + c, it);
      }
    }
  } else {
    // ── Legacy mode: read from scene shots (m_shots) ────────────────────────
    int legacyAcc = 0;  // running frame offset for cumulative in/out (timecode)
    for (int i = 0; i < m->shotCount(); i++) {
      const ShotData &sd = m->shot(i);

      auto *shotItem = new QTableWidgetItem(m->fullLabel(i));
      shotItem->setFlags(Qt::ItemIsEnabled);
      shotItem->setData(Qt::UserRole, sd.uuid);
      QFont f = shotItem->font();
      f.setBold(true);
      shotItem->setFont(f);
      m_table->setItem(i, 0, shotItem);

      auto *thumb = new QTableWidgetItem();
      thumb->setFlags(Qt::ItemIsEnabled);
      QPixmap pm = board ? board->firstPanelThumbnail(i) : QPixmap();
      if (!pm.isNull())
        thumb->setData(Qt::DecorationRole,
                       pm.scaled(72, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
      m_table->setItem(i, 1, thumb);

      const int frames = sd.totalDuration();
      auto *frItem = new QTableWidgetItem(QString::number(frames));
      frItem->setFlags(Qt::ItemIsEnabled);
      frItem->setTextAlignment(Qt::AlignCenter);
      m_table->setItem(i, 2, frItem);
      const int inF = legacyAcc + 1, outF = legacyAcc + frames;
      legacyAcc = outF;
      auto *tcItem = new QTableWidgetItem(
          frameToTimecode(inF, fps) + "-" + frameToTimecode(outF, fps));
      tcItem->setFlags(Qt::ItemIsEnabled);
      tcItem->setTextAlignment(Qt::AlignCenter);
      m_table->setItem(i, 3, tcItem);

      auto *wfItem = new QTableWidgetItem(m->techniqueForShot(i));
      wfItem->setFlags(Qt::ItemIsEnabled);
      wfItem->setTextAlignment(Qt::AlignCenter);
      wfItem->setToolTip(QObject::tr("Click to set this shot's workflow"));
      m_table->setItem(i, 4, wfItem);

      const QStringList shotTasks = m->taskTypesForShot(i);
      int done = 0;
      for (const QString &tt : shotTasks)
        if (sd.tasks.value(tt).status == TaskStatus::Done) done++;
      if (!shotTasks.isEmpty()) {
        auto *bar = new QProgressBar();
        bar->setRange(0, shotTasks.size());
        bar->setValue(done);
        bar->setFormat(QString("%1/%2").arg(done).arg(shotTasks.size()));
        bar->setAlignment(Qt::AlignCenter);
        bar->setMaximumHeight(18);
        bar->setStyleSheet(
            "QProgressBar{border:1px solid #555;border-radius:3px;background:#333;"
            "color:#fff;font-size:10px;}"
            "QProgressBar::chunk{background:#22D160;border-radius:2px;}");
        m_table->setCellWidget(i, 5, bar);
      } else {
        auto *empty = new QTableWidgetItem();
        empty->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(i, 5, empty);
      }

    for (int c = 0; c < taskCols.size(); c++) {
      const QString &tt = taskCols[c];
      auto *it = new QTableWidgetItem();
      it->setTextAlignment(Qt::AlignCenter);
      it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

      if (!shotTasks.contains(tt)) {
        // Task type not part of this shot's technique → not applicable.
        it->setText(QObject::tr("N/A"));
        it->setForeground(QColor("#777777"));
        it->setBackground(QColor("#3a3a3a"));
      } else {
        const TaskState ts = sd.tasks.value(tt);
        QString text = ZtoryModel::taskStatusLabel(ts.status);
        if (!ts.assignees.isEmpty()) text += "\n" + ts.assignees.join(", ");
        it->setText(text);
        if (!ts.assignees.isEmpty())
          it->setToolTip(QObject::tr("Assignees: %1").arg(ts.assignees.join(", ")));
        it->setBackground(statusColor(ts.status));
        it->setForeground(isLightStatus(ts.status) ? QColor(Qt::black)
                                                    : QColor(Qt::white));
      }
      m_table->setItem(i, kFixed + c, it);
    }
    }  // end outer shots for loop (legacy)
  }  // end else (legacy mode)

  m_table->resizeColumnsToContents();
  m_table->resizeRowsToContents();

}

//-----------------------------------------------------------------------------

void ZtoryProductionPanel::onCellClicked(int row, int col) {
  ZtoryModel *m = ZtoryModel::instance();
  const bool useProjectShots = !m->projectShots().empty();
  const int wfCol = useProjectShots ? 5 : 4;  // Workflow column index

  if (col == wfCol) {  // Workflow picker
    QMenu menu(this);
    QAction *defA = menu.addAction(
        QObject::tr("(project default: %1)").arg(m->defaultTechnique()));
    defA->setData(QString());
    menu.addSeparator();
    for (const Technique &t : m->techniques())
      menu.addAction(t.name)->setData(t.name);
    QAction *ch = menu.exec(QCursor::pos());
    if (!ch) return;
    if (useProjectShots) {
      if (row < 0 || row >= (int)m->projectShots().size()) return;
      const QString uuid = m->projectShots()[row].uuid;
      m->setProjectShotTechnique(uuid, ch->data().toString());
      persistProjectDb();
    } else {
      if (row < 0 || row >= m->shotCount()) return;
      m->shot(row).technique = ch->data().toString();
      persistViaBoard();
      emit m->taskStatusChanged();
    }
    return;
  }
  // Task cells: a single left-click only selects (so shift/⌘ multi-select works
  // for batch editing). Double-click edits one cell; right-click opens the menu.
}

//-----------------------------------------------------------------------------
// Batch edit: apply a status / assignees to every selected, applicable task
// cell at once (drag-select a block, then right-click).

void ZtoryProductionPanel::onShotContextMenu(const QPoint &pos) {
  ZtoryModel *m    = ZtoryModel::instance();
  const bool usePS = !m->projectShots().empty();
  const int kFixed = usePS ? 7 : 6;
  struct Target { int row; QString task; QString uuid; };
  QList<Target> targets;

  auto rowValid = [&](int row) {
    return usePS ? (row >= 0 && row < (int)m->projectShots().size())
                 : (row >= 0 && row < m->shotCount());
  };
  auto taskTypes = [&](int row) -> QStringList {
    return usePS ? m->taskTypesForProjectShot(m->projectShots()[row])
                 : m->taskTypesForShot(row);
  };
  auto shotUuid = [&](int row) -> QString {
    return usePS ? m->projectShots()[row].uuid : m->shot(row).uuid;
  };

  for (QTableWidgetItem *it : m_table->selectedItems()) {
    int row = it->row(), col = it->column();
    int ti  = col - kFixed;
    if (ti < 0 || ti >= m_taskCols.size()) continue;
    if (!rowValid(row)) continue;
    const QString &tt = m_taskCols[ti];
    if (!taskTypes(row).contains(tt)) continue;
    targets.append({row, tt, shotUuid(row)});
  }
  if (targets.isEmpty()) {
    if (QTableWidgetItem *it = m_table->itemAt(pos)) {
      int row = it->row(), ti = it->column() - kFixed;
      if (ti >= 0 && ti < m_taskCols.size() && rowValid(row) &&
          taskTypes(row).contains(m_taskCols[ti]))
        targets.append({row, m_taskCols[ti], shotUuid(row)});
    }
  }
  if (targets.isEmpty()) return;

  QMenu menu(this);
  QMenu *sm = menu.addMenu(QObject::tr("Set status (%1 tasks)").arg(targets.size()));
  for (TaskStatus s : kAllStatuses) {
    QPixmap pm(14, 14);
    pm.fill(statusColor(s));
    sm->addAction(QIcon(pm), ZtoryModel::taskStatusLabel(s))
        ->setData(static_cast<int>(s));
  }
  QAction *assignAct = menu.addAction(QObject::tr("Set assignees…"));
  QAction *chosen    = menu.exec(m_table->viewport()->mapToGlobal(pos));
  if (!chosen) return;

  TUndoManager::manager()->beginBlock();
  {
    QSignalBlocker block(m);
    if (chosen == assignAct) {
      QStringList newAssign;
      const QString &firstUuid = targets.first().uuid;
      QStringList cur;
      if (usePS) {
        for (const ProjectShot &ps : m->projectShots())
          if (ps.uuid == firstUuid) {
            cur = ps.tasks.value(targets.first().task).assignees; break;
          }
      } else {
        cur = m->shot(targets.first().row).tasks.value(targets.first().task).assignees;
      }
      if (pickAssignees(this, QObject::tr("selected tasks"), cur, newAssign)) {
        for (const Target &t : targets) {
          QStringList old;
          if (usePS) {
            for (const ProjectShot &ps : m->projectShots())
              if (ps.uuid == t.uuid) { old = ps.tasks.value(t.task).assignees; break; }
            if (old == newAssign) continue;
            m->setProjectShotAssigneesByUuid(t.uuid, t.task, newAssign);
            TUndoManager::manager()->add(
                new ProjectShotAssigneeUndo(t.uuid, t.task, old, newAssign));
          } else {
            old = m->shot(t.row).tasks.value(t.task).assignees;
            if (old == newAssign) continue;
            m->setShotTaskAssignees(t.row, t.task, newAssign);
            TUndoManager::manager()->add(
                new AssigneeEditUndo(m->shot(t.row).label(), t.task, old, newAssign));
          }
        }
      }
    } else {
      TaskStatus s = static_cast<TaskStatus>(chosen->data().toInt());
      for (const Target &t : targets) {
        if (usePS) {
          TaskStatus old = TaskStatus::Todo;
          for (const ProjectShot &ps : m->projectShots())
            if (ps.uuid == t.uuid) { old = ps.tasks.value(t.task).status; break; }
          if (old == s) continue;
          QVector<ZtoryTaskFlow::Change> fx;
          ZtoryTaskFlow::setStatus(ZtoryTaskFlow::Entity::Shot, t.uuid, t.task,
                                   s, ZtoryTaskFlow::Origin::User,
                                   /*batch=*/true, &fx);
          TUndoManager::manager()->add(
              new ProjectShotStatusUndo(t.uuid, t.task, old, s, fx));
        } else {
          TaskStatus old = m->shot(t.row).tasks.value(t.task).status;
          if (old == s) continue;
          m->setShotTaskStatus(t.row, t.task, s);
          TUndoManager::manager()->add(
              new StatusEditUndo(m->shot(t.row).label(), t.task, old, s));
        }
      }
    }
  }
  TUndoManager::manager()->endBlock();
  if (usePS) persistProjectDb(); else persistViaBoard();
  rebuild();
}

//-----------------------------------------------------------------------------

void ZtoryProductionPanel::editCell(int row, int col) {
  ZtoryModel *m = ZtoryModel::instance();
  const bool usePS = !m->projectShots().empty();
  const int kFixed = usePS ? 7 : 6;
  if (col < kFixed) return;
  const int taskIdx = col - kFixed;
  if (taskIdx < 0 || taskIdx >= m_taskCols.size()) return;
  const QString taskType = m_taskCols[taskIdx];

  if (usePS) {
    if (row < 0 || row >= (int)m->projectShots().size()) return;
    const ProjectShot &ps = m->projectShots()[row];
    if (!m->taskTypesForProjectShot(ps).contains(taskType)) return;
    const TaskState   cur       = ps.tasks.value(taskType);
    const TaskStatus  oldStatus = cur.status;
    const QStringList oldAssign = cur.assignees;
    const QString     uuid      = ps.uuid;
    TaskEditResult r = pickTaskEdit(this, taskType, oldStatus, oldAssign);
    if (r.kind == TaskEditResult::Status) {
      if (r.status == oldStatus) return;
      QVector<ZtoryTaskFlow::Change> fx;
      ZtoryTaskFlow::setStatus(ZtoryTaskFlow::Entity::Shot, uuid, taskType,
                               r.status, ZtoryTaskFlow::Origin::User,
                               /*batch=*/true, &fx);
      emit m->taskStatusChanged();
      persistProjectDb();
      TUndoManager::manager()->add(
          new ProjectShotStatusUndo(uuid, taskType, oldStatus, r.status, fx));
    } else if (r.kind == TaskEditResult::Assignees) {
      if (r.assignees == oldAssign) return;
      m->setProjectShotAssigneesByUuid(uuid, taskType, r.assignees);
      persistProjectDb();
      TUndoManager::manager()->add(
          new ProjectShotAssigneeUndo(uuid, taskType, oldAssign, r.assignees));
    }
  } else {
    if (row < 0 || row >= m->shotCount()) return;
    if (!m->taskTypesForShot(row).contains(taskType)) return;
    const TaskState   cur       = m->shot(row).tasks.value(taskType);
    const TaskStatus  oldStatus = cur.status;
    const QStringList oldAssign = cur.assignees;
    const QString     shotLabel = m->shot(row).label();
    TaskEditResult r = pickTaskEdit(this, taskType, oldStatus, oldAssign);
    if (r.kind == TaskEditResult::Status) {
      if (r.status == oldStatus) return;
      m->setShotTaskStatus(row, taskType, r.status);
      persistViaBoard();
      TUndoManager::manager()->add(
          new StatusEditUndo(shotLabel, taskType, oldStatus, r.status));
    } else if (r.kind == TaskEditResult::Assignees) {
      if (r.assignees == oldAssign) return;
      m->setShotTaskAssignees(row, taskType, r.assignees);
      persistViaBoard();
      TUndoManager::manager()->add(
          new AssigneeEditUndo(shotLabel, taskType, oldAssign, r.assignees));
    }
  }
  clearUntitledSceneDirty();
}

//=============================================================================
// Panel factory — auto-registers via the static instance below.  The panel
// type "ZtoryProductionPanel" must also be listed in menubar.xml (bundle + the
// user's ~/Library copy) to appear under the Panels menu.
//-----------------------------------------------------------------------------

class ZtoryProductionPanelFactory final : public TPanelFactory {
public:
  ZtoryProductionPanelFactory() : TPanelFactory("ZtoryProductionPanel") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryProductionPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Production Tracker"));
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryProductionPanelFactory;
