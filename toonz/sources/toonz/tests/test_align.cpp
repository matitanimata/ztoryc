//============================================================================
// Test del riallineamento «tempi di Whisper + parole del copione».
// Estrae il codice VERO (extract_align.py), non una riscrittura.
// I dati sono REALI: l'uscita misurata di whisper-cli sul modello tiny.
//
//   clang++ -std=c++17 test_align.cpp -o t -F$(brew --prefix qt@5)/lib \
//     -framework QtCore -I$(brew --prefix qt@5)/lib/QtCore.framework/Headers
//============================================================================
#include <QString>
#include <QStringList>
#include <QVector>
#include <QRegExp>
#include <QDebug>
#include <QtGlobal>

struct DialogueLine { QString character, assetUuid, text; bool matched = false; };
struct TimedWord {
  QString word; int startMs = 0, endMs = 0; QString assetUuid;
  bool fromScript = true;
};
#include "align_body.inc"

static int fails = 0;
static void expect(const QString &title, bool ok, const QString &detail) {
  if (ok) { qInfo().noquote() << "  ok  " << title; return; }
  fails++; qInfo().noquote() << "  FAIL" << title << "\n     " << detail;
}

int main() {
  // ---- dati VERI, misurati con whisper-cli -m ggml-tiny.bin -l it ----
  struct H { const char *w; int a, b; };
  const H heardRaw[] = {
    {"ma",10,130},{"dove",130,390},{"cre",390,590},{"di",590,720},{"di",720,850},
    {"andare,",850,1540},{"lascia",1540,1780},{"l'operdere,",1780,2560},
    {"non",2560,2870},{"ne",2870,2890},{"vale",2890,3150},{"la",3150,3280},
    {"pena.",3280,3840}};
  QVector<TimedWord> heard;
  for (const H &h : heardRaw) { TimedWord t; t.word=h.w; t.startMs=h.a; t.endMs=h.b; heard<<t; }

  DialogueLine dl;
  dl.character = "BRONTOLO"; dl.assetUuid = "uuid-BRONTOLO"; dl.matched = true;
  dl.text = "Ma dove credi di andare? Lascialo perdere, non ne vale la pena.";
  QVector<TimedWord> out = alignToScript(heard, {dl});

  QStringList words;
  for (const TimedWord &t : out) words << t.word;
  expect("le parole sono quelle del COPIONE, non quelle sentite",
         words.join(" ") == dl.text,
         "ottenuto: " + words.join(" "));

  expect("«credi» esiste (Whisper l'aveva spezzato in cre+di)",
         words.contains("credi"), "parole: " + words.join(" "));
  expect("«Lascialo» esiste (Whisper aveva prodotto l'operdere)",
         words.contains("Lascialo"), "parole: " + words.join(" "));

  bool monotone = true; int prev = -1;
  for (const TimedWord &t : out) { if (t.startMs < prev) monotone = false; prev = t.startMs; }
  expect("i tempi non tornano mai indietro", monotone, "");

  bool timed = true;
  for (const TimedWord &t : out) if (t.endMs <= 0) timed = false;
  expect("ogni parola ha un tempo", timed, "");

  bool owned = true;
  for (const TimedWord &t : out) if (t.assetUuid != "uuid-BRONTOLO") owned = false;
  expect("ogni parola sa di chi è", owned, "");

  expect("copre l'intera durata dell'audio",
         out.first().startMs < 200 && out.last().endMs > 3500,
         QString("da %1 a %2").arg(out.first().startMs).arg(out.last().endMs));

  // ---- due personaggi: l'attribuzione deve seguire il copione ----
  DialogueLine a, b;
  a.character="BRONTOLO"; a.assetUuid="uuid-A"; a.text="Ma dove credi di andare?";
  b.character="FATINA";   b.assetUuid="uuid-B"; b.text="Lascialo perdere, non ne vale la pena.";
  QVector<TimedWord> two = alignToScript(heard, {a, b});
  int nA = 0, nB = 0;
  for (const TimedWord &t : two) { if (t.assetUuid=="uuid-A") nA++; if (t.assetUuid=="uuid-B") nB++; }
  expect("due personaggi: 5 parole al primo, 7 al secondo",
         nA == 5 && nB == 7, QString("A=%1 B=%2").arg(nA).arg(nB));

  bool split = true;
  for (const TimedWord &t : two)
    if (t.assetUuid=="uuid-B" && t.startMs < 1400) split = false;
  expect("la battuta del secondo comincia dopo quella del primo", split, "");

  qInfo().noquote() << (fails ? QString("\n%1 FALLITI").arg(fails) : QString("\nTutti passati"));
  return fails;
}
