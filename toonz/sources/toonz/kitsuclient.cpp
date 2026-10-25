#include "kitsuclient.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include "ztorysecret.h"
#include "ztorytaskflow.h"

#include <QDir>
#include <QMimeDatabase>
#include <algorithm>
#include <functional>
#include <memory>

namespace {
const char *kGroupBaseUrl  = "Ztoryc/Kitsu/BaseUrl";
const char *kGroupLocalUrl = "Ztoryc/Kitsu/LocalUrl";  // optional upload endpoint
const char *kGroupEmail    = "Ztoryc/Kitsu/Email";
// Legacy: passwords used to be written here in plain text. Kept only so
// loadSettings() can move an old value into the keychain and delete it.
const char *kGroupPassword = "Ztoryc/Kitsu/Password";
const char *kSecretService = "Ztoryc/Kitsu";
const char *kGroupHasPwd   = "Ztoryc/Kitsu/PasswordSaved";
const char *kGroupAutoPush = "Ztoryc/Kitsu/AutoPushStatus";

// Pull a human-readable message out of an error reply (Zou answers with a JSON
// body { "message": ... } on most failures); fall back to Qt's error string.
QString errorMessage(QNetworkReply *reply, const QByteArray &body) {
  QString msg;
  QJsonParseError perr;
  QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
  if (perr.error == QJsonParseError::NoError && doc.isObject())
    msg = doc.object().value("message").toString();
  if (msg.isEmpty()) msg = reply->errorString();

  // Senza la richiesta che ha fallito un errore come "The requested URL was not
  // found on the server" non dice niente: puo' venire da una qualsiasi delle
  // rotte del push. Metodo, percorso e codice HTTP rendono l'errore diagnostico.
  const QVariant verb = reply->request().attribute(QNetworkRequest::CustomVerbAttribute);
  const QVariant code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
  QString dove = reply->request().url().path();
  if (!verb.isNull()) dove = verb.toString() + " " + dove;
  if (code.isValid()) dove += " [" + QString::number(code.toInt()) + "]";
  // Su una riga sola: il pannello che mostra l'errore non ne manda a capo,
  // e una seconda riga verrebbe semplicemente tagliata via.
  return dove.isEmpty() ? msg : msg + "  --  " + dove;
}
}  // namespace

//----------------------------------------------------------------------------

KitsuClient::KitsuClient(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)) {
  // Qt 5.15 non segue i redirect (ManualRedirectPolicy): un server dietro
  // Cloudflare con "Always Use HTTPS" risponde 301 a un URL scritto http://, e
  // senza questa riga la richiesta muore con un errore che non spiega niente.
  // NoLessSafe accetta la promozione http->https e rifiuta il contrario.
  m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

KitsuClient::~KitsuClient() = default;

KitsuClient *KitsuClient::instance() {
  // App-lifetime singleton (intentionally never deleted). loadSettings() once so
  // the saved URL/email/password are available immediately.
  static KitsuClient *s = nullptr;
  if (!s) {
    s = new KitsuClient();
    s->loadSettings();
    // Every status change made in Ztoryc passes here; onTaskTransition sends
    // it only when the automatic push is on.
    connect(ZtoryModel::instance()->taskEvents(), &ZtoryTaskEvents::transitioned,
            s, &KitsuClient::onTaskTransition);
  }
  return s;
}

//----------------------------------------------------------------------------

void KitsuClient::setBaseUrl(const QString &url) {
  m_baseUrl = url.trimmed();
  while (m_baseUrl.endsWith('/')) m_baseUrl.chop(1);
  m_trCachesLoaded = false;  // another server: other ids
}

void KitsuClient::setLocalUrl(const QString &url) {
  m_localUrl = url.trimmed();
  while (m_localUrl.endsWith('/')) m_localUrl.chop(1);
  m_trCachesLoaded = false;
}

void KitsuClient::loadSettings() {
  QSettings s;
  setBaseUrl(s.value(kGroupBaseUrl, "http://localhost:8012").toString());
  setLocalUrl(s.value(kGroupLocalUrl).toString());
  m_email         = s.value(kGroupEmail).toString();
  m_passwordSaved = s.value(kGroupHasPwd, false).toBool();
  m_autoPushStatus = s.value(kGroupAutoPush, false).toBool();
  if (!m_passwordSaved || m_email.isEmpty()) return;

  // Until 0.13.2 the password sat in QSettings as readable text — the registry
  // on Windows. Move any leftover into the keychain on first run and delete it:
  // keeping the readable copy next to the safe one would defeat the change.
  const QString legacy = s.value(kGroupPassword).toString();
  if (!legacy.isEmpty()) {
    m_passwordSaved = ZtorySecret::store(kSecretService, m_email, legacy);
    // Eager on purpose, unlike the read below: this is what gets the readable
    // password out of QSettings, and postponing it would leave it there for
    // anyone who never opens Kitsu again.
    m_passwordFetched = true;
    if (m_passwordSaved) m_password = legacy;
    s.remove(kGroupPassword);
    s.setValue(kGroupHasPwd, m_passwordSaved);
    return;
  }

  // NOT read here.  Touching the keychain at startup makes macOS ask for the
  // keychain password on every launch — the panels build KitsuClient::instance()
  // just to connect signals, so it happened even to someone who never opens
  // Kitsu.  The read waits until the password is about to be used: see
  // ensurePasswordLoaded(), called from login().
}

void KitsuClient::ensurePasswordLoaded() {
  if (m_passwordFetched) return;
  m_passwordFetched = true;
  if (!m_passwordSaved || m_email.isEmpty()) return;
  m_password = ZtorySecret::retrieve(kSecretService, m_email);
  // Gone from the keychain (another machine, the user cleared it, or they
  // refused the prompt): stop claiming it is remembered, or the dialog shows
  // "saved" over nothing.
  if (m_password.isEmpty()) m_passwordSaved = false;
}

void KitsuClient::saveSettings(bool savePassword) {
  QSettings s;
  s.setValue(kGroupBaseUrl, m_baseUrl);
  s.setValue(kGroupLocalUrl, m_localUrl);
  s.setValue(kGroupEmail, m_email);
  // Never written to QSettings again. Removing it unconditionally means an
  // upgrade cleans up the old plain-text value even for someone who never
  // reopens the dialog with "remember" ticked.
  s.remove(kGroupPassword);

  // store() failing is not an error to report: it means there is nowhere safe
  // to keep the password, so we simply do not remember it.
  if (savePassword && !m_email.isEmpty()) {
    // The dialog leaves m_password empty when the user typed nothing and is
    // relying on the saved one.  Since the read is lazy, storing here without
    // fetching first would overwrite the good password with an empty string —
    // and the login right after would then fetch back that empty string.
    ensurePasswordLoaded();
    // Never store an empty secret: "remembered" would be a lie, and it would
    // destroy a perfectly good saved password.
    if (!m_password.isEmpty())
      m_passwordSaved = ZtorySecret::store(kSecretService, m_email, m_password);
    else
      m_passwordSaved = false;
  } else {
    ZtorySecret::remove(kSecretService, m_email);
    m_passwordSaved = false;
  }
  s.setValue(kGroupHasPwd, m_passwordSaved);
}

//----------------------------------------------------------------------------
// Status mapping
//----------------------------------------------------------------------------

// One task status as the server sends it. Read in the status fetch and in
// the safe push: one parser, or the two copies drift apart.
static KitsuTaskStatus parseTaskStatus(const QJsonObject &o) {
  KitsuTaskStatus s;
  s.id                = o.value("id").toString();
  s.name              = o.value("name").toString();
  s.shortName         = o.value("short_name").toString();
  s.color             = o.value("color").toString();
  s.isDone            = o.value("is_done").toBool();
  s.isRetake          = o.value("is_retake").toBool();
  s.isFeedbackRequest = o.value("is_feedback_request").toBool();
  s.isDefault         = o.value("is_default").toBool();
  return s;
}

// The six short names of Ztoryc's pipeline: only these are used to WRITE a
// status, so Kitsu's approved/rejected/neutral don't shadow them.
static bool isPipelineShortName(const QString &shortName) {
  const QString sn = shortName.toLower();
  return sn == "todo" || sn == "ready" || sn == "wip" || sn == "wfa" ||
         sn == "retake" || sn == "done";
}

TaskStatus KitsuClient::mapStatus(const KitsuTaskStatus &s) {
  const QString sn = s.shortName.toLower();
  // Primary: the canonical Kitsu short names line up 1:1 with our pipeline.
  if (sn == "todo") return TaskStatus::Todo;
  if (sn == "ready") return TaskStatus::Ready;
  if (sn == "wip") return TaskStatus::Wip;
  if (sn == "wfa") return TaskStatus::Wfa;
  if (sn == "retake") return TaskStatus::Retake;
  if (sn == "done") return TaskStatus::Done;
  // Fallback: rely on the semantic flags so a renamed/custom pipeline still
  // lands in the right bucket.
  if (s.isDone) return TaskStatus::Done;
  if (s.isRetake) return TaskStatus::Retake;
  if (s.isFeedbackRequest) return TaskStatus::Wfa;
  // Kitsu's review-only statuses without flags: approval reads as done,
  // rejection as a retake; anything else (e.g. Neutral) is treated as Todo.
  if (sn == "approved") return TaskStatus::Done;
  if (sn == "rejected") return TaskStatus::Retake;
  return TaskStatus::Todo;
}

// Reverse map: Ztoryc TaskStatus -> Kitsu status id. Only the six pipeline
// short names, so approved/rejected/neutral don't shadow them. It was copied
// in the shot push, the asset push and the preview upload (2026-09-26).
void KitsuClient::rebuildStatusIdByZ() {
  m_statusIdByZ.clear();
  for (const KitsuTaskStatus &st : m_taskStatuses)
    if (isPipelineShortName(st.shortName))
      m_statusIdByZ.insert(static_cast<int>(mapStatus(st)), st.id);
}

TaskStatus KitsuClient::toZtoryStatus(const QString &kitsuStatusId) const {
  return m_statusById.value(kitsuStatusId, TaskStatus::Todo);
}

//----------------------------------------------------------------------------
// Login
//----------------------------------------------------------------------------

void KitsuClient::login() {
  if (m_baseUrl.isEmpty()) {
    emit loginFinished(false, tr("No Kitsu URL configured."));
    return;
  }
  QNetworkRequest req((QUrl(m_baseUrl + "/api/auth/login")));
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  ensurePasswordLoaded();  // the one place the saved password is actually used
  QJsonObject body;
  body["email"]    = m_email;
  body["password"] = m_password;

  QNetworkReply *reply =
      m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onLoginReply(reply); });
}

void KitsuClient::onLoginReply(QNetworkReply *reply) {
  reply->deleteLater();
  const QByteArray body = reply->readAll();

  if (reply->error() != QNetworkReply::NoError) {
    m_syncAfterLogin = false;
    emit loginFinished(false, errorMessage(reply, body));
    return;
  }

  const QJsonObject o = QJsonDocument::fromJson(body).object();
  m_accessToken       = o.value("access_token").toString();
  m_refreshToken      = o.value("refresh_token").toString();
  m_trCachesLoaded    = false;  // a new login may be on another server
  m_revAutomationsProject.clear();

  if (m_accessToken.isEmpty()) {
    m_syncAfterLogin = false;
    emit loginFinished(false, tr("Login response had no access token."));
    return;
  }

  const QJsonObject user = o.value("user").toObject();
  m_userRole             = user.value("role").toString();
  const QString who      = user.value("email").toString().isEmpty()
                               ? m_email
                               : user.value("email").toString();
  emit loginFinished(true, tr("Connected as %1").arg(who));

  if (m_syncAfterLogin) {
    m_syncAfterLogin = false;
    fetchProjects();
    fetchTaskStatuses();
  }
}

void KitsuClient::connectAndSync() {
  m_syncAfterLogin = true;
  login();
}

//----------------------------------------------------------------------------
// Projects
//----------------------------------------------------------------------------

void KitsuClient::fetchProjects() {
  if (!isLoggedIn()) {
    emit networkError(tr("Not logged in."));
    return;
  }
  QNetworkRequest req((QUrl(m_baseUrl + "/api/data/projects/open")));
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  QNetworkReply *reply = m_nam->get(req);
  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onProjectsReply(reply); });
}

void KitsuClient::onProjectsReply(QNetworkReply *reply) {
  reply->deleteLater();
  const QByteArray body = reply->readAll();
  if (reply->error() != QNetworkReply::NoError) {
    emit networkError(errorMessage(reply, body));
    return;
  }

  m_projects.clear();
  const QJsonArray arr = QJsonDocument::fromJson(body).array();
  for (const QJsonValue &v : arr) m_projects.push_back(parseProject(v.toObject()));
  emit projectsFetched(m_projects);
}

//-----------------------------------------------------------------------------

void KitsuClient::fetchEpisodes() {
  m_episodes.clear();
  m_episodesPending = 0;
  if (!isLoggedIn()) {
    emit networkError(tr("Not logged in."));
    emit episodesFetched(m_episodes);
    return;
  }

  QVector<KitsuProject> shows;
  for (const KitsuProject &p : m_projects)
    if (p.isTvshow()) shows.push_back(p);

  // Nothing to ask about: answer anyway, so a caller can always wait for this
  // one signal instead of branching on the production type.
  if (shows.isEmpty()) { emit episodesFetched(m_episodes); return; }

  m_episodesPending = shows.size();
  for (const KitsuProject &show : shows) {
    const QString projectId = show.id;
    QNetworkReply *reply =
        m_nam->get(authGet("/api/data/projects/" + projectId + "/episodes"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, projectId]() {
      reply->deleteLater();
      const QByteArray b = reply->readAll();
      // A show whose episodes we cannot read is reported and then simply has
      // none: one unreadable production must not sink the whole list.
      if (reply->error() != QNetworkReply::NoError) {
        emit networkError(errorMessage(reply, b));
      } else {
        QVector<KitsuEpisode> eps;
        for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
          const QJsonObject o = v.toObject();
          KitsuEpisode e;
          e.id        = o.value("id").toString();
          e.name      = o.value("name").toString();
          e.projectId = projectId;
          if (!e.id.isEmpty()) eps.push_back(e);
        }
        std::sort(eps.begin(), eps.end(),
                  [](const KitsuEpisode &a, const KitsuEpisode &b) {
                    return a.name.localeAwareCompare(b.name) < 0;
                  });
        m_episodes.insert(projectId, eps);
      }
      if (--m_episodesPending == 0) emit episodesFetched(m_episodes);
    });
  }
}

KitsuProject KitsuClient::parseProject(const QJsonObject &o) {
  KitsuProject p;
  p.id              = o.value("id").toString();
  p.name            = o.value("name").toString();
  p.code            = o.value("code").toString();
  p.fps             = o.value("fps").toString();
  p.ratio           = o.value("ratio").toString();
  p.resolution      = o.value("resolution").toString();
  p.productionType  = o.value("production_type").toString();
  p.productionStyle = o.value("production_style").toString();
  return p;
}

QByteArray KitsuClient::projectBody(const KitsuProject &p) const {
  // Only send non-empty fields so a PUT update never blanks a value the caller
  // left untouched. fps/ratio/resolution are strings on the Kitsu wire.
  QJsonObject o;
  if (!p.name.isEmpty()) o["name"] = p.name;
  if (!p.code.isEmpty()) o["code"] = p.code;
  if (!p.fps.isEmpty()) o["fps"] = p.fps;
  if (!p.ratio.isEmpty()) o["ratio"] = p.ratio;
  if (!p.resolution.isEmpty()) o["resolution"] = p.resolution;
  if (!p.productionType.isEmpty()) o["production_type"] = p.productionType;
  if (!p.productionStyle.isEmpty()) o["production_style"] = p.productionStyle;
  return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

//----------------------------------------------------------------------------
// Create / update project (admin / manager only — server enforces 403 too)
//----------------------------------------------------------------------------

void KitsuClient::createProject(const KitsuProject &p) {
  if (!isLoggedIn()) {
    emit projectCreated(false, KitsuProject(), tr("Not logged in."));
    return;
  }
  QNetworkRequest req((QUrl(m_baseUrl + "/api/data/projects")));
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  QNetworkReply *reply = m_nam->post(req, projectBody(p));
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    const QByteArray b = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      emit projectCreated(false, KitsuProject(), errorMessage(reply, b));
      return;
    }
    const KitsuProject created = parseProject(QJsonDocument::fromJson(b).object());
    emit projectCreated(true, created, tr("Project created."));
  });
}

void KitsuClient::updateProject(const QString &id, const KitsuProject &p) {
  if (!isLoggedIn()) {
    emit projectUpdated(false, tr("Not logged in."));
    return;
  }
  QNetworkRequest req((QUrl(m_baseUrl + "/api/data/projects/" + id)));
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  QNetworkReply *reply = m_nam->put(req, projectBody(p));
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    const QByteArray b = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      emit projectUpdated(false, errorMessage(reply, b));
      return;
    }
    emit projectUpdated(true, tr("Project updated."));
  });
}

