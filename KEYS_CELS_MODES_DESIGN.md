# Keys / Cels Modes — design (stile Toon Boom Harmony)

> ✅ MERGIATO su master il 2026-06-14 (merge `ee838a7e3`). La feature è opt-in
> (toggle default OFF → comportamento identico a upstream), verificato con giro di
> regressione toggle-OFF pulito. Il lavoro residuo (gruppo ritempi, rifiniture undo
> lato toggle-ON) prosegue direttamente su master in modo incrementale e gated.
>
> ⚠️ TODO undo (toggle ON) — DA DETTAGLIARE: l'utente ha segnalato "qualcosa da
> sistemare sugli undo" al momento del merge. Riprodurre e annotare la sequenza esatta.

## STATO / DECISIONI FINALI (2026-06-14)

Il design a **3 modalità** (Drawings/Keys/Both) è stato **abbandonato**: l'utente
ha giudicato Keys-only e Drawings-only inutili/invasive.  Modello finale: **un
solo toggle on/off** *"Keyframes Follow Exposure"* — i keyframe restano incollati
alle loro celle.

Implementato (Opzione B, hook centrale, undo-safe):
- ✅ **Preference** `KeyframesFollowExposure` (default OFF) — `preferencesitemids.h`,
  `preferences.h` (getter `isKeyframesFollowExposureEnabled()`), `preferences.cpp`.
  Persistente e leggibile dal core `TXsheet` (toonzlib).
- ✅ **Hook centrale** in `TXsheet::insertCells`/`removeCells` (txsheet.cpp): quando
  ON, slitta i keyframe della colonna insieme alle celle (usa
  `TStageObject::moveKeyframes`).  Undo-safe: le operazioni (Level Extender,
  Insert…) rieseguono do/undo passando da queste primitive → shift simmetrico.
  Verificato: niente doppio-shift perché il drag-move usa `CellsMover`, non
  insertCells.
- ✅ **Toggle** checkable `MI_ToggleKeyframesFollowExposure` (xsheetcmd.cpp +
  mainwindow.cpp `createToggle`, categoria Misc).
- ✅ **Selezione+drag**: gating in `xshcellviewer.cpp` (~4245) e
  `xsheetdragtool.cpp` (XsheetSelectionDragTool::onClick) ora legge la Preference
  → selezione a rettangolo include i keyframe, `CellKeyframeMoverTool` li muove.
- ✅ Singleton `cellkeyframemode.h` **ritirato**.
- 🧪 Testato OK dall'utente: selezione, drag in blocco, **frame-extend che slitta
  i keyframe**, undo/redo.

Repertorio comandi sulla selezione combinata (commit successivo):
- ✅ `TCellKeyframeSelection::enableCommands` ora delega l'INTERO repertorio celle
  a `m_cellSelection->enableCommands()` (prima era uno stub: solo copy/paste/cut/
  delete → reverse/swing/step/insert/… restavano GRIGI con i keyframe selezionati).
  I 4 comandi combinati (Copy/Paste/Cut/Clear) restano override.

## ✅ RISOLTO (2026-06-14) — undo della cancellazione keyframe (Level Extender shrink)

Accorciare il timing di un blocco (rimuove frame → `TXsheet::removeCells`) **cancellava**
i keyframe nello span e l'**undo NON li ripristinava** (chiavi perse).

Causa: lo SHIFT dei keyframe in insert/removeCells è undo-safe (simmetrico). Ma la
**cancellazione** delle chiavi nello span rimosso non lo era: l'undo chiama `insertCells`
(ri-slitta giù) che non può ricreare chiavi cancellate.

**Fix implementato** (a livello di COMANDO, non di primitiva) in `LevelExtenderUndo`
(`xsheetdragtool.cpp`):
- ✅ **Snapshot all'onClick** — `setCells()` salva i keyframe dell'intero blocco originale
  per colonna (`m_savedKeys`, mappa riga→`TStageObject::Keyframe`), una sola volta, gated
  sul flag `m_followExposure` letto al momento dell'operazione.
- ✅ **Restore nell'undo** — l'helper `insertCells()` (path undo dello shrink) riapplica via
  `setKeyframeWithoutUndo` solo le chiavi nello span del tail `[r0,r1]` rimosso. Lo shift
  centrale in `TXsheet::insertCells` resta (ri-slitta le chiavi sotto il blocco).
- 🧪 Testato OK dall'utente: shrink oltre una chiave → undo la ripristina, redo la ri-rimuove.

Contenimento verificato analiticamente: lossy solo il path `m_insert=true` non-invert
(undo via `insertCells`); non-insert/invert usano `clearCells`/`setCells` (non cancellano
chiavi); per l'extend il restore è no-op (chiavi salvate sopra il tail inserito).

