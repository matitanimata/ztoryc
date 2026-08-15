# Ztoryc — Animatic Panel: Task List for Claude Code

> Aggiornato 2026-05-29c. Task completati ridotti a una riga.
> Per le spec storiche dei task DONE vedere ANIMATIC_TASKS_ARCHIVE_2026-05.md e git history.
> NOTA: questo è il file canonico (puntato dal symlink ~/ZtorYc/ANIMATIC_TASKS.md);
> sostituisce ANIMATIC_TASKS2205.md, ANIMATIC_TASKS2305.md e tutti i precedenti.

---

## LEGEND

- BUG = existing code that is broken
- NEW = feature that does not exist yet
- MOD = existing code that needs modification

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

## Task aperti

---

### 🆕 Segnalazioni Franco 2026-07-19 (da triage/investigare)

Raccolte a fine sessione. Le voci con ✅ hanno una causa individuata **leggendo il codice**
(non verificata a runtime); le altre sono ancora da investigare da zero.

**Animatic / trimming**
1. **FEATURE — durata shot visibile durante il trim + timing manuale.** Mentre si trimma,
   mostrare la durata dello shot sulla timeline; e poter **impostare a mano il timing** della
   scena invece che solo trascinando.
2. **BUG — play riparte da frame 1 (o dal mark in) durante il trim** invece che dalla posizione
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

### BUG (da investigare) — Crash palette-switch su click/apertura shot (macOS)

**Priorità: MEDIA-ALTA | Tipo: BUG | Segnalato: 2026-05-31 (SB_APPENNINGERS)**

Famiglia di SIGSEGV quando si seleziona/apre uno shot dall'animatic: lo switch di
colonna/xsheet aggiorna la palette del livello corrente → oggetti che ascoltano la
palette crashano mid-switch (palette/livello stale). Due varianti osservate:

- `shotClicked → onColumnIndexSwitched → updateXshLevel → setPalette →
  editLevelPalette → setPalette → StyleEditor::onStyleSwitched()` → SIGSEGV
  (Crash-20260531-015855). Core Tahoma (libtoonzqt).
- `onShotDoubleClicked → openSubXsheet → onXsheetSwitched → updateXshLevel →
  setPalette → ToonzRasterBrushTool::onColorStyleChanged()` re-entrante → SIGSEGV
  (Crash-20260530-231701). **GIÀ FIXATO** in `8f8740628` (guardia re-entrancy).

Lo StyleEditor variant NON è coperto dal fix del brush. Pattern comune: il rapido
cambio colonna/xsheet dall'animatic emette un cascata di setPalette mentre il
livello/palette puntano a dati transitori. Possibile fix Ztoryc-side: deferire
(QTimer 0) il cambio colonna/palette in `shotClicked`/`onShotDoubleClicked`, o
guardia anti-reentrancy lato chiamante. Da indagare con SB_APPENNINGERS.

**Crashlog:** `Crash-20260531-015855.log` (StyleEditor), `Crash-20260530-231701.log` (brush, fixato).

---

### UX (bassa priorità) — Camera-view editing difficile da controllare

**Priorità: BASSA | Tipo: UX | Segnalato: 2026-05-30**

Comportamento storico di Toonz: stando in **camera view** e provando a modificare
la camera (Animate tool su colonna camera), le modifiche sono quasi impossibili da
controllare — trascinando si muove/zooma la *vista* invece dell'oggetto camera, e i
maniglioni di trasformazione sono ambigui rispetto al frame della camera stessa.

Non è un crash, è un limite di design del tool Animate quando il target è la camera
e si è già nel sistema di riferimento camera.

**Possibili approcci (da valutare):**
- Modalità/toggle "Edit Camera" che disabilita il pan-vista mentre si trascina la
  camera, o inverte la mappatura (drag = muovi camera, non vista).
- Maniglioni dedicati per la camera con feedback chiaro (frame + handle distinti).
- Eventuale gizmo camera custom (come per le annotazioni task 40).

**File (presumibili):** `sceneviewer.cpp` (gestione drag in camera mode),
`edittool.cpp` / `tool.cpp` (Animate tool su camera column).

---

### BUG (da investigare) — Stack overflow ricorsione layout QScrollArea (Windows)

**Priorità: MEDIA | Tipo: BUG | Segnalato: crash Windows 2026-05-28 (build 0.3.4)**

Crash `EXCEPTION_STACK_OVERFLOW` su Windows 10. Backtrace = ricorsione infinita
`QScrollArea::eventFilter → QWidget::resize → setGeometry_sys → QScrollArea::eventFilter`
ripetuta ~480+ volte. In cima allo stack: `QLabel::setWordWrap / sizeHint /
QTextEngine::itemize` → un **QLabel word-wrap dentro una QScrollArea** il cui
height-for-width oscilla (la larghezza dipende dalla scroll area che si ridimensiona
in base al sizeHint del label → loop).

**Contesto:** l'utente aveva installato 0.3.4 SOPRA un'installazione precedente
(file stale nella stessa cartella + config/layout `rooms` per-utente in AppData).
Possibile trigger: layout salvato incompatibile. **Workaround utente: clean install.**

**Da fare:** cercare nei pannelli Ztory una `QScrollArea` con dentro un `QLabel`
word-wrap senza larghezza fissata (candidati: StoryboardPanel grid, Script panel,
PanelWidget con label). Fix tipico: `label->setMinimumWidth()` o
`setWordWrap` + size policy fissa, oppure rompere il feedback con
`QScrollArea::setWidgetResizable` / gestione esplicita del resize.
Non riproducibile finora su Mac (probabilmente layout-file-specifico).

**Crashlog:** `SamDrive/Ztoryc/crash_windows/Crash-20260528-191004.log`

---
---

### NEW — Room TRADITIONAL (task 38)

**Priorità: MEDIA | Tipo: NEW**

Workflow tradizionale: Import scansioni → Cleanup → Ink & Paint.
Note: GTS (scanner TWAIN) è solo Windows e separato da Tahoma2D; su macOS la
scansione è esterna. Room dedicata con Cleanup + Xsheet + Viewer + Style Editor.
Pulsante "Import Shot Frames" nel BOARD per caricare immagini scannerizzate
nella sub-scene corretta.

**File:** `mainwindow.cpp` (nuova room), `storyboardpanel.h/.cpp` (pulsante import).

---

---

## Aperti al 2026-08-02 (travasati dalla lista di sessione)

> ⭐ **QUESTA E' L'UNICA LISTA DI STATO.** Unificata il 2026-08-03: le stesse voci
> ZtoRig vivevano anche dentro Priority Order e le due copie avevano gia' cominciato a
> divergere (una diceva aperto cio' che l'altra dava per chiuso). Li' sono rimaste solo
> **le prove e le misure**, che e' a cosa serve quella sezione; qui c'e' cosa e' aperto.
> Chi chiude una voce la chiude **qui**.
>
> Diagnosi raccolte nelle sessioni del 27/7 e del 2/8. Scritte qui perche' la
> lista di lavoro vive nella sessione e sparisce con la chat.

**ZtoRig / IK**

- ⬜ **Il personaggio «parte» manipolando le anche — solo su animazioni VECCHIE.**
  Su chiavi fresche non succede (Franco, 2/8). Sposta il sospetto dal solver al
  DATO GIA' IN SCENA: bersagli dei pin catturati con regole diverse, o chiavi
  scritte quando il write-back aveva un'altra semantica. **Primo test**: stessa
  scena vecchia su 0.11.0 contro branch — se il sintomo c'e' su entrambe, la
  ri-cattura dei pin (`fbafaeee5`) e' innocente. Misurato in precedenza su scene
  nuove: non e' la bisezione (`accepted` medio 0.995) ne' i limiti d'angolo;
  l'amplificazione sta fra bersaglio e posa risolta, firma di piu' bacini di
  convergenza dentro `solveMultiAnchor`.
- ⬜ **Pin legati allo scheletro.** I parametri PIN sono condivisi per nome fra
  gli scheletri della colonna. Serve un campo esplicito in `SkVD` (come
  `m_skelIds` per le pose), con «pin vecchi = ovunque». Tentativo di dedurre lo
  scheletro dal frame di attivazione: ritirato, sbagliato.
- ⬜ **Il personaggio scivola registrando pose con i PIN.** Da indagare, zona
  dell'autorita' del planting. Diagnostica: `ZTORYC_PIN_DIAG`.
- ⬜ **Residuo multi-pin ~1.5%.** La bisezione del drag giudica con
  `solveMultiAnchor`, ma l'ultima parola ce l'ha il solve di personaggio in
  `TStageObject`. Esporre anche quello come query, come `plantPins()` /
  `pinResidualForPose()` per la singola colonna.
- ✅ **Chiudere un loop di camminata — RISOLTO il 2026-08-14, senza scrivere
  codice.** Franco: «ha funzionato con **Part**». La prova che era annotata come
  «da fare prima» ha chiuso la voce da sola: la modalita' `Part` richiama **solo
  i parametri registrati**, quindi copiando una posa non si porta dietro il
  PIAZZAMENTO e il personaggio non torna indietro.
  **Quindi il comando nuovo NON serve.** Il difetto di fondo resta vero e va
  conosciuto — `SkVD::POSE_PARAMS` (`plasticskeletondeformation.cpp:111`)
  mescola FORMA (ANGLE, DISTANCE, SO) e PIAZZAMENTO (ROOTX/Y, TRANS, ROT, SCALE,
  PIVOT, SHEAR), e con i pin il piazzamento non andrebbe copiato affatto perche'
  il pin porta la posizione e gli angoli la forma — ma in pratica `Part` lo
  aggira. Se un giorno servisse copiare pose **senza** passare da Part, il
  rimedio e' un comando che copi solo ANGLE/DISTANCE/SO fra due frame.
  ⚠️ **Da mettere nel manuale**: «per chiudere un ciclo, richiama la posa in
  modalita' Part» e' conoscenza d'uso, non di codice, e senza scriverla si
  riperde.
- ⬜ **Template di scheletri riusabili + registrazione di animazioni.**
  ✅ **CONFERMATO da Franco il 2026-08-03** — era ricostruito a memoria, ora e' una voce
  vera. Restano aperte le domande di progetto: cosa contiene il template (topologia?
  limiti d'angolo? rigidity? SO?); le correttive **NON sono trasferibili** (sono delta
  per indice di vertice della MAGLIA, che cambia da disegno a disegno); riapplicazione
  **per nome** (unica chiave stabile fra scheletri diversi) o per indice; dove vivono i
  template (accanto alla scena? nella libreria di progetto?). Le azioni di posa ZtoRig
  sono il precedente piu' vicino e vanno guardate per prime.
