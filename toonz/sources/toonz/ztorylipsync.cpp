#include "ztorylipsync.h"

#include "thirdparty.h"
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
  // Niente stampa a video: la sola uscita che ci interessa e' il JSON.
  args << "-np" << "-nt";
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
  const QVector<TimedWord> heard = parseWhisperJson(f.readAll(), &err);
  f.close();
  QFile::remove(m_jsonPath);
  if (heard.isEmpty()) {
    emit finished(false, {}, err.isEmpty() ? tr("No words recognised.") : err);
    return;
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
