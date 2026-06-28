#include "kitsuconnectdialog.h"

#include "kitsuclient.h"
#include "ztorymodel.h"  // ZtoryModel::taskStatusLabel

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QColor>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>

KitsuConnectDialog::KitsuConnectDialog(QWidget *parent)
    : QDialog(parent), m_client(KitsuClient::instance()) {
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

  // Optional safety "handles" padded onto the pushed frame ranges (kept out of
  // Ztoryc's internal board timing).
  auto *handlesRow = new QHBoxLayout();
  m_handlesCheck = new QCheckBox(tr("Push with handles"), this);
  m_handlesCheck->setToolTip(
      tr("Pad each shot's frame_in/frame_out in Kitsu by N frames of safety\n"
         "margin (for layout/BG), leaving Ztoryc's board timing unchanged."));
  m_handlesSpin = new QSpinBox(this);
  m_handlesSpin->setRange(0, 240);
  m_handlesSpin->setValue(12);
  m_handlesSpin->setSuffix(tr(" fr"));
  handlesRow->addWidget(m_handlesCheck);
  handlesRow->addWidget(m_handlesSpin);
  handlesRow->addStretch(1);
  root->addLayout(handlesRow);

  // Shot push (Ztoryc → Kitsu) + status pull (Kitsu → Ztoryc); enabled once linked.
  m_pushShotsBtn = new QPushButton(tr("Push shots to Kitsu →"), this);
  m_pushShotsBtn->setToolTip(
      tr("Create/update the project's shots + tasks in Kitsu from Ztoryc.\n"
         "Shots are push-only: they are never pulled back from Kitsu."));
  m_pushShotsBtn->setEnabled(false);
  m_pullStatusBtn = new QPushButton(tr("← Pull statuses from Kitsu"), this);
  m_pullStatusBtn->setToolTip(
      tr("Pull task statuses down from Kitsu (review sync): the supervisor's\n"
         "WFA → Done/Retake on Kitsu appears back in the Production Tracker."));
  m_pullStatusBtn->setEnabled(false);
  auto *syncRow = new QHBoxLayout();
  syncRow->addWidget(m_pushShotsBtn);
  syncRow->addWidget(m_pullStatusBtn);
  root->addLayout(syncRow);

  // Preview upload (Ztoryc → Kitsu): pick a folder of per-shot clips (exported
  // with "one clip per shot"), attach each to its shot's Storyboard task and
  // set it to WFA. Enabled once linked.
  m_uploadPrevBtn = new QPushButton(tr("Upload shot previews to Kitsu →"), this);
  m_uploadPrevBtn->setToolTip(
      tr("Pick a folder of per-shot clips (exported with \"one clip per shot\").\n"
         "Each clip is attached to its shot's Storyboard task and set to WFA.\n"
         "Clips are matched to shots by the shot name in the file name."));
  m_uploadPrevBtn->setEnabled(false);
  root->addWidget(m_uploadPrevBtn);

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
  // The client is an app-lifetime singleton (already loaded its settings), so the
  // session and the saved URL/email/password survive across dialog opens.
  m_urlEdit->setText(m_client->baseUrl());
  m_emailEdit->setText(m_client->email());
  // Default "remember" to ON for convenience (typically a local/studio instance).
  m_savePwd->setChecked(true);
  if (m_client->hasSavedPassword())
    m_pwdEdit->setPlaceholderText(tr("•••••• (saved)"));  // signal it's remembered

  connect(m_connectBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onConnectClicked);
  connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_linkBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onLinkClicked);
  connect(m_createBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onCreateClicked);
  connect(m_pushShotsBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onPushShotsClicked);
  connect(m_pullStatusBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onPullStatusClicked);
  connect(m_uploadPrevBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onUploadPreviewsClicked);

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
            // Target the production this Ztoryc project is already bound to.
            const QString boundId = ZtoryModel::instance()->kitsuProjectId();
            if (!boundId.isEmpty()) {
              int idx = m_projectCombo->findData(boundId);
              if (idx >= 0) m_projectCombo->setCurrentIndex(idx);
            }
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
  connect(m_client, &KitsuClient::shotIdsResolved, this,
          [](const QHash<QString, QString> &byKey) {
            ZtoryModel *m = ZtoryModel::instance();
            auto &pshots  = m->projectShots_rw();
            bool dirty    = false;
            for (ProjectShot &ps : pshots) {
              const QString seq =
                  ps.seq.trimmed().isEmpty() ? "SQ01" : ps.seq.trimmed();
              auto it = byKey.find(seq + "\n" + ps.label.trimmed());
              if (it != byKey.end() && ps.kitsuShotId != it.value()) {
                ps.kitsuShotId = it.value();
                dirty = true;
              }
            }
            if (dirty) m->saveProjectDb();
          });
  connect(m_client, &KitsuClient::tasksPushed, this,
          [this](bool ok, int, const QString &msg) {
            setBusy(false);
            updateBindingButtons();
            m_statusLabel->setStyleSheet(ok ? "color:#22D160;" : "color:#FF3860;");
            m_statusLabel->setText(msg);
          });
  connect(m_client, &KitsuClient::previewsUploaded, this,
          [this](bool ok, int, const QString &msg) {
            setBusy(false);
            // Mirror the WFA the upload set on Kitsu into the local tracker
            // (only on success; a later pull would also bring it back).
            if (ok && !m_pendingPreviewStatus.isEmpty()) {
              ZtoryModel *m = ZtoryModel::instance();
              auto &pshots  = m->projectShots_rw();
              bool dirty    = false;
              for (const auto &pr : m_pendingPreviewStatus)
                for (ProjectShot &ps : pshots)
                  if (ps.uuid == pr.first) {
                    if (ps.tasks[pr.second].status != TaskStatus::Wfa) {
                      ps.tasks[pr.second].status = TaskStatus::Wfa;
                      dirty = true;
                    }
                    break;
                  }
              if (dirty) m->saveAndNotifyTasks();
            }
            m_pendingPreviewStatus.clear();
            updateBindingButtons();
            m_statusLabel->setStyleSheet(ok ? "color:#22D160;" : "color:#FF3860;");
            m_statusLabel->setText(msg);
          });
  connect(m_client, &KitsuClient::statusesPulled, this,
          [this](bool ok, const QVector<KitsuPullEntry> &entries, const QString &msg) {
            setBusy(false);
            updateBindingButtons();
            if (!ok) {
              m_statusLabel->setStyleSheet("color:#FF3860;");
              m_statusLabel->setText(msg);
              return;
            }
            // Apply pulled statuses (Kitsu authoritative on review).
            ZtoryModel *m = ZtoryModel::instance();
            auto &pshots  = m->projectShots_rw();
            int updated   = 0;
            bool dirty    = false;
            for (const KitsuPullEntry &e : entries) {
              const QString ekey = KitsuClient::normalizeTaskType(e.taskType);
              for (ProjectShot &ps : pshots) {
                // Prefer the stored Kitsu shot id (survives renames on either
                // side); fall back to seq+label until the link is recorded.
                bool match;
                if (!ps.kitsuShotId.isEmpty() && !e.kitsuShotId.isEmpty())
                  match = (ps.kitsuShotId == e.kitsuShotId);
                else {
                  const QString psseq =
                      ps.seq.trimmed().isEmpty() ? "SQ01" : ps.seq.trimmed();
                  match = (ps.label.trimmed() == e.shot.trimmed() &&
                           psseq == e.seq.trimmed());
                }
                if (!match) continue;
                // Record the Kitsu id so future syncs are rename-proof.
                if (ps.kitsuShotId.isEmpty() && !e.kitsuShotId.isEmpty()) {
                  ps.kitsuShotId = e.kitsuShotId;
                  dirty = true;
                }
                for (const QString &tt : m->taskTypesForProjectShot(ps))
                  if (KitsuClient::normalizeTaskType(tt) == ekey) {
                    if (ps.tasks[tt].status != e.status) {
                      ps.tasks[tt].status = e.status;
                      ++updated;
                      dirty = true;
                    }
                    // Approval cascade: a task going Done unblocks the next one
                    // (Todo→Ready), so the pipeline advances itself on review.
                    if (e.status == TaskStatus::Done) {
                      const QString nxt = m->nextTaskType(
                          m->techniqueForProjectShot(ps), tt);
                      if (!nxt.isEmpty() &&
                          ps.tasks.value(nxt).status == TaskStatus::Todo) {
                        ps.tasks[nxt].status = TaskStatus::Ready;
                        dirty = true;
                      }
                    }
                    break;
                  }
              }
            }
            if (dirty) m->saveAndNotifyTasks();
            m_statusLabel->setStyleSheet("color:#22D160;");
            m_statusLabel->setText(
                tr("%1 (%2 updated in Ztoryc)").arg(msg).arg(updated));
          });

  // Reflect an existing session (the singleton stays logged in across opens), or
  // auto-connect when we already have saved credentials — so the user doesn't
  // have to log in again every time.
  if (m_client->isLoggedIn()) {
    m_statusLabel->setStyleSheet("color:#22D160;");
    m_statusLabel->setText(tr("Connected as %1").arg(m_client->email()));
    m_client->fetchProjects();      // repopulates the dropdown (+ bound project)
    m_client->fetchTaskStatuses();  // repopulates the status table
  } else if (!m_client->email().isEmpty() && m_client->hasSavedPassword()) {
    onConnectClicked();
  }
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
  // Push/pull need a live session and a project already bound in the model.
  const bool linked = connected && ZtoryModel::instance()->isKitsuLinked();
  m_pushShotsBtn->setEnabled(linked);
  m_pullStatusBtn->setEnabled(linked);
  m_uploadPrevBtn->setEnabled(linked);
}

