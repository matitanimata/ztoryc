# Ztoryc — Animatic Panel: Task List for Claude Code

> Aggiornato 2026-05-24. Task completati ridotti a una riga.
> Per le spec storiche dei task DONE vedere ANIMATIC_TASKS_ARCHIVE_2026-05.md e git history.
> NOTA: questo è il file canonico (puntato dal symlink ~/ZtorYc/ANIMATIC_TASKS.md);
> sostituisce ANIMATIC_TASKS_2026-05-12.md e tutti i precedenti.

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
| 13b | BUG Undo Razor non ripristina sub-scena: UndoBoardState salva TXshLevelP ma il child xsheet viene modificato da materializeCells/trimChildXsheetTo/shiftChildXsheetBy prima che il clone sia separato → undo ripristina Board/animatic OK ma sub-scena resta troncata. Fix: undo dedicato che salva snapshot del child xsheet (TXsheet serializzato) prima delle modifiche. | 2026-05-26 |
| 14b | FIX Mark-out a fine timeline all'avvio | 2026-05-01 |
| 15b | FIX Onion skin rimosso toolbar | 2026-05-01 |
| 16b | NEW Workflow startup page | 2026-05-02 |
| 17 | FIX Stop marker update immediato | 2026-05-02 |
| 18 | FIX Zoom rotella solo sul ruler | 2026-05-02 |
| 19 | FIX Cursore SizeHorCursor bordi blocchi | 2026-05-02 |
| 23 | NEW Layout template per ogni workflow | 2026-05-02 |
| MAIN-AUDIO-TOGGLE | Riscrittura completa: cache (no audio fantasma/tracce cancellate), ON=main/OFF=sub-scena a ogni profondità, scrub gapless ~150ms | 2026-05-22 |
| BUG-PREVIEW-COLORS | Anteprime Board: rimosso rgbSwapped() — R/B non più scambiati (rosa→azzurro) | 2026-05-22 |
| 29 | BUG Script Panel persistenza: import → +extras/script + tag scriptFile nel .ztoryc, reload su apertura, clear su scena nuova | 2026-05-22 |
| AUDIO-TRACK-FIX | Delete key (setFocus), cross-track selection clear (selectionCleared signal), drag/trim undo (UndoAudioEdit), focus border indicator | 2026-05-19 |
| ADD-AUDIO-UNDO | UndoAddAudioTrack: "Add Audio Track" è undoabile | 2026-05-19 |
| RAZOR-LINKED-UNDO | Razor linkato video+audio: TUndoScopedBlock + fix indici colonne audio post-cloneChild | 2026-05-19 |
| RAM-WAVEFORM | Cache waveform viewport-aware (~1200px sliding window) invece di full trackW (80k+px = 16MB/track su scene lunghe) | 2026-05-19 |
| RAM-AUDIO-COL | requireColumnSoundTrack capped at videoFrameCount-1 invece di sc->getMaxFrame() (audio file 2h → 1.27GB/traccia × 18 tracce = 23GB) | 2026-05-19 |
| RAM-THREADSAFETY | preBuildSoundTrackAsync usa mixingTogether() invece di xsh->makeSound(); elimina race condition su m_mixedSound condiviso | 2026-05-19 |
| RAM-CACHE-RENDER | ImageManager::invalidateAllCached() chiamata in OnRenderCompleted::onDeliver(); libera 10GB+ retention post-render | 2026-05-19 |
| BUNDLE-STUFF | CMake usa `rsync -a` (preserva symlink) invece di `cmake -E copy_directory` (li seguiva → 30GB di favorites copiati nel bundle) | 2026-05-19 |
| ENTITLEMENTS | Aggiunto Ztoryc.entitlements (mancava, causava fallimento codesign al deploy) | 2026-05-19 |

---

## Task aperti

---

### MOD — UI Headers contestuali (task 32)

**Priorita: ALTA | Tipo: MOD**

Sintomo / richiesta: le room non hanno header che identifichino chiaramente i panel.
Claudio vuole header statici o dinamici con label contestuali per ridurre la
confusione visiva durante il lavoro.

Specifica:
- Left panel (Board room + Shot room): label `BOARD` / `SHOT` in alto a sinistra
- Right panel (Animatic room): label `SCRIPT` / `PALETTE` a seconda del panel attivo
- Il viewer contestuale deve mostrare la label della scena/shot corrente
- Stile coerente con la UI esistente (font Tahoma2D, colore testo panel header)

File: `storyboardpanel.h/.cpp`, `ztoryanimatic.h/.cpp`, `mainwindow.cpp`.

---

### NEW — Single-instance guard (task 33)

**Priorita: ALTA | Tipo: NEW**

Problema: e' possibile aprire piu' istanze di Ztoryc contemporaneamente, causando
conflitti di scrittura sul file .ztoryc e corruzione dati.

Soluzione: `QLockFile` in `main.cpp` su un file lock in una directory condivisa
(es. `QStandardPaths::TempLocation`). Se il lock fallisce, mostrare un dialog
"Ztoryc is already running" e uscire.

