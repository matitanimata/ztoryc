#include "kitsuconnectdialog.h"

#include "kitsuclient.h"
#include "ztorymodel.h"  // ZtoryModel::taskStatusLabel

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QColor>

KitsuConnectDialog::KitsuConnectDialog(QWidget *parent)
    : QDialog(parent), m_client(new KitsuClient(this)) {
  setWindowTitle(tr("Connect to Kitsu"));
  setMinimumWidth(460);

  auto *root = new QVBoxLayout(this);

  // --- Connection form -------------------------------------------------
  auto *connBox  = new QGroupBox(tr("Server"), this);
  auto *form     = new QFormLayout(connBox);
  m_urlEdit      = new QLineEdit(connBox);
  m_urlEdit->setPlaceholderText("http://localhost:8012");
  m_emailEdit    = new QLineEdit(connBox);
  m_emailEdit->setPlaceholderText("you@studio.com");
  m_pwdEdit      = new QLineEdit(connBox);
  m_pwdEdit->setEchoMode(QLineEdit::Password);
  m_savePwd      = new QCheckBox(tr("Remember password on this machine"), connBox);
  form->addRow(tr("URL:"),      m_urlEdit);
  form->addRow(tr("Email:"),    m_emailEdit);
  form->addRow(tr("Password:"), m_pwdEdit);
  form->addRow(QString(),       m_savePwd);
  root->addWidget(connBox);

  // --- Connect button + status line ------------------------------------
  auto *row     = new QHBoxLayout();
  m_connectBtn  = new QPushButton(tr("Connect"), this);
  m_statusLabel = new QLabel(tr("Not connected."), this);
  m_statusLabel->setWordWrap(true);
  row->addWidget(m_connectBtn);
  row->addWidget(m_statusLabel, 1);
  root->addLayout(row);

  // --- Results: projects + task statuses -------------------------------
  auto *projRow = new QHBoxLayout();
  projRow->addWidget(new QLabel(tr("Project:"), this));
  m_projectCombo = new QComboBox(this);
  m_projectCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  projRow->addWidget(m_projectCombo, 1);
  m_linkBtn   = new QPushButton(tr("Link selected"), this);
  m_linkBtn->setToolTip(tr("Pull this project's metadata into Ztoryc and bind to it.\n"
                           "Production and Code become read-only (managed in Kitsu)."));
  m_createBtn = new QPushButton(tr("Create new in Kitsu"), this);
  m_createBtn->setToolTip(tr("Create a Kitsu project from the current Ztoryc project\n"
                             "fields and bind to it. Requires an admin/manager role."));
  projRow->addWidget(m_linkBtn);
  projRow->addWidget(m_createBtn);
  root->addLayout(projRow);
  m_linkBtn->setEnabled(false);
  m_createBtn->setEnabled(false);

  // Push-only shot sync (Ztoryc -> Kitsu); enabled once linked.
  m_pushShotsBtn = new QPushButton(tr("Push shots to Kitsu →"), this);
  m_pushShotsBtn->setToolTip(
      tr("Create/update the project's shots in Kitsu from the Ztoryc shot list.\n"
         "Shots are push-only: they are never pulled back from Kitsu."));
  m_pushShotsBtn->setEnabled(false);
  root->addWidget(m_pushShotsBtn);

  m_statusTable = new QTableWidget(0, 3, this);
  m_statusTable->setHorizontalHeaderLabels(
      {tr("Kitsu status"), tr("Color"), tr("Ztoryc status")});
  m_statusTable->horizontalHeader()->setStretchLastSection(true);
  m_statusTable->verticalHeader()->setVisible(false);
  m_statusTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_statusTable->setSelectionMode(QAbstractItemView::NoSelection);
  root->addWidget(new QLabel(tr("Task status mapping:"), this));
  root->addWidget(m_statusTable, 1);

  auto *closeBtn = new QPushButton(tr("Close"), this);
  auto *btnRow   = new QHBoxLayout();
  btnRow->addStretch(1);
  btnRow->addWidget(closeBtn);
  root->addLayout(btnRow);

  // --- Wiring ----------------------------------------------------------
  m_client->loadSettings();
  m_urlEdit->setText(m_client->baseUrl());
  m_emailEdit->setText(m_client->email());
  m_savePwd->setChecked(m_client->hasSavedPassword());
  if (m_client->hasSavedPassword()) m_pwdEdit->setText(QString());  // never echo

  connect(m_connectBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onConnectClicked);
  connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_linkBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onLinkClicked);
  connect(m_createBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onCreateClicked);
  connect(m_pushShotsBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onPushShotsClicked);

  connect(m_client, &KitsuClient::loginFinished, this,
          [this](bool ok, const QString &msg) {
            setBusy(false);
            m_statusLabel->setText(msg);
            m_statusLabel->setStyleSheet(ok ? "color:#22D160;"
                                            : "color:#FF3860;");
            updateBindingButtons();
          });
  connect(m_client, &KitsuClient::networkError, this,
          [this](const QString &msg) {
            setBusy(false);
            m_statusLabel->setText(msg);
            m_statusLabel->setStyleSheet("color:#FF3860;");
          });
  connect(m_client, &KitsuClient::projectsFetched, this,
          [this](const QVector<KitsuProject> &projects) {
            m_projectCombo->clear();
            for (const KitsuProject &p : projects)
              m_projectCombo->addItem(
                  QString("%1  (%2, %3 fps)")
                      .arg(p.name, p.resolution, p.fps),
                  p.id);
            updateBindingButtons();
          });
  connect(m_client, &KitsuClient::taskStatusesFetched, this,
          [this](const QVector<KitsuTaskStatus> &statuses) {
            m_statusTable->setRowCount(statuses.size());
            for (int i = 0; i < statuses.size(); ++i) {
              const KitsuTaskStatus &s = statuses[i];
              auto *nameItem = new QTableWidgetItem(s.name);
              auto *colorItem = new QTableWidgetItem(s.color);
              colorItem->setBackground(QColor(s.color));
              auto *mapItem = new QTableWidgetItem(
                  ZtoryModel::taskStatusLabel(KitsuClient::mapStatus(s)));
              m_statusTable->setItem(i, 0, nameItem);
              m_statusTable->setItem(i, 1, colorItem);
              m_statusTable->setItem(i, 2, mapItem);
            }
            m_statusTable->resizeColumnsToContents();
          });

  connect(m_client, &KitsuClient::projectCreated, this,
          [this](bool ok, const KitsuProject &p, const QString &msg) {
            setBusy(false);
            m_statusLabel->setStyleSheet(ok ? "color:#22D160;" : "color:#FF3860;");
            if (!ok) { m_statusLabel->setText(msg); return; }
            // Adopt the server's canonical values and bind.
            ZtoryModel *m = ZtoryModel::instance();
            m->setProduction(p.name);
            m->setCode(p.code);
            m->setKitsuProject(p.id, p.name);
            m->saveProjectDb();
            m_statusLabel->setText(tr("Created & linked: %1").arg(p.name));
            updateBindingButtons();     // push becomes available
            m_client->fetchProjects();  // refresh the dropdown with the new one
          });
  connect(m_client, &KitsuClient::projectUpdated, this,
          [this](bool ok, const QString &msg) {
            setBusy(false);
            m_statusLabel->setStyleSheet(ok ? "color:#22D160;" : "color:#FF3860;");
            m_statusLabel->setText(msg);
          });
  connect(m_client, &KitsuClient::shotsPushProgress, this,
          [this](const QString &msg) {
            m_statusLabel->setStyleSheet(QString());
            m_statusLabel->setText(msg);
          });
  connect(m_client, &KitsuClient::shotsPushed, this,
          [this](bool ok, int, int, const QString &msg) {
            // Chain into task + status push once the shots exist.
            if (ok && !m_pendingTasks.isEmpty()) {
              m_statusLabel->setText(msg + tr("  Pushing task statuses…"));
              m_client->pushTasks(ZtoryModel::instance()->kitsuProjectId(),
                                  m_pendingTasks);
              return;
            }
            setBusy(false);
            updateBindingButtons();
            m_statusLabel->setStyleSheet(ok ? "color:#22D160;" : "color:#FF3860;");
            m_statusLabel->setText(msg);
          });
  connect(m_client, &KitsuClient::tasksPushed, this,
          [this](bool ok, int, const QString &msg) {
            setBusy(false);
            updateBindingButtons();
            m_statusLabel->setStyleSheet(ok ? "color:#22D160;" : "color:#FF3860;");
            m_statusLabel->setText(msg);
          });
}

