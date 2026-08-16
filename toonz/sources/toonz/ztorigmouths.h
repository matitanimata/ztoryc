#pragma once

//============================================================================
// ZtoRigMouthsTab — la scheda dove si MAPPANO le bocche del personaggio.
//
// Franco, 2026-08-16: «l'operazione di mappatura si fa all'interno della scena
// character in un pannello che deve essere un tab di ztorig».
//
// E' una delle TRE operazioni del lip sync, e le altre due non stanno qui:
//   1. generare le colonne parole+fonemi   -> comando sull'xsheet / export
//   2. MAPPARE le bocche                   -> questa scheda
//   3. applicare il lip sync a un intervallo -> popup, nello shot
//
// ── PERCHE' UNA SCHEDA E NON UN POPUP ─────────────────────────────────────
// Mappare non e' un'operazione che si fa e si chiude: si guarda il disegno, si
// prova, si torna indietro. Un popup modale costringerebbe a chiuderlo per
// vedere il personaggio. E sta in ZtoRig perche' e' li' che vive tutto cio' che
// appartiene al personaggio — pose e correttive sono le schede accanto.
//
// ── PIU' LIVELLI ──────────────────────────────────────────────────────────
// Due sensi diversi, ed e' importante non confonderli:
//
//   ALTERNATIVI  frontale e profilo sono livelli DIVERSI, ognuno con la sua
//                mappa. Si sceglie quale mappare col selettore in cima.
//   INSIEME      la bocca e i denti cambiano ALLO STESSO ISTANTE. Sono due
//                bersagli della stessa casella (tasto destro sul riquadro).
//============================================================================

#include "ztorymouthapply.h"
#include "ztorymouthmap.h"

#include "toonz/txshsimplelevel.h"

#include <QHash>
#include <QWidget>
#include <QVector>

class QComboBox;
class QLabel;
class QPushButton;

class ZtoRigMouthsTab final : public QWidget {
  Q_OBJECT

public:
  explicit ZtoRigMouthsTab(QWidget *parent = nullptr);

  //! Ricostruisce dalla scena corrente (scena o colonna cambiata).
  void rebuild();

private slots:
  void onLevelChanged(int);
  void onSetChanged(int);
  void onNewSet();
  void onSaveSet();
  void onDeleteSet();
  //! Freccia sotto un riquadro: scorre i disegni del livello ancora.
  void onNavClicked(int id);
  //! Tasto destro su un riquadro: i bersagli sugli ALTRI livelli.
  void onSlotContextMenu(const QPoint &pos);
  //! Le icone arrivano DOPO: IconGenerator mette in coda il render e la prima
  //! richiesta torna vuota. Senza questo, aprendo la scena i riquadri restano
  //! neri finche' non si tocca qualcosa.
  void onIconGenerated();
  //! Lo xsheet e' cambiato (colonna rinominata, sotto-scena creata, livello
  //! spostato dentro una sotto-scena): l'elenco dei livelli non e' piu' quello.
  //! Passa da un timer perche' questo segnale arriva a raffica.
  void onXsheetChanged();
  void onZoomChanged(int value);

private:
  //! I livelli della scena che possono essere bocche, quelli gia' mappati per
  //! primi. Non si indovina QUALE sia la bocca — i nomi sono numerati
  //! (CH_nome#7#group) e non c'e' convenzione che lo dica — ma un livello che
  //! ha gia' una mappa si sa, e va messo davanti.
  void refreshLevelCombo();
  void refreshSetCombo();
  //! Porta la scheda su \p level (senza rifare l'elenco).
  void selectLevel(TXshLevel *level);
  //! Indice nella tendina del set \p name sul livello corrente, o -1.
  int  setRow(const QString &name) const;
  //! Porta il set scelto nei dieci riquadri.
  void loadSet(int index);
  //! Rirende le immagini sorgente (costoso): quando cambia il livello, la
  //! casella o arriva un'icona.
  void refreshSources();
  //! Ritaglia le sorgenti secondo zoom e spostamento correnti e le mette nei
  //! riquadri (a buon mercato): e' cio' che gira mentre si trascina.
  void refreshPreviews();
  //! Il proprietario della mappa: il percorso DECODIFICATO del livello, oppure
  //! quello della SCENA quando le bocche sono in una sotto-scena (che un file
  //! suo non ce l'ha). La mappa vive dove vive la cosa.
  TFilePath ownerPath() const;
  //! Nome della sotto-scena in lavorazione, vuoto se e' un livello normale.
  QString subSceneName() const;
  //! I fotogrammi selezionabili: i fid del livello, o 1..N della sotto-scena.
  void reloadFrames();
  //! Il bersaglio sul livello ancora della casella \p i, o null.
  MouthTarget *anchorTarget(int i);
  //! Chi e' questo personaggio, dal sidecar della scena. Puo' non esserci: un
  //! livello di bocche mappato a mano funziona lo stesso.
  void readCharacterFromScene(QString *uuid, QString *name) const;

  //! Se la scheda non si vede, il lavoro si RIMANDA. In un'applicazione a room
  //! un pannello di un'altra room non e' visibile ma esiste e riceve i segnali:
  //! senza questo, uscire da una sotto-scena ricostruiva un elenco che nessuno
  //! stava guardando (Franco, 2026-08-16: «per salire di un livello ci mette un
  //! sacco»).
  void showEvent(QShowEvent *) override;

