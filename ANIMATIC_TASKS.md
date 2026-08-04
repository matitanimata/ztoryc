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
- ⬜ **Chiudere un loop di camminata.** `SkVD::POSE_PARAMS`
  (`plasticskeletondeformation.cpp:111`) mescola FORMA (ANGLE, DISTANCE, SO) e
  PIAZZAMENTO (ROOTX/Y, TRANS, ROT, SCALE, PIVOT, SHEAR): copiare «la posa»
  riporta indietro il personaggio. Con i pin il piazzamento non va copiato
  affatto — il pin porta la posizione, gli angoli la forma. **Da provare prima**:
  la modalita' `Part` richiama «solo i parametri registrati»; se Record cattura
  solo cio' che e' cambiato dal riposo, potrebbe bastare. Altrimenti serve un
  comando che copi ANGLE/DISTANCE/SO fra due frame lasciando il piazzamento.
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

⬜ **Restano DUE voci** delle tre escluse dalla prima fase (i **preset di easing** sono ✅ **FATTI e collaudati il 2026-08-04**, commit `07e6528f7`: quindici curve nominate Sine/Quad/Cubic/Quart/Expo x In/Out/In-Out nel menu contestuale del grafico). Nessuna e' bloccata dal
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
