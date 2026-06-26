# Production Tracker di progetto — Design & Roadmap

> Stato: **design** (brainstorming 2026-06-25 con Franco). Nessuna implementazione
> oltre la Fase 0 già fatta (pannello per-scena + editing status in-app).
> Doc canonico: `~/ZtorYc/DESIGN_production_tracker.md`. Pointer in `ANIMATIC_TASKS.md`.

-----

## 0-bis. Requisiti aggiuntivi (2026-06-26)

1. **Più storyboard nello stesso progetto** (es. film diviso in più `.tnz`/reel).
   Il tracker deve mostrare gli shot di TUTTI i file storyboard, **raggruppati/distinti
   per nome file** sorgente. → è il cuore di **B3** (file di progetto che aggrega gli shot
   da più `.tnz`). Ogni shot porta il riferimento al suo file storyboard; nella tab Shots
   serve una colonna o sezione "Storyboard/Reel". Serve distinguere i file *storyboard*
   dai `.tnz` di lavorazione del singolo shot.

2. **`.tnz` esportati dallo storyboard ereditano i metadati** (produzione/stagione/episodio/
   titolo) e sono **nominati secondo la naming convention** della produzione.
   Esempio: `AVIS_CS26_EP03_AMC_V01` = `<PROD>_<STAGIONE>_<EP>_<TASK>_V<NN>` (AMC = short
   name Kitsu dello Storyboard/animatic; V01 = versione). → feature **"Export shot → .tnz"**
   + **pattern naming token-based** nei settings di progetto (si lega alla vecchia idea
   "naming guidance"). I token attingono ai metadati progetto + stage + versione.

