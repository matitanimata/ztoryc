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

// A named asset type = its own ordered asset-task pipeline (Kitsu-aligned: each
// asset type — Character, Prop, … — carries its own tasks). Editable/custom and
// persisted in the project DB, exactly like Technique does for shots.
struct AssetType {
  QString     name;        // "Character", "Prop", "Environment", …
  QStringList taskTypes;   // ordered asset-task names for this type
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

// Una battuta estratta dal testo di un pannello: chi la dice e cosa dice.
//
// Il dialogo NON e' un campo strutturato ed e' una scelta: nel Board si incolla
// dallo script, e in sceneggiatura il personaggio e' gia' scritto nel testo.
// Costringere a compilare due campi dove se ne incolla uno solo avrebbe fatto
// perdere l'unica cosa che rende il lipsync automatico — che il testo c'e' gia'.
struct DialogueLine {
  QString character;   // nome come scritto nel testo, gia' ripulito
  QString assetUuid;   // asset di tipo Character corrispondente, se trovato
  QString text;        // cio' che viene DETTO (niente didascalie)
  bool    matched = false;  // false = nome nel testo che non e' un personaggio
};

// Una parola con il suo tempo, dopo il riallineamento fra cio' che Whisper ha
// SENTITO e cio' che il copione DICE.
//
// Il testo viene dal copione, i tempi da Whisper. Non e' un compromesso: e'
// l'unica combinazione affidabile. Misurato il 2026-08-15 — «credi» diventa
// «cre di» col modello tiny, e col modello base si aggiusta ma compare «non me
// vale» dove tiny aveva ragione. Nessuna dimensione di modello risolve; i tempi
// invece sono buoni in entrambi.
struct TimedWord {
  QString word;      // dal COPIONE (o da Whisper se il copione non lo copre)
  int     startMs = 0;
  int     endMs   = 0;
  QString assetUuid;  // personaggio, dalla battuta di provenienza
  bool    fromScript = true;  // false = parola sentita ma non nel copione
};

// Come l'export porta un asset dentro lo shot.
//
// La distinzione Load/Import NON e' cosmetica ed e' la scelta piu' pesante di
// tutta la faccenda:
//  - Load  = lo shot PUNTA al file dov'e'. Una sorgente sola: correggi l'asset
//            una volta e la correzione arriva in tutti gli shot. Ma se il file
//            si sposta o sparisce, gli shot si rompono tutti insieme.
//  - Import = il file viene COPIATO nel progetto dello shot. Lo shot e'
//            autosufficiente (consegna, archivio, chi lavora altrove), ma una
//            correzione va rifatta shot per shot.
// Non c'e' una risposta giusta in assoluto: dipende se l'asset e' ancora in
// lavorazione (Load) o congelato (Import). Per questo si sceglie, e si puo'
// scegliere per singolo asset.
struct AssetImportPolicy {
  enum Mode { Default = 0, Load, Import };
  Mode mode = Default;  // Default = usa quella di progetto

  // Opzioni del popup PSD (psdsettingspopup.cpp), replicate qui perche' un
  // export automatico non puo' fermarsi a chiedere per ognuno dei 145 asset.
  // Vuote = si usano quelle di progetto.
  QString psdLoadAs;     // "Single Image" | "Frames" | "Columns"
  QString psdLevelName;  // "FileName#LayerName" | "LayerName"
  QString psdGroups;     // "Ignore" | "SubSceneColumns" | "ColumnFrames"
  int     psdSubScene = -1;  // -1 = non impostato, 0 = no, 1 = si'