⚠️ **Da generalizzare**: lo stesso pattern distruttivo esiste in altri comandi che
rimuovono frame con chiavi (es. Reframe, Step/Each reduce, time-stretch). Per ora coperto
solo Level Extender (caso segnalato). Se emergono altri casi, estrarre un helper
riusabile snapshot/restore keyframe-in-span e adottarlo nei rispettivi Undo.

## Menu Cels → keys-follow: classificazione (decisa 2026-06-14)

Principio unico: **la chiave segue la sua cella**.  Operazioni che rimappano il
tempo (permuta/sposta/ritempo/duplica righe) → estendibili.  Operazioni che cambiano
il *contenuto* (numero disegno) o mettono un *marker di playback* → no.
NON opzionali per-comando: il toggle stesso è l'opt-in globale (OFF = niente tocca le chiavi).

| Voce | Keys-follow | Stato |
|------|-------------|-------|
| Reverse | ✅ specchia `r→r0+r1-r` (involuzione) | ✅ FATTO 2026-06-14 |
| Roll Up / Roll Down | ✅ rotazione ciclica, bordo incluso | ✅ FATTO 2026-06-14 |
| Swing | ✅ duplica chiavi nel tail rovesciato (`s→2*r1-s`) | ✅ FATTO 2026-06-14 |
| Repeat… | ✅ duplica pattern chiavi su ogni copia | ✅ FATTO 2026-06-14 |
| Time Stretch | ✅ ritempo proporzionale | ☐ TODO (gruppo ritempi) |
| Step (2/3/4) | ✅ chiave sul primo frame del run | ☐ TODO |
| Each (2/3/4) | ⚠️ decima: snap al frame superstite | ☐ TODO |
| Reframe (1/2/3/4) | ⚠️ espandi+decima | ☐ TODO (semantica da decidere) |
| Random | ❌ cambia numeri disegno, non il tempo | — |
| Autoexpose | ❌ contenuto | — |
| Auto Input Cell Number | ❌ contenuto | — |
| Loop Frames / Remove Loop | ❌ marker playback (righe virtuali) | feature a parte* |

\* **Loop "key-aware" = feature separata**: Loop non duplica celle reali
(`column->addLoop`, righe virtuali via `getLoopedFrame`).  Far seguire le chiavi
significherebbe *ciclare la valutazione del transform* per-range — tocca lo
`TStageObject` (esiste già `m_cycleEnabled` globale, non per-range), non il
rimappamento di righe.  Non incastrarla in questo toggle.

**Gruppo permutazioni + duplicazioni FATTO** (undo simmetrico gratis via primitive,
zero perdita) — tutto in `txsheet.cpp`, gated sulla preference, testato OK dall'utente:
- Reverse — `reverseCells`: mirror involutivo (`ReverseUndo` undo==redo).
- Roll Up/Down — `rollupCells`/`rolldownCells`: la chiave di bordo veniva cancellata
  da `removeCells`, ora salvata e riposizionata; undo via primitiva inversa.
- Swing — `swingCells`: duplica `[r0,r1-1]` specchiato nel tail; undo = `removeCells`
  del tail (cancella le chiavi duplicate, simmetrico).
- Repeat — `duplicateCells`: duplica il chunk su ogni copia; undo idem.
- Fix collaterale: `DuplicatePopup` (Repeat…) faceva `dynamic_cast<TCellSelection>`
  → grigio con la selezione combinata.  Ora usa `getCurrentCellSelection()` che
  estrae la cell-selection interna da `TCellKeyframeSelection`.  ⚠️ Stesso pattern
  da verificare su altri popup del menu Cels (Time Stretch…).

**Gruppo ritempi (TODO)**: Time Stretch/Step/Each/Reframe — richiedono snapshot/
restore stile Level Extender per la decimazione (chiavi su celle scartate → snap
al superstite, no perdita silenziosa).  Estrarre l'helper riusabile.

Altri da fare:
- Checkbox nel dialog Preferences + voce di menu (menubar.xml) per la visibilità.
- Verificare la riga "global keyframe" (camera/tutte le colonne).

Il resto di questo documento è la ricognizione/design originale (storico).

---

## Obiettivo

Tre modalità globali che cambiano come selezione e spostamento si applicano a
celle (exposure) e keyframe (transform), come in Harmony:

- **Drawings** — solo exposure
- **Keys** — solo keyframe
- **Both** — entrambi insieme

Decisioni utente (2026-06-13):
- Modalità **globale** (come lo strumento corrente), non per-pannello.
- In modalità **Keys** la **griglia celle diventa key-aware** (stile Harmony): un
  rettangolo sulla griglia seleziona/sposta i keyframe della colonna, ignorando
  l'exposure.
- Priorità reali: **selezione** prevedibile + **spostamento nel tempo**.

## Stato attuale del codice (ricognizione)

Mappatura dati ~1:1 con Harmony e **infrastruttura di spostamento già presente**:

