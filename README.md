# Ztoryc

**The complete solution for professional storyboarding.** From your first thumbnail to the final animatic, Ztoryc supports your creative work through every stage of pre-production — free, open source, and built by artists for artists.

Ztoryc is a fork of Tahoma2D 1.6.0 built for storyboard artists, directors and studios who need a serious, professional tool to tell better stories, before a single frame is animated.

Ztoryc works for any production — animated or live action. If you use Tahoma2D or OpenToonz for 2D animation, it integrates directly into your pipeline, but it works as a standalone storyboarding tool for any workflow.

---

## Why Ztoryc

The storyboard is where a production is won or lost. Ztoryc is built for the artists and directors who know that — and need a professional tool that respects their craft.

Professional storyboard software is either expensive, locked into proprietary ecosystems, or limited in functionality. Ztoryc aims to change that.

In most pipelines the storyboard is created in a separate application and later rebuilt in the animation software. This creates duplicated work, manual animatic timing, and loss of shot structure between departments.

Ztoryc, working together with Tahoma2D / OpenToonz, experiments with a different approach: the storyboard becomes the foundation of the scene itself — from first sketch to final layout.

---

## Output

- 📄 **PDF storyboard** — export your boards as a professional storyboard document
- 🎬 **Animatic** — export the animatic in standard video formats
- 🎞️ **Layouts** — for productions using Tahoma2D / OpenToonz, Ztoryc exports each shot as a ready-to-animate `.tnz` scene complete with storyboard panels, audio, and camera moves

---

## Current Features

⚠️ Work in progress. Many features are still experimental.

### Storyboard Panel
- Panel-based storyboard grid (multiple panels per shot)
- Auto-detection of panels from keyframes, camera moves, and drawing changes
- Configurable grid, drag & drop shot reordering
- Shot numbering modes: Auto / Keep / Renumber All
- Multi-selection, copy/clone/paste, multi-shot delete
- Thumbnail preview with automatic refresh
- Export shots as standalone `.tnz` scenes with isolated assets

### Animatic Panel
- Custom NLE-style animatic editor with integrated OpenGL viewer
- Shot blocks generated from main xsheet timing
- Audio tracks with waveform visualization
- Ripple editing and timeline zoom
- Playhead synchronized with the main timeline
- Viewer always shows the main xsheet (independent of Edit In Place)

### Workflow Modes
Ztoryc adds a Workflow menu to quickly switch between room sets:
- Storyboard
- 2D Tradigital
- Digital Cutout
- Stop-Motion

### Data & Persistence
- Central ZtoryModel shared across panels
- Project data stored in `.ztoryc` XML files saved alongside the scene
- Custom rooms: BOARD, SHOTEDITOR, ANIMATIC

---

## Planned Features

- Separate frame handle for animatic viewer (independent timelines for animatic and shot sub-scene)
- PDF storyboard export
- Animatic render export
- StoryStrip panel and Order Review panel
- Undo/Redo support
- Keyboard shortcuts
- Kitsu integration for production management

---

## Building

Ztoryc builds like Tahoma2D. See the existing build docs in `doc/`.

---

## About

Ztoryc was started by an animation director with over thirty years of experience in 2D animation, who has been working with Toonz — and later OpenToonz and Tahoma2D — since the late 1990s.

The project carries the know-how of Matitanimata, one of Italy's most respected animation studios, whose deep roots in the Toonz ecosystem and years of production experience shaped the vision behind Ztoryc.

Contributions, feedback, and ideas are welcome.

Based on Tahoma2D — BSD 2-Clause License.

---

---

# Ztoryc (Italiano)

**La soluzione completa per lo storyboard professionale.** Dal primo thumbnail all'animatic finale, Ztoryc supporta il tuo lavoro creativo in ogni fase della pre-produzione — gratuito, open source, e costruito da artisti per artisti.

Ztoryc è un fork di Tahoma2D 1.6.0 pensato per storyboard artist, registi e studi che hanno bisogno di uno strumento serio e professionale per raccontare storie migliori, prima che venga animato un singolo fotogramma.

