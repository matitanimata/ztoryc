#!/usr/bin/env bash
# =============================================================================
# Ztoryc — i due BINARI del lip sync che vanno COSTRUITI: whisper-cli e
# espeak-ng.
#
# ⚠️ PERCHE' NON SI SCARICANO GIA' PRONTI
# I binari precompilati portano dentro percorsi assoluti della macchina che li
# ha costruiti: whisper-cli cerca i backend di ggml dove stavano in Homebrew,
# espeak-ng cerca i suoi dati nel prefisso di installazione. Fuori da li' non
# esistono. Provato il 2026-08-15 copiando whisper-cli a mano: non funzionava.
# Costruirli e' l'unico modo di sapere dove guardano.
#
# ⚠️ PERCHE' UNO SCRIPT SOLO PER TRE SISTEMI
# Il procedimento e' identico ovunque — cmake e un compilatore — e cio' che
# cambia (il suffisso .exe) sta in tre righe. Su Windows gira in Git Bash, che
# sui runner c'e' sempre. Un .bat gemello sarebbe stata la quarta copia della
# stessa procedura in questo repo, e le copie qui divergono sempre: alla prima
# correzione fatta su una sola, una piattaforma esce senza un pezzo.
#
# ⚠️ ESPEAK-NG E' GPLv3: il SORGENTE dello stesso tag va allegato alla release.
# Lo scarica fetch-lipsync-deps.sh, e il job di pubblicazione lo allega. Qui si
# costruisce solo il binario — ma e' lo STESSO tag, ed e' cio' che rende valida
# l'offerta del sorgente.
#
# Uso:  ci-scripts/build-lipsync-tools.sh [cartella-di-destinazione]
# Idempotente: se i binari ci sono gia', non ricostruisce niente.
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
source "$HERE/thirdparty_versions.sh"

DST="${1:-$REPO/thirdparty/apps/lipsync}"
mkdir -p "$DST"
WORK="$DST/.build"
mkdir -p "$WORK"

say() { echo ">>> $*"; }

JOBS="$( (command -v nproc >/dev/null && nproc) || sysctl -n hw.ncpu || echo 4)"

# Windows si serve dello STESSO script, girato in Git Bash: cambia il suffisso
# degli eseguibili e il generatore di cmake, non il procedimento. Scrivere un
# .bat gemello avrebbe voluto dire mantenerne due, e in questo progetto le
# copie divergono sempre alla prima correzione fatta su una sola.
case "$(uname -s)" in
  Darwin|Linux)          EXE="" ;;
  MINGW*|MSYS*|CYGWIN*)  EXE=".exe" ;;
  *) echo "!!! sistema non riconosciuto: $(uname -s)"; exit 1 ;;
esac

# ─────────────────────────────────────────────────────────────────────────────
# 1. whisper-cli
#
# Librerie STATICHE apposta. Con quelle dinamiche ggml apre i suoi backend
# (Metal, BLAS, CPU) a runtime con dlopen e va cercarli accanto alla libreria:
# nel bundle non ci sono, e il fallimento e' silenzioso — whisper-cli parte e
# non riconosce niente. Statico = un file solo da copiare, e niente da cercare.
# ─────────────────────────────────────────────────────────────────────────────
if [ -x "$DST/whisper-cli$EXE" ]; then
  say "whisper-cli gia' presente"
else
  say "costruisco whisper.cpp ${ZTORYC_WHISPER_TAG}"
  rm -rf "$WORK/whisper.cpp"
  git clone --depth 1 --branch "$ZTORYC_WHISPER_TAG" \
      https://github.com/ggerganov/whisper.cpp.git "$WORK/whisper.cpp"
  cmake -S "$WORK/whisper.cpp" -B "$WORK/whisper-build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DWHISPER_BUILD_TESTS=OFF \
        -DWHISPER_BUILD_SERVER=OFF \
        -DGGML_NATIVE=OFF
  cmake --build "$WORK/whisper-build" --config Release -j "$JOBS" \
        --target whisper-cli
  found="$(find "$WORK/whisper-build" -name "whisper-cli$EXE" -type f | head -1)"
  [ -n "$found" ] || { echo "!!! whisper-cli non prodotto"; exit 1; }
  cp "$found" "$DST/whisper-cli$EXE"
  chmod 755 "$DST/whisper-cli$EXE"
  say "whisper-cli pronto ($(du -h "$DST/whisper-cli$EXE" | cut -f1))"
fi

