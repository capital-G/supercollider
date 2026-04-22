#!/bin/sh
set -eu

# use ports - do not use vorbis port since it is missing the encoder which we need!
embuilder build ogg mpg123 sqlite3

# create a temp directory to store all necessary builds
tmpBase="$(cd "$(dirname "$0")" && pwd)/tmp"
# acts as some kind of fake usr install - this is necessary b/c lame is not using cmake but make
installBase=$tmpBase/install

mkdir -p "$tmpBase"

echo "--- BUILD FLAC"
cd "$tmpBase"
git clone --depth 1 https://github.com/xiph/flac.git
cd flac
mkdir build
cd build
emcmake cmake \
  -DCMAKE_INSTALL_PREFIX=$(em-config CACHE)/sysroot \
  -DINSTALL_MANPAGES=OFF \
  -DBUILD_DOCS=OFF \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_PROGRAMS=OFF \
  -DBUILD_TESTING=OFF \
  ..
emmake make
emmake make install

echo "--- BUILD LAME"
cd "$tmpBase"
git clone --depth 1 https://github.com/lameproject/lame.git
cd lame
mkdir build
cd build
emconfigure ../configure \
    --prefix=$(em-config CACHE)/sysroot \
    CFLAGS="-DNDEBUG -DNO_STDIO -I../.." \
    --disable-dependency-tracking \
    --disable-shared \
    --disable-gtktest \
    --disable-analyzer-hooks \
    --disable-frontend
emmake make
emmake make install

echo "--- BUILD OPUS"
cd "$tmpBase"
git clone --depth 1 https://github.com/xiph/opus.git
cd opus
mkdir build
cd build
emcmake cmake -DCMAKE_INSTALL_PREFIX=$(em-config CACHE)/sysroot ..
emmake cmake --build .
emmake cmake --install .

echo "--- BUILD VORBIS"
cd "$tmpBase"
git clone --depth 1 https://github.com/xiph/vorbis.git
cd vorbis
mkdir build
cd build
emcmake cmake -DCMAKE_INSTALL_PREFIX=$(em-config CACHE)/sysroot ..
emmake make
emmake make install

echo "--- BUILD LIBSNDFILE"
cd "$tmpBase"
git clone --depth 1 https://github.com/libsndfile/libsndfile.git
cd libsndfile
mkdir build
cd build
emcmake cmake .. \
    -DCMAKE_INSTALL_PREFIX=$(em-config CACHE)/sysroot \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_PROGRAMS=OFF \
    -DBUILD_TESTING=OFF \
    -DINSTALL_MANPAGES=OFF
emmake make
emmake make install
