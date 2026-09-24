# ANIMATIC_TASKS — archivio 2026-09 (voci chiuse o superate)

> Creato il 2026-09-24 dal riordino di `ANIMATIC_TASKS.md`. Contiene, NEL LORO
> ORDINE ORIGINALE e senza modifiche, tutte le sezioni chiuse, decise o
> superate fino a quella data. Serve a ritrovare il *perche'*: le misure, le
> cause escluse, le decisioni. Non si lavora da qui.

---

## DONE (riepilogo)

| Task | Descrizione | Data |
|------|-------------|------|
| RAZOR-CRASH | SIGABRT razor + AV link | 2026-03-29 |
| AUTOFILL | AutoFill flood-fill BFS | 2026-03-31 |
| RAZOR-GAP | Audio gap visivo rimosso | 2026-03-31 |
| RAZOR-HOVER | Linea preview hover gialla tratteggiata cross-track | 2026-03-31 |
| BUILD-DEPLOY | libtnztools aggiunto, RelWithDebInfo | 2026-03-31 |
| 1 | Animatic Viewer visibile + ZtoryAnimaticController | 2026-03-19 |
| 1b | assertMainXsheet guard | 2026-03-21 |
| 2 | resequenceXsheet unificato in ZtoryModel | 2026-03-21 |
| 3 | Fix Copy vs Clone nel Board | 2026-03-21 |
| 4 | Rimossi bottoni duplicati | 2026-03-21 |
| 5 | Story-strip thumbnail orizzontale | 2026-03-25 |
| 6a | Zoom slider toolbar animatic | 2026-03-24 |
| 6c | Razor tool (SIGSEGV fix, trim child xsheet, audio linked) | 2026-03-26 |
| 6d | Link/Unlink audio-video (shiftLevelFromFrame) | 2026-03-26 |
| 6f | Merge shots con camera keyframe | 2026-03-26 |
| 7 | Double-click entra in edit mode | 2026-03-25 |
| 8 | Multi-selection in track | 2026-03-25 |
| 12a | Audio waveform visibile | 2026-03-19 |
| 12b | Audio scrubbing durante drag ruler | 2026-03-21 |
| 12c | Sound preview bar in ZtoryAudioTrack | 2026-03-25 |
| 13a-c | Onion skin markers, In/Out markers, Playhead style | 2026-03-21 |
| 14 | Startup dialog (4 sezioni) | 2026-03-21 |
| 15 | RecentFiles.ini cap a 50 entry | 2026-03-26 |
| TOOLBAR-ICONS | QPushButton -> QToolButton con icone SVG | 2026-04-05 |
| SVG-ICONS | 21 icone SVG Ztoryc in icons/dark/ztoryc/ + toonz.qrc | 2026-04-05 |
| CAMERA-INIT | Camera init fix inquadratura sottoscena | 2026-04-05 |
| BUG-ONION | Ghost onion skin disabilitato | 2026-04-05 |
| BUG-CAMRESIZE | Camera resize non scala piu il disegno | 2026-04-05 |
| WEBCAM-SELECT | selectCamera() forzata prima di ogni switch | 2026-04-05 |
| 16 | Audio sync: processedUSecs() master clock | 2026-03-28 |
| BUG-BOARDSYNC | Board desync: RAII guard + modelReset() | 2026-04-06 |
| BUG-TLV | Crash SIGSEGV TLV: libimage rpath patch | 2026-04-01 |
| BUG-AUTOFILL | AutoFill: TEnumProperty m_autoFillStyle | 2026-04-08 |
| PERF-AUDIO | Cache waveform in QImage | 2026-04-08 |
| MERGE-BOARD | m_mergeButton + shared selection | 2026-04-06 |
| SHORTCUTS | Cmd+C/X/V/Delete: two-phase eventFilter | 2026-04-19 |
| LOCK-AUDIO | Lock tracce audio | 2026-04-19 |
| NUMERAZIONE | Sistema SQ/SH/P: uuid, label, order_index | 2026-04-25 |
| 9 | Audio export con shot | 2026-04-20 |
| 11 | Viewer toggle: QStackedWidget | 2026-05-01 |
| 12b | BUG testi persi al reload: syncWidgetsToData() | 2026-05-01 |
| 12c | Export Animatic: label read-only | 2026-05-01 |
| 12d | BUG Audio toggle in sub-scena | 2026-05-01 |
| 13 | Undo/Redo: UndoBoardState snapshot-based | 2026-05-01 |
| 13b | BUG Undo Razor: snapshot child xsheet prima del trim | 2026-05-26 |
| 14b | FIX Mark-out a fine timeline all'avvio | 2026-05-01 |
| 15b | FIX Onion skin rimosso toolbar | 2026-05-01 |
| 16b | NEW Workflow startup page | 2026-05-02 |
| 17 | FIX Stop marker update immediato | 2026-05-02 |
| 18 | FIX Zoom rotella solo sul ruler | 2026-05-02 |
| 19 | FIX Cursore SizeHorCursor bordi blocchi | 2026-05-02 |
| 23 | NEW Layout template per ogni workflow | 2026-05-02 |
| MAIN-AUDIO-TOGGLE | Riscrittura: cache, ON=main/OFF=sub-scena, scrub gapless ~150ms | 2026-05-22 |
| BUG-PREVIEW-COLORS | Anteprime Board: rimosso rgbSwapped() — R/B non più scambiati | 2026-05-22 |
| 29 | BUG Script Panel persistenza: extras/script + scriptFile nel .ztoryc | 2026-05-22 |
| AUDIO-TRACK-FIX | Delete key, cross-track selection, drag/trim undo, focus border | 2026-05-19 |
| ADD-AUDIO-UNDO | UndoAddAudioTrack | 2026-05-19 |
| RAZOR-LINKED-UNDO | Razor linkato video+audio: TUndoScopedBlock | 2026-05-19 |
| RAM-WAVEFORM | Cache waveform viewport-aware ~1200px sliding window | 2026-05-19 |
| RAM-AUDIO-COL | requireColumnSoundTrack capped at videoFrameCount-1 | 2026-05-19 |
| RAM-THREADSAFETY | preBuildSoundTrackAsync usa mixingTogether() | 2026-05-19 |
| RAM-CACHE-RENDER | ImageManager::invalidateAllCached() in OnRenderCompleted | 2026-05-19 |
| BUNDLE-STUFF | CMake rsync -a invece di copy_directory (30GB symlink fix) | 2026-05-19 |
| ENTITLEMENTS | Ztoryc.entitlements aggiunto (fix codesign) | 2026-05-19 |
| 32 | MOD UI Headers: context chips BOARD/ANIMATIC/MONITOR | 2026-05-27 |
| 33 | NEW Single-instance guard: QLockFile in ~/Library/Caches | 2026-05-27 |
| 34 | NEW Room Ztoryc T + Panel Navigator + rinomina Ztoryc X + rimozione Browser | 2026-05-27 |
| 30 | PERF Board thumbnail: renderXsheetFrame + per-col cache + lazy visible-only | 2026-05-27 |
| 31 | PERF/BUG RAM: lazy thumbnail, debounce, SFH repair + fix waveform/audio/render | 2026-05-27 |
| MONITOR | ZtoryMonitorPanel: viewer + toolbar completa (zoom/select/trim/razor/add/merge/copy/clone/paste) + audio tracks + double-click shot entry | 2026-05-27 |
| BUG-DIRTY-SHOT | m_dirtyShotCol: detectAndUpdatePanels in contesto main-xsheet al Board show | 2026-05-27 |
| BUG-TEXT-CROSS | Cross-scene text contamination: m_currentZtoryPath lega save path a m_shots | 2026-05-29 |
| BUG-FFMPEG | ffmpeg regressione: bundle path + formati video output ripristinati | 2026-05-29 |
| BUG-PDF-THUMB | Risoluzione thumbnail PDF: render apposito ignorando cache Board | 2026-05-29 |
| 26 | NEW Roll Edit | 2026-05-29 |
| 27 | NEW Slide Edit | 2026-05-29 |
| 28 | NEW Doppio Viewer Contestuale | 2026-05-29 |
| BUG-WIN-INSTALLER | Windows installer path fix (Ztoryc vs Tahoma2D dir) — era già fixato; crash utente era da mixed install con vecchia versione | 2026-05-29 |
| BUG-MARKOUT | Mark-out main blocca play animatic: sostituito XsheetGUI::getPlayRange con animatic play range proprio in tutti e 4 i punti; aggiunto clampPlayRangeToTimeline() dopo resequence | 2026-05-29 |
| 25 | NEW In/Out Marker — superato: inPoint fisso a 1, Roll/Slide funzionano tramite trim su outPoint (durata) | 2026-05-29 |
| BUG-SCRIPT-CROSS | Script per-scena: import in extras/<scena>/script + load autoritativo da .ztoryc su sceneSwitched (indipendente dalla Board) | 2026-05-30 |
| 39 | MOD Feedback visivo: highlight shot attivo | 2026-05-31 |
| 42 | CRASH Geometric tool aprendo sub-scene (m_viewer guard) | 2026-06-08 |
| 43 | Export animatic (a/b/c/d) + burn-in stile SBPro | 2026-06-10 |
| 40 | Annotazioni Camera-Move + Light Direction (3 fasi) | 2026-06-10 |
| 45 | Status bar hint contestuali (Board + Animatic) | 2026-06-06 |
| 48 | Undo svuotava storyboard (UAF snapshot + TUndoManager hardening) | 2026-06-10 |
| 49 | Lag secondo tratto disegno (detect timer in sub-scene) | 2026-06-10 |
| 50 | Panel fantasma dopo undo che svuota uno shot | 2026-06-10 |
| KEYS-CELS | BUG-1/BUG-2 + key-only ops + Time Stretch combinato | 2026-06-15 |
| UI-HEADERS | Window title azzerato nei panel switcher + chip BOARD rimosso | 2026-06-15 |
| ICON-MIGRATION | Tabler/Lucide vendoring + wiring 5 toggle + dedup SVG | 2026-06-17 |

---

### 🆕 Segnalazioni Franco 2026-07-19 (da triage/investigare)

Raccolte a fine sessione. Le voci con ✅ hanno una causa individuata **leggendo il codice**
(non verificata a runtime); le altre sono ancora da investigare da zero.

**Animatic / trimming**
1. **FEATURE — durata shot visibile durante il trim + timing manuale.** Mentre si trimma,
   mostrare la durata dello shot sulla timeline; e poter **impostare a mano il timing** della
   scena invece che solo trascinando.
2. ✅ **RISOLTO (confermato da Franco, 2026-09-24)** — **BUG — play riparte da frame 1 (o dal mark in) durante il trim** invece che dalla posizione
   del cursore. Rende il trim a orecchio scomodo: si riascolta sempre dall'inizio.
3. **BUG — timing scena con transizione: la colonna note dei ftg extra in coda non segue il
   mark out.** Modificando il timing di una scena con transizione, le note dei fotogrammi extra
   in coda restano dov'erano invece di seguire il mark out.

**Audio**
4. **BUG — LINK AUDIO/VIDEO rompe l'UNDO sull'audio.** Con il link attivo, l'undo su una
   operazione audio non si comporta correttamente.
5. **BUG — multiselezione sulle clip audio non funziona.**

**Import**
6. **BUG — importando DUE psd come sotto-scene finiscono su UNA sola colonna.**
   ✅ **CAUSA TROVATA (lettura codice, non verificata a runtime).** In `loadPSDResource`
   (`toonz/iocommand.cpp:2565`) il ramo "expose in sub-xsheet" deposita la sotto-scena nel main
   xsheet con `xsh->setCell(row0 + r, col0, ...)` (`:2633`), ma **`col0` viene incrementato SOLO
   nel ramo non-subxsheet** (`:2621`). `col0` e' un riferimento ad `args.col0` e la funzione e'
   chiamata **una volta per file** dal loop di import (`:2891`) → il secondo PSD scrive nella
   stessa colonna del primo, sovrascrivendolo.
   **Fix**: nel ramo subxsheet, dopo il piazzamento, avanzare `col0` (+ `setColumnIndex`) come fa
   gia' l'altro ramo. Verificare l'interazione con `args.col1`.

**Tool / shortcut**
7. **BUG — le shortcut degli editing tool non funzionano / non sono quelle degli hint.**
   ✅ **CAUSA TROVATA (lettura codice).** I tooltip dei tool animatic promettono **S** (Select),
   **T** (Trim/Roll), **C** (Razor) — vedi le stringhe in `ztoryanimatic.cpp` — ma **non esiste
   alcun handler per `Qt::Key_S`, `Qt::Key_T` o `Qt::Key_C` "nudi"**: gli unici `Key_C` gestiti
   richiedono Ctrl/Cmd (copy, `:2170`, `:6194`, `:6225`). Le shortcut non sono "rotte": non sono
   **mai state implementate** (o sono andate perse), mentre gli hint le annunciano.
   **Fix**: implementarle nel keyPressEvent del pannello (i tasti nudi S/T/C sono liberi, nessun
   conflitto con Cmd+C), oppure — se si preferisce passarle dal CommandManager — attenzione alla
   nota gia' presente a `:5096` sull'inaffidabilita' di QShortcut+WidgetWithChildrenShortcut.

**Production tracker**
8. **BUG — i task non rispecchiano l'ordine del workflow (shot E asset).**
   ✅ **CAUSA TROVATA (lettura codice), due bug distinti:**
   - **Shot**: la catena di aggiornamento **funziona gia'** — riordino → `applyTaskTypesToTechnique`
     (`ztoryproductionpanel.cpp:1655`) salva e emette `taskStatusChanged`, il pannello fa `rebuild()`
     (`:409`). Il problema e' a valle: `rebuild()` prende le colonne da
     `ZtoryModel::spreadsheetTaskColumns()` (`ztorymodel.cpp:962`), che **riordina tutto secondo
     `canonicalTaskOrder()` — una lista statica hardcoded** (`ztorymodel.cpp:951`), buttando via
     l'ordine del workflow; i task fuori dalla canon finiscono in coda in ordine di `std::set`
     (alfabetico), non del workflow.
   - **Asset**: piu' grave — `rebuildAssets()` fa
     `m_assetTaskCols = ZtoryModel::canonicalAssetTaskOrder()` (`:1358`), cioe' una lista **fissa di
     4 voci** `{Concept, Rough, Clean, Color}` (`ztorymodel.cpp:265`) che **non consulta MAI i
     workflow**. Gli asset non sono proprio collegati al Workflow tab.
   **Fix**: ordinare per sequenza-workflow invece che per lista canonica. Con piu' tecniche in
   gioco serve una regola di merge (proposta: ordine di prima apparizione scorrendo le tecniche,
   preservando l'ordine interno di ciascuna); la lista canonica resta come fallback per i tipi
   orfani. Per gli asset va deciso PRIMA se devono avere workflow propri o condividere quelli
   delle tecniche — e' una scelta di modello, non solo un fix.

---

### BUG (PRIORITÀ ALTA) — Animatic camera ≠ shot camera = CAMERA FIELD INCOERENTE ✅ CAUSA TROVATA

**Priorità: ALTA | Tipo: BUG | Causa radice trovata: 2026-05-31 (SB_APPENNINGERS)**

> 🎯 **CAUSA RADICE (confermata dall'utente):** NON è un bug di rendering. È il
> **field guide della camera incoerente**: main a **12 fld**, shot a **16 fld** (o
> mix). Field diverso = coverage/zoom diverso → l'animatic (camera main) non combacia
> con lo shot (camera sub). Il fix BUG-CAMERA `7d1746f3a` NON è da buttare: se main e
> shot hanno lo stesso field, l'animatic COMBACIA.
>
> **PERCHÉ alcuni shot a 12 e altri a 16:** ogni shot copia `res`+`size` (il field)
> dalla camera del MAIN **al momento della creazione**:
> - Add Shot → `ztoryanimatic.cpp:6092-6093`
> - Collapse → `subscenecommand.cpp:1531-1532`
> - Split/clone (razor) → eredita dallo shot sorgente, non dal main
> Se il field del main cambia nel tempo (12→16), gli shot creati prima/dopo divergono.
>
> **FIX (semplice, basso rischio — NON toccare il rendering):**
> 1. Utility "**Sync camere shot → main**": itera tutti gli shot, imposta
>    `subCamera->setRes/setSize` = camera main corrente. Un click sistema le scene
>    esistenti. (pulsante nel Board o comando)
> 2. (Opzionale) Bloccare il field alla creazione / avvisare se il main cambia field
>    con shot già esistenti.
> 3. Verificare poi se, con field coerente, il fix BUG-CAMERA va bene così com'è o
>    se serve comunque la camera sub per il POSIZIONAMENTO (non solo il field).
>
> **Test di conferma (da rifare su scena nuova, già impostato con l'utente):**
> A) crea shot → field = main; cambia field main; crea 2° shot → ha il nuovo field;
>    il 1° shot resta col vecchio. B) field coerente → animatic combacia. C) razor di
>    uno shot → i pezzi ereditano dal sorgente, non dal main.

**Priorità: ALTA | Tipo: BUG | A/B confermato: 2026-05-31 (scena SB_APPENNINGERS)**

> ⚠️ **CONCLUSIONE A/B (decisiva):** ricompilato il baseline `e8d4a1466` (post-fix
> BUG-CAMERA, PRIMA di tutto il lavoro del 30-31/05) → **il bug è presente anche lì**.
> Quindi **NON è una regressione del lavoro recente**: è il design del fix
> `7d1746f3a` stesso che è sbagliato per il caso generale.
>
> **Comportamento corretto da ripristinare (parole utente):** PRIMA del fix,
> tornando sul main l'animatic viewer **si aggiustava e combaciava con lo shot**
> (usava la camera SUB). Il fix l'ha fatto passare alla camera MAIN → ora resta
> "leggermente più stretto" e NON combacia (e non in modo uniforme su tutti gli
> shot). Post-fix è PEGGIO del pre-fix per il caso generale.
>
> **Vera direzione del fix:** l'animatic DEVE usare la camera della SUB-scena di
> ogni shot (così combacia) — MA capire e risolvere PERCHÉ con la camera sub SH010
> andava off-screen (offset x=13.4). Probabile doppio-conteggio dell'offset camera
> sub nel compositing sub→parent. NON ri-ancorare alla camera main (rompe tutti gli
> altri shot). Studiare se l'offset x=13.4 di SH010 era legittimo (shot inquadrato
> off-center) o un artefatto.

Il fix BUG-CAMERA di 2026-05-31 (commit `7d1746f3a`) ancorò i viewer always-main
(animatic + monitor) alla camera del **MAIN xsheet** anche dentro uno shot, per
risolvere un caso specifico: contenuto fuori schermo su SH010 (camera sub con
offset x=13.4). 6 punti in `sceneviewer.cpp`.

**Problema riaperto:** se gli shot hanno **camere diverse** (ognuno inquadrato a
modo suo nella propria sub-scena), usare SEMPRE la camera MAIN fa sì che l'animatic
mostri tutti gli shot con la stessa inquadratura → **non corrispondono alla camera
del singolo shot**. Confermato su ENTRAMBI i viewer (animatic + monitor), su più
shot, camere sia statiche che animate. Il fix precedente ha ottimizzato per UNO
shot ma ha rotto il caso generale.

**Tensione di design da risolvere:**
- Animatic DEVE riprodurre l'inquadratura (camera) di OGNI shot → serve la camera
  della SUB-scena per il rendering del cell di quello shot.
- Ma la camera sub causava contenuto fuori schermo (SH010) → capire PERCHÉ
  (probabilmente compositing sub→parent: la camera sub va applicata al contenuto
  della sub, poi il risultato piazzato nel parent; il fix invece bypassava la
  camera sub). Va rifatto il calcolo corretto, non bypassato.

**Approccio:** riprodurre con SB_APPENNINGERS (camere diverse per shot). Studiare i
6 punti del commit `7d1746f3a` in `sceneviewer.cpp` (drawBuildVars ~1443,
getViewMatrix CAMERA_REFERENCE ~2737, fitToCamera, getCameraRect, drawOverlay).
Obiettivo: animatic = camera del singolo shot, SENZA reintrodurre l'off-screen di
SH010 (capire quel caso: forse era un offset legittimo della camera sub).

**File:** `toonz/sources/toonz/sceneviewer.cpp` (6 siti del fix BUG-CAMERA).
**Crashlog/scena rif.:** project cs26_grottazzolina / SB_APPENNINGERS.

---

### ✅ RISOLTO 2026-09-16 — CRASH aprendo una scena con `role="shot"` (corruzione dell'heap)

**Causa, inchiodata da AddressSanitizer** (commit `ad558bdda`):
`ZtoryThumbnailCanvas::~ZtoryThumbnailCanvas()` faceva `delete m_style`. Ma
`m_style` non e' posseduto: e' lo stesso oggetto di `m_styleRef` (TColorStyleP),
gia' convertito — lo dice l'header, *"m_styleRef, already downcast"* — e
appartiene alla **palette dei pennelli**, che lo conta per riferimenti. Un istante
dopo, alla parentesi di chiusura del distruttore, `m_styleRef` si distruggeva e
decrementava il contatore DENTRO la memoria appena liberata: 8 byte scritti su un
blocco morto (`heap-use-after-free`, riga 346 scrive quel che la 345 ha liberato).

Quel `delete` era un **residuo**: veniva da quando il canvas si costruiva lo stile
da solo a partire da un percorso di file. Introducendo la palette (`4dcebf35c`,
notte del 15→16) e' cambiato il proprietario e la riga che liberava e' rimasta li'.

Le due condizioni necessarie, che per ore erano sembrate arbitrarie, ora si
spiegano da sole: serviva il **`.tpl`** (senza file non esiste nessuno stile della
palette) e serviva un **cambio di room** (e' cio' che distrugge il pannello, e
quindi il canvas).

**Verificato**: passata ASan con piu' aperture di scena e piu' ricostruzioni di
room, **zero segnalazioni**. Non «sembra andare»: l'accesso non avviene piu', e
lo dice uno strumento che misura invece di aspettare che il programma cada.

**Un secondo use-after-free, PROVATO e corretto nello stesso commit:** il primo
rapporto ASan mostrava `MainWindow::clearRooms()` → `QLayout::removeWidget` →
`QStackedLayout` che MOSTRA la pagina successiva, cioe' una room che si sta
cancellando; il suo `FunctionViewer` riceve `showEvent()`, espande l'albero e
legge `TStageObject` gia' distrutti. Ora lo stack viene nascosto mentre lo si
svuota. ⚠️ **La radice resta aperta**: `functiontreeviewer.h` tiene
`TStageObject *m_stageObject; // (not owned)` — puntatori grezzi a oggetti di
scena, senza nessuna garanzia che siano vivi quando l'albero si disegna. Io ho
chiuso una porta, non ho tolto il pericolo. E' codice condiviso con Tahoma.

**Quello che e' costato, e che vale la pena ricordare:** quattro ipotesi
plausibili, tutte sbagliate e tutte ritirate — rimandare lo switch fuori dal
gestore del clic; legare il QTimer a qApp; il flag morto in `mypaint.h`;
rimandare `loadBrushPalette()` fuori dal costruttore. Tre su quattro avevano
fatto aprire la scena **una volta**, e sembravano correzioni. Una corruzione
dell'heap si nasconde dietro un cambio di tempi con la stessa faccia con cui si
risolve. **Il punto in cui il programma cade non dice niente**: qui cadeva ogni
volta altrove (icone SVG, costruzione di pannelli, barra dei menu, animazioni di
finestra), perche' e' semplicemente la prima allocazione che incontra la lista
libera avvelenata.

**La lezione operativa: ASan prima, non dopo.** Costa quaranta minuti di
compilazione e li ripaga al primo rapporto. Lo script e la configurazione sono
piu' sotto, nel blocco che descrive come e' stata montata la build in
`/Volumes/ZioSam/tahoma2d-workspace/asan-build`.

### ✅ RISOLTO 2026-09-18 — con il link audio-video, l'undo non riportava indietro l'audio

Segnalato da Franco, corretto e **verificato da lui lo stesso giorno**: con il
link acceso si fa «Match Subscene Duration», l'audio segue gli shot, e ora ⌘Z lo
riporta esattamente dov'era.

