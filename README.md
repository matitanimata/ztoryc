# Ztoryc

**Open-source storyboard and pre-production software for 2D animation.**

Ztoryc is a fork of Tahoma2D 1.6.0 that introduces a fully integrated storyboard
and animatic workflow directly inside the animation software. The goal is to
cover the early production pipeline:

Storyboard → Animatic → Layout

without forcing artists to switch between multiple tools.

Ztoryc explores a different idea:

> What if the storyboard was not a separate document, but the actual structure
> of the animation scene?

⚠️ Work in progress. Many features are still experimental.

---

## Why Ztoryc

In most animation pipelines the storyboard is created in a separate application
and later rebuilt in the animation software. This creates duplicated work,
manual animatic timing, and loss of shot structure between departments.

Ztoryc experiments with a different approach: the storyboard becomes the
foundation of the animation scene itself.

---

## Core Ideas

- Storyboard grid integrated with the animation scene
- Animatic timeline connected to the xsheet
- Shot-based workflow for animation pre-production
- Shot export to standalone scenes

---

## Current Features

**Storyboard Panel**

- Panel-based storyboard grid (multiple panels per shot)
- Auto-detection of panels from keyframes, camera moves, and drawing changes
- Configurable grid, drag & drop shot reordering
- Shot numbering modes: Auto / Keep / Renumber All
- Multi-selection, copy/clone/paste, multi-shot delete
- Thumbnail preview with automatic refresh
- Export shots as standalone `.tnz` scenes with isolated assets

**Animatic Panel**

- Custom NLE-style animatic editor with integrated OpenGL viewer
- Shot blocks generated from main xsheet timing
- Ripple editing and timeline zoom
- Playhead synchronized with the main timeline
- Viewer always shows the main xsheet (independent of Edit In Place)

**Data & Persistence**

- Central `ZtoryModel` shared across panels
- Project data stored in `.ztoryc` XML files saved alongside the scene
- Custom rooms: BOARD, SHOTEDITOR, ANIMATIC

---

## Planned Features

- PDF storyboard export
- Animatic render export
- Audio tracks in the animatic timeline (waveform + edit)
- Separate frame handle for animatic viewer
- StoryStrip panel and Order Review panel
- Undo/Redo support
- Keyboard shortcuts
- Production pipeline integration

---

## Workflow Modes

Ztoryc adds a **Workflow** menu to quickly switch between room sets without
restarting:

- Storyboard
- 2D Tradigital
- Cutout Digital
- Stop-Motion

These modes simply select a room by name (e.g. `BOARD`, `ANIMATIC`, `2D`). If a
room is missing, Ztoryc will show a warning. You can customize the room layouts
and names freely.

## Room Sets (Build)

Room set templates live in:

`stuff/profiles/layouts/rooms/`

To ship a custom set, add a new folder (e.g. `Ztoryc/`) containing `room*.ini`
files and a `layouts.txt` list. Those templates become available in builds.

---

## Building

Ztoryc builds like Tahoma2D. See the existing Tahoma2D build docs in `doc/`.

---

## About

Ztoryc started as an experiment by an animator and grew with the help of
developers. Contributions, feedback, and ideas are welcome.

Based on Tahoma2D — BSD 2-Clause License.

---

# Ztoryc (Italiano)

**Software open-source per storyboard e pre-produzione 2D.**

Ztoryc è un fork di Tahoma2D 1.6.0 che integra un workflow di storyboard e
animatic direttamente dentro il software di animazione. L’obiettivo è coprire
le fasi iniziali della produzione:

Storyboard → Animatic → Layout

senza costringere gli artisti a usare strumenti separati.

Ztoryc propone un’idea diversa:

> E se lo storyboard non fosse un documento separato, ma la struttura stessa
> della scena di animazione?

⚠️ Progetto in sviluppo. Molte funzioni sono ancora sperimentali.

---

## Perché Ztoryc

In molte pipeline lo storyboard nasce in un’app separata e poi viene ricostruito
nel software di animazione. Questo porta a lavoro duplicato, timing ricreato a
mano, e perdita della struttura degli shot.

Ztoryc sperimenta un approccio diverso: lo storyboard diventa la base della
scena di animazione.

---

## Idee Chiave

- Griglia storyboard integrata nella scena
- Timeline animatic collegata all’xsheet
- Workflow per shot
- Export shot in scene autonome

---

## Funzionalità Attuali

**Storyboard Panel**

- Griglia a pannelli (più pannelli per shot)
- Rilevamento automatico da keyframe, camera e cambi disegno
- Griglia configurabile, drag & drop per riordinare gli shot
- Numerazione shot: Auto / Keep / Renumber All
- Selezione multipla, copia/clona/incolla, eliminazione multipla
- Thumbnail aggiornate automaticamente
- Export shot in `.tnz` con asset isolati

**Animatic Panel**

- Editor animatic in stile NLE con viewer OpenGL integrato
- Blocchi shot generati dal timing del main xsheet
- Ripple edit e zoom timeline
- Playhead sincronizzato con la timeline principale
- Viewer sempre sul main xsheet (indipendente da Edit In Place)

**Dati & Persistenza**

- `ZtoryModel` centrale condiviso
- Dati progetto in file `.ztoryc` (XML) accanto alla scena
- Room dedicate: BOARD, SHOTEDITOR, ANIMATIC

---

## Funzionalità Pianificate

- Export PDF storyboard
- Export animatic
- Tracce audio nella timeline animatic (waveform + edit)
- Frame handle separato per il viewer animatic
- StoryStrip panel e Order Review panel
- Undo/Redo
- Shortcut tastiera
- Integrazione pipeline produzione

---

## Modalità Workflow

Ztoryc aggiunge un menu **Workflow** per cambiare rapidamente set di room senza
riavviare:

- Storyboard
- 2D Tradigital
- Cutout Digitale
- Stop‑Motion

Queste modalità selezionano una room per nome (es. `BOARD`, `ANIMATIC`, `2D`).
Se una room non esiste, Ztoryc mostra un avviso. Le room possono essere
personalizzate liberamente.

## Set di Room (Build)

I template dei set di room si trovano in:

`stuff/profiles/layouts/rooms/`

Per distribuire un set personalizzato, aggiungi una nuova cartella (es. `Ztoryc/`)
con i file `room*.ini` e un `layouts.txt`. I template diventano disponibili nella build.

---

## Build

Ztoryc si compila come Tahoma2D. Vedi la documentazione in `doc/`.

---

## About

Ztoryc è nato come esperimento di un animatore ed è cresciuto con il supporto
di sviluppatori. Feedback e contributi sono benvenuti.

Basato su Tahoma2D — licenza BSD 2-Clause.
