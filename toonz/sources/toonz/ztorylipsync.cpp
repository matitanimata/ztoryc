#include "ztorylipsync.h"

#include "ztoryvosk.h"
#include "ztoryphonemes.h"
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
#include "tsound.h"
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
#include <QThread>
#include <QRegularExpression>

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
  if (m_voskThread) {
    disconnect(m_voskThread, nullptr, this, nullptr);
    m_voskThread->wait();
    delete m_voskThread;
  }
}

//-----------------------------------------------------------------------------

ZtoryLipSync::Engine ZtoryLipSync::engineFor(const QString &language) {
  // Vosk vuole sapere la lingua: i suoi modelli sono uno per lingua e non c'e'
  // rilevamento automatico. Se il pannello non l'ha detta, il lavoro e' di
  // Whisper, che le riconosce tutte — e li' i tempi valgono meno, ma un tempo
  // approssimato batte un rifiuto.
  if (!language.isEmpty() && ZtoryVosk::isAvailable() &&
      ZtoryVosk::hasLanguage(language))
    return Engine::Vosk;
  return Engine::Whisper;
}

//-----------------------------------------------------------------------------

QString ZtoryLipSync::unavailableReason() {
  // Basta UNO dei due motori. Vosk non ha bisogno di niente di installato: la
  // libreria e i modelli stanno nel bundle.
  if (ZtoryVosk::isAvailable() && !ZtoryVosk::availableLanguages().isEmpty())
    return QString();
  if (!ThirdParty::checkWhisper())
    return tr("Neither Vosk nor whisper.cpp was found. Set whisper.cpp's "
              "folder in Preferences > Import/Export.");
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

namespace {

// Un QThread nudo invece di QtConcurrent: Qt5::Concurrent non e' fra i moduli
// del progetto, e aggiungerlo vorrebbe dire toccare il CMakeLists condiviso con
// Tahoma2D per una riga di codice. Nessun Q_OBJECT qui dentro: si usa il
// segnale finished() che QThread ha gia', quindi moc non serve.
class VoskAlignThread final : public QThread {
public:
  VoskAlignThread(const QString &wav, const QString &lang,
                  const QStringList &vocabulary)
      : m_wav(wav), m_lang(lang), m_vocabulary(vocabulary) {}

  QVector<TimedWord> result;
  QString            error;

protected:
  void run() override {
    result = ZtoryVosk::align(m_wav, m_lang, m_vocabulary, &error);
  }

private:
  QString     m_wav;
  QString     m_lang;
  QStringList m_vocabulary;
};

}  // namespace

//-----------------------------------------------------------------------------

bool ZtoryLipSync::start(const Request &req) {
  if (m_running) return false;
  if (!unavailableReason().isEmpty()) return false;
  if (!TSystem::doesExistFileOrLevel(TFilePath(req.wavPath))) return false;

  m_req = req;
  return engineFor(m_req.language) == Engine::Vosk ? startVosk()
                                                   : startWhisper();
}

//-----------------------------------------------------------------------------

bool ZtoryLipSync::startVosk() {
  // Le parole del copione diventano il vocabolario chiuso del riconoscitore: e'
  // questo che fa di Vosk un ALLINEATORE invece che un trascrittore. Le parole
  // che il modello non conosce le scarta lui, in ZtoryVosk::align().
  // ⚠️ QRegularExpression e NON QRegExp: `\p{L}` e' sintassi PCRE, e QRegExp
  // (il motore vecchio) non la conosce — se la mangia come classe letterale.
  // Con QRegExp questa riga estraeva UNA sola parola, «p», dalla battuta
  // «"Uncinetto Facile"? …no, questo non fa per me!». Peggio: «p» esiste davvero
  // nel vocabolario italiano di Vosk, quindi non veniva scartata, e il
  // riconoscitore restava vincolato a quella sola parola: due parole in colonna
  // invece di otto, con l'aria di un modello troppo piccolo. Misurato il
  // 2026-08-16.
  static const QRegularExpression kNotWord("[^\\p{L}\\p{N}']+");
  QStringList vocabulary;
  for (const DialogueLine &line :
       ZtoryModel::instance()->parseDialogue(m_req.dialogue))
    for (const QString &w : line.text.split(kNotWord, Qt::SkipEmptyParts))
      vocabulary << w.toLower();

  auto *worker = new VoskAlignThread(m_req.wavPath, m_req.language, vocabulary);
  m_voskThread = worker;
  m_running    = true;

  connect(worker, &QThread::finished, this, [this, worker]() {
    m_running    = false;
    m_voskThread = nullptr;
    const QVector<TimedWord> heard = worker->result;
    const QString            err   = worker->error;
    worker->deleteLater();
    if (heard.isEmpty()) {
      emit finished(false, {}, err.isEmpty() ? tr("No words recognised.") : err);
      return;
    }
    completeWith(heard);
  });

  // Il primo uso di una lingua scompatta il modello: qualche secondo in cui
  // senza un avviso l'applicazione sembrerebbe piantata.
  emit progress(ZtoryVosk::hasLanguage(m_req.language)
                    ? tr("Aligning the audio to the dialogue…")
                    : tr("Preparing the %1 model…").arg(m_req.language));
  worker->start();
  return true;
}

//-----------------------------------------------------------------------------

bool ZtoryLipSync::startWhisper() {
  const Request &req = m_req;

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
  if (!m_running) return;
  // Vosk non si interrompe a meta': la sua chiamata e' bloccante e non ha un
  // punto in cui chiedergli di smettere. Si stacca il segnale e si aspetta —
  // sono secondi, non minuti — perche' lasciare il thread vivo mentre l'oggetto
  // muore e' un crash, non un rischio.
  if (m_voskThread) {
    disconnect(m_voskThread, nullptr, this, nullptr);
    m_voskThread->wait();
    m_voskThread->deleteLater();
    m_voskThread = nullptr;
  }
  if (m_proc) m_proc->kill();
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
  completeWith(heard);
}

//-----------------------------------------------------------------------------

namespace {

// Quanto una bocca si vede. Serve a decidere chi cede il posto quando lo spazio
// non basta: le CHIUSURE sono le forme che l'occhio ancora, e una M senza
// chiusura si nota subito. Franco, 2026-08-16: «le bocche chiuse non sono
// sacrificabili, dovrebbero vedersi».
int visibilityRank(const QString &shape) {
  if (shape == "mbp" || shape == "fv") return 0;  // labiali: si vedono sempre
  if (shape == "ai" || shape == "o" || shape == "u" || shape == "e")
    return 1;                                     // aperture
  if (shape == "wq" || shape == "l") return 2;
  return 3;                                       // "etc": la piu' sacrificabile
}

bool isOpenMouth(const QString &shape) { return visibilityRank(shape) == 1; }

//-----------------------------------------------------------------------------
// L'inviluppo di energia, un valore per FOTOGRAMMA, normalizzato a 0..1.
//
// Serve perche' l'ampiezza dell'onda dice DOVE cade davvero un suono, e nessuna
// tabella di durate medie lo sa: quella e' una media di tutte le recitazioni,
// questa e' la recitazione che abbiamo. Idea di Franco, 2026-08-16: «dove e'
// piu' alta dobbiamo prediligere le bocche piu' aperte».
QVector<double> frameEnergy(const QString &wavPath, double fps) {
  QVector<double> out;
  TSoundTrackP st;
  if (!TSoundTrackReader::load(TFilePath(wavPath), st) || !st) return out;
  const double rate    = double(st->getSampleRate());
  const TINT32 total   = st->getSampleCount();
  const int perFrame   = std::max(1, int(rate / (fps > 0 ? fps : 24.0)));
  const int channels   = std::max(1, st->getChannelCount());
  double peak = 0;
  for (TINT32 i = 0; i + perFrame <= total; i += perFrame) {
    double acc = 0;
    for (int k = 0; k < perFrame; k++) {
      double v = st->getPressure(i + k, TSound::LEFT);
      if (channels > 1) v = 0.5 * (v + st->getPressure(i + k, TSound::RIGHT));
      acc += v * v;
    }
    const double rms = std::sqrt(acc / perFrame);
    out.push_back(rms);
    peak = std::max(peak, rms);
  }
  if (peak <= 0) return QVector<double>();
  for (double &v : out) v /= peak;
  return out;
}

// energia al fotogramma f (1-based, come i fotogrammi della sotto-scena)
double energyAt(const QVector<double> &env, int f) {
  const int i = f - 1;
  return (i >= 0 && i < env.size()) ? env[i] : 0.0;
}

//-----------------------------------------------------------------------------
// Rimette in fila le celle di UNA TRACCIA INTERA. Va fatto qui e non parola per
// parola: la colonna e' una sola, e il minimo va imposto su quella.
void normalizeCells(QVector<ZtoryCharacterTrack::Word> &cells, int minFrames) {
  if (cells.size() < 2) return;
  std::sort(cells.begin(), cells.end(),
            [](const ZtoryCharacterTrack::Word &a,
               const ZtoryCharacterTrack::Word &b) {
              return a.startFrame < b.startFrame;
            });
  auto len = [](const ZtoryCharacterTrack::Word &w) {
    return w.endFrame - w.startFrame + 1;
  };
  // 1) nessuna sovrapposizione: ogni cella si ferma prima che cominci la dopo.
  for (int i = 0; i + 1 < cells.size(); i++)
    cells[i].endFrame = std::min(cells[i].endFrame, cells[i + 1].startFrame - 1);

  bool changed = true;
  while (changed) {
    changed = false;
    for (int i = 0; i < cells.size(); i++) {
      if (len(cells[i]) >= minFrames) continue;
      int need = minFrames - len(cells[i]);

      // 2) Prelievo A CASCATA: si cerca il primo che abbia margine, anche
      // lontano, spostando tutti quelli in mezzo. Fermarsi al vicino immediato
      // lasciava celle da un fotogramma pur essendoci spazio cinque celle piu'
      // in la' — succedeva sulla /e/ fra la P di «per» e la M di «me».
      for (int dir = 1; dir >= -1 && need > 0; dir -= 2) {
        for (int j = i + dir; j >= 0 && j < cells.size() && need > 0; j += dir) {
          const int take = std::min(len(cells[j]) - minFrames, need);
          if (take <= 0) continue;
          if (dir > 0) {
            for (int k = i + 1; k <= j; k++) {
              cells[k].startFrame += take;
              cells[k].endFrame += take;
            }
            cells[j].endFrame -= take;
            cells[i].endFrame += take;
          } else {
            for (int k = j + 1; k < i; k++) {
              cells[k].startFrame -= take;
              cells[k].endFrame -= take;
            }
            cells[j].endFrame -= take;
            cells[i].startFrame -= take;
          }
          need -= take;
          changed = true;
        }
      }
      if (need <= 0) continue;

      // 3) Nessuno puo' dare: si scarta. MA non se cosi' due bocche UGUALI e
      // VISIBILI diventano adiacenti: la /e/ fra la P di «per» e la M di «me»
      // sembra sacrificabile, e toglierla fonde le due chiusure in un'unica
      // bocca chiusa — l'animatore vede una tenuta dove devono esserci due
      // colpi. Due "etc" attaccate invece sono la stessa forma generica.
      const bool prevEqNext = i > 0 && i + 1 < cells.size() &&
                              cells[i - 1].text == cells[i + 1].text &&
                              visibilityRank(cells[i - 1].text) <= 1;
      if (prevEqNext) continue;

      if (i > 0)
        cells[i - 1].endFrame =
            std::max(cells[i - 1].endFrame, cells[i].endFrame);
      else if (i + 1 < cells.size())
        cells[i + 1].startFrame = cells[i].startFrame;
      cells.remove(i);
      changed = true;
      break;
    }
  }

  // 4) I BUCHI. Una cella vuota NON e' silenzio: tiene il disegno precedente,
  // quindi la bocca resta com'era invece di chiudersi. Un buco piu' corto del
  // minimo lo assorbe la cella prima (un fotogramma di riposo sfarfalla); da li'
  // in su diventa RIPOSO esplicito.
  QVector<ZtoryCharacterTrack::Word> filled;
  for (int i = 0; i < cells.size(); i++) {
    filled.push_back(cells[i]);
    if (i + 1 >= cells.size()) continue;
    const int gap = cells[i + 1].startFrame - cells[i].endFrame - 1;
    if (gap <= 0) continue;
    if (gap < minFrames) {
      filled.last().endFrame = cells[i + 1].startFrame - 1;
    } else {
      ZtoryCharacterTrack::Word rest;
      rest.text       = ZtoryPhonemes::kRest;
      rest.startFrame = cells[i].endFrame + 1;
      rest.endFrame   = cells[i + 1].startFrame - 1;
      filled.push_back(rest);
    }
  }
  cells = filled;
}

//-----------------------------------------------------------------------------
// Distribuisce i viseme di una parola nel suo intervallo e li converte in
// fotogrammi.
//
// Due criteri, in quest'ordine:
//  1. la durata viene dai pesi di classe (le vocali tengono, le occlusive no);
//  2. la POSIZIONE delle bocche aperte la decide l'ONDA — il centro di ogni
//     vocale si aggancia al massimo di energia vicino.
// Il secondo e' quello che conta davvero: espeak ci da' l'ORDINE dei suoni e
// nessuna informazione su quando cadono, e misurando (2026-08-16) i picchi
// stavano sistematicamente accanto alle vocali, non sopra.
void spreadVisemes(const QVector<ZtoryPhonemes::Viseme> &visemes,
                   const TimedWord &w, double fps, int firstFrame, int lead,
                   const QVector<double> &energy,
                   QVector<ZtoryCharacterTrack::Word> &out) {
  const int kMinFrames = 2;
  const int kLabialLead = 1;  // le chiusure precedono il suono
  const int kSnap = 2;        // quanto lontano si cerca il picco

  const int startFrame =
      std::max(firstFrame,
               firstFrame + int(std::floor(w.startMs * fps / 1000.0)) - lead);
  // -1 perche' la fine e' ESCLUSIVA: senza, una parola si prendeva il primo
  // fotogramma della successiva e la coda le veniva poi sovrascritta.
  const int endFrame = std::max(
      startFrame,
      firstFrame + int(std::ceil(w.endMs * fps / 1000.0)) - lead - 1);
  const int available = endFrame - startFrame + 1;

  QVector<ZtoryPhonemes::Viseme> kept = visemes;
  const int room = std::max(1, available / kMinFrames);
  if (kept.size() > room)
    for (int worst = 3; worst >= 0 && kept.size() > room; worst--)
      for (int i = kept.size() - 1; i >= 0 && kept.size() > room; i--)
        if (visibilityRank(kept[i].shape) == worst) kept.remove(i);
  if (kept.isEmpty()) return;

  const int n = kept.size();
  double total = 0;
  for (const ZtoryPhonemes::Viseme &v : kept) total += v.weight;
  if (total <= 0) total = n;

  // durate per peso, mai sotto il minimo, somma esatta
  // Il minimo a tutti, poi l'avanzo UN FOTOGRAMMA ALLA VOLTA a chi e' piu'
  // sotto il proprio peso. ⚠️ Non per arrotondamento proporzionale: con sei
  // fonemi e tre fotogrammi d'avanzo l'arrotondamento ne dava al massimo uno a
  // testa, e la vocale accentata restava al minimo come le altre. E' cosi' che
  // la /a/ di «facile» ne prendeva 2 invece dei 4 che si sentono.
  QVector<int> take(n, kMinFrames);
  for (int left = available - n * kMinFrames; left > 0; left--) {
    int best = 0;
    double bestRatio = take[0] / std::max(0.01, kept[0].weight);
    for (int i = 1; i < n; i++) {
      const double r = take[i] / std::max(0.01, kept[i].weight);
      if (r < bestRatio) { bestRatio = r; best = i; }
    }
    take[best]++;
  }
  (void)total;

  // confini provvisori
  QVector<int> b(n + 1, startFrame);
  for (int i = 0; i < n; i++) b[i + 1] = b[i] + take[i];

  // AGGANCIO AI PICCHI: il centro di ogni bocca aperta va sul massimo di
  // energia li' attorno, senza mai scavalcare i vicini ne' togliere loro il
  // minimo.
  if (!energy.isEmpty()) {
    for (int i = 0; i < n; i++) {
      if (!isOpenMouth(kept[i].shape)) continue;
      const int lo = b[i], hi = b[i + 1] - 1;
      const int w0 = std::max(startFrame, lo - kSnap);
      const int w1 = std::min(endFrame, hi + kSnap);
      if (w1 < w0) continue;
      int peak = w0;
      for (int f = w0 + 1; f <= w1; f++)
        if (energyAt(energy, f) > energyAt(energy, peak)) peak = f;
      const int shift = peak - (lo + hi) / 2;
      if (shift == 0) continue;
      const int loMin = (i > 0) ? b[i - 1] + kMinFrames : startFrame;
      const int hiMax = (i + 2 <= n) ? b[i + 2] - kMinFrames : endFrame + 1;
      const int nb0 = std::min(std::max(b[i] + shift, loMin), b[i + 1] - kMinFrames);
      const int nb1 = std::max(std::min(b[i + 1] + shift, hiMax), nb0 + kMinFrames);
      b[i]     = nb0;
      b[i + 1] = nb1;
    }
  }

  for (int i = 0; i < n; i++) {
    ZtoryCharacterTrack::Word cell;
    cell.text       = kept[i].shape;
    cell.startFrame = b[i];
    cell.endFrame   = std::max(b[i] + kMinFrames - 1, b[i + 1] - 1);
    if (i == n - 1) cell.endFrame = std::max(cell.endFrame, endFrame);
    if (visibilityRank(cell.text) == 0)
      cell.startFrame = std::max(firstFrame, cell.startFrame - kLabialLead);
    if (cell.endFrame < cell.startFrame) cell.endFrame = cell.startFrame;
    out.push_back(cell);
  }
}

}  // namespace

//-----------------------------------------------------------------------------

void ZtoryLipSync::completeWith(QVector<TimedWord> heard) {
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

  // Anticipo: le bocche si posano PRIMA del suono. Non compensa un errore di
  // misura — sui due attacchi di /f/ misurati il 2026-08-16 lo scarto era +30 e
  // -30 ms, quindi niente ritardo sistematico da correggere — ma riproduce cio'
  // che fanno gli animatori: l'occhio legge la bocca in anticipo, e una bocca
  // esattamente in tempo sembra tarda. Franco lo ha notato sulla /f/ di
  // «Facile», che sentiva un paio di fotogrammi prima di dove era scritta.
  const int lead = std::max(0, Preferences::instance()->getLipSyncLeadFrames());

  // I FONEMI al posto delle parole: nelle celle va la bocca, come nell'x-sheet
  // tradizionale (decisione di Franco, 2026-08-14). Se espeak-ng non c'e' si
  // continua a scrivere le parole intere — una colonna con le parole si legge
  // ancora, una colonna vuota no.
  QStringList plain;
  for (const TimedWord &w : aligned) plain << w.word;
  const QVector<QVector<ZtoryPhonemes::Viseme>> visemes =
      ZtoryPhonemes::forWords(plain, m_req.language, nullptr);
  const bool usePhonemes = visemes.size() == aligned.size();
  // L'inviluppo si legge UNA volta per tutto lo shot: e' lo stesso audio per
  // ogni parola e per ogni personaggio.
  const QVector<double> energy = frameEnergy(m_req.wavPath, fps);

  for (int wi = 0; wi < aligned.size(); wi++) {
    const TimedWord &w = aligned[wi];
    // La parola va nella sua colonna SEMPRE: le due colonne sono indipendenti,
    // e chi legge il foglio deve poter sapere cosa si dice anche quando le
    // bocche ci sono.
    {
      ZtoryCharacterTrack::Word sp;
      sp.text = w.word;
      sp.startFrame = std::max(
          m_req.firstFrame,
          m_req.firstFrame + int(std::floor(w.startMs * fps / 1000.0)) - lead);
      sp.endFrame = std::max(
          sp.startFrame,
          m_req.firstFrame + int(std::ceil(w.endMs * fps / 1000.0)) - lead);
      trackFor(w.assetUuid).spoken.push_back(sp);
    }
    if (usePhonemes && !visemes[wi].isEmpty()) {
      spreadVisemes(visemes[wi], w, fps, m_req.firstFrame, lead, energy,
                    trackFor(w.assetUuid).words);
      continue;
    }
    ZtoryCharacterTrack::Word word;
    word.text = w.word;
    // floor sull'inizio e ceil sulla fine: una parola non deve MAI sparire per
    // arrotondamento, e un fotogramma in piu' si vede molto meno di una bocca
    // che non si apre.
    word.startFrame = m_req.firstFrame + int(std::floor(w.startMs * fps / 1000.0));
    word.endFrame   = m_req.firstFrame + int(std::ceil(w.endMs * fps / 1000.0));
    // Si sposta anche la fine: e' l'intera battuta ad andare in anticipo, non
    // solo il suo attacco. Allungarla soltanto farebbe strascicare le bocche.
    word.startFrame = std::max(m_req.firstFrame, word.startFrame - lead);
    word.endFrame   = std::max(m_req.firstFrame, word.endFrame - lead);
    if (word.endFrame < word.startFrame) word.endFrame = word.startFrame;
    trackFor(w.assetUuid).words.push_back(word);
  }

  for (ZtoryCharacterTrack &t : tracks) {
    normalizeCells(t.words, 2);
    normalizeCells(t.spoken, 1);  // le parole possono durare un fotogramma:
                                  // sono da leggere, non da guardare in moto
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
#include "toonz/tstageobject.h"
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
void writeCellsColumn(TXsheet *xsh, int col,
                      const QVector<ZtoryCharacterTrack::Word> &cells,
                      int lastFrame, bool restWhereSilent,
                      const QString &characterName) {
  // ⚠️ createSoundTextLevel(row, lista) mette lista[i] alla RIGA i, e la riga 0
  // e' il fotogramma 1. I fotogrammi qui sono 1-based, quindi l'indice e'
  // frame-1: scriverlo a `frame` sposta tutto avanti di uno.
  // La colonna si allunga oltre lo shot se una battuta sfora: perdere dialogo
  // per far quadrare una lunghezza e' il contrario di cio' che serve.
  int needed = lastFrame;
  for (const ZtoryCharacterTrack::Word &w : cells)
    needed = std::max(needed, w.endFrame);
  // ⚠️ IL RIPOSO VA SCRITTO OVUNQUE IL PERSONAGGIO TACCIA, non solo fra due sue
  // parole. Una cella vuota TIENE il disegno precedente: con due personaggi in
  // scena, quello che ha finito di parlare resterebbe con l'ultima bocca aperta
  // per tutta la battuta dell'altro. La colonna consegnata all'animatore e' una
  // linea temporale COMPLETA — fonemi dove parla, riposo dove tace — e il
  // silenzio si ricava per complemento, che e' il motivo per cui una traccia
  // audio mixata basta.
  // Non vale per la colonna delle PAROLE: li' il vuoto e' vuoto, e un «rest»
  // scritto fra una battuta e l'altra sarebbe rumore da leggere.
  QList<QString> rows;
  const QString filler =
      restWhereSilent ? QString::fromUtf8(ZtoryPhonemes::kRest) : QString();
  for (int i = 0; i < needed; i++) rows.append(filler);
  for (const ZtoryCharacterTrack::Word &w : cells)
    for (int f = w.startFrame; f <= w.endFrame; f++) {
      const int i = f - 1;
      if (i >= 0 && i < rows.size()) rows[i] = w.text;
    }

  TXshSoundTextColumn *sc = new TXshSoundTextColumn();
  sc->createSoundTextLevel(0, rows);
  xsh->insertColumn(col, sc);

  // ── IL NOME DI CHI PARLA SULLA COLONNA ────────────────────────────────
  // Con due personaggi in scena ci sono quattro colonne di testo, e senza nome
  // sono indistinguibili: applicare il lip sync diventa indovinare quale sia
  // quella giusta (Franco, 2026-08-16). Il nome sta gia' nella traccia — non
  // scriverlo era buttare via l'unica cosa che le distingue.
  if (!characterName.isEmpty())
    if (TStageObject *so = xsh->getStageObject(xsh->getColumnObjectId(col)))
      so->setName((characterName + (restWhereSilent ? QObject::tr(" mouths")
                                                    : QObject::tr(" words")))
                      .toStdString());
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
    // Lingua da Preferenze > Import/Export. Prima si ricavava dall'interruttore
    // «phonetic» di Rhubarb, che pero' e' binario: dava solo "en" o niente, e su
    // un progetto italiano non c'era modo di dire «italiano». Con Vosk la lingua
    // sceglie anche il MOTORE, quindi doveva diventare una scelta esplicita.
    req.language = Preferences::instance()->getLipSyncLanguage();
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
        if (t.assetUuid.isEmpty()) orphanWords += t.spoken.size();
        // Prima le parole, poi le bocche: nel foglio si legge da sinistra, e la
        // battuta viene prima di come la si esegue.
        // Senza personaggio riconosciuto il nome resta vuoto: meglio una
        // colonna anonima che una intestata a un nome inventato.
        const QString who = t.characterName;
        writeCellsColumn(sub, sub->getColumnCount(), t.spoken, lastF, false, who);
        written++;
        if (!t.words.isEmpty()) {
          writeCellsColumn(sub, sub->getColumnCount(), t.words, lastF, true, who);
          written++;
        }
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
