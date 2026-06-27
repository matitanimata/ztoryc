#pragma once
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QHash>
#include <vector>
#include <set>
#include "toonz/txshchildlevel.h"  // for TXshLevelP
#include "traster.h"               // TRaster32P (export-to-board panels)

// ─── NumberingConfig ─────────────────────────────────────────────────────────
// Persistent numbering scheme used both at startup and during Board editing.

struct NumberingConfig {
  enum Style { Simple, Sequence } style = Simple;
  QString shotPrefix  = "SH";
  QString seqPrefix   = "SQ";
  QString panelPrefix = "P";
  int     step        = 10;
  int     padding     = 3;
  int     seqPadding  = 3;
  int     startNumber = 10;
  int     seqNumber   = 1;   // active sequence number (for Sequence style)
  // When true (Sequence style only): shot counter resets to startNumber each
  // time the sequence changes (SQ01→SH010, SQ02→SH010…).
  // When false: shot counter is global and continuous across all sequences.
  bool    resetOnSeqChange = false;

  // Returns the shot name for a 0-based index (legacy; new code uses shotLabel)
  QString shotName(int idx) const;
};

// ─── Shared clipboard ─────────────────────────────────────────────────────────
// Used by both StoryboardPanel and ZtoryAnimaticPanel so that copy/cut in one
// panel is immediately available for paste in the other.

struct ZtoryClipEntry {
  int        srcCol   = 0;    // xsheet column at copy/clone time; -1 for cut
  int        duration = 24;   // cell count of original column
  bool       isCut    = false;
  bool       isClone  = false;
  TXshLevelP cutLevel;        // keeps sub-scene alive after immediate cut
};

// ─── SequenceData ─────────────────────────────────────────────────────────────
// One sequence (e.g. "SQ010"). Shots belong to a sequence via sequenceId==uuid.

struct SequenceData {
  QString uuid;           // immutable unique key (assigned once at creation)
  QString label;          // "SQ010" — stable display label
  int     orderIndex = 10;
};

// ─── Strutture dati ───────────────────────────────────────────────────────────

struct PanelData {
  int     startFrame;
  int     duration;
  QString dialog;
  QString action;
  QString notes;
  QString panelLabel;    // "P003" — stable identifier within shot
  int     orderIndex = 0;  // sort key (panel position)

  // ── Camera movement overlay ───────────────────────────────────────────────
  // Populated by detectAndUpdatePanels() when the panel boundary is a camera
  // keyframe. Used to draw the IN/OUT rectangle overlay on thumbnails.
  enum CameraMove { CamNone, CamTrkIn, CamTrkOut, CamPan, CamTilt, CamCombined };
  CameraMove cameraMoveType = CamNone;
  QString    cameraMoveLabel;   // auto-generated but user-editable ("Trk In" etc.)
  // Camera affine at startFrame (a11,a12,a13, a21,a22,a23 — row-major)
  double camA0[6] = {1,0,0, 0,1,0};
  // Camera affine at last frame of panel
  double camA1[6] = {1,0,0, 0,1,0};
  // Camera physical size in stage units (from TCamera::getSize())
  double camW = 0, camH = 0;
  // Which frame to use for the thumbnail render (startFrame or end frame)
  int    camRenderFrame = 0;  // absolute frame in sub-scene

  // ── Light-direction gizmo (task 40 FASE 3) ───────────────────────────────
  // User-placed 3D conical arrow: tail = light source (sun glyph), tip =
  // where the light goes.  Coordinates are normalized 0-1 over the thumbnail
  // (origin top-left) so the gizmo survives any render size (Board, PDF).
  bool    hasLight   = false;
  double  lightTailX = 0.25, lightTailY = 0.25;
  double  lightTipX  = 0.65, lightTipY  = 0.65;
  // Z component of the direction: -1 = into the background, +1 = toward the
  // camera, 0 = parallel to the picture plane. Drawn with foreshortening.
  double  lightDepth = 0.0;
  // Beam opening angle in degrees (full angle): narrow = hard spotlight,
  // wide = soft/ambient light. Storyboard conic-arrow convention.
  double  lightSpread = 35.0;
  QString lightColor = "#FFC34D";  // colour = light temperature

