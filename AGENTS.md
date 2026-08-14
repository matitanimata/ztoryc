# Ztoryc — AI Agent Rules

> This file is read automatically by Claude Code, Codex, Cursor, Copilot, and Windsurf.
> The canonical copy lives in `~/ZtorYc/AGENTS.md`.
> In the code repo (`/Volumes/ZioSam/tahoma2d-workspace/tahoma2d/`) CLAUDE.md is a
> symlink to this file, or a copy of it.

-----

## Project Overview

Ztoryc is a fork of Tahoma2D 1.6.0 that adds an integrated storyboard and animatic
pipeline for 2D animation pre-production. It is the first open-source storyboard tool
designed to work natively inside an animation application.

- **Repository:** https://github.com/matitanimata/ztoryc
- **Base:** Tahoma2D 1.6.0 (BSD 2-Clause License)
- **Code workspace (Claude Code):** `/Volumes/ZioSam/tahoma2d-workspace/tahoma2d`
- **Code backup (Cowork):** `~/ZtorYc/tahoma2d-workspace_local/tahoma2d`
- **Planning docs:** `~/ZtorYc/` (AGENTS.md, CHANGELOG.md, ANIMATIC_TASKS.md, DESIGN.md)
- **Language:** C++17, Qt5
- **Build system:** CMake + Ninja

-----

## Folder Structure

```
~/ZtorYc/
├── AGENTS.md                    ← questo file (canonical)
├── CHANGELOG.md                 ← symlink → Google Drive/Ztoryc/CHANGELOG.md
├── ANIMATIC_TASKS.md            ← symlink → Google Drive/Ztoryc/ANIMATIC_TASKS.md
├── DESIGN.md                    ← specifica funzionale
├── README.md                    ← readme pubblico
├── tahoma2d-workspace_local/    ← backup codice (rsync da ZioSam dopo ogni commit)
│   └── tahoma2d/
└── tahoma2d-workspace_bak/      ← snapshot storico (non modificare)
    └── tahoma2d/
```

> **CHANGELOG.md e ANIMATIC_TASKS.md sono symlink a Google Drive** (`Il mio Drive/Ztoryc/`).
> Qualsiasi scrittura su questi file aggiorna automaticamente Drive e quindi
> è visibile a Claudio Paddei (Claude su iPad). All'avvio di "nuova sessione"
> leggere sempre da `~/ZtorYc/` — se Claudio ha fatto modifiche su iPad,
> Drive le avrà sincronizzate e saranno già presenti via symlink.

-----

## Build & Run

> ⚠️ REGOLA OBBLIGATORIA: usare SEMPRE `./build_and_deploy.sh` per compilare.
> MAI eseguire ninja direttamente e poi aprire l'app — i binari helper
> `lzocompress`/`lzodecompress` non vengono copiati nel bundle e il salvataggio
> TLV crasha silenziosamente senza messaggi di errore.

```bash
# Build + deploy (SEMPRE usare questo)
cd /Volumes/ZioSam/tahoma2d-workspace/tahoma2d
./build_and_deploy.sh [file.cpp opzionale da toccare]

# Forza ricompilazione file specifico
./build_and_deploy.sh toonz/sources/toonz/ztoryanimatic.cpp
```

