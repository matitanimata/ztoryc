#!/bin/zsh
# build_and_deploy.sh
# Compila Ztoryc e aggiorna Ztoryc.app
# Uso: ./build_and_deploy.sh [file.cpp opzionale da toccare]
#
# Workspace: default macchina Cowork; override con export ZTORYC_WORKSPACE=/path/to/repo
# oppure si usa automaticamente la directory del clone se il path default non esiste.

SCRIPT_DIR="${0:A:h}"
DEFAULT_WS="/Volumes/ZioSam/tahoma2d-workspace/tahoma2d"
if [[ -n "${ZTORYC_WORKSPACE:-}" ]]; then
  WORKSPACE="$ZTORYC_WORKSPACE"
else
  WORKSPACE="$DEFAULT_WS"
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
if [[ -d "$WORKSPACE/toonz/Ztoryc.app" ]]; then
  APP="$WORKSPACE/toonz/Ztoryc.app"
else
  APP="$BUILD/toonz/Ztoryc.app"
fi
MACOS="$APP/Contents/MacOS"

if [ -n "$1" ]; then
  echo "→ Touch $1..."
  touch "$1"
fi

echo "→ Compilazione..."
ninja -j4 -C "$BUILD" 2>&1 | grep -E "error:|Linking|up-to-date"

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
# Infine firma il bundle completo (senza --deep per evitare re-firma ricorsiva)
codesign --force --sign - --entitlements "$WORKSPACE/Ztoryc.entitlements" "$APP"

# Ripristina ztorycstuff dopo la firma
[[ -n "$ZTORY_STUFF_TMP" && -d "$ZTORY_STUFF_TMP" ]] && mv "$ZTORY_STUFF_TMP" "$APP/ztorycstuff"

echo "✓ Fatto. Apertura app..."
open "$APP"
