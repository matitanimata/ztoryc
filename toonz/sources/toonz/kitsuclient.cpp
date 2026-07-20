#include "kitsuclient.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMimeDatabase>

namespace {
const char *kGroupBaseUrl  = "Ztoryc/Kitsu/BaseUrl";
const char *kGroupLocalUrl = "Ztoryc/Kitsu/LocalUrl";  // optional upload endpoint
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

KitsuClient *KitsuClient::instance() {
  // App-lifetime singleton (intentionally never deleted). loadSettings() once so
  // the saved URL/email/password are available immediately.
  static KitsuClient *s = nullptr;
  if (!s) {
    s = new KitsuClient();
    s->loadSettings();
  }
  return s;
}

//----------------------------------------------------------------------------

void KitsuClient::setBaseUrl(const QString &url) {
  m_baseUrl = url.trimmed();
  while (m_baseUrl.endsWith('/')) m_baseUrl.chop(1);
}

void KitsuClient::setLocalUrl(const QString &url) {
  m_localUrl = url.trimmed();
  while (m_localUrl.endsWith('/')) m_localUrl.chop(1);
}

void KitsuClient::loadSettings() {
  QSettings s;
  setBaseUrl(s.value(kGroupBaseUrl, "http://localhost:8012").toString());
  setLocalUrl(s.value(kGroupLocalUrl).toString());
  m_email         = s.value(kGroupEmail).toString();
  m_passwordSaved = s.value(kGroupHasPwd, false).toBool();
  if (m_passwordSaved) m_password = s.value(kGroupPassword).toString();
}

void KitsuClient::saveSettings(bool savePassword) {
  QSettings s;
  s.setValue(kGroupBaseUrl, m_baseUrl);
  s.setValue(kGroupLocalUrl, m_localUrl);
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
                            bool tvshow, const QVector<KitsuShotPush> &shots) {
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
    if (!m_pushEpisodeId.isEmpty()) { pushLoadSequences(); return; }
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
      m_pushSeqIds.insert(o.value("name").toString(), o.value("id").toString());
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
      m_pushShotIds.insert(
          o.value("parent_id").toString() + "/" + o.value("name").toString(),
          o.value("id").toString());
    }
    pushProcessNext();
  });
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

  // Common body (name/frames/timecode). Ztoryc is master on structure, so a PUT
  // also renames the Kitsu shot back to Ztoryc's name if it diverged.
  QJsonObject data;
  data["frame_in"]  = QString::number(sh.frameIn);
  data["frame_out"] = QString::number(sh.frameOut);
  QJsonObject body;
  body["name"]      = sh.name;
  body["nb_frames"] = sh.nbFrames;
  body["data"]      = data;

  // 1) Known Kitsu shot id → update THAT shot directly (rename-proof): no name
  //    lookup, so a shot renamed in Kitsu isn't duplicated.
  if (!sh.kitsuShotId.isEmpty()) {
    QNetworkReply *ur = authPut("/api/data/shots/" + sh.kitsuShotId,
                                QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(ur, &QNetworkReply::finished, this,
            [this, ur, resolvedKey, id = sh.kitsuShotId]() {
              ur->deleteLater();
              const QByteArray ub = ur->readAll();
              if (ur->error() != QNetworkReply::NoError) { pushFail(errorMessage(ur, ub)); return; }
              m_pushResolved[resolvedKey] = id;
              ++m_pushUpdated;
              ++m_pushIndex;
              pushProcessNext();
            });
    return;
  }

  // Ensure the sequence exists before we can place a shot under it.
  if (!m_pushSeqIds.contains(sh.seq)) {
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
      m_pushSeqIds.insert(seq, QJsonDocument::fromJson(cb).object().value("id").toString());
      pushProcessNext();  // retry the same shot now that its sequence exists
    });
    return;
  }

  const QString seqId = m_pushSeqIds.value(sh.seq);
  const QString key   = seqId + "/" + sh.name;
  if (m_pushShotIds.contains(key)) {
    const QString id = m_pushShotIds.value(key);
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
  m_taskCreateIdx = m_taskApplyIdx = m_taskStatusesSet = m_taskUnchanged = 0;

  // Reverse status map: Ztoryc TaskStatus -> canonical Kitsu status id (only the
  // six pipeline short names, so approved/rejected/neutral don't shadow them).
  m_statusIdByZ.clear();
  for (const KitsuTaskStatus &st : m_taskStatuses) {
    const QString sn = st.shortName.toLower();
    if (sn == "todo" || sn == "ready" || sn == "wip" || sn == "wfa" ||
        sn == "retake" || sn == "done")
      m_statusIdByZ.insert(static_cast<int>(mapStatus(st)), st.id);
  }
  // Load the roster first so assignee names resolve to person ids for the
  // add-only assign pass that follows the status updates.
  loadRosterThen(m_taskProjectId, [this]() { taskLoadTaskTypes(); });
}