  PanelData() : startFrame(0), duration(24) {}
};

// ─── Production tasks (Kitsu-aligned) ─────────────────────────────────────────
// A shot's task list depends on its production technique (Tradigital, Cut-out,
// …).  Each technique defines an ordered set of task types; each shot picks a
// technique and gets that set.  Statuses mirror Kitsu's pipeline so the data
// maps cleanly onto a future Kitsu sync (M5).

enum class TaskStatus { Todo, Ready, Wip, Wfa, Retake, Done };

// One task type's state inside a shot.
struct TaskState {
  TaskStatus  status = TaskStatus::Todo;
  QStringList assignees;  // people assigned (free text / Kitsu user names) — multiple
};

// A named production technique = ordered list of task-type names.
// Presets are editable and persisted in the .ztoryc.
struct Technique {
  QString     name;        // "Tradigital", "Cut-out", …
  QStringList taskTypes;   // ordered task-type names for this technique
};

struct ShotData {
  int                    xsheetColumn;
  QString                uuid;          // immutable project-unique key (assigned once); back-link to project tracker
  QString                shotNumber;    // legacy field — kept for XML backward compat
  QString                shotLabel;     // "SH020" — stable identifier (primary, v4+)
  int                    orderIndex = 0;  // sort key (100 × label number for step-10)
  QString                sequenceId;    // uuid of parent SequenceData (may be empty)
  std::vector<PanelData> panels;

  int transitionFrames = 0; // total overlap frames for cross-dissolve (T/2 tail in A, T/2 head in B)

  // ── Production tracking (spreadsheet / Kitsu) ──
  QString                  technique;  // Technique::name; empty = project default
  QMap<QString, TaskState> tasks;      // keyed by task-type name (only its technique's types)
  QString                  notes;      // spreadsheet "Notes" (general)
  QString                  vfxNotes;   // spreadsheet "VFX Notes"

  ShotData() : xsheetColumn(0) {}

  // Returns shotLabel if set, else shotNumber (backward compat with v1-v3 files)
  QString label() const { return shotLabel.isEmpty() ? shotNumber : shotLabel; }

  int totalDuration() const {
    int tot = 0;
    for (const auto &p : panels) tot += p.duration;
    return tot;
  }
};

// A production asset (character, prop, environment, …). Project-level entity,
// source-agnostic: it may have art attached or be just a name from the script
// breakdown. Has its own task pipeline, mirroring shot tasks.
struct Asset {
  QString                  uuid;   // stable project-unique key
  QString                  type;   // "Character" / "Prop" / "Environment" / "BG" / "FX"
  QString                  name;
  QMap<QString, TaskState> tasks;  // keyed by asset task-type name
  QStringList              tags;   // free categorisation (future: breakdown / AI)
};

// A project-level shot record. Owns the production progress (task status/
// assignee). The structural metadata (seq/label/frames/technique) is authored
// by the storyboard .tnz and published here via publishShotsToProjectDb().
struct ProjectShot {
  QString uuid;
  QString source;    // basename of the originating .tnz storyboard (e.g. "reel1.tnz")
  QString seq;       // e.g. "SQ010"
  QString label;     // e.g. "SH010"
  int     frames = 0;
  QString technique;
  QString kitsuShotId;  // Kitsu shot id once synced — keeps the link across renames
  QMap<QString, TaskState> tasks;  // progress — authoritative for the project DB
};

// ─── Animatic export burn-in ──────────────────────────────────────────────────
// Set by the export-animatic dialog right before MI_Render and cleared after.
// rendercommand.cpp reads it at render SETUP time and pins it into the
// MovieRenderer (so async/batch renders can't pick up a stale config).

