#!/usr/bin/env bash
set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR=$SCRIPT_DIR/..
STUB_DIR=$ROOT_DIR/third_party/curl_stub

source /etc/profile.d/devkit-env.sh

mkdir -p $STUB_DIR/build
cd $STUB_DIR/build

CC=$DEVKITARM/bin/arm-none-eabi-gcc
AR=$DEVKITARM/bin/arm-none-eabi-ar

$CC -c ../curl_stub.c -o curl_stub.o -march=armv6k -mtune=mpcore -mfloat-abi=hard -D__3DS__ -I${DEVKITPRO}/portlibs/3ds/include -I${DEVKITPRO}/libctru/include
$AR rcs libcurl.a curl_stub.o

mkdir -p ${DEVKITPRO}/portlibs/3ds/include/curl
mkdir -p ${DEVKITPRO}/portlibs/3ds/lib

cp ../curl.h ${DEVKITPRO}/portlibs/3ds/include/curl/curl.h
cp libcurl.a ${DEVKITPRO}/portlibs/3ds/lib/libcurl.a

echo "Installed curl stub to ${DEVKITPRO}/portlibs/3ds"
