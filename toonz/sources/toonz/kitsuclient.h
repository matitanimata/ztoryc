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
  void networkError(const QString &message);

private:
  void onLoginReply(QNetworkReply *reply);
  void onProjectsReply(QNetworkReply *reply);
  void onTaskStatusesReply(QNetworkReply *reply);
  // Serialise a KitsuProject into the JSON body POST/PUT expect.
  QByteArray projectBody(const KitsuProject &p) const;
  static KitsuProject parseProject(const class QJsonObject &o);

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
