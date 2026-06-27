#include "kitsuclient.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QUrl>

namespace {
const char *kGroupBaseUrl  = "Ztoryc/Kitsu/BaseUrl";
const char *kGroupEmail    = "Ztoryc/Kitsu/Email";
const char *kGroupPassword = "Ztoryc/Kitsu/Password";  // local convenience only
const char *kGroupHasPwd   = "Ztoryc/Kitsu/PasswordSaved";

// Pull a human-readable message out of an error reply (Zou answers with a JSON
// body { "message": ... } on most failures); fall back to Qt's error string.
QString errorMessage(QNetworkReply *reply, const QByteArray &body) {
  QJsonParseError perr;
  QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
  if (perr.error == QJsonParseError::NoError && doc.isObject()) {
    const QJsonObject o = doc.object();
    const QString msg   = o.value("message").toString();
    if (!msg.isEmpty()) return msg;
  }
  return reply->errorString();
}
}  // namespace

//----------------------------------------------------------------------------

KitsuClient::KitsuClient(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)) {}

KitsuClient::~KitsuClient() = default;

//----------------------------------------------------------------------------

void KitsuClient::setBaseUrl(const QString &url) {
  m_baseUrl = url.trimmed();
  while (m_baseUrl.endsWith('/')) m_baseUrl.chop(1);
}

void KitsuClient::loadSettings() {
  QSettings s;
  setBaseUrl(s.value(kGroupBaseUrl, "http://localhost:8012").toString());
  m_email         = s.value(kGroupEmail).toString();
  m_passwordSaved = s.value(kGroupHasPwd, false).toBool();
  if (m_passwordSaved) m_password = s.value(kGroupPassword).toString();
}

void KitsuClient::saveSettings(bool savePassword) {
  QSettings s;
  s.setValue(kGroupBaseUrl, m_baseUrl);
  s.setValue(kGroupEmail, m_email);
  m_passwordSaved = savePassword;
  s.setValue(kGroupHasPwd, savePassword);
  if (savePassword)
    s.setValue(kGroupPassword, m_password);
  else
    s.remove(kGroupPassword);
}

//----------------------------------------------------------------------------
// Status mapping
//----------------------------------------------------------------------------

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
    const QJsonObject o = v.toObject();
    KitsuTaskStatus s;
    s.id                = o.value("id").toString();
    s.name              = o.value("name").toString();
    s.shortName         = o.value("short_name").toString();
    s.color             = o.value("color").toString();
    s.isDone            = o.value("is_done").toBool();
    s.isRetake          = o.value("is_retake").toBool();
    s.isFeedbackRequest = o.value("is_feedback_request").toBool();
    s.isDefault         = o.value("is_default").toBool();
    m_taskStatuses.push_back(s);
    m_statusById.insert(s.id, mapStatus(s));
  }
  emit taskStatusesFetched(m_taskStatuses);
}
