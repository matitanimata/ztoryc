#!/bin/bash
set -e
# Leave one processor available for other processing if possible
parallel=$(($(nproc) < 2 ? 1 : $(nproc) - 1))
_should_rebuild_tiff() {
  if [ "${FORCE_TIFF_REBUILD:-0}" = "1" ]; then return 0; fi
  if [ ! -d thirdparty/tiff-4.2.0/libtiff/.libs ]; then return 0; fi
  if [ -z "$(find thirdparty/tiff-4.2.0/libtiff/.libs -maxdepth 1 -name 'libtiff.*' -print -quit 2>/dev/null)" ]; then
    return 0
  fi
  return 1
}
if _should_rebuild_tiff; then
  echo "TIFF: configuring and building"
  pushd thirdparty/tiff-4.2.0
  CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --disable-jbig --disable-webp && make -j "$parallel"
  popd
else
  echo "TIFF: reuse existing libtiff/.libs — skipped configure/make"
fi

cd toonz

if [ ! -d build ]
then
   mkdir build
fi
cd build

#source /opt/qt515/bin/qt515-env.sh

if [ -d ../../thirdparty/canon/Header ]
then
   export CANON_FLAG=-DWITH_CANON=ON
fi

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
cmake ../sources  $CANON_FLAG \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DWITH_GPHOTO2:BOOL=ON \
    -DWITH_SYSTEM_SUPERLU=ON \
    -DWITH_TRANSLATION=OFF

make -j "$parallel"
