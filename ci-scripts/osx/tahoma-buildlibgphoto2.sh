#!/bin/bash
# Builds libgphoto2 (tahoma2d fork) and installs it into $BREW_PREFIX, so that
# the Ztoryc build finds gphoto2/gphoto2.h. WITH_GPHOTO2 must stay ON: camera
# capture is used for stop-motion and for grabbing thumbnails.
#
# `set -e` is deliberate. Without it (until 2026-08-05) a failed `make` was
# followed by `sudo make install` and the step still reported success. That is
# how the 0.12.0 macOS DMGs went missing: the headers were never installed and
# the build failed ten minutes later on "'gphoto2/gphoto2.h' file not found".
set -euo pipefail

BREW_PREFIX="${BREW_PREFIX:-$(brew --prefix)}"
cd thirdparty

# thirdparty/libgphoto2_src is part of the CI cache, so an entry restored
# without the headers leaves the clone in place: reuse it instead of dying on
# `git clone` into an existing directory.
if [ -d libgphoto2_src/.git ]; then
  echo ">>> Reusing existing libgphoto2 clone"
else
  echo ">>> Cloning libgphoto2"
  rm -rf libgphoto2_src
  git clone https://github.com/tahoma2d/libgphoto2.git libgphoto2_src
fi

cd libgphoto2_src

git checkout tahoma2d-version-2.5.34

echo ">>> Configuring libgphoto2"
autoreconf --install --symlink

# --disable-nls: camlibs/Makefile.am links every camlib against libgphoto2.la
# and libgphoto2_port.la only -- never $(INTLLIBS), which libgphoto2/Makefile.am
# does add. On glibc that passes unnoticed because dgettext lives in libc; on
# macOS Homebrew's libintl.h rewrites it to libintl_dgettext and each camlib
# fails to link with "Undefined symbols: _libintl_dgettext" (seen on ax203,
# arm64 and x86_64 alike). The public headers are installed by a SUBDIR that
# comes *after* camlibs, so that link failure is exactly what leaves
# gphoto2/gphoto2.h missing. With NLS off, i18n.h turns every gettext call into
# an identity macro and libintl is not referenced at all -- the only thing lost
# is the translation of libgphoto2's own strings, which Ztoryc never shows.
./configure --prefix="$BREW_PREFIX" --disable-nls

echo ">>> Making libgphoto2"
make

echo ">>> Installing libgphoto2"
sudo make install

# The header is the whole reason this script exists for the Ztoryc build: fail
# here rather than ten minutes later inside the compile of gphotocam.h.
if [ ! -f "$BREW_PREFIX/include/gphoto2/gphoto2.h" ]; then
  echo "ERROR: $BREW_PREFIX/include/gphoto2/gphoto2.h missing after install" >&2
  exit 1
fi
echo ">>> libgphoto2 installed, headers in $BREW_PREFIX/include/gphoto2"

cd ..