```cpp
// In main.cpp, prima di creare QApplication:
QLockFile lockFile(QDir::tempPath() + "/ztoryc.lock");
if (!lockFile.tryLock(100)) {
    QMessageBox::warning(nullptr, "Ztoryc", "Ztoryc is already running.");
    return 1;
}
```

Edge cases:
- Lock non rilasciato dopo crash: `QLockFile::removeStaleLockFile()` + timestamp
- Utenti diversi sullo stesso Mac: path lock include username
- App bundle firmata: lock in `~/Library/Caches/ztoryc/` evita problemi SIP

File: `toonz/sources/toonz/main.cpp`.

---

### NEW — Room "Ztoryc T", Panel Navigator, rinomina "Ztoryc X", rimozione Browser (task 34)

**Priorita: ALTA | Tipo: NEW + MOD**

Richiesta Claudio (2026-05-23):

1. **Rinomina room esistente** "Ztoryc" -> "Ztoryc X" (mantiene il layout corrente)
2. **Nuova room "Ztoryc T"** -- layout semplificato, orientato al disegno di posa:
   - Left: StoryboardPanel (shot grid)
   - Center: ComboViewer (disegno shot corrente)
   - Right: palette / strumenti (no Browser, no Script)
3. **Panel Navigator** -- pannello compatto (stile Photoshop Navigator) che mostra
   una miniatura del viewer corrente con un rettangolo di navigazione.
   Posizione: sopra o sotto il right panel.
4. **Rimozione Browser** dai right panel di entrambe le room (Ztoryc X e Ztoryc T).
   Il Browser resta accessibile solo dalla room Tahoma2D standard.

Implementazione:
- Nuova room in `mainwindow.cpp` (`addRoom("Ztoryc T", ...)`)
- Layout salvato in `layout/rooms/ZtorYcT.ini` (o gestito via `TMainWindow`)
- Panel Navigator: nuova classe `ZtoryNavigatorPanel` in `ztorynavigator.h/.cpp`
  che si aggancia al `ComboViewer` attivo tramite signal
- Rimozione Browser: rimuovere tab Browser dal `DockLayout` dei right panel Ztoryc

File: `mainwindow.cpp`, `storyboardpanel.h/.cpp`, `ztoryanimatic.h/.cpp`,
nuovi file `ztorynavigator.h/.cpp`.

---

### BUG — Script Panel non persiste il file importato al reload

**Priorita: ALTA**

Sintomo: il pannello Script (right panel room Animatic) permette di importare
file .fdx e .txt, ma alla riapertura della scena il contenuto non viene
ripristinato -- il panel risulta vuoto.

Causa: il path del file importato non viene salvato nel .ztoryc.

Soluzione: copia nella cartella extras + path relativo nel XML.
Pattern identico a come Tahoma2D gestisce gli asset di scena.

Flusso:
1. Utente importa .fdx o .txt
2. File copiato in <nome_scena>_extras/script/ (cartella creata se mancante)
3. Path relativo salvato nel .ztoryc: <ScriptPanel file="extras/script/myscript.fdx"/>
4. All'apertura: ZtoryScriptPanel legge il tag e ricarica automaticamente

Save -- in saveZtoryc():
```cpp
if (!m_scriptPanel->currentFilePath().isEmpty()) {
    QString srcPath = m_scriptPanel->currentFilePath();
    QString extrasDir = sceneDir + "/" + sceneName + "_extras/script/";
    QDir().mkpath(extrasDir);
    QString destPath = extrasDir + QFileInfo(srcPath).fileName();
    if (srcPath != destPath) QFile::copy(srcPath, destPath);
    QString relPath = QDir(sceneDir).relativeFilePath(destPath);
    xml.writeAttribute("scriptFile", relPath);
}
```

Load -- in loadZtoryc():
```cpp
QString relPath = xml.attributes().value("scriptFile").toString();
if (!relPath.isEmpty()) {
    QString absPath = QDir(sceneDir).absoluteFilePath(relPath);
    if (QFile::exists(absPath)) m_scriptPanel->loadFile(absPath);
}
```

Edge cases:
- File gia in extras: nessuna copia, skip
- File mancante al reload: warning non bloccante
- Nessuno script importato: tag assente, nessun caricamento
- File grande (>1MB): copia asincrona o progress bar
- Reimportazione file diverso: sovrascrive il file in extras
- Export shot: lo script NON viene incluso (appartiene alla scena)

File: ztoryanimatic.h/.cpp (ZtoryScriptPanel: loadFile, currentFilePath),
ztorymodel.cpp o storyboardpanel.cpp (saveZtoryc/loadZtoryc), .ztoryc XML.

---

### PERF — Board thumbnail: rallentamento al switch Animatic <-> Board

**Priorita: ALTA | Tipo: PERF**

Sintomo: ogni switch Board/Animatic ricalcola tutte le anteprime da zero,
causando rallentamento proporzionale al numero di shot.

