<!-- DRAFT — Screenshot reali in ./screenshotDemo/.
     Da fornire: room switcher (barra room) e dialog Connect to Kitsu. -->

# Ztoryc — User Manual / Manuale Utente

> Ztoryc is an open-source storyboard & animatic pipeline built on Tahoma2D 1.6.
> Ztoryc è una pipeline open-source di storyboard e animatic costruita su Tahoma2D 1.6.

> 📌 Written for **Ztoryc 0.13.1**. / Aggiornato alla **0.13.1**.

> 💬 **Need help?** Join the [Ztoryc Discord](https://discord.gg/ZP2gqQwmDb) — questions, bug reports and
> release announcements.
> 💬 **Serve aiuto?** Entra nel [Discord di Ztoryc](https://discord.gg/ZP2gqQwmDb) — domande, segnalazioni e
> annunci delle nuove versioni.

---

# 🇬🇧 English

## 1. What is Ztoryc?

Ztoryc turns Tahoma2D into a pre-production tool: you draw your **storyboard**,
time it into an **animatic** (with audio), track production, and export it — all in
one app. Each **shot** is a column in the main timeline and a self-contained
**sub-scene** you can open and animate.

## 2. First launch

**macOS** — the app is not notarized. After copying `Ztoryc.app` to
`/Applications`, run in Terminal:

```bash
xattr -cr /Applications/Ztoryc.app
```

Then open it (double-click → right-click → *Open* the first time).

**Windows** — a **clean install** is recommended: uninstall any previous version first.

## 3. The Rooms

Ztoryc is organised into **Rooms** (workspaces). Switch from the room tabs at the
top of the window.

<!-- [screenshot da aggiungere: barra delle room] -->

- **Ztoryc X** and **Ztoryc T** — the two **main rooms**; from either you can do
  almost everything needed to build the storyboard. **X** is for people who prefer
  the **X-sheet**, **T** for people who prefer the **timeline**. (See §4.)
- **Browser** — a file explorer to **import assets** and **open projects**.
- **Production Tracker** — task tracking, team, assets, Kitsu. See §12.
- **Thumbnails** — rough-sketch the whole board on one canvas, on screen or on
  paper. See §13.

## 4. The two modes of Ztoryc X / T

Both main rooms work in **two modes** and swap their panels automatically when you
switch.

### Animatic mode
- **Left** — the **Board panel** (the shot grid).
- **Centre** — the **Animatic viewer**.
- **Right** — the **Script panel** and **audio recorder** (record a guide track
  directly inside the app).

Here you load/edit audio on one or more tracks, add text to the board, run shot
operations (add, delete, copy, clone, merge) and change shot timing with the
editing tools. **Changing a shot's timing on the animatic timeline automatically
moves the shot's internal OUT marker**, so you always know how many frames remain
to be filled.

![Animatic mode](screenshotDemo/03_ANIMATIC.png)

### Shot editing mode
**Double-click a shot** (from the board *or* the animatic timeline) to enter it.
Here you draw poses, add levels, animate, and set **camera movements**.
- **Main audio** toggle — hear the slice of audio that belongs to the shot.
- **Match shot duration** toggle — the timeline duration follows the shot's
  internal duration.

### How the panels swap
- **Ztoryc X** — Board → **X-sheet**; Animatic viewer → **native viewer** (shows
  only the current shot); Script → **Palette + Style Editor + Global Palette**.
- **Ztoryc T** — Board → **single-shot Board** (current shot only); Animatic
  timeline → **native timeline**.

![Ztoryc X — shot editing mode](screenshotDemo/04_SHOT_X.png)

![Ztoryc T — shot editing mode](screenshotDemo/05_SHOT_T.png)

A separate **Ztoryc monitor panel** can be placed on a second screen to keep the
whole animatic visible even while in shot editing mode.

## 5. Startup page — create a new project

On launch (or *File → New Ztoryc Project*) the startup page gathers everything to
start a storyboard.

![Startup page](screenshotDemo/00_STARTUP.png)

**Project** — *Project name*, *Location* (*Browse…*), *Production* (show name,
used in naming/export), *Title* (episode/project title).
**Camera** — *Format* preset, *Width/Height* (px), *FPS*, *Duration (frames)*.
**Workflow** — *STORYBOARD* (default), *ANIMATIC*, *STOPMOTION*.
**Shot Numbering**
- *Style* — *Simple* (`sh010, sh020…`) or *Sequence* (`sq01_sh010, sq01_sh020…`).
- *Sequence prefix / Shot prefix* — letters before the numbers (`sq`, `sh`).
- *Step* — increment between shots (10 → sh010, sh020 — leaves room to insert).
- *Padding* — digits in the number (`3` → `010`).
- *Start number* — the first shot's number.
- *Initial shots* — how many empty shots to create up front.

## 6. Load scene / Create new scene

![Browser room](screenshotDemo/06_BROWSER.png)

- **Open a project / scene** from the **Browser** room, or *File → Open*.
- **Create a new scene** from *File → New Ztoryc Project* (the startup page) or
  from the standard *New Scene* command.
- On opening a scene, Ztoryc can **auto-detect the right workflow** (see §7).

## 7. Workflow

The **Workflow** menu (top menu bar) switches the active workflow/technique at any
time. With *Automatic* enabled, opening a scene picks the matching workflow from
its role/technique (storyboard, cutout, stop-motion, …). **Storyboard mode
options** — the storyboard-specific choices (numbering, board layout) are set on
the startup page (§5) and the Production Tracker's *Project* tab (§12).

## 8. The Board — texts, panels, arrows

In **Animatic mode** the Board panel is the grid of shots.
- **Insert text** into the board cells (dialog/action/notes for each panel).
- **Panels auto-creation** — panels are created automatically from the drawings
  you make inside a shot (one panel per drawing/exposure).
- **Arrows / annotations** — draw arrows and annotations over a panel to indicate
  motion or staging.

## 9. Shots & sequences operations

From the Board or the animatic timeline:
- **Add** a shot (before/after the current one).
- **Copy** (a shared instance) or **Clone** (a fully independent sub-scene).
- **Merge** adjacent shots into one.
- **Delete** a shot.
Shots can be grouped into **sequences** (see numbering in §5); reorder by drag.

## 10. Audio

- **Load audio track** — import an audio file onto the main timeline (audio lives
  only on the main timeline, never inside sub-scenes).
- **Add audio track** — work with **one or more** tracks.
- **Record** a guide track directly from the audio recorder (right side, Animatic mode).
- **Audio track options** — name, **volume**, **solo/mute**, waveform display.
- **Main audio toggle** (shot editing mode) — hear the shot's slice of audio.

## 11. Animatic timeline — editing

- **Transitions** — cross-dissolve between adjacent shots (overlap frames).
- **Editing tools** — **razor** (split a shot), **roll** edit, ripple, slide, merge.
- **Modifying track heights** — drag the lower edge of a video/audio track; heights
  are remembered.
- **Match shot duration** toggle (shot editing mode) — see §4.

## 12. Production Tracker room

A Kitsu-style matrix: **rows = shots, columns = tasks**. Each cell shows the task
**status** with its Kitsu colour (Todo / Ready / WIP / WFA / Retake / Done). Tabs:
**Shots · Project · Team · Assets · Workflows**.

![Production Tracker](screenshotDemo/01A_PT_production.png)

![Project / production data](screenshotDemo/01A_PROJECT.png)

![Shots](screenshotDemo/01AB_PT_SHOTS.png)

![Assets](screenshotDemo/01C_PT_ASSET.png)

![Team](screenshotDemo/01D_PT_TEAM_V2.png)

![Workflows](screenshotDemo/01E_PT_WORKFLOW.png)

- **Project / production data** — production, **code**, season, episode, title,
  default technique, naming pattern.
- **Shots** — every shot from every storyboard, with timing and per-task status
  (click to edit; shift-select for batch edits, with undo).
- **Assets** — the project asset list with its own task pipeline.
- **Team** — the people on the production.
- **Workflows** — define techniques and their task types.
- **Kitsu connection** — *Connect to Kitsu…* (see below).
- **Export Project Spreadsheet** — one `.xlsx` with every storyboard and tab.

### Kitsu connection (M5)
*Project → Connect to Kitsu…* opens the connection dialog.

<!-- [screenshot da aggiungere: dialog Connect to Kitsu] -->

1. **URL / email / password → Connect** — shows open projects + status mapping.
2. **Link selected** — bind an existing Kitsu project and pull its metadata
   (Production and Code become read-only — Kitsu owns them).
3. **Create new in Kitsu** — create the project from your Ztoryc fields *(needs an
   admin/manager role)*.
4. **Push shots to Kitsu →** — send the shot list up (push-only: never pulled back).

## 13. Thumbnails room

One large gridded canvas to rough-sketch the **whole** board quickly. Draw
thumbnails, then **Export to Board**: each panel becomes a real shot (sub-scene +
`Rough` level). Adjacent panels can be **merged as a panorama** (one wide image for
camera moves). Includes a transform tool (move/copy/scale/rotate + lasso),
undo/redo and wheel-zoom.

![Thumbnails room](screenshotDemo/02_THUMBS.png)

### From paper and back

The room also works **on paper**.

- **Print Thumbnail Sheet** — an A4 PDF of the grid, either **blank to draw on**
  or **carrying the thumbnails you already have**.
- Draw on it, then bring it back: **import a photo or a scan** — the sheet is
  de-warped and cropped, and each panel drops into its cell — or **shoot it with
  a webcam or a capture card** and import straight from the camera.

Two things worth knowing. The sheet must have been printed for the **same number
of columns** as the room: rows simply grow, but a column is never split across
pages. And panels land in **capture order**, not by the page number printed on
the sheet — blank sheets are made to be photocopied, so every copy carries the
same code. Empty cells never overwrite what is already there, and panels drawn
too faint to be detected are skipped and listed by name, so you can go over them
darker and shoot again.

## 14. Load script

Import a screenplay into the **Script panel** (right side, Animatic mode). The
script stays available with the project so you can reference it while boarding.

## 15. Shot editing — drawing, camera, light

Inside a shot (double-click to enter):
- **Poses, levels, animation** with Tahoma2D's drawing tools.
- **Camera movement** — animate the camera **here**, inside the sub-scene (never
  from the animatic).
- **Light direction arrow** — a gizmo to set the light direction for the panel,
  with its own options.

## 16. ZtoRig — poses, correctives, mouths

*Panels → ZtoRig.* The rigging side of Ztoryc, for characters built on a
**plastic skeleton**. It does not replace the Plastic tool: it sits on top of it
and remembers what you build. Three tabs.

**Poses** — record the current pose as a named **action**, then reuse it. Every
frame carries a *strength*: `0` is the rest pose, `1` the pose exactly as
recorded. Values outside `0…1` are allowed on purpose — that is how you push
past the extreme, or pull back from it. An action can be limited to one skeleton
or offered to all of them.

**Correctives** — a joint that folds badly gets a **sculpted** correction driven
by the **angle of the joint itself**: the elbow fixes its own shape as it bends,
without a key anywhere. Deleting a corrective loses the sculpted shape.

**Mouths** — the mouth sets used by the lip sync (§17). A level can hold
**several sets** — the same mouth drawn happy and sad — and a set is saved
**next to the level**, so it travels with the character: an animator who imports
the sub-scene gets the drawings *and* the instructions for using them.

> ZtoRig is the youngest part of Ztoryc and still moving.

## 17. Lip sync

Three separate commands, and the separation is deliberate: the **timing** comes
from the sound, the **mapping** belongs to the drawings and is done once per
character, and **which set** to use changes every time the character turns.

**1 — Generate the phoneme columns.** *Xsheet → Lip Sync…* for the shot you are
inside, or *Xsheet → Generate Lip Sync Columns…* for several shots at once,
picked from the Board.
- If the dialogue is **written in the panels**, the words are *aligned* to the
  recording: the text says what was said, and the engine only has to time it.
  This needs the **language** — the models are one per language.
- With nothing written, choose **Sound only (Rhubarb)**, which reads the shape
  of the waveform instead. Less precise, but it asks for nothing.

**2 — Map the mouths.** In ZtoRig's *Mouths* tab (§16), once per character.

**3 — Assign Mouth Drawings…** *(Xsheet menu)* — say which mapped set to use on
which stretch of the shot.

> ⚠️ This is **not** Tahoma2D's *Apply Lip Sync to Column*, which is the Rhubarb
> command and does a different job. This one assigns the **drawings**.

The lip sync window stays open and follows what you do: map a character in
ZtoRig and come back — the rows update by themselves.

## 18. What's new vs Tahoma2D

**Keyframe operations** (on the selected keys)
- **Keys Follow Exposure** — when on, selecting cells also selects the keys above
  them, so cells and keys move/stretch together.
- **Repeat** *(with **Loop** option)* — repeat the keys N times; *Loop* = seamless cycle.
- **Swing** — play forward then back (A→B→A).
- **Invert / Revert** — reverse the order of the keys.
- **Time Stretch** — rescale a block (cells + keys together, proportionally).
- **Increase / Decrease step** — widen or tighten the spacing of keys (and gaps).

**Cells & raster**
- **Rolling cels** — roll/ripple-trim a cell range, shifting the neighbours.
- **AutoFill on Smart Raster** — fill closed areas on smart-raster levels.
- **Image sequences with `-`** — `frame-0006.jpg` (hyphen) is read as a sequence.

## 19. Reading keyframes — the diamond

A rigged character has **two independent things** that can be keyed: the **column
transform** (position, rotation, scale…) and the **plastic pose** (the skeleton's
shape). Ztoryc shows both in a single diamond, in the xsheet and on the viewer's
**Set Key** button alike:

- the **right half empty** means the key is **partial**;
- the **left half** says *which* system holds it — **white** for the transform,
  **gold** for the plastic pose, or white over gold when a partial key holds both.

![Keyframe diamond legend](images/keyframe_diamond_legend.svg)

**Set Key in the viewer** cycles towards a complete key and only then removes it:
click a partial key to complete it, click a pose-only key to add the transform,
click a complete key to remove it. What counts as "complete" follows the **Global
Key scope** (the *Key:* dropdown in the Animate and Plastic tool options — Stage /
Plastic / All): with scope *Stage* the pose is ignored entirely and a rigged column
behaves like a plain one. Hover the button to read the current state and what the
click will do.

---

## 20. Export

- **Export spreadsheet** — `.xlsx` production worksheet (per-scene or whole project).
- **Export storyboard PDF** — printable board (optional custom studio logo).
- **Export animatic** — render the timed animatic to video.
- **Export shots as `.tnz` layouts** — each shot to a standalone `.tnz`, inheriting
  project metadata and named with the token pattern
  (`{PROD}_{CODE}_{EP}_{SEQ}_{SHOT}_{TASK}_V{VER:02}`).

---

# 🇮🇹 Italiano

## 1. Cos'è Ztoryc?

Ztoryc trasforma Tahoma2D in uno strumento di pre-produzione: disegni lo
**storyboard**, lo metti a tempo in un **animatic** (con audio), tracci la
produzione ed esporti tutto — in un'unica app. Ogni **shot** è una colonna nella
timeline principale e una **sotto-scena** indipendente che puoi aprire e animare.

## 2. Primo avvio

**macOS** — l'app non è notarizzata. Dopo aver copiato `Ztoryc.app` in
`/Applications`, esegui nel Terminale:

```bash
xattr -cr /Applications/Ztoryc.app
```

Poi aprila (doppio clic → tasto destro → *Apri* la prima volta).

**Windows** — consigliata un'**installazione pulita**: disinstalla prima eventuali
versioni precedenti.

## 3. Le Room

Ztoryc è organizzato in **Room** (spazi di lavoro). Cambi room dalle linguette in
alto.

<!-- [screenshot da aggiungere: barra delle room] -->

- **Ztoryc X** e **Ztoryc T** — le due **room principali**; da entrambe puoi fare
  praticamente tutto ciò che serve per realizzare lo storyboard. **X** è per chi
  preferisce l'**X-sheet**, **T** per chi preferisce la **timeline**. (Vedi §4.)
- **Browser** — un file explorer per **importare asset** e **aprire progetti**.
- **Production Tracker** — tracking task, team, asset, Kitsu. Vedi §12.
- **Thumbnails** — schizza l'intero board su un'unica tela, a schermo o su
  carta. Vedi §13.

## 4. Le due modalità di Ztoryc X / T

Entrambe le room principali lavorano in **due modalità** e cambiano i panel
automaticamente quando passi dall'una all'altra.

### Modalità Animatic
- **Sinistra** — il **Board panel** (la griglia degli shot).
- **Centro** — l'**Animatic viewer**.
- **Destra** — lo **Script panel** e il **record audio** (registra un audio guida
  direttamente nel software).

Qui carichi/editi l'audio su una o più tracce, inserisci testi nel board, esegui
operazioni sugli shot (aggiungi, cancella, copia, clona, unisci) e modifichi il
timing con gli editing tool. **Modificare il timing in timeline animatic sposta
automaticamente il marker OUT interno allo shot**, così sai sempre quanti frame
restano da riempire.

![Modalità Animatic](screenshotDemo/03_ANIMATIC.png)

### Modalità Shot editing
**Doppio click su uno shot** (dal board *o* dalla timeline animatic) per entrarci.
Qui disegni le pose, aggiungi livelli, animi e imposti i **movimenti di camera**.
- Toggle **Main audio** — ascolti la porzione di audio relativa allo shot.
- Toggle **Match shot duration** — la durata in timeline segue quella interna dello shot.

### Come switchano i panel
- **Ztoryc X** — Board → **X-sheet**; Animatic viewer → **viewer nativo** (mostra
  solo lo shot corrente); Script → **Palette + Style Editor + Global Palette**.
- **Ztoryc T** — Board → **single-shot Board** (solo shot corrente); timeline
  animatic → **timeline nativa**.

![Ztoryc X — shot editing mode](screenshotDemo/04_SHOT_X.png)

![Ztoryc T — shot editing mode](screenshotDemo/05_SHOT_T.png)

Un **Ztoryc monitor panel** separato può essere messo su un secondo monitor per
vedere l'intero animatic anche in shot editing mode.

## 5. Pagina di avvio — creare un progetto

All'avvio (o *File → New Ztoryc Project*) la pagina di avvio raccoglie tutto il
necessario per iniziare uno storyboard.

![Pagina di avvio](screenshotDemo/00_STARTUP.png)

**Project** — *Project name*, *Location* (*Browse…*), *Production* (nome dello
show, usato in naming/export), *Title* (titolo episodio/progetto).
**Camera** — preset *Format*, *Width/Height* (px), *FPS*, *Duration (frames)*.
**Workflow** — *STORYBOARD* (default), *ANIMATIC*, *STOPMOTION*.
**Shot Numbering**
- *Style* — *Simple* (`sh010, sh020…`) o *Sequence* (`sq01_sh010, sq01_sh020…`).
- *Sequence prefix / Shot prefix* — lettere prima dei numeri (`sq`, `sh`).
- *Step* — incremento tra gli shot (10 → sh010, sh020 — lascia spazio per inserire).
- *Padding* — cifre del numero (`3` → `010`).
- *Start number* — numero del primo shot.
- *Initial shots* — quanti shot vuoti creare subito.

## 6. Aprire una scena / Creare una nuova scena

![Room Browser](screenshotDemo/06_BROWSER.png)

- **Apri un progetto / scena** dalla room **Browser**, o *File → Open*.
- **Crea una nuova scena** da *File → New Ztoryc Project* (pagina di avvio) o dal
  comando standard *New Scene*.
- All'apertura di una scena, Ztoryc può **rilevare automaticamente il workflow** (§7).

## 7. Workflow

Il menu **Workflow** (barra dei menu) cambia il workflow/tecnica attivo in
qualsiasi momento. Con *Automatic* attivo, aprendo una scena viene scelto il
workflow giusto in base al suo ruolo/tecnica (storyboard, cutout, stop-motion, …).
Le **opzioni di storyboard mode** (numerazione, layout del board) si impostano
nella pagina di avvio (§5) e nel tab *Project* del Production Tracker (§12).

## 8. Il Board — testi, panel, frecce

In **modalità Animatic** il Board panel è la griglia degli shot.
- **Inserisci testo** nelle celle del board (dialogo/azione/note per ogni panel).
- **Auto-creazione dei panel** — i panel vengono creati automaticamente dai disegni
  che fai dentro uno shot (un panel per disegno/esposizione).
- **Frecce / annotazioni** — disegna frecce e annotazioni sopra un panel per
  indicare movimento o staging.

## 9. Operazioni su shot e sequenze

Dal Board o dalla timeline animatic:
- **Aggiungi** uno shot (prima/dopo quello corrente).
- **Copia** (istanza condivisa) o **Clona** (sotto-scena del tutto indipendente).
- **Unisci** (merge) shot adiacenti.
- **Cancella** uno shot.
Gli shot possono essere raggruppati in **sequenze** (vedi numerazione §5); riordini
con il drag.

## 10. Audio

- **Carica traccia audio** — importa un file audio nella timeline principale
  (l'audio vive solo lì, mai nelle sotto-scene).
- **Aggiungi traccia audio** — lavora con **una o più** tracce.
- **Registra** un audio guida dal record audio (a destra, modalità Animatic).
- **Opzioni traccia audio** — nome, **volume**, **solo/mute**, visualizzazione waveform.
- **Toggle Main audio** (shot editing mode) — ascolti la porzione audio dello shot.

## 11. Timeline animatic — editing

- **Transizioni** — cross-dissolve tra shot adiacenti (frame di sovrapposizione).
- **Editing tools** — **razor** (taglia uno shot), **roll** edit, ripple, slide, merge.
- **Modifica altezza tracce** — trascina il bordo inferiore di una traccia
  video/audio; le altezze vengono ricordate.
- **Toggle Match shot duration** (shot editing mode) — vedi §4.

## 12. Room Production Tracker

Una matrice in stile Kitsu: **righe = shot, colonne = task**. Ogni cella mostra lo
**status** del task col colore Kitsu (Todo / Ready / WIP / WFA / Retake / Done).
Tab: **Shots · Project · Team · Assets · Workflows**.

![Production Tracker](screenshotDemo/01A_PT_production.png)

![Project / production data](screenshotDemo/01A_PROJECT.png)

![Shots](screenshotDemo/01AB_PT_SHOTS.png)

![Assets](screenshotDemo/01C_PT_ASSET.png)

![Team](screenshotDemo/01D_PT_TEAM_V2.png)

![Workflows](screenshotDemo/01E_PT_WORKFLOW.png)

- **Project / dati di produzione** — produzione, **code**, stagione, episodio,
  titolo, tecnica di default, pattern di naming.
- **Shots** — tutti gli shot di tutti gli storyboard, con timing e status per task
  (clic per modificare; selezione multipla per modifiche in blocco, con undo).
- **Assets** — la lista asset del progetto con la sua pipeline di task.
- **Team** — le persone della produzione.
- **Workflows** — definisci le tecniche e i loro task type.
- **Connessione Kitsu** — *Connect to Kitsu…* (vedi sotto).
- **Export Project Spreadsheet** — un `.xlsx` con tutti gli storyboard e i tab.

### Connessione Kitsu (M5)
*Project → Connect to Kitsu…* apre il dialog di connessione.

<!-- [screenshot da aggiungere: dialog Connect to Kitsu] -->

1. **URL / email / password → Connect** — mostra i progetti aperti + la mappa status.
2. **Link selected** — collega un progetto Kitsu esistente e scarica i metadati
   (Production e Code diventano read-only — li gestisce Kitsu).
3. **Create new in Kitsu** — crea il progetto dai campi di Ztoryc *(richiede ruolo
   admin/manager)*.
4. **Push shots to Kitsu →** — invia la lista shot (solo push: mai riscaricata).

## 13. Room Thumbnails

Un'unica grande tela a griglia per schizzare velocemente **tutto** il board.
Disegni le thumbnail, poi **Export to Board**: ogni panel diventa uno shot reale
(sotto-scena + livello `Rough`). Panel adiacenti si possono **unire come
panoramica** (un'unica immagine larga per i movimenti di camera). Include transform
tool (move/copy/scale/rotate + lazo), undo/redo e zoom con la rotella.

![Room Thumbnails](screenshotDemo/02_THUMBS.png)

### Dalla carta e ritorno

La room funziona anche **su carta**.

- **Print Thumbnail Sheet** — un PDF A4 della griglia, o **vuoto da disegnare**
  oppure **con le thumbnail che hai già**.
- Ci disegni sopra, poi lo riporti dentro: **importi una foto o una scansione**
  — il foglio viene raddrizzato e ritagliato, e ogni panel finisce nella sua
  casella — oppure lo **riprendi con una webcam o una scheda di acquisizione** e
  importi direttamente dalla telecamera.

Due cose da sapere. Il foglio dev'essere stato stampato per lo **stesso numero di
colonne** della room: le righe crescono e basta, ma una colonna non si spezza mai
fra due pagine. E i panel si posano **nell'ordine in cui li acquisisci**, non
secondo il numero di pagina stampato sul foglio — i fogli vuoti sono fatti per
essere fotocopiati, quindi ogni copia porta lo stesso codice. Le caselle vuote
non sovrascrivono mai quello che c'è già, e i panel disegnati troppo chiari per
essere riconosciuti vengono saltati ed elencati per nome, così puoi ripassarli
più scuri e riprendere il foglio.

## 14. Caricare lo script

Importa una sceneggiatura nello **Script panel** (a destra, modalità Animatic). Lo
script resta disponibile col progetto, così puoi consultarlo mentre lavori al board.

## 15. Shot editing — disegno, camera, luce

Dentro uno shot (doppio click per entrarci):
- **Pose, livelli, animazione** con gli strumenti di disegno di Tahoma2D.
- **Movimento di camera** — anima la camera **qui**, dentro la sotto-scena (mai
  dall'animatic).
- **Freccia direzione luce** — un gizmo per impostare la direzione della luce del
  panel, con le sue opzioni.

## 16. ZtoRig — pose, correttive, bocche

*Panels → ZtoRig.* La parte di rigging di Ztoryc, per i personaggi costruiti su
uno **scheletro plastic**. Non sostituisce il Plastic tool: ci sta sopra e si
ricorda quello che costruisci. Tre tab.

**Poses** — registri la posa corrente come **azione** con un nome, e poi la
riusi. Ogni fotogramma porta una *forza*: `0` è la posa a riposo, `1` la posa
esattamente com'è stata registrata. I valori fuori da `0…1` sono ammessi
apposta — è così che spingi oltre l'estremo, o che tiri indietro. Un'azione può
essere limitata a uno scheletro solo oppure offerta a tutti.

**Correctives** — una giuntura che si piega male si corregge **scolpendola**, e
la correzione è guidata dall'**angolo della giuntura stessa**: il gomito si
aggiusta da sé mentre si piega, senza una chiave da nessuna parte. Cancellare
una correttiva perde la forma scolpita.

**Mouths** — i set di bocche che usa il lip sync (§17). Un livello può tenerne
**più d'uno** — la stessa bocca disegnata felice e triste — e un set si salva
**accanto al livello**, così viaggia col personaggio: chi importa la sotto-scena
si porta dietro i disegni *e* le istruzioni per usarli.

> ZtoRig è la parte più giovane di Ztoryc, e si sta ancora muovendo.

## 17. Lip sync

Tre comandi separati, e la separazione è voluta: il **tempo** viene dal suono,
la **mappatura** appartiene ai disegni e si fa una volta per personaggio, e
**quale set** usare cambia ogni volta che il personaggio si gira.

**1 — Genera le colonne dei fonemi.** *Xsheet → Lip Sync…* per lo shot in cui
sei dentro, oppure *Xsheet → Generate Lip Sync Columns…* per più shot insieme,
presi dal Board.
- Se il dialogo è **scritto nei panel**, le parole vengono *allineate* alla
  registrazione: il testo dice cosa è stato detto, e al motore resta solo da
  cronometrarlo. Serve la **lingua** — i modelli sono uno per lingua.
- Se non c'è niente di scritto, scegli **Sound only (Rhubarb)**, che guarda
  invece la forma dell'onda. Meno preciso, ma non chiede nulla.

**2 — Mappa le bocche.** Nel tab *Mouths* di ZtoRig (§16), una volta per
personaggio.

**3 — Assign Mouth Drawings…** *(menu Xsheet)* — dici quale set mappato usare su
quale tratto dello shot.

> ⚠️ **Non** è *Apply Lip Sync to Column* di Tahoma2D, che è il comando di
> Rhubarb e fa un altro lavoro. Questo assegna i **disegni**.

La finestra del lip sync resta aperta e segue quello che fai: mappi un
personaggio in ZtoRig e torni indietro — le righe si aggiornano da sole.

## 18. Novità rispetto a Tahoma2D

**Operazioni sulle chiavi** (sulle chiavi selezionate)
- **Keys Follow Exposure** — se attivo, selezionando le celle selezioni anche le
  chiavi sopra: celle e chiavi si spostano/stirano insieme.
- **Repeat** *(con opzione **Loop**)* — ripete le chiavi N volte; *Loop* = ciclo continuo.
- **Swing** — riproduce avanti e poi indietro (A→B→A).
- **Invert / Revert** — inverte l'ordine delle chiavi.
- **Time Stretch** — riscala un blocco (celle + chiavi insieme, in proporzione).
- **Increase / Decrease step** — allarga o stringe la spaziatura delle chiavi (e i gap).

**Celle & raster**
- **Rolling cels** — roll/ripple-trim di un intervallo di celle, spostando i vicini.
- **AutoFill sullo Smart Raster** — riempie le aree chiuse sui livelli smart-raster.
- **Sequenze immagini con `-`** — `frame-0006.jpg` (trattino) letto come sequenza.

## 19. Leggere le chiavi — il diamante

Un personaggio riggato ha **due cose indipendenti** su cui puoi mettere delle chiavi: la
**trasformazione di colonna** (posizione, rotazione, scala…) e la **posa plastic** (la
forma dello scheletro). Ztoryc le mostra entrambe in un solo diamante, sia nell'xsheet
sia sul bottone **Set Key** del viewer:

- **metà destra vuota** = la chiave è **parziale**;
- la **metà sinistra** dice *quale* sistema la tiene — **bianco** per la trasformazione,
  **oro** per la posa plastic, oppure bianco sopra e oro sotto quando una chiave parziale
  le tiene entrambe.

![Legenda del diamante chiave](images/keyframe_diamond_legend.svg)

**Set Key nel viewer** cicla verso la chiave completa e solo da lì la rimuove: clicca su
una chiave parziale per completarla, su una chiave di sola posa per aggiungere la
trasformazione, su una chiave completa per cancellarla. Cosa conti come "completa" lo
decide la **portata della chiave globale** (il menu *Key:* nelle opzioni dell'Animate e
del Plastic tool — Stage / Plastic / All): con portata *Stage* la posa viene ignorata del
tutto e una colonna riggata si comporta come una normale. Passa il mouse sul bottone per
leggere lo stato corrente e cosa farà il click.

---

## 20. Export

- **Export spreadsheet** — worksheet di produzione `.xlsx` (per-scena o intero progetto).
- **Export storyboard PDF** — board stampabile (logo studio personalizzato opzionale).
- **Export animatic** — renderizza l'animatic a tempo in video.
- **Export shots come layout `.tnz`** — ogni shot in un `.tnz` standalone, eredita i
  metadati di progetto ed è nominato col pattern a token
  (`{PROD}_{CODE}_{EP}_{SEQ}_{SHOT}_{TASK}_V{VER:02}`).
