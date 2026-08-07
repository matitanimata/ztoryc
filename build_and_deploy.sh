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
# Si crede di provare le proprie modifiche e si sta provando tutt'altro ramo.
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
# feature/superplastic, Ztoryc-162.app per merge/upstream-1.6.2) per non
# confonderlo con la build master: se c'e', ha la precedenza. Senza questo il
# deploy scriveva su Ztoryc.app mentre si lanciava Ztoryc-SP.app, cioe' si
# testava un binario vecchio senza accorgersene.
# Il match e' su Ztoryc-*.app e non su un nome fisso: chi apre un worktree nuovo
# gli da' un nome qualsiasi e questo lo trova da solo. Se ce n'e' piu' d'uno la
# scelta sarebbe arbitraria, quindi in quel caso ci si ferma invece di indovinare.
setopt nullglob
_candidates=("$WORKSPACE"/toonz/Ztoryc-*.app)
unsetopt nullglob
if (( ${#_candidates} > 1 )); then
  echo "✗ Piu' di un bundle Ztoryc-*.app in $WORKSPACE/toonz — non so quale aggiornare:"
  for c in $_candidates; do echo "    ${c:t}"; done
  echo "  Lasciane uno solo."
  exit 1
elif (( ${#_candidates} == 1 )); then
  APP="$_candidates[1]"
elif [[ -d "$WORKSPACE/toonz/Ztoryc.app" ]]; then
  APP="$WORKSPACE/toonz/Ztoryc.app"
else
  APP="$BUILD/toonz/Ztoryc.app"
fi
MACOS="$APP/Contents/MacOS"
echo "→ Bundle di destinazione: $APP"

# ---------------------------------------------------------------------------
# Controllo di allineamento con la configurazione della CI.
#
# Questo script NON configura: usa la cache CMake che trova. Il 2026-08-07 sono
# andati via due giorni a cercare nel codice un render sbagliato che dipendeva
# dalla configurazione: la build dir locale era stata creata a mano con QT_PATH
# su una Qt 5.9.2 del 2017 mentre le librerie erano le 5.15.18 di Homebrew, e
# TIFF_INCLUDE_DIR sul libtiff di sistema invece che su thirdparty/. Il
# pacchetto CI rendeva bene sulla stessa macchina, la nostra build no.
# Chi ricrea la build dir a mano puo' rifare lo stesso danno in silenzio:
# qui almeno lo si vede prima di compilare.
#
# Riferimento: ci-scripts/osx/tahoma-build.sh
# Per saltare il controllo: export ZTORYC_SKIP_CONFIG_CHECK=1
# ---------------------------------------------------------------------------
check_cmake_config() {
  local cache="$BUILD/CMakeCache.txt"
  [[ -f "$cache" ]] || { echo "⚠ CMakeCache.txt non trovato in $BUILD — impossibile controllare la configurazione."; return; }

  # Legge una variabile dalla cache ignorandone il tipo (PATH/BOOL/STRING/UNINITIALIZED).
  cache_get() { sed -n "s|^$1:[^=]*=||p" "$cache" | head -1; }

  local -a drift
  local qt_expected="$(brew --prefix 2>/dev/null)/opt/qt@5/lib"
  local tiff_expected="$WORKSPACE/thirdparty/tiff-4.2.0/libtiff"
  local opencv_expected="$(brew --prefix 2>/dev/null)/lib/cmake/opencv4"

  # QT_PATH e' il sospetto numero uno: finisce in CMAKE_PREFIX_PATH e puo'
  # mescolare header di una Qt con le librerie di un'altra.
  local qt_actual="$(cache_get QT_PATH)"
  [[ "${qt_actual%/}" == "${qt_expected%/}" ]] || \
    drift+=("QT_PATH: '$qt_actual'  (CI: '$qt_expected')")

  local tiff_actual="$(cache_get TIFF_INCLUDE_DIR)"
  [[ "${tiff_actual%/}" == "${tiff_expected%/}" ]] || \
    drift+=("TIFF_INCLUDE_DIR: '$tiff_actual'  (CI: '$tiff_expected')")

  local dep_actual="$(cache_get CMAKE_OSX_DEPLOYMENT_TARGET)"
  [[ "$dep_actual" == "12.0" ]] || \
    drift+=("CMAKE_OSX_DEPLOYMENT_TARGET: '$dep_actual'  (CI: '12.0')")

  local superlu_actual="$(cache_get WITH_SYSTEM_SUPERLU)"
  [[ "$superlu_actual" == "ON" ]] || \
    drift+=("WITH_SYSTEM_SUPERLU: '$superlu_actual'  (CI: 'ON')")

  local buildtype_actual="$(cache_get CMAKE_BUILD_TYPE)"
  [[ "$buildtype_actual" == "RelWithDebInfo" ]] || \
    drift+=("CMAKE_BUILD_TYPE: '$buildtype_actual'  (CI: 'RelWithDebInfo')")

  # OpenCV_DIR: la cache memorizza il path risolto in Cellar, il confronto va
  # fatto sul realpath o segnala una differenza che non esiste.
  local opencv_actual="$(cache_get OpenCV_DIR)"
  if [[ -n "$opencv_expected" && -d "$opencv_expected" ]]; then
    local opencv_real="$(cd "$opencv_expected" 2>/dev/null && pwd -P)"
    local opencv_actual_real="$(cd "$opencv_actual" 2>/dev/null && pwd -P)"
    [[ "$opencv_actual_real" == "$opencv_real" ]] || \
      drift+=("OpenCV_DIR: '$opencv_actual'  (CI: '$opencv_expected')")
  fi

  if (( ${#drift} )); then
    echo ""
    echo "⚠️⚠️⚠️  LA BUILD DIR NON E' CONFIGURATA COME QUELLA DEI RILASCI  ⚠️⚠️⚠️"
    echo "    $BUILD/CMakeCache.txt diverge da ci-scripts/osx/tahoma-build.sh:"
    for d in $drift; do echo "      • $d"; done
    echo ""
    echo "    Il binario che ne esce puo' comportarsi diversamente dal pacchetto"
    echo "    pubblicato — e' gia' costato due giorni (vedi ANIMATIC_TASKS 2026-08-07)."
    echo "    Per riallineare, dalla build dir:"
    echo "      cmake $WORKSPACE/toonz/sources -G Ninja \\"
    echo "        -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \\"
    echo "        -DWITH_SYSTEM_SUPERLU=ON -DQT_PATH=$qt_expected \\"
    echo "        -DOpenCV_DIR=$opencv_expected -DTIFF_INCLUDE_DIR=$tiff_expected"
    echo ""
  fi

  # Differenze note e accettate: non sono allarmi, ma vanno ricordate perche'
  # nel 2026-08-07 non si e' ancora stabilito QUALE differenza causasse il
  # render sbagliato, e queste due restano in piedi.
  local -a known
  [[ "$(cache_get WITH_GPHOTO2)" == "ON" ]] || \
    known+=("WITH_GPHOTO2 spento (la CI compila libgphoto2 dal sorgente, qui non c'e')")
  local cmake_major="$(sed -n 's|^CMAKE_CACHE_MAJOR_VERSION:INTERNAL=||p' "$cache" | head -1)"
  [[ "$cmake_major" == "3" ]] || \
    known+=("CMake $cmake_major.x (il workflow fissa la 3.31.6)")
  if (( ${#known} )); then
    echo "ℹ Differenze note dalla CI (accettate): ${(j:; :)known}"
  fi
}

if [[ "${ZTORYC_SKIP_CONFIG_CHECK:-0}" != "1" ]]; then
  check_cmake_config
fi

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

# Ripristina ztorycstuff dopo la firma.
# ATTENZIONE al caso in cui l'app IN ESECUZIONE ha ricreato la cartella mentre
# noi la tenevamo da parte: `mv sorgente destinazione` con destinazione gia'
# esistente NON sostituisce, infila la sorgente DENTRO. Cosi' si erano formate
# ztorycstuff/ztorycstuff_deploy_52369/ztorycstuff_deploy_44980/... annidate,
# che poi facevano fallire la firma con «unsealed contents present in the
# bundle root». In quel caso si fondono i contenuti invece di annidarli.
if [[ -n "$ZTORY_STUFF_TMP" && -d "$ZTORY_STUFF_TMP" ]]; then
  if [[ -d "$APP/ztorycstuff" ]]; then
    rsync -a "$ZTORY_STUFF_TMP/" "$APP/ztorycstuff/" && rm -rf "$ZTORY_STUFF_TMP"
  else
    mv "$ZTORY_STUFF_TMP" "$APP/ztorycstuff"
  fi
fi

echo "✓ Fatto."
if [[ "${ZTORYC_NO_OPEN:-0}" == "1" ]]; then
  # Serve a chi deve solo aggiornare il bundle senza rubare il momento in cui
  # l'app viene lanciata (p.es. mentre Franco sta lavorando, o quando si vuole
  # decidere a mano quale dei bundle aprire).
  echo "→ Non apro l'app (ZTORYC_NO_OPEN=1). Bundle pronto: $APP"
elif ! pgrep -x Ztoryc > /dev/null 2>&1; then
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
