#!/bin/bash
export TAHOMA2DVERSION=1.6
#source /opt/qt515/bin/qt515-env.sh

echo ">>> Temporary install of Ztoryc"
SCRIPTPATH=`dirname "$0"`
export BUILDDIR=$SCRIPTPATH/../../toonz/build
cd $BUILDDIR
# Leave one processor available for other processing if possible
parallel=$(($(nproc) < 2 ? 1 : $(nproc) - 1))
sudo make -j "$parallel" install

sudo ldconfig

echo ">>> Creating appDir"
if [ -d appdir ]
then
   rm -rf appdir
fi
mkdir -p appdir/usr

echo ">>> Copy and configure Ztoryc installation in appDir"
cp -r /opt/tahoma2d/* appdir/usr
cp appdir/usr/share/applications/*.desktop appdir
cp appdir/usr/share/icons/hicolor/128x128/apps/*.png appdir
mv appdir/usr/lib/tahoma2d/* appdir/usr/lib
rmdir appdir/usr/lib/tahoma2d

echo ">>> Creating Ztoryc directory"
if [ -d Ztoryc ]
then
   rm -rf Ztoryc
fi
mkdir Ztoryc

echo ">>> Copying stuff to Ztoryc/ztorycstuff"

mv appdir/usr/share/tahoma2d/stuff Ztoryc/ztorycstuff
chmod -R 777 Ztoryc/ztorycstuff
rmdir appdir/usr/share/tahoma2d

find Ztoryc/ztorycstuff -name .gitkeep -exec rm -f {} \;

if [ -d ../../thirdparty/apps/ffmpeg/bin ]
then
   echo ">>> Copying FFmpeg to Ztoryc/ffmpeg"
   if [ -d Ztoryc/ffmpeg ]
   then
      rm -rf Ztoryc/ffmpeg
   fi
   mkdir -p Ztoryc/ffmpeg
   cp -R ../../thirdparty/apps/ffmpeg/bin/ffmpeg ../../thirdparty/apps/ffmpeg/bin/ffprobe Ztoryc/ffmpeg
   if [ -d ../../thirdparty/apps/ffmpeg/lib ]
   then
      cp -R ../../thirdparty/apps/ffmpeg/lib Ztoryc/ffmpeg/
   fi
   chmod -R 755 Ztoryc/ffmpeg
fi

if [ -d ../../thirdparty/apps/rhubarb ]
then
   echo ">>> Copying Rhubarb Lip Sync to Ztoryc/rhubarb"
   if [ -d Ztoryc/rhubarb ]
   then
      rm -rf Ztoryc/rhubarb
   fi
   mkdir -p Ztoryc/rhubarb
   cp -R ../../thirdparty/apps/rhubarb/rhubarb ../../thirdparty/apps/rhubarb/res Ztoryc/rhubarb
   chmod 755 -R Ztoryc/rhubarb
fi

# ── Lip sync: Vosk + Whisper + espeak-ng ────────────────────────────────────
# Stessa cartella portatile di ffmpeg e rhubarb: l'applicazione le cerca anche
# a partire da TEnv::getWorkingDirectory(), che nell'AppImage e' la cartella
# che contiene l'AppImage e non il montaggio.
#
# ⚠️ Se questa roba manca, la build RIESCE e l'applicazione parte: il lip sync
# si ripiega in silenzio. Per questo sotto c'e' un controllo esplicito.
LIPSYNC=../../thirdparty/apps/lipsync
if [ -d "$LIPSYNC" ]
then
   echo ">>> Copying Vosk to Ztoryc/vosk"
   rm -rf Ztoryc/vosk
   mkdir -p Ztoryc/vosk
   cp "$LIPSYNC"/libvosk.so "$LIPSYNC"/*.zvosk Ztoryc/vosk/
   chmod -R 755 Ztoryc/vosk

   echo ">>> Copying Whisper to Ztoryc/whisper"
   rm -rf Ztoryc/whisper
   mkdir -p Ztoryc/whisper
   cp "$LIPSYNC"/ggml-*.bin Ztoryc/whisper/
   if [ -x "$LIPSYNC/whisper-cli" ]; then
      cp "$LIPSYNC/whisper-cli" Ztoryc/whisper/
   fi
   chmod -R 755 Ztoryc/whisper

   if [ -x "$LIPSYNC/espeak-ng" ]; then
      echo ">>> Copying espeak-ng to Ztoryc/espeak"
      rm -rf Ztoryc/espeak
      mkdir -p Ztoryc/espeak
      cp "$LIPSYNC/espeak-ng" Ztoryc/espeak/
      cp -R "$LIPSYNC/espeak-ng-data" Ztoryc/espeak/
      chmod -R 755 Ztoryc/espeak
   fi

   # Il controllo che rende inutile fidarsi.
   for f in vosk/libvosk.so vosk/en.zvosk vosk/it.zvosk \
            whisper/ggml-base-q5_1.bin whisper/whisper-cli \
            espeak/espeak-ng espeak/espeak-ng-data/phontab; do
      if [ ! -s "Ztoryc/$f" ]; then
         echo "!!! manca nel pacchetto: $f"
         exit 1
      fi
   done
   # E che espeak-ng funzioni davvero: senza i suoi dati parte e restituisce
   # una riga vuota, che somiglia a «lingua non supportata».
   IPA="$(ESPEAK_DATA_PATH=Ztoryc/espeak Ztoryc/espeak/espeak-ng -v it --ipa -q casa 2>/dev/null | tr -d '[:space:]')"
   if [ -z "$IPA" ]; then
      echo "!!! espeak-ng nel pacchetto non produce fonemi (dati non trovati)"
      exit 1
   fi
   echo ">>> Lip sync bundled (espeak dice: casa = $IPA)"
else
   echo ">>> WARNING: thirdparty/apps/lipsync assente — il pacchetto NON avra'"
   echo "    il lip sync. Lanciare ci-scripts/fetch-lipsync-deps.sh e"
   echo "    ci-scripts/build-lipsync-tools.sh."
fi

if [ -d ../../thirdparty/canon/Library ]
then
   echo ">>> Copying canon libraries"
   cp -R ../../thirdparty/canon/Library/x86_64/* appdir/usr/lib
fi

echo ">>> Copying libghoto2 supporting directories"
cp -r /usr/local/lib/libgphoto2 appdir/usr/lib
cp -r /usr/local/lib/libgphoto2_port appdir/usr/lib

rm appdir/usr/lib/libgphoto2/print-camera-list
find appdir/usr/lib/libgphoto2* -name *.la -exec rm -f {} \;
find appdir/usr/lib/libgphoto2* -name *.so -exec patchelf --set-rpath '$ORIGIN/../..' {} \;

echo ">>> Creating Ztoryc/Ztoryc.AppImage"

if [ -f /usr/lib/qt5/bin/linuxdeployqt ]
then
   LINUXDEPLOYQT=/usr/lib/qt5/bin/linuxdeployqt
else
if [ ! -f linuxdeployqt*.AppImage ]
then
   wget -c "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
   chmod a+x linuxdeployqt*.AppImage
fi
   LINUXDEPLOYQT=./linuxdeployqt*.AppImage
fi

export LD_LIBRARY_PATH=appdir/usr/lib/tahoma2d
$LINUXDEPLOYQT appdir/usr/bin/Ztoryc -bundle-non-qt-libs -verbose=0 -always-overwrite -no-strip \
   -executable=appdir/usr/bin/lzocompress \
   -executable=appdir/usr/bin/lzodecompress \
   -executable=appdir/usr/bin/tcleanup \
   -executable=appdir/usr/bin/tcomposer \
   -executable=appdir/usr/bin/tconverter \
   -executable=appdir/usr/bin/tfarmcontroller \
   -executable=appdir/usr/bin/tfarmserver 

rm appdir/AppRun
cp ../sources/scripts/AppRun appdir
chmod 775 appdir/AppRun

$LINUXDEPLOYQT appdir/usr/bin/Ztoryc -appimage -no-strip

mv Ztoryc*.AppImage Ztoryc/Ztoryc.AppImage

echo ">>> Creating Ztoryc Linux package"

tar zcf Ztoryc-linux.tar.gz Ztoryc

echo ">>> Creating Ztoryc Debian Package"

chmod +x ../installer/linux/deb-creator/debcreator.sh 

../installer/linux/deb-creator/debcreator.sh \
 -p $TAHOMA2DVERSION \
 -v $TAHOMA2DVERSION \
 -t ../installer/linux/deb-creator/deb-template \
 -x ./appdir \
 -f ./Ztoryc/ffmpeg \
 -r ./Ztoryc/rhubarb \
 -s ../../stuff

 mv ztoryc_*_amd64.deb Ztoryc-linux.deb