//----------------------------------------------------------------------------
// Shot push — sequential async state machine (push-only: Ztoryc -> Kitsu)
//----------------------------------------------------------------------------

QNetworkRequest KitsuClient::authGet(const QString &path,
                                     const QString &base) const {
  QNetworkRequest req((QUrl((base.isEmpty() ? m_baseUrl : base) + path)));
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  return req;
}

QNetworkReply *KitsuClient::authPost(const QString &path, const QByteArray &body,
                                     const QString &base) {
  QNetworkRequest req((QUrl((base.isEmpty() ? m_baseUrl : base) + path)));
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  return m_nam->post(req, body);
}

QNetworkReply *KitsuClient::authPut(const QString &path, const QByteArray &body,
                                    const QString &base) {
  QNetworkRequest req((QUrl((base.isEmpty() ? m_baseUrl : base) + path)));
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  return m_nam->put(req, body);
}

void KitsuClient::pushFail(const QString &message) {
  emit shotsPushed(false, m_pushCreated, m_pushUpdated, message);
}

void KitsuClient::pushShots(const QString &projectId, const QString &episodeName,
                            bool tvshow, const QVector<KitsuShotPush> &shots,
                            bool renameExisting,
                            const QStringList &removedShotIds) {
  m_pushRenames = renameExisting;
  m_pushRemoved = removedShotIds;
  m_pushRemovedDone.clear();
  m_pushRemovedIndex = 0;
  m_pushRenameRetried.clear();
  if (!isLoggedIn()) { emit shotsPushed(false, 0, 0, tr("Not logged in.")); return; }
  if (projectId.isEmpty()) {
    emit shotsPushed(false, 0, 0, tr("Project not linked to Kitsu."));
    return;
  }
  m_pushProjectId   = projectId;
  m_pushEpisodeName = episodeName.trimmed();
  m_pushTvshow      = tvshow;
  m_pushQueue       = shots;
  m_pushSeqIds.clear();
  m_pushShotIds.clear();
  m_pushShotData.clear();
  m_pushShotSeq.clear();
  m_pushShotName.clear();
  m_pushRenamePlan.clear();
  m_pushRenameIndex = 0;
  m_pushResolved.clear();
  m_pushEpisodeId.clear();
  m_pushIndex = m_pushCreated = m_pushUpdated = 0;
  pushEnsureEpisode();
}

void KitsuClient::pushEnsureEpisode() {
  // Only tvshow productions have episodes; a blank episode name means "none".
  if (!m_pushTvshow || m_pushEpisodeName.isEmpty()) { pushLoadSequences(); return; }
  emit shotsPushProgress(tr("Ensuring episode %1…").arg(m_pushEpisodeName));
  QNetworkReply *reply =
      m_nam->get(authGet("/api/data/projects/" + m_pushProjectId + "/episodes"));
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    const QByteArray b = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) { pushFail(errorMessage(reply, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      if (o.value("name").toString() == m_pushEpisodeName) {
        m_pushEpisodeId = o.value("id").toString();
        break;
      }
    }
    if (!m_pushEpisodeId.isEmpty()) {
      emit episodeResolved(m_pushProjectId, m_pushEpisodeId, m_pushEpisodeName);
      pushLoadSequences();
      return;
    }
    QJsonObject body;
    body["name"] = m_pushEpisodeName;
    QNetworkReply *cr =
        authPost("/api/data/projects/" + m_pushProjectId + "/episodes",
                 QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(cr, &QNetworkReply::finished, this, [this, cr]() {
      cr->deleteLater();
      const QByteArray cb = cr->readAll();
      if (cr->error() != QNetworkReply::NoError) { pushFail(errorMessage(cr, cb)); return; }
      m_pushEpisodeId = QJsonDocument::fromJson(cb).object().value("id").toString();
      if (!m_pushEpisodeId.isEmpty())
        emit episodeResolved(m_pushProjectId, m_pushEpisodeId, m_pushEpisodeName);
      pushLoadSequences();
    });
  });
}

void KitsuClient::pushLoadSequences() {
  emit shotsPushProgress(tr("Loading sequences…"));
  QNetworkReply *reply =
      m_nam->get(authGet("/api/data/projects/" + m_pushProjectId + "/sequences"));
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    const QByteArray b = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) { pushFail(errorMessage(reply, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      // For a tvshow, restrict to our episode so same-named sequences in other
      // episodes don't get reused by mistake.
      if (m_pushTvshow && !m_pushEpisodeId.isEmpty() &&
          o.value("parent_id").toString() != m_pushEpisodeId)
        continue;
      // Chiave in minuscolo: Kitsu e Ztoryc possono scrivere lo stesso nome
      // con maiuscole diverse (SQ01 / sq01) e senza normalizzare si creano
      // sequenze e shot doppioni invece di riagganciarsi a quelli esistenti.
      m_pushSeqIds.insert(o.value("name").toString().toLower(),
                          o.value("id").toString());
    }
    pushLoadShots();
  });
}

void KitsuClient::pushLoadShots() {
  emit shotsPushProgress(tr("Loading existing shots…"));
  QNetworkReply *reply =
      m_nam->get(authGet("/api/data/projects/" + m_pushProjectId + "/shots"));
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    const QByteArray b = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) { pushFail(errorMessage(reply, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      const QString shotId = o.value("id").toString();
      m_pushShotIds.insert(
          o.value("parent_id").toString() + "/" + o.value("name").toString().toLower(),
          shotId);
      m_pushShotData.insert(shotId, o.value("data").toObject());
      m_pushShotSeq.insert(shotId, o.value("parent_id").toString());
      m_pushShotName.insert(shotId, o.value("name").toString());
    }
    pushNextRemoved();
  });
}

// Ztoryc: a renumber renames shots in a chain (sh150 -> sh140, sh160 ->
// sh150 …), and Kitsu refuses a name another shot of the sequence still holds
// (Cascina, 2026-10-25). Before the push, two kinds of renames clear the way:
//  - a shot that holds a name we need and is not ours to push — gone from the
//    storyboard — takes «_removed» (never deleted: its tasks and comments
//    stay, the production decides);
//  - every shot of ours whose name changes takes a provisional name first.
// The push then gives the final names, which are all free.
// Ztoryc: a shot deleted from the storyboard goes from Kitsu too (Franco,
// 2026-10-25), as Kitsu itself does it: deleted for good when it holds no
// work (every task Todo or Ready, no comment, no preview), cancelled when it
// does — hidden from Kitsu's lists, recoverable — and renamed «_removed» so
// its name is free for the renumbered shots. Only our own shots, the ones the
// tracker remembers: a shot the production made on Kitsu is never touched.
void KitsuClient::pushNextRemoved() {
  if (!m_pushRenames || m_pushRemovedIndex >= m_pushRemoved.size()) {
    if (!m_pushRemovedDone.isEmpty()) emit removedShotsHandled(m_pushRemovedDone);
    pushPlanRenames();
    return;
  }
  const QString id = m_pushRemoved[m_pushRemovedIndex];
  bool stillOurs   = false;
  for (const KitsuShotPush &sh : m_pushQueue) stillOurs = stillOurs || sh.kitsuShotId == id;
  bool inEpisode = false;
  for (const QString &seqId : m_pushSeqIds)
    inEpisode = inEpisode || seqId == m_pushShotSeq.value(id);
  // still in a storyboard, already gone from Kitsu, or not of this episode
  if (stillOurs || !m_pushShotSeq.contains(id) || !inEpisode) {
    pushRemovedDone(id);
    return;
  }
  emit shotsPushProgress(tr("Removing %1 from Kitsu…").arg(m_pushShotName.value(id)));
  QNetworkReply *r = m_nam->get(authGet("/api/data/shots/" + id + "/tasks"));
  connect(r, &QNetworkReply::finished, this, [this, r, id]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { pushFail(errorMessage(r, b)); return; }
    bool work = false;
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject t     = v.toObject();
      const TaskStatus status = toZtoryStatus(t.value("task_status_id").toString());
      work = work || (status != TaskStatus::Todo && status != TaskStatus::Ready) ||
             !t.value("last_comment_date").toString().isEmpty() ||
             !t.value("last_preview_file_id").toString().isEmpty();
    }
    if (work) { pushRetireRemoved(id, true); return; }
    QNetworkRequest req(QUrl(m_baseUrl + "/api/data/shots/" + id + "?force=true"));
    req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    QNetworkReply *d = m_nam->deleteResource(req);
    connect(d, &QNetworkReply::finished, this, [this, d, id]() {
      d->deleteLater();
      // Deleting needs a manager (or the shot's creator): refused, the shot
      // is retired like one that holds work.
      if (d->error() != QNetworkReply::NoError) { pushRetireRemoved(id, true); return; }
      pushForgetShot(id);
      pushRemovedDone(id);
    });
  });
}

// Renamed «_removed», then cancelled with Kitsu's own delete without force.
void KitsuClient::pushRetireRemoved(const QString &id, bool cancel) {
  const QString seqId = m_pushShotSeq.value(id);
  const QString name  = pushFreeName(seqId, m_pushShotName.value(id) + "_removed");
  QJsonObject body;
  body["name"] = name;
  QNetworkReply *r = authPut("/api/data/shots/" + id,
                             QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(r, &QNetworkReply::finished, this, [this, r, id, seqId, name, cancel]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { pushFail(errorMessage(r, b)); return; }
    m_pushShotIds.remove(seqId + "/" + m_pushShotName.value(id).toLower());
    m_pushShotIds.insert(seqId + "/" + name.toLower(), id);
    m_pushShotName.insert(id, name);
    if (!cancel) { pushRemovedDone(id); return; }
    QNetworkRequest req(QUrl(m_baseUrl + "/api/data/shots/" + id));
    req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    QNetworkReply *d = m_nam->deleteResource(req);
    connect(d, &QNetworkReply::finished, this, [this, d, id]() {
      d->deleteLater();  // a refused cancel leaves it renamed: enough
      pushRemovedDone(id);
    });
  });
}

void KitsuClient::pushRemovedDone(const QString &id) {
  m_pushRemovedDone << id;
  ++m_pushRemovedIndex;
  pushNextRemoved();
}

void KitsuClient::pushForgetShot(const QString &id) {
  m_pushShotIds.remove(m_pushShotSeq.value(id) + "/" +
                       m_pushShotName.value(id).toLower());
  m_pushShotSeq.remove(id);
  m_pushShotName.remove(id);
  m_pushShotData.remove(id);
}

// base, or base2, base3 … : the first name no shot of the sequence holds.
QString KitsuClient::pushFreeName(const QString &seqId, const QString &base) const {
  QSet<QString> taken;
  for (auto it = m_pushShotSeq.constBegin(); it != m_pushShotSeq.constEnd(); ++it)
    if (it.value() == seqId) taken.insert(m_pushShotName.value(it.key()).toLower());
  QString name = base;
  for (int n = 2; taken.contains(name.toLower()); ++n) name = base + QString::number(n);
  return name;
}

void KitsuClient::pushPlanRenames() {
  m_pushRenamePlan.clear();
  m_pushRenameIndex = 0;
  if (!m_pushRenames) { pushProcessNext(); return; }

  QSet<QString> ours;  // the shots this push updates by id, in our episode
  QSet<QString> episodeSeqs;
  for (const QString &id : m_pushSeqIds) episodeSeqs.insert(id);
  for (const KitsuShotPush &sh : m_pushQueue)
    if (!sh.kitsuShotId.isEmpty() &&
        episodeSeqs.contains(m_pushShotSeq.value(sh.kitsuShotId)))
      ours.insert(sh.kitsuShotId);

  auto rename = [this](const QString &id, const QString &name) {
    const QString seqId = m_pushShotSeq.value(id);
    m_pushShotIds.remove(seqId + "/" + m_pushShotName.value(id).toLower());
    m_pushShotIds.insert(seqId + "/" + name.toLower(), id);
    m_pushShotName.insert(id, name);
    m_pushRenamePlan.push_back({id, name});
  };

  for (const KitsuShotPush &sh : m_pushQueue) {
    const QString seqId = ours.contains(sh.kitsuShotId)
                              ? m_pushShotSeq.value(sh.kitsuShotId)
                              : m_pushSeqIds.value(sh.seq.toLower());
    if (seqId.isEmpty()) continue;
    const QString holder = m_pushShotIds.value(seqId + "/" + sh.name.toLower());
    if (holder.isEmpty() || holder == sh.kitsuShotId || ours.contains(holder))
      continue;
    // a shot without a valid id of ours is matched by name: it IS that shot
    if (!ours.contains(sh.kitsuShotId)) continue;
    rename(holder, pushFreeName(seqId, m_pushShotName.value(holder) + "_removed"));
  }
  for (const KitsuShotPush &sh : m_pushQueue) {
    if (!ours.contains(sh.kitsuShotId)) continue;
    const QString now = m_pushShotName.value(sh.kitsuShotId);
    if (now.compare(sh.name, Qt::CaseInsensitive) == 0) continue;
    rename(sh.kitsuShotId, now + "_renaming_" + sh.kitsuShotId.left(8));
  }
  pushNextRename();
}

void KitsuClient::pushNextRename() {
  if (m_pushRenameIndex >= m_pushRenamePlan.size()) { pushProcessNext(); return; }
  const QPair<QString, QString> step = m_pushRenamePlan[m_pushRenameIndex];
  emit shotsPushProgress(tr("Renaming shots on Kitsu (%1/%2)…")
                             .arg(m_pushRenameIndex + 1)
                             .arg(m_pushRenamePlan.size()));
  QJsonObject body;
  body["name"] = step.second;
  QNetworkReply *r = authPut("/api/data/shots/" + step.first,
                             QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { pushFail(errorMessage(r, b)); return; }
    ++m_pushRenameIndex;
    pushNextRename();
  });
}

QJsonObject KitsuClient::pushBodyFor(const KitsuShotPush &sh,
                                    const QString &shotId) const {
  // Si parte dal 'data' che lo shot ha gia' su Kitsu e vi si sovrascrivono solo
  // le chiavi di cui Ztoryc e' padrone. Tutto il resto sopravvive al push.
  QJsonObject data = m_pushShotData.value(shotId);
  data["frame_in"]  = QString::number(sh.frameIn);
  data["frame_out"] = QString::number(sh.frameOut);
  QJsonObject body;
  body["nb_frames"] = sh.nbFrames;
  body["data"]      = data;
  // Il NOME lo scrive solo chi crea lo shot. Su uno shot che esiste gia' Kitsu
  // e' l'autorita': in una produzione con piu' persone che pushano, rimandare
  // il nome vorrebbe dire che l'ultimo che sincronizza rinomina gli shot a
  // tutti gli altri. Ztoryc resta padrone di durata e timecode, non del nome.
  // Ztoryc: until the storyboard is locked, Ztoryc is the authority on names
  // and a renumber reaches Kitsu too (Franco, 2026-10-23).
  if (shotId.isEmpty() || m_pushRenames) body["name"] = sh.name;
  return body;
}