Step 1 -- Lazy loading + thread separato (impatto immediato):
Generare solo le thumbnail visibili nel viewport; le altre in background
via QThreadPool, risultato postato sul main thread con QMetaObject::invokeMethod.

```cpp
void StoryboardPanel::requestThumb(int shotIndex, Priority p) {
    QtConcurrent::run([this, shotIndex]() {
        QImage thumb = renderThumb(shotIndex, QSize(240, 135));
        QMetaObject::invokeMethod(this, [this, shotIndex, thumb]() {
            m_thumbCache[shotIndex] = thumb;
            updateCell(shotIndex);
        }, Qt::QueuedConnection);
    });
}
```

Step 2 -- Cache su disco con invalidazione su mtime:
Salva ogni thumbnail come PNG in <scena>_extras/thumbs/<uuid>.png.
Chiave invalidazione: QFileInfo(subscenePath).lastModified().
XML: <Shot uuid="..." thumbMtime="2026-05-10T14:32:00" .../>

```cpp
QString thumbPath = extrasDir + "thumbs/" + shot.uuid + ".png";
QDateTime subMtime = QFileInfo(shot.subscenePath).lastModified();
if (QFile::exists(thumbPath) && shot.thumbMtime == subMtime)
    m_thumbCache[i] = QImage(thumbPath);
else {
    QImage thumb = renderThumb(i, QSize(240, 135));
    thumb.save(thumbPath);
    shot.thumbMtime = subMtime;
    m_thumbCache[i] = thumb;
}
```

Step 3 -- Flag dirty per invalidazione selettiva:
ShotData.thumbDirty = true solo quando lo shot viene modificato.
Al refresh Board: ricalcola solo i dirty, usa cache per gli altri.

Step 4 -- Risoluzione fissa piccola:
renderThumb() produce sempre 240x135 (16:9) o 160x120 (4:3).
Mai passare frame full-res e scalare dopo.

Edge cases:
- Shot modificato mentre Board non visibile: flag dirty, ricalcolo al showEvent
- Shot appena aggiunto: placeholder grigio, thumbnail in background
- Shot cancellato: rimuovere PNG da extras e da cache in memoria
- Piu di 100 shot: thread pool limitato a QThread::idealThreadCount()
- Thumbnail corrotta: fallback, rigenera e sovrascrive

Ordine: Step 1+4 prima (nessuna modifica al formato file), poi Step 3, poi Step 2.

File: storyboardpanel.h/.cpp, ztorymodel.h/.cpp (thumbDirty, thumbMtime),
.ztoryc save/load.

---

### PERF/BUG — Saturazione RAM su scene lunghe (>7000 ftg, 30+ shot)

**Priorita: ALTA (era CRITICA) | Tipo: PERF + memory leak**
**Stato al 2026-05-19: parzialmente risolto. Restano cause #1 e #4-5 da investigare.**

#### Update 2026-05-19 — fix applicati

| # | Causa | Stato | Commit |
|---|-------|-------|--------|
| 2 | TImageCache retention post-render | ✅ FIXED | be20f9512 (ImageManager::invalidateAllCached) |
| 2b | Bundle CMake copiava 30GB seguendo symlink favorites | ✅ FIXED | bafa6a3b8 (rsync -a) |
| 3 | Cache waveform senza cap (16MB/traccia su scene lunghe) | ✅ FIXED | e9b1912e6 (viewport sliding window 1200px) |
| 3b | requireColumnSoundTrack allocava 1.27GB × traccia (audio file 2h × 18 tracce = 23GB) | ✅ FIXED | 69a8b9043 (cap at videoFrameCount) |
| 3c | preBuildSoundTrackAsync race su m_mixedSound shared | ✅ FIXED | e5a927b53 (mixingTogether direct) |
| 1 | Sub-scene mai scaricate (sospetto principale) | ❌ DA INVESTIGARE | — |
| 4 | Thumbnail cache senza cap | ❌ DA VERIFICARE | — |
| 5 | UndoBoardState snapshot accumulati | ❌ DA VERIFICARE | — |

#### Sintomo originale

Su scene lunghe (7000+ fotogrammi, 30+ shot) la RAM cresce progressivamente
durante la sessione fino a saturare il sistema (>32GB). Il rallentamento
e proporzionale alla durata della sessione, non solo alla dimensione della
scena -- comportamento tipico di un memory leak.

#### Osservazioni 2026-05-19

- Bug RAM riprodotto anche in Tahoma2D vanilla (render MP4 e PNG sequence)
  → conferma che le cause principali sono upstream, non Ztoryc-specific.
- Su render PNG 350 frame: 10GB residui post-render → fixato con #2.
- L'API `TImageCache::setMaxSize()` suggerita nel doc originale NON ESISTE:
  TImageCache fa eviction solo su `TSystem::memoryShortage()` (troppo tardi
  su Mac con RAM >= 32GB).

#### Cause probabili (in ordine di sospetto)

**1. Sub-scene mai scaricate dalla memoria (sospetto principale)**

