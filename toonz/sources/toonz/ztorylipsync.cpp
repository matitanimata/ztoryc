#include "ztorylipsync.h"

#include "thirdparty.h"
#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txsheet.h"
#include "toonz/childstack.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcell.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"
#include "tsound_io.h"
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "tsystem.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUuid>

#include <cmath>

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

ZtoryShotContext ztoryCurrentShotContext() {
  ZtoryShotContext ctx;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return ctx;
  ChildStack *cs = scene->getChildStack();
  if (!cs) return ctx;
  TXsheet *top = cs->getTopXsheet();
  TXsheet *cur = cs->getXsheet();
  // Al livello principale non c'e' nessuno shot aperto: e' un'informazione, non
  // un errore, e chi chiama la trasforma in un messaggio comprensibile.
  if (!top || !cur || cur == top) return ctx;

  for (int col = 0; col < top->getColumnCount(); col++) {
    // ⚠️ NON si guarda la riga 0. In Ztoryc gli shot stanno in FILA nel tempo:
    // il secondo comincia dove finisce il primo, quindi solo lo shot iniziale
    // ha una cella alla riga 0. Guardando li' si trovava sempre e solo il
    // primo shot, e da ogni altro il comando rispondeva «apri una sotto-scena»
    // stando gia' dentro una sotto-scena.
    int r0 = 0, r1 = 0;
    top->getCellRange(col, r0, r1);
    if (r1 < r0) continue;  // colonna vuota
    TXshChildLevel *cl = nullptr;
    for (int r = r0; r <= r1 && !cl; r++) {
      TXshCell cell = top->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level) cl = cell.m_level->getChildLevel();
    }
    if (!cl || cl->getXsheet() != cur) continue;

    ctx.column    = col;
    ctx.subXsheet = cur;
    ctx.firstRow  = r0;
    ctx.lastRow   = r1;
    const std::vector<ShotData> &shots = ZtoryModel::instance()->shots();
    for (int i = 0; i < (int)shots.size(); i++)
      if (shots[i].xsheetColumn == col) { ctx.shotIndex = i; break; }
    // Ripiego: xsheetColumn puo' essere stale (lo si riscrive solo in certe
    // operazioni). Contare la colonna fra quelle che contengono una sotto-scena
    // da' lo stesso numero senza dipendere da un campo che qualcuno deve aver
    // aggiornato.
    if (ctx.shotIndex < 0) {
      int n = 0;
      for (int c2 = 0; c2 < col; c2++) {
        int a = 0, b = 0;
        top->getCellRange(c2, a, b);
        for (int r = a; r <= b; r++) {
          TXshCell cc = top->getCell(r, c2);
          if (!cc.isEmpty() && cc.m_level && cc.m_level->getChildLevel()) { n++; break; }
        }
      }
      if (n < (int)shots.size()) ctx.shotIndex = n;
    }
    break;
  }
  return ctx;
}

//-----------------------------------------------------------------------------

QString ztoryExtractShotAudio(const ZtoryShotContext &ctx) {
  if (!ctx.isValid()) return QString();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return QString();
  TXsheet *top = scene->getChildStack()->getTopXsheet();
  if (!top) return QString();

  // Tutte le colonne sonore, mixate. Il mix va bene: chi parla lo dice il
  // copione, quindi non serve una pista separata per personaggio.
  std::vector<TXshSoundColumn *> cols;
  for (int c = 0; c < top->getColumnCount(); c++)
    if (TXshSoundColumn *sc = top->getColumn(c)->getSoundColumn())
      cols.push_back(sc);
  if (cols.empty()) return QString();

  const double fps =
      scene->getProperties()->getOutputProperties()->getFrameRate();
  TSoundTrackP st =
      cols[0]->mixingTogether(cols, ctx.firstRow, ctx.lastRow, fps);
  if (!st || st->getSampleCount() == 0) return QString();

  const QString cacheRoot = ToonzFolder::getCacheRootFolder().getQString();
  const QString dir       = cacheRoot + "/whisper";
  if (!TSystem::doesExistFileOrLevel(TFilePath(dir)))
    TSystem::mkDir(TFilePath(dir));
  // Nome per shot e non fisso: due shot analizzati in fila non devono
  // sovrascriversi l'audio a vicenda mentre il processo precedente legge ancora.
  const TFilePath out(dir + QString("/shot_%1.wav").arg(ctx.shotIndex));
  try {
    TSoundTrackWriter::save(out, st);
  } catch (...) {
    return QString();
  }
  // whisper.cpp ricampiona da solo: verificato su 44.1 kHz stereo, quindi non
  // serve passare da ffmpeg.
  return out.getQString();
}

