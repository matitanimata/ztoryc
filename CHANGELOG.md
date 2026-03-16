# Ztoryc — Changelog

## [Unreleased]

### Fix
- **Crash cambio workflow → room Storyboard**: il binario in `toonz/Tahoma2D.app` non veniva aggiornato automaticamente dopo la compilazione. La causa era che ninja compila in `toonz/build/toonz/` ma non copia il binario nel `.app` principale. Soluzione: copiare manualmente il binario e ri-firmare con `codesign --force --deep --sign -`.
- **showEvent ZtoryAnimaticPanel**: rimosso `QTimer::singleShot` temporaneo usato come workaround. Il vero fix era aggiornare il binario.

### Chore
- Aggiunto `build_and_deploy.sh`: script che compila, copia il binario e ri-firma il `.app` in un solo comando.
- Aggiornato `AGENTS.md` con istruzioni di build corrette (copia binario + codesign obbligatori dopo ogni build).

### Noto
- Crash su Cmd+Q durante chiusura app — da investigare in `iocommand.cpp` / `mainwindow.cpp`. Non presente in Tahoma2D base, probabilmente introdotto da modifiche Codex.

---

## [0.1.0-backup] — 2026-03-14

### Modifiche contribuibili a Tahoma2D / OpenToonz

#### Fix: Marker In/Out per-xsheet
- File: `subscenecommand.cpp`
- Mappa statica `s_frameRangeMap` salva/ripristina play range per ogni xsheet.
- Stato: Implementato — da proporre come PR

#### Fix: Save Sub-Scene As corrompe path scena principale
- File: `toonzscene.cpp`, `iocommand.cpp`
- Aggiunto `!subxsh &&` al rename path; backup preventivo di tutti i path livelli.
- Stato: Implementato — da proporre come PR

#### Fix: Switch room programmatico
- File: `mainwindow.h/.cpp`
- Metodo pubblico `MainWindow::switchToRoom(const QString &name)` aggiorna `QStackedWidget` e `QTabBar`.
- Stato: Implementato — valutare PR

#### Fix: Mesh sottoscene in cartella sbagliata
- File: `meshifypopup.cpp`
- Usa `scene->getDefaultLevelPath(MESH_XSHLEVEL)` invece di path hardcodato.
- Stato: Implementato — da proporre come PR

#### Fix: New Scene senza save dialog
- File: `iocommand.cpp`
- Aggiunto popup `SaveSceneAsPopup` alla fine di `newScene()`.
- Stato: Implementato — da proporre come PR

#### Fix: BaseViewerPanel::getPreviewButtonStates crash (nullptr)
- File: `viewerpane.cpp`
- Null check su `m_previewButton` e `m_subcameraPreviewButton` prima di `isChecked()`.
- Stato: Implementato — da proporre come PR

#### Fix: Sidecar/multi-monitor DPR dinamico
- File: `gutil.cpp`, `sceneviewer.cpp`
- Rimosso caching statico da `hasScreensWithDifferentDevPixRatio()`.
- `getDevicePixelRatio(widget)` usa `widget->windowHandle()->screen()` su macOS.
- `SceneViewer::showEvent` connette `QWindow::screenChanged` per forzare update() su cambio schermo.
- Stato: Implementato — test parziale (Sidecar instabile). Da proporre come PR

### Modifiche specifiche Ztoryc

#### StoryboardPanel — Architettura Panel-based
- File: `storyboardpanel.h/.cpp`
- Rilevamento automatico panel da keyframe animazione, camera e cambi composizione
- Griglia configurabile, drag & drop, numerazione Auto/Keep/Renumber All
- Selezione multipla, copy/clone/paste shot, delete multiplo
- Export Shots come `.tnz` autonoma con asset isolati
- Refresh thumbnail su `frameSwitched` con debounce 1000ms

#### ZtoryModel — Singleton dati condiviso
- File: `ztorymodel.h/.cpp`
- `std::vector<ShotData>`, preview cache, operazioni CRUD, segnali Qt

#### ZtoryAnimaticPanel — Timeline Animatic + Viewer integrato
- File: `ztoryanimatic.h/.cpp`
- `ZtoryAnimaticRuler` — righello temporale
- `ZtoryAnimaticTrack` — blocchi colorati, durata da `getRange()` incluse celle vuote/rosse
- Zoom rotella del mouse, range 2-64 ppf, factor 1.15 per step
- Ripple Edit — drag bordo destro con shift shot successivi
- Playhead sync su `frameSwitched` solo al main xsheet
- Match Subscene Duration — context menu tasto destro
- `ZtoryAnimaticViewer` — viewer integrato, `m_alwaysMainXsheet=true`
- Layout orizzontale QSplitter stile NLE

#### SceneViewer — flag m_alwaysMainXsheet
- File: `sceneviewer.h/.cpp`
- Flag con getter/setter pubblici, forza `editInPlace=true` nel render

#### Room Layout Ztoryc
- Room BOARD: StoryboardPanel
- Room SHOTEDITOR: ComboViewer + Timeline + ZtoryBackPanel
- Room ANIMATIC: ZtoryAnimaticPanel (viewer + track)

### Bug noti
- Frame handle condiviso tra viewer animatic e normale — fix pianificato Milestone 2
- Durata shot non aggiornata in StoryboardPanel quando modificata dall'animatic
- Panel non sparisce quando si cancella un disegno
- Panel mancanti all'apertura scena — `refreshFromScene` non carica tutti i panel

---

## Bug noti aperti
- Crash su Cmd+Q — `MotionPathPanel::refreshPaths` durante chiusura app
- Frame handle condiviso — playhead animatic muove anche frame sottoscena corrente

---

## Da implementare

### In corso
- Collegamento StoryboardPanel → ZtoryModel come master unico
- Durata shot da marker In/Out sottoscena (read-only nel panel)

### Milestone 1 — Struttura base
- [ ] Shortcut tastiera Cmd+C/D/V (CommandManager)
- [ ] Undo/Redo (TUndoManager)
- [ ] StoryStrip Panel (filmstrip orizzontale)
- [ ] Order Review Panel (griglia compatta)
- [ ] Renumber avanzato con popup opzioni

### Milestone 2 — Timeline/Animatic
- [ ] Frame handle separato per viewer animatic
- [ ] Zoom ruler adattivo
- [ ] Tracce audio con waveform, drag con lock
- [ ] Export Animatic collegato al render
- [ ] Eliminare ZtoryBackPanel

### Milestone 3 — Shot Editor avanzato
- [ ] Quick-shot selector nella toolbar SHOTEDITOR
- [ ] Book-up panel (thumbnail shot precedente/successivo)
- [ ] Export PDF con preview reali

### Milestone 4 — Polish
- [ ] Popup avviso passaggio Keep→Auto
- [ ] Salvataggio layout room predefiniti
- [ ] Toggle Edit/Board migliorato

### Milestone 5 — AI Export / Kitsu Integration
- [ ] Export to AI — dialog/action/notes come prompt
- [ ] Model sheet linkati ai personaggi
- [ ] Integrazione Kitsu (Docker su Mac mini/ZioSam)
- [ ] Pipeline: Ztoryc → Kitsu → AI → Tahoma2D