Ztoryc funziona per qualsiasi tipo di produzione — animata o live action. Se usi Tahoma2D o OpenToonz per l'animazione 2D, si integra direttamente nella tua pipeline, ma funziona anche come strumento autonomo per qualsiasi workflow.

---

## Perché Ztoryc

Lo storyboard è il momento in cui una produzione si vince o si perde. Ztoryc è costruito per gli artisti e i registi che lo sanno — e hanno bisogno di uno strumento professionale che rispetti il loro mestiere.

Gli strumenti professionali per storyboard sono quasi tutti a pagamento, chiusi in ecosistemi proprietari, o limitati nelle funzionalità. Ztoryc vuole cambiare questo.

In molte pipeline lo storyboard nasce in un'applicazione separata e poi viene ricostruito nel software di animazione. Questo porta a lavoro duplicato, timing ricreato a mano, e perdita della struttura degli shot tra i reparti.

Ztoryc, lavorando insieme a Tahoma2D / OpenToonz, sperimenta un approccio diverso: lo storyboard diventa la base della scena stessa — dal primo schizzo al layout finale.

---

## Output

- 📄 **PDF storyboard** — esporta le tavole come documento storyboard professionale
- 🎬 **Animatic** — esporta l'animatic nei principali formati video
- 🎞️ **Layout** — per le produzioni che usano Tahoma2D / OpenToonz, Ztoryc esporta ogni shot come scena `.tnz` pronta per l'animazione, completa di panels dello storyboard, audio e movimenti di camera

---

## Funzionalità Attuali

⚠️ Progetto in sviluppo. Molte funzioni sono ancora sperimentali.

### Storyboard Panel
- Griglia a pannelli (più pannelli per shot)
- Rilevamento automatico da keyframe, movimenti camera e cambi disegno
- Griglia configurabile, drag & drop per riordinare gli shot
- Numerazione shot: Auto / Keep / Renumber All
- Selezione multipla, copia/clona/incolla, eliminazione multipla
- Thumbnail aggiornate automaticamente
- Export shot in `.tnz` con asset isolati

### Animatic Panel
- Editor animatic in stile NLE con viewer OpenGL integrato
- Blocchi shot generati dal timing del main xsheet
- Tracce audio con visualizzazione waveform
- Ripple edit e zoom timeline
- Playhead sincronizzato con la timeline principale
- Viewer sempre sul main xsheet (indipendente da Edit In Place)

### Modalità Workflow
Ztoryc aggiunge un menu Workflow per cambiare rapidamente set di room:
- Storyboard
- 2D Tradigital
- Cutout Digitale
- Stop-Motion

### Dati & Persistenza
- ZtoryModel centrale condiviso tra i panel
- Dati progetto in file `.ztoryc` (XML) accanto alla scena
- Room dedicate: BOARD, SHOTEDITOR, ANIMATIC

---

## Funzionalità Pianificate

- Frame handle separato per il viewer animatic (timeline indipendenti per animatic e sottoscena)
- Export PDF storyboard
- Export animatic
- StoryStrip panel e Order Review panel
- Undo/Redo
- Shortcut tastiera
- Integrazione Kitsu per la gestione della produzione

---

## Build

Ztoryc si compila come Tahoma2D. Vedi la documentazione in `doc/`.

---

## About

Ztoryc è nato dal lavoro di un regista di animazione con oltre trent'anni di esperienza nel settore, che lavora con Toonz — e poi OpenToonz e Tahoma2D — dalla fine degli anni '90.

Il progetto porta con sé il know-how di Matitanimata, uno dei più importanti studi di animazione italiani, le cui radici profonde nell'ecosistema Toonz e gli anni di esperienza produttiva hanno dato forma alla visione di Ztoryc.

Contributi, feedback e idee sono benvenuti.

Basato su Tahoma2D — licenza BSD 2-Clause.