ZtoryLipSync::ZtoryLipSync(QObject *parent) : QObject(parent) {}

ZtoryLipSync::~ZtoryLipSync() {
  if (m_proc) {
    m_proc->kill();
    m_proc->waitForFinished(2000);
  }
}

//-----------------------------------------------------------------------------

QString ZtoryLipSync::unavailableReason() {
  if (!ThirdParty::checkWhisper())
    return tr("whisper.cpp was not found. Set its folder in "
              "Preferences > Import/Export.");
  if (ThirdParty::getWhisperModel().isEmpty())
    return tr("No Whisper model. One ships with Ztoryc; if it is missing, "
              "point Preferences > Import/Export at a .bin model file.");
  return QString();
}

//-----------------------------------------------------------------------------

QVector<TimedWord> ZtoryLipSync::parseWhisperJson(const QByteArray &json,
                                                  QString *error) {
  QVector<TimedWord> out;
  QJsonParseError perr;
  const QJsonDocument doc = QJsonDocument::fromJson(json, &perr);
  if (perr.error != QJsonParseError::NoError) {
    if (error) *error = perr.errorString();
    return out;
  }
  const QJsonArray segs = doc.object().value("transcription").toArray();
  for (const QJsonValue &v : segs) {
    const QJsonObject o   = v.toObject();
    const QString text    = o.value("text").toString().trimmed();
    // Con -ml 1 -sow ogni segmento e' una parola, ma il primo e' spesso vuoto
    // e i vuoti non sono parole: lasciarli dentro sballerebbe l'allineamento.
    if (text.isEmpty()) continue;
    const QJsonObject off = o.value("offsets").toObject();
    TimedWord w;
    w.word       = text;
    w.startMs    = off.value("from").toInt();
    w.endMs      = off.value("to").toInt();
    w.fromScript = false;
    out.push_back(w);
  }
  if (out.isEmpty() && error && error->isEmpty())
    *error = tr("Whisper produced no words.");
  return out;
}

//-----------------------------------------------------------------------------

