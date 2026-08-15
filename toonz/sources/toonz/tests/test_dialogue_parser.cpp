//============================================================================
// Test del riconoscimento «chi parla» nel testo dei pannelli.
//
// NON fa parte della build dell'app (il CMakeLists usa un elenco esplicito):
// e' un programma a se', che verifica ZtoryModel::parseDialogue().
//
// Il punto: NON riscrive la logica, ne estrae il TESTO ESATTO da ztorymodel.cpp
// in parser_body.inc, cosi' verifica il codice che gira davvero. Una copia
// riscritta verificherebbe la copia.
//
// Per rilanciarlo (da questa cartella):
//
//   python3 extract_parser.py     # rigenera parser_body.inc dal sorgente vero
//   clang++ -std=c++17 test_dialogue_parser.cpp -o t \
//     -F$(brew --prefix qt@5)/lib -framework QtCore \
//     -I$(brew --prefix qt@5)/lib/QtCore.framework/Headers
//   ./t
//
// Ha gia' preso due difetti veri (2026-08-15):
//  1. un nome sconosciuto veniva inghiottito nella battuta, e cosi'
//     unknownSpeakers() non poteva segnalare nulla — proprio il suo scopo;
//  2. escludendo le didascalie dal «seguito da», «MARIO / (sottovoce) / ...»
//     faceva sparire Mario. A negare un'intestazione e' la riga VUOTA.
//============================================================================

#include <QString>
#include <QStringList>
#include <QVector>
#include <QHash>
#include <QDebug>

struct DialogueLine {
  QString character, assetUuid, text;
  bool matched = false;
};
static QStringList gCharacters;
// Stub di ZtoryModel::m_speakerAliases: il parser lo consulta, quindi il banco
// deve fornirlo. E' il prezzo di verificare il codice VERO invece di una copia.
static QHash<QString, QString> m_speakerAliases;

#include "parser_body.inc"

static int fails = 0;
static void check(const QString &title, const QString &text,
                  const QStringList &want) {
  QStringList got;
  for (const DialogueLine &d : parseDialogue(text))
    got << QString("%1%2|%3").arg(d.character.isEmpty() ? "-" : d.character)
               .arg(d.matched ? "*" : "").arg(d.text);
  if (got == want) { qInfo().noquote() << "  ok  " << title; return; }
  fails++;
  qInfo().noquote() << "  FAIL" << title << "\n     atteso:" << want
                    << "\n     ottenuto:" << got;
}

int main() {
  gCharacters << "MARIO" << "LUCIA" << "Nonna Pina";

  check("Fountain: nome sopra, battuta sotto",
        "MARIO\nMa dove vai?\n\nLUCIA\nFatti gli affari tuoi",
        {"MARIO*|Ma dove vai?", "LUCIA*|Fatti gli affari tuoi"});

  check("Due punti sulla stessa riga",
        "MARIO: ma dove vai?\nLUCIA: fatti gli affari tuoi",
        {"MARIO*|ma dove vai?", "LUCIA*|fatti gli affari tuoi"});

  check("Estensione (V.O.) tolta dal nome",
        "MARIO (V.O.)\nStavo pensando...",
        {"MARIO*|Stavo pensando..."});

  check("Didascalia fra parentesi NON si pronuncia",
        "MARIO\n(sottovoce)\nNon ci credo",
        {"MARIO*|Non ci credo"});

  check("Battuta su piu' righe si ricompone",
        "LUCIA\nSenti,\nnon ho tempo",
        {"LUCIA*|Senti, non ho tempo"});

  check("«Nota:» NON e' un personaggio",
        "Nota: entra da destra",
        {"-|Nota: entra da destra"});

  check("Riga vuota chiude il blocco: la descrizione non e' di Mario",
        "MARIO\nCiao\n\nIl treno parte lentamente",
        {"MARIO*|Ciao", "-|Il treno parte lentamente"});

  check("Nome sconosciuto: riconosciuto MA non agganciato (lo segnala)",
        "GIOVANNI\nChi sono io?",
        {"GIOVANNI|Chi sono io?"});

  check("Didascalia urlata SENZA battuta sotto non e' un personaggio",
        "BUIO\n\nIl treno parte",
        {"-|BUIO", "-|Il treno parte"});

  check("Nome sconosciuto CON estensione: riconosciuto come battuta",
        "GIOVANNI (O.S.)\nChi sono io?",
        {"GIOVANNI|Chi sono io?"});

  check("Personaggio con spazio e minuscole",
        "Nonna Pina: mangia la minestra",
        {"Nonna Pina*|mangia la minestra"});

  check("Testo senza nessun personaggio",
        "Campo lungo sulla piazza deserta",
        {"-|Campo lungo sulla piazza deserta"});

  check("Vuoto", "", {});

  // Alias: un nome dello script forzato a mano su un personaggio.
  m_speakerAliases.insert("principessa", "uuid-PRINCENERENTOLA");
  check("Alias: nome del copione agganciato al personaggio",
        "PRINCIPESSA\nDammi la scarpetta",
        {"PRINCIPESSA*|Dammi la scarpetta"});
  m_speakerAliases.clear();
  check("Senza alias lo stesso nome resta non agganciato",
        "PRINCIPESSA\nDammi la scarpetta",
        {"PRINCIPESSA|Dammi la scarpetta"});

  qInfo().noquote() << (fails ? QString("\n%1 FALLITI").arg(fails)
                              : QString("\nTutti passati"));
  return fails;
}