- 💡 **Colore della linea come chiave di corrispondenza** (idea di Franco,
  2026-08-03, da valutare). Se l'animatore disegna la linea del naso con uno
  STILE dedicato, quella linea si riconosce in tutte le viste senza pickerare
  niente e senza indovinare dalla geometria. Il suo esempio e' preciso: la linea
  del naso a volte cade a sinistra e a volte a destra, e nessun criterio
  posizionale la segue.

  **Perche' e' forte**: lo stile e' **gia' nel formato file** — ogni `TStroke`
  porta il suo `getStyle()` e il PLI lo salva, a differenza di `getId()` che e'
  un contatore a runtime (`++maxStrokeId`) e si riassegna al caricamento. Quindi
  non chiede niente al formato, che era il vincolo peggiore. Ed e' un gesto che
  gli animatori gia' fanno: le palette di lavorazione esistono.

  **Limiti da guardare in faccia**: uno stile identifica una CLASSE, non
  un'istanza — con due occhi dello stesso colore non si distingue destro da
  sinistro (o due stili, o una regola sul lato). Ed e' opt-in: vale solo se
  qualcuno ha colorato apposta. Come **ripiego** invece che come obbligo e'
  perfetto: dove il colore c'e' lo si usa, dove non c'e' si ricade sull'indice
  e sul picker.

- ⬜ **Correttive di giuntura, milestone 3 (UI).** La milestone 2 (authoring, il
  pennello) e' entrata su master il 2026-08-03.
  **2026-08-14**: su branch `feature/ztorig-correttive-ui` (`785b0860d`) c'e' il
  pannello ZtoRig a schede + la scheda Correttive come **traccia in gradi** (una
  corsia per giunto, una chiave per correttiva, clic per andare a quella piega,
  trascinamento per spostarla, tasto destro per cancellare). **Da collaudare.**
  L'idea della traccia e' di Franco, ed e' come funzionano gli Smart Bones di
  Moho. Il dato non e' cambiato per ottenerla: le correttive nascono gia'
  incatenate, quindi erano gia' chiavi su una traccia scritte come tabella.
  Resta fuori: creare una correttiva NUOVA dal modo di rigging, e la
  separazione di Sculpt/Order fuori da Animate (deciso con Franco: modellare e
  riggare non e' animare).

- ⏸️ **Disco rigido di articolazione — PARCHEGGIATO il 2026-08-14.**
  Branch `feature/ztorig-joint-disc` (`50eb4cd95`), preferenza spenta.
  Il gomito pizzica perche' un giunto e' **UN** punto di comando e l'ARAP deve
  far coesistere li' due rotazioni. Il disco (corona di punti sintetici, raggio
  = meta' larghezza dell'arto, rotazione sulla bisettrice) toglie il
  pizzicamento, si vede nel tool e si tara.
  ⚠️ **Ma non puo' dare il bersaglio**: piegando, all'interno i due segmenti si
  SOVRAPPONGONO, e una maglia unica non puo' sovrapporsi a se stessa — puo' solo
  accartocciarsi o aprire un buco. Provato a Joint Blend 0 e 100: cambia solo
  quale difetto prevale.
  **La strada giusta e' il TAGLIO automatico**, che era la prima idea di Franco:
  dove il disco incontra l'arto la maglia si sdoppia in due meta' con calotta
  circolare, i vertici della calotta restano condivisi (quindi non si separano
  mai) e le due meta' si sovrappongono. Tocca la topologia: sessione a se'.
  Dettagli e i sei errori da non ripetere: memoria `project_ztorig_joint_disc`.

**Altro**

- ⬜ **Save and Render fa partire DUE render** — candidato upstream. ESCLUSI: la
  tavoletta (succede anche col mouse) e l'handler (`onSaveAndRender` fa un solo
  `doRender`). **Biforcazione da risolvere**: il comando parte due volte, oppure
  una esecuzione produce due render? Un contatore all'ingresso di
  `RenderCommand::onSaveAndRender` e uno in `doRender` lo dicono in un clic.
  A parte: i bottoni di `outputsettingspopup.cpp` usano `pressed()` invece di
  `clicked()` — sbagliato comunque, non e' la causa qui.
- ⬜ **CRASH 0.11.0 chiudendo la finestra di cattura — NON riproducibile.**
  Log `Crash-20260727-222814.log`. Backtrace (simbolicazione approssimata, i
  frame 1 e 2 sono identici): `onSelectionChanged` → `storeDeformation` →
  `onColumnSwitched` → `onXsheetChanged` → `saveSceneIfNeeded` → `closeEvent`.
  GIA' ESCLUSI leggendo, tutti guardati: `storeDeformation`, `onSelectionChanged`,
  `rootVd_animate`, `skeletonId()`. **Discrepanza da tirare**: il backtrace dice
  `MainWindow::closeEvent` ma Franco aveva chiuso solo la finestra di cattura.
- ⬜ **Ringraziamento sponsor DENTRO l'app** — generico, senza nomi (deciso da
  Franco 2/8; vuole prima vedere come lo fa Tahoma2D). Se un giorno si passa ai
  nomi, il consenso esplicito va chiesto: essere sponsor pubblico su GitHub non
  e' consenso a comparire nell'About.
- ⬜ **Script di shake camera** con lo scripting di Toonz — chiesto e mai fatto.

---

## Priority Order

### 🛑 SOSPESI PER DECISIONE DI FRANCO — non riproporli

> Leggere PRIMA di proporre qualsiasi cosa. Sono voci ancora aperte piu' in
> basso, ma Franco ha deciso di lasciarle stare: una sessione che le rilancia
> gli fa perdere tempo. Si riaprono solo se **lui** le riapre, o se il sintomo
> ricapita da solo lavorando.

- **I crash e i problemi sulle SCENE VECCHIE** (2026-08-14): *«per quanto
  riguarda i crash e i problemi con scene vecchie lascerei stare, vediamo se
  ricapita lavorandoci»*. Coperti da questa decisione:
  - crash su «Salva sotto-scena come scena», mesh non trovate
  - il pin va sul vertice sbagliato su scene precedenti all'IK
  - il personaggio «parte» manipolando le anche — solo su animazioni vecchie
- **L'IK resta com'e'** (2026-08-14): dopo la prova col rilevatore di
  ribaltamento, Franco: *«mi pare piu' stabile e controllabile di quel che
  ricordavo, forse possiamo lasciare l'ik com'era»*. Branch
  `feature/ik-pole-vector` non mergiato. Non riproporre annealing ne' pole
  vector senza un sintomo nuovo.
- **Otter (il secondo fork)** — ⚠️ **NON PIU' SOSPESO, ma nemmeno da fare
  adesso** (Franco, 2026-08-16): *«su Otter sono gia' praticamente convinto e
  dobbiamo lavorare COME SE fosse gia' cosi', pero' non ho urgenza di metterlo
  in atto subito»*. Sostituisce il «ci sto ancora ragionando» del 2026-08-13.
  **Cosa vuol dire in pratica**: le scelte di progetto vanno prese tenendo
  separati storyboard/animatic (Ztoryc) e character animation (ZtoRig,
  deformatori, fisica, auto-shadow → Otter), senza pero' spendere tempo nella
  separazione vera. Non proporre il fork come lavoro; non incrociare i due
  mondi in modo che poi separarli costi. Vedi `COMPETITIVE_ROADMAP.md` sez. 8.
- **Assistenti al disegno da OpenToonz** (2026-08-13): candidato misurato e
  registrato in `OPENTOONZ_PORT_CANDIDATES.md`, ma Franco ha scelto di passare
  prima al rig. Non e' il prossimo lavoro.
- **Render sbagliato sh110**: sospeso dal 2026-08-07. ⚠️ **Novita' del
  2026-08-14**: ora e' **riproducibile a comando** (tcomposer headless, frame
  110, MD5 stabile) — ma resta sospeso finche' Franco non lo riapre. Vedi la
  voce dedicata piu' sotto.

- **ZtoRig — FERMATO IN PAUSA** (2026-08-14, dopo il collaudo della traccia in
  gradi): *«le correttive impostate cosi' vanno bene [...] riguardo la parte
  ZtoRig mi fermerei un attimo visto che e' piuttosto laboriosa, ma gia' cosi'
  abbiamo degli strumenti utilissimi»*. **Non e' abbandonato, e' in pausa**: si
  riprende quando lo dice lui. Due cose gia' decise, da fare quando si riapre:
  - **evidenziare meglio il diamante della posa che si sta editando** (piccola,
    UI). Vedi la grammatica del diamante in `ztorykeydiamond.h`.
  - **modalita' rig vs modalita' animazione**: in modalita' rig queste
    operazioni **non devono generare chiavi di animazione** — sono modifiche al
    rig del personaggio, come la modalita' «build skeleton». Riferimento
    esplicito di Franco: anche Harmony ha una modalita' Animate e una in cui non
    anima davvero. Stessa direzione gia' registrata il 2026-08-14b (Sculpt e
    Order fuori da Animate).
  - Resta valido, per quando si riapre: il **taglio automatico sulla giuntura**,
    la strada emersa dal disco parcheggiato (memoria
    `project_ztorig_joint_disc`).

## 🔝 ORDINE DI PRIORITA' — rifatto da Franco il 2026-08-16

> *«La mia priorità è chiudere il discorso character, mi serve poter usare il
> nuovo lipsync, e vediamo se si riesce a creare un template di scheletro per
> riutilizzare le animazioni. Il resto a questo punto è secondario mi pare.»*

