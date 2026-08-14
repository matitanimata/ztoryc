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
//
// Dual URL (local/remote): the base URL above is the *primary* address used for
// login and every light API call (it may be a Cloudflare tunnel or CGWire host,
// reachable from anywhere). An optional *local* URL points at the SAME instance
// over the LAN; when set and reachable it carries the heavy preview upload —
// bypassing the remote proxy's body-size cap (e.g. Cloudflare's 100 MB) and its
// latency. The JWT is instance-wide, so it validates on either address. When the
// local URL is empty or unreachable, uploads fall back to the primary URL.
//============================================================================

#include "ztorymodel.h"  // TaskStatus

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QPair>

#include <functional>

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
  bool isTvshow() const { return productionType == "tvshow"; }
};

//----------------------------------------------------------------------------
// One episode of a tvshow (/api/data/projects/<id>/episodes).
// A Ztoryc project maps to ONE episode, not to the whole show: «Cartoon School
// 2026» is a single Kitsu project holding six episodes, and each episode is its
// own Ztoryc project. Binding to the project alone would make every episode's
// shots show up in every tracker.
//----------------------------------------------------------------------------
struct KitsuEpisode {
  QString id;
  QString name;
  QString projectId;  // the tvshow it belongs to
};

//----------------------------------------------------------------------------
// One line of a shot's BREAKDOWN — Kitsu calls it «casting» and stores it in
// entity_link (entity_in = shot, entity_out = asset).
// Flat on purpose, with the shot id inside each entry: the bulk endpoint hands
// back a map shot -> entries, and flattening it here keeps the caller from
// having to walk two levels.
//----------------------------------------------------------------------------
struct KitsuCastingEntry {
  QString kitsuShotId;
  QString kitsuAssetId;
  QString assetName;
  QString assetType;      // asset_type_name — arrives with the casting
  int     nbOccurrences = 1;
  QString label;          // free label (seen in the wild: "animate")
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
  QString seq;          // sequence name, e.g. "SQ010"
  QString name;         // shot name,     e.g. "SH010"
  QString kitsuShotId;  // if known, update THAT shot (rename-proof) instead of by name
  int     nbFrames = 0;
  int     frameIn  = 1;
  int     frameOut = 0;
};

// One production asset synced with Kitsu. Bidirectional: new assets are pushed
// up (create), and assets authored in Kitsu are pulled down to import. type is a
// canonical asset type (Character/Prop/FX/Environment) matched to a Kitsu
// asset-type by name; kitsuAssetId links the two rename-proof once known.
struct KitsuAsset {
  QString type;
  QString name;
  QString kitsuAssetId;  // non-empty → it already exists in Kitsu
};

// A Kitsu person (team member). name is the display name (full_name); id is the
// Kitsu person id used to resolve task assignees.
struct KitsuPerson {
  QString id;
  QString name;
};

// One shot-task whose status we push up to Kitsu (Phase 3b). taskType must match
// a Kitsu task-type name (for_entity = Shot).
struct KitsuTaskPush {
  QString     seq;
  QString     shot;
  QString     taskType;
  TaskStatus  status = TaskStatus::Todo;
  QStringList assignees;  // Ztoryc assignee names to add (add-only) in Kitsu
};

// One asset-task whose status we push up to Kitsu. The asset is resolved by
// type+name (as pushed by pushAssets); taskType must match a Kitsu task-type
// with for_entity = Asset. Mirrors KitsuTaskPush for the asset entity.
struct KitsuAssetTaskPush {
  QString     assetType;
  QString     assetName;
  QString     taskType;
  TaskStatus  status = TaskStatus::Todo;
  QStringList assignees;  // Ztoryc assignee names to add (add-only) in Kitsu
};

