#include "ztorymodel.h"
#include "ztorytaskflow.h"
#include "ztorycharacter.h"
#include "ztoryshotops.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"  // MI_Workflow* ids for workflowCommand()
#include "xsheetdragtool.h"   // XsheetGUI::setPlayRange
#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include <map>
#include "toonz/txshchildlevel.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelproperties.h"   // LevelProperties (export-to-board level)
#include "toonz/levelset.h"          // TLevelSet (unique level name check)
#include "toonz/stage.h"             // Stage::standardDpi
#include "toonz/tcamera.h"           // TCamera dpi for export-to-board
#include "trasterimage.h"            // TRasterImageP (export-to-board frames)
#include "tsystem.h"                 // doesExistFileOrLevel (unique level name)
#include "tparamcontainer.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/childstack.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonzqt/icongenerator.h"
#include "timagecache.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/toonzscene.h"

#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>

#include "toonz/tproject.h"
#include <QDir>
#include <QSet>
#include <QMessageBox>
#include <QRegularExpression>
#include <QFileInfo>
#include <QUuid>
#include <QBuffer>
#include <QCoreApplication>
#include <QFileSystemWatcher>
#include <QLockFile>
#include <QSaveFile>
#include <QTimer>
#include "ztrackmerge.h"
#include "ztorylocks.h"
#include <QSettings>
#include <climits>

// ─── ZtoryNumbering ───────────────────────────────────────────────────────────
// Internal helpers for the SH/SQ/P labelling system.
// orderIndex uses 100× scale: "SH010" ↔ orderIndex 1000, "SH020" ↔ 2000.

namespace ZtoryNumbering {

// Format integer n with given padding, e.g. formatN(10, 3) → "010"
static QString formatN(int n, int pad) {
  return QString("%1").arg(n, pad, 10, QChar('0'));
}

// Extract the numeric value from a label, stripping prefix and optional
// trailing alpha suffix.  Returns -1 on failure.
// E.g. labelNum("SH010A", "SH") → 10,  labelNum("SH020", "SH") → 20
static int labelNum(const QString &label, const QString &prefix) {
  if (!label.startsWith(prefix, Qt::CaseInsensitive)) return -1;
  QString rest = label.mid(prefix.length());
  while (!rest.isEmpty() && rest.back().isLetter()) rest.chop(1);
  bool ok;
  int n = rest.toInt(&ok);
  return ok ? n : -1;
}

// Strip trailing alpha suffix from a label.
// E.g. "SH010A" → "SH010",  "SH010" → "SH010"
static QString stripSuffix(const QString &label, const QString &prefix) {
  if (!label.startsWith(prefix, Qt::CaseInsensitive)) return label;
  QString rest = label.mid(prefix.length());
  while (!rest.isEmpty() && rest.back().isLetter()) rest.chop(1);
  return prefix + rest;
}

// Collect all current shotLabels from a shots vector.
static QStringList allLabels(const std::vector<ShotData> &shots) {
  QStringList res;
  for (const auto &s : shots)
    if (!s.shotLabel.isEmpty()) res << s.shotLabel;
  return res;
}

// Find the next available alpha suffix for a base label.
// E.g. existing = {"SH010", "SH010A", "SH010B"}, base = "SH010" → 'C'
static QChar nextSuffix(const QStringList &existing, const QString &base) {
  for (char c = 'A'; c <= 'Z'; c++) {
    if (!existing.contains(base + QChar(c))) return QChar(c);
  }
  return 'A';  // fallback (exhausted A-Z, shouldn't happen)
}

}  // namespace ZtoryNumbering

// ─── NumberingConfig ──────────────────────────────────────────────────────────

QString NumberingConfig::shotName(int idx) const {
  int number = startNumber + idx * step;
  if (style == Sequence) {
    return QString("%1%2_%3%4")
        .arg(seqPrefix)
        .arg(seqNumber, seqPadding, 10, QChar('0'))
        .arg(shotPrefix)
        .arg(number, padding, 10, QChar('0'));
  }
  return QString("%1%2").arg(shotPrefix).arg(number, padding, 10, QChar('0'));
}

// ─── Singleton ────────────────────────────────────────────────────────────────

const QString ZtoryModel::kCharacterType  = QStringLiteral("Character");
const QString ZtoryModel::kStoryboardTask = QStringLiteral("Storyboard");

bool ZtoryModel::isCharacterType(const QString &type) {
  return type.compare(kCharacterType, Qt::CaseInsensitive) == 0;
}

bool ZtoryModel::isStoryboardTask(const QString &taskType) {
  return taskType.compare(kStoryboardTask, Qt::CaseInsensitive) == 0;
}

const QVector<TaskStatus> &ZtoryModel::allTaskStatuses() {
  static const QVector<TaskStatus> all = {TaskStatus::Todo,  TaskStatus::Ready,
                                          TaskStatus::Wip,   TaskStatus::Wfa,
                                          TaskStatus::Retake, TaskStatus::Done};
  return all;
}

// Canonical Kitsu palette (matches the live Kitsu task_status colours).
QColor ZtoryModel::taskStatusColor(TaskStatus s) {
  switch (s) {
  case TaskStatus::Ready:  return QColor("#FBC02D");  // amber
  case TaskStatus::Wip:    return QColor("#3273DC");  // blue
  case TaskStatus::Wfa:    return QColor("#AB26FF");  // purple
  case TaskStatus::Retake: return QColor("#FF3860");  // red
  case TaskStatus::Done:   return QColor("#22D160");  // green
  case TaskStatus::Todo:
  default:                 return QColor("#9E9E9E");  // grey
  }
}

ZtoryModel::ZtoryModel() : m_fps(24) {
  m_follow = QSettings().value("Ztoryc/followBoardTimeline", false).toBool();
  m_animaticSidePanels = QStringList{ "Storyboard" };
  m_shotSidePanels     = QStringList{ "Xsheet", "ZtoryScriptPanel" };
  // Default naming pattern (B3d). Follows NABA convention with separate
  // PROD and SEASON tokens — can be overridden per-project in Project tab.
  m_namingPattern = defaultNamingPattern();
  seedDefaultTechniques();
  seedDefaultAssetTypes();
  // Room-independent shot auto-WIP (the StoryboardPanel version only runs when a
  // Board panel is in the current room).
  if (TApp::instance() && TApp::instance()->getCurrentScene())
    connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
            this, &ZtoryModel::onSceneSwitchedAdvanceShot);
  // A new scene (or Revert Scene): its .ztoryc must be read again.  Connected
  // here, in the model's constructor, so it runs before the Boards reload.
  if (TApp::instance() && TApp::instance()->getCurrentScene())
    connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
            this, [this]() {
              // Entering or leaving a shot also emits sceneSwitched: that is
              // the same scene object, and re-reading its file would put it
              // back over edits not saved yet (e.g. from the Shot Board
              // navigator).  Opening a scene or Revert Scene make a NEW
              // ToonzScene (IoCmd::loadScene) before dropping the old one.
              if (TApp::instance()->getCurrentScene()->getScene() !=
                  m_shotDataSceneObj)
                m_shotDataLoadedFor.clear();
            });
  // More than one Ztoryc may be open on the same project: read back what the
  // others write (debounced: one save can come as several file events).
  m_dbReloadTimer = new QTimer(this);
  m_dbReloadTimer->setSingleShot(true);
  connect(m_dbReloadTimer, &QTimer::timeout, this,
          &ZtoryModel::reloadProjectDbIfChangedOnDisk);
  m_dbWatcher = new QFileSystemWatcher(this);
  connect(m_dbWatcher, &QFileSystemWatcher::fileChanged, this,
          [this]() { m_dbReloadTimer->start(400); });
  if (TApp::instance() && TApp::instance()->getCurrentScene()) {
    TSceneHandle *sh = TApp::instance()->getCurrentScene();
    connect(sh, &TSceneHandle::sceneSwitched, this,
            [this]() { updateSceneLock(true); });
    connect(sh, &TSceneHandle::nameSceneChanged, this,
            [this]() { updateSceneLock(true); });
  }
  connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
    delete m_sceneLock;  // removes the lock file
    m_sceneLock = nullptr;
  });
}

// Lock on the scene open here, so a second instance opening the same scene can
// warn: both saving it, the last save erases the other's work. A warning only —
// opening it read-only to compare is legitimate.
void ZtoryModel::updateSceneLock(bool warnIfTaken) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  const QString tnz =
      (scene && !scene->isUntitled())
          ? QString::fromStdWString(scene->getScenePath().getWideString())
          : QString();
  const QString lockPath =
      tnz.isEmpty() ? QString() : ZtoryLocks::lockFilePath("scene", tnz);
  // nameSceneChanged also comes with every change of the dirty flag: the same
  // scene warns once, then only retries in silence (the other window may have
  // closed it meanwhile).
  const bool sameScene = (lockPath == m_sceneLockPath);
  if (sameScene && (m_sceneLock || lockPath.isEmpty())) return;
  delete m_sceneLock;
  m_sceneLock     = nullptr;
  m_sceneLockPath = lockPath;
  if (lockPath.isEmpty()) return;
  m_sceneLock = new QLockFile(lockPath);
  m_sceneLock->setStaleLockTime(0);  // held for hours: only a dead pid frees it
  if (m_sceneLock->tryLock(0)) return;
  delete m_sceneLock;
  m_sceneLock = nullptr;
  if (!warnIfTaken || sameScene) return;
  const QString name = QFileInfo(tnz).fileName();
  // After the scene has finished loading, not in the middle of it.
  QTimer::singleShot(0, this, [name]() {
    DVGui::warning(
        QObject::tr("«%1» is already open in another Ztoryc window.\n\n"
                    "If both windows save it, the last save erases the other "
                    "one's changes.")
            .arg(name));
  });
}

// Advance the first pipeline task of an exported shot scene Ready/Todo→WIP when
// it becomes current. Reads role/back-link from the scene's companion .ztoryc;
// idempotent (only the first open changes anything) and safe for non-shot scenes
// (returns early). Mirrors StoryboardPanel's logic but is always alive.
void ZtoryModel::onSceneSwitchedAdvanceShot() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  QString tnz = QString::fromStdWString(scene->getScenePath().getWideString());
  if (tnz.isEmpty()) return;
  QString ztorcPath = tnz;
  ztorcPath.replace(QRegularExpression("\\.tnz$"), ".ztoryc");
  QFile f(ztorcPath);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

  QString role, uuid, projectDb, technique;
  QXmlStreamReader xml(&f);
  while (!xml.atEnd()) {
    xml.readNext();
    if (!xml.isStartElement()) continue;
    if (xml.name() == QLatin1String("ztoryc")) {
      auto a    = xml.attributes();
      role      = a.value("role").toString();
      uuid      = a.value("projectShot").toString();
      projectDb = a.value("project").toString();
    } else if (xml.name() == QLatin1String("project")) {
      technique = xml.attributes().value("technique").toString();
    }
  }
  if (role != "shot" || uuid.isEmpty() || projectDb.isEmpty()) return;
  if (!QFile::exists(projectDb)) return;

  loadProjectDbFromPath(projectDb);
  // The rule itself lives in ZtoryTaskFlow (it used to be here AND, divergent,
  // in StoryboardPanel::loadZtoryc — which put Storyboard back to WIP).
  ZtoryTaskFlow::shotOpened(uuid, technique);
}

// ─── Production techniques / tasks ──────────────────────────────────────────

void ZtoryModel::seedDefaultTechniques() {
  if (!m_techniques.empty()) return;
  // Editable presets — tasks per technique.  Stop-motion / Traditional / Live
  // are reasonable starting points to be refined later.
  // Every pipeline starts with the Storyboard pass (it owns the board/animatic
  // task that preview-upload sets to WFA), so it leads each technique.
  m_techniques = {
    {"Tradigital",  {"Storyboard", "Layout", "Key Animation", "Inbetweening",
                     "Clean up", "Ink & Paint", "VFX", "Render", "Compositing"}},
    {"Traditional", {"Storyboard", "Layout", "Key Animation", "Inbetweening",
                     "Clean up", "Scan & Clean", "Ink & Paint", "X-Sheet", "VFX",
                     "Render", "Compositing"}},
    {"Cut-out",     {"Storyboard", "Layout", "Animation", "VFX", "Render",
                     "Compositing"}},
    {"3D / CGI",    {"Storyboard", "Layout", "Animation", "Lighting", "Render",
                     "Compositing"}},
    {"Stop-motion", {"Storyboard", "Set-up", "Layout", "Animation", "Rig Removal",
                     "Compositing"}},
    // Generic keeps every task active; Live is a trimmed live-action set.
    {"Generic", canonicalTaskOrder()},
    {"Live",    {"Storyboard", "Layout", "Shooting", "Editing", "VFX",
                 "Compositing"}},
  };
  if (m_defaultTechnique.isEmpty()) m_defaultTechnique = "Tradigital";
}

const Technique *ZtoryModel::findTechnique(const QString &name) const {
  for (const auto &t : m_techniques)
    if (t.name.compare(name, Qt::CaseInsensitive) == 0) return &t;
  return nullptr;
}

//-----------------------------------------------------------------------------
// Asset types (custom, per-type task pipeline)

void ZtoryModel::seedDefaultAssetTypes() {
  if (!m_assetTypes.empty()) return;
  // Seed one type per canonical name, each with the canonical asset task order
  // as its (editable) starting pipeline.
  for (const QString &type : canonicalAssetTypes())
    m_assetTypes.push_back(AssetType{type, canonicalAssetTaskOrder()});
}

const AssetType *ZtoryModel::findAssetType(const QString &name) const {
  for (const auto &t : m_assetTypes)
    if (t.name.compare(name, Qt::CaseInsensitive) == 0) return &t;
  return nullptr;
}

QStringList ZtoryModel::assetTaskTypesForType(const QString &type) const {
  const AssetType *t = findAssetType(type);
  // Unknown/empty type → canonical order, so a legacy or freshly-typed asset
  // still shows its tasks instead of an empty row.
  return (t && !t->taskTypes.isEmpty()) ? t->taskTypes : canonicalAssetTaskOrder();
}

//-----------------------------------------------------------------------------
// AssetImportPolicy <-> XML. Un helper solo, usato dal default di progetto e
// dagli scostamenti dei singoli asset: due copie divergono, e la seconda
// dimentica sempre un campo.

static void writeImportPolicy(QXmlStreamWriter &xml, const AssetImportPolicy &p) {
  if (p.mode == AssetImportPolicy::Load)   xml.writeAttribute("import", "load");
  else if (p.mode == AssetImportPolicy::Import) xml.writeAttribute("import", "import");
  if (!p.psdLoadAs.isEmpty())    xml.writeAttribute("psdLoadAs", p.psdLoadAs);
  if (!p.psdLevelName.isEmpty()) xml.writeAttribute("psdLevelName", p.psdLevelName);
  if (!p.psdGroups.isEmpty())    xml.writeAttribute("psdGroups", p.psdGroups);
  if (p.psdSubScene >= 0)
    xml.writeAttribute("psdSubScene", p.psdSubScene ? "1" : "0");
}

static AssetImportPolicy readImportPolicy(const QXmlStreamAttributes &a) {
  AssetImportPolicy p;
  const QString m = a.value("import").toString();
  if (m == QLatin1String("load"))        p.mode = AssetImportPolicy::Load;
  else if (m == QLatin1String("import")) p.mode = AssetImportPolicy::Import;
  p.psdLoadAs    = a.value("psdLoadAs").toString();
  p.psdLevelName = a.value("psdLevelName").toString();
  p.psdGroups    = a.value("psdGroups").toString();
  if (a.hasAttribute("psdSubScene"))
    p.psdSubScene = a.value("psdSubScene").toString() == QLatin1String("1") ? 1 : 0;
  return p;
}

//-----------------------------------------------------------------------------
// Dialoghi — riconoscere chi parla dentro il testo di un pannello.
//
// Le due forme che arrivano davvero, perche' nel Board si incolla dallo script:
//   «MARIO: ma dove vai?»   forma con i due punti
//   «MARIO»                 forma sceneggiatura: nome su riga propria, in
//   «Ma dove vai?»          maiuscolo, battuta sotto (Fountain, FDX, Final Draft)

// Toglie l'estensione fra parentesi da un nome: MARIO (V.O.) -> MARIO.
// Sono indicazioni di regia (voce fuori campo, fuori scena, continua), non
// personaggi diversi: senza questo «MARIO» e «MARIO (V.O.)» diventerebbero due.
static QString stripSpeakerExtension(const QString &name) {
  const int p = name.indexOf('(');
  return (p < 0 ? name : name.left(p)).trimmed();
}

// Una riga e' un'intestazione di personaggio in stile sceneggiatura?
// Regole prudenti, perche' un falso positivo si mangia una battuta:
//  - non vuota, ragionevolmente corta;
//  - nessuna lettera minuscola (i nomi in sceneggiatura sono in maiuscolo);
//  - almeno una lettera (una riga di soli «---» non e' un nome);
//  - non finisce con punteggiatura di frase.
static bool looksLikeSpeakerCue(const QString &line) {
  const QString s = stripSpeakerExtension(line);
  if (s.isEmpty() || s.length() > 40) return false;
  bool hasLetter = false;
  for (const QChar &ch : s) {
    if (ch.isLetter()) {
      hasLetter = true;
      if (ch.isLower()) return false;
    }
  }
  if (!hasLetter) return false;
  const QChar last = s.at(s.length() - 1);
  return last != '.' && last != '!' && last != '?' && last != ',';
}

QVector<DialogueLine> ZtoryModel::parseDialogue(const QString &text) const {
  QVector<DialogueLine> out;
  if (text.trimmed().isEmpty()) return out;

  // Indice dei personaggi del progetto, per nome minuscolo.
  QHash<QString, QString> uuidByName;
  for (const Asset &a : m_assets)
    if (ZtoryModel::isCharacterType(a.type))
      uuidByName.insert(a.name.trimmed().toLower(), a.uuid);
  for (auto it = m_speakerAliases.constBegin(); it != m_speakerAliases.constEnd(); ++it)
    uuidByName.insert(it.key(), it.value());

  auto emitLine = [&](const QString &speaker, const QString &saidRaw) {
    // Parentheticals INSIDE the line — «MARIO: (ride) Non ci credo», «Non ci
    // credo (sottovoce) davvero» — are directions, not words: left in, the
    // aligner looked for «ride» in the audio and shifted the words around it.
    // The two other forms were already handled: a parenthetical on its own
    // line is skipped below, an extension on the name is stripped
    // (stripSpeakerExtension). Asked by Franco, 2026-09-24.
    static const QRegularExpression kInlineParen(QStringLiteral("\\([^)]*\\)"));
    static const QRegularExpression kSpaces(QStringLiteral("\\s+"));
    QString said = saidRaw;
    said.replace(kInlineParen, QStringLiteral(" "));
    said.replace(kSpaces, QStringLiteral(" "));
    if (said.trimmed().isEmpty()) return;
    DialogueLine dl;
    dl.character = speaker;
    dl.text      = said.trimmed();
    if (!speaker.isEmpty()) {
      auto it = uuidByName.constFind(speaker.toLower());
      if (it != uuidByName.constEnd()) { dl.assetUuid = *it; dl.matched = true; }
    }
    out.push_back(dl);
  };

  QString current;      // personaggio in corso (forma sceneggiatura)
  QString pending;      // sue battute accumulate
  auto flush = [&]() {
    if (!pending.isEmpty()) emitLine(current, pending);
    pending.clear();
  };

  const QStringList rawLines = text.split('\n');
  for (int li = 0; li < rawLines.size(); li++) {
    const QString line = rawLines[li].trimmed();
    // La riga dopo, per la regola di Fountain «intestazione = maiuscolo SEGUITO
    // da qualcosa»: una didascalia urlata resta sola, col vuoto sotto.
    // Attenzione: una didascalia SUBITO sotto il nome — «MARIO / (sottovoce) /
    // Non ci credo» — e' normalissima in sceneggiatura e CONFERMA
    // l'intestazione. Escluderla (primo tentativo) faceva sparire Mario: preso
    // dal test, non dalla lettura.
    QString next;
    if (li + 1 < rawLines.size()) next = rawLines[li + 1].trimmed();
    const bool nextIsDialogue = !next.isEmpty();

    // Riga vuota: chiude la battuta in corso e ANCHE il personaggio. In
    // sceneggiatura il blocco finisce li'; tenerlo aperto attribuirebbe a
    // Mario la descrizione dell'inquadratura che segue.
    if (line.isEmpty()) { flush(); current.clear(); continue; }

    // Didascalia su riga propria: «(sottovoce)» non si pronuncia.
    if (line.startsWith('(') && line.endsWith(')')) continue;

    // Forma con i due punti. Si accetta solo se cio' che precede i due punti
    // sembra un nome: altrimenti «Nota: arriva da destra» diventerebbe una
    // battuta del personaggio «Nota».
    const int colon = line.indexOf(':');
    if (colon > 0) {
      const QString head = stripSpeakerExtension(line.left(colon));
      if (looksLikeSpeakerCue(head) || uuidByName.contains(head.toLower())) {
        flush();
        current.clear();
        emitLine(head, line.mid(colon + 1));
        continue;
      }
    }

    // Forma sceneggiatura: il nome da solo, la battuta sotto.
    // Regola di Fountain: un'intestazione e' in maiuscolo ED E' SEGUITA da una
    // battuta. E' il «seguita da» a distinguerla da una didascalia urlata.
    //
    // Volutamente si accetta anche un nome che il progetto NON conosce, con
    // matched=false. La prima versione lo rifiutava, e il test ha mostrato che
    // cosi' il nome finiva inghiottito dentro la battuta («GIOVANNI Chi sono
    // io?») e unknownSpeakers() non poteva piu' segnalarlo: proprio il caso per
    // cui esiste — hai incollato uno script con un personaggio che non hai
    // ancora creato. Meglio mostrarlo che mangiarlo.
    if (looksLikeSpeakerCue(line) && nextIsDialogue) {
      flush();
      current = stripSpeakerExtension(line);
      continue;
    }

    if (!pending.isEmpty()) pending += ' ';
    pending += line;
  }
  flush();
  return out;
}