void KitsuClient::pushProcessNext() {
  if (m_pushIndex >= m_pushQueue.size()) {
    emit shotIdsResolved(m_pushResolved);  // record ids so future syncs are rename-proof
    emit shotsPushed(true, m_pushCreated, m_pushUpdated,
                     tr("Done — %1 created, %2 updated.")
                         .arg(m_pushCreated)
                         .arg(m_pushUpdated));
    return;
  }
  const KitsuShotPush sh = m_pushQueue[m_pushIndex];
  const QString resolvedKey = sh.seq + "\n" + sh.name;

  // Il corpo si costruisce per singolo shot, perche' il 'data' da preservare
  // dipende da QUALE shot di Kitsu si sta per aggiornare: vedi pushBodyFor().
  // Su uno shot esistente il nome NON viene toccato: e' Kitsu l'autorita'.

  // A stored id must be a shot of OUR episode. A tracker could hold ids of
  // another episode, matched by name before the episode was linked: Cascina
  // held Messina's, and a Sync wrote Cascina's durations on Messina's shots
  // (2026-10-25). Such an id is dropped and the shot found by name, in the
  // episode's sequences only.
  if (!sh.kitsuShotId.isEmpty() && m_pushShotSeq.contains(sh.kitsuShotId)) {
    const QString seqId = m_pushShotSeq.value(sh.kitsuShotId);
    bool ours = false;
    for (const QString &id : m_pushSeqIds) ours = ours || id == seqId;
    if (!ours) {
      m_pushQueue[m_pushIndex].kitsuShotId.clear();
      pushProcessNext();
      return;
    }
  }

  // 1) Known Kitsu shot id → update THAT shot directly (rename-proof): no name
  //    lookup, so a shot renamed in Kitsu isn't duplicated.
  if (!sh.kitsuShotId.isEmpty()) {
    const QJsonObject body = pushBodyFor(sh, sh.kitsuShotId);
    QNetworkReply *ur = authPut("/api/data/shots/" + sh.kitsuShotId,
                                QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(ur, &QNetworkReply::finished, this,
            [this, ur, resolvedKey, id = sh.kitsuShotId]() {
              ur->deleteLater();
              const QByteArray ub = ur->readAll();
              if (ur->error() != QNetworkReply::NoError) {
                // 404 = lo shot a cui puntava l'id memorizzato non esiste piu'
                // (cancellato da Kitsu, cosa che un produttore fa di routine).
                // Far fallire l'intero push per questo e' sproporzionato:
                // si dimentica l'id morto e si riprova lo stesso shot per nome,
                // che al massimo lo ricrea.
                const int code =
                    ur->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (code == 404 && m_pushIndex < m_pushQueue.size()) {
                  m_pushQueue[m_pushIndex].kitsuShotId.clear();
                  pushProcessNext();
                  return;
                }
                // A rename can hit a name another shot still holds (SH020 →
                // SH030 before SH030 moves on): try it once more at the end,
                // when the others have been renamed.
                if (m_pushRenames && !m_pushRenameRetried.contains(id) &&
                    m_pushIndex < m_pushQueue.size()) {
                  m_pushRenameRetried.insert(id);
                  m_pushQueue.append(m_pushQueue[m_pushIndex]);
                  ++m_pushIndex;
                  pushProcessNext();
                  return;
                }
                pushFail(errorMessage(ur, ub));
                return;
              }
              m_pushResolved[resolvedKey] = id;
              ++m_pushUpdated;
              ++m_pushIndex;
              pushProcessNext();
            });
    return;
  }

  // Ensure the sequence exists before we can place a shot under it.
  if (!m_pushSeqIds.contains(sh.seq.toLower())) {
    emit shotsPushProgress(tr("Creating sequence %1…").arg(sh.seq));
    QJsonObject body;
    body["name"] = sh.seq;
    if (m_pushTvshow && !m_pushEpisodeId.isEmpty())
      body["episode_id"] = m_pushEpisodeId;
    QNetworkReply *cr =
        authPost("/api/data/projects/" + m_pushProjectId + "/sequences",
                 QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(cr, &QNetworkReply::finished, this, [this, cr, seq = sh.seq]() {
      cr->deleteLater();
      const QByteArray cb = cr->readAll();
      if (cr->error() != QNetworkReply::NoError) { pushFail(errorMessage(cr, cb)); return; }
      m_pushSeqIds.insert(seq.toLower(),
                          QJsonDocument::fromJson(cb).object().value("id").toString());
      pushProcessNext();  // retry the same shot now that its sequence exists
    });
    return;
  }

  const QString seqId = m_pushSeqIds.value(sh.seq.toLower());
  const QString key   = seqId + "/" + sh.name.toLower();
  if (m_pushShotIds.contains(key)) {
    const QString id = m_pushShotIds.value(key);
    const QJsonObject body = pushBodyFor(sh, id);
    QNetworkReply *ur = authPut("/api/data/shots/" + id,
                                QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(ur, &QNetworkReply::finished, this, [this, ur, resolvedKey, id]() {
      ur->deleteLater();
      const QByteArray ub = ur->readAll();
      if (ur->error() != QNetworkReply::NoError) { pushFail(errorMessage(ur, ub)); return; }
      m_pushResolved[resolvedKey] = id;
      ++m_pushUpdated;
      ++m_pushIndex;
      pushProcessNext();
    });
  } else {
    QJsonObject body = pushBodyFor(sh, QString());
    body["sequence_id"] = seqId;
    QNetworkReply *cr =
        authPost("/api/data/projects/" + m_pushProjectId + "/shots",
                 QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(cr, &QNetworkReply::finished, this, [this, cr, resolvedKey]() {
      cr->deleteLater();
      const QByteArray cb = cr->readAll();
      if (cr->error() != QNetworkReply::NoError) { pushFail(errorMessage(cr, cb)); return; }
      const QString id = QJsonDocument::fromJson(cb).object().value("id").toString();
      if (!id.isEmpty()) m_pushResolved[resolvedKey] = id;
      ++m_pushCreated;
      ++m_pushIndex;
      pushProcessNext();
    });
  }
}

//----------------------------------------------------------------------------
// Task + status push (Phase 3b) — sequential async (push-only)
//----------------------------------------------------------------------------

QString KitsuClient::statusIdFor(TaskStatus s) const {
  return m_statusIdByZ.value(static_cast<int>(s));
}

QString KitsuClient::normalizeTaskType(const QString &name) {
  QString k = name.toLower();
  // Ztoryc ↔ Kitsu name aliases (Kitsu's defaults use "Rendering" and "FX").
  if (k == "render")   k = "rendering";
  else if (k == "vfx") k = "fx";
  return k;
}

QString KitsuClient::resolveTaskTypeId(const QString &ztorycName) const {
  // keys stored lowercased in taskLoadTaskTypes
  return m_ttIdByName.value(normalizeTaskType(ztorycName));
}

void KitsuClient::taskFail(const QString &message) {
  emit tasksPushed(false, m_taskStatusesSet, message);
}

void KitsuClient::pushTasks(const QString &projectId,
                            const QVector<KitsuTaskPush> &tasks) {
  if (!isLoggedIn()) { emit tasksPushed(false, 0, tr("Not logged in.")); return; }
  if (projectId.isEmpty() || tasks.isEmpty()) {
    emit tasksPushed(true, 0, tr("No task statuses to push."));
    return;
  }
  m_taskProjectId = projectId;
  m_taskQueue     = tasks;
  m_ttIdByName.clear();
  m_taskSeqIds.clear();
  m_taskShotIds.clear();
  m_taskIdByKey.clear();
  m_taskStatusByKey.clear();
  m_taskAssigneesByKey.clear();
  m_assignQueue.clear();
  m_assignIdx = 0;
  m_ttCreateQueue.clear();
  m_ttPriorityById.clear();
  m_projectTtIds.clear();
  m_ttAddQueue.clear();
  m_taskCreateList.clear();
  m_taskMissingTypes.clear();
  m_taskMissingOrder.clear();
  m_ttAddIdx = m_tasksCreated = m_ttNewIdx = 0;
  m_taskCreateIdx = m_taskApplyIdx = m_taskStatusesSet = m_taskUnchanged = 0;

  // Reverse status map: Ztoryc TaskStatus -> canonical Kitsu status id (only the
  // six pipeline short names, so approved/rejected/neutral don't shadow them).
  rebuildStatusIdByZ();
  // Load the roster first so assignee names resolve to person ids for the
  // add-only assign pass that follows the status updates.
  loadRosterThen(m_taskProjectId, [this]() { taskLoadTaskTypes(); });
}

// The SHOT task types of a /api/data/task-types answer into m_ttIdByName
// (and their priority). Read by the task push and the clip upload: one
// parser, where there were two copies of the same loop.
void KitsuClient::parseShotTaskTypes(const QByteArray &body) {
  for (const QJsonValue &v : QJsonDocument::fromJson(body).array()) {
    const QJsonObject o = v.toObject();
    if (o.value("for_entity").toString() != QLatin1String("Shot")) continue;
    m_ttIdByName.insert(o.value("name").toString().toLower(),
                        o.value("id").toString());
    m_ttPriorityById.insert(o.value("id").toString(),
                            o.value("priority").toInt());
  }
}

void KitsuClient::taskLoadTaskTypes() {
  emit shotsPushProgress(tr("Loading task types…"));
  QNetworkReply *r = m_nam->get(authGet("/api/data/task-types"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { taskFail(errorMessage(r, b)); return; }
    parseShotTaskTypes(b);
    // Distinct task-type ids actually used by the queue. The ones Kitsu does
    // not have at all are created next (Franco, 2026-09-24: Ztoryc owns the
    // workflows, so Kitsu gets the task types they need).
    for (const KitsuTaskPush &t : m_taskQueue) {
      const QString id = resolveTaskTypeId(t.taskType);
      if (id.isEmpty()) {
        if (!m_taskMissingTypes.contains(t.taskType)) {
          m_taskMissingTypes << t.taskType;
          m_taskMissingOrder.insert(t.taskType, t.order);
        }
      } else if (!m_ttCreateQueue.contains(id))
        m_ttCreateQueue.push_back(id);
    }
    taskCreateTypeNext();
  });
}

void KitsuClient::taskCreateTypeNext() {
  if (m_ttNewIdx >= m_taskMissingTypes.size()) { taskLoadProject(); return; }
  const QString name = m_taskMissingTypes[m_ttNewIdx];
  emit shotsPushProgress(tr("Creating task type %1 in Kitsu…").arg(name));
  // Kitsu task types are server-wide: this one shows up in every project's
  // choices, but only joins THIS project's pipeline (taskAddTypeNext).
  QJsonObject body;
  body["name"]       = name;
  body["for_entity"] = "Shot";
  body["color"]      = "#999999";  // Kitsu's own neutral grey
  body["priority"]   = m_taskMissingOrder.value(name);
  QNetworkReply *r = authPost("/api/data/task-types",
                              QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(r, &QNetworkReply::finished, this, [this, r, name]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    // A refusal (e.g. not an admin) doesn't stop the push: that type stays in
    // m_taskMissingTypes and is reported at the end.
    if (r->error() == QNetworkReply::NoError) {
      const QJsonObject o = QJsonDocument::fromJson(b).object();
      const QString id    = o.value("id").toString();
      if (!id.isEmpty()) {
        m_ttIdByName.insert(normalizeTaskType(name), id);
        m_ttPriorityById.insert(id, o.value("priority").toInt());
        if (!m_ttCreateQueue.contains(id)) m_ttCreateQueue.push_back(id);
        m_taskMissingTypes.removeAt(m_ttNewIdx);
        taskCreateTypeNext();
        return;
      }
    }
    ++m_ttNewIdx;
    taskCreateTypeNext();
  });
}

void KitsuClient::taskLoadProject() {
  // A task whose type is not in the project's pipeline exists, but Kitsu does
  // not show its column on the shot page — it looks like it was never created.
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_taskProjectId));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { taskFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v :
         QJsonDocument::fromJson(b).object().value("task_types").toArray())
      m_projectTtIds.insert(v.toString());
    for (const QString &id : m_ttCreateQueue)
      if (!m_projectTtIds.contains(id)) m_ttAddQueue.push_back(id);
    taskAddTypeNext();
  });
}

void KitsuClient::taskAddTypeNext() {
  if (m_ttAddIdx >= m_ttAddQueue.size()) { taskLoadSequences(); return; }
  const QString ttId = m_ttAddQueue[m_ttAddIdx];
  emit shotsPushProgress(tr("Adding task types to the project (%1/%2)…")
                             .arg(m_ttAddIdx + 1)
                             .arg(m_ttAddQueue.size()));
  QJsonObject body;
  body["task_type_id"] = ttId;
  body["priority"]     = m_ttPriorityById.value(ttId);
  QNetworkReply *r =
      authPost("/api/data/projects/" + m_taskProjectId + "/settings/task-types",
               QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { taskFail(errorMessage(r, b)); return; }
    ++m_ttAddIdx;
    taskAddTypeNext();
  });
}

void KitsuClient::taskLoadSequences() {
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_taskProjectId + "/sequences"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { taskFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      m_taskSeqIds.insert(o.value("name").toString().toLower(),
                          o.value("id").toString());
    }
    taskLoadShots();
  });
}

void KitsuClient::taskLoadShots() {
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_taskProjectId + "/shots"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { taskFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      m_taskShotIds.insert(o.value("parent_id").toString() + "/" +
                               o.value("name").toString().toLower(),
                           o.value("id").toString());
    }
    taskLoadProjectTasks();
  });
}

QString KitsuClient::taskShotIdFor(const KitsuTaskPush &t) const {
  if (!t.kitsuShotId.isEmpty()) return t.kitsuShotId;
  // Fallback for a shot Ztoryc has no id for yet (seq+name, case-insensitive).
  return m_taskShotIds.value(m_taskSeqIds.value(t.seq.toLower()) + "/" +
                             t.shot.toLower());
}

void KitsuClient::taskLoadProjectTasks() {
  emit shotsPushProgress(tr("Reading existing tasks…"));
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_taskProjectId + "/tasks"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { taskFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      const QString key = o.value("entity_id").toString() + "/" +
                          o.value("task_type_id").toString();
      m_taskIdByKey.insert(key, o.value("id").toString());
      m_taskStatusByKey.insert(key, o.value("task_status_id").toString());
      QSet<QString> assignees;
      for (const QJsonValue &a : o.value("assignees").toArray())
        assignees.insert(a.toString());
      m_taskAssigneesByKey.insert(key, assignees);
    }
    // The tasks THIS push needs and Kitsu lacks: exactly these shots, exactly
    // these types. (The old "create-tasks" action worked on every shot of the
    // project, i.e. on every other episode too.)
    QSet<QString> queued;
    for (int i = 0; i < m_taskQueue.size(); ++i) {
      const KitsuTaskPush &t = m_taskQueue[i];
      const QString ttId   = resolveTaskTypeId(t.taskType);
      const QString shotId = taskShotIdFor(t);
      if (ttId.isEmpty() || shotId.isEmpty()) continue;
      const QString key = shotId + "/" + ttId;
      if (m_taskIdByKey.contains(key) || queued.contains(key)) continue;
      queued.insert(key);
      m_taskCreateList.push_back(i);
    }
    taskCreateMissingNext();
  });
}

QByteArray KitsuClient::newTaskBody(const QString &projectId,
                                    const QString &entityId,
                                    const QString &ttId) const {
  // Same fields gazu's new_task() sends. A new task starts at Todo.
  QJsonObject body;
  body["project_id"]     = projectId;
  body["entity_id"]      = entityId;
  body["task_type_id"]   = ttId;
  body["task_status_id"] = m_statusIdByZ.value(static_cast<int>(TaskStatus::Todo));
  body["assignees"]      = QJsonArray();
  body["name"]           = "main";
  return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

void KitsuClient::taskCreateMissingNext() {
  if (m_taskCreateIdx >= m_taskCreateList.size()) { taskApplyNext(); return; }
  if (m_statusIdByZ.value(static_cast<int>(TaskStatus::Todo)).isEmpty()) {
    taskFail(tr("Kitsu has no \"todo\" task status: cannot create tasks."));
    return;
  }
  const KitsuTaskPush &t = m_taskQueue[m_taskCreateList[m_taskCreateIdx]];
  const QString ttId   = resolveTaskTypeId(t.taskType);
  const QString shotId = taskShotIdFor(t);
  emit shotsPushProgress(tr("Creating tasks (%1/%2)…")
                             .arg(m_taskCreateIdx + 1)
                             .arg(m_taskCreateList.size()));
  QNetworkReply *r = authPost("/api/data/tasks",
                              newTaskBody(m_taskProjectId, shotId, ttId));
  const int qi = m_taskCreateList[m_taskCreateIdx];
  connect(r, &QNetworkReply::finished, this, [this, r, shotId, ttId, qi]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { taskFail(errorMessage(r, b)); return; }
    // A task just created is Todo on Kitsu: so is its base (a task deleted on
    // Kitsu and recreated here must not read as a conflict with an old base).
    // On a task just born, Ztoryc's status is the only one there is: it is
    // sent now, not at the next Sync.
    m_taskQueue[qi].expected   = TaskStatus::Todo;
    m_taskQueue[qi].createOnly = m_taskQueue[qi].status == TaskStatus::Todo;
    ZtoryTaskFlow::markSynced(ZtoryTaskFlow::Entity::Shot, m_taskQueue[qi].uuid,
                              m_taskQueue[qi].taskType, TaskStatus::Todo);
    const QJsonObject o = QJsonDocument::fromJson(b).object();
    const QString key   = shotId + "/" + ttId;
    m_taskIdByKey.insert(key, o.value("id").toString());
    m_taskStatusByKey.insert(key, o.value("task_status_id").toString());
    m_taskAssigneesByKey.insert(key, QSet<QString>());
    ++m_tasksCreated;
    ++m_taskCreateIdx;
    taskCreateMissingNext();
  });
}

