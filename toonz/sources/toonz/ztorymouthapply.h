#pragma once

//============================================================================
// ZtoryMouthApply — APPLICARE il lip sync alla colonna delle bocche.
//
// E' la terza delle tre operazioni, e le altre due stanno altrove:
//   1. generare le colonne parole+fonemi  -> ztorylipsync.cpp
//   2. mappare le bocche                  -> ztorigmouths.cpp (scheda ZtoRig)
//   3. APPLICARE a un intervallo          -> qui
//
// ── PERCHE' A INTERVALLI, E NON TUTTO IN UNA VOLTA ────────────────────────
// Franco, 2026-08-16: «da frame a frame usa questo set, da frame a frame
// quest'altro [...] durante la stessa frase il personaggio potrebbe cambiare
// posizione o espressione». Quindi la scelta del set non e' UNA per battuta:
// e' una per TRATTO. Un'applicazione automatica sull'intera battuta non puo'
// essere giusta, e sarebbe sbagliata in un modo che sembra corretto.
//
// ── IL PROBLEMA VERO: I FOTOGRAMMI NON COINCIDONO ─────────────────────────
// La colonna dei fonemi sta nell'xsheet dello SHOT. La bocca sta dentro la
// sotto-scena del personaggio, magari dentro quella della testa. Per far
// comparire la bocca giusta al momento giusto bisogna scrivere DENTRO quella
// sotto-scena, alla riga che corrisponde.
//
// La corrispondenza non si calcola, SI LEGGE: la cella che espone una
// sotto-scena porta il numero di fotogramma di quella sotto-scena. Percio'
// funziona anche se il personaggio e' tenuto, rallentato o rimontato — casi in
// cui `riga - inizio` darebbe risposte sbagliate senza dirlo.
//
// ⚠️ **LO STESSO FOTOGRAMMA ESPOSTO DUE VOLTE.** Un fermo di due fotogrammi su
// un personaggio che parla manda due fonemi diversi sulla STESSA riga della
// sotto-scena, dove c'e' una cella sola. Non si sceglie: si RILEVA e si dice.
// L'alternativa (vince l'ultimo) darebbe un lip sync che sembra applicato e non
// lo e'.
//============================================================================

#include "ztorymouthmap.h"

#include "tfilepath.h"

#include <QString>
#include <QVector>

class TXsheet;
class TXshLevel;
class ToonzScene;

//! Un posto in cui si possono applicare le bocche: un livello (o una
//! sotto-scena) che ha una mappa, ovunque sia annidato nella scena.
struct MouthApplyTarget {
  TXshLevel *level = nullptr;
  QString    subScene;   //!< vuoto se e' un livello semplice
  TFilePath  owner;      //!< dove sta la sua mappa
  QString    label;      //!< come mostrarlo all'utente
  MouthMap   map;        //!< gia' letta: serve per elencarne i set
};

//! Un tratto: da \p from a \p to (fotogrammi dello SHOT, 1-based) si usa il set
//! \p setName.
struct MouthApplyRange {
  int     from = 1;
  int     to   = 1;
  QString setName;
};

//! Cosa e' successo applicando. Si riporta tutto invece di un booleano: chi
//! applica deve sapere quante celle ha scritto E quante non e' riuscito a
//! scrivere, o crederebbe fatto un lavoro fatto a meta'.
struct MouthApplyReport {
  int written  = 0;   //!< celle scritte
  int noMap    = 0;   //!< viseme senza bersaglio nel set
  int poses    = 0;   //!< bersagli «posa», che qui non si sanno scrivere
  int offStage = 0;   //!< fotogrammi in cui il personaggio non e' esposto
  //! Fotogrammi della SOTTO-SCENA su cui cadevano viseme diversi (un fermo, un
  //! ciclo riusato). Elencati, non contati: servono per andarci a guardare.
  QVector<int> conflicts;

  bool isClean() const { return conflicts.isEmpty() && noMap == 0; }
  QString summary() const;
};

namespace ZtoryMouthApply {

//! L'xsheet in cui si sta lavorando: quello CORRENTE, non quello in cima alla
//! scena. In uno storyboard lo shot e' una sotto-scena e le colonne dei fonemi
//! stanno li' dentro.
TXsheet *workingXsheet();

//! Tutti i posti della scena che hanno una mappa delle bocche, scendendo nelle
//! sotto-scene. Un personaggio riggato le ha annidate, quindi cercare solo in
//! cima non troverebbe niente.
QVector<MouthApplyTarget> findTargets(ToonzScene *scene);

//! Le colonne di TESTO che contengono viseme, nell'xsheet \p xsh.
//! Si riconoscono dal CONTENUTO e non dal nome: il nome della colonna e'
//! rinominabile, i viseme no. Restituisce gli indici di colonna.
QVector<int> findPhonemeColumns(TXsheet *xsh);

//! Il viseme scritto alla riga \p row della colonna \p col, o stringa vuota.
QString phonemeAt(TXsheet *xsh, int col, int row);

//! Applica i tratti. Scrive DENTRO la sotto-scena giusta, con un undo unico.
//! \p phonemeCol e' la colonna dei fonemi nell'xsheet in cima.
MouthApplyReport apply(ToonzScene *scene, int phonemeCol,
                       const MouthApplyTarget &target,
                       const QVector<MouthApplyRange> &ranges);

}  // namespace ZtoryMouthApply