void ZtoryModel::setFollowEnabled(bool on) {
  if (m_follow == on) return;
  m_follow = on;
  QSettings().setValue("Ztoryc/followBoardTimeline", on);
  emit followChanged(on);
}

void ZtoryModel::setSpeakerAlias(const QString &scriptName,
                                const QString &assetUuid) {
  const QString key = scriptName.trimmed().toLower();
  if (key.isEmpty()) return;
  if (assetUuid.isEmpty()) m_speakerAliases.remove(key);
  else m_speakerAliases.insert(key, assetUuid);
}

//-----------------------------------------------------------------------------
// Riallineamento: i tempi di Whisper sulle parole del copione.
//
// Perché serve un allineamento vero e non un accoppiamento a indice: Whisper
// spezza e fonde. Misurato: «credi» → «cre»+«di», «lascialo perdere» →
// «lascia»+«l'operdere». Le due sequenze hanno lunghezze diverse, e dal primo
// scarto in poi un accoppiamento posizionale sbaglierebbe TUTTO il seguito.
//
// Si usa la distanza di edit fra sequenze di parole (Needleman-Wunsch), con
// una somiglianza fra parole invece dell'uguaglianza secca: «l'operdere» e
// «perdere» non sono uguali, ma sono chiaramente la stessa cosa.

// Somiglianza 0..1 fra due parole, ridotte a sole lettere minuscole (la
// punteggiatura di Whisper è rumore: «andare,» e «andare» sono la stessa cosa).
static double wordSimilarity(const QString &a, const QString &b) {
  auto norm = [](const QString &s) {
    QString o;
    for (const QChar &ch : s)
      if (ch.isLetter() || ch.isDigit()) o += ch.toLower();
    return o;
  };
  const QString x = norm(a), y = norm(b);
  if (x.isEmpty() || y.isEmpty()) return 0.0;
  if (x == y) return 1.0;
  // Prefisso comune: cattura i tagli di Whisper («cre» dentro «credi») e le
  // code sporche («l'operdere» contro «perdere» condivide poco in testa, ma il
  // contenimento sotto lo recupera).
  int p = 0;
  while (p < x.length() && p < y.length() && x[p] == y[p]) p++;
  const double byPrefix = double(2 * p) / double(x.length() + y.length());
  const double byContain =
      (x.contains(y) || y.contains(x))
          ? double(qMin(x.length(), y.length())) / double(qMax(x.length(), y.length()))
          : 0.0;
  return qMax(byPrefix, byContain);
}

QVector<TimedWord> ZtoryModel::alignToScript(
    const QVector<TimedWord> &heard, const QVector<DialogueLine> &spoken) {
  // Il copione, appiattito in parole, ognuna col suo personaggio.
  QVector<TimedWord> script;
  // Un pezzo senza lettere ne' cifre («—», «...») non e' una parola: non si
  // pronuncia, e da solo prendeva fotogrammi — espeak non ne ricava bocche e
  // nella colonna dei fonemi finiva il trattino stesso (Franco, 2026-09-27:
  // «non ha molto senso che la punteggiatura prenda dei fotogrammi»). Si
  // attacca alla parola prima, cosi' la colonna delle parole lo mostra
  // ancora; in testa a una battuta, alla parola dopo.
  static const QRegularExpression kSpoken(QStringLiteral("[\\p{L}\\p{N}]"));
  for (const DialogueLine &dl : spoken) {
    QString pending;  // punteggiatura in testa, in attesa di una parola
    const int lineStart = script.size();
    for (const QString &w : dl.text.split(QRegExp("\\s+"), Qt::SkipEmptyParts)) {
      if (!w.contains(kSpoken)) {
        if (script.size() > lineStart)
          script.last().word += " " + w;
        else
          pending += (pending.isEmpty() ? "" : " ") + w;
        continue;
      }
      TimedWord tw;
      tw.word      = pending.isEmpty() ? w : pending + " " + w;
      tw.assetUuid = dl.assetUuid;
      script.push_back(tw);
      pending.clear();
    }
  }
  // Senza copione non c'è niente da correggere: si tengono le parole sentite,
  // che è meglio di niente e mantiene i tempi.
  if (script.isEmpty()) return heard;
  if (heard.isEmpty()) return script;  // tempi a zero: non inventiamo nulla

  const int n = script.size(), m = heard.size();
  const double kGap = -0.6;  // costo di saltare una parola

  // Needleman-Wunsch. n e m sono le parole di un pannello, non di un film:
  // la matrice piena è piccola e leggibile, e non vale un algoritmo furbo.
  QVector<QVector<double>> d(n + 1, QVector<double>(m + 1, 0.0));
  for (int i = 1; i <= n; i++) d[i][0] = d[i - 1][0] + kGap;
  for (int j = 1; j <= m; j++) d[0][j] = d[0][j - 1] + kGap;
  for (int i = 1; i <= n; i++)
    for (int j = 1; j <= m; j++) {
      const double diag =
          d[i - 1][j - 1] + (wordSimilarity(script[i - 1].word, heard[j - 1].word) * 2.0 - 0.5);
      d[i][j] = qMax(diag, qMax(d[i - 1][j] + kGap, d[i][j - 1] + kGap));
    }

  // Ripercorso all'indietro: a ogni parola del copione si attaccano i tempi di
  // TUTTE le parole sentite che le corrispondono — così «cre»+«di» tornano una
  // «credi» sola, dall'inizio della prima alla fine dell'ultima.
  QVector<TimedWord> out;
  int i = n, j = m;
  while (i > 0 || j > 0) {
    if (i > 0 && j > 0) {
      const double diag =
          d[i - 1][j - 1] + (wordSimilarity(script[i - 1].word, heard[j - 1].word) * 2.0 - 0.5);
      if (qFuzzyCompare(d[i][j], diag)) {
        TimedWord tw = script[i - 1];
        tw.startMs   = heard[j - 1].startMs;
        tw.endMs     = heard[j - 1].endMs;
        // Assorbe le altre parole sentite che finiscono su questa del copione.
        while (i > 1 && j > 1 &&
               qFuzzyCompare(d[i - 1][j - 1], d[i - 1][j - 2] + kGap)) {
          j--;
          tw.startMs = heard[j - 1].startMs;
        }
        out.prepend(tw);
        i--; j--;
        continue;
      }
      if (qFuzzyCompare(d[i][j], d[i][j - 1] + kGap)) { j--; continue; }
    }
    if (i > 0) {
      // Parola del copione che Whisper non ha sentito: resta, senza tempi
      // propri. Toglierla vorrebbe dire perdere una battuta perché il
      // riconoscitore ha avuto una défaillance.
      out.prepend(script[i - 1]);
      i--;
      continue;
    }
    j--;
  }

  // Le parole senza tempo prendono quello del vicino: meglio una collocazione
  // approssimata che una parola a zero, che finirebbe all'inizio dello shot.
  for (int k = 0; k < out.size(); k++) {
    if (out[k].endMs > 0) continue;
    if (k > 0) { out[k].startMs = out[k - 1].endMs; out[k].endMs = out[k - 1].endMs; }
    else if (out.size() > 1) { out[k].startMs = 0; out[k].endMs = 0; }
  }
  return out;
}

bool ZtoryModel::speakerAt(const QString &rawLine, const QString &rawNext,
                           QString *outName, bool *outMatched) const {
  const QString line = rawLine.trimmed();
  if (line.isEmpty()) return false;
  if (line.startsWith('(') && line.endsWith(')')) return false;  // didascalia

  QHash<QString, QString> uuidByName;
  for (const Asset &a : m_assets)
    if (ZtoryModel::isCharacterType(a.type))
      uuidByName.insert(a.name.trimmed().toLower(), a.uuid);
  // Gli alias contano come nomi veri: e' il loro scopo.
  for (auto it = m_speakerAliases.constBegin(); it != m_speakerAliases.constEnd(); ++it)
    uuidByName.insert(it.key(), it.value());

  auto give = [&](const QString &name) {
    if (outName) *outName = name;
    if (outMatched) *outMatched = uuidByName.contains(name.toLower());
    return true;
  };

  // Forma coi due punti.
  const int colon = line.indexOf(':');
  if (colon > 0) {
    const QString head = stripSpeakerExtension(line.left(colon));
    if (looksLikeSpeakerCue(head) || uuidByName.contains(head.toLower()))
      return give(head);
  }
  // Forma sceneggiatura: maiuscolo, e seguito da qualcosa.
  if (looksLikeSpeakerCue(line) && !rawNext.trimmed().isEmpty())
    return give(stripSpeakerExtension(line));
  return false;
}

QStringList ZtoryModel::unknownSpeakers(const QString &text) const {
  QStringList out;
  for (const DialogueLine &dl : parseDialogue(text))
    if (!dl.character.isEmpty() && !dl.matched && !out.contains(dl.character))
      out << dl.character;
  return out;
}

void ZtoryModel::setAssetRigPsd(int i, const QString &absPath) {
  if (i < 0 || i >= (int)m_assets.size()) return;
  QString stored = absPath;
  const QString db = projectDbPath();
  if (!absPath.isEmpty() && !db.isEmpty()) {
    const QString rel = QDir(QFileInfo(db).absolutePath()).relativeFilePath(absPath);
    if (rel != ".." && !rel.startsWith("../") && !QDir::isAbsolutePath(rel))
      stored = rel;
  }
  m_assets[i].rigPsdPath = stored;
}

QString ZtoryModel::resolveAssetRigPsd(const Asset &a) const {
  if (a.rigPsdPath.isEmpty() || QDir::isAbsolutePath(a.rigPsdPath))
    return a.rigPsdPath;
  const QString db = projectDbPath();
  if (db.isEmpty()) return QString();
  return QDir(QFileInfo(db).absolutePath()).absoluteFilePath(a.rigPsdPath);
}

AssetImportPolicy ZtoryModel::effectiveImportPolicy(const Asset &a) const {
  // Campo per campo, non tutto-o-niente: chi cambia solo il modo su un asset
  // non deve ritrovarsi con le opzioni PSD azzerate, e chi cambia solo le
  // opzioni PSD non deve perdere il modo. Un merge grossolano qui produce
  // regressioni che si vedono solo all'export.
  AssetImportPolicy p = m_defaultImportPolicy;
  const AssetImportPolicy &o = a.importPolicy;
  if (o.mode != AssetImportPolicy::Default) p.mode = o.mode;
  if (!o.psdLoadAs.isEmpty())    p.psdLoadAs    = o.psdLoadAs;
  if (!o.psdLevelName.isEmpty()) p.psdLevelName = o.psdLevelName;
  if (!o.psdGroups.isEmpty())    p.psdGroups    = o.psdGroups;
  if (o.psdSubScene >= 0)        p.psdSubScene  = o.psdSubScene;
  // Il default del default: senza nulla di impostato si fa Load, che e' la
  // scelta non distruttiva — punta al file invece di moltiplicarne le copie.
  if (p.mode == AssetImportPolicy::Default) p.mode = AssetImportPolicy::Load;
  return p;
}

// ── Dal nome che parla all'ASSET ──────────────────────────────────────────

const Asset *ZtoryModel::assetByUuid(const QString &uuid) const {
  if (uuid.isEmpty()) return nullptr;
  for (const Asset &a : m_assets)
    if (a.uuid == uuid) return &a;
  return nullptr;
}

const Asset *ZtoryModel::assetByName(const QString &name) const {
  const QString n = name.trimmed();
  if (n.isEmpty()) return nullptr;
  // Prima l'alias: e' la correzione esplicita dell'utente («PRINCIPESSA» nel
  // copione, «PRINCENERENTOLA» fra gli asset) e vince su qualunque
  // corrispondenza per nome.
  const QString aliased = speakerAlias(n);
  if (!aliased.isEmpty())
    if (const Asset *a = assetByUuid(aliased)) return a;
  for (const Asset &a : m_assets)
    if (a.name.compare(n, Qt::CaseInsensitive) == 0) return &a;
  return nullptr;
}

QString ZtoryModel::assetNameKey(const QString &name) {
  const QString decomposed = name.normalized(QString::NormalizationForm_D);
  QString key;
  for (const QChar &c : decomposed)
    if (c.isLetterOrNumber()) key += c.toLower();  // accents' marks dropped
  return key;
}

// One typo apart: one letter replaced, or one letter added/missing INSIDE the
// name. Never at the ends and never a digit: «macchina2», «prop1»/«prop2»,
// «macchinav» (from «_v03») are other things, not typos (2026-09-27).
static bool oneTypoApart(const QString &a, const QString &b) {
  if (a.size() == b.size()) {
    int diff = -1;
    for (int i = 0; i < a.size(); ++i)
      if (a[i] != b[i]) {
        if (diff >= 0) return false;
        diff = i;
      }
    return diff >= 0 && !a[diff].isDigit() && !b[diff].isDigit();
  }
  const QString &lng = a.size() > b.size() ? a : b;
  const QString &sht = a.size() > b.size() ? b : a;
  if (lng.size() != sht.size() + 1) return false;
  for (int i = 1; i + 1 < lng.size(); ++i)  // inside only
    if (!lng[i].isDigit() && lng.left(i) + lng.mid(i + 1) == sht) return true;
  return false;
}

static QString fileNamePart(const QString &name);  // defined with the naming tokens

QString ZtoryModel::assetTypeFileCode(const QString &type) {
  if (type.compare("Prop", Qt::CaseInsensitive) == 0) return "PS";
  if (type.compare("Environment", Qt::CaseInsensitive) == 0) return "BG";
  if (isCharacterType(type)) return "CH";
  if (type.compare("FX", Qt::CaseInsensitive) == 0) return "FX";
  return QString();
}

namespace {

// Not asset files: backups, hidden files, the painting apps' own documents.
bool isAssetCandidate(const QFileInfo &fi) {
  const QString n = fi.fileName();
  if (n.startsWith('.') || n.endsWith('~')) return false;
  static const QStringList skip = {"af", "afdesign", "afphoto", "kra", "tmp",
                                   "bak", "db"};
  return !skip.contains(fi.suffix().toLower());
}

// When the same drawing exists in several formats, the one to import.
int formatRank(const QString &suffix) {
  static const QStringList order = {"psd", "tlv", "pli", "tif", "tiff",
                                    "png", "jpg", "jpeg"};
  const int i = order.indexOf(suffix.toLower());
  return i < 0 ? order.size() : i;
}

// The words of a name, as the convention compares them: lower case, no
// accents, letters and digits only. «Bacchetta Magica» → {bacchetta, magica}.
QSet<QString> nameWords(const QString &name) {
  QSet<QString> words;
  QString w;
  for (const QChar &c : name.normalized(QString::NormalizationForm_D)) {
    if (c.isLetterOrNumber()) w += c.toLower();
    else if (c.category() != QChar::Mark_NonSpacing) {
      if (!w.isEmpty()) words.insert(w);
      w.clear();
    }
  }
  if (!w.isEmpty()) words.insert(w);
  return words;
}

// The studio's convention CODE_TYPE_name_Vn (Franco, 2026-09-27):
// «CS2606_PS_bacchetta_V1» → name «bacchetta», version 1. The type code is the
// first or second token; the version is optional. False when the file does not
// follow it for this type code.
bool parseConventionName(const QString &base, const QString &typeCode,
                         QString *name, int *version) {
  static const QRegularExpression sep("[_\\s]+");
  static const QRegularExpression verRe("^[Vv](\\d+)$");
  QStringList tok = base.split(sep, Qt::SkipEmptyParts);
  int t = -1;
  for (int i = 0; i < qMin(2, tok.size()); ++i)
    if (tok[i].compare(typeCode, Qt::CaseInsensitive) == 0) { t = i; break; }
  if (t < 0) return false;
  tok = tok.mid(t + 1);
  *version = 0;
  if (!tok.isEmpty()) {
    const QRegularExpressionMatch v = verRe.match(tok.last());
    if (v.hasMatch()) {
      *version = v.captured(1).toInt();
      tok.removeLast();
    }
  }
  if (tok.isEmpty()) return false;
  *name = tok.join(' ');
  return true;
}

}  // namespace

