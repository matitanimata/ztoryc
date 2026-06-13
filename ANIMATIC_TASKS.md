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

---

## Task aperti

---

### MOD — Feedback visivo timeline durante shot editing (task 39)

**Priorità: ALTA | Tipo: MOD**

Quando si entra in edit shot (double-click su uno shot), la timeline resta
visivamente identica alla modalità animatic — l'utente perde il contesto
di "dove si trova". Tre punti di intervento:

#### A — Dimming + bordo attivo nella timeline

All'entrata in edit shot:
- Tutti i blocchi eccetto quello attivo ricevono overlay semi-trasparente scuro
- Il blocco attivo riceve un bordo colorato (accent color Ztoryc, 2px)
- Le tracce audio si scuriscono proporzionalmente
- All'uscita: tutto torna normale

#### B — Banner contestuale sopra la timeline

Widget a tutta larghezza (~28px) visibile solo in edit shot mode:
```
✏️  Editing  SQ010 · SH020  —  "Titolo shot"    [Esci]
```
- Sfondo accent color (variante scura)
- Label SQ/SH + titolo shot se presente
- Bottone [Esci] equivalente al doppio-click di uscita

#### C — Overlay nel viewer nativo

Label testuale nell'angolo in alto a sinistra del viewer, visibile solo
in edit shot mode. Testo bianco con ombra, font small, semi-trasparente.

**File:** `ztoryanimatic.h/.cpp` (dimming + bordo + banner),
viewer nativo (overlay label).

---

### BUG — Discrepanza camera main xsheet vs sub-scene

**Priorità: ALTA | Tipo: BUG**

Sintomo: la camera nella sub-scene ha valori diversi da quella nel main xsheet
(es. F12 Z16 nella sub-scene vs F16 Z16 nel main), per cui l'inquadratura
visualizzata nel main non corrisponde a quella impostata nella sotto-scena.

Causa probabile: la camera del main xsheet è un oggetto separato da quella
della sub-scene. Quando si chiude la sub-scene, i valori della camera non
vengono propagati al main (o viceversa: la camera del main sovrascrive quella
della sub al momento del collapse).

Da investigare:
1. Verificare come `ztoryCloseSubXsheet()` / `ztoryOpenSubXsheet()` gestisce
   la camera: viene copiata dal main al sub all'apertura? Viene copiata
   dal sub al main alla chiusura?
2. Controllare se `TStageObjectTree` del main ha una camera indipendente
   che non viene sincronizzata con quella del child xsheet
3. Verificare se il problema si manifesta solo su certi valori di frame (F12 vs F16
   suggerisce uno sfasamento di frame offset tra main e sub)

Fix atteso: all'uscita dalla sub-scene, propagare i valori camera (posizione,
zoom, rotazione) al corrispondente oggetto camera nel main xsheet.

**File:** `ztoryanimatic.cpp` (ztoryCloseSubXsheet / onReturnToMain),
`ztorymodel.cpp`.

---

**FINDINGS sessione 2026-05-30 (confermati dall'utente):**
- I valori F/Z divergenti sono lo **Stage/Transform della colonna camera**
  (N/S/E/W/Z/scala), NON la barra di stato del viewer e NON il Camera Settings.
  Quindi è il transform dell'oggetto camera che differisce tra main e sub.
- La discrepanza **sparisce tornando al main**, ma a volte resta una differenza
  residua "come se la camera del main, pur senza keyframe/movimenti, avesse F/Z
  diversi". → la camera del main non è inizializzata uguale alla sub.
- **shot-010-bianco nel Monitor**: entrando nel PRIMO shot (frame 0) il viewer del
  monitor è completamente bianco. Succede SOLO sul primo shot, gli altri si vedono
  normalmente. Preesistente e indipendente da qualsiasi fix di sessione.
  Ipotesi: la camera main alla riga 0 ha un transform che manda il contenuto fuori
  inquadratura, oppure è un edge case di rendering del frame 0 nel viewer
  always-main-xsheet.
- ⚠️ **NON ritoccare l'affine ancestrale in `sceneviewer.cpp`** (righe ~2404,
  `if (editInPlace) ... getAncestorAffine`). Un tentativo di skipparlo
  (`5f335a295`) è stato REVERTATO (`6901cd844`): non risolveva il bianco e
  introduceva una discrepanza di inquadratura monitor vs viewer nativo. L'affine
  ancestrale serve a far combaciare il monitor con la sub-scena — va lasciato.

**DIAGNOSI VISIVA (screenshot utente 2026-05-30) — root cause confermato:**
- Nel **monitor** (camera del MAIN) il riquadro camera è spostato MOLTO a destra,
  OLTRE il bordo del disegno, sopra l'area grigia vuota → il primo shot inquadra
  il vuoto → **bianco**.