**A. CHIUDERE IL PERSONAGGIO** — e' questo il lavoro, il resto viene dopo.
   1. **MouthSet**, perche' e' cio' che rende il lipsync USABILE: la catena
      fonemi→colonne e' finita e collaudata, ma senza l'associazione
      bocche↔fonemi salvata sul personaggio resta un elenco di sigle. Progetto
      pronto nella sezione «IL PERSONAGGIO COME OGGETTO»: sidecar `.ztoryc` con
      `role="character"`, riferimento per NOME DI LIVELLO + fotogramma.
   2. **Libreria di pose + ritorno in libreria** — «pubblica in libreria» /
      «prendi dalla libreria», con rifiuto se incompatibile e v2 se la struttura
      diverge (decisione di Franco, vedi sezione «IL RITORNO IN LIBRERIA»).
   3. **Template di scheletro con proporzioni standard, e compensazione**
      (idea di Franco, 2026-08-16): *«creare un template di scheletro con delle
      proporzioni standard; quando viene riadattato sul personaggio, facendo la
      differenza dall'originale si dovrebbe riuscire a compensare le modifiche
      in modo che le animazioni funzionino comunque»*. E' il **retargeting**, ed
      e' la strada giusta.

      💡 **E' economico, e si vede dalla struttura dei dati.** I delta di una
      posa sono salvati per NOME DI VERTICE (`plasticskeletondeformation.h:289`)
      e in **coordinate polari rispetto al padre** (`:139`), non in posizioni
      assolute. Quindi si spezzano in tre nature con destini opposti:
      - **ANGLE** — un angolo. 30° sono 30° su qualunque personaggio:
        **si trasferisce invariato**, nessuna compensazione.
      - **DISTANCE** — una lunghezza. E' **l'UNICO** che rompe cambiando
        proporzioni, ed e' esattamente il limite dichiarato alla riga 330.
      - **SO, PIN, PINTX/PINTY** — passano invariati.

      La compensazione non e' quindi un motore di retargeting, e' **una
      moltiplicazione su un parametro solo**:
      ```
      distanza_bersaglio = distanza_template × (riposo_bersaglio ÷ riposo_template)
      ```

      **Tre verifiche prima di progettare in grande**, in ordine di rischio:
      1. i **nomi dei vertici** devono coincidere — il template e' prima di
         tutto un ELENCO DI NOMI;
      2. le **lunghezze a riposo** devono essere leggibili da entrambi gli
         scheletri: cercando `restLength` non ho trovato un accessore pronto, va
         verificato nel codice vero (potrebbe esserci con altro nome, o si
         ricava dalle posizioni a riposo);
      3. il rapporto va preso **PER OSSO, non globale**: un personaggio con
         gambe lunghe e braccia corte con un fattore unico verrebbe peggio che
         non compensando affatto.

      ⚠️ **Limite onesto**: questo non salva una posa in cui la mano TOCCA il
      fianco. Cambiando proporzioni il contatto si perde comunque, perche' il
      vincolo era geometrico e non angolare. La scala completa sarebbe: angoli
      gratis → distanze in proporzione → **contatti tenuti dai PIN** (che
      esistono gia': `PIN/PINTX/PINTY`, «l'ancoraggio resta piantato anche sugli
      intermedi»). I pin pero' sono la parte collaudata e messa in pausa: e' il
      terzo pezzo naturale, non un lavoro da aprire adesso.

