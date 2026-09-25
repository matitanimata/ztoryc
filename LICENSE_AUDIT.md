# Audit licenze di Ztoryc

## Audit 1 — 2026-09-25 (preliminare, solo ScanCode)

**Stato:** completato il 2026-09-25 con la prima passata SCANOSS e una lettura mirata del
codice (vedi «Audit 1 bis» in fondo). Esito: **OK** — nessuna corrispondenza copyleft, e
nessuna derivazione da AnimeEffects nelle parti nate con esso come riferimento.

### Perimetro
File aggiunti o modificati da Ztoryc rispetto al tag `v1.6.3` di Tahoma2D
(base dell'ultimo sync), al commit `1c923917`:
`git diff --name-only --diff-filter=AM v1.6.3 HEAD` → **870 file** (521 nuovi):
438 in `toonz/sources`, 172 in `stuff/library`, 94 in `thirdparty/`
(QXlsx, openssl, superlu). Scansionati i 693 file di testo (esclusi immagini e binari).

### Strumenti
ScanCode Toolkit (rilevamento licenze e copyright), SPDX license list 3.27. 0 errori.

### Risultati

**Nessuna incorporazione di codice GPL rilevata da ScanCode.** Tutte le occorrenze GPL
sono *menzioni* (documentazione, commenti, script) legate a strumenti usati come
processi separati.

| Licenza rilevata | File | Valutazione |
|---|---|---|
| Nessuna licenza nel file | 655 | Normale: convenzione upstream (in Tahoma2D ~1 file su 200 ha un header). Licenza coperta da `LICENSE.txt`. |
| MIT | 12 | QXlsx (Debao Zhang, j2doll). Compatibile. |
| BSD-2 / BSD-3 | 5 | Compatibili. |
| Apache-2.0 | 3 | Vosk (`ztoryvosk.cpp`, testi di licenza). Compatibile. |
| OpenSSL | 1 | `thirdparty/openssl/LICENSE`. Testo di licenza. |
| CC0 + BSD | 2 | Metadati AppStream (`xdg-data`). |
| LGPL-3.0+ | 2 | Script di build di ffmpeg: nessun `--enable-gpl` / `--enable-nonfree` → ffmpeg resta LGPL. |
| GPL-3.0 (menzioni) | 10 | Menzioni di espeak-ng in `ztoryphonemes.h`, `thirdparty.h`, `aboutpopup.cpp`, script lipsync, workflow, documenti. Nessun codice incorporato. |

**espeak-ng (GPL-3.0):** gestione corretta — processo separato, mai linkato; sorgente
distribuito con il binario da un tag fissato (`1.52.0`); degradazione controllata se assente.

### Correzioni applicate (branch `governance-ai-licenze`)
- Aggiunto `stuff/doc/LICENSE/LICENSE_qxlsx.txt` (la MIT richiede l'avviso anche nei binari).
- Aggiunto `stuff/doc/LICENSE/LICENSE_espeak-ng.txt` (testo integrale GPLv3, dal tag 1.52.0),
  a cui `LICENSE_espeak-ng_info.txt` gia' rimandava.

### Da completare
- [x] **SCANOSS** sui 66 nuovi sorgenti e sulle aree nate da riferimenti GPL — fatto il
      2026-09-25, vedi «Audit 1 bis».
- [x] Licenze dei **modelli** lip sync — verificate il 2026-09-25:
      `vosk-model-small-en-us-0.15` e `vosk-model-small-it-0.22` **Apache-2.0** (tabella
      ufficiale https://alphacephei.com/vosk/models; gli archivi dei modelli NON contengono un
      file di licenza, solo un README — corretto `LICENSE_vosk_info.txt`, che diceva il
      contrario); `ggml-base-q5_1.bin` **MIT** (whisper.cpp e pesi OpenAI, gia' documentato
      in `LICENSE_whisper_info.txt`, verifica del 2026-08-15).
- [x] `stuff/doc/LICENSE/` **finisce nei pacchetti** (verificato sugli script del 2026-09-25):
      macOS `rsync` di tutto `stuff/` in `Contents/Resources/ztorycstuff`
      (`ci-scripts/osx/tahoma-buildpkg.sh:588`, escluso solo `profiles/users`); Windows
      `xcopy` in `ztorycstuff` (`ci-scripts/windows/tahoma-buildpkg.bat:187`); Linux `mv` in
      `Ztoryc/ztorycstuff` per il `.tar.gz` (`ci-scripts/linux/tahoma-buildpkg.sh:38`).
      Il `.deb` non e' stato verificato.
- [ ] Valutare se citare QXlsx nell'About, come gli altri componenti.

### Metodo per i riferimenti a progetti GPL
Krita e AnimeEffects (GPL) sono stati usati **solo come riferimento concettuale**:
- deformatori raster ispirati a Krita: **proposta di progetto, nessun codice** (verificato il
  2026-09-25: nessun sorgente di Ztoryc contiene un deformatore raster; la voce precedente
  diceva «riscritti dai paper», ma non sono mai stati scritti);
- `AE_REFERENCE_NOTES.md` (su Drive) raccoglie solo descrizioni concettuali di
  AnimeEffects (FFD, pesi delle ossa, UX del rigging), **senza estratti di codice**,
  e dichiara esplicitamente la regola "il credito concettuale va bene, il codice no"
  (verificato il 2026-09-25);
- per WhisperX: "l'idea si prende, il codice no".

## Audit 1 bis — 2026-09-25, prima passata SCANOSS

`scanoss-py` 1.54.2 (servizio osskb.org, inviate le impronte e non il codice). Dati grezzi e
copie dei file: `reference/tools/scans/2026-09-25/`; triage completo:
`~/ZtorYc/reviews/2026-09-25_scanoss_triage.md`.

**Nessuna corrispondenza con codice copyleft (GPL/LGPL/AGPL)** negli 80 file scansionati.

| Gruppo | File | Senza corrispondenze | Corrispondenze (tutte BSD-3-Clause) |
|---|---|---|---|
| a) nuovi rispetto a v1.6.3 | 66 | 51 | Ztoryc stesso (v0.2.0/v0.3.5): 11; Tahoma2D nightly: 4 (`txshpegbarcolumn.*`, `maintoolbar.*`, arrivati dal merge upstream: non sono codice Ztoryc) |
| b) aree da riferimenti GPL (plastic/skeleton modificati) | 14 | 1 (`plastictool_animate.cpp`) | OpenToonz / Tahoma2D (origine comune): 12; Ztoryc v0.2.0: 1 |

