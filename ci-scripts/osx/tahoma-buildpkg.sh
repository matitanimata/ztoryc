#!/bin/bash
#
# Ztoryc macOS packaging — produces a portable .app and optional DMG.
# After install_name_tool, embedded signatures are invalid; dyld may SIGKILL with
# CODESIGNING / Invalid Page. Re-seal the portable bundle ad hoc after every
# Mach-O rewrite; this uses no certificate/notarization and avoids --deep.
#
# Prerequisites (typical CI order):
#   1. CMake build has emitted toonz/build/toonz/Ztoryc.app
#   2. Optional: thirdparty/apps/{ffmpeg,rhubarb} from tahoma-bundle-apps.sh (gitignored)
#   3. Qt macdeployqt available via QTDIR (Homebrew qt@5)
#
# Environment:
#   SKIP_PKG=1     Skip .pkg installer (CI uses this; DMG still built unless SKIP_DMG=1)
#   SKIP_DMG=1     Stop after normalizing the .app, before building the DMG (fast local iteration)
#   BREW_PREFIX    Homebrew prefix (default: brew --prefix)
#   ZTORYC_DMG_BASENAME  Output DMG filename (default: Ztoryc-portable-osx.dmg)
#
# Paths are anchored to REPO_ROOT after an early cd — do not rely on caller cwd.
#
export TAHOMA2DVERSION=1.6

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
STUFF_SRC="$REPO_ROOT/stuff"

if [[ ! -f "$STUFF_SRC/config/qss/Dark/Dark.qss" ]]; then
  echo "ERROR: Theme files missing — expected $STUFF_SRC/config/qss/Dark/Dark.qss"
  echo "       (/checkout may be incomplete; verify stuff/config/qss is in git)"
  ls -la "$STUFF_SRC/config/qss" 2>&1 || true
  exit 1
fi
QSS_N="$(find "$STUFF_SRC/config/qss" -name '*.qss' 2>/dev/null | wc -l | tr -d ' ')"
echo ">>> Packaging stuff from $STUFF_SRC ($QSS_N theme .qss file(s) under config/qss)"

cd "$REPO_ROOT" || {
  echo "ERROR: cannot cd to REPO_ROOT=$REPO_ROOT"
  exit 1
}

export BREW_PREFIX="${BREW_PREFIX:-$(brew --prefix)}"
echo ">>> BREW_PREFIX=$BREW_PREFIX"

if [ -d /usr/local/Cellar/qt@5 ]
then
   export QTDIR=/usr/local/opt/qt@5
elif [ -d /opt/homebrew/opt/qt@5 ]
then
   export QTDIR=/opt/homebrew/opt/qt@5
else
   export QTDIR=/usr/local/opt/qt
fi
export TOONZDIR=toonz/build/toonz

# If present, use Xcode-style Release subdirectory for the app bundle
if [ -d "$TOONZDIR/Release" ]; then
   export TOONZDIR="$TOONZDIR/Release"
fi

if [ -d "$TOONZDIR/Ztoryc.app/Contents/Resources/ztorycstuff" ]
then
   rm -rf "$TOONZDIR/Ztoryc.app/Contents/Resources/ztorycstuff"
fi

