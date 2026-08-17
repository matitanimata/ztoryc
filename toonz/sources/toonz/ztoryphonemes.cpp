#include "ztoryphonemes.h"

#include "tsystem.h"

#include "tenv.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

namespace ZtoryPhonemes {
const char *kRest = "rest";
}

namespace {

const char *kEtc = "etc";  // tutte le consonanti senza una bocca propria

//-----------------------------------------------------------------------------
// IPA -> casella. Solo i suoni che hanno una bocca DISTINTA: il resto va in
// "etc", ed e' giusto cosi' — Preston Blair ha dieci disegni, non quaranta, e
// l'occhio a 25 fotogrammi al secondo non distingue una /t/ da una /s/.
//-----------------------------------------------------------------------------
QHash<QChar, QString> buildMap() {
  QHash<QChar, QString> m;
  auto put = [&m](const char *chars, const char *shape) {
    for (const QChar &c : QString::fromUtf8(chars)) m.insert(c, shape);
  };
  // Vocali. ⚠️ Lo SCHWA `ə` non e' un dettaglio: e' la vocale piu' frequente di
  // inglese, francese, tedesco, olandese e catalano (in catalano cinque volte
  // in una frase). Senza, prendeva la bocca da consonante.
  put("aɑæʌɐɶ", "ai");        // spalancata
  put("eɛiɪjəɘɜɞɨɯɪ̈", "e");   // media, piu' larga che alta (schwa compreso)
  put("oɔɒøœɵ", "o");         // tonda aperta (con le arrotondate anteriori)
  put("uʊyʏʉ", "u");          // tonda stretta
  // Consonanti con una bocca RICONOSCIBILE. Le altre stanno bene in "etc":
  // l'occhio a 25 fotogrammi non distingue una /t/ da una /s/.
  put("fv", "fv");            // incisivi sul labbro
  put("ʋ", "fv");             // approssimante labiodentale (olandese)
  put("lʎɫ", "l");            // lingua alzata
  put("θð", "l");             // interdentali: la lingua si VEDE fra i denti
  put("mbpɱβ", "mbp");        // labbra chiuse (β = fricativa bilabiale, spagnolo)
  put("wʍɥ", "wq");           // bacio
  return m;
}

const QHash<QChar, QString> &visemeMap() {
  static const QHash<QChar, QString> m = buildMap();
  return m;
}

double weightOf(const QString &shape) {
  // Quanto tiene ogni bocca rispetto alle altre. Le vocali portano la forma e
  // restano; le occlusive sono un istante. Sono proporzioni, non millisecondi.
  // Le VOCALI portano la forma e devono respirare; le consonanti sono
  // transizioni e il minimo di due fotogrammi gia' le rende leggibili. Questi
  // valori sono quelli collaudati il 2026-08-16 sulla battuta di sh020, con
  // l'accento di espeak che li moltiplica (vedi parseIpa).
  static const QHash<QString, double> w = {
      {"ai", 2.4}, {"o", 2.2},   {"e", 1.9},  {"u", 1.9}, {"fv", 1.0},
      {"l", 0.8},  {"mbp", 0.6}, {"wq", 1.0}, {"etc", 0.6}};
  return w.value(shape, 0.6);
}

QString espeakExe() {
  QStringList folders;
#ifdef MACOSX
  folders << QCoreApplication::applicationDirPath() + "/../Resources/espeak";
#endif
  folders << QCoreApplication::applicationDirPath() + "/espeak";
  // ⚠️ Nel pacchetto Linux l'applicazione gira dentro un'AppImage montata:
  // applicationDirPath() e' il montaggio, non la cartella portatile dove
  // stanno gli strumenti. Senza questa riga espeak-ng imballato non verrebbe
  // MAI trovato su Linux — e il difetto si vedrebbe solo come colonne con le
  // parole intere invece delle bocche. Stessa lista di ffmpeg.
  folders << TEnv::getWorkingDirectory().getQString() + "/espeak";
#ifndef _WIN32
  // Homebrew su Apple Silicon NON e' nel PATH dei processi lanciati dal Finder:
  // senza questa riga l'app non lo trova mai pur essendo installato. Stessa
  // trappola gia' pagata con whisper-cli.
  folders << "/opt/homebrew/bin" << "/usr/local/bin" << "/usr/bin";
  const QString exe = "espeak-ng";
#else
  const QString exe = "espeak-ng.exe";
#endif
  for (const QString &f : folders)
    if (QFile::exists(f + "/" + exe)) return f + "/" + exe;
  return QString();
}

//-----------------------------------------------------------------------------
// Spezza una trascrizione IPA nella sequenza di caselle.
//-----------------------------------------------------------------------------
QVector<ZtoryPhonemes::Viseme> parseIpa(const QString &ipa) {
  // ⚠️ L'ACCENTO NON E' PUNTEGGIATURA. `ˈ` e `ˌ` dicono che la vocale che segue
  // dura di piu' — in italiano parecchio — ed e' informazione che avevamo gia'
  // e buttavamo via con i separatori. Franco, 2026-08-16: la /a/ di «facile» la
  // sentiva distintamente di 4 fotogrammi e ne riceveva 2; e' accentata
  // (`fˈatʃile`), e bastava non ignorarlo.
  static const QChar kStress1 = QString::fromUtf8("ˈ").at(0);
  static const QChar kStress2 = QString::fromUtf8("ˌ").at(0);
  static const QString kModifiers = QString::fromUtf8(".‿ˑ|-");
  static const QChar kLong        = QString::fromUtf8("ː").at(0);
  static const QStringList kDigraphs = {
      QString::fromUtf8("tʃ"), QString::fromUtf8("dʒ"),
      QString::fromUtf8("ts"), QString::fromUtf8("dz")};

  QVector<ZtoryPhonemes::Viseme> out;
  double stress = 1.0;  // vale per il PROSSIMO fonema, poi si azzera
  for (int i = 0; i < ipa.size();) {
    const QChar c = ipa.at(i);
    if (c == kStress1) { stress = 2.5; i++; continue; }
    if (c == kStress2) { stress = 1.5; i++; continue; }
    if (c.isSpace() || kModifiers.contains(c)) { i++; continue; }
    // La geminata allunga il suono PRECEDENTE invece di aggiungerne uno: in
    // «uncinetto» la doppia t e' una t lunga, non due bocche.
    if (c == kLong) {
      if (!out.isEmpty()) out.last().weight *= 1.8;
      i++;
      continue;
    }
    // ⚠️ CIO' CHE NON E' UN SUONO. Si scarta per CATEGORIA Unicode, non con un
    // elenco scritto a mano: fuori dall'italiano l'IPA di espeak si riempie di
    // roba che non e' un fonema, e ognuna diventava un cambio di bocca dal
    // nulla. Misurato il 2026-08-16 su venti lingue:
    //   ʲ  palatalizzazione — OTTO volte in una frase russa
    //   ̃ ̞ ̪ ̝ ̊  diacritici combinanti (nasali francesi e portoghesi)
    //   1 2 5 6  i TONI di vietnamita e cinese, che espeak scrive come cifre
    //   ( )  il giapponese ne emetteva sedici per frase
    // Sono modificatori del suono precedente o annotazioni: mai bocche.
    if (c.isDigit() || c.isPunct() || c.isSymbol() ||
        c.category() == QChar::Mark_NonSpacing ||
        c.category() == QChar::Mark_SpacingCombining ||
        c.category() == QChar::Letter_Modifier) {
      i++;
      continue;
    }
    // Affricate: due caratteri, un suono solo.
    bool digraph = false;
    for (const QString &d : kDigraphs)
      if (ipa.midRef(i, 2) == d) {
        out.push_back({kEtc, weightOf(kEtc) * stress});
        stress = 1.0;
        i += 2;
        digraph = true;
        break;
      }
    if (digraph) continue;

    const QString shape = visemeMap().value(c, kEtc);
    out.push_back({shape, weightOf(shape) * stress});
    stress = 1.0;
    i++;
  }
  return out;
}

QString runEspeak(const QString &exe, const QString &lang,
                  const QString &stdinText, QString *error) {
  QProcess p;
  // ⚠️ ESPEAK_DATA_PATH, o il binario imballato non pronuncia NIENTE.
  // espeak-ng cerca `espeak-ng-data` nel prefisso in cui e' stato installato
  // quando lo si e' costruito — un percorso che sulla macchina dell'utente non
  // esiste. I dati viaggiano accanto all'eseguibile e glielo diciamo qui.
  // E' la stessa trappola di GGML_BACKEND_PATH per whisper-cli, e fallisce
  // allo stesso modo: il processo parte e restituisce una riga vuota.
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  const QString dataParent = QFileInfo(exe).absolutePath();
  if (QDir(dataParent + "/espeak-ng-data").exists())
    env.insert("ESPEAK_DATA_PATH", dataParent);
  p.setProcessEnvironment(env);
  p.start(exe, QStringList() << "-v" << lang << "-q" << "--ipa");
  if (!p.waitForStarted(5000)) {
    if (error) *error = QObject::tr("Could not start espeak-ng.");
    return QString();
  }
  p.write(stdinText.toUtf8());
  p.closeWriteChannel();
  if (!p.waitForFinished(15000)) {
    p.kill();
    if (error) *error = QObject::tr("espeak-ng did not answer.");
    return QString();
  }
  return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

}  // namespace

//=============================================================================

namespace ZtoryPhonemes {

bool isAvailable() { return !espeakExe().isEmpty(); }

QString unavailableReason() {
  if (isAvailable()) return QString();
  return QObject::tr(
      "espeak-ng was not found, so the columns hold whole words instead of "
      "mouth shapes. Install it (brew install espeak-ng) for phonemes.");
}

//-----------------------------------------------------------------------------

QVector<QVector<Viseme>> forWords(const QStringList &words,
                                  const QString &language, QString *error) {
  QVector<QVector<Viseme>> out;
  const QString exe = espeakExe();
  if (exe.isEmpty() || words.isEmpty()) {
    if (error) *error = unavailableReason();
    return out;
  }
  const QString lang = language.isEmpty() ? QString("it") : language;

  // Una parola per riga: espeak restituisce i token separati da spazio, uno per
  // parola. Molto piu' rapido di un processo per parola.
  const QString batch = words.join("\n") + "\n";
  const QString ipa   = runEspeak(exe, lang, batch, error);
  const QStringList tokens =
      ipa.split(QRegExp("\\s+"), Qt::SkipEmptyParts);

  if (tokens.size() == words.size()) {
    for (const QString &t : tokens) out.push_back(parseIpa(t));
    return out;
  }

  // ⚠️ Conteggio diverso: una parola ha prodotto due token (o zero). Non si
  // puo' tirare dritto — i fonemi finirebbero sulla parola sbagliata e da li'
  // in poi tutto slitterebbe. Si ripiega su una chiamata per parola, che e'
  // lenta ma non puo' disallinearsi.
  for (const QString &w : words) {
    const QString one = runEspeak(exe, lang, w + "\n", nullptr);
    out.push_back(parseIpa(one));
  }
  return out;
}

}  // namespace ZtoryPhonemes
