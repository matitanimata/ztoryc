#pragma once

//============================================================================
// ZtoryMouthMap — la mappatura delle bocche, ATTACCATA AL LIVELLO.
//
// Franco, 2026-08-16: «il role character e' legato alla scena, i dati del
// mapping delle bocche sono collegati ai livelli delle bocche di quel
// character». Due cose diverse su due oggetti diversi:
//
//   scena personaggio  -> `role="character"` nel .ztoryc   (ztorycharacter.h)
//   livello di bocche  -> la mappa qui, accanto al livello (questo file)
//
// ── PERCHE' AL LIVELLO, come la palette ───────────────────────────────────
// Una voce dice «il viseme AI e' il fotogramma 3 di QUESTO livello». E' una
// proprieta' del livello, non della scena che lo contiene — esattamente come
// una palette. E come la palette viaggia con lui: quando l'animatore importa
// la sotto-scena del personaggio si porta dietro le bocche E le istruzioni per
// usarle, senza bisogno del progetto Ztoryc che lui non ha.
//
// ── UN FILE PER LIVELLO, anche dentro lo stesso PSD ───────────────────────
// I personaggi caricati da PSD a gruppi hanno PIU' LIVELLI in un solo file
// fisico. Non e' un problema: ognuno ha gia' un percorso proprio, con il
// gruppo dentro il nome —
//
//     CH_giornalista#7#group.psd   ->  CH_giornalista#7#group.zmouth
//     CH_giornalista#8#group.psd   ->  CH_giornalista#8#group.zmouth
//
// — quindi le mappe non si pestano i piedi. Un personaggio con bocche
// frontali e di profilo su livelli distinti ha semplicemente due file.
//
// ── LE BOCCHE IN UNA SOTTO-SCENA ──────────────────────────────────────────
// Franco lavora anche cosi', per poter costruire un labiale con piu' livelli
// (bocca, denti, lingua) e ritoccarlo. Una sotto-scena pero' NON HA UN FILE:
// vive dentro il .tnz. Quindi:
//
//   LA MAPPA VIVE DOVE VIVE LA COSA.
//     livello con un file  -> `.zmouth` accanto al file
//     sotto-scena          -> `.zmouth` accanto alla SCENA, con l'attributo
//                             `subscene="nome"` su ogni set
//
// Non e' un caso speciale, e' la stessa regola applicata due volte. E ha un
// pregio: dentro un fotogramma di sotto-scena ci metti quanti livelli vuoi,
// quindi li' un viseme torna a essere UN fotogramma solo.
//
// ── UN VISEME, PIU' LIVELLI ───────────────────────────────────────────────
// Il file e' ATTACCATO a un livello ma puo' CITARNE altri: nel cutout la bocca
// e i denti sono livelli diversi che cambiano insieme. I livelli citati si
// indicano per NOME, che e' l'unica cosa che sopravvive alla copia fatta
// dall'import (il percorso cambia, il nome no). Il livello a cui il file e'
// attaccato si chiama ANCORA e non si nomina: e' implicito.
//
// ⚠️ L'EXPORT NON LO COPIA DA SOLO. `TXshSimpleLevel::getFiles()` e' l'elenco
// dei file che viaggiano con un livello (oggi palette e hook) e va esteso, ma
// l'export dello storyboard NON lo consulta: raccoglie il file del livello e
// basta (storyboardpanel.cpp ~5855). Va aggiunto anche li', o il personaggio
// arriva con le bocche e senza le istruzioni — la stessa dimenticanza dei
// binari helper lzocompress lasciati fuori dal bundle.
//============================================================================

#include "tfilepath.h"

#include <QString>
#include <QSet>
#include <QStringList>
#include <QVector>

//! UN bersaglio di un viseme: un disegno su un livello, oppure una posa.
//!
//! `levelName` vuoto = il livello ANCORA, cioe' quello a cui il file e'
//! attaccato. E' il caso normale, e non scriverlo lo rende anche impossibile
//! da scrivere sbagliato.
//!
//! La posa serve a ZtoRig, dove la bocca sta dentro il rig e non si scambia un
//! disegno ma si scrive una posa (vincolo posto da Franco: «non inchiodare il
//! bersaglio al livello di bocche»).
struct MouthTarget {
  QString  levelName;  //!< vuoto = il livello a cui la mappa e' attaccata
  TFrameId frameId;    //!< fotogramma su quel livello
  QString  poseName;   //!< in alternativa: azione registrata sul rig

  bool isEmpty() const { return poseName.isEmpty() && frameId.isEmptyFrame(); }
  bool isPose() const { return !poseName.isEmpty(); }
  bool isAnchorLevel() const { return levelName.isEmpty(); }
};

//! Un set di bocche: i dieci viseme di Preston Blair, piu' gli attributi che
//! dicono QUALE set e'. Un livello puo' averne piu' d'uno (la stessa bocca
//! disegnata felice e triste), e al lip sync si sceglie solo quale.
struct MouthSet {
  QString name;        //!< come lo chiama l'utente, ed e' la chiave
  //! Vuoto = il set appartiene al LIVELLO accanto a cui sta il file.
  //! Valorizzato = appartiene alla SOTTO-SCENA con questo nome, e allora il
  //! file sta accanto alla scena (vedi il commento in testa).
  QString subScene;
  QString view;        //!< "front" | "profile" | "threequarter"
  QString expression;  //!< "happy" | "sad" | … (libera: non e' un'enum chiusa)
  QString variant;     //!< "up" | "down" — la bocca in su o in giu'