# ─────────────────────────────────────────────────────────────────────────────
# 2. espeak-ng + i suoi DATI
#
# ⚠️ Il binario da solo non serve a niente: espeak-ng senza `espeak-ng-data`
# non sa pronunciare nulla. I dati si portano dietro e Ztoryc gli dice dove
# sono con ESPEAK_DATA_PATH quando lo lancia (stessa trappola di
# GGML_BACKEND_PATH per whisper).
# ─────────────────────────────────────────────────────────────────────────────
if [ -x "$DST/espeak-ng$EXE" ] && [ -d "$DST/espeak-ng-data" ]; then
  say "espeak-ng gia' presente"
else
  say "costruisco espeak-ng ${ZTORYC_ESPEAK_TAG}"
  rm -rf "$WORK/espeak-ng" "$WORK/espeak-build" "$WORK/espeak-install"
  git clone --depth 1 --branch "$ZTORYC_ESPEAK_TAG" \
      "${ZTORYC_ESPEAK_REPO}.git" "$WORK/espeak-ng"
  # Niente audio: a Ztoryc servono i FONEMI (`--ipa -q`), non il suono. Senza
  # questa riga la build pretende PulseAudio/sonic e su un runner pulito
  # fallisce per una dipendenza che non useremmo mai.
  #
  # I dizionari EXTRA di cinese e russo invece SI': costano 10 MB sui 9 di
  # base (misurato), e senza di loro il lip sync di due delle lingue piu'
  # parlate al mondo esce approssimativo. Vosk pubblica i modelli per
  # entrambe, quindi sono lingue che gli utenti useranno davvero.
  cmake -S "$WORK/espeak-ng" -B "$WORK/espeak-build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$WORK/espeak-install" \
        -DBUILD_SHARED_LIBS=OFF \
        -DESPEAK_BLD_EXECUTABLES=ON \
        -DUSE_ASYNC=OFF -DUSE_MBROLA=OFF -DUSE_LIBSONIC=OFF \
        -DUSE_LIBPCAUDIO=OFF -DUSE_KLATT=OFF -DUSE_SPEECHPLAYER=OFF \
        -DEXTRA_cmn=ON -DEXTRA_ru=ON
  cmake --build "$WORK/espeak-build" --config Release -j "$JOBS"
  cmake --install "$WORK/espeak-build" --config Release

  bin="$(find "$WORK/espeak-install" -name "espeak-ng$EXE" -type f | head -1)"
  [ -n "$bin" ] || bin="$(find "$WORK/espeak-build" -name "espeak-ng$EXE" -type f | head -1)"
  [ -n "$bin" ] || { echo "!!! espeak-ng non prodotto"; exit 1; }
  cp "$bin" "$DST/espeak-ng$EXE"
  chmod 755 "$DST/espeak-ng$EXE"

  data="$(find "$WORK/espeak-install" -type d -name "espeak-ng-data" | head -1)"
  [ -n "$data" ] || data="$(find "$WORK/espeak-build" -type d -name "espeak-ng-data" | head -1)"
  [ -n "$data" ] || { echo "!!! espeak-ng-data non prodotta"; exit 1; }
  rm -rf "$DST/espeak-ng-data"
  cp -R "$data" "$DST/espeak-ng-data"
  say "espeak-ng pronto ($(du -sh "$DST/espeak-ng-data" | cut -f1) di dati)"
fi

# ─────────────────────────────────────────────────────────────────────────────
# 3. La prova che contano: FUNZIONANO?
#
# Che il file esista non dice niente — un binario che cerca i backend o i dati
# dove non ci sono esiste benissimo e fallisce all'uso. Qui li si lancia
# davvero, ed e' il controllo che l'esperienza del 2026-08-15 ha reso
# obbligatorio.
# ─────────────────────────────────────────────────────────────────────────────
say "provo whisper-cli"
GGML_BACKEND_PATH="$DST" "$DST/whisper-cli$EXE" --help >/dev/null 2>&1 || {
  echo "!!! whisper-cli non parte"; exit 1; }

say "provo espeak-ng (deve rispondere in IPA)"
ipa="$(ESPEAK_DATA_PATH="$DST" "$DST/espeak-ng$EXE" -v it --ipa -q "casa" 2>/dev/null | tr -d '[:space:]')"
if [ -z "$ipa" ]; then
  echo "!!! espeak-ng non ha prodotto fonemi — dati non trovati?"
  exit 1
fi
say "espeak-ng dice «casa» = $ipa"

# Lo spazio del runner non e' infinito, e i sorgenti pesano piu' dei binari.
rm -rf "$WORK"

say "binari del lip sync pronti in $DST"
ls -lh "$DST"