**Solo per debug rapido (senza aprire l'app):**
```bash
touch [file] && ninja -j4 -C /Volumes/ZioSam/tahoma2d-workspace/tahoma2d/toonz/build 2>&1 | grep "error:" | head -10
# Poi OBBLIGATORIAMENTE: ./build_and_deploy.sh prima di aprire l'app
```

-----

## Main Ztoryc Files

```
toonz/sources/toonz/storyboardpanel.h/.cpp   — Board room, shot grid UI
toonz/sources/toonz/ztorymodel.h/.cpp        — Singleton data model
toonz/sources/toonz/ztoryanimatic.h/.cpp     — Animatic panel + viewer
toonz/sources/toonz/ztorybackpanel.h/.cpp    — Back to storyboard button
```

-----

## Architecture

### Core Concept

- Main xsheet = one column per shot
- Each shot = a sub-scene (subxsheet)
- Metadata saved in `.ztoryc` XML file alongside `.tnz`

### Key Classes

- `ZtoryModel` — singleton, owns `std::vector<ShotData>`
- `StoryboardPanel` — BOARD room, shot grid
- `ZtoryAnimaticPanel` — ANIMATIC room, NLE-style timeline
- `ZtoryAnimaticViewer` — inherits `BaseViewerPanel`, `m_alwaysMainXsheet=true`
- `ZtoryAnimaticTrack` — shot blocks, zoom, ripple edit
- `ZtoryAnimaticRuler` — time ruler

### Important Rules

- Shot duration = cells in main xsheet from `getRange()` (including empty/red cells)
- In/Out markers = play range inside sub-scene, NOT animatic duration
- Audio = main xsheet only, never in sub-scenes
- Camera = edited inside sub-scene only, not from animatic
- Thumbnail refresh = on `frameSwitched` with 1000ms debounce, NOT on `xsheetChanged`
- Copy = shared instance | Clone = fully independent sub-scene

### Board ↔ Animatic sync — REGOLA CRITICA

**Non emettere mai `shotAdded` o `shotRemovedAt` dopo `resequenceXsheet()` nelle
funzioni dell'Animatic.** Causa sempre un double-update nel Board:

```
resequenceXsheet()
  → emit modelReset()
    → StoryboardPanel::onModelResequenced()   ← Board già sincronizzato qui
      → refreshFromScene()  (quando xsheet count ≠ m_shots.size())

[poi]
emit shotAdded(col)
  → onShotInserted()  ← INSERISCE UN ALTRO SHOT → Board ha 1 di troppo ✗

emit shotRemovedAt(col)
  → onShotRemovedAt()  ← RIMUOVE UN ALTRO SHOT → Board ha 1 di meno ✗
```

**Il Board si sincronizza esclusivamente via `onModelResequenced()`** che usa il
conteggio reale delle colonne child-level nell'xsheet come ground truth
(NON `ZtoryModel::m_shots.size()` che può essere stale).

Funzioni Animatic già corrette (non toccare):
- `onRazorRequested()` — nessun emit post-resequence
- `onAddShot()` — nessun emit post-resequence
- `onMergeWithNext()` — nessun emit post-resequence
- `onMergeShots()` — nessun emit post-resequence

### Shared clipboard / selection — REGOLA

- `ZtoryModel::m_sharedClip` — sorgente unica per clipboard Board↔Animatic
- `ZtoryModel::m_sharedSelection` — xsheet columns selezionate, last-panel-wins
- Lo shared clip ha **sempre priorità** su `m_clipboard` locale del Board
- Board scrive shared clip in `onCopyShot/onCutShot/onCloneShot`
- Board scrive shared selection in `onPanelClicked`
- Animatic scrive shared selection su `selectionChanged` signal del track

-----

## Coding Conventions

### General

- Follow existing Tahoma2D code style
- C++17, Qt5 signals/slots
- Use descriptive names — no obscure abbreviations
- Keep functions focused and under ~50 lines; split if longer
- Add a comment explaining *why*, not *what*, for non-obvious logic

### File Editing on macOS (zsh)

**CRITICAL:** Always use Python scripts in `/tmp/` for file modifications.
Never use heredoc with special characters in zsh — it causes encoding issues.

```bash
# CORRECT way to edit files
cat > /tmp/fix_something.py << 'PYEOF'
fname = '/Volumes/ZioSam/.../file.cpp'
content = open(fname).read()
old = """exact text to replace"""
new = """replacement text"""
if old in content:
    open(fname, 'w').write(content.replace(old, new))
    print('Done')
else:
    print('ERROR - text not found')
PYEOF
python3 /tmp/fix_something.py
```

### Before Modifying Any File

Always verify the exact text exists first:

```bash
grep -n "text to find" path/to/file.cpp
```

### Qt Signals/Slots

Use new-style connect syntax where possible:

```cpp
connect(source, &SourceClass::signal, this, &ThisClass::slot);
```

Use old-style SIGNAL/SLOT macros only when connecting to existing Tahoma2D code
that uses them.

### Headers

- Include guards: `#pragma once`
- Group includes: Ztoryc → Tahoma2D → Qt → std

-----

## Tahoma2D Integration Rules

### Do NOT modify these classes without strong reason:

- `TXshSoundColumn` — audio data (read only from Ztoryc)
- `TFrameHandle` — global frame handle (create separate instance for animatic)
- `BaseViewerPanel` — viewer base class
- `TApp` — application singleton

### Reuse existing Tahoma2D functionality:

- Audio waveform: `soundLevel->getValueAtPixel(orientation, pixel, minmax)`
- Delete column: `ColumnCmd::deleteColumn(col)`
- Clone sub-scene: `cloneChildToPosition()`
- Save sub-scene: `IoCmd::saveScene(SAVE_SUBXSHEET)` — do NOT use `SILENTLY_OVERWRITE`
- Open/close sub-scene: `ztoryOpenSubXsheet()` / `ztoryCloseSubXsheet(int)`
- Resequence: `resequenceXsheet()` — call after any duration/order change

### Audio columns

```cpp
// Iterate all sound columns in main xsheet
TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshSoundColumn *sc = xsh->getColumn(col)->getSoundColumn();
    if (!sc) continue;
    // use sc
}
```

-----

## Before Submitting a PR to Tahoma2D

1. Run clang-format:

```bash
cd toonz/sources && ./beautification.sh
```

2. Verify no regressions in the modified area
3. PR candidates — la lista vive in **`~/ZtorYc/UPSTREAM_PR_CANDIDATES.md`**
   (symlink a Drive, copia anche in repo). Consultarla solo quando si prepara
   davvero una PR: non serve al lavoro quotidiano.

   > ⚠️ **Quando emerge un nuovo candidato PR, aggiungerlo LI', non qui, SUBITO.**
   > `UPSTREAM_PR_CANDIDATES.md` e' la **fonte di verita'** e va tenuto aggiornato
   > in tempo reale, non a fine sessione.
   > Un fix e' candidato upstream se tocca file core condivisi con
   > Tahoma2D/OpenToonz (cioe' fuori dai file Ztoryc: storyboardpanel, ztorymodel,
   > ztoryanimatic, ztorybackpanel). Scrivere: sintomo, file e riga, causa root,
   > fix applicato, commit, e se e' stato **verificato su Tahoma2D stock** o solo
   > diagnosticato. Segnalarlo anche nel CHANGELOG della sessione.
   >
   > **Non confondere i due file:**
   > - `UPSTREAM_PR_CANDIDATES.md` — fonte di verita', tecnica, italiano, real-time
   > - `PR_CANDIDATES_SHARE_EN.md` — versione **derivata** discorsiva in inglese per
   >   la condivisione esterna (issue, forum, upstream). Si rigenera DA quella
   >   tecnica quando serve condividere; non modificarla come se fosse l'originale.

-----

## Rooms & Workflow

| Room       | Purpose               | Key panels                               |
|------------|-----------------------|------------------------------------------|
| BOARD      | Storyboard drawing    | StoryboardPanel (shot grid)              |
| SHOTEDITOR | Pose/layout/animation | StoryStrip + ComboViewer + dual timeline |
| ANIMATIC   | Timing + audio        | ZtoryAnimaticPanel (viewer + track)      |

-----

## Known Bugs (do not regress)

- Panel not removed when a drawing is deleted — `detectAndUpdatePanels` does not
  handle panel removal.
- Panels missing on scene open — `refreshFromScene` does not load all panels correctly.

-----

## Session Workflow (Claude Code)

### Trigger: "nuova sessione"

When the user says **"nuova sessione"** (with or without additional text), automatically:
1. Read `~/ZtorYc/AGENTS.md` (this file) for rules and architecture
2. Read `~/ZtorYc/CHANGELOG.md` for context — **ONLY the first 60 lines** (recent sessions)
3. Read `~/ZtorYc/ANIMATIC_TASKS.md` a partire da `## Priority Order` — **e la
   prima cosa da leggere e' il blocco `🛑 SOSPESI PER DECISIONE DI FRANCO`**, che
   sta subito sotto quel titolo. Le voci elencate li' sono ancora scritte come
   aperte piu' in basso nel file, ma Franco ha deciso di lasciarle stare:
   riproporgliele gli fa perdere tempo.
   (L'istruzione precedente diceva «le ultime ~40 righe»: era sbagliata, il
   Priority Order NON e' in fondo al file — seguono centinaia di righe. Una
   sessione che leggeva la coda si perdeva tutto.)
   I dettagli di un task si leggono solo quando lo si sta per implementare.
4. Report briefly: last session summary + what you'll work on today (starting from
   the highest-priority pending task in ANIMATIC_TASKS.md)