QString ZtoryModel::resolveAssetFile(const Asset &a, QString *why,
                                     QHash<QString, QFileInfoList> *dirCache,
                                     AssetMatch *match) const {
  if (match) *match = AssetMatch::None;
  auto fail = [&](const QString &msg) {
    if (why) *why = msg;
    return QString();
  };
  auto found = [&](const QString &file, AssetMatch how, const QString &msg) {
    if (why) *why = msg;
    if (match) *match = how;
    return file;
  };
  // 0. No file on purpose: nothing to look for, nothing to deduce.
  if (a.noFile) {
    if (match) *match = AssetMatch::NoFile;
    return fail(tr("no file on purpose: drawn inside another asset"));
  }
  // 1. Il legame esplicito VINCE sempre. E' l'unica risposta che non e' una
  //    supposizione, quindi non si discute e non si cerca oltre.
  if (!a.filePath.isEmpty()) {
    if (QFile::exists(a.filePath)) return found(a.filePath, AssetMatch::Linked, QString());
    return fail(tr("linked file is missing: %1").arg(a.filePath));
  }

  // 2. Altrimenti la cartella della categoria.
  const QString dir = assetDirForType(a.type);
  if (dir.isEmpty())
    return fail(ZtoryModel::isCharacterType(a.type)
                    ? tr("a character has no folder: link its scene")
                    : tr("no folder set for type %1").arg(a.type));
  if (!QDir(dir).exists()) return fail(tr("folder not found: %1").arg(dir));

  QFileInfoList listed;
  if (!dirCache || !dirCache->contains(dir)) {
    listed = QDir(dir).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    if (dirCache) dirCache->insert(dir, listed);
  }
  const QFileInfoList &all = dirCache ? (*dirCache)[dir] : listed;
  QFileInfoList entries;  // backups and apps' documents are not asset files
  for (const QFileInfo &fi : all)
    if (isAssetCandidate(fi)) entries << fi;

  // 2a. The exact name, case aside, any extension. Both names in the same
  //     Unicode form: macOS often stores file names decomposed (NFD).
  //     Volutamente NON si accettano prefissi o suffissi qui: «macchina» non
  //     deve pescare «macchina_v03» ne' «macchina_rotta».
  QStringList hits;
  const QString nameNfc = a.name.normalized(QString::NormalizationForm_C);
  for (const QFileInfo &fi : entries)
    if (fi.completeBaseName()
            .normalized(QString::NormalizationForm_C)
            .compare(nameNfc, Qt::CaseInsensitive) == 0)
      hits << fi.absoluteFilePath();
  // Ambiguo: NON si indovina. Due file con lo stesso nome e estensione diversa
  // (macchina.tlv e macchina.psd) sono una scelta dell'utente.
  if (hits.size() > 1)
    return fail(tr("%1 files named «%2» in %3 — link the right one")
                    .arg(hits.size()).arg(a.name, dir));
  if (hits.size() == 1) return found(hits.first(), AssetMatch::Exact, QString());

  // Files that belong to ANOTHER asset — its name, or its link — are never
  // deduced for this one: «LIBRI» must not take «LIBRO.psd» when LIBRO is a
  // prop of its own. (Only here: an exact name needs none of this.)
  QSet<QString> othersKeys, othersFiles;
  QList<QSet<QString>> othersWords;  // same type: for the convention
  for (const Asset &o : m_assets) {
    if (o.uuid == a.uuid) continue;
    othersKeys.insert(assetNameKey(o.name));
    if (o.type.compare(a.type, Qt::CaseInsensitive) == 0)
      othersWords << nameWords(o.name);
    if (!o.filePath.isEmpty())
      othersFiles.insert(QFileInfo(o.filePath).absoluteFilePath().toLower());
  }
  auto belongsToOther = [&](const QFileInfo &fi) {
    return othersFiles.contains(fi.absoluteFilePath().toLower());
  };

  // 2b. Nearly the name (Franco, 2026-09-27): written another way — case,
  //     spaces, dashes, underscores, accents — or one typo apart.
  const QString key = assetNameKey(a.name);
  QStringList nearHits;
  for (const QFileInfo &fi : entries) {
    const QString k = assetNameKey(fi.completeBaseName());
    if (k.isEmpty() || key.isEmpty()) continue;
    if (othersKeys.contains(k) || belongsToOther(fi)) continue;
    if (k == key || (key.size() >= 5 && oneTypoApart(k, key)))
      nearHits << fi.absoluteFilePath();
  }
  if (nearHits.size() > 1)
    return fail(tr("%1 files with a name like «%2» in %3 — link the right one")
                    .arg(nearHits.size()).arg(a.name, dir));
  if (nearHits.size() == 1)
    return found(nearHits.first(), AssetMatch::NearName,
                 tr("the file's name differs: «%1»")
                     .arg(QFileInfo(nearHits.first()).fileName()));

  // 2c. The studio's naming convention (Franco, 2026-09-27): the production's
  //     («Asset file names»), then a tolerant CODE_TYPE_name_Vn. The file is
  //     the asset's if every word of its name is in the asset's (or the name
  //     is nearly the asset's) and it fits NO other asset of the type. Files
  //     with the asset's whole name come first; then the highest version;
  //     then the format to import (psd before png).
  const QString code = assetTypeFileCode(a.type);
  if (code.isEmpty()) return fail(tr("no file named «%1» in %2").arg(a.name, dir));
  const QRegularExpression convRe = assetFileRegex(a);  // compiled once
  const QSet<QString> assetWords = nameWords(a.name);
  struct Cand {
    QFileInfo fi;
    QString part, nameKey;
    int version;
    bool byPattern, wholeName;
  };
  QList<Cand> cands;
  for (const QFileInfo &fi : entries) {
    QString part;
    int version     = 0;
    const bool byPattern =
        matchAssetFileName(convRe, fi.completeBaseName(), &part, &version);
    if (!byPattern &&
        !parseConventionName(fi.completeBaseName(), code, &part, &version))
      continue;
    const QSet<QString> fileWords = nameWords(part);
    if (fileWords.isEmpty()) continue;
    const QString partKey = assetNameKey(part);
    const bool fits = assetWords.contains(fileWords) || partKey == key ||
                      (key.size() >= 5 && oneTypoApart(partKey, key));
    if (!fits || belongsToOther(fi)) continue;
    bool shared = false;  // «libro» fits LIBRO FAVOLE and LIBRO FORMULA 1
    for (const QSet<QString> &ow : othersWords)
      if (ow.contains(fileWords)) { shared = true; break; }
    if (shared) continue;
    QStringList sorted = fileWords.values();
    sorted.sort();
    cands << Cand{fi, part, sorted.join(' '), version, byPattern,
                  fileWords == assetWords || partKey == key};
  }
  // A file with the asset's whole name beats a partial one: «bacchetta» V1
  // and «bacchetta-magica» V2 for BACCHETTA MAGICA are not ambiguous.
  bool anyWhole = false;
  for (const Cand &c : cands) anyWhole = anyWhole || c.wholeName;
  if (anyWhole)
    for (int i = cands.size() - 1; i >= 0; --i)
      if (!cands[i].wholeName) cands.removeAt(i);
  if (!cands.isEmpty()) {
    QSet<QString> names;
    for (const Cand &c : cands) names.insert(c.nameKey);
    if (names.size() > 1)
      return fail(tr("%1 files fit «%2» by the naming convention in %3 — link "
                     "the right one")
                      .arg(cands.size()).arg(a.name, dir));
    const Cand *best = &cands.first();
    for (const Cand &c : cands)
      if (c.version > best->version ||
          (c.version == best->version &&
           formatRank(c.fi.suffix()) < formatRank(best->fi.suffix())))
        best = &c;
    for (const Cand &c : cands)  // a true tie: not ours to choose
      if (&c != best && c.version == best->version &&
          formatRank(c.fi.suffix()) == formatRank(best->fi.suffix()))
        return fail(tr("%1 and %2 both fit «%3» — link the right one")
                        .arg(best->fi.fileName(), c.fi.fileName(), a.name));
    QSet<int> versions;
    for (const Cand &c : cands) versions.insert(c.version);
    // Named EXACTLY by the production's convention: not a deduction, green.
    // Only by the production's pattern — the tolerant rule also reads other
    // episodes' files (CS2605_…), which are not «exactly» anything.
    const bool exactName =
        best->byPattern && fileNamePart(best->part) == fileNamePart(a.name);
    QString msg = exactName ? QString()
                            : tr("found by the naming convention: «%1»")
                                  .arg(best->fi.fileName());
    if (versions.size() > 1)
      msg += (msg.isEmpty() ? QString() : QString(" ")) +
             tr("(the latest of %1 versions)").arg(versions.size());
    return found(best->fi.absoluteFilePath(),
                 exactName ? AssetMatch::Exact : AssetMatch::Convention, msg);
  }
  return fail(tr("no file named «%1» in %2").arg(a.name, dir));
}

QString ZtoryModel::assetDirForType(const QString &type) const {
  // Character: nessuna cartella. Nel cutout digitale un personaggio e' una
  // SCENA dello stesso progetto e si importa come sotto-scena; nel tradigital
  // di lui si importa il model sheet, che ha la sua cartella a parte.
  if (type.compare("Prop", Qt::CaseInsensitive) == 0) return m_propsDir;
  if (type.compare("Environment", Qt::CaseInsensitive) == 0)
    return m_backgroundsDir;
  return QString();
}

void ZtoryModel::addAssetTaskType(const QString &type, const QString &taskType) {
  const QString name = taskType.trimmed();
  if (name.isEmpty()) return;

  AssetType *t = nullptr;
  for (auto &at : m_assetTypes)
    if (at.name.compare(type, Qt::CaseInsensitive) == 0) { t = &at; break; }
  if (!t) {
    // An asset type Kitsu has and we don't: start it from the canonical
    // pipeline so it isn't born with a single stray column.
    m_assetTypes.push_back(AssetType{type, canonicalAssetTaskOrder()});
    t = &m_assetTypes.back();
  }
  // Case-insensitive on purpose: Kitsu's «clean» and our «Clean» are the same
  // step, and adopting both would show two columns for one piece of work.
  for (const QString &existing : t->taskTypes)
    if (existing.compare(name, Qt::CaseInsensitive) == 0) return;
  t->taskTypes.push_back(name);
}

QStringList ZtoryModel::assetTaskColumns() const {
  // Which task types are in play = union across the types the assets actually
  // use. Ordered by the asset-type pipelines (the sequence set in the editor),
  // not a fixed list — reordering a type's tasks reflects in the asset table.
  std::set<QString> used;
  for (const Asset &as : m_assets)
    for (const QString &tt : assetTaskTypesForType(as.type)) used.insert(tt);
  QStringList cols;
  for (const AssetType &t : m_assetTypes)
    for (const QString &tt : t.taskTypes)
      if (used.count(tt)) { cols << tt; used.erase(tt); }
  // Fallbacks: canonical order, then anything still left (custom/legacy tasks).
  for (const QString &tt : canonicalAssetTaskOrder())
    if (used.count(tt)) { cols << tt; used.erase(tt); }
  for (const QString &tt : used) cols << tt;
  return cols;
}

QString ZtoryModel::techniqueForShot(int shotIdx) const {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return m_defaultTechnique;
  const QString &t = m_shots[shotIdx].technique;
  return t.isEmpty() ? m_defaultTechnique : t;
}

QStringList ZtoryModel::taskTypesForShot(int shotIdx) const {
  const Technique *t = findTechnique(techniqueForShot(shotIdx));
  return t ? t->taskTypes : QStringList();
}

void ZtoryModel::setShotTaskStatus(int shotIdx, const QString &taskType,
                                   TaskStatus status) {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  m_shots[shotIdx].tasks[taskType].status = status;
  emit taskStatusChanged();
}

void ZtoryModel::setShotTaskStatusByLabel(const QString &shotLabel,
                                          const QString &taskType,
                                          TaskStatus status) {
  for (int i = 0; i < (int)m_shots.size(); i++)
    if (m_shots[i].label() == shotLabel) {
      setShotTaskStatus(i, taskType, status);
      return;
    }
}

void ZtoryModel::setShotTaskAssignees(int shotIdx, const QString &taskType,
                                      const QStringList &assignees) {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  m_shots[shotIdx].tasks[taskType].assignees = assignees;
  emit taskStatusChanged();
}

void ZtoryModel::setShotTaskAssigneesByLabel(const QString &shotLabel,
                                             const QString &taskType,
                                             const QStringList &assignees) {
  for (int i = 0; i < (int)m_shots.size(); i++)
    if (m_shots[i].label() == shotLabel) {
      setShotTaskAssignees(i, taskType, assignees);
      return;
    }
}

// ─── Assets ─────────────────────────────────────────────────────────────────

const QStringList &ZtoryModel::canonicalAssetTypes() {
  static const QStringList types = {"Character", "Prop", "FX", "Environment"};
  return types;
}

const QStringList &ZtoryModel::canonicalAssetTaskOrder() {
  static const QStringList order = {"Concept", "Rough", "Clean", "Color"};
  return order;
}

void ZtoryModel::addAsset(const QString &type, const QString &name) {
  Asset a;
  a.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
  a.type = type;
  a.name = name;
  m_assets.push_back(a);
  emit assetsChanged();
}

void ZtoryModel::removeAssetAt(int i) {
  if (i < 0 || i >= (int)m_assets.size()) return;
  m_assets.erase(m_assets.begin() + i);
  emit assetsChanged();
}

// Ztoryc: a hand edit in the tracker. It goes through ZtoryTaskFlow so the
// rules (a Done readies the next task) and the push to Kitsu see it too.
void ZtoryModel::setAssetTaskStatus(int i, const QString &taskType,
                                    TaskStatus status) {
  if (i < 0 || i >= (int)m_assets.size()) return;
  ZtoryTaskFlow::setStatus(ZtoryTaskFlow::Entity::Asset, m_assets[i].uuid,
                           taskType, status, ZtoryTaskFlow::Origin::User,
                           /*batch=*/true);
  emit assetsChanged();
}

void ZtoryModel::setAssetTaskStatusByUuid(const QString &uuid,
                                          const QString &taskType,
                                          TaskStatus status) {
  for (int i = 0; i < (int)m_assets.size(); i++)
    if (m_assets[i].uuid == uuid) { setAssetTaskStatus(i, taskType, status); return; }
}

TaskStatus ZtoryModel::taskStatusOf(int entity, const QString &uuid,
                                    const QString &taskType, bool *found) const {
  const QMap<QString, TaskState> *tasks = tasksOf(entity, uuid);
  if (found) *found = tasks != nullptr;
  return tasks ? tasks->value(taskType).status : TaskStatus::Todo;
}

QMap<QString, TaskState> *ZtoryModel::tasksOf(int entity, const QString &uuid) {
  if (entity == 0) {
    for (ProjectShot &ps : m_projectShots)
      if (ps.uuid == uuid) return &ps.tasks;
  } else {
    for (Asset &a : m_assets)
      if (a.uuid == uuid) return &a.tasks;
  }
  return nullptr;
}

const QMap<QString, TaskState> *ZtoryModel::tasksOf(int entity,
                                                    const QString &uuid) const {
  return const_cast<ZtoryModel *>(this)->tasksOf(entity, uuid);
}

bool ZtoryModel::setTaskSynced(int entity, const QString &uuid,
                               const QString &taskType, TaskStatus synced) {
  QMap<QString, TaskState> *tasks = tasksOf(entity, uuid);
  if (!tasks) return false;
  TaskState &ts = (*tasks)[taskType];
  if (ts.hasSynced && ts.synced == synced) return false;  // nothing moved
  ts.hasSynced = true;
  ts.synced    = synced;
  return true;
}

bool ZtoryModel::taskSyncedOf(int entity, const QString &uuid,
                              const QString &taskType,
                              TaskStatus *synced) const {
  const QMap<QString, TaskState> *tasks = tasksOf(entity, uuid);
  if (!tasks) return false;
  const auto it = tasks->constFind(taskType);
  if (it == tasks->constEnd() || !it.value().hasSynced) return false;
  if (synced) *synced = it.value().synced;
  return true;
}

bool ZtoryModel::writeTaskStatus(int entity, const QString &uuid,
                                 const QString &taskType, TaskStatus status) {
  QMap<QString, TaskState> *tasks = tasksOf(entity, uuid);
  if (!tasks) return false;
  (*tasks)[taskType].status = status;
  // A project-shot write is mirrored into the open scene's shot.
  if (entity == 0)
    for (ShotData &sd : m_shots)
      if (sd.uuid == uuid) { sd.tasks[taskType].status = status; break; }
  return true;
}

void ZtoryModel::setAssetTaskAssignees(int i, const QString &taskType,
                                       const QStringList &assignees) {
  if (i < 0 || i >= (int)m_assets.size()) return;
  m_assets[i].tasks[taskType].assignees = assignees;
  emit assetsChanged();
}

void ZtoryModel::setAssetTaskAssigneesByUuid(const QString &uuid,
                                             const QString &taskType,
                                             const QStringList &assignees) {
  for (int i = 0; i < (int)m_assets.size(); i++)
    if (m_assets[i].uuid == uuid) { setAssetTaskAssignees(i, taskType, assignees); return; }
}

//-----------------------------------------------------------------------------
// Cache condivisa dei render di anteprima — vedi il commento in ztorymodel.h.

QPixmap ZtoryModel::cachedPanelRender(const QString &key) const {
  return m_panelRenderCache.value(key);
}

void ZtoryModel::cachePanelRender(const QString &key, const QPixmap &px) {
  if (key.isEmpty() || px.isNull()) return;
  // Svuotamento totale al superamento del tetto: una scena lunga a piu'
  // risoluzioni riempirebbe la memoria di pixmap grandi. Ricostruire e' lento
  // ma corretto; una politica di sfratto piu' furba sarebbe solo un altro posto
  // dove sbagliare.
  if (m_panelRenderCache.size() >= kPanelRenderCacheMax)
    m_panelRenderCache.clear();
  m_panelRenderCache.insert(key, px);
}

void ZtoryModel::invalidatePanelRenders(const QString &subSceneName) {
  if (subSceneName.isEmpty()) return;
  // La chiave comincia col nome della sotto-scena seguito da '|', quindi un
  // confronto di prefisso prende tutte le sue varianti (frame, dimensioni,
  // regione) senza toccare le altre sotto-scene. Il separatore evita che
  // "sh01" cancelli anche "sh010".
  const QString prefix = subSceneName + QLatin1Char('|');
  for (auto it = m_panelRenderCache.begin();
       it != m_panelRenderCache.end();) {
    if (it.key().startsWith(prefix))
      it = m_panelRenderCache.erase(it);
    else
      ++it;
  }
}

void ZtoryModel::clearPanelRenderCache() { m_panelRenderCache.clear(); }

//-----------------------------------------------------------------------------

void ZtoryModel::resetProjectLevelDefaults() {
  m_production.clear();
  m_code.clear();
  m_season.clear();
  m_title.clear();
  m_episode.clear();
  m_namingPattern.clear();
  m_episodeNumber.clear();
  m_assetFilePattern.clear();
  m_defaultTechnique.clear();
  m_team.clear();
  m_assets.clear();
  m_techniques.clear();
  m_assetTypes.clear();
  m_projectShots.clear();
  m_storyboardFiles.clear();
  // Kitsu binding + opt-in flag are per-project too: clear them so a new project
  // doesn't inherit the previous project's Kitsu link (which would wrongly keep
  // the Kitsu UI visible even when the new project disabled it).
  m_useKitsu = false;
  m_kitsuProjectId.clear();
  m_kitsuProjectName.clear();
  m_kitsuEpisodeId.clear();
  m_propsDir.clear();
  m_backgroundsDir.clear();
  m_modelSheetDir.clear();
  m_defaultImportPolicy = AssetImportPolicy();
  m_speakerAliases.clear();
  m_productionType.clear();
  m_productionStyle.clear();
  m_ratio.clear();
  m_resolution.clear();
  seedDefaultTechniques();  // re-seed presets + defaultTechnique = "Tradigital"
  seedDefaultAssetTypes();
}

// ─── Project DB (production.ztrack) — B3 pilot: team roster ───────────────────

namespace {
TFilePath projectDbFilePath() {
  auto proj = TProjectManager::instance()->getCurrentProject();
  if (!proj) return TFilePath();
  return proj->getProjectFolder() + TFilePath("production.ztrack");
}
}  // namespace

QString ZtoryModel::projectDbPath() const {
  TFilePath fp = projectDbFilePath();
  return fp == TFilePath() ? QString()
                           : QString::fromStdWString(fp.getWideString());
}

static QString thumbsDir() {
  TFilePath fp = projectDbFilePath();
  if (fp == TFilePath()) return QString();
  return QString::fromStdWString(
             (fp.getParentDir() + TFilePath("thumbs")).getWideString());
}

void ZtoryModel::updateThumbCache(const QString &uuid, const QPixmap &pm) {
  if (uuid.isEmpty() || pm.isNull()) return;
  m_thumbCache[uuid] = pm;
  // Persist to disk so other sessions and scene-switches can reload it.
  QString dir = thumbsDir();
  if (dir.isEmpty()) return;
  QDir().mkpath(dir);
  pm.save(dir + "/" + uuid + ".png", "PNG");
}

void ZtoryModel::evictThumbFromDisk(const QString &uuid) {
  if (uuid.isEmpty()) return;
  m_thumbCache.remove(uuid);
  QString dir = thumbsDir();
  if (!dir.isEmpty()) QFile::remove(dir + "/" + uuid + ".png");
}

void ZtoryModel::loadThumbsFromDisk() {
  QString dir = thumbsDir();
  if (dir.isEmpty()) return;
  QDir d(dir);
  if (!d.exists()) return;
  for (const QString &fn : d.entryList({"*.png"}, QDir::Files)) {
    QString uuid = fn.left(fn.length() - 4);  // strip ".png"
    if (!m_thumbCache.contains(uuid)) {       // don't overwrite in-memory version
      QPixmap pm;
      if (pm.load(dir + "/" + fn))
        m_thumbCache[uuid] = pm;
    }
  }
}

void ZtoryModel::saveAndNotifyTasks() {
  saveProjectDb();
  emit taskStatusChanged();
}