struct ZtoryBurnInSeg {
  int     from = 0, to = 0;   // inclusive scene-frame range
  QString label;              // e.g. "SQ010_SH020_P001"
};

struct ZtoryBurnInConfig {
  bool timecode  = false;
  bool shotNames = false;
  std::vector<ZtoryBurnInSeg> segments;
  bool isActive() const { return timecode || shotNames; }
};

// ─── Workflow ─────────────────────────────────────────────────────────────────
// Authoritative global workflow state.  Set via ZtoryModel::setWorkflow() at
// every transition point; query via ZtoryModel::currentWorkflow().

enum class ZtoryWorkflow {
  Storyboard,   // Storyboard Mode
  Tradigital,   // 2D Tradigital Mode
  CutoutDigital, // Cutout Digital Mode
  StopMotion,   // Stop-Motion Mode
};

// ─── ZtoryModel ───────────────────────────────────────────────────────────────

class ZtoryModel : public QObject {
  Q_OBJECT

  std::vector<ShotData>             m_shots;
  std::vector<Asset>                m_assets;       // project-level asset list
  std::vector<ProjectShot>          m_projectShots; // all project shots (from production.ztrack)
  QVector<QString>                  m_storyboardFiles; // registered storyboard basenames
  std::vector<SequenceData>         m_sequences;
  std::vector<std::vector<QPixmap>> m_previews; // [shotIdx][panelIdx]
  int                               m_fps;
  QString                           m_ztoryPath;
  // Imported screenplay, stored as a path relative to the project ("+extras/
  // script/<file>").  Persisted in the .ztoryc so the Script panel can reload
  // it when the scene is reopened.  Empty = no screenplay imported.
  QString                           m_scriptFile;
  QString                           m_production;  // user-defined production name
  QString                           m_code;        // short project code (Kitsu code, e.g. CS26)
  QString                           m_title;       // user-defined project title
  QString                           m_episode;     // user-defined episode
  QString                           m_season;      // user-defined season (e.g. CS26)
  QString                           m_namingPattern; // export naming convention (B3d)
  // Kitsu (M5) — project metadata kept aligned with the bound Kitsu project so
  // the Production Tracker mirrors it as closely as possible.
  QString                           m_kitsuProjectId;   // empty = not linked
  QString                           m_kitsuProjectName; // cached for display
  QString                           m_productionType;   // Kitsu: tvshow/short/featurefilm
  QString                           m_productionStyle;  // Kitsu: 2d/3d/2d3d
  QString                           m_ratio;            // Kitsu: e.g. 16:9
  QString                           m_resolution;       // Kitsu: e.g. 1920x1080
  std::vector<Technique>            m_techniques;       // editable presets (seeded with defaults)
  QString                           m_defaultTechnique; // project default technique name
  QStringList                       m_team;             // project roster (names) for the assignee picker
  QString                           m_pdfLogoPath; // custom PDF header logo (abs or scene-relative); empty = default Ztoryc logo
  bool                              m_pdfNoLogo = false;  // true = export PDF with no logo at all
  ZtoryWorkflow                     m_workflow = ZtoryWorkflow::Tradigital;
  std::vector<ZtoryClipEntry>       m_sharedClip;
  std::set<int>                     m_sharedSelection;
  NumberingConfig                   m_numberingConfig;
  ZtoryBurnInConfig                 m_burnIn;

  // Side-panel toggle: which panel types to show in animatic vs shot mode.
  // Defaults are reasonable; user can customise via ZtoryModel API.
  QStringList m_animaticSidePanels;  // shown in animatic mode
  QStringList m_shotSidePanels;      // shown in shot mode
  bool        m_sidePanelsLinked = true;  // if true, viewer toggle drives side panels
  // Auto-match: shared state so both ANIMATIC toolbar and SHOTEDITOR
  // ZtoryPanelNavigator always reflect the same toggle.
  bool        m_autoMatch = false;

  // Persistent thumbnail cache keyed by shot uuid — survives scene switches so
  // the Production Tracker can show thumbnails for all loaded storyboards.
  QHash<QString, QPixmap> m_thumbCache;