> ⚠️ **Quando Franco decide di NON fare una cosa, si scrive.** Vale quanto
> scrivere cosa si e' fatto. Le decisioni di sospendere vivono nella
> conversazione e muoiono con lei: se non finiscono nel blocco `🛑 SOSPESI` la
> sessione dopo le ripropone, e la colpa e' della lista, non sua. Successo il
> 2026-08-14 con i crash e le scene vecchie.

This keeps startup token cost low. Do NOT read full ANIMATIC_TASKS.md upfront.

> **IMPORTANTE:** il trigger "nuova sessione" funziona SOLO se Claude Code ha già letto
> questo file all'avvio (cioè se è aperto nella directory del progetto con CLAUDE.md presente).
> Se il contesto non viene trovato, usare questo messaggio esplicito come primo messaggio:
>
> ```
> nuova sessione — leggi ~/ZtorYc/AGENTS.md, le prime 60 righe di ~/ZtorYc/CHANGELOG.md,
> e la sezione "Priority Order" di ~/ZtorYc/ANIMATIC_TASKS.md. Poi fammi un recap.
> ```

### Chi decide quando fermarsi — REGOLA (Franco, 2026-07-27)

**Vai avanti finché non ti ferma Franco.** Non proporre di chiudere, non
suggerire di rimandare a domani, non fermarti «perché è tardi» o «perché la
cosa è delicata». Le sue parole: *«sono io quello che si stanca essendo umano,
per cui vai tranquillo finché non ti dico io basta»*.