bool ZtoryLipSync::start(const Request &req) {
  if (m_running) return false;
  if (!unavailableReason().isEmpty()) return false;
  if (!TSystem::doesExistFileOrLevel(TFilePath(req.wavPath))) return false;

  m_req = req;

  // Uscita in cache, non accanto all'audio: la cartella dell'audio puo' essere
  // di sola lettura (asset condivisi su rete), e non e' roba da lasciare in giro.
  const QString cacheRoot = ToonzFolder::getCacheRootFolder().getQString();
  const QString dir       = cacheRoot + "/whisper";
  if (!TSystem::doesExistFileOrLevel(TFilePath(dir)))
    TSystem::mkDir(TFilePath(dir));
  const QString base =
      dir + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
  m_jsonPath = base + ".json";

  QStringList args;
  args << "-m" << ThirdParty::getWhisperModel();
  // -ml 1 con -sow = un segmento per PAROLA. Senza, i segmenti sono frasi e i
  // tempi non bastano per posare i viseme.
  args << "-ml" << "1" << "-sow";
  args << "-oj" << "-of" << base;
  if (!m_req.language.isEmpty()) args << "-l" << m_req.language;
  // Niente stampa di avanzamento. ⚠️ MAI aggiungere `-nt` qui: significa «no
  // timestamps» e distrugge i tempi per parola, che sono l'UNICA cosa per cui
  // stiamo chiamando Whisper. Misurato il 2026-08-15: con `-nt` le parole
  // centrali escono tutte con durata ZERO sullo stesso istante e l'ultima
  // arriva a 30000 ms — la finestra da 30 secondi con cui whisper.cpp riempie
  // l'audio piu' corto. Nella colonna si vedeva una parola lunga cinquanta
  // fotogrammi, quattro sparite e l'ultima al fotogramma 751 di uno shot da 81.
  args << "-np";
  args << m_req.wavPath;

  if (!m_proc) {
    m_proc = new QProcess(this);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) { onProcessDone(code); });
  }
  m_running = true;
  emit progress(tr("Listening to the audio…"));

  const QString exe = ThirdParty::getWhisperDir() + "/whisper-cli";
  // ggml apre i suoi backend (BLAS, Metal, CPU) con dlopen a runtime, e di
  // default li cerca dove sono stati installati — cioe' in Homebrew, che sulla
  // macchina di un utente non c'e'. Senza questa riga il binario imballato
  // resta senza backend proprio dove serve di piu'.
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("GGML_BACKEND_PATH", QFileInfo(exe).absolutePath());
  m_proc->setProcessEnvironment(env);
  m_proc->start(exe, args);
  if (!m_proc->waitForStarted(5000)) {
    m_running = false;
    emit finished(false, {}, tr("Could not start whisper-cli."));
    return false;
  }
  return true;
}

//-----------------------------------------------------------------------------

void ZtoryLipSync::cancel() {
  if (!m_running || !m_proc) return;
  m_proc->kill();
  m_running = false;
}

//-----------------------------------------------------------------------------

void ZtoryLipSync::onProcessDone(int exitCode) {
  m_running = false;
  if (exitCode != 0) {
    // Il messaggio di whisper-cli e' su stderr; senza, l'utente riceve solo un
    // numero. E il 137 (SIGKILL) ha un significato preciso su macOS.
    QString err = QString::fromUtf8(m_proc->readAllStandardError()).trimmed();
    if (exitCode == 137)
      err = tr("whisper-cli was killed by the system (code signature?).");
    emit finished(false, {},
                  err.isEmpty()
                      ? tr("whisper-cli failed (exit %1).").arg(exitCode)
                      : err);
    return;
  }

  QFile f(m_jsonPath);
  if (!f.open(QIODevice::ReadOnly)) {
    emit finished(false, {}, tr("Whisper produced no output file."));
    return;
  }
  QString err;
  QVector<TimedWord> heard = parseWhisperJson(f.readAll(), &err);
  f.close();
  QFile::remove(m_jsonPath);
  if (heard.isEmpty()) {
    emit finished(false, {}, err.isEmpty() ? tr("No words recognised.") : err);
    return;
  }

  // Difesa: whisper.cpp riempie a 30 secondi l'audio piu' corto, e in certe
  // combinazioni di opzioni riporta la coda a fine FINESTRA invece che a fine
  // audio. Una parola oltre la durata vera non e' un dato: e' un artefatto, e
  // lasciandola passare finisce a scrivere centinaia di fotogrammi vuoti.
  if (m_req.audioMs > 0) {
    QVector<TimedWord> clamped;
    for (TimedWord w : heard) {
      if (w.startMs >= m_req.audioMs) continue;  // interamente nel riempimento
      w.endMs = qMin(w.endMs, m_req.audioMs);
      clamped.push_back(w);
    }
    if (!clamped.isEmpty()) heard = clamped;
  }

  emit progress(tr("Matching against the storyboard dialogue…"));

  // Il copione e' la verita' per le PAROLE; Whisper lo e' per i TEMPI.
  const QVector<DialogueLine> spoken =
      ZtoryModel::instance()->parseDialogue(m_req.dialogue);
  const QVector<TimedWord> aligned =
      ZtoryModel::alignToScript(heard, spoken);

  // Millisecondi -> fotogrammi. Da qui in poi i millisecondi non servono piu'.
  const double fps = m_req.fps > 0 ? m_req.fps : 24.0;
  QVector<ZtoryCharacterTrack> tracks;
  auto trackFor = [&](const QString &uuid) -> ZtoryCharacterTrack & {
    for (ZtoryCharacterTrack &t : tracks)
      if (t.assetUuid == uuid) return t;
    ZtoryCharacterTrack t;
    t.assetUuid = uuid;
    for (const Asset &a : ZtoryModel::instance()->assets())
      if (a.uuid == uuid) { t.characterName = a.name; break; }
    tracks.push_back(t);
    return tracks.last();
  };

  for (const TimedWord &w : aligned) {
    ZtoryCharacterTrack::Word word;
    word.text = w.word;
    // floor sull'inizio e ceil sulla fine: una parola non deve MAI sparire per
    // arrotondamento, e un fotogramma in piu' si vede molto meno di una bocca
    // che non si apre.
    word.startFrame = m_req.firstFrame + int(std::floor(w.startMs * fps / 1000.0));
    word.endFrame   = m_req.firstFrame + int(std::ceil(w.endMs * fps / 1000.0));
    if (word.endFrame < word.startFrame) word.endFrame = word.startFrame;
    trackFor(w.assetUuid).words.push_back(word);
  }

  emit finished(true, tracks,
                tr("%1 words across %2 characters.")
                    .arg(aligned.size())
                    .arg(tracks.size()));
}


