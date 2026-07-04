## [2026-07-04] — Export shot → progetto esterno OpenToonz/Tahoma (dialog unico)

Implementata la feature "Export progetto esterno" (memoria project_export_external_project_ot):
gli shot del Board diventano un progetto autonomo con asset copiati, compatibile
OpenToonz o Tahoma. Tre iterazioni con test di Franco nella stessa sessione.

### Added
- **File ▸ Export ▸ Ztoryc ▸ Shots to New Project...** (`MI_ZtoryExportShotsToProject`):
  dialog unico con: destinazione **New project** (nome/posizione) o **Existing project**
  (albero progetti — utile per raccogliere shot di più storyboard nello stesso progetto);
  **Target application** Tahoma/OpenToonz; combo **Asset organization**; sezione asset
  folders custom (ripiegata di default, prefilled dal progetto corrente); versione;
  shots **All/Selected/Range**; checkbox Production Tracker con dicitura chiara
  ("Update Production Tracker (writes a .ztoryc file next to each exported scene)" +
  tooltip sugli avanzamenti di stato). (storyboardpanel.cpp/.h, menubar.cpp,
  mainwindow.cpp, menubarcommandids.h)
- **Motore riusabile in ExportScenePopup**: statiche `createProjectFromSpec` (NewProjectSpec:
  nome, posizione, folder custom, useSubScenePath, targetOpenToonz) e
  `exportScenesToProject` (import + collectAssets + passata OT). Il popup MI_ExportScenes
  guadagna anche lui folder fields custom + toggle target. (exportscenepopup.h/.cpp)
- **Compatibilità OpenToonz**: conversione a explicit holds di tutte le scene esportate
  (ricorsiva nelle sub-xsheet) + copia del project file come `<nome>_otprj.xml`
  (coesiste con tahomaproject.xml: la cartella è valida per entrambe le app).
  Il companion `.ztoryc` è governato **solo** dalla checkbox tracker, indipendentemente
  dal target: un export mirato a OpenToonz può comunque essere riaperto in Ztoryc in
  seguito solo per aggiornare lo stato produzione, anche se disegno/animazione
  avvengono in OT (prima versione legava la scelta al target, rimosso dopo feedback
  di Franco — spunto "tool standalone per tracker cross-app" salvato per il futuro).
- **Combo "Asset organization"** (nell'export E in File ▸ New Project nativo):
  1) Project folders (default) · 2) Scene sub-folders (= flag nativo useSubScenePath)
  · 3) **Assets next to each scene** = `scenes/$scenepath/{drawings,extras,inputs,outputs,stopmotion}`
  (layout storico di Franco, comodo per spostare scene tra computer). Palettes e scripts
  restano a livello progetto (palette color design e script sono condivisi tra scene).
  (projectpopup.cpp/.h)
- Refactor: core di Export Shots estratto in `exportShotScenesToDir(indices, dir, ver,
  writeLink, fail)` — riusato da entrambi i flussi, supporta indici sparsi (Selected).

### Fixed
- **convertToExplicitHolds convertiva le child xsheet a IMPLICIT** (txsheet.cpp ~2732,
  copy-paste bug dalla funzione inversa): convertendo a explicit le sub-scene perdevano
  il timing. Ora ricorre con `convertToExplicitHolds(0)`. **Candidato PR upstream**
  (aggiunto alla lista in AGENTS.md).
- **Staging autopulente senza prompt "already exists"**: le copie asset del salvataggio
  sub-scene (takeCareSceneFolderItemsOnSaveSceneAs, scene subfolders) finivano fuori
  dalla cartella temporanea (`scenes/extras/<nome>/`) → dal secondo export appariva il
  prompt di overwrite. Ora lo staging è `+scenes/ztoryc_export_tmp/scenes/` così le
  copie cadono DENTRO ztoryc_export_tmp → un solo rmDirTree (fatto anche preventivamente).

### Notes
- Edge case noto: se importScene rinomina una scena per collisione nel progetto target,
  il companion .ztoryc non viene abbinato (raro).
- Residui del primo test in `demoztoryc/scenes/extras/matitanimata__*` eliminabili a mano.
- Il salvataggio del progetto forza sempre il nome `tahomaproject.xml`
  (TProject::save → getLatestVersionProjectPath): per OT si copia il file, non si rinomina.

---

## [2026-06-28b] — Production Tracker standalone: rifiniture + fix contaminazione → RELEASE v0.7.0

Sessione di rifinitura della room Production standalone. Branch
**feature/kitsu-m5-phase3b** mergiato in **master** e rilasciato come **v0.7.0**
(prima release con l'intera integrazione Kitsu M5 + export DaVinci/FCPXML +
Production Tracker di progetto).

### Fixed — Production Tracker standalone
- **Cambio status non sporca piu' la scena**: editare uno status/assignee/tecnica nel
  tracker non chiede piu' di salvare una scena. La registrazione undo marcava la scena
  untitled come "modificata"; ora la pulizia del dirty-flag e' centralizzata nelle
  funzioni di persist (persistViaBoard/persistProjectDb/persistAssets), quindi copre
  anche il context-menu batch e undo/redo. (ztoryproductionpanel.cpp)
- **Contaminazione cross-progetto degli shot** (in memoria E su disco): aprendo il
  tracker di un progetto senza production.ztrack, il modello veniva salvato con ancora
  dentro gli shot/metadati del progetto precedente -> finivano nel .ztrack dell'altro
  progetto. Ora loadProjectDb() resetta i dati di progetto prima di creare il DB vuoto,
  e loadProjectDbFromDevice() resetta prima di ripopolare dal file. (ztorymodel.cpp)
- **Tile "Production Tracker" non ricaricava il progetto corrente**: rientrando nel tile
  (es. dopo aver cambiato progetto) la room non si ricostruiva e il tracker restava sul
  progetto precedente. Ora forza loadProjectDb() + productionReloaded. (startuppopup.cpp)

### Added — Production Tracker standalone
- **Uscita dalla room**: pulsante "<- Open or Create Scene..." in cima al tracker (solo
  nella room Production standalone) che riapre la Startup page per caricare/creare una scena.
- **Chrome pulita**: nella room Production la menubar nativa (File/Edit/...), i tab room e la
  main toolbar sono nascosti - resta solo il tracker + il pulsante. Applicato anche al
  riavvio dell'app (helper applyRoomChrome chiamato da room-change e startup). (mainwindow.cpp)

### Upstream candidates
- Nessuno (codice Ztoryc-specifico).

---

## [2026-06-28] — Fix export FCPXML, automazione status, sync multi-board, Kitsu opt-in, room Production (branch feature/kitsu-m5-phase3b)

Sessione lunghissima. Tutto su **feature/kitsu-m5-phase3b** (NON mergiato in master).

### Fixed — export / FCPXML
- Clip per-shot che contenevano tutti gli shot: render serializzati (segnale
  `ZtoryModel::renderFinished` + QEventLoop) — i render partivano concorrenti e si
  contaminavano. (`ee6a10fbd`)
- Full animatic più corto del montaggio: `shotFrameRange` escludeva la cella STOP_FRAME (SFH).
- Audio FCPXML: in-point sorgente (`getStartOffset`) e durata visibile, prima ignorati.

### Added — Kitsu (M5)
- Upload preview mp4 (`uploadPreviews`): comment WFA → add-preview → POST multipart;
  crea il task se manca; `set-main-preview` = thumbnail shot (primo frame). (`c1ac2d6ed`)
- Automazione status: export shot→DONE/Layout READY; apertura .tnz→primo task non-Done WIP;
  upload→WFA; pull DONE→successivo READY. Helper `firstProductionTaskType`/`nextTaskType`.
- Controlli sync (Push/Pull/Upload/handles) spostati nel tab Project del tracker (sempre
  disponibili); rimossa la tabella status mapping (Ztoryc↔Kitsu già 1:1). Helper condivisi
  `buildUploadsFromFolder`/`buildShotPushFromProject`. (`27fa7fc54`)
- Checkbox "Upload to Kitsu" nell'export (post-export auto-upload).
- **Opt-in**: checkbox "Use Kitsu" nella creazione progetto (flag `useKitsu` nel .ztrack);
  con off tutta la UI Kitsu è nascosta. `resetProjectLevelDefaults` azzera anche il binding
  Kitsu (un nuovo progetto non eredita il link del precedente). (`b6985e45c`)

### Fixed — numerazione Keep + multi-board
- Keep numbering era rotta (regressione, anche in 0.6.3): `m_autoRenumber` era per-pannello →
  spostato globale in `ZtoryModel`; e le label si riderivano per indice invece che dal nome
  colonna (identità) → fix in `refreshFromScene`. (`57b01f3fb`)
- Crash `addPanelWidget` su shot senza panel (guard). (`92a2bdfb3`)
- Sync multi-board: `onModelResequenced` ricostruisce su drift di conteggio/ordine/widget;
  bottoni Add sul board flottante; reorder cross-board in Keep. (`53c97c75b`, `d64fc458f`)
  RESTA: reorder cross-board in modalità AUTO (deferred). Vedi project_multiboard_sync.

### Added — Rooms / startup
- Room di default Storyboard: Production Tracker + Thumbs (ordine tracker, thumbs,
  ztoryc X, ztoryc T, browser). Tile PT azzurra nello startup → apre la room-set isolata
  Production (solo il tracker, non è una scena; si esce con load/create scene). (`1f5b8704b`)
- Production Tracker standalone: carica il .ztrack del progetto corrente su showEvent; gli edit
  non sporcano più la scena untitled.

### Note / RESTA
- Branch NON mergiato. Restano un paio di rifiniture (da definire con Franco) + reorder
  AUTO cross-board + transizioni vere FCPXML.

### Upstream candidates
- Nessuno (codice Ztoryc-specifico).

---

## [2026-06-27c] — Kitsu M5 Fase 3b + fix contaminazione + sync bidirezionale + export DaVinci (branch feature/kitsu-m5-phase3b)

Sessione lunga. Branch **feature/kitsu-m5-phase3b** (da master dopo il merge di 1+2+3a).

### Fixed — bug critico tracker
- **Contaminazione cross-progetto** (`84cba915e`): gli shot di uno storyboard finivano nel
  `production.ztrack` di altri progetti/storyboard. Causa: `ZtoryModel::m_shots` cresceva ma non
  si troncava per scene più piccole → al publish scriveva shot residui. Fix: `setShotsFrom()`
  sincronizza il modello alla scena PRIMA di ogni publish; `clearShots()` sui rami early-return di
  `loadZtoryc`; firewall `saveProjectDb` meno aggressivo (blocca solo se sul disco ci sono davvero
  metadati) → ripristina l'aggregazione multi-storyboard. Trovato instrumentando (log su file).

### Added — Kitsu (completamento sync bidirezionale)
- **Fase 3b task+status push** (`201d73042`) + allineamento nomi task-type (`cd8693ccf`,
  Render↔Rendering/VFX↔FX, case-insensitive).
- **Frame in/out come timecode MM:SS:FR** (`065a52b0`/`e520b7e1`): colonna tracker 'Sec/Fr'→'In-Out'
  cumulativo per source; export XLSX; push manda frame_in/out cumulativi.
- **Pull status (review sync)** (`2e4e2eb5`): il supervisor mette WFA→Done/Retake su Kitsu e torna
  in Ztoryc; match per shot+task con alias.
- **Sessione persistente** (`caf3e561`): KitsuClient singleton, auto-connect con credenziali salvate,
  password Remember default ON, dropdown pre-seleziona la produzione linkata.
- **kitsuShotId per shot** (`6fc37c9e`) + **PUT-by-id nel push** (`afdb8eed`): link rename-proof.
- **Handles** (`4e91a7b4`): checkbox 'Push with handles +N fr' padda frame_in/out su Kitsu.

### Added — Export montaggio (DaVinci/NLE)
- **FCPXML** (`db617160e`/`baedde6e0`): checkbox 'Also export DaVinci timeline' (solo con 'one clip
  per shot'); clip per-shot in ordine + **audio multitraccia** (ogni colonna sonora → lane separata,
  dialoghi/musica/fx mixabili in DaVinci). Range combinabile con 'one clip per shot'.

### Notes / da fare
- **Upload preview mp4 su Kitsu** + task **Storyboard** (WIP→WFA): contratto API nailato in memoria.
- **FCPXML transizioni vere** (cross-dissolve da `transitionFrames`): da fare DOPO conferma import base.
- Branch NON ancora mergiato in master.

### Upstream candidates
- Nessuno (codice Ztoryc-specifico).

---

## [2026-06-27b] — Kitsu M5 Fase 1+2+3a (branch feature/kitsu-m5)

Avvio dell'integrazione Kitsu (CGWire/Zou). Lavoro su **branch `feature/kitsu-m5`**,
NON su master. Validato contro l'istanza locale docker (localhost:8012) interrogando
l'API reale per azzeccare i JSON shape e i contratti.

### Added — Fase 1 (login + pull)
- `kitsuclient.h/.cpp` — `KitsuClient` (QtNetwork, `Qt5::Network` già linkato): login JWT
  (`/api/auth/login`), pull progetti (`/api/data/projects/open`) e task-status
  (`/api/data/task-status`). Config in QSettings (`Ztoryc/Kitsu/*`).
- Mappa status Kitsu→`TaskStatus` via `short_name` + fallback flag (`is_done/is_retake/
  is_feedback_request`); colori già coincidenti con quelli Ztoryc.
- `kitsuconnectdialog.h/.cpp` — dialog di connessione (URL/email/password, Connect,
  dropdown progetti, tabella mapping status). Aperto da "Connect to Kitsu…" nel tab Project.

### Added — Fase 2 (binding bidirezionale role-aware)
- Campo progetto **`code`** (sigla breve, es. CS26) + nuovo token **`{CODE}`** nel naming
  (3 resolver: outputsettings + storyboard ×2). Modello: `kitsuProjectId/Name`,
  `productionType/Style`, `ratio`, `resolution`, `isKitsuLinked()`, tutto nel `.ztrack`.
- KitsuClient: legge `user.role`, `canManageProjects()` (admin/manager), `createProject()`
  (POST), `updateProject()` (PUT).
- Dialog: **Link selected** (pull → scrive il modello, bind) e **Create new in Kitsu**
  (push, role-gated). Tab Project: Production/Code read-only + label "🔗 Linked" quando legato.
- Verificato in-app: create di una produzione su Kitsu + link con pull dei parametri. ✓

### Added — Fase 3a (shot push, solo Ztoryc→Kitsu)
- `KitsuClient::pushShots()` — macchina a stati sequenziale async: ensure episode (tvshow,
  find-or-create) → sequences → shots; upsert per nome (POST nuovo / PUT esistente,
  `nb_frames` + `data{frame_in,frame_out}`). Bottone "Push shots to Kitsu →".
- Sorgente: `projectShots()` con fallback agli shot di scena; sequenza default **SQ01**
  quando assente. Verificato in-app: 3 shot creati su Kitsu. ✓

### Notes / da fare (prossima sessione)
- Mancano su Kitsu: **thumbnail** shot (upload preview) e **task + status** sugli shot.
- **Asset** bidirezionali (download/upload), route create da rifinire (non `/assets/new`).
- Contratti API e decisioni in memoria `project_kitsu_m5_integration.md`.

### Upstream candidates
- Nessuno (codice tutto Ztoryc-specifico in file nuovi).

---

## [2026-06-27] — Export progetto, naming render, status pipeline + fix crash/perdita-dati (v0.6.3)

Sessione lunga: dall'export completo del progetto al ciclo di stato della pipeline
(Modello A: un .tnz per shot riusato in tutte le fasi), più due fix pesanti
(crash su nuovo progetto, perdita dati Production Tracker).

### Added — Export & naming
- **Export Project Spreadsheet** (Production Tracker → Shots): un singolo `.xlsx`
  con TUTTI gli storyboard + TUTTI i tab → fogli `Project · Overview · <tecniche> ·
  Team · Assets · Workflows`. Thumbnail dalla cache su disco (nessuna scena da aprire).
- Board: l'export è rinominato **"Export Storyboard Spreadsheet"** (scena corrente);
  il Production Tracker usa la stessa icona (più larga) per l'export di progetto.
- **Riordino task** nel tab Workflows: drag&drop + frecce ▲▼ (QToolButton).
- **Output name dal pattern di progetto** (Output Settings → "Name from project
  pattern…"): compone il nome del render da PROD/SEASON/EP/SEQ/SHOT/TASK/VER con
  preview live (Opzione 1 — Modello A: task solo nel nome dei render).
- Tab `Project` spostato per primo (pannello + fogli export).

### Added/Changed — Pipeline Modello A
- **Export Shots**: rimosso il selettore Task stage; il `{TASK}` non finisce più nel
  nome del `.tnz` (un solo file per shot riusato in tutte le fasi).
- **Ciclo di stato del primo task** (di solito Layout): export → `READY`; primo
  caricamento dello shot → `WIP`. Tecnica risolta dal **project DB** (quella impostata
  nel tracker) → ogni shot apre nella room giusta e avanza il task corretto.
- **Auto workflow detection**: aprendo una scena si entra nel workflow del suo
  ruolo/tecnica (storyboard→Storyboard; shot→Cutout/StopMotion/Tradigital). Spunta
  **"Automatic"** nello startup (persistente in QSettings); switch manuale sempre
  possibile (auto-switch one-shot, deduplicato per scena).
- Auto-WIP reso **room-indipendente**: listener globale su `ZtoryModel::sceneSwitched`
  (prima viveva solo nello StoryboardPanel → non scattava nelle room senza Board).
- Badge "SB" nello startup solo per `role="storyboard"` (non per gli shot esportati).

### Fixed — Crash & perdita dati (alta priorità)
- **CRASH creando un nuovo progetto** (`dvdirtreeview.cpp`): `connect(...projectAdded,
  [=]{...})` senza receiver context → connessione dangling sul singleton `DvDirModel`;
  dopo la distruzione del tree (room switch) il successivo `projectAdded` sparava su
  `this` morto → SIGSEGV. Fix: passare `this` come context. **Candidato PR upstream.**
- **Perdita dati Production Tracker** (production/team/assets svuotati): firewall in
  `saveProjectDb()` — non sovrascrive un `.ztrack` esistente con meta+team+assets tutti
  vuoti (firma di un reset transitorio non ancora ripopolato da `loadProjectDb`).
- `saveZtoryc()` non riscrive più il `.ztoryc` companion delle scene `role="shot"`
  (riscriveva `role="storyboard"` → badge SB + workflow sbagliato).
- Colonne task del tracker non sparivano più aprendo uno shot: `spreadsheetTaskColumns()`
  ora copre anche `m_projectShots` (non solo gli `m_shots` della scena aperta).

### Upstream candidates
- `dvdirtreeview.cpp` — dangling lambda connection su `DvDirModel::projectAdded` (crash
  riproducibile su creazione progetto dopo distruzione del tree). Alto impatto, fix 1 riga.

---

## [2026-06-26b] — Thumbnail persistenti multi-storyboard + fix popup startup/progetti (B3b)

Sessione di hardening multi-scena del Production Tracker e fix UX della startup page.

### Fixed — Thumbnail Production Tracker (multi-storyboard)
- **Cache thumbnail su disco** (`<project>/thumbs/<uuid>.png`): caricata interamente
  all'avvio del Production Tracker, così le anteprime di TUTTI gli storyboard sono
  sempre visibili qualsiasi scena sia aperta (prima si vedevano solo quelle della
  scena corrente, e il pannello a tutta room non le caricava).
- **UUID v5 namespaced per file storyboard** (`makeSourcedUuid`): elimina le collisioni
  di uuid tra `.ztoryc` diversi (es. anteprima di "bugs" che appariva su "camera").
  `ensureShotUuids()` rileva e rigenera gli uuid in conflitto cross-storyboard.
- **Attribuzione per `source`** in `publishShotsToProjectDb`: upsert aggiorna solo gli
  shot di proprietà del file corrente, niente più ri-attribuzione errata.
- Refresh thumbnail **event-driven** via segnale `previewUpdated` (debounce 400ms),
  al posto dei timer di retry.
- Badge **"SB"** sulle scene storyboard nella schermata di avvio.

### Fixed — Startup page / cambio progetto (doppio popup)
- **Root cause**: selezionare un progetto nell'albero del browser chiama
  `DvDirModelNode::makeCurrent()` che creava SEMPRE una seconda `StartupPopup` →
  due finestre, una non si chiudeva al caricamento scena.
- Ora `makeCurrent()` riusa la popup di startup già visibile: chiude il browser
  (come "Choose") via `BrowserPopupController::closePopup()` (reject) e aggiorna la
  stessa finestra con `StartupPopup::refreshAfterProjectChange()`. Nessun duplicato.
- Tracking di tutte le istanze (`s_instances` + `visibleDefaultInstance()`), valido
  per **Open Project**, **Load Scene** e **New Scene** (tab corretto per modo).

### Upstream candidates (file core condivisi)
- `filefield.h` (+`closePopup()` su `BrowserPopupController`), `filebrowserpopup.cpp/.h`,
  `filebrowsermodel.cpp` — il doppio-popup al cambio progetto da browser è bug stock
  Tahoma2D potenziale (verificare su stock prima di proporre PR).

---

## [2026-06-26] — Production Tracker: room a tab, multiselezione, e file di progetto (B3a)

Grande arco sul Production Tracker. Studiata la Kitsu reale locale; il tracker e'
diventato un mini-Kitsu in-app (pannello a tab) e ora ha un **file di progetto**
(`production.ztrack`) per i dati project-level. Design B3 completo in
`DESIGN_production_tracker.md` (Modello B). Tutto su master.

### Added — Production Tracker a tab (pannello `ztoryproductionpanel`)
- **Tab Shots**: matrice shot x task con status+assignee; arricchita con Thumbnail
  (riuso anteprime del Board via `firstPanelThumbnail`), Frames, Sec/Fr, **Workflow
  editabile** (click sulla cella), **Done** (progress bar). Set Technique del Board
  disabilitato (icona tenuta per futuro breakdown tagging).
- **Tab Team**: roster di progetto (add/remove/rename) — spostato fuori da Storyboard
  Settings.
- **Tab Project**: production/season(nuovo)/episode/title/default technique inline.
- **Tab Assets**: entita' Asset (type/name/tasks/tags) con add/remove + matrice task.
- **Tab Workflows**: editor task-type custom alla Kitsu (workflow + lista task editabili).
- **Assignee multipli** per task; picker dal Team (checkbox) con fallback testo libero.
- **Multiselezione** celle task (shift=intervallo, Cmd=salti, drag) -> tasto destro =
  batch set status/assignee (un solo undo); doppio-click = edit singolo.

### Added — B3a: file di progetto `production.ztrack`
- Modello B (deciso): `production.ztrack` alla radice progetto e' la **fonte di verita'
  project-level**. Possiede meta (production/season/episode/title/defaultTechnique/
  namingPattern) + team + techniques + assets, **condivisi tra le scene del progetto**.
- `ZtoryModel::loadProjectDb/saveProjectDb/resetProjectLevelDefaults`; migrazione
  automatica dai `.ztoryc` vecchi; il `.ztoryc` resta scene-level (shot/sequenze/
  numbering/panel/pdfLogo).

### Fixed
- Bug latente: i task stavano in 2 copie non sincronizzate (StoryboardPanel vs
  ZtoryModel) -> editing tracker non persisteva/esportava. Ora ZtoryModel e' la fonte
  unica (bridge pull/push a load/save/export). Aggiunto `ShotData::uuid` stabile (Fase A).
- Refresh tab non-shot al riapri scena (`productionReloaded`).
- **Leak project-level tra scene/progetti**: azzeramento completo dei campi a ogni load
  scena (incl. techniques->reseed) su tutti i path, prima della migrazione. Ripuliti i
  file gia' contaminati (`2303v15.ztoryc`).
- Palette status allineata ai colori Kitsu reali (WIP blu / WFA viola) nel pannello e
  nell'export xlsx.

### Added (fuori repo)
- `~/Desktop/Production_Tracker_Kitsu_Template.xlsx`: template standalone Google-Sheets-ready
  ispirato a Kitsu (Shots/Assets/Team/Dashboard), costruito con la skill xlsx.

### Notes / prossimi passi
- **B3b** (prossima sessione): shot nel project file + pubblicazione multi-storyboard
  (per uuid+source), tab Shots raggruppata per file; nodo thumbnail per storyboard non
  aperti. Poi B3c (export-tnz + naming convention `AVIS_CS26_EP03_AMC_V01`) / B3d.
- Room dedicata "Production" + eventuale colonna "Due" opzionale = separati.
- Deciso vs Kitsu: qui status/assignee/workflow/progress/thumbnail; deadline+scheduling -> Kitsu (M5).

## [2026-06-25] — Production Tracker panel (status in-app) + palette Kitsu allineata

Nuova direzione export production: **status masterizzati in-app**, foglio = proiezione
(niente Google API; integrazione Kitsu rimandata a M5). Decisioni prese studiando la
Kitsu reale locale di Franco (docker, localhost:8012): task type/status e colori veri.

### Added — Production Tracker panel (`ztoryproductionpanel.h/.cpp`, nuovo)
- Pannello dockable **matrice Kitsu-style**: righe = shot, colonne = task-type della
  tecnica (Layout, Key Animation, Inbetweening, …). Celle status colorate; **N/A** grigio
  dove il task non si applica alla tecnica dello shot. Voce **Windows ▸ Ztoryc ▸ Production
  Tracker** (registrato come gli altri pannelli: factory + commandid + tpanels + mainwindow
  + menubar.cpp + le 2 menubar.xml).
- **Editing in-app**: click su una cella → menu con i 6 stati (pallino colorato) → scrive nel
  modello, salva il `.ztoryc` via `StoryboardPanel::saveZtoryc()`, **undo** (`StatusEditUndo`,
  keyed su `shotLabel` → sopravvive al riordino degli shot).
- Refresh via nuovo segnale leggero `ZtoryModel::taskStatusChanged` (NON `shotDataChanged`,
  così niente re-bake thumbnail nel Board/Animatic). Nuovo setter `setShotTaskStatus` /
  `setShotTaskStatusByLabel` nel modello.
- **Re-export non distruttivo "gratis"**: con gli status nel modello/`.ztoryc`, ri-esportare
  dopo aver aggiunto shot non azzera nulla.

### Modified — palette status allineata a Kitsu
- `statusColor` del pannello E dell'export xlsx (`storyboardpanel.cpp`) ora usano i colori
  **ufficiali Kitsu**: Todo grigio · Ready ambra `#FBC02D` · WIP blu `#3273DC` · WFA viola
  `#AB26FF` · Retake rosso `#FF3860` · Done verde `#22D160`. Prima WIP/WFA erano invertiti
  (viola/blu) rispetto a Kitsu. La formattazione condizionale dell'xlsx usa la stessa lambda
  → allineata automaticamente.

### Added (fuori repo) — template spreadsheet standalone
- `~/Desktop/Production_Tracker_Kitsu_Template.xlsx`: template riutilizzabile Google-Sheets-ready
  (Project Info / Shots / Assets / Todo / Dashboard) ispirato al modello Kitsu, con dropdown
  status + colori condizionali + frozen panes + legenda. Costruito con la skill xlsx.

### Notes / prossimi passi
- **Fase 3** (da fare): auto WIP→WFA al render (`RenderCommand::onRenderCompleted`, già
  individuato → stesso path `setShotTaskStatus`).
- Guida naming-convention della produzione (pattern in `.ztoryc` + lint) — idea separata, da fare.
- Integrazione Kitsu vera = M5 (via API, non file).

## [2026-06-23d] — Post-0.6.2: polish Thumbnail panel + fix build Windows

### Fixed (Windows release)
- **Build Windows 0.6.2 fallita in link** (LNK2001): il canvas usa `MyPaintToonzBrush`/
  `Raster32PMyPaintSurface` di tnztools dall'eseguibile, ma erano senza `DVAPI` (non
  esportate dalla DLL). Aggiunto export. Commit `401828950`. La 0.6.2 Windows e' poi uscita.
  → Gotcha: classi di lib usate dall'exe vanno marcate `DVAPI` o falliscono SOLO su Windows.

### Added — Thumbnail panel polish (commit `adaf8e2e0`)
- Griglia di default **4x4** (era 4x3) + reset alla default aprendo una scena senza canvas
  salvato (bug: prima ereditava le righe aggiunte con +Row).
- **Zoom con la rotella** (centrato sul cursore), niente piu' Ctrl.
- **Scrollbar laterali** (16px) che compaiono solo quando il contenuto sborda; sync col pan
  da tasto centrale.
- **Cursore pennello**: cerchio della dimensione reale (exp(RADIUS_LOGARITHMIC + sizeMod) *
  zoom) con mirino; system cursor blank in draw mode.

### Note
- Loop in `createNewLevel` con nome livello collidente (NameModifier `_N` letto come
  separatore frame): **NON confermato come bug upstream**. La helper `rfindFrameSep`
  (`common/tsystem/tfilepath.cpp`) che decide i separatori e' **codice nostro** (PR trattino,
  commit `d1f4e21ef`), quindi il comportamento puo' differire da Tahoma stock. Da verificare
  su stock prima di considerare qualsiasi PR — probabilmente NON e' un problema upstream.

## [2026-06-23c] — Release 0.6.2: Thumbnail room completa (export, persistenza, panoramiche, transform, undo)

Rilascio **v0.6.2** (Win + macOS). Completata la Thumbnail room (Fase 3) e fix vari.

### Added — Thumbnail room (branch `feature/thumbnail-room`, in master via FF)
- **Export-to-board + Shrink** (`2f3267add`): i pannelli selezionati diventano uno
  shot reale nel Board (livello OVL multi-frame in `extras/<scena>/`, sotto-scena,
  colonna main, resequence). Spinbox **Shrink** 1-8 (risoluzione disegni = cameraRes/shrink).
  - **Fix hang**: `createNewLevel` andava in loop infinito quando esistevano gia' i PNG
    di export precedenti. `NameModifier` disambigua con `_N`, ma Tahoma legge `_<cifre>`
    come separatore di frame → `thumb_1` collassa su livello `thumb` → collisione eterna.
    Ora il livello e' nominato col label dello shot (`SH0xx`) e la disambiguazione su
    disco usa un suffisso LETTERA (`SH040B`). **Bug core Tahoma → candidato PR upstream.**
- **Persistenza del canvas per scena** (`b93899885`): il raster contiguo viene salvato
  come PNG in `extras/<scena>/_ztorythumbs_<cols>x<rows>.png` (autosave debounced +
  flush a chiusura), ricaricato al cambio scena. Prima i disegni si perdevano a chiusura.
- **Panoramiche via merge** (`c94c775a6`): merge di un blocco rettangolare di pannelli
  in un unico panel-panorama (selezione diagonale auto-completa il rettangolo). In export
  tutti i selezionati = UN solo shot su UN livello dimensionato sul pannello piu' grande.
- **Transform tool** (`bb2cb24fc`): selezione raster (rettangolare o **lazo**) → sposta,
  copia/incolla (Cmd+C/V), scala (angoli), ruota (maniglia). Icone tool + cestino.
- **Undo/redo** (`5d3a9018f`): Cmd+Z / Cmd+Shift+Z (snapshot full-canvas, 16 step) su
  tratti, +Row, merge/split e operazioni Transform (uno step per operazione).

### Fixed (inclusi nella release, gia' su master/branch dalle sessioni precedenti)
- Camera unica di scena + anteprime camera-aware (`dbf0f5cc7`).
- Animatic: play prosegue se il video e' piu' lungo dell'audio; play non si ferma sulla
  1a traccia con piu' tracce audio; export non sporca la scena (`7bbadf0fe`, `f28b8f565`).
- Sync Export Animatic ↔ Render Settings (`24d91e076`, `ff456130d`).
- Board: undo del Delete Shot non duplica piu' gli shot vuoti (`aa3b16dea`).
- Persistenza toolbar dei viewer animatic/shot (`ebcf790d5`, `ed25d5df4`).

### Upstream candidates (PR Tahoma)
- **NameModifier vs separatore frame `_N`** in `createNewLevel`: loop infinito quando un
  livello con quel nome esiste gia' su disco (sequenza). Hardening: disambiguare con
  suffisso non-collassabile. Riproducibile su stock creando livelli con nome collidente.

### TODO prossima sessione
- Icone tool: sostituire le disegnate a mano con Lucide/Phosphor (path da fornire).
- Tasto Canc nudo non sempre cattura (focus): per ora cestino + Cmd+Backspace.

## [2026-06-23b] — Camera unica di scena + anteprime camera-aware; Thumbnail room Fase 3 (step 1-2)

### Fixed / Added (master, commit `dbf0f5cc7`)
- **Camera unica di scena** (regressione vs Tahoma). La camera (res+size) ora e' un
  parametro unico per main + tutte le sotto-scene, come in Tahoma nativo.
  - Nuovo `ZtoryShotOps::syncAllCamerasFrom(scene, srcXsh)`: propaga res+size della
    camera appena modificata a TUTTI gli altri xsheet (main + sotto-scene), in
    entrambi i versi. Hook in `CameraSettingsPopup::onChanged` (edit dal main O da
    dentro uno shot). Solo res+size: i keyframe di camera-move restano intatti.
  - `addShotNamed` (startup dialog) ora chiama `syncChildCameraToMain`: gli shot
    creati all'avvio non ereditano piu' una camera di default != camera Preferenze.