if [ -d thirdparty/apps/ffmpeg/bin ]
then
   echo ">>> Copying FFmpeg to Ztoryc.app/Contents/Resources/ffmpeg"
   if [ -d $TOONZDIR/Ztoryc.app/Contents/Resources/ffmpeg ]
   then
      rm -rf $TOONZDIR/Ztoryc.app/Contents/Resources/ffmpeg
   fi
   mkdir $TOONZDIR/Ztoryc.app/Contents/Resources/ffmpeg
   cp -R thirdparty/apps/ffmpeg/bin/ffmpeg thirdparty/apps/ffmpeg/bin/ffprobe $TOONZDIR/Ztoryc.app/Contents/Resources/ffmpeg
   if [ -d thirdparty/apps/ffmpeg/libs ]
   then
      cp -R thirdparty/apps/ffmpeg/libs $TOONZDIR/Ztoryc.app/Contents/Resources/ffmpeg/
   elif [ -d thirdparty/apps/ffmpeg/lib ]
   then
      cp -R thirdparty/apps/ffmpeg/lib $TOONZDIR/Ztoryc.app/Contents/Resources/ffmpeg/
   fi
   # dylibbundler writes @executable_path/../libs/*.dylib expecting a bin/ layout,
   # but the binaries land directly in Resources/ffmpeg/ (no bin/ subdir).
   # Fix: rewrite @executable_path/../libs/ → @executable_path/libs/ for ffmpeg + ffprobe.
   FFDIR="$TOONZDIR/Ztoryc.app/Contents/Resources/ffmpeg"
   for binary in "$FFDIR/ffmpeg" "$FFDIR/ffprobe"; do
     if [ -f "$binary" ]; then
       while IFS= read -r dep; do
         dep_trimmed="${dep#"${dep%%[! ]*}"}"  # ltrim spaces
         if [[ "$dep_trimmed" == @executable_path/../libs/* ]]; then
           libname="$(basename "$dep_trimmed")"
           install_name_tool -change "$dep_trimmed" "@executable_path/libs/$libname" "$binary" 2>/dev/null || true
         fi
       done < <(otool -L "$binary" | tail -n +2 | awk '{print $1}')
     fi
   done
   # Also fix internal IDs and cross-references inside the bundled dylibs.
   if [ -d "$FFDIR/libs" ]; then
     for dylib in "$FFDIR/libs/"*.dylib; do
       [ -f "$dylib" ] || continue
       while IFS= read -r dep; do
         dep_trimmed="${dep#"${dep%%[! ]*}"}"
         if [[ "$dep_trimmed" == @executable_path/../libs/* ]]; then
           libname="$(basename "$dep_trimmed")"
           install_name_tool -change "$dep_trimmed" "@executable_path/libs/$libname" "$dylib" 2>/dev/null || true
         fi
       done < <(otool -L "$dylib" | tail -n +2 | awk '{print $1}')
     done
   fi
   echo ">>> FFmpeg load commands patched (../libs → ./libs)"
   chmod -R 755 $TOONZDIR/Ztoryc.app/Contents/Resources/ffmpeg
fi

if [ -d thirdparty/apps/rhubarb ]
then
   echo ">>> Copying Rhubarb Lip Sync to Ztoryc.app/Contents/Resources/rhubarb"
   if [ -d $TOONZDIR/Ztoryc.app/Contents/Resources/rhubarb ]
   then
      rm -rf $TOONZDIR/Ztoryc.app/Contents/Resources/rhubarb
   fi
   mkdir $TOONZDIR/Ztoryc.app/Contents/Resources/rhubarb
   cp -R thirdparty/apps/rhubarb/rhubarb thirdparty/apps/rhubarb/res $TOONZDIR/Ztoryc.app/Contents/Resources/rhubarb
   chmod -R 755 $TOONZDIR/Ztoryc.app/Contents/Resources/rhubarb
fi

# ── Lip sync: Vosk + Whisper ────────────────────────────────────────────────
# I pezzi li mette ci-scripts/fetch-lipsync-deps.sh, uno script solo per i tre
# sistemi. Qui si copiano soltanto.
#
# ⚠️ Se questa cartella manca, la build RIESCE lo stesso e l'applicazione parte:
# libvosk si carica a runtime e in sua assenza il lip sync si ripiega. Cioe' una
# release senza questi file sembrerebbe sana e avrebbe la funzione morta — la
# stessa forma della dimenticanza dei binari helper LZO. Per questo sotto c'e'
# un controllo esplicito invece di un semplice `if [ -d ]`.
if [ -d thirdparty/apps/lipsync ]
then
   echo ">>> Copying Vosk to Ztoryc.app/Contents/Resources/vosk"
   rm -rf "$TOONZDIR/Ztoryc.app/Contents/Resources/vosk"
   mkdir -p "$TOONZDIR/Ztoryc.app/Contents/Resources/vosk"
   cp thirdparty/apps/lipsync/libvosk.dylib \
      thirdparty/apps/lipsync/*.zvosk \
      "$TOONZDIR/Ztoryc.app/Contents/Resources/vosk/"
   chmod -R 755 "$TOONZDIR/Ztoryc.app/Contents/Resources/vosk"

   echo ">>> Copying Whisper model to Ztoryc.app/Contents/Resources/whisper"
   rm -rf "$TOONZDIR/Ztoryc.app/Contents/Resources/whisper"
   mkdir -p "$TOONZDIR/Ztoryc.app/Contents/Resources/whisper"
   cp thirdparty/apps/lipsync/ggml-*.bin \
      "$TOONZDIR/Ztoryc.app/Contents/Resources/whisper/"
   chmod -R 755 "$TOONZDIR/Ztoryc.app/Contents/Resources/whisper"

   # Il controllo che rende inutile fidarsi: se il lip sync doveva esserci e
   # non c'e', la build si ferma qui invece di produrre un pacchetto monco.
   for f in vosk/libvosk.dylib vosk/en.zvosk vosk/it.zvosk; do
      if [ ! -s "$TOONZDIR/Ztoryc.app/Contents/Resources/$f" ]; then
         echo "!!! manca nel bundle: Resources/$f"
         exit 1
      fi
   done
   echo ">>> Lip sync bundled"
else
   echo ">>> WARNING: thirdparty/apps/lipsync assente — il pacchetto NON avra'"
   echo "    l'allineatore Vosk. Lanciare ci-scripts/fetch-lipsync-deps.sh."
fi

if [ ! -d $TOONZDIR/Ztoryc.app/Contents/Frameworks ]
then
   mkdir $TOONZDIR/Ztoryc.app/Contents/Frameworks
fi
mkdir -p "$TOONZDIR/Ztoryc.app/Contents/Resources"
# Legacy portable layout put ztorycstuff/ffmpeg/rhubarb next to Contents; remove leftovers.
rm -rf "$TOONZDIR/Ztoryc.app/ztorycstuff" "$TOONZDIR/Ztoryc.app/ffmpeg" \
       "$TOONZDIR/Ztoryc.app/rhubarb" "$TOONZDIR/Ztoryc.app/DSYM" 2>/dev/null || true

if [ -d thirdparty/canon/Framework ]
then
   if [ ! -d $TOONZDIR/Ztoryc.app/Contents/Frameworks/EDSDK.framework ]
   then
      echo ">>> Copying canon framework to Ztoryc.app/Contents/Frameworks/EDSDK.Framework"
      cp -R thirdparty/canon/Framework/ $TOONZDIR/Ztoryc.app/Contents/Frameworks
      chmod -R 755 $TOONZDIR/Ztoryc.app/Contents/Frameworks/EDSDK.framework
   fi
fi

if [ ! -d $TOONZDIR/Ztoryc.app/Contents/Frameworks/libgphoto2 ]
then
   echo ">>> Copying libghoto2 supporting directories to Ztoryc.app/Contents/Frameworks"
   cp -R "$BREW_PREFIX/lib/libgphoto2" $TOONZDIR/Ztoryc.app/Contents/Frameworks
   cp -R "$BREW_PREFIX/lib/libgphoto2_port" $TOONZDIR/Ztoryc.app/Contents/Frameworks

   rm -f $TOONZDIR/Ztoryc.app/Contents/Frameworks/libgphoto2/print-camera-list
   find "$TOONZDIR/Ztoryc.app/Contents/Frameworks"/libgphoto2* -name '*.la' -exec rm -f {} \; 2>/dev/null || true
fi

# dSYM generation is slow (dsymutil per helper binary). CI sets SKIP_DSYM_IN_PACKAGE=1
# to save several minutes; unset locally for Ztoryc.dSYM beside the .app.
if [ "${SKIP_DSYM_IN_PACKAGE:-0}" = "1" ]; then
   echo ">>> Skipping DSYM bundle (SKIP_DSYM_IN_PACKAGE=1)"
else
   echo ">>> Creating DSYM files"
   if [ -d $TOONZDIR/DSYM ]; then
      rm -rf $TOONZDIR/DSYM
   fi
   for X in `find $TOONZDIR/Ztoryc.app/Contents/MacOS -type f`
   do
      chmod u+w "$X" 2>/dev/null || true
      dsymutil -o $TOONZDIR/DSYM $X
      # No strip(1): stripping breaks embedded signatures and recent macOS kills at dlopen.
   done
fi

if [ -d $TOONZDIR/Ztoryc.app/DSYM ]
then
   rm -rf $TOONZDIR/Ztoryc.app/DSYM
fi

echo ">>> Configuring Ztoryc.app for deployment"

$QTDIR/bin/macdeployqt $TOONZDIR/Ztoryc.app -verbose=0 -always-overwrite \
   -executable=$TOONZDIR/Ztoryc.app/Contents/MacOS/lzocompress \
   -executable=$TOONZDIR/Ztoryc.app/Contents/MacOS/lzodecompress \
   -executable=$TOONZDIR/Ztoryc.app/Contents/MacOS/tcleanup \
   -executable=$TOONZDIR/Ztoryc.app/Contents/MacOS/tcomposer \
   -executable=$TOONZDIR/Ztoryc.app/Contents/MacOS/tconverter \
   -executable=$TOONZDIR/Ztoryc.app/Contents/MacOS/tfarmcontroller \
   -executable=$TOONZDIR/Ztoryc.app/Contents/MacOS/tfarmserver 

###
### Add additional frameworks
###
for FW in `echo "QtDBus QtPdf QtQml QtQmlModels QtQuick QtVirtualKeyboard"`
do
   if [ ! -d $TOONZDIR/Ztoryc.app/Contents/Frameworks/$FW.framework ]
   then
      echo ">>> Copying missing $FW.framework to Ztoryc.app/Contents/Frameworks"
      cp -r $QTDIR/Frameworks/$FW.framework $TOONZDIR/Ztoryc.app/Contents/Frameworks
   fi
done

if [ ! -d $TOONZDIR/Ztoryc.app/Contents/lib ]
then
   echo ">>> Adding Contents/lib symbolic link to Ztoryc.app/Contents/Frameworks"
   ln -s Frameworks $TOONZDIR/Ztoryc.app/Contents/lib
fi

echo ">>> Correcting library paths"
function checkLibFile() {
   local LIBFILE=$1   
   for DEPFILE in `otool -L $LIBFILE | sed -e "s/ (.*$//" | grep -e"$BREW_PREFIX" -e"@rpath" -e"\.\./\.\./\.\." | grep -v "/qt"`
   do
      local Z=`echo $DEPFILE | cut -c 1-1`
      if [ "$Z" = "/" -o "$Z" = "@" ]
      then
         local Y=`basename $DEPFILE`
         local W=`basename $LIBFILE`
         local X=`echo $DEPFILE | grep "\.framework\/"`
         if [ "$X" = "" -a ! -f $TOONZDIR/Ztoryc.app/Contents/Frameworks/$Y ]
         then
            local SRC=$DEPFILE
            local Z=`echo $DEPFILE | cut -c 1-16`
            local Z2=`echo $DEPFILE | cut -c 1-6`
            if [ "$Z" = "@loader_path/../" ]
            then
               local V=`echo $DEPFILE | sed -e"s/^.*\/\.\.\///"`
               local SRC=$BREW_PREFIX/$V
            elif [ "$Z2" = "@rpath" ]
            then
                local SRC=$BREW_PREFIX/lib/$Y
            fi
            # La sorgente puo' non esistere dove la dipendenza dice: le formule
            # brew versionate (protobuf@33, protobuf@34...) sono keg-only e NON
            # stanno in $BREW_PREFIX/lib. Se non la si cerca, il cp fallisce, ma
            # l'install_name viene riscritto lo stesso qui sotto -> il bundle
            # punta a un file che non c'e' e l'app muore in dyld all'avvio, con
            # la build verde. Upstream Tahoma2D questa guardia ce l'ha.
            if [ ! -f "$SRC" ]
            then
               echo ">>> $SRC non trovato — cerco in $BREW_PREFIX/opt, /usr, /opt"
               SRC=`find $BREW_PREFIX/opt -name $Y 2>/dev/null | head -1`
               if [ "$SRC" = "" ]; then SRC=`find /usr -name $Y 2>/dev/null | head -1`; fi
               if [ "$SRC" = "" ]; then SRC=`find /opt -name $Y 2>/dev/null | head -1`; fi
               if [ "$SRC" = "" ]
               then
                  echo "ERROR: dipendenza $Y non trovata (richiesta da $LIBFILE)."
                  echo "       Riscrivere l'install name senza averla copiata"
                  echo "       produrrebbe un bundle che non si avvia. Mi fermo."
                  exit 1
               fi
               echo ">>> trovata: $SRC"
            fi
            echo ">>>Checking for $SRC"
            if [ ! -f $SRC ]
            then
               echo ">>>NOT Found...Searching /usr"
               local SRC=`find /usr -name $Y | head -1`
               if [ "$SRC" = "" ]
               then
                  echo ">>>NOT Found...Searching /opt"
                  local SRC=`find /opt -name $Y | head -1`
                  if [ "$SRC" = "" ]
                  then
                     echo "Error: Unable to find dependency $Y"
                     exit 1
                  fi
               fi
            fi
            echo "Copying $SRC to Frameworks"
            if ! cp "$SRC" $TOONZDIR/Ztoryc.app/Contents/Frameworks
            then
               echo "ERROR: copia di $SRC in Frameworks fallita."
               exit 1
            fi
            chmod 644 $TOONZDIR/Ztoryc.app/Contents/Frameworks/$Y
            local ORIGDEPFILE=$DEPFILE
            checkLibFile $TOONZDIR/Ztoryc.app/Contents/Frameworks/$Y
            DEPFILE=$ORIGDEPFILE
         fi
         # libgfortran -> @rpath/libgcc_s.* is shorter than @executable_path/../Frameworks/...
         # install_name_tool cannot lengthen it without relinking (-headerpad). We vendor
         # libgcc_s and strip brew LC_RPATH so @loader_path / bundle rpaths resolve instead.
         case "$LIBFILE" in
            */libgfortran*.dylib)
               case "$DEPFILE" in
                  @rpath/libgcc_s*) continue ;;
               esac ;;
         esac
         if [ "$Y" != "$W" ]
         then
            if [ "$X" != "" ]
            then
               local Y=`echo $DEPFILE | sed -e"s/^.*\/\.\.\///" -e"s/@rpath.//"`
            fi
            echo "Fixing $DEPFILE in $LIBFILE"
            install_name_tool -change $DEPFILE @executable_path/../Frameworks/$Y $LIBFILE
         fi
         FIXCHECK=`otool -D $LIBFILE | grep -v ":" | grep -F "$BREW_PREFIX"`
         if [ "$FIXCHECK" == "$DEPFILE" ]
         then
            echo "   Fixed ID!"
            install_name_tool -id @rpath/$Y $LIBFILE
         fi
      fi
   done
}

