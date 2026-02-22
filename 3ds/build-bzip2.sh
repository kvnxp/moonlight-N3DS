#!/usr/bin/env bash

set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
BZIP2_DIR=$ROOT_DIR/third_party/bzip2

cd $ROOT_DIR/third_party

if [ ! -d bzip2 ]; then
    wget https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
    tar -xzf bzip2-1.0.8.tar.gz
    mv bzip2-1.0.8 bzip2
    rm bzip2-1.0.8.tar.gz
fi

cd $BZIP2_DIR

export CFLAGS="-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations -Wno-psabi -fomit-frame-pointer -ffunction-sections"
export CXXFLAGS="${CFLAGS}"
export CPPFLAGS="-D__3DS__ -I${DEVKITPRO}/libctru/include"
export LDFLAGS="-L${DEVKITPRO}/libctru/lib"
export LIBS="-lctru -lm"

export CC=$DEVKITARM/bin/arm-none-eabi-gcc
export AR=$DEVKITARM/bin/arm-none-eabi-ar
export RANLIB=$DEVKITARM/bin/arm-none-eabi-ranlib

make -j$(nproc)
make install PREFIX=$DEVKITPRO/portlibs/3ds/

cd -