  bool isDefault() const {
    return mode == Default && psdLoadAs.isEmpty() && psdLevelName.isEmpty() &&
           psdGroups.isEmpty() && psdSubScene < 0;
  }
};

// A production asset (character, prop, environment, …). Project-level entity,
// source-agnostic: it may have art attached or be just a name from the script
// breakdown. Has its own task pipeline, mirroring shot tasks.
struct Asset {
  QString                  uuid;   // stable project-unique key
  QString                  type;   // "Character" / "Prop" / "FX" / "Environment" (Kitsu-aligned)
  QString                  name;
  QString kitsuAssetId;  // Kitsu asset (entity) id once synced — link across renames
  QMap<QString, TaskState> tasks;  // keyed by asset task-type name
  QStringList              tags;   // free categorisation (future: breakdown / AI)
  // Il file di questo asset, per l'export che lo importa nello shot.
  // - Character (cutout): la SCENA .tnz che lo contiene, importata come
  //   sotto-scena. Va indicata a mano: non c'e' convenzione che la indovini.
  // - Prop / Environment: facoltativo. Se vuoto si risolve per convenzione
  //   dalla cartella della categoria (vedi assetDirForType) piu' il nome.
  QString filePath;
  // Come portarlo nello shot. Vuota = si usa la politica di progetto.
  AssetImportPolicy importPolicy;
};

// A project-level shot record. Owns the production progress (task status/
// assignee). The structural metadata (seq/label/frames/technique) is authored
// by the storyboard .tnz and published here via publishShotsToProjectDb().
// One line of a shot's breakdown: an asset this shot needs. Kitsu calls it
// «casting» and stores it in entity_link (shot -> asset), which is where this
// syncs to and from.
// The asset is held by UUID, not by name: a rename in the tracker must not
// silently empty a shot's breakdown.
struct BreakdownEntry {
  QString assetUuid;
  int     nbOccurrences = 1;  // same asset appearing more than once in the shot
  QString label;              // Kitsu's free label (seen in the wild: "animate")
};

struct ProjectShot {
  QString uuid;
  QString source;    // basename of the originating .tnz storyboard (e.g. "reel1.tnz")
  QString seq;       // e.g. "SQ010"
  QString label;     // e.g. "SH010"
  int     frames = 0;
  QString technique;
  QString kitsuShotId;  // Kitsu shot id once synced — keeps the link across renames
  QMap<QString, TaskState> tasks;  // progress — authoritative for the project DB
  QVector<BreakdownEntry>  breakdown;  // assets this shot needs
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
  Character,    // Character Mode — il personaggio di libreria (rig, bocche, pose)
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
  bool                              m_useKitsu = false; // opt-in Kitsu sync UI
  QString                           m_kitsuProjectId;   // empty = not linked
  QString                           m_kitsuProjectName; // cached for display
  // Episode binding, for tvshow productions. One Kitsu tvshow holds several
  // episodes and each one is a separate Ztoryc project, so the binding is the
  // PAIR (project, episode) — the project alone would pull every episode's
  // shots into this tracker. Empty on non-tvshow productions.
  // The id is what makes the link survive a rename: m_episode is the name the
  // user typed and Kitsu lets it change.
  QString                           m_kitsuEpisodeId;

  // ── Dove stanno i file degli asset (per l'export completo) ──────────────
  // Una cartella PER CATEGORIA, non un percorso per asset: con 145 asset la
  // seconda strada non la compila nessuno. Il file si risolve per convenzione,
  // cartella della categoria + nome dell'asset.
  // I PERSONAGGI non stanno qui: nel cutout digitale sono scene vere e proprie
  // dello stesso progetto, e si importano come sotto-scene (scelta di Franco,
  // 2026-08-15). m_modelSheetDir serve invece al tradigital, dove del
  // personaggio si importa il model sheet come immagine.
  // Nomi dello script forzati a mano su un personaggio, in minuscolo -> uuid.
  // Serve perche' negli script i nomi non coincidono mai del tutto con quelli
  // del tracker: «PRINCIPESSA» nel copione, «PRINCENERENTOLA» fra gli asset.
  // Senza, l'unica via sarebbe correggere il copione o rinominare l'asset —
  // due cose che non si vogliono fare per far funzionare un riconoscimento.
  QHash<QString, QString>           m_speakerAliases;

