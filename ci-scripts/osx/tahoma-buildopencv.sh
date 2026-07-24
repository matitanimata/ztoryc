#!/bin/bash
BREW_PREFIX="${BREW_PREFIX:-$(brew --prefix)}"
_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "$_SCRIPT_DIR/../thirdparty_versions.sh"
cd thirdparty

if [ ! -d opencv/.git ]
then
  echo ">>> Cloning opencv"
  if [ -n "$TAHOMA_OPENCV_GIT_REF" ]; then
    git clone -b "$TAHOMA_OPENCV_GIT_REF" "$TAHOMA_OPENCV_REPO" opencv
  else
    git clone "$TAHOMA_OPENCV_REPO" opencv
  fi
else
  echo ">>> Reusing existing opencv checkout"
fi

cd opencv
echo "*" >| .gitignore

if [ ! -d build ]
then
   mkdir build
fi
cd build

echo ">>> Cmaking opencv"
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      -DBUILD_JASPER=OFF \
      -DBUILD_JPEG=OFF \
      -DBUILD_OPENEXR=OFF \
      -DBUILD_PERF_TESTS=OFF \
      -DBUILD_PNG=OFF \
      -DBUILD_PROTOBUF=OFF \
      -DBUILD_TESTS=OFF \
      -DBUILD_TIFF=OFF \
      -DBUILD_WEBP=OFF \
      -DBUILD_ZLIB=OFF \
      -DBUILD_opencv_hdf=OFF \
      -DBUILD_opencv_java=OFF \
      -DBUILD_opencv_dnn=OFF \
      -DWITH_PROTOBUF=OFF \
      -DBUILD_opencv_text=ON \
      -DOPENCV_ENABLE_NONFREE=ON \
      -DOPENCV_GENERATE_PKGCONFIG=ON \
      -DPROTOBUF_UPDATE_FILES=ON \
      -DWITH_1394=OFF \
      -DWITH_CUDA=OFF \
      -DWITH_EIGEN=ON \
      -DWITH_FFMPEG=ON \
      -DWITH_GPHOTO2=OFF \
      -DWITH_GSTREAMER=OFF \
      -DWITH_JASPER=OFF \
      -DWITH_OPENEXR=ON \
      -DWITH_OPENGL=OFF \
      -DWITH_QT=OFF \
      -DWITH_TBB=ON \
      -DWITH_VTK=ON \
      -DBUILD_opencv_python2=OFF \
      -DBUILD_opencv_python3=ON \
      -DCMAKE_MACOS_RPATH=FALSE \
      -DCMAKE_INSTALL_PREFIX="$BREW_PREFIX" \
      -DCMAKE_INSTALL_NAME_DIR="$BREW_PREFIX/lib" \
      ..

echo ">>> Building opencv"
make -j7 # runs 7 jobs in parallel

echo ">>> Installing opencv"
make install
