#!/usr/bin/env bash
# =============================================================================
# Ztoryc — i pezzi del lip sync che NON dipendono dal sistema operativo.
#
# Modelli Vosk, modello Whisper e SORGENTE di espeak-ng: sono gli stessi file
# su macOS, Windows e Linux, quindi si scaricano una volta sola e con un codice
# solo. I binari (libvosk, whisper-cli, espeak-ng) sono per piattaforma e li
# prende lo script di bundling del rispettivo sistema.
#
# ⚠️ PERCHE' UNO SCRIPT SOLO E NON TRE
# Tre copie della stessa procedura divergono alla prima correzione fatta su una
# e non sulle altre. In questo progetto e' gia' successo piu' volte — due
# elenchi di file che viaggiano col livello, due tabelle di comandi workflow,
# due ricerche della mappa delle bocche. Il costo si paga mesi dopo, quando una
# release esce senza un pezzo su una piattaforma sola.
#
# ⚠️ IL SORGENTE DI ESPEAK-NG NON E' FACOLTATIVO
# espeak-ng e' GPLv3: spedirne il binario obbliga a offrire il sorgente
# corrispondente a chi riceve la release. Qui si scarica l'archivio DELLO
# STESSO tag da cui si costruisce il binario, e la release lo allega. Cosi'
# l'obbligo e' soddisfatto da una riga di CI invece che dalla memoria di
# qualcuno.
#
# Uso:  ci-scripts/fetch-lipsync-deps.sh <cartella-di-destinazione>
# Idempotente: cio' che c'e' gia' non viene riscaricato.
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
source "$HERE/thirdparty_versions.sh"

DST="${1:-$REPO/thirdparty/apps/lipsync}"
mkdir -p "$DST"
cd "$DST"

say() { echo ">>> $*"; }

# ─────────────────────────────────────────────────────────────────────────────
# 1. Modelli Vosk, riconfezionati in .zvosk
#
# Si riconfezionano invece di spedire le cartelle: il bundle passa da 155 MB a
# 86, e su disco all'utente finiscono solo le lingue che usa davvero.
# ─────────────────────────────────────────────────────────────────────────────
pack_model() {
  local name="$1" out="$2"
  if [ -f "$out" ]; then
    say "modello $out gia' presente"
    return
  fi
  if [ ! -d "$name" ]; then
    say "scarico $name"
    curl -fsSL -o "$name.zip" "$ZTORYC_VOSK_MODEL_BASEURL/$name.zip"
    unzip -q "$name.zip"
    rm -f "$name.zip"
  fi
  say "riconfeziono $name -> $out"
  python3 "$REPO/tools/pack_vosk_model.py" "$name" "$out"
  # La cartella scompattata non serve piu': pesa quanto il modello intero.
  rm -rf "$name"
}

pack_model "$ZTORYC_VOSK_MODEL_EN" "en.zvosk"
pack_model "$ZTORYC_VOSK_MODEL_IT" "it.zvosk"

# ─────────────────────────────────────────────────────────────────────────────
# 2. Modello Whisper
# ─────────────────────────────────────────────────────────────────────────────
if [ -f "$ZTORYC_WHISPER_MODEL" ]; then
  say "modello whisper gia' presente"
else
  say "scarico $ZTORYC_WHISPER_MODEL"
  curl -fsSL -o "$ZTORYC_WHISPER_MODEL" \
    "$ZTORYC_WHISPER_MODEL_URL/$ZTORYC_WHISPER_MODEL"
fi

# ─────────────────────────────────────────────────────────────────────────────
# 3. SORGENTE di espeak-ng — l'adempimento GPLv3
#
# Va allegato alla release insieme al binario. Il nome porta il tag dentro, cosi'
# guardando l'asset si vede subito a quale binario corrisponde.
# ─────────────────────────────────────────────────────────────────────────────
ESPEAK_SRC="espeak-ng-${ZTORYC_ESPEAK_TAG}-source.tar.gz"
if [ -f "$ESPEAK_SRC" ]; then
  say "sorgente espeak-ng gia' presente"
else
  say "scarico il sorgente di espeak-ng ${ZTORYC_ESPEAK_TAG} (obbligo GPLv3)"
  curl -fsSL -o "$ESPEAK_SRC" \
    "${ZTORYC_ESPEAK_REPO}/archive/refs/tags/${ZTORYC_ESPEAK_TAG}.tar.gz"
fi

# ─────────────────────────────────────────────────────────────────────────────
# 4. libvosk — l'unico binario che si scarica gia' pronto
#
# Una sola procedura con tre nomi di file, invece di tre script: la parte che
# cambia fra i sistemi e' l'archivio, non il procedimento. Vedi l'avviso in
# testa sul perche' non si duplica.
#
# whisper-cli ed espeak-ng NON si scaricano: si COSTRUISCONO dai tag fissati
# (script per sistema), perche' i binari precompilati portano dentro percorsi
# di backend che fuori dalla loro macchina non esistono — provato il 2026-08-15
# copiando whisper-cli a mano, e non funzionava.
# ─────────────────────────────────────────────────────────────────────────────
case "$(uname -s)" in
  Darwin)  VOSK_ZIP="$ZTORYC_VOSK_ZIP_OSX";   VOSK_LIB="libvosk.dylib" ;;
  Linux)   VOSK_ZIP="$ZTORYC_VOSK_ZIP_LINUX"; VOSK_LIB="libvosk.so"    ;;
  MINGW*|MSYS*|CYGWIN*) VOSK_ZIP="$ZTORYC_VOSK_ZIP_WIN"; VOSK_LIB="libvosk.dll" ;;
  *) echo "!!! sistema non riconosciuto: $(uname -s)"; exit 1 ;;
esac

if [ -f "$VOSK_LIB" ]; then
  say "$VOSK_LIB gia' presente"
else
  say "scarico $VOSK_ZIP"
  curl -fsSL -o "$VOSK_ZIP" \
    "$ZTORYC_VOSK_BASEURL/v$ZTORYC_VOSK_RELEASE/$VOSK_ZIP"
  unzip -q -o "$VOSK_ZIP"
  # L'archivio ha una cartella dentro: si tira su solo la libreria e si butta
  # il resto (intestazioni ed esempi, che nel bundle non servono).
  found="$(find . -name "$VOSK_LIB" -type f | head -1)"
  [ -n "$found" ] || { echo "!!! $VOSK_LIB non trovata in $VOSK_ZIP"; exit 1; }
  [ "$found" = "./$VOSK_LIB" ] || mv "$found" "./$VOSK_LIB"
  rm -rf "$VOSK_ZIP" "${VOSK_ZIP%.zip}"
  say "$VOSK_LIB pronta ($(du -h "$VOSK_LIB" | cut -f1))"
fi

# ─────────────────────────────────────────────────────────────────────────────
# Controllo finale: meglio fermarsi qui che scoprire a release fatta che un
# pezzo manca. E' successo con i binari helper LZO fuori dal bundle.
# ─────────────────────────────────────────────────────────────────────────────
missing=0
for f in en.zvosk it.zvosk "$ZTORYC_WHISPER_MODEL" "$ESPEAK_SRC" "$VOSK_LIB"; do
  if [ ! -s "$f" ]; then
    echo "!!! manca o e' vuoto: $f"
    missing=1
  fi
done
[ "$missing" -eq 0 ] || exit 1

say "pezzi comuni del lip sync pronti in $DST"
ls -lh