void KitsuClient::taskApplyNext() {
  while (m_taskApplyIdx < m_taskQueue.size()) {
    const KitsuTaskPush t = m_taskQueue[m_taskApplyIdx];
    const QString ttId     = resolveTaskTypeId(t.taskType);
    const QString shotId   = taskShotIdFor(t);
    const QString taskId   = m_taskIdByKey.value(shotId + "/" + ttId);
    const QString statusId = statusIdFor(t.status);
    // Skip anything we couldn't resolve (unknown task-type, shot or status),
    // and the untouched workflow tasks: they only had to exist.
    if (t.createOnly || ttId.isEmpty() || shotId.isEmpty() ||
        taskId.isEmpty() || statusId.isEmpty()) {
      ++m_taskApplyIdx;
      continue;
    }
    // Already at the target status in Kitsu → don't re-comment (would spam the
    // activity feed and notifications); only touch what actually changed.
    const QString remoteId = m_taskStatusByKey.value(shotId + "/" + ttId);
    if (remoteId == statusId) {
      ZtoryTaskFlow::markSynced(ZtoryTaskFlow::Entity::Shot, t.uuid,
                                t.taskType, t.status);
      ++m_taskUnchanged;
      ++m_taskApplyIdx;
      continue;
    }
    // Kitsu moved on since the last sync, or has a status Ztoryc does not
    // know: its status is kept (the pull that follows brings it in).
    if (!isPipelineStatusId(remoteId) || toZtoryStatus(remoteId) != t.expected) {
      ++m_taskUnchanged;
      ++m_taskApplyIdx;
      continue;
    }
    emit shotsPushProgress(tr("Setting status (%1/%2)…")
                               .arg(m_taskApplyIdx + 1)
                               .arg(m_taskQueue.size()));
    QJsonObject body;
    body["task_status_id"] = statusId;
    body["comment"]        = "Status synced from Ztoryc";
    QNetworkReply *r = authPost("/api/actions/tasks/" + taskId + "/comment",
                                QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this, r, t]() {
      r->deleteLater();
      const QByteArray b = r->readAll();
      if (r->error() != QNetworkReply::NoError) { taskFail(errorMessage(r, b)); return; }
      // Written: the base is what Kitsu has now, even if the Sync stops later.
      ZtoryTaskFlow::markSynced(ZtoryTaskFlow::Entity::Shot, t.uuid, t.taskType,
                                t.status);
      ZtoryModel::instance()->saveProjectDb();
      ++m_taskStatusesSet;
      ++m_taskApplyIdx;
      taskApplyNext();
    });
    return;  // resume in the reply callback
  }
  // Statuses done — now queue the add-only assignee updates and run them before
  // reporting completion. Each Ztoryc assignee that maps to a known person and
  // isn't already on the task is added (never removed).
  const int statusesSet = m_taskStatusesSet, unchanged = m_taskUnchanged;
  const int created  = m_tasksCreated;
  const QStringList missingTypes = m_taskMissingTypes;
  m_assignQueue.clear();
  m_assignIdx = 0;
  QSet<QString> skippedNames;  // Ztoryc assignees that aren't on the Kitsu team
  for (const KitsuTaskPush &t : m_taskQueue) {
    if (t.assignees.isEmpty()) continue;
    const QString ttId   = resolveTaskTypeId(t.taskType);
    const QString shotId = taskShotIdFor(t);
    const QString taskId = m_taskIdByKey.value(shotId + "/" + ttId);
    if (taskId.isEmpty()) continue;
    QSet<QString> &have = m_taskAssigneesByKey[shotId + "/" + ttId];
    for (const QString &name : t.assignees) {
      const QString pid = m_personIdByName.value(name.trimmed().toLower());
      // Only assign people who are on this project's Kitsu team — never create
      // an out-of-team assignment. Unknown/out-of-team names are reported.
      if (pid.isEmpty() || !m_teamPersonIds.contains(pid)) {
        if (!name.trimmed().isEmpty()) skippedNames.insert(name.trimmed());
        continue;
      }
      if (have.contains(pid)) continue;
      have.insert(pid);  // avoid duplicate assigns within this push
      m_assignQueue.push_back({taskId, pid});
    }
  }
  const int assigned = m_assignQueue.size(), skipped = skippedNames.size();
  m_assignOnDone = [this, statusesSet, unchanged, assigned, skipped, created,
                    missingTypes]() {
    QString msg = unchanged > 0
                      ? tr("Done — %1 task statuses changed, %2 unchanged.")
                            .arg(statusesSet).arg(unchanged)
                      : tr("Done — %1 task statuses set in Kitsu.").arg(statusesSet);
    if (assigned > 0) msg += tr("  %1 people assigned.").arg(assigned);
    if (skipped > 0)  msg += tr("  %1 not in team (skipped).").arg(skipped);
    if (created > 0)  msg += tr("  %1 tasks created.").arg(created);
    if (!missingTypes.isEmpty()) {
      msg += tr("  %1 task types missing in Kitsu.").arg(missingTypes.size());
      emit taskTypesMissing(missingTypes);
    }
    emit tasksPushed(true, statusesSet, msg);
  };
  assignRun();
}

//----------------------------------------------------------------------------
// Pull statuses (review sync) — Kitsu -> Ztoryc, sequential async
//----------------------------------------------------------------------------

void KitsuClient::pullFail(const QString &message) {
  emit statusesPulled(false, {}, message);
}

void KitsuClient::pullStatuses(const QString &projectId,
                               const QString &episodeId) {
  if (!isLoggedIn()) { emit statusesPulled(false, {}, tr("Not logged in.")); return; }
  if (projectId.isEmpty()) {
    emit statusesPulled(false, {}, tr("Project not linked to Kitsu."));
    return;
  }
  m_pullProjectId = projectId;
  m_pullEpisodeId = episodeId;
  m_pullSeqName.clear();
  m_pullShotSeq.clear();
  m_pullShotName.clear();
  m_pullTtName.clear();
  // Load the roster first so task assignees resolve to display names.
  loadRosterThen(m_pullProjectId, [this]() { pullLoadSequences(); });
}

void KitsuClient::pullLoadSequences() {
  emit shotsPushProgress(tr("Reading Kitsu sequences…"));
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_pullProjectId + "/sequences"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { pullFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      // On a tvshow a sequence's parent is its episode. Keeping only our
      // episode's sequences is what makes the shot filter below work, and it
      // mirrors what the push path already does to avoid reusing a same-named
      // sequence from another episode.
      if (!m_pullEpisodeId.isEmpty() &&
          o.value("parent_id").toString() != m_pullEpisodeId)
        continue;
      m_pullSeqName.insert(o.value("id").toString(), o.value("name").toString());
    }
    pullLoadShots();
  });
}

void KitsuClient::pullLoadShots() {
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_pullProjectId + "/shots"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { pullFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o  = v.toObject();
      const QString id     = o.value("id").toString();
      const QString seqId  = o.value("parent_id").toString();
      // /shots returns the WHOLE project, every episode included. Filtering the
      // sequences alone would not be enough: a foreign shot would still land
      // here, just with an empty sequence name.
      if (!m_pullEpisodeId.isEmpty() && !m_pullSeqName.contains(seqId)) continue;
      m_pullShotName.insert(id, o.value("name").toString());
      m_pullShotSeq.insert(id, m_pullSeqName.value(seqId));
    }
    pullLoadTaskTypes();
  });
}

void KitsuClient::pullLoadTaskTypes() {
  QNetworkReply *r = m_nam->get(authGet("/api/data/task-types"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { pullFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      if (o.value("for_entity").toString() == "Shot")
        m_pullTtName.insert(o.value("id").toString(), o.value("name").toString());
    }
    pullLoadTasks();
  });
}

void KitsuClient::pullLoadTasks() {
  emit shotsPushProgress(tr("Reading Kitsu task statuses…"));
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_pullProjectId + "/tasks"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { pullFail(errorMessage(r, b)); return; }
    QVector<KitsuPullEntry> entries;
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o    = v.toObject();
      const QString shotId   = o.value("entity_id").toString();
      const QString ttName   = m_pullTtName.value(o.value("task_type_id").toString());
      if (ttName.isEmpty() || !m_pullShotName.contains(shotId)) continue;
      KitsuPullEntry e;
      e.seq         = m_pullShotSeq.value(shotId);
      e.shot        = m_pullShotName.value(shotId);
      e.kitsuShotId = shotId;
      e.taskType    = ttName;
      e.status      = toZtoryStatus(o.value("task_status_id").toString());
      for (const QJsonValue &a : o.value("assignees").toArray()) {
        const QString nm = m_personNameById.value(a.toString());
        if (!nm.isEmpty()) e.assignees.push_back(nm);
      }
      entries.push_back(e);
    }
    emit statusesPulled(true, entries,
                        tr("Pulled %1 task statuses from Kitsu.").arg(entries.size()));
  });
}

//----------------------------------------------------------------------------
// Asset sync (bidirectional) — Ztoryc <-> Kitsu, sequential async.
// Routes verified against live Zou:
//   asset-types  : GET  /api/data/asset-types
//   project assets: GET /api/data/projects/<pid>/assets
//   create asset : POST /api/data/projects/<pid>/asset-types/<atid>/assets/new
//                  body {name, description, data}
//----------------------------------------------------------------------------

QVector<KitsuAsset> KitsuClient::buildAssetsFromModel() {
  QVector<KitsuAsset> out;
  const auto &assets = ZtoryModel::instance()->assets();
  for (const Asset &a : assets) {
    if (a.name.trimmed().isEmpty()) continue;  // unnamed rows aren't real assets
    KitsuAsset ka;
    ka.type         = a.type;
    ka.name         = a.name.trimmed();
    ka.kitsuAssetId = a.kitsuAssetId;
    out.push_back(ka);
  }
  return out;
}

QVector<KitsuAssetTaskPush> KitsuClient::buildAssetTasksFromModel() {
  QVector<KitsuAssetTaskPush> out;
  ZtoryModel *m = ZtoryModel::instance();
  for (const Asset &a : m->assets()) {
    if (a.name.trimmed().isEmpty()) continue;
    // Push every task in this asset TYPE's (custom) pipeline, with its status
    // (defaults to TODO when the asset has no explicit state for it yet).
    for (const QString &tt : m->assetTaskTypesForType(a.type)) {
      KitsuAssetTaskPush p;
      p.assetType = a.type;
      p.assetName = a.name.trimmed();
      p.taskType  = tt;
      p.status    = a.tasks.value(tt).status;
      p.assignees = a.tasks.value(tt).assignees;
      // As for shots: only what changed in Ztoryc, only over the base.
      p.createOnly = !ZtoryTaskFlow::changedSinceSync(
          ZtoryTaskFlow::Entity::Asset, a.uuid, tt);
      p.expected = ZtoryTaskFlow::expectedOnServer(
          ZtoryTaskFlow::Entity::Asset, a.uuid, tt);
      p.uuid     = a.uuid;
      out.push_back(p);
    }
  }
  return out;
}

void KitsuClient::asPushFail(const QString &message) {
  emit assetsPushed(false, m_asCreated, m_asUpdated, message);
}

void KitsuClient::pushAssets(const QString &projectId,
                             const QVector<KitsuAsset> &assets,
                             const QString &episodeId) {
  if (!isLoggedIn()) { emit assetsPushed(false, 0, 0, tr("Not logged in.")); return; }
  if (projectId.isEmpty() || assets.isEmpty()) {
    emit assetsPushed(true, 0, 0, tr("No assets to push."));
    return;
  }
  m_asProjectId = projectId;
  m_asEpisodeId = episodeId;
  m_asQueue     = assets;
  m_asTypeIdByName.clear();
  m_asExisting.clear();
  m_asResolved.clear();
  m_asIndex = m_asCreated = m_asUpdated = 0;
  m_asSkipped.clear();
  m_asNotMoved.clear();
  asPushLoadTypes();
}

void KitsuClient::asPushLoadTypes() {
  emit shotsPushProgress(tr("Reading Kitsu asset types…"));
  QNetworkReply *r = m_nam->get(authGet("/api/data/asset-types"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { asPushFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      m_asTypeIdByName.insert(o.value("name").toString().toLower(),
                              o.value("id").toString());
    }
    asPushLoadAssets();
  });
}

void KitsuClient::asPushLoadAssets() {
  emit shotsPushProgress(tr("Reading existing Kitsu assets…"));
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_asProjectId + "/assets"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { asPushFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      // Matched by type+name: on a series, only among THIS episode's assets
      // and the Main Pack. Across the whole show, a character with the same
      // name in another episode would be taken for ours and linked to it.
      const QString src = o.value("source_id").toString();
      if (!m_asEpisodeId.isEmpty() && !src.isEmpty() && src != m_asEpisodeId)
        continue;
      m_asExisting.insert(o.value("entity_type_id").toString() + "/" +
                              o.value("name").toString().toLower(),
                          o.value("id").toString());
    }
    asPushProcessNext();
  });
}

void KitsuClient::asPushProcessNext() {
  while (m_asIndex < m_asQueue.size()) {
    const KitsuAsset a  = m_asQueue[m_asIndex];
    const QString typeId = m_asTypeIdByName.value(a.type.toLower());
    const QString key    = a.type + "\n" + a.name;
    // No matching Kitsu asset-type → can't create it there. Skip it, but
    // remember it: a silent skip looked like a successful sync while the asset
    // never reached Kitsu.
    if (typeId.isEmpty()) {
      m_asSkipped[a.type].append(a.name);
      ++m_asIndex;
      continue;
    }
    // Already linked, or an asset with this type+name exists → record + move on.
    QString existing = a.kitsuAssetId;
    if (existing.isEmpty())
      existing = m_asExisting.value(typeId + "/" + a.name.toLower());
    if (!existing.isEmpty()) {
      m_asResolved.insert(key, existing);
      ++m_asUpdated;
      ++m_asIndex;
      continue;
    }
    // Create it.
    emit shotsPushProgress(tr("Creating asset %1/%2 (%3)…")
                               .arg(m_asIndex + 1)
                               .arg(m_asQueue.size())
                               .arg(a.name));
    QJsonObject body;
    body["name"]        = a.name;
    body["description"] = "";
    body["data"]        = QJsonObject();
    // Created WITHOUT an episode, an asset of a series lands in the Main Pack
    // and every episode's Sync pulls it in: one episode's characters turned up in
    // another episode's tracker (Franco, 2026-09-29). `episode_id` is the field the
    // official client (gazu new_asset) sends; Zou stores it as source_id.
    if (!m_asEpisodeId.isEmpty()) body["episode_id"] = m_asEpisodeId;
    QNetworkReply *r = authPost("/api/data/projects/" + m_asProjectId +
                                    "/asset-types/" + typeId + "/assets/new",
                                QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this, r, key]() {
      r->deleteLater();
      const QByteArray b = r->readAll();
      if (r->error() != QNetworkReply::NoError) { asPushFail(errorMessage(r, b)); return; }
      const QJsonObject created = QJsonDocument::fromJson(b).object();
      const QString id          = created.value("id").toString();
      if (!id.isEmpty()) m_asResolved.insert(key, id);
      ++m_asCreated;
      ++m_asIndex;
      // Some Zou versions ignore episode_id on creation and leave the asset
      // in the Main Pack (Franco, 2026-09-29: «manette»). Then it is moved
      // with an update of source_id — the way gazu's update_asset does it —
      // before going on. A failed move is not fatal: the asset exists, it is
      // only shared; said in the Sync's warnings.
      if (!id.isEmpty() && !m_asEpisodeId.isEmpty() &&
          created.value("source_id").toString() != m_asEpisodeId) {
        QJsonObject upd;
        upd["source_id"] = m_asEpisodeId;
        QNetworkReply *ur =
            authPut("/api/data/entities/" + id,
                    QJsonDocument(upd).toJson(QJsonDocument::Compact));
        connect(ur, &QNetworkReply::finished, this, [this, ur, key]() {
          ur->deleteLater();
          const QByteArray ub = ur->readAll();
          if (ur->error() != QNetworkReply::NoError)
            m_asNotMoved << key.section('\n', 1);
          asPushProcessNext();
        });
        return;
      }
      asPushProcessNext();
    });
    return;  // resume in the callback
  }
  emit assetIdsResolved(m_asResolved);
  if (!m_asNotMoved.isEmpty()) emit assetsLeftInMainPack(m_asNotMoved);
  int skipped = 0;
  if (!m_asSkipped.isEmpty()) {
    QStringList lines;
    for (auto it = m_asSkipped.cbegin(); it != m_asSkipped.cend(); ++it) {
      skipped += it.value().size();
      lines << it.key() + ": " + it.value().join(", ");
    }
    emit assetsSkipped(lines);
  }
  QString msg = tr("Assets synced — %1 created, %2 already in Kitsu.")
                    .arg(m_asCreated)
                    .arg(m_asUpdated);
  if (skipped > 0)
    msg += " " + tr("%1 skipped (asset type missing in Kitsu).").arg(skipped);
  emit assetsPushed(true, m_asCreated, m_asUpdated, msg);
}

//----------------------------------------------------------------------------
// Push asset TASK statuses (mirror of the shot task pipeline, for_entity=Asset)
//----------------------------------------------------------------------------

void KitsuClient::atFail(const QString &message) {
  emit assetTasksPushed(false, m_atStatusesSet, message);
}