# --- Portable Mach-O: dyld + library validation (EXC_BAD_ACCESS / CODESIGNING) ---
# macdeployqt leaves Qt PlugIns with install ids under /opt/homebrew/.../plugins/.
# Vendored libgcc_s still carries the Cellar id until rewritten. Either can make
# dyld map "invalid" pages when the file is inside an ad-hoc bundle. Fix install
# names to @executable_path/../<path-under-Contents/> and strip brew LC_RPATH.

ztoryc_macho_lc_rpaths() {
   otool -l "$1" 2>/dev/null | awk '
      /cmd LC_RPATH/ {p=1; next}
      p && /^[[:space:]]*path / {
         line=$0
         sub(/^[[:space:]]*path[[:space:]]+/,"",line)
         sub(/[[:space:]]*\(offset.*$/,"",line)
         if (line != "") print line
         p=0; next
      }
      p && /^[[:space:]]*cmd / { p=0 }
   '
}

ztoryc_lc_rpath_is_brew() {
   case "$1" in
      /opt/homebrew/*|/usr/local/opt/*|/usr/local/Cellar/*) return 0 ;;
      "$BREW_PREFIX"/*) return 0 ;;
   esac
   return 1
}

ztoryc_fix_brew_install_ids() {
   local BUNDLE_ROOT="$REPO_ROOT/$TOONZDIR/Ztoryc.app"
   [ -d "$BUNDLE_ROOT/Contents" ] || return 0
   while IFS= read -r _M; do
      file "$_M" 2>/dev/null | grep -q 'Mach-O' || continue
      _id=$(otool -D "$_M" 2>/dev/null | tail -1 | sed 's/^[[:space:]]*//')
      case "$_id" in
         /opt/homebrew/*|/usr/local/opt/*|/usr/local/Cellar/*)
            _rel="${_M#*/Contents/}"
            if [ "$_rel" = "$_M" ]; then
               echo "ERROR: ztoryc_fix_brew_install_ids: path has no /Contents/: $_M"
               exit 1
            fi
            chmod u+w "$_M" 2>/dev/null || true
            if ! install_name_tool -id "@executable_path/../$_rel" "$_M"; then
               echo "ERROR: install_name_tool -id failed: $_M (was: $_id)"
               exit 1
            fi
            ;;
      esac
   done < <(find "$BUNDLE_ROOT/Contents" -type f 2>/dev/null)
}