- Nel **viewer nativo** (camera della SUB) il riquadro camera è correttamente
  centrato sul contenuto.
- Quindi NON è un edge case del frame 0: è un **offset orizzontale della camera
  del main rispetto alla camera della sub**. Sul primo shot l'offset è massimo
  (esce dal contenuto → bianco), sugli altri è minore (si vedono, disallineati).
- **Direzione fix:** quando si crea/clona uno shot, la camera del MAIN xsheet alla
  posizione di quello shot deve essere allineata alla camera della SUB (stessa
  posizione/size/zoom). Verificare `cloneChildToPosition()`, `CAMERA-INIT`
  (commit 2026-04-05) e come la camera del main viene posizionata per ogni shot.
  Confrontare a runtime `xsh->getStageObject(CameraId(0))` (main) vs quello del
  child xsheet: N/S/E/W/Z/scala.

**ROOT CAUSE DEFINITIVO (analisi .tnz `SB_APPENNINGERS` 2026-05-30):**
- È un **mismatch di `cameraSize` tra sub-scena e main**, NON un problema di rendering.
- Camera MAIN: `16 9` (default, origine, nessun keyframe).
- Su 59 sub-scene: 29 hanno camera `16 9` (= main, si vedono bene), ma **4 sono
  anomale**: 3 × `12 6.75` e 1 × `12 9`. La camera del primo shot SH010 è `12 6.75`
  con offset x=13.4, scale 0.52, z 0→333 (camera move) → nel main 16×9 finisce
  fuori inquadratura → BIANCO. Gli altri 3 anomali sono solo disallineati.
- Spiega PERFETTAMENTE "solo il primo è bianco": è uno dei 4 con camera ≠ main.
- `onAddShot` (ztoryanimatic.cpp ~5544) GIÀ fa `childCamera->setSize/setRes =
  parentCamera` — ma evidentemente il path di creazione di QUESTI shot (import/clone
  dal Board) NON sincronizza la camera. Da trovare e fixare quel path.
- **Nota:** 12×6.75 ha lo stesso aspect di 16×9 (16:9, solo più piccola); 12×9 è
  4:3 (aspect diverso). Cambiare la size della sub a posteriori RICALCOLA
  l'inquadratura (i keyframe camera erano tarati su 12×6.75) → repair destruttivo,
  serve decisione utente.
- **Fix in 2 parti:** (1) PREVENZIONE: tutti i path di creazione shot (Board import/
  clone) devono forzare sub cameraSize/Res = main, come onAddShot. (2) REPAIR scene
  esistenti: azione per riallineare le camere sub al main (con avviso che reframma
  gli shot anomali).

**DECISIONE UTENTE (2026-05-30) — NON toccare i dati camera:**
- La camera della sub, se è più piccola (es. 12×6.75), **deve restare tale** — è una
  scelta legittima dell'animatore e l'animatic PURO la mostra correttamente.
- Quindi NIENTE forzatura sub=main, NIENTE repair distruttivo. (Le opzioni 1 e 2
  proposte sono SCARTATE.)
- **Il fix è SOLO nel rendering del MONITOR**: il viewer del monitor deve restare
  ancorato al **livello/camera del MAIN** anche quando si è dentro uno shot — cioè
  comportarsi ESATTAMENTE come in animatic puro (che funziona), invece di scendere
  in edit-in-place sulla sub.

**MECCANISMO (perché solo dentro lo shot e solo su shot con camera ≠ main):**
- In animatic puro la camera corrente È quella del main → tutto ok.
- Entrando nello shot, la "camera corrente" diventa quella della SUB. Il monitor
  (always-main-xsheet) renderizza il contenuto del MAIN ma si riferisce/inquadra
  sulla camera SUB. Per gli shot con sub=16×9 all'origine ≈ main → si vedono; per
  SH010 (sub 12×6.75 a x=13.4) → inquadra a destra mentre il contenuto main è
  centrato → fuori frame → BIANCO.

**PIANO IMPLEMENTAZIONE (sessione dedicata, iterare con test visivi):**
Orientare 3 punti in `sceneviewer.cpp` perché l'always-main-xsheet viewer, quando
`insideSubScene`, si comporti come al top level (camera del MAIN):
1. **Affine ancestrale** (~riga 2404, `if (editInPlace) ... getAncestorAffine`):
   non applicarlo per `m_alwaysMainXsheet && insideSubScene`. (Già provato in
   `5f335a295`, da solo NON bastava → vedi punti 2-3.)