void KitsuClient::pushAssetTasks(const QString &projectId,
                                 const QVector<KitsuAssetTaskPush> &tasks) {
  if (!isLoggedIn()) { emit assetTasksPushed(false, 0, tr("Not logged in.")); return; }
  if (projectId.isEmpty() || tasks.isEmpty()) {
    emit assetTasksPushed(true, 0, tr("No asset task statuses to push."));
    return;
  }
  m_atProjectId = projectId;
  m_atQueue     = tasks;
  m_atTtIdByName.clear();
  m_atAssetTypeIdByName.clear();
  m_atAssetIds.clear();
  m_atTaskIdByKey.clear();
  m_atStatusByKey.clear();
  m_atAssigneesByKey.clear();
  m_assignQueue.clear();
  m_assignIdx = 0;
  m_atCreateList.clear();
  m_atCreateIdx = m_atApplyIdx = m_atStatusesSet = m_atUnchanged = 0;

  // Reverse status map (same six pipeline statuses as the shot push).
  rebuildStatusIdByZ();
  // Load the roster first so assignee names resolve for the add-only assign pass.
  loadRosterThen(m_atProjectId, [this]() { atLoadTaskTypes(); });
}

void KitsuClient::atLoadTaskTypes() {
  emit shotsPushProgress(tr("Loading asset task types…"));
  QNetworkReply *r = m_nam->get(authGet("/api/data/task-types"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { atFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      if (o.value("for_entity").toString() == "Asset")
        m_atTtIdByName.insert(o.value("name").toString().toLower(),
                              o.value("id").toString());
    }
    // The missing tasks are created after the existing ones are read
    // (atLoadTasks → atCreateNext), for exactly these assets.
    atLoadAssetTypes();
  });
}

// Ztoryc: the missing tasks of THESE assets, created one by one, as the shot
// push does. The «/assets/create-tasks» action it replaced, sent with an
// empty body, created the task type on EVERY asset of the project — the
// other episodes' too — and since the Sync (2026-09-27) at every Sync.
void KitsuClient::atCreateNext() {
  if (m_atCreateIdx >= m_atCreateList.size()) { atApplyNext(); return; }
  if (m_statusIdByZ.value(static_cast<int>(TaskStatus::Todo)).isEmpty()) {
    atFail(tr("Kitsu has no \"todo\" task status: cannot create tasks."));
    return;
  }
  const KitsuAssetTaskPush &t = m_atQueue[m_atCreateList[m_atCreateIdx]];
  const QString ttId    = m_atTtIdByName.value(normalizeTaskType(t.taskType));
  const QString typeId  = m_atAssetTypeIdByName.value(t.assetType.toLower());
  const QString assetId = m_atAssetIds.value(typeId + "/" + t.assetName.toLower());
  emit shotsPushProgress(tr("Creating asset tasks (%1/%2)…")
                             .arg(m_atCreateIdx + 1)
                             .arg(m_atCreateList.size()));
  QNetworkReply *r = authPost("/api/data/tasks",
                              newTaskBody(m_atProjectId, assetId, ttId));
  const int qi = m_atCreateList[m_atCreateIdx];
  connect(r, &QNetworkReply::finished, this, [this, r, assetId, ttId, qi]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { atFail(errorMessage(r, b)); return; }
    // As for shots: a created task starts at Todo, and so does its base.
    m_atQueue[qi].expected   = TaskStatus::Todo;
    m_atQueue[qi].createOnly = m_atQueue[qi].status == TaskStatus::Todo;
    ZtoryTaskFlow::markSynced(ZtoryTaskFlow::Entity::Asset, m_atQueue[qi].uuid,
                              m_atQueue[qi].taskType, TaskStatus::Todo);
    const QJsonObject o = QJsonDocument::fromJson(b).object();
    const QString key   = assetId + "/" + ttId;
    m_atTaskIdByKey.insert(key, o.value("id").toString());
    m_atStatusByKey.insert(key, o.value("task_status_id").toString());
    m_atAssigneesByKey.insert(key, QSet<QString>());
    ++m_atCreateIdx;
    atCreateNext();
  });
}

void KitsuClient::atLoadAssetTypes() {
  QNetworkReply *r = m_nam->get(authGet("/api/data/asset-types"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { atFail(errorMessage(r, b)); return; }
    m_assetTypeTaskTypes.clear();
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      m_atAssetTypeIdByName.insert(o.value("name").toString().toLower(),
                                   o.value("id").toString());
      readAssetTypeLinks(o);
    }
    atLoadAssets();
  });
}

void KitsuClient::atLoadAssets() {
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_atProjectId + "/assets"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { atFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      m_atAssetIds.insert(o.value("entity_type_id").toString() + "/" +
                              o.value("name").toString().toLower(),
                          o.value("id").toString());
    }
    atLoadTasks();
  });
}

void KitsuClient::atLoadTasks() {
  emit shotsPushProgress(tr("Reading existing asset tasks…"));
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_atProjectId + "/tasks"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { atFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      const QString key = o.value("entity_id").toString() + "/" +
                          o.value("task_type_id").toString();
      m_atTaskIdByKey.insert(key, o.value("id").toString());
      m_atStatusByKey.insert(key, o.value("task_status_id").toString());
      QSet<QString> assignees;
      for (const QJsonValue &a : o.value("assignees").toArray())
        assignees.insert(a.toString());
      m_atAssigneesByKey.insert(key, assignees);
    }
    // The tasks this push needs and Kitsu lacks: exactly these assets,
    // exactly these types.
    QSet<QString> queued;
    for (int i = 0; i < m_atQueue.size(); ++i) {
      const KitsuAssetTaskPush &t = m_atQueue[i];
      const QString ttId    = m_atTtIdByName.value(normalizeTaskType(t.taskType));
      const QString typeId  = m_atAssetTypeIdByName.value(t.assetType.toLower());
      const QString assetId = m_atAssetIds.value(typeId + "/" + t.assetName.toLower());
      if (ttId.isEmpty() || assetId.isEmpty()) continue;
      // Never create a task Kitsu would hide (type not linked to the asset's).
      if (!taskTypeLinked(t.assetType, ttId)) continue;
      const QString key = assetId + "/" + ttId;
      if (m_atTaskIdByKey.contains(key) || queued.contains(key)) continue;
      queued.insert(key);
      m_atCreateList.push_back(i);
    }
    atCreateNext();
  });
}

void KitsuClient::atApplyNext() {
  while (m_atApplyIdx < m_atQueue.size()) {
    const KitsuAssetTaskPush t = m_atQueue[m_atApplyIdx];
    const QString ttId      = m_atTtIdByName.value(normalizeTaskType(t.taskType));
    const QString typeId    = m_atAssetTypeIdByName.value(t.assetType.toLower());
    const QString assetId   = m_atAssetIds.value(typeId + "/" + t.assetName.toLower());
    const QString taskId    = m_atTaskIdByKey.value(assetId + "/" + ttId);
    const QString statusId  = statusIdFor(t.status);
    // Skip anything we couldn't resolve (unknown task-type, asset or status),
    // and what did not change in Ztoryc since the last sync.
    if (t.createOnly || ttId.isEmpty() || assetId.isEmpty() ||
        taskId.isEmpty() || statusId.isEmpty() ||
        !taskTypeLinked(t.assetType, ttId)) {
      ++m_atApplyIdx;
      continue;
    }
    // Kitsu moved on since the last sync, or has a status Ztoryc does not
    // know: its status is kept.
    const QString remoteId = m_atStatusByKey.value(assetId + "/" + ttId);
    if (remoteId != statusId &&
        (!isPipelineStatusId(remoteId) || toZtoryStatus(remoteId) != t.expected)) {
      ++m_atUnchanged;
      ++m_atApplyIdx;
      continue;
    }
    // Already at the target status in Kitsu → don't re-comment (would spam the
    // activity feed and notifications); only touch what actually changed.
    if (remoteId == statusId) {
      ZtoryTaskFlow::markSynced(ZtoryTaskFlow::Entity::Asset, t.uuid,
                                t.taskType, t.status);
      ++m_atUnchanged;
      ++m_atApplyIdx;
      continue;
    }
    emit shotsPushProgress(tr("Setting asset task status (%1/%2)…")
                               .arg(m_atApplyIdx + 1)
                               .arg(m_atQueue.size()));
    QJsonObject body;
    body["task_status_id"] = statusId;
    body["comment"]        = "Status synced from Ztoryc";
    QNetworkReply *r = authPost("/api/actions/tasks/" + taskId + "/comment",
                                QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this, r, t]() {
      r->deleteLater();
      const QByteArray b = r->readAll();
      if (r->error() != QNetworkReply::NoError) { atFail(errorMessage(r, b)); return; }
      ZtoryTaskFlow::markSynced(ZtoryTaskFlow::Entity::Asset, t.uuid, t.taskType,
                                t.status);
      ZtoryModel::instance()->saveProjectDb();
      ++m_atStatusesSet;
      ++m_atApplyIdx;
      atApplyNext();
    });
    return;  // resume in the reply callback
  }
  // Statuses done — queue the add-only assignee updates, then report.
  const int statusesSet = m_atStatusesSet, unchanged = m_atUnchanged;
  m_assignQueue.clear();
  m_assignIdx = 0;
  QSet<QString> skippedNames;  // assignees that aren't on the Kitsu team
  for (const KitsuAssetTaskPush &t : m_atQueue) {
    if (t.assignees.isEmpty()) continue;
    const QString ttId    = m_atTtIdByName.value(normalizeTaskType(t.taskType));
    const QString typeId  = m_atAssetTypeIdByName.value(t.assetType.toLower());
    const QString assetId = m_atAssetIds.value(typeId + "/" + t.assetName.toLower());
    const QString taskId  = m_atTaskIdByKey.value(assetId + "/" + ttId);
    if (taskId.isEmpty()) continue;
    if (!taskTypeLinked(t.assetType, ttId)) continue;  // hidden on Kitsu
    QSet<QString> &have = m_atAssigneesByKey[assetId + "/" + ttId];
    for (const QString &name : t.assignees) {
      const QString pid = m_personIdByName.value(name.trimmed().toLower());
      if (pid.isEmpty() || !m_teamPersonIds.contains(pid)) {
        if (!name.trimmed().isEmpty()) skippedNames.insert(name.trimmed());
        continue;
      }
      if (have.contains(pid)) continue;
      have.insert(pid);
      m_assignQueue.push_back({taskId, pid});
    }
  }
  const int assigned = m_assignQueue.size(), skipped = skippedNames.size();
  // Changed in Ztoryc on a task type Kitsu does not link to that asset type:
  // not sent, and said — silence would look like it had worked.
  int unlinked = 0;
  for (const KitsuAssetTaskPush &t : m_atQueue)
    if (!t.createOnly &&
        !taskTypeLinked(t.assetType,
                        m_atTtIdByName.value(normalizeTaskType(t.taskType))))
      ++unlinked;
  if (unlinked > 0) emit assetTasksUnlinked(unlinked);
  m_assignOnDone = [this, statusesSet, unchanged, assigned, skipped, unlinked]() {
    QString msg = unchanged > 0
                      ? tr("Done — %1 asset task statuses changed, %2 unchanged.")
                            .arg(statusesSet).arg(unchanged)
                      : tr("Done — %1 asset task statuses set in Kitsu.").arg(statusesSet);
    if (assigned > 0) msg += tr("  %1 people assigned.").arg(assigned);
    if (skipped > 0)  msg += tr("  %1 not in team (skipped).").arg(skipped);
    if (unlinked > 0)
      msg += tr("  %1 not sent: task type not linked to the asset type on Kitsu.")
                 .arg(unlinked);
    emit assetTasksPushed(true, statusesSet, msg);
  };
  assignRun();
}

void KitsuClient::asPullFail(const QString &message) {
  emit assetsPulled(false, {}, message);
}

void KitsuClient::pullAssets(const QString &projectId,
                             const QString &episodeId) {
  if (!isLoggedIn()) { emit assetsPulled(false, {}, tr("Not logged in.")); return; }
  if (projectId.isEmpty()) { emit assetsPulled(false, {}, tr("No project.")); return; }
  m_asProjectId     = projectId;
  m_entityEpisodeId = episodeId;
  m_asTypeNameById.clear();
  asPullLoadTypes();
}

void KitsuClient::asPullLoadTypes() {
  emit shotsPushProgress(tr("Reading Kitsu asset types…"));
  QNetworkReply *r = m_nam->get(authGet("/api/data/asset-types"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { asPullFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      m_asTypeNameById.insert(o.value("id").toString(), o.value("name").toString());
    }
    asPullLoadAssets();
  });
}

void KitsuClient::pullBreakdown(const QString &projectId,
                                const QString &episodeId) {
  if (!isLoggedIn()) { emit breakdownPulled(false, {}, tr("Not logged in.")); return; }
  if (projectId.isEmpty()) {
    emit breakdownPulled(false, {}, tr("Project not linked to Kitsu."));
    return;
  }
  // One call for the whole episode. Asking shot by shot would be dozens of
  // round trips for the same data.
  const QString path =
      episodeId.isEmpty()
          ? "/api/data/projects/" + projectId + "/sequences/all/casting"
          : "/api/data/projects/" + projectId + "/episodes/" + episodeId +
                "/sequences/all/casting";

  QNetworkReply *r = m_nam->get(authGet(path));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) {
      emit breakdownPulled(false, {}, errorMessage(r, b));
      return;
    }
    // Shape: { "<shot id>": [ {asset_id, asset_name, asset_type_name,
    //                          nb_occurences, label}, … ], … }
    QVector<KitsuCastingEntry> out;
    const QJsonObject byShot = QJsonDocument::fromJson(b).object();
    for (auto it = byShot.constBegin(); it != byShot.constEnd(); ++it) {
      const QString shotId = it.key();
      for (const QJsonValue &v : it.value().toArray()) {
        const QJsonObject o = v.toObject();
        KitsuCastingEntry e;
        e.kitsuShotId  = shotId;
        e.kitsuAssetId = o.value("asset_id").toString();
        e.assetName    = o.value("asset_name").toString();
        e.assetType    = o.value("asset_type_name").toString();
        // Kitsu spells it «occurences», with one r. Reading our own spelling
        // here would silently give 0 on every line.
        e.nbOccurrences = o.value("nb_occurences").toInt(1);
        e.label         = o.value("label").toString();
        if (!e.kitsuAssetId.isEmpty()) out.push_back(e);
      }
    }
    emit breakdownPulled(
        true, out,
        tr("Pulled %1 breakdown links from Kitsu.").arg(out.size()));
  });
}

void KitsuClient::pushCasting(const QString &projectId,
                              const QVector<KitsuCastingPush> &shots) {
  if (!isLoggedIn() || projectId.isEmpty()) {
    emit castingPushed(false, {}, tr("Not connected to a Kitsu project."));
    return;
  }
  // One shot at a time, chained on the replies: the state travels with the
  // chain instead of living in members that a second push would overwrite.
  struct Chain {
    QString                   projectId;
    QVector<KitsuCastingPush> shots;
    int                       next = 0;
    QStringList               written;
    QString                   firstError;
  };
  auto chain       = std::make_shared<Chain>();
  chain->projectId = projectId;
  chain->shots     = shots;
  auto step        = std::make_shared<std::function<void()>>();
  // The lambda holds only a weak reference to itself: a strong one would be
  // a cycle, and the chain would never be freed.
  std::weak_ptr<std::function<void()>> weakStep = step;
  *step = [this, chain, weakStep]() {
    if (chain->next >= chain->shots.size()) {
      emit castingPushed(chain->firstError.isEmpty(), chain->written,
                         chain->firstError.isEmpty()
                             ? tr("Breakdown written for %1 shot(s).")
                                   .arg(chain->written.size())
                             : chain->firstError);
      return;
    }
    const KitsuCastingPush &sh = chain->shots.at(chain->next++);
    // Zou reads a bare JSON array, and spells it «nb_occurences».
    QJsonArray body;
    for (const KitsuCastingEntry &e : sh.entries) {
      QJsonObject o;
      o["asset_id"]      = e.kitsuAssetId;
      o["nb_occurences"] = e.nbOccurrences;
      o["label"]         = e.label;
      body.append(o);
    }
    QNetworkReply *r = authPut("/api/data/projects/" + chain->projectId +
                                   "/entities/" + sh.kitsuShotId + "/casting",
                               QJsonDocument(body).toJson(QJsonDocument::Compact));
    const QString shotId = sh.kitsuShotId;
    auto self = weakStep.lock();
    connect(r, &QNetworkReply::finished, this,
            [this, r, chain, shotId, self]() {
      r->deleteLater();
      const QByteArray b = r->readAll();
      if (r->error() == QNetworkReply::NoError)
        chain->written << shotId;
      else if (chain->firstError.isEmpty())
        chain->firstError = errorMessage(r, b);
      emit castingProgress(chain->next, chain->shots.size());
      (*self)();
    });
  };
  (*step)();
}