// One shot-task status pulled DOWN from Kitsu (review sync). taskType is the
// Kitsu task-type name; the caller matches it onto its Ztoryc task by
// KitsuClient::normalizeTaskType().
struct KitsuPullEntry {
  QString     seq;
  QString     shot;
  QString     kitsuShotId;  // Kitsu shot id — robust link across renames
  QString     taskType;
  TaskStatus  status = TaskStatus::Todo;
  QStringList assignees;  // assignee display names read from Kitsu
};

// One asset-task status pulled DOWN from Kitsu (review sync). Mirrors
// KitsuPullEntry for the asset entity; the caller matches the asset by
// kitsuAssetId first, then type+name, and the task by normalizeTaskType().
struct KitsuAssetStatusEntry {
  QString     assetType;
  QString     assetName;
  QString     kitsuAssetId;  // Kitsu asset id — robust link across renames
  QString     taskType;
  TaskStatus  status = TaskStatus::Todo;
  QStringList assignees;  // assignee display names read from Kitsu
};

//----------------------------------------------------------------------------
// One per-shot preview movie to upload to a Kitsu task (Phase 4: upload-on-
// export). The task is resolved by kitsuShotId + taskType; uploading also sets
// the task status (default Wfa — "waiting for approval" after a board pass).
//----------------------------------------------------------------------------
struct KitsuPreviewUpload {
  QString    kitsuShotId;            // Kitsu shot id (entity) — required
  QString    uuid;                   // project-shot uuid (for local status mirror)
  QString    shot;                   // shot name, for progress / reporting
  QString    taskType = "Storyboard";
  QString    filePath;               // local movie file to upload
  TaskStatus status   = TaskStatus::Wfa;
};

//----------------------------------------------------------------------------
class KitsuClient : public QObject {
  Q_OBJECT
public:
  explicit KitsuClient(QObject *parent = nullptr);
  ~KitsuClient() override;

  // App-lifetime singleton: keeps the JWT session (and fetched projects/statuses)
  // alive across Connect-dialog opens, so the user stays logged in for the session.
  static KitsuClient *instance();

  // --- Config (persisted in QSettings, group "Ztoryc/Kitsu") -----------
  QString baseUrl() const { return m_baseUrl; }
  void    setBaseUrl(const QString &url);  // trailing slash trimmed
  // Optional LAN-direct address of the SAME Kitsu instance, used only for the
  // heavy preview upload. Empty = uploads use the primary URL (baseUrl).
  QString localUrl() const { return m_localUrl; }
  void    setLocalUrl(const QString &url);  // trailing slash trimmed
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
  // Episodes of every tvshow in the last fetchProjects() result, one GET each,
  // emitted as a single episodesFetched() once they are all in — so the caller
  // builds its list once instead of redrawing per reply. Emits immediately with
  // an empty map when there is no tvshow to ask about.
  void fetchEpisodes();      // GET  /api/data/projects/<id>/episodes

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

  // Pull task statuses DOWN from Kitsu (review sync: Kitsu is authoritative on
  // WFA→Done/Retake). -> statusesPulled().
  // `episodeId` restricts the pull to one episode of a tvshow. Empty (the
  // default) keeps the old whole-project behaviour, which is right for
  // productions that have no episodes at all.
  void pullStatuses(const QString &projectId,
                    const QString &episodeId = QString());
  // Pull the BREAKDOWN (Kitsu «casting») of every shot, in ONE call: the bulk
  // endpoint returns a map shot -> assets. With an episode bound it asks only
  // for that episode's sequences. -> breakdownPulled()
  void pullBreakdown(const QString &projectId,
                     const QString &episodeId = QString());

  // Bidirectional asset sync. pushAssets creates the assets missing in Kitsu
  // (upsert by type+name, resolving each canonical type onto a Kitsu asset-type)
  // -> assetsPushed(). pullAssets fetches the project's assets so ones authored
  // in Kitsu can be imported into the tracker -> assetsPulled().
  void pushAssets(const QString &projectId, const QVector<KitsuAsset> &assets);
  // `episodeId` restricts the pull to the assets of one episode of a tvshow.
  // Assets with no episode are the show's and always come through.
  void pullAssets(const QString &projectId,
                  const QString &episodeId = QString());