3. **L'esportato memorizza la tecnica/workflow** → al `load scene` il campo **Workflow**
   mostra quella più adatta (già: la tecnica persiste nel `.ztoryc` e la colonna Workflow
   la legge; va garantito che l'export single-shot scriva `technique`).

4. **Multi-selezione task** nella matrice: selezionare più celle/righe e cambiare
   **status o assegnatario in un colpo solo**. → feature UI **immediata** (indipendente da B3).

Mapping roadmap: (1)+(3-export) → **B3** · (2) → feature naming/export-tnz · (4) → fattibile subito.

-----

## 0-ter. B3 — File di progetto (DESIGN DETTAGLIATO, deciso 2026-06-26)

> Decisione: **Modello B** — il file di progetto è la **fonte di verità del progresso**.
> Build in sessione dedicata. Questo è lo spec da seguire.

### File
`<project_root>/production.ztrack` (XML, stile QXmlStream come `.ztoryc`). Uno per progetto
Toonz. Posizione via `TProjectManager` (radice progetto). Schema:

```xml
<ztrack version="1">
  <project production="" season="" episode="" title="" fps="25"
           namingPattern="{PROD}_{SEASON}_{EP}_{TASK}_V{VER:02}"/>
  <team><person name="..."/></team>
  <techniques><technique name="..." tasks="a|b|c"/></techniques>
  <assets>
    <asset uuid="" type="" name="" tags="">
      <atask type="" status="" assignee="..."/>
    </asset>
  </assets>
  <storyboards><storyboard file="reel1.tnz" label="Reel 1"/></storyboards>
  <shots>
    <shot uuid="" source="reel1.tnz" seq="SQ010" label="SH010" frames="47" technique="">
      <task type="" status="" assignee="..."/>
    </shot>
  </shots>
</ztrack>
```

### Chi possiede cosa
- **Struttura** (quali shot esistono, seq/label/frames, file sorgente) → autorata in ogni
  storyboard `.tnz`; **pubblicata** nel file di progetto.
- **Progresso** (task status/assignee, technique per-shot override) → del **file di progetto**.
- **Team / asset / techniques / meta progetto** → del file di progetto (project-level), spostati
  qui dal `.ztoryc` di scena.

### Attributo `role` nel `.ztoryc` di scena
- `role="storyboard"` (default/legacy): è uno storyboard; i suoi shot si pubblicano nel progetto.
- `role="shot" projectShot="<uuid>" project="<path>"`: `.tnz` di lavorazione di un singolo shot
  esportato; back-link al progetto + uuid shot. Non pubblica una lista; legge/scrive il progresso
  del suo shot nel file di progetto.

### Flusso di pubblicazione (storyboard)
All'apertura (dopo loadZtoryc) e al save: per ogni shot, **upsert** in `<shots>` per uuid
(`source=questoFile`, seq/label/frames/technique). **Non** sovrascrivere status/assignee (di
proprietà del progetto) — solo default per shot NUOVI. Shot rimossi: marcare/rimuovere (decidere:
omit vs delete).

### Migrazione
Prima apertura di uno storyboard con progresso nel suo `.ztoryc` (formato B2): importare team/asset/
techniques/meta + progresso shot nel file di progetto (una tantum). Poi i campi nel `.ztoryc` si
ignorano (evitare il dual-copy a livello progetto — cfr. bug bridge già visto).

### Tracker legge il file di progetto
Il Production Tracker legge `production.ztrack` (non il modello di scena) per shot/asset/team/ecc.
Tab Shots **raggruppata per `source`** (storyboard) — sezioni o colonna "Reel". Editing scrive
direttamente nel file di progetto → tracker completo e coerente a prescindere dalla scena aperta.
Concorrenza: single-user last-write-wins; Kitsu (D) gestirà la concorrenza vera.

### Export shot → `.tnz` (B3c)
Comando "Export shot to .tnz": salva la sotto-scena dello shot come `.tnz` standalone. Il suo
`.ztoryc` riceve `role="shot"` + `projectShot` + `project`, eredita production/season/episode/title
+ la `technique` dello shot, ed è nominato con la naming convention. Al load: il campo Workflow
mostra la tecnica memorizzata; il tracker evidenzia "tu sei lo shot X"; gli edit di status scrivono
nel progetto per uuid.

### Naming convention (B3d)
Pattern a token nel file di progetto, editabile nei settings: `{PROD} {SEASON} {EP} {SEQ} {SHOT}
{TASK} {VER}` con separatori e padding (`{VER:02}`). Es. `{PROD}_{SEASON}_{EP}_{TASK}_V{VER:02}` →
`AVIS_CS26_EP03_AMC_V01`. `{TASK}` = codice stage (AMC=animatic/storyboard, short-name Kitsu).
Usato per nominare gli export; in futuro anche per lint/validazione nomi (vecchia "naming guidance").

### Sotto-fasi B3
- **B3a** file di progetto + schema R/W; tracker legge da lì; migrazione da `.ztoryc`.
- **B3b** pubblicazione shot da storyboard (upsert per uuid + source); aggregazione multi-storyboard
  + raggruppamento per source nella tab Shots; attributo `role`.
- **B3c** export shot → `.tnz` (role=shot, back-link, eredità metadati, technique).
- **B3d** naming convention pattern + naming export.

### Rischi / da risolvere a inizio build
Discovery radice progetto (`TProjectManager`) + nome/estensione file; shot orfani se il file
storyboard sorgente è rinominato/cancellato; omit vs delete per shot rimossi; deprecare il progresso
nel `.ztoryc` di scena per non riavere il dual-copy; la **room** dedicata è concern separato (con/dopo B3).

-----

## 1. L'intuizione di fondo

Non è "un tracker": è un **piccolo database di produzione di progetto**, con **molte
proiezioni**. Lo stesso modello dati alimenta quattro uscite:

```
                    ┌─ Pannello Production Tracker (in-app)
   PRODUCTION DB  ──┼─ Spreadsheet (.xlsx / Google)
   (di progetto)    ├─ Kitsu (sync status + upload render)     [Fase D / M5]
                    └─ Export-to-AI (manifest per animatix)    [Fase E]
```

**Il breakdown (casting Shot↔Asset) è il perno condiviso**: serve a Kitsu per il
tracking E all'export-AI per sapere quali asset (model pack) caricare per ogni shot.
Una struttura, due grandi obiettivi.

-----

## 2. Flusso di lavoro reale (confermato da Franco)

```
STORYBOARD ──export──> .tnz indipendenti per shot ──> layout / animazione / vfx
   │                        (lavorati separatamente)
   │
   ├─ a monte: SPOGLIO sceneggiatura → lista ASSET (char / prop / bkg)
   │    asset = immagini importate, livelli fatti nel programma, .tnz, o solo un NOME
   │
   └─ sui PANEL: tagging "questo disegno = Hero/Backpack/Forest" (dalla lista asset)
        → costruisce il BREAKDOWN dello shot
        → dice all'AI quale MODELLO usare su quel disegno (anche se abbozzato)
```

Produzione tipo: **multi-scena / multi-episodio** (es. tvshow con episodi — cfr.
Kitsu reale `CARTOON SCHOOL 2026`). Gli shot diventano `.tnz` autonomi: serve un
**file tracker a livello di progetto** che aggrega gli shot da tutte le scene.

-----

## 3. Modello dati

Da "shot + task" a mini-Kitsu (mappa 1:1 sull'istanza Kitsu di Franco).

```
Project = { id, production, episodes[], fps, ratio, resolution, team[], techniques[] }

Person  = { id, name, role, kitsuPersonId? }          // il Team

Shot    = { uuid, episode, sequence, label, frames, technique,
            tasks{ type → { status, assignees[] } },   // assignee MULTIPLI
            notes }

Asset   = { uuid, type (Char/Prop/Bkg), name,
            refImages[] (il "modello", OPZIONALE — può essere solo un nome),
            tags[],
            tasks{ type → { status, assignees[] } } }

Panel   = { …esistente…, assetTags[uuid], dialog, action, notes, prompt? }
          // assetTags e prompt sono AGGIUNTE a PanelData

Casting / Breakdown = relazione molti-a-molti Shot↔Asset,
          DERIVATA dall'unione degli assetTags dei panel dello shot.
```

Decisioni prese (2026-06-25):
- **Assignee** come label; **assegnatari multipli** per task (lista).
- Asset **source-agnostic**: l'arte è un allegato facoltativo, non l'identità.
- **Niente auto-detect** degli asset dalle sotto-scene. Tagging **manuale sul panel**
  dalla lista cast di progetto.
- Tag a livello **Panel** (non solo Shot) → granularità per inquadratura.
- Prompt a livello **Panel**; si riusano **dialog / action / notes** + un **passaggio
  di traduzione** in prompt veri (assist LLM, override manuale).
- Concorrenza pre-Kitsu: **single-user last-write-wins**, niente lock. Con Kitsu è lui
  la fonte di verità e il file locale diventa cache.

-----

## 4. Identità stabile cross-scena (il perno tecnico)

Ogni **Shot** e ogni **Asset** ha un **UUID stabile** assegnato alla creazione e
**trasportato** dentro il `.tnz` esportato. Oggi solo `SequenceData` ha l'uuid;
va aggiunto a `ShotData` (e all'entità Asset).

Il `.tnz` esportato di uno shot porta nel suo `.ztoryc`:
`{ projectId, shotUuid, productionDbPath }` → aprendo la scena di animazione, il
pannello ritrova il DB di progetto, mostra "tu sei SH020", e status/casting tornano
indietro **per UUID**. Idem per gli asset-`.tnz`.

-----

## 5. Due sottosistemi (separare le responsabilità)

| | **ZtoryModel** (resta per-scena) | **ProjectTracker** (nuovo, per-progetto) |
|---|---|---|
| Ruolo | Authoring storyboard, board, xsheet | Progresso di produzione aggregato |
| File | `.ztoryc` accanto al `.tnz` | file unico alla **radice del progetto** (es. `production.ztrack`) |
| Dati | shot-colonne, panel, durate, tecnica, **assetTags**, **prompt** | righe Shot+Asset di tutti gli episodi, task, casting, Team, meta |
| Scrive | la scena corrente | qualsiasi scena del progetto |

Lo storyboard **pubblica** la struttura (shot, seq, durata, tecnica) nel ProjectTracker
per UUID. Il progresso vive solo nel ProjectTracker. Migrazione: import dei task già
nei `.ztoryc` di scena al primo avvio (non perdiamo dati).

-----

## 6. Export-to-AI — pipeline e manifest (bozza)

Collaborazione con il dev di **animatix** (gestore di modelli AI generativi). Nessuno
schema fisso: si **co-progetta partendo da un pacchetto-campione** da fargli testare.

Unità di export = **Panel**. Il sketch fa da guida strutturale (control), il modello
taggato dà l'identità → image-to-image guidato dal casting.

Struttura proposta (bozza da discutere):

```
export_SH020/
├── manifest.json
├── panels/
│   ├── p001/  sketch.png
│   └── p002/  sketch.png
└── modelpack/
    ├── Hero/      ref01.png ref02.png …
    ├── Backpack/  ref01.png
    └── Forest/    ref01.png
```

```jsonc
// manifest.json (BOZZA v0 — da rifinire con animatix)
{
  "project": "MyShow",
  "episode": "EP01",
  "shot": "SQ010_SH020",
  "fps": 25,
  "panels": [
    {
      "id": "p001",
      "sketch": "panels/p001/sketch.png",
      "durationFrames": 36,
      "prompt": "Hero crouches behind a tree, scared, looking left",  // tradotto da action+notes
      "rawText": { "dialog": "", "action": "Hero si nasconde", "notes": "paura" },
      "assets": [
        { "name": "Hero",   "type": "Char", "model": "modelpack/Hero" },
        { "name": "Forest", "type": "Bkg",  "model": "modelpack/Forest" }
      ]
    }
  ]
}
```

> **MVP anticipabile**: un export-campione di UNA scena reale (sketch + testi + tag)
> da mandare ad animatix, anche PRIMA del DB di progetto completo. Serve solo:
> UUID/tag sul panel (parziale) + un dumper. Sblocca subito i test della collaborazione.

-----

## 7. Roadmap a fasi

- **Fase 0 — FATTO (2026-06-25):** pannello Production Tracker per-scena + editing
  status in-app + undo + palette Kitsu. Commit `282af0c64`.
- **Fase A — UUID stabili (shot + asset)** + back-link nei `.tnz` esportati. Fondamenta.
- **Fase B — ProjectTracker DB**: file di progetto, Shot+Asset, task, Team + Assignee
  multipli, migrazione dai `.ztoryc`, pannello project-wide.
- **Fase C — Cast list + tagging sul panel** → breakdown automatico dai tag.
- **Fase D — Render hooks + Kitsu (M5)**: WIP→WFA, upload clip, sync status, casting→Kitsu.
- **Fase E — Export-to-AI**: traduzione prompt + manifest per-panel + model pack → animatix.

**Indipendenze**: C ed E sono in buona parte indipendenti da Kitsu (D). Se l'export-AI
è l'obiettivo che scotta, si può anticipare l'**MVP di E** (§6) prima del resto.

-----

## 8. Domande ancora aperte

- Formato manifest definitivo: emerge dai test con animatix (iterativo).
- Dove vive esattamente il file di progetto (radice progetto Toonz / `TProjectManager`)
  e nome estensione (`.ztrack`?).
- "Translate to prompt": LLM in-app (chiamata API) vs solo campo manuale per ora.
- Spoglio sceneggiatura → estrazione asset assistita dallo Script panel (futuro).