  QString                           m_propsDir;
  QString                           m_backgroundsDir;
  QString                           m_modelSheetDir;
  // Politica di default per l'import degli asset. I singoli asset possono
  // scostarsene; questa e' quella che vale quando non lo fanno.
  AssetImportPolicy                 m_defaultImportPolicy;
  QString                           m_productionType;   // Kitsu: tvshow/short/featurefilm
  QString                           m_productionStyle;  // Kitsu: 2d/3d/2d3d
  QString                           m_ratio;            // Kitsu: e.g. 16:9
  QString                           m_resolution;       // Kitsu: e.g. 1920x1080
  std::vector<Technique>            m_techniques;       // editable presets (seeded with defaults)
  QString                           m_defaultTechnique; // project default technique name
  std::vector<AssetType>            m_assetTypes;       // custom asset types + their task pipelines
  QStringList                       m_team;             // project roster (names) for the assignee picker
  QString                           m_pdfLogoPath; // custom PDF header logo (abs or scene-relative); empty = default Ztoryc logo
  bool                              m_pdfNoLogo = false;  // true = export PDF with no logo at all
  ZtoryWorkflow                     m_workflow = ZtoryWorkflow::Tradigital;
  std::vector<ZtoryClipEntry>       m_sharedClip;
  std::set<int>                     m_sharedSelection;
  NumberingConfig                   m_numberingConfig;
  bool                              m_autoRenumber = true;  // global numbering mode
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
  // Opt-in: the Kitsu sync UI is shown only when the project enables it (chosen
  // at project creation). An already-linked project counts as using Kitsu too
  // (backward compat for projects saved before the flag existed).
  bool    useKitsu()         const { return m_useKitsu || isKitsuLinked(); }
  void    setUseKitsu(bool on)     { m_useKitsu = on; }
  // True once at least one project shot has been created in Kitsu (has an id).
  // The Board locks numbering to Keep mode in this case so shot labels stay put
  // and the Kitsu links / statuses don't drift when a shot is added.
  bool    hasKitsuShots()    const {
    for (const ProjectShot &ps : m_projectShots)
      if (!ps.kitsuShotId.isEmpty()) return true;
    return false;
  }
  void    setKitsuProject(const QString &id, const QString &name) {
    m_kitsuProjectId = id; m_kitsuProjectName = name;
  }
  // Episode binding (tvshow only). `name` also lands in m_episode, which is
  // what pushShots() already sends and what the worksheet header prints — so
  // the two never drift apart.
  QString kitsuEpisodeId() const { return m_kitsuEpisodeId; }
  void    setKitsuEpisode(const QString &id, const QString &name) {
    m_kitsuEpisodeId = id; m_episode = name;
  }
  // True when this project is bound to one episode of a tvshow: the pulls must
  // then be restricted to it, or the other episodes' shots land here too.
  bool    isKitsuEpisodeLinked() const { return !m_kitsuEpisodeId.isEmpty(); }

  // Cartelle degli asset, per categoria (vedi il commento sui membri).
  QString propsDir()       const { return m_propsDir; }
  QString backgroundsDir() const { return m_backgroundsDir; }
  QString modelSheetDir()  const { return m_modelSheetDir; }
  void setPropsDir(const QString &s)       { m_propsDir = s; }
  void setBackgroundsDir(const QString &s) { m_backgroundsDir = s; }
  void setModelSheetDir(const QString &s)  { m_modelSheetDir = s; }
  // La cartella in cui cercare i file di un asset di questo tipo, o vuota se
  // per quel tipo non si passa da una cartella (i Character sono sotto-scene).
  QString assetDirForType(const QString &type) const;
  const AssetImportPolicy &defaultImportPolicy() const { return m_defaultImportPolicy; }
  void setDefaultImportPolicy(const AssetImportPolicy &p) { m_defaultImportPolicy = p; }
  void setAssetImportPolicy(int i, const AssetImportPolicy &p) {
    if (i >= 0 && i < (int)m_assets.size()) m_assets[i].importPolicy = p;
  }
  // La politica EFFETTIVA di un asset: la sua dove l'ha impostata, quella di
  // progetto dove non l'ha fatto. Campo per campo, non tutto-o-niente: chi
  // cambia solo il modo non deve riscrivere anche le opzioni PSD.
  AssetImportPolicy effectiveImportPolicy(const Asset &a) const;
  // Il file vero di un asset, per l'export. Regola, in ordine:
  //   1. il legame esplicito (Asset::filePath) VINCE sempre;
  //   2. altrimenti cartella della categoria + nome base UGUALE al nome
  //      dell'asset (senza distinzione di maiuscole, qualunque estensione);
  //   3. zero risultati o PIU' di uno → si restituisce vuoto e si spiega il
  //      perche' in `why`. Non si indovina mai: scegliere a caso fra
  //      «macchina.tlv» e «macchina.psd» e' un errore che si vede solo in
  //      render, giorni dopo.
  // Volutamente niente prefissi/suffissi: «macchina» non pesca «macchina_v03».
  QString resolveAssetFile(const Asset &a, QString *why = nullptr) const;

  // ── Dialoghi: chi dice cosa ────────────────────────────────────────────────
  // Estrae le battute dal testo di un pannello, riconoscendo le due forme in cui
  // un personaggio compare in una sceneggiatura:
  //   «MARIO: ma dove vai?»          → forma con i due punti
  //   «MARIO» su una riga sua, in     → forma sceneggiatura (Fountain, FDX,
  //   maiuscolo, battuta sotto           copia-incolla da Final Draft)
  // Le estensioni fra parentesi (V.O.), (O.S.), (CONT'D) si tolgono dal nome; le
  // didascalie su riga propria — «(sottovoce)» — NON si pronunciano e si
  // scartano. I nomi si risolvono sugli asset di tipo Character del progetto.
  QVector<DialogueLine> parseDialogue(const QString &text) const;
  // I soli nomi che il testo contiene ma il progetto non conosce. Serve a
  // MOSTRARE il riconoscimento invece di lasciarlo magico: una convenzione che
  // non si vede fallire in silenzio e' peggio di un campo in piu'.
  QStringList unknownSpeakers(const QString &text) const;

