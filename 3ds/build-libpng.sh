#!/usr/bin/env bash

set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
PNG_DIR=$ROOT_DIR/third_party/libpng

cd $ROOT_DIR/third_party

if [ ! -d libpng ]; then
    wget https://download.sourceforge.net/libpng/libpng-1.6.43.tar.gz
    tar -xzf libpng-1.6.43.tar.gz
    mv libpng-1.6.43 libpng
    rm libpng-1.6.43.tar.gz
fi

cd $PNG_DIR

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
    CC=$DEVKITARM/bin/arm-none-eabi-gcc \
    AR=$DEVKITARM/bin/arm-none-eabi-ar \
    RANLIB=$DEVKITARM/bin/arm-none-eabi-ranlib \
    PKG_CONFIG=$DEVKITPRO/portlibs/3ds/bin/arm-none-eabi-pkg-config

make -j$(nproc)
make install

cd -