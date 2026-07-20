#!/bin/zsh
# build_and_deploy.sh
# Compila Ztoryc e aggiorna Ztoryc.app
# Uso: ./build_and_deploy.sh [file.cpp opzionale da toccare]
#
# Workspace: la directory dello SCRIPT, cioe' il worktree da cui lo lanci.
# Override esplicito con export ZTORYC_WORKSPACE=/path/to/repo.
#
# ATTENZIONE, non rimettere un path fisso qui: prima c'era hardcoded il path di
# master, quindi lanciare lo script da un worktree di feature (es. superplastic)
# compilava e deployava MASTER senza dirlo — vanificando anche la scelta del
# bundle Ztoryc-SP.app qui sotto, che veniva risolta sul workspace sbagliato.
# Il worktree giusto e' sempre quello in cui lo script si trova.

SCRIPT_DIR="${0:A:h}"
if [[ -n "${ZTORYC_WORKSPACE:-}" ]]; then
  WORKSPACE="$ZTORYC_WORKSPACE"
else
  WORKSPACE="$SCRIPT_DIR"
fi

# Individua la directory di build: se build.ninja è nella root del workspace
# (layout attuale), BUILD = WORKSPACE.  Altrimenti usa toonz/build (layout legacy).
if [[ -f "$WORKSPACE/build.ninja" ]]; then
  BUILD="$WORKSPACE"
elif [[ -d "$WORKSPACE/toonz/build" ]]; then
  BUILD="$WORKSPACE/toonz/build"
else
  BUILD="$WORKSPACE"
fi

# Bundle prodotto da Ninja: toonz/Ztoryc.app (root layout) o toonz/build/toonz/Ztoryc.app (legacy).
# I worktree di feature usano un bundle rinominato (es. Ztoryc-SP.app per
# feature/superplastic) per non confonderlo con la build master: se c'e', ha la
# precedenza. Senza questo il deploy scriveva su Ztoryc.app mentre si lanciava
# Ztoryc-SP.app, cioe' si testava un binario vecchio senza accorgersene.
if [[ -d "$WORKSPACE/toonz/Ztoryc-SP.app" ]]; then
  APP="$WORKSPACE/toonz/Ztoryc-SP.app"
elif [[ -d "$WORKSPACE/toonz/Ztoryc.app" ]]; then
  APP="$WORKSPACE/toonz/Ztoryc.app"
else
  APP="$BUILD/toonz/Ztoryc.app"
fi
MACOS="$APP/Contents/MacOS"
echo "→ Bundle di destinazione: $APP"

if [ -n "$1" ]; then
  echo "→ Touch $1..."
  touch "$1"
fi

echo "→ Compilazione..."
ninja -j4 -C "$BUILD" 2>&1 | tee /tmp/ztoryc_build.log | grep -E "error:|warning:.*error|Linking|up-to-date"
NINJA_STATUS=${pipestatus[1]}
if [[ $NINJA_STATUS -ne 0 ]]; then
  echo "✗ Build fallita (ninja exit $NINJA_STATUS) — app in esecuzione non toccata."
  exit 1
fi

echo "→ Copia binario..."
cp "$BUILD/toonz/Ztoryc.app/Contents/MacOS/Ztoryc" "$MACOS/Ztoryc"

echo "→ Copia dylib Ztoryc..."
# Usa sempre i path espliciti nelle sottodirectory — le copie nella root di
# build/ sono stale (non aggiornate da ninja dopo il primo link).
cp "$BUILD/toonzlib/libtoonzlib.dylib"          "$MACOS/"
cp "$BUILD/tnzcore/libtnzcore.dylib"             "$MACOS/"
cp "$BUILD/tnzbase/libtnzbase.dylib"             "$MACOS/"
cp "$BUILD/sound/libsound.dylib"                 "$MACOS/"
cp "$BUILD/tnztools/libtnztools.dylib"           "$MACOS/"
cp "$BUILD/tnzext/libtnzext.dylib"               "$MACOS/"
cp "$BUILD/colorfx/libcolorfx.dylib"             "$MACOS/"
cp "$BUILD/stdfx/libtnzstdfx.dylib"              "$MACOS/"
cp "$BUILD/toonzqt/libtoonzqt.dylib"             "$MACOS/"
cp "$BUILD/toonzfarm/tfarm/libtfarm.dylib"       "$MACOS/"

echo "→ Patch rpath libimage (libtiff: /usr/local/lib → @executable_path)..."
install_name_tool -change \
  /usr/local/lib/libtiff.5.dylib \
  @executable_path/libtiff.5.dylib \
  "$BUILD/image/libimage.dylib" 2>/dev/null || true
cp "$BUILD/image/libimage.dylib" "$MACOS/"

echo "→ Copia risorse..."
# SystemVar.ini: path assoluti alle risorse (stuff dir).
# Viene scritto esplicitamente con chiavi sia TAHOMA2D* che ZTORYC* per coprire
# l'init anticipata di TEnv (che usa toUpper(appName)="ZTORYC" come prefix
# prima che main.cpp chiami setSystemVarPrefix("TAHOMA2D")).
SYSVAR="$APP/Contents/Resources/SystemVar.ini"
STUFF="$WORKSPACE/stuff"
cat > "$SYSVAR" << EOF
TAHOMA2DROOT=$STUFF
TAHOMA2DPROFILES=$STUFF/profiles
TAHOMA2DCONFIG=$STUFF/config
ZTORYCROOT=$STUFF
ZTORYCPROFILES=$STUFF/profiles
ZTORYCCONFIG=$STUFF/config
EOF

