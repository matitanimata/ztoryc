# =============================================================================
# Single source of truth: third-party versions for Tahoma2D / Ztoryc CI scripts.
# Bump versions only here. Shell scripts: source this file:
#   source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/thirdparty_versions.sh"
# Windows .bat: use read_thirdparty_version.py — do not duplicate literals.
#
# FFmpeg (macOS / Linux): one fork tag is built from source into the prefix
# (Homebrew / /usr/local). Bundled CLI binaries for the app are staged from that
# same install (ci-scripts/*/tahoma-bundle-apps.sh) — no separate “static zip”
# download from third-party releases.
#
# OpenCV: TAHOMA_OPENCV_GIT_REF empty means `git clone` default branch of the fork.
# Set to a tag or branch for reproducible CI (e.g. 4.x-tahoma).
# =============================================================================

export TAHOMA_FFMPEG_REPO=https://github.com/tahoma2d/FFmpeg.git
export TAHOMA_FFMPEG_GIT_TAG=v4.3.1

export TAHOMA_OPENCV_REPO=https://github.com/tahoma2d/opencv.git
export TAHOMA_OPENCV_GIT_REF=

export TAHOMA_RHUBARB_RELEASE=v1.13.0

# =============================================================================
# Lip sync (Ztoryc): Vosk + whisper.cpp + espeak-ng
#
# ⚠️ LE VERSIONI QUI SONO FISSATE, e non e' pedanteria di riproducibilita':
# espeak-ng e' GPLv3, e spedirne il binario obbliga a offrire IL SORGENTE
# CORRISPONDENTE. Un'offerta che punta a «l'ultima versione» non corrisponde a
# niente: e' il numero fissato qui che rende valida l'offerta, perche' la CI
# costruisce il binario e allega l'archivio DALLO STESSO tag.
#
# Ztoryc non linka mai espeak-ng: lo invoca come processo separato, come fa con
# ffmpeg e Rhubarb. Percio' Ztoryc resta BSD 3-Clause.
# =============================================================================

# Vosk API — Apache 2.0. Binari precompilati per piattaforma.
#
# ⚠️ NON ALZARE QUESTO NUMERO SENZA CONTROLLARE GLI ASSET.
# La 0.3.42 e' l'ULTIMA release che pubblica un binario macOS
# (`vosk-osx-0.3.42.zip`, universal2). Dalla 0.3.45 in poi ci sono solo Linux,
# Windows e i wheel Python: alzando il pin, macOS resterebbe senza libvosk e il
# lip sync si ripiegherebbe su Whisper senza che nessuna build fallisca —
# cioe' il difetto si scoprirebbe dagli utenti.
# Verificato il 2026-08-16 interrogando l'API delle release.
export ZTORYC_VOSK_RELEASE=0.3.42
export ZTORYC_VOSK_BASEURL=https://github.com/alphacep/vosk-api/releases/download
export ZTORYC_VOSK_ZIP_OSX=vosk-osx-${ZTORYC_VOSK_RELEASE}.zip
export ZTORYC_VOSK_ZIP_WIN=vosk-win64-${ZTORYC_VOSK_RELEASE}.zip
export ZTORYC_VOSK_ZIP_LINUX=vosk-linux-x86_64-${ZTORYC_VOSK_RELEASE}.zip

# Modelli Vosk «small» — Apache 2.0, da alphacephei.com.
# Vengono riconfezionati in .zvosk dalla CI con tools/pack_vosk_model.py:
# nel bundle finiscono compressi, e Ztoryc li scompatta in cache al primo uso
# di quella lingua.
export ZTORYC_VOSK_MODEL_EN=vosk-model-small-en-us-0.15
export ZTORYC_VOSK_MODEL_IT=vosk-model-small-it-0.22
export ZTORYC_VOSK_MODEL_BASEURL=https://alphacephei.com/vosk/models

# whisper.cpp — MIT. Serve per le lingue senza modello Vosk e per il
# rilevamento automatico della lingua.
export ZTORYC_WHISPER_TAG=v1.7.4
export ZTORYC_WHISPER_MODEL=ggml-base-q5_1.bin
export ZTORYC_WHISPER_MODEL_URL=https://huggingface.co/ggerganov/whisper.cpp/resolve/main

# espeak-ng — GPLv3. Vedi l'avviso in cima a questo blocco: da questo tag si
# costruisce IL BINARIO e si allega IL SORGENTE alla stessa release.
export ZTORYC_ESPEAK_TAG=1.52.0
export ZTORYC_ESPEAK_REPO=https://github.com/espeak-ng/espeak-ng

export DAV1D_TAG=0.9.2
