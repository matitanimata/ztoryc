# Registro dei port da OpenToonz

Ztoryc e' un fork di **Tahoma2D** (base, merge regolari) che integra **selettivamente**
funzionalita' e fix da **OpenToonz**. Da OpenToonz non si fa merge (le storie git sono
separate da anni): si fanno **port mirati**, uno alla volta, adattati alla base Tahoma2D.

Questo file registra cio' che e' **effettivamente entrato** in Ztoryc.
I candidati da valutare stanno in `OPENTOONZ_PORT_CANDIDATES.md`.

## Perche' esiste

Tahoma2D importa periodicamente modifiche da OpenToonz. Se Ztoryc ha gia' portato una
modifica e poi la porta anche Tahoma2D, al merge successivo le due versioni vanno in
conflitto. Il registro permette di riconoscere subito questi casi.

**Regola di conflitto:** se al merge da Tahoma2D arriva una modifica gia' portata da
OpenToonz, di norma si **tiene la versione di Tahoma2D** (riduce la divergenza), poi si
aggiorna la voce qui sotto ("Stato in Tahoma2D").

## Regole

- Licenza: OpenToonz e' BSD 3-Clause come Tahoma2D e Ztoryc, quindi compatibile.
- **Mantenere gli header di copyright originali** nei file portati.
- Verificare la licenza di eventuali parti di `thirdparty/` coinvolte.
- Ogni port aggiorna questo file **nello stesso commit** del codice.
- `license-guard` controlla che ogni port abbia la sua voce.

## Formato di una voce

```
## [AAAA-MM-GG] Titolo del port
- Origine: opentoonz PR #NNNN / commit <hash>
- Autore originale:
- File toccati in Ztoryc:
- Adattamenti rispetto all'originale:
- Stato in Tahoma2D: non presente / importato in Tahoma2D (versione X)
- Note:
```

---

## Port registrati

_Da compilare: registrazione retroattiva dei port gia' fatti prima del 2026-09-25._

## [2026-10-20] Assistenti al disegno (drawing assistants) e replicatori
- Origine: opentoonz master (sottosistema assistenti, `tnztools/assistant*`,
  `assistants/`, `inputmanager`, `modifiers/`, `image/tzm/`, `tmetaimage`)
- Autore originale: Ivan Mahonin (quasi tutto); icona `assistant.svg`: konero;
  singoli commit di andeon, luzpaz, Abel You
- File toccati in Ztoryc: 55 file con lo stesso percorso di OpenToonz (36
  identici byte per byte, 19 adattati); sostituiti con la versione OpenToonz
  `include/tsmartpointer.h`, `include/tvariant.h`, `include/tstringid.h`,
  `common/tgeometry/tgeometry.cpp`, `common/tvariant.cpp`; integrazioni in
  `tgeometry.h`, `tmathutil`, `tproperty.h`, `tools/tool.h`, `tiio.cpp`,
  `timage.h`, `tfiletype.h`, `txshleveltypes.h`, pennelli, sceneviewer, menu
- Adattamenti rispetto all'originale: `TToolViewer` -> `TTool::Viewer`,
  `bind()` senza alias, `onPropertyChanged` senza flag addToUndo, `TFrameId` da
  stringa nel lettore tzm. Nostro (non in OpenToonz): aggancio del tratto senza
  `TInputManager` (`tnztools/assistantstrokesnap`), replicatori via la simmetria
  (`tnztools/replicamap`), interpolazione fra disegni chiave e Grid Density
  (chiavi `interpolation` e `gridDensity` nei `.tzm`), menu Guides, correzione
  del RangeError in `TAssistantBase::updateProperty` (candidato upstream)
- Stato in Tahoma2D: non presente
- Note: `inputmanager`, `track`, `modifiers/` compilati ma quasi inutilizzati,
  tenuti per ridurre i conflitti futuri con OpenToonz. SCANOSS segnala i file
  come «otx» (GPL): falso positivo, sono codice OpenToonz BSD-3.

## [2026-10-20] Import FLA/XFL (Adobe Animate)
- Origine: OpenAnimationLibrary/opentoonz-dev PR #267 «Add FLA/XFL (Adobe
  Animate) import» — OPEN, non mergiata; non in OpenToonz
- Autore originale: Abdullah (`doangelshavewings`); la PR dichiara codice
  generato con Claude Code
- File toccati in Ztoryc: 72 file su 74 identici alla PR (`flash/`,
  `toonzlib/flash*`, `toonz/flash*`), adattati `toonz/flashimportpopup.{cpp,h}`;
  core: toonzscene, txsheet, txshcolumn, txshlevelcolumn, stage, scenefx,
  pli_io/tiio_pli, regioni vettoriali, iwa_textfx, cellselection, nuovi FX
- Adattamenti rispetto all'originale: import sempre nella scena aperta e
  salvata, come sotto-scena o colonne, in un solo passo di annulla (caricamento
  protetto); bundle negli extras del progetto; istanze che seguono le celle
  fuse con loop e indicatori di Ztoryc; maschere: portata Flash per conteggio e,
  nostro, maschera dentro una cartella che ritaglia tutta la cartella. Non
  portati: Flash Save As, nomi delle colonne vuote nell'annulla dell'incolla
- Stato in Tahoma2D: non presente
- Note: dipende solo da zlib e Qt (lettore ZIP proprio, QXmlStreamReader);
  Ruffle citato solo come riferimento di comportamento. Formati: livelli
  importati in PLI 151, scene con dati Flash in 71.2-71.4, non leggibili dalle
  versioni precedenti.
