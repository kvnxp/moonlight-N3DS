#!/usr/bin/env bash

set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
FREETYPE_DIR=$ROOT_DIR/third_party/freetype

cd $ROOT_DIR/third_party

if [ ! -d freetype ]; then
    wget https://download.savannah.gnu.org/releases/freetype/freetype-2.13.2.tar.gz
    tar -xzf freetype-2.13.2.tar.gz
    mv freetype-2.13.2 freetype
    rm freetype-2.13.2.tar.gz
fi

cd $FREETYPE_DIR

export CFLAGS="-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations -Wno-psabi -fomit-frame-pointer -ffunction-sections"
export CXXFLAGS="${CFLAGS}"
export CPPFLAGS="-D__3DS__ -I${DEVKITPRO}/libctru/include -I${DEVKITPRO}/portlibs/3ds/include"
export LDFLAGS="-L${DEVKITPRO}/libctru/lib -L${DEVKITPRO}/portlibs/3ds/lib"
export LIBS="-lctru -lm -lz"
export PKG_CONFIG_PATH="${DEVKITPRO}/portlibs/3ds/lib/pkgconfig"

./configure \
    --prefix=$DEVKITPRO/portlibs/3ds/ \
    --host=arm-none-eabi \
    --enable-static \
    --disable-shared \
    --without-harfbuzz \
    CC=$DEVKITARM/bin/arm-none-eabi-gcc \
    AR=$DEVKITARM/bin/arm-none-eabi-ar \
    RANLIB=$DEVKITARM/bin/arm-none-eabi-ranlib \
    PKG_CONFIG=$DEVKITPRO/portlibs/3ds/bin/arm-none-eabi-pkg-config

make -j$(nproc)
make install

cd -