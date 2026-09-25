# Audit licenze di Ztoryc

## Audit 1 — 2026-09-25 (preliminare, solo ScanCode)

**Stato:** preliminare. Manca la scansione snippet (SCANOSS), da eseguire in locale.

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
- [ ] **SCANOSS** sui file di codice del perimetro, priorita' ai 66 nuovi sorgenti in
      `toonz/sources` e alle aree nate da riferimenti a progetti GPL: sculpt/FFD, pesi delle
      ossa, IK, deformatori raster.
- [ ] Licenze dei **modelli** lip sync: `vosk-model-small-en-us-0.15`,
      `vosk-model-small-it-0.22`, `ggml-base-q5_1.bin` (Whisper).
- [ ] Verificare che `stuff/doc/LICENSE/` finisca nei pacchetti di tutte le piattaforme.
- [ ] Valutare se citare QXlsx nell'About, come gli altri componenti.

### Metodo per i riferimenti a progetti GPL
Krita e AnimeEffects (GPL) sono stati usati **solo come riferimento concettuale**:
- deformatori raster ispirati a Krita **riscritti dai paper**, senza codice di Krita;
- `AE_REFERENCE_NOTES.md` (su Drive) raccoglie solo descrizioni concettuali di
  AnimeEffects (FFD, pesi delle ossa, UX del rigging), **senza estratti di codice**,
  e dichiara esplicitamente la regola "il credito concettuale va bene, il codice no"
  (verificato il 2026-09-25);
- per WhisperX: "l'idea si prende, il codice no".