ztoryc_strip_brew_lc_rpath_all() {
   local BUNDLE_ROOT="$REPO_ROOT/$TOONZDIR/Ztoryc.app"
   local _M _rp _pass _any
   while IFS= read -r _M; do
      file "$_M" 2>/dev/null | grep -q 'Mach-O' || continue
      chmod u+w "$_M" 2>/dev/null || true
      for _pass in $(seq 1 30); do
         _any=
         while IFS= read -r _rp; do
            [ -z "$_rp" ] && continue
            ztoryc_lc_rpath_is_brew "$_rp" || continue
            _any=1
            install_name_tool -delete_rpath "$_rp" "$_M" 2>/dev/null || true
         done < <(ztoryc_macho_lc_rpaths "$_M")
         [ -z "$_any" ] && break
      done
   done < <(find "$BUNDLE_ROOT/Contents" -type f 2>/dev/null)
}

ztoryc_verify_portable_macho() {
   local BUNDLE_ROOT="$REPO_ROOT/$TOONZDIR/Ztoryc.app"
   local _M _rp _id _fail _dep
   _fail=0
   while IFS= read -r _M; do
      file "$_M" 2>/dev/null | grep -q 'Mach-O' || continue
      while IFS= read -r _rp; do
         ztoryc_lc_rpath_is_brew "$_rp" || continue
         echo "ERROR: forbidden LC_RPATH in $_M -> $_rp"
         _fail=1
      done < <(ztoryc_macho_lc_rpaths "$_M")
      _id=$(otool -D "$_M" 2>/dev/null | tail -1 | sed 's/^[[:space:]]*//')
      case "$_id" in
         /opt/homebrew/*|/usr/local/opt/*|/usr/local/Cellar/*)
            echo "ERROR: forbidden install name in $_M -> $_id"
            _fail=1
            ;;
      esac
      while IFS= read -r _dep; do
         case "$_dep" in
            /opt/homebrew/*|/usr/local/opt/*|/usr/local/Cellar/*)
               echo "ERROR: forbidden dylib path in $_M -> $_dep"
               _fail=1
               ;;
         esac
      done < <(otool -L "$_M" 2>/dev/null | tail -n +2 | sed 's/^[[:space:]]*//;s/ (compatibility.*//')
      # Riferimenti PENDENTI: un @executable_path/@loader_path perfettamente
      # riscritto verso un file che non e' stato copiato passa i controlli qui
      # sopra (non c'e' nessun path brew) e fa morire l'app in dyld all'avvio.
      # E' il difetto che ha bloccato la 0.10.1 su Intel: libopencv_dnn cercava
      # @executable_path/../Frameworks/libprotobuf.34.1.0.dylib, mai copiata.
      while IFS= read -r _dep; do
         local _rel="" _c1="" _c2="" _c3=""
         case "$_dep" in
            @executable_path/*)
               # @executable_path e' la cartella dell'eseguibile che CARICA la
               # libreria, non quella della libreria. Per l'app e'
               # Contents/MacOS; per gli helper spediti in Resources (ffmpeg,
               # ffprobe) e' la loro cartella; e per le librerie dentro
               # Resources/ffmpeg/libs il caricatore sta un livello SOPRA di
               # loro, quindi serve anche la cartella padre — senza, una
               # dipendenza "@executable_path/libs/X" diventerebbe "libs/libs/X".
               # Da un Mach-O non si sa chi lo caricherà: provo i tre punti di
               # partenza plausibili e fallisco solo se non risolve in nessuno.
               _rel="${_dep#@executable_path/}"
               _c1="$BUNDLE_ROOT/Contents/MacOS/$_rel"
               _c2="$(dirname "$_M")/$_rel"
               _c3="$(dirname "$(dirname "$_M")")/$_rel" ;;
            @loader_path/*)
               _rel="${_dep#@loader_path/}"
               _c1="$(dirname "$_M")/$_rel"
               _c2="$_c1"
               _c3="$_c1" ;;
            *) continue ;;
         esac
         { [ -e "$_c1" ] || [ -e "$_c2" ] || [ -e "$_c3" ]; } && continue
         echo "ERROR: dangling reference in $_M -> $_dep"
         echo "       (provati: $_c1 | $_c2 | $_c3)"
         _fail=1
      done < <(otool -L "$_M" 2>/dev/null | tail -n +2 | sed 's/^[[:space:]]*//;s/ (compatibility.*//')
   done < <(find "$BUNDLE_ROOT/Contents" -type f 2>/dev/null)
   if [ "$_fail" != 0 ]; then
      echo "ERROR: bundle verification failed (Homebrew paths o riferimenti pendenti)."
      exit 1
   fi
}

for FILE in `find $TOONZDIR/Ztoryc.app/Contents -type f | grep -v -e"\.h" -e"\.prl" -e"\.plist" -e"\.conf" -e"\.icns" -e"EDSDK" -e"\/Headers\/"`
do
   checkLibFile $FILE
done

# gfortran uses @rpath/libgcc_s.1.1.dylib; strip brew LC_RPATH so dyld does not
# pick /opt/homebrew before Frameworks. Ship GCC's libgcc_s next to libgfortran
# (no -change on libgfortran — longer paths do not fit without -headerpad).
echo ">>> Vendoring libgcc_s from GCC toolchain (if present)"
_LIBGCC_SRC=$(find "$BREW_PREFIX/opt/gcc" "$BREW_PREFIX/Cellar/gcc" -name 'libgcc_s*.dylib' 2>/dev/null | head -1)
if [ -n "$_LIBGCC_SRC" ] && [ -f "$_LIBGCC_SRC" ]; then
   _LIBGCC_BASE=$(basename "$_LIBGCC_SRC")
   cp -f "$_LIBGCC_SRC" "$TOONZDIR/Ztoryc.app/Contents/Frameworks/"
   chmod u+w "$TOONZDIR/Ztoryc.app/Contents/Frameworks/$_LIBGCC_BASE" 2>/dev/null || true
   chmod 644 "$TOONZDIR/Ztoryc.app/Contents/Frameworks/$_LIBGCC_BASE"
fi

echo ">>> Rewriting Mach-O install names that still point at Homebrew (PlugIns, libgcc_s, …)"
ztoryc_fix_brew_install_ids

echo ">>> Removing Homebrew LC_RPATH from bundled Mach-O (portable @rpath resolution)"
ztoryc_strip_brew_lc_rpath_all

echo ">>> Verifying no Homebrew absolute paths remain in bundle Mach-O"
ztoryc_verify_portable_macho

echo ">>> Moving DSYM beside Ztoryc.app (not inside .app bundle)"
if [ -d "$TOONZDIR/DSYM" ]; then
   rm -rf "$TOONZDIR/Ztoryc.dSYM" 2>/dev/null || true
   mv "$TOONZDIR/DSYM" "$TOONZDIR/Ztoryc.dSYM"
elif [ "${SKIP_DSYM_IN_PACKAGE:-0}" = "1" ]; then
   rm -rf "$TOONZDIR/Ztoryc.dSYM" 2>/dev/null || true
fi

if [ "${SKIP_PKG:-0}" != "1" ]; then
  echo ">>> Creating Ztoryc-install-osx.pkg"
  toonz/installer/osx/app.rb "$TOONZDIR" "$STUFF_SRC" toonz/installer/osx/scripts $TAHOMA2DVERSION
  if [ -f "$TOONZDIR/Ztoryc-install-osx.pkg" ]; then
    mv "$TOONZDIR/Ztoryc-install-osx.pkg" "$TOONZDIR/.."
  else
    echo "ERROR: Missing $TOONZDIR/Ztoryc-install-osx.pkg after app.rb"
    exit 1
  fi
else
  echo ">>> Skipping PKG (SKIP_PKG=1)"
fi

FINAL_DMG_NAME="${ZTORYC_DMG_BASENAME:-Ztoryc-portable-osx.dmg}"
echo ">>> Creating portable DMG: $FINAL_DMG_NAME (dmgbuild — no Finder during build)"

# rsync with --exclude='profiles/users/' so the bundle does not ship the
# build machine's private user layouts/preferences (these would otherwise be
# read by any user with the same OS username as the builder, and leak the
# builder's settings to everyone else).
# The CMake POST_BUILD step already uses this rsync for ZTORYC_BUNDLE_STUFF;
# mirror the same exclusion here in the CI packager.
rsync -a --delete --exclude='profiles/users/***' \
  "$STUFF_SRC/" "$TOONZDIR/Ztoryc.app/Contents/Resources/ztorycstuff/"
# Recreate an empty profiles/users with .gitkeep so the app does not need to
# mkdir at runtime (and so it is visible inside the read-only DMG).
mkdir -p "$TOONZDIR/Ztoryc.app/Contents/Resources/ztorycstuff/profiles/users"
touch "$TOONZDIR/Ztoryc.app/Contents/Resources/ztorycstuff/profiles/users/.gitkeep"
chmod -R u+rwX,go+rX "$TOONZDIR/Ztoryc.app/Contents/Resources/ztorycstuff"

find "$TOONZDIR/Ztoryc.app/Contents/Resources/ztorycstuff" -name .gitkeep -not -path '*/profiles/users/*' -exec rm -f {} \;

# Remove SystemVar.ini from portable bundle: toonz/install/SystemVar.ini holds
# absolute /Applications/Ztoryc/Ztoryc_stuff/... paths intended for non-portable
# (brew / system-wide) installs.  CMake POST_BUILD copies it into the .app,
# but in a portable bundle those absolute paths override the portable layout
# (PROFILES, LAYOUTS, ...) and cause the Storyboard workflow to fail to load
# rooms — symptom: clicking Storyboard makes the workflow "disappear" or
# crashes on workflow switch.  Portable detection (ztorycstuff next to MacOS)
# is sufficient — no SystemVar.ini needed.
rm -f "$TOONZDIR/Ztoryc.app/Contents/Resources/SystemVar.ini"

# libimage.dylib was built against libtiff.5 (libtiff 4.4.x ABI),
# but ships with a hardcoded absolute path /usr/local/lib/libtiff.5.dylib.
# On user machines that path doesn't exist (Apple Silicon brew uses
# /opt/homebrew/lib, and current libtiff is .6).  Result: dyld halts at launch
# with "Library not loaded: /usr/local/lib/libtiff.5.dylib".
# Fix: rewrite the load command to a portable @executable_path/libtiff.5.dylib
# and ship the libtiff.5.dylib built from source by tahoma-install.sh.
LIBIMAGE="$TOONZDIR/Ztoryc.app/Contents/MacOS/libimage.dylib"
if [ -f "$LIBIMAGE" ]; then
  echo ">>> Patching libimage.dylib libtiff.5 reference + bundling libtiff.5"
  install_name_tool -change \
    /usr/local/lib/libtiff.5.dylib \
    @executable_path/libtiff.5.dylib \
    "$LIBIMAGE" 2>/dev/null || true
  # Source libtiff.5 from our tahoma-install.sh build (libtiff44 removed from brew).
  LIBTIFF5_SRC="/tmp/libtiff44-build/lib/libtiff.5.dylib"
  if [ -f "$LIBTIFF5_SRC" ]; then
    cp "$LIBTIFF5_SRC" "$TOONZDIR/Ztoryc.app/Contents/MacOS/libtiff.5.dylib"
    chmod u+w "$TOONZDIR/Ztoryc.app/Contents/MacOS/libtiff.5.dylib"
  else
    echo "ERROR: $LIBTIFF5_SRC not found. tahoma-install.sh build may have failed."
    exit 1
  fi
fi

if [[ ! -f "$TOONZDIR/Ztoryc.app/Contents/Resources/ztorycstuff/config/qss/Dark/Dark.qss" ]]; then
  echo "ERROR: After copy, portable bundle missing $TOONZDIR/Ztoryc.app/Contents/Resources/ztorycstuff/config/qss/Dark/Dark.qss"
  exit 1
fi

cd $TOONZDIR

# Drag-to-Applications can fail with error -8060 if the bundle has odd xattrs,
# immutable flags, or non-writable Mach-O.
echo ">>> Normalizing Ztoryc.app for install-by-copy (xattr, flags, permissions)"
find Ztoryc.app -name '.DS_Store' -exec rm -f {} \; 2>/dev/null || true
find Ztoryc.app -name '._*' -exec rm -f {} \; 2>/dev/null || true
xattr -cr Ztoryc.app 2>/dev/null || true
chmod -R u+rwX Ztoryc.app 2>/dev/null || true
chflags -R nouchg,noschg Ztoryc.app 2>/dev/null || true

if ! command -v codesign >/dev/null 2>&1; then
   echo "ERROR: codesign not in PATH (needed to ad-hoc seal the portable bundle)."
   exit 1
fi

ztoryc_is_macho_file() {
   local _ft
   _ft="$(file -b "$1" 2>/dev/null || true)"
   case "$_ft" in
      Mach-O\ *) return 0 ;;
   esac
   return 1
}

ztoryc_adhoc_sign_macho_files() {
   local _main _mf _count _sign_out _tmp_main
   _main="Ztoryc.app/Contents/MacOS/Ztoryc"
   _count=0
   while IFS= read -r -d '' _mf; do
      ztoryc_is_macho_file "$_mf" || continue
      [ "$_mf" = "$_main" ] && continue
      chmod u+w "$_mf" 2>/dev/null || true
      if ! _sign_out="$(codesign --force --sign - --timestamp=none "$_mf" 2>&1)"; then
         echo "$_sign_out"
         echo "ERROR: failed to ad-hoc sign Mach-O: $_mf"
         exit 1
      fi
      _count=$((_count + 1))
   done < <(find Ztoryc.app/Contents -type f -print0)
   _tmp_main="$(mktemp "$REPO_ROOT/toonz/build/.ztoryc_main_sign.XXXXXX")"
   cp "$_main" "$_tmp_main"
   codesign --remove-signature "$_tmp_main" 2>/dev/null || true
   if ! _sign_out="$(codesign --force --sign - --timestamp=none --identifier io.github.ztoryc.Ztoryc "$_tmp_main" 2>&1)"; then
      echo "$_sign_out"
      rm -f "$_tmp_main"
      echo "ERROR: failed to ad-hoc sign main executable: $_main"
      exit 1
   fi
   cp "$_tmp_main" "$_main"
   rm -f "$_tmp_main"
   _count=$((_count + 1))
   echo ">>> Ad-hoc signed $_count Mach-O file(s)"
}

ztoryc_verify_codesign() {
   local _main _mf _fail _tmp_main
   _main="Ztoryc.app/Contents/MacOS/Ztoryc"
   _fail=0
   while IFS= read -r -d '' _mf; do
      ztoryc_is_macho_file "$_mf" || continue
      if [ "$_mf" = "$_main" ]; then
         # codesign treats the main executable path as the whole .app bundle.
         # Verify a copy so we validate the Mach-O page hashes, not resources.
         _tmp_main="$(mktemp "$REPO_ROOT/toonz/build/.ztoryc_main_verify.XXXXXX")"
         cp "$_mf" "$_tmp_main"
         if ! codesign -v "$_tmp_main" >/dev/null 2>&1; then
            echo "ERROR: invalid Mach-O code signature: $_mf"
            codesign -v "$_tmp_main" 2>&1 | sed 's/^/       /'
            _fail=1
         fi
         rm -f "$_tmp_main"
         continue
      fi
      if ! codesign -v "$_mf" >/dev/null 2>&1; then
         echo "ERROR: invalid Mach-O code signature: $_mf"
         codesign -v "$_mf" 2>&1 | sed 's/^/       /'
         _fail=1
      fi
   done < <(find Ztoryc.app/Contents -type f -print0)
   if [ "$_fail" != 0 ]; then
      exit 1
   fi
}

echo ">>> Re-sealing Mach-O files with ad-hoc signatures (no certificate, no --deep)"
rm -rf Ztoryc.app/Contents/_CodeSignature 2>/dev/null || true
ztoryc_adhoc_sign_macho_files
rm -rf Ztoryc.app/Contents/_CodeSignature 2>/dev/null || true
echo ">>> Verifying portable bundle code signatures"
ztoryc_verify_codesign

if [ "${SKIP_DMG:-0}" = "1" ]; then
   echo ">>> SKIP_DMG=1 — skipping portable DMG (bundle: $REPO_ROOT/$TOONZDIR/Ztoryc.app)"
   exit 0
fi

OUTPUT_DMG="$REPO_ROOT/toonz/build/$FINAL_DMG_NAME"
mkdir -p "$(dirname "$OUTPUT_DMG")"

if ! python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)'; then
   echo "ERROR: dmgbuild requires Python 3.10 or newer (found $(python3 -V 2>&1))"
   exit 1