Questo **non** significa lavorare alla cieca:
- Se una modifica è rischiosa, **dillo e proponi come ridurre il rischio** —
  ma poi procedi, non usarlo come motivo per fermarti.
- Se servono decisioni che solo lui può prendere (semantica, priorità, quale
  comportamento è quello giusto), **chiedi** — quello resta giusto.
- Se hai un dubbio sui **margini di contesto**, **chiediglielo**: lui guarda il
  pannello e ti dice i numeri veri. Non dedurli e non annunciare percentuali
  inventate (già successo: annunciato 60-70% mentre il pannello segnava 31%).

La finestra è da **1M token**: le vecchie soglie tarate su 200k non valgono più.
Nessun allarme prima di **700-800k**, e comunque la decisione di chiudere è sua.

Vale anche la sua indicazione complementare: **se controlli una cosa e non trovi
errori evidenti, non intervenire** — riferire e fermare la modifica, non «già che
ci sono» sistemare.

---

### Trigger: "sessione chiusa"

When the user says **"sessione chiusa"**, automatically:

1. **Update `~/ZtorYc/CHANGELOG.md`** — prepend a new entry:
   ```
   ## [YYYY-MM-DD] — title
   ### Fixed / Added / Modified / Upstream candidates / Notes
   ```

   > ⚠️ **MAI usare `mv` (o `>` diretto) per riscrivere CHANGELOG.md,
   > ANIMATIC_TASKS.md o UPSTREAM_PR_CANDIDATES.md: sono SYMLINK a Drive.**
   > `mv file symlink` sostituisce il symlink con un file normale, e da quel
   > momento le scritture restano locali — Drive (e quindi Claudio su iPad) non
   > vede piu' nulla, in silenzio. Successo davvero il 2026-07-20.
   > Per prependere in sicurezza scrivere ATTRAVERSO il symlink:
   > ```bash
   > cat nuova_entry.md ~/ZtorYc/CHANGELOG.md > /tmp/cl.md
   > cat /tmp/cl.md > ~/ZtorYc/CHANGELOG.md   # `cat >` scrive nel target, `mv` no
   > ```
   > Verificare dopo: `ls -la ~/ZtorYc/CHANGELOG.md` deve mostrare `->` a Drive.

2. **Commit and push:**
   ```bash
   cd /Volumes/ZioSam/tahoma2d-workspace/tahoma2d
   git add -A
   git commit -m "descrizione sintetica"
   git push origin master   # SEMPRE esplicito: il branch traccia upstream
                            # (Tahoma2D, push DISABLED). `git push` semplice
                            # fallirebbe silenziosamente. Origin = fork Ztoryc.
   ```

   > ⚠️ **MAI usare `git push` senza argomenti.** Il branch `master` traccia
   > `upstream/master` (tahoma2d/tahoma2d) per poter confrontare le differenze,
   > ma `upstream` ha push URL = `DISABLED`. Usare sempre `git push origin master`
   > per pushare sul fork Ztoryc (`matitanimata/ztoryc`). Verificare l'esito:
   > deve stampare `... master -> master`, non un errore `DISABLED`.