echo "→ Copia FFmpeg nel bundle (se assente)..."
FFMPEG_DST="$APP/Contents/Resources/ffmpeg"
if [[ ! -f "$FFMPEG_DST/ffmpeg" ]]; then
  # Cerca ffmpeg: prima nel bundle /Applications/Ztoryc.app (con libs/), poi Homebrew
  FFMPEG_SRC=""
  [[ -f "/Applications/Ztoryc.app/Contents/Resources/ffmpeg/ffmpeg" ]] && \
    FFMPEG_SRC="/Applications/Ztoryc.app/Contents/Resources/ffmpeg"
  if [[ -n "$FFMPEG_SRC" ]]; then
    # Copia l'intera directory (include libs/ per @executable_path)
    cp -R "$FFMPEG_SRC/" "$FFMPEG_DST/"
    echo "  ffmpeg copiato da $FFMPEG_SRC"
  elif [[ -f "$(brew --prefix 2>/dev/null)/bin/ffmpeg" ]]; then
    mkdir -p "$FFMPEG_DST"
    cp "$(brew --prefix)/bin/ffmpeg"  "$FFMPEG_DST/"
    cp "$(brew --prefix)/bin/ffprobe" "$FFMPEG_DST/" 2>/dev/null || true
    echo "  ffmpeg copiato da Homebrew (senza libs)"
  else
    echo "  ⚠ ffmpeg non trovato — formati video non disponibili"
  fi
fi

echo "→ Copia helper LZO..."
LZO_DIR="$BUILD"
[[ -f "$BUILD/lzodriver/lzocompress" ]] && LZO_DIR="$BUILD/lzodriver"
cp "$LZO_DIR/lzocompress"   "$MACOS/lzocompress"
cp "$LZO_DIR/lzodecompress" "$MACOS/lzodecompress"

echo "→ Rimozione xattr e runtime artifacts prima della firma..."
# Rimuove attributi estesi (com.apple.provenance ecc.) che invalidano il seal
xattr -cr "$APP" 2>/dev/null || true
# Rimuove directory create a runtime dall'app (profiles/, cache/ ecc.)
# che non fanno parte del bundle sigillato
rm -rf "$APP/profiles" "$APP/cache" "$APP/logs" 2>/dev/null || true

# ztorycstuff è una directory nella root del bundle (non in Contents/) usata
# da Ztoryc a runtime. codesign la vede come "unsealed contents" e fallisce.
# Soluzione: spostala fuori temporaneamente, firma, poi rimettila.
ZTORY_STUFF_TMP=""
if [[ -d "$APP/ztorycstuff" ]]; then
  ZTORY_STUFF_TMP="/tmp/ztorycstuff_deploy_$$"
  mv "$APP/ztorycstuff" "$ZTORY_STUFF_TMP"
fi

echo "→ Firma codice (dylib prima, poi bundle)..."
# Prima firma ogni dylib singolarmente
setopt nullglob
for f in "$MACOS"/*.dylib; do
  codesign --force --sign - "$f" 2>/dev/null
done
unsetopt nullglob
# Poi firma i binari helper
codesign --force --sign - "$MACOS/lzocompress"   2>/dev/null
codesign --force --sign - "$MACOS/lzodecompress" 2>/dev/null
# Firma ffmpeg/ffprobe (e libs/) se presenti — le dylib devono essere firmate prima degli exe
if [[ -d "$APP/Contents/Resources/ffmpeg" ]]; then
  xattr -cr "$APP/Contents/Resources/ffmpeg" 2>/dev/null || true
  for f in "$APP/Contents/Resources/ffmpeg/libs/"*.dylib; do
    [[ -f "$f" ]] && codesign --force --sign - "$f" 2>/dev/null
  done
  codesign --force --sign - "$APP/Contents/Resources/ffmpeg/ffmpeg"  2>/dev/null
  codesign --force --sign - "$APP/Contents/Resources/ffmpeg/ffprobe" 2>/dev/null
fi
# Infine firma il bundle completo (senza --deep per evitare re-firma ricorsiva)
codesign --force --sign - --entitlements "$WORKSPACE/Ztoryc.entitlements" "$APP"

# Ripristina ztorycstuff dopo la firma
[[ -n "$ZTORY_STUFF_TMP" && -d "$ZTORY_STUFF_TMP" ]] && mv "$ZTORY_STUFF_TMP" "$APP/ztorycstuff"

echo "✓ Fatto."
if ! pgrep -x Ztoryc > /dev/null 2>&1; then
  echo "→ Apertura app..."
  open "$APP"
else
  # Un'istanza avviata PRIMA di questo deploy esegue il binario VECCHIO: ogni
  # test fatto lì sopra è invalido. È già successo tre volte in una sessione
  # (2026-07-19), una delle quali ha quasi fatto scartare un fix corretto.
  echo ""
  echo "⚠️⚠️⚠️  ISTANZA GIÀ APERTA — STA ESEGUENDO IL BINARIO VECCHIO  ⚠️⚠️⚠️"
  echo "    Chiudila e riaprila PRIMA di testare, o il test non vale niente."
  echo ""
fi