void KitsuConnectDialog::onPullStatusClicked() {
  ZtoryModel *m = ZtoryModel::instance();
  if (!m->isKitsuLinked()) return;
  setBusy(true);
  m_statusLabel->setStyleSheet(QString());
  m_statusLabel->setText(tr("Pulling statuses from Kitsu…"));
  m_client->pullStatuses(m->kitsuProjectId());
}

void KitsuConnectDialog::onUploadPreviewsClicked() {
  ZtoryModel *m = ZtoryModel::instance();
  if (!m->isKitsuLinked()) return;
  m_pendingPreviewStatus.clear();

  const auto &pshots = m->projectShots();
  if (pshots.empty()) {
    m_statusLabel->setStyleSheet("color:#FF3860;");
    m_statusLabel->setText(tr("No project shots — push shots to Kitsu first."));
    return;
  }

  const QString dir = QFileDialog::getExistingDirectory(
      this, tr("Folder with per-shot clips"));
  if (dir.isEmpty()) return;

  // Candidate movie files in the folder.
  static const QStringList kExts =
      {"mp4", "mov", "avi", "webm", "gif", "mkv", "m4v"};
  QStringList filters;
  for (const QString &e : kExts) filters << ("*." + e);
  const QFileInfoList files =
      QDir(dir).entryInfoList(filters, QDir::Files, QDir::Name);
  if (files.isEmpty()) {
    m_statusLabel->setStyleSheet("color:#FF3860;");
    m_statusLabel->setText(tr("No movie clips found in that folder."));
    return;
  }

  // Match each file to the shot whose label is the LONGEST one contained in the
  // file name — so "scene_SH010.mp4" picks SH010, not SH01.  The clip names are
  // exported from label(), which is also the Kitsu shot name, so this lines up.
  QVector<KitsuPreviewUpload> uploads;
  int unmatched = 0, noId = 0;
  for (const QFileInfo &fi : files) {
    const QString base       = fi.completeBaseName();
    const ProjectShot *match = nullptr;
    int bestLen              = 0;
    for (const ProjectShot &ps : pshots) {
      const QString label = ps.label.trimmed();
      if (label.isEmpty()) continue;
      if (base.contains(label, Qt::CaseInsensitive) &&
          label.length() > bestLen) {
        match   = &ps;
        bestLen = label.length();
      }
    }
    if (!match) { ++unmatched; continue; }
    if (match->kitsuShotId.isEmpty()) { ++noId; continue; }
    // Detect which task the clip belongs to from its name, using the short codes
    // ({TASK} token in the naming pattern) of THIS shot's technique only — so a
    // "..._LAY_..." render lands on Layout and "..._ANIM_..." on Animation.
    // Board/animatic previews carry no task code and default to Storyboard.
    QString task    = "Storyboard";
    int     codeLen = 0;
    for (const QString &tt : m->taskTypesForProjectShot(*match)) {
      const QString code = ZtoryModel::taskShortCode(tt);
      if (!code.isEmpty() && base.contains(code, Qt::CaseInsensitive) &&
          code.length() > codeLen) {
        task    = tt;
        codeLen = code.length();
      }
    }
    KitsuPreviewUpload u;
    u.kitsuShotId = match->kitsuShotId;
    u.shot        = match->label.trimmed();
    u.taskType    = task;
    u.filePath    = fi.absoluteFilePath();
    u.status      = TaskStatus::Wfa;
    uploads.push_back(u);
    m_pendingPreviewStatus.push_back(qMakePair(match->uuid, u.taskType));
  }

  if (uploads.isEmpty()) {
    m_statusLabel->setStyleSheet("color:#FF3860;");
    m_statusLabel->setText(
        noId ? tr("Matched shots have no Kitsu id — push shots to Kitsu first.")
             : tr("No clips matched a shot name."));
    return;
  }

  setBusy(true);
  m_statusLabel->setStyleSheet(QString());
  // Surface skipped clips so a not-yet-pushed shot (no Kitsu id) is visible
  // rather than silently dropped — the user knows to push shots again.
  QStringList skips;
  if (unmatched) skips << tr("%1 unmatched").arg(unmatched);
  if (noId)      skips << tr("%1 not on Kitsu yet").arg(noId);
  m_statusLabel->setText(
      tr("Uploading %1 previews…%2")
          .arg(uploads.size())
          .arg(skips.isEmpty() ? QString()
                               : QString(" (") + skips.join(", ") + ")"));
  m_client->uploadPreviews(m->kitsuProjectId(), uploads);
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
  // Optional safety handles padded onto the pushed ranges (timeline unchanged).
  const int handles = m_handlesCheck->isChecked() ? m_handlesSpin->value() : 0;

  // Prefer the project-wide shot list (all storyboards); fall back to the open
  // scene's shots — mirroring what the Production Tracker actually displays.
  QVector<KitsuShotPush> shots;
  int skipped = 0;
  if (!m->projectShots().empty()) {
    const auto frameRanges = m->projectShotFrameRanges();  // cumulative in/out
    const auto &pshots = m->projectShots();
    for (size_t i = 0; i < pshots.size(); i++) {
      const ProjectShot &ps = pshots[i];
      if (ps.label.trimmed().isEmpty()) { ++skipped; continue; }
      KitsuShotPush s;
      s.seq      = ps.seq.trimmed().isEmpty() ? kDefaultSeq : ps.seq.trimmed();
      s.name     = ps.label.trimmed();
      s.frameIn     = std::max(1, frameRanges[i].first - handles);
      s.frameOut    = frameRanges[i].second + handles;
      s.nbFrames    = s.frameOut - s.frameIn + 1;
      s.kitsuShotId = ps.kitsuShotId;  // rename-proof update when known
      shots.push_back(s);
      for (auto it = ps.tasks.constBegin(); it != ps.tasks.constEnd(); ++it) {
        KitsuTaskPush tp;
        tp.seq = s.seq; tp.shot = s.name;
        tp.taskType = it.key(); tp.status = it.value().status;
        m_pendingTasks.push_back(tp);
      }
    }
  } else {
    int acc = 0;  // running frame offset for cumulative in/out (edit timecode)
    for (int i = 0; i < m->shotCount(); i++) {
      const ShotData &sd  = m->shot(i);
      const int dur       = sd.totalDuration();
      const QString label = sd.label().trimmed();
      if (label.isEmpty()) { acc += dur; ++skipped; continue; }
      QString seq;  // parent sequence label, if the shot belongs to one
      if (!sd.sequenceId.isEmpty())
        for (const SequenceData &sq : m->sequences())
          if (sq.uuid == sd.sequenceId) { seq = sq.label.trimmed(); break; }
      KitsuShotPush s;
      s.seq      = seq.isEmpty() ? kDefaultSeq : seq;
      s.name     = label;
      s.frameIn  = std::max(1, acc + 1 - handles);
      s.frameOut = acc + dur + handles;
      s.nbFrames = s.frameOut - s.frameIn + 1;
      acc        = acc + dur;  // timeline advances by the real duration
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