  ZtoryModel();

public:
  static ZtoryModel *instance();

  // ── Accesso dati ──────────────────────────────────────────────────────────
  int  shotCount() const { return (int)m_shots.size(); }
  ShotData       &shot(int i)       { return m_shots[i]; }
  const ShotData &shot(int i) const { return m_shots[i]; }

  // Returns the shot index for a given xsheet column, or -1 if not found.
  int  shotIndexForCol(int col) const;
  std::vector<ShotData>       &shots()       { return m_shots; }
  const std::vector<ShotData> &shots() const { return m_shots; }
  int  fps() const { return m_fps; }
  void setFps(int fps) { if (fps > 0) m_fps = fps; }
  QString production() const { return m_production; }
  QString title()      const { return m_title; }
  QString episode()    const { return m_episode; }
  void setProduction(const QString &s) { m_production = s; }
  void setTitle(const QString &s)      { m_title = s; }
  void setEpisode(const QString &s)    { m_episode = s; }
  QString season() const { return m_season; }
  void    setSeason(const QString &s) { m_season = s; }
  // Short project code (Kitsu code), used as the {CODE} naming token.
  QString code() const { return m_code; }
  void    setCode(const QString &s) { m_code = s; }
  // Kitsu binding + mirrored project metadata.
  QString kitsuProjectId()   const { return m_kitsuProjectId; }
  QString kitsuProjectName() const { return m_kitsuProjectName; }
  bool    isKitsuLinked()    const { return !m_kitsuProjectId.isEmpty(); }
  void    setKitsuProject(const QString &id, const QString &name) {
    m_kitsuProjectId = id; m_kitsuProjectName = name;
  }
  QString productionType()  const { return m_productionType; }
  void    setProductionType(const QString &s) { m_productionType = s; }
  QString productionStyle() const { return m_productionStyle; }
  void    setProductionStyle(const QString &s) { m_productionStyle = s; }
  QString ratio()       const { return m_ratio; }
  void    setRatio(const QString &s) { m_ratio = s; }
  QString resolution()  const { return m_resolution; }
  void    setResolution(const QString &s) { m_resolution = s; }
  QString namingPattern() const { return m_namingPattern; }
  void    setNamingPattern(const QString &s) { m_namingPattern = s; }
  // Project team roster (people names) used by the Production Tracker's
  // assignee picker. Project-level metadata (persisted in the .ztoryc for now).
  const QStringList &team() const { return m_team; }
  void setTeam(const QStringList &t) { m_team = t; }
  // Project-level DB file (production.ztrack at the project root). B3 pilot:
  // for now it owns the team roster (truly project-wide, shared across the
  // project's scenes). Assets/techniques/shots will migrate here next.
  void loadProjectDb();                              // from current project folder
  void loadProjectDbFromPath(const QString &path);  // B3c: load from explicit path
  void saveProjectDb();
  // Persist project-DB edits made directly via projectShots_rw() and refresh the
  // Production Tracker (used by the Kitsu pull/review sync). One save + one signal.
  void saveAndNotifyTasks();
  QString projectDbPath() const;
  // Non-const access to project shots (used by B3c auto-WIP).
  std::vector<ProjectShot> &projectShots_rw() { return m_projectShots; }
  // Thumbnail cache — persisted in <project>/thumbs/<uuid>.png.
  const QHash<QString, QPixmap> &thumbCache() const { return m_thumbCache; }
  void updateThumbCache(const QString &uuid, const QPixmap &pm);
  void evictThumbFromDisk(const QString &uuid);  // remove stale PNG on uuid regeneration
  void loadThumbsFromDisk();   // call after loadProjectDb to pre-fill cache