bool KitsuClient::episodeScoped(const QJsonObject &entity) const {
  // No episode bound (or not a tvshow): everything belongs here, as before.
  if (m_entityEpisodeId.isEmpty()) return true;
  // In a Kitsu tvshow an asset can belong to ONE episode, and then its
  // source_id points at that episode. Measured on a real six-episode show: all
  // 145 assets are episode-scoped this way, 40/39/38/28 across four episodes —
  // which is why pulling by project alone dragged in the other episodes'.
  const QString src = entity.value("source_id").toString();
  // An asset with no episode is the show's, shared by every episode: it must
  // keep arriving, or a common prop would vanish from all of them at once.
  if (src.isEmpty()) return true;
  return src == m_entityEpisodeId;
}

void KitsuClient::asPullLoadAssets() {
  emit shotsPushProgress(tr("Reading Kitsu assets…"));
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_asProjectId + "/assets"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { asPullFail(errorMessage(r, b)); return; }
    QVector<KitsuAsset> out;
    QStringList elsewhere;  // in another episode of this series
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      if (!episodeScoped(o)) {
        elsewhere << o.value("id").toString();
        continue;
      }
      KitsuAsset a;
      a.kitsuAssetId = o.value("id").toString();
      a.name         = o.value("name").toString();
      a.type = m_asTypeNameById.value(o.value("entity_type_id").toString());
      a.mainPack = !m_entityEpisodeId.isEmpty() &&
                   o.value("source_id").toString().isEmpty();
      if (!a.name.isEmpty()) out.push_back(a);
    }
    if (!m_entityEpisodeId.isEmpty()) emit assetsInOtherEpisodes(elsewhere);
    emit assetsPulled(true, out,
                      tr("Pulled %1 assets from Kitsu.").arg(out.size()));
  });
}

//----------------------------------------------------------------------------
// Asset-task status pull (review sync) — Kitsu -> Ztoryc, mirror of
// pullStatuses for the asset entity. Chain: asset-types -> assets ->
// task-types(Asset) -> tasks -> emit assetStatusesPulled.
//----------------------------------------------------------------------------

void KitsuClient::apFail(const QString &message) {
  emit assetStatusesPulled(false, {}, message);
}

void KitsuClient::pullAssetStatuses(const QString &projectId,
                                    const QString &episodeId) {
  if (!isLoggedIn()) { emit assetStatusesPulled(false, {}, tr("Not logged in.")); return; }
  if (projectId.isEmpty()) {
    emit assetStatusesPulled(false, {}, tr("Project not linked to Kitsu."));
    return;
  }
  m_apProjectId     = projectId;
  m_entityEpisodeId = episodeId;
  m_apAssetTypeName.clear();
  m_apAssetName.clear();
  m_apAssetType.clear();
  m_apTtName.clear();
  // Load the roster first so task assignees resolve to display names.
  loadRosterThen(m_apProjectId, [this]() { apLoadTypes(); });
}

void KitsuClient::apLoadTypes() {
  emit shotsPushProgress(tr("Reading Kitsu asset types…"));
  QNetworkReply *r = m_nam->get(authGet("/api/data/asset-types"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { apFail(errorMessage(r, b)); return; }
    m_assetTypeTaskTypes.clear();
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      m_apAssetTypeName.insert(o.value("id").toString(), o.value("name").toString());
      readAssetTypeLinks(o);
    }
    apLoadAssets();
  });
}

void KitsuClient::apLoadAssets() {
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_apProjectId + "/assets"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { apFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      if (!episodeScoped(o)) continue;
      const QString id    = o.value("id").toString();
      m_apAssetName.insert(id, o.value("name").toString());
      m_apAssetType.insert(
          id, m_apAssetTypeName.value(o.value("entity_type_id").toString()));
    }
    apLoadTaskTypes();
  });
}

void KitsuClient::apLoadTaskTypes() {
  QNetworkReply *r = m_nam->get(authGet("/api/data/task-types"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { apFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      if (o.value("for_entity").toString() == "Asset")
        m_apTtName.insert(o.value("id").toString(), o.value("name").toString());
    }
    apLoadTasks();
  });
}

void KitsuClient::apLoadTasks() {
  emit shotsPushProgress(tr("Reading Kitsu asset task statuses…"));
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_apProjectId + "/tasks"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { apFail(errorMessage(r, b)); return; }
    QVector<KitsuAssetStatusEntry> entries;
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o    = v.toObject();
      const QString assetId  = o.value("entity_id").toString();
      const QString ttName   = m_apTtName.value(o.value("task_type_id").toString());
      if (ttName.isEmpty() || !m_apAssetName.contains(assetId)) continue;
      // A task Kitsu itself hides: its type is not linked to the asset's
      // type. Adopting it put Rigging/Modeling back in the Prop pipeline at
      // every Sync (a real episode, 2026-09-27: orphan tasks of the old create-tasks).
      if (!taskTypeLinked(m_apAssetType.value(assetId),
                          o.value("task_type_id").toString()))
        continue;
      KitsuAssetStatusEntry e;
      e.assetType    = m_apAssetType.value(assetId);
      e.assetName    = m_apAssetName.value(assetId);
      e.kitsuAssetId = assetId;
      e.taskType     = ttName;
      e.status       = toZtoryStatus(o.value("task_status_id").toString());
      for (const QJsonValue &a : o.value("assignees").toArray()) {
        const QString nm = m_personNameById.value(a.toString());
        if (!nm.isEmpty()) e.assignees.push_back(nm);
      }
      entries.push_back(e);
    }
    emit assetStatusesPulled(
        true, entries,
        tr("Pulled %1 asset task statuses from Kitsu.").arg(entries.size()));
  });
}

//----------------------------------------------------------------------------
// Team / assignees (persons) — shared by the status pulls (to resolve assignee
// names) and by the assign pass (to resolve names -> person ids and to restrict
// assignments to the project team). Routes verified against live Zou:
//   persons     : GET /api/data/persons                     (id, full_name, …)
//   project team: GET /api/data/projects/<pid>?relations=true  ("team" = [id,…])
//   assign      : PUT /api/actions/tasks/<tid>/assign  {person_id}  (add-only)
// Note: the project's `team` is a many-to-many relationship, so it only appears
// with ?relations=true — without it the field is absent and the team looks empty.
//----------------------------------------------------------------------------

void KitsuClient::loadRosterThen(const QString &projectId,
                                 std::function<void()> next) {
  emit shotsPushProgress(tr("Reading Kitsu team…"));
  m_teamProjectId = projectId;
  QNetworkReply *r = m_nam->get(authGet("/api/data/persons"));
  connect(r, &QNetworkReply::finished, this, [this, r, projectId, next]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    m_personNameById.clear();
    m_personIdByName.clear();
    m_rosterError.clear();
    // Persons are best-effort context: if the roster can't be read (restricted
    // account) we still continue — the project GET below carries its own error.
    // But we REMEMBER why: swallowing it produced the misleading «their names
    // couldn't be read (persons endpoint restricted?)», a guess dressed up as a
    // diagnosis. Whoever reports the failure can now say what actually broke.
    if (r->error() != QNetworkReply::NoError) {
      m_rosterError = errorMessage(r, b);
    } else {
      for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
        const QJsonObject o = v.toObject();
        const QString id    = o.value("id").toString();
        QString name        = o.value("full_name").toString();
        if (name.trimmed().isEmpty())
          name = (o.value("first_name").toString() + " " +
                  o.value("last_name").toString()).trimmed();
        if (name.trimmed().isEmpty()) name = o.value("email").toString();
        // Last resort: a person with no readable name would silently vanish
        // from the team, which is worse than showing a placeholder the user can
        // recognise as «something is off with this account».
        if (!id.isEmpty() && name.trimmed().isEmpty())
          name = tr("(unnamed person %1)").arg(id.left(8));
        if (id.isEmpty() || name.isEmpty()) continue;
        m_personNameById.insert(id, name);
        m_personIdByName.insert(name.toLower(), id);
      }
    }
    // Now resolve which of them are on THIS project's team (assignable set).
    QNetworkReply *pr = m_nam->get(
        authGet("/api/data/projects/" + projectId + "?relations=true"));
    connect(pr, &QNetworkReply::finished, this, [this, pr, next]() {
      pr->deleteLater();
      const QByteArray pb = pr->readAll();
      m_teamPersonIds.clear();
      if (pr->error() == QNetworkReply::NoError) {
        const QJsonObject proj = QJsonDocument::fromJson(pb).object();
        for (const QJsonValue &v : proj.value("team").toArray())
          m_teamPersonIds.insert(v.toString());
      }
      if (next) next();
    });
  });
}

void KitsuClient::teamFail(const QString &message) {
  emit teamPulled(false, {}, message);
}

void KitsuClient::pullTeam(const QString &projectId) {
  if (!isLoggedIn()) { emit teamPulled(false, {}, tr("Not logged in.")); return; }
  if (projectId.isEmpty()) {
    emit teamPulled(false, {}, tr("Project not linked to Kitsu."));
    return;
  }
  loadRosterThen(projectId, [this]() {
    QVector<KitsuPerson> team;
    for (const QString &id : m_teamPersonIds) {
      const QString nm = m_personNameById.value(id);
      if (!nm.isEmpty()) team.push_back({id, nm});
    }
    // Distinguish the failure modes so a stuck-looking pull is diagnosable.
    if (m_teamPersonIds.isEmpty())
      emit teamPulled(true, team, tr("Kitsu project has no team members."));
    else if (team.isEmpty())
      emit teamPulled(false, team,
                      m_rosterError.isEmpty()
                          ? tr("Team has %1 members but none of their ids match "
                               "the %2 persons read from Kitsu.")
                                .arg(m_teamPersonIds.size())
                                .arg(m_personNameById.size())
                          : tr("Team has %1 members but the persons list "
                               "failed: %2")
                                .arg(m_teamPersonIds.size())
                                .arg(m_rosterError));
    else
      emit teamPulled(true, team,
                      tr("Pulled %1 team members from Kitsu.").arg(team.size()));
  });
}

void KitsuClient::assignRun() {
  if (m_assignIdx >= m_assignQueue.size()) {
    if (m_assignOnDone) m_assignOnDone();
    return;
  }
  const QString taskId   = m_assignQueue[m_assignIdx].first;
  const QString personId = m_assignQueue[m_assignIdx].second;
  emit shotsPushProgress(tr("Assigning people (%1/%2)…")
                             .arg(m_assignIdx + 1)
                             .arg(m_assignQueue.size()));
  QJsonObject body;
  body["person_id"] = personId;
  QNetworkReply *r = authPut("/api/actions/tasks/" + taskId + "/assign",
                             QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    r->readAll();
    // Assignment is best-effort (add-only); a failure on one person shouldn't
    // abort the whole push, so we just move on.
    ++m_assignIdx;
    assignRun();
  });
}

//----------------------------------------------------------------------------
// Preview upload (Phase 4) — Ztoryc -> Kitsu, sequential async.
// Per entry the Zou contract is three POSTs (verified live):
//   1. /api/actions/tasks/<task>/comment {task_status_id, comment} -> comment id
//   2. /api/actions/tasks/<task>/comments/<cid>/add-preview {}     -> preview id
//   3. /api/pictures/preview-files/<pid>  (multipart file=...)     -> uploads it
//----------------------------------------------------------------------------

void KitsuClient::uplFail(const QString &message) {
  emit previewsUploaded(false, m_uplDone, message);
}

QVector<KitsuPreviewUpload> KitsuClient::buildUploadsFromFolder(
    const QString &dir, int &outUnmatched, int &outNoId) {
  outUnmatched = outNoId = 0;
  QVector<KitsuPreviewUpload> uploads;
  ZtoryModel *m = ZtoryModel::instance();
  const auto &pshots = m->projectShots();
  if (dir.isEmpty() || pshots.empty()) return uploads;

  static const QStringList kExts =
      {"mp4", "mov", "avi", "webm", "gif", "mkv", "m4v"};
  QStringList filters;
  for (const QString &e : kExts) filters << ("*." + e);
  const QFileInfoList files =
      QDir(dir).entryInfoList(filters, QDir::Files, QDir::Name);

  for (const QFileInfo &fi : files) {
    const QString base       = fi.completeBaseName();
    // Match the shot whose label is the LONGEST one contained in the file name
    // (so "scene_SH010.mp4" picks SH010, not SH01).
    const ProjectShot *match = nullptr;
    int bestLen              = 0;
    for (const ProjectShot &ps : pshots) {
      const QString label = ps.label.trimmed();
      if (label.isEmpty()) continue;
      if (base.contains(label, Qt::CaseInsensitive) && label.length() > bestLen) {
        match   = &ps;
        bestLen = label.length();
      }
    }
    if (!match) { ++outUnmatched; continue; }
    if (match->kitsuShotId.isEmpty()) { ++outNoId; continue; }
    // Detect the task from the {TASK} short code in the name, limited to this
    // shot's technique; default Storyboard for board/animatic previews.
    QString task    = ZtoryModel::kStoryboardTask;
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
    u.uuid        = match->uuid;
    u.shot        = match->label.trimmed();
    u.taskType    = task;
    u.filePath    = fi.absoluteFilePath();
    u.status      = TaskStatus::Wfa;
    uploads.push_back(u);
  }
  return uploads;
}

QVector<KitsuShotPush> KitsuClient::buildShotPushFromProject(
    int handles, QVector<KitsuTaskPush> &outTasks, int &outSkipped) {
  outTasks.clear();
  outSkipped = 0;
  QVector<KitsuShotPush> shots;
  ZtoryModel *m = ZtoryModel::instance();
  const QString kDefaultSeq = "SQ01";  // Kitsu shots must live under a sequence
  const auto frameRanges = m->projectShotFrameRanges();  // cumulative in/out
  const auto &pshots = m->projectShots();
  for (size_t i = 0; i < pshots.size(); i++) {
    const ProjectShot &ps = pshots[i];
    if (ps.label.trimmed().isEmpty()) { ++outSkipped; continue; }
    KitsuShotPush s;
    s.seq         = ps.seq.trimmed().isEmpty() ? kDefaultSeq : ps.seq.trimmed();
    s.name        = ps.label.trimmed();
    s.frameIn     = qMax(1, frameRanges[i].first - handles);
    s.frameOut    = frameRanges[i].second + handles;
    s.nbFrames    = s.frameOut - s.frameIn + 1;
    s.kitsuShotId = ps.kitsuShotId;  // rename-proof update when known
    shots.push_back(s);
    // Every task of the shot's WORKFLOW goes up, not only the ones that have a
    // state here: a fresh shot has Storyboard and Layout at most, and Kitsu
    // ended up with a single Storyboard column while the tracker showed the
    // whole pipeline. The untouched ones are created, never given a status.
    QStringList types = m->taskTypesForProjectShot(ps);
    for (auto it = ps.tasks.constBegin(); it != ps.tasks.constEnd(); ++it)
      if (!types.contains(it.key())) types << it.key();
    for (int ti = 0; ti < types.size(); ++ti) {
      const QString &tt = types[ti];
      KitsuTaskPush tp;
      tp.seq = s.seq; tp.shot = s.name; tp.taskType = tt;
      tp.order = ti + 1;
      tp.kitsuShotId = ps.kitsuShotId;
      tp.uuid        = ps.uuid;
      auto it = ps.tasks.constFind(tt);
      if (it != ps.tasks.constEnd()) {
        tp.status    = it.value().status;
        tp.assignees = it.value().assignees;
        // Only what changed in Ztoryc since the last sync is sent; the rest
        // is Kitsu's to say (the push used to undo a supervisor's Done).
        tp.createOnly = !ZtoryTaskFlow::changedSinceSync(
            ZtoryTaskFlow::Entity::Shot, ps.uuid, tt);
        tp.expected = ZtoryTaskFlow::expectedOnServer(
            ZtoryTaskFlow::Entity::Shot, ps.uuid, tt);
      } else
        tp.createOnly = true;
      outTasks.push_back(tp);
    }
  }
  return shots;
}

void KitsuClient::uploadPreviews(const QString &projectId,
                                 const QVector<KitsuPreviewUpload> &uploads) {
  if (!isLoggedIn()) { emit previewsUploaded(false, 0, tr("Not logged in.")); return; }
  if (projectId.isEmpty() || uploads.isEmpty()) {
    emit previewsUploaded(true, 0, tr("No previews to upload."));
    return;
  }
  m_uplProjectId = projectId;
  m_uplQueue     = uploads;
  m_uploadBase.clear();  // resolved by uplProbeLocalThenRun() below
  m_ttIdByName.clear();
  m_uplTaskIdByKey.clear();
  m_uplTtCreate.clear();
  m_uplTtIdx = m_uplIndex = m_uplDone = 0;

  // Reverse status map (Ztoryc TaskStatus -> Kitsu status id), same six pipeline
  // short names used by the task push, so Wfa/Done/etc. resolve correctly.
  rebuildStatusIdByZ();
  uplProbeLocalThenRun();
}