//=============================================================================
// Il comando: dallo shot aperto alle colonne di testo, una per personaggio.
//=============================================================================

#include "menubarcommandids.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/dvdialog.h"
#include "toonz/txshsoundtextcolumn.h"
#include "toonz/tcolumnhandle.h"
#include "historytypes.h"
#include "tundo.h"

#include <QMainWindow>

#include <algorithm>

namespace {

// Scrive una colonna di testo per un personaggio: i FONEMI dove parla — per ora
// le parole, i fonemi arriveranno con espeak-ng — e il RESTO esplicito dove
// tace.
//
// ⚠️ Il riposo va SCRITTO, non lasciato vuoto. Una cella vuota tiene l'ultimo
// disegno: il personaggio resterebbe con la bocca aperta a meta' parola per
// tutta la battuta dell'altro. Il silenzio e' un dato, non un'assenza.
void writeCharacterColumn(TXsheet *xsh, int col,
                          const ZtoryCharacterTrack &track, int lastFrame) {
  // ⚠️ createSoundTextLevel(row, lista) mette lista[i] alla RIGA i, e la riga 0
  // e' il fotogramma 1. I fotogrammi qui sono 1-based, quindi l'indice e'
  // frame-1: scriverlo a `frame` sposta tutto avanti di uno.
  // La colonna si allunga oltre lo shot se una battuta sfora: perdere dialogo
  // per far quadrare una lunghezza e' il contrario di cio' che serve.
  int needed = lastFrame;
  for (const ZtoryCharacterTrack::Word &w : track.words)
    needed = std::max(needed, w.endFrame);
  QList<QString> cells;
  for (int i = 0; i < needed; i++) cells.append(QString());
  for (const ZtoryCharacterTrack::Word &w : track.words)
    for (int f = w.startFrame; f <= w.endFrame; f++) {
      const int i = f - 1;
      if (i >= 0 && i < cells.size()) cells[i] = w.text;
    }

  TXshSoundTextColumn *sc = new TXshSoundTextColumn();
  sc->createSoundTextLevel(0, cells);
  xsh->insertColumn(col, sc);
}

class ZtoryLipSyncCommand final : public MenuItemHandler {
public:
  ZtoryLipSyncCommand() : MenuItemHandler(MI_ZtoryLipSyncShot) {}