void KitsuClient::taskLoadTaskTypes() {
  emit shotsPushProgress(tr("Loading task types…"));
  QNetworkReply *r = m_nam->get(authGet("/api/data/task-types"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { taskFail(errorMessage(r, b)); return; }
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      if (o.value("for_entity").toString() == "Shot")
        m_ttIdByName.insert(o.value("name").toString().toLower(),
                            o.value("id").toString());
    }
    // Distinct task-type ids actually used by the queue (and known to Kitsu).
    for (const KitsuTaskPush &t : m_taskQueue) {
      const QString id = resolveTaskTypeId(t.taskType);
      if (!id.isEmpty() && !m_ttCreateQueue.contains(id)) m_ttCreateQueue.push_back(id);
    }
    taskCreateNext();
  });
}

void KitsuClient::taskCreateNext() {
  if (m_taskCreateIdx >= m_ttCreateQueue.size()) { taskLoadSequences(); return; }
  const QString ttId = m_ttCreateQueue[m_taskCreateIdx];
  emit shotsPushProgress(tr("Creating tasks (%1/%2)…")
                             .arg(m_taskCreateIdx + 1)
                             .arg(m_ttCreateQueue.size()));
  QNetworkReply *r = authPost("/api/actions/projects/" + m_taskProjectId +
                                  "/task-types/" + ttId + "/shots/create-tasks",
                              "{}");
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { taskFail(errorMessage(r, b)); return; }
    ++m_taskCreateIdx;
    taskCreateNext();
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
      m_taskSeqIds.insert(o.value("name").toString(), o.value("id").toString());
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
      m_taskShotIds.insert(
          o.value("parent_id").toString() + "/" + o.value("name").toString(),
          o.value("id").toString());
    }
    taskLoadProjectTasks();
  });
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
    taskApplyNext();
  });
}

