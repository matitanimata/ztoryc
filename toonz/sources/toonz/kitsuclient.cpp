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
#include <QMimeDatabase>

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
// Shot push — sequential async state machine (push-only: Ztoryc -> Kitsu)
//----------------------------------------------------------------------------

QNetworkRequest KitsuClient::authGet(const QString &path) const {
  QNetworkRequest req((QUrl(m_baseUrl + path)));
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  return req;
}

QNetworkReply *KitsuClient::authPost(const QString &path, const QByteArray &body) {
  QNetworkRequest req((QUrl(m_baseUrl + path)));
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  return m_nam->post(req, body);
}

QNetworkReply *KitsuClient::authPut(const QString &path, const QByteArray &body) {
  QNetworkRequest req((QUrl(m_baseUrl + path)));
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
  m_ttCreateQueue.clear();
  m_taskCreateIdx = m_taskApplyIdx = m_taskStatusesSet = 0;

  // Reverse status map: Ztoryc TaskStatus -> canonical Kitsu status id (only the
  // six pipeline short names, so approved/rejected/neutral don't shadow them).
  m_statusIdByZ.clear();
  for (const KitsuTaskStatus &st : m_taskStatuses) {
    const QString sn = st.shortName.toLower();
    if (sn == "todo" || sn == "ready" || sn == "wip" || sn == "wfa" ||
        sn == "retake" || sn == "done")
      m_statusIdByZ.insert(static_cast<int>(mapStatus(st)), st.id);
  }
  taskLoadTaskTypes();
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
      m_taskIdByKey.insert(o.value("entity_id").toString() + "/" +
                               o.value("task_type_id").toString(),
                           o.value("id").toString());
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
  emit tasksPushed(true, m_taskStatusesSet,
                   tr("Done — %1 task statuses set in Kitsu.").arg(m_taskStatusesSet));
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
  pullLoadSequences();
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
      entries.push_back(e);
    }
    emit statusesPulled(true, entries,
                        tr("Pulled %1 task statuses from Kitsu.").arg(entries.size()));
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

void KitsuClient::uploadPreviews(const QString &projectId,
                                 const QVector<KitsuPreviewUpload> &uploads) {
  if (!isLoggedIn()) { emit previewsUploaded(false, 0, tr("Not logged in.")); return; }
  if (projectId.isEmpty() || uploads.isEmpty()) {
    emit previewsUploaded(true, 0, tr("No previews to upload."));
    return;
  }
  m_uplProjectId = projectId;
  m_uplQueue     = uploads;
  m_ttIdByName.clear();
  m_uplTaskIdByKey.clear();
  m_uplIndex = m_uplDone = 0;

  // Reverse status map (Ztoryc TaskStatus -> Kitsu status id), same six pipeline
  // short names used by the task push, so Wfa/Done/etc. resolve correctly.
  m_statusIdByZ.clear();
  for (const KitsuTaskStatus &st : m_taskStatuses) {
    const QString sn = st.shortName.toLower();
    if (sn == "todo" || sn == "ready" || sn == "wip" || sn == "wfa" ||
        sn == "retake" || sn == "done")
      m_statusIdByZ.insert(static_cast<int>(mapStatus(st)), st.id);
  }
  uplLoadTaskTypes();
}

void KitsuClient::uplLoadTaskTypes() {
  emit shotsPushProgress(tr("Loading task types…"));
  QNetworkReply *r = m_nam->get(authGet("/api/data/task-types"));
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
    uplLoadTasks();
  });
}

void KitsuClient::uplLoadTasks() {
  emit shotsPushProgress(tr("Reading existing tasks…"));
  QNetworkReply *r =
      m_nam->get(authGet("/api/data/projects/" + m_uplProjectId + "/tasks"));
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
  QNetworkReply *r = authPost("/api/actions/tasks/" + taskId + "/comment",
                              QJsonDocument(body).toJson(QJsonDocument::Compact));
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
      QByteArray("{}"));
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
      (QUrl(m_baseUrl + "/api/pictures/preview-files/" + previewFileId)));
  req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
  QNetworkReply *r = m_nam->post(req, mp);
  mp->setParent(r);  // multipart (and file) freed with the reply
  connect(r, &QNetworkReply::finished, this, [this, r]() {
    r->deleteLater();
    const QByteArray b = r->readAll();
    if (r->error() != QNetworkReply::NoError) { uplFail(errorMessage(r, b)); return; }
    ++m_uplDone;
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
