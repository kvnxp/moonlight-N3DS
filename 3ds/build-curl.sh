#!/usr/bin/env bash

set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
CURL_DIR=$ROOT_DIR/third_party/curl

mkdir -p $CURL_DIR
cd $CURL_DIR

CURL_VER=7.87.0
TARBALL=curl-${CURL_VER}.tar.gz
URL=https://curl.se/download/${TARBALL}

if [ ! -f ${TARBALL} ]; then
    wget -O ${TARBALL} ${URL}
fi

if [ ! -d curl-${CURL_VER} ]; then
    tar xzf ${TARBALL}
fi

cd curl-${CURL_VER}

export CFLAGS="-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations -Wno-psabi -fomit-frame-pointer -ffunction-sections"
export CPPFLAGS="-D__3DS__ -I${DEVKITPRO}/portlibs/3ds/include -I${DEVKITPRO}/libctru/include"
export LDFLAGS="-L${DEVKITPRO}/portlibs/3ds/lib -L${DEVKITPRO}/libctru/lib"

# Avoid configure tests that require linking host resolver libraries which are
# not available in the cross-toolchain. Force known symbols as present and
# disable optional dependencies to simplify the build for 3DS.
export ac_cv_func_gethostbyname=yes
export ac_cv_func_gethostbyname_r=yes
export ac_cv_func_getaddrinfo=yes
export ac_cv_header_netdb_h=yes

# Provide common 3DS/libs to satisfy socket/connect tests during configure.
# Try multiple likely library names; configure will ignore unknown link symbols
# if the linker cannot find them, but having these helps the cross-link tests.
export LIBS="-lctru -lssl -lcrypto"

./configure --host=arm-none-eabi --prefix=$DEVKITPRO/portlibs/3ds \
    --disable-shared --enable-static --without-ssl --without-zlib --disable-ipv6 \
    --disable-unix-sockets --without-libidn2 --without-librtmp --disable-ldap \
    --disable-ftp --disable-file \
    CC=$DEVKITARM/bin/arm-none-eabi-gcc AR=$DEVKITARM/bin/arm-none-eabi-ar \
    RANLIB=$DEVKITARM/bin/arm-none-eabi-ranlib PKG_CONFIG=$DEVKITPRO/portlibs/3ds/bin/arm-none-eabi-pkg-config

make -j$(nproc)
make install

cd -