  // Riallinea le parole SENTITE da Whisper su quelle del copione, tenendo i
  // tempi delle prime e il testo delle seconde.
  //
  // `heard` sono le parole di Whisper coi loro millisecondi; `spoken` sono le
  // battute del pannello gia' attribuite (uscita di parseDialogue). Il risultato
  // e' la sequenza del COPIONE con i tempi addosso.
  //
  // Serve un allineamento vero e non un accoppiamento uno-a-uno: Whisper spezza
  // e fonde le parole («credi» -> «cre»+«di», «lascialo perdere» ->
  // «lascia»+«l'operdere»), quindi le due sequenze hanno lunghezze diverse e
  // l'indice non basta.
  static QVector<TimedWord> alignToScript(const QVector<TimedWord> &heard,
                                          const QVector<DialogueLine> &spoken);
  // Questa riga e' un'intestazione di personaggio? Stessa identica regola di
  // parseDialogue — esposta perche' l'evidenziatore del campo di testo la deve
  // usare, e due copie della regola divergono al primo caso limite.
  // `nextLine` serve alla regola di Fountain (nome seguito da qualcosa).
  // Restituisce false se la riga non e' un'intestazione; altrimenti riempie
  // `outName` e dice in `outMatched` se e' un personaggio del progetto.
  bool speakerAt(const QString &line, const QString &nextLine,
                 QString *outName, bool *outMatched) const;

