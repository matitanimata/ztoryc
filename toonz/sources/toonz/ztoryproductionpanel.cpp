#include "ztoryproductionpanel.h"

#include "ztorymodel.h"
#include "storyboardpanel.h"
#include "kitsuconnectdialog.h"

#include "toonzqt/gutil.h"

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

// Canonical Kitsu palette (matches the live Kitsu task_status colours):
// Todo grey, Ready amber, WIP blue, WFA purple, Retake red, Done green.
QColor statusColor(TaskStatus s) {
  switch (s) {
  case TaskStatus::Ready:  return QColor("#FBC02D");  // amber
  case TaskStatus::Wip:    return QColor("#3273DC");  // blue
  case TaskStatus::Wfa:    return QColor("#AB26FF");  // purple
  case TaskStatus::Retake: return QColor("#FF3860");  // red
  case TaskStatus::Done:   return QColor("#22D160");  // green
  case TaskStatus::Todo:
  default:                 return QColor("#9E9E9E");  // grey
  }
}

// Light statuses read better with black text.
bool isLightStatus(TaskStatus s) {
  return s == TaskStatus::Todo || s == TaskStatus::Ready;
}

const TaskStatus kAllStatuses[] = {TaskStatus::Todo,  TaskStatus::Ready,
                                   TaskStatus::Wip,   TaskStatus::Wfa,
                                   TaskStatus::Retake, TaskStatus::Done};

// Persist the .ztoryc through the Board (the in-app source of truth lives in
// the model; the Board owns the save path). No-op if the Board is closed —
// the model still holds the change, it just isn't written until next save.
StoryboardPanel *findBoard() {
  for (QWidget *w : QApplication::allWidgets())
    if (auto *b = qobject_cast<StoryboardPanel *>(w)) return b;
  return nullptr;
}

void persistViaBoard() {
  if (auto *b = findBoard()) b->saveZtoryc();
}

// Assets are project-level: they live in production.ztrack, not the .ztoryc.
void persistAssets() { ZtoryModel::instance()->saveProjectDb(); }

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
class ProjectShotStatusUndo final : public TUndo {
  QString    m_uuid, m_taskType;
  TaskStatus m_old, m_new;
public:
  ProjectShotStatusUndo(const QString &uuid, const QString &taskType,
                        TaskStatus o, TaskStatus n)
      : m_uuid(uuid), m_taskType(taskType), m_old(o), m_new(n) {}
  void undo() const override {
    ZtoryModel::instance()->setProjectShotTaskStatusByUuid(m_uuid, m_taskType, m_old);
    ZtoryModel::instance()->saveProjectDb();
  }
  void redo() const override {
    ZtoryModel::instance()->setProjectShotTaskStatusByUuid(m_uuid, m_taskType, m_new);
    ZtoryModel::instance()->saveProjectDb();
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
    ZtoryModel::instance()->saveProjectDb();
  }
  void redo() const override {
    ZtoryModel::instance()->setProjectShotAssigneesByUuid(m_uuid, m_taskType, m_new);
    ZtoryModel::instance()->saveProjectDb();
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Set %1 assignees").arg(m_taskType);
  }
};