| Harmony   | Tahoma2D                         | Selezione                | Mover (drag)            |
|-----------|----------------------------------|--------------------------|-------------------------|
| Drawings  | celle `TXshCell` (exposure)      | `TCellSelection`         | `LevelMoverTool`        |
| Keys      | keyframe su `TStageObject`       | `TKeyframeSelection`     | `KeyframeMoverTool`     |
| Both      | —                                | `TCellKeyframeSelection` | `CellKeyframeMoverTool` |

Fatti chiave:
- `CellKeyframeMoverTool` (`xsheetdragtool.cpp:1573`) è **pura composizione**:
  inoltra onClick/onDrag/onRelease sia a `LevelMoverTool` sia a
  `KeyframeMoverTool`, in un blocco undo. → "Both" time-move funziona già.
- `KeyframeMoverTool(viewer, /*embedded=*/true)` è già la variante **guidata
  dalla griglia** (usata da CellKeyframeMover). → "Keys" time-move sulla griglia
  è già fattibile riusandola.
- La scelta attuale tra i tre è **spaziale/euristica**: flag `isKeySelection`
  (`xshcellviewer.cpp:4245`, `= hasDragBar ? Ctrl : Alt`) decide se la selezione
  include i key, e il mover è scelto via `dynamic_cast` della selezione corrente
  (`xshcellviewer.cpp:4313-4317`).
- La selezione combinata `TCellKeyframeSelection` è uno **stub a metà**: abilita
  SOLO Copy/Paste/Cut/Clear (`cellkeyframeselection.cpp::enableCommands`); tutto
  il resto (reverse, swing, step/each, duplicate, time stretch, …) è
  **commentato** in `cellkeyframeselection.h:50-81`.
- `TCellSelection` ha ~50 comandi (lista in `cellselection.cpp::enableCommands`),
  inclusi già `MI_SetKeyframes`, `MI_ShiftKeyframesUp/Down`.

Xsheet e Timeline sono lo **stesso codice** (`xsheetviewer` con orientamento) →
una sola implementazione copre entrambi.

## Punti di iniezione del routing (da ricablare per-modalità)

- `xshcellviewer.cpp:4245` — `isKeySelection` (sostituire/affiancare con la modalità)
- `xshcellviewer.cpp:4300-4317` — costruzione selezione + scelta mover (drag da cella)
- `xshcellviewer.cpp:4104, 4669` — path selezione/colonna keyframe
- path della selezione rubber-band (rettangolo) — verificare e instradare uguale

## Piano a fasi

**Fase 0 — fondazione + spike (basso rischio):**
- ✅ `cellkeyframemode.h` — singleton globale `CellKeyframeModeManager`
  (Drawings/Keys/Both), default Drawings = comportamento corrente. Header-only,
  niente moc. *(FATTO — additivo, nessuno lo legge ancora.)*
- [ ] Comando `MI_` per ciclare la modalità + shortcut (testabilità).
- [ ] Toolbar xsheet/timeline: 3 toggle (promuovere il manager a QObject con
  segnale `modeChanged` quando serve aggiornare la UI).
- [ ] Cablare il routing a `xshcellviewer.cpp:4300` con la modalità che
  **prevale** su isKeySelection. Default Drawings → identico ad oggi (no
  regressione). Both/Keys → montano il mover corrispondente (riuso puro).

**Fase 1 — Keys-aware grid (parte Harmony, più invasiva):**
- [ ] Rettangolo sulla griglia → costruire `TKeyframeSelection` per le colonne in
  range (per ogni colonna, i key nei frame [r0,r1]).
- [ ] `KeyframeMoverTool(viewer, true)` guidato dal drag sulla griglia in modalità
  Keys (riuso della variante embedded).
- [ ] Rendering: evidenziare i key selezionati sulla griglia (`CellArea::drawCells`).

**Fase 2 — repertorio combinato (completare lo stub):**
- [ ] Implementare i metodi commentati in `cellkeyframeselection.h` instradando le
  operazioni di `TCellSelection` su celle **e** key coerentemente con la modalità
  (priorità: time-move via comando, reverse, step/each, insert/remove frames).
- [ ] Casi limite: riga **global keyframe** (camera/tutte-le-colonne), ease/handle
  preservati nello spostamento, multi-colonna, copertura undo.

## Incognite — stato

1. ✅ `KeyframeMoverTool` guidabile da drag sulla griglia → SÌ (variante embedded già usata).
2. ☐ Ease/handle e key parziali preservati nel move → verificare in fase 1.
3. ☐ Riga global keyframe nelle tre modalità → definire in fase 2.
4. ✅ Selezione multi-colonna in Keys → coerente (key sono per-colonna).

## Rischio & disciplina

Hot-path del pannello più usato → incrementi piccoli, build+test ad ogni passo,
default Drawings sempre = comportamento corrente. Candidabile **upstream**
(feature mancante a OpenToonz/Tahoma2D da sempre).