void KitsuClient::taskApplyNext() {
  while (m_taskApplyIdx < m_taskQueue.size()) {
    const KitsuTaskPush t = m_taskQueue[m_taskApplyIdx];
    const QString ttId     = resolveTaskTypeId(t.taskType);
    const QString seqId    = m_taskSeqIds.value(t.seq);
    const QString shotId   = m_taskShotIds.value(seqId + "/" + t.shot);
    const QString taskId   = m_taskIdByKey.value(shotId + "/" + ttId);
    const QString statusId = statusIdFor(t.status);
    // Skip anything we couldn't resolve (unknown task-type, shot or status).
    if (ttId.isEmpty() || shotId.isEmpty() || taskId.isEmpty() ||
        statusId.isEmpty()) {
      ++m_taskApplyIdx;
      continue;
    }
    // Already at the target status in Kitsu → don't re-comment (would spam the
    // activity feed and notifications); only touch what actually changed.
    if (m_taskStatusByKey.value(shotId + "/" + ttId) == statusId) {
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
    connect(r, &QNetworkReply::finished, this, [this, r]() {
      r->deleteLater();
      const QByteArray b = r->readAll();
      if (r->error() != QNetworkReply::NoError) { taskFail(errorMessage(r, b)); return; }
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
  m_assignQueue.clear();
  m_assignIdx = 0;
  QSet<QString> skippedNames;  // Ztoryc assignees that aren't on the Kitsu team
  for (const KitsuTaskPush &t : m_taskQueue) {
    if (t.assignees.isEmpty()) continue;
    const QString ttId   = resolveTaskTypeId(t.taskType);
    const QString shotId = m_taskShotIds.value(m_taskSeqIds.value(t.seq) + "/" + t.shot);
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
  m_assignOnDone = [this, statusesSet, unchanged, assigned, skipped]() {
    QString msg = unchanged > 0
                      ? tr("Done — %1 task statuses changed, %2 unchanged.")
                            .arg(statusesSet).arg(unchanged)
                      : tr("Done — %1 task statuses set in Kitsu.").arg(statusesSet);
    if (assigned > 0) msg += tr("  %1 people assigned.").arg(assigned);
    if (skipped > 0)  msg += tr("  %1 not in team (skipped).").arg(skipped);
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

void KitsuClient::pullStatuses(const QString &projectId) {
  if (!isLoggedIn()) { emit statusesPulled(false, {}, tr("Not logged in.")); return; }
  if (projectId.isEmpty()) {
    emit statusesPulled(false, {}, tr("Project not linked to Kitsu."));
    return;
  }
  m_pullProjectId = projectId;
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
      m_pullShotName.insert(id, o.value("name").toString());
      m_pullShotSeq.insert(id, m_pullSeqName.value(o.value("parent_id").toString()));
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
      out.push_back(p);
    }
  }
  return out;
}

void KitsuClient::asPushFail(const QString &message) {
  emit assetsPushed(false, m_asCreated, m_asUpdated, message);
}

void KitsuClient::pushAssets(const QString &projectId,
                             const QVector<KitsuAsset> &assets) {
  if (!isLoggedIn()) { emit assetsPushed(false, 0, 0, tr("Not logged in.")); return; }
  if (projectId.isEmpty() || assets.isEmpty()) {
    emit assetsPushed(true, 0, 0, tr("No assets to push."));
    return;
  }
  m_asProjectId = projectId;
  m_asQueue     = assets;
  m_asTypeIdByName.clear();
  m_asExisting.clear();
  m_asResolved.clear();
  m_asIndex = m_asCreated = m_asUpdated = 0;
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
    // No matching Kitsu asset-type → can't create it there; skip.
    if (typeId.isEmpty()) { ++m_asIndex; continue; }
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
    QNetworkReply *r = authPost("/api/data/projects/" + m_asProjectId +
                                    "/asset-types/" + typeId + "/assets/new",
                                QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this, r, key]() {
      r->deleteLater();
      const QByteArray b = r->readAll();
      if (r->error() != QNetworkReply::NoError) { asPushFail(errorMessage(r, b)); return; }
      const QString id = QJsonDocument::fromJson(b).object().value("id").toString();
      if (!id.isEmpty()) m_asResolved.insert(key, id);
      ++m_asCreated;
      ++m_asIndex;
      asPushProcessNext();
    });
    return;  // resume in the callback
  }
  emit assetIdsResolved(m_asResolved);
  emit assetsPushed(true, m_asCreated, m_asUpdated,
                    tr("Assets synced — %1 created, %2 already in Kitsu.")
                        .arg(m_asCreated)
                        .arg(m_asUpdated));
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
  m_atTtCreateQueue.clear();
  m_atCreateIdx = m_atApplyIdx = m_atStatusesSet = m_atUnchanged = 0;

  // Reverse status map (same six pipeline statuses as the shot push).
  m_statusIdByZ.clear();
  for (const KitsuTaskStatus &st : m_taskStatuses) {
    const QString sn = st.shortName.toLower();
    if (sn == "todo" || sn == "ready" || sn == "wip" || sn == "wfa" ||
        sn == "retake" || sn == "done")
      m_statusIdByZ.insert(static_cast<int>(mapStatus(st)), st.id);
  }
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
    // Distinct asset task-type ids used by the queue and known to Kitsu.
    for (const KitsuAssetTaskPush &t : m_atQueue) {
      const QString id = m_atTtIdByName.value(normalizeTaskType(t.taskType));
      if (!id.isEmpty() && !m_atTtCreateQueue.contains(id))
        m_atTtCreateQueue.push_back(id);
    }
    atCreateNext();
  });
}

void KitsuClient::atCreateNext() {
  if (m_atCreateIdx >= m_atTtCreateQueue.size()) { atLoadAssetTypes(); return; }
  const QString ttId = m_atTtCreateQueue[m_atCreateIdx];
  emit shotsPushProgress(tr("Creating asset tasks (%1/%2)…")
                             .arg(m_atCreateIdx + 1)
                             .arg(m_atTtCreateQueue.size()));
  QNetworkReply *r = authPost("/api/actions/projects/" + m_atProjectId +
                                  "/task-types/" + ttId + "/assets/create-tasks",
                              "{}");
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { atFail(errorMessage(r, b)); return; }
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
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      m_atAssetTypeIdByName.insert(o.value("name").toString().toLower(),
                                   o.value("id").toString());
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
    atApplyNext();
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
    // Skip anything we couldn't resolve (unknown task-type, asset or status).
    if (ttId.isEmpty() || assetId.isEmpty() || taskId.isEmpty() ||
        statusId.isEmpty()) {
      ++m_atApplyIdx;
      continue;
    }
    // Already at the target status in Kitsu → don't re-comment (would spam the
    // activity feed and notifications); only touch what actually changed.
    if (m_atStatusByKey.value(assetId + "/" + ttId) == statusId) {
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
    connect(r, &QNetworkReply::finished, this, [this, r]() {
      r->deleteLater();
      const QByteArray b = r->readAll();
      if (r->error() != QNetworkReply::NoError) { atFail(errorMessage(r, b)); return; }
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
  m_assignOnDone = [this, statusesSet, unchanged, assigned, skipped]() {
    QString msg = unchanged > 0
                      ? tr("Done — %1 asset task statuses changed, %2 unchanged.")
                            .arg(statusesSet).arg(unchanged)
                      : tr("Done — %1 asset task statuses set in Kitsu.").arg(statusesSet);
    if (assigned > 0) msg += tr("  %1 people assigned.").arg(assigned);
    if (skipped > 0)  msg += tr("  %1 not in team (skipped).").arg(skipped);
    emit assetTasksPushed(true, statusesSet, msg);
  };
  assignRun();
}

void KitsuClient::asPullFail(const QString &message) {
  emit assetsPulled(false, {}, message);
}

void KitsuClient::pullAssets(const QString &projectId) {
  if (!isLoggedIn()) { emit assetsPulled(false, {}, tr("Not logged in.")); return; }
  if (projectId.isEmpty()) { emit assetsPulled(false, {}, tr("No project.")); return; }
  m_asProjectId = projectId;
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

void KitsuClient::asPullLoadAssets() {
  emit shotsPushProgress(tr("Reading Kitsu assets…"));
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_asProjectId + "/assets"));
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { asPullFail(errorMessage(r, b)); return; }
    QVector<KitsuAsset> out;
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      KitsuAsset a;
      a.kitsuAssetId = o.value("id").toString();
      a.name         = o.value("name").toString();
      a.type = m_asTypeNameById.value(o.value("entity_type_id").toString());
      if (!a.name.isEmpty()) out.push_back(a);
    }
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

void KitsuClient::pullAssetStatuses(const QString &projectId) {
  if (!isLoggedIn()) { emit assetStatusesPulled(false, {}, tr("Not logged in.")); return; }
  if (projectId.isEmpty()) {
    emit assetStatusesPulled(false, {}, tr("Project not linked to Kitsu."));
    return;
  }
  m_apProjectId = projectId;
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
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      m_apAssetTypeName.insert(o.value("id").toString(), o.value("name").toString());
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
    // Persons are best-effort context: if the roster can't be read (restricted
    // account) we still continue — the project GET below carries its own error.
    if (r->error() == QNetworkReply::NoError) {
      for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
        const QJsonObject o = v.toObject();
        const QString id    = o.value("id").toString();
        QString name        = o.value("full_name").toString();
        if (name.trimmed().isEmpty())
          name = (o.value("first_name").toString() + " " +
                  o.value("last_name").toString()).trimmed();
        if (name.trimmed().isEmpty()) name = o.value("email").toString();
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
                      tr("Team has %1 members but their names couldn't be read "
                         "(persons endpoint restricted?).")
                          .arg(m_teamPersonIds.size()));
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
    for (auto it = ps.tasks.constBegin(); it != ps.tasks.constEnd(); ++it) {
      KitsuTaskPush tp;
      tp.seq = s.seq; tp.shot = s.name;
      tp.taskType = it.key(); tp.status = it.value().status;
      tp.assignees = it.value().assignees;
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
  m_statusIdByZ.clear();
  for (const KitsuTaskStatus &st : m_taskStatuses) {
    const QString sn = st.shortName.toLower();
    if (sn == "todo" || sn == "ready" || sn == "wip" || sn == "wfa" ||
        sn == "retake" || sn == "done")
      m_statusIdByZ.insert(static_cast<int>(mapStatus(st)), st.id);
  }
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
    for (const QJsonValue &v : QJsonDocument::fromJson(b).array()) {
      const QJsonObject o = v.toObject();
      if (o.value("for_entity").toString() == "Shot")
        m_ttIdByName.insert(o.value("name").toString().toLower(),
                            o.value("id").toString());
    }
    // Distinct task-type ids the queue uploads to (so we can create the tasks if
    // they don't exist yet — otherwise there's nothing to attach the preview to).
    for (const KitsuPreviewUpload &u : m_uplQueue) {
      const QString id = resolveTaskTypeId(u.taskType);
      if (!id.isEmpty() && !m_uplTtCreate.contains(id)) m_uplTtCreate.push_back(id);
    }
    uplEnsureTasks();
  });
}

void KitsuClient::uplEnsureTasks() {
  // create-tasks is idempotent (only makes the missing ones); run it per used
  // task-type so the shots have the task before we attach previews to it.
  if (m_uplTtIdx >= m_uplTtCreate.size()) { uplLoadTasks(); return; }
  const QString ttId = m_uplTtCreate[m_uplTtIdx];
  emit shotsPushProgress(tr("Ensuring tasks (%1/%2)…")
                             .arg(m_uplTtIdx + 1)
                             .arg(m_uplTtCreate.size()));
  QNetworkReply *r = authPost("/api/actions/projects/" + m_uplProjectId +
                                  "/task-types/" + ttId + "/shots/create-tasks",
                              "{}", uploadBase());
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { uplFail(errorMessage(r, b)); return; }
    ++m_uplTtIdx;
    uplEnsureTasks();
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
    uplProcessNext();
  });
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

void KitsuClient::uplUploadFile(const QString &previewFileId,
                                const QString &filePath) {
  QFile *file = new QFile(filePath);
  if (!file->open(QIODevice::ReadOnly)) {
    delete file;
    uplFail(tr("Cannot open %1").arg(QFileInfo(filePath).fileName()));
    return;
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
      (QUrl(uploadBase() + "/api/pictures/preview-files/" + previewFileId)));
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  QNetworkReply *r = m_nam->post(req, mp);
  mp->setParent(r);  // multipart (and file) freed with the reply
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