void ZtoryModel::saveProjectDb() {
  TFilePath fp = projectDbFilePath();
  if (fp == TFilePath()) return;
  QString path = QString::fromStdWString(fp.getWideString());

  // DATA-LOSS FIREWALL: never overwrite an existing project DB when the model's
  // project metadata, team AND assets are ALL empty. That combination is an
  // unnatural state for a real project (it's the signature of a transient
  // resetProjectLevelDefaults() that loadProjectDb() hasn't repopulated yet, or
  // a stray save during a scene/room switch). Shots may be present (published
  // from the scene) — without this guard such a save wipes production/team/
  // assets while keeping the shots, exactly the observed data loss.
  const bool metaEmpty = projectMetaEmpty();
  if (metaEmpty && QFile::exists(path)) {
    // Block ONLY if the on-disk file actually carries metadata we would wipe
    // (the transient-reset data-loss case). A brand-new project legitimately
    // has empty meta — letting its shots persist is what enables a project
    // Tracker to aggregate multiple storyboards. So if the on-disk meta is ALSO
    // empty, there is nothing to lose: proceed with the save.
    QFile rf(path);
    if (rf.open(QIODevice::ReadOnly | QIODevice::Text)) {
      const QString disk = QString::fromUtf8(rf.readAll());
      rf.close();
      const bool diskHasMeta =
          disk.contains(QRegularExpression("production=\"[^\"]+\"")) ||
          disk.contains(QRegularExpression("season=\"[^\"]+\"")) ||
          disk.contains(QRegularExpression("episode=\"[^\"]+\"")) ||
          disk.contains(QRegularExpression("title=\"[^\"]+\"")) ||
          disk.contains("<person ") || disk.contains("<asset ");
      if (diskHasMeta) return;  // would wipe real metadata → block
    }
  }

  const QByteArray ours = serializeProjectDb();
  const QString key     = ZtoryLocks::canonicalPath(path);
  // One instance at a time between reading the file and writing it back.
  QLockFile lock(ZtoryLocks::lockFilePath("ztrack", key));
  lock.setStaleLockTime(0);  // a crashed holder is detected by its pid
  if (!lock.tryLock(3000))
    qWarning("production.ztrack: lock busy, saving without it");

  QByteArray out = ours;
  bool reload    = false;
  bool write     = true;
  QFile df(path);
  if (key == m_dbBasePath && df.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QByteArray disk = df.readAll();
    df.close();
    // The file changed since this instance last read or wrote it: another
    // instance (or Drive) wrote it. Merge, don't overwrite.
    if (!ZtrackMerge::sameContent(disk, m_dbBase)) {
      QStringList conflicts;
      bool ok = false;
      const QByteArray merged =
          ZtrackMerge::merge(m_dbBase, ours, disk, &conflicts, &ok);
      if (!ok) {
        qWarning("production.ztrack: the file on disk does not parse, "
                 "overwritten with this window's tracker");
      } else {
        for (const QString &c : conflicts)
          qWarning("production.ztrack merge: %s", qPrintable(c));
        out    = merged;
        reload = !ZtrackMerge::sameContent(merged, ours);
        // Everything of ours is already on disk: nothing to write.
        if (ZtrackMerge::sameContent(merged, disk)) write = false;
      }
    }
  }
  if (write && !writeProjectDbFile(path, out)) return;
  // The memory is still «ours»: that is what it derives from. When the merge
  // brought in changes from elsewhere, the reload below reads them in, after
  // the caller of this save is done with its indices.
  m_dbBase     = ours;
  m_dbBasePath = key;
  watchProjectDb(path);
  if (reload && m_dbReloadTimer) m_dbReloadTimer->start(0);
}

QByteArray ZtoryModel::serializeProjectDb() const {
  QByteArray bytes;
  QXmlStreamWriter xml(&bytes);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement("ztrack");
  xml.writeAttribute("version", "1");

  xml.writeStartElement("project");
  xml.writeAttribute("production", m_production);
  if (!m_code.isEmpty()) xml.writeAttribute("code", m_code);
  xml.writeAttribute("season",     m_season);
  xml.writeAttribute("episode",    m_episode);
  xml.writeAttribute("title",      m_title);
  xml.writeAttribute("defaultTechnique", m_defaultTechnique);
  if (!m_namingPattern.isEmpty())
    xml.writeAttribute("namingPattern", m_namingPattern);
  if (!m_episodeNumber.isEmpty())
    xml.writeAttribute("episodeNumber", m_episodeNumber);
  if (!m_assetFilePattern.isEmpty())
    xml.writeAttribute("assetFilePattern", m_assetFilePattern);
  // Opt-in Kitsu: the sync UI only shows when the project enables it (chosen at
  // creation). The Production Tracker itself is always available.
  if (m_useKitsu) xml.writeAttribute("useKitsu", "1");
  // Kitsu (M5) binding + mirrored metadata.
  if (!m_kitsuProjectId.isEmpty()) {
    xml.writeAttribute("kitsuProjectId",   m_kitsuProjectId);
    xml.writeAttribute("kitsuProjectName", m_kitsuProjectName);
  }
  // The episode NAME is already saved as "episode"; this is the stable id that
  // survives a rename on the Kitsu side.
  if (!m_kitsuEpisodeId.isEmpty())
    xml.writeAttribute("kitsuEpisodeId", m_kitsuEpisodeId);
  // Cartelle degli asset per categoria (export completo).
  if (!m_propsDir.isEmpty())       xml.writeAttribute("propsDir", m_propsDir);
  if (!m_backgroundsDir.isEmpty()) xml.writeAttribute("backgroundsDir", m_backgroundsDir);
  if (!m_modelSheetDir.isEmpty())  xml.writeAttribute("modelSheetDir", m_modelSheetDir);
  writeImportPolicy(xml, m_defaultImportPolicy);
  if (!m_productionType.isEmpty())  xml.writeAttribute("productionType",  m_productionType);
  if (!m_productionStyle.isEmpty()) xml.writeAttribute("productionStyle", m_productionStyle);
  if (!m_ratio.isEmpty())           xml.writeAttribute("ratio",           m_ratio);
  if (!m_resolution.isEmpty())      xml.writeAttribute("resolution",      m_resolution);
  // Gli alias sono FIGLI, non attributi: sono una lista, e gli attributi
  // vanno scritti tutti prima di aprire qualunque figlio.
  for (auto it = m_speakerAliases.constBegin(); it != m_speakerAliases.constEnd(); ++it) {
    xml.writeStartElement("alias");
    xml.writeAttribute("name",  it.key());
    xml.writeAttribute("asset", it.value());
    xml.writeEndElement();
  }
  xml.writeEndElement();

  xml.writeStartElement("team");
  for (const QString &p : m_team) {
    xml.writeStartElement("person");
    xml.writeAttribute("name", p);
    xml.writeEndElement();
  }
  xml.writeEndElement();  // team

  xml.writeStartElement("techniques");
  for (const Technique &t : m_techniques) {
    xml.writeStartElement("technique");
    xml.writeAttribute("name",  t.name);
    xml.writeAttribute("tasks", t.taskTypes.join("|"));
    xml.writeEndElement();
  }
  xml.writeEndElement();  // techniques

  xml.writeStartElement("assetTypes");
  for (const AssetType &t : m_assetTypes) {
    xml.writeStartElement("assetType");
    xml.writeAttribute("name",  t.name);
    xml.writeAttribute("tasks", t.taskTypes.join("|"));
    xml.writeEndElement();
  }
  xml.writeEndElement();  // assetTypes

  xml.writeStartElement("assets");
  for (const Asset &as : m_assets) {
    xml.writeStartElement("asset");
    xml.writeAttribute("uuid", as.uuid);
    xml.writeAttribute("type", as.type);
    xml.writeAttribute("name", as.name);
    if (!as.kitsuAssetId.isEmpty())
      xml.writeAttribute("kitsuAssetId", as.kitsuAssetId);
    if (as.kitsuMainPack) xml.writeAttribute("mainPack", "1");
    if (!as.tags.isEmpty()) xml.writeAttribute("tags", as.tags.join("|"));
    if (!as.filePath.isEmpty()) xml.writeAttribute("file", as.filePath);
    if (!as.rigPsdPath.isEmpty()) xml.writeAttribute("rigPsd", as.rigPsdPath);
    if (as.noFile) xml.writeAttribute("noFile", "1");
    if (!as.previewSig.isEmpty()) xml.writeAttribute("previewSig", as.previewSig);
    if (!as.importPolicy.isDefault()) writeImportPolicy(xml, as.importPolicy);
    for (auto it = as.tasks.constBegin(); it != as.tasks.constEnd(); ++it) {
      xml.writeStartElement("atask");
      xml.writeAttribute("type",   it.key());
      xml.writeAttribute("status", taskStatusLabel(it.value().status));
      if (it.value().hasSynced)
        xml.writeAttribute("synced", taskStatusLabel(it.value().synced));
      if (!it.value().assignees.isEmpty())
        xml.writeAttribute("assignee", it.value().assignees.join(", "));
      xml.writeEndElement();
    }
    xml.writeEndElement();
  }
  xml.writeEndElement();  // assets

  xml.writeStartElement("storyboards");
  for (const QString &f : m_storyboardFiles) {
    xml.writeStartElement("storyboard");
    xml.writeAttribute("file", f);
    xml.writeEndElement();
  }
  xml.writeEndElement();  // storyboards

  xml.writeStartElement("shots");
  for (const ProjectShot &ps : m_projectShots) {
    xml.writeStartElement("shot");
    xml.writeAttribute("uuid",      ps.uuid);
    xml.writeAttribute("source",    ps.source);
    xml.writeAttribute("seq",       ps.seq);
    xml.writeAttribute("label",     ps.label);
    xml.writeAttribute("frames",    QString::number(ps.frames));
    if (!ps.technique.isEmpty())
      xml.writeAttribute("technique", ps.technique);
    if (!ps.kitsuShotId.isEmpty())
      xml.writeAttribute("kitsuShotId", ps.kitsuShotId);
    // An EMPTY base is still a base (Kitsu had nothing for the shot): the
    // attribute says it exists and which Kitsu shot it was taken on, the
    // <castSynced> children say what it holds.
    if (ps.hasBreakdownBase)
      xml.writeAttribute("castSynced", ps.breakdownBaseShotId);
    for (auto it = ps.tasks.constBegin(); it != ps.tasks.constEnd(); ++it) {
      xml.writeStartElement("task");
      xml.writeAttribute("type",   it.key());
      xml.writeAttribute("status", taskStatusLabel(it.value().status));
      if (it.value().hasSynced)
        xml.writeAttribute("synced", taskStatusLabel(it.value().synced));
      if (!it.value().assignees.isEmpty())
        xml.writeAttribute("assignee", it.value().assignees.join(", "));
      xml.writeEndElement();
    }
    for (const BreakdownEntry &be : ps.breakdown) {
      if (be.assetUuid.isEmpty()) continue;
      xml.writeStartElement("needs");
      xml.writeAttribute("asset", be.assetUuid);
      if (be.nbOccurrences != 1)
        xml.writeAttribute("n", QString::number(be.nbOccurrences));
      if (!be.label.isEmpty()) xml.writeAttribute("label", be.label);
      xml.writeEndElement();
    }
    for (const BreakdownEntry &be : ps.breakdownBase) {
      if (be.assetUuid.isEmpty()) continue;
      xml.writeStartElement("castSynced");
      xml.writeAttribute("asset", be.assetUuid);
      if (be.nbOccurrences != 1)
        xml.writeAttribute("n", QString::number(be.nbOccurrences));
      if (!be.label.isEmpty()) xml.writeAttribute("label", be.label);
      xml.writeEndElement();
    }
    xml.writeEndElement();  // shot
  }
  xml.writeEndElement();  // shots

  xml.writeEndElement();  // ztrack
  xml.writeEndDocument();
  return bytes;
}

bool ZtoryModel::projectMetaEmpty() const {
  return m_production.isEmpty() && m_title.isEmpty() && m_season.isEmpty() &&
         m_episode.isEmpty() && m_team.isEmpty() && m_assets.empty();
}

// Atomic: another instance reading the file meanwhile sees the old one or the
// new one, never half of it.
bool ZtoryModel::writeProjectDbFile(const QString &path,
                                    const QByteArray &bytes) {
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
  if (file.write(bytes) != bytes.size()) {
    file.cancelWriting();
    return false;
  }
  return file.commit();
}

void ZtoryModel::readProjectDbBytes(const QString &path,
                                    const QByteArray &bytes) {
  QBuffer buf;
  buf.setData(bytes);
  buf.open(QIODevice::ReadOnly | QIODevice::Text);
  loadProjectDbFromDevice(buf);
  m_dbBase     = bytes;
  m_dbBasePath = ZtoryLocks::canonicalPath(path);
  watchProjectDb(path);
}

void ZtoryModel::watchProjectDb(const QString &path) {
  if (!m_dbWatcher) return;
  const QStringList files = m_dbWatcher->files();
  // Re-added after every write: the atomic save replaces the file, and some
  // systems stop watching the one that was replaced.
  if (!files.isEmpty()) m_dbWatcher->removePaths(files);
  if (QFile::exists(path)) m_dbWatcher->addPath(path);
}

void ZtoryModel::holdDiskReload(bool hold) {
  m_dbReloadHold += hold ? 1 : -1;
  if (m_dbReloadHold < 0) m_dbReloadHold = 0;
  if (m_dbReloadHold == 0 && m_dbReloadPending && m_dbReloadTimer)
    m_dbReloadTimer->start(0);
}

void ZtoryModel::reloadProjectDbIfChangedOnDisk() {
  if (m_dbBasePath.isEmpty()) return;
  if (m_dbReloadHold > 0) {
    m_dbReloadPending = true;
    return;
  }
  m_dbReloadPending  = false;
  const QString path = m_dbBasePath;
  watchProjectDb(path);
  QLockFile lock(ZtoryLocks::lockFilePath("ztrack", path));
  lock.setStaleLockTime(0);
  if (!lock.tryLock(3000)) {
    m_dbReloadTimer->start(1000);  // the other instance is writing: later
    return;
  }
  QFile df(path);
  if (!df.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  const QByteArray disk = df.readAll();
  df.close();
  if (ZtrackMerge::sameContent(disk, m_dbBase)) return;
  if (!ZtrackMerge::isWellFormed(disk)) return;  // Drive mid-download: next event
  QByteArray merged = disk;
  // The same guard as saveProjectDb(): memory in a transient empty state must
  // not count as «everything deleted here».
  if (!projectMetaEmpty()) {
    QStringList conflicts;
    bool ok = false;
    merged = ZtrackMerge::merge(m_dbBase, serializeProjectDb(), disk,
                                &conflicts, &ok);
    if (!ok) return;
    for (const QString &c : conflicts)
      qWarning("production.ztrack merge: %s", qPrintable(c));
    if (!ZtrackMerge::sameContent(merged, disk) &&
        !writeProjectDbFile(path, merged))
      return;
  }
  // Released before the model is refreshed and the panels are told: a slot
  // that saves (saveProjectDb takes the same lock) would otherwise wait the
  // full 3 s on a lock held by this very process, the UI frozen, and then
  // write without it (review 2026-10-02). The file is written; what follows
  // works on `merged`, in memory.
  lock.unlock();
  readProjectDbBytes(path, merged);
  emit productionReloaded();
  emit assetsChanged();
  emit taskStatusChanged();
}

void ZtoryModel::loadProjectDbFromPath(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  readProjectDbBytes(path, file.readAll());
}

void ZtoryModel::loadProjectDb() {
  TFilePath fp = projectDbFilePath();
  if (fp == TFilePath()) return;
  QFile file(QString::fromStdWString(fp.getWideString()));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    // This project has no production.ztrack yet. CRITICAL: do NOT saveProjectDb()
    // with the model still holding the PREVIOUS project's data — that wrote one
    // project's shots/meta into another project's .ztrack (cross-project
    // contamination, observed when switching project then opening its Tracker).
    // Start from a clean slate, then seed an empty DB for this project.
    resetProjectLevelDefaults();
    saveProjectDb();
    return;
  }
  readProjectDbBytes(file.fileName(), file.readAll());
}

// Internal: parse a production.ztrack XML from an already-opened device.
void ZtoryModel::loadProjectDbFromDevice(QIODevice &file) {
  // Full clean slate before repopulating from THIS project's file. Some fields
  // are written conditionally (e.g. defaultTechnique, namingPattern, code), so
  // without a reset they would silently retain the previously-loaded project's
  // values — a subtle cross-project leak. Shots/team/assets are replaced wholesale
  // below; resetting first guarantees no field survives from another project.
  resetProjectLevelDefaults();

  QStringList team;
  std::vector<Technique> techs;
  std::vector<AssetType> atypes;
  std::vector<Asset> assets;
  std::vector<ProjectShot> pshots;
  QVector<QString> sboards;
  int ai = -1, psi = -1;
  QXmlStreamReader xml(&file);
  while (!xml.atEnd()) {
    xml.readNext();
    if (!xml.isStartElement()) continue;
    if (xml.name() == QLatin1String("project")) {
      auto a = xml.attributes();
      m_production = a.value("production").toString();
      m_code       = a.value("code").toString();
      m_season     = a.value("season").toString();
      m_episode    = a.value("episode").toString();
      m_title      = a.value("title").toString();
      if (a.hasAttribute("defaultTechnique"))
        m_defaultTechnique = a.value("defaultTechnique").toString();
      if (a.hasAttribute("namingPattern"))
        m_namingPattern = a.value("namingPattern").toString();
      m_episodeNumber    = a.value("episodeNumber").toString();
      m_assetFilePattern = a.value("assetFilePattern").toString();
      m_useKitsu = (a.value("useKitsu").toString() == "1");
      // Kitsu (M5) binding + mirrored metadata.
      m_kitsuProjectId   = a.value("kitsuProjectId").toString();
      m_kitsuProjectName = a.value("kitsuProjectName").toString();
      // Assente nei progetti salvati prima del legame per episodio: resta vuoto
      // e il comportamento e' quello di prima (nessun filtro).
      m_kitsuEpisodeId   = a.value("kitsuEpisodeId").toString();
      m_propsDir         = a.value("propsDir").toString();
      m_backgroundsDir   = a.value("backgroundsDir").toString();
      m_modelSheetDir    = a.value("modelSheetDir").toString();
      m_defaultImportPolicy = readImportPolicy(a);
    } else if (xml.name() == QLatin1String("alias")) {
      auto a = xml.attributes();
      const QString n = a.value("name").toString();
      const QString u = a.value("asset").toString();
      if (!n.isEmpty() && !u.isEmpty()) m_speakerAliases.insert(n.toLower(), u);
      m_productionType   = a.value("productionType").toString();
      m_productionStyle  = a.value("productionStyle").toString();
      m_ratio            = a.value("ratio").toString();
      m_resolution       = a.value("resolution").toString();
    } else if (xml.name() == QLatin1String("person")) {
      QString nm = xml.attributes().value("name").toString().trimmed();
      if (!nm.isEmpty()) team << nm;
    } else if (xml.name() == QLatin1String("technique")) {
      Technique t;
      t.name      = xml.attributes().value("name").toString();
      t.taskTypes = xml.attributes().value("tasks").toString().split('|', Qt::SkipEmptyParts);
      if (!t.name.isEmpty()) techs.push_back(t);
    } else if (xml.name() == QLatin1String("assetType")) {
      AssetType t;
      t.name      = xml.attributes().value("name").toString();
      t.taskTypes = xml.attributes().value("tasks").toString().split('|', Qt::SkipEmptyParts);
      if (!t.name.isEmpty()) atypes.push_back(t);
    } else if (xml.name() == QLatin1String("asset")) {
      Asset as;
      auto a   = xml.attributes();
      as.uuid  = a.value("uuid").toString();
      as.type  = a.value("type").toString();
      // Legacy taxonomy: "BG" folded into "Environment" (Kitsu-aligned types).
      if (as.type == QLatin1String("BG")) as.type = "Environment";
      as.name  = a.value("name").toString();
      as.kitsuAssetId = a.value("kitsuAssetId").toString();
      as.kitsuMainPack = (a.value("mainPack") == QLatin1String("1"));
      as.filePath     = a.value("file").toString();
      as.rigPsdPath   = a.value("rigPsd").toString();
      as.noFile       = a.value("noFile") == QLatin1String("1");
      as.previewSig   = a.value("previewSig").toString();
      as.importPolicy = readImportPolicy(a);
      QString tg = a.value("tags").toString();
      if (!tg.isEmpty()) as.tags = tg.split('|', Qt::SkipEmptyParts);
      assets.push_back(as);
      ai = (int)assets.size() - 1;
      psi = -1;
    } else if (xml.name() == QLatin1String("atask")) {
      if (ai >= 0 && ai < (int)assets.size()) {
        auto a       = xml.attributes();
        QString type = a.value("type").toString();
        if (!type.isEmpty()) {
          TaskState ts;
          ts.status = taskStatusFromLabel(a.value("status").toString());
          if (a.hasAttribute("synced")) {
            ts.hasSynced = true;
            ts.synced    = taskStatusFromLabel(a.value("synced").toString());
          }
          for (const QString &p : a.value("assignee").toString().split(',', Qt::SkipEmptyParts)) {
            QString t = p.trimmed();
            if (!t.isEmpty()) ts.assignees << t;
          }
          assets[ai].tasks.insert(type, ts);
        }
      }
    } else if (xml.name() == QLatin1String("storyboard")) {
      QString f = xml.attributes().value("file").toString();
      if (!f.isEmpty() && !sboards.contains(f)) sboards << f;
    } else if (xml.name() == QLatin1String("shot")) {
      auto a = xml.attributes();
      ProjectShot ps;
      ps.uuid      = a.value("uuid").toString();
      ps.source    = a.value("source").toString();
      ps.seq       = a.value("seq").toString();
      ps.label     = a.value("label").toString();
      ps.frames    = a.value("frames").toInt();
      ps.technique = a.value("technique").toString();
      ps.kitsuShotId = a.value("kitsuShotId").toString();
      ps.breakdownBaseShotId = a.value("castSynced").toString();
      ps.hasBreakdownBase    = a.hasAttribute("castSynced");
      if (!ps.uuid.isEmpty()) {
        pshots.push_back(ps);
        psi = (int)pshots.size() - 1;
        ai  = -1;
      }
    } else if (xml.name() == QLatin1String("task")) {
      // task child of a <shot> element (project shots)
      if (psi >= 0 && psi < (int)pshots.size()) {
        auto a       = xml.attributes();
        QString type = a.value("type").toString();
        if (!type.isEmpty()) {
          TaskState ts;
          ts.status = taskStatusFromLabel(a.value("status").toString());
          if (a.hasAttribute("synced")) {
            ts.hasSynced = true;
            ts.synced    = taskStatusFromLabel(a.value("synced").toString());
          }
          for (const QString &p : a.value("assignee").toString().split(',', Qt::SkipEmptyParts)) {
            QString t = p.trimmed();
            if (!t.isEmpty()) ts.assignees << t;
          }
          pshots[psi].tasks.insert(type, ts);
        }
      }
    } else if (xml.name() == QLatin1String("needs")) {
      // breakdown child of a <shot>: an asset this shot needs
      if (psi >= 0 && psi < (int)pshots.size()) {
        auto a = xml.attributes();
        BreakdownEntry be;
        be.assetUuid = a.value("asset").toString();
        be.nbOccurrences =
            a.hasAttribute("n") ? a.value("n").toInt() : 1;
        be.label = a.value("label").toString();
        if (!be.assetUuid.isEmpty()) pshots[psi].breakdown.push_back(be);
      }
    } else if (xml.name() == QLatin1String("castSynced")) {
      // the breakdown's base: what Kitsu had at the last sync
      if (psi >= 0 && psi < (int)pshots.size() &&
          pshots[psi].hasBreakdownBase) {  // only under a declared base
        auto a = xml.attributes();
        BreakdownEntry be;
        be.assetUuid = a.value("asset").toString();
        be.nbOccurrences =
            a.hasAttribute("n") ? a.value("n").toInt() : 1;
        be.label = a.value("label").toString();
        if (!be.assetUuid.isEmpty()) pshots[psi].breakdownBase.push_back(be);
      }
    }
  }
  m_team = team;  // project file is authoritative
  if (!techs.empty()) m_techniques = techs;
  // Migration: older projects were seeded without the Storyboard task. Prepend
  // it where missing so the board/animatic task exists (and can be pushed to
  // Kitsu + receive preview uploads). Persisted on the next project save.
  for (Technique &t : m_techniques)
    if (!t.taskTypes.contains(kStoryboardTask, Qt::CaseInsensitive))
      t.taskTypes.prepend(kStoryboardTask);
  // Asset types: adopt the file's list; a legacy project (no <assetTypes> block)
  // re-seeds the canonical defaults so the taxonomy is never empty.
  m_assetTypes = atypes;
  seedDefaultAssetTypes();
  m_assets    = assets;
  m_projectShots   = pshots;
  m_storyboardFiles = sboards;
  // Task names are not case-sensitive. A pull from before addAssetTaskType
  // compared names without case left «modeling»/«Modeling» and
  // «rigging»/«Rigging» on every character of CS2606 (2026-09-26), the
  // lowercase ones stuck at TODO. Merged here, in memory; the file is
  // rewritten clean on the next save.
  for (Technique &t : m_techniques)
    t.taskTypes = ZtoryTaskFlow::uniqueIgnoringCase(t.taskTypes);
  for (AssetType &t : m_assetTypes)
    t.taskTypes = ZtoryTaskFlow::uniqueIgnoringCase(t.taskTypes);
  for (Asset &a : m_assets)
    ZtoryTaskFlow::foldTaskNames(a.tasks, assetTaskTypesForType(a.type));
  for (ProjectShot &ps : m_projectShots)
    ZtoryTaskFlow::foldTaskNames(ps.tasks, taskTypesForProjectShot(ps));
  loadThumbsFromDisk();
}