2. **Camera di riferimento / box** (`getCurrentCameraId()` su `getCurrentXsheet()`
   ~righe 1444, 962, 2579): per l'always-main viewer usare la camera del TOP xsheet,
   non quella corrente (sub).
3. **Re-fit on scene switch**: verificare se entrando nello shot il monitor rifà il
   fit sulla camera sub (BaseViewerPanel::onSceneSwitched / fit-to-camera). Se sì,
   impedirlo per l'always-main viewer (mantenere il fit sulla camera main).
Test: SH010 (sub 12×6.75 offset) deve vedersi nel monitor identico all'animatic puro.

**Prossimi passi suggeriti (sessione dedicata, app aperta per test interattivo):**
1. Confrontare il TStageObject camera del main xsheet vs quello del child xsheet
   subito dopo `cloneChildToPosition()` / creazione shot: stampare N/S/E/W/Z/scala.
2. Verificare `CAMERA-INIT` (commit 2026-04-05): forse inizializza la camera sub
   ma non garantisce che la camera MAIN combaci, o viceversa.
3. Caso frame 0 / primo shot: verificare il transform della camera main alla
   riga 0 e perché il primo shot finisce fuori inquadratura.

---

### BUG — Mark-out nel main xsheet blocca il play della timeline

**Priorità: ALTA | Tipo: BUG**

Sintomo: c'è un mark-out impostato nel main xsheet che non corrisponde
alla durata reale della timeline animatic. Quando si fa play dalla timeline,
la riproduzione si ferma al mark-out del main invece di percorrere tutta
la timeline. L'utente deve cambiare workflow, trovare il mark-out e spostarlo
manualmente.