«Pixar» e «Unlicense» compaiono tra le licenze di alcune corrispondenze: sono licenze del
**pacchetto** nightly in cui il file e' stato trovato, non del file (BSD-3 come il resto di Toonz).

Riferimenti nei commenti (nessuno di codice): Moho come esempio d'interfaccia
(`ztorigpanel.h:129`), un'idea di DragonBones sull'annealing IK (`plastictool_animate.cpp:1492`),
la regola «GPL solo come processo separato» in `ztoryphonemes.h`.

**Limite dello strumento, e lettura mirata che lo copre.** SCANOSS trova codice copiato, non
codice riscritto. Per `plastictool_animate.cpp` (sculpt, pesi, IK, nato con AnimeEffects e
DragonBones come riferimento concettuale) e' stata fatta il 2026-09-25 una lettura mirata
contro `AE_REFERENCE_NOTES.md`. Esito **OK**:
- il pennello di sculpt (correttive di giuntura, `d32e6c5ea`, 2026-07-28) e' **precedente**
  alle note su AnimeEffects (2026-07-29), usa una caduta *smoothstep* (formula standard) e
  non quadratica/quartica, realizza un concetto diverso (correttiva guidata dall'angolo del
  giunto) ed e' costruito sulle strutture Plastic di Toonz;
- il modello di pesi di AnimeEffects (capsula + smorzamento angolare) **non e' implementato**:
  l'appartenenza dei vertici ai giunti usa l'ordine di sovrapposizione del Plastic;
- da AnimeEffects sono state prese scelte di interfaccia (pennelli separati per dipingere e
  cancellare l'influenza, posa separata dal rigging), dichiarate nel CHANGELOG;
- IK: CCD/FABRIK sono algoritmi pubblicati; DragonBones non e' GPL.