// ─── B3b — Project shots ──────────────────────────────────────────────────────

QString ZtoryModel::techniqueForProjectShot(const ProjectShot &ps) const {
  return ps.technique.isEmpty() ? m_defaultTechnique : ps.technique;
}

QStringList ZtoryModel::taskTypesForProjectShot(const ProjectShot &ps) const {
  const Technique *t = findTechnique(techniqueForProjectShot(ps));
  return t ? t->taskTypes : QStringList();
}

QString ZtoryModel::firstProductionTaskType(const QString &technique) const {
  const Technique *t = findTechnique(technique);
  if (!t) return QString();
  for (const QString &tt : t->taskTypes)
    if (!isStoryboardTask(tt)) return tt;
  return QString();
}

std::vector<std::pair<int, int>> ZtoryModel::projectShotFrameRanges() const {
  std::vector<std::pair<int, int>> ranges(m_projectShots.size());
  QString curSource;
  int acc = 0;
  for (size_t i = 0; i < m_projectShots.size(); i++) {
    const ProjectShot &ps = m_projectShots[i];
    if (ps.source != curSource) { curSource = ps.source; acc = 0; }
    const int dur = ps.frames > 0 ? ps.frames : 0;
    const int in  = acc + 1;          // 1-based, like an edit timeline
    const int out = acc + dur;        // inclusive last frame
    ranges[i] = {in, out};
    acc = out;
  }
  return ranges;
}

void ZtoryModel::publishShotsToProjectDb(const QString &sourceFile) {
  if (sourceFile.isEmpty()) return;
  // Register the storyboard file.
  if (!m_storyboardFiles.contains(sourceFile))
    m_storyboardFiles << sourceFile;

  // Build uuid set of current scene shots.
  QSet<QString> sceneUuids;
  for (const ShotData &sd : m_shots)
    if (!sd.uuid.isEmpty()) sceneUuids.insert(sd.uuid);

  // Remove shots that belonged to this source but are no longer in the scene.
  m_projectShots.erase(
      std::remove_if(m_projectShots.begin(), m_projectShots.end(),
                     [&](const ProjectShot &ps) {
                       return ps.source == sourceFile &&
                              !sceneUuids.contains(ps.uuid);
                     }),
      m_projectShots.end());

  // Build quick-lookup map: uuid → index in m_projectShots.
  QHash<QString, int> byUuid;
  for (int i = 0; i < (int)m_projectShots.size(); i++)
    byUuid[m_projectShots[i].uuid] = i;

  // Find the sequence label for a shot (for the "seq" field).
  auto seqLabel = [this](const ShotData &sd) -> QString {
    for (const SequenceData &seq : m_sequences)
      if (seq.uuid == sd.sequenceId) return seq.label;
    return QString();
  };

  for (const ShotData &sd : m_shots) {
    if (sd.uuid.isEmpty()) continue;
    auto it = byUuid.find(sd.uuid);
    if (it != byUuid.end() && m_projectShots[it.value()].source == sourceFile) {
      // Existing project shot that belongs to this source: update structural metadata.
      ProjectShot &ps = m_projectShots[it.value()];
      ps.seq    = seqLabel(sd);
      ps.label  = sd.label();
      ps.frames = sd.totalDuration();
      // technique: only update if the scene has a value (project may have an override)
      if (!sd.technique.isEmpty()) ps.technique = sd.technique;
    } else if (it == byUuid.end()) {
      // Genuinely new shot: create with structure + copy initial task state from scene.
      ProjectShot ps;
      ps.uuid      = sd.uuid;
      ps.source    = sourceFile;
      ps.seq       = seqLabel(sd);
      ps.label     = sd.label();
      ps.frames    = sd.totalDuration();
      ps.technique = sd.technique;
      ps.tasks     = sd.tasks;
      m_projectShots.push_back(ps);
    }
    // else: uuid found but belongs to a different source (storyboard copied from
    // another — uuid collision). Leave the existing entry untouched; the current
    // storyboard's copy of this shot is treated as distinct and NOT published
    // (the scene .ztoryc should get a fresh uuid on next save to resolve the clash).
  }

  // Re-sort project shots: by source file order in m_storyboardFiles, then by
  // their position in the current scene for the active storyboard, or by their
  // existing position in m_projectShots for shots from other storyboards.
  QHash<QString, int> sourceOrder;
  for (int i = 0; i < m_storyboardFiles.size(); i++)
    sourceOrder[m_storyboardFiles[i]] = i;
  // sceneOrder: position in the currently open scene (uuid → index).
  QHash<QString, int> sceneOrder;
  for (int i = 0; i < (int)m_shots.size(); i++)
    if (!m_shots[i].uuid.isEmpty()) sceneOrder[m_shots[i].uuid] = i;
  // prevOrder: position in m_projectShots BEFORE this sort — used to preserve
  // the order of shots from storyboards that are not currently open.
  QHash<QString, int> prevOrder;
  for (int i = 0; i < (int)m_projectShots.size(); i++)
    prevOrder[m_projectShots[i].uuid] = i;

  std::stable_sort(m_projectShots.begin(), m_projectShots.end(),
                   [&](const ProjectShot &a, const ProjectShot &b) {
                     int sa = sourceOrder.value(a.source, 999);
                     int sb = sourceOrder.value(b.source, 999);
                     if (sa != sb) return sa < sb;
                     // Within the same source: if this is the active storyboard use
                     // the live scene order; otherwise preserve the existing DB order.
                     bool aActive = sceneOrder.contains(a.uuid);
                     bool bActive = sceneOrder.contains(b.uuid);
                     if (aActive && bActive)
                       return sceneOrder[a.uuid] < sceneOrder[b.uuid];
                     return prevOrder.value(a.uuid, 0) < prevOrder.value(b.uuid, 0);
                   });

  saveProjectDb();
  emit taskStatusChanged();
}

// Ztoryc: a hand edit in the tracker — through ZtoryTaskFlow, like the asset
// one above. writeTaskStatus mirrors it into the open scene's shot.
void ZtoryModel::setProjectShotTaskStatusByUuid(const QString &uuid,
                                                const QString &taskType,
                                                TaskStatus status) {
  if (ZtoryTaskFlow::setStatus(ZtoryTaskFlow::Entity::Shot, uuid, taskType,
                               status, ZtoryTaskFlow::Origin::User,
                               /*batch=*/true))
    emit taskStatusChanged();
}

void ZtoryModel::setProjectShotAssigneesByUuid(const QString &uuid,
                                               const QString &taskType,
                                               const QStringList &assignees) {
  for (ProjectShot &ps : m_projectShots) {
    if (ps.uuid == uuid) {
      ps.tasks[taskType].assignees = assignees;
      for (ShotData &sd : m_shots)
        if (sd.uuid == uuid) { sd.tasks[taskType].assignees = assignees; break; }
      emit taskStatusChanged();
      return;
    }
  }
}

void ZtoryModel::setProjectShotTechnique(const QString &uuid,
                                         const QString &technique) {
  for (ProjectShot &ps : m_projectShots) {
    if (ps.uuid == uuid) {
      ps.technique = technique;
      for (ShotData &sd : m_shots)
        if (sd.uuid == uuid) { sd.technique = technique; break; }
      emit taskStatusChanged();
      return;
    }
  }
}

// ─── B3d — Naming convention ──────────────────────────────────────────────────

bool ZtoryModel::autoWorkflowDetection() {
  return QSettings().value("Ztoryc/autoWorkflowDetection", true).toBool();
}

QString ZtoryModel::workflowCommand(const QString &role,
                                    const QString &technique) {
  // Il personaggio ha il SUO set di room. All'inizio e' una copia di quello del
  // cutout (la fa MainWindow::ensureCharacterRoomsFromCutout al primo
  // passaggio), ma e' una copia vera: da li' in poi si personalizza senza
  // toccare il cutout. Franco, 2026-08-16: «poi penseremo a delle room
  // ottimizzate per il rigging che potrebbero essere diverse da quelle del
  // cutout».
  if (role == "character") return MI_WorkflowCharacter;
  if (role == "shot") {
    QString t = technique.toLower().trimmed();
    if (t.contains("cut-out") || t.contains("cutout") || t.contains("cut out"))
      return MI_WorkflowCutout;
    if (t.contains("stop-motion") || t.contains("stopmotion") ||
        t.contains("stop motion"))
      return MI_WorkflowStopMotion;
    return MI_Workflow2D;  // Tradigital (also Traditional/3D/Generic/Live)
  }
  return MI_WorkflowStoryboard;  // storyboard (and default/legacy)
}

void ZtoryModel::setAutoWorkflowDetection(bool on) {
  QSettings().setValue("Ztoryc/autoWorkflowDetection", on);
}

QString ZtoryModel::taskShortCode(const QString &taskType) {
  // NABA-aligned short codes. Unknown types use the first 3-4 letters uppercased.
  static const QHash<QString, QString> codes = {
    { "Layout",          "LAY"  },
    { "Key Animation",   "KAN"  },
    { "Animation",       "ANIM" },
    { "Inbetweening",    "INB"  },
    { "Clean up",        "CU"   },
    { "Scan & Clean",    "SCN"  },
    { "Ink & Paint",     "INK"  },
    { "X-Sheet",         "XSH"  },
    { "Lighting",        "LGT"  },
    { "Rig Removal",     "RIG"  },
    { "Shooting",        "SHT"  },
    { "Editing",         "EDT"  },
    { "VFX",             "VFX"  },
    { "Render",          "RND"  },
    { "Compositing",     "COMP" },
    { "Set-up",          "SET"  },
    { "Rough",           "RGH"  },
    { "Storyboard",      "STB"  },
    { "Animatic",        "AMC"  },
  };
  auto it = codes.constFind(taskType);
  if (it != codes.constEnd()) return it.value();
  // Fallback: first 4 chars uppercase, spaces stripped.
  return taskType.toUpper().remove(' ').left(4);
}

//! The pattern used when the project has not set one of its own.
//!
//! {CODE} and not {PROD}: production naming uses the short code -- MGZ_, not
//! MaggiolataZombie_ -- because the full name makes file names unwieldy the
//! moment they are joined into a path. {PROD} is kept as the fallback for a
//! project whose code was never filled in: a long name beats a missing one.
//! The short code to use in names: the one the user set, or one derived from
//! the production name when the field was never filled in.
//!
//! Derived rather than left empty because {CODE} is now the head of the default
//! naming pattern: an empty code would have silently fallen back to the full
//! production name, which is the very thing the code exists to avoid.
//! CamelCase gives its capitals (MaggiolataZombie -> MZ); otherwise the first
//! three letters. Nothing is written to the project: type your own and that
//! wins.
QString ZtoryModel::effectiveCode() const {
  const QString set = m_code.trimmed();
  if (!set.isEmpty()) return set;

  const QString prod = m_production.trimmed();
  if (prod.isEmpty()) return QString();

  QString caps;
  for (const QChar &c : prod)
    if (c.isUpper()) caps += c;
  if (caps.size() >= 2) return caps.left(4);

  return prod.left(3).toUpper();
}

QString ZtoryModel::effectiveEpisodeNumber() const {
  if (!m_episodeNumber.trimmed().isEmpty()) return m_episodeNumber.trimmed();
  return derivedEpisodeNumber();
}

QString ZtoryModel::derivedEpisodeNumber() const {
  const QString code = effectiveCode();
  if (code.isEmpty() || !m_episode.startsWith(code, Qt::CaseInsensitive))
    return QString();
  const QRegularExpressionMatch d =
      QRegularExpression("^(\\d+)").match(m_episode.mid(code.size()));
  return d.hasMatch() ? d.captured(1) : QString();
}

// The tokens of the asset files' convention that come from the project.
static QMap<QString, QString> projectTokens(const ZtoryModel *m) {
  QMap<QString, QString> tok;
  tok["PROD"]   = m->production();
  tok["CODE"]   = m->effectiveCode();
  tok["SEASON"] = m->season();
  tok["EP"]     = m->episode();
  tok["EPNUM"]  = m->effectiveEpisodeNumber();
  return tok;
}

// «Bacchetta Magica» → «bacchetta-magica»: the asset's name as it sits in the
// file names (Franco, 2026-09-27: lower case, words joined by a dash).
static QString fileNamePart(const QString &name) {
  QStringList words;
  QString w;
  for (const QChar &c : name.normalized(QString::NormalizationForm_C)) {
    if (c.isLetterOrNumber()) w += c.toLower();
    else if (!w.isEmpty()) { words << w; w.clear(); }
  }
  if (!w.isEmpty()) words << w;
  return words.join('-');
}

QString ZtoryModel::assetFileName(const Asset &a, int version,
                                  const QString &suffix) const {
  QMap<QString, QString> tok = projectTokens(this);
  tok["TYPE"] = assetTypeFileCode(a.type);
  tok["NAME"] = fileNamePart(a.name);
  tok["VER"]  = QString::number(version);
  const QString pat = m_assetFilePattern.trimmed().isEmpty()
                          ? defaultAssetFilePattern()
                          : m_assetFilePattern.trimmed();
  const QString base = resolvePattern(pat, tok);
  return suffix.isEmpty() ? base : base + "." + suffix;
}

QRegularExpression ZtoryModel::assetFileRegex(const Asset &a) const {
  const QString pat = m_assetFilePattern.trimmed().isEmpty()
                          ? defaultAssetFilePattern()
                          : m_assetFilePattern.trimmed();
  // One {NAME}, at most one {VER}: repeated named groups make no valid
  // expression, and a convention without a name recognises nothing.
  if (pat.count("{NAME}") != 1 || pat.count(QRegularExpression("\\{VER(:\\d+)?\\}")) > 1)
    return QRegularExpression();
  // The pattern resolved with markers where the name and the version go, then
  // turned into an anchored expression: one grammar for making names and for
  // reading them.
  QMap<QString, QString> tok = projectTokens(this);
  tok["TYPE"] = assetTypeFileCode(a.type);
  tok["NAME"] = "QQZNAMEQQZ";
  tok["VER"]  = "QQZVERQQZ";
  QString rx = QRegularExpression::escape(resolvePattern(pat, tok));
  rx.replace("QQZNAMEQQZ", "(?<name>.+?)");
  rx.replace("QQZVERQQZ", "(?<ver>\\d+)");
  return QRegularExpression("^" + rx + "$",
                            QRegularExpression::CaseInsensitiveOption);
}

bool ZtoryModel::matchAssetFileName(const QRegularExpression &re,
                                    const QString &baseName, QString *namePart,
                                    int *version) {
  if (!re.isValid() || re.pattern().isEmpty()) return false;
  const QRegularExpressionMatch mt = re.match(baseName);
  if (!mt.hasMatch()) return false;
  *namePart = mt.captured("name");
  *version  = mt.captured("ver").toInt();
  return true;
}

bool ZtoryModel::parseAssetFileName(const QString &baseName, const Asset &a,
                                    QString *namePart, int *version) const {
  return matchAssetFileName(assetFileRegex(a), baseName, namePart, version);
}

QString ZtoryModel::defaultNamingPattern() const {
  const QString head = effectiveCode().isEmpty() ? "{PROD}" : "{CODE}";
  return head + "_{SEASON}_{EP}_{SEQ}_{SHOT}_{TASK}_V{VER:02}";
}

QString ZtoryModel::resolveNamingPattern(const QMap<QString,QString> &tokens) const {
  QString pat = m_namingPattern;
  if (pat.isEmpty()) pat = defaultNamingPattern();
  return resolvePattern(pat, tokens);
}