Quando si entra in uno shot (double-click), Tahoma carica la sub-scene
completa in memoria: ToonzScene, TXsheet, tutti i livelli TLV, palette,
cache di rendering. Se la sub-scene non viene deallocata correttamente
all'uscita dallo shot, dopo N switch si hanno N scene caricate
contemporaneamente. Con shot da centinaia di frame TLV, si arriva
facilmente a decine di GB.

Verifica: aprire Activity Monitor durante una sessione e misurare la RAM
dopo ogni switch shot. Se cresce linearmente con il numero di switch,
e quasi certamente questo.

Fix: assicurarsi che ToonzScene / TXsheet della sub-scene vengano
distrutti (non solo dereferenziati) all'uscita dallo shot. Verificare
che non esistano shared_ptr o riferimenti circolari che impediscono
la deallocazione. Usare weak_ptr dove appropriato.

```cpp
// In ZtoryAnimaticController::exitShot() o equivalente:
void ZtoryAnimaticController::exitCurrentShot() {
    if (m_currentSubScene) {
        m_currentSubScene->clear(); // libera livelli e cache
        m_currentSubScene.reset();  // distrugge ToonzScene
    }
    TImageCache::instance()->clear(); // svuota cache frame TLV
}
```

**2. TImageCache di Tahoma senza limite (sospetto alto)**

Tahoma mantiene una cache interna dei frame TLV renderizzati
(TImageCache). Su una scena da 7000 fotogrammi con livelli TLV
ad alta risoluzione, questa cache puo crescere senza limite se
setMaxSize() non e impostato correttamente in ztoryc.

Verifica: cercare TImageCache::instance()->setMaxSize() nel codice
ztoryc. Se mancante o impostato a un valore troppo alto, e il problema.

Fix: impostare un cap ragionevole all'avvio, proporzionale alla RAM
disponibile del sistema (es. 25% della RAM fisica):

```cpp
// In ZtoryAnimaticController::initialize() o main ztoryc:
qint64 physicalRam = /* QSysInfo o sysctl su macOS */;
qint64 cacheMax = physicalRam / 4; // 25% della RAM fisica
TImageCache::instance()->setMaxSize(cacheMax);

// Svuotare esplicitamente la cache al cambio shot:
void ZtoryAnimaticController::onShotChanged(int newCol) {
    TImageCache::instance()->clear();
    // poi caricare il nuovo shot
}
```

**3. Cache waveform senza cap (sospetto medio)**

Il task PERF-AUDIO implementa la cache waveform in QImage, ma se non
c'e un limite alla dimensione totale della cache, su 7000 fotogrammi
di audio la cache cresce indefinitamente.

Fix: limite esplicito in MB sulla cache waveform totale, con LRU
eviction per le tracce meno recenti:

```cpp
// In ZtoryAudioTrack o ZtoryModel:
static constexpr qint64 MAX_WAVEFORM_CACHE_BYTES = 256 * 1024 * 1024; // 256MB
// Se cache totale supera il limite, rimuovere le QImage piu vecchie
```

**4. Thumbnail cache senza cap (sospetto medio)**

m_thumbCache e una QMap<int, QImage> senza limite. Con 100+ shot a
240x135x4 bytes = ~130KB cadauna, 100 thumbnail = ~13MB (accettabile),
ma se le QImage sono a risoluzione maggiore il problema si amplifica.
Verificare la risoluzione effettiva delle QImage in cache.

Fix: cap esplicito + LRU eviction (correlato al task PERF-BOARD):

```cpp
static constexpr int MAX_THUMB_CACHE_COUNT = 50;
// Se m_thumbCache.size() > MAX: rimuovere la thumbnail con LRU timestamp
```

**5. UndoBoardState accumula snapshot non limitati (sospetto basso)**

UndoBoardState salva snapshot dell'intero stato Board. Su sessioni
lunghe con molte operazioni, la storia undo puo accumularsi.

Fix: cap sulla storia undo (es. 50 operazioni max):

```cpp
// In ZtoryUndoManager o TUndoManager:
TUndoManager::manager()->setHistorySize(50);
```

#### Piano di investigazione (da fare prima di fixare)

Claude Code deve eseguire questi passi in ordine prima di implementare fix:

1. Aggiungere logging temporaneo in exitCurrentShot() e enterShot():
   ```cpp
   qDebug() << "RAM before exit:" << getCurrentRamUsage() << "MB";
   ```
2. Aprire Activity Monitor, aprire la scena problematica, fare 5-6
   switch tra shot diversi e misurare la crescita RAM tra un switch
   e l'altro.
3. Se la crescita e ~costante per switch: causa 1 (sub-scene leak).
   Se cresce piu lentamente e poi accelera: causa 2 (TImageCache).
   Se cresce solo durante playback: causa 3 (waveform cache).
4. Identificata la causa principale, implementare il fix specifico.
   Poi misurare di nuovo per verificare.

#### Fix da implementare in ogni caso (indipendenti dalla causa)

Questi fix sono sicuri da implementare subito senza investigazione:

```cpp
// 1. Cap TImageCache -- in initialize():
TImageCache::instance()->setMaxSize(2LL * 1024 * 1024 * 1024); // 2GB max

// 2. Clear TImageCache al cambio shot:
connect(ZtoryModel::instance(), &ZtoryModel::currentShotChanged,
        this, []() { TImageCache::instance()->clear(); });

// 3. Cap storia undo:
TUndoManager::manager()->setHistorySize(50);

// 4. Cap thumbnail cache (vedi task PERF-BOARD):
// Implementare LRU con MAX_THUMB_CACHE_COUNT = 50
```

#### Edge cases

| Caso | Comportamento |
|------|---------------|
| Clear TImageCache troppo aggressivo | Rallentamento al primo frame dopo switch -- accettabile vs saturazione RAM |
| Sub-scene con modifiche non salvate al clear | Chiedere conferma prima di scaricare, o salvare automaticamente |
| RAM fisica < 8GB (sistemi modesti) | Cap TImageCache proporzionale: RAM/4 con minimo 512MB |
| Playback lungo senza switch shot | TImageCache cresce durante il play -- clear periodico ogni N frame o su pausa |
| Undo oltre il cap | Le operazioni piu vecchie vengono silenziosamente rimosse dalla storia |
| Waveform cache invalidata da clear | Ricalcolo lazy al prossimo disegno del waveform -- nessun impatto visivo |

**File:** ztoryanimatic.h/.cpp (exitCurrentShot, onShotChanged),
ztorymodel.cpp (inizializzazione TImageCache cap),
ztoryundo.h/.cpp (history size cap),
storyboardpanel.cpp (thumb cache LRU).
**Riferimento Tahoma:** timage_cache.h/.cpp, toonzscene.h/.cpp.

---

### NEW — Sistema In/Out Marker per shot (PREREQUISITO BLOCCANTE per Roll e Slide)

**Priorita: ALTA**

Motivazione: Roll e Slide sono non distruttivi solo con in/out marker separati
dalla durata timeline. Senza di essi, accorciare uno shot cancella disegni.

Modello (come DaVinci Resolve / Premiere):
- Durata nella timeline = celle nel main xsheet
- Contenuto reale = sub-scene sempre intatta
- In/Out marker = porzione "attiva" della sub-scene

Invariante fondamentale: duration_in_timeline == outPoint - inPoint

Struttura dati -- aggiungere a ShotData in ztorymodel.h:
```cpp
int inPoint  = 0;   // frame sub-scene inizio porzione attiva
int outPoint = -1;  // frame sub-scene fine (-1 = usa durata naturale)
```

Salvataggio XML: <Shot uuid="..." inPoint="0" outPoint="47" .../>
Retrocompatibilita: se mancano i tag, inPoint=0, outPoint=durata-1.

Rendering timeline:
- Triangoli/tacche ai bordi del blocco per in/out
- inPoint > 0: indicatore "contenuto nascosto a sinistra"
- outPoint < durata_subscena-1: indicatore "contenuto nascosto a destra"
- Tooltip: "In: 0 | Out: 47 | Sub-scene: 72 frames"

matchSubsceneDuration: legge durata sub-scene, imposta outPoint=durata-1,
aggiorna durata nel main xsheet.

Export: se inPoint==0 && outPoint==durata-1: normale; altrimenti esporta
solo [inPoint, outPoint].

File: ztorymodel.h/.cpp, ztoryanimatic.cpp, storyboardpanel.cpp, .ztoryc.

---

### NEW — Roll Edit (RICHIEDE: In/Out Marker)

**Priorita: MEDIA | Stima: 2-3h**

Sposta il punto di taglio tra due shot adiacenti senza cambiare la durata
totale della timeline. Shot A e B si scambiano frame via marker in/out.
Contenuto interno intatto.

UI: icona toolbar ztoryc_roll.svg, cursore doppia freccia, hit area +-8px
sul bordo tra shot, overlay "+N/-N frames" durante drag, shortcut R.

```cpp
void ZtoryAnimaticPanel::onRollDrag(int colA, int colB, int delta) {
    ShotData& a = ZtoryModel::instance()->shotData(colA);
    ShotData& b = ZtoryModel::instance()->shotData(colB);
    int newOutA = a.outPoint - delta;
    int newInB  = b.inPoint  - delta;
    if (newOutA < a.inPoint + 1) return;
    if (newInB  > b.outPoint - 1) return;
    if (newOutA > subsceneDuration(colA) - 1) return;
    if (newInB  < 0) return;
    a.outPoint = newOutA; b.inPoint = newInB;
    updateMainXsheetDuration(colA, newOutA - a.inPoint + 1);
    updateMainXsheetDuration(colB, b.outPoint - newInB + 1);
    ZtoryModel::instance()->emitShotDurationChanged(colA);
    ZtoryModel::instance()->emitShotDurationChanged(colB);
}
```
Wrappare in UndoBoardState.

Edge cases:
- A o B collassa a 0: clamp + cursore rosso
- A/B senza contenuto oltre i marker: clamp su subsceneDuration
- Estremi timeline: funziona uguale
- Audio linkato: spostare boundary sound column
- Multi-selezione: agisce solo sui due shot adiacenti al click

