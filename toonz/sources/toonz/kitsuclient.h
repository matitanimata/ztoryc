#pragma once

//============================================================================
// KitsuClient — minimal async client for a Kitsu (CGWire / Zou) server.
//
// Phase 1 (M5): JWT login + pull of the open projects and task statuses, plus
// the mapping from Kitsu task statuses onto Ztoryc's internal TaskStatus enum.
// Pushing the shot list and render-upload / status sync land in later phases.
//
// Deployment-agnostic by design: a single base URL (local docker, LAN, tunnel
// or CGWire hosted) chosen by the user. URL + credentials persist in QSettings
// under the "Ztoryc/Kitsu" group, so the same build talks to any instance.
//============================================================================

#include "ztorymodel.h"  // TaskStatus

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

//----------------------------------------------------------------------------
// A Kitsu project as returned by /api/data/projects/open. Note: the numeric-
// looking fields (fps / ratio / resolution) are *strings* on the wire — keep
// them verbatim rather than coercing to numbers.
//----------------------------------------------------------------------------
struct KitsuProject {
  QString id;
  QString name;
  QString code;
  QString fps;
  QString ratio;
  QString resolution;
  QString productionType;   // tvshow / short / featurefilm
  QString productionStyle;  // 2d / 3d / 2d3d
};

//----------------------------------------------------------------------------
// A Kitsu task status (/api/data/task-status). The semantic flags drive the
// mapping onto Ztoryc's enum, so a renamed pipeline still maps sanely without
// depending on display names.
//----------------------------------------------------------------------------
struct KitsuTaskStatus {
  QString id;
  QString name;
  QString shortName;
  QString color;
  bool    isDone            = false;
  bool    isRetake          = false;
  bool    isFeedbackRequest = false;
  bool    isDefault         = false;
};

//----------------------------------------------------------------------------
// One shot to push up to Kitsu (push-only: Ztoryc is authoritative on the shot
// list, so we never pull shots back).
//----------------------------------------------------------------------------
struct KitsuShotPush {
  QString seq;       // sequence name, e.g. "SQ010"
  QString name;      // shot name,     e.g. "SH010"
  int     nbFrames = 0;
  int     frameIn  = 1;
  int     frameOut = 0;
};

// One shot-task whose status we push up to Kitsu (Phase 3b). taskType must match
// a Kitsu task-type name (for_entity = Shot).
struct KitsuTaskPush {
  QString    seq;
  QString    shot;
  QString    taskType;
  TaskStatus status = TaskStatus::Todo;
};

//----------------------------------------------------------------------------
class KitsuClient : public QObject {
  Q_OBJECT
public:
  explicit KitsuClient(QObject *parent = nullptr);
  ~KitsuClient() override;

  // --- Config (persisted in QSettings, group "Ztoryc/Kitsu") -----------
  QString baseUrl() const { return m_baseUrl; }
  void    setBaseUrl(const QString &url);  // trailing slash trimmed
  QString email() const { return m_email; }
  void    setEmail(const QString &email) { m_email = email; }
  // Kept in memory for the session; only written to QSettings when the caller
  // passes savePassword=true to saveSettings() (convenient on a local box).
  void    setPassword(const QString &pwd) { m_password = pwd; }
  bool    hasSavedPassword() const { return m_passwordSaved; }

  void loadSettings();
  void saveSettings(bool savePassword);

  bool isLoggedIn() const { return !m_accessToken.isEmpty(); }

  // Role of the logged-in user (admin / manager / supervisor / user / …).
  QString userRole() const { return m_userRole; }
  // Only admin / manager may create or edit projects in Kitsu (server enforces
  // this too; we gate the UI so the user isn't offered a doomed action).
  bool canManageProjects() const {
    return m_userRole == "admin" || m_userRole == "manager";
  }

  // --- Async operations ------------------------------------------------
  void login();              // POST /api/auth/login        -> loginFinished()
  void fetchProjects();      // GET  /api/data/projects/open -> projectsFetched()
  void fetchTaskStatuses();  // GET  /api/data/task-status   -> taskStatusesFetched()

  // Login, then on success fetch projects + task statuses in one shot.
  void connectAndSync();

  // Create a new project from the given fields (id ignored) -> projectCreated().
  void createProject(const KitsuProject &p);
  // Push field updates onto an existing project -> projectUpdated().
  void updateProject(const QString &id, const KitsuProject &p);

