# OpenToonz → Ztoryc/Otter — candidati di porting

> Direzione **opposta** a `UPSTREAM_PR_CANDIDATES.md`: lì si va da Ztoryc verso
> Tahoma2D/OpenToonz, qui si guarda cosa OpenToonz ha e noi no.
> Metodo: `OPENTOONZ_PORT_METHODOLOGY.md` (Claudio, 2026-08-09).
>
> Stesso standard di certezza dell'altro file: ✅ confermato con misura /
> ❓ da verificare / ⚠️ incerto. **Non si scrive niente qui per intuizione.**

## Setup verificato (2026-08-13)

- Remote `opentoonz` presente nel workspace principale, fetch attivo, push `DISABLED`
- Punto di divergenza reale: `dbc8ecd89` — **29 giugno 2020**
- Da lì: **2636 commit solo su OpenToonz**, **3080 solo su Tahoma2D**
- Licenza: entrambi BSD modificata, stessa origine Toonz → **porting legalmente pulito**
  (a differenza di Krita e AnimeEffects, che sono GPL)

---

## ALTA PRIORITÀ

### ✅ Assistenti al disegno — sottosistema INTERO che non abbiamo

**Verificato con misura il 2026-08-13**, su segnalazione di Franco («mi dicono che
la parte di assistenti al disegno è più completa su OT»). La segnalazione è
esatta, e per difetto: non è più completa, è **presente contro assente**.

**Cosa abbiamo noi:** due strumenti isolati e basta —
`tnztools/perspectivetool.cpp` (griglia prospettica) e `tnztools/symmetrytool.cpp`.
Nessun framework, nessuno snap del tratto alle guide.

**Cosa ha OpenToonz:** ~**8.400 righe su 42 file**.

| pezzo | file | righe |
|---|---|---:|
| framework | `include/tools/assistant.h` + `tnztools/assistant.cpp` | 1335 |
| strumento di modifica | `tnztools/editassistantstool.cpp` | 907 |
| assistenti | line, ellipse, **perspective**, **vanishing point**, fisheye | ~2200 |
| replicatori | grid, star, mirror, affine, jitter | ~1240 |
| guideline (matematica dello snap) | guidelineline, guidelineellipse | ~590 |
| pipeline di input | `tnztools/inputmanager.cpp` + header | 947 |
| modificatori | smooth, simplify, segmentation, tangents, line, clone, jitter, assistants, test | ~1100 |

**Il costo vero, ed è qui che va guardato:** gli assistenti non si agganciano al
pennello. Si agganciano a **`TInputManager`**, una catena di modificatori che sta
**fra la tavoletta e il pennello**. In OpenToonz la pilotano tutti e tre i
pennelli — `toonzvectorbrushtool`, `toonzrasterbrushtool`, `fullcolorbrushtool` —
che sono esattamente i file dove Tahoma2D ha divergiuto di più:

| file | OpenToonz | noi |
|---|---:|---:|
| `toonzvectorbrushtool.cpp` | 2431 | 2688 |
| `toonzrasterbrushtool.cpp` | 2485 | 3699 |
| `fullcolorbrushtool.cpp` | 1236 | 1939 |

Solo sul pennello vettoriale il diff fra le due versioni è di **~2957 righe**.
Quindi **non è una copia di file**: è innestare una pipeline di input nuova
dentro tre strumenti già riscritti da un'altra parte.

**Ordine di lavoro proposto (tre tempi):**

1. **Framework + strumento di modifica + un assistente** (punto di fuga), *senza*
   snap. Si piazzano le guide e si vedono. È la parte che tocca meno il nostro
   codice, e da sola non può rompere il disegno.
2. **`inputmanager` + `modifierassistants` innestati in UN pennello solo**, il
   **vettoriale** (il meno divergente). **Qui sta tutto il rischio**: è il
   percorso più usato dell'applicazione.
3. Il resto degli assistenti (ellisse, prospettiva, fisheye) e poi i
   **replicatori**, che sono indipendenti e regalati.

**Valore:** alto per uno strumento di storyboard — le guide prospettiche sono
pane quotidiano. **Rischio:** la fase 2. Non è un lavoro da fare "di lato": o è
la priorità del momento, o si aspetta.

**Stato:** non iniziato. Deciso con Franco il 2026-08-13 di annotarlo e passare
prima al rig/IK.

---

## MEDIA PRIORITÀ

*(vuoto — da riempire quando si farà la passata sistematica sui 2636 commit,
col filtro della sezione 5 del metodo: `toonzlib/`, `tcore/`, `common/`,
`tnztools/`)*

---

## SCARTATI

*(vuoto)*