**Causa.** Lo snapshot dell'undo del Board (`ZtoryShotSnap`) conteneva solo
`ShotData`, il livello e la durata: **dell'audio non sapeva niente**. Ma con
`m_audioLinked` acceso `ZtoryAnimaticPanel::resequenceXsheet()`
(`ztoryanimatic.cpp:8211`) sposta lo `startFrame` dei ColumnLevel audio per farli
seguire gli shot. L'undo rimetteva a posto il video, e l'audio restava dove
l'operazione l'aveva portato. Non era un ripristino difettoso: era uno stato che
l'undo non aveva **mai fotografato**.

> ⚠️ **La trappola, e il motivo per cui la prima idea ovvia era sbagliata.**
> Rifare lo spostamento al contrario NON ripristina lo stato:
> `shiftLevelFromFrame()` non e' invertibile. Quando uno spostamento a sinistra
> farebbe sovrapporre due livelli, **taglia** quello precedente
> (`setEndOffset`). Uno shift inverso rimette le posizioni e **lascia il
> taglio**. Per questo si salva lo STATO — `startFrame`, `startOffset`,
> `endOffset` per ogni ColumnLevel — e non il movimento.

**Correzione.** `captureSnapshot()` restituisce ora `ZtoryBoardSnap` (gli shot
**piu'** le posizioni dell'audio); `UndoBoardState` se le porta dietro.
`ZtoryBoardSnap` espone `empty()/clear()/size()/operator[]` che inoltrano agli
shot, quindi i ~44 punti di chiamata non sono cambiati: il tipo nuovo si
comporta come il vecchio dove il codice tratta lo snapshot come la sola lista di
shot, ed e' giusto cosi' — l'audio riguarda solo l'undo.

Due cautele, entrambe necessarie e non ornamentali:
- i livelli si ritrovano per **identita'** (`ColumnLevel*`), non per indice:
  `shiftLevelFromFrame` **riordina** `m_levels`. E se la struttura e' cambiata
  (un livello tagliato o aggiunto nel frattempo) quella colonna si salta:
  rimettere numeri su livelli diversi da quelli fotografati non e' un undo;
- undo e redo toccano l'audio **solo se le due fotografie differiscono**, cioe'
  solo se quell'operazione lo aveva davvero mosso. Senza questa condizione
  `beginExternalEdit()` — che tiene uno snapshot aperto a lungo — avrebbe potuto
  far spostare, a un undo qualsiasi, un audio che l'utente aveva mosso a mano
  nel frattempo.

**File:** `ztoryundo.h` (le strutture e le tre funzioni),
`storyboardpanel.cpp` (`ztoryCaptureAudioSnap` / `ztoryRestoreAudioSnap` /
`ztoryAudioSnapDiffers`, accanto a `UndoBoardState::undo`).

> **Sull'undo «faticoso»** (l'altra meta' della segnalazione): Franco lo ha
> trovato «molto piu' veloce» subito dopo, ma **senza che sia stata scritta una
> riga per la velocita'**. L'effetto di rimbalzo plausibile e' la correzione di
> `onMoveShot` dello stesso giorno: prima riscriveva `xsheetColumn` sbagliato, e
> da li' in poi `onModelResequenced` vedeva deriva e faceva la ricostruzione
> completa — che rilegge il `.ztoryc` dal DISCO, ed e' la stessa strada da cui
> passa `restoreFromSnapshot`. **Non misurato.** Se ricapita di sembrare lento,
> mettere un cronometro attorno a `restoreFromSnapshot` prima di ipotizzare.

---

### ✅ CORRETTO E COLLAUDATO 2026-09-24 — «Back to Animatic» lento: MISURATO con `sample`

> Franco: *«ottimo ora è immediato!»*

Nessuno dei due sospetti del 23 (51 ascoltatori, `loadZtoryc` a ogni
resequence). `sample` sull'app aperta, Franco clicca Back: **7,7 s in
`ZtoryAnimaticTrack::refreshFromScene`** a ridisegnare TUTTE le miniature degli
shot, e **il 77% di quel tempo e' creare un contesto OpenGL fuori schermo per
ogni miniatura** (`QtOfflineGL::createContext`, dentro
`IconGenerator::renderXsheetFrame` → `ToonzScene::renderFrame`). Il Monitor e'
la stessa classe con la sua cache: altri 4,3 s, sincroni dentro
`closeSubXsheet`. Causa: al ritorno sul main xsheet la cache si svuotava TUTTA
(«i disegni cambiano solo da dentro uno shot»), mentre puo' essere cambiato
solo lo shot da cui si esce. Correzione: si ricorda la chiave dello shot
aperto (`ZtoryShotOps::shotThumbKeyForXsheet`) e al ritorno si butta solo
quella — nella timeline (anche quella del Monitor) e nella StoryStrip.
Lo stesso metodo si rifa' dopo la correzione, per confermare.