fi

DMG_VENV="$REPO_ROOT/toonz/build/.dmgbuild-venv"
DMG_REQ="$SCRIPT_DIR/requirements-dmgbuild.txt"
if [ ! -x "$DMG_VENV/bin/dmgbuild" ]; then
   echo ">>> Installing dmgbuild into $DMG_VENV (one-time; needs network for pip)"
   python3 -m venv "$DMG_VENV"
   # --no-cache-dir avoids noisy/broken pip cache deserialization on some machines
   "$DMG_VENV/bin/pip" install -q --no-cache-dir --upgrade pip
   "$DMG_VENV/bin/pip" install -q --no-cache-dir -r "$DMG_REQ"
fi

DMG_BG_FALLBACK="$SCRIPT_DIR/assets/ztoryc-dmg-background.png"

# dmgbuild does not mount the *output* DMG. This only clears stale /Volumes/Ztoryc*
# from a previous local open of an installer DMG (avoids name clashes / busy volume).
if [ -z "${CI:-}" ] && [ -z "${GITHUB_ACTIONS:-}" ]; then
   _had_ztoryc_vol=0
   while IFS= read -r _vol; do
      [ -n "$_vol" ] || continue
      _had_ztoryc_vol=1
      hdiutil detach "$_vol" -force >/dev/null 2>&1 || true
   done < <(find /Volumes -maxdepth 1 \( -name 'Ztoryc' -o -name 'Ztoryc *' \) 2>/dev/null)
   if [ "$_had_ztoryc_vol" = "1" ]; then
      echo ">>> Detached leftover Ztoryc installer volume(s) under /Volumes (not opened by this script)"
   fi