  // Pull asset TASK statuses DOWN from Kitsu (review sync; mirror of
  // pullStatuses for the asset entity). -> assetStatusesPulled().
  void pullAssetStatuses(const QString &projectId,
                         const QString &episodeId = QString());

  // Pull the project's TEAM (its Kitsu persons) down into the roster used by the
  // assignee picker. -> teamPulled().
  void pullTeam(const QString &projectId);

  // Push asset TASK statuses (mirror of pushTasks for the asset entity): ensure
  // each task exists on its asset and set its status. Chained after pushAssets so
  // the asset entities already exist. -> assetTasksPushed().
  void pushAssetTasks(const QString &projectId,
                      const QVector<KitsuAssetTaskPush> &tasks);

  // Build the asset-task push list from the model: each asset × the ordered
  // task types of its (custom) asset type, with the current status.
  static QVector<KitsuAssetTaskPush> buildAssetTasksFromModel();

  // Build the asset push list from the project's assets (ZtoryModel::assets()).
  static QVector<KitsuAsset> buildAssetsFromModel();

  // Upload per-shot preview movies (Phase 4). For each entry: post a comment
  // that sets the task status, attach a preview to it, then upload the movie
  // file (multipart). Requires task statuses already fetched. -> previewsUploaded().
  void uploadPreviews(const QString &projectId,
                      const QVector<KitsuPreviewUpload> &uploads);

  // Match the movie files in `dir` to the project shots (by shot label) and to a
  // task (by the {TASK} short code in the file name, limited to the shot's
  // technique; defaults to Storyboard). Reads ZtoryModel::projectShots(). Shared
  // by the Connect dialog and the post-export auto-upload. outUnmatched/outNoId
  // report files with no matching shot / matched shots not yet pushed to Kitsu.
  static QVector<KitsuPreviewUpload> buildUploadsFromFolder(const QString &dir,
                                                            int &outUnmatched,
                                                            int &outNoId);

  // Build the shot + task push lists from the project's shots (ZtoryModel::
  // projectShots()), padding frame ranges by `handles`. Shared by the Connect
  // dialog and the Production Tracker's Project tab. outTasks gets the per-task
  // statuses to push after the shots; outSkipped counts label-less shots.
  static QVector<KitsuShotPush> buildShotPushFromProject(
      int handles, QVector<KitsuTaskPush> &outTasks, int &outSkipped);

  // Canonical lowercased task-type key (handles Ztoryc↔Kitsu name aliases
  // Render↔Rendering, VFX↔FX) so push and pull match the same tasks.
  static QString normalizeTaskType(const QString &name);

  QVector<KitsuProject>    projects() const { return m_projects; }
  QVector<KitsuTaskStatus> taskStatuses() const { return m_taskStatuses; }
  // Episodes of a tvshow, from the last fetchEpisodes(). Empty for a project
  // that has none (or that was never asked about).
  QVector<KitsuEpisode>    episodes(const QString &projectId) const {
    return m_episodes.value(projectId);
  }

  // Map a fetched Kitsu status id onto Ztoryc's enum (valid after statuses are
  // fetched). Unknown ids fall back to Todo.
  TaskStatus toZtoryStatus(const QString &kitsuStatusId) const;