void KitsuConnectDialog::updateBindingButtons() {
  const bool connected = m_client->isLoggedIn();
  m_linkBtn->setEnabled(connected && m_projectCombo->count() > 0);
  const bool canManage = connected && m_client->canManageProjects();
  m_createBtn->setEnabled(canManage);
  if (connected && !canManage)
    m_createBtn->setToolTip(
        tr("Your Kitsu role (%1) can't create projects — ask an admin/manager.")
            .arg(m_client->userRole()));
  // Push needs a live session and a project already bound in the model.
  m_pushShotsBtn->setEnabled(connected &&
                             ZtoryModel::instance()->isKitsuLinked());
}

void KitsuConnectDialog::onLinkClicked() {
  const QString id = m_projectCombo->currentData().toString();
  if (id.isEmpty()) return;
  // Find the full project record from the last fetch.
  KitsuProject sel;
  for (const KitsuProject &p : m_client->projects())
    if (p.id == id) { sel = p; break; }
  if (sel.id.isEmpty()) return;

  ZtoryModel *m = ZtoryModel::instance();
  m->setProduction(sel.name);
  m->setCode(sel.code);
  if (!sel.fps.isEmpty()) m->setFps(sel.fps.toInt());
  m->setProductionType(sel.productionType);
  m->setProductionStyle(sel.productionStyle);
  m->setRatio(sel.ratio);
  m->setResolution(sel.resolution);
  m->setKitsuProject(sel.id, sel.name);
  m->saveProjectDb();

  m_statusLabel->setStyleSheet("color:#22D160;");
  m_statusLabel->setText(tr("Linked to %1.").arg(sel.name));
  updateBindingButtons();  // push becomes available now that we're linked
}