QString ZtoryModel::resolvePattern(const QString &pattern,
                                   const QMap<QString,QString> &tokens) {
  QString result = pattern;
  // Replace {TOKEN} and {TOKEN:FORMAT} (format = zero-padding width).
  static const QRegularExpression re(R"(\{(\w+)(?::(\d+))?\})");
  // Collect all matches first (process right-to-left to preserve indices).
  QList<QRegularExpressionMatch> matches;
  QRegularExpressionMatchIterator it = re.globalMatch(result);
  while (it.hasNext()) matches.prepend(it.next());
  for (const QRegularExpressionMatch &m : matches) {
    QString key = m.captured(1);
    QString fmt = m.captured(2);  // digits only (the width)
    QString val = tokens.value(key, "");
    if (!fmt.isEmpty() && !val.isEmpty()) {
      bool ok;
      int n = val.toInt(&ok);
      if (ok) val = QString("%1").arg(n, fmt.toInt(), 10, QChar('0'));
    }
    result.replace(m.capturedStart(), m.capturedLength(), val);
  }
  // Sanitize: replace spaces with _, strip characters invalid in filenames.
  result.replace(' ', '_');
  result.remove(QRegularExpression(R"([\\/:*?"<>|])"));

  // An empty field leaves nothing behind, separators included. Without this an
  // unset season turned "{CODE}_{SEASON}_{EP}" into "MZ__EP01" -- the gap shows
  // where a field ISN'T, which is the opposite of what leaving it blank means.
  // Runs of the same separator collapse to one, and any left at the ends go.
  result.replace(QRegularExpression(R"(_{2,})"), "_");
  result.replace(QRegularExpression(R"(-{2,})"), "-");
  result.replace(QRegularExpression(R"(\.{2,})"), ".");
  result.remove(QRegularExpression(R"(^[_\-.]+|[_\-.]+$)"));
  return result;
}

const QStringList &ZtoryModel::canonicalTaskOrder() {
  // Master column order: union of all known task types, stable across exports.
  static const QStringList order = {
    "Storyboard", "Set-up", "Layout", "Key Animation", "Animation",
    "Inbetweening", "Clean up", "Scan & Clean", "Ink & Paint", "X-Sheet",
    "Lighting", "Rig Removal", "Shooting", "Editing", "VFX", "Render",
    "Compositing",
  };
  return order;
}

QStringList ZtoryModel::spreadsheetTaskColumns() const {
  // Collect every task type used by any shot's technique.
  std::set<QString> used;
  for (int si = 0; si < (int)m_shots.size(); si++)
    for (const QString &tt : taskTypesForShot(si)) used.insert(tt);
  // Also cover the project-level shots: the Production Tracker shows those in
  // project mode, and the open scene's m_shots may be empty (e.g. while a shot
  // scene is current) — without this the task columns vanish.
  for (const ProjectShot &ps : m_projectShots)
    for (const QString &tt : taskTypesForProjectShot(ps)) used.insert(tt);
  // Order the columns by the WORKFLOWS' own pipeline order — the sequence the
  // user set in the Workflows tab — NOT a fixed canonical list. Reordering a
  // workflow's tasks must reflect immediately in the shot matrix. Walk each
  // technique in order and append its task types as first seen.
  // Only the workflows the shots ACTUALLY use, in project order. Walking every
  // technique instead let a workflow nobody uses dictate the order for one that
  // is used: with shots on Cut-out, Tradigital (listed first) placed Storyboard,
  // Layout, VFX, Render and Compositing, and "Animation" -- which only Cut-out
  // names -- was appended after all of them, landing last. Reordering it inside
  // its own workflow could not help, because its position was decided by WHICH
  // workflow mentioned it first, not by where it sits within one.
  std::set<QString> usedTechs;
  for (int si = 0; si < (int)m_shots.size(); si++)
    usedTechs.insert(techniqueForShot(si));
  for (const ProjectShot &ps : m_projectShots) usedTechs.insert(ps.technique);

  QStringList cols;
  for (const Technique &t : m_techniques) {
    if (!usedTechs.count(t.name)) continue;
    for (const QString &tt : t.taskTypes)
      if (used.count(tt)) { cols << tt; used.erase(tt); }
  }
  // Then the unused workflows, for any task type they alone own.
  for (const Technique &t : m_techniques)
    for (const QString &tt : t.taskTypes)
      if (used.count(tt)) { cols << tt; used.erase(tt); }
  // Fallbacks for any used type not owned by a technique: canonical order
  // first, then whatever is left — keeps custom/legacy types visible.
  for (const QString &tt : canonicalTaskOrder())
    if (used.count(tt)) { cols << tt; used.erase(tt); }
  for (const QString &tt : used) cols << tt;
  return cols;
}

QString ZtoryModel::taskStatusLabel(TaskStatus s) {
  switch (s) {
  case TaskStatus::Ready:  return "READY";
  case TaskStatus::Wip:    return "WIP";
  case TaskStatus::Wfa:    return "WFA";
  case TaskStatus::Retake: return "RETAKE";
  case TaskStatus::Done:   return "DONE";
  case TaskStatus::Todo:
  default:                 return "TODO";
  }
}

TaskStatus ZtoryModel::taskStatusFromLabel(const QString &s) {
  const QString u = s.trimmed().toUpper();
  if (u == "READY")  return TaskStatus::Ready;
  if (u == "WIP")    return TaskStatus::Wip;
  if (u == "WFA")    return TaskStatus::Wfa;
  if (u == "RETAKE") return TaskStatus::Retake;
  if (u == "DONE")   return TaskStatus::Done;
  return TaskStatus::Todo;
}

// ─── Sequences ────────────────────────────────────────────────────────────────

SequenceData* ZtoryModel::findSequence(const QString &uuid) {
  for (auto &seq : m_sequences)
    if (seq.uuid == uuid) return &seq;
  return nullptr;
}

int ZtoryModel::projectShotCountFromSource(const QString &sourceFile) const {
  int n = 0;
  for (const ProjectShot &ps : m_projectShots)
    if (ps.source.compare(sourceFile, Qt::CaseInsensitive) == 0) n++;
  return n;
}

int ZtoryModel::removeProjectShotsFromSource(const QString &sourceFile) {
  if (sourceFile.isEmpty()) return 0;
  const int before = (int)m_projectShots.size();
  m_projectShots.erase(
      std::remove_if(m_projectShots.begin(), m_projectShots.end(),
                     [&](const ProjectShot &ps) {
                       return ps.source.compare(sourceFile,
                                                Qt::CaseInsensitive) == 0;
                     }),
      m_projectShots.end());
  for (int i = m_storyboardFiles.size() - 1; i >= 0; i--)
    if (m_storyboardFiles[i].compare(sourceFile, Qt::CaseInsensitive) == 0)
      m_storyboardFiles.remove(i);
  return before - (int)m_projectShots.size();
}

QStringList ZtoryModel::collidingShotLabels(const QString &sourceFile) const {
  // ⚠️ Solo gli shot che vengono da scene che sono ANCORA storyboard.
  // Una scena diventata personaggio (o shot) lascia dietro di se' gli shot che
  // aveva pubblicato: sono residui, non un conflitto, e segnalarli vorrebbe
  // dire chiedere una sequenza per distinguersi da qualcosa che non esiste
  // piu'. Successo il 2026-08-17, con il popup che tornava a ogni salvataggio
  // per una scena marcata CH.
  QHash<QString, bool> isStoryboard;  // sorgente -> ancora storyboard?
  const QString projDir = QFileInfo(projectDbPath()).absolutePath();
  auto sourceIsStoryboard = [&](const QString &src) {
    auto it = isStoryboard.constFind(src);
    if (it != isStoryboard.constEnd()) return it.value();
    bool ok = true;  // senza prove del contrario, e' uno storyboard
    if (!projDir.isEmpty() && !src.isEmpty()) {
      const QString sidecar = projDir + "/scenes/" + src;
      if (QFile::exists(sidecar)) {
        const QString role = ZtoryCharacter::roleOf(sidecar);
        ok = role.isEmpty() || role == QLatin1String("storyboard");
      }
    }
    isStoryboard.insert(src, ok);
    return ok;
  };

  // Le chiavi degli shot che nel progetto vengono da un ALTRO storyboard.
  QSet<QString> others;
  for (const ProjectShot &ps : m_projectShots)
    if (ps.source != sourceFile && sourceIsStoryboard(ps.source))
      others.insert(
          (ps.seq.trimmed() + "\n" + ps.label.trimmed()).toLower());
  if (others.isEmpty()) return QStringList();

  QStringList out;
  for (const ShotData &sd : m_shots) {
    QString seq;
    for (const SequenceData &s : m_sequences)
      if (s.uuid == sd.sequenceId) { seq = s.label; break; }
    const QString key =
        (seq.trimmed() + "\n" + sd.label().trimmed()).toLower();
    if (!others.contains(key)) continue;
    const QString shown =
        seq.trimmed().isEmpty() ? sd.label() : (seq + " " + sd.label());
    if (!out.contains(shown)) out << shown;
  }
  return out;
}

QString ZtoryModel::proposeFreeSequenceLabel() const {
  // Il prefisso e il numero di cifre si prendono da cio' che il progetto usa
  // gia': proporre "SQ040" in un progetto che scrive "SEQ04" sarebbe una terza
  // convenzione inventata da noi.
  // Il progetto decide: prefisso e padding vengono dalla configurazione di
  // numerazione, che si imposta sulla PRIMA scena di storyboard e da li' in poi
  // vale per tutte. Il padding e' anche cio' che detta il passo, perche' e' la
  // stessa convenzione vista da due lati: a tre cifre si scrive 010, 020, 030
  // lasciando posto in mezzo; a due cifre si scrive 01, 02, 03.
  QString prefix = m_numberingConfig.seqPrefix.isEmpty()
                       ? QString("SQ")
                       : m_numberingConfig.seqPrefix;
  int digits     = qBound(1, m_numberingConfig.seqPadding, 6);
  // Il salto e' "step", che nella configurazione e' proprio il campo che dice
  // di quanto avanza la numerazione. Il padding e' un'altra cosa: quante cifre
  // scrivere. Erano due cose diverse e le avevo confuse, deducendo il passo dal
  // padding — che per SQ010/SQ020 dava il risultato giusto per il motivo
  // sbagliato, e in un progetto a passo 5 avrebbe sbagliato e basta.
  int configStep = qMax(1, m_numberingConfig.step);
  int maxN       = 0;
  QSet<QString> used;
  auto consider = [&](const QString &label) {
    const QString l = label.trimmed();
    if (l.isEmpty()) return;
    used.insert(l.toLower());
    int i = 0;
    while (i < l.size() && !l[i].isDigit()) i++;
    if (i == 0 || i >= l.size()) return;
    prefix = l.left(i);
    const QString num = l.mid(i);
    digits = num.size();
    bool ok = false;
    const int n = num.toInt(&ok);
    if (ok) maxN = std::max(maxN, n);
  };
  // Tutto il progetto, non solo questo storyboard: le sequenze della scena
  // aperta E quelle degli shot gia' pubblicati nel tracker da qualunque altro
  // storyboard. E' il caso per cui la sequenza esiste — piu' storyboard
  // collegati allo stesso progetto, uno per sequenza su un film lungo.
  for (const SequenceData &s : m_sequences) consider(s.label);
  for (const ProjectShot &ps : m_projectShots) consider(ps.seq);

  // Il passo e' quello della configurazione, punto: e' il campo fatto per dirlo,
  // ed e' lo stesso che governa la numerazione degli shot. Prima era fisso a
  // dieci, e in un progetto numerato 01, 02, 03 proponeva 10 saltando da 04 a 09.
  const int step = configStep;
  int n          = maxN + step;
  if (n <= 0) n = step;
  for (int guard = 0; guard < 1000; guard++) {
    const QString cand =
        prefix + QString("%1").arg(n, digits, 10, QChar('0'));
    if (!used.contains(cand.toLower())) return cand;
    n += step;
  }
  return prefix + "999";
}

SequenceData* ZtoryModel::findOrCreateSequence(const QString &label) {
  if (label.isEmpty()) return nullptr;
  // Case-insensitive lookup by label
  for (auto &seq : m_sequences)
    if (seq.label.compare(label, Qt::CaseInsensitive) == 0) return &seq;
  // Not found — create a new sequence
  SequenceData seq;
  seq.uuid  = QUuid::createUuid().toString(QUuid::WithoutBraces);
  seq.label = label;
  // Derive orderIndex from the numeric part of the label
  QString numPart = label;
  while (!numPart.isEmpty() && numPart[0].isLetter()) numPart.remove(0, 1);
  bool ok;
  int n = numPart.toInt(&ok);
  seq.orderIndex = ok ? n : (int)m_sequences.size() + 1;
  m_sequences.push_back(seq);
  return &m_sequences.back();
}

void ZtoryModel::ensureDefaultSequence() {
  if (!m_sequences.empty()) return;
  SequenceData seq;
  seq.uuid  = QUuid::createUuid().toString(QUuid::WithoutBraces);
  seq.label = m_numberingConfig.seqPrefix +
              ZtoryNumbering::formatN(m_numberingConfig.startNumber,
                                     m_numberingConfig.seqPadding);
  seq.orderIndex = m_numberingConfig.startNumber;
  m_sequences.push_back(seq);
}

// ─── Labelling ────────────────────────────────────────────────────────────────

// Static implementation — works on any vector<ShotData>.
// Called by generateShotLabel() and by StoryboardPanel via projected vector.
void ZtoryModel::assignShotLabel(std::vector<ShotData> &shots, int si,
                                  const NumberingConfig &cfg) {
  if (si < 0 || si >= (int)shots.size()) return;
  ShotData &s    = shots[si];
  const QString pfx   = cfg.shotPrefix;
  const int     pad   = cfg.padding;
  const int     step  = cfg.step;
  const int     scale = 100;  // orderIndex = labelNumber * scale

  // Collect existing labels, excluding this shot's own current label
  QStringList existing = ZtoryNumbering::allLabels(shots);
  existing.removeAll(s.shotLabel);

  // Resolve effective orderIndex for a neighbour (fallback: position-based)
  auto effectiveOrder = [&](int idx) -> int {
    int o = shots[idx].orderIndex;
    return (o > 0) ? o : (idx + 1) * step * scale;
  };

  const bool hasPrev = (si > 0);
  const bool hasNext = (si + 1 < (int)shots.size());
  int prevOrder = hasPrev ? effectiveOrder(si - 1) : 0;
  int nextOrder = hasNext ? effectiveOrder(si + 1) : 0;

  if (!hasPrev && !hasNext) {
    // Only shot in the project
    int num = cfg.startNumber;
    s.orderIndex = num * scale;
    s.shotLabel  = pfx + ZtoryNumbering::formatN(num, pad);

  } else if (!hasPrev) {
    // Inserting at the very beginning
    int nextNum = ZtoryNumbering::labelNum(shots[si + 1].label(), pfx);
    if (nextNum <= 0) nextNum = nextOrder / scale;
    int num = qMax(1, nextNum - step);
    QString cand = pfx + ZtoryNumbering::formatN(num, pad);
    if (existing.contains(cand)) {
      QString base = ZtoryNumbering::stripSuffix(cand, pfx);
      s.shotLabel = base + ZtoryNumbering::nextSuffix(existing, base);
    } else {
      s.shotLabel = cand;
    }
    s.orderIndex = nextOrder / 2;

  } else if (!hasNext) {
    // Appending at the end
    int prevNum = ZtoryNumbering::labelNum(shots[si - 1].label(), pfx);
    if (prevNum <= 0) prevNum = prevOrder / scale;
    int num = prevNum + step;
    QString cand = pfx + ZtoryNumbering::formatN(num, pad);
    while (existing.contains(cand)) {
      num += step;
      cand = pfx + ZtoryNumbering::formatN(num, pad);
    }
    s.shotLabel  = cand;
    s.orderIndex = prevOrder + step * scale;

  } else {
    // Inserting between two existing shots
    int prevNum = ZtoryNumbering::labelNum(shots[si - 1].label(), pfx);
    int nextNum = ZtoryNumbering::labelNum(shots[si + 1].label(), pfx);
    if (prevNum <= 0) prevNum = prevOrder / scale;
    if (nextNum <= 0) nextNum = nextOrder / scale;
    int midOrder = (prevOrder + nextOrder) / 2;

    // Prefer midpoint; scan for any free integer in (prevNum, nextNum)
    int midNum = (prevNum + nextNum) / 2;
    int found  = -1;
    if (midNum > prevNum && midNum < nextNum) {
      if (!existing.contains(pfx + ZtoryNumbering::formatN(midNum, pad)))
        found = midNum;
    }
    if (found < 0) {
      for (int n = prevNum + 1; n < nextNum && found < 0; n++) {
        if (!existing.contains(pfx + ZtoryNumbering::formatN(n, pad))) found = n;
      }
    }
    if (found >= 0) {
      s.shotLabel  = pfx + ZtoryNumbering::formatN(found, pad);
      s.orderIndex = midOrder;
    } else {
      // No integer space: alphabetical suffix on the previous label
      QString base = ZtoryNumbering::stripSuffix(shots[si - 1].label(), pfx);
      s.shotLabel  = base + ZtoryNumbering::nextSuffix(existing, base);
      s.orderIndex = midOrder;
    }
  }

  // Keep legacy shotNumber in sync for backward compat
  s.shotNumber = s.shotLabel;
}

void ZtoryModel::generateShotLabel(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  std::vector<ShotData> plain = m_shots.copy();
  assignShotLabel(plain, si, m_numberingConfig);
  m_shots[si].shotLabel  = plain[si].shotLabel;
  m_shots[si].shotNumber = plain[si].shotNumber;
  m_shots[si].orderIndex = plain[si].orderIndex;
}

void ZtoryModel::cleanRenumber() {
  const NumberingConfig &cfg   = m_numberingConfig;
  const QString          pfx   = cfg.shotPrefix;
  const int              pad   = cfg.padding;
  const int              step  = cfg.step;
  const int              scale = 100;

  for (int i = 0; i < (int)m_shots.size(); i++) {
    int num = cfg.startNumber + i * step;
    m_shots[i].shotLabel  = pfx + ZtoryNumbering::formatN(num, pad);
    m_shots[i].shotNumber = m_shots[i].shotLabel;
    m_shots[i].orderIndex = num * scale;
    updateColumnName(i);
  }
}

void ZtoryModel::generatePanelLabels(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  const QString &pfx = m_numberingConfig.panelPrefix;
  auto &panels = m_shots[si].panels;
  for (int pi = 0; pi < (int)panels.size(); pi++) {
    panels[pi].panelLabel = pfx + ZtoryNumbering::formatN(pi + 1, 3);
    panels[pi].orderIndex = pi;
  }
}

QString ZtoryModel::fullLabel(int si) const {
  if (si < 0 || si >= (int)m_shots.size()) return QString();
  const ShotData &s = m_shots[si];
  if (s.sequenceId.isEmpty()) return s.label();
  for (const auto &seq : m_sequences)
    if (seq.uuid == s.sequenceId) return seq.label + "_" + s.label();
  return s.label();
}

ZtoryModel *ZtoryModel::instance() {
  static ZtoryModel inst;
  return &inst;
}

// ─── Preview ──────────────────────────────────────────────────────────────────

QPixmap ZtoryModel::preview(int si, int pi) const {
  if (si < 0 || si >= (int)m_previews.size()) return QPixmap();
  if (pi < 0 || pi >= (int)m_previews[si].size()) return QPixmap();
  return m_previews[si][pi];
}

void ZtoryModel::updatePreview(int si, int pi) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  const ShotData &s = m_shots[si];
  if (pi < 0 || pi >= (int)s.panels.size()) return;

  while ((int)m_previews.size() <= si)
    m_previews.push_back({});
  while ((int)m_previews[si].size() <= pi)
    m_previews[si].push_back(QPixmap());

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getXsheet();
  if (!xsh) return;

  int col = s.xsheetColumn;
  TXshCell cell = xsh->getCell(s.panels[pi].startFrame, col);
  if (cell.isEmpty()) { emit previewUpdated(si, pi); return; }

  TXshSimpleLevel *sl = cell.getSimpleLevel();
  if (!sl) { emit previewUpdated(si, pi); return; }

  QPixmap px = IconGenerator::instance()->getIcon(sl, cell.getFrameId());
  if (!px.isNull()) {
    m_previews[si][pi] = px;
    emit previewUpdated(si, pi);
  }
}

void ZtoryModel::updateAllPreviews() {
  for (int si = 0; si < (int)m_shots.size(); si++)
    for (int pi = 0; pi < (int)m_shots[si].panels.size(); pi++)
      updatePreview(si, pi);
}

