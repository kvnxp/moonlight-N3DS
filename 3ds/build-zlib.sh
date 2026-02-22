#!/usr/bin/env bash

set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
ZLIB_DIR=$ROOT_DIR/third_party/zlib

cd $ROOT_DIR/third_party

if [ ! -d zlib ]; then
    wget https://github.com/madler/zlib/archive/refs/tags/v1.2.13.tar.gz
    tar -xzf v1.2.13.tar.gz
    mv zlib-1.2.13 zlib
    rm v1.2.13.tar.gz
fi

cd $ZLIB_DIR

echo "DEVKITPRO=$DEVKITPRO"

export CFLAGS="-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations -Wno-psabi -fomit-frame-pointer -ffunction-sections"
export CXXFLAGS="${CFLAGS}"
export CPPFLAGS="-D__3DS__ -I${DEVKITPRO}/libctru/include"
export LDFLAGS="-L${DEVKITPRO}/libctru/lib"
export LIBS="-lctru -lm"

export CC=$DEVKITARM/bin/arm-none-eabi-gcc
export AR=$DEVKITARM/bin/arm-none-eabi-ar
export RANLIB=$DEVKITARM/bin/arm-none-eabi-ranlib

$CC $CFLAGS $CPPFLAGS -c *.c
$AR rc libz.a *.o
$RANLIB libz.a

mkdir -p $DEVKITPRO/portlibs/3ds/lib
mkdir -p $DEVKITPRO/portlibs/3ds/include
cp libz.a $DEVKITPRO/portlibs/3ds/lib/
cp zlib.h zconf.h $DEVKITPRO/portlibs/3ds/include/

cd -