  // B3b — Project shots (multi-storyboard)
  const std::vector<ProjectShot> &projectShots() const { return m_projectShots; }
  // Cumulative frame ranges [in,out] (1-based) for each project shot, in
  // m_projectShots order, reset at every source storyboard change. Maps onto
  // Kitsu's frame_in/frame_out (the shot's start/end timecode in the edit).
  std::vector<std::pair<int, int>> projectShotFrameRanges() const;
  const QVector<QString> &storyboardFiles() const { return m_storyboardFiles; }
  // Upsert scene shots (m_shots) into m_projectShots keyed by uuid; sourceFile
  // is the .tnz basename.  Preserves existing task progress; initialises tasks
  // for new shots from the scene's current task state. Removes shots whose
  // source==sourceFile but whose uuid is no longer in the scene.
  void publishShotsToProjectDb(const QString &sourceFile);
  // Task editing on project shots — authoritative, persist with saveProjectDb().
  void setProjectShotTaskStatusByUuid(const QString &uuid,
                                      const QString &taskType, TaskStatus status);
  void setProjectShotAssigneesByUuid(const QString &uuid,
                                     const QString &taskType,
                                     const QStringList &assignees);
  void setProjectShotTechnique(const QString &uuid, const QString &technique);
  // Helper: effective technique for a ProjectShot (falls back to project default).
  QString techniqueForProjectShot(const ProjectShot &ps) const;
  QStringList taskTypesForProjectShot(const ProjectShot &ps) const;

  // B3d — Naming convention
  // Resolve m_namingPattern substituting token map. Tokens: PROD, SEASON, EP,
  // SEQ, SHOT, TASK, VER. Optional format suffix: {VER:02} → zero-padded.
  QString resolveNamingPattern(const QMap<QString,QString> &tokens) const;
  // Resolve an arbitrary pattern string (same token grammar). Lets callers use
  // a derived pattern, e.g. the project pattern with the {TASK} token removed.
  static QString resolvePattern(const QString &pattern,
                                const QMap<QString,QString> &tokens);
  // Default short code for a task type (Layout→LAY, Animation→ANIM, …).
  // Used as the {TASK} token in the naming pattern.
  static QString taskShortCode(const QString &taskType);

  // App-level preference: when true, opening a scene auto-switches to the
  // room/workflow matching its role/technique. Persisted via QSettings.
  static bool autoWorkflowDetection();
  static void setAutoWorkflowDetection(bool on);
  // Workflow/room command id (MI_Workflow*) for a scene with the given role and
  // technique. role "storyboard" → Storyboard; role "shot" → its technique's
  // workflow (Cut-out/Stop-motion/…); unknown techniques → 2D Tradigital.
  static QString workflowCommand(const QString &role, const QString &technique);

  // Set a shot's per-task status (in-app source of truth for production
  // tracking). Emits taskStatusChanged() so the Production Tracker refreshes;
  // deliberately does NOT emit shotDataChanged (which triggers Board/Animatic
  // thumbnail re-bakes — unwanted for a pure status edit).
  void setShotTaskStatus(int shotIdx, const QString &taskType, TaskStatus status);
  // Same, keyed by stable shotLabel (used by undo, which may run after the
  // shot order changed). No-op if the label is not found.
  void setShotTaskStatusByLabel(const QString &shotLabel, const QString &taskType,
                                TaskStatus status);
  // Per-task assignees (multiple). Same dual API as the status setters.
  void setShotTaskAssignees(int shotIdx, const QString &taskType,
                            const QStringList &assignees);
  void setShotTaskAssigneesByLabel(const QString &shotLabel, const QString &taskType,
                                   const QStringList &assignees);

  // ── Assets (project-level) ─────────────────────────────────────────────────
  const std::vector<Asset> &assets() const { return m_assets; }
  std::vector<Asset>       &assets()       { return m_assets; }
  int  assetCount() const { return (int)m_assets.size(); }
  void addAsset(const QString &type, const QString &name);  // assigns a uuid; emits
  void removeAssetAt(int i);
  // Asset task pipeline (fixed for now; the Workflows editor will make it custom).
  static const QStringList &canonicalAssetTaskOrder();
  // Edit asset tasks — keyed by index (live) or by uuid (undo-safe across reorders).
  void setAssetTaskStatus(int i, const QString &taskType, TaskStatus status);
  void setAssetTaskStatusByUuid(const QString &uuid, const QString &taskType,
                                TaskStatus status);
  void setAssetTaskAssignees(int i, const QString &taskType,
                             const QStringList &assignees);
  void setAssetTaskAssigneesByUuid(const QString &uuid, const QString &taskType,
                                   const QStringList &assignees);