**B. SECONDARIO da qui in poi** (era il Priority Order precedente):
   - **Deformatori** (punto 3 sotto). Stimato il 2026-08-16 in **2-3 sessioni**
     per il primo (Liquify, che serve a misurare l'impalcatura) e 1-2 per
     ciascuno degli altri tre. ⚠️ Scoperta del 2026-08-16: il Plastic e' gia'
     `bind(TTool::AllImages)`, quindi un deformatore ANIMABILE su tutti i tipi
     di livello **esiste gia'** — il buco e' il ritocco DIRETTO, senza mesh ne'
     rig. Nessuna sovrapposizione, l'inquadramento di Claudio era giusto.
   - **Assistenti al disegno da OpenToonz** — Franco li ha ricordati il
     2026-08-16 come ancora in sospeso. Restano sotto al blocco A.
   - 2.5D Cartoon Models (ricerca), auto-shadow (per ultimo, sua indicazione).

---

**Cosa e' invece VIVO dal 2026-08-14 (ordine dato da Franco)**:
1. ~~**Kitsu — legare il Production Tracker al singolo EPISODIO**~~ ✅ **FATTO
   il 2026-08-14** (`3775c1144`). Legame = coppia (progetto, episodio) per ID;
   filtro su shot e asset; team NON filtrato di proposito. Due bug chiusi per
   strada: 156 task su 591 scartati in silenzio, e i nomi del team invisibili
   *perche'* Franco e' admin (Zou non serializza `full_name` per gli admin).
   ⚠️ **Resta da verificare**: il conteggio shot per episodio e' stato provato
   con **un solo shot**. Gli asset entrati per errore invece sono **chiusi**:
   rimossi a mano da Franco, e il filtro impedisce che ne arrivino altri —
   **non proporre un ripulitore automatico.**
2. ✅ **LIPSYNC — L'ALLINEATORE FORZATO E' FATTO (2026-08-16).**
   **Vosk** ha sostituito Whisper nel ruolo di cronometro. Whisper resta per le
   lingue senza modello e per il rilevamento automatico. Misurato sull'audio
   vero di sh020 con riscontro sulle **fricative dello spettro** (la /s/ di
   «que*s*to», la /f/ di «fa»: eventi fisici, non l'opinione di un altro
   modello):

   | | scarto medio | peggiore |
   |---|---|---|
   | **Vosk small it (48 MB)** | **10 ms — 0,2 fotogrammi** | 30 ms |
   | Whisper base-q5_1 | 30 ms — 0,8 | 110 ms |
   | Whisper + DTW | 191 ms — 4,8 | 400 ms |

   Su 14 battute: Whisper 86 parole a durata ZERO su 263 e un'allucinazione
   (108 parole, coda a 22 s oltre la fine); Vosk zero degenerazioni su 158.

   ⚠️ **Correzione a quanto scritto il 2026-08-15**: `-dtw` non «non ha avuto
   effetto» per la quantizzazione — **non era mai stato eseguito**, perche' il
   flash attention e' attivo di default e lo disabilita stampando una riga nel
   log. Acceso davvero (`-nfa`), PEGGIORA.

   **La catena completa, com'e' adesso**:
   `Vosk` dice QUANDO cade la parola (10 ms) → `espeak-ng --ipa` dice DI COSA e'
   fatta (processo separato, GPL, mai linkato) → l'**ONDA** dice dove cade il
   suono dentro la parola.

   Fatto, in ordine di quanto e' costato scoprirlo:
   - **Il centro delle vocali si aggancia ai massimi di energia** (idea di
     Franco). Non pesare i fonemi con l'energia — provato e MISURATO inutile,
     perche' campionava l'onda nella posizione indovinata, ed e' la posizione a
     essere sbagliata. Vocali sul picco: da 4/13 a **10/13**.
   - **L'accento di espeak e' durata, non punteggiatura.** `fˈatʃile`: la ˈ
     sulla /a/. Lo scartavo coi separatori. Ora moltiplica il peso (×2,5
     primario, ×1,5 secondario).
   - **L'avanzo va un fotogramma alla volta a chi e' piu' sotto il proprio
     peso**, non per arrotondamento: con 6 fonemi e 3 fotogrammi d'avanzo
     l'arrotondamento ne dava al massimo uno a testa e la vocale accentata
     restava al minimo come le altre.
   - **Durata minima 2 fotogrammi imposta sulla COLONNA**, non parola per
     parola, con prelievo **a cascata** (il vicino immediato spesso e' gia' al
     minimo mentre c'e' spazio cinque celle piu' in la').
   - **Mai fondere due bocche uguali e visibili**: togliere la /e/ fra la P di
     «per» e la M di «me» sembra innocuo e fa leggere UNA tenuta dove devono
     esserci due colpi.
   - **Anticipo** 2 fotogrammi regolabile (Preferenze > Import/Export > Lip
     Sync) **+1 sulle labiali**, solo sull'attacco.
   - **Rest esplicito** nelle pause ≥ 2 fotogrammi; buchi piu' corti assorbiti
     dalla cella precedente (una cella vuota TIENE il disegno prima, non chiude
     la bocca).
   - **Due colonne** per personaggio: prima le parole, poi le bocche.

   🧪 **Il banco di prova sta in `reference/forced-align/`** e la verita' di
   riscontro sono le **fricative dello spettro**, non un altro modello:
   `check_align.py`, `sibilance.py`, `robustness.py`, `visemes.py`.

   **Resta da fare**: spostare la generazione delle colonne all'EXPORT, e
   imballare espeak-ng (oggi viene da Homebrew, come whisper-cli — se manca, le
   colonne tornano a contenere le parole intere invece di fallire).

   💡 **POSSIBILE SVILUPPO FUTURO — codici dei fonemi personalizzabili**
   (Franco, 2026-08-16: *«i fonemi potremmo anche farli customizzabili, molto
   spesso le produzioni hanno delle tavole con dei codici»* — poi: *«per adesso
   lasciamo cosi', segniamocela come possibile implementazione futura»*).
   Quando si riapre: i codici sono della **PRODUZIONE**, quindi vanno nel
   progetto come gli alias dei personaggi, non nelle preferenze
   dell'applicazione. Resta da chiarire se basta rinominare le dieci caselle o
   se le tavole vere hanno un numero diverso di bocche — Franco si era offerto
   di mostrarne una.

   Recap originale del 2026-08-14 piu' sotto.
   ✅ **WHISPER E' DECISO**, non e' piu' un'opzione da valutare. Franco: *«whisper
   lo voglio assolutamente visto che sara' utile anche per l'inglese, credo fara'
   la differenza in ogni situazione»*. Ha ragione anche sull'inglese: PocketSphinx
   e' tecnologia dei primi anni 2000, Whisper e' migliore in assoluto, non solo
   sulle lingue che l'altro non copre.

   Vincoli gia' accertati, da non riscoprire:
   - **Usare `whisper.cpp`** (C/C++, licenza **MIT**, nessuna dipendenza Python,
     Metal su Apple Silicon). La versione originale in Python non e'
     distribuibile dentro il bundle.
   - ⚠️ **espeak-ng e' GPL-3.0.** Serve per testo→fonemi, ma NON va linkato:
     va invocato come **processo separato**, esattamente come gia' si fa con
     Rhubarb e ffmpeg. Linkarlo contaminerebbe la BSD di Ztoryc — stessa
     trappola gia' incontrata con Krita e AnimeEffects.
   - I **timestamp per parola** non sono nativi in Whisper: si ricavano
     allineando i pesi di attenzione, ed e' la parte meno solida della catena.
   - Whisper **inventa testo** su silenzio e rumore. In un tool di lipsync, dove
     le pause contano, va gestito esplicitamente.
   - ✅ **DECISO (Franco, 2026-08-14): il modello NON va nel bundle.** *«Se
     pesano tanto dobbiamo prevedere che sia una scelta dell'utente scaricarlo o
     meno e in che versione»*. Quindi serve una UI di gestione modelli: scegli
     quale, scarichi, vedi quanto pesa, lo puoi togliere. Ztoryc deve funzionare
     senza nessun modello scaricato (lipsync col solo Rhubarb, come oggi).
   - ⭐ **Osservazione che puo' ridurre di molto il modello necessario**: se il
     testo lo diamo noi (`PanelData::dialog`), il lavoro non e' piu'
     *riconoscimento* ma **allineamento forzato** — sappiamo gia' cosa e' detto,
     serve solo sapere quando. E' un compito molto piu' facile, su cui un modello
     piccolo se la cava. Il modello grande serve solo quando il copione NON c'e'.
     Da misurare prima di imporre a tutti un download da qualche GB.
   - **WhisperX — l'idea si prende, il codice no.** Franco l'ha segnalato il
     2026-08-14 («esiste una versione whisper X che supporta i fonemi»). Non e'
     un modello diverso: e' una *pipeline* attorno a Whisper (gruppo di Oxford)
     con VAD prima (taglia il silenzio -> meno allucinazioni + piu' veloce),
     **allineamento forzato con un modello CTC** per lingua, e diarizzazione
     opzionale.
     ⚠️ Due precisazioni che cambiano le conclusioni:
     - il modello di allineamento serve a **datare**, non a produrre fonemi da
       mappare sui viseme: l'uscita sono parole/caratteri con tempi precisi. Il
       pezzo testo->fonemi resta comunque da fare.
     - **e' Python e tira dentro PyTorch**: svariati GB, impraticabile in un
       bundle .app firmato. NON e' la versione leggera — quella e' whisper.cpp,
       che e' un'altra cosa.
     **Cosa prendere**: l'idea che i timestamp nativi di Whisper sono la parte
     fragile e un allineatore forzato li batte. In C++: o l'opzione **DTW per
     token gia' presente in whisper.cpp**, o un allineatore CTC invocato come
     processo separato (schema Rhubarb/ffmpeg).
     ❓ **Da verificare prima di adottarlo**: licenza di WhisperX e dei modelli
     di allineamento (quelli di diarizzazione stanno dietro condizioni d'uso su
     HuggingFace). In questo progetto la licenza e' stata la sorpresa finale
     troppe volte — Krita, AnimeEffects, espeak-ng.
   - 👥 **I PERSONAGGI — chi parla, e con quale bocca** (Franco, 2026-08-14).
     Il problema vero non e' il lipsync di una battuta: e' la catena
     **chi parla -> quale audio -> quale testo -> quale livello animare**.

     **Due anelli su tre ci sono gia', senza indovinare niente:**
     - *Chi parla*: una **colonna audio per personaggio**. E' come lavora il
       montaggio del suono, e Ztoryc le colonne sonore le scorre gia'. Se
       l'audio arriva separato, chi parla e' un DATO, non una deduzione.
     - *Chi esiste*: i personaggi sono **gia' asset di tipo Character** (63 nel
       progetto di Franco) e da oggi si sincronizzano da Kitsu. La lista non va
       inventata.

     **L'anello mancante**: `PanelData` ha `dialog`, `action`, `notes` ma **non
     ha un campo personaggio**. Il testo c'e' ed e' anonimo. Aggiungere un
     riferimento all'asset Character e' la modifica che chiude la catena.

     **La diarizzazione e' un ripiego**, non il progetto: serve solo con traccia
     unica mixata e nessuna annotazione, e comunque restituisce SPEAKER_00 /
     SPEAKER_01 — qualcuno deve poi dire chi sono.

   - 👄 **MOUTH SET — assegnare i disegni ai fonemi UNA VOLTA SOLA** (idea di
     Franco, 2026-08-14, ed e' il pezzo di maggior valore pratico).
     *«I fonemi vanno associati a due set diversi di bocche (bocca in su o in
     giu') per ogni posizione. Potremmo fare in modo che tutto questo venga
     registrato e associato al personaggio, cosi' e' un'operazione veloce invece
     di stare ogni volta a riassegnare i disegni ai fonemi: lo si fa una volta e
     basta e poi si sceglie quale set usare (frontale o profilo, felice o
     triste).»*

     Struttura dati che ne segue:
     - un **MouthSet** = mappa viseme (A-H, Preston Blair — gia' quelli che
       usiamo, `--datUsePrestonBlair`) -> disegno specifico (livello + frame);
     - il MouthSet ha i suoi attributi: **vista** (frontale / profilo / 3-4),
       **espressione** (felice / triste), **variante** (bocca in su / in giu');
     - un **personaggio possiede PIU' MouthSet**, e al momento del lipsync si
       sceglie solo quale usare: tutto il resto e' gia' noto.

     **Dove va salvato**: sul PERSONAGGIO, non sullo shot — cosi' viaggia fra
     shot, episodi e produzioni. Il che lo rende **lo stesso problema della
     «libreria di rig riusabili»** (punto 4 delle priorita'): un personaggio
     porta con se' il suo rig, i suoi mouth set, i suoi asset. E' una
     *definizione di personaggio* che esiste una volta sola. Progettarli
     separati sarebbe farlo due volte.

     ⚠️ **Vincolo per il futuro**: oggi il lipsync scambia disegni in un
     livello. Con ZtoRig un personaggio riggato ha la bocca DENTRO il rig,
     quindi prima o poi dovra' scrivere **pose** e non scambi di disegno. Non
     inchiodare il bersaglio al «livello di bocche»: il MouthSet deve poter
     puntare a un disegno OPPURE a una posa.

   - ✅ **FATTO il 2026-08-15 — CHI PARLA, ricavato dal testo.** Era il perno di
     tutta la catena: `PanelData::dialog` era una stringa ANONIMA.

     **Strada scelta da Franco: la convenzione, non un campo strutturato.**
     *«Va bene la B perche' tanto facciamo copia e incolla dallo script, e in
     alcuni formati come l'FDX e il Fountain il character e' riconoscibilissimo
     nel testo.»* Decisiva: la strada strutturata avrebbe toccato **18 punti**
     fra Board, animatic e due serializzatori, e avrebbe fatto compilare due
     campi dove se ne incolla uno solo — perdendo l'unica cosa che rende il
     lipsync automatico, cioe' che il testo c'e' gia'.

     `ZtoryModel::parseDialogue()` riconosce le due forme vere:
     `MARIO: battuta` e la forma sceneggiatura (nome da solo in maiuscolo,
     battuta sotto). Toglie le estensioni `(V.O.)` `(O.S.)`, scarta le
     didascalie fra parentesi, ricompone le battute su piu' righe.

     **Il nome si colora DENTRO il campo** (idea di Franco: *«non potrebbe
     bastare evidenziare in verde il nome?»* — si', ed e' meglio: il riscontro
     va dove sta la causa). Verde = personaggio del progetto, arancione = no.
     Resta una riga di avviso SOLO per i non riconosciuti, che in un campo lungo
     e scrollato resterebbero fuori vista. Nel Board **e** nello Shot Board.

     **ALIAS**: si seleziona un nome e lo si forza su un personaggio (tasto
     destro). Serve davvero — negli script i nomi non coincidono mai del tutto
     con quelli del tracker («PRINCIPESSA» nel copione, «PRINCENERENTOLA» fra
     gli asset) e le alternative erano correggere il copione o rinominare
     l'asset. L'alias e' di PROGETTO, non di pannello.

     ⚠️ **La regola sta in UN posto solo** (`ZtoryModel::speakerAt()`), usata sia
     dal parser sia dall'evidenziatore. Due copie divergono al primo caso
     limite, e in questa feature e' gia' successo.

     🧪 **13 casi di test** sul codice VERO (estratto testualmente, non
     riscritto): `scratchpad/test_parser.cpp`. Ne hanno presi due, e valgono
     piu' del codice:
     1. la prima versione rifiutava un nome sconosciuto, e cosi' «GIOVANNI»
        finiva inghiottito nella battuta — la funzione che deve SEGNALARE i
        personaggi mancanti non poteva vederne nemmeno uno;
     2. correggendo, avevo escluso le didascalie dal «seguito da»: ma
        `MARIO / (sottovoce) / Non ci credo` e' normalissimo e la parentesi
        CONFERMA l'intestazione. Facevo sparire Mario. A negarla e' la riga
        VUOTA, non la parentesi.

     📌 Nel progetto di Franco **51 personaggi su 63 hanno nomi veri e gia' in
     maiuscolo** (BRONTOLO, FATINA, LUPO, SOFIA…), quindi la convenzione morde
     da subito. I 12 chiamati «1».."12" non si riconoscono nella forma
     sceneggiatura — il parser pretende almeno una lettera, o una riga di numeri
     diventerebbe un personaggio — ma funzionano con i due punti.

   - 🎯 **DECISO 2026-08-15: serve un ALLINEATORE FORZATO. I tempi di
     whisper.cpp NON bastano — misurato.** Franco: *«dobbiamo arrivare a un
     sistema preciso, deve funzionare subito senza doverci rimettere le mani,
     altrimenti non ha senso»*. Quindi NON si fa la pezza di ridistribuzione:
     si fa la cosa giusta.

     **La misura che chiude la questione** (audio vero di Franco, 3,24 s, una
     battuta di SOFIA). I tempi per parola di `-ml 1 -sow` non sono un
     allineamento: sono una segmentazione approssimata dei token, ed **erano
     ERRATICI fra un modello e l'altro**:

     | parola | base-q5_1 (57 MB) | base intero (141 MB) |
     |---|---|---|
     | facile? | 680–1430 | 1190–2000 |
     | fa | 2350–2500 | **3240–3240** (durata zero) |
     | me! | 2720–2920 | 3240–**4660** (oltre la fine dell'audio!) |

     ⚠️ **Il modello INTERO da' tempi PEGGIORI del quantizzato**: parole a
     durata zero e coda oltre l'audio. Quindi il problema **non si risolve con
     un modello piu' grande**, e non e' un difetto del nostro codice.
     `-dtw base` provato: nessun effetto (il modello quantizzato non ha le teste
     di allineamento).
     Verificato anche che l'inviluppo complessivo e' giusto: il parlato finisce
     a 2886 ms (silencedetect) e Whisper dice 2920. E' la distribuzione DENTRO
     a sbagliare.

     **La strada**: un modello **CTC di allineamento forzato** che riascolta
     l'audio SAPENDO gia' le parole e dice dove cadono — il secondo stadio di
     WhisperX, e la ragione per cui WhisperX esiste. Precisione a decine di
     millisecondi invece che centinaia. A 25 fps, 200 ms sono 5 fotogrammi:
     nello scrub si vedono.
     Da valutare: wav2vec2 CTC per lingua (quelli usati da WhisperX), oppure un
     allineatore separato invocato come processo (schema Rhubarb/ffmpeg).
     ⚠️ Licenza e peso dei modelli di allineamento **da verificare**, come si e'
     fatto per whisper.cpp.

   - 🔇 **IL SILENZIO E' UN DATO — una traccia audio sola basta**
     (ragionato con Franco il 2026-08-15). Sua domanda: *«come fa a capire che a
     un certo punto quel personaggio deve restare muto e parla un altro? Con
     Rhubarb colleghi il livello delle bocche alla colonna dell'audio.»*

     **Non serve dividere l'audio per personaggio.** Separarlo servirebbe a
     DEDURRE chi parla — ma non lo dobbiamo dedurre, **c'e' scritto** nel testo
     del pannello. Dall'allineamento esce (personaggio, parola, inizio, fine),
     quindi per ogni personaggio si sanno DUE cose: dove parla → viseme, e
     **tutto il resto** → Rest. Il silenzio si ricava per complemento.

     ⚠️ **Il Rest va SCRITTO, non lasciato vuoto.** Una cella vuota tiene
     l'ultimo disegno: il personaggio resterebbe con la bocca aperta a meta'
     parola per tutta la battuta dell'altro. La colonna consegnata all'animatore
     e' una **linea temporale completa**: fonemi dove parla, Rest dove tace.

     **E' MEGLIO della separazione audio, non un aggiramento.** Il legame
     Rhubarb livello-bocche ↔ colonna audio presuppone che quell'audio sia di
     quel personaggio: dandogli un mix, Rhubarb fa muovere la bocca su TUTTE le
     battute, anche quelle degli altri — il difetto che Franco aveva gia'
     individuato chiedendo una colonna per personaggio. Con l'attribuzione dal
     testo sappiamo anche chi NON sta parlando, informazione che nemmeno una
     pista pulita per personaggio da' (li' il silenzio non distingue fra pausa e
     battuta altrui).

     **Cosa creare in automatico e cosa no:**
     - ✅ le **colonne di TESTO** (`TXshSoundTextColumn`), una per personaggio
       che parla in quello shot: dati derivati, si rigenerano, e finiscono
       nell'exposure sheet stampato.
     - ❌ **NON** le tracce audio. L'audio vive nel main xsheet ed e' di tutta la
       scena, i personaggi compaiono shot per shot: uno che parla in un pannello
       solo si porterebbe una traccia per l'intero progetto. E una traccia vuota
       «da riempire correttamente» e' un compito assegnato all'utente senza
       dargli niente in cambio.
     - 🔧 il legame **traccia → personaggio** resta come **RIFINITURA**: quando
       l'audio arriva gia' separato (doppiaggio, una pista per attore) e'
       guadagno netto — Whisper sente una voce sola. Ma e' un di piu' quando
       c'e', non un requisito da soddisfare.

     **Due limiti da tenere presenti:**
     - il testo dev'essere **ragionevolmente completo**: qui Whisper fa
       allineamento forzato, e una BATTUTA INTERA mancante puo' far slittare
       tutto il seguito (un refuso invece non fa danno);
     - il **parlato sovrapposto** e' l'unico caso in cui la traccia separata
       vince davvero — da trattare come eccezione, non come regola che detta
       l'architettura.

   - Primo passo comunque indipendente da Whisper: collegare `PanelData::dialog`
     all'argomento `-d` di Rhubarb. Il copione ce l'abbiamo gia' scritto, e
     nessun riconoscitore batte il testo vero.

---

### 🧍 IL PERSONAGGIO COME OGGETTO — com'e' fatto DAVVERO (rilevato 2026-08-16)

> Franco ha fermato la progettazione del MouthSet: *«prima definiamo il
> PERSONAGGIO»*, perche' e' lo stesso problema della libreria di rig riusabili e
> farlo due volte non ha senso. Poi ha spiegato com'e' fatto oggi e ha indicato
> gli esempi veri. Questa sezione e' OSSERVATA sul suo progetto
> `2604_grottazzolina`, non immaginata.

**Com'e' fatto un personaggio oggi (parole sue):** una **scena** che contiene il
personaggio riggato; la mesh applicata a una **sotto-scena** con le varie parti;
le **bocche di solito sono un livello a parte dentro la sotto-scena della
testa** (che a volte sta insieme al corpo). Quando serve, si **importa come
sotto-scena**.

**Verificato nel progetto reale** (`scenes/LIB_*.tnz`, sette personaggi):
- un personaggio = `scenes/LIB_NOME.tnz` + un PSD in `extras/LIB_NOME/`;
- il PSD e' caricato **a gruppi**: ogni gruppo diventa un livello chiamato
  `CH_nome#@N#group`. ⚠️ **I nomi sono NUMERATI, non descrittivi**: non esiste
  convenzione che permetta di indovinare quale livello sia la bocca. Va indicato
  a mano una volta, e ricordato. E' esattamente il sidecar che dice Franco.
- accanto al PSD stanno i `.mesh` del rig (`sub.0007.mesh`…).

⚠️ **IL PSD E' UN CASO, NON LA REGOLA** (correzione di Franco, 2026-08-16):
*«il sidecar e' legato al livello della scena, non al psd, potrebbe essere anche
un personaggio disegnato direttamente in ztoryc, vettoriale, smart raster o
raster che sia»*. Quindi il MouthSet punta a **un LIVELLO della scena
personaggio** — qualunque tipo, qualunque provenienza — e non va legato ne' al
formato ne' al file d'origine. Il PSD a gruppi e' solo il modo in cui e' fatta
questa produzione.

🎯 **IL FATTO CHE DECIDE L'ARCHITETTURA**: importare il personaggio in uno shot
**COPIA** i suoi file dentro `extras/<shot>/LIB_NOME/` (23 MB di PSD per ogni
shot). Quindi:

```
percorso   +extras/LIB_GIORNALISTA/…   →  +extras/scsh010/LIB_GIORNALISTA/…   CAMBIA
nome       CH_giornalista#@7#group     →  CH_giornalista#@7#group             RESTA
```

**Quindi il MouthSet NON va agganciato al percorso del file, ma al NOME DEL
LIVELLO** (piu' il numero di fotogramma). Il nome sopravvive alla copia, il
percorso no. Un sidecar messo accanto al PSD si romperebbe al primo import — o
costringerebbe a inseguire le copie.

🏠 **DOVE METTERLO: il sidecar `.ztoryc` ESISTE GIA'** accanto alle scene
personaggio (`LIB_GIORNALISTA.ztoryc` c'e' davvero, scritto perche' la scena e'
stata aperta in Ztoryc) e ha gia' un attributo **`role`**. Oggi i ruoli sono due,
`"storyboard"` e `"shot"` (`ztorymodel.cpp:151, 1445`). Un terzo ruolo
**`"character"`** e' il posto naturale per mouth set, pose registrate e
correttive: viaggia col file della scena, e' gia' letto e scritto, e non serve
inventare un formato nuovo.

**Struttura che ne segue** (da implementare, non ancora fatto):
- il MouthSet vive nel sidecar della scena personaggio (`role="character"`),
  con un riferimento incrociato all'**Asset** di progetto via `uuid`;
- una voce = `viseme -> (nome livello, fotogramma)`, dieci voci. **Il nome del
  livello, non il percorso**: e' l'unica cosa che sopravvive alla copia
  nell'import, e non dipende dal tipo di livello;
- un personaggio ne possiede **piu' d'uno**, con attributi vista / espressione /
  variante, e al lip sync si sceglie solo quale;
- ⚠️ deve poter puntare a un disegno **OPPURE a una posa**: con ZtoRig la bocca
  sta dentro il rig, e prima o poi si scrivono pose, non scambi di disegno.
- il personaggio deve conservare anche **pose registrate e correttive** (Franco,
  2026-08-16): il MouthSet e' UNA delle cose che gli appartengono, non un
  oggetto a se'.

**Il pezzo di interfaccia c'e' gia'**: `LipSyncPopup` ha le dieci caselle con
anteprima e frecce per scorrere i disegni del livello. Manca **salvare
quell'assegnazione sul personaggio e ripescarla** — non l'assegnazione in se'.

---

### ♻️ IL RITORNO IN LIBRERIA — le pose create animando (Franco, 2026-08-16)

> *«Creo il personaggio e lo importo come sottoscena per animarlo nei vari shot,
> animandolo però creo nuove pose e animazioni che potrebbero tornarmi utili, la
> sua libreria si arricchisce: come aggiorniamo il file sorgente?»*

🎯 **LA PARTE DIFFICILE E' GIA' RISOLTA, e non l'avevamo notato.** Da
`include/ext/plasticskeletondeformation.h:289`:
> «Deltas are stored **BY VERTEX NAME**, like keyframes are, and never by vertex
> index: that is what lets an action be **copied to a skeleton whose internal
> vertex numbering differs**.»

Cioe' una `PoseAction` e' **portabile per costruzione**: si trapianta da una
copia del personaggio a un'altra. E' esattamente il caso del ritorno in
libreria, ed e' il problema che di solito costa caro. Manca solo il TRASPORTO.

C'e' anche gia' il controllo di compatibilita': `m_skelIds` dice su quali
scheletri una posa e' lecita (riga 330). Una posa registrata sul frontale
replicata sul profilo «lands somewhere nobody authored» — quindi il travaso non
puo' essere cieco.

**Come farlo, quando si riapre:**
- ⚠️ **Esplicito e a senso unico, MAI automatico.** Il personaggio nello shot e'
  una COPIA (l'import copia i file, verificato: 23 MB di PSD per shot), quindi
  e' un fork. Una sincronizzazione automatica propagherebbe anche gli errori e
  le pose sbagliate a tutta la produzione.
- Due comandi simmetrici: **«pubblica in libreria»** dallo shot (scegli quali
  azioni, finiscono nel personaggio) e **«prendi dalla libreria»** nello shot.
  E' il modello dei template di Harmony, ed e' come ragiona un animatore:
  «questa me la salvo».
- Le pose vanno nel sidecar `role="character"` (vedi la sezione sopra), non
  dentro la scena: cosi' si leggono senza aprire il personaggio.
- **I CICLI di animazione sono un'altra cosa**, piu' grossa: non un insieme di
  valori ma curve nel tempo su piu' parametri. Da progettare a parte — una posa
  e' uno stato, un ciclo e' una clip. Non trattarli come lo stesso oggetto.
✅ **DECISO da Franco (2026-08-16) — divergenza strutturale**: *«il salvataggio
sul character sorgente viene RIFIUTATO se non e' compatibile; se e' diverso
strutturalmente potremmo esportarlo come una v2 dello stesso personaggio»*.
Buona regola: trasforma un caso d'errore in un atto deliberato, invece di
lasciare all'utente una fusione a meta'.

Il controllo di compatibilita' ha una definizione CONCRETA, e la si ha gratis
perche' i delta sono per nome di vertice:
- lo scheletro di destinazione contiene **tutti** i nomi citati dalla posa →
  compatibile, si accetta (se ne ha altri in piu' restano fermi: la posa e'
  PART, tocca solo i suoi);
- ne manca anche uno → **rifiuto**, e si propone la **v2 del personaggio**.

**Il flusso, come lo ha descritto Franco:** animi nello shot → comando
«pubblica in libreria» → scegli quali azioni registrate → controllo → finiscono
nella libreria del personaggio sorgente, disponibili da li' in poi in ogni shot.

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

### 📦 EXPORT COMPLETO — gli asset linkati a file veri (idea di Franco, 2026-08-14)

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

### 🔴 APERTO — il controller funziona nel viewer e non nel render

Franco, 2026-08-05: «forse e' il nostro controller (quella specie di animate tool
legato allo skeleton) che se lo uso per riposizionare un elemento funziona nel
viewer ma non nel render». **Primo sospetto da verificare** quando si riprende il
render plastico.
Appiglio misurato: `getSquashControllerAffine` in un caso valeva
`[1, 0, 462.308, 0, 1, -2.17253]`, cioe' una **traslazione di 462 unita'**, non
l'identita'. Il codice lo descrive come «un affine SOPRA il risultato deformato»,
e viene composto in **due punti diversi** nelle due strade: `stagevisitor.cpp`
(viewer) fa `... * worldMeshToMeshAff * ctrl * meshToWorldMeshAff`,
`plasticdeformerfx.cpp` (render) fa `... * meshToWorldMeshAff * worldMeshToMeshAff
* squashCtrl * meshToWorldMeshAff`. **Verifica diretta**: stampare le due matrici
finali sullo stesso frame e confrontarle.

### 🔴 APERTO — crash su «Salva sotto-scena come scena», mesh non trovate

Segnalato il 2026-08-05, **mai indagato** (Franco mi ha fermato mentre cercavo il
log, e poi la giornata e' andata altrove). Salvando una sotto-scena come scena
non trova le mesh, e poi crasha. Per la regola «i crash vengono prima di tutto»
questo viene prima delle feature. Il crash handler scrive in
`QStandardPaths::AppLocalDataLocation + "/crash"`.
Possibile parentela con i percorsi delle mesh: nei log del render convivono due
radici diverse, `+extras/sh090/sub_2.0001.mesh` e
`+scenes/sh110/LIB_ZOMBIE01/extras/...`.

### 🟠 APERTO — doppio render occasionale

Franco: «succede ogni tanto». Parte due volte lo stesso lavoro. Da capire se
succede lanciando dal Task panel o dal menu — e se due processi scrivono lo
stesso file di output, e' un difetto a se'. Non e' la causa degli artefatti di
oggi (troppo ripetibili per una corsa fra processi).

### 🟠 APERTO — uno zombie si smonta in alcuni frame

Dopo aver reimportato la scena e' rimasto **un solo** caso: un personaggio i cui
pezzi appaiono staccati in un momento. Ipotesi di Franco: il **pin**, che li' sta
su piu' livelli. Ora e' isolato a un caso solo, quindi trattabile.

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

### 🎥 VALUTATO E RIMANDATO — AV1 e l'ffmpeg del 2020

Franco ha chiesto (2026-08-04) se dalle specifiche AV1 di aomedia esca qualcosa
di utile. **Dalla pagina no**: sono definizioni di codec (bitstream, binding
ISOBMFF, payload RTP, HDR10+), roba per chi scrive encoder.

L'unica idea sensata sarebbe **AV1 come formato di uscita** per gli animatic
(30-50% piu' leggeri a parita' di qualita', royalty-free). Ma:
**l'ffmpeg che distribuiamo e' del 2020** (`N-99076`, copyright fino al 2020) e
**non ha alcun encoder AV1** — verificato con `ffmpeg -encoders`, l'unica
corrispondenza e' `wmav1`, che e' audio.

Quindi il costo non e' nel nostro codice (una voce in piu' nella lista formati)
ma nel **sostituire il binario ffmpeg in tre bundle**, e quel binario legge e
scrive TUTTI i video, importazione compresa.

**Decisione: non ora.** Se un giorno si aggiorna ffmpeg, la modifica che si
ripaga non e' AV1 ma [[project_video_import_slow]] — l'importazione che estrae
tutti i frame su disco nel thread dell'interfaccia. Quella fa perdere tempo a
ogni import; AV1 farebbe risparmiare megabyte una volta a consegna. Aggiornando
ffmpeg, AV1 arriva quasi gratis nello stesso giro.

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

### 🔴 BUG — sussulto fra due chiavi sui vertici plastici (2026-08-04)

Segnalato da Franco, **ricorrente** («ogni tanto succede»). Fra due chiavi il
valore fa un'escursione che non dovrebbe esserci: visto su
`MaggiolataZombie/sh330` → sotto-scena **lib_armando** → sotto-scena **col1**,
sulle curve dei **vertici dello skeleton plastico** (colonna `Angle`), frames
9-21. A schermo: due chiavi a 0 e in mezzo l'angolo sale a **132** e torna.

**CAUSA (confermata dal rimedio)**: tangenti **non clampate** sulle chiavi dei
vertici. Chiave trovata nel `.tnz`:
```
S  frame 27   valore 55.017   maniglia.x 2.667   maniglia.y +21.69      (VD > Angle)
```
`maniglia.x = 2.667` e' un terzo di 8 frame, cioe' la maniglia standard: la
lunghezza e' giusta, e' la **pendenza** a essere fuori scala — oltre 8 unita'
per frame su un tratto che non deve muoversi.

✅ **Franco conferma che applicando Auto Bezier il problema sparisce**, il che
CONFERMA la diagnosi: il clamp di `KeyframeSetter::setAutoBezier`
(Fritsch-Carlson, tangente limitata a 3x la pendenza del segmento piu' piatto,
piatta sugli estremi locali) e' esattamente cio' che manca.

**Dove si scrive quella chiave** — catena tracciata:
`plastictool_animate.cpp` → `::setKeyframe(vd->m_params[SkVD::ANGLE], ...)` →
`plastictool.cpp:196 setKeyframe(TDoubleParamP&, double)` → `createKeyframe()`.
⚠️ Ma `createKeyframe` NON puo' produrre quella pendenza: con Auto Bezier acceso
clampa, spento assegna maniglie **orizzontali** (`segmentWidth/3, 0`). **Quindi
qualcosa RUOTA le maniglie dopo la creazione.** Due candidati, nessuno
verificato: le **maniglie linkate** (forzano i due lati di una chiave a restare
allineati e possono importare la pendenza del segmento vicino) oppure una
riscrittura successiva dello stamping delle pose.

**DA QUI SI RIPARTE**: trovare chi ruota le maniglie, e applicare li' il clamp
gia' esistente. Rimedio nel frattempo: Auto Bezier sul segmento (o Linear).

⚠️ **QUATTRO misure sbagliate prima di arrivarci, non rifarle**: (1) maniglie vs
lunghezza del segmento; (2) salti di valore; (3) maniglie vs dislivello **con
una soglia assoluta** che scartava i valori piccoli dei vertici plastici;
(4) segmenti **piatti** con tangenti non nulle — e il caso vero non e' piatto
(la chiave vale 55, non 0), quindi cadeva fuori da ogni filtro. Il parser dei
`.tnz` copriva tutto (7349 tag su 7349): non era un problema di copertura ma di
**criterio**. Vedi [[feedback_instrument_before_optimizing]].

### 🔴 BUG — Production Tracker legge la scena sbagliata (2026-08-04)

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

### 🔮 Famiglia futura: GENERATORI di keyframe (overshoot, bounce, shake cam)
Idea di Franco, 2026-08-04. Sono **una feature a se'**, non dei preset:
- **Overshoot** — il valore supera il bersaglio e ci rientra (una posa che non
  si raggiunge ma si oltrepassa: e' la differenza fra meccanico e con peso).
- **Bounce** — la stessa cosa ripetuta e **smorzata**: supera, torna, risupera
  di meno, si posa.
- **Shake cam** — aggiunta da Franco: tremolio di camera.

**Perche' non sono preset**: un segmento fra due chiavi e' una singola bezier
cubica, puo' fare UNA gobba. Un'oscillazione smorzata richiede **piu' chiavi con
ampiezza calante**, cioe' vanno GENERATE. Gli altri preset scrivono due numeri su
un segmento che esiste gia'; questi cambiano quante chiavi ci sono.

**Avranno dei settings** (intuizione di Franco, ed e' giusta): durata, ampiezza,
numero di oscillazioni, smorzamento. Quindi servono un dialogo e delle scelte:
dove finisce l'oscillazione se dopo c'e' un'altra chiave? spostando la chiave
finale, le chiavi generate seguono o restano? il Rove che fa su chiavi non messe
dall'utente? Probabilmente la risposta e' la stessa data al Rove — **one-shot**,
nessun flag serializzato — ma va deciso e collaudato a parte.

✅ **FUNCTION EDITOR COMPLETO — 2026-08-04** (commit `9b8e73921`). Tutte e tre
le voci escluse dalla prima fase sono chiuse e collaudate da Franco: preset di
easing, **speed graph** in sola lettura, **curve linkate**.

Decisioni di progetto prese con Franco, da non ribaltare senza motivo:
- **Speed graph**: riquadro sotto ad asse tempo condiviso (non sovrapposto — lo
  screenshot di una scena vera con sei curve ciano ha mostrato che sovrapporre
  le derivate sarebbe stato illeggibile); **solo curve selezionate**, come
  «only show selected» di Blender; nessun asse verticale etichettato.
- **Curve linkate**: il flusso parte dalle **guide**, non dalla curva da
  pilotare — quella spesso non esiste ancora e non si potrebbe selezionare
  (difetto trovato da Franco al primo collaudo). Canali abbinati **per nome**,
  bersaglio = elenco di **colonne** a scelta multipla. L'intervallo lo decide la
  **selezione**, perche' guidare un tratto e animare a mano il resto e'
  legittimo.
- **Niente trascinamento** dall'albero: e' un gesto uno-a-uno e il modello e'
  molti-a-molti. Scartato da Franco: «per come funziona non avrebbe senso».
- **Niente dialogo di modifica** del collegamento: l'espressione e' gia'
  visibile e modificabile selezionando la curva guidata. Sarebbe stata una
  seconda strada peggiore per la stessa cosa.
- **Marcatori nell'albero**: freccia + colore + corsivo. La freccia perche' un
  canale puo' essere guida E guidato insieme (catena) e il caso misto si mostra
  da se'; il colore serve a trovare la riga scorrendo, non a dire cosa fa.

⬜ ~~Restano DUE voci~~ **(storico, ora chiuse)** delle tre escluse dalla prima fase (i **preset di easing** sono ✅ **FATTI e collaudati il 2026-08-04**, commit `07e6528f7`: quindici curve nominate Sine/Quad/Cubic/Quart/Expo x In/Out/In-Out nel menu contestuale del grafico). Nessuna e' bloccata dal
modello dati — verificato leggendo il codice, la nota iniziale che diceva il
contrario era sbagliata:
- **Speed graph** — vista della derivata, dove gli errori di spacing si vedono
  (nel valore no). In **sola lettura** e' contenuto e vale il 90% del beneficio;
  renderlo editabile e' un progetto a se', perche' un punto spostato nello spazio
  della velocita' va reintegrato in quello del valore.
- **Funzioni linkate** — una curva che pilota piu' parametri con un offset.
  **Gia' possibile oggi**: `TDoubleKeyframe::Expression` + grammatica che
  referenzia altre colonne + rilevamento dei riferimenti circolari, e
  `Channel::getExprRefName()` e' gia' il payload del trascinamento col tasto
  centrale dall'albero. Manca solo la UI: un gesto invece di sintassi digitata.
- ✅ **Preset di easing — FATTO 2026-08-04.** Si sdoppiava, e infatti e' stata fatta solo la meta' facile. Le **forme di ease** sono valori di
  `m_speedIn`/`m_speedOut` su un segmento e viaggiano sulla macchina di
  applicazione in blocco gia' costruita: poche ore. **Overshoot e bounce no**:
  richiedono di GENERARE keyframe, ed e' una feature diversa e piu' grande.

Ordine consigliato per valore/rischio: preset di ease, speed graph in sola
lettura, funzioni linkate.

**🔧 IN LAVORAZIONE — ZtoRig pose-blend (task 59 + correttive di giuntura).**
⚠️ **Worktree separato**: `/Volumes/ZioSam/tahoma2d-workspace/tahoma2d-superplastic`,
branch `feature/ztorig-pose-blend`. **Il bundle da lanciare e deployare è
`Ztoryc-SP.app`, NON `Ztoryc.app`** (quello è master e NON contiene ZtoRig).
Master resta releasabile: il lavoro non va lì.

Stato al 2026-07-26d — il motore c'è (pose assolute/offset, stamping delle chiavi
plastic sull'xsheet, correttive di giuntura milestone 1/3). Franco ha collaudato
il rework finale dello slider auto-keying (l'unica parte mai verificata a mano):

- ✅ **Offset che schizzava via — RISOLTO** (`422461463`, verificato).
- ✅ **Gli altri slider non si azzerano — RISOLTO** (2026-07-26c, verificato). La forza
  ora è REGISTRATA nella curva guida invece che dedotta. Trappola trovata strada
  facendo: `m_guide` alimentava ancora il *blend* a runtime, quindi il fix proposto
  avrebbe applicato la posa due volte → blend rimosso (era vestigiale).
- ✅ **Pose di PERSONAGGIO, non di colonna** (2026-07-26c, verificato). Sparivano
  cambiando colonna sul rig esploso. `characterParts()` usa la stessa risalita di
  `PlasticTool::characterColumns()`; Record scrive su tutte le colonne, le operazioni
  si propagano per nome, un solo undo per gesto.
- ✅ **Posa Base** (2026-07-26c, verificato). Su un rig esploso il riposo vero è il
  disassemblato: si marca un'azione come Base e lo stamping interpola da lì.
- ✅ **Modalità `Part`** (2026-07-26c). Richiamo esatto sui soli parametri registrati —
  per fonemi e pose per-arto. `Offset` rinominato `Add`.
- ✅ **IK spento spegne davvero i pin** (2026-07-26c, verificato). `pinsEnabled` non era
  guardato da NESSUNA parte nella valutazione.
- ✅ **Angle bounds: il gizmo non creava mai la prima chiave** (2026-07-26c). Il ramo
  animato era irraggiungibile. Ora chiavia sempre → i bound seguono anche i livelli.
- ✅ **Multi-pin — MIGLIORATO molto** (2026-07-26d). A/B fatto: non era una regressione,
  era preesistente. Causa vera: `solveMultiAnchor` (drag) non clampa ai limiti d'angolo,
  `plant()` (valutazione) sì → il pin secondario mollava appena un arto aveva bound. Ora
  i limiti cedono al pin; più «il corpo resiste» (bisezione di fattibilità nel drag).
  Residuo peggiore da 10.4% a ~1.5% della diagonale del rig. Nuovo slider **IK Max Step**
  (1-90 gradi/evento, default 15).
- 📐 **Anche e spalle — LE MISURE.** Lo *stato* di questa voce sta in «Aperti al
  2026-08-02», dove il 2/8 Franco l'ha ricaratterizzata ancora: succede **solo su
  animazioni vecchie**, il che sposta il sospetto dal solver al dato già in scena. Qui
  sotto restano le misure del 27/7, che valgono comunque e che nessuno deve rifare.

  RICARATTERIZZATO
  2026-07-27 **con misure**, la diagnosi precedente era sbagliata. Franco: il controllo
  delle anche è ORA BUONO (dopo «il corpo resiste» + IK Max Step del 26d) e va
  **conservato**: NON fare la riscrittura «leva = cursore» a tappeto. Resta che
  «a volte basta poco e il personaggio scatta di colpo».

  **Misura** (build con `[IK_FEASIBLE]`, `lib_gino` rig single level, frame 23, pin sui
  due talloni, anca sx = `v=1`, anca dx = `v=5`, ramo `multiAnchor`):

  | | bersaglio | mouse | movimento |
  |---|---|---|---|
  | v=1, 1° evento | 9.93 | 9.93 | **204.77** (20x), poi il giunto si PIANTA |
  | v=5, metà corsa | 114 | 164 | **360** |
  | v=5, metà corsa | 87 | 486 | **359** — ma 90 → 16 |

  **ESCLUSO con misura** (non per deduzione): NON è la bisezione di fattibilità né i
  limiti d'angolo — `accepted` medio 0.995, 45 eventi su 46 al valore pieno, nessuno
  sotto 0.1: la bisezione non contratta quasi mai. Non è il Distance, non sono i bound
  semiaperti (il codice ripiega correttamente sul limite statico, `plastictool_animate.cpp:2034`).

  **Dove sta**: l'amplificazione è FRA il bersaglio e la posa risolta, con il vincolo
  pienamente soddisfatto. **La posa risolta non è funzione continua del bersaglio**:
  bersagli vicini → pose lontane (90→16, 87→359). Firma di un solver con più bacini di
  convergenza (FABRIK dentro `solveMultiAnchor`), non di un anello di retroazione.

  **Prossimo passo**: strumentare DENTRO `solveMultiAnchor`/`poseAt` — confrontare il
  bersaglio `t` con `P[v]` risolto e vedere se passate FABRIK vicine convergono a
  configurazioni diverse. Serve una build.

  **Strumenti già in codice** (non sono fix, non vanno in release): `ZTORYC_NO_ANGLE_CLAMP`
  spegne il clamp dei limiti (keyed E statici) per A/B; `[IK_FEASIBLE]` logga quanta parte
  del passo sopravvive alla bisezione. **Sono su master** (ci sono arrivati con il merge
  di ZtoRig, non col merge del 3/8): innocui a variabile spenta, ma restano due
  interruttori di debug in una build che si rilascia — da togliere quando la voce si
  chiude. I cinque membri `m_ikSweep*` che erano segnati come inutilizzati **non ci sono
  più**: verificato il 2026-08-03, quella pulizia è già stata fatta.
- ✅ **Angle bounds che risentono della rotazione del padre** (solo multi-colonna) —
  **RISOLTO 2026-07-27, verificato da Franco** («ora questa cosa è perfetta»).
  **Non era un problema di limiti: era il PIAZZAMENTO.** Il parenting a un hook portava
  la POSIZIONE del vertice ma non l'orientamento del suo osso, quindi piegando il busto
  la colonna del braccio non ruotava — e il braccio finiva fuori da bound che invece
  seguivano il corpo. Fix in `TStageObject::computeLocalPlacement` (`tstageobject.cpp`
  ~1884): un figlio agganciato a un vertice di mesh **plastica** eredita anche di quanto
  l'osso di quel vertice ha ruotato dal riposo → `makeRotation(ang + hookAng)`. Guardia
  stretta: un hook ordinario su un disegno normale non ha osso né deformazione e si
  comporta esattamente come prima. Interruttore A/B: `ZTORYC_NO_HOOK_ROT`.
  **Candidato upstream** (è comportamento storico di Toonz, non un bug): vedi
  UPSTREAM_PR_CANDIDATES.md, sezione feature request.

  **Due vicoli ciechi, annotati perché costano ore a chi li ripercorre:**
  1. `parentColumnRefDirs_animate()` cablato nel clamp — il vecchio TODO lo indicava come
     la cura. È la cura sbagliata: una volta che la colonna ruota, lo spazio locale ruota
     con lei e osso/bound/ventaglio seguono da soli. Sommare anche uno scostamento ai
     limiti **conta la rotazione due volte** (sintomo: i bound «si modificano leggermente»
     e compaiono linee con valori diversi da quelli del gizmo). Scritto e ritirato in
     giornata.
  2. Il TODO diceva di strumentare `writeBackAnglesFor_animate`. Misurato: per il drag di
     un giunto non-IK **non viene mai chiamata** (0 righe su 52 eventi). Il clamp che conta
     è `PlasticSkeletonDeformation::updateAngle`. E il range misurato era **identico** nelle
     due pose del busto (-95.38 in entrambe): il range non è mai stato il difetto.
- ✅ **Riattivando l'IK il personaggio salta — RISOLTO** (`fbafaeee5`, mergiato su master
  il 2026-08-03). Uscire dall'IK fa il bake in FK e molla i pin, ma i target di scena
  (PINWX/PINWY) catturati prima restavano indietro: descrivevano dove stava il piede
  quando venne piantato, cioè una posa che il bake aveva già assorbito. Rientrando, il
  primo solve trascinava il personaggio su quel bersaglio stantio. Ora rientrando si
  **ri-piantano i pin ATTIVI dove il personaggio sta in quel momento**, con lo stesso
  identico calcolo di `togglePinAtCurrentFrame` (per non avere due modi di catturare un
  bersaglio che un giorno divergono) e solo per i pin già attivi al frame corrente
  (riaccendere l'IK non deve inventare vincoli che non c'erano).
- ✅ **Correttive di giuntura, milestone 2 (authoring) — FATTA** (`d32e6c5ea` +
  `a9263e0a2` + `960a856e9`, mergiati il 2026-08-03). Il pennello misura la distanza
  **lungo la maglia** (BFS su `buildDistances`) e non a schermo, così col gomito piegato
  lavora su ciò che tocca invece che su ciò che copre; selezione multipla dei giunti con
  shift+clic; lo stacking order si assegna a tutta la selezione in un solo undo.
  ⚠️ Trappola pagata due volte: `buildDistances` scrive **solo** i vertici che la BFS
  visita, quindi con l'array a zero le isole staccate (braccia, gambe) restavano a
  distanza 0 — cioè più vicine di tutto. **Non raggiunto deve voler dire lontano.**

> **Le voci ancora aperte di ZtoRig non stanno più qui.** Vivevano in due posti che
> avevano già cominciato a divergere. Stanno tutte in **«Aperti al 2026-08-02»**, che è
> l'unica lista di stato; qui restano solo le prove e le misure, che è ciò per cui
> questa sezione serve.

**✅ FATTO — Import da carta nella Thumbnail room (task 63)**, completo e mergiato su
master il 2026-07-26 (`d7b7ff283`). Stampa foglio A4 (vuoto fotocopiabile o con i
thumbs), import da file multi-foglio e cattura da webcam, tutto verificato da Franco.
Dettagli nel CHANGELOG del 2026-07-26.

**🆕 NUOVO FILONE — Pose vettoriali / blend shape (avviato 2026-07-27).**
Primo test **funzionante e verificato**: due disegni vettoriali come estremi,
`TInbetween` come motore, slider a pilotare → «è una bocca eccome» (Franco).
Riquadro di test nel pannello ZtoRig, distruttivo e opt-in.

Architettura decisa: **NON serve un nuovo tipo di livello**. Una colonna con
deformazione Plastic espone già UN disegno e cambia forma via parametri
chiaviabili applicati al render — serve una **deformazione nuova sullo stage
object**. E il sistema di pose (Add/Pose/Part + curva di forza) **è già un
sistema di blend shape**: manca solo un secondo tipo di bersaglio, delta di
PUNTI per id di stroke accanto ai delta dei parametri.

Mattoni, in ordine:
1. **ID persistenti sui punti** — primo e non aggirabile. Inserimento a forma
   invariata via suddivisione di de Casteljau (stesso *t* su tutte le pose).
   Cancellazione = punto nascosto con peso 0, mai rimozione vera.
2. **Delta vettoriale dentro `PoseAction`**, accanto ai delta dei parametri.
3. **Sostituzione a render-time**, come le dissolvenze animatic (v0.8.0).

Da decidere prima di partire: stroke interi che compaiono/scompaiono, unione e
divisione di stroke, e che gli ID sopravvivano al salvataggio (entrano nel
formato file — la decisione più vincolante).
⚠️ Il vettoriale è **PLI**. TLV è raster colormappato.

**🆕🆕 POI — M5: Integrazione Kitsu [brainstorming 2026-06-26/27].**
Il prossimo grande filone. Tracking/review della pipeline via Kitsu (CGWire).
- **Client config-driven** (`KitsuClient`, QtNetwork + QJsonDocument): un solo URL+login,
  funziona identico su istanza locale docker, LAN, tunnel Cloudflare e **CGWire hosted**.
- **Modello sync = partizione di autorità** (NON bidirezionale campo-per-campo):
  Ztoryc autorità su struttura pre-produzione (shot/sequenze/timing/tecnica) → *push*;
  Kitsu autorità su review (WFA→DONE/RETAKE del supervisor) → *pull*. Conflitto vero
  ridotto a 1 campo (status stesso task) → last-write-wins su `updated_at`, Kitsu vince
  sugli stati di approvazione.
- **Upload-on-render**: opzione nei Render Settings → a render finito carica il filmato
  come preview sul task e fa **WIP→WFA**. Niente problema 100MB: upload su **endpoint locale**
  (LAN, salta Cloudflare); doppio URL `kitsu_local_url` / `kitsu_remote_url`. Proxy ffmpeg
  opzionale per review leggere da remoto.
- **Deployment-agnostico per altri utenti**: thin launcher sopra il docker-compose
  UFFICIALE CGWire (immagini pullate a runtime, no fork del deploy) come companion opzionale;
  oppure CGWire cloud (solo URL). Status/task_type **letti dal server**, mai hardcoded.
- **Fasi:** (1) login JWT + pull progetti/task_status reali + mappa status; (2) push shot
  list; (3) upload-on-render + sync status. **Partire dalla Fase 1** sull'istanza locale.

**🆕 DOPO KITSU — Export montaggio (DaVinci Resolve) [brainstorming 2026-06-27].**
Ultimo tassello della pipeline. L'animatic È già un rough edit (shot+timing+in/out+audio).
- **Via consigliata:** export **OTIO** (OpenTimelineIO, nativo in Resolve) o FCPXML/EDL —
  one-way Ztoryc→Resolve, portabile anche a Premiere/FCP. Round-trip solo se serve davvero.
- Alternativa "live": Resolve Scripting API (Python) per popolare la timeline con un click.
- Da verificare: cosa importa Resolve più pulito (OTIO vs FCPXML) + relink dei media.

**✅ FATTO — RILASCIATO in 0.6.3 (2026-06-27) — Production Tracker DI PROGETTO (roadmap A→B3d).**
Il tracker è ora un sottosistema di progetto completo (`production.ztrack`): shot list da
TUTTI gli storyboard del progetto con timing + task per-tecnica + status; Asset list, Team,
production data, **naming convention** e **Workflow** definibili/customizzabili. UUID v5
stabili shot+asset, **back-link nei .tnz esportati** (Fase A), pipeline status automatica
(export→READY, primo open→WIP), auto-workflow detection per tecnica, badge SB sugli
storyboard. Restano per il design doc completo: **export-to-AI per animatix** (proiezione
non ancora implementata) e l'integrazione **Kitsu** (M5, sopra). Vedi `DESIGN_production_tracker.md`.

**✅ FATTO — RILASCIATO in 0.6.3 — Export to worksheet (XLSX di progetto).**
`exportFullProject` (QXlsx): un .xlsx con tutti gli storyboard + tutti i tab (Project /
Overview / per-tecnica / Team / Assets / Workflows), status colorati Kitsu + dropdown.
Anche export per-scena sul Board ("Export Storyboard Spreadsheet").

**✅ FATTO — RILASCIATO in 0.6.2 (2026-06-23) — Thumbnail panel (sketch grid → export to board).**
Pannello (non ancora una "room") con griglia di panel su un raster contiguo MyPaint:
export-to-board (ritaglio panel → livello OVL multi-frame + sotto-scena + shot reale) +
shrink; persistenza per scena; **merge panoramiche** (merge esplicito di panel adiacenti, no
slicer geometrico); **transform tool** (move/copy/scale/rotate + lazo); **undo/redo**;
zoom-rotella + scrollbar + cursore pennello; griglia 4x4 default.
Aperti (prossime release): tasto Canc nudo (focus), icone Lucide/Phosphor — vedi
[[project_thumbnail_room_fase3]].

**✅ FATTO — Task 54: Custom logo nel PDF storyboard.** Campo UI + resolve path + render
(`painter.drawPixmap(...logoPixmap)` in `storyboardpanel.cpp`).

**✅ FATTO (auto-return) — Task 53: Shot ops in edit-shot mode.** Copy/Clone/Cut/Paste/Delete
da dentro una sub-scena funzionano: `onCopyShot()` & co. risalgono al main xsheet
(`while getAncestorCount()>0: MI_CloseChild`) prima di operare. Se in futuro serve l'operazione
**in-place** (senza uscire dalla sub-scena), è un raffinamento separato.

**✅ FATTO (2026-06-19, commit `d194149ad`) — Finalizzazione UI dedup: toolbar Board↔Animatic.**
Niente bottoni shot duplicati tra Board e Animatic nelle room Ztoryc. I comandi shot
condivisi vanno sulla toolbar della **timeline Animatic, parte SINISTRA** (così cadono
sotto il Board); i tool di editing a seguire a destra. Questo libera la toolbar del
**Board** per: menu auto/keep/renumber, numbering options, light arrow (+opzioni),
export PDF/scene/animatic (gli export forse anch'essi sull'Animatic). Motivo: il panel
Board può essere ristretto → troppe icone danno problemi.
- **Vincolo invariante:** ogni panel resta self-sufficient — una room custom col solo
  Board deve riavere TUTTI i bottoni. Quindi NON spostare i bottoni: ogni panel tiene la
  toolbar completa e nella room di default **nasconde i duplicati a runtime** se rileva il
  panel "owner" vicino (owner = Animatic).
- **Come:** enumerare i vicini con `currentRoom->findChildren<TPanel*>()` (room è un
  `TMainWindow`; pattern già usato in `floatingpanelcommand.cpp`, `mainwindow.cpp:1584`);
  su `showEvent`/cambio room il Board nasconde i bottoni condivisi se c'è un Animatic.
- **Complementare:** overflow "»" sulla toolbar Board (QToolBar extension o menu More) così
  da sola ristretta non perde mai bottoni.
- **Da decidere a inizio task:** lista canonica dei bottoni condivisi (un posto solo) +
  identificare i file room `.ini` di default (bundle + ~/Library). Logica shot già in
  `ztoryshotops` (dedup di logica fatto); questo è SOLO la parte UI/toolbar.

### ⌨️ Keys-cels — residuo aperto (resto della feature: FATTO, vedi DONE/archivio)

5. **Selezione combinata governata dal link "Keyframes Follow Exposure" [DESIGN, NUOVO].**
   Richiesta utente: con pref ON, *qualsiasi* selezione (incluso il drag sui diamanti)
   deve produrre una `TCellKeyframeSelection` (chiavi + celle sottostanti); con pref OFF
   selezione indipendente. Oggi metà già funziona (selezione CELLE → combinata); manca
   il verso selezione DIAMANTI → combinata quando pref ON. Cambio trasversale alla logica
   di selezione dell'xsheet (xsheetviewer/cell viewer mouse handling), impatta TUTTI i
   comandi combinati → da testare sull'intero repertorio. Priorità: valutare dopo dedup.

### 🔧 Aperti — investigare / bassa priorità

47. ✅ RISOLTO (verificato da Franco 2026-07-21) — Audio scrub meno reattivo dopo il merge.
    Lo scrub del viewer/xsheet normale e' tornato reattivo sul singolo frame; nessun
    intervento ulteriore necessario. (L'indagine A/B pre/post merge non serve piu'.)
41. NEW Cache RAM threshold configurabile (BASSA) — ora a 14.3% shipped in `tsystempd.cpp` (il tentativo di alzarlo al 25% è stato revertito perché l'eviction aggressiva crashava il Save All su scene pesanti, raster liberato durante `TRasterCodecLZO::compress`). Rifarlo in modo MIRATO: non toccare l'eviction globale durante i save; semmai rilevamento per classe di macchina (≤8GB→più aggressivo) + opzione utente. ⚠️ Collegato: cache-leak post-render (frame restano in cache, ~17GB su scena pesante; fix upstream `be20f9512` da portare).

Milestone:
- M2: In/Out Marker, Roll, Slide, Doppio Viewer, Export render
- M3: Quick-shot selector, Export PDF migliorato
- M4: Room REFERENCE (canvas PureRef-style)
- M5: Kitsu Integration (kitsu.ztoryc.org su Mac mini M4)

---
---
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
