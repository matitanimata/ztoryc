#include "ztoryproductionpanel.h"

#include "ztorymodel.h"
#include "storyboardpanel.h"

#include "tundo.h"

#include <QVBoxLayout>
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
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QComboBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFormLayout>

#include <cassert>

namespace {

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
void persistViaBoard() {
  for (QWidget *w : QApplication::allWidgets())
    if (auto *board = qobject_cast<StoryboardPanel *>(w)) {
      board->saveZtoryc();
      return;
    }
}

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

}  // namespace

//-----------------------------------------------------------------------------

ZtoryProductionPanel::ZtoryProductionPanel(QWidget *parent) : TPanel(parent) {
  m_tabs = new QTabWidget(this);
  m_tabs->setDocumentMode(true);
  m_tabs->addTab(buildShotsTab(),   QObject::tr("Shots"));
  m_tabs->addTab(buildTeamTab(),    QObject::tr("Team"));
  m_tabs->addTab(buildProjectTab(), QObject::tr("Project"));
  // Placeholders for the next increments (kept visible so the structure reads).
  auto stub = [this](const QString &t) {
    QLabel *l = new QLabel(t, this);
    l->setAlignment(Qt::AlignCenter);
    l->setEnabled(false);
    return l;
  };
  m_tabs->addTab(stub(QObject::tr("Assets — coming soon")),    QObject::tr("Assets"));
  m_tabs->addTab(stub(QObject::tr("Workflows — coming soon")), QObject::tr("Workflows"));

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

  rebuild();
  reloadTeamTab();
  reloadProjectTab();
}

//-----------------------------------------------------------------------------

void ZtoryProductionPanel::onModelChanged() {
  rebuild();
  reloadTeamTab();
  reloadProjectTab();
}

//-----------------------------------------------------------------------------

QWidget *ZtoryProductionPanel::buildShotsTab() {
  QWidget *w = new QWidget(this);
  m_table    = new QTableWidget(w);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionMode(QAbstractItemView::NoSelection);
  m_table->verticalHeader()->setVisible(false);
  m_table->setShowGrid(true);
  m_table->setAlternatingRowColors(false);
  connect(m_table, &QTableWidget::cellClicked, this,
          &ZtoryProductionPanel::onCellClicked);
  auto *lay = new QVBoxLayout(w);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->addWidget(m_table);
  return w;
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
  persistViaBoard();
}

//-----------------------------------------------------------------------------
// Project tab — production metadata + default technique (pipeline-level, lives
// here rather than in Storyboard Settings).

QWidget *ZtoryProductionPanel::buildProjectTab() {
  QWidget *w  = new QWidget(this);
  auto *form  = new QFormLayout(w);
  m_prodEdit  = new QLineEdit(w);
  m_titleEdit = new QLineEdit(w);
  m_epEdit    = new QLineEdit(w);
  m_techCombo = new QComboBox(w);
  form->addRow(QObject::tr("Production:"),        m_prodEdit);
  form->addRow(QObject::tr("Title:"),             m_titleEdit);
  form->addRow(QObject::tr("Episode:"),           m_epEdit);
  form->addRow(QObject::tr("Default technique:"), m_techCombo);

  for (QLineEdit *e : {m_prodEdit, m_titleEdit, m_epEdit})
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
  m_titleEdit->setText(m->title());
  m_epEdit->setText(m->episode());
  m_techCombo->clear();
  for (const Technique &t : m->techniques()) m_techCombo->addItem(t.name);
  int di = m_techCombo->findText(m->defaultTechnique());
  if (di >= 0) m_techCombo->setCurrentIndex(di);
  m_projLoading = false;
}

void ZtoryProductionPanel::applyProjectFromFields() {
  if (m_projLoading || !m_prodEdit) return;
  ZtoryModel *m = ZtoryModel::instance();
  m->setProduction(m_prodEdit->text().trimmed());
  m->setTitle(m_titleEdit->text().trimmed());
  m->setEpisode(m_epEdit->text().trimmed());
  if (!m_techCombo->currentText().isEmpty())
    m->setDefaultTechnique(m_techCombo->currentText());
  persistViaBoard();
  rebuild();  // default-technique change may alter which task columns apply
}

//-----------------------------------------------------------------------------