  // ── Production techniques / tasks (Kitsu-aligned) ──────────────────────────
  const std::vector<Technique> &techniques() const { return m_techniques; }
  std::vector<Technique>       &techniques()       { return m_techniques; }
  void seedDefaultTechniques();                       // populate presets if empty
  const Technique *findTechnique(const QString &name) const;
  QString defaultTechnique() const { return m_defaultTechnique; }
  void    setDefaultTechnique(const QString &s) { m_defaultTechnique = s; }
  // Effective technique name for a shot (its own, else project default).
  QString techniqueForShot(int shotIdx) const;
  // Ordered task-type names that apply to a shot (from its technique).
  QStringList taskTypesForShot(int shotIdx) const;
  // Union of task-type names across all techniques actually used by the shots,
  // in canonical column order — the spreadsheet's task columns.
  QStringList spreadsheetTaskColumns() const;
  // Canonical ordering of all known task types (drives spreadsheet column order).
  static const QStringList &canonicalTaskOrder();
  // Status <-> label (TODO/READY/WIP/WFA/RETAKE/DONE) for persistence + export.
  static QString    taskStatusLabel(TaskStatus s);
  static TaskStatus taskStatusFromLabel(const QString &s);
  // PDF export logo: empty path + !noLogo → default Ztoryc logo; path set → custom; noLogo → none.
  QString pdfLogoPath() const { return m_pdfLogoPath; }
  void    setPdfLogoPath(const QString &s) { m_pdfLogoPath = s; }
  bool    pdfNoLogo() const { return m_pdfNoLogo; }
  void    setPdfNoLogo(bool b) { m_pdfNoLogo = b; }

  // ── Sequences ─────────────────────────────────────────────────────────────
  const std::vector<SequenceData>& sequences() const { return m_sequences; }
  std::vector<SequenceData>&       sequences()       { return m_sequences; }
  SequenceData* findSequence(const QString &uuid);
  void ensureDefaultSequence();
  // Find sequence by label (case-insensitive); create it with a new UUID if absent.
  SequenceData* findOrCreateSequence(const QString &label);

  // ── Preview ───────────────────────────────────────────────────────────────
  QPixmap preview(int shotIdx, int panelIdx) const;
  void    updatePreview(int shotIdx, int panelIdx);
  void    updateAllPreviews();

  // ── Operazioni su shot ────────────────────────────────────────────────────
  void addShot(int insertAt = -1);
  // Creates a fully-wired shot (xsheet column + sub-scene) with a given name.
  // Used by ZtoryStartupDialog to pre-populate a new project.
  void addShotNamed(const QString &name);
  // Creates a fully-wired multi-panel shot from a list of panel rasters (one
  // drawing per panel).  Builds an OVL raster level (saved under drawings/), a
  // sub-scene exposing those drawings as a sequence — each held kPanelHoldFrames
  // frames — then a column in the main xsheet.  Used by the Thumbnail room
  // export-to-board.  Board picks the shot up via modelReset()/onModelResequenced.
  void addShotFromRasters(const QString &name,
                          const std::vector<TRaster32P> &panels);
  // Clears model data only (no xsheet changes). Call before re-populating.
  void clearShots() { m_shots.clear(); m_previews.clear(); }
  // Replace the model's shot list with the given one (the current scene's shots,
  // authored by the Board). Used right before publishing to the project DB so a
  // previously-open larger scene's leftover shots never leak into this project.
  void setShotsFrom(const std::vector<ShotData> &shots) {
    m_shots = shots;
    m_previews.resize(m_shots.size());
  }
  void removeShot(int shotIdx);
  void moveShot(int fromIdx, int toIdx);
  void cloneShot(int shotIdx);
  // Sync panel data (and optionally the shot label + xsheet column) from the
  // Board into ZtoryModel.  Updates m_shots[si].panels and resizes
  // m_previews[si].  Grows m_shots if needed.  When label is non-empty it is
  // written to shotLabel/shotNumber.  xsheetCol = -1 means "don't update".
  void syncShotPanels(int si, const std::vector<PanelData> &panels,
                      const QString &label = {}, int xsheetCol = -1);

