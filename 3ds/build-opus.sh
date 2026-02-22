#!/usr/bin/env bash

set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
OPUS_DIR=$ROOT_DIR/third_party/opus

mkdir -p $OPUS_DIR
cd $OPUS_DIR

OPUS_VER=1.3.1
TARBALL=opus-${OPUS_VER}.tar.gz
URL=https://archive.mozilla.org/pub/opus/${TARBALL}

if [ ! -f ${TARBALL} ]; then
    wget -O ${TARBALL} ${URL}
fi

if [ ! -d opus-${OPUS_VER} ]; then
    tar xzf ${TARBALL}
fi

cd opus-${OPUS_VER}

export CFLAGS="-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations -Wno-psabi -fomit-frame-pointer -ffunction-sections"
export CPPFLAGS="-D__3DS__ -I${DEVKITPRO}/libctru/include"
export LDFLAGS="-L${DEVKITPRO}/libctru/lib"

./configure --host=arm-none-eabi --prefix=$DEVKITPRO/portlibs/3ds --disable-shared --enable-static CC=$DEVKITARM/bin/arm-none-eabi-gcc AR=$DEVKITARM/bin/arm-none-eabi-ar RANLIB=$DEVKITARM/bin/arm-none-eabi-ranlib
make -j$(nproc)
make install

cd -