3. **Sync code to local backup:**
   ```bash
   rsync -av --delete \
     /Volumes/ZioSam/tahoma2d-workspace/tahoma2d/ \
     ~/ZtorYc/tahoma2d-workspace_local/tahoma2d/
   ```

4. **Copy planning docs back to repo** (keep them in git history):
   ```bash
   cp ~/ZtorYc/CHANGELOG.md /Volumes/ZioSam/tahoma2d-workspace/tahoma2d/CHANGELOG.md
   cp ~/ZtorYc/ANIMATIC_TASKS.md /Volumes/ZioSam/tahoma2d-workspace/tahoma2d/ANIMATIC_TASKS.md
   cp ~/ZtorYc/AGENTS.md /Volumes/ZioSam/tahoma2d-workspace/tahoma2d/AGENTS.md
   ```
   > CHANGELOG.md e ANIMATIC_TASKS.md sono symlink a Google Drive — il `cp` qui
   > legge da Drive e scrive nel repo. Drive è già aggiornato automaticamente
   > da ogni scrittura via `~/ZtorYc/`. Non serve un passo separato per Drive.

5. Confirm to the user: commit hash + files synced.

> **Why two locations:** Cowork (Claude desktop app) reads from `~/ZtorYc/` because
> `/Volumes/ZioSam/` is not accessible from the Cowork sandbox.
> `tahoma2d-workspace_local/` is the live mirror; `tahoma2d-workspace_bak/` is a
> historical snapshot — do not overwrite it.

-----

## Release Checklist

Eseguire **prima di ogni release** (trigger: utente dice "prepara release" o "chiudi sessione con release"):

### 1. Diff dal tag precedente

```bash
# Trova l'ultimo tag di release
git tag --sort=-version:refname | head -5

# Tutti i commit dall'ultimo release
git log --oneline <last-tag>..HEAD

# Statistiche file modificati
git diff <last-tag>..HEAD --stat
```

**Regola obbligatoria:** prima di scrivere le release note, classificare ogni commit:
- **User-reported** — bug segnalato dall'utente (include nella release note pubblica)
- **Dev-only** — fix interno a feature non ancora rilasciata (menzione breve o ometti)
- **Tahoma2D PR candidate** — fix in file core condivisi (segnala nella sezione Upstream)

### 2. Bump versione

```bash
# Unico file da modificare:
toonz/cmake/ZtorycVersion.cmake  # incrementa ZTORYC_VERSION_PATCH (o MINOR)
```

### 3. Ringraziamento agli sponsor — OBBLIGATORIO, si controlla ogni volta

