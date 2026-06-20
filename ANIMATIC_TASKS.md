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

## Priority Order

### 🆕 DA FARE (giugno 2026) — in cima per priorità

**🆕 Thumbnail room (sketch grid → export to board) [richiesto utente, giugno 2026] — MILESTONE.**
Nuova room con una grande griglia vettoriale per fare thumbnail veloci: si schizzano i
panel (con pan H e V multi-panel), si selezionano i panel di uno shot, si riordina se
serve, poi **export to board** → il programma ritaglia i panel dalla griglia, crea gli shot
reali e li mette in timeline + board.
- **Stima:** è una milestone, non un task. Parente della M4 Reference room.
- **Componenti:** canvas vettoriale (QGraphicsView o riuso flip canvas) + selezione
  multi-panel con pan H/V + riordino + **slicer** che ritaglia i riquadri e li promuove a
  shot reali (mapping riquadro → subxsheet). La parte non banale è l'auto-slicing
  geometrico e il mapping verso `ZtoryModel`.

**🆕 Export to worksheet (Excel production management) [richiesto utente, giugno 2026].**
Finito lo storyboard, generare un file di production management con: produzione, episodio,
titolo; poi per ogni scena: seq, shot, timing, + i task (layout, animazione, vfx,
compositing, ...) con status (todo, ready, wip, wfa, retake, done). Versione "easy" in
attesa dell'integrazione Kitsu (M5).
- **Dati:** seq/shot/timing già in `ZtoryModel`; mancano i campi metadata (produzione/
  episodio/titolo) e la matrice task×status (da aggiungere al modello o a un .ztoryc field).
- **Tecnica:** C++/Qt non scrive `.xlsx` nativamente. Strategia: **prima CSV** (zero
  dipendenze, valida lo schema dei campi con Franco), **poi QXlsx** (Qt-based, MIT) per
  header colorati / dropdown status / formattazione "production".

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

47. INVESTIGATE Audio scrub meno reattivo dopo il merge (viewer/xsheet normale) (MEDIA) — l'utente
    nota che col toggle audio scrub NON distingue piu' il singolo frame come prima. VERIFICATO: il fix
    Ztoryc "widen scrub window" (scrubLen = max(samplePerFrame, sampleRate*0.15) ~150ms, commit 7b52a5e2e
    Fase D + evoluzioni gapless 8b1e57ac1/e978d61e4) E' ANCORA PRESENTE in txsheet.cpp:2121 (merge NON
    l'ha perso). Quindi il regresso e' in un percorso DIVERSO: lo scrub del viewer/xsheet normale usa il
    native sound scrub (tframehandle/txshsoundcolumn/flipconsole), e il merge ha cambiato il timing del
    frame-advance in flipconsole.cpp (auto-merge, fix upstream "changing cell during playback"/"resume
    play"). Ipotesi: il timing del frame-switch influenza durata/frequenza dello scrub nativo. Indagare
    con confronto A/B (build pre-merge b4aff742f vs post) + percorso onFrameSwitched -> sound scrub.
    Riferimenti commit audio: 7b52a5e2e, 8b1e57ac1, e978d61e4, ac383df1e (native scrub sub-scene).
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
