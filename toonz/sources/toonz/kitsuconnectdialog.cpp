#include "kitsuconnectdialog.h"

#include "kitsuclient.h"
#include "ztorymodel.h"  // ZtoryModel::taskStatusLabel

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include "ztorysecret.h"

#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QAbstractItemView>
#include <QFontMetrics>
#include <algorithm>
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
  m_urlEdit->setToolTip(
      tr("Primary Kitsu address, used for login and all syncing.\n"
         "May be a LAN address, a Cloudflare tunnel or a CGWire host."));
  m_localUrlEdit = new QLineEdit(connBox);
  m_localUrlEdit->setPlaceholderText(tr("(optional) http://192.168.1.10:8012"));
  m_localUrlEdit->setToolTip(
      tr("Optional LAN-direct address of the SAME Kitsu instance, used only for\n"
         "uploading shot previews. When you're on the studio network this skips\n"
         "the remote proxy's upload size cap (e.g. Cloudflare's 100 MB) and is\n"
         "faster. Leave empty to upload through the primary URL. If it can't be\n"
         "reached at upload time, Ztoryc falls back to the primary URL."));
  m_emailEdit    = new QLineEdit(connBox);
  m_emailEdit->setPlaceholderText("you@studio.com");
  m_pwdEdit      = new QLineEdit(connBox);
  m_pwdEdit->setEchoMode(QLineEdit::Password);
  m_savePwd      = new QCheckBox(tr("Remember password on this machine"), connBox);
  form->addRow(tr("URL:"),           m_urlEdit);
  form->addRow(tr("Local URL:"),     m_localUrlEdit);
  form->addRow(tr("Email:"),         m_emailEdit);
  form->addRow(tr("Password:"),      m_pwdEdit);
  form->addRow(QString(),            m_savePwd);
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

  // This dialog handles only connection + project binding. The recurring sync
  // actions (push/pull shots, statuses, assets and preview upload) live in the
  // Production Tracker's Project tab, where the work happens.
  root->addStretch(1);

  auto *closeBtn = new QPushButton(tr("Close"), this);
  auto *btnRow   = new QHBoxLayout();
  btnRow->addStretch(1);
  btnRow->addWidget(closeBtn);
  root->addLayout(btnRow);

  // --- Wiring ----------------------------------------------------------
  // The client is an app-lifetime singleton (already loaded its settings), so the
  // session and the saved URL/email/password survive across dialog opens.
  m_urlEdit->setText(m_client->baseUrl());
  m_localUrlEdit->setText(m_client->localUrl());
  m_emailEdit->setText(m_client->email());
  // Default "remember" to ON for convenience (typically a local/studio instance)
  // — but only where there is a keychain to put it in. Offering it elsewhere
  // would promise something we deliberately refuse to do (see ztorysecret.h).
  if (ZtorySecret::isAvailable())
    m_savePwd->setChecked(true);
  else {
    m_savePwd->setChecked(false);
    m_savePwd->setEnabled(false);
    m_savePwd->setToolTip(
        tr("No system keychain available on this platform, so the password "
           "cannot be stored safely and is not kept."));
  }
  if (m_client->hasSavedPassword())
    m_pwdEdit->setPlaceholderText(tr("•••••• (saved)"));  // signal it's remembered

  connect(m_connectBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onConnectClicked);
  connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_linkBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onLinkClicked);
  connect(m_createBtn, &QPushButton::clicked, this,
          &KitsuConnectDialog::onCreateClicked);

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
          [this](const QVector<KitsuProject> &) {
            // The list is built in episodesFetched, which arrives right after:
            // a tvshow must appear as its episodes, not as itself, and we only
            // know them once they are in. fetchEpisodes() always answers, even
            // when there is no tvshow at all.
            m_client->fetchEpisodes();
          });
  connect(m_client, &KitsuClient::episodesFetched, this,
          [this](const QMap<QString, QVector<KitsuEpisode>> &) {
            rebuildProjectCombo();
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

void KitsuConnectDialog::rebuildProjectCombo() {
  m_projectCombo->clear();
  for (const KitsuProject &p : m_client->projects()) {
    // Only the parts we actually have: an empty resolution used to leave a
    // stray "(, 25 fps)".
    QStringList metaBits;
    if (!p.resolution.isEmpty()) metaBits << p.resolution;
    if (!p.fps.isEmpty()) metaBits << QString("%1 fps").arg(p.fps);
    const QString meta =
        metaBits.isEmpty() ? QString()
                           : QString("  (%1)").arg(metaBits.join(", "));
    const QVector<KitsuEpisode> eps = m_client->episodes(p.id);

    // A show with no episodes yet still gets its own row: binding to it and
    // pushing is how the first episode gets created (pushEnsureEpisode).
    if (!p.isTvshow() || eps.isEmpty()) {
      m_projectCombo->addItem(p.name + meta, p.id);
      m_projectCombo->setItemData(m_projectCombo->count() - 1, QString(),
                                  Qt::UserRole + 1);
      m_projectCombo->setItemData(m_projectCombo->count() - 1, p.name,
                                  Qt::ToolTipRole);
      continue;
    }
    for (const KitsuEpisode &e : eps) {
      // EPISODE FIRST, production after. Qt elides at the end, so whatever
      // leads is what survives a narrow combo — and with six episodes of one
      // show the production name is the half that is identical on every row.
      const QString label = QString("%1 — %2").arg(e.name, p.name);
      m_projectCombo->addItem(label + meta, p.id);
      m_projectCombo->setItemData(m_projectCombo->count() - 1, e.id,
                                  Qt::UserRole + 1);
      m_projectCombo->setItemData(m_projectCombo->count() - 1, label,
                                  Qt::ToolTipRole);
    }
  }

  // Let the drop-down be as wide as its longest row even when the closed combo
  // is squeezed by the dialog: AdjustToContents alone only sizes the closed
  // widget, which the layout can then shrink again.
  if (m_projectCombo->count() > 0) {
    const QFontMetrics fm(m_projectCombo->font());
    int w = 0;
    for (int i = 0; i < m_projectCombo->count(); ++i)
      w = std::max(w, fm.horizontalAdvance(m_projectCombo->itemText(i)));
    // Room for the frame, the scrollbar and a little air.
    m_projectCombo->view()->setMinimumWidth(w + 48);
  }

  // Re-target the row this Ztoryc project is already bound to. Matching on the
  // PAIR matters: with six episodes of one show in the list, the project id
  // alone would land on the first of them.
  ZtoryModel *m           = ZtoryModel::instance();
  const QString boundProj = m->kitsuProjectId();
  const QString boundEp   = m->kitsuEpisodeId();
  if (!boundProj.isEmpty()) {
    for (int i = 0; i < m_projectCombo->count(); ++i) {
      if (m_projectCombo->itemData(i).toString() != boundProj) continue;
      if (m_projectCombo->itemData(i, Qt::UserRole + 1).toString() != boundEp)
        continue;
      m_projectCombo->setCurrentIndex(i);
      break;
    }
  }
  updateBindingButtons();
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
}

void KitsuConnectDialog::onLinkClicked() {
  const int row = m_projectCombo->currentIndex();
  if (row < 0) return;
  const QString id = m_projectCombo->itemData(row).toString();
  if (id.isEmpty()) return;
  const QString episodeId =
      m_projectCombo->itemData(row, Qt::UserRole + 1).toString();
  // Find the full project record from the last fetch.
  KitsuProject sel;
  for (const KitsuProject &p : m_client->projects())
    if (p.id == id) { sel = p; break; }
  if (sel.id.isEmpty()) return;

  QString episodeName;
  for (const KitsuEpisode &e : m_client->episodes(id))
    if (e.id == episodeId) { episodeName = e.name; break; }

  ZtoryModel *m = ZtoryModel::instance();
  m->setProduction(sel.name);
  m->setCode(sel.code);
  if (!sel.fps.isEmpty()) m->setFps(sel.fps.toInt());
  m->setProductionType(sel.productionType);
  m->setProductionStyle(sel.productionStyle);
  m->setRatio(sel.ratio);
  m->setResolution(sel.resolution);
  m->setKitsuProject(sel.id, sel.name);
  // Only overwrite the episode when a row actually carries one: on a show with
  // no episodes yet the user may have typed a name in the tracker, and binding
  // must not wipe it — pushEnsureEpisode() will create it under that name.
  if (!episodeId.isEmpty()) m->setKitsuEpisode(episodeId, episodeName);
  m->saveProjectDb();

  // Pull the project's team right away so the assignee picker is populated
  // (the Production panel handles teamPulled and merges it into the roster).
  m_client->pullTeam(sel.id);

  m_statusLabel->setStyleSheet("color:#22D160;");
  m_statusLabel->setText(episodeName.isEmpty()
                             ? tr("Linked to %1.").arg(sel.name)
                             : tr("Linked to %1 — episode %2.")
                                   .arg(sel.name, episodeName));
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

void KitsuConnectDialog::onConnectClicked() {
  m_client->setBaseUrl(m_urlEdit->text());
  m_client->setLocalUrl(m_localUrlEdit->text());
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
