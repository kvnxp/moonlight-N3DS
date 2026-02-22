#!/usr/bin/env bash
set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
MBED_DIR=$ROOT_DIR/third_party/mbedtls

mkdir -p $MBED_DIR
cd $MBED_DIR

MBED_VER=2.28.0
TARBALL=mbedtls-${MBED_VER}-gp.tar.gz
URL=https://github.com/ARMmbed/mbedtls/archive/refs/tags/v${MBED_VER}.tar.gz

if [ ! -f v${MBED_VER}.tar.gz ]; then
    wget -O v${MBED_VER}.tar.gz ${URL}
fi

if [ -d mbedtls-${MBED_VER} ]; then
    rm -rf mbedtls-${MBED_VER}
fi

tar xzf v${MBED_VER}.tar.gz
cd mbedtls-${MBED_VER}

source /etc/profile.d/devkit-env.sh

export CC=$DEVKITARM/bin/arm-none-eabi-gcc
export AR=$DEVKITARM/bin/arm-none-eabi-ar
export RANLIB=$DEVKITARM/bin/arm-none-eabi-ranlib
export CFLAGS="-march=armv6k -mtune=mpcore -mfloat-abi=hard -D__3DS__ -DMBEDTLS_NO_PLATFORM_ENTROPY -O2 -fomit-frame-pointer"

# Disable timing and platform-specific features not available on 3DS.
sed -i "s/^#define MBEDTLS_TIMING_C/\/\* #undef MBEDTLS_TIMING_C \*\//" include/mbedtls/config.h || true
sed -i "s/^#define MBEDTLS_HAVE_TIME_DATE/\/\* #undef MBEDTLS_HAVE_TIME_DATE \*\//" include/mbedtls/config.h || true
sed -i "s/^#define MBEDTLS_NET_C/\/\* #undef MBEDTLS_NET_C \*\//" include/mbedtls/config.h || true

# Build only the library target (avoid tests that require platform entropy)
make -j$(nproc) CC="$CC" AR="$AR" RANLIB="$RANLIB" CFLAGS="$CFLAGS" lib

mkdir -p ${DEVKITPRO}/portlibs/3ds/include/mbedtls
mkdir -p ${DEVKITPRO}/portlibs/3ds/lib

cp -r include/mbedtls ${DEVKITPRO}/portlibs/3ds/include/
cp library/libmbedtls.a ${DEVKITPRO}/portlibs/3ds/lib/ || true
cp library/libmbedx509.a ${DEVKITPRO}/portlibs/3ds/lib/ || true
cp library/libmbedcrypto.a ${DEVKITPRO}/portlibs/3ds/lib/ || true

echo "Installed mbedTLS ${MBED_VER} to ${DEVKITPRO}/portlibs/3ds"