  //! Trascinamento e rotellina sui riquadri. Un filtro invece di una
  //! sottoclasse di QLabel: servono tre eventi, e una classe con Q_OBJECT in
  //! un .cpp vorrebbe dire tirarsi dietro il moc per niente.
  bool eventFilter(QObject *obj, QEvent *event) override;

  QComboBox   *m_levelCombo = nullptr;
  QComboBox   *m_setCombo   = nullptr;
  QPushButton *m_newBt      = nullptr;
  QPushButton *m_saveBt     = nullptr;
  QPushButton *m_deleteBt   = nullptr;
  QLabel      *m_note       = nullptr;
  class QSlider     *m_zoom   = nullptr;
  class QTimer      *m_refreshTimer = nullptr;
  //! ── L'INQUADRATURA, condivisa dai dieci riquadri ────────────────────────
  //! Il riquadro resta della sua misura: a ingrandire e' il CONTENUTO (Franco,
  //! 2026-08-16). Ed e' una sola inquadratura per tutti e dieci, perche' le
  //! dieci bocche stanno nello stesso punto del fotogramma: regolarla una
  //! volta le sistema tutte, e tenerle nella stessa inquadratura e' anche
  //! l'unico modo per confrontarle.
  QSize   m_thumb;        //!< misura fissa del riquadro
  double  m_viewZoom = 1.0;
  QPointF m_viewPan;      //!< spostamento in pixel della SORGENTE, non del riquadro

  //! Le immagini rese in grande, una per casella, e il riquadro di partenza
  //! (il ritaglio automatico sul disegno). Si tengono qui perche' trascinare
  //! deve costare un ritaglio, non dieci render.
  QImage m_src[10];
  QRect  m_srcBox[10];
  //! Cosa c'e' DENTRO ogni sorgente, per non richiederla di nuovo.
  //!
  //! ⚠️ Senza questo la scheda si impiantava. `IconGenerator::addTask()`
  //! inserisce l'id in un insieme ma ACCODA COMUNQUE il lavoro: richiedere
  //! un'icona gia' in lavorazione la rimette in coda. E siccome ogni icona
  //! pronta faceva ridisegnare tutti e dieci i riquadri, ogni icona ne
  //! accodava altre dieci — con un render di sotto-scena a 552x432 la coda
  //! cresceva piu' in fretta di quanto si svuotasse.
  TXshLevel *m_srcLevel[10] = {};
  TFrameId   m_srcFid[10];

  //! Trascinamento in corso: da dove e con che spostamento si era partiti.
  QPoint  m_dragFrom;
  QPointF m_dragPanFrom;
  bool    m_dragging = false;
  QLabel      *m_emptyLabel = nullptr;
  QWidget     *m_grid       = nullptr;

  QLabel      *m_preview[10] = {};
  QLabel      *m_caption[10] = {};
  QPushButton *m_nav[20]     = {};

  //! Su cosa si sta lavorando: un livello semplice OPPURE una sotto-scena.
  //! Si tiene il TXshLevel perche' l'anteprima si chiede a lui in entrambi i
  //! casi — IconGenerator sa gia' rendere il fotogramma di un sub-xsheet.
  TXshLevel            *m_level = nullptr;
  std::vector<TFrameId> m_fids;

  //! La mappa letta dal disco e il set in lavorazione. `m_targets` e' la copia
  //! di lavoro: si scrive su disco solo con Salva, cosi' scorrere i set non
  //! sporca il file.
  MouthMap             m_map;
  QVector<MouthTarget> m_targets[10];
  QString              m_currentSetName;
  //! Riempire le combo emette currentIndexChanged: senza guardia, ricaricare
  //! la lista caricherebbe un set che l'utente non ha scelto, buttando via
  //! l'assegnazione in corso.
  bool m_filling = false;
  //! Ricostruzione rimandata perche' la scheda non era visibile.
  bool m_dirty = false;

  //! Firma di cio' che l'elenco dei livelli dipende da: scena, quanti livelli
  //! ci sono e come si chiamano. Tutto in memoria, nessun accesso a disco.
  //!
  //! ⚠️ Serve perche' NAVIGARE FRA LE SOTTO-SCENE non cambia niente di tutto
  //! questo, ma faceva ripartire l'intera ricostruzione: scansione ricorsiva
  //! dell'albero piu' un accesso a file per livello. Su un progetto che sta su
  //! un volume esterno quegli accessi si pagano tutti, sul thread
  //! dell'interfaccia, a ogni spostamento (Franco, 2026-08-16: «ogni
  //! spostamento sulle sottoscene e' diventato lentissimo»).
  QString m_levelsSignature;
  //! Esito di `.zmouth` per percorso, per non ripetere lo stesso accesso a
  //! disco. Si svuota quando la firma cambia.
  QHash<QString, bool> m_mapCache;

  //! Dove stanno i set in questa scena, con dentro le mappe gia' lette.
  //!
  //! ⚠️ E' la STESSA funzione che usa il popup di «Apply» (findTargets), e non
  //! una seconda copia della stessa ricerca. La copia c'era, ed e' costata un
  //! difetto: il recupero dei set importati era stato aggiunto solo di la',
  //! quindi l'apply li trovava e questa scheda diceva che non ce n'erano.
  QVector<MouthApplyTarget> m_sources;
};