// Undo for asset task edits — keyed by the asset's stable uuid.
class AssetStatusUndo final : public TUndo {
  QString m_uuid, m_taskType;
  TaskStatus m_old, m_new;
public:
  AssetStatusUndo(const QString &uuid, const QString &type, TaskStatus o, TaskStatus n)
      : m_uuid(uuid), m_taskType(type), m_old(o), m_new(n) {}
  void undo() const override {
    ZtoryModel::instance()->setAssetTaskStatusByUuid(m_uuid, m_taskType, m_old);
    persistAssets();
  }
  void redo() const override {
    ZtoryModel::instance()->setAssetTaskStatusByUuid(m_uuid, m_taskType, m_new);
    persistAssets();
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

  // TPanel (a TDockWidget) mounts its content via setWidget, not setLayout.
  setWidget(m_tabs);

  ZtoryModel *m = ZtoryModel::instance();
  connect(m, &ZtoryModel::modelReset,        this, &ZtoryProductionPanel::onModelChanged);
  connect(m, &ZtoryModel::shotAdded,         this, [this](int) { rebuild(); });
  connect(m, &ZtoryModel::shotRemoved,       this, [this](int) { rebuild(); });
  connect(m, &ZtoryModel::shotRemovedAt,     this, [this](int) { rebuild(); });
  connect(m, &ZtoryModel::shotMoved,         this, [this](int, int) { rebuild(); });
  connect(m, &ZtoryModel::shotDataChanged,   this, [this](int) { rebuild(); });
  connect(m, &ZtoryModel::taskStatusChanged, this, [this] { rebuild(); reloadProjectTab(); });
  connect(m, &ZtoryModel::assetsChanged,     this, [this] { rebuildAssets(); });
  connect(m, &ZtoryModel::productionReloaded, this, &ZtoryProductionPanel::onModelChanged);

  // --- Kitsu sync result handling (Project tab) ----------------------------
  // KitsuClient is the app-lifetime singleton; keep these connected so the
  // tracker's sync buttons report progress and apply results. The logic mirrors
  // the (now optional) Connect dialog; both are idempotent, and the dialog is
  // only ever open transiently, so there's no harmful double-handling.
  KitsuClient *kc = KitsuClient::instance();
  connect(kc, &KitsuClient::shotsPushProgress, this, [this](const QString &msg) {
    if (m_kitsuSyncLabel) { m_kitsuSyncLabel->setStyleSheet(QString()); m_kitsuSyncLabel->setText(msg); }
  });
  connect(kc, &KitsuClient::shotsPushed, this, [this](bool ok, int, int, const QString &msg) {
    if (ok && !m_kitsuPendingTasks.isEmpty()) {
      if (m_kitsuSyncLabel) m_kitsuSyncLabel->setText(msg + tr("  Pushing task statuses…"));
      KitsuClient::instance()->pushTasks(ZtoryModel::instance()->kitsuProjectId(),
                                         m_kitsuPendingTasks);
      m_kitsuPendingTasks.clear();
      return;
    }
    if (m_kitsuSyncLabel) {
      m_kitsuSyncLabel->setStyleSheet(ok ? "color:#22D160;" : "color:#FF3860;");
      m_kitsuSyncLabel->setText(msg);
    }
    updateKitsuButtons();
  });
  connect(kc, &KitsuClient::shotIdsResolved, this, [](const QHash<QString, QString> &byKey) {
    ZtoryModel *mm = ZtoryModel::instance();
    auto &pshots = mm->projectShots_rw();
    bool dirty = false;
    for (ProjectShot &ps : pshots) {
      const QString seq = ps.seq.trimmed().isEmpty() ? "SQ01" : ps.seq.trimmed();
      auto it = byKey.find(seq + "\n" + ps.label.trimmed());
      if (it != byKey.end() && ps.kitsuShotId != it.value()) { ps.kitsuShotId = it.value(); dirty = true; }
    }
    if (dirty) mm->saveProjectDb();
  });
  connect(kc, &KitsuClient::tasksPushed, this, [this](bool ok, int, const QString &msg) {
    if (m_kitsuSyncLabel) {
      m_kitsuSyncLabel->setStyleSheet(ok ? "color:#22D160;" : "color:#FF3860;");
      m_kitsuSyncLabel->setText(msg);
    }
  });
  connect(kc, &KitsuClient::previewsUploaded, this, [this](bool ok, int, const QString &msg) {
    if (m_kitsuSyncLabel) {
      m_kitsuSyncLabel->setStyleSheet(ok ? "color:#22D160;" : "color:#FF3860;");
      m_kitsuSyncLabel->setText(msg);
    }
  });
  connect(kc, &KitsuClient::statusesPulled, this,
          [this](bool ok, const QVector<KitsuPullEntry> &entries, const QString &msg) {
    if (!ok) {
      if (m_kitsuSyncLabel) { m_kitsuSyncLabel->setStyleSheet("color:#FF3860;"); m_kitsuSyncLabel->setText(msg); }
      return;
    }
    ZtoryModel *mm = ZtoryModel::instance();
    auto &pshots = mm->projectShots_rw();
    int updated = 0; bool dirty = false;
    for (const KitsuPullEntry &e : entries) {
      const QString ekey = KitsuClient::normalizeTaskType(e.taskType);
      for (ProjectShot &ps : pshots) {
        bool match;
        if (!ps.kitsuShotId.isEmpty() && !e.kitsuShotId.isEmpty())
          match = (ps.kitsuShotId == e.kitsuShotId);
        else {
          const QString psseq = ps.seq.trimmed().isEmpty() ? "SQ01" : ps.seq.trimmed();
          match = (ps.label.trimmed() == e.shot.trimmed() && psseq == e.seq.trimmed());
        }
        if (!match) continue;
        if (ps.kitsuShotId.isEmpty() && !e.kitsuShotId.isEmpty()) { ps.kitsuShotId = e.kitsuShotId; dirty = true; }
        for (const QString &tt : mm->taskTypesForProjectShot(ps))
          if (KitsuClient::normalizeTaskType(tt) == ekey) {
            if (ps.tasks[tt].status != e.status) { ps.tasks[tt].status = e.status; ++updated; dirty = true; }
            if (e.status == TaskStatus::Done) {
              const QString nxt = mm->nextTaskType(mm->techniqueForProjectShot(ps), tt);
              if (!nxt.isEmpty() && ps.tasks.value(nxt).status == TaskStatus::Todo) {
                ps.tasks[nxt].status = TaskStatus::Ready; dirty = true;
              }
            }
            break;
          }
      }
    }
    if (dirty) mm->saveAndNotifyTasks();
    if (m_kitsuSyncLabel) {
      m_kitsuSyncLabel->setStyleSheet("color:#22D160;");
      m_kitsuSyncLabel->setText(tr("%1 (%2 updated)").arg(msg).arg(updated));
    }
  });

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
}

//-----------------------------------------------------------------------------

void ZtoryProductionPanel::onModelChanged() {
  rebuild();
  reloadTeamTab();
  reloadProjectTab();
  rebuildAssets();
  reloadWorkflowsTab();
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

  // M5 — Kitsu integration, gated behind the project's opt-in flag: the whole
  // group is hidden unless the project enables Kitsu (chosen at creation).
  m_kitsuGroup = new QGroupBox(QObject::tr("Kitsu integration"), w);
  auto *kgl = new QVBoxLayout(m_kitsuGroup);
  m_kitsuLabel = new QLabel(QObject::tr("Not linked to Kitsu."), m_kitsuGroup);
  m_kitsuLabel->setWordWrap(true);
  kgl->addWidget(m_kitsuLabel);
  auto *kitsuBtn = new QPushButton(QObject::tr("Connect to Kitsu…"), m_kitsuGroup);
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

  m_kitsuPushBtn   = new QPushButton(QObject::tr("Push shots + statuses →"), m_kitsuGroup);
  m_kitsuPullBtn   = new QPushButton(QObject::tr("← Pull statuses"), m_kitsuGroup);
  m_kitsuUploadBtn = new QPushButton(QObject::tr("Upload shot previews →"), m_kitsuGroup);
  m_kitsuPushBtn->setToolTip(QObject::tr(
      "Create/update the project's shots + tasks in Kitsu from Ztoryc."));
  m_kitsuPullBtn->setToolTip(QObject::tr(
      "Pull task statuses down from Kitsu (the supervisor's WFA→Done/Retake)."));
  m_kitsuUploadBtn->setToolTip(QObject::tr(
      "Pick a folder of per-shot clips and upload each to its shot's task\n"
      "(matched by shot name + {TASK} code), setting it to WFA."));
  auto *syncRow = new QHBoxLayout();
  syncRow->addWidget(m_kitsuPushBtn);
  syncRow->addWidget(m_kitsuPullBtn);
  kgl->addLayout(syncRow);
  kgl->addWidget(m_kitsuUploadBtn);
  m_kitsuSyncLabel = new QLabel(QString(), m_kitsuGroup);
  m_kitsuSyncLabel->setWordWrap(true);
  kgl->addWidget(m_kitsuSyncLabel);
  form->addRow(m_kitsuGroup);

  connect(m_kitsuPushBtn,   &QPushButton::clicked, this, &ZtoryProductionPanel::onKitsuPush);
  connect(m_kitsuPullBtn,   &QPushButton::clicked, this, &ZtoryProductionPanel::onKitsuPull);
  connect(m_kitsuUploadBtn, &QPushButton::clicked, this, &ZtoryProductionPanel::onKitsuUpload);

  for (QLineEdit *e : {m_prodEdit, m_codeEdit, m_seasonEdit, m_titleEdit, m_epEdit, m_patternEdit})
    connect(e, &QLineEdit::editingFinished, this,
            [this] { applyProjectFromFields(); });
  connect(m_techCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int) { applyProjectFromFields(); });
  return w;
}