  // ── Numerazione / Labelling ───────────────────────────────────────────────
  void    renumberAll();                    // full renumber using m_numberingConfig
  void    assignKeepNumbers(int insertAt);  // letter-suffix for Keep-# mode (legacy)
  QString nextShotName() const;             // next auto name after existing shots

  void setNumberingConfig(const NumberingConfig &cfg);
  NumberingConfig       &numberingConfig()       { return m_numberingConfig; }
  const NumberingConfig &numberingConfig() const { return m_numberingConfig; }

  // Assign shotLabel + orderIndex to shot at si using the midpoint algorithm.
  // Falls back to alphabetical suffix (e.g. "SH010A") when no integer space.
  // Also keeps shotNumber in sync for backward compat.
  void generateShotLabel(int si);

  // Static variant — works on any vector<ShotData>.
  // Used by both ZtoryModel::generateShotLabel() and StoryboardPanel when its
  // local m_shots list is temporarily projected to ShotData.
  static void assignShotLabel(std::vector<ShotData> &shots, int si,
                               const NumberingConfig &cfg);

  // Bulk-reassign all shotLabels with clean step-10 numbering.
  // Resets orderIndex too. Does NOT ask for confirmation — caller must.
  void cleanRenumber();

  // Assign panelLabel (step-1: P001, P002, …) to all panels in shot si.
  void generatePanelLabels(int si);

  // Full label including sequence prefix: "SQ010_SH020".
  // Returns just label() if no sequence is assigned or found.
  QString fullLabel(int shotIdx) const;

  // ── Panel automatici ──────────────────────────────────────────────────────
  void detectAndUpdatePanels(int shotIdx);
  void refreshFromScene();

  // ── Persistenza ───────────────────────────────────────────────────────────
  void save();
  void load();
  void setZtoryPath(const QString &path) { m_ztoryPath = path; }

  // Imported screenplay path (project-relative, e.g. "+extras/script/x.fdx").
  QString scriptFile() const { return m_scriptFile; }
  void setScriptFile(const QString &path) {
    if (m_scriptFile == path) return;
    m_scriptFile = path;
    emit scriptFileChanged();
  }

  // ── Shared clipboard (Board ↔ Animatic) ──────────────────────────────────
  const std::vector<ZtoryClipEntry>& sharedClip() const { return m_sharedClip; }
  void setSharedClip(std::vector<ZtoryClipEntry> v)     { m_sharedClip = std::move(v); }

  // ── Shared selection (Board ↔ Animatic) — xsheet column indices ─────────
  // Written by whichever panel last had user interaction.
  const std::set<int>& sharedSelection() const { return m_sharedSelection; }
  void setSharedSelection(std::set<int> s) {
    if (s == m_sharedSelection) return;  // no-op guard breaks update loops
    m_sharedSelection = std::move(s);
    emit sharedSelectionChanged();
  }

  // ── Resequencing ──────────────────────────────────────────────────────────
  void resequenceXsheet();

  // Returns true if at main xsheet level; optionally shows a warning dialog.
  static bool assertMainXsheet(bool showWarning = true);

  // ── Workflow state ────────────────────────────────────────────────────────
  ZtoryWorkflow currentWorkflow() const { return m_workflow; }
  void setWorkflow(ZtoryWorkflow w);