void ZtoryProductionPanel::rebuild() {
  ZtoryModel *m = ZtoryModel::instance();

  m_taskCols = m->spreadsheetTaskColumns();
  const QStringList &taskCols = m_taskCols;
  const int kFixed = 1;  // "Shot" column

  m_table->clear();
  m_table->setColumnCount(kFixed + taskCols.size());
  m_table->setRowCount(m->shotCount());

  QStringList headers;
  headers << QObject::tr("Shot");
  headers += taskCols;
  m_table->setHorizontalHeaderLabels(headers);

  for (int i = 0; i < m->shotCount(); i++) {
    const ShotData &sd = m->shot(i);

    auto *shotItem = new QTableWidgetItem(m->fullLabel(i));
    shotItem->setFlags(Qt::ItemIsEnabled);
    QFont f = shotItem->font();
    f.setBold(true);
    shotItem->setFont(f);
    m_table->setItem(i, 0, shotItem);

    const QStringList shotTasks = m->taskTypesForShot(i);
    for (int c = 0; c < taskCols.size(); c++) {
      const QString &tt = taskCols[c];
      auto *it = new QTableWidgetItem();
      it->setTextAlignment(Qt::AlignCenter);
      it->setFlags(Qt::ItemIsEnabled);

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
  }

  m_table->resizeColumnsToContents();
  m_table->resizeRowsToContents();
}

//-----------------------------------------------------------------------------

void ZtoryProductionPanel::onCellClicked(int row, int col) {
  editCell(row, col);
}

//-----------------------------------------------------------------------------

void ZtoryProductionPanel::editCell(int row, int col) {
  const int kFixed = 1;
  if (col < kFixed) return;  // "Shot" label column, not editable
  const int taskIdx = col - kFixed;
  if (taskIdx < 0 || taskIdx >= m_taskCols.size()) return;

  ZtoryModel *m = ZtoryModel::instance();
  if (row < 0 || row >= m->shotCount()) return;
  const QString taskType = m_taskCols[taskIdx];

  // Only applicable tasks (part of the shot's technique) are editable.
  if (!m->taskTypesForShot(row).contains(taskType)) return;

  const TaskState   cur        = m->shot(row).tasks.value(taskType);
  const TaskStatus  oldStatus  = cur.status;
  const QStringList oldAssign  = cur.assignees;
  const QString     shotLabel  = m->shot(row).label();

  // Status picker: a swatch per status, plus an "Assignees…" entry.
  QMenu menu(this);
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
  if (!chosen) return;

  if (chosen == assignAct) {
    const QStringList team = m->team();
    QStringList newAssign;

    if (team.isEmpty()) {
      // No project team defined yet → free-text fallback (define a Team in
      // Storyboard Settings to get a pick list).
      bool ok = false;
      QString text = QInputDialog::getText(
          this, QObject::tr("Assignees"),
          QObject::tr("People assigned to %1 (comma-separated):").arg(taskType),
          QLineEdit::Normal, oldAssign.join(", "), &ok);
      if (!ok) return;
      newAssign = parseAssignees(text);
    } else {
      // Pick from the project team (checkable) + free text for anyone else.
      QDialog dlg(this);
      dlg.setWindowTitle(QObject::tr("Assignees — %1").arg(taskType));
      QVBoxLayout *lay = new QVBoxLayout(&dlg);
      lay->addWidget(new QLabel(QObject::tr("Assign to:"), &dlg));
      QListWidget *list = new QListWidget(&dlg);
      for (const QString &p : team) {
        auto *it = new QListWidgetItem(p, list);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(oldAssign.contains(p) ? Qt::Checked : Qt::Unchecked);
      }
      lay->addWidget(list);
      QStringList extras;
      for (const QString &a : oldAssign)
        if (!team.contains(a)) extras << a;
      lay->addWidget(new QLabel(QObject::tr("Others (comma-separated):"), &dlg));
      QLineEdit *extraEdit = new QLineEdit(extras.join(", "), &dlg);
      lay->addWidget(extraEdit);
      auto *bb = new QDialogButtonBox(
          QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
      QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
      QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
      lay->addWidget(bb);
      if (dlg.exec() != QDialog::Accepted) return;
      for (int r = 0; r < list->count(); r++)
        if (list->item(r)->checkState() == Qt::Checked)
          newAssign << list->item(r)->text();
      newAssign += parseAssignees(extraEdit->text());
    }

    if (newAssign == oldAssign) return;
    m->setShotTaskAssignees(row, taskType, newAssign);
    persistViaBoard();
    TUndoManager::manager()->add(
        new AssigneeEditUndo(shotLabel, taskType, oldAssign, newAssign));
    return;
  }

  const TaskStatus chosenStatus = static_cast<TaskStatus>(chosen->data().toInt());
  if (chosenStatus == oldStatus) return;
  m->setShotTaskStatus(row, taskType, chosenStatus);  // emits taskStatusChanged → rebuild
  persistViaBoard();
  TUndoManager::manager()->add(
      new StatusEditUndo(shotLabel, taskType, oldStatus, chosenStatus));
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