  // Name / flag-based mapping used to build the per-id table. Static so it can
  // be unit-reasoned about without a live client.
  static TaskStatus mapStatus(const KitsuTaskStatus &s);

signals:
  void loginFinished(bool ok, const QString &message);
  void projectsFetched(const QVector<KitsuProject> &projects);
  // Keyed by project id; only tvshow projects appear.
  void episodesFetched(const QMap<QString, QVector<KitsuEpisode>> &episodes);
  void breakdownPulled(bool ok, const QVector<KitsuCastingEntry> &entries,
                       const QString &message);
  void taskStatusesFetched(const QVector<KitsuTaskStatus> &statuses);
  void projectCreated(bool ok, const KitsuProject &project, const QString &message);
  void projectUpdated(bool ok, const QString &message);
  void shotsPushProgress(const QString &message);
  // Resolved Kitsu shot ids per "seq\nlabel" key — lets the caller record them so
  // later syncs are rename-proof. Emitted just before shotsPushed.
  void shotIdsResolved(const QHash<QString, QString> &byKey);
  void shotsPushed(bool ok, int created, int updated, const QString &message);
  void tasksPushed(bool ok, int statusesSet, const QString &message);
  void assetTasksPushed(bool ok, int statusesSet, const QString &message);
  void statusesPulled(bool ok, const QVector<KitsuPullEntry> &entries,
                      const QString &message);
  // Resolved Kitsu asset ids per "type\nname" key — lets the caller store them so
  // later syncs are rename-proof. Emitted just before assetsPushed.
  void assetIdsResolved(const QHash<QString, QString> &byKey);
  void assetsPushed(bool ok, int created, int updated, const QString &message);
  void assetsPulled(bool ok, const QVector<KitsuAsset> &assets,
                    const QString &message);
  void assetStatusesPulled(bool ok,
                           const QVector<KitsuAssetStatusEntry> &entries,
                           const QString &message);
  void teamPulled(bool ok, const QVector<KitsuPerson> &persons,
                  const QString &message);
  void previewsUploaded(bool ok, int uploaded, const QString &message);
  void networkError(const QString &message);

private:
  void onLoginReply(QNetworkReply *reply);
  void onProjectsReply(QNetworkReply *reply);
  void onTaskStatusesReply(QNetworkReply *reply);
  // Serialise a KitsuProject into the JSON body POST/PUT expect.
  QByteArray projectBody(const KitsuProject &p) const;
  static KitsuProject parseProject(const class QJsonObject &o);

  // --- Shot push state machine (all steps are sequential async) --------
  // base defaults to m_baseUrl; the preview-upload machine passes uploadBase()
  // so only that traffic is routed to the (optional) LAN-direct endpoint.
  QNetworkRequest authGet(const QString &path,
                          const QString &base = QString()) const;
  QNetworkReply  *authPost(const QString &path, const QByteArray &body,
                           const QString &base = QString());
  QNetworkReply  *authPut(const QString &path, const QByteArray &body,
                          const QString &base = QString());
  // Base URL the current preview upload uses: the local endpoint if it was
  // resolved reachable at the start of uploadPreviews(), else the primary URL.
  QString uploadBase() const {
    return m_uploadBase.isEmpty() ? m_baseUrl : m_uploadBase;
  }
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
  QHash<QString, QString>    m_pushResolved; // "seq\nlabel" -> resolved kitsu shot id
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
  // Map a Ztoryc task-type name onto a Kitsu Shot task-type id, case-insensitively
  // and with name aliases (Render→Rendering, VFX→FX). Empty if Kitsu has no match.
  QString resolveTaskTypeId(const QString &ztorycName) const;

  QString m_taskProjectId;
  QVector<KitsuTaskPush>   m_taskQueue;
  QHash<QString, QString>  m_ttIdByName;     // Kitsu Shot task-type name -> id
  QHash<QString, QString>  m_taskSeqIds;     // seq name -> sequence id
  QHash<QString, QString>  m_taskShotIds;    // "seqId/shotName" -> shot id
  QHash<QString, QString>  m_taskIdByKey;    // "shotId/ttId" -> task id
  QHash<QString, QString>  m_taskStatusByKey; // "shotId/ttId" -> current status id
  QHash<QString, QSet<QString>> m_taskAssigneesByKey; // "shotId/ttId" -> person ids
  QHash<int, QString>      m_statusIdByZ;    // TaskStatus (int) -> Kitsu status id
  QVector<QString>         m_ttCreateQueue;  // task-type ids to create-tasks for
  int m_taskCreateIdx = 0;
  int m_taskApplyIdx  = 0;
  int m_taskStatusesSet = 0;
  int m_taskUnchanged   = 0;  // already at target status -> skipped