File: ztoryanimatic.h/.cpp, ztorymodel.h/.cpp, toonz.qrc.

---

### NEW — Slide Edit (RICHIEDE: In/Out Marker)

**Priorita: MEDIA | Stima: 4-6h**

Sposta lo shot selezionato nella timeline. Durata e contenuto di B invariati.
Shot A e C adiacenti assorbono lo spostamento via marker in/out.

UI: icona toolbar ztoryc_slide.svg, drag sul corpo centrale (non bordi),
i vicini si ridisegnano live, shortcut U.

```cpp
void ZtoryAnimaticPanel::onSlideDrag(int colB, int delta) {
    int colA = colB - 1; int colC = colB + 1;
    if (colA < 0 || colC >= shotCount()) return;
    ShotData& a = ZtoryModel::instance()->shotData(colA);
    ShotData& c = ZtoryModel::instance()->shotData(colC);
    int newOutA = a.outPoint + delta;
    int newInC  = c.inPoint  + delta;
    if (newOutA < a.inPoint + 1) return;
    if (newOutA > subsceneDuration(colA) - 1) return;
    if (newInC  > c.outPoint - 1) return;
    if (newInC  < 0) return;
    a.outPoint = newOutA; c.inPoint = newInC;
    updateMainXsheetDuration(colA, newOutA - a.inPoint + 1);
    updateMainXsheetDuration(colC, c.outPoint - newInC + 1);
    ZtoryModel::instance()->emitShotDurationChanged(colA);
    ZtoryModel::instance()->emitShotDurationChanged(colC);
}
```
Wrappare in UndoBoardState.

Edge cases:
- B primo o ultimo: ForbiddenCursor, return immediato
- A/C senza contenuto: clamp su subsceneDuration / 0
- A/C collassa: durata minima 1 frame
- Audio B linkato: shiftLevelFromFrame su sound column
- Audio A/C: aggiornare boundary audio
- Multi-selezione: agisce solo sullo shot sotto il cursore

Nota: Slip non e tool separato -- gestito dai marker in/out dello shot.

File: ztoryanimatic.h/.cpp, ztorymodel.h/.cpp, toonz.qrc.

---

### NEW — Doppio Viewer Contestuale (RICHIEDE: Roll e/o Slide)

**Priorita: MEDIA**

Layout automatico per modalita trim:
  ROLL:  | OUT (fine shot A) | IN (inizio shot B) |
  SLIDE: | PREV OUT          | NEXT IN            |
  SLIP:  | PROGRAM           | SOURCE IN/OUT      |

Attivazione automatica all'attivazione del tool, ritorno a singolo alla
disattivazione. Scrub sincronizzato: mouse -> entrambi i viewer reagiscono.

Bonus opzionale: filmstrip Slip con frame -2/-1/CURRENT/+1/+2.

File: ztoryanimatic.h/.cpp (ZtorySplitViewer).

---

### NEW — Taglia/copia/incolla audio da tastiera

Cmd+X/C/V/Delete per tracce audio. File: ztoryanimatic.cpp.

---

### NEW — Volume per traccia audio

Slider/knob gain per-track. Campo float gain in ZtoryModel,
letto in TXsheet::scrub. File: ztoryanimatic.h/.cpp, ztorymodel.h/.cpp, txsheet.cpp.

---

### NEW — Transizioni

Dissolve tra shot, x/2 frame extra nella sub-scena.
UI: handle di overlap sul bordo. File: ztoryanimatic.h/.cpp, ztorymodel.h/.cpp.

---

### NEW — Startup popup hub

Riusare ZtoryStartup per New/Load/Load as Subscene.
Cancel contestuale: "Quit Ztoryc?" se nessuna scena aperta.
File: ztorystartup.h/.cpp, mainwindow.cpp.

---

### NEW — Navigation tags sul ruler

Tag colorati con label. Riferimento: RowArea::paintEvent in xsheetviewer.cpp.
Design session necessaria per relazione con sequenze.
File: ztoryanimatic.h/.cpp.

---

### NEW — Storyboard Arrow Tool (task 35)

**Priorita: MEDIA | Tipo: NEW | Stima: 2-4h**

Richiesta Claudio (2026-05-27): strumento freccia vettoriale per indicare
la direzione del movimento nei panel storyboard. Usato insieme allo strumento
arco (arc) è fondamentale per comunicare traiettorie, entrate/uscite campo,
e movimenti di camera.

**Funzionamento:**
- Disegna un tratto (linea retta o curva Bézier/arco) con punta di freccia
  auto-calcolata dalla tangente dell'endpoint
- Opzione arrowhead: inizio / fine / entrambi
- Spessore, colore, stile tratteggio configurabili nel tool options bar
- Si integra con lo strumento arco esistente di Tahoma2D (TBenderTool o
  analogo) oppure come tool standalone

**Implementazione suggerita (Opzione B — tool dedicato):**