// ─── Operazioni su shot ───────────────────────────────────────────────────────

void ZtoryModel::setWorkflow(ZtoryWorkflow w) {
  if (m_workflow == w) return;
  m_workflow = w;
  emit workflowChanged(w);
}

bool ZtoryModel::assertMainXsheet(bool showWarning) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return false;
  if (scene->getChildStack()->getAncestorCount() == 0) return true;
  if (showWarning)
    QMessageBox::warning(nullptr, QObject::tr("Ztoryc"),
        QObject::tr("This operation is only available at the main xsheet level.\n"
                    "Please close the current sub-scene first (double-click outside)."));
  return false;
}

void ZtoryModel::addShot(int insertAt) {
  if (!assertMainXsheet(true)) return;
  ShotData s;
  PanelData pd;
  s.panels.push_back(pd);
  if (insertAt < 0 || insertAt >= (int)m_shots.size()) {
    m_shots.push_back(s);
    m_previews.push_back({QPixmap()});
    generateShotLabel((int)m_shots.size() - 1);
    emit shotAdded((int)m_shots.size() - 1);
  } else {
    m_shots.insertAt(insertAt, s);
    m_previews.insert(m_previews.begin() + insertAt, {QPixmap()});
    generateShotLabel(insertAt);
    emit shotAdded(insertAt);
  }
}

void ZtoryModel::addShotNamed(const QString &name) {
  // Creates a fully-wired shot: xsheet column + sub-scene + model entry.
  // Used by ZtoryStartupDialog to pre-populate new projects.
  if (!assertMainXsheet(false)) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  if (!scene || !xsh) return;

  static const int kDefaultDuration = 24;
  int col = xsh->getColumnCount();  // append at end

  // Create a new sub-scene (child level)
  TXshLevel *xl = scene->createNewLevel(CHILD_XSHLEVEL);
  if (!xl || !xl->getChildLevel()) return;
  TXshChildLevel *cl = xl->getChildLevel();

  // Native invariant: every sub-scene shares the main xsheet's camera framing
  // (res + size).  Startup-created shots went through createNewLevel() without
  // this sync, so they could inherit a default camera ≠ the scene camera set
  // in Preferences. (Other creation paths already sync — see onAddShot.)
  ZtoryShotOps::syncChildCameraToMain(xsh, cl);

  xsh->insertColumn(col);
  for (int r = 0; r < kDefaultDuration; r++)
    xsh->setCell(r, col, TXshCell(cl, TFrameId(r + 1)));
  xsh->updateFrameCount();

  // Build model entry
  ShotData s;
  s.xsheetColumn = col;
  s.shotNumber   = name;
  s.shotLabel    = name;  // keep shotLabel in sync (primary display field)
  PanelData pd;
  pd.duration = kDefaultDuration;
  s.panels.push_back(pd);
  m_shots.push_back(s);
  m_previews.push_back({QPixmap()});
  recordShotIdentity((int)m_shots.size() - 1);

  app->getCurrentXsheet()->notifyXsheetChanged();
  resequenceXsheet();
  emit modelReset();
}

void ZtoryModel::addShotFromRasters(const QString &name,
                                    const std::vector<TRaster32P> &panels) {
  if (panels.empty()) return;
  // Inside a sub-scene this used to return in SILENCE: no shot, no message, no
  // reason, so the command looked like it worked sometimes and not others
  // depending on state the user cannot see.  But asking them to close the shot
  // first would only be a politer way of making them do our work: we know where
  // the shot has to go, so come back out and put it there — the same thing Add
  // Shot and the undo restore already do.
  {
    ToonzScene *scn = TApp::instance()->getCurrentScene()->getScene();
    while (scn && scn->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  }
  if (!assertMainXsheet(true)) return;  // still not there: then it is worth saying
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = app->getCurrentXsheet()->getXsheet();
  if (!scene || !xsh) return;

  // Each panel becomes one drawing held for this many frames in the sub-scene,
  // so the shot has a usable length in the animatic (re-timable afterwards).
  // Matches the single-panel default used by addShotNamed.
  static const int kPanelHoldFrames = 24;
  const int n                       = (int)panels.size();
  // Level resolution = the incoming panel rasters (already framed + shrunk by
  // the caller); all panels share the same size.  Fall back to the camera res.
  const TDimension res =
      panels[0] ? panels[0]->getSize() : ZtoryShotOps::cameraRes(scene);

  // Camera dpi: making the level dpi match the camera makes the res-sized image
  // fill the frame exactly (image inches == camera size inches).
  double dpi = Stage::standardDpi;
  {
    TStageObjectTree *tree = xsh->getStageObjectTree();
    TStageObject *camObj =
        tree->getStageObject(tree->getCurrentCameraId(), false);
    TCamera *cam = camObj ? camObj->getCamera() : nullptr;
    if (cam && cam->getDpi().x > 0) dpi = cam->getDpi().x;
  }

  // 0) Model entry first (panels metadata only) so we can derive the shot label
  //    now and name the OVL level after it (e.g. "SH040").
  ShotData s;
  for (int i = 0; i < n; i++) {
    PanelData pd;
    pd.startFrame = i * kPanelHoldFrames;
    pd.duration   = kPanelHoldFrames;
    s.panels.push_back(pd);
  }
  m_shots.push_back(s);
  m_previews.push_back(std::vector<QPixmap>(n));
  const int si = (int)m_shots.size() - 1;
  if (name.isEmpty())
    generateShotLabel(si);  // appended at end → next number (SH010, SH020, …)
  else
    m_shots[si].shotLabel = m_shots[si].shotNumber = name;
  auto rollback = [&]() { m_shots.pop_back(); m_previews.pop_back(); };

  // 1) OVL raster level, one frame per panel, named after the shot it becomes,
  //    so its drawings land in extras/<scene>/SH040.000N.png.  The name must NOT
  //    already exist on disk: createNewLevel's own disambiguation appends
  //    "_1", "_2"… but Tahoma reads "_<digits>" as a frame separator, so
  //    "SH040_1" collapses back to level "SH040"; if "SH040" drawings already
  //    exist (e.g. a previous export) that check never finds a free name and
  //    loops forever — the export hang.  We disambiguate ourselves with a
  //    trailing LETTER (never a frame separator) and hand createNewLevel a
  //    guaranteed-free name, so its loop exits on the first try.
  QString baseLabel = m_shots[si].shotLabel;
  if (baseLabel.isEmpty()) baseLabel = "thumb";
  std::wstring levelName = baseLabel.toStdWString();
  {
    // A name is unusable if it is already loaded in the scene's level set (a
    // level can exist in RAM without being on disk yet) or its default path
    // exists on disk.
    auto nameTaken = [&](const std::wstring &nm) {
      if (scene->getLevelSet() && scene->getLevelSet()->getLevel(nm))
        return true;
      return TSystem::doesExistFileOrLevel(
          scene->decodeFilePath(scene->getDefaultLevelPath(OVL_XSHLEVEL, nm)));
    };
    int guard = 0;
    while (nameTaken(levelName) && guard < 25)
      levelName =
          baseLabel.toStdWString() + std::wstring(1, (wchar_t)(L'B' + guard++));
    // Handing createNewLevel a taken name is the hang described above: its own
    // "_N" disambiguation collapses back to the same level name and never
    // terminates.  Give up cleanly instead of freezing the application.
    if (nameTaken(levelName)) {
      DVGui::warning(
          QObject::tr("Could not find a free name for the shot's drawing level "
                      "(tried \"%1\" and 25 variants).\nRename or remove the "
                      "existing levels with that name and try again.")
              .arg(baseLabel));
      return rollback();
    }
  }

  TXshLevel *rl =
      scene->createNewLevel(OVL_XSHLEVEL, levelName, res, dpi, TFilePath());
  if (!rl) return rollback();
  TXshSimpleLevel *sl = rl->getSimpleLevel();
  if (!sl) return rollback();
  sl->setPath(scene->getDefaultLevelPath(OVL_XSHLEVEL, sl->getName()), true);
  sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
  sl->getProperties()->setDpi(dpi);
  sl->getProperties()->setImageDpi(TPointD(dpi, dpi));
  sl->getProperties()->setImageRes(res);
  for (int i = 0; i < n; i++) {
    if (!panels[i]) continue;
    TRasterImageP ri(panels[i]);
    ri->setDpi(dpi, dpi);
    sl->setFrame(TFrameId(i + 1), ri);
  }
  // No inline sl->save(): the frames live in RAM and are persisted with the
  // scene at the next save, like any freshly painted level (addShotNamed never
  // saves its sub-scene either).  Blocking disk I/O here was an earlier hang.

  // 2) Sub-scene exposing the drawings as a held sequence (panel i → its hold of
  //    rows), so the Board's detectAndUpdatePanels sees N evenly-sized panels.
  TXshLevel *xl = scene->createNewLevel(CHILD_XSHLEVEL);
  if (!xl || !xl->getChildLevel()) return rollback();
  TXshChildLevel *cl = xl->getChildLevel();
  ZtoryShotOps::syncChildCameraToMain(xsh, cl);
  TXsheet *childXsh = cl->getXsheet();
  for (int i = 0; i < n; i++)
    for (int h = 0; h < kPanelHoldFrames; h++)
      childXsh->setCell(i * kPanelHoldFrames + h, 0,
                        TXshCell(sl, TFrameId(i + 1)));
  childXsh->updateFrameCount();

  // 3) Main-xsheet column exposing the sub-scene 1:1 (row r → sub frame r+1).
  const int duration = n * kPanelHoldFrames;
  const int col      = xsh->getColumnCount();  // append at end
  xsh->insertColumn(col);
  for (int r = 0; r < duration; r++)
    xsh->setCell(r, col, TXshCell(cl, TFrameId(r + 1)));
  xsh->updateFrameCount();

  // 4) Finalise the model entry now that the column exists.  xsheetColumn is
  //    critical: refreshPreview() uses it to render the sub-scene thumbnail.
  m_shots[si].xsheetColumn = col;
  recordShotIdentity(si);
  // Name the column after the shot, like every other shot-creating path: the
  // Board's reorder detection compares this name against the shot label, and an
  // unnamed column carries no ordering information.
  updateColumnName(si);

  app->getCurrentXsheet()->notifyXsheetChanged();
  resequenceXsheet();
  emit modelReset();
}

// ─── Numerazione ─────────────────────────────────────────────────────────────

void ZtoryModel::setNumberingConfig(const NumberingConfig &cfg) {
  m_numberingConfig = cfg;
  // Don't call save() here — caller decides when to persist
}

QString ZtoryModel::nextShotName() const {
  const NumberingConfig &cfg = m_numberingConfig;
  // Parse existing shot numbers to find the highest matching number
  QRegularExpression re;
  if (cfg.style == NumberingConfig::Sequence) {
    re.setPattern(
        QString("^%1\\d+_%2(\\d+)$")
            .arg(QRegularExpression::escape(cfg.seqPrefix),
                 QRegularExpression::escape(cfg.shotPrefix)));
  } else {
    re.setPattern(
        QString("^%1(\\d+)$")
            .arg(QRegularExpression::escape(cfg.shotPrefix)));
  }
  int maxNum = cfg.startNumber - cfg.step;
  for (const auto &s : m_shots) {
    auto m = re.match(s.label());
    if (m.hasMatch()) {
      int n = m.captured(1).toInt();
      if (n > maxNum) maxNum = n;
    }
  }
  int next = qMax(cfg.startNumber, maxNum + cfg.step);
  if (cfg.style == NumberingConfig::Sequence) {
    return QString("%1%2_%3%4")
        .arg(cfg.seqPrefix)
        .arg(cfg.seqNumber, cfg.seqPadding, 10, QChar('0'))
        .arg(cfg.shotPrefix)
        .arg(next, cfg.padding, 10, QChar('0'));
  }
  return QString("%1%2").arg(cfg.shotPrefix).arg(next, cfg.padding, 10, QChar('0'));
}

void ZtoryModel::assignKeepNumbers(int insertAt) {
  int total = (int)m_shots.size();
  if (total == 0) return;
  if (m_shots[insertAt].shotNumber.isEmpty()) {
    if (insertAt == 0) {
      m_shots[0].shotNumber = "01";
      return;
    }
    if (insertAt >= total - 1) {
      int n = 0; bool ok = false;
      for (int j = insertAt - 1; j >= 0 && !ok; j--) {
        QString prev = m_shots[j].shotNumber;
        int i = prev.length() - 1;
        while (i >= 0 && prev[i].isLetter()) i--;
        n = prev.left(i + 1).toInt(&ok);
      }
      if (!ok) n = insertAt;
      m_shots[insertAt].shotNumber = QString("%1").arg(n + 1, 2, 10, QChar('0'));
      return;
    }
    QString prev = m_shots[insertAt - 1].shotNumber;
    int i = prev.length() - 1;
    while (i >= 0 && prev[i].isLetter()) i--;
    QString base = prev.left(i + 1);
    QChar nextLetter = 'A';
    for (int j = 0; j < total; j++) {
      if (j == insertAt) continue;
      if (m_shots[j].shotNumber.startsWith(base)) {
        QString suffix = m_shots[j].shotNumber.mid(base.length());
        if (suffix.length() == 1 && suffix[0].isLetter())
          if (suffix[0] >= nextLetter) nextLetter = QChar(suffix[0].unicode() + 1);
      }
    }
    m_shots[insertAt].shotNumber = base + nextLetter;
  }
}

// ─── Panel automatici ─────────────────────────────────────────────────────────

void ZtoryModel::detectAndUpdatePanels(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  ShotData &s = m_shots[si];
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getXsheet();
  if (!xsh) return;

  int col = s.xsheetColumn;
  int frameCount = xsh->getFrameCount();
  std::set<int> keyframes;
  keyframes.insert(0);

  TStageObjectTree *tree = xsh->getStageObjectTree();
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    TStageObject *obj = tree->getStageObject(TStageObjectId::ColumnId(c), false);
    if (obj) {
      for (int f = 0; f < frameCount; f++)
        if (obj->isKeyframe(f)) keyframes.insert(f);
    }
  }
  TStageObject *cam = tree->getStageObject(TStageObjectId::CameraId(0), false);
  if (cam)
    for (int f = 0; f < frameCount; f++)
      if (cam->isKeyframe(f)) keyframes.insert(f);

  std::vector<PanelData> newPanels;
  std::vector<int> kfList(keyframes.begin(), keyframes.end());
  for (int k = 0; k < (int)kfList.size(); k++) {
    PanelData pd;
    pd.startFrame = kfList[k];
    pd.duration   = (k + 1 < (int)kfList.size()) ? (kfList[k+1] - kfList[k]) : qMax(1, frameCount - kfList[k]);
    if (k < (int)s.panels.size()) {
      pd.dialog = s.panels[k].dialog;
      pd.action = s.panels[k].action;
      pd.notes  = s.panels[k].notes;
    }
    newPanels.push_back(pd);
  }
  if (newPanels.empty()) { PanelData pd; pd.duration = qMax(1, frameCount); newPanels.push_back(pd); }
  s.panels = newPanels;
  emit shotDataChanged(si);
}

void ZtoryModel::refreshFromScene() {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getXsheet();
  if (!xsh) return;

  int colCount = xsh->getColumnCount();
  while ((int)m_shots.size() < colCount) {
    ShotData s; s.xsheetColumn = (int)m_shots.size();
    PanelData pd; s.panels.push_back(pd);
    m_shots.push_back(s);
  }
  emit modelReset();
}

// ─── Il documento degli shot (.ztoryc) ───────────────────────────────────────

// The sub-scene level name exposed in a main-xsheet column: the identity a
// shot keeps through inserts, deletes and reorders (the Animatic caches its
// thumbnails by it too). Empty for a column with no sub-scene.
QString ZtoryModel::shotLevelNameAt(TXsheet *xsh, int col) {
  if (!xsh || col < 0 || col >= xsh->getColumnCount()) return QString();
  TXshColumn *column = xsh->getColumn(col);
  if (!column || column->isEmpty()) return QString();
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1);
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = xsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel())
      return QString::fromStdWString(cell.m_level->getName());
  }
  return QString();
}

// Serialization only: the callers decide whether this scene's file may be
// written at all (shot and character scenes keep their own sidecar) and
// publish to the project afterwards.  The format is the one the Board has
// always written ("version 2", role="storyboard").
bool ZtoryModel::writeShotDocument(const QString &path,
                                   const std::vector<const ShotData *> &shots) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
  QXmlStreamWriter xml(&file);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement("ztoryc");
  xml.writeAttribute("version", "2");
  xml.writeAttribute("role", "storyboard");
  // "No — local only" has to SURVIVE.  It used to live only in
  // m_suppressProjectPublication, a plain member reset with every new panel and
  // gone at every restart: the next session published the scene into the
  // project anyway, against an answer the user had explicitly given — and the
  // duplicate shots that followed brought up the "Two shots with the same name"
  // question, which is how this surfaced.  One attribute, and the answer sticks.
  // "productionTracker" says what the choice really is; "projectPublication"
  // is the name it was first written with and is still read below, so a scene
  // saved in between keeps its answer.
  if (m_docState.trackerOff) {
    xml.writeAttribute("productionTracker", "off");
    xml.writeAttribute("projectPublication", "local");
  }
  if (m_docState.shotIdentityAsked) xml.writeAttribute("shotIdentityAsked", "1");
  // Project metadata (production + title entered by user at scene creation).
  {
    ZtoryModel *model = this;
    // production/title/episode/season/defaultTechnique/techniques now live in
    // the project DB (production.ztrack). The .ztoryc keeps only the per-scene
    // PDF logo settings here, and still READS the old attrs for migration.
    if (!model->pdfLogoPath().isEmpty() || model->pdfNoLogo()) {
      xml.writeStartElement("project");
      if (!model->pdfLogoPath().isEmpty())
        xml.writeAttribute("pdfLogo", model->pdfLogoPath());
      if (model->pdfNoLogo())
        xml.writeAttribute("pdfNoLogo", "1");
      xml.writeEndElement();
    }
    // NOTE: the team roster now lives in the project-level DB
    // (production.ztrack), not in the per-scene .ztoryc. The <team> block is
    // still READ on load (loadZtoryc) for one-time migration of legacy scenes.
    // Assets now live in the project DB (production.ztrack), not the .ztoryc.
    // The <assets> block is still READ on load for one-time migration.
  }
  // Imported screenplay (Script panel) — project-relative path.
  {
    QString sf = scriptFile();
    if (!sf.isEmpty()) xml.writeTextElement("scriptFile", sf);
  }
  // Numbering scheme + sequence list — so the SQ/SH structure survives reload
  // (previously only per-shot number/label were saved, so sequences were lost).
  {
    ZtoryModel *model = this;
    const NumberingConfig &cfg = model->numberingConfig();
    xml.writeStartElement("numbering");
    xml.writeAttribute("style",       QString::number((int)cfg.style));
    xml.writeAttribute("shotPrefix",  cfg.shotPrefix);
    xml.writeAttribute("seqPrefix",   cfg.seqPrefix);
    xml.writeAttribute("panelPrefix", cfg.panelPrefix);
    xml.writeAttribute("step",        QString::number(cfg.step));
    xml.writeAttribute("padding",     QString::number(cfg.padding));
    xml.writeAttribute("seqPadding",  QString::number(cfg.seqPadding));
    xml.writeAttribute("startNumber", QString::number(cfg.startNumber));
    xml.writeAttribute("seqNumber",   QString::number(cfg.seqNumber));
    xml.writeAttribute("resetOnSeqChange", cfg.resetOnSeqChange ? "1" : "0");
    xml.writeEndElement();
    for (const SequenceData &seq : model->sequences()) {
      xml.writeStartElement("sequence");
      xml.writeAttribute("uuid",  seq.uuid);
      xml.writeAttribute("label", seq.label);
      xml.writeAttribute("order", QString::number(seq.orderIndex));
      xml.writeEndElement();
    }
  }
  ToonzScene *scn = TApp::instance()->getCurrentScene()->getScene();
  TXsheet *top    = scn ? scn->getChildStack()->getTopXsheet() : nullptr;
  for (int si = 0; si < (int)shots.size(); si++) {
    const ShotData &sd = *shots[si];
    xml.writeStartElement("shot");
    xml.writeAttribute("index",      QString::number(si));
    if (!sd.uuid.isEmpty())
      xml.writeAttribute("uuid",     sd.uuid);
    // Which sub-scene this entry belongs to: loadZtoryc() matches on it, so
    // the text of a shot stays with the shot when one is inserted before it.
    {
      const QString lvl = shotLevelNameAt(top, sd.xsheetColumn);
      if (!lvl.isEmpty()) xml.writeAttribute("level", lvl);
    }
    xml.writeAttribute("number",     sd.shotNumber);
    xml.writeAttribute("label",      sd.shotLabel);
    xml.writeAttribute("order",      QString::number(sd.orderIndex));
    xml.writeAttribute("sequenceId", sd.sequenceId);
    if (sd.transitionFrames > 0)
      xml.writeAttribute("transition", QString::number(sd.transitionFrames));
    // Production tracking (spreadsheet / Kitsu).
    if (!sd.technique.isEmpty())
      xml.writeAttribute("technique", sd.technique);
    if (!sd.notes.isEmpty())
      xml.writeTextElement("shotNotes", sd.notes);
    if (!sd.vfxNotes.isEmpty())
      xml.writeTextElement("shotVfxNotes", sd.vfxNotes);
    for (auto it = sd.tasks.constBegin(); it != sd.tasks.constEnd(); ++it) {
      xml.writeStartElement("task");
      xml.writeAttribute("type",   it.key());
      xml.writeAttribute("status", ZtoryModel::taskStatusLabel(it.value().status));
      if (!it.value().assignees.isEmpty())
        xml.writeAttribute("assignee", it.value().assignees.join(", "));
      xml.writeEndElement();
    }
    for (int pi = 0; pi < (int)sd.panels.size(); pi++) {
      const PanelData &pd = sd.panels[pi];
      xml.writeStartElement("panel");
      xml.writeAttribute("index",      QString::number(pi));
      xml.writeAttribute("startFrame", QString::number(pd.startFrame));
      xml.writeAttribute("duration",   QString::number(pd.duration));
      if (pd.cameraMoveType != PanelData::CamNone) {
        xml.writeAttribute("camMove",  QString::number((int)pd.cameraMoveType));
        xml.writeAttribute("camLabel", pd.cameraMoveLabel);
        xml.writeAttribute("camRenderFrame", QString::number(pd.camRenderFrame));
        xml.writeAttribute("camW", QString::number(pd.camW));
        xml.writeAttribute("camH", QString::number(pd.camH));
        // Store affines as space-separated doubles
        auto affToStr = [](const double a[6]) {
          return QString("%1 %2 %3 %4 %5 %6")
              .arg(a[0],0,'g',10).arg(a[1],0,'g',10).arg(a[2],0,'g',10)
              .arg(a[3],0,'g',10).arg(a[4],0,'g',10).arg(a[5],0,'g',10);
        };
        xml.writeAttribute("camA0", affToStr(pd.camA0));
        xml.writeAttribute("camA1", affToStr(pd.camA1));
      }
      if (pd.hasLight) {
        xml.writeAttribute("lightTail", QString("%1 %2")
            .arg(pd.lightTailX, 0, 'g', 6).arg(pd.lightTailY, 0, 'g', 6));
        xml.writeAttribute("lightTip", QString("%1 %2")
            .arg(pd.lightTipX, 0, 'g', 6).arg(pd.lightTipY, 0, 'g', 6));
        xml.writeAttribute("lightDepth", QString::number(pd.lightDepth, 'g', 4));
        xml.writeAttribute("lightSpread", QString::number(pd.lightSpread, 'g', 4));
        xml.writeAttribute("lightColor", pd.lightColor);
      }
      xml.writeTextElement("dialog", pd.dialog);
      xml.writeTextElement("action", pd.action);
      xml.writeTextElement("notes",  pd.notes);
      xml.writeEndElement();
    }
    xml.writeEndElement();
  }
  xml.writeEndElement();
  xml.writeEndDocument();
  file.close();
  return true;
}