void ZtoryProductionPanel::reloadProjectTab() {
  if (!m_prodEdit) return;
  m_projLoading = true;
  ZtoryModel *m = ZtoryModel::instance();
  m_prodEdit->setText(m->production());
  if (m_codeEdit) m_codeEdit->setText(m->code());
  m_seasonEdit->setText(m->season());
  m_titleEdit->setText(m->title());
  m_epEdit->setText(m->episode());
  m_techCombo->clear();
  for (const Technique &t : m->techniques()) m_techCombo->addItem(t.name);
  int di = m_techCombo->findText(m->defaultTechnique());
  if (di >= 0) m_techCombo->setCurrentIndex(di);
  if (m_patternEdit) m_patternEdit->setText(m->namingPattern());

  // M5 — when linked to Kitsu, that instance owns the project metadata: mirror
  // it and make the synced fields read-only ("managed in Kitsu").
  const bool linked = m->isKitsuLinked();
  if (m_kitsuLabel) {
    if (linked) {
      QString info = tr("🔗 Linked: %1").arg(m->kitsuProjectName());
      if (!m->productionType().isEmpty())
        info += "  ·  " + m->productionType();
      if (!m->resolution().isEmpty())
        info += "  ·  " + m->resolution() + " @ " + QString::number(m->fps()) + "fps";
      m_kitsuLabel->setText(info);
      m_kitsuLabel->setStyleSheet("color:#22D160;");
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
  const bool linked = ZtoryModel::instance()->isKitsuLinked();
  if (m_kitsuPushBtn)   m_kitsuPushBtn->setEnabled(linked);
  if (m_kitsuPullBtn)   m_kitsuPullBtn->setEnabled(linked);
  if (m_kitsuUploadBtn) m_kitsuUploadBtn->setEnabled(linked);
  if (m_kitsuHandlesCheck) m_kitsuHandlesCheck->setEnabled(linked);
  if (m_kitsuHandlesSpin)  m_kitsuHandlesSpin->setEnabled(linked);
}

void ZtoryProductionPanel::onKitsuPush() {
  ZtoryModel *m = ZtoryModel::instance();
  if (!m->isKitsuLinked()) return;
  const int handles =
      m_kitsuHandlesCheck->isChecked() ? m_kitsuHandlesSpin->value() : 0;
  int skipped = 0;
  QVector<KitsuShotPush> shots =
      KitsuClient::buildShotPushFromProject(handles, m_kitsuPendingTasks, skipped);
  if (shots.isEmpty()) {
    m_kitsuSyncLabel->setStyleSheet("color:#FF3860;");
    m_kitsuSyncLabel->setText(tr("No shots to push."));
    return;
  }
  const bool tvshow = m->productionType() == "tvshow";
  m_kitsuSyncLabel->setStyleSheet(QString());
  m_kitsuSyncLabel->setText(tr("Pushing %1 shots…").arg(shots.size()));
  KitsuClient::instance()->pushShots(m->kitsuProjectId(), m->episode(), tvshow, shots);
}

void ZtoryProductionPanel::onKitsuPull() {
  ZtoryModel *m = ZtoryModel::instance();
  if (!m->isKitsuLinked()) return;
  m_kitsuSyncLabel->setStyleSheet(QString());
  m_kitsuSyncLabel->setText(tr("Pulling statuses from Kitsu…"));
  KitsuClient::instance()->pullStatuses(m->kitsuProjectId());
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
  // Optimistic local WFA mirror (the upload sets WFA on Kitsu too).
  auto &pshots = m->projectShots_rw();
  bool dirty = false;
  for (const KitsuPreviewUpload &u : uploads)
    for (ProjectShot &ps : pshots)
      if (ps.uuid == u.uuid) {
        if (ps.tasks[u.taskType].status != TaskStatus::Wfa) {
          ps.tasks[u.taskType].status = TaskStatus::Wfa;
          dirty = true;
        }
        break;
      }
  if (dirty) m->saveAndNotifyTasks();
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
  m->setEpisode(m_epEdit->text().trimmed());
  if (!m_techCombo->currentText().isEmpty())
    m->setDefaultTechnique(m_techCombo->currentText());
  if (m_patternEdit && !m_patternEdit->text().trimmed().isEmpty())
    m->setNamingPattern(m_patternEdit->text().trimmed());
  m->saveProjectDb();  // project-meta lives in the project DB
  rebuild();  // default-technique change may alter which task columns apply
}

//-----------------------------------------------------------------------------
// Assets tab — project-level asset list with its own task pipeline.

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
    ZtoryModel::instance()->addAsset("Character", QObject::tr("New asset"));
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

void ZtoryProductionPanel::rebuildAssets() {
  if (!m_assetTable) return;
  ZtoryModel *m   = ZtoryModel::instance();
  m_assetTaskCols = ZtoryModel::canonicalAssetTaskOrder();
  m_assetLoading  = true;
  m_assetTable->clear();
  const int kFixed = 2;  // Type, Name
  m_assetTable->setColumnCount(kFixed + m_assetTaskCols.size());
  m_assetTable->setRowCount(m->assetCount());
  QStringList headers;
  headers << QObject::tr("Type") << QObject::tr("Name");
  headers += m_assetTaskCols;
  m_assetTable->setHorizontalHeaderLabels(headers);

  for (int i = 0; i < m->assetCount(); i++) {
    const Asset &as = m->assets()[i];
    auto *typeItem  = new QTableWidgetItem(as.type);
    typeItem->setFlags(Qt::ItemIsEnabled);  // edited via click menu
    typeItem->setTextAlignment(Qt::AlignCenter);
    m_assetTable->setItem(i, 0, typeItem);
    auto *nameItem = new QTableWidgetItem(as.name);
    nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable);
    m_assetTable->setItem(i, 1, nameItem);
    for (int c = 0; c < m_assetTaskCols.size(); c++) {
      const TaskState ts = as.tasks.value(m_assetTaskCols[c]);
      auto *it = new QTableWidgetItem();
      it->setTextAlignment(Qt::AlignCenter);
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
    const char *types[] = {"Character", "Prop", "Environment", "BG", "FX"};
    for (const char *t : types) menu.addAction(QString::fromLatin1(t));
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
        m->setAssetTaskStatusByUuid(t.uuid, t.task, s);
        TUndoManager::manager()->add(new AssetStatusUndo(t.uuid, t.task, old, s));
      }
    }
  }
  TUndoManager::manager()->endBlock();
  persistAssets();
  rebuildAssets();
}