void KitsuConnectDialog::onCreateClicked() {
  ZtoryModel *m = ZtoryModel::instance();
  if (m->production().trimmed().isEmpty()) {
    m_statusLabel->setStyleSheet("color:#FF3860;");
    m_statusLabel->setText(tr("Set a Production name before creating the project."));
    return;
  }
  KitsuProject p;
  p.name            = m->production().trimmed();
  p.code            = m->code().trimmed();
  p.fps             = QString::number(m->fps());
  p.ratio           = m->ratio();
  p.resolution      = m->resolution();
  p.productionType  = m->productionType().isEmpty() ? "short" : m->productionType();
  p.productionStyle = m->productionStyle().isEmpty() ? "2d" : m->productionStyle();

  setBusy(true);
  m_statusLabel->setStyleSheet(QString());
  m_statusLabel->setText(tr("Creating project…"));
  m_client->createProject(p);
}

void KitsuConnectDialog::onPushShotsClicked() {
  ZtoryModel *m = ZtoryModel::instance();
  if (!m->isKitsuLinked()) return;
  m_pendingTasks.clear();

  // A shot in Kitsu must live under a sequence; default unsequenced shots to one.
  const QString kDefaultSeq = "SQ01";

  // Prefer the project-wide shot list (all storyboards); fall back to the open
  // scene's shots — mirroring what the Production Tracker actually displays.
  QVector<KitsuShotPush> shots;
  int skipped = 0;
  if (!m->projectShots().empty()) {
    for (const ProjectShot &ps : m->projectShots()) {
      if (ps.label.trimmed().isEmpty()) { ++skipped; continue; }
      KitsuShotPush s;
      s.seq      = ps.seq.trimmed().isEmpty() ? kDefaultSeq : ps.seq.trimmed();
      s.name     = ps.label.trimmed();
      s.nbFrames = ps.frames;
      s.frameIn  = 1;
      s.frameOut = ps.frames;
      shots.push_back(s);
      for (auto it = ps.tasks.constBegin(); it != ps.tasks.constEnd(); ++it) {
        KitsuTaskPush tp;
        tp.seq = s.seq; tp.shot = s.name;
        tp.taskType = it.key(); tp.status = it.value().status;
        m_pendingTasks.push_back(tp);
      }
    }
  } else {
    for (int i = 0; i < m->shotCount(); i++) {
      const ShotData &sd  = m->shot(i);
      const QString label = sd.label().trimmed();
      if (label.isEmpty()) { ++skipped; continue; }
      QString seq;  // parent sequence label, if the shot belongs to one
      if (!sd.sequenceId.isEmpty())
        for (const SequenceData &sq : m->sequences())
          if (sq.uuid == sd.sequenceId) { seq = sq.label.trimmed(); break; }
      KitsuShotPush s;
      s.seq      = seq.isEmpty() ? kDefaultSeq : seq;
      s.name     = label;
      s.nbFrames = sd.totalDuration();
      s.frameIn  = 1;
      s.frameOut = sd.totalDuration();
      shots.push_back(s);
      for (auto it = sd.tasks.constBegin(); it != sd.tasks.constEnd(); ++it) {
        KitsuTaskPush tp;
        tp.seq = s.seq; tp.shot = s.name;
        tp.taskType = it.key(); tp.status = it.value().status;
        m_pendingTasks.push_back(tp);
      }
    }
  }
  if (shots.isEmpty()) {
    m_statusLabel->setStyleSheet("color:#FF3860;");
    m_statusLabel->setText(tr("No shots to push."));
    return;
  }

  const bool tvshow = m->productionType() == "tvshow";
  setBusy(true);
  m_statusLabel->setStyleSheet(QString());
  m_statusLabel->setText(tr("Pushing %1 shots…%2")
                             .arg(shots.size())
                             .arg(skipped ? tr(" (%1 skipped)").arg(skipped) : QString()));
  m_client->pushShots(m->kitsuProjectId(), m->episode(), tvshow, shots);
}

void KitsuConnectDialog::onConnectClicked() {
  m_client->setBaseUrl(m_urlEdit->text());
  m_client->setEmail(m_emailEdit->text().trimmed());
  // Use the typed password; if blank and a password was saved, keep the saved
  // one (loadSettings already populated it in the client).
  if (!m_pwdEdit->text().isEmpty()) m_client->setPassword(m_pwdEdit->text());
  m_client->saveSettings(m_savePwd->isChecked());

  setBusy(true);
  m_statusLabel->setText(tr("Connecting…"));
  m_statusLabel->setStyleSheet(QString());
  m_client->connectAndSync();
}

void KitsuConnectDialog::setBusy(bool busy) {
  m_connectBtn->setEnabled(!busy);
}