  void execute() override {
    const QString why = ZtoryLipSync::unavailableReason();
    if (!why.isEmpty()) { DVGui::warning(why); return; }

    const ZtoryShotContext ctx = ztoryCurrentShotContext();
    if (!ctx.isValid()) {
      DVGui::warning(QObject::tr(
          "Open a shot's sub-scene first: the dialogue and the audio are read "
          "from that shot."));
      return;
    }

    // Il copione: tutti i pannelli dello shot, nell'ordine in cui stanno.
    ZtoryModel *m = ZtoryModel::instance();
    QStringList text;
    for (const PanelData &p : m->shot(ctx.shotIndex).panels)
      if (!p.dialog.trimmed().isEmpty()) text << p.dialog.trimmed();
    if (text.isEmpty()) {
      DVGui::warning(QObject::tr(
          "This shot has no dialogue in its storyboard panels. The words come "
          "from there — Whisper only supplies the timing."));
      return;
    }

    const QString wav = ztoryExtractShotAudio(ctx);
    if (wav.isEmpty()) {
      DVGui::warning(QObject::tr(
          "No audio over this shot in the main xsheet."));
      return;
    }

    ZtoryLipSync::Request req;
    req.wavPath  = wav;
    req.dialogue = text.join("\n\n");
    // Si riusa la scelta «Dialogue language» del pannello Rhubarb invece di
    // aggiungerne una seconda: e' la stessa domanda, e due interruttori per la
    // stessa cosa finiscono per contraddirsi. «Altra lingua» diventa qui
    // rilevamento automatico, perche' Whisper le conosce tutte e noi non
    // abbiamo un elenco da far scegliere.
    req.language = Preferences::instance()->isLipSyncPhonetic() ? QString()
                                                                : QString("en");
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    req.fps = scene->getProperties()->getOutputProperties()->getFrameRate();
    // La sotto-scena parte dal fotogramma 1, e l'audio estratto parte
    // dall'inizio dello shot: i due zeri coincidono.
    req.firstFrame = 1;
    // Durata vera dell'audio, dalle righe dello shot: e' il metro con cui si
    // scarta la coda inventata dal riempimento a 30 secondi.
    req.audioMs = int((ctx.lastRow - ctx.firstRow + 1) * 1000.0 / req.fps);

    auto *job = new ZtoryLipSync(TApp::instance()->getMainWindow());
    const int shotIdx = ctx.shotIndex;
    TXsheet *sub      = ctx.subXsheet;
    const int lastF   = ctx.lastRow - ctx.firstRow + 1;

    QObject::connect(job, &ZtoryLipSync::finished, job,
                     [job, sub, lastF](bool ok,
                                       const QVector<ZtoryCharacterTrack> &tracks,
                                       const QString &msg) {
      job->deleteLater();
      if (!ok) { DVGui::warning(msg); return; }

      TUndoManager::manager()->beginBlock();
      int written = 0, orphanWords = 0;
      for (const ZtoryCharacterTrack &t : tracks) {
        // Le parole senza personaggio NON spariscono piu' in silenzio: prima
        // l'intera traccia veniva saltata, e chi guardava vedeva mancare
        // proprio le battute dell'altro personaggio senza sapere perche'.
        // Si scrive comunque la colonna e si dice quante parole erano orfane:
        // quasi sempre e' un nome che il progetto non conosce.
        if (t.assetUuid.isEmpty()) orphanWords += t.words.size();
        writeCharacterColumn(sub, sub->getColumnCount(), t, lastF);
        written++;
      }
      TUndoManager::manager()->endBlock();
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      QString extra;
      if (orphanWords > 0)
        extra = QObject::tr(
                    "  —  %1 words have no character: check the names in the "
                    "dialogue (they turn green when recognised).")
                    .arg(orphanWords);
      DVGui::info(QObject::tr("%1  —  %2 dialogue columns written.")
                      .arg(msg).arg(written) + extra);
    });
    job->start(req);
  }
} ztoryLipSyncCommand;

}  // namespace