fi

let MAXTRY=5
for TRY in $(seq 1 $MAXTRY)
do
   if [ $TRY -gt 1 ]; then
      echo ">>> dmgbuild retry $TRY/$MAXTRY..."
      sleep $((TRY * 3))
   fi
   DMG_STAGE=$(mktemp -d "$REPO_ROOT/toonz/build/.ztoryc_dmg_bg.XXXXXX")
   mkdir -p "$DMG_STAGE/.background"
   if ! python3 "$SCRIPT_DIR/render-dmg-background.py" "$DMG_STAGE/.background/background.png"; then
      echo ">>> WARN: render-dmg-background.py failed, trying fallback asset"
      if [ -f "$DMG_BG_FALLBACK" ]; then
         cp "$DMG_BG_FALLBACK" "$DMG_STAGE/.background/background.png"
      fi
   fi

   APP_ABS="$(pwd)/Ztoryc.app"
   BG_ABS="$DMG_STAGE/.background/background.png"
   rm -f "$OUTPUT_DMG"

   if "$DMG_VENV/bin/dmgbuild" -s "$SCRIPT_DIR/ztoryc_dmg_settings.py" \
         -D "app=$APP_ABS" -D "bg=$BG_ABS" \
         "Ztoryc" "$OUTPUT_DMG"; then
      rm -rf "$DMG_STAGE"
      echo ">>> DMG created successfully: $OUTPUT_DMG"
      exit 0
   fi
   rm -rf "$DMG_STAGE"
done

echo "ERROR: dmgbuild failed after $MAXTRY attempts. Aborting!"
exit 1
