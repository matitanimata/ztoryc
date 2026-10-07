# Copia unica dei dati degli shot — piano

Decisione di Franco, 2026-10-06: **i dati degli shot (testi, dialoghi, note, luci, tracking)
hanno una sola copia, nel modello**. Il Board resta il posto dove si scrivono e si leggono; non
è più il posto dove abitano. Punto di partenza: `docs/SHOT_OPS_AUDIT.md` (§7 per i difetti
provati).

Ramo: `feature/shot-document` (parte da `fix/navigator-stale-shot-index`).

---

## Perché

- Oggi gli stessi dialoghi esistono in **cinque copie**: i widget dei pannelli, `m_shots` di
  ognuno dei **tre Board** vivi (uno per room: `storyboardpanel.cpp:10347`,
  `ztoryanimatic.cpp:5059`, `:8839`) e `ZtoryModel::m_shots`. Si allineano per posizione; chi
  salva per ultimo vince.
- Il `.ztoryc` lo scrivono solo gli eventi dei Board; il ⌘S della scena no. Un testo scritto
  nel Navigator va perso (§7.5 dell'audit, provato).
- Lo stesso schema del pannello Xsheet di Tahoma: il pannello non possiede le celle, le mostra.
  Per questo due pannelli Xsheet non si contraddicono mai.

## Vincoli

1. **Il formato del `.ztoryc` non cambia.** I file vecchi si aprono uguali; una versione
   precedente di Ztoryc apre i file nuovi.
2. **Ogni passo è rilasciabile da solo** e passa la rete di sicurezza (sotto) prima del commit.
3. **Le prove si fanno solo su copie** in `reference/test_projects/`, con il tracker spento
   (`productionTracker="off"`), mai su un progetto vero.

## Identità di uno shot

- **In memoria**: la colonna dello xsheet principale (`TXshColumn*`). È unica anche per due
  Copy della stessa sotto-scena (che condividono il `TXshChildLevel`), e sopravvive al
  riordino (`TXsheet::moveColumn` ruota i puntatori, `txsheet.cpp:2099`).
- **Su disco**: come oggi, ordine + nome della sotto-scena (`level="sub_1"`).
- **Caso scoperto**: l'undo di Tahoma di una cancellazione di colonna ricrea colonne nuove
  (clona da `StageObjectsData`, `columncommand.cpp:591`). Lì il modello riaggancia i dati per
  sotto-scena, da un «cimitero» degli shot cancellati di recente. Con due Copy della stessa
  sotto-scena cancellate e ripristinate l'abbinamento può scambiarle: tollerato.

## Passi

| # | Passo | Cosa cambia per l'utente |
|---|---|---|
| 0 | **Rete di sicurezza + nessun testo perso**: il ⌘S della scena scrive anche il `.ztoryc`; il Navigator segna la scena come modificata; modalità di prova `ZTORYC_ROUNDTRIP` per confrontare i `.ztoryc` | il dialogo del Navigator non si perde più |
| 1 | **Il modello riconosce gli shot per colonna**: si riallinea dallo xsheet a ogni cambiamento, invece di essere un elenco da tenere in passo | nessuno shot riceve dati di un altro |
| 2 | **Il Board legge e scrive nel modello**: tiene solo i widget; i tre Board diventano tre finestre sulla stessa copia | due Board non divergono più |
| 3 ✅ | **Caricamento e salvataggio nel modello** (oggi `loadZtoryc`/`saveZtoryc` del Board) | il `.ztoryc` si salva anche senza Board nella room |
| 4 ✅ | **Via le sincronizzazioni**: `syncShotPanels`, `pushTrackingToBoard`/`pullTrackingFromBoard`, lo specchio per indice di `shotDataChanged` | — |
| 5 | **Operazioni uniche** (Add, Delete, Cut, Copy, Clone, Paste, Merge, Split, Move) in `ZtoryShotOps`, chiamate da Board, Animatic, Monitor | stessi comandi, stesso comportamento ovunque |
| 6 | **Undo sullo xsheet** (decisione ancora da prendere con Franco) | undo anche senza Board |

## Rete di sicurezza

Con `ZTORYC_ROUNDTRIP=1`, Ztoryc apre la scena passata sulla riga di comando, riscrive il
`.ztoryc` e si chiude (`_exit`, senza scrivere preferenze né `env.ini`); chiude da solo le finestre
modali (i «file mancante» delle copie senza disegni).

Corpus in `/Volumes/ZioSam/tahoma2d-workspace/reference/test_projects/roundtrip/` — copie, tracker
spento (`productionTracker="off"`), mai i file veri:

| copia | origine | shot |
|---|---|---|
| rt01 | Messina, storyboard `CS26_06ME_STB_NCP_V1` | 70 |
| rt02 | Cascina, `SB_` | 71 |
| rt03 | Maggiolata Zombie | 34 |
| rt04 | filorosso `SB_test` (il `.ztoryc` era rimasto a 7 shot, la scena ne ha 1) | 1 |
| rt05 | argo intro | 3 |
| rt06 | ZtorycTest `demo_ztoryc_v1` (vecchio, senza uuid) | 8 |

- `run_roundtrip.sh <etichetta>` → `out/<etichetta>/rtNN.ztoryc` (rimette `env.ini` com'era).
- `compare_roundtrip.py <base> <altro>` confronta, con gli uuid sostituiti da segnaposto in ordine di
  comparsa (rt06 riceve uuid casuali a ogni apertura).
- **Base** = `out/base1`, prodotta con il codice della 0.16.2 + passo 0. Due giri sullo stesso codice
  (`base1`, `base2`): identici.
- Dopo ogni passo: `run_roundtrip.sh passoN` e `compare_roundtrip.py base1 passoN` deve dire
  «identico» per tutti, salvo differenze volute e spiegate.

## Stato

- [x] Passo 0 — il ⌘S della scena scrive il `.ztoryc` (segnale `sceneSaved` → Board), il Navigator
  segna la scena come modificata (provato sul Mac); rete di sicurezza `ZTORYC_ROUNDTRIP` con base
  `out/base1` (2026-10-06).
- [x] Passo 1 — `ZtoryModel::reconcileWithXsheet()` alla fine di `resequenceXsheet`, identità per
  voce in `m_shotIds` (colonna + sotto-scena, solo in memoria); abbinamento: stessa colonna → stessa
  sotto-scena con colonna sparita (undo) → stessa posizione per le voci appena aggiunte
  (`addShotNamed`, `addShotFromRasters`). Rete di sicurezza: sei scene identiche alla base
  (2026-10-06). Prova a mano fatta (2026-10-06): «+» dell'Animatic in mezzo → «4 → 5, 1 nuovo», tecniche
  al loro posto, il Navigator trova lo shot nuovo (prima: crash/«No shot») e mostra il suo numero
  (`renumberAll` passa l'etichetta alla voce del modello sulla stessa colonna); la battuta scritta lì
  arriva nel `.ztoryc` dello shot giusto. Delete + Undo: «6 → 5, 1 gone», poi «5 → 6, 5 by sub-scene,
  1 new» — l'undo ricrea tutte le colonne; lo shot ripristinato rientra nel modello come voce nuova
  (senza i dati che stanno solo nel modello: per i testi comanda ancora il Board). **Da fare al passo
  2**: un «cimitero» degli shot cancellati, ripresi per sotto-scena. Non provati: Send to Board.
- [~] Passo 2 — diviso in tre (2026-10-06): **2a** il modello tiene gli shot come oggetti condivisi
  (`ZtoryShotList`, `std::shared_ptr<ShotData>`), stessa interfaccia di prima — FATTO, rete di sicurezza
  identica; **2b** il Board prende l'oggetto dello shot dal modello (`shot.data` → `shot.data->`), le
  fotografie dell'undo diventano copie vere, il gestore di `shotDataChanged` aggiorna solo i widget, un
  «cimitero» degli shot cancellati per l'undo; **2c** via `syncShotPanels` (testi),
  `pushTrackingToBoard`/`pullTrackingFromBoard`, `setShotsFrom`.
  - **2b scritto (2026-10-06), rete di sicurezza identica, prove a mano FATTE in gran parte** (2026-10-06,
    su navtest1): testi condivisi fra i Board di X e T; secondo disegno; Paste di una Copy prima
    dell'originale (uuid nuovo, Copy vuota); cancellare l'originale di una Copy + Undo; testo del
    Navigator conservato entrando e uscendo da un altro shot senza salvare; riordino con una Copy (i
    dati seguono lo shot, la Copy resta vuota); Merge (un solo «removed» per Board, dati dello shot
    dopo intatti, uguale in X e T); Revert Scene (rilegge il file: vedi la decisione per il passo 3).
    Restano: luce dal Navigator, Send to Board, cambio scena. Cosa fa:
    `Shot::data` è `ZtoryShotList::Ptr`, preso con `modelShotFor(col)`; `bindShotsToModel()` dopo ogni
    riallineamento del Board (oggetto del Board = oggetto del modello per quella colonna; una voce
    «fresca» del modello prende quello del Board); il `.ztoryc` si legge **una volta per apertura**
    (`shotDataLoadedFor`, azzerato a `sceneSwitched`): le riletture riempiono solo gli shot freschi;
    `onShotInserted`/`onShotRemovedAt` rimettono le colonne in assoluto (uno spostamento relativo
    sarebbe stato applicato una volta per Board); `syncShotPanels` del Board → `notifyShotEdited`;
    il gestore di `shotDataChanged` trova lo shot per puntatore e aggiorna solo i widget (la luce:
    ultima vista per pannello); `pullTrackingFromBoard` vuota; i `setShotsFrom` prima della
    pubblicazione → `reconcileWithXsheet`; il riallineamento del modello abbina per colonna solo se
    espone ancora la stessa sotto-scena (il riordino sposta le celle, non le colonne).
  - Review del 2b (2026-10-06): 2 bloccanti e 5 punti, tutti corretti — `ensurePanelWidgets` (i Board
    allineano i loro widget ai pannelli condivisi: dal gestore degli avvisi e dalla riconciliazione);
    riallineamento del modello a passate con «colonna attesa» per voce (se la colonna si è spostata
    conta la colonna, se è ferma conta il dato: il riordino) — due Copy restano distinte;
    `takeShotObject` non mette mai lo stesso oggetto in due posti; segno «fresco» azzerato dove la
    lista si svuota e tolto quando un Board ha completato lo shot; colonne sempre in assoluto e con
    controllo della sotto-scena (`reanchorColumnsFromScene`, altrimenti si ricostruisce); il «già
    letto» ricorda l'oggetto scena (entrare/uscire da uno shot non fa rileggere il file); la
    rilettura non scrive più `sequenceId` nel modello per indice. Rete di sicurezza: identica.
  - Seconda review (2026-10-06): chiusi i punti della prima; due bloccanti sulle Copy, corretti —
    una voce con la colonna morta non vince la passata «dove atteso» (cancellando uno shot che ha
    una Copy il modello teneva i dati del cancellato); `onShotInserted` ricostruisce invece di mettere
    due volte lo stesso oggetto (Paste di una Copy prima dell'originale); ricostruzione della griglia
    raccolta e rimandata, con le anteprime; tolto lo `shotRemovedAt` dopo il resequence nel Merge del
    Board (contro la regola di AGENTS.md). Codice morto del modello (`removeShot`, `moveShot`,
    `cloneShot`): da togliere nel 2c. Rete di sicurezza: identica.
  - Prove a mano da fare (aggiunte dalle review: cancellare l'originale di una Copy e la Copy; Paste di
    una Copy prima dell'originale e fra due Copy; secondo disegno in uno shot e i tre Board; Merge; Copy
    e Paste prima dell'originale; riordino con una Copy; luce dal Navigator, poi entrare e uscire da
    uno shot senza salvare): testo nel Board di Ztoryc X visibile subito nel Board di Ztoryc T; Navigator;
    «+»; Delete + Undo; Cut/Paste da Board e Animatic; riordino nel Board; Merge; Send to Board; luce
    dal Navigator; salvataggio, Revert Scene, cambio scena (nessun testo di un'altra scena).
- **2c scritto (2026-10-06), rete di sicurezza identica.** Tolti: nel modello `syncShotPanels`,
  `removeShot`, `moveShot`, `cloneShot`, `renumberAll`, `setShotsFrom` (nessun chiamante); nel Board
  `pushTrackingToBoard`/`pullTrackingFromBoard`, la copia dell'etichetta nel modello in `renumberAll`,
  la copia nel modello in `ensureShotUuids` (e il ripiego per posizione delle scene senza uuid), la
  copia per posizione di `transitionFrames` dopo `renumberAll` e di `sequenceId` nella cascata delle
  sequenze. Corretto **Set Technique**: scriveva `model->shot(si)` con un indice del Board e contava
  sul push per riportarlo nel Board; ora scrive lo shot del Board (l'oggetto del modello). Restano
  per il passo 4 il `notifyShotEdited` dopo ogni caricamento e il ciclo sul modello in
  «assegna sequenza» (scrive gli oggetti giusti, è solo doppio).
- **Passo 3 — mappa (2026-10-06).** Oggi il `.ztoryc` lo scrive solo `StoryboardPanel::saveZtoryc`,
  chiamata da 36 punti (33 nel Board, fra cui **ogni tasto premuto** in un campo di testo,
  `storyboardpanel.cpp:2166`; 2 nell'Animatic; il tracker via `persistViaBoard`). Il testo arriva già
  nell'oggetto condiviso a ogni tasto, quindi il modello ha sempre i dati vivi. `ZtoryModel::save()` /
  `load()` sono codice morto (`setZtoryPath` non è mai chiamata, `m_ztoryPath` resta vuoto) e
  scrivevano un altro formato (v4, senza uuid né luci): da togliere. Sotto-passi:
  - **3a — lo scrittore nel modello**, nessun cambiamento visibile: `ZtoryModel::writeShotDocument(path)`
    produce lo stesso XML di oggi (rete di sicurezza identica); passano nel modello anche gli stati del
    documento che oggi sono membri del Board (`productionTracker="off"`, `shotIdentityAsked`, ruolo
    della scena shot/personaggio). Il Board tiene per ora i controlli, la domanda «Connect to the
    Production Tracker?» e la pubblicazione nel progetto.
    **Fatto (2026-10-06)**, rete di sicurezza identica: `writeShotDocument(path, shots)` e
    `shotLevelNameAt` nel modello; i quattro stati in `shotDocumentState()`; tolti `save()`/`load()`/
    `setZtoryPath`/`m_ztoryPath` del modello. Da tenere a mente per il 3b: ogni Board che rilegge il file
    azzera e rilegge anche questi stati, che ora sono uno solo per tutti; oggi è innocuo perché la
    risposta «Stay disconnected» si scrive subito nel file, ma col file scritto solo al ⌘S una rilettura
    a metà sessione non deve toccarli (stessa regola «letto una volta» dei dati degli shot).
  - **3b — si scrive solo col salvataggio della scena** (decisione di Franco): i 36 punti diventano
    «documento modificato» (la scena prende l'asterisco, quindi chiudendo Tahoma chiede di salvare);
    al `sceneSaved` il modello scrive il `.ztoryc`, anche senza Board nella room, e subito dopo si
    pubblica nel `production.ztrack`. Conseguenze da dire a Franco: la domanda sul Production Tracker
    arriva al primo ⌘S invece che alla prima modifica; gli altri storyboard del progetto vedono gli
    shot di questo nel Tracker dopo il ⌘S, non in diretta.
    **Scritto (2026-10-06), compilato, da collaudare.** `markShotDocumentChanged()` (Board) /
    `markShotDocumentDirty()` (modello) al posto delle scritture per le azioni dell'utente (testi,
    luci, durate, numerazione, sequenze, impostazioni, Add/Delete/Move, undo, technique, tracker,
    transizioni dell'Animatic, luce del Navigator); tolte quelle delle ricostruzioni (riconciliazione,
    rilevamento pannelli, `onShotInserted`/`onShotRemovedAt`, apertura della scena: aprire una scena
    non la segna modificata). Restano tre scritture: `sceneSaved`, la seconda passata interna, la
    modalità `ZTORYC_ROUNDTRIP`. **Save As** ora segue la scena (prima il `.ztoryc` della copia non
    veniva scritto). Una rilettura a metà sessione non rimette più dal file numerazione, sequenze,
    sceneggiatura, logo PDF e stati del documento; dopo ogni scrittura il percorso conta come «letto»;
    senza file (prima del primo ⌘S) la memoria resta la verità.
    Senza Board vivo (flusso Cutout: le sue stanze non ne hanno) il ⌘S non arrivava a nessuno: ora
    scrive il modello (`writeShotDocumentWithoutBoard`), solo se ha letto il file di quella stessa
    scena in questa sessione (altrimenti scriverebbe dati vuoti sopra quelli veri), senza la
    pubblicazione nel Tracker. La rete di sicurezza ora forza il flusso Storyboard e rimette quello di
    Franco (in Cutout restava ferma sulla prima finestra). Rete di sicurezza: identica.
  - **3c — il recupero porta il `.ztoryc`** (sotto). **Scritto, compilato, da collaudare**: lo snapshot
    scrive il documento dal modello in `scene/<nome>.ztoryc` (attributo `ztorycFile` nel manifesto),
    il ripristino lo rimette accanto alla scena; i recuperi vecchi senza l'attributo restano come prima.
  - **Prove a mano di 3b/3c (2026-10-07, navtest1): passate.** Aprire la scena non la segna modificata;
    scrivere un testo mette l'asterisco senza toccare il `.ztoryc`; ⌘S scrive `.tnz` e `.ztoryc`
    insieme; Copy + Paste + riordino senza scrivere nulla, poi Revert: l'originale tiene il suo dialogo
    (il caso del 6/10); recupero dopo un'uscita forzata: rimette `.tnz` e `.ztoryc` dello stesso
    momento e il Board mostra il testo; chiudere senza salvare chiede, «Discard» lascia il file com'era.
    Restano: Save As, ⌘S in flusso Cutout (scrive il modello), luce dal Navigator, Send to Board,
    technique e task dal Production Tracker + salva + riapri.
  - **3d — il lettore nel modello** (`loadZtoryc` legge nel modello; il Board costruisce solo i widget).
    **Fatto (2026-10-07).** `readShotDocument(bytes, targets, out)` nel modello: lo stesso parse e lo
    stesso abbinamento per sotto-scena; ruolo, sceneggiatura e back-link tornano al Board. Senza Board
    vivo il modello legge da sé (`readShotDocumentWithoutBoard`, dopo il cambio scena, stessi
    azzeramenti del Board e poi il DB di progetto). Rete di sicurezza identica; provato a mano in flusso
    Cutout su navtest1: il modello legge all'apertura e al ⌘S scrive un `.ztoryc` identico a quello letto,
    insieme al `.tnz`. **Il passo 3 è chiuso**, salvo le prove a mano elencate sopra.
- **Passo 4 — già fatto dal 2c (2026-10-07).** Non restano copie fra Board e modello: gli ascoltatori
  di `shotDataChanged` (Navigator, titolo del viewer, Tracker) aggiornano solo ciò che mostrano; i
  `notifyShotEdited` sono avvisi. Resta solo un doppio ciclo innocuo in «assegna sequenza».
- **Passo 5 — mappa (2026-10-07).** Sette operazioni esistono due volte, Board e Animatic, ~900 righe:
  Merge (98/107), Delete (104/34), Add (71/54), Cut (53/60), Paste (45/33), Copy (24/22), Clone
  (24/21); solo Animatic: Razor (181), Merge con il successivo (97); solo Board: Move (85). Tutte usano
  lo stesso undo (istantanea del Board + `UndoBoardState`), quindi il passo 5 non aspetta la decisione
  del passo 6. Ordine: prima le coppie più divergenti (più probabile che si comportino diversamente).
  - **Delete — unificato, compilato, da collaudare.** `ZtoryShotOps::closeSubScenes()` e
    `deleteShotColumns(cols)`: cancella dall'alto in basso e toglie dal cast i livelli rimasti senza
    utente (restituiti per l'undo). **Differenza trovata:** il Delete dell'Animatic non toglieva i
    livelli orfani — la correzione `2bdb3d19e` (blocco dell'export) valeva solo per il Board. Prova:
    cancellare uno shot dall'Animatic, poi Export to Board / controllare il cast; undo e redo.
  - **Add — unificato, compilato, da collaudare.** `columnAfterLastShot` e `insertNewShot(col)`.
    **Differenze trovate:** senza selezione l'Animatic metteva lo shot in fondo all'xsheet (dopo le
    colonne audio), il Board dopo l'ultimo shot — ora entrambi dopo l'ultimo shot; il Board usava
    l'indice della scheda come colonna dell'xsheet (giusto solo se gli shot sono le prime colonne) —
    ora la colonna vera; il blocco della numerazione con shot su Kitsu ora vale anche dall'Animatic.
    Prova: Add con e senza selezione da Board e Animatic, con una colonna audio; undo e redo.
  - **Copy / Cut / Clone / Paste — unificati, compilati, da collaudare.** `makeShotClip(xsh, cols,
    kind)` e `dropOneShotClipEntries()`; la Paste usa `closeSubScenes` e `columnAfterLastShot`.
    **Differenze trovate:** Copy e Clone del Board prendevano la durata del primo pannello e ti
    tiravano fuori dallo shot che stavi disegnando (non toccano l'xsheet: ora non lo fanno più);
    quelle dell'Animatic contavano anche il fotogramma di chiusura; il Cut dell'Animatic non toglieva
    i livelli orfani e leggeva i dati dello shot dal Board (ora dal modello, anche senza Board); senza
    selezione la Paste andava in fondo all'xsheet (ora dopo l'ultimo shot). Prova: Copy/Cut/Clone e
    Paste da Board e da Animatic, incrociati (copia dall'uno, incolla nell'altro), dentro e fuori da
    uno shot; Cut + Paste conserva testi e uuid; undo e redo.
- **Decisione di Franco (2026-10-06), passo 3: il `.ztoryc` si scrive solo col salvataggio della
  scena** (⌘S, Save Scene, Save All), mai più dagli eventi dei Board (riordino, Paste, ecc.). Così
  `.tnz` e `.ztoryc` sono sempre dello stesso momento; un testo non salvato si perde chiudendo senza
  salvare, come un disegno. Perché: provato a mano il 2026-10-06 — Copy incollata (non salvata),
  riordino (il Board scrive il `.ztoryc`, `onMoveShot`, già così su master), Revert Scene: il `.tnz`
  ha 5 shot, il `.ztoryc` 6 voci, l'abbinamento per ordine di sotto-scena dà all'unica colonna `sub_4`
  la prima voce `sub_4`, cioè la Copy vuota → l'originale perde dialogo e uuid. Stesso esito
  chiudendo senza salvare e riaprendo. Non è una regressione del 2b.
- **Conseguenza obbligata (domanda di Franco, 2026-10-06): il recupero deve portarsi dietro il
  `.ztoryc`.** Oggi `ZtoryRecovery` copia solo il `.tnz` e i livelli modificati
  (`ztoryrecovery.cpp:238-256` la copia, `:412-447` il ripristino): funziona per caso perché il Board
  scrive il `.ztoryc` da solo, e anche così dopo un recupero il `.ztoryc` può essere di un momento
  diverso dal `.tnz`. Nel passo 3 lo snapshot scrive anche il `.ztoryc` dal modello nella cartella di
  recupero (stesso momento del `.tnz`), il manifesto lo elenca e `restore` lo rimette accanto alla
  scena. Senza questo, col `.ztoryc` scritto solo al salvataggio, un crash perderebbe i testi.
- Noto per il passo 3: dopo «Save Scene As» `saveZtoryc` non scrive (il percorso non coincide
  con `m_currentZtoryPath`), come già prima.
- Scoperto il 2026-10-06: **Cut → Paste dal Board perde i testi dello shot** e dà uuid doppi
  (audit §7.6). Lo risolvono i passi 1 e 5 (la clip porta lo shot intero; nessun ripiego per
  posizione).
