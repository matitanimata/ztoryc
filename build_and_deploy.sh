#!/bin/zsh
# build_and_deploy.sh
# Compila Ztoryc e aggiorna Tahoma2D.app
# Uso: ./build_and_deploy.sh [file.cpp opzionale da toccare]

WORKSPACE="/Volumes/ZioSam/tahoma2d-workspace/tahoma2d"
BUILD="$WORKSPACE/toonz/build"
APP="$WORKSPACE/toonz/Tahoma2D.app"

if [ -n "$1" ]; then
  echo "→ Touch $1..."
  touch "$1"
fi

echo "→ Compilazione..."
ninja -j4 -C "$BUILD" 2>&1 | grep -E "error:|Linking|up-to-date"

echo "→ Copia binario..."
cp "$BUILD/toonz/Tahoma2D.app/Contents/MacOS/Tahoma2D" "$APP/Contents/MacOS/Tahoma2D"

echo "→ Firma codice..."
codesign --force --deep --sign - "$APP"

echo "✓ Fatto. Apertura app..."
open "$APP"
