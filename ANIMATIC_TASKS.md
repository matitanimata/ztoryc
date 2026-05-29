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

---

## Task aperti

---

### BUG — Windows installer: DLL conflict su mixed install (ALTA)

**Priorità: ALTA | Tipo: BUG | Stato: confermato da utenti**

#### Sintomo 1 — path di installazione errato (parzialmente confermato risolto)

L'installer installa in `C:\Program Files\Tahoma2D\` invece di
`C:\Program Files\Ztoryc\`. Confermato dall'errore: l'exe è in
`C:\Program Files\Tahoma2D\Ztoryc.exe`. Utenti che hanno Tahoma2D già
installato vedono questo problema; chi installa da zero potrebbe non notarlo.
Workaround: disinstallare Tahoma2D prima, poi installare Ztoryc.

#### Sintomo 2 — Entry point not found al lancio

```
Ztoryc.exe - Entry Point Not Found
?onViewerDestroyed@TTool@@SAXPEAVViewer@1@@Z could not be located in
the dynamic link library C:\Program Files\Tahoma2D\Ztoryc.exe.
```

Causa: Ztoryc installato nella stessa cartella di Tahoma2D trova le vecchie DLL
di T2D (es. `toonzlib.dll`) che non hanno `TTool::onViewerDestroyed`, funzione
aggiunta da Ztoryc. Windows carica la DLL dal PATH prima di quella in bundle.
Fix definitivo = installer usa cartella dedicata `C:\Program Files\Ztoryc\`.

#### Sintomo 3 — Crash su New Scene (Windows-specific)

Sequenza: avvio OK → New Scene → crash durante creazione primo Board vuoto.
Probabilmente correlato al DLL conflict (funzioni mancanti chiamate durante
l'init del Board), oppure bug Windows-specific nel codice Board inizializzazione.
Da investigare dopo aver risolto il path installer.

#### Da fare

1. Verificare script installer (NSIS/WiX/CPack): cambiare `INSTALL_PREFIX` da
   `Tahoma2D` a `Ztoryc` in `CMakeLists.txt` o nel file `.nsi`/`.wxs`
2. Assicurarsi che tutte le DLL Ztoryc-modified vengano installate nella cartella
   Ztoryc e non cerchino quelle T2D in PATH
3. Testare su macchina senza Tahoma2D installato (caso pulito)
4. Se crash Board persiste dopo fix path: investigare separatamente

**File:** script installer Windows (`packaging/windows/` o `CMakeLists.txt` CPack section).

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

### NEW — Sistema In/Out Marker per shot (PREREQUISITO BLOCCANTE per Roll e Slide)

**Priorità: MEDIA**

Modello (come DaVinci Resolve / Premiere):
- Durata nella timeline = celle nel main xsheet
- Contenuto reale = sub-scene sempre intatta
- In/Out marker = porzione "attiva" della sub-scene

Invariante fondamentale: `duration_in_timeline == outPoint - inPoint`

Struttura dati — aggiungere a ShotData in ztorymodel.h:
```cpp
int inPoint  = 0;   // frame sub-scene inizio porzione attiva
int outPoint = -1;  // frame sub-scene fine (-1 = usa durata naturale)
```

Salvataggio XML: `<Shot uuid="..." inPoint="0" outPoint="47" .../>`
Retrocompatibilità: se mancano i tag, inPoint=0, outPoint=durata-1.

Rendering timeline:
- Triangoli/tacche ai bordi del blocco per in/out
- Tooltip: "In: 0 | Out: 47 | Sub-scene: 72 frames"

matchSubsceneDuration: legge durata sub-scene, imposta outPoint=durata-1,
aggiorna durata nel main xsheet.

Export: se inPoint==0 && outPoint==durata-1: normale; altrimenti esporta
solo [inPoint, outPoint].

**File:** `ztorymodel.h/.cpp`, `ztoryanimatic.cpp`, `storyboardpanel.cpp`, `.ztoryc`.

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

### NEW — Storyboard Arrow Tool (task 35)

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

1. BUG Windows installer path — cambiare INSTALL_PREFIX in Ztoryc, evita DLL conflict
2. BUG Windows crash Board — investigare dopo fix installer
3. NEW In/Out Marker — prerequisito per eventuali tool di trim futuri
4. NEW Arrow Tool (task 35) — approccio PLI brush preset
5. NEW Room TRADITIONAL (task 38)
6. NEW Integrazione Kitsu (M5)

---

## Priority Order

BUG-WIN-INSTALLER. BUG Windows installer / DLL conflict su mixed install (ALTA)
25. NEW In/Out Marker
35. NEW Storyboard Arrow Tool (MEDIA)
38. NEW Room TRADITIONAL (MEDIA)
20. NEW Audio cut/copy/paste tastiera
21. NEW Volume traccia audio
22. NEW Transizioni
24. NEW Startup popup hub
36. NEW Frecce 3D / Prospettiva (BASSA)
37. NEW Indicatore Direzione Luce (BASSA)
Kitsu. NEW Integrazione Kitsu (M5)

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