- **Anteprime camera-aware** (Board, PDF, track Animatic, story strip). Nuovo helper
  `ZtoryShotOps::cameraAspect(scene)` (= res.lx/res.ly, fallback 16:9) sostituisce
  tutti gli hardcode 16:9. Una camera quadrata mostra inquadrature quadrate ovunque.
  Board re-renderizza le anteprime al cambio di aspect (`onXsheetChanged` +
  `m_lastCameraAspect`); cache thumbnail animatic invalidata su cambio forma.
- **GUI Show/Hide nel viewer ANIMATIC main**: `ZtoryAnimaticViewer` sovrascriveva
  `addShowHideContextMenu`/`updateShowHide` con corpi VUOTI dal primo commit (non era
  `bvp` null come ipotizzato): ora deleghano alla base, il submenu compare. Rimosso
  il log diagnostico temporaneo da `onContextMenu`.

### Added — Thumbnail room, Fase 3 step 1-2 (branch `feature/thumbnail-room`)
- Merge di master nel branch (`5458b8f1e`): l'helper `cameraAspect` disponibile qui.
- **Griglia con aspect camera** (`e063dc724`): le box derivano l'altezza da
  `cameraAspect()` invece del 16:9 fisso (480x270). NB: letto alla creazione del
  canvas, non live (follow-up flaggato come task in background).
- **Step 1 — selezione pannelli** (`29452d1de`): modalita' Select in toolbar, click
  in ordine (= ordine export), overlay arancio + badge numerato, contatore + Clear.
- **Step 2 — rilevamento vuoti** (`70d865ab1`): `isPanelEmpty()` (raster non-bianco,
  soglia 250); i pannelli vuoti non sono selezionabili.

### TODO prossima sessione
- **Thumbnail room Fase 3 step 3 — export-to-board** (il grosso): ritaglio regione
  raster per pannello selezionato → child level (sotto-scena) con livello raster a N
  frame (1 per pannello, ordine di selezione) salvato in `drawings/` → shot reale in
  main xsheet + board + timeline (wiring tipo `addShotNamed`). Decisioni gia' prese:
  selezione → 1 shot multi-panel, pannelli come sequenza di frame, salta i vuoti.
- (opz.) refresh live della griglia thumbnail al cambio camera.

### Decisioni / scelte tecniche
- Cameras sono per-xsheet in Tahoma (`getCurrentCamera()` = camera dell'xsheet
  corrente); `TSceneProperties::m_cameras` e' solo un mirror del main per la
  serializzazione. La coerenza "camera unica" si ottiene sincronizzando gli stage
  object camera, non centralizzando il dato.

## [2026-06-23] — Batch fix segnalazioni utente (undo Delete Shot, persistenza GUI viewer)

### Fixed (su master, pushato)
- **Undo del Delete Shot duplicava tutti gli shot/sotto-scene** (`storyboardpanel.cpp`
  `restoreFromSnapshot`). Il conteggio delle colonne-shot da rimuovere le identificava
  come "colonna con una cella child-level" e si fermava alla prima senza: uno **shot vuoto**
  (solo celle vuote/rosse — stato valido) non ha celle child-level → stop anticipato →
  rimosse troppo poche colonne → il re-insert dello snapshot duplicava tutto dopo lo shot
  vuoto. Ora rimuove tutte le colonne iniziali fino alla prima traccia audio o al primo
  livello reale (non sub-scene). Regression-safe. Commit `aa3b16dea` (+ merge `f542b0830`).
  NB: bug Windows-only nei test utente ma codice identico cross-OS → era data-dependent.
- **Toolbar del viewer di disegno (shot) non persistevano** tra riavvii (a differenza di
  Tahoma). Il viewer è un `ComboViewerPanel` *embedded* (non pannello top-level della room)
  → la room non ne serializza i visible-parts. Ora salvati/ripristinati in QSettings
  (load alla creazione, save su uscita shot-mode + distruttore). **Confermato funzionante.**
  Commit `ed25d5df4`. Stessa persistenza aggiunta al viewer animatic (`ebcf790d5`).
- **`SceneViewer::onContextMenu`**: il pannello viewer ora si trova via match
  `p->sceneViewer()==this` su `QApplication::allWidgets` invece dell'annidatura fissa
  `parentWidget()->parentWidget()` → robusto ai viewer embedded. Candidato upstream. `ebcf790d5`.

### Aperti (prossima sessione — vedi ANIMATIC_TASKS #5/#6/#7)
- **"GUI Show / Hide" non compare nel viewer ANIMATIC main**: `bvp` resta null a runtime
  nonostante il match (binario verificato aggiornato). Serve log diagnostico in `onContextMenu`.
- **#2 Camera unica di scena** (regressione vs Tahoma): la camera deve essere un parametro
  unico per tutta la scena (main + tutte le sub-scene). Modello: `TSceneProperties::m_cameras`
  scena + camere per-sub-xsheet da allineare. Piano in task #6 (sync + hook cambio camera).
- **#1 CRASH new project → selezione cartella (Windows only, no crash log)**: serve stack trace.

## [2026-06-22] — Thumbnail room (Fasi 1-2): canvas MyPaint + griglia continua + palette  [branch `feature/thumbnail-room`]

> Lavoro su branch dedicato `feature/thumbnail-room` (NON master). Milestone "Thumbnail room":
> griglia per schizzare panel veloci, poi export-to-board (Fase 3 da fare).

### Added — Thumbnail room (nuova room/panel)
- **Panel `ZtoryThumbnailPanel`** registrato in `Windows ▸ Ztoryc ▸ Ztoryc Thumbnails`
  (factory + `OpenFloatingPanel` in `tpanels.cpp` + `MI_OpenZtoryThumbnail` + menubar + CMake).
- **Canvas di disegno custom `ZtoryThumbnailCanvas`** che pilota il **motore brush MyPaint
  vero di Tahoma** (`MyPaintToonzBrush` su `TRaster32P`), del tutto **disaccoppiato da
  SceneViewer/TTool**. Brush `.myb` reali caricati via `TMyPaintBrushStyle`; colore→HSV come
  il fullcolor tool; `dtime` reale per le dinamiche. Scelta architetturale chiave: NON
  embeddare un `ComboViewerPanel` (TPanel annidato in TPanel rompe il routing del viewer
  attivo → non si disegna). Decoupling raster = export futuro banale (ritaglio, no slicing).
- **Superficie continua** (un unico raster contiguo per tutta la griglia): gli stroke
  attraversano i confini dei pannelli → **panoramiche orizzontali/verticali** come canvas
  ad hoc. I bordi pannello sono solo overlay (linee sottili).
- **Pan** (tasto centrale / rotella, Shift = orizzontale) e **zoom** (Ctrl+rotella, centrato
  sul cursore). **`+ Row`** ingrandisce la griglia (raster ricopiato preservando il disegno).
- **Palette toolbar**: strumenti con **icone = preview MyPaint del brush** (`_prev.png`):
  Pencil/Brush/Airbrush/Kneaded/Eraser + **`+`** per aggiungere un brush dalla libreria.
  **Colore separato dal brush**: chip blu `#1D5C83`/nero/rosso + **swatch attivo** (più grande,
  bordo marcato, ▾) che apre il color picker. Le gomme dipingono **bianco** sulla carta opaca
  (normale = pieno, gomma pane = bianco a opacità 30% → schiarisce). Slider **Size**.

### Fixed (in-feature)
- `TPanel` è un `TDockWidget`: contenuto installato via `setWidget()` (col layout diretto il
  canvas restava invisibile).
- Link: l'app non linkava `libmypaint` (lo tirava solo `tnztools`) → aggiunto `${MYPAINT_LIB_LDFLAGS}`
  in `toonz/CMakeLists.txt`; aggiunto `tnztools` agli include dir dell'app.
- Brush aggiunto col **+** (path assoluto dal file dialog): `resolveBrushFile` ora passa i path
  assoluti → l'icona `_prev.png` si carica e il brush disegna col proprio stile.

### TODO (prossime sessioni)
- Fase 2 resto: selezione multi-pannello + riordino + aggiungi/rimuovi pannelli.
- Fase 3: **export to board** (riquadri/panoramiche → shot reali in `ZtoryModel` + timeline/board).
- Salvare la palette come **global/studio palette** Tahoma, default anche in modalità normale.
- Possibili "pagine" oltre alle righe per gestire decine di pannelli.

## [2026-06-21b] — Release 0.6.1 (macOS+Windows), fix CI ccache, fix post-release

### Release
- **Pubblicata v0.6.1** (macOS Intel+Silicon DMG + Windows installer/portable), note bilingui IT/EN.
- Bump `ZtorycVersion.cmake` 0.6.0 → 0.6.1. Trattino aggiunto ai **PR candidates** upstream (AGENTS.md).

### CI
- **macOS+Linux: `CC="ccache <compiler>"` rompeva Qt AutoMoc** (`moc_predefs.h`): la ccache
  aggiornata sui runner rifiuta `ccache -std=… clang++` (`invalid option -- t`). Fix: CC/CXX nudi +
  shim ccache nel PATH (macOS `brew --prefix ccache`/libexec; Linux `/usr/lib/ccache`). macOS OK.
  Linux: ccache risolto, resta QXlsx che richiede header privato Qt (`qzipreader_p.h` →
  `qtbase5-private-dev` + tweak CMake) — task a sé.
- Push dei file `.github/workflows/*` richiede scope `workflow` sul token (push manuale utente).