void KitsuClient::uplProbeLocalThenRun() {
  // No LAN override configured → upload straight to the primary URL.
  if (m_localUrl.isEmpty()) { uplLoadTaskTypes(); return; }

  emit shotsPushProgress(tr("Checking local upload endpoint…"));
  QNetworkRequest req((QUrl(m_localUrl + "/api/")));
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  req.setTransferTimeout(4000);  // fail fast when the LAN box isn't around
  QNetworkReply *r = m_nam->get(req);
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QNetworkReply::NetworkError e = r->error();
    // "Reachable" means the host answered at all: even a 401/403/404 proves the
    // instance is up on the LAN. Only connection-level failures (refused, host
    // not found, timeout) fall back to the remote URL.
    const bool reachable = e == QNetworkReply::NoError ||
                           e == QNetworkReply::AuthenticationRequiredError ||
                           e == QNetworkReply::ContentAccessDenied ||
                           e == QNetworkReply::ContentNotFoundError ||
                           e == QNetworkReply::ContentOperationNotPermittedError;
    if (reachable) {
      m_uploadBase = m_localUrl;
      emit shotsPushProgress(
          tr("Uploading via local endpoint (%1)…").arg(m_localUrl));
    } else {
      m_uploadBase.clear();  // → uploadBase() yields m_baseUrl
      emit shotsPushProgress(
          tr("Local endpoint unreachable — uploading via %1.").arg(m_baseUrl));
    }
    uplLoadTaskTypes();
  });
}

void KitsuClient::uplLoadTaskTypes() {
  emit shotsPushProgress(tr("Loading task types…"));
  QNetworkReply *r = m_nam->get(authGet("/api/data/task-types", uploadBase()));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { uplFail(errorMessage(r, b)); return; }
    parseShotTaskTypes(b);
    uplLoadTasks();
  });
}

void KitsuClient::uplLoadTasks() {
  emit shotsPushProgress(tr("Reading existing tasks…"));
  QNetworkReply *r = m_nam->get(
      authGet("/api/data/projects/" + m_uplProjectId + "/tasks", uploadBase()));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { uplFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      m_uplTaskIdByKey.insert(o.value("entity_id").toString() + "/" +
                                  o.value("task_type_id").toString(),
                              o.value("id").toString());
    }
    m_uplTtIdx = 0;
    uplEnsureTasks();
  });
}

void KitsuClient::uplEnsureTasks() {
  // A preview needs a task to hang on: create the missing one, on THIS shot
  // only. (This used the project-wide "create-tasks" action, which added the
  // task to every shot of the project — other episodes included.)
  // m_uplTtIdx walks m_uplQueue here.
  while (m_uplTtIdx < m_uplQueue.size()) {
    const KitsuPreviewUpload &u = m_uplQueue[m_uplTtIdx];
    const QString ttId = resolveTaskTypeId(u.taskType);
    const QString key  = u.kitsuShotId + "/" + ttId;
    if (u.kitsuShotId.isEmpty() || ttId.isEmpty() ||
        m_uplTaskIdByKey.contains(key) ||
        m_statusIdByZ.value(static_cast<int>(TaskStatus::Todo)).isEmpty()) {
      ++m_uplTtIdx;
      continue;
    }
    emit shotsPushProgress(tr("Creating task for %1…").arg(u.shot));
    QNetworkReply *r = authPost("/api/data/tasks",
                                newTaskBody(m_uplProjectId, u.kitsuShotId, ttId),
                                uploadBase());
    connect(r, &QNetworkReply::finished, this, [this, r, key]() {
      r->deleteLater();
      const QByteArray b = r->readAll();
      if (r->error() != QNetworkReply::NoError) { uplFail(errorMessage(r, b)); return; }
      m_uplTaskIdByKey.insert(key,
                              QJsonDocument::fromJson(b).object().value("id").toString());
      ++m_uplTtIdx;
      uplEnsureTasks();
    });
    return;  // resume in the callback
  }
  uplProcessNext();
}

void KitsuClient::uplProcessNext() {
  while (m_uplIndex < m_uplQueue.size()) {
    const KitsuPreviewUpload u = m_uplQueue[m_uplIndex];
    const QString ttId     = resolveTaskTypeId(u.taskType);
    const QString taskId   = m_uplTaskIdByKey.value(u.kitsuShotId + "/" + ttId);
    const QString statusId = statusIdFor(u.status);
    // Skip anything we can't resolve, or whose file is gone.
    if (u.kitsuShotId.isEmpty() || ttId.isEmpty() || taskId.isEmpty() ||
        statusId.isEmpty() || u.filePath.isEmpty() ||
        !QFileInfo::exists(u.filePath)) {
      ++m_uplIndex;
      continue;
    }
    emit shotsPushProgress(tr("Uploading preview %1/%2 (%3)…")
                               .arg(m_uplIndex + 1)
                               .arg(m_uplQueue.size())
                               .arg(u.shot));
    uplPostComment(taskId, statusId, u.filePath, u.shot);
    return;  // resume in the callback chain
  }
  emit previewsUploaded(true, m_uplDone,
                        tr("Done — %1 previews uploaded to Kitsu.").arg(m_uplDone));
}

void KitsuClient::uplPostComment(const QString &taskId, const QString &statusId,
                                 const QString &filePath, const QString &shot) {
  QJsonObject body;
  body["task_status_id"] = statusId;
  body["comment"]        = tr("Storyboard preview uploaded from Ztoryc");
  QNetworkReply *r =
      authPost("/api/actions/tasks/" + taskId + "/comment",
               QJsonDocument(body).toJson(QJsonDocument::Compact), uploadBase());
  connect(r, &QNetworkReply::finished, this, [this, r, taskId, filePath, shot]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { uplFail(errorMessage(r, b)); return; }
    const QString cid = QJsonDocument::fromJson(b).object().value("id").toString();
    if (cid.isEmpty()) { uplFail(tr("Kitsu did not return a comment id.")); return; }
    uplAddPreview(taskId, cid, filePath, shot);
  });
}

void KitsuClient::uplAddPreview(const QString &taskId, const QString &commentId,
                                const QString &filePath, const QString &shot) {
  QNetworkReply *r = authPost(
      "/api/actions/tasks/" + taskId + "/comments/" + commentId + "/add-preview",
      QByteArray("{}"), uploadBase());
  connect(r, &QNetworkReply::finished, this, [this, r, filePath]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { uplFail(errorMessage(r, b)); return; }
    const QJsonObject o = QJsonDocument::fromJson(b).object();
    // add-preview returns the created preview-file dict; id under "id", with
    // "preview_file_id" as a fallback for older Zou builds.
    QString pid = o.value("id").toString();
    if (pid.isEmpty()) pid = o.value("preview_file_id").toString();
    if (pid.isEmpty()) { uplFail(tr("Kitsu did not return a preview id.")); return; }
    uplUploadFile(pid, filePath);
  });
}

QNetworkReply *KitsuClient::postPreviewFile(const QString &previewFileId,
                                            const QString &filePath,
                                            const QString &base) {
  QFile *file = new QFile(filePath);
  if (!file->open(QIODevice::ReadOnly)) {
    delete file;
    return nullptr;
  }
  QHttpMultiPart *mp = new QHttpMultiPart(QHttpMultiPart::FormDataType);
  QHttpPart filePart;
  const QString mime =
      QMimeDatabase().mimeTypeForFile(filePath).name();  // e.g. video/mp4
  filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                     QVariant(mime.isEmpty() ? "application/octet-stream" : mime));
  filePart.setHeader(
      QNetworkRequest::ContentDispositionHeader,
      QVariant("form-data; name=\"file\"; filename=\"" +
               QFileInfo(filePath).fileName() + "\""));
  filePart.setBodyDevice(file);
  file->setParent(mp);  // file freed with the multipart
  mp->append(filePart);

  QNetworkRequest req(
      (QUrl(base + "/api/pictures/preview-files/" + previewFileId)));
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  QNetworkReply *r = m_nam->post(req, mp);
  mp->setParent(r);  // multipart (and file) freed with the reply
  return r;
}

void KitsuClient::uplUploadFile(const QString &previewFileId,
                                const QString &filePath) {
  QNetworkReply *r = postPreviewFile(previewFileId, filePath, uploadBase());
  if (!r) {
    uplFail(tr("Cannot open %1").arg(QFileInfo(filePath).fileName()));
    return;
  }
  connect(r, &QNetworkReply::finished, this,
          [this, r, previewFileId]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { uplFail(errorMessage(r, b)); return; }
    ++m_uplDone;
    // Promote this preview to the shot's cover thumbnail so the grid shows the
    // latest uploaded phase at a glance.
    uplSetMainPreview(previewFileId);
  });
}

void KitsuClient::uplSetMainPreview(const QString &previewFileId) {
  QJsonObject body;
  body["frame_number"] = 0;  // first frame of the clip as the shot thumbnail
  QNetworkReply *r = authPut(
      "/api/actions/preview-files/" + previewFileId + "/set-main-preview",
      QJsonDocument(body).toJson(QJsonDocument::Compact), uploadBase());
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    // Best-effort: the clip is already uploaded, so a failed thumbnail set must
    // not abort the batch — just move on to the next entry.
    ++m_uplIndex;
    uplProcessNext();
  });
}

//----------------------------------------------------------------------------
// Task statuses
//----------------------------------------------------------------------------

void KitsuClient::fetchTaskStatuses() {
  if (!isLoggedIn()) {
    emit networkError(tr("Not logged in."));
    return;
  }
  QNetworkRequest req((QUrl(m_baseUrl + "/api/data/task-status")));
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  QNetworkReply *reply = m_nam->get(req);
  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onTaskStatusesReply(reply); });
}

void KitsuClient::onTaskStatusesReply(QNetworkReply *reply) {
  reply->deleteLater();
  const QByteArray body = reply->readAll();
  if (reply->error() != QNetworkReply::NoError) {
    emit networkError(errorMessage(reply, body));
    return;
  }

  m_taskStatuses.clear();
  m_statusById.clear();
  const QJsonArray arr = QJsonDocument::fromJson(body).array();
  for (const QJsonValue &v : arr) {
    const KitsuTaskStatus s = parseTaskStatus(v.toObject());
    m_taskStatuses.push_back(s);
    m_statusById.insert(s.id, mapStatus(s));
  }
  emit taskStatusesFetched(m_taskStatuses);
}

//----------------------------------------------------------------------------
// Safe push of single status changes (Ztoryc, 2026-09-26)
//
// The full push writes Ztoryc's status over Kitsu's wherever they differ: run
// automatically, it would silently undo a Retake a supervisor set a minute
// ago. Here a change is written only if the task on Kitsu still has the
// status the change started from. It has its own queue, caches and state, so
// it cannot corrupt a full push, pull or upload running at the same time.
//----------------------------------------------------------------------------

void KitsuClient::setAutoPushStatus(bool on) {
  m_autoPushStatus = on;
  QSettings().setValue(kGroupAutoPush, on);
}

void KitsuClient::onTaskTransition(int entity, const QString &uuid,
                                   const QString &taskType, int from, int to,
                                   int origin) {
  if (!m_autoPushStatus || !isLoggedIn()) return;
  ZtoryModel *m = ZtoryModel::instance();
  if (!m->useKitsu() || m->kitsuProjectId().isEmpty()) return;
  QString kitsuId;
  if (entity == 0) {
    for (const ProjectShot &ps : m->projectShots())
      if (ps.uuid == uuid) { kitsuId = ps.kitsuShotId; break; }
  } else {
    for (const Asset &a : m->assets())
      if (a.uuid == uuid) { kitsuId = a.kitsuAssetId; break; }
  }
  // Not on Kitsu yet: creating it is the full push's job.
  if (kitsuId.isEmpty()) return;
  KitsuTransition t;
  t.entity        = entity;
  t.origin        = origin;
  t.uuid          = uuid;
  t.kitsuEntityId = kitsuId;
  t.taskType      = taskType;
  t.from          = static_cast<TaskStatus>(from);
  t.to            = static_cast<TaskStatus>(to);
  pushTransition(t);
}

void KitsuClient::pushTransition(const KitsuTransition &t) {
  m_trQueue.append(t);
  if (m_trBusy) return;
  // Started from the event loop, not from inside the edit that emitted the
  // change (possibly halfway through a bulk edit of the tracker).
  m_trBusy = true;
  QMetaObject::invokeMethod(this, [this]() { trNext(); }, Qt::QueuedConnection);
}

// A reply that never comes would leave the queue busy forever, silently.
static void armTimeout(QNetworkReply *r) {
  QTimer::singleShot(15000, r, [r]() {
    if (r->isRunning()) r->abort();
  });
}

void KitsuClient::trNext() {
  if (m_trQueue.isEmpty()) {
    m_trBusy = false;
    return;
  }
  m_trBusy = true;
  if (!isLoggedIn()) {
    trFinish(TrFailed, TaskStatus::Todo, tr("Not logged in to Kitsu."));
    return;
  }
  if (m_trCachesLoaded)
    trCheckTask();
  else
    trLoadCaches([this]() { trCheckTask(); },
                 [this](const QString &msg) {
                   trFinish(TrFailed, TaskStatus::Todo, msg);
                 });
}

void KitsuClient::mirrorUploadedWfa(const QVector<KitsuPreviewUpload> &uploads) {
  ZtoryModel *m = ZtoryModel::instance();
  const int shot = static_cast<int>(ZtoryTaskFlow::Entity::Shot);
  bool dirty     = false;
  for (const KitsuPreviewUpload &u : uploads) {
    if (m->taskStatusOf(shot, u.uuid, u.taskType) != TaskStatus::Wfa &&
        m->writeTaskStatus(shot, u.uuid, u.taskType, TaskStatus::Wfa))
      dirty = true;
    if (m->setTaskSynced(shot, u.uuid, u.taskType, TaskStatus::Wfa))
      dirty = true;
  }
  if (dirty) m->saveAndNotifyTasks();
}

void KitsuClient::readAssetTypeLinks(const QJsonObject &assetType) {
  QSet<QString> ids;
  for (const QJsonValue &v : assetType.value("task_types").toArray())
    ids.insert(v.isObject() ? v.toObject().value("id").toString()
                            : v.toString());
  m_assetTypeTaskTypes.insert(assetType.value("name").toString().toLower(), ids);
}

// No links for the asset type (older servers, or none set) means Kitsu shows
// every task type: then nothing is filtered.
bool KitsuClient::taskTypeLinked(const QString &assetTypeName,
                                 const QString &taskTypeId) const {
  const auto it = m_assetTypeTaskTypes.constFind(assetTypeName.toLower());
  if (it == m_assetTypeTaskTypes.constEnd() || it->isEmpty()) return true;
  return it->contains(taskTypeId);
}

bool KitsuClient::isPipelineStatusId(const QString &statusId) const {
  if (statusId.isEmpty()) return true;  // no task yet: nothing to protect
  for (auto it = m_statusIdByZ.constBegin(); it != m_statusIdByZ.constEnd(); ++it)
    if (it.value() == statusId) return true;
  return false;
}

QString KitsuClient::trTypeKey(int entity, const QString &taskType) {
  return QString(entity == 0 ? "shot/" : "asset/") + normalizeTaskType(taskType);
}

// Task types and statuses of the server, loaded once into caches of this
// machine alone (the full push clears and refills its own).
void KitsuClient::trLoadCaches(std::function<void()> onDone,
                               std::function<void(const QString &)> onFail) {
  QNetworkReply *r = m_nam->get(authGet("/api/data/task-types"));
  armTimeout(r);
  connect(r, &QNetworkReply::finished, this, [this, r, onDone, onFail]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) {
      onFail(errorMessage(r, b));
      return;
    }
    m_trTaskTypeId.clear();
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      const int kind =
          o.value("for_entity").toString() == QLatin1String("Shot") ? 0 : 1;
      m_trTaskTypeId.insert(trTypeKey(kind, o.value("name").toString()),
                            o.value("id").toString());
    }
    QNetworkReply *r2 = m_nam->get(authGet("/api/data/task-status"));
    armTimeout(r2);
    connect(r2, &QNetworkReply::finished, this, [this, r2, onDone, onFail]() {
      r2->deleteLater();
      const QByteArray b2 = r2->readAll();
      if (r2->error() != QNetworkReply::NoError) {
        onFail(errorMessage(r2, b2));
        return;
      }
      m_trStatusById.clear();
      m_trStatusIdByZ.clear();
      for (const QJsonValue &v : QJsonDocument::fromJson(b2).array()) {
        const KitsuTaskStatus st = parseTaskStatus(v.toObject());
        const TaskStatus z       = mapStatus(st);
        m_trStatusById.insert(st.id, z);
        if (isPipelineShortName(st.shortName))
          m_trStatusIdByZ.insert(static_cast<int>(z), st.id);
      }
      m_trCachesLoaded = true;
      onDone();
    });
  });
}