  // Push-only shot sync (Ztoryc -> Kitsu). Ensures the episode (tvshow) and the
  // sequences exist, then upserts each shot by name. -> shotsPushed().
  void pushShots(const QString &projectId, const QString &episodeName,
                 bool tvshow, const QVector<KitsuShotPush> &shots);

  // Push-only task + status sync (Phase 3b): ensure each task exists on its shot
  // and set its status to match Ztoryc. Requires task statuses already fetched.
  // -> tasksPushed().
  void pushTasks(const QString &projectId, const QVector<KitsuTaskPush> &tasks);

  QVector<KitsuProject>    projects() const { return m_projects; }
  QVector<KitsuTaskStatus> taskStatuses() const { return m_taskStatuses; }

  // Map a fetched Kitsu status id onto Ztoryc's enum (valid after statuses are
  // fetched). Unknown ids fall back to Todo.
  TaskStatus toZtoryStatus(const QString &kitsuStatusId) const;

  // Name / flag-based mapping used to build the per-id table. Static so it can
  // be unit-reasoned about without a live client.
  static TaskStatus mapStatus(const KitsuTaskStatus &s);

signals:
  void loginFinished(bool ok, const QString &message);
  void projectsFetched(const QVector<KitsuProject> &projects);
  void taskStatusesFetched(const QVector<KitsuTaskStatus> &statuses);
  void projectCreated(bool ok, const KitsuProject &project, const QString &message);
  void projectUpdated(bool ok, const QString &message);
  void shotsPushProgress(const QString &message);
  void shotsPushed(bool ok, int created, int updated, const QString &message);
  void tasksPushed(bool ok, int statusesSet, const QString &message);
  void networkError(const QString &message);

private:
  void onLoginReply(QNetworkReply *reply);
  void onProjectsReply(QNetworkReply *reply);
  void onTaskStatusesReply(QNetworkReply *reply);
  // Serialise a KitsuProject into the JSON body POST/PUT expect.
  QByteArray projectBody(const KitsuProject &p) const;
  static KitsuProject parseProject(const class QJsonObject &o);

  // --- Shot push state machine (all steps are sequential async) --------
  QNetworkRequest authGet(const QString &path) const;
  QNetworkReply  *authPost(const QString &path, const QByteArray &body);
  QNetworkReply  *authPut(const QString &path, const QByteArray &body);
  void pushEnsureEpisode();
  void pushLoadSequences();
  void pushLoadShots();
  void pushProcessNext();
  void pushFail(const QString &message);

  QString m_pushProjectId;
  QString m_pushEpisodeName;
  QString m_pushEpisodeId;
  bool    m_pushTvshow = false;
  QVector<KitsuShotPush>     m_pushQueue;
  QHash<QString, QString>    m_pushSeqIds;   // seqName  -> sequence id
  QHash<QString, QString>    m_pushShotIds;  // "seqId/shotName" -> shot id
  int m_pushIndex   = 0;
  int m_pushCreated = 0;
  int m_pushUpdated = 0;

  // --- Task + status push (Phase 3b) -----------------------------------
  void taskLoadTaskTypes();
  void taskCreateNext();
  void taskLoadSequences();
  void taskLoadShots();
  void taskLoadProjectTasks();
  void taskApplyNext();
  void taskFail(const QString &message);
  QString statusIdFor(TaskStatus s) const;

  QString m_taskProjectId;
  QVector<KitsuTaskPush>   m_taskQueue;
  QHash<QString, QString>  m_ttIdByName;     // Kitsu Shot task-type name -> id
  QHash<QString, QString>  m_taskSeqIds;     // seq name -> sequence id
  QHash<QString, QString>  m_taskShotIds;    // "seqId/shotName" -> shot id
  QHash<QString, QString>  m_taskIdByKey;    // "shotId/ttId" -> task id
  QHash<int, QString>      m_statusIdByZ;    // TaskStatus (int) -> Kitsu status id
  QVector<QString>         m_ttCreateQueue;  // task-type ids to create-tasks for
  int m_taskCreateIdx = 0;
  int m_taskApplyIdx  = 0;
  int m_taskStatusesSet = 0;

  QNetworkAccessManager *m_nam = nullptr;

  QString m_baseUrl;
  QString m_email;
  QString m_password;
  bool    m_passwordSaved = false;

  QString m_accessToken;
  QString m_refreshToken;
  QString m_userRole;
  bool    m_syncAfterLogin = false;  // connectAndSync() pending

  QVector<KitsuProject>      m_projects;
  QVector<KitsuTaskStatus>   m_taskStatuses;
  QHash<QString, TaskStatus> m_statusById;  // built after fetchTaskStatuses()
};