  // Alias: forza un nome dello script su un personaggio. uuid vuoto = toglie
  // l'alias. Vale per tutto il progetto, non per il singolo pannello: se
  // «PRINCIPESSA» e' quel personaggio qui, lo e' anche negli altri 40 pannelli.
  void setSpeakerAlias(const QString &scriptName, const QString &assetUuid);
  QString speakerAlias(const QString &scriptName) const {
    return m_speakerAliases.value(scriptName.trimmed().toLower());
  }
  const QHash<QString, QString> &speakerAliases() const { return m_speakerAliases; }
  // Il file legato a un asset (per un Character cutout: la scena da importare
  // come sotto-scena). Vuoto = nessun legame diretto.
  void setAssetFilePath(int i, const QString &path) {
    if (i >= 0 && i < (int)m_assets.size()) m_assets[i].filePath = path;
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
  // Replace one shot's breakdown. A setter and not a non-const projectShots():
  // this is the only field of a project shot the sync writes, and opening the
  // whole vector to mutation to write one field is how invariants get lost.
  void setShotBreakdown(int i, const QVector<BreakdownEntry> &b) {
    if (i >= 0 && i < (int)m_projectShots.size())
      m_projectShots[i].breakdown = b;
  }
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

  // --- Status automation engine (shared task-order logic) --------------------
  // First *production* task of a technique (the first one that isn't Storyboard);
  // empty if the technique has none. Storyboard is the board/animatic pass that
  // precedes the .tnz work, so production advances start from this task.
  QString firstProductionTaskType(const QString &technique) const;
  // The task that follows `afterTask` in the technique's order; empty if it is
  // the last one (or not found). Drives the DONE → next-task-Ready cascade.
  QString nextTaskType(const QString &technique, const QString &afterTask) const;

  // B3d — Naming convention
  // Resolve m_namingPattern substituting token map. Tokens: PROD, SEASON, EP,
  // SEQ, SHOT, TASK, VER. Optional format suffix: {VER:02} → zero-padded.
  //! Pattern used when the project has none of its own: uses {CODE}, or
  //! {PROD} when no code was filled in.
  //! The code to use in names: the one set, or derived from the production
  //! name when never filled in. Never written to the project.
  QString effectiveCode() const;
  QString defaultNamingPattern() const;
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

  // ── Dal nome che parla all'ASSET ──────────────────────────────────────────
  // Il primo anello della catena decisa da Franco: chi parla -> asset ->
  // livello di bocche -> set. Gli altri anelli non stanno qui — la mappa delle
  // bocche e' attaccata al LIVELLO (ztorymouthmap.h), non al progetto.
  const Asset *assetByUuid(const QString &uuid) const;
  const Asset *assetByName(const QString &name) const;  // senza distinzione di caso

  // ── Assets (project-level) ─────────────────────────────────────────────────
  const std::vector<Asset> &assets() const { return m_assets; }
  std::vector<Asset>       &assets()       { return m_assets; }
  int  assetCount() const { return (int)m_assets.size(); }
  void addAsset(const QString &type, const QString &name);  // assigns a uuid; emits
  void removeAssetAt(int i);
  // Canonical asset types, aligned 1:1 with Kitsu's default asset-types so the
  // bidirectional sync maps by name without a translation table. Legacy "BG"
  // assets are migrated to "Environment" on load. Used to SEED the editable
  // m_assetTypes; the live taxonomy is assetTypes() below.
  static const QStringList &canonicalAssetTypes();
  // Default asset task pipeline — seeds each type's editable pipeline.
  static const QStringList &canonicalAssetTaskOrder();

  // ── Asset types (custom, per-type task pipeline — Kitsu-aligned) ────────────
  const std::vector<AssetType> &assetTypes() const { return m_assetTypes; }
  std::vector<AssetType>       &assetTypes()       { return m_assetTypes; }
  void seedDefaultAssetTypes();                       // populate presets if empty
  const AssetType *findAssetType(const QString &name) const;
  // Ordered asset-task names for an asset type (falls back to the canonical
  // order for an unknown/empty type, so legacy assets never lose their tasks).
  QStringList assetTaskTypesForType(const QString &type) const;
  // Append a task type to an asset type's pipeline, creating the asset type if
  // it isn't there yet. Used when a Kitsu pull brings a task type this project
  // has no counterpart for: adopting it keeps the task, dropping it loses data
  // without telling anyone. No-op when the name is already in the pipeline
  // (compared through the same normalization the sync uses, so «Rough» and
  // «rough» don't both end up as columns).
  void addAssetTaskType(const QString &type, const QString &taskType);
  // Union of asset-task names across the types actually used by the assets, in
  // asset-type pipeline order — the asset table's task columns.
  QStringList assetTaskColumns() const;
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

  // Numbering mode is GLOBAL (shared by every StoryboardPanel instance): the
  // Board and the Shot-editor StoryStrip both renumber on add, so a per-panel
  // flag let an Auto-mode panel clobber a Keep-mode panel's result. Keeping it
  // on the model makes all panels agree. true = Auto (renumber all on insert),
  // false = Keep (preserve existing labels, midpoint the new shot).
  bool autoRenumber() const { return m_autoRenumber; }
  void setAutoRenumber(bool on) { m_autoRenumber = on; }

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

  // ---- Cache CONDIVISA dei render di anteprima ------------------------------
  // Renderizzare un frame di sotto-scena e' di gran lunga la cosa piu' cara del
  // Board. Ogni pannello aveva la sua cache privata, quindi con un Board per
  // room la STESSA anteprima veniva renderizzata una volta per pannello. Qui e'
  // una sola: il primo che la chiede paga, gli altri leggono.
  //
  // Sta in ZtoryModel perche' e' il posto dove vive lo stato condiviso (vedi la
  // regola in AGENTS.md: niente stato globale mutabile altrove).
  //
  // La chiave e' costruita da chi renderizza e comprende TUTTO cio' che cambia
  // il risultato: nome della sotto-scena (sopravvive ai riordini, a differenza
  // del puntatore o dell'indice colonna), frame, dimensioni in pixel fisici e
  // regione di camera. In cache va il render NUDO: l'overlay della camera e'
  // economico e viene riapplicato su una copia a ogni uso.
  QPixmap cachedPanelRender(const QString &key) const;
  void    cachePanelRender(const QString &key, const QPixmap &px);
  //! Butta via i render di UNA sotto-scena (il disegno e' cambiato).
  void    invalidatePanelRenders(const QString &subSceneName);
  //! Butta via tutto (cambio scena).
  void    clearPanelRenderCache();

private:
  QHash<QString, QPixmap> m_panelRenderCache;
  // Tetto prudenziale: le anteprime sono grandi e una scena lunga ne ha tante.
  // Superato il tetto si svuota tutto — semplice e prevedibile, invece di una
  // politica di sfratto che sarebbe un'altra cosa da sbagliare.
  static const int kPanelRenderCacheMax = 400;

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
  // A (non-preview) movie render has finished. Emitted from
  // OnRenderCompleted::onDeliver in rendercommand.cpp via notifyRenderFinished().
  // The per-shot animatic export (StoryboardPanel::onExportAnimatic) listens to
  // this to render shots SEQUENTIALLY: concurrent renders of the same scene
  // contaminate each other's output (per-shot clips ended up containing every
  // shot), so each render must complete before the next starts.
  void renderFinished();

 public:
  // Public emitter for renderFinished() — Qt signals are protected, so
  // rendercommand.cpp (outside this class) routes completion through here.
  void notifyRenderFinished() { emit renderFinished(); }
};