Causa: la timeline animatic usa il play range del main xsheet (In/Out markers
globali di Tahoma). Se questi vengono impostati o spostati accidentalmente
(es. durante l'editing di una sub-scene), rimangono "incastrati" e
condizionano il play dell'animatic.

Soluzioni possibili:
1. **Fix immediato**: alla fine di ogni operazione animatic che modifica
   la durata (add shot, merge, razor, resequence), resettare il mark-out
   del main xsheet all'ultimo frame valido
2. **Fix strutturale**: la timeline animatic dovrebbe gestire il proprio
   play range indipendentemente dai marker globali di Tahoma, o almeno
   aggiornarlo automaticamente dopo ogni resequenceXsheet()

Da verificare: in `resequenceXsheet()` o in `ZtoryModel`, dopo aver
aggiornato la durata del main xsheet, aggiornare anche i marker
In/Out globali:
```cpp
TApp::instance()->getCurrentXsheet()->getXsheet()
    ->setPlayRange(0, totalDuration - 1);
// oppure: TApp::instance()->getCurrentScene()->...
```

**File:** `ztorymodel.cpp` (resequenceXsheet), `ztoryanimatic.cpp`.

---

### BUG — ffmpeg non funziona / formati video assenti nell'output (REGRESSIONE)

**Priorità: ALTA | Tipo: BUG | Stato: regressione**

Sintomo: ffmpeg non funziona e i formati video (MP4, MOV, AVI ecc.) non compaiono
tra i formati di output disponibili nell'export. Il problema era già stato risolto
in precedenza ed è rientrato.

Da verificare:
1. Controllare che ffmpeg sia presente nel bundle (Ztoryc.app/Contents/MacOS/ o
   Ztoryc.app/Contents/Resources/)
2. Verificare che il path ffmpeg in `ffmpegframeworkinstaller.cpp` o `ffmpegplugin.cpp`
   punti correttamente alla posizione nel bundle
3. Controllare se il deploy script `build_and_deploy.sh` copia ancora ffmpeg
4. Verificare che le voci di formato video siano registrate in `tiio_ffmpeg.cpp`

File: `build_and_deploy.sh`, `toonz/sources/image/ffmpeg/`,
`toonz/sources/toonz/ffmpegframeworkinstaller.cpp`.

---

### BUG — Risoluzione thumbnail Board nel PDF pessima (REGRESSIONE)

**Priorità: ALTA | Tipo: BUG**

Sintomo: le thumbnail dei panel nello storyboard esportato in PDF hanno risoluzione
molto bassa, rendendo il PDF inutilizzabile per review/presentazione.

Da verificare:
1. In `onExportPdf()` / `exportStoryboard()`, controllare la risoluzione usata
   per renderizzare i frame: potrebbe usare la thumbnail della cache Board
   (240×135) invece di fare un render full-res apposito per il PDF
2. Verificare se c'è un path separato per il render PDF che dovrebbe ignorare
   la cache Board e rendere a risoluzione più alta (es. 640×360 o 1280×720)
3. Controllare il DPI impostato nel QPagedPaintDevice / QPrinter

File: `storyboardpanel.cpp` (onExportPdf), eventualmente `ztoryexport.h/.cpp`.

---


### NEW — Taglia/copia/incolla audio da tastiera

Cmd+X/C/V/Delete per tracce audio. **File:** `ztoryanimatic.cpp`.

---

### NEW — Volume per traccia audio

Slider/knob gain per-track. Campo float gain in ZtoryModel,
letto in TXsheet::scrub. **File:** `ztoryanimatic.h/.cpp`, `ztorymodel.h/.cpp`, `txsheet.cpp`.

---

### NEW — Transizioni

Dissolve tra shot, x/2 frame extra nella sub-scena.
UI: handle di overlap sul bordo. **File:** `ztoryanimatic.h/.cpp`, `ztorymodel.h/.cpp`.

---

### NEW — Startup popup hub

Riusare ZtoryStartup per New/Load/Load as Subscene.
Cancel contestuale: "Quit Ztoryc?" se nessuna scena aperta.
**File:** `ztorystartup.h/.cpp`, `mainwindow.cpp`.

---

### NEW — Navigation tags sul ruler

Tag colorati con label. Riferimento: `RowArea::paintEvent` in `xsheetviewer.cpp`.
Design session necessaria per relazione con sequenze.
**File:** `ztoryanimatic.h/.cpp`.

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

### NEW — Sistema Annotazioni Camera-Move + Light Direction (task 40) ⭐ UNIFICA 35/36/37

**Priorità: MEDIA-ALTA | Tipo: NEW | Stima: multi-sessione (3 fasi)**

> Design rivalutato 2026-05-31. **Supera e unifica i task 35, 36, 37.**

**REDESIGN 2026-05-31 — Annotazioni automatiche da camera animation:**

Idea chiave approvata: le annotazioni di camera move possono essere **generate
automaticamente** leggendo i parametri del Camera1 pegbar nella sub-scena:
- **Scale** cambia → Zoom in / Zoom out
- **Z** (posizione sull'asse profondità) cambia → Truck in / Truck out
- **X** dominante → Pan (destra/sinistra)
- **Y** dominante → Tilt (su/giù)
- Combinazioni → es. Pan + Zoom

Vantaggio: l'animazione di camera esiste già — l'annotazione è un riassunto
visivo automatico del movimento reale, non un'indicazione manuale che può
disallinearsi dall'animazione.

**Architettura ripensata (3 livelli separati):**

1. **Camera moves automatici** — overlay leggero sul thumbnail nel BOARD.
   Non una colonna PLI separata. Si rigenera al cambio camera.
   API: legge `TStageObject` del Camera1 pegbar nella sub-scena.

2. **Simboli manuali** — colonna "Annotazioni" (PLI) nella sub-scena per
   indicazioni non derivabili dalla camera: match cut, hook up, zip pan, ecc.
   Pannello `ZtoryCameraMovesPanel` già creato (FASE 1 in corso).

3. **Light direction** — gizmo 3D dedicato, completamente separato. FASE 3.

**Simboli standard:**
- Pan / Tilt → freccia dritta
- Truck in / Truck out → cornice + frecce interno/esterno
- Zoom in / Zoom out → cornice-nella-cornice + frecce angoli
- Zip pan → freccia + linee velocità
- Hook up → connettore tra panel
- Match cut / Match speed → tag testuale + marker

**FASE 1 — Fondazione manuale (IN CORSO 2026-05-31):**
- ✅ Pannello `ZtoryCameraMovesPanel` (`ztoryannotations.h/.cpp`)
- ✅ Colonna PLI "Annotazioni" auto-creata nella sub-scena
- ✅ Frecce Pan (8 direzioni) inseribili al centro canvas
- ✅ Crash fix: `sl->setScene(scene)` prima di `setFrame`
- ✅ Aggiunta a menu Panels → Ztoryc (fix: richiede 4 file — vedi memoria)
- 🔲 UI/UX da rifinire (scala frecce, posizione, palette visibile)
- 🔲 Simboli Truck In/Out, Zoom In/Out, Match Cut, Hook Up

**FASE 2 — Rilevamento automatico da camera:**
- Leggere keyframe Camera1 pegbar (X, Y, Z, scale) nella sub-scena
- Calcolare tipo mossa dominante confrontando primo e ultimo frame
- Overlay icona/freccia automatica sul thumbnail BOARD
- Aggiornamento on `xsheetChanged` della sub-scena (con debounce)
- API: `TStageObject::getParam(TStageObject::T_X)` ecc.

**FASE 3 — Light direction + render:**
- Gizmo luce: freccia conica + glifo sole, handle per angolo (gradi)
- Toggle visibilità annotazioni nel viewer + opzione export animatic

**File esistenti:** `toonz/sources/toonz/ztoryannotations.h/.cpp`
**File da aggiungere (FASE 2):** overlay in `storyboardpanel.cpp`,
lettura pegbar in `ztoryannotations.cpp`

**NOTA:** i task 35/36/37 sotto sono il design originale frammentato — mantenuti
per riferimento storico ma SUPERATI da questo task 40 unificato.

---

### NEW — Storyboard Arrow Tool (task 35) — ⚠️ SUPERATO da task 40

**Priorità: MEDIA | Tipo: NEW | Stima: 2-4h**

Strumento freccia vettoriale per indicare la direzione del movimento nei panel.
Approccio preferito: PLI custom brush a forma di freccia + preset Brush Tool
"Freccia annotazione"; PLI da creare manualmente nell'app e committare in
`stuff/library/vector brushes/`.

Alternativa (tool dedicato):
```cpp
class ZtoryArrowTool : public TTool {
    TPointD m_startPt, m_endPt;
    bool m_drawAtEnd = true;
    double m_arrowSize = 20.0; // px, scalato con zoom
    void addArrowhead(TVectorImage *vi, const TPointD &tip,
                      const TPointD &tangent);
};
```

Varianti arrowhead: Standard (triangolo pieno), Open (chevron), Double.
Edge cases: stroke troppo corta → no arrowhead se length < arrowSize×2.

**File:** `toonz/sources/tools/ztoryarrowtool.h/.cpp`, `tooloptions.cpp`, `ztoryc_arrow.svg`.

---

### NEW — Frecce 3D / Prospettiva (task 36)

**Priorità: BASSA | Tipo: NEW | Stima: 4-8h**

Estensione del task 35: frecce foreshortened per movimenti sull'asse Z.
Variante 1 (2D stilizzata, prioritaria): asse accorciato progressivamente,
parametro "depth" (0=piatta, 1=massima prospettiva).
Variante 2 (gizmo 3D, futura): proiezione camera TCamera → matrice proiezione.

**File:** `ztoryarrowtool.h/.cpp` (estensione task 35), `tooloptions.cpp`.

---

### NEW — Indicatore Direzione Luce (task 37)

**Priorità: BASSA | Tipo: NEW | Stima: 4-6h**

Overlay non distruttivo nel panel: freccia conica (gambo cilindrico + testa conica)
che indica la sorgente e la direzione della luce. Posizionabile con drag.
Colore = temperatura colore. Toggle visibilità shortcut L. Salvato nel .ztoryc.

```cpp
struct LightIndicator {
    TPointD tail;      // posizione sorgente (coord norm. 0-1)
    TPointD tip;       // dove va la luce
    double  coneAngle; // semi-angolo testa conica (default 20°)
    QColor  color;     // temperatura colore
};
```

**File:** `storyboardpanel.h/.cpp` (PanelWidget overlay), `ztorymodel.h`
(LightIndicator in PanelData), `.ztoryc` save/load.

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

## Ordine implementazione consigliato

1. BUG Mark-out main blocca play timeline — fix in resequenceXsheet()
2. BUG Discrepanza camera main vs sub-scene — propagare camera alla chiusura sub
3. MOD Feedback visivo shot editing (task 39) — dimming + banner + overlay
4. NEW Arrow Tool (task 35) — approccio PLI brush preset
5. NEW Room TRADITIONAL (task 38)
6. NEW Integrazione Kitsu (M5)

---

## Priority Order

✅ [FATTO 2026-05-31] BUG-CAMERA — monitor ancorato a camera main (commit 7d1746f3a)
✅ [FATTO 2026-05-31] 39. MOD Feedback visivo — highlight shot attivo (commit 043b5020b)
✅ [FATTO 2026-05-31] Marker/navigation tag in timeline (commit 7182b5543)
✅ [FATTO 2026-05-31] Sync selezione Board↔Animatic (commit 7182b5543)
✅ [FATTO precedente] 20. Audio cut/copy/paste tastiera
✅ [FATTO precedente] 22. Transizioni

✅ [FATTO 2026-05-31] 43b. Audio export clip per-shot — pinned in MovieRenderer::setAudioRange (commit 20f8f3e3b)
✅ [FATTO 2026-05-31] 43c. Nome file UUID → label leggibile sequenza (commit 20f8f3e3b)
✅ [FATTO] 43a. FIX Export clip per-shot — prima clip mancante (non riprodotto con certezza — confermato risolto)
✅ [FATTO 2026-06-10] 40. Sistema Annotazioni Camera-Move + Light Direction — COMPLETO
    — FASE 1: ✅ pannello Arrows con libreria .pli (colori originali, group, frame-aware).
    — FASE 2: ✅ overlay camera-move su Board+PDF (backed-out render, A→B, frecce, toggle Trk).
    — FASE 3: ✅ gizmo light direction (commit d498c010b): freccia conica 3D per-panel,
      drag + rotella (Z) + Shift+rotella (apertura fascio), ⊙/⊗ head-on, persistenza
      .ztoryc, undo, PDF. Fasi 1-2 rilasciate in v0.4.0-beta.2.

✅ [FATTO 2026-06-10] Burn-in export animatic (stile SBPro) — timecode + nome SQ_SH_P
    per-panel + checkbox clapperboard (mirror Board setting nativo) + bottone Render
    Settings nel dialog (non-modale, riepilogo formato live). Architettura:
    TRasterImageUtils::addBurnIn + MovieRenderer::setBurnIn pinnato al setup +
    ZtoryBurnInConfig in ZtoryModel. Raffinamenti estetici rimandati.

✅ [FATTO 2026-06-10 sera] 48. Undo svuotava lo storyboard — RISOLTO. Root cause:
    use-after-free dello snapshot. Cmd+Z dentro una sub-scene → restoreFromSnapshot
    → MI_CloseChild → closeChild pusha CloseChildUndo → TUndoManager::add() durante
    undo() attivo tronca il redo e DISTRUGGE l'UndoBoardState in esecuzione → la
    reference a m_before dangla → re-insert legge memoria liberata. Fix: deep-copy
    dello snapshot a inizio restoreFromSnapshot + firewall anti-wipe in saveZtoryc
    + fix captureSnapshot (TOP xsheet). RESIDUO per sessione lldb: TUndoManager
    rientra nell'oggetto undo liberato dopo add() — UB, possibile parente crash
    random (famiglia task 42).

✅ [FATTO 2026-06-10] 49. Lag/scatto sul secondo tratto di disegno — RISOLTO
    (commit e7676d620). Root cause: il detect timer (1s) partiva a ogni
    xsheetChanged in sub-scene e scattava a metà del tratto successivo
    (detectAndUpdatePanels + render thumbnail sincroni sul thread UI).
    Fix: xsheetChanged in sub-scene marca solo m_dirtyShotCol; detect+render
    solo su frameSwitched / ritorno al Board / showEvent, più guardia
    mouse-premuto sul timeout del timer.
✅ [FATTO 2026-06-10] 50. Panel fantasma dopo undo che svuota uno shot —
    verificato risolto (test utente) dopo i fix task 48 + TUndoManager.
✅ [GIÀ FATTO — rimosso 2026-06-10] 21. Volume traccia audio — già implementato
    (volume/mute/solo sulle tracce audio dell'Animatic).
✅ [GIÀ FATTO — rimosso 2026-06-10] 24. Startup popup hub — già implementato
    (ZtoryStartup).
✅ [GIÀ FATTO — rimosso 2026-06-10] Frame handle separato per l'animatic viewer —
    risolto di fatto col panel Monitor ancorato alla camera main.
✅ [DECLASSATO 2026-06-13] 52. CRASH Shift+N da dentro uno shot — palette dangling
    SIGSEGV in PaletteViewer::updateTabBar (crash log Crash-20260611-181136.log).
    Catena: MI_ZtoryNewShotAfter da dentro una sub-scene → close + nuovo shot + open
    → shotActivatedForViewing → ZtoryRightPanel::showShotMode → stack page SHOT
    → PaletteViewer::showEvent → onPaletteSwitched → updateTabBar → deref di
    TPaletteHandle::getPalette() NON nullo ma dangling.
    ── Esito sessione lldb 2026-06-13: NON RIPRODUCIBILE. Due torture test:
    (1) debug build ../debug-build + MallocScribble (UAF → crash deterministico):
        path esercitato 70× in updateTabBar (shot vuoti, raster, smart raster,
        global palette caricata, brush MyPaint) — MAI crashato; la handle del
        level-palette viene SEMPRE azzerata correttamente (setPalette con 0x0)
        nel percorso closeChild→newShot→openChild. L'address-match palette
        letta↔distrutta era falso positivo (riuso heap dello stesso indirizzo).
    (2) ESATTO binario release che crashò l'11 (toonz/Ztoryc.app, Jun 10 21:01,
        TUndoManager hardening c0e7c92bf GIÀ incluso) girato sotto lldb senza
        MallocScribble (timing/heap originali): nessun SIGSEGV in nessuna variante.
    → Heisenbug state-dependent, stessa famiglia del crash StudioPalette
    declassato 2026-06-11. NON patchato (regola: niente fix alla cieca; nel codice
    attuale non c'è bug identificabile sulla handle da correggere). CrashHandler
    resta a presidio: riaprire solo su recidiva con nuovo crash log.
    File di riferimento: ztoryanimatic.cpp (showShotMode ~4290, deferral singleShot
    già presente dal 31/05 commit 9c4b5ae02), paletteviewer.cpp:681, tpalettehandle.cpp.

51. 🟢 [DECLASSATO 2026-06-13] Brush feel — da ALTA strategica a feature opzionale on-demand
    La premessa "feel inferiore a TVPaint" era un'ASSUNZIONE di Fable 5, mai
    misurata né lamentata da un utente reale (e non abbiamo TVPaint per il
    confronto). L'utente percepisce il pennello "piuttosto reattivo".
    ── FASE 1 AUDIT LATENZA: FATTA (2026-06-13). Aggiunto BrushProfiler
    (`toonz/sources/include/brushprofiler.h`, header-only zero-cost; flag
    `ZTORYC_BRUSH_PROFILE=1`; instrumenta leftButtonDrag, paintGL, evt→paint).
    Misure su tavoletta reale (153 blocchi × 120 dab):
      dab_compute (event→raster)   med 0.08 ms  (gratis)
      paintGL (durata repaint)     med 0.27 ms  (gratis, incrementale+scissor OK)
      evt→paint (end-to-end CPU)   med 2.18 ms  (SOTTO un frame @60Hz)
    → La pipeline software NON è il collo di bottiglia. Repaint già incrementale
    (invalidateRect→clipRect→glScissor), tablet events NON compressi
    (AA_CompressTabletEvents off), nessun lavoro estraneo nel drag.
    Eventuale latenza percepita residua = presentazione/vsync/compositor macOS
    (livello Qt/OS, non nostra CPU) — NON inseguire senza una lamentela concreta.
    Reperti minori non risolti (solo se emergerà un problema reale):
      A) throttle 10ms MyPaint (toonzrasterbrushtool ~:2001): non è latenza,
         scarta punti sui tratti VELOCI → fedeltà; quantificabile con drop counter.
      B) stabilizzatore (m_smoothStroke): lag spaziale by-design quando Smooth>0.
    ── FASI 2-4 (stabilizzatore pull-string, preset pennelli, curve MyPaint .myb):
    restano valide SOLO come feature UX a sé, on-demand, NON come fix di latenza.
    BrushProfiler resta in repo come strumento di diagnosi riutilizzabile.
38. NEW Room TRADITIONAL (BASSA — "vedremo", declassata 2026-06-10)
Kitsu. NEW Integrazione Kitsu (M5) — unica voce roadmap pubblica nel README
41. NEW Cache RAM threshold configurabile (BASSA) — ora a 14.3% shipped in `tsystempd.cpp` (il tentativo di alzarlo al 25% è stato revertito perché l'eviction aggressiva crashava il Save All su scene pesanti, raster liberato durante `TRasterCodecLZO::compress`). Rifarlo in modo MIRATO: non toccare l'eviction globale durante i save; semmai rilevamento per classe di macchina (≤8GB→più aggressivo) + opzione utente. ⚠️ Collegato: cache-leak post-render (frame restano in cache, ~17GB su scena pesante; fix upstream `be20f9512` da portare).

✅ [DECLASSATO 2026-06-11] Crash palette StudioPalette→assign: NON riproducibile su
    debug build + lldb + MallocScribble (torture test completo). Era un derivato del
    bug undo-wipe task 48 (livelli distrutti dal restore rotto → palette corrente
    dangling). Fixato a monte: (a) task 48 deep-copy snapshot, (b) TUndoManager
    hardening 2026-06-11 (deferred delete dell'undo in esecuzione, tcore/tundo.cpp —
    CANDIDATO UPSTREAM). Riaprire solo in caso di recidiva (CrashHandler a presidio).


⚠️ AGGIORNAMENTO 2026-06-10 (task 42 residuo, storico): nuovo crash riprodotto disegnando —
    `StudioPaletteTreeViewer::loadInCurrentPalette → StudioPaletteCmd::loadIntoCurrentPalette
    → TPalette::assign → std::map<int,TColorStyleP>::destroy` SIGBUS su puntatore dentro
    la dyld shared cache __TEXT = use-after-free della palette corrente (dangling dopo
    switch livello/sub-scene). Crash report: Ztoryc-2026-06-10-112817.ips. Stessa famiglia
    del residuo: affrontare con DEBUG BUILD + LLDB, non patchare alla cieca.

42. ✅ [FATTO 2026-06-08] CRASH Geometric tool aprendo sub-scene (era "palette-switch ri-entrante")
    — root cause REALE diversa da quella ipotizzata: `GeometricTool::onDeactivate()` deref `m_viewer` nullo durante openSubXsheet. Fix in `geometrictool.cpp` (guard `if (m_viewer)`). Diagnosi via lldb. Il caso raster brush era già fixato (`8f8740628`). RESIDUO (pre-esistente, ALTA) — SIGSEGV cliccando colonna / aprendo sub-scena mentre è attivo un tool di disegno o pannello palette. Cascata: `TApp::updateXshLevel → TPaletteHandle::setPalette → (emit paletteSwitched) → PaletteController::editLevelPalette → setPalette → (emit) → slot del tool/pannello → deref stato transitorio → crash`. Connessione chiave: `palettecontroller.cpp:65-66` (paletteSwitched→editLevelPalette). Slot che crashano osservati: `StyleEditor::onStyleSwitched`, `ToonzRasterBrushTool::onColorStyleChanged`, `GeometricToolNotifier::onColorStyleChanged`, `PaletteViewer::onFrameSwitched`/`TPalette::setFrame`. Dump 0-byte = la variante stack-overflow. WORKAROUND utente: selezionare lo strumento freccia/Animate prima di operazioni che fanno openSubXsheet. ⚠️ NON patchare a scatola chiusa (tentativi del 2026-05-31 — TPaletteHandle ref forte + guard onStyleSwitched + TPaletteP — hanno causato REGRESSIONE: crash al disegno su scene nuove → tutto revertito). Affrontare con BUILD DI DEBUG + lldb per vedere lo stato reale dell'oggetto al crash, poi una sola fix mirata. File: `tpalettehandle.cpp`, `palettecontroller.cpp`, `styleeditor.cpp`, `geometrictool.cpp`, `toonzrasterbrushtool.cpp`, `paletteviewer.cpp`.

✅ [FATTO 2026-06-06] 43. Fix export animatic — tutti i bug risolti (a/b/c/d).

   ~~35. Storyboard Arrow Tool~~ → assorbito in task 40
   ~~36. Frecce 3D / Prospettiva~~ → assorbito in task 40
   ~~37. Indicatore Direzione Luce~~ → assorbito in task 40

44b. INVESTIGATE PSD bottom-layer "not found" caricando come sub-scene (BASSA) — una scena libreria personaggio (.tnz) creata importando un PSD da Affinity Designer (modalità "Columns + layers in a group as frames in a column", senza sub-xsheet), quando viene caricata come child level (sub-scene) in un'altra scena, mostra il layer PSD più in basso (posizione visiva più bassa nello stack, prima colonna nell'xsheet) come "not found" subito dopo l'import. Bug Tahoma2D confermato. Caso: `CH_sibilllaCat.psd` — 40 layer nel blocco `Lr16` (estensione 16-bit di Affinity), tutti i nomi layer sono stringhe vuote nel campo nome Pascal. Compare immediatamente (no save+reload necessario). Solo con PSD esportati da Affinity — da verificare con PSD di Krita e Photoshop. I nomi layer vuoti nel blocco `Lr16` potrebbero interagire con `REF_LAYER_BY_NAME` / `getLevelIdByName` in `tiio_psd.cpp`: `getLevelIdByName(layerStr)` lancia `TImageException("Layer ID not exists")` se nessun level ha nome corrispondente — eccezione catturata silenziosamente in `ChildLevelResourceImporter::process`. WORKAROUND: aggiungere un layer dummy/sacrificale come primo (bottom) layer in Affinity prima dell'export. File da investigare: `toonz/sources/image/psd/tiio_psd.cpp` (costruttore, blocco REF_LAYER_BY_NAME), `toonz/sources/common/psdlib/psd.cpp` (getLevelIdByName, gestione blocco Lr16).

✅ [FATTO 2026-06-06] 45. Status bar hint contestuali (Board + Animatic) — implementato.

~~46. Explode sub-scene con "maintain parenting" crea peg inutili~~ — non è un bug, è comportamento intenzionale di Tahoma2D. Chiuso.

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

Milestone:
- M2: In/Out Marker, Roll, Slide, Doppio Viewer, Export render
- M3: Quick-shot selector, Export PDF migliorato
- M4: Room REFERENCE (canvas PureRef-style)
- M5: Kitsu Integration (kitsu.ztoryc.org su Mac mini M4)

---

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