int ZtoryModel::shotIndexForCol(int col) const {
  // Scan the actual main xsheet for child-level columns in order and return
  // the ordinal of the column that matches `col`. Same algorithm the Board
  // uses in refreshFromScene(), so the two stay consistent without relying
  // on m_shots[i].xsheetColumn (which can be stale until the next
  // reconcileWithXsheet).
  TApp *app = TApp::instance();
  if (!app) return -1;
  ToonzScene *scene = app->getCurrentScene() ? app->getCurrentScene()->getScene() : nullptr;
  if (!scene) return -1;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return -1;
  int childIdx = 0;
  int numCols = xsh->getColumnCount();
  for (int c = 0; c < numCols; c++) {
    TXshColumn *column = xsh->getColumn(c);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    bool isChild = false;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, c);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        isChild = true;
        break;
      }
    }
    if (!isChild) continue;
    if (c == col) {
      // The ordinal comes from the xsheet; the caller indexes m_shots with it.
      // An Animatic "+" writes the xsheet and leaves m_shots to catch up
      // later (through the Board), so the model can be one shot short: return
      // "not found" rather than an index past the end (Windows crash in the
      // Panel Navigator, 2026-10-04, 0.14.1).
      if (childIdx >= (int)m_shots.size()) {
        qWarning("[ZTORY] shotIndexForCol: col %d -> idx %d but model has %d "
                 "shots (stale)",
                 col, childIdx, (int)m_shots.size());
        return -1;
      }
      return childIdx;
    }
    childIdx++;
  }
  return -1;
}

void ZtoryModel::resequenceXsheet() {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;
  int numCols = xsh->getColumnCount();
  int maxFrames = xsh->getFrameCount() + 200;
  int startFrame = 0;
  // Cross-dissolve pass — STEP 0: strip the overlap cells + blend fx placed by
  // the previous resequence, so the layout loop below measures each shot's TRUE
  // duration (the exposed overlap cells otherwise inflate getRange() and the
  // shots grow cumulatively).  No-op when no dissolve is present.
  ZtoryShotOps::teardownCrossDissolves(xsh);
  std::vector<ZtoryShotOps::ShotLayout> dissolveLayout;
  for (int col = 0; col < numCols; col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    // STEP 1 — strip the SFH we placed in the previous resequence.
    // Why explicit strip instead of getRange(ignoreLastStop=true)?
    // After a trim/removeCells the cell layout can be:
    //     [real cells] [empty rows] [trailing SFH]
    // ignoreLastStop=true just decrements r1 by 1 — landing on an EMPTY
    // row.  duration would then = old_duration (wrong, shot doesn't shrink).
    // By physically removing the SFH first we let getRange skip the empty
    // rows backward and find the actual last drawing.
    column->getRange(r0, r1);
    if (r1 >= 0) {
      TXshCell lastCell = xsh->getCell(r1, col);
      if (lastCell.getFrameId().isStopFrame()) {
        xsh->clearCells(r1, col, 1);
        // Re-read after stripping the SFH.
        column->getRange(r0, r1);
      }
    }
    int duration = r1 - r0 + 1;
    TXshChildLevel *cl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        cl = cell.m_level->getChildLevel();
        break;
      }
    }
    // Audio (or any non-child-level) columns are independent of the shot
    // timeline — they stay where they are and must NOT contribute to
    // startFrame.  The previous version bumped startFrame by the audio
    // column's range, so a single sound column (e.g. a 1000-frame voice
    // track) pushed every subsequent SHOT 1000 frames down the main xsheet:
    // the shots survived but vanished from the visible range — exactly the
    // tester's "timeline wiped itself" / "adjusted length and everything
    // disappeared" reports.
    if (!cl) continue;
    // Cross-dissolve head-hold: if this shot has an incoming dissolve, its
    // sub-scene carries |headOffset| extra hold copies at the head (marked by
    // the persisted "XD-in" note column).  Skip them by exposing sub-scene
    // frame (r+1+headOffset) instead of (r+1), so the animatic shows the shot's
    // real content on time.  Rebuilding the frameIds as a plain 1..N sequence
    // (the old code) silently wiped the offset applied by onTransitionChanged,
    // desyncing the main xsheet from the head-hold and making shot B hold its
    // first frame after any resequence.  Deriving it from XD-in here makes the
    // offset self-healing across resequence / reload / undo.
    int headOffset = ZtoryShotOps::xdInHeadOffset(cl->getXsheet());
    for (int r = 0; r <= maxFrames; r++) xsh->clearCells(r, col);
    for (int r = 0; r < duration; r++)
      xsh->setCell(startFrame + r, col,
                   TXshCell(cl, TFrameId(r + 1 + headOffset)));
    // Stop Frame Hold at startFrame+duration: prevents the shot's last
    // drawing from "bleeding" via implicit hold into the next shot during
    // animatic playback/render.  The shot's duration in the main xsheet IS
    // the sub-scene's mark-out+1 (Ztoryc convention), so the SFH sits at
    // row markOut+1 within this column — exactly between this shot's last
    // cell and the next shot's column space.  Re-applied every resequence
    // (match-duration / trim / rolling-edit / add / delete / merge) so it
    // stays glued to the current boundary.
    xsh->setCell(startFrame + duration, col,
                 TXshCell(cl, TFrameId(TFrameId::STOP_FRAME)));
    dissolveLayout.push_back(
        {col, startFrame, duration, cl->getXsheet()});
    startFrame += duration;
  }
  // Cross-dissolve pass — STEP N: re-expose overlap + rebuild the blend fx on
  // the freshly laid-out (clean) columns.  Keyed off the persisted XD note
  // columns, so it is self-healing across resequence / reload / undo.
  ZtoryShotOps::applyCrossDissolves(xsh, dissolveLayout);
  xsh->updateFrameCount();

  // Always pin the main xsheet mark-out to the last occupied frame (video OR
  // audio, whichever is further).  This prevents a stale native mark-out from
  // blocking the animatic playhead: the FlipConsole stops at m_markerTo which
  // comes from the native play range whenever the two are out of sync.
  // Using xsh->getFrameCount() (not videoFrameCount) here so that a long audio
  // column that extends past the last shot is also covered.
  // ONLY at main level: setPlayRange acts on the CURRENT context, so when a
  // resequence fires while a sub-scene is open (e.g. editing a transition from
  // inside the shot) this would move the shot's mark-out to the end of the
  // MAIN timeline. The sub's range is owned by ztorySetShotRange.
  if (scene->getChildStack()->getAncestorCount() == 0) {
    int lastFrame = xsh->getFrameCount() - 1;
    if (lastFrame >= 0)
      XsheetGUI::setPlayRange(0, lastFrame, 1, false);
  }

  app->getCurrentXsheet()->notifyXsheetChanged();
  reconcileWithXsheet();
  emit modelReset();
}

// The sub-scene a main-xsheet column exposes, or nullptr if it is not a shot.
static TXshChildLevel *shotColumnLevel(TXsheet *xsh, int col) {
  TXshColumn *column = xsh ? xsh->getColumn(col) : nullptr;
  if (!column || column->isEmpty()) return nullptr;
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1);
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = xsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel())
      return cell.m_level->getChildLevel();
  }
  return nullptr;
}

void ZtoryModel::recordShotIdentity(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  if ((int)m_shotIds.size() < (int)m_shots.size())
    m_shotIds.resize(m_shots.size());
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet *top      = scene ? scene->getChildStack()->getTopXsheet() : nullptr;
  const int col     = m_shots[si].xsheetColumn;
  TXshChildLevel *cl = shotColumnLevel(top, col);
  m_shotIds[si].column = cl ? top->getColumn(col) : nullptr;
  m_shotIds[si].level  = cl;
  m_shotIds[si].col    = cl ? col : -1;
}

ZtoryShotList::Ptr ZtoryModel::shotPtrForColumn(int col) const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet *top      = scene ? scene->getChildStack()->getTopXsheet() : nullptr;
  TXshColumn *column = top ? top->getColumn(col) : nullptr;
  if (!column) return nullptr;
  for (int i = 0; i < (int)m_shots.size(); i++)
    if (m_shots[i].xsheetColumn == col && i < (int)m_shotIds.size() &&
        m_shotIds[i].column == column)
      return m_shots.ptr(i);
  return nullptr;
}

bool ZtoryModel::takeShotObject(int col, ZtoryShotList::Ptr holder) {
  ZtoryShotList::Ptr current = shotPtrForColumn(col);
  if (!current || !holder || current == holder) return current == holder;
  if (!isFreshShot(current.get())) return false;
  const int i = indexOfShot(current.get());
  if (i < 0) return false;
  // Never the same object in two places: if the model already has it
  // elsewhere, two shots would share one.
  const int already = indexOfShot(holder.get());
  if (already >= 0 && already != i) return false;
  m_freshShots.erase(current.get());
  holder->xsheetColumn = col;
  m_shots.setPtr(i, std::move(holder));
  return true;
}

void ZtoryModel::notifyShotEdited(const ShotData *sd) {
  const int i = indexOfShot(sd);
  if (i < 0) return;
  if (m_previews.size() < m_shots.size()) m_previews.resize(m_shots.size());
  m_previews[i].resize(m_shots[i].panels.size(), QPixmap());
  recordShotIdentity(i);
  emit shotDataChanged(i);
}

void ZtoryModel::setShotDataLoadedFor(const QString &ztoryPath) {
  m_shotDataLoadedFor = ztoryPath;
  m_shotDataSceneObj  = TApp::instance()->getCurrentScene()->getScene();
}

int ZtoryModel::indexOfShot(const ShotData *sd) const {
  for (int i = 0; i < (int)m_shots.size(); i++)
    if (&m_shots[i] == sd) return i;
  return -1;
}

void ZtoryModel::reconcileWithXsheet() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet *top      = scene ? scene->getChildStack()->getTopXsheet() : nullptr;
  if (!top) return;
  m_shotIds.resize(m_shots.size());
  m_previews.resize(m_shots.size());

  // The scene's shot columns, in order.
  std::vector<int> cols;
  std::vector<TXshColumn *> colObjs;
  std::vector<TXshChildLevel *> levels;
  for (int c = 0; c < top->getColumnCount(); c++)
    if (TXshChildLevel *cl = shotColumnLevel(top, c)) {
      cols.push_back(c);
      colObjs.push_back(top->getColumn(c));
      levels.push_back(cl);
    }

  const int n = (int)m_shots.size();
  std::vector<bool> taken(n, false);
  ZtoryShotList next;
  std::vector<std::vector<QPixmap>> nextPreviews;
  std::vector<ShotIdentity> nextIds;
  // Where each entry is expected now.  If its column object moved in the scene
  // (a shot inserted or deleted before it), the column says where the shot
  // went and the entry's xsheetColumn is the stale one.  If the column stayed
  // where it was and the entry says another column, the Board placed it there
  // on purpose: its reorder moves the CELLS between columns that stay put.
  std::map<TXshColumn *, int> colIndex;
  for (int c = 0; c < top->getColumnCount(); c++)
    if (TXshColumn *column = top->getColumn(c)) colIndex[column] = c;
  std::vector<int> expected(n, -1);
  for (int k = 0; k < n; k++) {
    auto it = m_shotIds[k].column ? colIndex.find(m_shotIds[k].column)
                                  : colIndex.end();
    const int now = it != colIndex.end() ? it->second : -1;
    // A column that is no longer in the scene expects nothing: its entry
    // (a deleted shot) must not win pass 1 over a living Copy of the same
    // sub-scene.  Pass 2 can still re-attach it by sub-scene (the undo
    // re-creates columns).
    if (m_shotIds[k].column && now < 0)
      expected[k] = -1;
    else
      expected[k] = (now >= 0 && now != m_shotIds[k].col) ? now
                                                          : m_shots[k].xsheetColumn;
  }
  // Pass by pass over ALL the columns, the strictest first — one pass per
  // column at a time let a looser test take an entry another column matched
  // exactly (two Copies of the same sub-scene swapped their data).
  const int J = (int)cols.size();
  std::vector<int> match(J, -1);
  int matchedSame = 0, matchedLevel = 0, matchedIndex = 0, fresh = 0;
  auto pass = [&](int &counter, auto pred) {
    for (int j = 0; j < J; j++) {
      if (match[j] >= 0) continue;
      for (int k = 0; k < n; k++)
        if (!taken[k] && pred(k, j)) {
          match[j] = k;
          taken[k] = true;
          counter++;
          break;
        }
    }
  };
  // 1. its sub-scene, where it is expected: the shot itself
  pass(matchedSame, [&](int k, int j) {
    return m_shotIds[k].level == levels[j] && expected[k] == cols[j];
  });
  // 2. its sub-scene, elsewhere (the undo re-creates columns)
  pass(matchedLevel, [&](int k, int j) { return m_shotIds[k].level == levels[j]; });
  // 3. an entry not matched to any column yet, appended for this one
  pass(matchedIndex, [&](int k, int j) {
    return !m_shotIds[k].column && m_shots[k].xsheetColumn == cols[j];
  });
  for (int j = 0; j < J; j++) {
    const int i = match[j];
    if (i >= 0) {
      next.push_back(m_shots.ptr(i));  // the object itself, not a copy
      nextPreviews.push_back(std::move(m_previews[i]));
    } else {
      fresh++;
      ShotData s;
      PanelData pd;
      int r0 = 0, r1 = 0;
      top->getColumn(cols[j])->getRange(r0, r1, /*ignoreLastStop=*/true);
      pd.duration = r1 >= r0 ? r1 - r0 + 1 : 24;
      s.panels.push_back(pd);
      next.push_back(std::move(s));
      m_freshShots.insert(&next.back());
      nextPreviews.push_back({QPixmap()});
    }
    next.back().xsheetColumn = cols[j];
    nextIds.push_back({colObjs[j], levels[j], cols[j]});
  }
  const int dropped = n - (matchedSame + matchedLevel + matchedIndex);
  if (fresh || dropped || matchedLevel || matchedIndex)
    qWarning("[ZTORY] model reconcile: %d -> %d shots (%d same column, %d by "
             "sub-scene, %d by position, %d new, %d gone)",
             n, (int)next.size(), matchedSame, matchedLevel, matchedIndex,
             fresh, dropped);
  // Forget the fresh marks of objects that left the list.
  for (auto it = m_freshShots.begin(); it != m_freshShots.end();) {
    bool kept = false;
    for (const ShotData &sd : next)
      if (&sd == *it) { kept = true; break; }
    it = kept ? std::next(it) : m_freshShots.erase(it);
  }
  m_shots    = std::move(next);
  m_previews = std::move(nextPreviews);
  m_shotIds  = std::move(nextIds);
}

void ZtoryModel::updateColumnName(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  // Not in an exported shot or a character scene: there a sub-scene column
  // is a character, and naming it after a shot label renamed SOFIA's column
  // «sh010» (Franco, 2026-09-27). Same guard as StoryboardPanel's.
  if (!scene->isUntitled()) {
    const QString role = ZtoryCharacter::roleOf(
        scene->decodeFilePath(scene->getScenePath()).getQString());
    if (role == QLatin1String("shot") || role == QLatin1String("character"))
      return;
  }
  TXsheet *xsh = scene->getXsheet();
  if (!xsh) return;
  int col = m_shots[si].xsheetColumn;
  TStageObject *obj = xsh->getStageObjectTree()->getStageObject(TStageObjectId::ColumnId(col), false);
  if (obj) obj->setName(m_shots[si].label().toStdString());
}

// NOTE: updateAllPreviews() must NOT be called from onXsheetChanged().
// Calling IconGenerator::getIcon() during an xsheet mutation (e.g. import scene)
// triggers PlasticDeformerStorage::process() in an uninitialized GL context → crash.
// Thumbnail refresh happens via frameSwitched signal with a debounce timer
// (see StoryboardPanel). See AGENTS.md: "Thumbnail refresh = on frameSwitched".
void ZtoryModel::onXsheetChanged() { /* thumbnails updated via frameSwitched debounce */ }
void ZtoryModel::onSceneChanged()  { refreshFromScene(); }

void ZtoryModel::activateShotForViewing(int col) {
  // NOTE: do NOT call TImageCache::instance()->clear() here.  It wipes the
  // ENTIRE app-wide image cache, including the still-needed images of the
  // shot we're switching into and any levels the user is actively drawing
  // on — causing drawings to "disappear" and the red-dot cursor (cache
  // miss on the current cell).  Memory pressure is now handled by
  // TSystem::memoryShortage() (implemented for macOS/Linux) which lets
  // TImageCache evict naturally when RAM gets low.
  emit shotActivatedForViewing(col);
}
void ZtoryModel::requestReturnToViewer()         { emit returnToViewerMainRequested(); }