### Fixed
- **Monitor animatic: tracce che glitchavano/sparivano + gap verticale**. `refreshFromScene` usava
  `animaticFrameCount` (include l'audio piazzato) per la larghezza; dopo un razor un ColumnLevel
  audio con endOffset=0 fa restituire a `getVisibleEndFrame` la durata del file raw → minWidth
  abnorme → glitch/gap. Fix: larghezza dai **soli blocchi video** (come `updateTrackWidths`).
- **Rebrand popup render** (`dvdialog.cpp`): `ProgressDialog`/`RadioButtonDialog` usavano
  `tr("Tahoma2D")` come titolo → ora `getAppName()` ("Ztoryc"). Risolve il popup "Finalizing render".
- **Play si fermava alla fine della 1ª traccia audio** con due tracce di lunghezza diversa
  (`getMasterAudioUsecs`): leggeva il `processedUsecs` della PRIMA colonna, che si congela quando
  quella traccia finisce. Fix: usare il **massimo** tra tutte le colonne (il player ancora attivo).
- **Play si fermava a fine audio se il video è più lungo** (`onDrawFrame`): il playback è
  audio-clocked, quindi col video oltre l'audio il playhead si bloccava a fine audio invece di
  arrivare al mark-out. Fix: quando il clock audio è congelato (audio finito) ma il mark-out è
  ancora avanti, avanza sul **wall-clock della FlipConsole** (`m_lastMasterAudioUsecs` rileva se
  l'audio avanza ancora).
- **Export/render marcava la scena come modificata** (`onExportAnimatic`): il ripristino delle
  output properties post-render chiamava `notifySceneChanged()` (default `setDirty=true`) → asterisco
  spurio dopo ogni export. Fix: `notifySceneChanged(false)` (ripristino net-zero, niente dirty).
  NOTA: resta aperto un bug intermittente non riprodotto — asterisco che non si pulisce sul MAIN
  mentre si lavora sull'animatic (in sotto-scena ok). Da indagare con repro o log su setDirtyFlag.

### Changed
- **Export Animatic ↔ Render Settings: sync bidirezionale live** (`onExportAnimatic`).
  Output folder/Filename ↔ Save in/Name (poll non-distruttivo + write-back con `code/decodeFilePath`
  per l'alias `+outputs`); nota Format ed estensione filename aggiornate live; il "…" scrive in
  `prop` (il poll non riverte) + rialza il dialog (z-order macOS) e usa la label **"Choose"**;
  `notifySceneChanged()` rinfresca il popup nativo in tempo reale (Export→Render).



### Added
- **Riconoscimento sequenze con trattino** (`frame-0006.jpeg`). Aggiunto `-` come separatore
  frame alla pari di `.`/`_` nel parser core (`tfilepath.cpp`): helper `rfindFrameSep()` usato
  in read (getDots/getSepChar/getFrame/getWideName/getLevelNameW) e write (withName/withFrame,
  che preserva il `-` nel round-trip) + regex `analyzePath`. La guardia "solo cifre tra
  separatore ed estensione" rende sicuro `my-file.jpg` (resta singolo). **Candidato PR upstream.**
- **Modalità rilevamento sequenze (Automatic / Sequence / Individual frames)** stile DaVinci.
  Preferenza globale `numberedFilesImportMode` (tab Loading) + override per-caricamento nei popup
  **Load Level** e **Import Assets** (helper riusabile `FileBrowserPopup::createNumberedFilesModeCombo`).
  `FileBrowser::SequenceMode` pilota il grouping in `refreshCurrentFolderItems` (Automatic =
  raggruppa ma demota i singleton a still; Individual = ogni file separato).
- **Board "Compact view"** (toggle in toolbar, icona `ztoryc_compact_view` stacked-cards): una card
  per shot (mostra il panel corrente) con frecce ◀ ▶ per navigare i panel in place. Allevia il Board
  con scene animate (un panel per keyframe). `rebuildGrid` collassato + swap della singola cella su
  nav (no rebuild completo); `updateVisiblePreviews` salta i panel nascosti. Stato persistito
  (`TEnv ZtoryBoardCollapsePanels`).

### Fixed
- **Animatic: il play si fermava prima della fine / audio muto su shot vuoti** (`ztoryanimatic.cpp`).
  Due cause: (1) il playback è audio-clocked e la cache audio per-colonna è tagliata a
  `videoFrameCount`, ma `soundColumnsFingerprint` guardava solo i sound column → allungando il video
  (Load nelle sub-scene) la cache restava stale (play fermo a 828). Fix: includere la lunghezza nel
  fingerprint. (2) timeline = solo video → audio oltre l'ultimo shot non suonava. Fix: nuovo
  `animaticFrameCount = max(videoFrameCount, audioPlacedFrameCount)` (placement via `getRange` sui
  sound column, non il file raw) usato per cap audio, play range, FlipConsole range, stop/markOut,
  larghezza track. Montaggio audio-led ora riproduce tutto l'audio.
- **CRASH su Windows esportando lo Spreadsheet** (`storyboardpanel.cpp` `onExportSpreadsheet`).
  `xlsx.sheetNames().first()` su `QXlsx::Document` appena costruito: nessun foglio finché non si
  scrive → QList vuota → `.first()` UB → `EXCEPTION_ACCESS_VIOLATION` in `renameSheet`/`operator==`
  (innocuo su macOS per la QString nulla condivisa). Fix: guardia — rename del foglio default se
  presente, altrimenti `addSheet(overviewName)`.
- **Undo dello split shot perdeva i disegni della seconda metà** (`ztoryanimatic.cpp`
  `onRazorRequested`). Lo split muta `origCL` in place (`trimChildXsheetTo`), ma lo snapshot
  `UndoBoardState` tiene solo un PUNTATORE al child level → l'undo riesponeva `origCL` già trimmato.
  Fix: clonare `origCL` in un livello backup pristine PRIMA del trim (via `cloneChild`), orfanare la
  colonna (il `TXshLevelP` lo tiene vivo) e ripuntare lo snapshot `before` sul backup → undo lossless.
- **Compact view: anteprime non aggiornate dopo undo/redo** (`storyboardpanel.cpp`
  `restoreFromSnapshot`). Mancava il refresh deferito post-rebuild. Fix: `onRefreshPreviews` via
  `singleShot(0)` dopo `rebuildGrid()` (vale per qualunque undo: reorder/split/delete…).

### Changed
- **Razor audio dentro lo shot**: `onAudioRazorRequested` ora funziona anche con una sub-scena aperta
  (opera sempre su `mainXsheet()`; dentro una sub-scena usa `notifyCastChange` invece di
  `notifyXsheetChanged` per non innescare cascate dal contesto sbagliato). Comodo per gli spostamenti
  della guida audio senza uscire dallo shot. Il drag dei segmenti audio già funzionava. Razor degli
  shot (video) lasciato gated.

### Notes
- Build dir Ninja = ROOT del workspace (non `toonz/build`, che non esiste più e fa fallire ninja in
  silenzio). Per i check rapidi: `ninja -C /Volumes/ZioSam/tahoma2d-workspace/tahoma2d`.

## [2026-06-21] — Fix crash maniglia "Drawing #" Animate tool (bug upstream)

### Fixed
- **CRASH trascinando la maniglia "Drawing #" dell'Animate tool** (`tnztools/edittool.cpp`,
  `DragDrawingNumberTool::leftButtonDrag`). Root cause trovata con lldb sul debug build:
  il tool registra UN solo canale (`DragChannelTool(T_DrawingNumber, false)` -> `m_channels.size()==1`)
  ma il drag usava l'API a due canali `setValues(v0, getOldValue(1)+delta.y*factor)` (copiata dal tool
  di posizione), accedendo a `m_channels[1]` inesistente. Debug: `assert` in
  `TStageObjectValues::getValue` (stageobjectutil.cpp:147). Release: OOB read+write -> corruzione heap
  -> trap `malloc` (i crash log erano mal-simbolicati in overlay GL/Qt, fuorvianti). Scattava al primo
  micro-movimento del mouse sulla maniglia. Fix: usare l'API a canale singolo `setValue(v0)`
  (solo il drag orizzontale cambia il drawing number).
- **VERIFICATO su Tahoma2D stock** -> bug upstream (feature PR #2124, merge `67f0ef7f4`), NON regressione
  Ztoryc. Candidato PR upstream ad alto impatto (vedi AGENTS.md).

### Added / Changed (Animate tool, stessa sessione)
- **Drawing # — comportamento corretto**: il riferimento del numero di disegno viene letto dalla
  CELLA (non dal param `T_DrawingNumber`, che e' 0 senza keyframe). Un click secco o un micro-drag
  non cambia nulla e non aggiunge keyframe; la chiave nasce solo quando il drag passa davvero a un
  altro disegno. Clamp minimo a **1** (0 = cella vuota). Allineato al campo Drawing# di tool options.
- **Maniglia "Drawing #" nascosta di default** (`m_showDrawingNumber`, env `EditToolShowDrawingNumber=0`):
  gate su disegno **+** `glPushName` -> sparisce anche dal pick (non cliccabile per sbaglio).
  Riattivabile dalle opzioni tool. Nota: upstream non collegava affatto la flag al disegno.
- **Gizmo Animate adattivo allo sfondo (ripristinato)**: `sampleBgColor` (glReadPixels al centro) +
  `gizmoContrastColor` (complementare a luminanza forzata; b/n su sfondi neutri); highlight del device
  in hover sempre vivido a luminosita' media, leggibile sia su gizmo chiaro che scuro. Era stato tolto
  per errore col revert anti-crash; il crash era il Drawing#, non questo.
- **Animate tool disabilitato nel viewer animatic** (`ZtoryAnimaticViewer::eventFilter` sul SceneViewer):
  ingoia click/drag sinistro e tablet quando il tool attivo e' `T_Edit` -> niente trasformazioni nella
  vista timing/preview; pan, zoom e hover restano attivi. Hint al primo click.

## [2026-06-20c] — Tema chiaro "Abete" (WIP) + Animate tool: gizmo leggibile su ogni sfondo

### Added (WIP — tema Abete)
- **Tema chiaro "Abete" (legno caldo)** — `stuff/config/qss/Abete/Abete.qss`, generato da
  Light.qss: neutri tintati su rampa legno per luminanza, selezione magenta -> ambra/oro,
  semantici (rosso/arancio/blu/verde) invariati. Auto-scoperto (Preferences > Interface > Style).
- **`ztorytheme.h`** — palette centralizzata header-only per i pannelli custom: `isLight()`
  (legge il tema attivo), `wood(lum)` rampa abete, `g(v[,a])` grigio neutro che segue il tema
  (dark: grigio originale; light: inversione di luminanza sul legno), `activeShot()`/`accent()`.
- **Strato 2 (timeline animatic)** — `ztoryanimatic.cpp`: 32 grigi neutri ora via `ZtoryTheme::g()`,
  testo bianco -> testo-tema; scrim "muted"/chip marker restano scuri in entrambi i temi.
  Shot attivo: oro logo `#ECA61C` in Abete, **magenta `#E0249B` mantenuto nei temi scuri**.
- STATO: **WIP**. L'oro su legno e la resa generale vanno ancora rifiniti; il Board (storyboardpanel,
  stylesheet-based) non e' ancora tematizzato. Default invariato (Dark).

### Added (Animate tool — leggibilita' gizmo)
- **Colore gizmo adattivo allo sfondo** (`tnztools/edittool.cpp/.h`) — `sampleBgColor()` legge
  via `glReadPixels` il colore medio sotto il centro del gizmo; `gizmoContrastColor()` colora il
  manipolatore col **complementare a luminanza forzata** (fondo chiaro -> gizmo scuro e viceversa;
  fondo neutro -> bianco/nero). Si aggiorna mentre si sposta il gizmo. Risolve il gizmo invisibile
  su disegni di colore simile.
- **Highlight device sempre distinguibile** — l'elemento in hover (centro/estremi bracci) usa un
  colore **vivido a luminosita' media** con tinta nettamente diversa dal normale, leggibile sia
  quando il gizmo e' chiaro sia quando e' scuro.
- Implementata prima come "casing" a doppio tratto (alone), poi sostituita dalla versione a colore
  adattivo su richiesta utente; l'infrastruttura casing resta nel codice ma disabilitata.
- **REVERT (2026-06-21)**: `edittool.cpp/.h` ripristinati all'originale (gizmo fisso rosa/verde). La `glReadPixels` durante l'overlay corrompe il primitive-buffer immediate-mode su macOS (Metal-GL) -> crash non deterministico in `glEnd` (visto toccando il braccio Drawing Number, che disegna testo extra). Da rifare con sampling crash-safe (lettura dal raster dell'immagine, non dal framebuffer live, come fa lo style picker in un pass separato).

### Upstream candidates
- **Gizmo Animate tool adattivo allo sfondo** (`tnztools/edittool.cpp`) — codice core Tahoma2D,
  il gizmo invisibile su sfondi di colore simile affligge tutti gli utenti. Candidato PR upstream.

## [2026-06-20b] — Task 55: altezza tracce video/audio regolabile

### Added
- **Import audio dall'interno di uno shot** (`ztoryanimatic.cpp`, menu contestuale
  timeline). "Load Audio..." e "Add Audio Track" non chiedono piu' di tornare
  all'Animatic: se sei dentro una sub-scena chiudono lo shot (`while ancestorCount>0
  -> MI_CloseChild`) e operano sul main xsheet, coerente col pattern delle op
  strutturali del Task 53. Rispetta la regola "audio solo nel main xsheet".
- **Resize verticale delle tracce della timeline animatic** (`ztoryanimatic.h/.cpp`).
  Handle sul bordo inferiore di ogni traccia (striscia di `kZtoryResizeGrip=5px`,
  riservata), drag -> ridimensiona; cursore `SizeVerCursor` in hover. Limiti
  condivisi `kZtoryMinTrackH=24` / `kZtoryMaxTrackH=120`. Grip visivo (separatore +
  tratto centrato) disegnato in fondo a entrambe le tracce.
  - **Video track** (`ZtoryAnimaticTrack`): nuovo `m_trackHeight` (default 80) +
    `setTrackHeight()` con clamp; il `paintEvent` scalava gia' con `height()` ->
    thumbnail si adattano da sole. `DragMode::Resize`.
  - **Audio track** (`ZtoryAudioTrack`): `setTrackHeight()` ora fa clamp + no-op
    guard; la waveform (`m_waveformDirty`) si rigenera alla nuova altezza senza
    ricalcolo completo. `DragMode::Resize`.
- **Persistenza globale** via `QSettings` (`Ztoryc/VideoTrackHeight`,
  `Ztoryc/AudioTrackHeight`) - pattern coerente con `Ztoryc/LightColor`. Altezza
  audio **condivisa**: resize di una traccia le ridimensiona tutte e persiste.
- **Densita' label audio progressiva**: a scalare sparisce prima il nome file
  (`<kAudioShowNameMinH=45`), poi anche la barra del volume (`<kAudioShowVolMinH=36`);
  al minimo restano solo i bottoni L/M/S. Hit-test del volume disattivato quando la
  barra e' nascosta (no drag fantasma).

### Fixed
- **Cursore resize sull'audio track non appariva in hover**: l'audio gestisce il
  cursore via `QEvent::HoverMove` in `event()` (WA_Hover, piu' affidabile di
  `setMouseTracking` dentro `QScrollArea` su macOS), non via `mouseMoveEvent`.
  Aggiunto il check del grip li', con priorita' sul cursore di trim dei bordi.
- **Nome shot nella traccia video** ora centrato verticalmente (`AlignVCenter`
  invece di `AlignBottom`) -> resta leggibile a qualsiasi altezza traccia.
- **Label timecode che si ammucchiavano in zoom**: `kMinLabelPx` era fisso a 40px
  (ok per i numeri di frame, troppo stretto per `MM:SS:FF`/`H:MM:SS:FF`). Ora misurato
  con `QFontMetrics` sulla label piu' larga visibile (frame piu' a destra) + 14px gap ->
  l'intervallo tra label (serie 1/2/5/10/25/50...) si dirada da solo. Formatter `fmtFrame`
  spostato prima del calcolo dello spacing.

## [2026-06-20] — Export to Spreadsheet (production tracking Kitsu-aligned, XLSX+CSV)

### Added
- **Matrice tecnica→task nel modello** (`ztorymodel.h/.cpp`): `TaskStatus` (TODO/READY/WIP/WFA/
  RETAKE/DONE), `TaskState{status,assignee}`, `Technique{name,taskTypes}`. `ShotData` +=
  `technique`, `QMap<QString,TaskState> tasks`, `notes`, `vfxNotes`. ZtoryModel += `m_episode`,
  preset tecniche editabili + helper. Persistito nel `.ztoryc` (path attivo
  `StoryboardPanel::saveZtoryc/loadZtoryc`): `<project>` episode/defaultTechnique, blocco
  `<techniques>`, per-shot `technique` + `<task>` + `<shotNotes>`/`<shotVfxNotes>`.
- **Tecniche preset** (Kitsu-aligned, per-shot, editabili): Tradigital, Traditional (scan&clean/
  ink&paint/x-sheet), Cut-out, 3D/CGI, Stop-motion (Set-up→…→Rig Removal), Generic (tutti i task),
  Live (ridotto). Bottone toolbar Board **Set Technique** (multi-selezione) + colonna Workflow.
- **Export Spreadsheet XLSX** (`onExportSpreadsheet`) via **QXlsx 1.5.1 vendorato** (MIT,
  `thirdparty/QXlsx`, CMakeLists minimale Qt5): foglio **All Shots** (tutti gli shot, unione task,
  N/A grigio dove non pertinente) + **uno sheet per tecnica usata** (solo i suoi task). Per riga:
  thumbnail primo panel, seq, shot, frames, Sec/Fr (durata), Workflow, Notes (dal primo panel),
  VFX Notes; per ogni task colonna status (colore Kitsu via **conditional formatting** che segue
  il valore) + assegnatario. Dropdown status + **autofilter** (header).
- **Export Spreadsheet CSV** (`onExportSpreadsheetCsv`) per import in sistemi di production mgmt.
- **Menu File ▸ Export ▸ Ztoryc**: Storyboard PDF, Spreadsheet XLSX, Spreadsheet CSV, Shots/Scenes,
  Animatic (comandi globali che instradano al Board).
- **Campo Episode** in creazione scena (`startuppopup`) + dialog/testata PDF.

### Fixed (patch su QXlsx vendorato — cercare "Ztoryc:" nei commenti)
- Conditional formatting: le regole via API non avevano `priority` (tutte 0 → Excel/LibreOffice
  ne tenevano una sola). Ora `priority=i+1` nel saveToXml.
- Aggiunto `Worksheet::setAutoFilter()` (QXlsx non aveva autofilter) + emit `<autoFilter>` nell'ordine
  schema corretto, + defined-name `_xlnm._FilterDatabase` per LibreOffice.
- Thumbnail: `insertImage` è 0-based (vs `write()` 1-based) → immagini shiftate; fix `insertImage(row-1,0)`
  + DPI 96 pinnata (renderXsheetFrame può tornare immagine retina 2×).

### Notes
- Terminologia: rinominato "worksheet"→"spreadsheet" in tutto il codice Ztoryc (non nei token QXlsx).
- Limite: cambiare Workflow nel foglio non aggiorna gli N/A (export statico) → Workflow reso read-only;
  la tecnica si setta in-app. I colori status invece sono dinamici (CF).
- **DA FARE prossima sessione** (richiesto a fine 2026-06-20): (1) tecnica di default nelle opzioni
  di creazione scena; (2) voce "Storyboard Settings" nel menu per editare produzione/episodio/titolo/
  tecnica/numbering dopo la creazione. Dettagli in memory `project_worksheet_export`.
- Tecniche riviste = solo su scene NUOVE (le scene esistenti hanno i preset salvati nel .ztoryc).

## [2026-06-19b] — Task 53 shot ops in edit-shot mode + Task 54 logo custom PDF + footer branding

### Added
- **Task 53 — Shot ops dall'interno di una sub-scena (edit-shot mode)** (`ztoryanimatic.cpp`).
  Le 6 shot ops dell'Animatic (Copy/Clone/Cut/Paste/Delete/Merge) erano bloccate silenziosamente
  da `if (!ZtoryModel::assertMainXsheet(false)) return;` quando si era dentro uno shot. Rimosso il
  guard e garantito che ogni op operi sul main xsheet:
  - **Copy/Clone** (non distruttivi): leggono da `scene->getChildStack()->getTopXsheet()` senza
    chiudere la sub-scena → l'utente resta in edit-mode.
  - **Cut/Paste/Delete/Merge** (strutturali): chiudono la sub-scena (`while ancestorCount>0 →
    MI_CloseChild`) poi operano sul main, esattamente come fanno già le op del Board. Cut/Paste
    avevano già il loop di chiusura (era solo dopo l'assert, irraggiungibile); a Delete aggiunto
    il loop; Merge usava già `getTopXsheet()`, aggiunto il close-children + rimosso l'assert.
- **Task 54 — Logo custom + metadata nell'export PDF** (`storyboardpanel.cpp`, `ztorymodel.h`).
  Premendo Export PDF appare un dialog "Export Storyboard PDF" con:
  - **Production** e **Title** editabili (prima nel modello ma senza UI) → finiscono nell'header.
  - **Header logo**: campo path + Browse (PNG/SVG/JPG/BMP) + Clear, checkbox "No logo (clean
    export)", **preview live** con warning ⚠ se il file non esiste.
  - Scelte persistite per-progetto nel `.ztoryc` (`<project>` attr `pdfLogo`/`pdfNoLogo`),
    ricaricate alla riapertura scena. Nuovi campi modello `m_pdfLogoPath`/`m_pdfNoLogo` +
    accessor; helper `resolvePdfLogoFile()` (assoluto o relativo alla cartella scena).
  - Logo header: custom valido → custom; rotto → fallback al logo Ztoryc (export non si rompe mai);
    "No logo" → header pulito.
- **Footer PDF branding sempre presente** (richiesta utente): il footer "Made with Ztoryc" + logo
  Ztoryc resta **indipendente** dalla scelta header (logo footer hardcoded su `ztoryc_about.png`).
  Aggiunto link al repo: `Made with Ztoryc  ·  github.com/matitanimata/ztoryc` (URL in colore link;
  risulta cliccabile nei viewer PDF testati dall'utente).

### Fixed
- **Anteprime Board non rigenerate dopo shot ops dall'Animatic** finché non si scrollava (il fix
  del 2026-06-19 copriva solo le op nate dal Board). Causa: quando l'op nasce dall'Animatic il
  Board non riceve un ciclo di paint/layout, quindi al `singleShot(0)` il `QGridLayout` non era
  ancora ricalcolato → `mapTo()` su geometrie stale → test viewport-intersect fallisce → niente
  render. Fix in `updateVisiblePreviews()`: `m_grid->activate()` forza il flush del layout
  pendente prima del test geometrico. Punto unico, copre tutti i chiamanti.

### Notes
- **Prossimo**: task 55 (altezza tracce video/audio regolabile) → 56 (Thumbnail Room, milestone).

## [2026-06-19] — Toolbar dedup Board↔Animatic + overflow scroll + UI polish + ottimizzazione task list

### Added
- **Dedup toolbar Board↔Animatic a livello UI** (commit `d194149ad`, su master). Riuscito dove
  il tentativo del 2026-06-17 era fallito, rispettando i vincoli della memoria: SOLO `setVisible`,
  niente `QToolBar`, niente hook su `showEvent`, niente delega add/delete cross-panel.
  - `MainWindow::updateZtoryToolbarDedup()` nasconde sul Board i 6 shot-ops
    (add/delete/merge/copy/clone/paste) + i 2 export (Shots/Animatic) quando un `ZtoryAnimaticPanel`
    è nella stessa room. Pilotato da `onCurrentRoomChanged`/`switchToRoom`/costruttore via
    `QTimer::singleShot(0)`, MAI dal `showEvent` del Board. Room col solo Board → tutto visibile
    (self-sufficient).
  - Toolbar Animatic riordinata: shot-ops a SINISTRA (sotto il Board), tool editing
    (fit/select/trim/razor/link) a destra.
  - **Export spostati sulla timeline** (spazio libero): Export Shots/Scene + Export Animatic
    sull'Animatic a destra, delegano al Board sibling; PDF resta sul Board.
- **Overflow scroll sulla toolbar del Board** via `DvScrollWidget` nativo di Tahoma: frecce
  laterali quando le icone non c'entrano, invece di tagliarle/ammucchiarle (stessa UX dei pannelli
  nativi). Rimosso l'`addStretch()` interno che avrebbe assorbito lo spazio.
- **Icone nuove**: `ztoryc_fit_all` (arrows-maximize) e `ztoryc_zoom` (lente). Fit All riportato
  accanto allo zoom slider; etichetta "Zoom:" sostituita dalla lente.

### Modified
- **Tracce video/audio**: Lock con icona lucchetto aperto/chiuso (`ztoryc_lock`/`_on`); Mute con
  icona speaker nativa di Tahoma (`sound_on`/`sound`); Solo resta "S" (paint dei track, non più
  drawText "L"/"M").
- **Show light direction + Show camera movement**: ora hanno lo stato `:checked` (background #666)
  come razor/select, sia Board che Animatic.

### Fixed
- **Anteprime Board non rigenerate dopo add/copy/paste/delete/cut/merge** finché non si scrollava.
  `refreshFromScene()` ricostruisce con thumbnail vuote (lazy by design per non freezare al load);
  ora in `onModelResequenced` (ramo count-cambiato, solo path operazione) un `singleShot(0)` →
  `updateVisiblePreviews()` rigenera SOLO i pannelli nel viewport (salta quelli già renderizzati),
  istantaneo e senza appesantire. Il path di caricamento scena resta lazy.

### Docs
- **ANIMATIC_TASKS.md ottimizzato**: 1213 → 611 righe. Pregresso (614 righe, verbatim) spostato in
  `ANIMATIC_TASKS_ARCHIVE_2026-05.md` (sezione giugno 2026): spec dei task DONE rimaste tra gli
  "aperti" (39,40,42,43,45,48,49,50, mark-out, ffmpeg, pdf-thumb, audio/volume/transizioni/startup/
  nav-tags), task 35/36/37 (assorbiti in 40), keys-cels 1-4, icon migration, blocco FATTO/DECLASSATO
  (51,52,StudioPalette). Tabella DONE aggiornata con 11 voci giugno (incl. `ICON-MIGRATION`).
  Regola adottata: completato → riga in tabella DONE + spec in archivio (basta voci `✅ FATTO`
  accumulate nella sezione attiva).

### Notes
- Branch `feat/toolbar-dedup-board-animatic` mergiato in `master` (fast-forward) e pushato su origin.
- **Prossimo**: task 53 (shot ops in edit-shot mode, MEDIA-ALTA) → 54 → 55, e valutare 57. Task 56
  (Thumbnail Room) è lavoro a parte (milestone).

## [2026-06-17] — Wiring icone toolbar + fix add-shot xsheetColumn

### Added
- **Wiring icone ai 5 toggle** (commit `ef833e133`): snap (`m_snapBtn`, magnet/magnet-off —
  base = off, `_on` = on, `createQIcon` carica auto la variante `_on` per lo stato checked);
  light_arrow (bottone "Show light arrows" "L", sia Board che Animatic); cam_moves (Board "Trk");
  keys_follow (`MI_ToggleKeyframesFollowExposure`, era `segment_linked`); listen_audio
  (`MI_ToggleMainAudio`, era `sub_main_audio`). Rimosso il fondo colorato `:checked` da cam_moves
  e light_arrow.
- Rimossi file morti icone (`slip`, `onion`/`_on`, `shotedit`/`_on`, `snap_off`) + righe qrc;
  aggiunto `ztoryc_snap_on`.

### Fixed
- **`onAddShot` non aggiornava `xsheetColumn` degli shot successivi** (commit `ea1ab3490`).
  Inserendo uno shot in mezzo, la colonna inserita spostava a destra gli shot seguenti
  nell'xsheet ma `data.xsheetColumn` non veniva incrementato (a differenza di `onDeleteShot`
  che lo tiene sempre allineato) → cliccando uno shot dopo l'inserimento si apriva la
  sotto-scena sbagliata (es. ultimo → penultimo). **Da verificare:** l'utente conferma che la
  **v0.5 NON ha il baco**, pur avendo `onAddShot` byte-identico al tag → il trigger reale
  coinvolge un'interazione con una modifica post-v0.5 ancora **non tracciata**. Il fix è
  comunque corretto (allinea `onAddShot` a `onDeleteShot`). ⚠️ Indagare l'origine prima di
  considerarlo chiuso.

### Reverted / Notes
- **Dedup toolbar Board↔Animatic + delega add/delete dall'Animatic al Board: TENTATI e ANNULLATI**
  (regressioni: numero panel sballato). Tutto revertito. Lezione: il dedup deve toccare SOLO
  `setVisible` dei bottoni — mai convertire il container a `QToolBar`, mai agganciarsi a
  `showEvent`/timer. Vedi memoria `project_toolbar_dedup_ui`.
- **Trappola single-instance:** i sintomi "catastrofici" osservati durante le prove (disegni
  spariti, scene nuove rotte, 030→020) erano il **binario vecchio bacato ancora in esecuzione**
  (single-instance guard rifocalizza invece di riavviare); codice/binario erano già puliti
  (verificato con `nm`). Regola: dopo ogni deploy **Cmd+Q + riapri**. Vedi memoria
  `feedback_deploy_single_instance_restart`.
- `onMergeShots`/`onMergeWithNext` Animatic: NON toccati (regola AGENTS "già corrette").

## [2026-06-16b] — Increase/Decrease chiavi + refactor selezione combinata

### Fixed
- **Pannello-fantasma da 1 frame nel paste (e ricalcoli durata).** Lo Stop Frame Hold
  che `resequenceXsheet()` mette in coda a ogni colonna shot è una cella non vuota
  (`isEmpty()` controlla solo `m_level`) e veniva contato come frame: `refreshFromScene`,
  `detectAndUpdatePanels` (entrambi i rami), `onXsheetChanged`, `onShotInserted`
  gonfiavano la durata di +1 → il filtro di visibilità pannelli (`f < timelineDuration`)
  lasciava passare un boundary → pannello-fantasma. Fix: escludere la SFH dal conteggio
  (`getRange(..., ignoreLastStop=true)` / break sullo stop-frame), allineati a
  `onModelResequenced`. File `storyboardpanel.cpp`.

### Added
- **Increase/Decrease spaziatura chiavi (key-only)** su `TKeyframeSelection`
  (`keyframeselection.cpp`): aggiunge/toglie un frame per ogni intervallo tra chiavi
  consecutive; prima chiave ancorata, chiavi successive che **slittano** (ripple via
  `moveKeyframes`); Decrease no-op se un gap è già 1; blocco riselezionato dopo
  l'operazione (ripetibile). Agganciato a MI_IncreaseStep/MI_DecreaseStep. Undo
  `KeyframeSpaceUndo`.
- **Increase/Decrease combinato celle+chiavi** (`cellkeyframeselection.cpp`): in
  modalità combinata NON ripete i disegni — inserisce/rimuove un frame negli intervalli
  tra le chiavi via `insertCells`/`removeCells` (celle+chiavi slittano insieme,
  pref-independent). Single-point su una cella sola → un frame nel punto selezionato;
  sul primo frame del disegno il disegno cresce in testa (no frame vuoto). Undo
  `KeyframeGapResizeUndo` con snapshot completo per colonna.

### Refactor
- **`TCellKeyframeSelection` ora EREDITA `TCellSelection`** (tolto `final`). Con
  keys-follow ON ogni selezione celle diventava la selezione combinata che NON era una
  `TCellSelection` → gli ~82 `dynamic_cast<TCellSelection*>` fallivano e i comandi cella
  (drawing substitution W/Q, filtri, reframe…) ricadevano sulla singola cella. Ora il
  cast riesce → operano sull'intero blocco. La combinata È la cell selection (possiede il
  range); ctor prende solo `TKeyframeSelection*`. Ogni override (copy/cut/clear/paste,
  increase/decrease) ha fall-through al comportamento cella base quando non ci sono chiavi
  (o keys-follow OFF) → modalità normale invariata. Aggiornati `xsheetviewer.cpp` e
  `cellselectioncommand.cpp` (ctor).

### Notes / TODO
- Aggiunto **task PRIORITARIO** in ANIMATIC_TASKS.md: finalizzazione UI dedup toolbar
  Board↔Animatic (bottoni condivisi su timeline Animatic sinistra, Board liberato per
  auto/keep/renumber + numbering + light arrow + export; nascondere duplicati a runtime
  via `findChildren<TPanel*>`, panel sempre self-sufficient).
- Task 5 (drag sui diamanti → selezione combinata quando pref ON) ancora aperto; il
  refactor di oggi è infrastruttura utile in quella direzione.

## [2026-06-16] — Dedup comandi shot Board↔Timeline (ZtoryShotOps) + cleanup UI

### Added
- **`ztoryshotops.{h,cpp}` — modulo condiviso `ZtoryShotOps`** con la logica xsheet
  pura dei comandi shot: `syncChildCameraToMain`, `cloneChildToPosition`,
  `pasteSharedClip`, `colDuration`. Prima erano duplicate come static file-local in
  `storyboardpanel.cpp` e `ztoryanimatic.cpp` e si erano **disallineate** (il clone
  del Board usava `storeObjects(ids)`+`setDagNodePos`, quello dell'Animatic
  `storeColumns(indices)` senza dagNodePos). Canonico = versione Board (battle-tested).
  Entrambi i panel ora delegano. Net Step 1: -314/+245.

### Modified
- **Board: clipboard shot ora SOLO `ZtoryModel::sharedClip()`** — rimosso il membro
  locale `m_clipboard` + struct `ClipboardEntry` e il ramo "legacy" di `onPasteShot`
  (127 righe). `onCopy/onCut/onCloneShot` scrivono solo lo shared clip; `onPasteShot`
  delega a `ZtoryShotOps::pasteSharedClip` + resequence + refreshFromScene +
  `UndoBoardState`, identico a `onPasteShots` dell'Animatic. Net Step 2: -230/+63.
  **Cambio di consistenza voluto:** un secondo Paste di un Cut non duplica piu (Board
  allineato all'Animatic).
- **Rimosso il chip blu "ANIMATIC"** dalla toolbar della timeline (coerente con la
  pulizia dei chip decorativi; toolbar ora parte da "Zoom:").
- **Main Toolbar nascosta di default** (`ShowMainToolbarAction` 1->0) - la barra
  window-top (new level / Reframe 1's-2's-3's / audio) duplicava la QuickToolbar
  dell'xsheet e ingombrava le room storyboard. Resta riattivabile da
  View > Show Main Toolbar. La QuickToolbar dell'xsheet/timeline e invariata.
- **README**: rimosso "Brush feel" dalla Roadmap (declassato).

### Verified (a video)
- copy/cut/clone/paste same-panel (Board): Copy→Paste→Paste (copia persiste),
  Cut→Paste→Paste (2o no-op); cross-panel Board→Animatic e Animatic→Board (Cmd+C/Cmd+V);
  clone Animatic (chiavi camera + colonne preservate); undo; sync Board↔Animatic.

### Notes / TODO
- **Bug pre-esistente (NON il dedup):** un paste puo lasciare un pannello-fantasma da
  1 frame su uno shot vicino (`refreshFromScene`/`resequence` fa crescere la colonna).
  Verificato dal diff: non ho toccato quella logica e il paste normale gia passava da
  `refreshFromScene` su master → si riproduce anche su master. Da fixare a parte.
- **TODO UI:** togliere la toolbar finestra "nuovo livello/1's2's3's/repeat" in Animatic
  e de-doppiarla in Shot editing (file `room*.ini` in bundle + ~/Library; le room nel
  bundle hanno nomi default che non corrispondono a Ztoryc X/T → identificare i file
  giusti prima).

## [2026-06-15c] — Time Stretch combinato celle+chiavi + UI cleanup titoli panel

### Added
- **Time Stretch COMBINATO celle+chiavi (keys-follow ON)** — completa il gruppo
  ritempi rimandato. Selezionando un blocco di celle con keyframe (modalità
  "Keyframes Follow Exposure"), il Time Stretch ora ritempra **sia le celle sia le
  chiavi** in modo proporzionale (caso walk cycle 18↔24). Nuova
  `CellAndKeyframeStretchUndo` (`timestretchpopup.cpp`) che **compone**
  `TimeStretchUndo` per le celle e gestisce le chiavi esplicitamente: si lascia che
  `xsh->timeStretch` rippli tutto (corretto per le chiavi *sotto* il blocco), poi si
  sovrascrivono **solo le chiavi del blocco** `[r0,r1]` con il rimappamento
  proporzionale (estremi ancorati). Niente doppio-handling (classe BUG-2). Undo/redo
  atomici celle+chiavi. Snapshot del blocco preso PRIMA dello stretch.

### Fixed
- **`Old Range 0` nel Time Stretch con celle+chiavi selezionate.** Quando si
  selezionano celle che contengono keyframe la selezione corrente è una
  `TCellKeyframeSelection` (wrapper nativo Tahoma con dentro una cell-selection +
  una keyframe-selection), non una `TCellSelection`. Il `TimeStretchPopup` faceva un
  `dynamic_cast<TCellSelection>` che falliva → range 0 e nessuno stretch.
  **Fix**: helper `asCellSelection()` che spacchetta il wrapper alla cell-selection
  interna, usato in `updateValues` (display) e nella free function `timeStretch`
  (range + colonne + trigger del path combinato). Diagnosi con logging mirato su
  stderr (typeid della selezione), poi rimosso.

### Modified
- **UI cleanup — titoli ridondanti nei panel compositi.** Rimosso il chip verde
  decorativo "BOARD" (`storyboardpanel.cpp`) e azzerato il window title nei 3 panel
  con switcher — `ZtoryRightPanel` (SCRIPT|PALETTE), `ZtoryLeftPanel` (BOARD|XSHEET),
  `ZtoryDrawLeftPanel` (BOARD/SHOT) — sia nel costruttore che nelle factory
  (`ztoryanimatic.cpp`). Lo switcher row già etichetta il panel; la barra del titolo
  resta come drag-handle per il docking (vincolo autosufficienza room custom). I
  panel a scopo singolo (Animatic/Viewer/StoryStrip/Navigator) non toccati.

### Notes / TODO
- **Selezione combinata governata dal link "Keyframes Follow Exposure" [DESIGN, da fare].**
  Modello richiesto dall'utente: con pref ON, *qualsiasi* selezione (incluso il drag
  sui diamanti) deve produrre una `TCellKeyframeSelection` (chiavi + celle
  sottostanti); con pref OFF, selezione indipendente (chiavi isolabili). Oggi metà
  funziona già (selezione celle → combinata); manca l'altro verso (selezione dai
  diamanti → combinata quando pref ON). Cambio trasversale alla logica di selezione
  dell'xsheet (xsheetviewer/cell viewer), impatta TUTTI i comandi combinati → da fare
  e testare sull'intero repertorio.
- **Dedup comandi Board↔Timeline [refactor] ora sbloccato** (BUG-1/BUG-2 chiusi):
  estrarre la logica shot condivisa in un punto unico, panel thin che delegano.
  Primo passo: audit grep dei comandi duplicati.

## [2026-06-15b] — BUG-1 (drag cross-colonna keys+cels) + Gruppo A operazioni key-only

### Fixed
- **BUG-1 [keys+cels] — drag cross-colonna ora trasferisce i keyframe.** Trascinando
  un sotto-blocco di celle con keyframe su un'altra colonna (modalità "Keyframes
  Follow Exposure"), le chiavi seguono le celle al nuovo stage object. Implementazione
  **unificata** in `CellKeyframeMoverTool` (`xsheetdragtool.cpp`): rimosso del tutto il
  keyframe-mover *live* dal tool combinato — le celle si muovono live (LevelMover), le
  chiavi vengono trasferite **al rilascio** dove le celle sono atterrate (posizione già
  validata da `canMove`, quindi righe libere → atomico). Questo elimina la transizione
  fragile `revertMove` che corrompeva la cell-selection (stessa classe di BUG-2,
  diagnosticata con log su 3 giri). Collisione di riga esatta sulla destinazione →
  blocco dell'intero drag (celle incluse); convivenza permessa se le righe target sono
  libere. Nuova `CrossColumnKeyframeUndo` (snapshot via `TKeyframeData`). Il drag di
  colonna intera → dest vuota (path clone "Cells and Column Data") resta invariato,
  rilevato a runtime via `keysStillAtSource` per non fare doppio spostamento.

### Added
- **Gruppo A — operazioni timing su selezione di SOLE chiavi (`TKeyframeSelection`).**
  Routing via `enableCommands`, tutte con undo dedicato, in `keyframeselection.cpp`:
  - **Reverse** — specchia `r→r0+r1-r` (involuzione, undo==redo).
  - **Swing** — appende la coda specchiata (ping-pong); undo ripristina la coda.
  - **Roll Up / Roll Down** — rotazione ciclica entro `[r0,r1]`; undo = direzione opposta.
  - **Repeat** — appende il pattern N volte (aggancio a `DuplicatePopup` per la selezione
    chiavi). Nuova opzione **Loop** (visibile solo con chiavi selezionate): sovrappone la
    chiave di giunzione (passo `r1-r0`) per cicli seamless senza doppia posa.
  - **Time Stretch** — rimappamento **proporzionale** `r→r0+round((r-r0)·(N-1)/(old-1))`,
    estremi ancorati (caso walk cycle 18→24). Aggancio a `TimeStretchPopup` + abilitazione
    `MI_TimeStretch` in `enableCommands`. Undo con snapshot dell'intero span interessato.
- Step/Each/Reframe **archiviati** per le sole chiavi: non hanno semantica sensata su
  keyframe interpolati (decidono esposizione di disegni tenuti).

### Notes / TODO
- **Time Stretch su selezione combinata celle+chiavi RIMANDATO** a sessione dedicata con
  build di debug + lldb: lo stretch celle (`xsh->timeStretch`) con keys-follow ON shifta
  le chiavi in modo uniforme (non proporzionale) → doppio-handling sul path keys+undo
  (classe BUG-2). Serve undo-class dedicata (snapshot originale + stato intermedio).
- Reverse mantiene gli ease di ogni chiave (non flippa le tangenti in/out) — come il
  Reverse celle. Eventuale rifinitura futura.

## [2026-06-15] — BUG-2 (perdita chiavi su undo) + drag celle: modificatori nativi

### Fixed
- **BUG-2 [perdita dati] — l'undo del cut perdeva i keyframe (modalità keys+cels).**
  In `cutCellsKeyframes()`, con "Keyframes Follow Exposure" ON, `cutCells()` →
  `removeCells()` cancellava già i keyframe nello span tagliato; il successivo
  `deleteKeyframesWithShift()` faceva lo snapshot a chiavi già sparite
  (`data->m_keyData=0`) → `DeleteKeyframesUndo` non aveva nulla da ripristinare.
  **Fix** (`cellkeyframeselection.cpp`): cancellare i keyframe con
  `deleteKeyframes()` (senza shift) PRIMA di `cutCells()`; lo snapshot cattura le
  chiavi e l'ordine di undo si inverte correttamente (CutCellsUndo ripristina
  celle+shift, poi DeleteKeyframesUndo ri-incolla le chiavi). Rimuove anche un
  latente doppio-shift. Diagnosi con debug build + lldb (probe Python su
  `TKeyframeData::m_keyData`). Commit `73f1a5d64`.

### Changed / Added
- **Drag celle: ripristinata la convenzione modificatori nativa di Tahoma.**
  Rimossa l'intercettazione di Alt+drag che attivava il Cell/Block Swap: ora il
  drag celle è gestito da `LevelMoverTool` — drag = move, Shift = insert,
  Alt = overwrite, Ctrl = copy (comportamenti già presenti in upstream ma
  "coperti" dallo swap Ztoryc).
- **Cell/Block Swap spostato su Ctrl+Alt+drag** (Cmd+Option su macOS): non ruba
  più il modificatore overwrite; nessun conflitto con il rolling cel (regioni
  diverse — drag bar vs smart-tab extender). Il block swap ora funziona anche
  afferrando il corpo della cella (prima il fallback forzava una cella sola).
  Commit `c5095c6a7`.
- **Tooltip hint** sui modificatori del drag celle, mostrato sulla drag bar
  (nomi tasti per piattaforma: Cmd/Option su macOS, Ctrl/Alt altrove).

### Notes
- **BUG-1 rimandato** (drag cross-colonna in modalità combinata keys+cels →
  trasferimento keyframe al nuovo stage object). È una feature sostanziosa sullo
  stesso path delicato keys+undo di BUG-2: da fare a sessione fresca con verifica
  lldb. Accertato che la preference "Cell-dragging Behaviour: Cells and Column
  Data" già muove l'intero stage object (keyframe inclusi) per colonne intere su
  destinazione vuota; manca il caso del **sotto-blocco** in modalità combinata,
  bloccato da `KeyframeMoverTool::canMove` (col != startCol).
- Dead-code del CellSwapper NON rimosso (swap tenuto, su Ctrl+Alt).
- Pulizia repo: rimossi branch effimeri (`feature/keys-cels-modes` già mergiato,
  e 2 worktree `claude/*` con relativi branch).

## [2026-06-14] — Keys Follow Exposure: operazioni Cels portano i keyframe + toggle visibile

Branch `feature/keys-cels-modes` (NON ancora su master).

### Fixed
- **Undo della cancellazione keyframe nel Level Extender (shrink)** — accorciare il
  timing di un blocco cancellava i keyframe nel tail rimosso e l'undo non li
  ripristinava. Fix a livello di comando in `LevelExtenderUndo` (`xsheetdragtool.cpp`):
  snapshot dei keyframe del blocco all'onClick (`m_savedKeys`, gated su `m_followExposure`)
  + restore nel path undo (`insertCells()`) limitato al tail `[r0,r1]`. Testato OK.
- **Popup Repeat… grigio con selezione combinata** — `DuplicatePopup` faceva
  `dynamic_cast<TCellSelection>` che fallisce su `TCellKeyframeSelection` → campi/bottoni
  disabilitati. Nuovo helper `getCurrentCellSelection()` estrae la cell-selection interna
  (`duplicatepopup.cpp`). ⚠️ Stesso pattern da verificare su Time Stretch….

### Added
- **Keys-follow per le operazioni del menu Cels** (toggle "Keyframes Follow Exposure" ON) —
  i keyframe seguono il rimappamento delle celle. Tutto in `txsheet.cpp`, gated sulla
  preference, undo simmetrico via primitive. Testato OK:
  - **Reverse** — mirror involutivo `r→r0+r1-r` (`ReverseUndo` undo==redo).
  - **Roll Up/Down** — rotazione ciclica; chiave di bordo salvata e riposizionata
    (prima cancellata da `removeCells`).
  - **Swing** — duplica `[r0,r1-1]` specchiato nel tail (`s→2*r1-s`), pivot escluso.
  - **Repeat** — duplica il chunk di keyframe su ogni copia.
- **Toggle visibile in toolbar** — `MI_ToggleKeyframesFollowExposure` ora ha icona
  `segment_linked` (`mainwindow.cpp`): aggiungibile alla Quick Toolbar dell'xsheet via
  "Customize Quick Toolbar" → Misc, mostra stato premuto/rilasciato.

### Notes
- **Classificazione completa menu Cels** in `KEYS_CELS_MODES_DESIGN.md`: estendibili
  (rimappano il tempo) vs no (cambiano contenuto/marker playback). Loop Frames resta
  fuori (righe virtuali, no celle reali) → eventuale "loop transform" è feature a parte.
- **Gruppo ritempi TODO** (per sessione fresca): Step/Each/Time Stretch richiedono
  snapshot/restore keyframe *dentro le Undo class* (ripristinano celle wholesale, non via
  primitiva inversa). Reframe: semantica keyframe da decidere. Mappature già definite nel
  design.
- Toggle di default nella Quick Toolbar per tutti: eventuale, via `buildDefaultToolbar`.

## [2026-06-13] — task 52 declassato, autofill antialias indagato, brush-feel audit

### Added
- **BrushProfiler** (`toonz/sources/include/brushprofiler.h`) — strumentazione
  latenza header-only, zero-cost senza flag, attivabile con
  `ZTORYC_BRUSH_PROFILE=1`. Instrumenta `leftButtonDrag` (dab_compute),
  `paintGL` e la latenza end-to-end evento→paint; stampa min/med/max/avg ogni
  120 campioni su stderr. Punti in `sceneviewer.cpp`, `sceneviewerevents.cpp`,
  `toonzrasterbrushtool.cpp`.

### Investigated / Declassed
- **Task 51 — Brush feel → DECLASSATO** (da ALTA strategica a feature opzionale).
  La premessa "feel < TVPaint" era un'assunzione di Fable 5, mai misurata.
  Audit fase 1 col BrushProfiler su tavoletta reale (153 blocchi × 120 dab):
  dab_compute med **0.08 ms**, paintGL med **0.27 ms**, evt→paint med **2.18 ms**
  (sotto un frame @60Hz). La pipeline software NON è il collo di bottiglia:
  repaint già incrementale (invalidateRect→clipRect→glScissor), tablet events
  non compressi, nessun lavoro estraneo nel drag. Fasi 2-4 (stabilizzatore,
  preset, curve MyPaint) restano solo come feature UX on-demand.
- **Task 52 — crash palette Shift+N → DECLASSATO**: non riproducibile né su
  debug build + lldb + MallocScribble (path esercitato 70× senza crash, handle
  sempre azzerata correttamente) né sull'ESATTO binario release che crashò l'11
  (TUndoManager hardening già incluso). Heisenbug state-dependent, famiglia del
  crash StudioPalette declassato l'11. CrashHandler a presidio.

### Notes
- **AutoFill bordino bianco antialiased (smart raster) — IRRISOLTO, baseline safe
  ripristinata.** 5 approcci provati, tutti regrediti sui pennelli morbidi:
  (1) dipingere pixel ink adiacenti → sborda; (2) + guardia no-esterno → sborda;
  (3) delega al fill() nativo → leak totale (soft brush senza core tone=0);
  (4) tone-march `<=` → ricopre tutto; (5) tone-march `<` → colora tratti aperti.
  Causa radice: nessuna regola locale per-pixel distingue versante interno/esterno
  della linea sui soft brush (gradiente di tone esteso). Strada giusta (da fare
  OFFLINE, testata su raster sintetici): logica scanline-direzionale di
  `calcFillRow` confinata alla regione BFS. Lezione salvata in memoria Claude.
- Lezione trasversale: misurare/riprodurre prima di patchare ha evitato di
  inseguire problemi inesistenti (52, 51); l'errore è stato spedire pezze autofill
  in live invece di testarle offline.

## [2026-06-10b] — New Shot After Current, fix task 49/50, README, release v0.4.1

### Added
- **New Shot After Current (Shift+N)** — comando globale MI_ZtoryNewShotAfter:
  da dentro una sub-scene chiude, crea lo shot subito dopo quello in editing
  e ci entra direttamente (l'artista resta nello SHOTEDITOR col pennello in
  mano). Dal Board/Animatic aggiunge dopo la selezione senza entrare.
  Rimappabile da Configure Shortcuts. Bottone "+" anche sullo Shot Board
  (room T); il "+" della timeline animatic dentro una sub-scene ora delega
  al comando invece del warning "main xsheet only".
- Timeline animatic si ricostruisce su modelReset anche con sub-scene aperta
  (refreshFromScene legge il TOP xsheet) — prima restava stale dopo Shift+N.

### Fixed
- **Task 49 — scatto sul secondo tratto di disegno** (commit e7676d620):
  il detect timer (1s) partiva a ogni xsheetChanged in sub-scene e scattava
  a metà del tratto successivo (detect + render thumbnail sincroni sul thread
  UI corrompevano la linea). Ora xsheetChanged marca solo m_dirtyShotCol;
  detect+render solo su frameSwitched / ritorno al Board / showEvent, più
  guardia mouse-premuto sul timeout.
- **Task 50 — panel fantasma dopo undo che svuota uno shot**: verificato
  risolto (test utente) dopo i fix task 48 + TUndoManager hardening.

### Modified
- **README riscritto** (f8791f070, 4d3a625c8): sezione Download, feature per
  room (camera-move, light gizmo, Arrows, burn-in, import sceneggiatura…),
  4 screenshot nuovi (animatic+board, 2× shot editing, pagina PDF con camera
  move), roadmap ridotta a Kitsu.
- ANIMATIC_TASKS: chiusi 49/50; rimossi come già-fatti 21 (volume audio),
  24 (startup hub), frame handle separato (risolto dal Monitor); TRADITIONAL
  declassata; nuovo task 51 Brush feel (ALTA, strategica) con piano in 4 fasi
  (audit latenza → stabilizzatore → preset → MyPaint). NO codice da Krita
  (GPL); libmypaint già in casa.

### Release
- **v0.4.1** — bump ZtorycVersion.cmake, prima stabile della linea 0.4.
  Diff da v0.4.0-beta.2: light gizmo (fase 3) Board+Shot Board, burn-in
  export, New Shot After Current, fix task 48/49/50, TUndoManager hardening
  (candidato upstream), parità Board↔Shot Board, README nuovo.

### Notes
- gh CLI: risolti i 401 intermittenti — c'erano 4 voci keychain duplicate
  `gh:github.com`; pulite tutte + login fresco. Un solo token ora.

# Ztoryc — Changelog

> **Come aggiornare (istruzioni per Claude Code):** dopo ogni sessione aggiungi
> una voce in cima con: data, `### Fixed`, `### Added`, `### Modified`,
> `### Upstream candidates`, `### Notes`. Poi esegui rsync (vedi AGENTS.md).
> Voci più vecchie di ~2 settimane → spostarle in `CHANGELOG_ARCHIVE.md`.

---

## [2026-06-11] — sessione stabilità: TUndoManager hardening, crash palette declassato

### Fixed
- **TUndoManager use-after-free (core, candidato upstream)**: se un comando
  eseguito DENTRO `TUndo::undo()` rientra nel manager (es. chiusura sub-scene
  → push di `CloseChildUndo`), `doAdd`/`beginBlock`/`reset` cancellavano il
  ramo redo che CONTIENE l'oggetto in esecuzione → l'oggetto proseguiva su
  memoria liberata (root cause a monte del wipe task 48). Fix in
  `tcore/tundo.cpp`: la cancellazione dell'entry in esecuzione è deferita alla
  fine di `undo()`/`redo()` (`m_executing`/`m_deferredDelete` +
  `safeDeleteUndo`); guard anche su `++it` in `redo()` dopo rientranza.

### Notes
- **Crash palette DECLASSATO**: non riproducibile su build debug sotto lldb
  nemmeno con MallocScribble (torture test: load studio palette dopo delete
  livello, undo spam, switch sub-scene). Teoria confermata dai fatti: era un
  derivato del bug undo-wipe (restore rotto → livelli distrutti → palette
  corrente dangling), fixato il 2026-06-10. Si tiene il workaround note e il
  CrashHandler a presidio; togliere dal radar salvo recidiva.
- Build di debug permanente in `/Volumes/ZioSam/tahoma2d-workspace/debug-build`
  (cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5, stesse
  dipendenze homebrew). Script lldb repro in /tmp/ztory_lldb_script.txt.
- Nuovo task minore 50: panel fantasma nel Board dopo undo che svuota uno shot
  (si riallinea a enter/exit della sub-scene).

---

## [2026-06-10] — light direction (task 40 fase 3), burn-in export, fix undo/lag

### Added
- **Gizmo Light Direction (task 40 FASE 3 — COMPLETA il task 40)**: freccia
  conica 3D per-panel nel Board. Bottone ☀ → drag sul thumbnail (coda =
  sorgente), rotella = inclinazione Z (a ±100% notazione ⊙ verso camera /
  ⊗ verso fondo), Shift+rotella = apertura fascio (12–90°, hard/soft light).
  Cono con silhouette tangente all'ellisse di base, occlusione corretta
  front/back, basi piatte di profilo (roundness = sin(tilt)), shading assiale
  (luce che cade lungo la freccia). Fascio conico traslucido + sole + readout
  gradi solo durante il drag. Right-click rimuove. Toggle visibilità (bottone
  L + shortcut L), swatch colore temperatura. Persistenza `.ztoryc`
  (lightTail/Tip/Depth/Spread/Color), undo via UndoBoardState, anche su PDF.
  Commit `d498c010b`.
- **Burn-in export animatic (stile Storyboard Pro)**: nel dialog Export
  Animatic gruppo "Burn-in" — timecode di sequenza (basso-dx), nome
  SQ_SH_P per-panel (alto-sx), checkbox Clapperboard (mirror live del Board
  setting nativo). Architettura: `TRasterImageUtils::addBurnIn` (toonzlib) +
  `MovieRenderer::setBurnIn` pinnato al setup (stesso pattern del fix audio
  task 43) + `ZtoryBurnInConfig` in ZtoryModel letta da rendercommand.cpp.
  Bottone "Render Settings…" nel dialog con riepilogo formato live (poll
  700ms); estensione file riletta alla conferma.

### Fixed
- **Dialog Export Animatic bloccava l'Output Settings popup**: era
  WindowModal (blocca tutta la catena del main window). Ora non-modale con
  QEventLoop locale.
- **Undo svuotava lo storyboard** (parziale — vedi Known issues):
  `captureSnapshot()` usava l'xsheet CORRENTE → dentro una sub-scene snapshot
  con level tutti nulli → `restoreFromSnapshot` rimuoveva tutte le colonne e
  salvava `.ztoryc` vuoto. Fix: snapshot sempre dal TOP xsheet + guardia che
  rifiuta snapshot senza alcun level valido.
- **Lag disegno con auto-match attivo**: ogni burst di tratti eseguiva resize
  colonna + resequence + refresh anche a durata invariata. Aggiunta guardia
  no-op in `onMatchSubsceneDuration`.

### Fixed (addendum sera — task 48 RISOLTO)
- **Undo-wipe definitivo (use-after-free)**: con Cmd+Z da DENTRO una sub-scene,
  `restoreFromSnapshot` eseguiva `MI_CloseChild` → `closeChild` pusha un
  `CloseChildUndo` → `TUndoManager::add()` durante un undo() attivo tronca il
  ramo redo e DISTRUGGE l'`UndoBoardState` in esecuzione → la reference al suo
  `m_before` (lo snapshot) diventava dangling → re-insert leggeva memoria
  liberata → tutti i level "nulli" → Board svuotato. Diagnosi via logging
  [ZTORY] su repro reale. Fix: deep-copy dello snapshot come prima istruzione
  (le TXshLevelP della copia tengono vivi anche i livelli). In più: firewall in
  saveZtoryc (mai persistere .ztoryc con 0 shot se l'xsheet ha colonne child) e
  logging diagnostico permanente su refreshFromScene/restore/resequence.

### Known issues (prossima sessione — DEBUG BUILD + LLDB)
1. **Crash palette** — `StudioPaletteCmd::loadIntoCurrentPalette →
   TPalette::assign → destroy mappa stili` SIGBUS su puntatore garbage
   (use-after-free, famiglia task 42 residuo). Report:
   `Ztoryc-2026-06-10-112817.ips`.
2. **TUndoManager UB residuo**: quando `add()` distrugge l'undo in esecuzione,
   il manager rientra in un oggetto liberato — oggi innocuo in release, ma è
   UB e possibile parente dei crash random. Da verificare sotto lldb.
3. **Lag sui primi tratti** (anche senza auto-match) — già risolto in versioni
   pre-merge, forse fix perso col merge 1.6.1: cercare nel CHANGELOG_ARCHIVE.
   Candidati: m_panelDetectTimer/refresh thumbnail su xsheetChanged in sub.

### Notes
- Burn-in testato e funzionante (raffinamenti rimandati); clapperboard da
  verificare con Board configurato nei render settings.

---

## [2026-06-09] — v0.4.0-beta.2: libreria frecce, overlay camera-move, fix crash

### Added
- **Pannello Arrows** (ex "Camera Moves"): libreria di frecce vettoriali `.pli`
  per indicazioni di movimento personaggi/camera. Picker a thumbnail da cartella
  bundled (`stuff/library/directional arrows`, 16 frecce) + cartella personale
  opzionale (QSettings). Click → stampa la freccia nella colonna "Annotazioni"
  della sub-scena, con **colori originali** preservati (mergeImage + palette dal
  livello PLI), strokes raggruppate, inserimento **frame-aware** (nuova cel su
  cella vuota o in hold con "Enable Creation in Hold Cells").
- **Overlay camera-move FASE 2** sui thumbnail Board + PDF: render "backed-out"
  che copre START e STOP (le pan non vengono più tagliate), rettangoli + frecce
  d'angolo (notazione classica), lettere **A→B continue** tra i panel dello shot,
  toggle "Trk" per le label tipo movimento (persistito), label PDF a ~6pt.

### Fixed
- **Crash Geometric tool** aprendo una sub-scene (doppio-click shot in Animatic):
  `GeometricTool::onDeactivate()` dereferenziava `m_viewer` nullo. Diagnosi lldb.
- **Disallineamento view-mode** Animatic↔Shot (Camera Stand vs Camera View): il
  viewer attivo ora segue sempre il button set condiviso, niente toggle manuale.
- **Trk In/Out** invertiti nell'overlay (scala su = Out, giù = In); le label si
  auto-correggono al caricamento di scene vecchie.

### Upstream candidates (Tahoma2D)
- Null deref in `GeometricTool::onDeactivate()` (bug core).

### Notes
- Workspace unificato (merge merge-1.6.1 in master). Release via workflow
  (publish_release + release_tag v0.4.0-beta.2), note bilingui poi `gh release edit`.

---

## [2026-06-08] — consolidamento workspace + fix crash Geometric tool (task 42)

### Modified
- **Workspace unificato**: mergiato il branch `merge/upstream-nightly` in `master`
  (task 40 fase 1, PSD fix, PDF template, hints + CrashHandler thread-safe ora tutti
  insieme). Worktree `merge-1.6.1` e branch `merge/upstream-nightly` dismessi.
  Risolto conflitto in `storyboardpanel.cpp` (frammento stale) e duplicazione
  `fps`/`framesToTC` nell'export PDF post-merge.

### Fixed
- **Task 42 — CRASH Geometric tool aprendo sub-scene** (`geometrictool.cpp`):
  doppio-click su uno shot nell'Animatic con il Geometric tool attivo →
  SIGSEGV. `GeometricTool::onDeactivate()` chiamava `m_viewer->getDevPixRatio()`
  in modo incondizionato; durante l'apertura sub-scene la catena
  `ToolHandle::onImageChanged → setTool → onDeactivate` disattiva il tool prima
  che fosse agganciato a un viewer → `m_viewer == NULL` → null deref
  (EXC_BAD_ACCESS @ 0x90 dentro getDevPixRatio). Fix: il viewer serve solo per
  il rendering nel ramo `m_isRotatingOrMoving`, ora è guardato `if (m_viewer)`.
  Diagnosi con **lldb sulla repro reale** (il crash log della release mostrava
  "due frame setTool+316" fuorvianti — simbolicazione approssimata).

### Notes
- Il caso "raster brush" del task 42 era già coperto dal guard `m_inColorStyleChanged`
  (commit `8f8740628`). Verificati non vulnerabili anche StyleEditor/PaletteViewer
  (caso C non crasha). Il task 42 resta da affrontare con debug+lldb solo se emergono
  altri percorsi (palette-switch ri-entrante puro).
- **Candidato upstream Tahoma2D**: il null deref in `GeometricTool::onDeactivate()`
  è un bug del core, riproducibile anche fuori da Ztoryc se onDeactivate scatta con
  viewer nullo. Valutare PR.

## [2026-06-06] — fix CrashHandler da render thread; verifica ImageManager leak

### Fixed
- `crashhandler.cpp`: `CrashHandler::trigger()` non mostra più la dialog quando
  chiamato da un thread non-main (render thread). Su macOS, creare `NSWindow`
  da un background thread lancia un'ObjC exception → abort secondario che
  mascherava il crash reale. Ora il log file viene sempre scritto; la dialog
  appare solo se si è sul main thread. Fix applicato in entrambi i repo
  (`tahoma2d/` e `merge-1.6.1/`).

### Notes
- Crash render analizzato: root cause `TLevelColumnFx::doCompute` → `TRasterFx::applyAffine`
  null ptr (0x14) dopo 2.5h render con 17.9 GB MALLOC (Plastic + PSD). Causa
  profonda: memoria esaurita durante render lungo. Fix `be20f9512`
  (invalidateAllCached post-render) e `b79ba7d32` (memoryShortage macOS 14.3%)
  sono già presenti e attivi in entrambi i branch — nessun ulteriore intervento
  necessario. Workaround: renderizzare in batch più piccoli su scene molto lunghe.
- Analizzato anche crash Tahoma2D 1.6.1 su macOS 26.5 (beta): EXC_CRASH SIGKILL
  Code Signature Invalid in dyld — non è un bug Ztoryc, è firma codice non valida
  su macOS 26.5 beta con enforcement più stretto. Fix per lo sviluppatore Tahoma:
  `codesign --deep --force --sign - Tahoma2D.app`.
- Task 43 (export animatic a/b/c/d), 45 (status bar hints) — confermati risolti.
- Task 46 (explode peg inutili) — chiuso, comportamento intenzionale Tahoma2D.

## [2026-06-04] — PDF storyboard template, hints, fix audio/crash

### Added
- **Task 45 — Status bar hints contestuali** (gold `#d4a017`):
  - Board: hover su panel → hint workflow Board vs Animatic; hover toolbar buttons → hint specifico per ogni bottone
  - Animatic: hint tool-aware in `mouseMoveEvent` (SelectTool, TrimTool roll/ripple, RazorTool); `leaveEvent` pulisce
  - XSheet: `setStatusTip` su Roll Up/Down, Autofill, Auto Fill checkbox (BrushToolOptionsBox)
  - Nuovi metodi `StatusBar::showZtoryHint` / `clearZtoryHint` + helper `TApp`
- **PDF Export — template professionale**:
  - Header: logo Ztoryc, Production, Title, Page X/Y
  - 3 panel per pagina (1 riga), celle full-height; sub-header con shot label + durate
  - Griglia senza gap: linea grigia tra panel stesso shot, nera+spessa tra shot diversi
  - Footer: logo piccolo + "Made with Ztoryc"
- **Metadati Production/Title**: campi in `ZtoryStartupDialog` + `StartupPopup`; persistiti in `.ztoryc`; fix race condition con `refreshFromScene`

### Fixed
- **PDF timecode fps**: leggeva fps fisso 24; ora da `scene->getProperties()->getOutputProperties()->getFrameRate()`
- **Camera keyframe singolo**: non crea più panel boundary in `detectAndUpdatePanels`
- **Audio +1 frame nelle scene esportate**: `getRange()` ora con `ignoreLastStop=true` in `onExportShots`
- **Crash al quit/workflow-switch (OpenGL static destructor)**: `signalHandler` tentava QDialog su Qt già distrutto. Fix: flag `s_appExiting` su `aboutToQuit` → `_Exit(0)` silenzioso
- **"sub-scene" → "shot"**: tooltip Auto Match Duration e Match Duration button

### Notes
- Plastic drawing invisibile (mesh visibile): intermittente, non riproducibile. Da investigare con debug build.

---

## [2026-06-03] — Merge upstream nightly ✅ SU MASTER + 🚀 RELEASE v0.4.0-beta.1

> 🚀 **Rilasciata `v0.4.0-beta.1`** (prima beta sulla base mergeata) — binari pubblicati su
> GitHub Releases per tutte le piattaforme: macOS Intel + Apple Silicon (DMG), Windows
> (install .exe + portable .zip). Note bilingui IT/EN applicate, marcata prerelease.
> Workflow: `gh workflow run macOS_build.yml/windows_build.yml -f publish_release=true -f release_tag=...`
> (NON via git tag). Note in `~/ZtorYc/v0.4.0-beta.1_notes.md`.
>
> **Fix/migliorie della giornata post-merge (tutti su master):**
> - 🔴 crash al salvataggio (`CleanupParameters`/`~TPalette`, `LastSavedParameters` morta) — risolto
> - crash Set Key Plastic Tool (`m_sd` null in `keyFunc_undo`) — risolto
> - default Ztoryc: Ease In/Out (era Linear), drag bars ON, implicit hold OFF
> - rebranding Preferences + shortcut dialog (Tahoma2D→Ztoryc, lasciati import + crediti)
> - confermati risolti DAL merge: asterisco/save dal main in storyboard; vari refresh
>
> **Task loggati per dopo (ANIMATIC_TASKS.md):** 45 status-bar hint contestuali; 46 explode
> sub-scene crea peg inutili (pre-esistente); 47 audio scrub meno reattivo nel viewer normale
> (il fix widen-scrub 150ms è SALVO in txsheet.cpp:2121 — regresso in native scrub/flipconsole timing).
> **Crash noti rari (beta known issues):** keyframe move durante save concomitante; Plastic storeDeformation.

> ✅ **Mergiato su `master`** (fast-forward `b4aff742f..9d88943cf`, push origin master).
> Lavorato su branch `merge/upstream-nightly` + worktree isolato, poi portato su master.
> Crash keyframe (Drawing Number) rimandato a **hotfix** futuro (raro, non ripresentato).

### Merged
- **Merge `upstream/master` (nightly, post-v1.6.1)** in `master`.
  - merge-base: `c8b768aa3` (10 mar 2026). Ztoryc 333 commit / upstream 154 (46 fix, 10 feat).
  - **Solo 17 conflitti**, hotspot (mainwindow, txsheet, xshcellviewer, tooloptions,
    flipconsole) **auto-mergeati**. Conflitti risolti: 15 infra/branding → `ours`;
    `CMakeLists.txt` → ours (scartato `set(VERSION 1.6.1)`, Ztoryc usa `ZtorycVersion.cmake`);
    `cellselectioncommand.cpp` → **upstream** (il loro fix "set key" su peg column rende
    ridondante il mio PR candidate).
  - **2 fix post-merge** (`b2ca7abb4`): (a) `onStageObjectChange` — upstream ha aggiunto
    `bool isDragging` alla virtuale, allineata la firma dell'override Ztoryc; (b) `maintoolbar.h/.cpp`
    riaggiunti al CMakeLists (persi col `--ours`, causavano undefined `MainToolbar::MainToolbar`).
  - **Build pulita** + deploy isolato funzionante (`merge-1.6.1/toonz/Ztoryc.app`, `ZTORYC_WORKSPACE`).
- Novità upstream verificate presenti e funzionanti: **Render Settings** (ex Output Settings),
  **Sync with Play Range**, Animate tool (Reset Center/Set Key/Pick mode), Master Toolbar,
  Drawing Number/Mark.

### Fixed (post-merge)
- ✅ **CRASH Set Key Plastic Tool** (`plastictool_animate.cpp`, commit `9d88943cf`) — le azioni
  Set Key / Set Rest Key (nuove di upstream per il Plastic) erano raggiungibili via shortcut
  anche senza deformazione attiva (`m_sd == nullptr`) → SIGSEGV in `keyFunc_undo →
  m_sd->getKeyframeAt()`. Aggiunta guardia `if (!m_sd) return;`. **Candidato fix upstream.**
  Era l'unico crash *introdotto* dal merge (gli altri sono pre-esistenti).

### Hotfix da fare (non bloccanti — merge già su master)
- 🔴 **CRASH muovendo keyframe nella xsheet** (`Crash-20260602-161810.log`) — SIGSEGV in
  `TStageObject::setKeyframeWithoutUndo → TDoubleParam::setKeyframe`, via `KeyframeMover::moveKeyframes`.
  **NON è regressione del merge**: `keyframemover.cpp` è identico a upstream puro (mai toccato da Ztoryc).
  È un bug upstream nightly: muovendo un keyframe su **colonna di livello** in una scena **esistente**
  (`scsh120.tnz`, salvata pre-Drawing-Number), il canale nuovo `T_DrawingNumber` viene copiato con dati
  non inizializzati → crash. Il fix noto upstream `cf52a1776` ("moving non-column based channel keys")
  è già incluso ma non copre questo path. **TODO**: (1) ripro su scsh120 vs scena nuova; (2) guard/init
  difensivo del canale Drawing Number al load o in `setKeyframeWithoutUndo`. File: `tstageobject.cpp`,
  `tparam.cpp` (TDoubleParam::setKeyframe).

### Notes
- Bug pre-esistenti (non da merge) osservati: (a) glitch refresh monitor panel (tracce video/audio,
  solo scene vecchie finora); (b) entrando in uno shot il viewer ignora il view mode corrente e parte
  in camera view invece di camera-stand (serve toggle per refresh). Entrambi da loggare/indagare.
- Integrazione fine "Sync with Play Range" ↔ marker In/Out dell'animatic: il sync si aggancia al drag
  via `xsheetdragtool`; nell'animatic i marker passano per `subscenecommand::setPlayRange`, quindi il
  sync potrebbe non scattare lì. Da collegare esplicitamente se serve.

### Altri crash/bug emersi durante l'uso (pre-esistenti, NON da merge)
- 🟡 **Crash Plastic Tool intermittente** (`Crash-20260602-172557.log`, sull'app PRE-merge) —
  SIGSEGV in `PlasticTool::onColumnSwitched → storeDeformation → onSelectionChanged` (ricorsivo)
  attivando il tool via shortcut. Raro ma l'utente usa il Plastic tantissimo. Le funzioni del crash
  NON sono toccate dal merge (upstream cambia solo la parte "animate Set Key" del Plastic, 315 righe),
  quindi il crash persiste anche nel merge. Ipotesi: deref durante cambio colonna/attivazione su stato
  transitorio. File: `plastictool.cpp`.
- 🟡 **Disallineamento mesh↔skeleton Plastic — TRANSITORIO** — animando una gamba la mesh non seguiva
  lo skeleton (bone "fuori" dalla mesh). ⚠️ CORREZIONE: **NON dipende dall'import come sotto-scena**
  (reimportando la libreria torna a funzionare) → i dati NON sono corrotti, è uno stato transitorio.
  Ipotesi più probabile: **glitch di refresh/invalidazione del viewer** — la mesh deformata non viene
  ridisegnata in sync con lo skeleton; reload forza il ridisegno. Coerente con gli altri problemi di
  refresh osservati (monitor panel; view mode all'ingresso shot che parte in camera view). Possibile
  pattern comune: **invalidazione viewer mancante entrando/lavorando nelle sotto-scene** in Ztoryc.
  Da indagare: refresh/invalidate del viewer Ztoryc su enter sub-xsheet, deformazione Plastic.

### Ripresa (sessione nuova)
1. Test riproducibilità crash keyframe (scsh120 vs scena nuova)
2. Fix crash Drawing Number su scene pre-esistenti
3. `git checkout master && git merge merge/upstream-nightly` → push origin master → rsync → rebuild app principale
4. (minori) crash Plastic intermittente + disallineamento mesh in sotto-scena (entrambi pre-esistenti)

---

## [2026-06-02] — CLONE shot: fix perdita keyframe camera (e pegbar)

### Fixed
- **CLONE shot perdeva i keyframe di camera** (`storyboardpanel.cpp`,
  `ztoryanimatic.cpp`). Causa radice in `restoreCamera()`
  (`stageobjectsdata.cpp`): ogni child xsheet creato da `createChild()` ha già
  una Camera 0 di default; ripristinando la camera via `StageObjectsData` il
  codice la trova "occupata" e crea una **Camera 1 fantasma** con i keyframe,
  lasciando la Camera 0 (usata per il rendering) vuota → chiavi apparentemente
  perse. Due path affetti:
  - **Board** (`cloneChildToPosition`): includeva `CameraId(0)` negli ids →
    camera fantasma.
  - **Animatic** (`animCloneChildToPosition`): usava `storeColumns()` che
    **omette del tutto** la camera → nessun keyframe copiato.
- **Fix**: dopo `restoreObjects`, copia manuale degli stage object non-colonna
  (camera + pegbar) sull'oggetto con lo **stesso id** già esistente nel clone
  (`assignParams` keyframe + copia `TCamera` + parent). Mirror della logica
  stock `cloneXsheetTStageObjectTree()` (in namespace anonimo, non richiamabile
  da fuori). Esteso ai **pegbar** così anche i rig cutout mantengono
  l'animazione.

### Notes
- **COPY shot** verificato OK: condivide lo stesso `TXshChildLevel`
  (sub-scene condivisa) → camera = stesso oggetto, nessuna perdita possibile.
- Il razor/split usa lo stock `ColumnCmd::cloneChild` (già corretto via
  `cloneXsheetTStageObjectTree`) — non toccato.
- Non coperto: camera su spline (motion path) — caso edge raro; i keyframe di
  posizione/rotazione/scala/zoom sono comunque coperti.

---

## [2026-06-02] — Mark In/Out persistenti per-xsheet (tutti i workflow)

### Fixed
- **Mark In/Out delle sotto-scene non persistevano dopo save+reload**
  (`txsheet.h/.cpp`, `subscenecommand.cpp`, `iocommand.cpp`). Causa: il play
  range "live" è in `scene preview properties`, ed è **globale e unico** —
  condiviso tra main xsheet e tutte le sotto-scene. La vecchia `s_frameRangeMap`
  era runtime-only (chiave `TXsheet*`, invalidata al reload). Inoltre il
  preview-range globale veniva serializzato così com'era: se l'ultimo range
  attivo era quello di una sotto-scena (o si salvava da dentro una sotto-scena),
  al reload il **main ereditava** quel range (bug nidificazione: con main/-1/-2
  il main si ritrovava i marker di -2).
- **Fix strutturale — marker In/Out per-xsheet persistiti nel `.tnz`:**
  - `TXsheet`: nuovi campi `m_markerIn`/`m_markerOut` (-1 = unset), API
    `get/setInOutMarkers()` + `hasInOutMarkers()`, serializzati in
    `saveData`/`loadData` con tag `<inOutMarkers>` (assente nei file vecchi →
    default unset, retrocompatibile).
  - `openSubXsheet` (scendendo): salva il range dell'xsheet che si lascia sia in
    cache sia nei marker persistenti (`prevXsh->setInOutMarkers`). Risalendo:
    priorità di ripristino = marker persistenti → cache di sessione → fallback
    auto (durata shot + cross-dissolve XD).
  - `closeSubXsheet`: scrive i marker dell'xsheet che si chiude nel TXsheet +
    dirty flag.
  - `IoCmd::saveScene`: sincronizza il play range live → marker dell'xsheet
    **corrente** prima di serializzare (copre il caso "imposto un range e salvo
    senza entrare/uscire da una sub").
  - `IoCmd::loadScene`: il **main xsheet diventa autoritativo** — il play range
    live viene inizializzato dai marker del main (o disabilitato se assenti),
    sovrascrivendo il preview-range stantio ereditato da una sotto-scena.
    Elimina il leak alla radice. Funziona in **ogni workflow** e a qualsiasi
    profondità di nidificazione.

### Upstream candidates
- **Per-xsheet In/Out markers** — la parte `txsheet.h/.cpp` (campi + API +
  serializzazione `<inOutMarkers>`) è pulita e proponibile a Tahoma2D così
  com'è. Gli agganci in `iocommand.cpp`/`subscenecommand.cpp` sono la logica di
  sync; la parte cross-dissolve (XD-in/XD-out) resta Ztoryc-specifica. Era già
  in lista come feature request — ora c'è un'implementazione di riferimento.

### Notes
- `ztorymodel.cpp` includeva una modifica **pre-esistente non committata**
  (pinning del play range a `[0, lastFrame]` in `resequenceXsheet`, + include
  `xsheetdragtool.h`) — inclusa in questo commit.
- **Bug aperto (bassa priorità)**: importando un `.psd` da Affinity (40 layer,
  blocco `Lr16` 16-bit, nomi layer vuoti) come libreria personaggio, e poi
  caricando quella scena come **sotto-scena** in un'altra, il layer più in basso
  risulta "not found". La scena originale aperta direttamente è OK; anche
  importare il PSD direttamente in una sotto-scena è OK. Da indagare
  (`tiio_psd.cpp` `REF_LAYER_BY_NAME`/`getLevelIdByName`, `psd.cpp` blocco
  `Lr16`). Workaround utente: aggiungere un layer sacrificale come primo (più in
  basso). Da loggare in `ANIMATIC_TASKS.md`.

---

## [2026-05-31] — Windows Storyboard startup crash fix + audio flicker + autoMatch perf + Render Tile default + workflow anti-flicker + Task 40 FASE 1

### Fixed
- **Flicker tracce audio durante operazioni** (`ztoryanimatic.cpp/.h`) —
  `refreshAudioTracks()` distruggeva e ricreava tutti i widget traccia ad ogni
  modello cambiato (anche aggiungendo/clonando shot, che NON tocca l'audio),
  causando lo sfarfallio. Fix: fast-path che matcha le tracce esistenti per
  puntatore `TXshSoundColumn*` (stabile agli shift di indice colonna) e aggiorna
  in-place (`setColumnIndex` + `invalidateWaveform`) senza ricreare i widget.
  Rebuild completo solo se la struttura delle colonne audio cambia davvero.
- **Lentezza con autoMatch attivo** (`ztoryanimatic.cpp`) — il loop che cerca
  la colonna corrispondente alla sub-scena aperta era O(col × righe): controllava
  ogni frame di ogni colonna ad ogni `xsheetChanged`. Fix: O(col) — controlla solo
  la prima cella per colonna (tutte le celle di un shot puntano allo stesso livello).
- **Crash avvio Windows entrando nel workflow Storyboard** (`storyboardpanel.cpp`)
  — `m_scrollArea` usava la policy default `ScrollBarAsNeeded`. L'altezza dei
  `PanelWidget` dipende dalla larghezza (preview con aspect-ratio): la scrollbar
  verticale che appare/sparisce cambia la larghezza del viewport → larghezza
  pannello → altezza pannello → ri-toggle scrollbar, oscillando. Su Windows
  l'eventFilter interno di `QScrollArea` (setWidgetResizable) gira sincrono e
  ricorsivo → stack overflow → uscita silenziosa + dump 0 byte. Si manifestava
  solo alla geometria transitoria d'avvio (Storyboard scelto dal popup iniziale;
  passare alla room dopo, a finestra dimensionata, non crashava). Fix: pinnare le
  policy (verticale AlwaysOn = larghezza costante, orizzontale AlwaysOff). Stessa
  classe del fix già presente sui QTextEdit del file. Diagnosi confermata via
  backtrace `QScrollArea::eventFilter ↔ resize` ×446 + repro tester. Commit `5af4a994f`.
- **Sfarfallio grafico al cambio workflow** (`mainwindow.cpp`) — `switchRoomChoice`
  fa `clearRooms()`+`readSettings()` e `Room::load()` chiama `processEvents()`,
  quindi ogni stato intermedio delle room veniva dipinto (le "ghost windows"
  ansiogene, evidenti su Windows). Fix: `setUpdatesEnabled(false/true)` attorno
  alla ricostruzione → si dipinge solo lo stato finale.
- **NON modificata la soglia RAM cache** (`tsystempd.cpp`) — un tentativo di
  alzarla 14.3%→25% per contenere l'uso RAM su scene pesanti rendeva l'eviction
  troppo aggressiva e **crashava il Save All** (raster liberato durante
  `TRasterCodecLZO::compress`). **Revertito** al 14.3% shipped. L'ottimizzazione
  RAM va rifatta in modo mirato (task 41) senza toccare l'eviction durante i save.
- **BUG-CAMERA — dati stale in SB_APPENNINGERS.tnz** — 4 sub-scene avevano
  `cameraSize: 12 6.75` (valore baked prima del fix). Corretto via script Python
  → tutte e 46 le camere ora `16 9`. Backup `.tnz.rtkcam` conservato.
- **BUG-CAMERA — confermato risolto nel codice** — test su scena nuova: camera
  main e sub-scena corrispondono perfettamente anche cambiando F e Z. Era un
  problema di dati stale, non di codice.

### Added / Changed
- **Render Tile default = Small** (`outputproperties.cpp`) — il default era
  `None` (frame intero), che su scene di minuti fa gonfiare la cache immagini
  oltre la RAM fisica fino allo swap (osservato: 17 GB su Mac da 16 GB durante
  un render full). Small tiene basso il footprint per-operazione. Solo per scene
  nuove; le esistenti mantengono il valore salvato nel `.tnz`.
- **Task 40 FASE 1 — Pannello Camera Moves** (`ztoryannotations.h/.cpp`) —
  Pannello "Ztoryc Camera Moves" nel menu Panels → Ztoryc con 8 pulsanti
  freccia Pan (4 ortogonali + 4 diagonali). Crea automaticamente una colonna
  PLI "Annotazioni" nella sub-scena corrente (con `sl->setScene(scene)` prima
  di `setFrame` per evitare crash). Frecce vettoriali centrate nel frame,
  editabili con tool di selezione nativo.

### Reverted (regressione introdotta e annullata in sessione)
- **Patch crash palette/style ritirate** — durante la sessione ho tentato di
  fixare il crash `StyleEditor::onStyleSwitched` (famiglia palette-switch
  ri-entrante) con modifiche a `TPaletteHandle` (riferimento forte), guard di
  re-entrancy in `onStyleSwitched`, `PaletteViewer::onFrameSwitched`,
  `toonzrasterbrushtool`, `tooloptions`. Queste patch hanno introdotto una
  **regressione** (crash al disegno anche su scene nuove) e sono state **tutte
  revertite** al baseline. Lezione: non patchare a scatola chiusa un cascade core
  complesso deployando build a metà.

### Notes
- **Crash StyleEditor (pre-esistente) ancora aperto** — re-entrancy nella cascata
  `updateXshLevel → setPalette → editLevelPalette → setPalette → onStyleSwitched`
  (palettecontroller.cpp:65). Da affrontare con build di debug + lldb, NON a
  tentativi. Il crash inseguito in sessione era in gran parte causato dalle mie
  patch parziali già deployate: al baseline la scena non crasha più.
- **Task 40 REDESIGN** — FASE 2 riprogettata: annotazioni camera-move automatiche
  leggendo i parametri del Camera1 pegbar (X/Y→Pan/Tilt, Z→Truck, Scale→Zoom).
  Overlay sul thumbnail BOARD, non colonna PLI separata. Vedi ANIMATIC_TASKS.
- **Regola memoria: aggiungere pannelli al menu Panels richiede 4 file** —
  `tpanels.cpp` (OpenFloatingPanel), `menubar.cpp` (sottomenu Ztoryc hardcoded),
  `stuff/profiles/layouts/menubar.xml`, e
  `~/Library/Application Support/Ztoryc/Ztoryc/profiles/layouts/menubar.xml`.
  Documentato in memory/feedback_ztoryc_panel_menu.md.

---

## [2026-05-31] — Release v0.3.5 + crash fix brush + diagnosi BUG-CAMERA

### Released
- **Ztoryc v0.3.5** pubblicata (Windows + macOS via CI) con tutto il lavoro sotto.
  Release note con sezione macOS `xattr` + raccomandazione clean install Windows.
  Bump `ZtorycVersion.cmake` 0.3.4 → 0.3.5 (`5fb3ea3e7`).

### Fixed
- **Crash SIGSEGV al doppio-click su uno shot col brush raster attivo** (`8f8740628`)
  — `onShotDoubleClicked → openSubXsheet → switch xsheet/palette →
  ToonzRasterBrushTool::onColorStyleChanged()` rientrante su stato di tratto pendente
  (`m_tileSaver` ancora set, immagine sbagliata post-switch) → use-after-free.
  Aggiunta guardia `m_inColorStyleChanged`.

### Notes (da riprendere)
- **BUG-CAMERA RIAPERTO (priorità ALTA)** — A/B test (baseline `e8d4a1466`) ha
  confermato che l'animatic-camera ≠ shot-camera **NON è una regressione recente**:
  è il design del fix `7d1746f3a` (animatic forzato sulla camera MAIN). Prima del fix
  l'animatic usava la camera SUB e combaciava (ma SH010 off-screen, offset x=13.4).
  Fix corretto da fare: usare la camera SUB risolvendo il compositing sub→parent di
  SH010, senza ri-ancorare al main. Diagnosi completa in ANIMATIC_TASKS.
- **Crash StyleEditor su click shot** (`Crash-20260531-015855`) — famiglia
  palette-switch, diversa da quella del brush (non coperta dal fix). Da indagare.
- **Bug QScrollArea layout-recursion (Windows)** e **UX camera-view editing** —
  segnalati in ANIMATIC_TASKS.

---

## [2026-05-31] — Transizioni cross-dissolve + fix timeline animatic + autofill undo/only-new

### Added
- **Transizioni cross-dissolve** (`7ccba5721`, `6c245442e`) — handle Alt+drag sul
  bordo destro di uno shot nella timeline animatic imposta la durata della
  dissolvenza; triangoli arancioni visualizzano l'overlap su entrambi i lati +
  etichetta frame. Inserisce fisicamente T/2 frame extra in coda alla sub-scena A
  e T/2 in testa a B (frame ID nel main xsheet shiftati di +T/2 così l'animatic
  mostra ancora il contenuto originale). Colonne note SoundText `XD-out`/`XD-in`
  marcano i frame extra in SHOTEDITOR e sono la **fonte di verità** persistente:
  triangolo e mark-out si derivano contandole, quindi sopravvivono al reload senza
  dipendere dal timing del `.ztoryc`. Mark-out esteso per coprire i frame della
  dissolvenza (mark-in resta a 0). Durata board sempre al netto degli extra.
  `detectAndUpdatePanels` salta le colonne Sound/SoundText (niente panel spurio).
- **Multi-select tracce audio + group move** (`9699c06a7`) — Ctrl/Cmd+click
  seleziona segmenti su più tracce; trascinando un segmento selezionato tutti si
  spostano dello stesso delta, in un unico step di undo (`TUndoScopedBlock`).
- **Snap (magnete)** (`8a6ad68c5`) — toggle in toolbar animatic (default ON):
  i bordi trascinati di audio e blocchi video si agganciano (entro 8px) a confini
  shot, playhead e bordi di altri segmenti audio. Bottone con glifo "U" segnaposto
  (icona vera rimandata — le icone sono un capitolo a parte).

### Fixed
- **Zoom-to-cursor timeline** (`546c4712a`) — forza la larghezza del content prima
  di riposizionare la scrollbar così `setValue` non viene clampato al massimo
  pre-zoom: il frame sotto il cursore resta fisso.
- **Razor — vista non salta** (`546c4712a`, `39c37bc4f`) — sia il razor video che
  quello audio mantengono la posizione di scroll; il path audio (`onAudioRazorRequested`)
  era quello che faceva saltare a ~1690 (estensione audio gonfiata post-taglio).
  `refreshFromScene` ora preserva sempre lo scroll (copre paste/cut/delete/drag);
  reset a 0 solo allo switch di scena.
- **Incolla audio al playhead** (`39c37bc4f`) — Ctrl+V incolla alla posizione del
  playhead invece che sulla selezione di copia persistente.
- **Autofill undo/redo + solo forme nuove** (`25ad78f53`) — l'autofill del brush
  smart-raster: (1) undo lasciava artefatti di riempimento e redo perdeva il fill →
  nuovo `AutoFillUndo` dedicato (tile prima/dopo) raggruppato col tratto; (2) riempiva
  TUTTE le regioni chiuse ad ogni tratto (anche forme preesistenti vuote) → ora
  riempie solo le regioni il cui contorno tocca l'inchiostro del tratto corrente,
  rilevato via flood-fill delle regioni + adiacenza all'inchiostro dentro il
  **footprint del tratto preso dai tile dell'undo** (coordinate raster affidabili;
  `m_strokeRect`/`m_points` erano vuoti coi brush hard/pencil).

### Notes
- **Design task 40 approvato** — Sistema Annotazioni Camera-Move + Light Direction
  (unifica 35/36/37): simboli parametrici + libreria PLI, colonna vettoriale per
  shot, toggle render. Piano a 3 fasi salvato in ANIMATIC_TASKS.md (prossima priorità).

### Upstream candidates
- **AutoFill undo/redo fix** (`toonzrasterbrushtool.cpp`) — il fix dell'undo che
  lascia artefatti e del redo che perde il fill è applicabile a qualsiasi build con
  l'autofill brush. Da valutare per PR se l'autofill è feature condivisa.

---

## [2026-05-31] — BUG-CAMERA fix + audio scrub + marker timeline + sync selezione

### Fixed
- **Preset camera vuoto + prevenzione camere sub non standard (`09dc82463`)** — il
  combo preset in Camera Settings restava vuoto anche con camera = preset (es. HD
  1920x1080): `updatePresetListOm()` faceva solo il reset a `<custom>`, mai il
  forward-match. Ora cerca e seleziona il preset corrispondente (estratto in
  `presetMatchesFields()`, con fx/fy init a -1 per i preset a 3 token). Stesso
  forward-match aggiunto alla Startup popup su nuova scena. **Prevenzione causa
  radice BUG-CAMERA**: ogni path che crea una sub-scena nuova/vuota ora forza la
  camera della sub = camera del main (res+size). Logica estratta in
  `syncChildCameraToMain`/`animSyncChildCameraToMain` e applicata ai paste-fallback
  prima scoperti. Clone NON toccato (reincornicerebbe i keyframe), sub esistenti
  non modificate. Candidato PR upstream: il forward-match in `camerasettingswidget.cpp`.
- **Sequenze/numerazione non persistite nel `.ztoryc` (`871aea839`)** — `saveZtoryc()`
  salvava solo numero/label dei singoli shot: riaprendo la scena le sequenze
  sparivano (3 sequenze → sequenza unica). Ora il `.ztoryc` salva anche
  `<numbering>` (NumberingConfig: stile Simple/Sequence, prefissi, step, padding,
  seqNumber, resetOnSeqChange), gli elementi `<sequence>` (uuid/label/order) e il
  `sequenceId` di ogni shot. `loadZtoryc()` azzera le sequenze a inizio caricamento
  (no leak tra scene) e ripristina config/sequenze/sequenceId nel board e nel model;
  `renumberAll()` post-load preserva il raggruppamento. File vecchi retro-compatibili.
  ⚠️ Le scene salvate PRIMA del fix vanno ricreate+risalvate una volta.
- **BUG-CAMERA risolto** (`7d1746f3a`) — il monitor (viewer always-main) ora resta
  ancorato alla camera del MAIN xsheet anche dentro uno shot, invece di usare la
  camera della sub-scena. 6 punti in `sceneviewer.cpp`: `drawBuildVars()` (camera
  top-xsheet), skip dell'affine ancestrale per always-main inside-sub,
  `getViewMatrix()` CAMERA_REFERENCE (inverte la camera main, non la sub —
  risolveva il contenuto fuori schermo su SH010 con offset x=13.4),
  `fitToCamera()`/`fitToCameraOutline()` (rect dalla camera main), `getCameraRect()`,
  e `drawOverlay()` con `cameraRectAff = m_drawCameraAff * TScale(main/sub size)`
  per maschera/contorno/safe-area che `ViewerDraw::getCameraRect()` calcolava ancora
  sulla sub.
- **Scrub audio main da dentro lo shot muto (regressione, `06423030b`)** —
  `onNativeFrameSwitched` usava `scrubDevice()`, un `TSoundOutputDevice` grezzo mai
  aperto col formato audio → nessun suono. Routing su `mainXsh->play()`, stesso path
  dello scrub del ruler animatic (che funziona). Il play funzionava perché usa il
  device gestito di ogni colonna.
- **Delete audio track non funzionava** — `ColumnCmd::deleteColumns` opera sulla
  current xsheet (la sub dentro uno shot). Guard al livello main + undo +
  `notifyXsheetSoundChanged` + refresh esplicito. Menu tasto-destro sull'area
  etichetta della traccia audio.
- **Cursore roll-edit nell'xsheet non compariva** — `CellArea::mouseMoveEvent`
  reimpostava ArrowCursor ad ogni movimento. Guard `!getDragTool()` + `setCursor`
  SplitV/SplitH ai call-site del `LevelRollingTool` (Alt sul confine tra due celle).
- **Nomi colonne sub-scene = "SH010"** — `StoryboardPanel::updateColumnName` usava
  `scene->getXsheet()` (la sub se dentro uno shot). Cambiato in `getTopXsheet()`.

### Added
- **Task 39 — highlight shot attivo** (`043b5020b`) — entrando in edit-shot, il blocco
  attivo nella timeline animatic riceve glow magenta (`#E0249B`) + bordo 2px. Colonna
  attiva da `ChildStack::getAncestorInfo(0)->m_col`; repaint su `xsheetSwitched`.
- **Marker / navigation tag nella timeline animatic** (`7182b5543`) — i navigation tag
  della main xsheet disegnati nel ruler con la forma nativa Tahoma
  (`PredefinedPath::NAVIGATION_TAG`, pin a goccia). Etichetta solo in hover, click
  sinistro posiziona il playhead, tasto-destro Add/Edit/Remove. L'edit riusa il popup
  nativo `NavTagEditorPopup` (testo + colore). Persistono con la scena.
- **Sync selezione Board↔Animatic** (`7182b5543`) — `ZtoryModel::setSharedSelection`
  emette `sharedSelectionChanged()`; selezionare una clip nella timeline evidenzia lo
  shot nel Board e viceversa. Guardie no-op anti-loop.

### Upstream candidates
- Il cursore roll-edit (SplitV/H) e il guard `!getDragTool()` in `xshcellviewer.cpp`
  sono fix puliti riproponibili upstream.

### Notes
- `build_and_deploy.sh` riapre automaticamente l'app dopo il deploy.

---

## [2026-05-30b] — Findings BUG-CAMERA (tentativo monitor-white revertato)

### Reverted
- **Tentativo fix monitor-bianco (`5f335a295`) revertato in `6901cd844`** — era
  basato su una premessa sbagliata: credevo che TUTTI gli shot fossero bianchi nel
  monitor entrando in sub-scena, ma l'utente ha confermato che **solo il primo shot
  (frame 0) è sempre stato bianco**, sia prima che dopo. Rimuovendo l'affine
  ancestrale in `sceneviewer.cpp` non ho risolto il bianco e ho introdotto una
  discrepanza di inquadratura monitor vs viewer nativo. Ripristinato il codice
  originale (l'affine ancestrale fa combaciare il monitor con la sub-scena).

### Notes
- **Confermato dall'utente:** solo il **primo shot (frame 0)** diventa bianco nel
  monitor, comportamento preesistente e indipendente dai fix di questa sessione.
- I valori camera divergenti (F/Z) sono lo **Stage transform della colonna camera**
  (N/S/E/W/Z) — è **BUG-CAMERA** (camera main ≠ camera sub), preesistente. Findings
  dettagliati in ANIMATIC_TASKS.md. Da affrontare in sessione dedicata con test
  interattivo. NON ritoccare l'affine ancestrale in sceneviewer.cpp.

### BUG-CAMERA — diagnosi completa (analisi .tnz SB_APPENNINGERS)
- **Root cause confermato:** mismatch di `cameraSize` tra sub-scena e main. Main =
  16×9; su 59 sub-scene, 4 sono anomale (3× `12 6.75`, 1× `12 9`). SH010 è `12×6.75`
  con offset x=13.4 → nel monitor finisce fuori frame → bianco. Spiega "solo il
  primo shot bianco".
- **Decisione utente:** NON toccare i dati camera (la camera piccola è legittima,
  l'animatic puro la mostra giusta). Il fix è SOLO nel rendering del monitor: deve
  restare ancorato alla camera del MAIN anche dentro lo shot (come animatic puro),
  invece di scendere in edit-in-place sulla camera della sub.
- **Piano implementazione** (3 punti in sceneviewer.cpp: affine ancestrale, camera
  di riferimento, re-fit on scene-switch) salvato in ANIMATIC_TASKS.md → BUG-CAMERA.
  Richiede sessione dedicata con iterazione a test visivi su SH010.

---

## [2026-05-30] — Script per-scena: fix binding scena↔sceneggiatura

### Fixed
- **Script importato condiviso tra scene (BUG-SCRIPT-CROSS)** — lo script restava
  caricato cambiando scena e si mescolava tra progetti diversi. Due cause:
  1. **Posizione errata** — l'import scriveva in `+extras/script/` (livello
     progetto, condiviso da TUTTE le scene). Ma il progetto ha
     `<folder name="extras" useScenePath="yes"/>`: gli extras sono per-scena
     come i drawings. Ora l'import va in `+extras/<scena>/script/` replicando
     `getDefaultLevelPath()` (`+extras + getSavePath() + "script"`).
  2. **Load fragile** — il caricamento/clear dello script era un side-effect di
     `StoryboardPanel::loadZtoryc()` (dipendeva dall'esistenza/refresh della
     Board). `ZtoryScriptView` ora si connette direttamente a
     `TSceneHandle::sceneSwitched` e legge il tag `scriptFile` dal `.ztoryc`
     della scena corrente — autoritativo e indipendente dalla Board. Scena senza
     script → panel vuoto.

### Notes
- **Migrazione "solo nuovo schema"**: le scene vecchie mantengono il path
  `+extras/script/...` (risolve finché il file esiste); reimportare lo script lo
  sposta nella cartella per-scena. Nessuna migrazione automatica dei file esistenti.
- I file `.ztoryc` esistenti potevano avere tag `scriptFile` contaminati (3 scene
  puntavano allo stesso "Il Palazzo Scomparso v7.fdx") — risolto al re-import.

---

## [2026-05-29] — Mark-out fix, Monitor sub-scene guard, Clone camera keyframes + task reconcile

### Fixed
- **Mark-out main blocca play animatic (BUG-MARKOUT)** — la timeline animatic
  usava `XsheetGUI::getPlayRange()` (mark-out del native xsheet) in 4 punti di
  playback/audio. Se stale (impostato dentro una sub-scena o da sessione
  precedente) il play si fermava al frame sbagliato. Sostituito con
  `ZtoryAnimaticController::getAnimaticPlayRange()` (range proprio dell'animatic)
  in tutti e 4 i punti. Aggiunto `ZtoryAnimaticRuler::clampPlayRangeToTimeline()`
  chiamato dopo ogni `resequenceXsheet()`: riduce il mark-out se oltre la nuova
  durata (shots cancellati/accorciati).
- **Monitor track si azzera entrando nello shot** — `refreshFromScene()` chiamava
  `getTopXsheet()` senza guard `ancestorCount`; dentro una sub-scena restituiva
  la sub-scena e svuotava i blocchi della track animatic. Aggiunto guard
  `ancestorCount == 0` nel timer callback e nello `showEvent`.
- **Clone non copiava i keyframe camera** — `cloneChildToPosition()` usava
  `storeColumns()` che serializza solo `ColumnId`; la camera (`CameraId(0)`) non
  veniva mai memorizzata. Fix: `storeObjects()` con IDs espliciti incluso
  `CameraId(0)` → `restoreObjects()` chiama `restoreCamera()` → `assignParams()`
  copia tutti i keyframe (posizione, rotazione, zoom).
- **Cross-scene text contamination (BUG-TEXT-CROSS)** — già committato a inizio
  sessione: `m_currentZtoryPath` lega il save path al ciclo di vita di `m_shots`.

### Modified
- **ANIMATIC_TASKS.md riconciliato** — c'erano 3 file con date diverse (2205, 2305,
  canonico) e numerazione task incoerente. Pulito il canonico: marcati DONE i task
  32/33/34/30/31/25/26/27/28 + ffmpeg/PDF/Windows-installer (già fixati/superati).
  Aggiunti task aperti: 39 (feedback visivo shot editing), BUG-CAMERA (discrepanza
  camera main vs sub-scene), e BUG-MARKOUT (poi fixato). Priority order ora inizia
  da task 39 + BUG-CAMERA, poi 35 (Arrow Tool), 38 (Room Traditional), Kitsu.

### Notes
- **Task 25 In/Out Marker** marcato superato: approccio semplificato con `inPoint`
  fisso a 1, Roll/Slide funzionano via trim su `outPoint` (durata).
- **Windows installer** — crash utente (`onViewerDestroyed` entry point not found)
  era da mixed install: Ztoryc installato in `C:\Program Files\Tahoma2D\` con vecchie
  DLL T2D in PATH. Bug installer già fixato; workaround utente: disinstallare T2D prima.
- **Rimangono aperti:** task 39 (feedback visivo shot editing), BUG-CAMERA.

---

## [2026-05-27e] — Release v0.3.4: Monitor keyboard, thumbnails, version bump

### Added
- **ZtoryMonitorPanel — keyboard shortcuts** — Del/Backspace, Cmd+C/X/V/D/N attivi
  quando il track del Monitor ha il focus; ShortcutOverride per prevenire che
  CommandManager intercetti i tasti prima del panel.
- **Release checklist in AGENTS.md** — sezione permanente con procedura per release,
  istruzioni macOS `xattr -cr`, e regola "diff dal tag precedente prima di scrivere note".

### Fixed
- **Monitor delete button** — implementazione diretta senza delegare a
  `findAnimaticPanel()` (null se la stanza Animatic non era mai aperta). Undo via `UndoBoardState`.
- **ZtoryStoryStrip thumbnails vuote** — `renderXsheetFrame()` + cache per colonna
  (sostituisce `ZtoryModel::preview()` che restituiva null su sub-scene columns).

### Modified
- **Versione bump → 0.3.4**

### Upstream candidates
- **`thirdparty.cpp` ffmpeg autodetect** — ❌ NON candidato PR: bug specifico del bundle Ztoryc (`Contents/Resources/ffmpeg`). In Tahoma2D l'ffmpeg è nella stessa cartella dell'eseguibile e veniva già trovato — non affligge gli utenti Tahoma2D.
- **`tcodec.cpp` signal deadlock** — ✅ Candidato PR: `sigprocmask` attorno a `QProcess::start()` in `lzoCompress`/`lzodecompress`. Interessa tutti gli utenti Mac/Linux di Tahoma2D. Alta priorità.

### Notes
- Release note v0.3.4 approvate dall'utente prima del commit (diff-based, categorizzate).
- Regola stabilita: prima di release note, sempre `git diff <last-tag>..HEAD` per
  separare bug user-reported da fix interni mai arrivati all'utente.
- PDF quality fix incluso in v0.3.4 (300 DPI + pre-render + re-render a risoluzione cella).

---

## [2026-05-27d] — Fix cross-scene text contamination in Board

### Fixed
- **Cross-scene text contamination** — aprendo la scena 1 dopo aver editato la scena 2
  si ritrovavano i testi della scena 2. Root cause: `saveZtoryc()` usava `ztoryPath()`
  (scena attiva) mentre `m_shots` apparteneva ancora alla scena precedente, durante il
  window tra scene-switch e `clearShots()`. Fix: aggiunto `m_currentZtoryPath` che viene
  azzerato da `clearShots()` e impostato solo a fine `refreshFromScene()` — qualsiasi
  `saveZtoryc()` con `m_shots` stale diventa un no-op.

### Notes
- Regressioni da verificare nella prossima sessione:
  - **ffmpeg non funziona** + formati video assenti tra gli output (era già stato fixato)
  - **Risoluzione thumbnail PDF pessima** nel Board
- Task 32, 34, 31 risultano già completati in sessioni precedenti (da verificare/aggiornare ANIMATIC_TASKS)

---

## [2026-05-27c] — ZtoryMonitorPanel full toolbar + context chips + single-instance guard

### Added
- **ZtoryMonitorPanel full toolbar** — zoom slider, fit-all, select/trim/razor,
  add/delete/merge/copy/clone/paste, TC toggle. Toolbar posizionata tra viewer e
  timeline (sopra la traccia video).
- **Double-click sul Monitor** — apre la sotto-scena nel contesto principale
  (seek + play range + activateShotForViewing). Return button chiude la sub-scene.
- **Audio tracks nel Monitor** — refresh con fingerprint per evitare rebuild inutili.
- **Context chips** — badge colorati nella toolbar: "BOARD" (verde), "ANIMATIC" (blu),
  "MONITOR" (viola).
- **Single-instance guard** (task 33) — QLockFile in
  `~/Library/Caches/ztoryc/ztoryc_<user>.lock`. QMessageBox se seconda istanza.

### Modified
- **ztoryanimatic.h** — slot di edit spostati a `public slots:` per forwarding dal Monitor.
- **onDeleteShots/onCopyShots/onCutShots/onCloneShots** — usano sharedSelection come fallback.
- **StoryboardPanel** — `m_dirtyShotCol` tracking + detectAndUpdatePanels in
  contesto main-xsheet (legge sub-xsheet da TXshChildLevel senza iterare celle main).

### Notes
- **BUG APERTO (PRIORITÀ 1 prossima sessione):** testi Board spariscono ad ogni
  riapertura (nei primi panel soprattutto). Root cause non trovata via code
  inspection. Richiede test interattivo con l'app aperta. Da riprendere subito.
- Commit: `140d790ac`

---

## [2026-05-27b] — ANIMATIC_TASKS: arrow tool feature requests

### Added (task list only)
- **Task 35** — Storyboard Arrow Tool: freccia vettoriale disegnabile su arco/curva
  Bézier con arrowhead auto-calcolato dalla tangente dell'endpoint; opzioni
  inizio/fine/entrambi; integrazione con tool arco Tahoma2D.
- **Task 36** — Frecce 3D / Prospettiva: estensione del task 35 con frecce
  foreshortened per comunicare movimenti sull'asse Z. Variante 1 (2D stilizzata)
  prioritaria, variante 2 (gizmo 3D con proiezione camera) come futura iterazione.
- **Task 37** — Indicatore Direzione Luce: overlay non distruttivo nel panel
  per posizionare la sorgente di luce in 3D (angleH + angleV). Salvato nel .ztoryc
  come metadato, disegnato in PanelWidget::paintEvent() sopra il thumbnail.

### Notes
- Nessuna modifica al codice sorgente in questa sessione.

---

## [2026-05-27] — ZtoryMonitorPanel + camera view fix + RAM/performance fixes

### Added
- **ZtoryMonitorPanel** — nuovo pannello "Ztoryc Monitor" (secondo monitor): combina
  `ZtoryAnimaticViewer` + `ZtoryAnimaticRuler` + `ZtoryAnimaticTrack` in un QSplitter
  verticale. Doppio click cerca il frame (seek only, non apre sub-scene).
  Registrato in CMakeLists, menubar (`MI_OpenZtoryMonitor`), tpanels.cpp, mainwindow.cpp.

### Fixed
- **Camera rect drift in Camera View** — `SceneViewer::getViewMatrix()` usava il frame
  handle globale invece di `m_customFrameHandle`, causando disallineamento nel viewer
  animatico. Fix: usa `m_customFrameHandle` quando impostato.
- **RAM 50GB su apertura scena** (root cause 1) — `ZtoryAnimaticTrack::refreshFromScene()`
  usava `found = !b.thumbnail.isNull()`: se l'icona non era in cache iterava ogni cella
  × ogni layer della sub-xsheet → fino a 240.000 `getIcon()` calls per scena complessa.
  Fix: `found = true` alla prima cella trovata.
- **RAM 50GB su apertura scena** (root cause 2) — `onRefreshPreviews()` renderizzava tutti
  i pannelli (631 per la scena messina HARMONYA). Fix: sostituito con `updateVisiblePreviews()`.
- **SFH explosion repair** — `loadZtoryc()` rileva e collassa automaticamente i pannelli
  SFH-esplosi (> 20 pannelli, durata media ≤ 5 frame) → collassa a 1 pannello e riscrive
  il `.ztoryc` su disco. Soglia aggiornata: usa media invece di "tutti a 1 frame".
- **Scroll lento nel Board** — connessione `scrollBar::valueChanged → updateVisiblePreviews`
  debounced a 250ms. Prima: `renderXsheetFrame()` sincrono ad ogni tick dello scroll.
- **Skip thumbnail esistenti** — `updateVisiblePreviews()` salta i pannelli che hanno già
  un pixmap, evitando re-render inutili durante lo scroll di ritorno.
- **Freeze al caricamento con scene dense** — placeholder usa CSS + emoji 🎥 invece di
  `QPixmap(640×360)`. Prima: 631 allocazioni QPixmap sincrone nel loop `rebuildGrid()`
  bloccavano il main thread per ~6 secondi.
- **No auto-render all'apertura** — rimosso `QTimer::singleShot(500ms, updateVisiblePreviews)`
  da `refreshFromScene()`. Thumbnails renderizzati solo su scroll stop o "Refresh Previews".

### Notes
- v0.3.3 CI triggerato su GitHub Actions (macOS + Windows)
- `build_and_deploy.sh` fix: rilevamento automatico directory di build

## [2026-05-26] — SFH pipeline + split/merge/undo fixes + startup popup fix

### Fixed
- **Stop Frame Hold (SFH) in main xsheet** — `resequenceXsheet()` piazza ora una
  cella `STOP_FRAME` alla fine di ogni colonna shot nel main xsheet, impedendo che
  l'ultimo disegno di uno shot faccia implicit hold sul frame successivo durante
  playback/render animatic.
- **Split (Razor) durata corretta** — `ignoreLastStop=true` su `srcColumn->getRange`
  esclude la SFH da `totalDuration`/`secondHalf`: Shot 2 aveva 1 frame in più.
- **Split Shot 2 partiva da frame 0 invece che dal punto di taglio** — `materializeCells`
  riempiva solo fino a `lastContent`; se il punto di split era in zona implicit hold,
  `shiftChildXsheetBy` calcolava `keep < 0` e non spostava nulla. Aggiunto parametro
  `fillToEnd=true` per il caso split che riempie hold fino a `duration-1`.
- **SFH in sub-scena Shot 1 dopo split** — piazza SFH a `splitRel` in ogni colonna
  della sub-scena di Shot 1 per terminare la catena di hold al punto di taglio.
- **materializeCells non propaga SFH come hold** — celle SFH azzerano `last`.
- **Merge durata corretta** — `dstColumn->getRange(ignoreLastStop=true)`;
  `appendAt` ora sovrascrive la SFH esistente invece di appendere dopo di essa.
- **onMatchSubsceneDuration +1 frame vuoto** — `ignoreLastStop=true` + skip SFH
  nel backward scan: mark-out al frame precedente la SFH.
- **captureSnapshot contava SFH come frame reale** — `s.duration` gonfiato di 1
  → undo ricostruiva con 1 frame extra → timeline vuota. Fix: skip `isStopFrame()`.
- **Popup "Unable to create a new document" all'avvio** — rimossa `NSDocumentClass`
  da `CFBundleDocumentTypes` in `BundleInfo.plist.in`.
- **openSubXsheet mark-out inflato** — `ignoreLastStop=true` in `subscenecommand.cpp`.

### Notes
- Build CI macOS + Windows lanciate su commit `ef7e934ea`.
- Undo del razor: Board/animatic si ripristinano correttamente; contenuto interno
  della sub-scena non viene undone (limitazione architetturale — da risolvere con
  undo dedicato in futuro).

## [2026-05-25d] — Xsheet: cell swap, block swap, rolling edit + animatic TC sync

### Added
- **Cell swap** (`xsheetdragtool.cpp`) — `⌥ Option + drag` su una singola cella
  in xsheet: scambia il contenuto della cella sorgente con la destinazione (stessa
  colonna). Highlight arancione sulla destinazione durante il drag. Undo/redo.
- **Block swap** — stessa gesture con una selezione multi-frame preesistente: il
  blocco intero viene scambiato con un range uguale alla destinazione. La selezione
  segue il blocco dopo il rilascio.
- **Rolling edit** (`RollingEditTool`) — `⌥ Option + smart tab inferiore`: sposta
  il confine tra due livelli adiacenti senza cambiare la durata totale. Drag giù →
  cel corrente si allunga, il successivo si accorcia dall'inizio (e viceversa).
  `⌘ Cmd + ⌥ Option + smart tab superiore`: rolling edit verso l'alto. Linea
  verde indica il confine durante il drag. Undo/redo con capture lazy pre-drag.

### Fixed
- **TC non si aggiornava al cambio FPS** (`ztoryanimatic.cpp`) — `refreshFromScene()`
  ora chiama `m_ruler->setFps()` sincronizzando il timecode quando si modifica
  il frame rate da Scene Settings.
- **Delete shot nella toolbar animatic** posizionato correttamente accanto ad Add (+/-).

### Notes
- `CellSwapUndo` range-aware: gestisce sia swap singola cella che blocchi N frame.
- `RollingEditUndo`: cattura lo stato pre-drag lazily per frame toccati; redo
  ri-applica da stored before/after vectors.
- Hook Alt in `mousePressEvent` (xshcellviewer.cpp) intercettato prima del blocco
  level-range-selection per evitare che la selezione venga allargata all'intero livello.


## [2026-05-25c] — Animatic: zoom Ctrl+Scroll, Fit All, ruler adattivo

### Added
- **Ctrl+Scroll zoom** su tutto l'animatic — funziona su video track, audio track
  e ruler (prima solo sul ruler). Ctrl+Scroll → zoom, scroll plain → pan.
- **Pulsante Fit All `[]`** nella toolbar animatic — calcola il ppf esatto per
  vedere tutta la timeline in un click. Anche shortcut **Ctrl+0**.
- **Limiti zoom estesi**: min `0.02 ppf` (supporta 26 minuti a 24fps in ~750px),
  max `200 ppf` (editing frame-per-frame). Slider ricalibrato ×100.
- **`ZtoryAudioTrack::zoomChanged` signal** — connesso a `onZoomChanged` del panel.

### Fixed
- **Ruler adattivo** — label spacing completamente adattivo in entrambe le
  direzioni con serie base-10/5 (fps-agnostica: 1,2,5,10,25,50,100,250,500…):
  - Zoom in alto: label su ogni frame
  - Zoom normale (~8ppf): label ogni 5-10 frame
  - Zoom out estremo (26min): label ogni 2500-5000 frame senza accavallamento
  - Font ripristinato al default app (era `QFont("",8)` → spaziatura anomala)
- **`assignKeepNumbers()` crash su board vuota** (Windows) — accesso
  `m_shots[-1]` con board vuota. Fix: `if (total <= 1) return` early.

### Notes
- Thumbnail quality Board + Navigator: render a risoluzione fisica (ppf × DPR),
  rescalePreview DPR-aware, auto-resize su ridimensionamento finestra (150ms debounce),
  re-render su resize con previewRerenderNeeded signal + 200ms debounce.
- Min cella Board: 200→150px.


## [2026-05-25b] — Fix crash Board + su scena vuota (Windows)

### Fixed
- **`assignKeepNumbers()` crash su board vuota** (`storyboardpanel.cpp`) — con il
  primo shot (total=1, insertAt=0), la condizione `insertAt >= total-1` (0>=0) era
  vera e accedeva a `m_shots[-1]` → segfault su Windows (UB silenzioso su macOS).
  Fix: early return `if (total <= 1)` — nessun vicino da ereditare, ci pensa
  `renumberAll()`. In uso normale non si manifesta perché Ztoryc parte con uno shot
  già creato dalla startup dialog.

### Notes
- Build locale aggiornata; CI non triggerata (fix non urgente per utenti normali).


## [2026-05-25] — Script panel: import multi-formato (.fdx, .fountain, .docx, .odt, .txt)

### Added
- **`parseFountain()`** (`ztoryscriptpanel.cpp`) — parser completo del formato Fountain
  (open standard per sceneggiature): scene heading, character, dialogue, parenthetical,
  action, transition, lyrics, boneyard, note inline. Title page saltata automaticamente.
- **`parseDocx()`** — parser cross-platform per `.docx` (Word 2007+): estrae
  `word/document.xml` dallo ZIP con un lettore zlib custom (nessuna dipendenza esterna),
  legge gli stili di paragrafo Word. Se nessuno stile è riconosciuto come screenplay,
  ricade sull'euristica di `parseTxt()`.
- **`parseOdt()`** — parser cross-platform per `.odt` (LibreOffice/OpenDocument): estrae
  `content.xml`, gestisce `<text:span>` annidati tramite `readElementText()`, stili ODF.
  Stesso fallback euristico di DOCX.
- **`parseTxt()` migliorato** — ora rileva automaticamente se il testo è una
  sceneggiatura (conta le scene heading SC\d+ / INT. / EXT.): se sì applica la
  formattazione visiva identica a FDX; se no restituisce il testo grezzo.
- **zlib linkata esplicitamente** (`toonz/sources/toonz/CMakeLists.txt`) al target
  Ztoryc via `find_package(ZLIB REQUIRED)` — necessaria per il decompressore ZIP dei
  parser DOCX/ODT.
- **Word wrap** nel panel script — `WidgetWidth` invece di `NoWrap`: le righe si
  adattano alla larghezza del panel mantenendo la formattazione (indentazioni, dialoghi).
- **Conversione .doc → .fdx** via script Python standalone (`/tmp/convert_fdx.py`):
  usato in sessione per convertire "Il Palazzo Scomparso v7.doc" → `.fdx`.

### Fixed
- **DOCX fallback**: paragrafi vuoti ora inclusi nel flat text → l'euristica riceve
  le righe vuote separatrici indispensabili per distinguere i blocchi dialogo/azione.
- **ODT**: il `continue` iniziale nel loop XML filtrava tutti gli `isCharacters()` →
  testo completamente vuoto. Fix: rimosso il filtro; `<text:span>` gestito con
  `readElementText(IncludeChildElements)`.
- **ODT style-name**: attributo ora letto correttamente via namespace URI esplicito
  (`urn:oasis:names:tc:opendocument:xmlns:text:1.0`) invece di `contains("text")`.

### Notes
- `.doc` (formato binario legacy) non supportato cross-platform: mostra messaggio
  che invita a riesportare come `.docx`.
- File dialog e drag & drop ora accettano: `.fdx`, `.fountain`, `.docx`, `.odt`,
  `.doc`, `.txt`.

---

## [2026-05-25] — Qualità anteprime Board e Navigator

### Fixed
- **ZtoryPanelNavigator preview sfocata** (`ztoryanimatic.cpp`) — rendering fisso a
  320×180 px indipendentemente dalla dimensione del label e dal DPR. Fix: render al
  `label_size × devicePixelRatio` (cappato a 1280×720), pixmap taggato con
  `setDevicePixelRatio`, display senza upscaling. Su Retina il navigator mostra
  immagini pixel-perfect.
- **Board thumbnails sfocate su Retina e su pannelli larghi** (`storyboardpanel.cpp`):
  - `updatePreview()`: render a `panel_width × DPR` fisici invece di 320×180 fisso.
  - `rescalePreview()`: ora DPR-aware — scala a pixel fisici e tagga con
    `setDevicePixelRatio` prima di passare al QLabel.
  - `PanelWidget::resizeEvent()`: se la larghezza fisica richiesta supera del 20%
    quella del pixmap memorizzato, emette `previewRerenderNeeded(si, pi)`.
  - `connectPanelWidget()`: debounce 200ms su `previewRerenderNeeded` — ri-renderizza
    in coda solo i pannelli che ne hanno bisogno, una sola volta dopo il resize.

### Added
- **Auto-resize Board su ridimensionamento finestra** (`storyboardpanel.cpp`) —
  `StoryboardPanel::resizeEvent` (nuovo) con debounce 150ms: ricalcola `colW` dal
  viewport e aggiorna `setFixedWidth` su tutte le celle. Le celle si adattano in
  tempo reale alla finestra senza toccare il numero di colonne.
- **Dimensione minima celle ridotta a 150 px** (era 200 px) — permette più colonne
  visibili su schermi stretti o con molti shot.

## [2026-05-24c] — Post-release fix: crash Cutout Digital + upstream candidates

### Fixed
- **TasksViewer crash on room switch** (`tasksviewer.cpp`) — `~TasksViewer()` vuoto
  lasciava puntatore dangling in `BatchesController::m_tasksTree`; switchando su
  Cutout Digital crashava in `QHeaderView::setModel()`. Fix: `setTasksTree(nullptr)`
  nel distruttore. 1 riga. Commit `1569cf2cc`. ✅ Pronto per PR upstream Tahoma2D.

### Notes
- Lista upstream PR candidates aggiornata in AGENTS.md con priorità e checkbox
  "da verificare su Tahoma2D". TasksViewer crash è il primo verificato e pronto.
- Nuovo build macOS in corso (deployment target 12.0 + tutti i fix del giorno).
  Sostituirà i DMG in v0.3.2 quando finisce (~80 min).

---
## [2026-05-24b] — Post-release fix: FFmpeg, Gatekeeper, deployment target

### Fixed
- **FFmpeg dylib path nel bundle**: `dylibbundler` scriveva `@executable_path/../libs/`
  aspettando layout `bin/`, ma i binari finiscono direttamente in `Resources/ffmpeg/`.
  Fix: `install_name_tool` in `tahoma-buildpkg.sh` riscrive tutti i riferimenti a
  `@executable_path/libs/` dopo la copia (sia binari che dylibs cross-ref).
  Risultato: formati video ora visibili nel render.
- **"Unable to create a new document" all'avvio**: macOS session-restore machinery
  attivata perché mancavano `NSQuitAlwaysKeepsWindows=false` e
  `NSApplicationSupportsSecureRestorableState=true` in `BundleInfo.plist.in`.
- **"Non puoi usare questa versione" su macOS 12-14**: nessun `CMAKE_OSX_DEPLOYMENT_TARGET`
  impostato → il runner macOS 15 embed `minos 15.0`. Aggiunto `-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0`
  in `tahoma-build.sh` (allineato a Tahoma2D upstream). Nuovo build macOS triggerato.
- **Windows CI**: fix `publish_release` input — rimossa dipendenza da
  `github.repository_owner == 'tahoma2d'`.

### Notes
- Percorso immagine DMG drag-and-drop: `ci-scripts/osx/assets/ztoryc-dmg-background.png`
  (generata da `render-dmg-background.py`)
- Fix FFmpeg applicato manualmente all'app installata (senza rebuild)
- Nuovo build macOS in corso per v0.3.2 con deployment target 12.0

---
## [2026-05-24] — Release v0.3.2: fix CI macOS + pacchetti Windows/macOS

### Fixed
- **CI macOS: build falliva su "Package portable app and DMG"**: Homebrew ha rimosso
  la formula `libtiff44`. `tahoma-install.sh` ora builda libtiff 4.4.0 da sorgenti
  in `/tmp/libtiff44-build`; `tahoma-buildpkg.sh` legge `libtiff.5.dylib` da lì.

### Added
- **Windows CI: `publish_release` workflow_dispatch input**: il workflow Windows ora
  ha lo stesso meccanismo di macOS — triggerando con `publish_release=true` carica
  installer + portable zip direttamente nella GitHub Release.

### Notes
- Release v0.3.2 pubblicata con tutti e 4 i pacchetti (2 DMG + exe + zip).
- Percorso QSS temi: `/Applications/Ztoryc.app/Contents/Resources/tahomastuff/config/qss/`
- Fix macOS Gatekeeper "file danneggiato": `xattr -cr /Applications/Ztoryc.app`

---
## [2026-05-25] — Shot Board: fix preview, label e titolo viewer

### Fixed
- **Shot Board preview sempre su shot 010**: `ZtoryModel::m_shots[si].xsheetColumn`
  era sempre 0 (default) perché `syncShotPanels` non lo propagava. `refreshPreview()`
  usa `shot.xsheetColumn` per scegliere da quale colonna rendere la thumbnail: con
  valore 0 fisso, tutti gli shot mostravano la thumbnail di SH010. Ora `syncShotPanels`
  accetta un parametro `xsheetCol` (opzionale) e lo scrive in `m_shots[si]`. Tutti e 4
  i call site nel Board ora passano `m_shots[i].data.xsheetColumn`.
- **Shot Board header "  -  1 panels"** (label vuoto): `syncShotPanels` non propagava
  il `shotLabel`. Aggiunto parametro `label` opzionale; ogni call site passa
  `m_shots[i].data.shotLabel`. Dopo `renumberAll()` in `refreshFromScene` viene
  eseguita una seconda bulk-sync per aggiornare ZtoryModel con i label finali
  (prima i label erano quelli pre-renumber del .ztoryc, o vuoti se nessun .ztoryc).
- **Titolo viewer sempre "Ztoryc Viewer"**: `ZtoryAnimaticViewerPanelFactory::createPanel()`
  chiamava `panel->setWindowTitle("Ztoryc Viewer")` dopo il costruttore, annullando
  `updateTitle()`. Spostato `updateTitle()` in un `QTimer::singleShot(0, ...)` così
  viene eseguito nell'event loop successivo, dopo il factory. Ora mostra
  "Animatic - scenename" in main e il label shot ("SH020" ecc.) in shot editing mode.

### Modified
- `ZtoryModel::syncShotPanels`: firma estesa con `label = {}` e `xsheetCol = -1`
  (parametri opzionali, backward-compatible con tutti i call site esistenti)
- `StoryboardPanel::refreshFromScene`: bulk sync aggiuntivo dopo `renumberAll()`
  per garantire che ZtoryModel abbia i label finali post-renumber

---
## [2026-05-24] — Sessione breve: sync task list da Drive

### Notes
- Sessione post-compact molto carica di contesto, nessuna modifica al codice.
- Letta la nuova `ANIMATIC_TASKS (1).md` (2026-05-23) preparata da Claudio su
  Drive. La nuova Priority Order mette in cima:
  1. MOD UI Headers (viewer contestuale, left `BOARD/SHOT`, right `SCRIPT/PALETTE`)
  2. NEW Single-instance guard (`QLockFile` in `main.cpp`)
  3. NEW Room "Ztoryc T" + Panel Navigator + rinomina "Ztoryc X" +
     rimozione Browser dal right panel di entrambe le room
  4. PERF Board thumbnail cache (lazy + thread + dirty + disco)
  5. PERF/BUG RAM causa 1 — sub-scene leak su scene lunghe
- Task in sospeso dalla sessione precedente (rimozione Browser dal right
  panel + zoom-out timeline illimitato) restano da fare: la rimozione
  Browser ora è formalizzata dentro il task "Ztoryc T" e va applicata a
  entrambe le room, non solo a quella corrente.
- Chiusura sessione per ripartire pulito al prossimo turno.

---
## [2026-05-22] — Main Audio toggle: riscrittura completa + Script panel persistente

### Contesto
Il toggle Main Audio (sovrappone la colonna sonora del main a uno shot, serve
per il lavoro di lipsync) aveva una serie di problemi: audio di scene
precedenti, tracce cancellate ancora udibili, scrub diverso dal play, scrub
impreciso. Riscrittura in 4 fasi A/B/C/D. In più, persistenza dello Script
panel (sceneggiatura importata).

### Fixed — audio cache (Fase A)
- `m_columnSoundTracks` era indicizzata per numero di colonna: dopo delete/
  riordino di una colonna audio l'indice puntava alla traccia sbagliata o
  cancellata. Ora chiave = puntatore `TXshSoundColumn*`.
- `preBuildSoundTrackAsync()` scriveva il risultato con `if (!m_soundTrack)`:
  dopo `invalidateSoundTrack()` (cambio scena) `m_soundTrack` è null, quindi
  una build della scena PRECEDENTE veniva scritta nella corrente — l'audio
  fantasma. Aggiunto generation counter (`m_soundGen`): l'async scarta il
  risultato se la generazione è cambiata.
- Aggiunto `validateSoundCache()`: fingerprint delle colonne audio (puntatori
  + lunghezze); `requireSoundTrack`/`startPerColumnAudio`/`preBuildSound
  TrackAsync` lo chiamano e auto-invalidano la cache su qualsiasi modifica.

### Fixed — comportamento toggle (Fasi B + C)
- `onFrameSwitched` riproduceva l'audio SOLO con toggle ON (`&& mainAudio
  Enabled`): con OFF lo shot era muto. Tolto il gate — il viewer nativo
  riproduce sempre l'audio della sua xsheet; `hasSoundtrack()` decide la
  sorgente.
- `ownsSubSceneAudio()` valeva solo a profondità 1. Ora: OFF → false (audio
  sub-scena), ON → true a qualsiasi profondità (audio main).
- `onNativeFrameSwitched()` usciva se profondità ≠ 1. `ChildStack::getAncestor()`
  chaina già tutti i livelli → limite rimosso, funziona a ogni profondità.
- `TXsheet::scrub()` aveva il gate invertito (suonava solo con ON). Ora cede
  solo quando il controller possiede l'audio (ON + sub-scena).
- Logica finale = come il toggle video: ON = solo audio main, OFF = audio
  della sotto-scena corrente.

### Fixed — precisione scrub (Fase D)
- Finestra di scrub estesa da 1 frame (~41 ms, impercettibile) a ~150 ms.
- Scrub gapless: non si interrompe più il segmento in corso (interromperlo
  tagliava la parte centrale — "articolo" → "art--olo"); il segmento
  successivo riparte da dove l'audio era arrivato. Applicato a entrambi i
  percorsi (toggle ON e OFF).

### Fixed — colori anteprime Board
- `IconGenerator::renderXsheetFrame` applicava `rgbSwapped()` → R/B scambiati,
  un rosa diventava azzurro. Rimosso lo swap (le funzioni canoniche di Tahoma
  non lo fanno).

### Added — Script panel persistente
- All'import, la sceneggiatura (.fdx/.txt) viene copiata in
  `<progetto>/extras/script/`; il path relativo è salvato nel `.ztoryc`
  (`<scriptFile>`).
- `StoryboardPanel::saveZtoryc/loadZtoryc` (i veri reader/writer del `.ztoryc`)
  gestiscono il tag; `loadZtoryc` chiama sempre `setScriptFile` → aprendo una
  scena senza sceneggiatura (o una scena nuova) il panel si svuota.
- `ZtoryModel::scriptFileChanged` pilota il reload del panel e il salvataggio
  del `.ztoryc` (un File>Save semplice non chiama saveZtoryc).

### Upstream candidates
- `onActiveViewerChanged` parentWidget-chain null guard, `BaseViewerPanel`
  preview button init — entrambi fixati per il crash Windows, validi upstream.

### Notes
- Build #11 Windows + DMG macOS rigenerati a fine sessione.



### Contesto
La build Windows era rotta da mesi (silenziosamente: lo script non propagava
gli errori msbuild e impacchettava uno zip senza `Ztoryc.exe`). Sono servite
10 iterazioni CI per arrivare a una build Windows funzionante e testata.

### Come riprodurre la build (per Vincenzo / chi fa le release)

**Windows** — GitHub Actions, workflow `Ztoryc Windows Build`
(`.github/workflows/windows_build.yml`), trigger manuale:
```
gh workflow run "Ztoryc Windows Build" --ref master
```
Il workflow (runner `windows-2022`) esegue in sequenza:
1. `ci-scripts/windows/tahoma-install.bat` — deps (Qt 5.15.2_wintab, Boost, OpenCV)
2. `ci-scripts/windows/tahoma-get3rdpartyapps.bat` — ffmpeg, rhubarb
3. `ci-scripts/windows/tahoma-build.bat` — `cmake` + `msbuild RelWithDebInfo ALL_BUILD.vcxproj`
4. `ci-scripts/windows/tahoma-buildpkg.bat` — assembla `Ztoryc\`, `windeployqt`,
   zip portable + installer Inno Setup
Output: artifact `Ztoryc-portable-win.zip` + `Ztoryc-install-win.exe`.
Build #10 funzionante = run CI `26226282757`, commit `8324c58ca`.

**macOS** — build locale (non CI):
1. CMake + `ninja` standard
2. `SKIP_PKG=1 SKIP_DSYM_IN_PACKAGE=1 bash ci-scripts/osx/tahoma-buildpkg.sh`
Output: `Ztoryc-portable-osx.dmg`. Il workflow `macOS_build.yml` fa lo stesso
su CI e può creare direttamente una GitHub Release (`workflow_dispatch` con
input `publish_release: true`).

### Fixed — compilazione Windows (MSVC)
- `TXsheet::setMasterVolume` chiamava `TSoundOutputDevice::setVolume`, che su
  Windows non esiste (dichiarato `#ifndef _WIN32` in tsound.h — il backend
  `tsound_nt.cpp` non ha API di volume per-device). Aggiunto guard `#ifndef _WIN32`.
- 48 occorrenze dell'operatore `not` → `!` (MSVC non accetta i token
  alternativi C++ senza `/permissive-`): storyboardpanel.cpp, subscenecommand.cpp,
  tcenterlinecolors.cpp, borders_extractor.h.
- Variabile locale `near` in ztoryanimatic.cpp rinominata `nearEdge` — `near`
  è una macro retaggio 16-bit in `windef.h`.
- Cherry-pick "Windows port: MSVC compatibility fixes" dal branch `windows-build`
  (DVAPI exports, altri `not`→`!`). Il branch `windows-build` è ora obsoleto.
- `tahoma-build.bat` ora propaga l'exit code di msbuild (`|| exit /b 1`) e
  verifica che `RelWithDebInfo\Ztoryc.exe` esista — prima impacchettava
  silenziosamente uno zip senza l'eseguibile principale.

### Fixed — crash runtime Windows
- Crash `EXCEPTION_ACCESS_VIOLATION` al doppio click su uno shot
  (`onShotDoubleClicked → enterShotMode → ... → getPreviewButtonStates`).
  Causa: il costruttore `BaseViewerPanel` inizializzava `m_symmetryButton` e
  `m_perspectiveButton` a nullptr ma NON `m_previewButton` /
  `m_subcameraPreviewButton`. Quelli vengono creati in `initializeTitleBar()`,
  invocato dal *container* del pannello (tpanels.cpp), non dal costruttore.
  `ZtoryAnimaticViewerPanel::enterShotMode` crea un `ComboViewerPanel` nudo
  che non passa mai da quel container → `m_previewButton` restava memoria
  garbage → su Windows non-null → `->isChecked()` crashava. Su macOS la
  memoria capitava a zero (guard regge) → bug Windows-only. Fix: init a
  nullptr nel costruttore.
- `MainWindow::onActiveViewerChanged`: catena `parentWidget()->parentWidget()`
  senza null-check — guard difensivo aggiunto.

### Fixed — installer / packaging Windows
- Registry root: l'app leggeva da `SOFTWARE\Tahoma2D\Ztoryc\ZTORYCROOT`
  (residuo pre-rebranding in tenv.cpp) ma l'installer scrive su
  `Software\Ztoryc\Ztoryc`. L'app installata trovava ZTORYCROOT vuoto e
  abortiva. Registry root → `SOFTWARE\Ztoryc\Ztoryc`.
- Icona: l'exe Windows aveva ancora `Tahoma2D.ico`. Aggiunto `Ztoryc.ico`
  (6 risoluzioni 16–256, generato da Ztoryc.icns) + `toonz.rc` aggiornato.
- Versione installer: `setup.iss` aveva `MyAppVersion "1.6"` hardcoded
  (versione base Tahoma2D). Ora `tahoma-buildpkg.bat` legge la semver da
  `ZtorycVersion.cmake` e la passa a ISCC via `/DMyAppVersion`.

### Modified
- Cartella stuff del bundle portable rinominata `tahomastuff` → `ztorycstuff`
  (tenv.cpp, CMakeLists.txt, build script mac/win/linux, preferencespopup.cpp).
  `tenv.cpp` prova prima `ztorycstuff`, poi fallback legacy `tahomastuff`.
- Window title: rimosso il suffisso "Tahoma2D <versione>".

### Notes
- I pacchetti delle build sono artifact CI temporanei (scadono 90 giorni,
  richiedono login). Per condividerli con tester serve una GitHub Pre-release
  (tag tipo `v0.3.2-beta1`, link pubblico permanente).
- `master` HEAD `9b804d8e0` ha il fix versione installer non incluso nella
  build #10 — per la pre-release serve una build #11 fresca.

---
## [2026-05-19] — Audio track: undo completo, fix RAM su scene lunghe

### Fixed
- Linked razor (video+audio): l'undo ora annulla sia il taglio video che quello audio
  (prima annullava solo il video). Usa `TUndoScopedBlock` + `UndoAudioEdit` per gruppo.
- Linked razor: fix index shift — dopo `cloneChild(col)` le colonne audio sono spostate
  di +1; ora l'indice viene corretto prima di `splitAudioColumn`.
- Waveform cache RAM: `QPixmap(trackW, trackH)` allocava l'intera larghezza (es. 80.000px
  su scene da 10.000 frame → 16 MB per traccia). Sostituito con cache viewport-aware:
  solo la zona visibile + 600px di overscan, indipendente dalla lunghezza della scena.
- Add Audio Track: ora supporta undo/redo (`UndoAddAudioTrack` con `insertColumn`/`removeColumn`).
- Delete segmento audio: ora funziona correttamente (focus esplicito con `setFocus()` in
  `mousePressEvent` garantisce che il widget riceva l'evento tastiera).
- Selezione audio cross-track: il click su un segmento di un'altra traccia (o sulla
  traccia video) deseleziona le altre tracce. Nuovo segnale `selectionCleared`.
- Undo drag/trim audio: `mouseReleaseEvent` ora fa snapshot prima/dopo con `UndoAudioEdit`
  per `SegmentDrag`, `TrimLeft`, `TrimRight`.
- Focus indicator traccia audio: bordo blu (#50A0FF) 2px intorno alla traccia attiva,
  così è chiaro quale traccia riceverà il Ctrl+V.

### Notes
- La waveform cache usa un "sliding window" da ~1200px. Al primo scroll fuori banda
  la cache si rigenera (solo la banda visibile). CPU e RAM costanti indipendentemente
  dalla lunghezza della scena.

## [2026-05-10] — Early Beta v0.2: overlay buttons shot viewer + symmetry guide fix

### Fixed
- **Crash Perspective Grid Tool (SIGSEV)**: `m_perspectiveButton` era un pointer non inizializzato in `ComboViewerPanel` (che non chiama `initializeTitleBar()`). Fix: inizializzati `m_symmetryButton` e `m_perspectiveButton` a `nullptr` nel costruttore di `BaseViewerPanel` + null-guard in `onToolSwitched()` prima di dereferenziare entrambi. (`viewerpane.cpp`)
- **Camera View e Render Preview non funzionavano in shot mode**: i bottoni nel title bar di `ZtoryAnimaticViewerPanel` (Camera Stand, Camera View, Preview) erano connessi permanentemente allo SceneViewer dell'animatic viewer (pagina 0 dello stack). In shot mode (pagina 1, ComboViewerPanel), i bottoni non avevano effetto sul viewer visibile. Fix: in `enterShotMode()` le connessioni vengono riinstradate al `ComboViewerPanel`; in `returnToAnimaticMode()` vengono riportate all'animatic viewer. Il shot viewer si apre di default in `CAMERA_REFERENCE`. (`ztoryanimatic.cpp`, `viewerpane.h`)
- **Room nere quando si tornava al workflow storyboard con simmetria attiva**: causato dalla lambda `xsheetSwitched` che non ripristinava il routing dei bottoni quando l'xsheet tornava al livello principale via percorso diverso dal back button. Fix: aggiunto `restoreAnimaticButtons()` nella lambda. (`ztoryanimatic.cpp`)
- **Symmetry Guide non funzionava in shot edit mode**: il bottone overlay triggherava `MI_ShowSymmetryGuide` via CommandManager, aggiornando la `TEnv` var ma senza mai chiamare `SymmetryTool::setGuideEnabled()` (che normalmente è in `onSymmetryGuideToggled()`, connessa solo nei viewer che chiamano `initializeTitleBar()`). Fix: aggiunta connessione diretta a `SymmetryTool`/`PerspectiveTool::setGuideEnabled()` nel costruttore di `ZtoryAnimaticViewerPanel`. (`ztoryanimatic.cpp`)

### Added
- **Overlay buttons in shot edit mode**: bottoni Symmetry Guide e Perspective Grid nel title bar di `ZtoryAnimaticViewerPanel`, visibili solo in shot edit mode (enterShotMode/restoreAnimaticButtons). Bottoni Safe Area e Field Guide sempre visibili sull'animatic viewer. (`ztoryanimatic.cpp`)
- `BaseViewerPanel::sceneViewer()`, `referenceModeButtonSet()`, `previewButton()` — accessor pubblici per permettere a `ZtoryAnimaticViewerPanel` di reindirizzare le connessioni dei bottoni senza accedere a membri protetti. (`viewerpane.h`)
- `ZtoryAnimaticViewerPanel::restoreAnimaticButtons()` — helper che nasconde gli overlay buttons e ripristina il routing dei bottoni verso l'animatic viewer; chiamato in `returnToAnimaticMode()` e nella lambda `xsheetSwitched`. (`ztoryanimatic.cpp`)

### Notes
- Task 21 (Volume per traccia audio) completato nella sessione precedente dello stesso giorno
- Early Beta (v0.2) milestone raggiunta: Undo/Redo ✅, audio toggle ✅, crash fix ✅, shot viewer camera view ✅, overlay buttons shot viewer ✅, symmetry guide fix ✅
- Fix version: `ZtorycVersion.cmake` era già a `0.3` (next milestone) — ripristinato a `0.2.0` per la release corrente

---
## [2026-05-09] — Audio sub-scene fix completo + cleanup SLIP/onion residui

### Fixed
- **Audio in sub-scene (workflow 2D Tradigital + Storyboard)**: risolto il "frammento + double-play". Catena di fix:
  - **Guard `m_frameHandle->isPlaying()` sostituito con `isContinuousPlaying()`** in `onNativePlayingStatusChanged()` e `onNativeFrameSwitched()`. Il guard precedente bloccava l'audio quando il frame handle dell'animatic restava "stuck" in playing dopo un cambio room senza stop esplicito → audio a volte non partiva o non ripartiva al loop
  - **Room nera durante play**: causata da flooding del `QAudioOutput::notify` ogni 50ms quando `onNativeFrameSwitched()` veniva chiamato per ogni frame durante il play (frame handle globale aggiornato anche durante play animatic). Fix con il guard preciso sopra
  - **Doppio play (frame 1 + frame 31)**: aggiunto `ZtoryAnimaticController::ownsSubSceneAudio()` chiamato da `BaseViewerPanel::hasSoundtrack()` per sopprimere il path nativo per-frame quando il controller streama già il main audio
  - **Frame senza mapping main → muti**: check `ancestor.first != mainXsh` in entrambe le funzioni — sub-frame fuori dal range mappato ora correttamente silenziosi
  - **Audio non parte se play comincia su frame muto**: `onNativeFrameSwitched()` ora ritenta `onNativePlayingStatusChanged()` quando entra nel mapped range
  - **Audio muto al loop**: aggiunto `m_lastNativePlayFrame` per rilevare il salto all'indietro del FlipConsole; al loop resetta `m_nativeAudioPlaying=false` e fa ripartire l'audio
- **`m_scrubDevice` dedicato** (`TSoundOutputDevice` separato dal `mainXsh->m_player`) per scrub audio sia in `onNativeFrameSwitched()` che in `playAnimaticAudioFrame()` — evita interferenze fra scrub e continuous play. Destructor in `~ZtoryAnimaticController()` per evitare leak a chiusura
- **`mainXsh->stopScrub()` esplicito prima di `play()`** in `onNativePlayingStatusChanged()` per scartare residui del ring buffer hardware

### Removed
- **SlipTool eliminato completamente** (decisione di scope dopo analisi onesta — implementazione corretta richiedeva 2-3 settimane su mobile mark-in nei sub-scene cell):
  - Toolbar `slipBtn` rimosso da `ZtoryAnimaticPanel`
  - Enum `Tool::SlipTool` e `DragMode::Slip` rimossi
  - Field `ShotData::slipOffset`, metodi `getSlipOffset`/`adjustSlipOffset` rimossi da `ZtoryModel`
  - Attributo `slipOffset` rimosso da save/load XML (read no-op per backward compat)
  - Signal `slipEdit`/slot `onSlipEdit` rimossi
  - Slip indicator (striscia arancione + label `+N`) rimosso dal paint dei block
  - Cell write in `resequenceXsheet()` semplificato a `TFrameId(r + 1)` (era `TFrameId(slipOff + r + 1)`)
  - In `ztoryOpenSubXsheet()` il play range ora copre l'intera durata animatic (non più clamp via slipOff)
- **Onion skin residui in `ZtoryAnimaticRuler`** (era stato deciso di rimuoverlo ma rimanevano frammenti che causavano malfunzionamenti):
  - Strip FOS (top) + MOS (bottom) rimosse → ruler ora alto 18px (era 36px)
  - `m_localMask`, `m_onionEnabled`, `setOnionEnabled()`, `syncOnionToGlobal()` rimossi
  - HoverZone (HoverFOS/HoverMOS), `m_hoverFrame`, `m_hoverZone` rimossi
  - Signal `onionEnabledChanged` rimosso
  - Connect `onionSkinMaskChanged` (sia su `m_sceneViewer` che su `m_ruler`) rimossi
  - Include `onionskinmask.h` e `tonionskinmaskhandle.h` rimossi dal `ztoryanimatic.cpp`/`.h`

### Modified
- `BaseViewerPanel::hasSoundtrack()` (viewerpane.cpp): aggiunto early return su `ZtoryAnimaticController::ownsSubSceneAudio()` analogo a `ownsAudioAtMainLevel()` — sopprime la path audio nativa quando il controller gestisce lo streaming continuo dal main

### Notes
- Lista feature/fix completa rivista in sessione → piano d'azione in 7 fasi:
  1. Audio finiture (icon toggle, waveform normalization, volume per traccia, audio keyboard shortcuts)
  2. Timeline QoL (peg column residue, PASTE behavior, default palette)
  3. Sync shot duration toggle + mesh extras path
  4. Camera sensitivity + waveform residui
  5. Layout per workflow (config-only, replicare struttura `Storyboard/`)
  6. Transizioni (formula `extra_per_lato = N/2`)
  7. Heavy a parte: CEL/KEYS modes (timeline only), PSD robustness + ORA importer, Autofill vector, Multi-select import
- **DROPPED**: SLIP, SLIDE (workaround manuale accettabile), navigation tag fissi per export range
- Toggle audio invertito (icona ON↔OFF) noto, da fixare in Fase 1 — è solo asset/QAction state

---
## [2026-05-05] — Trim/Roll + Slip + fix workflow Load Scene

### Added
- **TrimTool (T)** nella toolbar animatic: drag sul seam tra due shot → Roll edit (A±Δ, B∓Δ, durata totale invariata); drag sul bordo destro dell'ultimo shot → Ripple Trim classico. Cursore `SplitHCursor` sul seam, `SizeHorCursor` sul bordo isolato
- **SlipTool (Y)** nella toolbar animatic: drag dentro un blocco → sposta la finestra sub-scena senza cambiare durata o posizione nel timeline. Indicatore visivo: striscia arancione bordo sinistro + testo `+N` in basso a destra
- **`ShotData.slipOffset`** (int, 0-based): campo persistente in ZtoryModel, salvato/caricato nel `.ztoryc` (`slipOffset` attribute)
- **`ZtoryModel::resequenceXsheet()`** aggiornato: scrive `TFrameId(slipOff + r + 1)` per ogni colonna, preservando lo slip offset ad ogni resequence
- **`ZtoryModel::shotIndexForCol / getSlipOffset / adjustSlipOffset`**: metodi helper per gestione slip
- **Marker dentro lo shot con Slip**: `onShotDoubleClicked` ora imposta play range `[slipOff, slipOff+duration-1]` nella sub-scena; `onSlipEdit` aggiorna `ztorySetShotRange` con il range slippato
- **Icone SVG**: `ztoryc_trim.svg`, `ztoryc_slip.svg` + registrazione in `toonz.qrc`
- **`onRollEdit` / `onSlipEdit`** slot in `ZtoryAnimaticPanel`: implementazione completa con undo snapshot

### Fixed
- **Workflow non applicato su scene recenti** nel popup Load Scene: `onRecentSceneClicked` ora esegue il comando MI_Workflow* selezionato in `m_loadWorkflowCB` (condizione `m_mode != LoadSubSceneMode`)

### Notes
- Slide (spostamento shot con compensazione vicini) rimandato — complessità alta
- Slip funziona tecnicamente (frameIds cambiano, viewer mostra frame diversi) ma feedback visivo durante il drag è limitato: il blocco non cambia apparenza a schermo; il contenuto cambia solo alla riproduzione successiva. Miglioramento previsto

---
## [2026-05-05] — Fix crash + Add to Favorites funzionante

### Fixed
- **Crash "Refresh Folder"**: causato da vtable corruption dopo aggiunta di `virtual TFilePath getPath()` nella base class `DvDirModelNode`. Rimossa la virtual (non necessaria), rimosso `override` da `DvDirModelFileFolderNode::getPath()`, ripristinato cast-based approach in `dvdirtreeview.cpp`
- **Errori compile `TFilePath + QString`**: `TFilePath::operator+` non accetta `QString` — corretti tutti e 4 i siti (dvdirtreeview.cpp ×2, filebrowser.cpp ×2) usando `favFolder + srcFp.withoutParentDir()` (TFilePath + TFilePath)

### Added
- **Add to Favorites (pannello destra)**: right-click su spazio vuoto nella cartella corrente → "Add to Favorites"; right-click su una cartella → "Add to Favorites". Implementato in `filebrowser.cpp::getContextMenu()`
- **Remove from Favorites (pannello destra)**: right-click dentro Favorites → "Remove from Favorites"
- **Add/Remove Favorites (albero sinistro)**: right-click su nodo folder nell'albero → "Add to Favorites" / "Remove from Favorites". Funziona per tutti i nodi `DvDirModelFileFolderNode` (Desktop, Downloads, cartelle progetto, ecc.) — non per nodi virtuali root (My Computer, History) che non hanno path reale
- **Drag & drop su Favorites**: trascinare una cartella sul nodo Favorites nell'albero crea un symlink

### Notes
- Symlinks creati con `QFile::link()` nella cartella `ToonzFolder::getMyFavoritesFolder()`
- `ImportAssetsPopup`: browser multi-selezione, tutti i tipi file eccetto .tnz, chiama `IoCmd::loadResources()`

---
## [2026-05-04] — Task 24: Startup popup come hub scene management

### Added
- **StartupPopup `Mode` enum** (4 modalità): `DefaultMode` (avvio a freddo, entrambi i tab, blocco chiusura), `CreateMode` (File > New Scene, solo tab Create, niente recent panel), `LoadMode` (File > Load Scene, solo tab Load), `LoadSubSceneMode` (File > Load As Sub-Scene, solo tab Load, multi-selezione con Shift/Cmd + pulsante "Load Selected", niente recent panel)
- **`File > Import > Import Assets...`** — browser nativo macOS (vecchio comportamento Load Scene), aggiunto in `menubar.cpp` + `menubarcommandids.h` + `mainwindow.cpp/.h`
- **Multi-selezione in LoadSubSceneMode**: `ExtendedSelection` + `setMultiSelect(true)` su `StartupScenesList` che disabilita hover-clear in `leaveEvent` e hover-setCurrentItem in `mouseMoveEvent`

### Modified
- **`onNewScene()`**: non chiama più `IoCmd::newScene()` prima del popup — la scena corrente non viene chiusa se l'utente fa Cancel. `IoCmd::newScene()` spostato dentro `onCreateButton()` per `CreateMode` only
- **Startup a freddo**: popup mostrato da `main.cpp` (DefaultMode); File > New Scene usa sempre `CreateMode`
- **Titoli popup dinamici**: "Ztoryc Startup" / "Create New Scene" / "Load Scene" / "Load Scene as Sub-Scene"
- **Bottone Cancel**: `setMinimumSize(65,25)` + `setMaximumHeight(25)` (uguale ai pulsanti nativi DVGui); label "Quit Ztoryc" solo a freddo con scena untitled, "Cancel" altrimenti
- **`onProjectComboChanged` in LoadSubSceneMode**: carica le scene del progetto selezionato senza cambiare progetto attivo né chiudere la scena corrente (`TProject::load()` + `refreshExistingScenes(scenesFolder)`)
- **`refreshExistingScenes`**: accetta `TFilePath scenesFolder = TFilePath()` opzionale
- **`IoCmd::loadSubScene(path)` in `iocommand.cpp`**: invariato (usa ASK_USER default per tutte le scene)
- **`ResourceImportDialog::askImportQuestion`**: messaggio contestuale — "doesn't belong to the current project" solo per file effettivamente esterni; messaggio neutro "Do you want to import it or load it from its current location?" per file nella scenes folder del progetto corrente (check `isExternPath` + ancestor della `+scenes` folder)

### Notes
- Bug layout Cancel button: `addWidget(btn, Qt::AlignLeft)` passava AlignLeft (=1) come stretch factor → pulsante si espandeva. Fix: `addWidget(btn, 0, Qt::AlignLeft)`

---
## [2026-05-03] — Peg columns narrow (22px) + Set Key fix

### Fixed
- **Set Key (Z) su peg columns**: `TCellSelection::setKeyframes()` usava `ColumnId(col)` invece di `xsh->getColumnObjectId(col)` → il keyframe veniva impostato sullo stage object sbagliato, nessun diamante visibile. Fix: una riga in `cellselectioncommand.cpp`
- **`setKeyframeWithoutUndo(int frame)`**: aggiunto `invalidate()` alla fine così `isKeyframe()` riflette subito le modifiche (lazy cache `m_lazyData` non veniva refreshata)
- **Peg columns narrow in vertical timeline**: colonne peg ora larghe 22px come camera. Modifiche:
  - `ColumnFan`: per-column width support (`m_width`, `setColumnWidth()`, `getCameraColumnDim()`, `getColWidth()`) con `update()` che usa larghezze per-colonna
  - `TXsheet`: peg columns marchiate 22px su `insertColumn()` e al termine di `loadData()`
  - `xshcolumnviewer`: peg in vertical timeline usa `CAMERA_LAYER_HEADER`/`CAMERA_LAYER_NAME`, nome ruotato 90°, nessuna icona
  - `xshcellviewer`: celle/keyframe/selection/focus border usano `CAMERA_CELL`/`CAMERA_KEY_ICON`/`CAMERA_LOOP_ICON` per peg in vertical timeline

### Added
- **Task 24** in ANIMATIC_TASKS: Startup popup come hub scene management (New/Load/Subscene + Cancel contestuale + Import Assets in File > Import)

### Upstream candidates
- Set Key (Z) non mostra diamante su peg columns — `cellselectioncommand.cpp` una riga

---
## [2026-05-02d] — Fix writeRoomList/renameRoom crash + Storyboard layout template

### Fixed
- `writeRoomList` salta rooms con path vuota: le "fallback rooms" create durante
  un workflow switch prima che `makePrivate()` assegni i path scrivevano "." in
  layouts.txt corrompendolo → crash "room not found" al prossimo switch
- `renameRoom` guarded: salva il file INI solo se la room ha un path valido
  (preveniva crash al rename su room senza path)

### Modified
- Template Storyboard (`stuff/profiles/layouts/rooms/Storyboard/ztoryc.ini` e
  `browser.ini`) aggiornati con il layout produzione di francobianco: Board a
  sinistra, Viewer al centro, RightPanel a destra, Animatic in basso
  Gerarchia corretta: `-1 1 [ [ 0 1 2 ] 3 ]` (era `-1 1 [ [ [ 0 1 2 ] 3 ] ]`)

### Notes
- Il meccanismo di fallback `getRoomsFile()` (user dir → template dir) è già
  in produzione. Nuovi utenti e CI builds ora ricevono il layout corretto.
- Task 23 completato per Storyboard; StopMotion già aveva template DragonFrame.

---
## [2026-05-02c] — Fix workflow combo "Open Existing Scene" + rimossa regressione StopMotion

### Fixed
- **Bug workflow popup "Open Existing Scene"** (`startuppopup.cpp`): `CommandManager::execute(cmd)`
  spostato DOPO `IoCmd::loadScene` invece che prima — il `switchRoomChoice` (clearRooms +
  readSettings) avveniva mentre la scena era ancora in caricamento, causando schermata nera
  per StopMotion e potenziali interferenze per tutti i workflow.
- **Regressione StopMotion rimossa**: in un passaggio intermedio era stato erroneamente
  eliminato "Stop-Motion Mode" dai combo del popup — ripristinato correttamente.

### Notes
- Feedback utente ricevuto: chiedere conferma PRIMA di rimuovere funzionalità esistenti.

---
## [2026-05-02b] — Task 19 completato: cursore resize su video e audio track

### Fixed
- **SIGABRT dyld crash** (`build_and_deploy.sh`): le dylib nella root di `build/`
  sono stale; tutti i path aggiornati a usare le sottodirectory (`build/tnzcore/`,
  `build/toonzlib/`, ecc.) dove ninja deposita le build aggiornate.
- **Compile error `mx` undefined** (`ztoryanimatic.cpp`): `int mx = e->x() - kLabelW`
  spostato all'inizio di `ZtoryAnimaticTrack::mouseMoveEvent`; rimossa la ridefinizione
  duplicata nel blocco RazorTool.

### Added
- **Task 19 — Cursore resize audio track** (`ztoryanimatic.cpp`): `ZtoryAudioTrack`
  ora mostra `SizeHorCursor` quando il mouse si avvicina ai bordi di un segmento audio.
  Implementato via `setAttribute(Qt::WA_Hover)` + override di `event()` con
  `QEvent::HoverMove`/`HoverLeave`. Helper statico `nearSegmentEdge()` aggiunto.
- **Task 19 — Cursore resize video track** (`ztoryanimatic.cpp`): `ZtoryAnimaticTrack`
  mostra `SizeHorCursor` quando il mouse si avvicina al bordo destro di un blocco shot
  in `mouseMoveEvent` (zona ±6px). Cursore resettato in `leaveEvent`.

### Modified
- **`SystemVar.ini`** (`toonz/install/`): risolto conflict rebase; accettata versione
  remote con chiavi `ZTORYC*` e path `/Applications/Ztoryc/Ztoryc_stuff`.
- **`postinstall-script.sh`**: fallback per entrambi i nomi file (`ztorycstuffdirloc` /
  `tahoma2dstuffdirloc`) dall'AppleScript dell'installer.

---
## [2026-05-02] — Task 16/17/18 completati; Task 19 cursor ancora irrisolto
### Added
- **Task 16:** Workflow combo nel tab "Open Existing Scene" della StartupPopup —
  scelta workflow viene applicata prima di caricare la scena; entrambi i combo
  (Create + Load) si sincronizzano al workflow corrente all'apertura del dialog.
- **Task 16:** Voci workflow nel menu Windows ora sono checkable e mostrano
  spunta sul workflow attivo; `updateWorkflowMenuChecks()` chiamata ogni volta
  che `switchRoomChoice()` cambia il layout.
- **Task 17:** All'apertura di una sotto-scena (doppio-click shot nell'animatic),
  il play-range viene impostato automaticamente su `[0, subFrameCount-1]` via
  `XsheetGUI::setPlayRange()`.
- **Task 18:** Zoom con rotella del mouse spostato dal `ZtoryAnimaticTrack` al
  `ZtoryAnimaticRuler`; il track ignora ora la wheel (`e->ignore()`). Aggiunto
  signal `zoomChanged(double)` al ruler, connesso a `onZoomChanged` nel panel.
### Notes
- **Task 19 (cursor resize audio):** `setMouseTracking(true)` aggiunto al
  costruttore di `ZtoryAudioTrack`; logica hover aggiornata per cappare `xRight`
  alla larghezza del widget. Risultato non ancora visibile — da investigare.
- Reverted tutte le modifiche problematiche della sessione precedente (audio
  cut/paste/undo + `#include "ztoryanimatic.h"` in storyboardpanel.cpp che
  causava board panel regression) con `git restore .` prima di re-implementare.

---
## [2026-05-01d] — Fix audio toggle 12d + onion skin rimosso + mark-out default + nuovi task
### Fixed
- **Bug 12d — Audio toggle in sub-scena** (`sceneviewer.cpp`, `ztoryanimatic.h/.cpp`):
  `execute()` di `MI_ToggleMainAudio` ora chiama `stopNativeAudio()` sul controller
  per fermare lo streaming avviato da `onNativePlayingStatusChanged`. Aggiunto
  `restartNativeAudioIfPlaying()` per ri-abilitare l'audio durante il play quando
  si toglie il mute.
- **Mark-out a fine timeline all'avvio** (`ztoryanimatic.cpp`): aggiunto
  `resetPlayRangeToFull()` su `sceneSwitched` in `ZtoryAnimaticPanel` — il mark-out
  si posiziona automaticamente all'ultimo frame della scena ad ogni caricamento.
### Removed
- **Onion skin dalla toolbar animatic** (`ztoryanimatic.cpp`): rimosso `onionBtn`
  e i relativi connect dalla toolbar di `ZtoryAnimaticPanel`.
### Added (ANIMATIC_TASKS)
- Task 16–22: Workflow startup, stop marker immediato, zoom ruler, cursore resize,
  taglia/copia/incolla audio, volume per traccia, transizioni.

---
## [2026-05-01c] — Undo/Redo CRUD completo + fix refresh anteprime Board
### Added
- **Task 13 — Undo/Redo completo** (`ztoryundo.h`, `storyboardpanel.cpp`, `ztoryanimatic.cpp`):
  - Nuovo file `ztoryundo.h`: `ZtoryShotSnap {ShotData, TXshLevelP, int duration}` +
    classe `UndoBoardState` (before/after snapshot, chiama `restoreFromSnapshot` su undo/redo)
  - `StoryboardPanel::captureSnapshot()` / `restoreFromSnapshot()`: full rebuild xsheet+Board
    da un vettore di snapshot; `TXshLevelP` mantiene in vita livelli eliminati per undo-of-delete
  - Board: undo su Add, Delete, Move (drag&drop), Paste, Merge, Match Duration
  - Board: undo duration con timer di coalescenza 600ms (`m_durationCommitTimer`) — un solo
    item per "sessione di editing" invece di uno per ogni tick della spinbox
  - Animatic: undo su Delete, Cut, Paste, Duration resize, Merge, MergeWithNext, Razor
  - Fix anti-polluzione stack: `ColumnCmd::deleteColumns(..., withoutUndo=true)` per tutti i
    delete interni; `TUndoManager::manager()->popUndo(1)` dopo `ColumnCmd::cloneChild()` in Razor
  - `findBoardPanel()`: helper statico in ztoryanimatic.cpp tramite `QApplication::allWidgets()`
### Fixed
- **Bug `updatePreview` colonna errata** (`storyboardpanel.cpp:updatePreview`):
  usava `shotIdx` come indice colonna xsheet invece di `shot.data.xsheetColumn`.
  Poteva rendere la thumbnail dello shot sbagliato quando ordine shot ≠ ordine colonne.
- **Anteprime Board stantie dopo disegno** (`storyboardpanel.cpp`):
  - `showEvent`: aggiunto `onRefreshPreviews()` anche quando Board torna visibile con shots
    già caricate (prima refresh solo a primo caricamento). Fix caso: disegno in sub-scena →
    cambio room → Board mostra thumbnail aggiornate.
  - `xsheetSwitched` handler: quando si entra in una sub-scena, `xsheetChanged` viene ora
    connesso anche a `m_panelDetectTimer->start()` — ogni modifica (disegno/cancella) riavvia
    il timer da 1s; al timeout si ri-renderizza la thumbnail dello shot corrente.

---
## [2026-05-01b] — Fix testi Board persi al reload, cleanup Export Animatic
### Fixed
- **BUG critico: testi dialog/action/notes persi al salvataggio** (`storyboardpanel.cpp`):
  aggiunta `syncWidgetsToData()` chiamata all'inizio di `saveZtoryc()`. Il handler
  `dataChanged` aggiornava solo `shotLabel`, mai `data.panels[pi].dialog/action/notes`,
  quindi il salvataggio XML scriveva dati stale. Ora prima del write tutti i widget
  vengono copiati nel data model.
### Modified
- **Export Animatic dialog**: rimosso pulsante "Output Settings…" (bloccava l'UI con
  `ApplicationModal`). Sostituito con label read-only che mostra formato/fps/risoluzione
  correnti + nota "change via Render > Output Settings".

---
## [2026-05-01] — Branding Ztoryc, fix crash panel, fix reorder shot
### Added
- `DockWidget::setEmbedded()` in `docklayout.h`: setta `m_floating=false`,
  `m_parentLayout=nullptr`, rimuove margini floating — impedisce drag-to-float
  sui panel embedded in `ZtoryLeftPanel` / `ZtoryRightPanel`
- Doppio click su `ComboViewerPanel` in shot mode → esce dalla shot mode
  (eventFilter su `ZtoryAnimaticViewerPanel`)
- Cartella `Ztoryc` su Google Drive con copia CHANGELOG e ANIMATIC_TASKS
### Fixed
- **Crash QTextEdit stack overflow**: `ZtoryScriptView::m_textEdit` ora ha
  `setMinimumSize(80,60)` + `setLineWrapMode(NoWrap)`, evita layout ricorsivo
  a larghezza zero quando il panel è nascosto in QStackedWidget
- **Board "sganciato"**: panel embedded in ZtoryLeftPanel/ZtoryRightPanel ora
  usano `setEmbedded()` + `getTitleBar()->hide()` → non più draggabili come
  panel floating; null-guard in `mouseDoubleClickEvent` e `maximizeDock`
- **Room duplicate al Reset**: `layouts.txt` (template + utente) aggiornato
  a soli `ztoryc.ini` + `browser.ini`; `currentRoom.txt` → `ZTORYC`; rimossi
  vecchi file `animatic.ini`, `board.ini`, `room1-6.ini` da entrambe le dir
- **Reorder shot apriva lo shot sbagliato**: in `onMoveShot()`, dopo lo
  spostamento fisico delle celle xsheet, aggiornamento di
  `m_shots[i].data.xsheetColumn = i` per tutti gli slot
### Modified (altra istanza Claude)
- **About dialog** (`aboutpopup.cpp`, `toonz.qrc`): titolo "About Ztoryc",
  logo `ztoryc_about.png` (400×400, scalato 80×80), link GitHub
  `github.com/matitanimata/ztoryc`, licenza GPL v3, note FFmpeg (LGPLv2.1)
  + Rhubarb Lip Sync (MIT), ringraziamenti team Tahoma2D
- **Splash screen** (`Resources/tahoma2d_splash.svg`): versione corretta
  da `v1.0.0` a `v0.2.0`

---
## [2026-04-25b] — revert side-fix, mantenuto solo Homebrew SuperLU

### Modified
- Revert di `tlin_superlu_wrap.cpp` e `plasticdeformer.cpp` allo stato pre-sessione
  (commit `d3ac737e3`). Le modifiche aggiuntive (safety net sigsetjmp,
  inversa analitica 4×4, validazioni colptr) erano superflue una volta
  passato a Homebrew SuperLU 7 e creavano potenziale per bug subdoli.
- `BundleInfo.plist.in`: rimosso `LSRequiresCarbon=true` — bloccava
  l'AutoFill UI. Era stato aggiunto come tentativo, ma il vero fix del
  drag crash era già il cambio a Homebrew SuperLU.
- `storyboardpanel.cpp::updatePreview()`: ripristinato
  `IconGenerator::renderXsheetFrame()` (i preview thumbnail si aggiornano
  di nuovo al cambio xsheet — ora safe con Homebrew SuperLU).

### Notes
- Unica modifica essenziale del fix di oggi rimasta: `CMakeLists.txt`
  `WITH_SYSTEM_SUPERLU=ON` di default su macOS (commit `fc625e448`).
- Crash open: `TProjectManager::notifyListeners()` SIGBUS durante click
  in DvDirTreeView (dangling listener pointer). Da indagare in nuova
  sessione — chiunque chiama `addListener` su `TProjectManager::instance()`
  e non rimuove nel distruttore.
- Commit revert: `48b42a8d3 revert: keep only essential SuperLU fix (Homebrew)`

---
## [2026-04-25] — fix crash plastic deformer + drag (SuperLU bundled vs Homebrew)

### Fixed
- **PlasticDeformer SIGSEGV su arm64**: il bundled SuperLU 4.1 ha UB latente
  che su Apple Silicon nativo corrompe memoria — `dgstrf` crasha direttamente
  in `compileStep1`/`initializeStep2`, e (sorpresa) la stessa corruzione
  emerge come `BUG IN CLIENT OF LIBPLATFORM: recursive os_unfair_lock` nel
  drag-and-drop di scene nel cast (NSCoreDragManager).
- **Root cause**: dopo il rebranding Ztoryc, `WITH_SYSTEM_SUPERLU` di default
  su macOS è rimasto `OFF` → linker ha incluso libsuperlu_4.1.a bundled
  invece di `libsuperlu.7.dylib` di Homebrew (che la vecchia Tahoma2D.app
  funzionante usava dinamicamente).

### Modified
- `toonz/sources/CMakeLists.txt`: default `WITH_SYSTEM_SUPERLU=ON` su macOS.
  Richiede `brew install superlu` una sola volta.
- `toonz/cmake/BundleInfo.plist.in`: re-aggiunto `LSRequiresCarbon=true`
  (era nel vecchio Tahoma2D, dropped nel rename Ztoryc).
- `toonz/sources/tnzext/tlin/tlin_superlu_wrap.cpp`: guard difensivi
  permanenti in `factorize()` — validazione NaN/Inf valori, bounds-check
  rowind, monotonia colptr, safety net `sigsetjmp`/`siglongjmp` intorno
  a `dgstrf`, fix swap argomenti `relax`/`panel_size`.
- `toonz/sources/tnzext/plasticdeformer.cpp`: rimpiazzato SuperLU per la
  factorizzazione 4×4 per-faccia in `initializeStep2`/`deformStep2` con
  inversa analitica closed-form (Schur complement) — più veloce e azzera
  esposizione UB SuperLU per il sistema per-triangolo.
- `toonz/sources/toonz/storyboardpanel.cpp`: rimosso workaround mesh-column
  in `updatePreview()` (non più necessario), thumbnail sempre via `getIcon()`.

### Notes
- L'app è arm64 nativo (non Rosetta come sospettato inizialmente). Il vecchio
  Tahoma2D.app funzionante era anch'esso arm64 ma linkava dinamicamente la
  SuperLU 7 di Homebrew → da qui il diverso comportamento.
- Setup post-pull per dev macOS: `brew install superlu` (one-shot).
- Commit: `fc625e448 fix: PlasticDeformer + drag crashes — switch to system SuperLU on macOS`

---
## [2026-04-24] — resetOnSeqChange: riavvio contatore SH per sequenza

### Added
- **`NumberingConfig::resetOnSeqChange`** — nuovo campo bool (default `false`).
  Quando `true` (solo Sequence style): il contatore SH si azzera a `startNumber`
  ad ogni cambio di sequenza (SQ01→SH010, SQ02→SH010…). Quando `false`:
  numerazione globale continua tra tutte le sequenze.
- **`m_resetOnSeqChangeCB`** in `StartupPopup` — checkbox "Restart shot # at each
  new sequence", visibile solo in Sequence style; stato salvato in `NumberingConfig`.
- **`resetOnSeqCB`** in `StoryboardPanel::onNumberingConfig()` — stessa checkbox
  nel dialogo di configurazione numerazione del Board.

### Modified
- **`StoryboardPanel::renumberAll()`** — in Auto mode, le sequenze sopravvivono
  al renumber (solo SH cambia). I nuovi shot senza `sequenceId` ereditano la
  sequenza dello shot precedente. Con `resetOnSeqChange`, `shotIdx` è relativo
  alla sequenza (non globale).
- **`StartupPopup::onCreateButton()`** — in Sequence mode, crea una sequenza
  default "sq01" e vi assegna tutti gli shot iniziali (campo SQ pre-popolato).

### Fixed
- **Crash SIGABRT su import scena con Plastic Deformer** — `ZtoryAnimaticTrack::
  refreshFromScene()` e `ZtoryStoryStripPanel::refreshFromScene()` chiamavano
  `IconGenerator::getIcon()` sincronicamente durante `xsheetChanged`. Durante
  l'import di una scena, la xsheet non è ancora stabilizzata: il rendering
  triggerava `PlasticDeformerStorage::process()` → `PlasticDeformer::initialize()`
  → `tlin::factorize()` → `StatFree()` su SuperLU Matrix non inizializzata → crash.
  Fix: entrambi gli handler `xsheetChanged` wrappati con `QTimer::singleShot(0)`
  per differire l'esecuzione all'iterazione successiva dell'event loop.
  Rimossa anche la chiamata ridondante `updateAllPreviews()` da
  `ZtoryModel::onXsheetChanged()` (violava regola AGENTS.md).

---
## [2026-04-23] — Numerazione SQ/SH, rename app Ztoryc, fix firma bundle

### Added
- **Sequenze editabili nel Board** — campo SQ separato e editabile per ogni shot.
  Digitando un numero di sequenza (es. "020") viene assegnata la sequenza a quello
  shot e a tutti i seguenti fino al prossimo cambio manuale (`seqLabelEdited` cascade).
- **`ZtoryModel::findOrCreateSequence()`** — trova o crea `SequenceData` by label,
  usata sia dal cascade handler che dal renumber automatico.
- **`ZtoryModel::assignShotLabel()` (static)** — algoritmo midpoint condiviso tra
  `ZtoryModel` e `StoryboardPanel` per generare label senza duplicati al momento
  dell'inserimento (Keep mode → SH015 tra SH010 e SH020; Auto mode → rinumera tutto).

### Fixed
- **Doppio click entra e ritorna subito** — `PanelWidget::mouseDoubleClickEvent`
  chiamava `QFrame::mouseDoubleClickEvent(e)` che propagava l'evento a
  `StoryboardPanel::mouseDoubleClickEvent` il quale eseguiva `MI_CloseChild`.
  Fix: sostituito con `e->accept()`.
- **Shot duplicato al momento dell'inserimento** — in modalità Auto, `renumberAll()`
  usava `ZtoryModel::m_shots` come sorgente invece della lista locale del Board,
  ottenendo l'indice errato. Fix: algoritmo statico opera sulla lista locale del Board.
- **Campo SH mostrava "SH - sq01_sh010"** — `setShotNumber()` ora separa SQ e SH
  sul separatore `_`, mostra solo la parte numerica in ciascun campo e salva il
  prefisso in `m_storedShotPrefix`/`m_storedSeqPrefix` per la ricostruzione.
- **`renumberAll()` Auto + Sequence style** — `cfg.shotName(i)` restituisce
  "SQ001_SH010"; ora viene splittato correttamente: SH → `shotLabel`, SQ → `sequenceId`.

### Modified
- **Rename app: Tahoma2D → Ztoryc** — bundle ID `io.github.ztoryc.Ztoryc`,
  `CFBundleName/ExecutableName = Ztoryc`, versione 1.0.0.
  File cambiati: `CMakeLists.txt` (target), `BundleInfo.plist.in`, `main.cpp`,
  `Ztoryc.entitlements`, `build_and_deploy.sh`.
- **`build_and_deploy.sh`** — firma corretta senza `--deep` (dylib firmate
  singolarmente prima del bundle); `xattr -cr` prima della firma; `rm -rf profiles/`
  per evitare "unsealed contents in bundle root"; copia automatica `SystemVar.ini`
  se mancante; copia dylib secondarie dal build tree.

### Notes
- `Ztoryc.app/profiles/` viene ricreata dall'app ad ogni avvio — è normale,
  non invalida la firma al lancio (il seal è valido al momento di `open`).
- `SystemVar.ini` punta a `/Volumes/ZioSam/.../stuff` — path assoluto,
  non portabile; da parametrizzare per distribuzione.
- Per permessi TCC stabili: aggiungere Ztoryc.app al Full Disk Access in
  System Settings → Privacy & Security.

---
## [2026-04-23b] — Branding Ztoryc completato

### Modified
- **`tversion.h`** — `applicationName = "Ztoryc"`, versione 1.0 (era Tahoma2D 1.6).
  Propaga su titolo finestra, startup popup, about dialog, tutti i log.
- **`tahoma2d_splash.svg`** — icona Ztoryc (PNG embedded base64) + wordmark +
  tagline "STORYBOARD · ANIMATIC · ANIMATION" su sfondo scuro.
- **`tahoma2d_startup.svg`** — banner orizzontale: icona + "ZTORYC" in giallo `#F5B800`.
- **`tipspopup.cpp`** — titolo "Ztoryc Tips".
- **`mainwindow.cpp`** — update checker punta a github.com/matitanimata/ztoryc.
- **`main.cpp`** — tips popup disabilitato; update check automatico disabilitato
  (contenuti ancora riferiti a Tahoma2D).
- **`Ztoryc.icns`** — generato da `ztoryc_icon.png` con tutte le risoluzioni macOS
  (16×16 → 1024×1024).

### Notes
- `toonz.qrc` va touchato prima di ogni modifica SVG per forzare la ricompilazione
  delle risorse Qt: `touch toonz/sources/toonz/toonz.qrc && ./build_and_deploy.sh`

---
## [2026-04-20] — Fix: 7 crash + audio export + workflow switch lento

### Fixed
- **Crash FlipConsole::doButtonPressed (QThread::isRunning SIGSEGV)** — durante
  `clearRooms()` i widget venivano nascosti e `hideEvent` → `setActive(false)` →
  `pressButton(ePause)` → `doButtonPressed` iterava `m_visibleConsoles` con pointer
  potenzialmente stale. Fix: `setActive(false)` ora abortisce direttamente il
  `PlaybackExecutor` inline invece di passare per click→signal→slot chain.
  (`flipconsole.cpp`)

- **Crash ~FlipConsole dangling pointer** — `m_visibleConsoles` non veniva pulita
  nel distruttore. Aggiunto `~FlipConsole()` che rimuove `this` dalla lista.
  (`flipconsole.cpp`, `flipconsole.h`)

- **Crash SceneViewer/FxGadgetController (TTool::m_viewer dangling)** — al load di
  una scena il `SceneViewer` veniva distrutto ma `TTool::m_viewer` non veniva
  azzerato → crash in `onFxSwitched`. Fix: `SceneViewer::~SceneViewer()` chiama
  `TTool::onViewerDestroyed(this)` che azzera tutti i tool che puntano a quel viewer.
  (`sceneviewer.cpp`, `tool.cpp`, `tool.h`)

- **Crash PlasticDeformer SuperLU (dgstrf NaN)** — triangoli degeneri in una mesh
  producevano NaN/Inf da `ortCoords()` che venivano passati a SuperLU → crash.
  Fix: guard `isfinite()` in `initializeStep2()` salta la fattorizzazione per facce
  degeneri; `deformStep2()` usa posizione invariata quando `m_invF[f]` è null.
  (`plasticdeformer.cpp`)

- **Crash Room::save() da switchRoomChoice re-entrante** — `Room::load()` chiama
  `qApp->processEvents()` che fa scattare il `QTimer::singleShot(0)` che resettava
  `m_isHandlingWorkflow=false`, permettendo un secondo `switchRoomChoice` annidato
  che settava poi `m_isSwitchingRooms=false`. L'outer `readSettings` entrava in
  `makePrivate(rooms)` con pointer dangling → SIGSEGV. Fix: guard
  `if (m_isSwitchingRooms) return;` all'inizio di `switchRoomChoice`.
  (`mainwindow.cpp`)

- **Audio export oltre lunghezza shot** — `vsf - shotR0` usava `getVisibleStartFrame()`
  invece di `getStartFrame()` per calcolare la posizione nella colonna destinazione.
  Fix: usa `cl->getStartFrame() - shotR0`.
  (`storyboardpanel.cpp`)

- **"Load Audio" non apriva il dialog su macOS** — parent `this` invece di `nullptr`
  rendeva il dialog invisibile dietro la finestra principale. (`ztoryanimatic.cpp`)

- **Audio stale tra scene diverse** — `requireSoundTrack()` usava la cache della
  scena precedente al cambio scena. Fix: `invalidateSoundTrack()` chiamato nel
  handler `sceneSwitched`. (`ztoryanimatic.cpp`)

### Performance
- **Workflow switch verso Storyboard lento (1–3 s)** — `makeSound()` bloccava il
  main thread perché veniva chiamato da `singleShot(0)` che scattava dentro
  `qApp->processEvents()` di `Room::load()`. Fix: `preBuildSoundTrackAsync()` esegue
  `makeSound()` in un `std::thread` detached; il risultato è consegnato al main
  thread via `QMetaObject::invokeMethod(QueuedConnection)`. Zero blocking.
  (`ztoryanimatic.cpp`, `ztoryanimatic.h`)

### Modified
- `flipconsole.cpp` — `~FlipConsole()`, `setActive(false)` riscritta
- `flipconsole.h` — aggiunto `~FlipConsole()`
- `sceneviewer.cpp` — `~SceneViewer()` chiama `TTool::onViewerDestroyed`
- `tool.cpp` / `tool.h` — aggiunto `TTool::onViewerDestroyed(Viewer*)`
- `plasticdeformer.cpp` — guard triangoli degeneri in step2
- `mainwindow.cpp` — re-entrancy guard in `switchRoomChoice`
- `storyboardpanel.cpp` — fix audio export frame offset
- `ztoryanimatic.cpp` / `.h` — Load Audio fix, sceneSwitched invalidate, async sound build

### Notes
- Il ritardo al primo switch verso Storyboard è fisiologico: il Board carica le
  anteprime (500ms timer) e l'audio viene costruito in background. Non è un bug.

---
## [2026-04-19b] — Fix: double-update Board dopo operazioni Animatic

### Fixed
- **Razor, AddShot, MergeWithNext dall'Animatic aggiungevano uno shot vuoto extra nel Board**
  - Root cause: stessa classe di bug del merge double-removal. Dopo `resequenceXsheet()`
    → `modelReset()` → `onModelResequenced()` → `refreshFromScene()` il Board era già
    corretto (4 shot dopo razor), poi arrivava `emit shotAdded(newCol)` →
    `onShotInserted()` inseriva un altro shot vuoto (senza sub-scene) → Board a 5 shot.
  - Fix: rimossi `emit shotAdded()`/`emit shotRemovedAt()` da `onRazorRequested()`,
    `onAddShot()`, `onMergeWithNext()`. Il Board si sincronizza esclusivamente via
    `resequenceXsheet()` → `modelReset()` → `onModelResequenced()` (xsheet count check).

### Modified
- `ztoryanimatic.cpp` — rimossi 3 emit ridondanti post-resequenceXsheet

---
## [2026-04-19] — Shared clipboard e shared selection Board ↔ Animatic + fix merge double-removal

### Added
- **Shared clipboard Board ↔ Animatic** (`ztorymodel.h`, `ztoryanimatic.cpp`, `storyboardpanel.cpp`)
  - `ZtoryClipEntry` struct e `m_sharedClip` in `ZtoryModel` — unica source of truth per clipboard
  - Board (`onCopyShot`, `onCutShot`, `onCloneShot`): scrive sempre su `ZtoryModel::setSharedClip()`
  - Animatic (`onCopyShots`, `onCutShots`, `onCloneShots`): usa già `ZtoryModel::sharedClip()`
  - `pasteSharedClipToBoard()` — helper statico in `storyboardpanel.cpp` che replica
    la logica di `pasteFromClip()` usando il `cloneChildToPosition()` locale
  - Board `onPasteShot()`: shared clip ha sempre priorità su `m_clipboard` locale
    (fix bug: `m_clipboard` stale con 3 shot causava incolla 3 invece di 1 dopo copy da Animatic)

- **Shared selection Board ↔ Animatic** (`ztorymodel.h`, `ztoryanimatic.cpp`, `storyboardpanel.cpp`)
  - `m_sharedSelection` (set di xsheet columns) in `ZtoryModel` con getter/setter
  - Animatic: `selectionChanged` signal → `ZtoryModel::setSharedSelection()`
  - Board `onPanelClicked()`: converte `m_selectedIndices` → xsheet columns → `setSharedSelection()`
  - Merge cross-panel: seleziona in Animatic → merge button Board funziona (e viceversa)
  - Fallback "last panel wins": vince sempre l'ultima interazione utente

### Fixed
- **Bug merge cross-panel: double-removal nel Board** (`storyboardpanel.cpp`, `ztoryanimatic.cpp`)
  - Root cause: `onModelResequenced()` usava `ZtoryModel::m_shots.size()` come riferimento
    ma quella dimensione è stale dopo operazioni copy/paste/clone che bypassano
    `ZtoryModel::addShot()/removeShot()`. Se stale ≠ Board count → `refreshFromScene()` (Board → 5 shot)
    poi arrivava anche `emit shotRemovedAt(4)` → `onShotRemovedAt()` → rimozione extra (Board → 4 shot)
  - Fix 1: `onModelResequenced()` conta le colonne child-level direttamente dall'xsheet (ground truth),
    non da `ZtoryModel::m_shots.size()`
  - Fix 2: Animatic `onMergeShots()`: rimosso `emit shotRemovedAt()` — il Board si sincronizza già
    via `resequenceXsheet()` → `modelReset()` → `onModelResequenced()`
  - Fix 3: Board `onMergeShots()`: `m_updating=true` attorno all'emit di `shotRemovedAt()` per
    prevenire self-processing (double-removal anche per merge nativo del Board)

- **Bug clipboard priorità**: Board usava `m_clipboard` locale (stale) invece dello shared clip
  - Fix: in `onPasteShot()` lo shared clip ha sempre la precedenza; `m_clipboard` è solo fallback

### Modified
- `ztorymodel.h` — aggiunti `ZtoryClipEntry`, `m_sharedClip`, `m_sharedSelection` + `#include <set>`
- `ztoryanimatic.h` — rimossi `AnimClipEntry`/`m_animClip`; commento shared clipboard
- `ztoryanimatic.cpp` — riscritta gestione clipboard; merge fix; connect selectionChanged
- `storyboardpanel.cpp` — shared clipboard write in copy/cut/clone; paste fallback; merge fix;
  shared selection write in onPanelClicked; pasteSharedClipToBoard() helper

---
## [2026-04-17] — Fix: crash BrushToolOptionsBox + AutoFill restore

### Fixed
- **`tooloptions.cpp` — crash on sub-xsheet entry and app close (`BrushToolOptionsBox::updateStatus`)**
  - Root cause: `updateStatus()` era chiamata sincronamente durante la signal chain
    dell'xsheet switch (`openSubXsheet` / `saveSceneIfNeeded`); in quel momento
    `m_pltHandle->getPalette()` può restituire un puntatore temporaneamente invalido
    → SIGSEGV in `rebuildAutoFillStyleCombo`.
  - Fix: entrambe le chiamate critiche (`rebuildAutoFillStyleCombo` +
    `notifyToolComboBoxListChanged`) deferite con `QTimer::singleShot(0, this, lambda)`,
    così vengono eseguite solo dopo che la signal chain si è completamente disfatta.
  - Aggiunto change-detection (`m_lastPalette`, `m_lastPaletteStyles`) per evitare
    rebuild superflui.
  - `try-catch(...)` non era sufficiente: SIGSEGV è un segnale Unix, non un'eccezione C++.

### Modified
- **`tooloptions.cpp`** — `BrushToolOptionsBox::updateStatus()` con QTimer deferred rebuild
- **`tooloptions.h`** — aggiunti `m_lastPalette` / `m_lastPaletteStyles` a `BrushToolOptionsBox`
- **`toonzrasterbrushtool.cpp`** — `rebuildAutoFillStyleCombo` ripristinato con lista completa
  palette; fill code ripristinato al comportamento originale (`getPaint() == 0`)
- **`toonzrasterbrushtool.h`** — `rebuildAutoFillStyleCombo(TPaletteP pal)` dichiarazione ripristinata

### Notes
- AutoFill "Fill Style" combo ora mostra di nuovo tutti i colori della palette (non solo "+1")
- Fill con antialias ripristinato al comportamento originale (era stato rimosso per errore)
- Savebox fix mantenuto: `sb = sb + m_strokeRect` per evitare scan area 1×1 al primo stroke

---
## [2026-04-16] — Fix: render preview frame bianco/trasparente

### Fixed
- **`toonz/sources/tnzbase/trasterfx.cpp` — `enlargeToI()` UB con `TConsts::infiniteRectD`**
  - Root cause definitivo identificato e corretto.
  - `enlargeToI(TRectD &r)` applica `tfloor`/`tceil` (che fanno `(int)(x)`) a `TConsts::infiniteRectD = TRectD(-DBL_MAX,-DBL_MAX,DBL_MAX,DBL_MAX)`. Cast `(int)(±DBL_MAX)` è undefined behavior; su questo Mac produce `(int)(DBL_MAX)=-1` e `(int)(-DBL_MAX)=0`, corrompendo il rect a `(-1,-1)-(0,0)`.
  - `ColorCardFx::doGetBBox` ritorna `infiniteRectD` → dopo `enlargeToI` il bbox di `overFx` diventa `(-1,-1)-(0,0)` → `interestingRect` = 1×1 pixel → tutto il render è 1 pixel trasparente.
  - **Fix**: guard in `enlargeToI` che skippa la conversione se qualsiasi coordinata supera `INT_MAX/2`:
    ```cpp
    const double kMaxSafeInt = static_cast<double>(std::numeric_limits<int>::max() / 2);
    if (r.x0 < -kMaxSafeInt || r.x1 > kMaxSafeInt || r.y0 < -kMaxSafeInt || r.y1 > kMaxSafeInt)
        return;
    ```

### Modified
- Rimossi tutti i log diagnostici `std::cerr` aggiunti nelle sessioni precedenti da:
  - `trasterfx.cpp` (logger rimosso dall'agent)
  - `tcolumnfx.cpp`, `scenefx.cpp`, `previewer.cpp`, `sceneviewer.cpp` (rimossi con Python script)

### Notes — Diagnosi completa (path del bug)
```
ColorCardFx::doGetBBox → restituisce TConsts::infiniteRectD
  → TRasterFx::getBBox chiama enlargeToI(infiniteRectD)
    → (int)(DBL_MAX) = -1 [UB sul Mac]
    → temp = TRectD(-1,-1,0,0)
    → myIsEmpty(-1,-1,0,0) = false (getLx()=1 ≥ 1)
    → r corrotto a (-1,-1)-(0,0)
  → overFx.compute: interestingRect = tileRect * (-1,-1,0,0) = 1×1 pixel
  → tutta la chain renderizza 1 pixel trasparente → frame bianco in output
```
Confirmato con log14: `[compute_extract] fx=overFx tile=1920x1080 bbox=(-1,-1)-(0,0) interesting_tile=1x1`

### Nuovo bug da investigare (sessione successiva)
- Con 2+ livelli il render a volte produce frame **nero** (intermittente)
- Con 3+ livelli il terzo livello quasi mai viene renderizzato
- In visualizzazione normale il 3° livello appare **sotto** il 2° (z-order invertito)
- Probabile causa: `TImageCombinationFx::doCompute` gestisce il livello più alto come
  "background" (render diretto sulla tile) e quelli sotto con `allocateAndCompute`.
  Se l'ordering dei port è invertito rispetto all'atteso, l'ordine di compositing
  è sbagliato. Da verificare in `binaryFx.cpp` e `scenefx.cpp` (`makePF`).

### Upstream candidate
- Il fix di `enlargeToI` è pulito e applicabile a Tahoma2D upstream: il commento
  originale diceva "the rect may become empty" ma non lo proteggeva. Fix corretto
  e backward-compatible.

---
## [2026-04-15b] — Diagnosi: render preview produce raster TRASPARENTE (bug Ztoryc-specifico)

### Modified
- `toonz/sources/toonz/sceneviewer.cpp` — `drawPreview()`:
  - Camera usata per `rasterToStageRef` cambiata da `scene->getCurrentCamera()`
    → `scene->getTopXsheet()->getStageObjectTree()->getCurrentCamera()` per
    allineare la camera a quella usata dal Previewer (in test erano già
    equivalenti 1920x1080, ma fix coerente con `Previewer::updateCamera()`).
  - Aggiunto logging diagnostico (ogni 60 frame): row, dimensioni camera
    root/sub, validità raster, pixel sample TL/Center/BR.
- `toonz/sources/toonz/previewer.cpp` — logging diagnostico in:
  - `updateCamera()`: cameraRes, renderArea, flag subcamera
  - `refreshFrame()`: previewRect, renderArea, motivi abort
  - callback render completed: dimensione raster + pixel centrale

### Notes — Scoperta chiave
Il raster **NON è bianco, è totalmente TRASPARENTE**:
```
[Previewer::renderCompleted] frame=0 rasSize=1920x1080 centerPix=(0,0,0,0)
[drawPreview] ras=valid rasSize=1920x1080 TL=(0,0,0,0) C=(0,0,0,0) BR=(0,0,0,0)
```

Tutti i pixel sono `RGBA=(0,0,0,0)` — alpha zero. Il viewer compone
il trasparente sopra `m_visualSettings.m_blankColor` (bianco di default),
**facendoci vedere bianco**.

Quindi il bug NON è:
- ❌ camera mismatch (root e sub entrambe 1920x1080)
- ❌ scheduling/trasporto (`refreshFrame` parte, `renderCompleted` firma,
  raster arriva valido al viewer con dim corretta)
- ❌ legato alle sub-scene (confermato dall'utente: succede anche
  renderizzando un disegno direttamente nel main xsheet)

**Il bug è Ztoryc-specifico**: la stessa scena aperta in Tahoma2D vanilla
renderizza correttamente. Una modifica di fork introdotta da Ztoryc rompe
il render preview → da bisettare rispetto a upstream `tahoma2d/tahoma2d`.

### Prossima sessione — piano concreto
1. **Diff con upstream Tahoma2D** — `git diff upstream/master -- toonz/sources/toonz/previewer.cpp toonz/sources/toonz/sceneviewer.cpp toonz/sources/common/tfx/` per vedere cosa Ztoryc ha toccato nel path render preview.
2. **Ricerca aree sospette**: `scenefx.cpp`, `trop.cpp`, `trasterfx.cpp`,
   qualsiasi modifica alla composizione `makeOver(bgCard, fx)`.
3. **Bisect**: se il diff è grande, `git bisect` partendo da un commit
   pre-animatic che funzionava. Candidati iniziali:
   - commit `ac5e46ca8` "Add storyboard/ztory sources" (potrebbe essere OK)
   - commit `35577720e` "ZtoryAnimaticController + dedicated TFrameHandle"
     (tocca TFrameHandle, area a rischio)
4. **Opus per analisi** — dato che il codice di rendering è denso, usare
   Claude Opus per leggere il diff upstream vs Ztoryc e identificare
   subito l'area rotta.

---
## [2026-04-15] — Indagine render preview bianco (bug ancora aperto)

### Modified
- `toonz/sources/toonz/previewer.cpp`:
  - `Previewer::Imp::buildSceneFx()`: cambiato `scene->getXsheet()` →
    `scene->getTopXsheet()` — il Previewer ora renderizza sempre dalla root
    xsheet anziché dalla sub-scene aperta. Fix corretto ma non sufficiente.
  - `Previewer::Imp::updateCamera()`: cambiato `scene->getCurrentCamera()`
    (che usava `getXsheet()`, tornando la camera della sub-scene aperta) →
    `scene->getTopXsheet()->getStageObjectTree()->getCurrentCamera()`.
    Camera del Previewer ora sempre allineata alla root xsheet.
  - Aggiunti include: `toonz/fxdag.h`, `toonz/tcolumnfxset.h`,
    `toonz/tstageobjecttree.h` (necessari per i fix).

### Notes — Bug render preview ancora aperto
Il render preview mostra bianco sia nel viewer animatico che in quello nativo.

**Investigazione effettuata:**
- Debug confermato: il FX tree è valido end-to-end:
  - Root xsheet: `cols=6, frameCount=214, termFxs=4` → `fxA=non-null`
  - Sub-scene: `subCols=1, subTermFxs=1, outputConnected=1` → `buildFx=non-null`
- Il render completa e `ras=VALID` (raster non-null restituito al viewer).
- `buildSceneFx()` in `scenefx.cpp` fa sempre `makeOver(bgCard, fx)` — quindi
  `fxA=non-null` non garantisce contenuto visivo (potrebbe essere solo bgCard).
- GL error 1286 (`GL_INVALID_FRAMEBUFFER_OPERATION`) pre-esistente, non causa
  del bianco (LUT non attiva, `lutValid=0`).

**Ipotesi ancora da verificare:**
1. La `drawPreview()` in `sceneviewer.cpp` usa ancora
   `scene->getCurrentCamera()` per calcolare `rasterToStageRef` — se la camera
   della sub-scene ha dimensioni diverse dalla root, l'immagine potrebbe essere
   mappata fuori dal viewport.
2. Il raster renderizzato potrebbe contenere effettivamente solo il colore
   sfondo (bianco) perché le sub-scene, pur avendo `termFxs=1`, non producono
   pixel visibili per qualche ragione ancora ignota (palette? DPI? blend mode?).
3. Il Previewer singleton potrebbe condividere cache tra viewer diversi in modo
   conflittuale.

**Prossima sessione — cosa fare:**
- Fixare `drawPreview()` in `sceneviewer.cpp` per usare la root xsheet camera
  nel calcolo di `rasterToStageRef`.
- Aggiungere debug mirato al valore dei pixel del raster renderizzato (es.
  `ras->pixels(0)[0]`) per capire se il contenuto è bianco o trasparente.
- Considerare di usare Opus per analisi più profonda.

---
## [2026-04-09] — Camera mismatch parziale fix + design room unificata

### Fixed
- **`getViewMatrix()` rimossa logica errata `getTopXsheet()`** (`sceneviewer.cpp`): il branch `if (m_alwaysMainXsheet)` in `getViewMatrix()` usava `getTopXsheet()` (camera root = identity) rendendo il viewer animatic cieco alle camera delle sottoscene. Rimosso: ora usa sempre `getCurrentXsheet()` + `TApp::getCurrentFrame()` (comportamento originale Tahoma2D). Il `m_customFrameHandle` resta solo per `drawScene()` dove serve il frame animatic per renderizzare la root xsheet al frame corretto.

### Notes
- **Bug aperto — Camera mismatch inside shot**: il mismatch tra viewer animatic e ComboViewer quando si è dentro uno shot persiste. La causa root è che Stage NON applica la camera della sottoscena quando renderizza dalla root xsheet (la camera sub-scene è applicata solo quando si è *dentro* la sottoscena). Il viewer animatic renderizza sempre la root xsheet, quindi non può applicare la camera delle singole sottoscene via `getViewMatrix()`. Richiede investigazione approfondita su Stage::visit() o una soluzione alternativa (e.g. quando si è dentro uno shot, il viewer animatic usa getCurrentXsheet() come shot viewer).
- **Design room unificata discussa**: proposta utente di room SHOT+ANIMATIC con toggle (QStackedWidget Left: Board↔XSheet, Center: AnimaticViewer↔ComboViewer, Right: Script+Inspector↔Palette+SmallViewer). Fasi: sprint 1 = toggle Left+Center + highlight giallo shot corrente.

---
## [2026-04-08] — Animatic viewer: marker indipendenti, camera view, real-time update

### Fixed
- **TSoundTrackP dangling pointer** (`viewerpane.h`): `m_sound` era `TSoundTrack*` raw — diventava dangling quando `m_mixedSound` veniva liberato da `invalidateSound()`. Fix: cambiato a `TSoundTrackP` (smart pointer). Null check aggiornati da `!= NULL` a `if (m_sound)`.
- **AutoFill combo non si popolava** (`tooloptions.cpp`): `m_controls` è indicizzato per `getName()` = `"Fill Style:"`, non per `getId()` = `"AutoFillStyle"`. Fix: corretti 3 punti in `tooloptions.cpp` (lookup, filter set, notifyToolComboBoxListChanged).
- **Mute/solo interferisce con ComboViewer nativo** (`viewerpane.cpp`, `ztoryanimatic.cpp`): quando sia il ComboViewer nativo che l'animatic viewer erano aperti, i rispettivi `play()` competevano per lo stesso `TSoundOutputDevice`. Fix quickfix: `ownsAudioAtMainLevel()` in `ZtoryAnimaticController` — il viewer nativo cede il controllo audio quando siamo a main level e l'animatic è aperto (gated su `isStoryboardWorkflow()`).
- **Marker animatic si spostavano entrando in uno shot** (`ztoryanimatic.cpp`, `ztoryanimatic.h`): `openSubXsheet()` sovrascriveva `XsheetGUI::setPlayRange()` con il range della sottoscena — storage singolo globale. Fix: range indipendente `m_animaticR0/m_animaticR1` in `ZtoryAnimaticController`; `updateFrameMarkers()` virtuale overridato in `ZtoryAnimaticViewer` per leggere sempre dallo storage proprio.
- **Camera animatic viewer non si aggiornava in real-time** (`ztoryanimatic.cpp`): aggiunto `objectChanged → m_sceneViewer->update()` in `showEvent()` — `objectChanged` si emette durante il drag interattivo di camera/peg, che era già connesso in `SceneViewer::showEvent()` ma non sopravviveva ai cicli disconnect delle sottoscene.

### Added
- **CAMERA_REFERENCE come default** (`ztoryanimatic.cpp`): l'animatic viewer si avvia in camera view — mostra l'inquadratura della sottoscena corrente senza che l'utente debba cambiare modalità manualmente.
- **`getViewMatrix()` usa root xsheet per animatic** (`sceneviewer.cpp`): quando `m_alwaysMainXsheet` è true, `getViewMatrix()` usa `getTopXsheet()` (camera del main) invece di `getCurrentXsheet()` (camera della sottoscena), evitando doppia applicazione della camera. Usa `m_customFrameHandle` per il frame animatic invece di `getCurrentFrame()` (che punterebbe alla frame della sottoscena).

### Notes
- **Bug aperto**: real-time update della camera mentre si edita all'interno di uno shot ancora da verificare dopo l'aggiunta di `objectChanged`. Il build `4f48e4da5` include il fix.
- **Design session in sospeso**: toggle Animatic↔ComboViewer rooms (architettura rooms definitiva).

---
## [2026-04-08] — Fix crash mute scene vecchie + antialias autofill + palette picker

### Fixed
- **Crash SIGABRT mute su scene vecchie** (`txshsoundcolumn.cpp`): `mixingTogether()` aveva `assert(soundLevel)` attivo in RelWithDebInfo — se l'audio file di una scena vecchia ha un riferimento rotto, `l->getSoundLevel()` ritorna null → assert → crash. Fix: sostituito con `if (!soundLevel) return mix`. Stesso fix in `getOverallSoundTrack()`: `overallSoundTrack->blank()` crashava se `TSoundTrack::create()` aveva lanciato un'eccezione (overallSoundTrack null). Fix: guard `if (!overallSoundTrack) return`. Aggiunto anche null check per `soundLevel` nel loop degli sound levels. Upstream candidate fix per Tahoma2D.
- **Bordino bianco tra linea e fill (antialias autofill)**: il BFS usava `getInk() != 0` come barriera (corretto), ma la fill condition richiedeva `getInk() == 0` — i pixel antialiased interni (ink>0, tone>0) venivano esclusi → gap bianco. Fix: rimossa condizione `getInk() == 0` → fill su tutti i pixel interni con `getPaint() == 0`. Per pixel puramente inchiostrati (tone=0) il paint viene settato ma il canale ink domina visivamente — nessun impatto.

### Added
- **AutoFill palette picker** (`toonzrasterbrushtool.h/.cpp`, `tooloptions.h/.cpp`): il combo "Fill Style" ora si popola dinamicamente con tutti gli stili della palette corrente (oltre a "Next Style (N+1)" e "Current Style"). Rebuild automatico quando cambia la palette o il livello. Ogni stile appare come `[N] NomeStile`. Selezione persistente tra un refresh e l'altro.

---
## [2026-04-08] — Fix crash audio mute + mute immediato durante play + AutoFill picker

### Fixed
- **Crash heap corruption su mute (scena con audio lungo)**: `makeSound()` con `fromFrame=-1, toFrame=-1` → `mixingTogether()` usava `getFrameCount()` inflato (durata file raw, potenzialmente ore) → buffer centinaia di MB → corruzione heap. Fix in `viewerpane.cpp`: bounded `prop->m_toFrame` al frame count delle sole colonne video (`maxFrame` da `col->getRange()`).
- **Crash heap corruption durante `refreshAudioTracks()`**: `restoreTrackStates()` chiamava `applyMuteSolo()` → `invalidateSound()` + restart audio device mentre ancora in play → corruzione. Fix: `restoreTrackStates()` ripristina solo stato UI (checked/unchecked), non tocca il device audio.
- **Null dereference in viewerpane.cpp**: `m_sound->getSampleRate()` chiamato prima del null check → spostato `if (!m_sound) return` prima del dereference.
- **Mute non ha effetto immediato durante play**: `applyMuteSolo()` chiamava `stopScrub()`/`play()` dal click handler, in race con i callback CoreAudio XPC → EXC_BAD_ACCESS. Fix: flag `m_pendingAudioRestart` settato da `applyMuteSolo()`, consumato in `onDrawFrame()` che viene chiamato dal Qt timer tra i callback XPC — contesto sicuro per `stopScrub()`/`play()`. Il mute ora è effettivo entro il prossimo frame (~40ms).
- **Mute/solo non persistente dopo `refreshAudioTracks()`**: stato salvato in `m_colMuted`/`m_colSolo`, ripristinato in `restoreTrackStates()`.

### Added
- **AutoFill fill style picker** (`toonzrasterbrushtool.h/.cpp`, `tooloptions.cpp`): nuovo `TEnumProperty m_autoFillStyle` con valori "Next Style (N+1)" (default, comportamento precedente) e "Current Style" (riempie con lo stile attualmente selezionato in palette). Il combo appare nella toolbar del brush tool accanto al checkbox AutoFill. Aggiunto anche `invalidate()` dopo autofill per aggiornare il canvas subito dopo mouseUp senza aspettare il prossimo hover.

### Notes
- Pattern sicuro per restart audio durante play: flag `m_pendingAudioRestart` → `restartAudioIfPlaying()` da `onDrawFrame()`.
- `stopScrub()`/`play()` sono sicuri solo se chiamati tra i callback CoreAudio XPC (Qt timer) — non dai click handler UI.

---
## [2026-04-06] — Board desync fix (merge/cut/delete), edit shot fix, durate panel, match button

### Fixed

- **3-shot merge lascia uno shot in più nel Board** (`storyboardpanel.cpp`, `onShotRemovedAt`):
  quando il secondo `shotRemovedAt` non trova lo shot per `data.xsheetColumn` (tracking
  desynced da operazioni precedenti), ora cade back su `refreshFromScene()` invece di
  tornare silenziosamente.

- **Edit shot button non selezionava lo shot** (`storyboardpanel.cpp`, `onEditShot`):
  aggiunto `selectShot(shotIdx)` prima di aprire la sottoscena.

- **Edit shot button usava board index come colonna xsheet** (`storyboardpanel.cpp`, `onEditShot`):
  ora usa `m_shots[shotIdx].data.xsheetColumn` — fix critico dopo merge/cut che desincronizzano
  indice Board dall'indice xsheet.

- **T: (durata totale) aggiornava panels[0].duration invece del display** (`onXsheetChanged`):
  per shot multi-panel questo sovrascriveva la durata parziale del panel 0 con la durata
  totale. Ora `onXsheetChanged` aggiorna solo il display T: per tutti i panel; D: (parziale)
  viene aggiornata solo per shot a panel singolo (dove D: == T:).

- **D: (durata parziale) includeva frame nascosti** (`detectAndUpdatePanels`):
  l'ultimo panel usava `numFrames` (frame count completo della sottoscena, inclusi frame
  oltre la durata visibile in timeline). Ora legge la durata visibile dalla colonna del
  main xsheet ancestor e cappa l'ultimo panel al limite timeline.

- **Panel oltre l'area visibile in timeline venivano mostrati nel Board**
  (`detectAndUpdatePanels`): aggiunto filtro — i panel con `startFrame >= timelineDuration`
  vengono esclusi dal Board.

### Added

- **Bottone ⇔ (Match Duration)** (`storyboardpanel.h/.cpp`, `PanelWidget`):
  ogni shot nel Board ha un piccolo bottone ⇔ accanto al campo T:. Quando cliccato,
  legge il `getFrameCount()` reale della sottoscena e ridimensiona la colonna nel main
  xsheet di conseguenza, poi chiama `resequenceXsheet()`. Consente di allineare la durata
  timeline alla durata effettiva della sottoscena.

### Notes

- `detectAndUpdatePanels` è chiamato dal `m_panelDetectTimer` (1000ms debounce) mentre
  si è dentro una sottoscena. Ora richiede un AncestorNode valido per calcolare
  `timelineDuration`; se l'ancestor non è disponibile, usa `numFrames` come fallback.
- Il bottone ⇔ è visibile in tutti i panel dello shot ma opera sempre sulla colonna
  dell'intero shot nel main xsheet.

---
## [2026-04-05] — Icone toolbar QToolButton, SVG Ztoryc, camera init sottoscene

### Modified

- **QPushButton → QToolButton in toolbar** (`storyboardpanel.h/.cpp`, `ztoryanimatic.cpp`):
  tutti i bottoni toolbar convertiti da QPushButton con testo a QToolButton con icone SVG
  via `createQIcon()`. Stile uniforme: `setFixedSize(28,28)`, `setIconSize(20,20)`,
  background trasparente, hover `#555`, checked `#666`.
  Connect aggiornati da `&QPushButton::clicked` a `&QToolButton::clicked`.

### Added

- **21 icone SVG Ztoryc** (`toonz/sources/toonz/icons/dark/ztoryc/`, `toonz.qrc`):
  `ztoryc_add_shot`, `ztoryc_delete_shot`, `ztoryc_merge`, `ztoryc_edit_shot`,
  `ztoryc_numbering`, `ztoryc_export_pdf`, `ztoryc_export_animatic`, `ztoryc_export_shots`,
  `ztoryc_select`, `ztoryc_razor`, `ztoryc_av_link`, `ztoryc_av_link_on`, `ztoryc_onion`,
  `ztoryc_onion_on`, `ztoryc_lock`, `ztoryc_lock_on`, `ztoryc_copy`, `ztoryc_clone`,
  `ztoryc_paste`, `ztoryc_shotedit`, `ztoryc_shotedit_on`, `ztoryc_refresh_preview`.
  Embedded nel binario via qrc. Toggle on/off gestiti automaticamente da `createQIcon`.

- **Camera init sottoscene** (`storyboardpanel.cpp`, `onAddShot()`): copia res e size
  dalla camera del main xsheet alla nuova sottoscena, stesso comportamento di
  `subscenecommand.cpp`. Risolve la piccola differenza di inquadratura tra sottoscena
  e main su scene create con Ztoryc.

### Removed

- `m_refreshButton` — rimosso da header, cpp e layout Board (refresh automatico
  con debounce già attivo).
- `m_backButton` — rimosso da header, cpp e layout Board (doppio click per tornare
  al Board già implementato).

### Notes

- Per aggiornare un'icona: sostituire il file SVG in `icons/dark/ztoryc/` e ricompilare.
  Se l'icona non cambia dopo la modifica al qrc: `ninja -C toonz/build -t clean` poi rebuild.
- Il bottone merge nel Board (`m_mergeButton`) è presente ma disabilitato
  (`setEnabled(false)`) — implementazione pendente come task aperto.
- Edit In Place deve essere **spento** quando si lavora sulla camera dentro uno shot.
  Con Edit In Place spento la camera locale funziona correttamente.
  L'audio del main si sente anche con Edit In Place spento — comportamento corretto.

---
## [2026-04-03] — Audio track L/M/S buttons, mute/solo fix, crash fix, cursor jump fix

### Fixed
- **Crash on mute (memory corruption of free block)**: `m_sound` (raw ptr in base
  `viewerpane.h`) was dangling after controller released `m_soundTrack` ref. Fixed by
  giving `ZtoryAnimaticViewer` its own `TSoundTrackP m_soundTrackRef` to keep the
  object alive until `refreshAnimaticSound()` replaces it. Removed the fragile
  `soundTrackInvalidating` signal approach.
- **Mute/Solo not updating during playback**: Mute handler was calling `setVolume()`
  directly without going through `applyMuteSolo()`, so solo state was ignored and
  `restartAudioIfPlaying()` was never called. Now both M and S delegate entirely to
  `applyMuteSolo()` via signals. `applyMuteSolo()` invalidates both TXsheet internal
  cache (`xsh->invalidateSound()`) and controller cache, then calls
  `restartAudioIfPlaying()` synchronously.
- **Solo logic**: Fixed `effectiveMute = muted || (hasSolo && !solo)` — M wins over S.
  Previously used `hasSolo ? !solo : muted` which gave wrong result when M+S both active.
- **`applyMuteSolo()` corrupting m_muted state**: Was calling `at->setMuted()` to
  apply solo overrides, which destroyed the user's own mute flag. Now uses separate
  `m_effectiveMuted` bool (set by `setEffectiveMuted()`) for visual dim only.
- **Cursor jumps right after audio cut/move**: `segmentMoved` lambda was calling
  `xsh->updateFrameCount()` which included long audio columns (trailing ColumnLevel
  with `endOffset=0` after razor cut = raw file length). Removed the call; animatic
  length is driven by video shots, not audio.
- **Selection not clearing on razor cut**: `m_selSeg` was never reset when razor was
  active (selection logic gated on `!m_razorActive`). Now cleared when razor fires.

### Added
- **L/M/S painted buttons** on audio track headers (horizontal row, 22×16px each).
  Pure paint approach — no QToolButton children (they don't render in custom-painted
  QWidgets on macOS).
- **Lock painted button** on video track header.
- **Waveform dim overlay** when track is muted (M) or solo-silenced — semi-transparent
  black rect over waveform area.
- **`m_effectiveMuted` flag** on `ZtoryAudioTrack`: tracks solo-silenced state
  separately from user's `m_muted`, so applyMuteSolo never corrupts user state.
- **`restartAudioIfPlaying()`** on `ZtoryAnimaticViewer`: rebuilds merged track and
  calls `mainXsh->play()` in-place (no stopScrub) so QAudioOutput hot-swaps data.
- **`ZtoryAnimaticController::setViewer/viewer()`**: lets the panel call
  `restartAudioIfPlaying()` on the viewer without a direct reference.

### Notes
- Audio update during play has ~100ms latency (QAudioOutput hardware buffer drain
  time) — same as DaVinci Resolve. Acceptable.
- M + S both active on same track: M wins (track is muted). Both S active: both play.

---
## [2026-04-01] — NLE audio track: zoom, edge trim, overlap, add track, cross-track

### Fixed
- **Razor audio split**: `splitAudioColumn` ripristinata a `splitLevelAtFrame` (nessun frame perso). `findSegments()` ora itera `ColumnLevel` direttamente (non celle xsheet) → segmenti razor indipendentemente selezionabili e trascinabili
- **Zoom/scroll audio lungo**: `updateTrackWidths()` calcola la larghezza totale includendo sia i blocchi video che i range audio — i file audio lunghi non vengono più tagliati
- **Cut lines fantasma**: cut lines ora mostrate solo dove c'è audio nel punto di taglio; aggiornate dopo ogni `segmentMoved` e `shotDurationChanged`
- **Cursore hover edge**: `SizeHorCursor` su hover pixel-based ai bordi segmento (non solo al click)

### Added
- **Edge trim segmenti audio**: drag bordo sinistro/destro per accorciare o allungare il segmento; commit via `modifyCellRange` (nessun frame audio perso all'interno del ColumnLevel)
- **Overlap prevention**: durante `SegmentDrag` il movimento è clampato contro i segmenti adiacenti per evitare sovrapposizioni nella stessa traccia
- **Add Audio Track**: context menu panel → inserisce nuova colonna sound vuota nell'xsheet
- **Cross-track segment move**: drag segmento fuori dalla traccia → drop su altra traccia; posizionamento preciso con `dragOffset`; clamp anti-overlap sulla traccia destinazione

### Modified
- `TXshSoundColumn`: `getColumnLevel`/`getColumnLevelCount` spostati da `protected` a `public`; aggiunti `detachLevelByFrame` e `adoptLevel` come API pubbliche
- `refreshAudioTracks`: rimosso check `sc->isEmpty()` per mostrare tracce audio vuote (necessario per Add Audio Track)

### Notes
- Cross-track drop: se la traccia destinazione ha segmenti sovrapposti, il clamp li evita ma può posizionare il segmento in modo non intuitivo — da migliorare in sessione futura con feedback visivo durante il drag

---
## [2026-04-01] — Fix SIGSEGV salvataggio TLV (libimage ABI mismatch)

### Fixed
- **Crash SIGSEGV salvataggio TLV** (`build_and_deploy.sh`): `libimage.dylib` nel bundle era un residuo di una build Debug precedente. `TLevelWriterTzl` (in `libimage`) leggeva `m_creator` a `this+0x48`, ma il nuovo `libtnzcore` RelWithDebInfo lo scrive a `this+0x50` (8 byte di differenza per layout di `TSmartObject`). Fix: aggiunto deploy di `libimage` con `install_name_tool` che patcha il rpath `libtiff` da `/usr/local/lib/libtiff.5.dylib` → `@executable_path/libtiff.5.dylib` (il path `/usr/local/lib` non esiste su questo Mac).

### Notes
- Root cause: `libimage` e `libtnzcore` devono essere sempre della stessa build. Qualsiasi cambio di build type (Debug/RelWithDebInfo/Release) richiede di ri-deployare `libimage`.
- `libpng` e `libjpeg` linkati via `/opt/homebrew` — risolvono correttamente a runtime.
- `libcolorfx` e `libtnzstdfx` NON deployate: dipendono da `libimage` ma non cambiano → usano quella nel bundle già aggiornata.

---

## [2026-06-05] — PSD fix, crash fix, camera overlay Phase 1

### Fixed
- **PSD first layer "not found" as sub-scene** — root cause: `getLevelPathAndSetNameWithPsdLevelName` replaced `##` → `#` unconditionally, turning `file##group.psd` (empty name + group mode, common in Affinity Designer 16-bit PSDs) into `file#group.psd` where "group" was misread as a layer name. Fix: two-part — (1) skip replace for mode keywords; (2) fallback in TLevelReaderPsd reader. Commit `5b8eeb3c1`. PR candidate upstream.
- **Crash on quit / workflow switch (OpenGL static destructor)** — `signalHandler` tried to open QDialog during Qt static destructor teardown → abort. Fix: `s_appExiting` flag set on `aboutToQuit`. Commit `428f6c0d9`.
- **Audio +1 frame in exported shots** — `getRange()` without `ignoreLastStop=true` included stop-hold frame. Commit `976db07c4`.
- **PDF fps hardcoded to 24** — now reads from scene output properties. Camera single keyframe no longer creates panel boundary. `sub-scene` → `shot` in tooltips.

### Added
- **Camera move overlay on Board thumbnails (Task 40 Phase 1)**: PanelData stores camera affines at panel start/end; `computeCameraMove()` classifies Pan/Tilt/TrkIn/TrkOut; `applyCameraOverlay()` draws red IN/OUT rectangles + labels on thumbnails. Applied in Board view and PDF export. Persisted in `.ztoryc`. Commit `d19a84551`. Phase 2: backed-out wide render for Pan/Tilt.

### Notes
- PSD bug is PR candidate for Tahoma2D upstream (added to AGENTS.md).
- Camera overlay still needs tuning (Phase 2: Pan wide render, editable label).

---
