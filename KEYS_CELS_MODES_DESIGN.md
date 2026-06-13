# Keys / Cels Modes — design (stile Toon Boom Harmony)

> Branch: `feature/keys-cels-modes`. NON mergiare su master finché non solido:
> si tocca il routing di selezione/drag del pannello più usato dell'app.

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

Da fare (prossimi incrementi):
- Altri comandi che dovrebbero portare i keyframe: reverse, swing, step/each,
  paste-insert, ecc. (molti già passano da insert/removeCells → potrebbero
  funzionare gratis; da verificare uno a uno).
- Eventuale checkbox nel dialog Preferences + voce di menu (menubar.xml) per
  rendere visibile lo stato del toggle.
- Verificare interazione con la riga "global keyframe" (camera/tutte le colonne).

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