> Saltato nella **0.11.0** (primo rilascio dopo l'arrivo del primo sponsor):
> nessun ringraziamento, né nell'app né nelle note. Era saltato perché questa
> voce non esisteva in checklist. Ora esiste: non si salta più.

Prima di scrivere le note, aprire la
[dashboard sponsor](https://github.com/sponsors/matitanimata/dashboard) e
confrontarla con `SUPPORTERS.md`. Il token `gh` di questa macchina **non** ha lo
scope `read:user`, quindi da CLI gli sponsor non si leggono: va guardata a mano.

Per ogni sponsor **nuovo** dall'ultimo rilascio:
1. È in `SUPPORTERS.md`, nel tier giusto? (solo se la sponsorizzazione è
   **pubblica** su GitHub — e solo dopo avergli chiesto il permesso)
2. **Il consenso c'è già: i tier dicono che il nome compare nei ringraziamenti.**
   Non richiederlo a parte — è nell'accordo (correzione di Franco, 2026-08-04:
   la versione precedente di questa regola diceva il contrario ed era sbagliata).
   L'unico filtro che resta è che la sponsorizzazione sia **pubblica** su GitHub.
3. → ringraziamento in-app nella schermata About, dove lo vedono anche quelli
   che su GitHub non ci vanno mai. Il blocco esiste (`aboutpopup.cpp`, modellato
   su quello di Tahoma2D: una riga di introduzione, «Special thanks to:», i nomi
   in corsivo, l'invito a sponsorizzare). **Va aggiornato a mano**: il token `gh`
   di questa macchina non ha lo scope `read:user`, quindi la lista non si può
   leggere da script e non si aggiorna da sola.
4. Riga di ringraziamento nelle note di rilascio, in **entrambe** le lingue.

Controllare anche le promesse di tier ancora scoperte (canale di discussione per
i Backer, voto sulle feature per i Pro, logo per Studio/Sponsor): sono manuali,
non c'è nessun automatismo che le onori.

### 4. Release note — SEMPRE bilingue (🇬🇧 EN + 🇮🇹 IT) + istruzioni per i tre OS

**Regola obbligatoria:** le release note GitHub devono essere **bilingue** —
sezioni Novità/Fix sia in **English** che in **Italiano** (es. blocchi
`### 🇬🇧 English` e `### 🇮🇹 Italiano`, **inglese per primo**). Anche le note
macOS, Windows e Linux sotto vanno in entrambe le lingue.

> Il workflow CI pubblica la release con **body vuoto**: le note vanno scritte/
> aggiornate a mano con `gh release edit v0.X.Y --notes-file <file>` dopo che la
> release è creata.

La sezione macOS è obbligatoria in ogni release perché l'app non è notarizzata:

```markdown
**macOS — prima apertura / first launch:**
L'app non è notarizzata. Dopo aver copiato `Ztoryc.app` in `/Applications`:
```bash
xattr -cr /Applications/Ztoryc.app
```
Poi aprire dall'app o doppio clic → Tasto destro → Apri la prima volta.

*The app is not notarized. After copying `Ztoryc.app` to `/Applications`, run the command above in Terminal, then open normally.*

**Windows — installazione / installation:**
Si raccomanda un'**installazione pulita**: disinstallare eventuali versioni precedenti prima di installare (installare sopra una vecchia versione può lasciare file/layout stale e causare instabilità).

*A **clean install** is recommended: uninstall any previous version before installing (installing over an old version may leave stale files/layout and cause instability).*

**Linux — installazione / installation:**
Su Debian/Ubuntu usare il pacchetto `.deb`; altrove il `.tar.gz` portatile.
Delle due build, prendere la **gcc**: la `clang` è la stessa applicazione compilata
con l'altro compilatore, utile solo se la gcc dà problemi.

*On Debian/Ubuntu use the `.deb` package; elsewhere the portable `.tar.gz`. Of the two builds, take the **gcc** one — `clang` is the same application built with the other compiler, useful only if the gcc build misbehaves.*
```

### 5. Trigger CI

⚠️ **Il tag NON pubblica niente.** I workflow hanno solo `workflow_dispatch`:
pushare un tag fa partire la CI ordinaria, non una release. Verificato il
2026-08-04 — questa sezione diceva il contrario ed era sbagliata.

**Tutti e tre insieme**, Linux compreso (prima era descritto come un passo a
parte, ed e' il motivo per cui la 0.11.0 e' uscita senza binari Linux):

```bash
for w in macOS_build.yml windows_build.yml linux_build.yml; do
  gh workflow run $w -f publish_release=true -f release_tag=v0.X.Y
done
```

Il tag git si puo' comunque creare, come segnalibro nella storia, ma non e' cio'
che pubblica.

**Linux va lanciato a parte** (dal 2026-07-27 il workflow ha il job
`publish-release`, ma `linux_build.yml` è `workflow_dispatch` — sul tag **non**
parte da solo):

```bash
gh workflow run linux_build.yml -f publish_release=true -f release_tag=v0.X.Y
```

Produce quattro asset: `Ztoryc-linux-{gcc,clang}.{tar.gz,deb}`. Verificare che
siano davvero comparsi nella release (`gh release view v0.X.Y`) prima di
annunciarla: la **0.11.0** è uscita senza binari Linux perché il job è arrivato
otto ore dopo la pubblicazione.

-----

## Do NOT

- Use `SILENTLY_OVERWRITE` when saving sub-scenes (bypasses asset copy)
- Modify camera from the animatic timeline
- Add audio to sub-scenes (audio lives in main xsheet only)
- Use heredoc with special characters in zsh shell scripts
- Use `widget->screen()` for DPR on macOS — use `widget->windowHandle()->screen()`
- Add global mutable state outside `ZtoryModel`
