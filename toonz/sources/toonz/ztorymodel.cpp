#include "ztorymodel.h"
#include "ztoryshotops.h"     // syncChildCameraToMain
#include "menubarcommandids.h"  // MI_Workflow* ids for workflowCommand()
#include "xsheetdragtool.h"   // XsheetGUI::setPlayRange
#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
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
#include <QMessageBox>
#include <QRegularExpression>
#include <QFileInfo>
#include <QUuid>
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

ZtoryModel::ZtoryModel() : m_fps(24) {
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
  for (ProjectShot &ps : m_projectShots) {
    if (ps.uuid != uuid) continue;
    QString tech = !technique.isEmpty() ? technique
                   : (ps.technique.isEmpty() ? m_defaultTechnique : ps.technique);
    QStringList tts;
    if (const Technique *t = findTechnique(tech)) tts = t->taskTypes;
    // Opening the shot scene resumes work: walk the technique's tasks in order,
    // skip Storyboard (the pre-.tnz board pass) and any already-Done task; the
    // first not-Done task is the active one — if it's Ready (next up) or Retake
    // (kicked back by review) it advances to Wip. value() avoids inserting empty
    // Todo entries for tasks the artist hasn't reached yet.
    for (const QString &tt : tts) {
      if (tt == "Storyboard") continue;
      const TaskStatus s = ps.tasks.value(tt).status;
      if (s == TaskStatus::Done) continue;
      if (s == TaskStatus::Ready || s == TaskStatus::Retake) {
        ps.tasks[tt].status = TaskStatus::Wip;
        saveProjectDb();
        emit taskStatusChanged();
      }
      break;  // first not-Done task handled — stop
    }
    break;
  }
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
    if (a.type.compare("Character", Qt::CaseInsensitive) == 0)
      uuidByName.insert(a.name.trimmed().toLower(), a.uuid);
  for (auto it = m_speakerAliases.constBegin(); it != m_speakerAliases.constEnd(); ++it)
    uuidByName.insert(it.key(), it.value());

  auto emitLine = [&](const QString &speaker, const QString &said) {
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

void ZtoryModel::setSpeakerAlias(const QString &scriptName,
                                const QString &assetUuid) {
  const QString key = scriptName.trimmed().toLower();
  if (key.isEmpty()) return;
  if (assetUuid.isEmpty()) m_speakerAliases.remove(key);
  else m_speakerAliases.insert(key, assetUuid);
}

bool ZtoryModel::speakerAt(const QString &rawLine, const QString &rawNext,
                           QString *outName, bool *outMatched) const {
  const QString line = rawLine.trimmed();
  if (line.isEmpty()) return false;
  if (line.startsWith('(') && line.endsWith(')')) return false;  // didascalia

  QHash<QString, QString> uuidByName;
  for (const Asset &a : m_assets)
    if (a.type.compare("Character", Qt::CaseInsensitive) == 0)
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

QString ZtoryModel::resolveAssetFile(const Asset &a, QString *why) const {
  auto fail = [&](const QString &msg) {
    if (why) *why = msg;
    return QString();
  };
  // 1. Il legame esplicito VINCE sempre. E' l'unica risposta che non e' una
  //    supposizione, quindi non si discute e non si cerca oltre.
  if (!a.filePath.isEmpty()) {
    if (QFile::exists(a.filePath)) { if (why) why->clear(); return a.filePath; }
    return fail(tr("linked file is missing: %1").arg(a.filePath));
  }

  // 2. Altrimenti la convenzione: cartella della categoria + nome dell'asset.
  const QString dir = assetDirForType(a.type);
  if (dir.isEmpty())
    return fail(a.type.compare("Character", Qt::CaseInsensitive) == 0
                    ? tr("a character has no folder: link its scene")
                    : tr("no folder set for type %1").arg(a.type));
  if (!QDir(dir).exists()) return fail(tr("folder not found: %1").arg(dir));

  // Corrispondenza sul NOME BASE, senza distinzione di maiuscole, con qualunque
  // estensione. Volutamente NON si accettano prefissi o suffissi: «macchina»
  // non deve pescare «macchina_v03» ne' «macchina_rotta», perche' sceglierne
  // uno a caso e' peggio che non trovarlo — l'errore si vedrebbe solo in
  // render, giorni dopo.
  QStringList hits;
  const QFileInfoList entries =
      QDir(dir).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
  for (const QFileInfo &fi : entries)
    if (fi.completeBaseName().compare(a.name, Qt::CaseInsensitive) == 0)
      hits << fi.absoluteFilePath();

  if (hits.isEmpty())
    return fail(tr("no file named «%1» in %2").arg(a.name, dir));
  // 3. Ambiguo: NON si indovina. Due file con lo stesso nome e estensione
  //    diversa (macchina.tlv e macchina.psd) sono una scelta dell'utente, non
  //    nostra; si chiede il legame esplicito.
  if (hits.size() > 1)
    return fail(tr("%1 files named «%2» in %3 — link the right one")
                    .arg(hits.size()).arg(a.name, dir));

  if (why) why->clear();
  return hits.first();
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

void ZtoryModel::setAssetTaskStatus(int i, const QString &taskType,
                                    TaskStatus status) {
  if (i < 0 || i >= (int)m_assets.size()) return;
  m_assets[i].tasks[taskType].status = status;
  emit assetsChanged();
}

void ZtoryModel::setAssetTaskStatusByUuid(const QString &uuid,
                                          const QString &taskType,
                                          TaskStatus status) {
  for (int i = 0; i < (int)m_assets.size(); i++)
    if (m_assets[i].uuid == uuid) { setAssetTaskStatus(i, taskType, status); return; }
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
  bool metaEmpty = m_production.isEmpty() && m_title.isEmpty() &&
                   m_season.isEmpty() && m_episode.isEmpty() &&
                   m_team.isEmpty() && m_assets.empty();
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

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QXmlStreamWriter xml(&file);
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
    if (!as.tags.isEmpty()) xml.writeAttribute("tags", as.tags.join("|"));
    if (!as.filePath.isEmpty()) xml.writeAttribute("file", as.filePath);
    if (!as.importPolicy.isDefault()) writeImportPolicy(xml, as.importPolicy);
    for (auto it = as.tasks.constBegin(); it != as.tasks.constEnd(); ++it) {
      xml.writeStartElement("atask");
      xml.writeAttribute("type",   it.key());
      xml.writeAttribute("status", taskStatusLabel(it.value().status));
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
    for (auto it = ps.tasks.constBegin(); it != ps.tasks.constEnd(); ++it) {
      xml.writeStartElement("task");
      xml.writeAttribute("type",   it.key());
      xml.writeAttribute("status", taskStatusLabel(it.value().status));
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
    xml.writeEndElement();  // shot
  }
  xml.writeEndElement();  // shots

  xml.writeEndElement();  // ztrack
  xml.writeEndDocument();
}

void ZtoryModel::loadProjectDbFromPath(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  loadProjectDbFromDevice(file);
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
  loadProjectDbFromDevice(file);
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
      as.filePath     = a.value("file").toString();
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
    }
  }
  m_team = team;  // project file is authoritative
  if (!techs.empty()) m_techniques = techs;
  // Migration: older projects were seeded without the Storyboard task. Prepend
  // it where missing so the board/animatic task exists (and can be pushed to
  // Kitsu + receive preview uploads). Persisted on the next project save.
  for (Technique &t : m_techniques)
    if (!t.taskTypes.contains("Storyboard")) t.taskTypes.prepend("Storyboard");
  // Asset types: adopt the file's list; a legacy project (no <assetTypes> block)
  // re-seeds the canonical defaults so the taxonomy is never empty.
  m_assetTypes = atypes;
  seedDefaultAssetTypes();
  m_assets    = assets;
  m_projectShots   = pshots;
  m_storyboardFiles = sboards;
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
    if (tt != "Storyboard") return tt;
  return QString();
}

QString ZtoryModel::nextTaskType(const QString &technique,
                                 const QString &afterTask) const {
  const Technique *t = findTechnique(technique);
  if (!t) return QString();
  const int i = t->taskTypes.indexOf(afterTask);
  if (i < 0 || i + 1 >= t->taskTypes.size()) return QString();
  return t->taskTypes[i + 1];
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

void ZtoryModel::setProjectShotTaskStatusByUuid(const QString &uuid,
                                                const QString &taskType,
                                                TaskStatus status) {
  for (ProjectShot &ps : m_projectShots) {
    if (ps.uuid == uuid) {
      ps.tasks[taskType].status = status;
      // Mirror into the open scene's shot for .ztoryc consistency.
      for (ShotData &sd : m_shots)
        if (sd.uuid == uuid) { sd.tasks[taskType].status = status; break; }
      emit taskStatusChanged();
      return;
    }
  }
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
  assignShotLabel(m_shots, si, m_numberingConfig);
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

void ZtoryModel::syncShotPanels(int si, const std::vector<PanelData> &panels,
                                const QString &label, int xsheetCol) {
  if (si < 0) return;
  // Grow m_shots so it mirrors the Board's shot count. The Board calls this
  // for every shot it knows about after refreshFromScene, so once all calls
  // complete ZtoryModel::m_shots has one entry per child-level column.
  while ((int)m_shots.size() <= si) {
    ShotData s;
    PanelData pd;
    s.panels.push_back(pd);
    m_shots.push_back(s);
    m_previews.push_back({QPixmap()});
  }
  m_shots[si].panels = panels;
  // The Board is authoritative for shot labels. Sync it whenever provided so
  // the Shot Board header always reflects the current label, even for scenes
  // that were created before the .ztoryc labelling system was introduced.
  if (!label.isEmpty()) {
    m_shots[si].shotLabel  = label;
    m_shots[si].shotNumber = label;  // keep legacy field in sync
  }
  // xsheetColumn is critical: refreshPreview() uses it to render the correct
  // sub-scene thumbnail. Without it, all shots would render column 0 (SH010).
  if (xsheetCol >= 0)
    m_shots[si].xsheetColumn = xsheetCol;
  m_previews[si].resize(panels.size(), QPixmap());
  emit shotDataChanged(si);
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
    m_shots.insert(m_shots.begin() + insertAt, s);
    m_previews.insert(m_previews.begin() + insertAt, {QPixmap()});
    generateShotLabel(insertAt);
    emit shotAdded(insertAt);
  }
  save();
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

  app->getCurrentXsheet()->notifyXsheetChanged();
  resequenceXsheet();
  emit modelReset();
}

void ZtoryModel::addShotFromRasters(const QString &name,
                                    const std::vector<TRaster32P> &panels) {
  if (panels.empty()) return;
  if (!assertMainXsheet(false)) return;
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
    if (nameTaken(levelName)) return rollback();
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
  // Name the column after the shot, like every other shot-creating path: the
  // Board's reorder detection compares this name against the shot label, and an
  // unnamed column carries no ordering information.
  updateColumnName(si);

  app->getCurrentXsheet()->notifyXsheetChanged();
  resequenceXsheet();
  emit modelReset();
}

void ZtoryModel::removeShot(int si) {
  if (!assertMainXsheet(true)) return;
  if (si < 0 || si >= (int)m_shots.size()) return;
  m_shots.erase(m_shots.begin() + si);
  if (si < (int)m_previews.size())
    m_previews.erase(m_previews.begin() + si);
  emit shotRemoved(si);
  save();
}

void ZtoryModel::moveShot(int from, int to) {
  if (!assertMainXsheet(false)) return;
  if (from == to) return;
  if (from < 0 || from >= (int)m_shots.size()) return;
  if (to   < 0 || to   >= (int)m_shots.size()) return;
  ShotData s = m_shots[from];
  std::vector<QPixmap> px = (from < (int)m_previews.size()) ? m_previews[from] : std::vector<QPixmap>();
  m_shots.erase(m_shots.begin() + from);
  m_shots.insert(m_shots.begin() + to, s);
  if (!m_previews.empty()) {
    m_previews.erase(m_previews.begin() + from);
    m_previews.insert(m_previews.begin() + to, px);
  }
  emit shotMoved(from, to);
  save();
}

void ZtoryModel::cloneShot(int si) {
  if (!assertMainXsheet(true)) return;
  if (si < 0 || si >= (int)m_shots.size()) return;
  ShotData s = m_shots[si];
  s.shotNumber = "";   // reset — will be assigned by generateShotLabel
  s.shotLabel  = "";
  s.orderIndex = 0;
  m_shots.insert(m_shots.begin() + si + 1, s);
  std::vector<QPixmap> px = (si < (int)m_previews.size()) ? m_previews[si] : std::vector<QPixmap>();
  m_previews.insert(m_previews.begin() + si + 1, px);
  generateShotLabel(si + 1);
  emit shotAdded(si + 1);
  save();
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

void ZtoryModel::renumberAll() {
  const int scale = 100;
  for (int i = 0; i < (int)m_shots.size(); i++) {
    m_shots[i].shotNumber = m_numberingConfig.shotName(i);
    m_shots[i].shotLabel  = m_shots[i].shotNumber;  // keep shotLabel in sync
    m_shots[i].orderIndex =
        (m_numberingConfig.startNumber + i * m_numberingConfig.step) * scale;
    updateColumnName(i);
  }
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
  save();
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

// ─── Persistenza ─────────────────────────────────────────────────────────────

void ZtoryModel::save() {
  if (m_ztoryPath.isEmpty()) return;
  QFile file(m_ztoryPath);
  if (!file.open(QIODevice::WriteOnly)) return;
  QXmlStreamWriter xml(&file);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement("ztoryc");
  xml.writeAttribute("version", "4");
  // ── Numbering config ──
  xml.writeStartElement("numberingConfig");
  xml.writeAttribute("style",       QString::number((int)m_numberingConfig.style));
  xml.writeAttribute("shotPrefix",  m_numberingConfig.shotPrefix);
  xml.writeAttribute("seqPrefix",   m_numberingConfig.seqPrefix);
  xml.writeAttribute("panelPrefix", m_numberingConfig.panelPrefix);
  xml.writeAttribute("step",        QString::number(m_numberingConfig.step));
  xml.writeAttribute("padding",     QString::number(m_numberingConfig.padding));
  xml.writeAttribute("seqPadding",  QString::number(m_numberingConfig.seqPadding));
  xml.writeAttribute("startNumber", QString::number(m_numberingConfig.startNumber));
  xml.writeAttribute("seqNumber",   QString::number(m_numberingConfig.seqNumber));
  xml.writeEndElement();
  // ── Sequences ──
  if (!m_sequences.empty()) {
    xml.writeStartElement("sequences");
    for (const auto &seq : m_sequences) {
      xml.writeStartElement("sequence");
      xml.writeAttribute("uuid",  seq.uuid);
      xml.writeAttribute("label", seq.label);
      xml.writeAttribute("order", QString::number(seq.orderIndex));
      xml.writeEndElement();
    }
    xml.writeEndElement();
  }
  // ── Shots ──
  for (int si = 0; si < (int)m_shots.size(); si++) {
    const ShotData &s = m_shots[si];
    xml.writeStartElement("shot");
    xml.writeAttribute("index",  QString::number(si));
    xml.writeAttribute("number", s.shotNumber);       // legacy
    xml.writeAttribute("label",  s.shotLabel);        // v4
    xml.writeAttribute("order",  QString::number(s.orderIndex)); // v4
    xml.writeAttribute("seqId",  s.sequenceId);       // v4
    xml.writeAttribute("column", QString::number(s.xsheetColumn));
    for (int pi = 0; pi < (int)s.panels.size(); pi++) {
      const PanelData &pd = s.panels[pi];
      xml.writeStartElement("panel");
      xml.writeAttribute("index",      QString::number(pi));
      xml.writeAttribute("startFrame", QString::number(pd.startFrame));
      xml.writeAttribute("duration",   QString::number(pd.duration));
      if (!pd.panelLabel.isEmpty())
        xml.writeAttribute("panelLabel", pd.panelLabel);
      xml.writeAttribute("panelOrder",  QString::number(pd.orderIndex));
      xml.writeTextElement("dialog", pd.dialog);
      xml.writeTextElement("action", pd.action);
      xml.writeTextElement("notes",  pd.notes);
      xml.writeEndElement();
    }
    xml.writeEndElement();
  }
  xml.writeEndElement();
  xml.writeEndDocument();
}

void ZtoryModel::load() {
  if (m_ztoryPath.isEmpty()) return;
  QFile file(m_ztoryPath);
  if (!file.open(QIODevice::ReadOnly)) return;
  m_shots.clear();
  m_previews.clear();
  m_sequences.clear();

  QXmlStreamReader xml(&file);
  int si = -1, pi = -1;

  while (!xml.atEnd()) {
    xml.readNext();
    if (!xml.isStartElement()) continue;
    const auto name = xml.name();

    if (name == QLatin1String("numberingConfig")) {
      m_numberingConfig.style =
          (NumberingConfig::Style)xml.attributes().value("style").toInt();
      m_numberingConfig.shotPrefix  = xml.attributes().value("shotPrefix").toString();
      m_numberingConfig.seqPrefix   = xml.attributes().value("seqPrefix").toString();
      QString ppfx = xml.attributes().value("panelPrefix").toString();
      if (!ppfx.isEmpty()) m_numberingConfig.panelPrefix = ppfx;
      m_numberingConfig.step        = xml.attributes().value("step").toInt();
      m_numberingConfig.padding     = xml.attributes().value("padding").toInt();
      m_numberingConfig.seqPadding  = xml.attributes().value("seqPadding").toInt();
      m_numberingConfig.startNumber = xml.attributes().value("startNumber").toInt();
      m_numberingConfig.seqNumber   = xml.attributes().value("seqNumber").toInt();
      // Safety defaults for old files
      if (m_numberingConfig.step <= 0)    m_numberingConfig.step = 10;
      if (m_numberingConfig.padding <= 0) m_numberingConfig.padding = 3;
      if (m_numberingConfig.shotPrefix.isEmpty())  m_numberingConfig.shotPrefix  = "SH";
      if (m_numberingConfig.panelPrefix.isEmpty()) m_numberingConfig.panelPrefix = "P";

    } else if (name == QLatin1String("sequence")) {
      SequenceData seq;
      seq.uuid       = xml.attributes().value("uuid").toString();
      seq.label      = xml.attributes().value("label").toString();
      seq.orderIndex = xml.attributes().value("order").toInt();
      if (!seq.uuid.isEmpty()) m_sequences.push_back(seq);

    } else if (name == QLatin1String("shot")) {
      si = xml.attributes().value("index").toInt();
      while ((int)m_shots.size() <= si) m_shots.push_back(ShotData());
      m_shots[si].shotNumber   = xml.attributes().value("number").toString();
      m_shots[si].shotLabel    = xml.attributes().value("label").toString();
      m_shots[si].orderIndex   = xml.attributes().value("order").toInt();
      m_shots[si].sequenceId   = xml.attributes().value("seqId").toString();
      m_shots[si].xsheetColumn = xml.attributes().value("column").toInt();
      // Backward compat (v1–v3): if shotLabel absent, copy from shotNumber
      if (m_shots[si].shotLabel.isEmpty())
        m_shots[si].shotLabel = m_shots[si].shotNumber;
      m_previews.resize(m_shots.size());

    } else if (name == QLatin1String("panel") && si >= 0) {
      pi = xml.attributes().value("index").toInt();
      PanelData pd;
      pd.startFrame = xml.attributes().value("startFrame").toInt();
      pd.duration   = xml.attributes().value("duration").toInt();
      pd.panelLabel = xml.attributes().value("panelLabel").toString();
      pd.orderIndex = xml.attributes().value("panelOrder").toInt();
      while ((int)m_shots[si].panels.size() <= pi) m_shots[si].panels.push_back(PanelData());
      m_shots[si].panels[pi] = pd;
      m_previews[si].resize(m_shots[si].panels.size());

    } else if (name == QLatin1String("dialog") && si >= 0 && pi >= 0)
      m_shots[si].panels[pi].dialog = xml.readElementText();
    else if (name == QLatin1String("action") && si >= 0 && pi >= 0)
      m_shots[si].panels[pi].action = xml.readElementText();
    else if (name == QLatin1String("notes") && si >= 0 && pi >= 0)
      m_shots[si].panels[pi].notes  = xml.readElementText();
  }
  emit modelReset();
}

int ZtoryModel::shotIndexForCol(int col) const {
  // Scan the actual main xsheet for child-level columns in order and return
  // the ordinal of the column that matches `col`. Same algorithm the Board
  // uses in refreshFromScene(), so the two stay consistent without relying
  // on m_shots[i].xsheetColumn (which can be stale after Animatic-side ops
  // that don't go through ZtoryModel::addShot/removeShot).
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
    if (c == col) return childIdx;
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
  emit modelReset();
}

void ZtoryModel::updateColumnName(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
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
void ZtoryModel::onSceneChanged()  { refreshFromScene(); load(); }

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