// Tracker edits change production.ztrack, not the .tnz scene. In standalone mode
// (the Production room opened without a scene) the undo registration would
// otherwise leave the empty untitled scene flagged "modified" (Untitled*). Clear
// that — a titled scene is left untouched.
static void clearUntitledSceneDirty() {
  TApp *app = TApp::instance();
  if (app->getCurrentScene() && app->getCurrentScene()->getScene() &&
      app->getCurrentScene()->getScene()->isUntitled())
    app->getCurrentScene()->setDirtyFlag(false);
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
    m->setAssetTaskStatus(row, taskType, r.status);
    persistAssets();
    TUndoManager::manager()->add(
        new AssetStatusUndo(uuid, taskType, oldStatus, r.status));
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
      m->saveProjectDb();
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
          m->setProjectShotTaskStatusByUuid(t.uuid, t.task, s);
          TUndoManager::manager()->add(
              new ProjectShotStatusUndo(t.uuid, t.task, old, s));
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
  if (usePS) m->saveProjectDb(); else persistViaBoard();
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
      m->setProjectShotTaskStatusByUuid(uuid, taskType, r.status);
      m->saveProjectDb();
      TUndoManager::manager()->add(
          new ProjectShotStatusUndo(uuid, taskType, oldStatus, r.status));
    } else if (r.kind == TaskEditResult::Assignees) {
      if (r.assignees == oldAssign) return;
      m->setProjectShotAssigneesByUuid(uuid, taskType, r.assignees);
      m->saveProjectDb();
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