  //! ⚠️ UN VISEME PUO' PILOTARE PIU' LIVELLI INSIEME. Nel cutout la bocca e'
  //! un livello e i denti (o la lingua) un altro, e la posizione «AI» li deve
  //! cambiare entrambi allo stesso fotogramma. Quindi una casella e' una LISTA
  //! di bersagli, non un bersaglio solo.
  //!
  //! Franco, 2026-08-16, sul perche' vale la pena prevederlo adesso: «in
  //! prospettiva quando inseriremo il 2.5D e riprenderemo il discorso del
  //! controller su animazioni vettoriali intercalate automaticamente questo
  //! sara' da prevedere». Il formato si cambia gratis finche' nessuno ha
  //! ancora scritto file; dopo si migra.
  //!
  //! Indicizzate da ZtoryMouthMap::kShapes, NELL'ORDINE — lo stesso di
  //! m_activeFrameIds in lipsyncpopup.cpp, che e' la tabella che applica
  //! davvero il lip sync.
  //!
  //! NON chiamarle `slots`: e' una macro di Qt e il campo non compila.
  QVector<MouthTarget> mouths[10];

  //! Un set senza nemmeno un bersaglio non serve a nessuno: non si salva,
  //! invece di scrivere dieci caselle vuote che poi non applicano niente e
  //! sembrano un bug del lip sync.
  bool isUsable() const {
    for (const QVector<MouthTarget> &v : mouths)
      for (const MouthTarget &t : v)
        if (!t.isEmpty()) return true;
    return false;
  }

  //! I nomi dei livelli citati dal set, ANCORA ESCLUSA. Serve a chi applica:
  //! sono le colonne che dovra' trovare oltre a quella delle bocche.
  QStringList extraLevels() const;
};

//! Cio' che il file accanto a un livello contiene: i set definiti su quel
//! livello, piu' il personaggio a cui appartiene.
struct MouthMap {
  //! Riferimento incrociato all'Asset di progetto. Serve al percorso
  //! automatico (da chi parla al suo set) e sopravvive a una rinomina.
  //! Puo' essere vuoto: un livello di bocche mappato a mano funziona lo
  //! stesso, semplicemente non si trova partendo dal copione.
  QString characterUuid;
  QString characterName;

  QVector<MouthSet> sets;

  //! Indice del set chiamato \p n, o -1. I set si citano per NOME: e' cio' che
  //! l'utente sceglie, e sopravvive al riordino della lista.
  int indexOfSet(const QString &n) const;
};

namespace ZtoryMouthMap {

//! I dieci viseme, nell'ordine delle caselle. Sorgente unica: sono gli stessi
//! nomi che ZtoryPhonemes produce e che LipSyncPopup legge. Una seconda copia
//! divergerebbe al primo viseme aggiunto — e' gia' successo in questa feature
//! con la regola di chi parla.
extern const char *const kShapes[10];

//! Indice della casella per il nome del viseme, o -1. Accetta anche "other",
//! che il percorso Papagayo scrive al posto di "etc".
int shapeIndex(const QString &shape);

//! Il file della mappa per \p ownerPath: stesso percorso, estensione `.zmouth`.
//! \p ownerPath e' il percorso DECODIFICATO (assoluto) del livello, oppure —
//! per una sotto-scena — quello della SCENA che la contiene.
TFilePath pathFor(const TFilePath &ownerPath);

//! Vero se c'e' una mappa per \p subScene (vuoto = il livello stesso). La sua
//! esistenza DICHIARA che li' ci sono le bocche, che e' meta' del problema.
bool exists(const TFilePath &ownerPath, const QString &subScene = QString());

//! I nomi delle sotto-scene che hanno almeno un set in questo file, in UNA
//! lettura. Serve a chi deve dirlo per molte sotto-scene insieme: chiamare
//! `exists()` una volta per ciascuna rileggeva e riparsificava lo stesso file
//! tante volte quante le sotto-scene, e su una scena vera si sentiva.
void mappedSubScenes(const TFilePath &ownerPath, class QSet<QString> *out);

//! Legge la mappa. \p subScene vuoto = i set del livello accanto a cui sta il
//! file; valorizzato = i set di quella sotto-scena. false (col motivo in
//! \p error) se non c'e' o e' rovinata. «Non c'e'» e' il caso normale, non un
//! errore da nascondere: quasi niente e' bocche.
bool load(const TFilePath &ownerPath, const QString &subScene, MouthMap &out,
          QString *error = nullptr);

//! Scrive la mappa. ⚠️ Tocca SOLO i set di \p subScene: un file di scena tiene
//! le mappe di piu' sotto-scene, e riscriverlo per intero cancellerebbe quelle
//! delle altre.
//! Una mappa senza set toglie i suoi set; se il file resta vuoto viene
//! cancellato, cosi' `exists()` continua a voler dire qualcosa.
bool save(const TFilePath &ownerPath, const QString &subScene,
          const MouthMap &map, QString *error = nullptr);

}  // namespace ZtoryMouthMap
