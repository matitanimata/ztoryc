#include "ztoryvosk.h"

#include "toonz/toonzfolders.h"
#include "tsound.h"
#include "tsound_io.h"
#include "tsop.h"
#include "tsystem.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QMutex>
#include <QMutexLocker>

namespace {

//-----------------------------------------------------------------------------
// La C API di Vosk, presa a runtime. Le firme sono copiate da src/vosk_api.h
// (Apache-2.0); non si include il loro header per non portare in repo sorgenti
// di terzi per dodici dichiarazioni.
//-----------------------------------------------------------------------------
struct VoskModel;
struct VoskRecognizer;

struct Api {
  VoskModel *(*model_new)(const char *)                                = nullptr;
  void (*model_free)(VoskModel *)                                      = nullptr;
  int (*model_find_word)(VoskModel *, const char *)                    = nullptr;
  VoskRecognizer *(*rec_new)(VoskModel *, float)                       = nullptr;
  VoskRecognizer *(*rec_new_grm)(VoskModel *, float, const char *)     = nullptr;
  void (*rec_set_words)(VoskRecognizer *, int)                         = nullptr;
  int (*rec_accept_s)(VoskRecognizer *, const short *, int)            = nullptr;
  const char *(*rec_final)(VoskRecognizer *)                           = nullptr;
  void (*rec_free)(VoskRecognizer *)                                   = nullptr;
  void (*set_log_level)(int)                                           = nullptr;
  bool ok = false;
};

QString g_loadError;

Api &api() {
  static Api a;
  static bool tried = false;
  if (tried) return a;
  tried = true;

  // Prima nel bundle, poi nel sistema: stesso ordine di ffmpeg e whisper, cosi'
  // l'app imballata non dipende da cosa c'e' installato sulla macchina.
  QStringList candidates;
#ifdef MACOSX
  candidates << QCoreApplication::applicationDirPath() +
                    "/../Resources/vosk/libvosk.dylib";
#elif defined(_WIN32)
  candidates << QCoreApplication::applicationDirPath() + "/vosk/libvosk.dll";
#else
  candidates << QCoreApplication::applicationDirPath() + "/vosk/libvosk.so";
#endif
  candidates << "vosk";  // lasciato al caricatore di sistema

  QLibrary *lib = nullptr;
  for (const QString &c : candidates) {
    auto *l = new QLibrary(c);
    if (l->load()) {
      lib = l;
      break;
    }
    g_loadError = l->errorString();
    delete l;
  }
  if (!lib) return a;

  auto sym = [&](const char *n) { return lib->resolve(n); };
  a.model_new = (decltype(a.model_new))sym("vosk_model_new");
  a.model_free = (decltype(a.model_free))sym("vosk_model_free");
  a.model_find_word = (decltype(a.model_find_word))sym("vosk_model_find_word");
  a.rec_new = (decltype(a.rec_new))sym("vosk_recognizer_new");
  a.rec_new_grm = (decltype(a.rec_new_grm))sym("vosk_recognizer_new_grm");
  a.rec_set_words = (decltype(a.rec_set_words))sym("vosk_recognizer_set_words");
  a.rec_accept_s =
      (decltype(a.rec_accept_s))sym("vosk_recognizer_accept_waveform_s");
  a.rec_final = (decltype(a.rec_final))sym("vosk_recognizer_final_result");
  a.rec_free = (decltype(a.rec_free))sym("vosk_recognizer_free");
  a.set_log_level = (decltype(a.set_log_level))sym("vosk_set_log_level");

  a.ok = a.model_new && a.model_free && a.rec_new && a.rec_new_grm &&
         a.rec_set_words && a.rec_accept_s && a.rec_final && a.rec_free;
  if (!a.ok)
    g_loadError = QObject::tr("libvosk found but incomplete (wrong version?).");
  else if (a.set_log_level)
    a.set_log_level(-1);  // i log di Kaldi vanno su stderr e sono un fiume
  return a;
}

//-----------------------------------------------------------------------------
// Percorsi
//-----------------------------------------------------------------------------

QString packedDir() {
#ifdef MACOSX
  return QCoreApplication::applicationDirPath() + "/../Resources/vosk";
#else
  return QCoreApplication::applicationDirPath() + "/vosk";
#endif
}

QString packedArchive(const QString &lang) {
  return packedDir() + "/" + lang + ".zvosk";
}

// I modelli scompattati vivono in cache, non nel bundle: il bundle e' di sola
// lettura una volta firmato, e su macOS scriverci dentro invalida la firma.
QString unpackedDir(const QString &lang) {
  return ToonzFolder::getCacheRootFolder().getQString() + "/vosk/" + lang;
}

//-----------------------------------------------------------------------------
// Scompattamento
//
// Si scrive in una cartella temporanea e si rinomina alla fine: se il processo
// muore a meta' (o l'utente chiude), non resta un modello monco che poi
// fallirebbe per sempre senza che nessuno capisca perche'.
//-----------------------------------------------------------------------------
bool unpack(const QString &archive, const QString &destDir, QString *error) {
  QFile f(archive);
  if (!f.open(QIODevice::ReadOnly)) {
    if (error) *error = QObject::tr("Cannot open %1").arg(archive);
    return false;
  }
  QDataStream in(&f);
  in.setByteOrder(QDataStream::BigEndian);

  char magic[7] = {0};
  if (in.readRawData(magic, 7) != 7 || qstrncmp(magic, "ZVOSK1\n", 7) != 0) {
    if (error) *error = QObject::tr("%1 is not a Vosk model archive").arg(archive);
    return false;
  }

  const QString tmpDir = destDir + ".part";
  QDir(tmpDir).removeRecursively();
  if (!QDir().mkpath(tmpDir)) {
    if (error) *error = QObject::tr("Cannot create %1").arg(tmpDir);
    return false;
  }

  quint32 count = 0;
  in >> count;
  for (quint32 i = 0; i < count; i++) {
    quint32 pathLen = 0;
    in >> pathLen;
    QByteArray rel(pathLen, 0);
    if (in.readRawData(rel.data(), pathLen) != int(pathLen)) {
      if (error) *error = QObject::tr("Truncated archive");
      QDir(tmpDir).removeRecursively();
      return false;
    }
    quint32 blobLen = 0;
    in >> blobLen;
    QByteArray blob(blobLen, 0);
    if (in.readRawData(blob.data(), blobLen) != int(blobLen)) {
      if (error) *error = QObject::tr("Truncated archive");
      QDir(tmpDir).removeRecursively();
      return false;
    }

    if (blobLen < 4) {
      if (error) *error = QObject::tr("Truncated archive");
      QDir(tmpDir).removeRecursively();
      return false;
    }
    // I primi quattro byte del blocco sono la lunghezza attesa (formato di
    // qCompress): si confronta con quella ottenuta invece di guardare se il
    // risultato e' vuoto. ⚠️ Il modello contiene un file di ZERO byte
    // (ivector/online_cmvn.conf) e Vosk pretende che esista: trattare "vuoto"
    // come "corrotto" faceva fallire lo scompattamento dell'intero modello su
    // un file che era giusto.
    const quint32 expected = (quint32(quint8(blob[0])) << 24) |
                             (quint32(quint8(blob[1])) << 16) |
                             (quint32(quint8(blob[2])) << 8) |
                             quint32(quint8(blob[3]));
    const QByteArray data = qUncompress(blob);
    if (quint32(data.size()) != expected) {
      if (error)
        *error = QObject::tr("Corrupted archive entry: %1")
                     .arg(QString::fromUtf8(rel));
      QDir(tmpDir).removeRecursively();
      return false;
    }

    const QString out = tmpDir + "/" + QString::fromUtf8(rel);
    QDir().mkpath(QFileInfo(out).absolutePath());
    QFile o(out);
    if (!o.open(QIODevice::WriteOnly) || o.write(data) != data.size()) {
      if (error) *error = QObject::tr("Cannot write %1").arg(out);
      QDir(tmpDir).removeRecursively();
      return false;
    }
  }

  QDir(destDir).removeRecursively();
  if (!QDir().rename(tmpDir, destDir)) {
    if (error) *error = QObject::tr("Cannot finalise %1").arg(destDir);
    QDir(tmpDir).removeRecursively();
    return false;
  }
  return true;
}

//-----------------------------------------------------------------------------
// I modelli restano caricati: aprirne uno costa circa un secondo, e chi fa il
// lip sync di uno shot dopo l'altro lo pagherebbe ogni volta.
//-----------------------------------------------------------------------------
QMutex g_modelMutex;
QHash<QString, VoskModel *> g_models;

VoskModel *modelFor(const QString &lang, QString *error) {
  QMutexLocker lock(&g_modelMutex);
  auto it = g_models.find(lang);
  if (it != g_models.end()) return it.value();

  const QString dir = unpackedDir(lang);
  VoskModel *m = api().model_new(dir.toUtf8().constData());
  if (!m) {
    if (error)
      *error = QObject::tr("Vosk could not load the %1 model.").arg(lang);
    return nullptr;
  }
  g_models.insert(lang, m);
  return m;
}

//-----------------------------------------------------------------------------
// Audio: Vosk vuole 16 kHz, mono, interi a 16 bit. Non ricampiona da solo, e
// dandogli 44.1 kHz stereo non protesta: riconosce male e basta, che e' il modo
// peggiore di sbagliare.
//-----------------------------------------------------------------------------
bool isWhatVoskWants(const TSoundTrackP &st) {
  if (!st) return false;
  const TSoundTrackFormat f = st->getFormat();
  return f.m_sampleRate == 16000 && f.m_channelCount == 1 &&
         f.m_bitPerSample == 16 && f.m_sampleType == TSound::INT;
}

QString describe(const TSoundTrackP &st) {
  if (!st) return QObject::tr("nothing");
  const TSoundTrackFormat f = st->getFormat();
  return QString("%1 Hz, %2 ch, %3 bit")
      .arg(f.m_sampleRate)
      .arg(f.m_channelCount)
      .arg(f.m_bitPerSample);
}

TSoundTrackP loadAs16kMono(const QString &wavPath, QString *error) {
  TSoundTrackP st;
  if (!TSoundTrackReader::load(TFilePath(wavPath), st) || !st) {
    if (error) *error = QObject::tr("Cannot read %1").arg(wavPath);
    return TSoundTrackP();
  }
  if (isWhatVoskWants(st)) return st;

  // Due passi espliciti invece di una TSop::convert() sola. L'audio di scena
  // arriva a 48 kHz e 24 bit, e chiedere rate+bit+canali in un colpo solo manda
  // TSop per una strada interna diversa a seconda del formato di partenza.
  // Separarli lascia poco spazio all'imprevisto, e soprattutto rende chiaro
  // QUALE dei due non ha consegnato.
  if (st->getSampleRate() != 16000) {
    TSoundTrackP r = TSop::resample(st, 16000);
    if (r) st = r;
  }
  if (!isWhatVoskWants(st)) {
    TSoundTrackFormat want(16000, 16, 1, TSound::INT);
    TSoundTrackP c = TSop::convert(st, want);
    if (c) st = c;
  }

  // ⚠️ La verifica NON e' una cintura di sicurezza, e' la parte che conta.
  // Vosk crede a cio' che gli si dichiara: dandogli 48 kHz a 24 bit spacciati
  // per 16 kHz a 16 bit non protesta — riconosce due parole su otto e sbaglia i
  // tempi, cioe' fallisce nel modo che somiglia di piu' a «serve un modello piu'
  // grande». E' successo davvero il 2026-08-16. Meglio un errore che lo dice.
  if (!isWhatVoskWants(st)) {
    if (error)
      *error = QObject::tr("Could not convert the audio to 16 kHz mono 16-bit "
                           "(got %1). Alignment needs it.")
                   .arg(describe(st));
    return TSoundTrackP();
  }
  return st;
}

}  // namespace