  // --- Pull statuses (review sync) -------------------------------------
  void pullLoadSequences();
  void pullLoadShots();
  void pullLoadTaskTypes();
  void pullLoadTasks();
  void pullFail(const QString &message);

  QString m_pullProjectId;
  // When set, the pull is restricted to this episode: a tvshow holds every
  // episode's shots in one project, so without it a six-episode show drops all
  // six episodes' shots into whichever tracker asked.
  QString m_pullEpisodeId;
  QHash<QString, QString> m_pullSeqName;     // sequence id   -> name
  QHash<QString, QString> m_pullShotSeq;     // shot id       -> sequence name
  QHash<QString, QString> m_pullShotName;    // shot id       -> shot name
  QHash<QString, QString> m_pullTtName;      // task-type id  -> Kitsu name

  // --- Asset sync (bidirectional) — sequential async ---------------------
  void asPushLoadTypes();    // GET /api/data/asset-types
  void asPushLoadAssets();   // GET /api/data/projects/<pid>/assets (existing)
  void asPushProcessNext();
  void asPushFail(const QString &message);
  void asPullLoadTypes();
  void asPullLoadAssets();
  void asPullFail(const QString &message);

  QString m_asProjectId;
  QVector<KitsuAsset>     m_asQueue;
  QHash<QString, QString> m_asTypeIdByName;  // asset-type name(lower) -> id (push)
  QHash<QString, QString> m_asTypeNameById;  // id -> asset-type name    (pull)
  QHash<QString, QString> m_asExisting;      // "typeId/name(lower)" -> asset id
  QHash<QString, QString> m_asResolved;      // "type\nname" -> kitsu asset id
  int m_asIndex   = 0;
  int m_asCreated = 0;
  int m_asUpdated = 0;

  // Asset-task push pipeline (mirror of the shot task pipeline for assets).
  void atLoadTaskTypes();    // GET /api/data/task-types (for_entity = Asset)
  void atCreateNext();       // create-tasks per asset task-type
  void atLoadAssetTypes();   // GET /api/data/asset-types (name -> id)
  void atLoadAssets();       // GET project assets ("typeId/name" -> asset id)
  void atLoadTasks();        // GET project tasks ("assetId/ttId" -> task id)
  void atApplyNext();        // set each task status via a comment
  void atFail(const QString &message);

  QString m_atProjectId;
  QVector<KitsuAssetTaskPush> m_atQueue;
  QHash<QString, QString> m_atTtIdByName;       // Asset task-type name(lower) -> id
  QHash<QString, QString> m_atAssetTypeIdByName;// asset-type name(lower) -> id
  QHash<QString, QString> m_atAssetIds;         // "typeId/name(lower)" -> asset id
  QHash<QString, QString> m_atTaskIdByKey;      // "assetId/ttId" -> task id
  QHash<QString, QString> m_atStatusByKey;      // "assetId/ttId" -> current status id
  QHash<QString, QSet<QString>> m_atAssigneesByKey; // "assetId/ttId" -> person ids
  QVector<QString>        m_atTtCreateQueue;    // task-type ids to create-tasks for
  int m_atCreateIdx    = 0;
  int m_atApplyIdx     = 0;
  int m_atStatusesSet  = 0;
  int m_atUnchanged    = 0;  // already at target status -> skipped

  // --- Asset-task status pull (review sync; mirror of the shot pull) ------
  void apLoadTypes();      // GET /api/data/asset-types (id -> name)
  void apLoadAssets();     // GET project assets (id -> name/type)
  void apLoadTaskTypes();  // GET /api/data/task-types (for_entity = Asset)
  void apLoadTasks();      // GET project tasks -> build entries
  void apFail(const QString &message);