void KitsuClient::trCheckTask() {
  const KitsuTransition t = m_trQueue.first();
  const QString ttId = m_trTaskTypeId.value(trTypeKey(t.entity, t.taskType));
  if (ttId.isEmpty()) {
    trFinish(TrNoTask, TaskStatus::Todo,
             tr("Task type «%1» is not on Kitsu.").arg(t.taskType));
    return;
  }
  QNetworkReply *r = m_nam->get(authGet("/api/data/tasks?entity_id=" +
                                        t.kitsuEntityId +
                                        "&task_type_id=" + ttId));
  armTimeout(r);
  connect(r, &QNetworkReply::finished, this, [this, r, t]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) {
      trFinish(TrFailed, TaskStatus::Todo, errorMessage(r, b));
      return;
    }
    const QJsonArray arr = QJsonDocument::fromJson(b).array();
    if (arr.isEmpty()) {
      trFinish(TrNoTask, TaskStatus::Todo,
               tr("No «%1» task on Kitsu for this entity.").arg(t.taskType));
      return;
    }
    const QJsonObject task = arr.first().toObject();
    const QString statusIdNow = task.value("task_status_id").toString();
    // A status created on the server after the caches were loaded: reading
    // it as Todo would let a Todo→X change overwrite it. Reload next time.
    if (!m_trStatusById.contains(statusIdNow)) {
      m_trCachesLoaded = false;
      trFinish(TrFailed, TaskStatus::Todo,
               tr("Unknown status on Kitsu: change not sent."));
      return;
    }
    // Compared as Ztoryc statuses, not as ids: «approved» and «done» are both
    // Done here, and must not count as a difference.
    const TaskStatus cur = m_trStatusById.value(statusIdNow);
    if (cur == t.to) {
      trFinish(TrAlreadyThere, cur, QString());
      return;
    }
    if (cur != t.from) {
      trFinish(TrConflict, cur,
               tr("«%1» was changed on Kitsu meanwhile: Kitsu's status kept.")
                   .arg(t.taskType));
      return;
    }
    const QString statusId = m_trStatusIdByZ.value(static_cast<int>(t.to));
    if (statusId.isEmpty()) {
      trFinish(TrFailed, cur, tr("Kitsu has no matching status."));
      return;
    }
    QJsonObject body;
    body["task_status_id"] = statusId;
    body["comment"]        = "Status from Ztoryc";
    QNetworkReply *p =
        authPost("/api/actions/tasks/" + task.value("id").toString() +
                     "/comment",
                 QJsonDocument(body).toJson(QJsonDocument::Compact));
    armTimeout(p);
    connect(p, &QNetworkReply::finished, this, [this, p, cur]() {
      p->deleteLater();
      const QByteArray pb = p->readAll();
      if (p->error() != QNetworkReply::NoError) {
        trFinish(TrFailed, cur, errorMessage(p, pb));
        return;
      }
      trFinish(TrPushed, cur, QString());
    });
  });
}

// «SH010 · Layout» / «BRONTOLO · Rigging»: what the message is about.
static QString transitionLabel(const KitsuTransition &t) {
  const ZtoryModel *m = ZtoryModel::instance();
  QString name;
  if (t.entity == 0) {
    for (const ProjectShot &ps : m->projectShots())
      if (ps.uuid == t.uuid) { name = ps.label; break; }
  } else {
    for (const Asset &a : m->assets())
      if (a.uuid == t.uuid) { name = a.name; break; }
  }
  return name.isEmpty() ? t.taskType : name + " · " + t.taskType;
}

void KitsuClient::trFinish(TransitionResult r, TaskStatus server,
                           const QString &msg) {
  const KitsuTransition t = m_trQueue.takeFirst();
  const bool sent = (r == TrPushed || r == TrAlreadyThere);
  const auto ent  = static_cast<ZtoryTaskFlow::Entity>(t.entity);
  // The base follows what Kitsu now has: the change, or its own status.
  if (sent) {
    ZtoryTaskFlow::markSynced(ent, t.uuid, t.taskType, t.to);
    ZtoryModel::instance()->saveProjectDb();
  }
  // Kitsu wins: the local status takes the server's, as a pull would.
  if (r == TrConflict) {
    ZtoryTaskFlow::markSynced(ent, t.uuid, t.taskType, server);
    ZtoryTaskFlow::applyFromServer(ent, t.uuid, t.taskType, server);
  }
  // What this change CAUSED in Ztoryc (a Done readies the next task) was
  // queued right behind it. If the change did not reach Kitsu, sending those
  // would write «next task Ready» next to a Retake the supervisor just set.
  // They are dropped; after a conflict they are also taken back locally.
  if (!sent) {
    for (int i = 0; i < m_trQueue.size();) {
      const KitsuTransition &q = m_trQueue[i];
      if (q.uuid == t.uuid && q.entity == t.entity &&
          q.origin == static_cast<int>(ZtoryTaskFlow::Origin::App)) {
        if (r == TrConflict)
          ZtoryTaskFlow::applyFromServer(
              static_cast<ZtoryTaskFlow::Entity>(q.entity), q.uuid,
              q.taskType, q.from);
        m_trQueue.removeAt(i);
      } else {
        ++i;
      }
    }
  }
  emit transitionPushed(t.entity, t.uuid, t.taskType, r,
                        static_cast<int>(server),
                        msg.isEmpty() ? msg
                                      : transitionLabel(t) + ": " + msg);
  // Next from the event loop: a long queue must not deepen the stack.
  QMetaObject::invokeMethod(this, [this]() { trNext(); }, Qt::QueuedConnection);
}

//----------------------------------------------------------------------------
// Review preview: one file, on one task, in a comment that sets the status
// (Ztoryc, 2026-09-26 — the character's WFA with its preview). On the safe
// push's caches: the clip upload knows only the SHOT task types, and a rig's
// «Rigging» is an asset one.
//----------------------------------------------------------------------------

int KitsuClient::uploadReviewPreview(int entity, const QString &kitsuEntityId,
                                     const QString &taskType,
                                     const QString &filePath,
                                     TaskStatus status,
                                     const QString &comment,
                                     bool keepServerStatus) {
  const int token = ++m_revToken;
  auto fail = [this, token](const QString &msg) {
    emit reviewPreviewUploaded(token, false, msg);
  };
  // The steps, once the caches are there.
  auto run = [this, token, fail, entity, kitsuEntityId, taskType, filePath,
              status, comment, keepServerStatus]() {
    const QString ttId = m_trTaskTypeId.value(trTypeKey(entity, taskType));
    const QString wantedStatusId =
        m_trStatusIdByZ.value(static_cast<int>(status));
    if (ttId.isEmpty()) {
      fail(tr("Task type «%1» is not on Kitsu.").arg(taskType));
      return;
    }
    if (!keepServerStatus && wantedStatusId.isEmpty()) {
      fail(tr("Kitsu has no matching status."));
      return;
    }
    QNetworkReply *r = m_nam->get(authGet("/api/data/tasks?entity_id=" +
                                          kitsuEntityId + "&task_type_id=" +
                                          ttId));
    armTimeout(r);
    connect(r, &QNetworkReply::finished, this, [=]() {
      r->deleteLater();
      const QByteArray b = r->readAll();
      if (r->error() != QNetworkReply::NoError) { fail(errorMessage(r, b)); return; }
      const QJsonArray arr = QJsonDocument::fromJson(b).array();
      if (arr.isEmpty()) {
        fail(tr("No «%1» task on Kitsu.").arg(taskType));
        return;
      }
      const QJsonObject task = arr.first().toObject();
      const QString taskId   = task.value("id").toString();
      const QString statusId = keepServerStatus
                                   ? task.value("task_status_id").toString()
                                   : wantedStatusId;
      if (keepServerStatus &&
          m_revAutomations.contains(ttId + "|" + statusId)) {
        emit reviewPreviewSkipped(
            token, tr("«%1» is in a status that starts one of the project's "
                      "automations on Kitsu: a comment would run it again")
                       .arg(taskType));
        return;
      }
      QJsonObject body;
      body["task_status_id"] = statusId;
      body["comment"] = comment.isEmpty()
                            ? tr("Preview for approval, from Ztoryc")
                            : comment;
      QNetworkReply *c = authPost("/api/actions/tasks/" + taskId + "/comment",
                                  QJsonDocument(body).toJson(QJsonDocument::Compact));
      armTimeout(c);
      connect(c, &QNetworkReply::finished, this, [=]() {
        c->deleteLater();
        const QByteArray cb = c->readAll();
        if (c->error() != QNetworkReply::NoError) { fail(errorMessage(c, cb)); return; }
        const QString cid = QJsonDocument::fromJson(cb).object().value("id").toString();
        if (cid.isEmpty()) { fail(tr("Kitsu did not return a comment id.")); return; }
        QNetworkReply *a = authPost("/api/actions/tasks/" + taskId + "/comments/" +
                                        cid + "/add-preview",
                                    QByteArray("{}"));
        armTimeout(a);
        connect(a, &QNetworkReply::finished, this, [=]() {
          a->deleteLater();
          const QByteArray ab = a->readAll();
          if (a->error() != QNetworkReply::NoError) { fail(errorMessage(a, ab)); return; }
          const QJsonObject o = QJsonDocument::fromJson(ab).object();
          QString pid = o.value("id").toString();
          if (pid.isEmpty()) pid = o.value("preview_file_id").toString();
          if (pid.isEmpty()) { fail(tr("Kitsu did not return a preview id.")); return; }
          QNetworkReply *f = postPreviewFile(pid, filePath, m_baseUrl);
          if (!f) {
            fail(tr("Cannot open %1").arg(QFileInfo(filePath).fileName()));
            return;
          }
          armTimeout(f);
          connect(f, &QNetworkReply::finished, this, [=]() {
            f->deleteLater();
            const QByteArray fb = f->readAll();
            if (f->error() != QNetworkReply::NoError) { fail(errorMessage(f, fb)); return; }
            // The preview becomes the entity's cover. NO frame_number: Zou
            // accepts it only for a movie, and answered 400 to every
            // picture — no cover was ever set this way (2026-09-27).
            QNetworkReply *mp = authPut(
                "/api/actions/preview-files/" + pid + "/set-main-preview",
                QByteArray("{}"));
            armTimeout(mp);
            connect(mp, &QNetworkReply::finished, this, [=]() {
              mp->deleteLater();
              const QByteArray mpb = mp->readAll();
              // The preview is there either way: ok, and the message says
              // what did not go.
              emit reviewPreviewUploaded(
                  token, true,
                  mp->error() == QNetworkReply::NoError
                      ? QString()
                      : tr("cover not set: %1").arg(errorMessage(mp, mpb)));
            });
          });
        });
      });
    });
  };
  if (!isLoggedIn()) {
    QMetaObject::invokeMethod(
        this, [fail]() { fail(tr("Not logged in to Kitsu.")); },
        Qt::QueuedConnection);
  } else {
    // The automations only matter when the status is kept; they are read
    // after the caches, which a new login clears.
    auto withAutomations = [this, run, fail, keepServerStatus]() {
      const QString pid = ZtoryModel::instance()->kitsuProjectId();
      if (!keepServerStatus || m_revAutomationsProject == pid)
        run();
      else
        revLoadAutomations(run, fail);
    };
    if (m_trCachesLoaded)
      QMetaObject::invokeMethod(this, withAutomations, Qt::QueuedConnection);
    else
      trLoadCaches(withAutomations, fail);
  }
  return token;
}

int KitsuClient::setCoverFromTaskPreview(int entity,
                                         const QString &kitsuEntityId,
                                         const QString &taskType,
                                         const QString &skipText) {
  const int token = ++m_revToken;
  auto answer     = [this, token](int result, const QString &msg) {
    emit taskCoverSet(token, result, msg);
  };
  auto run = [this, answer, entity, kitsuEntityId, taskType, skipText]() {
    const QString ttId = m_trTaskTypeId.value(trTypeKey(entity, taskType));
    if (ttId.isEmpty()) {
      answer(CoverNoPreview, QString());
      return;
    }
    QNetworkReply *r = m_nam->get(authGet("/api/data/tasks?entity_id=" +
                                          kitsuEntityId + "&task_type_id=" +
                                          ttId));
    armTimeout(r);
    connect(r, &QNetworkReply::finished, this, [=]() {
      r->deleteLater();
      const QByteArray b = r->readAll();
      if (r->error() != QNetworkReply::NoError) {
        answer(CoverFailed, errorMessage(r, b));
        return;
      }
      const QJsonArray arr = QJsonDocument::fromJson(b).array();
      if (arr.isEmpty()) {
        answer(CoverNoPreview, QString());
        return;
      }
      const QString taskId = arr.first().toObject().value("id").toString();
      QNetworkReply *c =
          m_nam->get(authGet("/api/data/tasks/" + taskId + "/comments"));
      armTimeout(c);
      connect(c, &QNetworkReply::finished, this, [=]() {
        c->deleteLater();
        const QByteArray cb = c->readAll();
        if (c->error() != QNetworkReply::NoError) {
          answer(CoverFailed, errorMessage(c, cb));
          return;
        }
        // The newest comment with a picture that is not ours. Sorted here:
        // the order of the answer is Zou's business.
        QString bestPreview, bestTime;
        for (const QJsonValue &v : QJsonDocument::fromJson(cb).array()) {
          const QJsonObject o = v.toObject();
          if (o.value("text").toString() == skipText) continue;
          const QJsonArray previews = o.value("previews").toArray();
          if (previews.isEmpty()) continue;
          const QString when = o.value("created_at").toString();
          if (!bestTime.isEmpty() && when <= bestTime) continue;
          const QJsonValue p = previews.last();
          bestPreview = p.isObject() ? p.toObject().value("id").toString()
                                     : p.toString();
          bestTime    = when;
        }
        if (bestPreview.isEmpty()) {
          answer(CoverNoPreview, QString());
          return;
        }
        QNetworkReply *mp = authPut(
            "/api/actions/preview-files/" + bestPreview + "/set-main-preview",
            QByteArray("{}"));
        armTimeout(mp);
        connect(mp, &QNetworkReply::finished, this, [=]() {
          mp->deleteLater();
          const QByteArray mpb = mp->readAll();
          if (mp->error() != QNetworkReply::NoError)
            answer(CoverFailed, errorMessage(mp, mpb));
          else
            answer(CoverSet, QString());
        });
      });
    });
  };
  auto fail = [answer](const QString &msg) { answer(CoverFailed, msg); };
  if (!isLoggedIn())
    QMetaObject::invokeMethod(
        this, [fail]() { fail(tr("Not logged in to Kitsu.")); },
        Qt::QueuedConnection);
  else if (m_trCachesLoaded)
    QMetaObject::invokeMethod(this, run, Qt::QueuedConnection);
  else
    trLoadCaches(run, fail);
  return token;
}

void KitsuClient::revLoadAutomations(
    std::function<void()> onDone,
    std::function<void(const QString &)> onFail) {
  const QString pid = ZtoryModel::instance()->kitsuProjectId();
  QNetworkReply *r  = m_nam->get(authGet("/api/data/projects/" + pid));
  armTimeout(r);
  connect(r, &QNetworkReply::finished, this, [this, r, pid, onDone, onFail]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) {
      onFail(errorMessage(r, b));
      return;
    }
    // The project lists the ids of ITS automations; the objects are global.
    QSet<QString> ids;
    for (const QJsonValue &v :
         QJsonDocument::fromJson(b).object().value("status_automations").toArray())
      ids.insert(v.isObject() ? v.toObject().value("id").toString()
                              : v.toString());
    QNetworkReply *r2 = m_nam->get(authGet("/api/data/status-automations"));
    armTimeout(r2);
    connect(r2, &QNetworkReply::finished, this,
            [this, r2, pid, ids, onDone, onFail]() {
      r2->deleteLater();
      const QByteArray b2 = r2->readAll();
      if (r2->error() != QNetworkReply::NoError) {
        onFail(errorMessage(r2, b2));
        return;
      }
      m_revAutomations.clear();
      for (const QJsonValue &v : QJsonDocument::fromJson(b2).array()) {
        const QJsonObject o = v.toObject();
        if (!ids.contains(o.value("id").toString())) continue;
        if (o.value("archived").toBool()) continue;
        m_revAutomations.insert(o.value("in_task_type_id").toString() + "|" +
                                o.value("in_task_status_id").toString());
      }
      m_revAutomationsProject = pid;
      onDone();
    });
  });
}