//=============================================================================

namespace ZtoryVosk {

bool isAvailable() { return api().ok; }

QString unavailableReason() {
  if (api().ok) return QString();
  return g_loadError.isEmpty()
             ? QObject::tr("The Vosk library was not found.")
             : QObject::tr("Vosk could not be loaded: %1").arg(g_loadError);
}

//-----------------------------------------------------------------------------

QStringList availableLanguages() {
  QStringList out;
  for (const QFileInfo &fi :
       QDir(packedDir()).entryInfoList(QStringList() << "*.zvosk", QDir::Files))
    out << fi.completeBaseName();
  // Anche quelle gia' scompattate: se un domani si potranno scaricare, saranno
  // qui e non nel bundle.
  for (const QFileInfo &fi :
       QDir(ToonzFolder::getCacheRootFolder().getQString() + "/vosk")
           .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
    if (!out.contains(fi.fileName())) out << fi.fileName();
  out.sort();
  return out;
}

bool hasLanguage(const QString &lang) {
  return QFile::exists(packedArchive(lang)) ||
         QFile::exists(unpackedDir(lang) + "/am/final.mdl");
}

//-----------------------------------------------------------------------------

bool prepareLanguage(const QString &lang, QString *error) {
  if (lang.isEmpty()) {
    if (error) *error = QObject::tr("No language given.");
    return false;
  }
  // `am/final.mdl` e non la sola cartella: una directory vuota o monca
  // esisterebbe lo stesso, e il modello fallirebbe piu' avanti con un messaggio
  // che non dice niente.
  if (QFile::exists(unpackedDir(lang) + "/am/final.mdl")) return true;

  const QString archive = packedArchive(lang);
  if (!QFile::exists(archive)) {
    if (error)
      *error = QObject::tr("No Vosk model for language '%1'.").arg(lang);
    return false;
  }
  QDir().mkpath(ToonzFolder::getCacheRootFolder().getQString() + "/vosk");
  return unpack(archive, unpackedDir(lang), error);
}

//-----------------------------------------------------------------------------

QVector<TimedWord> align(const QString &wavPath, const QString &lang,
                         const QStringList &scriptWords, QString *error) {
  QVector<TimedWord> out;
  if (!api().ok) {
    if (error) *error = unavailableReason();
    return out;
  }
  if (!prepareLanguage(lang, error)) return out;

  VoskModel *model = modelFor(lang, error);
  if (!model) return out;

  TSoundTrackP st = loadAs16kMono(wavPath, error);
  if (!st) return out;

  // Il vocabolario chiuso e' cio' che trasforma il riconoscitore in un
  // allineatore. Le parole che il modello non conosce si tolgono dal VINCOLO:
  // lasciarle dentro fa rifiutare la grammatica intera, e un nome di fantasia
  // nel copione non deve far cadere l'allineamento di tutta la battuta.
  QStringList distinct, known;
  if (api().model_find_word) {
    for (const QString &w : scriptWords) {
      const QString lw = w.toLower().trimmed();
      if (lw.isEmpty() || distinct.contains(lw)) continue;
      distinct << lw;
      if (api().model_find_word(model, lw.toUtf8().constData()) >= 0)
        known << lw;
    }
  }

  // ⚠️ Un vocabolario quasi vuoto e' PEGGIO di nessun vocabolario: il
  // riconoscitore resta inchiodato alle poche parole rimaste e restituisce
  // quasi niente. Il riconoscimento libero, su questo materiale, e' gia' molto
  // buono — quindi il vincolo si applica solo quando copre davvero il parlato.
  // Non e' teoria: il 2026-08-16 un errore nell'estrazione delle parole ha
  // ridotto il vocabolario a una parola sola, e il risultato somigliava a un
  // modello troppo piccolo invece che a un vocabolario rotto.
  if (known.size() < 2 || known.size() * 2 < distinct.size()) known.clear();

  VoskRecognizer *rec = nullptr;
  if (!known.isEmpty()) {
    QJsonArray arr;
    for (const QString &w : known) arr.append(w);
    arr.append("[unk]");  // senza, e' costretto a mappare su una parola nota
                          // anche il rumore, e i tempi ne risentono
    const QByteArray grm =
        QJsonDocument(arr).toJson(QJsonDocument::Compact);
    rec = api().rec_new_grm(model, 16000.0f, grm.constData());
  }
  // Niente grammatica utilizzabile (copione vuoto, o tutte parole sconosciute):
  // riconoscimento libero. Il copione rientra comunque a valle, in
  // ZtoryModel::alignToScript(), che sulle PAROLE ha l'ultima parola.
  if (!rec) rec = api().rec_new(model, 16000.0f);
  if (!rec) {
    if (error) *error = QObject::tr("Vosk could not start the recogniser.");
    return out;
  }
  api().rec_set_words(rec, 1);

  const short *samples = reinterpret_cast<const short *>(st->getRawData());
  const int total      = int(st->getSampleCount());
  const int chunk      = 8000;  // mezzo secondo
  for (int i = 0; i < total; i += chunk)
    api().rec_accept_s(rec, samples + i, qMin(chunk, total - i));

  const QByteArray json = QByteArray(api().rec_final(rec));
  api().rec_free(rec);

  const QJsonArray words =
      QJsonDocument::fromJson(json).object().value("result").toArray();
  for (const QJsonValue &v : words) {
    const QJsonObject o = v.toObject();
    TimedWord tw;
    tw.word       = o.value("word").toString();
    tw.startMs    = int(o.value("start").toDouble() * 1000.0);
    tw.endMs      = int(o.value("end").toDouble() * 1000.0);
    tw.fromScript = false;  // il copione lo rimettera' alignToScript()
    if (tw.word.isEmpty()) continue;
    out.push_back(tw);
  }
  if (out.isEmpty() && error) *error = QObject::tr("No words recognised.");
  return out;
}

}  // namespace ZtoryVosk