🔍 **Visto di passaggio, NON affrontato:** zoomando/scorrendo la timeline,
`ZtoryAudioTrack::paintEvent` costa ~150 ms a ridisegno, quasi tutto in
`TSoundTrackT::getMinMaxPressure` (le forme d'onda si ricalcolano).

### ✅ CORRETTO E COLLAUDATO 2026-09-24 — i dialoghi scalavano di uno dopo Clone + Paste

> Collaudato da Franco lo stesso giorno. (Collaudato anche ⌘⌥0 nella Thumbs room.)

Segnalato da Franco: incollando uno shot a meta', dialoghi/azioni/note di tutti
gli shot dopo si sono spostati di uno. Causa: `loadZtoryc()` gira dopo ogni
resequence e rileggeva un `.ztoryc` salvato PRIMA dell'operazione assegnando
le voci PER POSIZIONE. Ora il file scrive per ogni shot il nome della sua
sotto-scena (`level=`) e la rilettura abbina per nome (in ordine per le copie
condivise); file vecchi senza nomi → posizionale come prima, finche' non vengono
risalvati. Lo shot incollato parte vuoto: **copiare i dialoghi del sorgente nel
clone NON e' fatto** — lavoro a parte, se Franco lo vuole.

### ✅ FATTO E COLLAUDATO 2026-09-24 — «Follow»: pannello del Board ↔ testina della timeline

> Franco: *«follow funziona, ottimo»*. Stato in `ZtoryModel::followEnabled()`
> (QSettings `Ztoryc/followBoardTimeline`), bottone nella barra della timeline
> e copia nel Board nascosta con gli altri bottoni degli shot. Il segno sul
> pannello NON e' la selezione (una barra-testina sul bordo alto), cosi' il play
> non trascina la selezione condivisa.
>
> **Ritocchi dopo l'uso (Franco, stesso giorno), collaudati:** il Board scorre
> SEMPRE, anche in play (la prima scelta «in play segna e non scorre» e'
> superata); la testina della timeline resta in vista come in DaVinci —
> scatto di pagina in play, centratura da ferma solo se fuori vista
> (`ZtoryAnimaticPanel::keepPlayheadVisible`); il segno del Follow e' ORO e
> disegnato sopra il bordo di selezione (l'arancione si confondeva). Posizioni degli shot in cache
> (`m_followSpans`), invalidate a xsheetChanged/modelReset.


Richiesta di Franco: cliccando un pannello del Board la testina della timeline
va al suo primo frame; spostando la testina (o cliccando nella timeline) il
Board evidenzia il pannello sotto la testina.
Decisioni sue:
- **in play evidenzia ma NON scorre**; da fermo evidenzia **e scorre** fino a
  rendere visibile il pannello;
- **un bottone solo** nelle room Ztoryc, col meccanismo gia' usato per non avere
  bottoni doppi fra Board e timeline;
- nome **«Follow»** — non «Sync» (si confonde col sync del sonoro) ne' «Link»
  (c'e' gia' A/V Link). Tooltip che dica le due direzioni.
Dati gia' presenti: `PanelData::startFrame/duration` (dentro lo shot) +
inizio VERO dello shot (`shotTrueSpan`, dissolvenze escluse), e la selezione
condivisa degli shot esiste gia' (`m_sharedSelection`).

### 🎯 DECISO 2026-09-24 — NIENTE nightly: build di prova SU RICHIESTA, e la regola della numerazione

Prima deciso «nightly si', con cautele», poi Franco ha chiesto come
funzionano davvero e ha cambiato idea: *«per come lavoriamo noi non credo sia
il metodo giusto»*. Su master lavoriamo direttamente e lui prova le cose prima
di chiunque: una build automatica pubblicherebbe cose non ancora viste da lui.
**Scelta: la «terza via»** — la stessa macchina, ma lanciata SOLO a mano:
- `.github/workflows/preview.yml`: `gh workflow run preview.yml` ricrea la
  pre-release **«preview»** (mai latest, commit nel titolo, avviso bilingue) e
  lancia `macOS_build.yml`/`windows_build.yml` con `preview=true`; li' un job
  a parte (`publish-preview`) carica i file `Ztoryc-preview-<commit>-…`. Il job
  del rilascio vero non e' toccato. **Rifiuta di partire se una build macOS
  gira**: il gruppo di concorrenza macOS e' per ref, annullerebbe un rilascio.
- About mostra il commit (`ztorycbuild.h`, generato da CMake) e, in una
  preview, «PREVIEW BUILD» in arancione.
- Si lancia **solo quando lo chiede Franco** — tipicamente per dare una
  correzione a un tester prima del rilascio (quello che prima era l'installer
  fatto a mano su Drive). **Mai provata in CI**: il primo lancio e' il collaudo.

**Numerazione e ritmo** (scritti nella checklist di rilascio di AGENTS.md):
patch = correzioni e ritocchi; minor = funzione nuova, comportamento che
cambia, o cambio nel salvataggio dei dati. Si rilascia per gravita', non per
calendario. **La prossima e' la 0.15.0**: Follow, «chiudi senza salvare» dei
thumbs (le scene ora chiedono di salvare anche per i soli thumbs), ⌥0 che
cambia significato, `.ztoryc` con i nomi delle sotto-scene, base Tahoma 1.6.3.

### ✅ CORRETTO E CONFERMATO 2026-09-24 — ⌘C dalla sceneggiatura copiava la frase di prima

> **Le cause erano DUE**, la seconda trovata con una sonda sul focus: oltre al
> campo in sola lettura, `SceneViewer::onEnter()` si prendeva la tastiera
> quando il mouse lo attraversava (risparmiava solo i `QLineEdit`). Corretto
> li' anche per `QTextEdit`/`QPlainTextEdit` — candidato upstream. Confermato
> da Franco: 50 ⌘C su 50 al testo. Probabilmente e' anche il backspace che gli
> aveva cancellato uno shot mentre voleva cancellare del testo.

Segnalato da Franco, intermittente. Causa verificata sul sorgente di Qt 5.15
(non dedotta): un `QTextEdit` in sola lettura non rivendica le scorciatoie
(`qwidgettextcontrol.cpp:1183`, solo se `TextEditable`), quindi ⌘C andava al
Copy dell'applicazione quando era attivo. Corretto in `ztoryscriptpanel.cpp`
con un filtro che rivendica Copy e Select All. Intermittente per natura: si
conferma solo usandolo.

### 🎯 DECISO 2026-09-24 — ⌥ + tasto centrale ruota la vista in TUTTE le room, e ⌥0 raddrizza soltanto

Franco: *«la rotazione option + tasto centrale confermo che la vorrei di
default anche nelle altre room»*, e poi, correggendo la mia prima versione:
*«devi semplicemente mettere quella combinazione come shortcut di default del
comando rotate tool nativo»*. Quindi NIENTE rotazione scritta da noi:
⌥ + pressione del centrale accende la modalita' temporanea NATIVA del Rotate
tool (`m_mouseRotating`, stessa `mouseRotate()`, stesso cursore), spenta al
rilascio con `m_resetOnRelease`. Una scorciatoia di tastiera non puo'
contenere un tasto del mouse, per questo sta in `SceneViewer::onPress`.
**Collaudato da Franco: funziona.**

**⌥0 = Reset Rotation ovunque** (scelta di Franco: *«è comodo resettare la
rotazione senza dover resettare anche lo zoom»*), come nella Thumbs room.
Reset View passa a **⌘⌥0** (Ctrl+Alt+0), NON a ⌥⇧0: su tastiera italiana
Maiusc+0 e' «=» e macOS abbina le scorciatoie per carattere. Cambiati i default
in `mainwindow.cpp` e nel preset `deftahoma2d.ini`; chi ha scorciatoie
personali tiene le sue.

⚠️ La prima versione (rotazione scritta a mano in `onMove`) e' **sparita dal
file** prima del collaudo senza che si sia capito come — niente git, le altre
modifiche intatte. Da allora, prima di ogni deploy, si controlla nel sorgente
che le modifiche ci siano, non solo che compili.

### ✅ FATTO E COLLAUDATO 2026-09-24 — i thumbs obbediscono a «chiudi senza salvare»

> Collaudato da Franco: Discard, ⌘S + riapertura, uscita forzata + recupero.

Implementata la strada 2 (dettagli nel CHANGELOG del 2026-09-24): copia di
lavoro in `thumbs/_ztorythumbs_working/`, ufficiale al salvataggio della scena
(`ZtoryModel::sceneSaved`, emesso da `IoCmd::saveScene`), buttata a
`sceneSwitching`/`aboutToQuit`, recuperata con una domanda se l'ha lasciata un
crash. Le modifiche ai thumbs ora segnano la scena come modificata (non lo
facevano: senza, «non salvare» non poteva funzionare).

### 🎯 DECISO 2026-09-23 — i thumbs devono obbedire a «chiudi senza salvare»

**Segnalato da Franco:** disegna nella Thumbs room, chiude SENZA salvare per
tornare alla scena com'era, e riaprendo si ritrova i thumbs che non voleva.

**Causa, non una svista.** La tela ha una persistenza SUA: 700 ms dopo
l'ultima pennellata scrive le bande PNG su disco, senza aspettare il
salvataggio della scena. Quell'autosalvataggio esiste apposta — la tela e'
grossa, ridisegnarla costa, e perderla per un crash sarebbe peggio. Ma il
prezzo e' che **la scena non e' piu' un'unita'**: una parte obbedisce a «non
salvare» e l'altra no.

**SCELTA DI FRANCO: la strada 2** — scrivere in un'area d'appoggio e
consolidare al salvataggio della scena.
- l'autosalvataggio continua a proteggere dal crash, ma finisce in una copia
  di lavoro;
- salvando la scena, la copia di lavoro diventa ufficiale;
- chiudendo senza salvare, si butta;
- alla riapertura, se c'e' una copia di lavoro piu' recente dell'ufficiale,
  si puo' chiedere all'utente se recuperarla — come dopo un crash.

Scartate: legare i thumbs al salvataggio della scena (rinuncia alla
protezione dal crash, che e' il motivo per cui l'autosalvataggio esiste) e
lasciare com'e' documentandolo (la sorpresa resta, per chiunque).

⚠️ **Non fatto il 2026-09-23 di proposito:** tocca il percorso di persistenza,
quello che se sbaglia non fa rumore, e c'era un rilascio in corso.

---

### 🎯 DECISO 2026-09-23 — la posizione dell'audio e' DERIVATA, non memorizzata

**Principio, parole di Franco:** *«se l'audio inizia al frame 5 dello shot 02,
qualsiasi operazione faccio sempre da quel frame dovra' iniziare»*.

Cioe' la posizione di un segmento audio non e' un dato assoluto sulla linea
del tempo: e' la coppia **(shot di ancoraggio, scarto in frame dal suo
inizio)**. La posizione assoluta si RICALCOLA, non si conserva.

**Il codice di oggi fa l'opposto** e da li' viene tutta la famiglia di difetti
inseguita il 2026-09-23: tiene la posizione assoluta e cerca di aggiornarla a
ogni modifica spostando tutto di un **delta comune** a partire da un punto
(`resequenceXsheet` + `shiftLevelFromFrame`). E' un'approssimazione che regge
solo finche' tutti gli shot si spostano della stessa quantita'.

**La conseguenza piu' importante: se la posizione e' derivata, non va ne'
fotografata ne' ripristinata.** Sparisce tutto il meccanismo che stavamo
debuggando — `ZtoryAudioColSnap`, `ztoryCaptureAudioSnap`,
`ztoryRestoreAudioSnap`, la guardia `ztoryAudioSnapDiffers`, e il conflitto fra
quel ripristino e lo spostamento del resequence che agiscono sulla stessa cosa
nello stesso istante. Si annulla il video e l'audio si ricalcola da se'.

**Misurato il 2026-09-23, sonde su entrambi i percorsi:**
- i valori ripristinati sono **esatti** e la catena annulla/ripeti **coerente**
  (75 → 32 → 86 → 30, e i ritorni combaciano);
- ma fra un annullamento e il successivo l'audio si sposta **da solo** (27 → 40)
  senza che nessun ripristino lo tocchi: e' il resequence;
- e da li' in poi il ripristino riscrive **lo stesso valore che trova**, quindi
  non corregge piu' niente mentre il video continua a tornare indietro.

**Le due domande sono state DECISE da Franco il 2026-09-23:**
1. l'ancoraggio e' al **primo shot che il segmento tocca**;
2. **trascinando l'audio a mano si sposta anche l'ancora**.

⚠️ **PRIMO TENTATIVO SCRITTO E MESSO DA PARTE la sera del 2026-09-23.** La
patch e' in `/Volumes/ZioSam/tahoma2d-workspace/ancore-audio-WIP.patch` (94
righe, su `ztoryanimatic.cpp`). Calcola le ancore prima di ricompattare e
ricalcola le posizioni dopo, al posto del delta comune. **NON rilasciata**,
perche' incompleta in un punto trovato rileggendola:

> `shiftLevelFromFrame()`, che sostituisce, dopo aver spostato i livelli li
> **RIORDINA** (`std::sort`). La versione con le ancore sposta e non riordina,
> e `m_levels` e' privato: da fuori non si puo'. Una lista fuori ordine manda
> in confusione chi disegna la forma d'onda, chi suona e chi legge le celle —
> ed e' con ogni probabilita' il «continua a fare cose strane» di Franco.
> `checkColumn()` non aiuta a scoprirlo: e' `#ifndef NDEBUG`.

**Quindi la riscrittura va fatta DENTRO `TXshSoundColumn`**, dove si possono
riordinare i livelli — non nel pannello animatic. Un metodo tipo
`setLevelVisibleStart(ColumnLevel*, int)` che sposta, riordina e lascia al
chiamante la politica sulle sovrapposizioni.

⚠️ Fino ad allora: **col link A/V spento non succede nulla di tutto questo.**

✅ **FATTO la sera del 2026-09-23, COLLAUDATO da Franco il 2026-09-24.** Tre pezzi:
1. `TXshSoundColumn::setLevelsVisibleStart()` — sposta ogni livello al suo
   frame, riordina, e una sovrapposizione taglia la coda del precedente
   (se non ne resta niente lo toglie). E' il metodo «dentro la colonna» che
   mancava alla patch WIP.
2. `resequenceXsheet` col link: ancore prese PRIMA di ricompattare (primo shot
   toccato, inizio VERO), posizioni ricalcolate dopo. Il delta comune e'
   sparito — nel roll trascinava l'audio di C, D… insieme a B.
3. **La causa del «continua a fare cose strane» era un'altra**, trovata nel
   log `ztprobe_undo2.log`: lo snapshot del Board riconosceva i livelli audio
   per INDIRIZZO, e annullare/ripetere un'operazione sull'audio
   (`UndoAudioEdit` → `assignLevels`) li ricrea tutti. Da li' ogni annulla
   video saltava la colonna in silenzio (27 → 40, poi 40 su 40 per sempre).
   Ora lo snapshot copia i livelli per valore e il ripristino ricostruisce la
   colonna intera (`replaceLevels`): torna anche un segmento tagliato.

🎯 **DECISO da Franco il 2026-09-24: l'audio SPORGE oltre la fine del video.**
Se un roll sposta avanti l'inizio dell'ultimo shot, il segmento ancorato a lui
lo segue e puo' finire dopo l'ultimo frame video (misurato: audio fino a 181,
video a 167); l'animatic si allunga fino alla fine dell'audio. **Non si taglia
alla fine del video**: sarebbe un taglio che l'utente non ha chiesto e non vede.
Come in un montaggio: la parte che sporge si vede e si taglia a mano.

Resta fuori, di proposito: **cambiare la durata di una dissolvenza** non muove
l'audio (usa il resequence senza link). Con le ancore sarebbe naturale
aggiungerlo, ma e' un comportamento nuovo: si decide dopo il collaudo.

---

### 🔴 APERTI 2026-09-23 — dissolvenza: il roll sbaglia giuntura, e il link A/V sposta l'audio dalla parte sbagliata

Segnalati da Franco. **Stessa radice**, e non sono regressioni di questa
sessione: il difetto c'e' da quando esistono le dissolvenze.

**Sintomi.**
0. Tagliando l'AUDIO con una dissolvenza fra due shot, compare un **buco**
   fra i due blocchi nel track (schermata di Franco, 2026-09-23).
   *Spiegazione plausibile, NON verificata:* i blocchi sono disegnati alle
   durate vere, che escludono gli extra della dissolvenza; ma quegli extra
   occupano righe reali (coda di A + stop + testa di B), quindi fra la fine
   vera di A e l'inizio vero di B c'e' materialmente uno spazio. Di norma lo
   copre il marcatore della transizione: nella schermata la diagonale sta
   solo sul blocco di sinistra e non scavalca. Il taglio audio chiama
   `notifyXsheetChanged()` → rifacimento dei blocchi: il sospetto e' che li'
   la transizione non venga ridisegnata a cavallo.
   ⚠️ NON puo' venire dalla correzione del link A/V del 2026-09-23: quella
   sposta solo i livelli audio, non tocca le colonne video.
1. Con una dissolvenza, un **trim/roll** fa saltare il punto di giuntura
   qualche frame **indietro**, verso la dissolvenza.
2. Col **link audio/video**, allungando uno shot verso destra l'audio si
   sposta a **sinistra** e taglia il segmento audio precedente.

✅ **1 e 2 CORRETTI il 2026-09-24** (collaudati da Franco, sonda su 30 roll:
durate ottenute = chieste, fine del video ferma). Tre cause, non una:
- **roll con dissolvenza su uno shot PRIMA**: `onRollEdit`/`resizeCol`
  misurava col `getRange` lordo (frame di testa della dissolvenza compresi)
  contro durate nette: chiesto 60, ottenuto 56, la fine dell'animatic arretrava
  di mezza dissolvenza a ogni roll. Ora smonta la dissolvenza prima di
  misurare, come gia' faceva il trim;
- **roll/trim che allunga A attraverso la giuntura con dissolvenza**:
  `teardownCrossDissolves` cancellava `[X, X+half]` di A anche a dissolvenza
  NON esposta, mangiandosi i frame appena aggiunti. Ora salta la coppia se la
  testa di B non e' esposta;
- **audio a sinistra / audio di C trascinato da B**: il delta comune, sostituito
  dalle ancore (vedi la voce DECISO qui sopra).
✅ **Sintomo 0 CORRETTO il 2026-09-24** (`ad3184c65`, collaudato da Franco):
`shotTrueSpan` toglieva la meta' di coda appena la sotto-scena aveva la nota
XD-out, anche a dissolvenza NON esposta (ultimo shot, meta' che non combaciano,
smontaggio in attesa del resequence) → blocco A corto e buco prima di B. Il
taglio audio faceva solo ridisegnare. Ora la coda conta se la testa dello shot
dopo e' esposta. Nello stesso commit: il **mark out** all'apertura dello shot
era shot + dissolvenza INTERA (la colonna contiene gia' la meta' esposta, e le
note la risommavano) — ora shot + meta' per lato, anche nel ripiego di
MI_OpenChild.

**Radice, accertata leggendo il codice.** `shotTrueSpan()` esiste proprio per
questo, e il suo commento lo dice: *«Both the animatic track and any duration
consumer must use this instead of a raw getRange(), or the overlap inflates
durations»*. I **blocchi del track** lo usano (riga 2488), quindi si VEDONO al
posto giusto. I percorsi di **scrittura** no:

| punto | cosa sbaglia |
|---|---|
| `onRollEdit` → `resizeCol` (riga ~7667) | `curDur` da `getRange` grezzo: con una dissolvenza e' gonfiato dalla sovrapposizione, e la giuntura calcolata da li' cade indietro |
| `resequenceXsheet` (righe ~8243 e ~8263) | misura le posizioni prima/dopo con `getRange` grezzo: lo «spostamento» non e' di quanto si e' mosso lo shot ma di quanto e' cambiata la sovrapposizione, e puo' risultare NEGATIVO mentre il video si allunga a destra |

Il secondo spiega il taglio dell'audio: lo spostamento finisce in
`shiftLevelFromFrame()`, che **non e' invertibile** — quando uno spostamento a
sinistra farebbe sovrapporre due livelli, TAGLIA il precedente
(`setEndOffset`). Con delta negativo sbagliato, taglia audio buono.

⚠️ **Il link A/V ha anche un'assunzione a parte da verificare**: prende lo
shot piu' a sinistra che si e' mosso e applica IL SUO spostamento a tutti
(*«resequence packs tightly, so a single duration change propagates
uniformly»*). Con le true span quell'uniformita' dovrebbe tornare vera, ma va
misurato: se con le dissolvenze gli shot si spostano di quantita' diverse, il
delta unico resta sbagliato anche dopo la correzione.

⚠️ **NON e' una sostituzione di una riga.** `resizeCol` usa la durata e poi
MANIPOLA LE CELLE nell'intervallo grezzo: cambiando solo la durata
l'aritmetica diventa incoerente e si inserirebbero frame dopo la coda della
dissolvenza invece che dentro lo shot. Va rifatta tenendo separati i due
intervalli — quello vero e quello con gli extra — e provata con una
dissolvenza in mezzo.

✅ **Proprieta' che rende la correzione a rischio basso:** senza dissolvenza
`shotTrueSpan` coincide con `getRange(ignoreLastStop=true)` — gestisce
esplicitamente il caso «niente extra, solo lo stop». Quindi la modifica non
cambia nulla nelle scene senza transizioni.

⚠️ **Non toccare le altre 30 `getRange` del file alla cieca**: molte sono
giuste. `onTransitionChanged` per esempio manipola proprio le celle della
dissolvenza e la durata grezza gli serve.

---

### ✅ FATTO 2026-09-24 — merge Tahoma2D 1.6.3 (in master da `a4f54e53b`)

Rilasciata prima la **0.14.2** (pubblicata la notte del 24, nove pacchetti,
DMG Apple Silicon montato: `codesign` exit=0). Poi il merge, sul branch
`merge/tahoma-1.6.3`: parentela 1.6.2 registrata con `-s ours` (`b19611d1c`),
12 conflitti risolti (`15e137725`), **tre workflow verdi sul branch** (macOS,
Windows, Linux gcc+clang, senza pubblicare), e solo dopo fuso in master.
Esce con la prossima versione. ⚠️ **Non ancora provato a mano**: la CI dice
che si compila e si impacchetta, non che l'app si comporta — un giro di prova
di Franco sulla `Ztoryc.app` aggiornata prima della prossima release.

La nota storica qui sotto resta per il ragionamento sulla 1.6.2.

### (storico) DA FARE, IN QUEST'ORDINE — merge Tahoma2D 1.6.3

**Ordine deciso da Franco il 2026-09-23:** prima il rilascio 0.14.2, poi il
**lavoro sulla Thumbs room finito** (raster per pagina), poi questo merge.
Non prima: tocca `CMakeLists`, script di CI e installer, cioe' la categoria
che la checklist vieta di mergiare a ridosso di un rilascio.

⚠️ **PRIMA DEL MERGE VA REGISTRATA LA PARENTELA DELLA 1.6.2.** Non e' mai
stata registrata: il contenuto e' in master ma `v1.6.2` non e' un antenato, ne'
di master ne' del branch `merge/upstream-1.6.2`. Quindi git riparte dalla base
reale `4a2ec7305`, **91 commit** indietro, e rifa' tutta la 1.6.2 su codice che
quelle modifiche ce le ha gia'.

**Misurato il 2026-09-23** (confronto a tre fra base, v1.6.2 e master sui 293
file cambiati fra base e 1.6.2):

| | |
|---|---|
| contenuto 1.6.2 presente in master | 183 |
| contenuto 1.6.2 **mancante** | **0** |
| nostre modifiche sopra | 86 |
| assenti perche' tolti da noi (kiss_fft130) | 22 |
| assenti davvero | 2 (`tips.md` portoghese e russo) |

Zero contenuto perso: la dichiarazione `git merge -s ours v1.6.2` e' onesta.
**Provato in un worktree usa-e-getta:** conflitti da **39 a 12**, e i 12 sono
tutti file nostri (CI, installer, `tversion.h.in`, il `CMakeLists` con i
sorgenti Ztoryc, e `CHANGELOG.md` che collide solo di nome). **Zero conflitti
nel codice C++.**

**Cosa porta la 1.6.3** (7 commit, tutte correzioni). Due ci riguardano:
- **tolto il requisito di versione su OpenCV** + compilazione con 4.11+/5.x —
  noi OpenCV lo usiamo per l'import da foglio di carta e oggi si chiede 4.1;
- **build libgphoto2 su macOS** — la libreria che ci ha gia' dato guai nel
  confezionamento.
Piu' il salvataggio dei parametri di cleanup e correzioni sugli stili
vettoriali.

---

### ✅ CHIUSO (rilasciato nella 0.14.2) — col TOCCO, pan/zoom nella Thumbs room cancella i disegni

> Correzioni `c48d36f06` (il pizzico letto come ANNULLA), `8ad228feb` (la pila
> sopravviveva al cambio scena), `b09dfb2c7` (la riallineata alla camera fuori
> dall'annullamento). Collaudato da Franco sulla Companion 2, uscito nella
> 0.14.2 il 2026-09-24. Il testo sotto resta per la diagnosi.

Segnalato dall'utente Surface. **Distrugge lavoro.** Col mouse non succede, e
il tocco e' nuovo nella 0.14.1: e' una regressione nostra.

**Causa accertata nel codice (ma forse non tutta la causa).** Portando
`touchEvent`/`gestureEvent` da SceneViewer il 2026-09-18 e' andata persa
`m_touchPoints = 100` nel ramo dello zoom (in originale con accanto il
commento «This will block undo/redo action», `sceneviewerevents.cpp:1390`), e
la stessa riga nel ramo `CenterPointChanged`. Senza, dopo un pizzico
`m_touchPoints` resta 2 e al rilascio `mouseReleaseEvent` lo legge come tap a
due dita = **ANNULLA**, che su Windows e' la preferenza predefinita.
Spiega perche' l'utente non rimediava con l'undo: il danno ERA un undo.

⚠️ **Cosa NON torna, da non dimenticare:** un undo annulla **un passo solo**,
mentre l'utente descrive un wipe **in un colpo**. E c'e' gia' una guardia che
scarta i tocchi oltre 250 ms, che dovrebbe coprire un pizzico lento — a meno
che su Windows il `TouchEnd` non arrivi piu' quando Qt riconosce il gesto.
Quindi il secondo sospetto resta in piedi: `onSceneChanged` rifonde la griglia
e `xsheetCameraRes` **ripiega su 1920x1080 in silenzio**, in modo
indistinguibile da una camera 16:9 vera.

**Stato:** correzione su master (parita' con SceneViewer ripristinata).
**Build di diagnosi** sul branch `feature/thumbs-paged-raster` (`914707b79`),
SENZA la correzione, con sonde su entrambi i sospetti che scrivono in
`Desktop/ztprobe.log`. Build Windows su Drive in `Ztoryc/build-di-prova/`.
**Franco la prova sulla Companion 2** (unico touch raggiungibile).

⚠️ **Quella build ha anche la rotazione nuova**, il cui ramo mette
`m_touchPoints = 100`: pizzicando storto il difetto viene MASCHERATO. Il
pizzico di prova va fatto dritto.

**Serve una 0.14.2** appena la causa e' chiusa: la 0.14.1 distrugge lavoro su
qualunque macchina col tocco e le gesture accese.

---

### 🔴 APERTO 2026-09-21 — l'audio dell'animatic: muto alla ripresa, doppio se si riparte subito

> **2026-09-24, nuovo indizio (Franco):** dopo aver spostato l'uscita del Mac
> dagli altoparlanti alle cuffie, facendo play di tutta la timeline l'audio a
> volte si ammutoliva mentre il video andava avanti; fermando, tornando indietro
> e ripartendo, riprendeva — ma lo SCRUB usciva dagli altoparlanti e il play
> dalle cuffie. Causa del secondo, dal codice: `TSoundOutputDeviceImp` crea il
> `QAudioOutput` una volta e lo ricrea solo se cambia il FORMATO, e un
> `QAudioOutput` resta legato all'uscita attiva quando e' nato. Corretto il
> 2026-09-24 (ricreato se cambia il dispositivo di sistema; candidato
> upstream). **Da vedere se il muto sparisce con la stessa correzione**: e'
> plausibile (un dispositivo rimasto sull'uscita vecchia), ma lo stesso
> sintomo c'era gia' il 21 senza cambi di uscita — non darlo per chiuso.
>
> **Stesso giorno, collaudo di Franco:** l'uscita giusta ora e' seguita
> (✅ chiuso), ma il MUTO resta, e secondo lui arriva **in un momento di
> silenzio**. Ipotesi (NON misurata): il dispositivo Qt lavora «a spinta» e si
> riempie solo su `notify()`, che Qt emette solo mentre consuma audio; un
> buffer che si svuota (UI occupata >100 ms) manda l'uscita in IdleState,
> `notify()` smette, nessuno riempie piu' → muto fino al prossimo play.
> Sonda ZTPROBE-AUDIO2 in `tsound_qt.cpp` (stati dell'uscita, play, stop) →
> `~/Desktop/ztprobe_audio2.log`.
>
> **MISURATO lo stesso giorno — ipotesi confermata.** Alle 15:29:13.625 le
> due uscite (una per traccia) sono andate in IdleState + UnderrunError nello
> stesso millisecondo, dopo 18,1 s di play (indice 2,6 MB su 64 = 18,1 s), e
> non sono piu' ripartite fino allo STOP 12 s dopo. **Corretto:** su
> IdleState+Underrun con audio ancora da suonare si richiama `sendBuffer()`
> (candidato upstream). **Collaudato da Franco: non si ammutolisce piu'.**
> Resta da capire COSA blocca
> l'interfaccia per >100 ms a quel punto del play (Franco: «nei silenzi») —
> con `sample` durante il play, se il buco si sente.

Segnalato da Franco mentre lavorava nella room Ztoryc con l'animatic:
*«se interrompo e riprendo il play a volte va in play senza audio e se stoppo
e riprendo subito a volte si sente l'audio doppio»*. ✅ **Audio doppio: RISOLTO, confermato da
Franco il 2026-09-24** (il muto era gia' chiuso da `8b692cb04`). **Intermittente** — le due
facce sono probabilmente lo stesso difetto visto da due lati.

⚠️ **IPOTESI, non misurata.** Scritta per dare un punto di partenza, NON da
credere finche' non e' verificata con una sonda sulla riproduzione vera.
`ZtoryAnimaticController::startPerColumnAudio()` (`ztoryanimatic.cpp:305`)
chiama `sc->play(colTrack, ts0, colSamples-1, false)` per ogni colonna audio:
un buffer lungo fino alla fine della traccia. `stopPerColumnAudio()` (:332)
chiama `sc->stop()`. Il commento a :3881 dice che `TSoundOutputDeviceImp` usa
`QAudioOutput` con un **buffer hardware da 100 ms** — cioe' esiste una finestra
in cui lo stop non ha ancora rilasciato il dispositivo. Un `play()` dentro
quella finestra spiegherebbe **tutte e due** le facce: o il nuovo non parte
(muto), o parte sovrapposto al precedente (doppio). Combacia con il
«se riprendo SUBITO» di Franco, ed e' l'unica parte del racconto che indica
un tempo.

**Come si misura** (prima di toccare qualsiasi cosa): `qWarning` con il tempo
in ms all'ingresso di `startPerColumnAudio` e `stopPerColumnAudio`, piu' lo
stato di ogni `sc` al momento del play. Serve il caso muto E il caso doppio:
se la distanza fra stop e play successivo e' sotto i 100 ms nei casi rotti e
sopra in quelli buoni, l'ipotesi regge; altrimenti cade e si riparte.
`qWarning` e non `printf` — stdout rediretto e' bufferizzato a blocchi e la
traccia non arriverebbe.

⏸️ **Rimandato da Franco il 2026-09-21**, subito dopo averlo segnalato: la
sonda richiede di ricompilare e rilanciare la `Ztoryc.app` in cui stava
lavorando, e ha preferito non interrompere il lavoro sulla Thumbs room.
Non e' sfuggito: e' stato visto e messo in coda.

---

### ✅ CHIUSO (rilasciato nella 0.14.2) — ROTAZIONE DELLA VISTA nella Thumbs room

> Collaudata da Franco sulla Companion 2 (pizzico, segno corretto in `c142845a2`)
> e col mouse (⌥ + tasto centrale, `ceffd8380`). Uscita nella 0.14.2.

> ✅ **Implementata** sul branch `feature/thumbs-paged-raster`: tutti e quattro
> i pezzi qui sotto. A rotazione zero e' stato MISURATO che il codice nuovo da'
> gli stessi numeri del vecchio (scarto 0 px su 120 casi, sonda compilata
> contro le Qt vere). Tasti: ⌥← ⌥→ ruotano di 15°, ⌥0 raddrizza.
> **Non collaudata su un touch**: il segno del pizzico e' ragionato, non
> misurato. Manca un pulsante nella barra della room: ⌥0 non lo indovina
> nessuno.

### (piano originale)

Chiesta da Franco il 2026-09-18, dopo la prova sulla Wacom Companion 2: il
pizzico a due dita zooma ma non ruota, mentre nelle altre room ruota. **Da fare
nella stessa sessione del raster per pagina** (decisione di Franco).

⚠️ **Prima avevo risposto di no, e la risposta era sbagliata.** Avevo in testa
«ruotare il contenuto DENTRO la griglia», che in effetti la disallineerebbe.
Franco parla di ruotare **la VISTA**: la griglia ruota insieme al foglio, non si
disallinea niente, ed e' il gesto di girare il foglio sul tavolo mentre si
disegna — utile davvero, e presente in ogni programma di disegno.

**Misurato il 2026-09-18** (`ztorythumbnailcanvas.cpp`): la conversione passa
gia' quasi tutta dalle due funzioni ufficiali (33 usi di `worldToWidget` /
`widgetToWorld` / `widgetToRaster`), ma **una quindicina di punti danno per
scontato che la trasformazione sia solo scala + traslazione**. Il caso tipico,
negli overlay di griglia, fusioni e selezioni:

```cpp
const QRectF sr(worldToWidget(wr.topLeft()),
                QSizeF(wr.width() * m_zoom, wr.height() * m_zoom));
```

Sotto rotazione un rettangolo del mondo non e' piu' un rettangolo dritto sullo
schermo: quello e' sbagliato per costruzione.

✅ **La correzione e' una SEMPLIFICAZIONE, non trigonometria in piu':** si
costruisce **una** `QTransform` (traslazione × rotazione × scala), la si da' al
painter e si disegna in **coordinate del mondo** — `p.drawRect(wr)` invece di
calcolare `sr`. Sparisce aritmetica. Le penne vogliono `setCosmetic(true)` per
non ingrossarsi con lo zoom (molte lo sono gia').
**Il modello e' gia' in casa**: `paintFloat()` costruisce gia' un `world2widget`
e lo passa al painter.

**I quattro pezzi:**
1. `m_rot` + una `viewTransform()` sola, e la sua INVERSA per l'input
   (`widgetToWorld` / `widgetToRaster`);
2. ridisegno: `paintEvent`, `paintFloat` e tutti gli overlay passano dalla
   trasformazione invece di calcolare rettangoli dritti;
3. **barre di scorrimento**: con la pagina storta vanno ricalcolate sul riquadro
   ruotato (oggi `updateScrollBars` assume assi paralleli);
4. il pizzico che scrive l'angolo, **piu' un comando «azzera rotazione»** —
   senza, tornare dritti a mano e' una tortura, ed e' il motivo per cui ce l'hanno
   tutti.

✅ **DECISO da Franco il 2026-09-18: ruota TUTTO INSIEME, numerini compresi.**
E' il foglio che gira sul tavolo: se i numeri restassero dritti sembrerebbero
appiccicati allo schermo invece che alla pagina.

⚠️ **Si lavora sul branch e si compila su `Ztoryc-SP.app`.** Tocca il percorso
di disegno da cui dipende tutta la room: sbagliando li' non si rompe la
funzione nuova, si rompe il canvas anche per chi la rotazione non la usa.

---

### ✅ CHIUSO — tocco e gesti nella Thumbs room (commit `356a3645a`)

> Confermato da Franco sulla Companion 2 insieme alla rotazione; nella 0.14.2.

Segnalato da un **utente Surface**: nella Thumbs room un dito che prova a
spostare la tela ci disegnava sopra col pennello attivo. Corretto il 2026-09-18
portando `touchEvent`/`gestureEvent` da `ImageViewer` (imageviewer.cpp) e il ramo
`TapGesture` da `SceneViewer`, piu' il rifiuto del palmo che nessuno dei due ha.

⚠️ **NON VERIFICATO DA NESSUNO.** Ne' Franco ne' io abbiamo uno schermo touch
raggiungibile: il Mac e' un mini senza trackpad, e la Wacom Companion 2 non e'
utilizzabile. Il 2026-09-18 Franco ha mandato l'installer Windows
(build da `356a3645a`) **direttamente a chi aveva segnalato il problema**:
la conferma puo' arrivare solo da li'.

**Cosa chiedergli, perche' «sembra a posto» non basta.** Sono tre cose diverse
che possono rompersi separatamente:
1. **un dito sposta la tela** senza lasciare il segno del pennello — e' la
   segnalazione originale;
2. **la penna disegna con la mano appoggiata allo schermo** — il rifiuto del
   palmo. E' l'unico dei tre che, se e' sbagliato, ROVINA un disegno invece di
   dare fastidio: la tela scivolerebbe sotto il segno;
3. **il pizzico zooma**, e in **Seleziona/Trasforma** la penna muove ancora le
   selezioni — sono le uniche due modalita' in cui la penna passa dal mouse
   sintetizzato, cioe' dove sta la guardia nuova.

Se il 2 fallisce, si torna indietro subito: e' l'unico che fa danno.

---

### ✅ RISOLTO 2026-09-18 — il lazo premoltiplicava DUE volte: bordo scuro e colore mangiato

Segnalato da Franco in due tempi, e **il secondo pezzo e' quello che ha risolto
il caso**: prima «resta un bordo scuro attorno alla cancellatura», poi —
riaprendo la scena — «nell'area cancellata il disegno ha perso il colore».
Col solo bordo c'era un fatto e nessuna spiegazione; un colore che diventa
grigio MANTENENDO la trasparenza, invece, viene da una causa sola.

**Causa.** I tre percorsi della selezione flottante (`liftFloatLasso`,
`commitFloat`, `cancelFloat`) fanno il giro raster → QImage → raster cosi':

```cpp
QImage img = rasterToQImage(m_ras, /*premul=*/true, ...);
... lavoro col painter ...
m_ras = rasterFromQImage(img, /*premul=*/true, ...);   // ← SBAGLIATO
```

`rasterToQImage(premul=true)` **non converte**: ETICHETTA i byte come gia'
premoltiplicati, e il painter lavora in quello spazio. L'immagine che torna
indietro e' quindi **gia'** premoltiplicata. Ma `rasterFromQImage(premultiply=
true)` chiama `TRop::premultiply()`, che moltiplica i canali per l'alpha
**un'altra volta**. Corretto passando `false`: la conversione non serve, i dati
sono gia' nello spazio giusto.

**Perche' sembrava capriccioso** (e perche' a un certo punto «non lo faceva
piu'»): sui pixel OPACHI la doppia premoltiplicazione non cambia nulla, quindi
il grosso del disegno stava bene. Colpisce solo i pixel a trasparenza PARZIALE
— cioe' esattamente il bordo morbido di gomma e lazo — ed e' **cumulativa**:
agisce solo sulle zone passate per un'operazione di lazo, e ogni volta un po' di
piu'. Non era capriccio, era la storia di quel pezzo di tela.

**I numeri:** un azzurro (37,41,74) diventa (13,15,27), poi (4,5,10), poi
(1,1,3). I rapporti fra i canali muoiono nell'arrotondamento → grigio.
Misurato sulla tela vera prima della correzione: pixel del bordo a **(0,0,0)
con alpha 95**, cioe' grigio 160 sul bianco — esattamente dove finisce un colore
premoltiplicato due o tre volte.

⚠️ **CORREGGE SOLO IL DANNO FUTURO.** Dove il colore e' gia' stato schiacciato
l'informazione non c'e' piu': quelle zone restano grigie. Si recuperano solo da
un salvataggio precedente della scena.

> **Due lezioni, che sono la parte che vale.**
> 1. La prima ipotesi — cercare il colpevole nel TRACCIATO del lazo — non portava
>    da nessuna parte, e l'unica cosa che ha funzionato e' stata **misurare i
>    pixel veri sul PNG salvato** (PIL/numpy) invece di ragionare sul codice.
> 2. Il sintomo che ha risolto il caso non e' quello segnalato per primo. Un
>    bordo scuro puo' venire da dieci cause; il colore perso da una. **Quando un
>    difetto non si spiega, chiedere l'ALTRO sintomo.**

> ✅ Corretto anche, trovato per strada e **indipendente**: `canvasImage()`
> dichiarava il buffer NON premoltiplicato, e alimenta il contenuto della
> **stampa del foglio** — i bordi morbidi si stampavano piu' scuri dello schermo.
> ⚠️ NON si tocca allo stesso modo il salvataggio/caricamento della tela, che
> pure dice `false`: li' l'etichetta sbagliata c'e' in andata E in ritorno,
> quindi il giro e' byte per byte identico e i file sono sani. "Correggerli"
> farebbe ripremoltiplicare dati gia' premoltiplicati e scurirebbe TUTTE le tele
> esistenti, in silenzio.

---

### ⏳ IN ATTESA — WizzerWorks — la correzione CMake 4, e chi la firma

Il 2026-09-17 Franco ha scritto a **Mark Millard <msm@wizzerworks.com>**
(`magic-lantern-workbench`, che ha un fork di Ztoryc) chiedendogli di aprire
come PR il suo commit `31cfa33dc` «Fix CMakeLists files for CMake 4.» del
24 luglio, che vive su `mlw/magiclantern` e non era mai stato proposto. Le sue
parole: *«I'd rather merge it with your name on it than copy it out of your
branch»*.

**Promemoria fissato al 2026-09-25** (task `wizzerworks-cmake4-follow-up`).

**Verificato sul codice il 2026-09-18** — il commit fa tre cose, e due le
abbiamo gia':

| | lui, 24 luglio | noi, su master |
|---|---|---|
| `cmake_minimum_required` 2.8 → 3.10 (due punti) | ✅ | **gia' presente** |
| `find_package(Boost)` → `Boost CONFIG` | ✅ | **gia' presente** |
| togliere `DEPENDS` dai `POST_BUILD` | 2 righe | **manca** — ma da noi sono **7** |

⚠️ **Il terzo pezzo NON e' cherry-pickabile.** Il suo ramo e' fermo a luglio e
da allora `toonz/sources/toonz/CMakeLists.txt` e' cresciuto: lui aveva due
`add_custom_command(TARGET ... DEPENDS)`, noi ne abbiamo sette (si sono
aggiunti `tcomposer`, `tcleanup`, `tconverter`, `tfarmcontroller`,
`tfarmserver`). Un cherry-pick darebbe conflitto e finirebbe riscritto comunque.

**Perche' non si sollecita e non ci si mette le mani.** Al 18 settembre era
passato **un giorno**, e sul loro fork l'ultimo push e' del **31 luglio**: non
e' silenzio, e' un progetto che non stanno toccando. E prendergli il commito un
giorno dopo avergli scritto «preferisco mergiarlo col tuo nome sopra»
contraddirebbe la mail. Vale anche la memoria corta: **siamo stati noi a tenere
la loro PR #3 ferma un mese senza un commento** (vedi il passo 1-bis della
checklist di rilascio, che esiste per quello).

**La forma giusta del sollecito** non e' «hai visto la mail?» ma una seconda
mail che gli fa RISPARMIARE lavoro: due delle tre correzioni le avevamo gia'
raggiunte da soli, resta quella dei `DEPENDS`, il file da luglio e' passato da
due righe a sette — quindi la apre lui sul file di oggi, o la facciamo noi
citandolo. La scelta resta sua.

---

### 🔴 APERTO 2026-09-16 — gli asset con un tipo che Kitsu non ha vengono SALTATI IN SILENZIO

`KitsuClient::asPushProcessNext()`:

```cpp
const QString typeId = m_asTypeIdByName.value(a.type.toLower());
// No matching Kitsu asset-type → can't create it there; skip.
if (typeId.isEmpty()) { ++m_asIndex; continue; }
```

Il commento e' onesto, il comportamento no: l'asset non viene creato e **nessuno
lo dice**. Il riepilogo finale recita «%1 created, %2 already in Kitsu» — i
saltati non compaiono in nessuno dei due numeri. L'utente preme «push assets»,
legge un messaggio che sembra un successo, e degli asset non sono mai arrivati.
E' la famiglia dei difetti che perdono lavoro in silenzio, la peggiore.

**Non e' un problema di tassonomia** — quello era il modo in cui l'avevo scritto
a memoria, e verificandolo il 2026-09-16 si e' rivelato falso. Ztoryc usa **gia'**
i nomi di Kitsu (`Character`, `Prop`, `FX`, `Environment`), con migrazione del
vecchio `BG` dentro `Environment`, e il push accoppia per nome contro i tipi veri
letti da `/api/data/asset-types`. L'allineamento c'e'.

**Le due cose da fare, distinte:**

1. **Non saltare mai in silenzio.** Contare i saltati e dirlo, col nome e col tipo
   che non ha trovato corrispondenza. Anche solo questo trasforma una perdita in
   una segnalazione. E' la parte urgente, ed e' piccola.
2. **Leggere i tipi dal progetto Kitsu invece di averne quattro scolpiti nel
   codice** (`ZtoryModel`, la lista `{"Character","Prop","FX","Environment"}`).
   Uno studio che in Kitsu ha `Vehicle` o `Set Dressing` oggi non puo' nemmeno
   classificare un asset di quel tipo dentro Ztoryc: il tipo non esiste nella
   tendina, quindi il problema nasce prima del push. Scelta di progetto da fare:
   i tipi diventano dati del progetto, non una costante.

> Nato da una domanda di Franco (2026-09-16): «la tassonomia degli asset cosa
> intendi? non basta adeguarci a quella esistente in Kitsu?». Bastava, ed era
> gia' stato fatto: la domanda ha smontato una mia nota vecchia e ha fatto
> emergere il difetto vero, che era un altro.

### 🆕 APERTI DAL 2026-08-18 (sera)

> 📏 **Una voce di questo blocco dice COME e' stata verificata, o non vale.**
> Le sessioni nuove leggono questo elenco e lo prendono per buono: una voce
> sbagliata costa piu' di una voce mancante, perche' manda a lavorare su un
> problema che non c'e'. Quindi ogni «APERTO» porta con se' il metodo, non solo
> l'esito — e chi legge puo' fidarsi in proporzione al metodo.
>
> **Un `grep` di un nome NON e' una verifica di comportamento.** Se la domanda
> e' «questo percorso fa la cosa X?», si guarda **quale funzione chiama**: il
> codice delega, e la delega non contiene la parola che stai cercando. E' cosi'
> che la voce sul `.zmouth` e' rimasta aperta per sbaglio dal 2026-08-27 al
> 2026-08-29, ed e' costata mezza sessione (riaperta e richiusa due volte).
>
> Le decisioni di NON fare una cosa si segnano ✅ **DECISA** e restano qui: non
> sono difetti aperti, e riproporle fa perdere tempo a Franco. Vale anche il
> blocco `🛑 SOSPESI` piu' in alto.

🔵 **TAVOLOZZA PENNELLI — la meta' del lavoro ESISTE GIA' in Tahoma** (verificato
sul codice il 2026-09-15, non dedotto). Nato dalla domanda di Franco: si possono
salvare i pennelli aggiunti nella Thumbs room, e sceglierli/personalizzarli con
lo Style Editor?

**Cosa c'e' gia', e va usato invece di riscriverlo:**
- `MyPaintBrushStyleChooserPage` (browser dei pennelli, chip 64x64, ricerca) e
  `SettingsPage` (modifica i PARAMETRI del pennello e le curve sugli input) sono
  gia' nello Style Editor.
- ⚠️ **`StyleEditor::setPaletteHandle()` NON si puo' usare — mia affermazione
  sbagliata del 2026-09-15, corretta la notte stessa.** E' dichiarata nell'header
  ma la sua **implementazione e' COMMENTATA** in `styleeditor.cpp:4522`: dal solo
  header sembra a posto, e lo scopre il linker. E il corpo commentato dice anche
  perche' e' spenta: scambia il puntatore e chiama `onStyleSwitched()` **senza
  ricollegare i segnali** che il costruttore aveva agganciato alla maniglia
  precedente — cioe' e' incompleta, non solo inutilizzata. Riattivarla cosi'
  com'e' darebbe un editor che mostra una palette e ne ascolta un'altra.
  **Scriverla per davvero** (scambio + riconnessione) e' lavoro in codice
  condiviso, quindi **candidato PR a monte**. Fatto quello, lo Style Editor si
  costruisce a sé (`StyleEditor(PaletteController*, parent)`, come gia' fa
  `ztoryanimatic.cpp:4946`) e si punta sulla NOSTRA maniglia: cosi' non si tocca
  quello condiviso e non c'e' niente da rimettere a posto uscendo dalla room.
- **Le personalizzazioni si salvano davvero.** `TColorStyle::save` scrive il nome;
  `TMyPaintBrushStyle::saveData` scrive percorso, colore, ogni parametro
  modificato **e le curve complete** delle mappature. `loadData` e' simmetrico.
  (Franco dubitava di questo: il dubbio e' infondato.)
- `m_styles` e' un **vettore indicizzato**, non una mappa per percorso → si
  possono avere piu' varianti dello stesso `.myb` con nomi e parametri diversi.
- **Palette predefinite per tipo di livello, gia' funzionanti**: il comando
  `MI_SaveAsDefaultPalette` («Save As Default Palette», menu contestuale del
  pannello Palette) scrive in `<cartella palette utente>/<tipo>_default.tpl`
  (`raster` / `smart_raster` / `vector`). Il raster la rilegge via
  `FullColorPalette::getPalette()`, TLV e vettoriale via `tool.cpp` quando il
  livello nasce senza palette. **Quindi «la mia tavolozza sui livelli nuovi»
  funziona oggi, senza codice e senza opzione nelle preferenze.**

⚠️ **Precedenza da sapere, o sembra rotto:** per il raster la palette **di
progetto** (`+palettes/fullcolorPalette.tpl`) vince sulla predefinita
dell'utente. Voluto (uno studio condivide una tavolozza), ma confonde.

✅ **I pennelli MyPaint funzionano ANCHE sullo smart raster** — correzione di
Franco, verificata: `ToonzRasterBrushTool` fa lo stesso
`dynamic_cast<TMyPaintBrushStyle*>(getCurrentLevelStyle())` del raster a colori
pieni. **E il TLV e' il livello preferenziale di Ztoryc, quindi e' il caso
principale, non un di piu'.** Una palette TLV contiene anche i colori ink/paint:
la cosa pulita e' usare le **pagine** della palette (una Colori, una Pennelli),
che il `.tpl` salva gia'.

✅ **FATTO E RILASCIATO nella 0.14.0 (2026-09-16)** — tutti e tre i punti che
questa voce elencava come da fare:
1. il canvas della Thumbs room prende il pennello da una **palette vera**, non da
   un percorso di file;
2. lo **Style Editor** punta su quella palette quando si e' nella room, costruito
   sulla nostra maniglia e non su quella condivisa (era il pezzo delicato, e la
   strada scelta e' proprio quella che evitava di dover rimettere a posto
   qualcosa uscendo);
3. la striscia dei pennelli e' rimasta come **accesso rapido**, con Duplica e
   Rimuovi nel menu contestuale, e lo Style Editor e' il posto dove si
   personalizza.
In piu', non previsto qui: una **gomma vera** (toglie i pixel invece di
dipingerli di bianco, quindi sfuma e si annulla) e i **percorsi relativi** dei
`.myb`, senza i quali una palette copiata su un'altra macchina ripiegava in
silenzio sul pennello di fabbrica.

> ⚠️ **Questa voce e' rimasta scritta come «da fare» per due giorni dopo che il
> lavoro era atterrato**, e il 2026-09-18 ha quasi fatto rifare cose gia' fatte.
> Quando una voce viene chiusa da un rilascio, si chiude QUI nello stesso giro:
> una lista di lavoro che non si aggiorna e' peggio di nessuna lista, perche'
> viene creduta.

> **Il piano con `QSettings` e' SUPERATO.** Se i pennelli vivono in una palette,
> la persistenza e' il `.tpl` e non va scritta: nomi, parametri, curve, piu'
> tavolozze, condivisione. Non riproporre l'elenco in QSettings.

✅ **RASTER PER PAGINA COMPLETO il 2026-09-24** (passo 3a `529f97a69` + 3b):
la finestra di lavoro tiene 3 pagine (~30 MB qualunque sia la lunghezza).
Verificato due volte: **autocollaudo** (`ZTORYC_THUMBS_SELFTEST=1`, stesse
operazioni vere a finestra piena e a 1/2/3/5 pagine, tele identiche byte per
byte, pennellate nello stesso punto, annulla/ripeti esatti) e **collaudo di
Franco** su 30 righe (disegno in alto e in fondo, annulla/ripeti, salva e
riapri). Con la Thumbs room non resta niente di aperto su questo filone.

**COSA RESTA DAVVERO DA FARE sulla Thumbs room:** solo il **raster per pagina**,
la voce 🔴 qui sotto. Nient'altro.

🔴 **IN PIEDI — il raster unico della Thumbs room non regge l'obiettivo di
produzione.** Non e' un difetto: e' un limite di struttura, con i numeri in mano.

Obiettivo dichiarato da Franco (2026-09-15): **26' di storyboard, 16 shot al
minuto, 3 panel per shot** = 1248 vignette = griglia 4×312 = `1920×84240`.

| | oggi (4×26) | obiettivo (4×312) |
|---|---|---|
| raster in RAM | 51 MB | **617 MB** |
| codifica PNG | 270 ms | **~3,2 s** (M4) |
| copia sul thread UI | 5 ms | **~60 ms** |
| picco RAM nel salvataggio | 100 MB | **~1,2 GB** |

⚠️ **Il salvataggio per pagine DA SOLO non basta**, ed e' il motivo per cui la
voce e' scritta cosi': risolverebbe la codifica e lascerebbe gli altri tre
numeri. Guardare il terzo — la copia che resta sul thread UI risale da 5 a 60 ms,
cioe' **il ritardo appena tolto tornerebbe da un'altra strada**. Peggio: `addRow`
copia tutto il raster a ogni riga aggiunta (alla riga 312 e' una memcpy da
617 MB), e 617 MB contigui su un portatile Windows sono un'allocazione fragile,
non solo grossa.

**La forma giusta: un raster PER PAGINA.** 1920×1350, ~10 MB, codifica ~52 ms —
**costante**, qualunque sia la lunghezza dello storyboard. In RAM solo le pagine
vicine a dove si lavora (~63 pagine in totale), le altre caricate quando servono.

✅ **DECISIONE DI FRANCO che rende il progetto possibile (2026-09-15): «un
disegno non attraversa mai due pagine».** Una pagina e' un foglio fisico, quelli
che si disegnano su Procreate e si importano. Quindi il panorama — il motivo per
cui il raster era stato fatto contiguo — sta **dentro la pagina** e sopravvive.
Senza questa risposta il progetto avrebbe una forma completamente diversa.

> **Se e quando si fa, la parte pericolosa e' una sola: cosa marca «sporco».**
> Oltre al pennello ci sono ~10 strade che scrivono sul raster (`addRow`,
> `applyImportedCells`, `commitFloat`, `cancelFloat`, `liftFloatLasso`, incolla,
> pulisci, reflow dell'aspetto camera, ripristino dell'undo). Se una non marca la
> sua zona, quella modifica **non viene mai salvata** — niente crash, niente
> messaggio, te ne accorgi domani.
> **Rovesciare il valore predefinito:** salva TUTTO, tranne quando si sa con
> certezza che e' stata solo una pennellata. Cosi' una strada dimenticata costa
> **prestazioni, non dati**. La via stretta del pennello e' gia' collaudata: e'
> la stessa informazione (`askWrite`) su cui gira il ridisegno parziale, e Franco
> ha confermato che il segno e' pulito.

#### 🔍 RICOGNIZIONE 2026-09-18 — letta tutta la superficie prima di toccarla

Branch pronto: **`feature/thumbs-paged-raster`**, che si compila nel worktree
`tahoma2d-superplastic` e produce **`Ztoryc-SP.app`** — cosi' la `Ztoryc.app` su
cui Franco lavora resta quella di master. Lo `build_and_deploy.sh` di quel
worktree riconosce da se' il bundle rinominato.

**L'impalcatura esiste gia', ed e' meta' del lavoro.** Il salvataggio a bande
(2026-09-16) lavora gia' a gruppi di **5 righe di griglia**, che e' esattamente
l'altezza di una pagina: 4 × 5 × 270 px = **1920×1350**, il numero della tabella
qui sopra. Sul disco i file sono gia' `_ztorythumbs_band000.png …` con il loro
manifesto, e ci sono gia' `bandCount()`, `bandRasterRange()`, `m_bandDirty` e la
via stretta `askWrite()`. **Non si tratta di inventare le pagine: si tratta di
far combaciare la memoria con la struttura che e' gia' su disco.**

**Misurato, non dedotto: i panorami ATTRAVERSANO gia' una pagina.** Nel file vero
di Franco `_ztorythumbs_merges.txt` c'e' `1 9 1 2`, cioe' una fusione sulle righe
**9 e 10** — e il confine fra bande cade a multipli di 5. Quindi la regola «un
disegno non attraversa mai due pagine» vale per il DISEGNO, non per le fusioni.
Vietarle romperebbe un disegno esistente: vanno **composte** da due pagine.

**Due costi che la voce qui sopra NON elencava, e pesano quanto quelli elencati:**
1. **`pushUndo()` clona il raster INTERO** (`s.ras = m_ras->clone()`). Le
   pennellate usano gia' le tessere, ma ogni operazione che pennellata non e'
   (importa, incolla, pulisci, fondi, trasforma) si porta via una copia
   completa. A 617 MB e' insostenibile — ed e' lo stesso guaio che il commento
   di `addRow` racconta di aver gia' vissuto («la macchina comincio' a swappare
   e non si riprese piu'»).
2. **Lazo e selezione flottante fanno un giro raster→QImage→raster su TUTTA la
   tela**: `liftFloatLasso`, `commitFloat`, `cancelFloat`. Tre conversioni da
   617 MB per un lazo.

❌ **Sospetto CADUTO, scritto perche' non torni:** `paintEvent` **non** converte
tutta la tela a ogni ridisegno. Usa `rasterToQImage(..., mirrored=false)`, che
avvolge la memoria senza copiarla, e lascia il ribaltamento al painter; il
commento accanto lo spiega gia'. Era stato ottimizzato apposta.

**IL NODO DI PROGETTO: il pennello.** `beginStroke` fa
`new MyPaintToonzBrush(m_ras, ...)` — il motore MyPaint riceve IL raster e ha uno
stato interno (velocita', sbavatura). Una pennellata vicina al confine dovrebbe
scrivere su due pagine, e quello stato non si spezza in due.

→ **La forma che risolve pennello, lazo e undo insieme: la FINESTRA DI LAVORO.**
Le pagine stanno separate in memoria; quelle su cui si sta lavorando (∼3, una
trentina di MB) vivono in una finestra **contigua**. Pennello, lazo e undo
lavorano sulla finestra, esattamente come oggi e senza cambiare il loro codice.
Quando l'utente si sposta, la finestra si riversa nelle pagine e si riapre
altrove. E' questo che «tessere» voleva dire, ed e' il motivo per cui era una
fase a se'.

> ⚠️ **La finestra ALZA il rischio gia' segnalato su «chi marca sporco»**, non
> lo abbassa: aggiunge un momento in cui gli stessi pixel stanno in due posti
> (finestra e pagina). Il riversamento va trattato come una strada di scrittura
> a tutti gli effetti, e vale ancora — a maggior ragione — la regola di
> rovesciare il valore predefinito: salva tutto, tranne quando si sa con
> certezza che e' stata solo una pennellata.

**Mappa delle 82 scritture/letture di `m_ras`**, per non scoprirne una a meta'
lavoro: `applyImportedCells` (8), `askWrite` (6), `restoreGeometry` (5),
`isPanelEmpty` (5), `persistSave` (4), `persistLoad` (4), `liftFloatLasso` (4),
`applyPatches` (4), `capturePatchesAt` (4), `panelRaster` (4), `onSceneChanged`
(3), `liftFloat` (3), `commitFloat` (3), `cancelFloat` (3), `addRow` (3), piu'
`undo`/`redo`/`pushUndo`/`restoreSnapshot`, `paintEvent`, `markBandsDirty`,
`beginStroke`, `canvasImage`.

✅ **RISOLTO 2026-09-15 — la numerazione degli shot si sfasciava: una colonna
audio in mezzo bastava.** Segnalato da Franco su una scena vera
(`CS2605CA_UGC/scenes/SB_.tnz`, Cartoon School): importate 5 pagine di thumbnail
da Procreate senza problemi, ma gli shot esportati venivano numerati
`sh300 · sh290 · sh300 · sh310 · sh300 …` — valori che rimbalzano fra due
vicini e si duplicano.

**Il modello numerava GIUSTO.** E' il fatto che ribalta la lettura del sintomo:
nella scena i livelli sono `sh020 … sh320`, uno per export, in fila. Quei nomi
li scrive `addShotFromRasters` subito dopo `generateShotLabel()`, quindi
l'etichetta nasceva corretta tutte e 31 le volte. Sbagliati erano solo i nomi
delle colonne e il `.ztoryc`.

**Causa: `StoryboardPanel::updateColumnName` scriveva su `ColumnId(si)`** —
`int col = si; // la colonna corrisponde all indice dello shot`. Quel commento
era una supposizione, e il codice lì intorno dice che è falsa:
`refreshFromScene()` salta ogni colonna che non contiene una sotto-scena
(`if (!cl) continue`) ed è per questo che esiste `shot.data.xsheetColumn`.

**Perché una riga sbagliata rovinava TUTTA la numerazione, e non un nome solo:
lettura e scrittura non erano d'accordo su quale colonna sia di quale shot.**
La lettura era già giusta (riga 4661 usa `xsheetColumn`), la scrittura no. E il
nome della colonna è insieme l'uscita e l'ingresso — `refreshFromScene`
ri-deduce l'etichetta dal nome della colonna — quindi una scrittura sfasata
tornava indietro come verità al refresh successivo, e l'insieme marciava di una
tacca ogni volta, finché i valori non si accatastavano in cima alla serie. Da
qui il rimbalzo e i duplicati. In modo **Auto #** non si vede (l'etichetta si
ricalcola dalla posizione): serve **Keep #**.

**MISURATA, non dedotta.** Sonda `qWarning` nei due `updateColumnName`, scena
aperta davvero: **96 chiamate, 93 sfasate**. La scena ha
`Col1` = shot, `Col2`+`Col3` = le due colonne audio (`CASCINA voci`,
`CASCINA effetti`), `Col4…` = gli altri 31 shot — quindi ogni shot dall'indice 1
in poi scriveva **due colonne a sinistra**, sulle colonne audio e su quelle
degli shot precedenti.

**Correzione:** si usa `m_shots[si].data.xsheetColumn`, con `return` se è fuori
range. **Niente ripiego su `si`**: quello è il difetto, e un id negativo
arriverebbe a `ColumnId(-1)`, che in questo repo è già costato tre crash (la
famiglia «pegbar zombie»). Verificata con la stessa sonda sulla copia della
scena: shot 1 → `Col4`, shot 31 → `Col34` (prima `Col2` e `Col32`).

⚠️ **Manca la conferma di Franco sul giro vero**: export dalla Thumbs room in
modo **Keep #**, che è il percorso da cui è nata la segnalazione. Quello che è
provato è che la colonna bersaglio adesso è quella giusta.

> **Non è candidato upstream:** `storyboardpanel.cpp` è un file solo Ztoryc.

> **Nota sulla scena di Franco:** il suo `SB_.ztoryc` è già tornato a posto
> (`sh010 … sh320`, zero duplicati) — riaprire la scena in **Auto #** rinumera
> per posizione, e l'ordine degli shot non era mai stato toccato.

**0. 🔴 SU WINDOWS MANCA IL TLS — nessuna connessione HTTPS funziona.**
Scoperto il 2026-08-30 da Simona Manganaro (storyboard di filorosso), che non
riusciva a collegarsi a Kitsu: `TLS initialization failed`.

**Non e' un problema di Kitsu.** Qt fa HTTPS solo se trova OpenSSL, e
`windeployqt` **non lo copia** — le librerie TLS non fanno parte di Qt. Nei
nostri script di confezionamento OpenSSL non compariva da nessuna parte
(verificato con `grep -rn -i openssl ci-scripts/ .github/workflows/`: zero
occorrenze). Quindi **ogni** chiamata a un indirizzo `https://` moriva sul
pacchetto Windows: Kitsu, controllo aggiornamenti, qualunque cosa.

Non ce ne eravamo accorti perche' **Kitsu lo abbiamo sempre provato dal Mac**,
dove il TLS e' di sistema e non serve spedire niente.

**Correzione COMMITTATA** (`d47d62479`) **e VERIFICATA su Windows** il
2026-08-30 sulla macchina di Franco. Il metodo sta nel riquadro qui sotto:
- `thirdparty/openssl/bin/x64/` — `libssl-1_1-x64.dll` e
  `libcrypto-1_1-x64.dll`, versionate come si fa gia' per `freeglut` e `glew`,
  con licenza e `README.md`
- `ci-scripts/windows/tahoma-buildpkg.bat` — le copia dopo `windeployqt`, **con
  un controllo che fa fallire la build se mancano**

⚠️ **Qt 5.15.2 vuole OpenSSL 1.1.1**, non la 3.x: nomi diversi e altra ABI.

> **Debito noto:** la 1.1.1 e' fuori supporto dal settembre 2023 e la `1.1.1w`
> e' l'ultima mai rilasciata. Stiamo spedendo una libreria di crittografia non
> piu' mantenuta. Si chiude **solo aggiornando Qt**, non cambiando le DLL.

> **Rimedio immediato** per un'installazione gia' fatta: copiare le due DLL
> accanto a `Ztoryc.exe` e riavviare. Niente installazioni, niente variabili
> d'ambiente.

> **Come si e' arrivati alla causa:** il messaggio dice «TLS initialization
> failed» e sembra un problema di rete o di certificato. La verifica decisiva e'
> stata cercare OpenSSL negli script di confezionamento — non nel codice
> dell'applicazione, che e' corretto.

> **COME E' STATA VERIFICATA** (2026-08-30, macchina Windows di Franco, portable
> `C:\portables\Ztoryc`, build del 21 luglio):
> 1. **Difetto riprodotto qui**: nessuna delle due installazioni portable aveva
>    le DLL, e `thirdparty/qt/5.15.2_wintab/msvc2019_64/bin/qtdiag.exe` — cioe'
>    il **nostro** Qt, non un Qt qualsiasi — stampava `SSL is not supported.`
> 2. **Nomi giusti**: il `Qt5Network.dll` **installato** contiene le stringhe
>    `libssl-1_1-x64` e `libcrypto-1_1-x64` e cita `OPENSSL_init_ssl`. Il
>    supporto SSL dentro Qt non e' mai mancato: mancavano le librerie.
> 3. **DLL giuste**: messe accanto a `qtdiag.exe`, quella riga diventa
>    `Using "OpenSSL 1.1.1w  11 Sep 2023", version: 0x1010117f`. Se fossero
>    state 3.x, o a 32 bit, non si sarebbero caricate e la riga sarebbe rimasta
>    la prima.
> 4. **sha256 delle due DLL identici** a quelli dichiarati in
>    `thirdparty/openssl/README.md`.
> 5. **End-to-end**: Franco si e' collegato a Kitsu dal portable con le DLL
>    accanto. Due controlli perche' il collegamento riuscito **da solo non
>    prova niente**: (a) l'indirizzo usato era davvero cifrato —
>    `HKCU\Software\Ztoryc\...\Kitsu\BaseUrl = https://kitsu.ztoryc.org` — e il
>    client non impone lo schema (`kitsuclient.cpp:145`), tanto che il valore di
>    ripiego e' `http://localhost:8012`; (b) il processo vivo **aveva caricato**
>    `libssl-1_1-x64.dll` e `libcrypto-1_1-x64.dll`, e Windows mappa una DLL solo
>    quando qualcuno la chiede — l'unico che la chiede e' `Qt5Network` per aprire
>    un socket cifrato.
>
> ✅ **IL CONFEZIONAMENTO E' VERIFICATO** (2026-09-15, run CI `34999755586`,
> commit `996f4ea64`). Come chiede AGENTS.md § 4-bis, controllato **sull'artefatto
> pubblicato** e non solo sul log — la 0.13.2 e' fallita proprio per la verifica
> giusta fatta sull'artefatto sbagliato:
> 1. il passo `>>> Copy OpenSSL DLLs (TLS support for Qt)` gira nel log;
> 2. nel `Ztoryc-0.13.2-win-portable.zip` le due DLL ci sono e stanno in
>    `Ztoryc/`, **la stessa cartella di `Ztoryc.exe`** — dove Qt le cerca;
> 3. sha256 **identici** a quelli dichiarati in `thirdparty/openssl/README.md`:
>    `libcrypto` `67a17e76…`, `libssl` `1dfdf13b…`. Sono quelle giuste, non due
>    file col nome giusto.
>
> ⚠️ **Non e' provato il ramo NEGATIVO**: che la build fallisca davvero se le DLL
> mancano. Vorrebbe romperla apposta e spendere un'ora di CI. Quello che sappiamo
> e' che quando ci sono, arrivano dove devono.

**0-bis. ✅ CORRETTA — la password di Kitsu era scritta IN CHIARO nel registro
di Windows.** Commit `de098ab02`, 2026-08-30. La voce resta qui perché il
percorso macOS non era ancora stato compilato quando è stata scritta: vedi
«cosa manca» in fondo.

**Com'era.** Trovata il 2026-08-30 leggendo il registro per un'altra ragione
(controllare che l'indirizzo di Kitsu fosse `https://`), quindi **non era
un'ispezione teorica**: la password era li' in chiaro, leggibile senza
strumenti. `kitsuclient.cpp:102` salvava con `QSettings::setValue` e basta, e la
riga 22 portava gia' il commento «local convenience only» — una scorciatoia
presa consapevolmente, non una svista, ma su una macchina condivisa un difetto.
Su Windows finiva in `HKCU\Software\Ztoryc\...\Kitsu\Password`: la leggeva
chiunque avesse accesso all'utente, e se la portava dietro ogni backup del
profilo. Valeva anche per Simona, che la password se la salva sul suo computer.

**Com'è adesso.** `ztorysecret.h/.cpp` — tre funzioni sopra il portachiavi di
sistema: **Credential Manager** su Windows (`CredWriteW/CredReadW/CredDeleteW`,
advapi32), **Keychain Services** su macOS (`SecItem*`, non le `SecKeychain*`
deprecate). Dove un portachiavi non c'è (Linux) `store()` **rifiuta** invece di
ripiegare sul testo in chiaro, e il dialogo disabilita «Remember password»
quando `isAvailable()` è falso, invece di promettere una cosa che poi non fa.

**La migrazione fa pulizia da sola:** `loadSettings` sposta nel portachiavi il
valore vecchio trovato in `QSettings` e poi lo **cancella**; `saveSettings`
rimuove la chiave vecchia **sempre**, anche quando non si salva niente — cosi'
la password in chiaro sparisce anche a chi il dialogo non lo riapre mai.

> ✅ **Il percorso macOS ora è verificato — compila e collega** (2026-09-15, su
> questo Mac). Il commit dichiarava verificato solo MSVC 2022; mancavano il ramo
> Apple e il collegamento dell'applicazione intera. Misurato adesso:
> `ztorysecret.cpp.o` compila con **zero warning** anche con clang, la build
> completa esce `exit=0`, e nel binario finito i quattro simboli `_SecItemAdd`,
> `_SecItemCopyMatching`, `_SecItemDelete`, `_SecItemUpdate` risolvono contro
> `/System/Library/Frameworks/Security.framework` (letto con `nm -u` e
> `otool -L`, non dedotto dal CMakeLists).
>
> ✅ **E il portachiavi risponde davvero** (2026-09-15). Provato con una **sonda
> linkata all'oggetto vero** — `toonz/CMakeFiles/Ztoryc.dir/ztorysecret.cpp.o`,
> quello che finisce nell'app, non una copia del sorgente. Otto controlli, otto
> passati: `retrieve()` su una voce inesistente torna vuoto senza esplodere;
> `store()` scrive; `retrieve()` rida' la stessa password **UTF-8 compresa**
> (accenti ed emoji); un secondo `store()` passa per `SecItemUpdate` e
> **sostituisce** invece di affiancare; `remove()` cancella; `remove()` due volte
> non esplode. La sonda non ha lasciato niente nel portachiavi (verificato con
> `security find-generic-password`).
>
> ✅ **Il giro dentro l'applicazione su macOS: FATTO il 2026-09-15, e ha trovato
> un difetto.** Ztoryc chiedeva la password del portachiavi **a ogni avvio**,
> perche' `loadSettings()` leggeva il segreto e il primo a costruire il singleton
> e' il pannello di produzione, che lo fa solo per collegare dei segnali. Ora la
> lettura e' pigra (`ensurePasswordLoaded()`, chiamata da `login()`). Nel farlo e'
> stato chiuso anche il buco che la pigrizia apriva: `saveSettings` avrebbe
> potuto scrivere una password vuota sopra quella salvata.
>
> ⚠️ **COSA MANCA ANCORA: il giro su WINDOWS.** Resta da provare
> il dialogo vero — salvare la password, riavviare Ztoryc, e controllare con
> **Accesso Portachiavi** che sia li' e **non** in
> `~/Library/Preferences/*Ztoryc*.plist`. E lo stesso su Windows, dove va
> verificata anche la **migrazione**: chi ha gia' la password nel registro deve
> vedersela sparire da `HKCU` al primo avvio. Quello che il portachiavi fa la
> sua parte e' ormai misurato; quello che manca e' il cablaggio.

✅ **DECISA — NON si corregge: cambiare email lascia la vecchia password nel
portachiavi, e va bene cosi'** (Franco, 2026-09-15): *«lascia cosi', se voglio
eliminare la vecchia lo faccio dall'app»* — cioe' a mano da Accesso Portachiavi
su macOS, da Credential Manager su Windows. **Non riproporre la correzione.**
Resta scritto qui sotto cos'e', perche' se un domani salta fuori come sintomo
(«ho tolto la spunta e la password c'e' ancora») la risposta e' gia' pronta e
non e' un difetto nuovo.

**Cos'e'.** Non e' ipotesi, e' la lettura del
percorso: `kitsuconnectdialog.cpp:337` chiama `setEmail()` col valore nuovo
**prima** di `saveSettings()` alla riga 341, quindi `saveSettings` conosce solo
l'email nuova. La voce salvata sotto quella vecchia non la cancella nessuno —
`loadSettings` legge solo l'email corrente.

Il caso che fa male non e' il cambio di email ma **togliere la spunta «remember
password» cambiando anche l'email**: il ramo `else` fa
`ZtorySecret::remove(kSecretService, m_email)` sull'email **nuova**, che una voce
non ce l'ha, mentre quella vecchia — l'unica che una password ce l'ha davvero —
sopravvive. L'utente ha appena detto «non ricordarla» e la password resta.

Non e' il difetto di partenza — nel portachiavi la password e' **protetta, non in
chiaro**, quindi la voce 0-bis resta risolta. E' una promessa non mantenuta, non
una fuga di dati, ed e' per questo che la decisione di lasciarla sta in piedi:
la pulizia a mano si fa in dieci secondi e la si fa una volta ogni cambio di
account, che non e' una cosa che capita.


> (il punto 1, Puppetoonz, e' passato in ANIMATIC_TASKS.md)

**2. ✅ DECISA (non e' un difetto aperto) — NON E' SOLO INTEL: ENTRAMBE
le build richiedono macOS 15.** La decisione e' in fondo alla voce: si
aggiornano i REQUISITI, non si insegue la correzione. Ricontrollata il
2026-08-29: la riga e' gia' nella checklist di AGENTS.md § «macOS —
requisiti», in entrambe le lingue. **Non riproporla.**
MISURATO sul DMG della 0.13.1 il 2026-08-28, non stimato — e la voce come era
scritta prima sottostimava il problema, perche' incolpava il runner
`macos-15-intel` di una cosa che succede identica su Apple Silicon:

| | librerie a 14.0 | a 15.0 | minimo vero |
|---|---|---|---|
| Intel | 28 | 12 | **macOS 15** |
| Apple Silicon | — | 40 | **macOS 15** |

I due eseguibili dichiarano entrambi `minos 12.0`, ed e' falso in entrambi. Chi
sta su Sonoma o precedenti scarica un'app che **non parte, su qualsiasi Mac**.

Le librerie a 15.0 sono `libzstd`, `liblzma`, `liblz4`, `libpcre2` (che serve a
Qt), `libquadmath`, `libltdl`: roba di base tirata dentro da Homebrew, non peso
morto rimovibile. Le X11 in `Resources/ffmpeg/libs/` invece **non** c'entrano —
stanno in Resources, non le carica nessuno all'avvio, le usa ffmpeg come
processo a parte.

**Non si sistema con poco** (valutato con Franco, 2026-08-28): vorrebbe un
runner piu' vecchio, se GitHub ne offre ancora uno Intel, oppure ricompilare
quaranta dipendenze dal sorgente con un deployment target piu' basso.
**Decisione: si aggiornano i REQUISITI invece di inseguire la correzione** — la
riga sta nella checklist di rilascio in AGENTS.md, sezione «macOS — requisiti».

**Come rimisurare** (a ogni cambio di runner): `otool -l` sulle dylib accanto
all'eseguibile DENTRO IL DMG. Non sul build locale, che porta i minimi della
macchina di chi compila e non della CI — misurarlo li' da numeri diversi e
fuorvianti.

**Il testo ORIGINALE della voce, per memoria:**
La build Intel dichiara Monterey ma richiede Sequoia. L'eseguibile ha
`minos 12.0`, ma delle librerie impacchettate 10 pretendono macOS **15.0** e 28
pretendono **14.0**: vengono da Homebrew del runner `macos-15-intel`. Chi ha un
Mac Intel con Ventura o Sonoma scarica il DMG e trova un'app che non parte. Mai
documentato, mai segnalato. Si aggiusta costruendo su un runner piu' vecchio o
forzando le dipendenze a un deployment target piu' basso. **Le note di rilascio
dicono «macOS 12+» e al momento non e' vero.**

**3. `build_and_deploy.sh` (solo locale) lascia la firma invalida.** Difetto
diverso da quello gia' corretto nella CI: li' e' `ztorycstuff` nella RADICE del
bundle, e lo schema «sposto fuori → firmo → rimetto dentro» non puo' funzionare
per costruzione, perche' l'ultimo passo annulla sempre il penultimo. Riguarda
solo la copia di sviluppo. Si risolve portandolo al layout della CI
(`Contents/Resources`).

> **Confermata il 2026-08-29, ma vale meno di come suona, e la correzione
> proposta non basta.** Misurato: lo script *firma davvero* (riga 447) e
> *verifica* (452) — il sigillo c'e', `Contents/_CodeSignature/CodeResources`.
> Solo che dura poco: lo script rimette `ztorycstuff` dopo aver firmato, e in
> piu' **ogni `ninja` successivo rilinka l'eseguibile dentro il bundle** e lo
> lascia piu' recente del sigillo. Un bundle nell'albero di build non puo'
> avere un sigillo valido, ed e' senza conseguenze: l'app parte lo stesso.
>
> **E il layout della CI non risolverebbe comunque.** La copia rilasciata ha
> gia' `ztorycstuff` in `Contents/Resources`, e il sigillo le si rompe uguale
> al primo avvio, perche' `TProject::createSandboxIfNeeded()`
> (`toonzlib/tproject.cpp:1211`) scrive il progetto sandbox li' dentro. Chi
> prende in mano questa voce sappia che ottiene un deploy piu' pulito, **non**
> un sigillo che dura.
>
> L'unica conseguenza pratica e' in checklist, ed e' gia' scritta in AGENTS.md
> § 4-bis: la firma si giudica **sul DMG montato**, mai sulla copia locale
> ne' su `/Applications`.


### ✅ RILASCIO 0.13.1 — COMPLETO (verificato il 2026-08-19 mattina)

<https://github.com/matitanimata/ztoryc/releases/tag/v0.13.1> — **nove asset su
nove**, note bilingui applicate, pubblica e non pre-release. La 0.13.0 resta
pre-release e NON cancellata (sorgente espeak, GPLv3).

Binari: Linux da `a4bc8655c`, macOS e Windows da `b7cdb1a60`. L'unica differenza
e' la macro di esportazione che serve solo a Windows — detto in fondo alle note.

**Controllo sponsor: FATTO da Franco il 2026-08-19** — nessuno nuovo dalla
0.13.0, i ringraziamenti restano Slam Rockwell e Rodney Baker. (Il token `gh`
di questa macchina non ha lo scope `read:user`: la dashboard la deve aprire lui.
Se in futuro arriva qualcuno va aggiunto in **tre** posti — `SUPPORTERS.md`, le
note della release, e la schermata About in `aboutpopup.cpp`.)

**Asset Linux in sostituzione** (2026-08-19): Franco ha deciso di NON bumpare a
0.13.2 ma di rimpiazzare i pacchetti Linux della 0.13.1 — non l'aveva ancora
annunciata e l'aveva scaricata solo lui. Ci lavora un'altra sessione di Claude
sul Dell: icona di Ztoryc al posto di quella di Tahoma, e lo script del `.deb`
che lanciava Tahoma2D invece di Ztoryc.

⚠️ **Quando gli asset Linux sono stati sostituiti, aggiornare la riga in fondo
alle note della release** che dice da quali commit vengono i binari: dira'
ancora `a4bc8655c` per Linux, che non sara' piu' vero.

**Provati da Franco e a posto:** Windows e macOS Intel.

### ✅ CHIUSI IL 2026-08-18

**1. Le sotto-scene nell'anteprima dei pannelli — RISOLTO** (commit `a501495d3`).
Tre difetti in fila, tutti trovati strumentando il render su file, nessuno
leggendo il codice:
1. `renderXsheetFrame` passava `false` credendo fosse `forSceneIcon`: e'
   `checkFlags`, e `forSceneIcon` restava al suo default VERO. Con quello vero
   `RasterPainter::onImage` salta il ramo della deformazione plastica, e i
   personaggi ZtoRig sono sotto-xsheet senza livello semplice → `player.image()`
   nulla → non si disegnava niente. Non uscivano deformati male: sparivano.
2. `renderFrame` e' **rientrante** (la texture della sotto-scena si costruisce
   richiamandola) e la `fb->release()` del ramo macOS lega il framebuffer **0**
   invece di quello di prima. In un contesto offscreen e' incompleto: ogni
   disegno successivo falliva con `GL_INVALID_FRAMEBUFFER_OPERATION` (0x506) e
   l'anteprima usciva **vuota**, schizzo compreso. → candidato PR upstream,
   registrato in `UPSTREAM_PR_CANDIDATES.md`.
3. Le texture stanno in una cache globale che non sa in quale contesto OpenGL
   sono nate: quella del viewer e' un numero morto nel contesto dell'anteprima,
   e legarla non da' errore — campiona bianco. Erano le sagome bianche. Ora
   `glIsTexture` la controlla e la ricostruisce.

**2. Quanti pannelli fa una sotto-scena — DECISO E FATTO** (commit `02e2a98f0`).
Regola scelta da Franco: **un pannello e' un disegno, non un fotogramma**.
- il disegno esposto si guarda DENTRO le sotto-scene (fino a tre livelli), cosi'
  un rig che tiene la posa non fa confine mentre la sotto-scena scorre;
- confine sempre: chiavi di colonna e movimenti di camera;
- livelli in **animazione piena**: non un pannello per cambio ma una griglia
  regolare, al ritmo di una nuova preferenza **«Panels/s»** nella barra del
  Board (1-24, predefinito 1) — di fatto un *each*;
- nessun caso speciale per gli shot senza schizzo.

**3. Rettangoli rossi di un movimento di camera che non c'e' piu' — RISOLTO**
(commit `08c00a0a7`, verificato da Franco). Il `.ztoryc` non aspetta il Save
della scena: si scrive a ogni modifica ed e' voluto, altrimenti dialoghi, note e
numerazione si perderebbero a ogni chiusura non salvata. Il difetto era che al
ricaricamento nessuno rimetteva in discussione quel dato: `computeCameraMove()`
gira solo con almeno due chiavi di camera, e senza quelle il pannello restava
intatto col valore vecchio. Ora senza chiavi il movimento si cancella — fra i
due comanda la **scena**, e non si perde niente di scritto a mano perche' quel
dato non lo scrive l'utente ma `classifyCameraMove()`.

> Obiezione sollevata e **scartata da Franco** il 2026-08-18: annotare a mano un
> movimento di camera sul pannello sarebbe **un disegno vero e proprio**, non un
> campo di dati — quindi vive nel livello e questa pulizia non lo tocca. Non
> serve nessun campo «da dove viene il movimento».

---

## 📦 RILASCIO 0.13.0 — cosa manca (stato al 2026-08-16 sera)

> Franco vuole rilasciare **appena il lip sync e' completo** (sua decisione del
> 2026-08-16: strada **B**, prima si impacchetta e poi si rilascia — «non c'e'
> fretta e sarebbe un peccato aspettare ancora»). Ha anche detto che **espeak
> serve**, quindi va spedito, con l'obbligo GPLv3 che ne consegue.
>
> Da qui si riprende senza dover ricostruire niente: sotto c'e' quello che e'
> fatto e verificato, e quello che manca con il perche'.

### ✅ FATTO E VERIFICATO (commit locali, NON ancora pushati al 2026-08-16 sera)

- `c4eeec038` **licenze allineate**. C'erano TRE affermazioni diverse: il
  `LICENSE.txt` diceva «all rights reserved» sulle aggiunte Ztoryc (che NEGA il
  permesso concesso piu' sotto dalla clausola BSD), la finestra About diceva
  «GPL v3» e apriva un file che GPL non e', e l'intenzione di Franco era BSD
  come Tahoma2D/OpenToonz. Ora dicono tutte **BSD 3-Clause**.
  Aggiunta **Digital Video S.p.A.** alla genealogia: non compariva in nessuno
  dei 26 file di licenza ed e' l'autore originale di Toonz.
  Aggiunti `LICENSE_vosk.txt` (Apache 2.0, testo canonico ripreso da quello
  gia' nel repo), `LICENSE_whisper.txt` (MIT, suo e dei pesi OpenAI) e
  `LICENSE_espeak-ng_info.txt`.

- `1f05c6036` **`ci-scripts/fetch-lipsync-deps.sh`** — UNO script per i tre
  sistemi. Scarica i modelli Vosk dalle fonti originali e li riconfeziona in
  `.zvosk` con `tools/pack_vosk_model.py`, prende il modello Whisper, scarica
  il SORGENTE di espeak-ng, e sceglie l'archivio libvosk in base al sistema.
  **Eseguito davvero** (22 s) e controllato:
  - i `.zvosk` prodotti sono **identici byte per byte** a quelli collaudati;
  - `libvosk.dylib` ha lo **stesso md5**, e' universal2 (x86_64+arm64) e non ha
    dipendenze fuori dal sistema (Accelerate, libc++, libSystem);
  - tutti gli URL fissati rispondono 200.
  Piu' il passo di bundling macOS in `tahoma-buildpkg.sh`, che copia **e poi
  controlla**: se i file dovevano esserci e non ci sono, si ferma.

### ✅ TUTTO FATTO IL 2026-08-17 — la 0.13.0 e' IN COSTRUZIONE

> Tre workflow lanciati sul commit `18a0d0300` con `publish_release=true` e
> `release_tag=v0.13.0`. **Quando la release esiste, applicare le note**:
> `gh release edit v0.13.0 --notes-file ~/ZtorYc/RELEASE_NOTES_v0.13.0.md`
> (il workflow pubblica col corpo VUOTO). Poi controllare che i quattro asset
> Linux e il sorgente di espeak-ng ci siano davvero (`gh release view v0.13.0`):
> la 0.11.0 e' uscita senza binari Linux perche' nessuno ha guardato.
>
> ⚠️ **Windows e Linux non sono mai stati provati** su questi passi nuovi: se
> la CI si lamenta, e' quasi certamente `build-lipsync-tools.sh` (cmake che non
> trova il compilatore in Git Bash) o un percorso nei tre script di pacchetto.
> macOS e' stato provato end-to-end in locale.
>
> Fatti oggi: **A** (lip sync nell'export + comando dal Board), **B** (import
> asset dal breakdown, con il breakdown ora scrivibile a mano), **C** (fatto
> ieri), **E** (cartella modelli utente, elenco lingue vero, «Add language…»,
> motore dichiarato), e l'impacchettamento **1-3**. Resta **D**.
>
> Sotto resta la descrizione di com'erano concepiti: serve a capire il perche'
> delle scelte, non e' lavoro aperto.

### ❌ QUELLO CHE ERA IL LAVORO DI OGGI (fatto, tenuto per il perche')

**Franco, 2026-08-16 sera: il lavoro di domani NON e' solo l'impacchettamento.**
Vanno completate anche le quattro cose sotto, che sono cio' che resta dello
scenario A (dallo storyboard allo shot animabile). L'impacchettamento (punti
1-3) e queste quattro fanno insieme la 0.13.0.

**A. Opzione lip sync nell'export.** Una casella nel popup di export: se attiva,
   ogni shot esportato esce con le colonne parole+fonemi gia' generate. Il
   generatore c'e' (`ztorylipsync.cpp`), va richiamato per shot dentro l'export.
   Serve anche poterlo fare PRIMA, durante la lavorazione dello storyboard, come
   controllo.

**B. Import degli asset dal breakdown — IL PEZZO GROSSO.**

   ✅ **QUANDO — confermato da Franco il 2026-08-17**: li importa **l'export**,
   non l'utente a mano dopo. *«anche i characters devono essere importati
   automaticamente come sottoscene negli shot»*. Quindi lo shot esportato nasce
   gia' popolato, e il codice va nel percorso di export — non nel pannello
   Breakdown.

   ✅ **COME SI COMPORTA — deciso da Franco il 2026-08-17.**
   Opzione nel popup di export (nome proposto: **«Import each shot's assets from
   the breakdown»** — dice cosa fa e da dove prende; «automatic import» direbbe
   *quando* e non *cosa*, e si confonderebbe con le opzioni di organizzazione
   degli asset che stanno li' accanto).

   Con l'opzione attiva, PRIMA di esportare si fa il **controllo** e si mostra un
   **rapporto** di cio' che non si risolve. L'utente decide:
   - **proseguire** saltando gli asset mancanti, oppure
   - **interrompere**, sistemare cio' che manca, e rilanciare l'export.

   Parole sue: *«fa un check degli asset e fa un report se manca qualcosa cosi'
   l'utente puo' decidere se proseguire saltando gli asset mancanti oppure
   interrompere, inserire le cose che mancano e rilanciare l'export»*.

   **Il rapporto NON deve dire «manca».** `resolveAssetFile()` restituisce gia'
   il motivo — «linked file is missing: X», «a character has no folder: link its
   scene», «no folder set for type Prop», «folder not found» — e sono frasi che
   dicono gia' cosa fare. Nel rapporto va quella riga, non un conteggio.

   ⚠️ **Controllare una volta per ASSET DISTINTO, non per shot.** Gli asset si
   ripetono su molti shot: controllando per shot diventano centinaia di accessi
   al disco, e su un volume esterno si sentono. Franco lo da' per immediato, ed
   e' vero solo con la deduplicazione. E' lo stesso errore che il 2026-08-16 ha
   fatto caricare uno storyboard in un minuto e mezzo (`collectColumnNames`, che
   ricorreva per cella invece che per sotto-scena).

   Il controllo e' lo stesso che il pannello Breakdown mostra gia' nella colonna
   «cosa troverebbe l'export»: la logica c'e', va solo chiamata prima
   dell'export e raccolta in un rapporto.

   ⚠️ Nota del 2026-08-16: Franco lo dava per fatto («abbiamo previsto l'import
   automatico»), ma **non lo e'**. Cercando `breakdown` non c'e' nessun
   consumatore nel percorso di export. Cio' che esiste e' la colonna del
   pannello Breakdown che mostra *cosa troverebbe* l'export (commento al
   condizionale in `ztoryproductionpanel.cpp` ~1946) — serve a vedere prima
   quali asset non si risolvono, non a importarli.
   Quindi: la catena breakdown -> asset -> file risolto e' **preparata**, ma
   l'export non li importa come sotto-scene. E' il pezzo che manca davvero allo
   scenario A.

**C. I folder degli asset nel browser.** Props, Backgrounds e Model sheets,
   impostati nel tracker, devono comparire come radici nel browser senza doverli
   cercare nei preferiti. I nodi si aggiungono in
   `DvDirModelRootNode::refreshChildren()` (filebrowsermodel.cpp), dove stanno
   My Documents / Desktop / Downloads / Favorites — sono
   `DvDirModelSpecialFileFolderNode`. Vanno rifatti quando cambia progetto.
   ⚠️ NON aggiungere una cartella «personaggi»: Franco l'ha bocciata il
   2026-08-16 («va bene quella model sheet che gia' esiste»), ed era stata messa
   e poi tolta.

**E. Lip sync in ALTRE LINGUE — installare un modello Vosk** (Franco,
   2026-08-16). ⚠️ Da non affrontare come se non funzionasse niente: **oggi le
   altre lingue gia' vanno**, solo con tempi meno precisi. Senza modello Vosk si
   ripiega su Whisper (multilingue) ed espeak-ng copre un centinaio di lingue
   per i fonemi. Si perde la precisione misurata — 10 ms con Vosk contro 30 con
   Whisper — non la funzione.

   **Il codice e' gia' quasi pronto.** `availableLanguages()` (ztoryvosk.cpp
   ~313) cerca i `<lang>.zvosk` nel bundle **e** le cartelle gia' scompattate
   nella cache, con il commento «se un domani si potranno scaricare, saranno qui
   e non nel bundle». `hasLanguage()` e `prepareLanguage()` guardano gia'
   entrambi i posti. Quindi un modello scompattato in cache **verrebbe gia'
   trovato**.

   **Cosa manca, ed e' poco:**
   - una cartella dei modelli UTENTE che non sia la cache — la cache si puo'
     svuotare, e un modello scaricato dall'utente non e' roba rigenerabile.
     Naturale: `ToonzFolder::getMyModuleDir() + "vosk"`, cercata accanto alle
     altre due;
   - un comando «Aggiungi lingua…» che prenda la CARTELLA del modello scaricata
     da alphacephei.com/vosk/models e la installi li'. Non serve
     riconfezionarla in `.zvosk`: il codice legge gia' le cartelle scompattate.
     Validare che dentro ci sia `am/final.mdl`, o si installa una cartella a
     caso e l'errore si vede al primo lip sync;
   - dire nell'interfaccia QUALE motore ha lavorato. `ZtoryLipSync::engineFor()`
     esiste gia' apposta: se uno ottiene tempi mediocri deve poter capire che ha
     lavorato Whisper perche' per la sua lingua non c'e' il modello, invece di
     concludere che il lip sync e' impreciso.

   ⚠️ **Non impacchettare altre lingue nel bundle**: en+it sono gia' 86 MB.
   Scaricabili a richiesta e' l'unica strada che scala.

   **SCARICARE i modelli dall'app — verificato il 2026-08-16, non ipotizzato.**
   Vosk pubblica un catalogo: `https://alphacephei.com/vosk/models/model-list.json`
   risponde 200 e contiene **32 modelli piccoli non obsoleti** con nome,
   dimensione e lingua (ar, ca, cn, cs, de, en-gb/in/us, es, fa, fr, hi, it, ja,
   ko, nl, pl, pt, ru, sv, tr, ua, vn e altre). Quindi «Aggiungi lingua…» puo'
   mostrare l'elenco invece di far cercare l'utente. Pesi: 40-160 MB, quindi
   servono avanzamento e annullamento.

   ⚠️ **L'ostacolo e' che i modelli sono `.zip` e Ztoryc non legge gli zip** —
   scelta deliberata: il commento in `tools/pack_vosk_model.py` dice che tirare
   dentro minizip avrebbe toccato la build su tre sistemi, «il punto dove questo
   progetto ha perso piu' tempo». Si risolve come per ffmpeg, Rhubarb ed espeak:
   **processo esterno**. `unzip` su macOS e Linux, `tar -xf` su Windows (dalla
   10 legge gli zip; ⚠️ il tar GNU di Linux invece NO, li' serve `unzip`).
   Quella tabellina va scritta in UN posto solo.

   Alternativa scartata: ospitare noi i `.zvosk` gia' confezionati. Download
   piu' piccoli e nessun unzip, ma 32 file da mantenere allineati a mano.
   Scaricare dalla fonte resta aggiornato da solo e lascia l'attribuzione a chi
   li pubblica.

   Da prevedere: verifica di cio' che si e' scaricato (il catalogo non da'
   checksum — almeno controllare che dentro ci sia `am/final.mdl` prima di
   dichiarare installata la lingua).

**D. Il segnale Kitsu «modifiche non pushate».** ⬅️ **L'UNICO ANCORA APERTO.** Non esiste nessuna nozione di
   «sporco» nel modello (cercato: nessun campo dirty/pending/lastSync). E'
   lavoro nuovo, non un'opzione da accendere.

---

#### L'impacchettamento (quello che restava dal 2026-08-16)



1. **Costruire whisper-cli ed espeak-ng dai tag fissati**, per i tre sistemi.
   ⚠️ NON si possono scaricare gia' pronti: i binari precompilati portano dentro
   percorsi di backend che fuori dalla loro macchina non esistono — provato il
   2026-08-15 copiando whisper-cli a mano, non funzionava. Sono due
   compilazioni CMake per piattaforma.
2. **Windows e Linux**: lo script condiviso li gestisce gia' (sceglie l'archivio
   giusto), mancano il passo che copia nel pacchetto e l'aggancio ai due
   workflow. Su macOS il passo c'e'.
3. **Allegare il sorgente di espeak alla release** — una riga nel job di
   pubblicazione. E' l'adempimento GPLv3: senza, spedire il binario non e'
   lecito. Il tag e' fissato apposta perche' l'archivio corrisponda al binario.

### Il resto della checklist di rilascio

- bump `toonz/cmake/ZtorycVersion.cmake` 0.12.0 -> **0.13.0** (69+ commit dal
  4 agosto: e' una minor, non una patch);
- **Rodney Baker c'e' gia'** nei ringraziamenti in-app accanto a Slam Rockwell.
  Se Franco intendeva ringraziarlo come **revisore upstream** e' un'altra riga,
  in un altro posto — da chiarire;
- controllo dashboard sponsor contro `SUPPORTERS.md` (il token `gh` non ha lo
  scope `read:user`: si guarda a mano);
- note bilingui **inglese per primo** con le tre sezioni OS;
- lanciare **tutti e tre** i workflow insieme, Linux compreso.

### Due cose da provare prima di spedire, non dopo

- **Le miniature delle sotto-scene** ora rendono ogni fotogramma invece di
  riusare il primo: e' corretto, ma cambia il comportamento in TUTTA
  l'applicazione. Va provato su uno storyboard pesante.
- **La rinomina che scollega i set** (la chiave e' il nome del livello). Franco
  ci e' incappato sapendo cosa cercare; un utente vede i set sparire. O
  l'adozione dei set orfani, o almeno renderlo leggibile.

---

### 🎬 ARCHITETTURA LIPSYNC — dove vive il dato (progetto di Franco, 2026-08-14)

> Franco mi ha fermato mentre mettevo un pulsante «prendi il copione dallo
> storyboard» nel pannello lipsync: *«aspetta aspetta, vediamo di capirci bene
> [...] qui c'e' da pensarla bene»*. Aveva ragione: quel pulsante tappa un buco,
> mentre la domanda vera e' **dove il dato deve vivere**. Questa sezione e' la
> risposta, ed e' da leggere PRIMA di scrivere codice sul lipsync.

**L'idea**: ogni shot genera una scena a parte che ha gia' il suo audio (anche
su piu' tracce, una per personaggio) e i suoi dialoghi divisi per personaggio.
Il lipsync si **prepara ed esporta da Ztoryc**, ma viene **applicato
dall'animatore** nello shot esportato, quando importa come scena la sotto-scena
del personaggio.

#### Cosa ESISTE gia' (non inventare)
- **`TXshSoundTextColumn`** — la colonna dialoghi classica dell'x-sheet, con
  `createSoundTextLevel(riga, listaDiTesti)`: una stringa per cella, cioe' per
  fotogramma. Gia' persistita, gia' disegnata dallo xsheet viewer, e **gia'
  gestita da `exportxsheetpdf`** — quindi finisce nell'exposure sheet STAMPATO.
- **I personaggi sono gia' asset** di tipo Character, sincronizzati da Kitsu.
- **L'export verso progetto** fa gia' il grosso: scorre il level set, risolve e
  copia i file con `decodeFilePath`, riscrive i percorsi a destinazione, salva
  le sotto-scene con `IoCmd::saveScene(SAVE_SUBXSHEET)`, staging + log.

#### I DUE campi che mancano, e sono i perni
1. **`PanelData::dialog` e' una stringa ANONIMA.** Deve diventare una **lista di
   battute, ognuna con un personaggio**. Non un campo singolo: in un pannello
   parlano in due, e un campo solo costringerebbe a spezzare i pannelli per
   ragioni sbagliate. Tutto il resto poggia qui.
2. **`Asset` non ha un percorso file** (ha uuid/type/name/kitsuAssetId/tasks/
   tags). Serve per l'export completo, vedi sotto.

#### Il legame traccia audio → personaggio
Va tenuto in **`ZtoryModel`**, NON dentro `TXshSoundColumn`: la regola del
progetto dice che l'audio si legge e non si tocca, e cosi' sopravvive anche ai
file aperti con Tahoma2D. Persistito nel `.ztoryc`.

#### Le tre decisioni, PRESE da Franco il 2026-08-14
1. **Dove gira il lipsync**: si **prepara ed esporta da Ztoryc**, si **applica
   dall'animatore** nello shot esportato quando importa la sotto-scena del
   personaggio. → Il **MouthSet deve VIAGGIARE col personaggio**, perche' viene
   usato dopo, altrove, magari da un'altra persona.
2. **Nelle celle vanno i FONEMI**, come nell'x-sheet tradizionale. Il testo
   normale si legge nel pannello shot board. (Io avevo proposto le parole:
   scelta di Franco, ed e' quella dell'animatore.)
3. **Una colonna per personaggio**, non una condivisa: *«altrimenti fa il
   lipsync anche delle battute dette da altri»*.

#### Whisper serve solo per i TEMPI
Il testo lo abbiamo gia' scritto noi. Il compito e' **allineamento**, non
riconoscimento — e questo e' anche il motivo per cui forse basta un modello
piccolo.

#### Una room dedicata
Idea di Franco: x-sheet + un pannello **Ztoryc shot board** in cui leggere i
testi mentre si anima.

---

### 👄 MOUTH SET — FILE ACCANTO AL LIVELLO (progetto di Franco, 2026-08-15)

> *«Dovremmo stabilire che il tal livello contiene le bocche e salvare un file
> associato al livello con i vari set diversi per posizione o espressione, cosi'
> che praticamente neanche serve aprirlo il popup di Rhubarb una volta salvato
> il personaggio e la sua bocca.»* Ambito dichiarato da lui: **cutout digitale
> con personaggi da libreria**.

#### 🔎 Scoperta che rimpicciolisce il problema
**Il pannello lipsync ha GIA' i dieci slot di Preston Blair** — vedi
`lipsyncpopup.cpp` ~riga 232: `A I`, `O`, `E`, `U`, `L`, `W Q`, `M B P`, `F V`,
`Rest`, `C D G K N R S Th Y Z`.
Cioe' l'interfaccia **chiede gia'** l'associazione fonema→disegno che Franco
vuole salvare, e poi **la dimentica**, ogni volta, per ogni shot. Il MouthSet
non e' una struttura nuova da inventare: e' **dare una casa a un dato che il
programma gia' raccoglie e butta via**.

#### Dove sta il dato: un SIDECAR accanto al file di livello
Non dentro il livello. Motivi, in ordine di peso:
1. **Viaggia con la cosa che l'animatore importa davvero.** Franco ha deciso che
   il lipsync si prepara in Ztoryc e si applica **dopo, altrove**, quando
   l'animatore importa la sotto-scena del personaggio: il dato deve stare
   attaccato al livello, non al progetto Ztoryc che l'animatore non ha.
2. **Non tocca il formato del livello**, quindi Tahoma2D continua ad aprirlo.
3. La sua **esistenza dichiara** che quel livello contiene le bocche — che e'
   esattamente il «stabilire che il tal livello contiene le bocche».

Contenuto proposto (XML, come il `.ztoryc`):
- il **personaggio** (uuid dell'asset + nome, per ritrovarlo dopo un rename);
- una lista di **MouthSet**, ognuno con: nome, **vista** (frontale/profilo/3-4),
  **espressione** (felice/triste), **variante** (bocca in su / in giu');
- ogni set = i **dieci slot** → un `TFrameId` del livello.

#### Conseguenza: il popup di Rhubarb non serve piu'
Con personaggio + set scelto, l'associazione e' gia' nota. Il pannello resta per
il caso in cui le bocche sono disegnate per quello shot, ma per il cutout da
libreria diventa una scelta a due voci: **quale personaggio, quale set.**

⚠️ **TRAPPOLA DA NON SCOPRIRE DOPO**: l'export che copia gli asset deve copiare
**anche il sidecar**, o il personaggio arriva all'animatore con le bocche e
senza le istruzioni per usarle. E' lo stesso genere di dimenticanza dei binari
helper `lzocompress` fuori dal bundle.

---

### 🎚️ LE FINEZZE DEL LIPSYNC — perche' funzionano (spiegato a Franco 2026-08-15)

Valgono **anche sull'uscita di Rhubarb**: non aspettano Whisper, e sono il pezzo
col miglior rapporto sforzo/risultato.

**1. Minimo due fotogrammi per viseme (a 24 fps).** Non e' una regola di
software, e' percezione: una bocca tenuta UN fotogramma non viene letta come una
forma ma come uno sfarfallio. L'occhio ha bisogno di circa 1/12 di secondo.
Applicazione: i segmenti troppo corti devono sparire, ma i due modi **non sono
equivalenti** — *allungarlo* rubando al vicino sposta il tempismo, *buttarlo*
perde un suono. Regola: se e' uguale a un vicino si fondono (gratis); se e'
diverso si butta **il piu' debole visivamente** — la chiusura M/B/P e le
aperture larghe vincono sul gruppo neutro `C D G K N R S Th Y Z`, che e' gia'
quasi una posa di riposo.

**2. Anticipare M, B, P di 1-2 fotogrammi.** Non e' un trucco, e' fisiologia:
per fare /m/ /b/ /p/ **le labbra devono essere GIA' chiuse** — il suono E'
l'apertura, lo scoppio che le separa. Quindi la marca temporale segna il momento
in cui le labbra **si aprono**, non quello in cui si chiudono. Mettere la bocca
chiusa sul fotogramma del suono e' essere in ritardo di 1-2 fotogrammi, ed e'
precisamente cio' che fa sembrare *doppiato* un lipsync in cui tutto il resto e'
giusto.

**3. ⚠️ Le due regole LITIGANO, ed e' qui che si sbaglia.** Anticipare la
bilabiale accorcia il viseme precedente, magari sotto il minimo; e il minimo,
applicato dopo, se lo rimangia. Ordine giusto: **prima anticipare, poi imporre
il minimo**, e trattare la chiusura come **PROTETTA** — mai lei quella accorciata
o buttata. Altrimenti la regola che corregge l'errore piu' visibile finisce
mangiata da quella che corregge il meno visibile.

**4.** I numeri vanno espressi **in tempo, non in fotogrammi** (2 frame a 24 fps
= 1/12 s; a 12 fps sono il doppio) e messi **su uno slider subito** — lezione
gia' pagata col disco di articolazione.

---

### 🔌 WHISPER + ESPEAK-NG — come entrano
Come **processi separati**, come Rhubarb e ffmpeg lo sono gia'. Non e'
un'analogia: **`thirdparty.cpp` ha gia' la macchina** (`checkRhubarb()`,
`autodetectRhubarb()`, percorso in preferenze con ricerca automatica) — i due
nuovi si infilano nello stesso schema. Il confine di processo e' anche cio' che
tiene pulita la licenza GPL-3 di espeak-ng.

Catena, **per personaggio**:
```
traccia audio del personaggio + le sue battute (testo che abbiamo gia')
  → whisper.cpp   parole con inizio/fine   ← ALLINEAMENTO, non riconoscimento
  → espeak-ng     parola → sequenza di fonemi
  → distribuzione dei fonemi nella finestra della parola
  → fonema → viseme (i 10 slot che il pannello ha gia')
  → le finezze qui sopra
  → fotogrammi → TXshSoundTextColumn (una per personaggio)
```
❓ Le opzioni esatte da riga di comando vanno verificate sulla versione che si
imballa: non sono state confermate.

---

### ✅ FATTO il 2026-08-17 (`ad3e3201e`) — EXPORT COMPLETO — gli asset linkati a file veri (idea di Franco, 2026-08-14)

> Verificato il 2026-09-24 su segnalazione di Franco: l'export controlla gli
> asset del breakdown una volta per asset, li importa nello shot (.tnz come
> sotto-scene, PSD con le impostazioni del progetto) e scrive il lip sync.
> I «Manca:» qui sotto sono storia.

*«Se dal production tracker gli asset fossero linkati a dei file reali, si
potrebbe pensare a un export ancora piu' completo, dove il programma provvede
anche a importare nello shot tutti gli asset necessari, anche i personaggi
importati come sotto-scene.»*

**Fattibile, e la parte pesante e' gia' scritta** (vedi sopra: l'export copia i
file, riscrive i percorsi, salva le sotto-scene). Manca:
1. **un percorso file su `Asset`** + il modo di assegnarlo (sfoglia, o dedotto
   dalla struttura cartelle di produzione, o da Kitsu);
2. ✅ **FATTO il 2026-08-15** (`d48f46b0e`) — scheda Breakdown nel tracker,
   modello + persistenza + pull da Kitsu (una chiamata per episodio, sola
   lettura). **DA COLLAUDARE sui 53 link veri.** Fatti anche: cartelle asset per
   categoria, `resolveAssetFile()`, Load/Import e opzioni PSD (default di
   progetto + scostamento per asset), legame a mano dalla scheda Breakdown.
   **RESTA il pezzo 3**: il passo che PIAZZA davvero gli asset nello shot.
   ~~quali asset servono a quale shot~~ → era il **BREAKDOWN di Kitsu** (segnalato da Franco, contratto verificato sull'istanza
   locale il 2026-08-14). Kitsu lo chiama *casting*.

   **Dato**: tabella `entity_link` — `entity_in_id` = lo SHOT,
   `entity_out_id` = l'ASSET, piu' `nb_occurences`, `label` (stringa libera) e
   un `data` jsonb. Nell'istanza di Franco ci sono **53 link** veri: p.es. lo
   shot `boh` usa `cittaIngranaggio` (Environment), `veicolo`, `macchina`,
   `piazza`, `parco`, con label «animate».

   **API**:
   - `GET /api/data/projects/<project_id>/entities/<entity_id>/casting`
     → array di `{asset_id, asset_name, asset_type_name, ready_for,
     episode_id, preview_file_id, nb_occurences, label, is_shared, project_id}`.
     Comodo: **`asset_type_name` arriva gia' qui**, non serve risolverlo a parte.
   - `PUT` sullo stesso URL, corpo = array di `{asset_id, nb_occurences, label}`.
   - Letture in blocco (da preferire, ora che ci leghiamo a un episodio):
     `/data/projects/<id>/episodes/<episode_id>/sequences/all/casting`,
     `/data/projects/<id>/sequences/<sequence_id>/casting`,
     `/data/projects/<id>/episodes/casting`.

   ⚠️ **IL PUT SOSTITUISCE TUTTO IL CASTING DELLO SHOT.** In
   `breakdown_service.update_casting()` c'e'
   `entity.update({"entities_out": [], "nb_entities_out": 0})` e poi ricrea i
   link dall'array ricevuto. Quindi **mandare una lista parziale CANCELLA il
   resto**: non e' un merge. E' la trappola numero uno di questa sincronia.

   **Nel tracker**: una **pagina Breakdown** (idea di Franco), righe = shot,
   con la lista degli asset necessari. Nel modello: shot → lista di
   (uuid asset, nb_occurences, label).

3. **un percorso file su `Asset`** — vedi punto 1 qui sopra;
3. il passo che li **piazza** nello shot: livelli caricati nel level set ed
   esposti in colonna, personaggi caricati come **sotto-scene**.

⚠️ **Rischio da tenere d'occhio, non da riaprire**: importare un personaggio
come sotto-scena e' parente stretto del percorso che ha il crash noto su «Salva
sotto-scena come scena, mesh non trovate» (nel blocco SOSPESI). E' la macchina
su cui questa feature poggia: meglio saperlo prima.

---

### ⚠️ Lavoro del 2026-08-14 sera, NON committato — da rivedere alla luce di sopra
- ✅ **Selettore di lingua** (`lipsyncpopup.cpp` + preferenza `lipSyncPhonetic`):
  **resta valido**, indipendente da tutto questo. La combo diceva «Recognizer» e
  spariva proprio quando carichi un file audio, cioe' dove serve; il default era
  l'inglese e su audio italiano PocketSphinx cercava parole inglesi. Ora si
  chiama «Dialogue language», e' sempre visibile quando Rhubarb gira, e la
  scelta si ricorda. Corretto anche un confronto su `currentText()` che si
  sarebbe rotto in ogni build tradotta.
- 🗑️ **Pulsante «From storyboard»: TOLTO** su decisione di Franco (*«visto che
  non serve lo toglierei»*), il giorno stesso in cui l'avevo scritto. Leggeva il
  dialogo **senza sapere chi parla**, cioe' gli mancava il perno del progetto qui
  sopra: sarebbe rimasto in giro come scorciatoia da disfare. Il suo posto lo
  prende il dialogo per personaggio.
  *(Resta annotato il modo per risalire allo shot corrente, se un domani
  servisse: confrontare la sotto-scena aperta — `ChildStack::getXsheet()` —
  con il `TXshChildLevel` delle celle delle colonne del top xsheet. La COLONNA
  sta in `ChildStack::AncestorNode::m_col` ma non ha accessori pubblici, e
  `childstack.h` e' core condiviso con Tahoma2D.)*
3. **Deformatori raster** ispirati a Krita ma **riscritti dai paper** (Krita e'
   GPL): MLS per il warp, Mean Value Coordinates per la cage.
4. **Libreria di rig riusabili**.
5. **Ricerca 2.5D Cartoon Models** (Rivers).
6. **Auto-shadow agganciato alla Light Arrow del Board** — esplicitamente
   **per ultimo**.


### ⏸️ SOSPESO — render sbagliato su sh110: quindici cause ESCLUSE, nessuna trovata (2026-08-07 notte)

Franco: «lasciamo perdere, vedremo con i prossimi progetti se risuccede».
Sospeso per decisione sua dopo una nottata di misure. **Il valore di quella
nottata e' l'elenco qui sotto: sono piste gia' pagate, non riaprirle.**

**Escluse CON MISURA (non per intuizione):**
1. Configurazione della build — era davvero sbagliata e va tenuta allineata, ma
   non e' questa causa.
2. Il codice, e quindi anche il merge 1.6.2 — **dato decisivo di Franco: lo
   stesso identico binario scaricato dal repo prima rendeva bene e poi no.**
3. Sincronizzazione cloud — Google Drive messo in pausa, nessun cambiamento.
4. ffmpeg / formato di uscita — la sequenza PNG ha lo stesso difetto.
5. Multithreading — `Dedicated CPUs` era gia' su Single.
6. Tiling — provato a cambiarlo.
7. Interruttori di visibilita' colonna — nessuna colonna con `status=2`
   (visibile nel viewer e spenta nel render); la logica dei bit e' **invertita**
   (bit acceso = nascosto), vedi `txshcolumn.cpp:780` e `:828`.
8. File mancanti — 89 percorsi su 90 si risolvono; l'unico no e' `+outputs/.tif`,
   un segnaposto. (Attenzione: `+extras` = `scenes/$scenepath/extras`, le
   sequenze sono `nome..ext`, e nei PSD il `#` separa il sotto-livello.)
9. Pezzi saltati in `doCompute` — 280 su 280 disegnati.
10. Allocazione texture — mai fallita, mai dimezzata.
11. Istante di valutazione viewer vs render — scarto **0** su 27 matrici.
12. Valore del controller squash — identico fra le due strade.
13. Mesh sbagliate — stessi file in entrambe; `row == frame` su 53 coppie.
14. Perdita di pezzi in `addPlasticDeformerFx` — tutte rinunce legittime
    (colonne col padre Table, `handle='B'`).
15. Deformazione e stacking order — `process()` (viewer) contro `processOnce()`
    (render): scarto **0.000** su 40 pezzi, ordine identico.

**Falso allarme da non ripetere:** le texture in ingresso risultano vuote in
alcune passate. **E' normale**: il render lavora a quattro tessere e un pezzo
che sta in un altro quadrante ha legittimamente zero pixel in quella tessera.
Ci avevo costruito sopra una spiegazione (cache fredda) che Franco ha demolito
in una riga: lui la preview del fotogramma la fa sempre prima del render.

**L'unica misura che puo' ancora discriminare**, quando ricapitera': confrontare
il log di un render **buono** con quello di un **cattivo** dello stesso
fotogramma. Serve prima riuscire a ottenere un render buono a comando.

**Vincolo piu' forte, da tenere fisso:** lo stesso binario, stessa scena,
preview calda in entrambi i casi, a volte rende bene e a volte no.

### ✅ RISOLTO — 1.6.2: «sparita l'icona della visibilita'» (2026-08-07 notte)

Non era il merge. Upstream 1.6.2 introduce la preferenza
`unifyColumnVisibilityToggles` **con default `true`**: sostituisce i due
interruttori della testa di colonna (occhio preview + camstand) con **uno solo**.
Peggio del cosmetico: quando si attiva, `ColumnCmd::unifyColumnVisibilityToggles()`
scorre tutte le colonne **anche nelle sotto-scene**, forza preview = camstand e
marca la scena modificata.
**Fix applicato** sul branch `merge/upstream-1.6.2`: default portato a `false`
in `toonzlib/preferences.cpp`, con commento che spiega perche'. Resta
attivabile a mano dalle preferenze. Ricompilato, rc=0.


### ⛔ RITIRATA — «il render dipende da DefLevelType» era SBAGLIATA (2026-08-07 sera)

**La bisezione sulle preferenze non ha provato niente: scriveva nel posto
sbagliato.** I file venivano copiati in `merge-1.6.2/stuff/profiles/users/
francobianco/preferences.ini`, che e' dove punta il `SystemVar.ini` del bundle —
ma l'app legge e scrive in **`Ztoryc-162.app/ztorycstuff/profiles/users/
francobianco/preferences.ini`**, dentro il bundle. Sei render, sempre lo stesso
file di preferenze davvero in uso, sempre «cattivo». Il segnale d'allarme c'era
ed e' stato ignorato: **in una bisezione valida deve uscire almeno un «buono»**.

Il file realmente usato contiene sei voci e **`DefLevelType` non c'e'**: la
1.6.2 ha sempre girato col tipo di livello al default, prima rendendo bene e poi
male. Quindi quella conclusione e' falsa, non solo non verificata.

**Cosa resta vero, e non e' poco:**
- Il difetto **non e' nel codice**: lo stesso binario scaricato dal repo prima
  rendeva bene e poi no (dato di Franco). Questo regge ancora.
- Il worktree 1.6.2 ha reso **bene** al primo render di stasera e **male** a
  tutti i successivi, **con lo stesso identico file di preferenze**. Quindi e'
  cambiato qualcos'altro, fuori dalle preferenze e fuori dal codice.
- Le preferenze restano **non testate** come causa: nessuna prova valida.

**Da fare la prossima volta:** cambiare le preferenze **dall'interfaccia**, non
da file, oppure agire su `Ztoryc-162.app/ztorycstuff/...`. E prima di dimezzare,
verificare che il metodo funzioni: una prova che deve dare «buono» e una che
deve dare «cattivo».

**Sospetto principale rimasto:** qualcosa nella scena o nei suoi file cambia fra
un render e l'altro e non torna indietro. Precedente identico il 2026-08-05
(«la causa era la SCENA, risolta reimportandola»). Da verificare sulle date di
modifica dei file del progetto — sh110 non e' stato trovato sui dischi
scansionati, serve il percorso.

### ~~🟡 CAUSA ISOLATA~~ (VOCE RITIRATA, vedi sopra) — DefLevelType (2026-08-07 sera)

**`DefLevelType=18` (Toonz Raster) invece del default `OVL_XSHLEVEL`=34 (Raster).**
Con quella riga nel `preferences.ini`, sh110 rende con molti personaggi ridotti a
pochi pezzi visibili. Senza, rende bene. Isolato per bisezione in sei render,
partendo dalle 32 voci del file di Franco e dimezzando.

**Rimedio per lavorare:** Preferenze → tipo di livello predefinito su **Raster**.

**Come ci si e' arrivati, e cosa NON era** (due giorni e mezzo di caccia):
- ❌ **non e' la configurazione della build.** Riconfigurare come la CI ha
  corretto il render su scene nuove, ma non sh110. Restava un miglioramento
  vero, non la causa.
- ❌ **non e' il codice.** Prova decisiva portata da Franco: *lo stesso identico
  binario scaricato dal repo prima rendeva bene e poi non piu'*. Un binario non
  cambia da solo → e' stato, non codice. Regge anche il dato del Mac Pro late
  2013 (stessa scena, codice vecchio, rende bene): macchine diverse hanno
  preferenze diverse.
- ❌ **non e' il merge 1.6.2.** Sembrava risolverlo solo perche' quel worktree
  aveva un `stuff/` vergine, quindi preferenze di default. Copiandogli quelle di
  Franco, la 1.6.2 sbaglia identica.
- ❌ ipotesi cadute lungo la strada, tutte verificate e scartate: troncamento
  `(int)frame` in `plasticdeformerfx.cpp` (upstream `10ffabce0`, applicato e
  provato: non cambia niente), spazio colore lineare (`362c7010a`), oggetti
  compilati con configurazioni diverse (falso: 1323 su 1324 ricompilati),
  implicit hold (escluso da Franco: la scena non e' stata creata con quello).

**DA CAPIRE (il difetto vero):** perche' una preferenza che riguarda la
*creazione di livelli nuovi* cambia il *render di una scena esistente*.
Punti dove viene letta: `tnztools/tool.cpp:623`, `toonz/tapp.cpp:309`, `:330`,
`:444`, `toonzqt/paletteviewer.cpp:770/812/910`, `toonz/iocommand.cpp:3623`.
In `tapp.cpp:444` decide se la scena riceve la palette full-color — pista
indebolita dal fatto che `txshsimplelevel.cpp:1226/1290/1449/2120` la carica
comunque per conto suo. Da guardare a mente fresca.
⚠️ Quando si trova: e' quasi certamente un **candidato PR upstream** (file core,
nessuno e' Ztoryc). Registrarlo in `UPSTREAM_PR_CANDIDATES.md`.

### 🔴 ~~MIGLIORATO MA NON RISOLTO~~ SUPERATO DALLA VOCE QUI SOPRA — render plastic e configurazione della build (2026-08-07)

Due giorni di caccia. **Non era la scena, non era il codice, non era Qt, non era
la RAM, non era l'ottimizzazione**: la nostra build locale non e' mai stata
configurata come quella che produce i rilasci. Il pacchetto 0.12 scaricato dal
repo renderizzava bene sulla stessa macchina; la nostra no; il merge 1.6.2 (codice
piu' recente) sbagliava uguale.

> ⚠️ **AGGIORNAMENTO 2026-08-07 sera — «anche il merge 1.6.2 sbagliava uguale» NON
> vale come prova.** Controllata la `CMakeCache.txt` del worktree
> `merge-1.6.2`: era configurata **esattamente come quella rotta** — `QT_PATH`
> sulla Qt 5.9.2 del 2017, `TIFF_INCLUDE_DIR` su libtiff44, `CMAKE_BUILD_TYPE`
> `Release`, deployment target vuoto. Quindi il merge non era un secondo
> esperimento indipendente: era lo **stesso** esperimento con codice diverso, e
> non dice niente su cosa causi il difetto. Riconfigurato il 2026-08-07 sera con la
> configurazione di master (che Franco ha verificato buona su scena nuova) e
> ricompilato da zero. Il worktree e' stato anche allineato a master
> (`a1468a00b`, merge senza conflitti).

**Differenze trovate** fra `ci-scripts/osx/tahoma-build.sh` e la nostra CMakeCache:
| | CI | nostra (rotta) |
|---|---|---|
| `QT_PATH` | `/opt/homebrew/opt/qt@5/lib/` | **`~/Qt5.9.2/5.9.2/clang_64/lib`** (Qt del 2017) |
| `TIFF_INCLUDE_DIR` | `thirdparty/tiff-4.2.0/libtiff/` | libtiff44 di Homebrew |
| `CMAKE_OSX_DEPLOYMENT_TARGET` | `12.0` | vuoto |
| `WITH_SYSTEM_SUPERLU` | `ON` | non impostato |
| `WITH_GPHOTO2` | `ON` | spento (libgphoto2 non installata) |
| CMake | 3.31.6 (fissata dal workflow) | 4.x |

Ricompilando con la configurazione della CI il render torna corretto **su scena
nuova** (verificato da Franco). La cartella di build nella radice e' stata
riconfigurata di conseguenza il 2026-08-07.

⛔ **MA NON E\' RISOLTO.** Franco, stesso giorno: «su quel progetto e su **sh110**
soprattutto continua a dare problemi». Quindi la configurazione della build era
**una** causa, non **la** causa: spiega perche' il pacchetto CI rendeva bene dove
la nostra build no, ma non spiega sh110. Restano in piedi le differenze fra
scene nuove e scene di quel progetto — ed e\' li\' che va guardato, NON di nuovo
nella configurazione.
**Dato correlato dello stesso giorno**: su uno shot precedente all\'IK il pin
finisce sul vertice sbagliato (voce qui sotto). Due sintomi diversi che
compaiono entrambi su materiale VECCHIO e non su scene nuove: vale la pena
chiedersi se abbiano la stessa radice, cioe\' una migrazione dati incompleta.

⚠️ **DA FARE perche' non si ripresenti**: `build_and_deploy.sh` non configura, usa
la cache esistente. Se qualcuno ricrea la build dir a mano torna il problema.
Andrebbe fatto configurare come la CI, o almeno avvisare quando la cache diverge.
⚠️ Non sappiamo ancora **quale** delle differenze fosse la causa: si trova
riaccendendone una per volta. Il sospetto e' `QT_PATH`, che finisce in
`CMAKE_PREFIX_PATH` e mescolava header di Qt 5.9.2 con librerie 5.15.18.

**Lezione**: quando lo stesso sorgente si comporta diversamente fra il pacchetto
rilasciato e la build locale, confrontare **tutta** la configurazione di build
PRIMA di cercare nel codice. Vedi [[feedback_instrument_the_fx_input_first]].

### 🔴 APERTO — su scene vecchie il pin va sul vertice sbagliato (2026-08-07)

Segnalato da Franco subito dopo il fix sopra: aprendo uno shot **precedente
all'introduzione della cinematica inversa**, mettendo il pin su un vertice lo
mette su un altro. Su scena nuova funziona bene.
Ipotesi da verificare per prima: e' un problema di **migrazione dati**. Il canale
`PIN` e' stato aggiunto a `SkVD` dopo, e se l'enum dei canali e' usato come
indice nella serializzazione, i file vecchi mappano i valori sugli slot
sbagliati. Guardare l'ordine di `SkVD::Channel` e come `PlasticSkeletonDeformation`
legge i vertici dai `.tnz` privi del canale PIN.


### 🔧 APERTO — richieste di Franco del 2026-08-05 (fine sessione)

**✅ FATTO — bucature (peg) ripristinate nello stage schematic.**
Era gia' tutto annotato piu' in basso in questo stesso file («ripristinare le
bucature (peg) nello stage schematic»): analisi completa e fix di due righe.
Applicato in `toonzqt/stageschematicnode.cpp`: ancoraggio del ciclo riportato ad
`'A'` come OpenToonz, e sugli indici positivi la porta mostra di nuovo **la
lettera** invece di sempre `"B"`. Default `"B"` invariato, geometria invariata.
Compila. **Da collaudare nello schematic**: la sequenza attesa e'
`… H2 H1 A [B] C D E …`.
⚠️ **Mio errore da non ripetere**: avevo cercato «A.B.C» nell'**header di
colonna** e concluso che non fosse annotato niente. Era annotato, e riguardava lo
**stage schematic**. Prima di dire «non c'e' nulla per iscritto», cercare il
concetto (bucature/handle/schematic) e non solo le parole dell'ultimo messaggio.

**✅ FATTO — toggle «Show Mesh» globale e persistente.** Comando
`MI_ZtoryShowMesh` nel menu Xsheet, aggiungibile alla Quick Toolbar, icona
`ztoryc_show_mesh` (maglia di triangoli con l'occhio della preview sopra, idea di
Franco). Dettagli nel CHANGELOG. **Non committato: da collaudare.** Se il default
va invertito (mesh nascosta all'avvio) e' una riga.

### 🔴 APERTO — crash su «Salva sotto-scena come scena», mesh non trovate

Segnalato il 2026-08-05, **mai indagato** (Franco mi ha fermato mentre cercavo il
log, e poi la giornata e' andata altrove). Salvando una sotto-scena come scena
non trova le mesh, e poi crasha. Per la regola «i crash vengono prima di tutto»
questo viene prima delle feature. Il crash handler scrive in
`QStandardPaths::AppLocalDataLocation + "/crash"`.
Possibile parentela con i percorsi delle mesh: nei log del render convivono due
radici diverse, `+extras/sh090/sub_2.0001.mesh` e
`+scenes/sh110/LIB_ZOMBIE01/extras/...`.

### 🟢 PRONTO, DA COLLAUDARE — merge Tahoma2D 1.6.2

Branch `merge/upstream-1.6.2` nel worktree
`/Volumes/ZioSam/tahoma2d-workspace/merge-1.6.2`, commit `0a430ad42`. 61 commit,
249 file. **Compila a freddo (ninja rc=0), mai aperto nell'app.** Non portato su
master di proposito: master resta releasable.
Zone da provare per prime: **xsheet** (`xshcolumnviewer` ha preso codice loro),
**file browser** (nodo Scene Folder nuovo), **preferenze**, e la **zona plastica**
(`plasticskeletondeformation.cpp` toccato da entrambi).
Quando e' collaudato: `git merge` su master senza conflitti, il lavoro e' gia'
tutto risolto.


### ✅ RISOLTO — i DMG macOS mancanti (2026-08-05)

**La 0.12.0 e' completa**: `Ztoryc-0.12.0-portable-osx-silicon.dmg` (163 MB) e
`Ztoryc-0.12.0-portable-osx-mactel.dmg` (186 MB) sono sulla release, insieme ai
sei asset Windows/Linux gia' presenti. Run `30978752717`, entrambi i job verdi.

**La causa non era la cache.** Nel log della run fallita (`30958746904`) c'e'
scritto `Cache not found for input keys` — la cache era **scaduta**, e lo script
e' stato eseguito davvero per la prima volta dopo mesi. E' fallito cosi', su
ARM64 e su x86_64 allo stesso modo:
```
Undefined symbols for architecture arm64:
  "_libintl_dgettext", referenced from: _camera_summary in la-library.o
make[3]: *** [ax203.la] Error 1
```

Tre cose impilate, commit `52ff4c16e`:
1. `camlibs/Makefile.am` del fork tahoma2d linka ogni camlib solo contro
   `libgphoto2.la` e `libgphoto2_port.la`, **mai contro `$(INTLLIBS)`** che
   invece `libgphoto2/Makefile.am` aggiunge (riga 70). Su glibc non si vede,
   `dgettext` sta nella libc; su macOS il `libintl.h` di Homebrew — installato
   da `tahoma-install.sh`, ultima riga — lo riscrive in `libintl_dgettext` e il
   simbolo non c'e' sulla riga di link dei camlib.
2. **Le intestazioni pubbliche si installano in un SUBDIR che viene DOPO
   `camlibs`**: ecco perche' il sintomo era `gphoto2/gphoto2.h` introvabile,
   dieci minuti dopo, in un punto che con gettext non c'entra niente.
3. Lo script **non aveva `set -e`**: dopo il `make` fallito partiva comunque
   `sudo make install` e il passo tornava zero. Per questo il difetto e' potuto
   restare li' per mesi, mascherato da una cache che si rinnovava a ogni
   rilascio.

Fix: `--disable-nls` (con NLS spento `i18n.h` rende identita' tutte le chiamate
gettext e libintl non viene piu' referenziato — si perdono solo le traduzioni
interne di libgphoto2, che Ztoryc non mostra), `set -euo pipefail`, controllo
dell'header dopo l'install, e clone idempotente perche' `thirdparty/libgphoto2_src`
sta nella cache e una entry parziale faceva morire `git clone`.
**`WITH_GPHOTO2` resta ON**: la cattura da fotocamera non e' stata toccata.

⚠️ **Candidato PR upstream, gia' annotato**: `ci-scripts/osx/tahoma-buildlibgphoto2.sh`
e' preso da Tahoma2D e a monte ha lo stesso difetto — nessun `set -e`, nessun
`--disable-nls`. La loro CI macOS ci sbattera' contro appena la cache scade.

### 🔐 DA FARE — la password Kitsu e' salvata IN CHIARO

Trovato il 2026-08-04 leggendo le preferenze per capire perche' l'integrazione
non compariva. In `~/Library/Preferences/com.ztoryc.Ztoryc.plist`:
```
Ztoryc.Kitsu.BaseUrl / Email / Password / PasswordSaved
```
La password e' **testo in chiaro**, leggibile con un `defaults read`. Il codice
la scrive con `QSettings` (`kitsuclient.cpp`, chiavi `Ztoryc/Kitsu/...`).

Su una macchina personale con un Kitsu in docker e' poco grave; diventa serio il
giorno che si punta a un **Kitsu remoto di produzione**, perche' a quel punto e'
la credenziale di un servizio vero, non di un container locale.

**Fix**: usare il **Portachiavi** di macOS invece del plist (e l'equivalente su
Windows/Linux). Contenuto: e' un solo punto di lettura e uno di scrittura.
⚠️ Franco e' stato avvisato che la password gli e' comparsa nell'output di un
comando durante la diagnosi: se la riusa altrove, valutare di cambiarla.

### 🔧 DA FARE — ripristinare le bucature (peg) nello stage schematic

**Due righe, analisi gia' completa.** Nei nodi dello schematic si puo' ciclare
solo fra **B** e gli hook numerici: le altre bucature — **A, C, D…** — sono
sparite. Franco le vuole indietro.

**Cosa sono** (confermato da Franco e dal codice): sono le **bucature del foglio
di animazione**. `B` e' quella centrale, il default; `A` e' una bucatura alla sua
sinistra, `C`, `D`… alla sua destra. In `TStageObject::getHandlePos` sono
scostamenti puramente orizzontali, `unit * (handle[0] - 'B')` con `unit = 8`.
Le **minuscole** sono gli stessi punti a **mezzo passo** (`0.5 * unit`).

**Perche' sono sparite** — `toonzqt/stageschematicnode.cpp`, ciclo della porta:

| | OpenToonz | Tahoma2D (e quindi noi) |
|---|---|---|
| ancoraggio | `index = handle[0] - 'A'` | `index = handle[0] - 'B'` |
| indice positivo | `handle = 'A' + index` → **la lettera** | `handle = "B"` → **sempre B** |

Spostando l'ancoraggio da A a B, la A e' finita a indice −1 — dove stanno gia'
gli hook (`H1` = −1) — e per uscire dalla collisione che si erano creati hanno
schiacciato **tutto** il positivo su `B`, perdendo anche C e D. E' un danno
collaterale di un refactoring, non una scelta.

**Il fix**: tornare allo schema di OpenToonz, quelle due righe. Con l'ancoraggio
ad `'A'` le lettere stanno tutte negli indici positivi e non collidono con gli
hook, che sono negativi. Sequenza risultante, verificata da Franco nell'app:
```
… H2  H1  A  [B]  C  D  E …
              ↑ default, invariato
```
⚠️ **Il default resta "B"** (`setHandle("B")` alla creazione della porta): non
si tocca. L'ancoraggio ad `'A'` e' **solo aritmetica interna** del ciclo.
⚠️ **La geometria non cambia**: `getHandlePos` continua a misurare da B, che
resta lo zero degli scostamenti. Indice del ciclo e offset geometrico sono due
cose separate.
⚠️ `tcrop(index, min, 25)` va gia' bene: `min` e' negativo solo per le colonne
(gli hook), zero per i pegbar.

**E' anche un candidato PR upstream**: Tahoma ha perso le lettere per un
incidente di refactoring, e OpenToonz accanto mostra il comportamento originale
— il confronto e' la dimostrazione.

### ✅ RISOLTO (confermato da Franco, 2026-09-24) — Production Tracker legge la scena sbagliata (2026-08-04)

**Segnalato da Franco, NON diagnosticato.** Nel progetto **MaggiolataZombie** il
Production Tracker non legge gli shot del file giusto: **ha caricato come
storyboard una scena che non lo è**.

**Precedente da leggere prima di indagare** — non è il primo caso di questa
famiglia: la «cross-project contamination» risolta col commit `84cba915e`
(`m_shots` del modello non troncato → `setShotsFrom` prima del publish, più un
firewall meno aggressivo, preservando il multi-storyboard). Da verificare
subito se questa è una **regressione** di quel fix, un caso che quel fix non
copriva, oppure un problema diverso di **individuazione** della scena
storyboard — cioè con quale criterio il tracker decide che una scena *è* uno
storyboard. Se il criterio è euristico (nome, presenza di sotto-scene, conteggio
colonne) è lì che va guardato per primo.

**Da chiedere a Franco quando si riprende**: quale scena si aspettava e quale ha
caricato — i due nomi sono il dato che discrimina fra le tre ipotesi.

Priorità: **dopo i crash aperti** (regola: i crash vengono prima di tutto), ma
prima delle feature — è un dato di produzione sbagliato, non un fastidio.

### 🆕 DA FARE (giugno 2026) — in cima per priorità

**✅ FATTO — "Generate Path from Keys": crea la spline dalle chiavi**
(idea di Franco, 2026-08-03). **Scritto, collaudato e su master** lo stesso
giorno, commit `703397712`. Il progetto qui sotto e' stato seguito quasi alla
lettera; le due cose che sono cambiate strada facendo:

- la domanda «cosa fanno x e y quando c'e' una spline» aveva una risposta
  migliore del previsto: **non vengono lette affatto** (`computeLocalPlacement`
  fa uno switch sullo stato), quindi non c'era niente da azzerare e staccare la
  spline riporta il movimento originale;
- serviva un getter `TStageObject::getFrameCenter()`, che non c'era: su un
  percorso il piazzamento e' `puntoSpline - frameCenter`, quindi la curva va
  traslata di quello o l'oggetto si sposta.

Dettagli e verifiche nel CHANGELOG 2026-08-03d e in UPSTREAM_PR_CANDIDATES.

*(Il testo che segue e' il progetto originale, tenuto perche' spiega il PERCHE'
delle scelte. Non e' piu' una cosa da fare.)*

⚠️ **DUE COMANDI SEPARATI, non uno.** Nato come "Path & Roving" unico, **diviso
su decisione di Franco** e ha ragione: rispondono a due domande diverse (la
forma della traiettoria / il timing), si compongono meglio (percorso senza
ridistribuire, oppure roving su una spline disegnata a mano), e **meta' e' gia'
fatta** — `Even Speed Along Path` funziona su qualsiasi `posPath`, comunque sia
nata la spline. Resta da scrivere **solo** la generazione del percorso.

**Dove sta il comando:** NON nel menu del grafico come gli altri, perche' le
chiavi del percorso si selezionano nella colonna keyframe dell'xsheet o nel
viewer con l'Animate tool — dove il movimento si vede. E ha senso: prende un
OGGETTO e ne cambia il modo di muoversi, non tocca una curva sola.

**Il problema.** Un movimento di camera che va a destra, sale stringendosi, poi
ridiscende allargandosi. A mano si fanno tre chiavi X/Y e il movimento viene
**segmentato**: angolo alla chiave centrale. Disegnare la spline a mano per
avere l'arco è scomodo, ed è il motivo per cui Franco non usa quasi mai le
spline (e quindi nemmeno `posPath`).

**L'idea.** Selezioni le chiavi, un comando **Path & Roving**: crea la spline
che ci passa attraverso, aggancia l'oggetto, converte le chiavi in `posPath` e
mette quelle intermedie **dove devono stare** nel tempo.

Perché e' la strada giusta: la spline **e' gia'** l'accoppiamento X/Y che
serviva, collaudato. Rovingare X e Y separatamente li desincronizza e deforma
la traiettoria — il roving accoppiato multi-canale (l'opzione "C" scartata)
diventa **inutile** se la traiettoria e' una spline.

**API verificate, ci sono tutte:**
- `TStageObjectTree::createSpline()` (`tstageobjecttree.cpp:672`) — id,
  registrazione e addRef gia' fatti
- `TStageObject::setSpline()` (`tstageobject.h:235`)
- `TStroke::getLength()`, `getParameterAtLength()`, `getLengthAtControlPoint()`
- **`T_Path` e' documentato come «position along the spline, as a PERCENTAGE OF
  THE LENGTH»** — quindi «velocita' costante» = «posPath lineare nel tempo»,
  esatto e non approssimato. La spline generata e' una spline normale,
  editabile a mano dopo: era il requisito di Franco.

**I passi:** leggere X/Y alle chiavi selezionate → costruire una `TStroke`
Catmull-Rom→Bezier che ci passi (la **stessa matematica dell'Auto Bezier**, ma
nello spazio invece che nel tempo) → `createSpline` + `setSpline` → convertire
le chiavi in `posPath` alla percentuale di lunghezza di ogni punto → distribuire
le intermedie con `KeyframeSetter::distributeEvenly` (**gia' scritta**).

⚠️ **Da verificare PRIMA di scrivere:** quando un oggetto ha una spline, cosa
fanno X e Y? Se restano attive come scostamento dal punto sul percorso vanno
azzerate, o il movimento raddoppia. Mezz'ora di lettura del codice di placement.

**Avvertenze:** operazione trasformativa (da due curve a spline+posPath, l'undo
copre ma non e' un ritocco); decidere cosa fare se l'oggetto ha **gia'** una
spline; abilitare da **tre** chiavi in su (con due genera una retta); con molte
chiavi la spline ondeggia e va clampata come le curve.

**✅ FATTO — Tangenti e distribuzione (2026-08-03), nel menu del grafico:**
**Auto Bezier** (tangente dai vicini + clamp anti-sorpasso, come l'Auto Clamped
di Blender / Auto di Maya; nome preso da AE perche' "Smooth" in Blender e' un
filtro che sposta i VALORI), con opzione in Preferences → Animation per
applicarlo mentre si mettono le chiavi; **Flat** (tangente orizzontale: marca un
estremo, e disfa un Auto Bezier che ha indovinato male); **Copy/Paste Tangents**
(copia la FORMA — maniglie come frazione della larghezza e del dislivello del
segmento — quindi incollata altrove da lo stesso carattere con ampiezza
diversa); **Even Speed Along Path**, il roving one-shot su `posPath`.

Il roving e' **one-shot di proposito**: nessun flag sul keyframe, quindi niente
tocca il formato file e niente deve ricalcolarsi da solo. Sposti una chiave
d'estremita' e rilanci. La versione persistente stile AE (chiave che si
riposiziona da sola) e' la stessa cosa piu' un flag serializzato: da valutare
solo se l'uso lo chiede davvero.

Scartate perche' **gia' presenti**: time reverse (c'e' nel revert di edit
cels/keys) e ghosting delle curve (le curve non correnti si vedono gia', ora
anche col tratteggio per colonna).

**✅ FATTO — Function Editor (2026-08-02/03), su master.** Aggancio allo xsheet,
ricerca, filtro animati globale, multi-selezione albero e grafico, visibilita' e
interpolazione in blocco, selezione multipla di **segmenti**, spostamento e
scalatura nel tempo su piu' curve, tratteggio per colonna, hint contestuali.
Collaudato da Franco. Testo inglese per la proposta upstream (a **Tahoma2D e
OpenToonz**) in Drive → `FUNCTION_EDITOR_UPSTREAM_EN.md`, con la traccia del video.

**DECISIONE DI FRANCO (2026-08-04): nessuna fretta di rilasciare, si vuole il
Function Editor DEFINITIVO.** Speed graph e curve linkate si fanno subito;
l'agente aveva proposto di rilasciare prima e la proposta e' stata scartata.

### NEW — Shot ops in edit-shot mode (task 53)

**Priorità: MEDIA-ALTA | Tipo: NEW | Stima: 1 sessione**

Quando l'utente è dentro una sub-scena (edit-shot mode), i comandi shot
dell'animatic (Copy Shot, Clone Shot, Cut Shot, Paste Shot, Delete Shot, ecc.)
sono disabilitati perché il focus è sull'xsheet nativo della sub-scena.

Obiettivo: estendere la disponibilità di questi comandi anche dall'interno
della sub-scena, esattamente come già fatto per `MI_ZtoryNewShotAfter` (Add Shot).

**Approccio:** nessun nuovo meccanismo — solo rimuovere il guard / condizione
che disabilita i comandi shot quando `isInsideSubScene()` è true. Verificare
che ogni comando operi correttamente sul main xsheet (non sulla sub corrente)
e che il ritorno al main dopo l'operazione sia coerente (resequence + Board sync).

**Comandi da abilitare:**
- `MI_ZtoryCopyShot` / `MI_ZtoryCloneShot`
- `MI_ZtoryCutShot` / `MI_ZtoryPasteShot`
- `MI_ZtoryDeleteShot`
- `MI_ZtoryMergeShots` (valutare se ha senso in edit-shot mode)

**Test:** eseguire ogni operazione stando dentro uno shot, verificare che
Board + timeline si aggiornino correttamente all'uscita.

**File:** `ztoryanimatic.cpp` (enablement dei comandi), `ztoryshotops.cpp` (logica).

---

### NEW — Custom logo nel PDF storyboard (task 54)

**Priorità: MEDIA | Tipo: NEW | Stima: 1 sessione**

Il PDF di export dello storyboard mostra attualmente il logo Ztoryc nell'header
di ogni pagina. L'utente deve poter sostituirlo con il logo del proprio studio/progetto.

**Design:**
- Preferenza per-progetto (salvata nel `.ztoryc`): path a un file immagine logo
  (PNG/SVG, trasparenza supportata)
- Campo nelle impostazioni export: "Logo personalizzato" con browse + preview
- Se nessun logo è impostato, comportamento attuale (logo Ztoryc)
- Dimensione logo: adattata all'area header esistente (max height ~40px nell'header)
- Opzione "Nessun logo" per export completamente puliti

**File:** `storyboardpanel.cpp` (dialog export + onExportPdf),
`ztorymodel.h/.cpp` (campo preferenza logo path),
eventualmente `ztoryexport.h/.cpp`.

---

### ✅ FATTO (2026-06-20) — Altezza tracce video/audio regolabile (task 55)

**Priorità: MEDIA | Tipo: NEW | Stima: 1 sessione | STATO: COMPLETATO**

> Implementato: handle di resize sul bordo inferiore (grip 5px) per video e
> audio track, cursore SizeVerCursor, min 24 / max 120px, persistenza QSettings
> (Ztoryc/VideoTrackHeight, Ztoryc/AudioTrackHeight) con altezza audio condivisa.
> Bonus: label audio progressiva (nome->volume->solo L/M/S), nome shot centrato
> verticalmente, fix diradamento label timecode in zoom (QFontMetrics).

Le tracce video e audio nella timeline animatic hanno altezza fissa.
L'utente deve poter ridimensionarle verticalmente per adattare la
densità visiva al proprio workflow (più spazio per vedere le waveform,
meno spazio per avere più tracce in vista).

**Design:**
- Handle di resize tra tracce (drag verticale sul bordo inferiore di ogni traccia)
- Altezza minima: ~24px (solo label + mute/lock); altezza massima: ~120px
- Altezza video track e audio track indipendenti
- Persistenza per-progetto nel `.ztoryc` (o in preferenze globali — decidere)
- Cursore `SizeVerCursor` sull'hover del bordo

**Nota:** per le tracce audio, l'altezza influenza la visibilità della waveform
(già renderizzata in `QImage` viewport-aware) — verificare che il repaint
della waveform si adatti alla nuova altezza senza ricalcolo completo.

**File:** `ztoryanimatic.h/.cpp` (ZtoryVideoTrack, ZtoryAudioTrack —
mouse events + paintEvent), `ztorymodel.h/.cpp` (persistenza altezze).

---

### NEW — Thumbnail Room (task 56) ⭐ FEATURE MAGGIORE

**Priorità: MEDIA | Tipo: NEW | Stima: 4-5 sessioni**

Nuova room dedicata al rough sketching rapido di tutto lo storyboard
su un unico canvas con griglia, prima di costruire la timeline reale.

#### Concept

L'utente disegna thumbnail grezzi su un canvas grande con griglia 3×N,
seleziona le celle per formare gli shot (anche non-rettangoalri: L, Z, pan),
riordina gli shot con drag&drop, poi esporta tutto in Board + timeline
con un click.

#### Canvas e griglia

- Room `ZtoryThumbnailRoom` con `QScrollArea` 2D (pan H e V)
- Griglia fissa **3 colonne × N righe** (N cresce automaticamente)
- Overlay griglia disegnato in Qt (non vettoriale — solo guida visiva)
- **Un unico livello PLI** (`Thumbnail.pli`) che si estende su tutto il canvas;
  ogni cella corrisponde a una regione dello spazio PLI
- Strumenti di disegno nativi Tahoma2D accessibili (vettoriale/raster)
- La PLI è salvata nel `.ztoryc` come risorsa permanente

#### Selezione e raggruppamento in shot

- Click su cella → selezione singola
- Shift+click / drag → selezione multipla (qualsiasi forma: L, T, Z, ecc.)
- Comando "Assegna a nuovo shot" → raggruppa le celle selezionate in uno shot
  con label editabile (SQ/SH assegnati automaticamente dal modello)
- Celle non assegnate = grigie; celle assegnate = bordo colorato per shot
  (colore distinto per shot, come nel Board)
- Un click su un gruppo già assegnato → lo seleziona come shot corrente

#### Riordino shot

- Drag&drop dei gruppi-shot nella griglia per riordinare la sequenza
  (stessa UX del Board)
- Il riordino aggiorna l'ordine di export ma non sposta fisicamente i disegni
  nel canvas (i disegni restano dove sono — l'ordine è logico)

#### Export to Board

Bottone "Export to Board" nella toolbar della room. Per ogni shot, in ordine:

**Caso normale (1 cella o N celle non panoramiche):**
- Ogni cella → 1 frame nel livello `Rough` della sub-scena
- N frame in sequenza nell'xsheet della sub-scena (frame 1, 2, … N)

**Caso panoramica (N celle adiacenti marcate esplicitamente come pan):**
- L'utente seleziona le celle e usa "Unisci come panoramica" (comando esplicito)
- Il sistema calcola il **bounding box** dell'unione delle celle
- Renderizza la regione corrispondente dalla PLI come **1 unica immagine larga/alta**
  (le celle vuote nel bounding box = area trasparente/bianca)
- L'immagine viene importata come singolo frame nel livello `Rough` della sub-scena
- Il frame viene esposto per la durata dello shot nell'xsheet (hold lungo)
- La camera si muoverà sopra quell'immagine in produzione

**Per tutti i casi:**
- Crea la sub-scena dello shot (come `onAddShot`) se non esiste già
- Importa i frame come livello `Rough` (TLV o PLI, da decidere — PLI mantiene
  il vettoriale se il disegno è vettoriale)
- Inserisce lo shot in ZtoryModel → Board + timeline aggiornati
- Gli shot già esistenti con lo stesso SQ/SH vengono aggiornati, non duplicati

#### Persistenza

- Le celle assegnate, i raggruppamenti shot e l'ordine sono salvati nel `.ztoryc`
- La PLI `Thumbnail.pli` è salvata come livello nella scena

#### File

```
toonz/sources/toonz/ztorythumbnailroom.h/.cpp   (nuova room + canvas)
toonz/sources/toonz/ztorythumbnailpanel.h/.cpp  (pannello con griglia + toolbar)
toonz/sources/toonz/ztorymodel.h/.cpp           (persistenza raggruppamenti)
toonz/sources/toonz/mainwindow.cpp              (registrazione room)
```

#### Fasi di sviluppo

1. **FASE 1** — Room + canvas PLI + griglia overlay + pan H/V
2. **FASE 2** — Selezione celle, assegnazione shot, colori bordo
3. **FASE 3** — Drag&drop riordino shot
4. **FASE 4** — Export to Board (caso normale)
5. **FASE 5** — Export to Board (caso panoramica: bounding box + merge immagine)

---

### NEW — Export to Worksheet Excel (task 57)

**Priorità: MEDIA | Tipo: NEW | Stima: 1-2 sessioni**

Al termine dello storyboard, genera un file `.xlsx` di production management
con i dati dello storyboard già compilati e colonne task pronte per il tracking.
Versione "easy" in attesa dell'integrazione Kitsu (M5).

#### Struttura del file generato

**Header (prime righe):**
| Campo | Valore |
|---|---|
| Produzione | (da ZtoryModel o input utente) |
| Episodio | (da nome file `.ztoryc` o input) |
| Titolo | (da metadati progetto) |
| Data export | (automatica) |
| Versione | (numero di versione storyboard) |

**Tabella shot (una riga per shot):**

| SQ | SH | Titolo | Timing (sec) | Frame | Note | Layout | Animazione | Sfondi | VFX | Compositing | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 010 | 010 | … | 2.5 | 60 | … | TODO | TODO | TODO | - | TODO | TODO |

**Colonne task:** pre-impostate con valori TODO; l'utente le modifica
manualmente nel file Excel (dropdown con TODO/WIP/WFA/RETAKE/DONE).

**Colonne task configurabili:** lista editabile nelle preferenze progetto
(default: Layout, Animazione, Sfondi, VFX, Compositing). L'utente può
aggiungere/rimuovere/rinominare colonne prima dell'export.

**Formattazione:**
- Header colorato (colore accent Ztoryc `#e8b84b`)
- Celle status con colori distinti per valore (verde=DONE, giallo=WIP, ecc.)
- Colonne SQ/SH frozen (sempre visibili scrollando a destra)
- Row alternata per leggibilità

#### Integrazione UI

- Voce "Export → Production Worksheet…" nel menu File o nel dialog export animatic
- Dialog pre-export: nome produzione, episodio, titolo, selezione colonne task
- Output: file `.xlsx` nella directory del progetto (o path scelto dall'utente)

#### Nota Kitsu

Quando M5 (integrazione Kitsu) sarà implementata, il Worksheet potrà essere
generato leggendo gli status reali da Kitsu invece di TODO fissi — o potrà
essere il punto di import iniziale per popolare Kitsu da zero.

**Dipendenza:** libreria `xlsxwriter` o `openpyxl` (già disponibile nell'ambiente
o da aggiungere come dipendenza Python/bundled). Valutare se implementare in
Python (script bundled) o in C++ con una libreria xlsx minimale.

**File:** `storyboardpanel.cpp` (voce menu + dialog),
`ztoryexport.h/.cpp` (logica generazione xlsx),
`ztorymodel.h/.cpp` (lettura dati shot + preferenze colonne task).

### NEW — Import da carta nella Thumbnail room (task 63) ⭐ FEATURE MAGGIORE

Richiesta dagli utenti; il riferimento è il *print worksheet / import worksheet* di
**Storyboarder** (open source). Ciclo completo: **stampa la griglia → disegna a matita →
fotografa o acquisisce → i pannelli rientrano nella thumb room** già raddrizzati e
ritagliati, da lì *Send to Board* esistente fa il resto senza modifiche.

#### Perché è fattibile senza dipendenze nuove (verificato 2026-07-24)

- **OpenCV 4 è già linkato nell'eseguibile `toonz`** su tutte le piattaforme
  (`toonz/sources/CMakeLists.txt:532` e seguenti) → omografia, soglia adattiva e
  rilevamento marker sono disponibili subito da `ztorythumbnail*.cpp`.
- **`QPdfWriter` già in uso** per l'export PDF del Board
  (`storyboardpanel.cpp:7317`) → la stampa del foglio riusa lo stesso schema
  A4/300dpi/`QPainter`.
- **Webcam già disponibile**: `stopmotion/webcam.h`, `getWebcamImage(TRaster32P&)` →
  la variante "acquisisci" invece di "importa file" è quasi gratis.
- **Il canvas è fatto apposta**: `ZtoryThumbnailCanvas` è UN raster contiguo con i box
  come rettangoli logici → incollare un pannello ritagliato è un blit in un rettangolo
  noto. Serve solo il duale di `panelRaster()` (`ztorythumbnailcanvas.h:106`), che oggi
  esiste **solo in lettura**.

#### Fase 1 — Stampa del foglio (`Print Sheet…`)

Bottone nella toolbar della thumb room. PDF A4, landscape se la camera è 16:9, con
tanti box quanti ne entrano **mantenendo l'aspect della camera** (2×3 sta comodo, 3×3
si stringe — vedi nota risoluzione). Se la griglia in scena è più grande di un foglio
(es. 4×15) escono **più pagine**. Su ogni foglio:

- **4 marker di registro** agli angoli, di cui **uno diverso dagli altri** per dare
  l'orientamento → una foto ruotata o capovolta si raddrizza da sola.
- **Codice pagina** stampato (testo leggibile + fila di quadratini binari): scena,
  numero pagina, riga/colonna di partenza nella griglia. All'import il software sa
  **dove incollare senza chiederlo**.
- **Cornici dei box in ciano chiaro.** All'import si legge il **canale rosso** della
  foto: il ciano sparisce, la matita nera resta. È il trucco che evita che la cornice
  stampata finisca dentro il disegno.

#### Fase 2 — Acquisizione

Tre sorgenti, stessa pipeline a valle:
- **File** (JPG/PNG/PDF), multi-selezione = più fogli in una volta.
- **Webcam / capture card** riusando `Webcam` di stopmotion.
- *(v2)* cartella "watch" per le foto che arrivano dal telefono via sync.

#### Fase 3 — Raddrizzamento e ritaglio (il cuore)

1. Grayscale + `cv::adaptiveThreshold` per isolare i marker.
2. `findContours` con **gerarchia** → quadrati concentrici → i 4 centri; il marker
   asimmetrico dà l'ordine.
3. `getPerspectiveTransform` + `warpPerspective` verso un rettangolo di dimensione
   nota → **prospettiva, rotazione e foto storta corrette in un colpo solo**.
4. Lettura del codice pagina (ora è in posizione nota).
5. **Normalizzazione carta**: divisione per il fondo sfocato (`GaussianBlur` largo +
   `divide`) → via ombre e vignettatura, carta bianca uniforme. **Non binarizzare di
   default** — la matita coi suoi grigi è più bella; toggle *"tratto secco"* per chi
   lo vuole.
6. **Ritaglio per geometria**, non per detection: dopo il warp i box sono in posizione
   nota. Resample alla dimensione del box nel canvas → blit nel raster contiguo.
7. Pannelli sotto soglia d'inchiostro → **saltati**, non sovrascrivono quello che c'è
   già (riusare la logica di `isPanelEmpty`).

#### Fase 4 — Anteprima e conferma

Dialog con: immagine raddrizzata + griglia sovrapposta, slider contrasto/soglia,
spunte per-pannello (quali importare), scelta *Sostituisci / Fondi*. Commit come **una
sola operazione undo** — `pushUndo()` full-canvas esiste già.

#### Punti delicati (decisi/da tenere d'occhio)

- ⚠️ **La CI non compila i moduli contrib di OpenCV**
  (`ci-scripts/osx/tahoma-buildopencv.sh` non passa `OPENCV_EXTRA_MODULES_PATH`):
  `cv::aruco` c'è sul Mac di sviluppo via brew (4.13) ma **non è garantito nei binari
  di release**. Da OpenCV 4.7 ArUco sta in `objdetect` (modulo main), ma dipende dalla
  versione del fork `tahoma2d/opencv`. **Decisione: marker fatti a mano** (quadrati
  concentrici stile finder pattern QR, ~80 righe con `findContours` + gerarchia) →
  usano solo `imgproc`, presente ovunque, e la domanda non si pone.
- **Risoluzione.** A4 fotografato con telefono 12MP ≈ 3000px sul lato lungo. Con 2×3
  box il pannello esce ~1400×800 (ottimo); con 4×4 box scende a ~700×400 — ancora
  buono per una thumb, ma è il limite. **Da dire nell'UI** quando si sceglie quanti
  box per foglio.
- **HEIC dell'iPhone**: Qt non lo legge di serie. Supportare JPG/PNG/PDF e documentare
  "esporta in JPEG".
- *(v2, non ora)* **Fallback "carta libera"**: foto di un foglio senza griglia
  stampata, riquadri disegnati a mano, riconosciuti via contours. Meno affidabile;
  Storyboarder stesso non lo fa.

#### Stima e ordine di lavoro

1. **Stampa del foglio** — mezza giornata, ed è **utile da sola**: si può già stampare
   e disegnare mentre il resto non esiste.
2. **Import da file** (marker + warp + ritaglio) — una sessione buona. È la parte vera.
3. **Dialog di anteprima** — mezza giornata.
4. **Sorgente webcam** — piccola, appoggiata alla pipeline già fatta.

**File:** `ztorythumbnailpanel.h/.cpp` (bottoni toolbar + dialog),
`ztorythumbnailcanvas.h/.cpp` (`setPanelRaster()` — il duale di `panelRaster()`),
nuovo `ztorypapersheet.h/.cpp` (stampa PDF del foglio + pipeline OpenCV di
raddrizzamento/ritaglio), `stopmotion/webcam.h` (riuso, sola lettura).

## File Structure

toonz/sources/toonz/storyboardpanel.h/.cpp   -- Board room
toonz/sources/toonz/ztorymodel.h/.cpp        -- Singleton data model
toonz/sources/toonz/ztoryanimatic.h/.cpp     -- Animatic panel + viewer
toonz/sources/toonz/ztorymonitorpanel.h/.cpp -- Monitor panel (secondo monitor)
toonz/sources/toonz/ztorystartup.h/.cpp      -- Startup dialog
toonz/sources/toonz/icons/dark/ztoryc/       -- SVG icons (21 files)
toonz/sources/toonz/toonz.qrc               -- Icon registration
toonz/sources/stopmotion/webcam.h/.cpp       -- Webcam + AVCapture
toonz/sources/toonzqt/txshsoundcolumn.h/.cpp -- Audio column
toonz/sources/image/tzl/tiio_tzl.cpp        -- TLV save
toonz/sources/toonz/main.cpp                -- Single-instance guard
toonz/sources/toonz/mainwindow.cpp           -- Workflow switch + room switcher
toonz/sources/toonzlib/timage_cache.h/.cpp  -- TImageCache
toonz/sources/toonzlib/toonzscene.h/.cpp    -- ToonzScene
toonz/sources/image/ffmpeg/               -- ffmpeg plugin + formati video