  QString m_apProjectId;
  QHash<QString, QString> m_apAssetTypeName;  // asset-type id -> name
  QHash<QString, QString> m_apAssetName;      // asset id      -> name
  QHash<QString, QString> m_apAssetType;      // asset id      -> asset-type name
  QHash<QString, QString> m_apTtName;         // task-type id  -> Kitsu name (Asset)

  // --- Team / assignees (persons) — shared by pulls and the assign pass ---
  // GET /api/data/persons (id<->name maps) then GET the project (team person
  // ids into m_teamPersonIds), then run `next`. The team set is what the assign
  // pass is allowed to touch, so Ztoryc never assigns anyone outside the team.
  void loadRosterThen(const QString &projectId, std::function<void()> next);
  void teamFail(const QString &message);
  // Assign each (taskId, personId) in m_assignQueue sequentially (add-only),
  // then invoke m_assignOnDone. Kitsu route: PUT /actions/tasks/<id>/assign.
  void assignRun();

  QString m_teamProjectId;
  QHash<QString, QString> m_personNameById;   // person id     -> display name (all)
  QHash<QString, QString> m_personIdByName;   // name(lower)   -> person id  (all)
  // Why the persons list failed, when it did. Kept so the team pull can report
  // the real cause instead of guessing at one.
  QString m_rosterError;
  // Episode the current asset pull is scoped to (empty = whole project).
  // Shared by both asset chains, which is why it isn't one of their m_as*/m_ap*
  // fields: they must answer the same question the same way.
  QString m_entityEpisodeId;
  // True when this Kitsu entity belongs in the bound episode: its own episode,
  // or no episode at all (a shared asset from the show's library).
  bool episodeScoped(const class QJsonObject &entity) const;
  QSet<QString>           m_teamPersonIds;    // project team person ids (assignable)
  QVector<QPair<QString, QString>> m_assignQueue;  // (taskId, personId) to add
  int m_assignIdx = 0;
  std::function<void()> m_assignOnDone;

  // --- Preview upload (Phase 4) — sequential async, 3 steps per entry ----
  // Resolve which base URL the upload uses (local if reachable, else primary),
  // then kick off the upload machine.
  void uplProbeLocalThenRun();
  void uplLoadTaskTypes();
  void uplEnsureTasks();  // create-tasks for the task-types we'll upload to
  void uplLoadTasks();
  void uplProcessNext();
  void uplPostComment(const QString &taskId, const QString &statusId,
                      const QString &filePath, const QString &shot);
  void uplAddPreview(const QString &taskId, const QString &commentId,
                     const QString &filePath, const QString &shot);
  void uplUploadFile(const QString &previewFileId, const QString &filePath);
  void uplSetMainPreview(const QString &previewFileId);  // -> shot thumbnail
  void uplFail(const QString &message);

  QString m_uplProjectId;
  QString m_uploadBase;  // resolved per run: local URL if reachable, else empty
  QVector<KitsuPreviewUpload> m_uplQueue;
  QHash<QString, QString> m_uplTaskIdByKey;  // "entityId/ttId" -> task id
  QVector<QString>        m_uplTtCreate;     // distinct task-type ids to ensure
  int m_uplTtIdx = 0;
  int m_uplIndex = 0;
  int m_uplDone  = 0;

  QNetworkAccessManager *m_nam = nullptr;

  QString m_baseUrl;
  QString m_localUrl;  // optional LAN-direct address for uploads (see uploadBase)
  QString m_email;
  QString m_password;
  bool    m_passwordSaved = false;

  QString m_accessToken;
  QString m_refreshToken;
  QString m_userRole;
  bool    m_syncAfterLogin = false;  // connectAndSync() pending

  QVector<KitsuProject>      m_projects;
  QMap<QString, QVector<KitsuEpisode>> m_episodes;  // by project id (tvshow only)
  int                        m_episodesPending = 0; // replies still in flight
  QVector<KitsuTaskStatus>   m_taskStatuses;
  QHash<QString, TaskStatus> m_statusById;  // built after fetchTaskStatuses()
};