```cpp
// ZtoryArrowTool: Tool che disegna stroke + arrowhead triangolare
class ZtoryArrowTool : public TTool {
    TPointD m_startPt, m_endPt;
    std::vector<TPointD> m_ctrlPts;  // per curva Bézier
    bool m_drawAtStart = false;
    bool m_drawAtEnd   = true;
    double m_arrowSize = 20.0;       // px, scalato con zoom

    void addArrowhead(TVectorImage *vi, const TPointD &tip,
                      const TPointD &tangent) {
        // tangente normalizzata → 3 punti triangolo → TStroke chiusa
        TPointD perp = TPointD(-tangent.y, tangent.x) * (m_arrowSize * 0.4);
        TPointD base = tip - tangent * m_arrowSize;
        // costruisce triangolo filled
    }
};
```

**Varianti arrowhead:**
- Standard: triangolo pieno, 30° angolo
- Open: "V" aperta (a chevron, come frecce da NLE)
- Double: freccia doppia (per movimenti bidirezionali)

Edge cases:
- Stroke troppo corta: nessun arrowhead se length < arrowSize × 2
- Zoom estremo: arrowSize clampato tra 5px e 100px schermo
- Colore: eredita dal pennello corrente (FG color)
- Undo: TUndoManager standard su leftButton release

File: nuovi file `toonz/sources/tools/ztoryarrowtool.h/.cpp`,
registrazione in `toonz/sources/tools/tooloptions.cpp`,
icona `ztoryc_arrow.svg`.

---

### NEW — Frecce 3D / Prospettiva (task 36)

**Priorita: BASSA | Tipo: NEW | Stima: 4-8h**

Estensione del task 35: frecce che comunicano movimenti in profondità (asse Z),
utili per storyboard con movimenti in prospettiva (dolly, zoom ottico,
personaggio che si avvicina/allontana dalla camera).

**Concept:**

Due modalità:

1. **Freccia foreshortened** (2D stilizzata):
   - Asse lungo accorciato progressivamente verso la punta per simulare
     la prospettiva dell'asse Z
   - Arrowhead più piccolo della base: rapporto size_tip / size_base
     controllato da un parametro "depth" (0=piatta, 1=massima prospettiva)
   - Implementabile come shape parametrica 2D — no matrici 3D reali

2. **Widget freccia 3D** (avanzato, futura iterazione):
   - Gizmo interattivo a 3 assi (X/Y/Z) posizionabile nel panel
   - Proietta la freccia 3D sulla prospettiva del panel usando
     la camera della sub-scena (TCamera → matrice proiezione)
   - Più complesso: richiede integrazione con sistema camera Tahoma2D

**Priorità implementazione:** variante 1 (2D foreshortened) prima,
variante 2 (gizmo 3D) come futura estensione.

Parametri UI: slider "depth" (0-100%), toggle direzione (verso/lontano camera),
colore + spessore ereditati da ZtoryArrowTool.

File: `ztoryarrowtool.h/.cpp` (estensione task 35), tool options.

---

### NEW — Indicatore Direzione Luce (task 37)

**Priorita: BASSA | Tipo: NEW | Stima: 4-6h**

Richiesta Claudio (2026-05-27): indicatore conico della sorgente di luce
nel panel storyboard, posizionabile in 3D. Riferimento visivo: i widget
spotlight dei software 3D (Cinema 4D, Blender) — un cono con il vertice
sulla sorgente di luce e la base sull'area illuminata, che mostra sia la
direzione che l'ampiezza del fascio.

**Concept — freccia conica (come i gizmo 3D di Blender/Cinema 4D):**

```
  ════════╗
  gambo   ╠══╗
  (shaft) ║  ║╲
          ║  ║ ╲
          ║  ║  ★  ← punto = direzione da cui viene la luce
          ║  ║ ╱
          ║  ║╱
  ════════╣  ║
          ╚══╝
           ↑
        testa conica (cone head)
```

- **Gambo cilindrico** = shaft della freccia, parte dalla sorgente di luce
- **Testa conica** = arrowhead conico appuntito, la punta indica la direzione
  verso cui va la luce (verso il soggetto/area illuminata)
- Identica alle frecce dei transform gizmo 3D: cilindro + cono
- La freccia rappresenta il raggio di luce: la coda è la sorgente,
  la punta del cono è dove arriva la luce
- Colore indica temperatura colore: giallo caldo, bianco neutro, blu freddo

**Struttura dati:**
```cpp
struct LightIndicator {
    TPointD tail;        // coda = posizione sorgente luce (coord norm. 0-1)
    TPointD tip;         // punta del cono = dove va la luce (soggetto/area)
    double  coneAngle;   // semi-angolo testa conica (default 20°)
    double  coneLength;  // lunghezza testa conica rispetto a shaft (default 30%)
    QColor  color;       // temperatura colore
};
```