  // Convenience: true when currentWorkflow() == ZtoryWorkflow::Storyboard.
  bool isStoryboardWorkflow() const {
    return m_workflow == ZtoryWorkflow::Storyboard;
  }

  // ── Sincronizzazione scena ────────────────────────────────────────────────
  void onXsheetChanged();
  void onSceneChanged();
  void updateColumnName(int shotIdx);

  // Viewer switch helpers — call these instead of emitting signals directly.
  // They decouple Board/Timeline from ZtoryAnimaticViewerPanel.
  void activateShotForViewing(int col);   // emits shotActivatedForViewing
  void requestReturnToViewer();           // emits returnToViewerMainRequested

  // Side-panel toggle configuration.
  bool        sidePanelsLinked()    const { return m_sidePanelsLinked; }
  void        setSidePanelsLinked(bool on) { m_sidePanelsLinked = on; }
  QStringList animaticSidePanels()  const { return m_animaticSidePanels; }
  QStringList shotSidePanels()      const { return m_shotSidePanels; }
  void        setAnimaticSidePanels(const QStringList &l) { m_animaticSidePanels = l; }
  void        setShotSidePanels(const QStringList &l)     { m_shotSidePanels = l; }

  // Animatic export burn-in config (see ZtoryBurnInConfig above).
  ZtoryBurnInConfig       &burnIn()       { return m_burnIn; }
  const ZtoryBurnInConfig &burnIn() const { return m_burnIn; }

  // Auto-match toggle — shared between ANIMATIC toolbar and SHOTEDITOR navigator
  bool autoMatch() const { return m_autoMatch; }
  void setAutoMatch(bool on) {
    if (m_autoMatch == on) return;
    m_autoMatch = on;
    emit autoMatchChanged(on);
  }

  // Clear/re-seed all project-level fields (production/season/title/episode/team/
  // assets/techniques/projectShots) so data never leaks across scenes/projects.
  // Called on scene switch BEFORE the .ztoryc migration read and loadProjectDb().
  void resetProjectLevelDefaults();

private:
  void loadProjectDbFromDevice(QIODevice &dev);  // shared XML parser

private slots:
  // Global sceneSwitched handler: when an exported shot scene (role="shot")
  // becomes current, advance its first pipeline task Ready/Todo→WIP. Lives here
  // (not only in StoryboardPanel) so it fires regardless of the current room.
  void onSceneSwitchedAdvanceShot();

signals:
  // Overlay display settings changed (light visibility/colour, camera-move
  // label toggle — all persisted in QSettings by the emitter). Board and
  // Shot Board listen to mirror button states and re-bake their previews,
  // so the toggles stay in lockstep across panels.
  void overlayDisplayChanged();
  void workflowChanged(ZtoryWorkflow workflow);
  void modelReset();                          // tutto cambiato
  void shotAdded(int shotIdx);
  void shotRemovedAt(int col);  // col = xsheet column deleted (symmetric with shotAdded)
  void shotRemoved(int shotIdx);
  void shotMoved(int fromIdx, int toIdx);
  void shotDataChanged(int shotIdx);
  void taskStatusChanged();  // a per-task status was edited (Production Tracker)
  void assetsChanged();      // asset list or an asset task changed (Production Tracker)
  void productionReloaded(); // .ztoryc finished loading (project/team/assets populated)
  void previewUpdated(int shotIdx, int panelIdx);
  void scriptFileChanged();  // imported screenplay changed (or cleared)
  void autoMatchChanged(bool on);  // auto-match toggle flipped
  // Viewer-switch signals: emitted by activateShotForViewing / requestReturnToViewer.
  // ZtoryAnimaticViewerPanel connects to these to switch stack pages.
  void shotActivatedForViewing(int col);
  void returnToViewerMainRequested();
  // Shared selection changed — Board and Animatic listen to mirror the
  // highlighted shot/clip across panels. The panel that caused the change
  // must NOT re-apply (it already shows the selection); appliers must not
  // write back to setSharedSelection or an update loop forms.
  void sharedSelectionChanged();
};