**Rendering (QPainter in PanelWidget::paintEvent):**
```cpp
QPointF dir    = (tip - tail).normalized();
QPointF perp   = { -dir.y(), dir.x() };
double  total  = QLineF(tail, tip).length();
double  conLen = total * coneLength;          // lunghezza testa conica
double  conRad = conLen * tan(coneAngle);     // raggio base cono
double  shaftR = conRad * 0.3;               // raggio gambo

QPointF coneBase = tip - dir * conLen;        // dove finisce il gambo

// 1. rettangolo arrotondato (gambo): tail → coneBase, larghezza shaftR*2
// 2. triangolo (testa conica): coneBase±perp*conRad → tip
// 3. arco convesso sulla base del cono (come nello sketch)
```

- Overlay non distruttivo: non fa parte del disegno TLV
- Toggle visibilità rapido (shortcut L nel panel)
- Export PDF/immagine: opzionale (flag "includi overlay")

**Interazione:**
- Drag sulla coda: sposta l'intera freccia
- Drag sulla punta: ruota e scala la freccia
- Drag sulla base del cono: regola coneAngle (ampiezza testa)

Edge cases:
- Multiple luci per panel: lista LightIndicator[], click "+" per aggiungere
- Copy shot: gli indicatori vengono copiati con il panel
- Export: se export_overlays=false, non incluso nell'immagine esportata
- Freccia molto corta: coneLength clampato, gambo può sparire ma cono rimane

File: `storyboardpanel.h/.cpp` (PanelWidget overlay), `ztorymodel.h`
(LightIndicator struct in PanelData), `.ztoryc` save/load.

---

## Ordine implementazione consigliato

1. MOD UI Headers contestuali -- rapido, impatto visivo immediato
2. NEW Single-instance guard -- 1 file, 10 righe
3. NEW Room "Ztoryc T" + rinomina + Navigator + rimozione Browser -- piu' complesso, varie fasi
4. PERF/BUG RAM -- fix immediati (TImageCache cap + clear on shot change + undo cap)
5. PERF/BUG RAM -- investigazione leak sub-scene (logging + Activity Monitor)
6. PERF Board thumbnail Step 1+4 -- lazy loading + thread + risoluzione fissa
7. PERF Board thumbnail Step 3 -- flag dirty
8. PERF Board thumbnail Step 2 -- cache su disco
9. In/Out Marker -- prerequisito bloccante per Roll/Slide
8. Roll
9. Slide
10. Doppio Viewer

---

## Priority Order

~~32. MOD UI Headers contestuali~~ ✅ DONE 2026-05-27 (context chips Board/Animatic/Monitor)
~~33. NEW Single-instance guard~~ ✅ DONE 2026-05-27 (QLockFile in ~/Library/Caches)
~~34. NEW Room "Ztoryc T" + Panel Navigator + rinomina "Ztoryc X" + rimozione Browser~~ ✅ DONE 2026-05-27
~~31. PERF/BUG Saturazione RAM~~ ✅ DONE 2026-05-27 (lazy load, debounce, SFH repair)
29. ~~BUG Script Panel~~ ✅ DONE 2026-05-22
~~30. PERF Board thumbnail cache~~ ✅ DONE 2026-05-27 (renderXsheetFrame + per-col cache)
35. NEW Storyboard Arrow Tool (MEDIA) -- freccia vettoriale su arco/curva  ← PROSSIMO
36. NEW Frecce 3D / Prospettiva (BASSA) -- foreshortened, poi gizmo 3D
37. NEW Indicatore Direzione Luce (BASSA) -- overlay non distruttivo, posizionabile in 3D
20. NEW Audio cut/copy/paste tastiera
21. NEW Volume traccia audio
22. NEW Transizioni
24. NEW Startup popup hub
25. NEW In/Out Marker (PREREQUISITO Roll/Slide)
26. NEW Roll Edit
27. NEW Slide Edit
28. NEW Doppio Viewer Contestuale

Milestone:
- M2: In/Out Marker, Roll, Slide, Doppio Viewer, Export render
- M3: Quick-shot selector, Export PDF
- M4: Room REFERENCE (canvas PureRef-style)
- M5: Kitsu Integration (kitsu.ztoryc.org su Mac mini M4)

---

## File Structure

toonz/sources/toonz/storyboardpanel.h/.cpp   -- Board room
toonz/sources/toonz/ztorymodel.h/.cpp        -- Singleton data model
toonz/sources/toonz/ztoryanimatic.h/.cpp     -- Animatic panel + viewer
toonz/sources/toonz/ztorystartup.h/.cpp      -- Startup dialog
toonz/sources/toonz/icons/dark/ztoryc/       -- SVG icons (21 files)
toonz/sources/toonz/toonz.qrc               -- Icon registration
toonz/sources/stopmotion/webcam.h/.cpp       -- Webcam + AVCapture
toonz/sources/toonzqt/txshsoundcolumn.h/.cpp -- Audio column
toonz/sources/image/tzl/tiio_tzl.cpp        -- TLV save
toonz/sources/toonz/mainwindow.cpp           -- Workflow switch
toonz/sources/toonzlib/timage_cache.h/.cpp  -- TImageCache (riferimento per cap RAM)
toonz/sources/toonzlib/toonzscene.h/.cpp    -- ToonzScene (riferimento per leak sub-scene)
