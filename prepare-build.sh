#!/usr/bin/env bash

# Convenience script for MSYS2/devkitPro environment;
# prepares everything needed before invoking `make`.
# Run from the workspace root (`d:/3ds/projects/moonlight-N3DS`).
# Usage: ./prepare-build.sh

set -e

echo "==> updating MSYS2 packages (you may be prompted)"
pacman -Syu --noconfirm

echo "==> installing host build tools"
pacman -S --noconfirm --needed \
    autoconf automake libtool pkg-config make patch base-devel \
    gettext dos2unix cmake wget

# ensure the 3ds scripts use LF line endings
if command -v dos2unix >/dev/null 2>&1; then
    dos2unix 3ds/*.sh || true
fi

# make sure devkitPro environment is active
source /etc/profile.d/devkit-env.sh

echo "==> patching project sources (Makefile, http.c)"

# patch Makefile once
patch -N -p0 <<'PATCH' || true
*** Begin Patch
*** Update File: Makefile
@@
-LIBS	:= -lswresample -lavformat -lswscale -lavcodec -lavutil -lcitro2d -lcitro3d -lopus -lexpat -lm -lcurl -lssl -lcrypto -lmbedtls -lmbedx509 -lmbedcrypto -lctru
-# note: freetype/png/bz2/zlib not available in portlibs on 3DS.  omit them for now
-# LIBS += -lfreetype -lpng -lbz2 -lz
+LIBS	:= -lswresample -lavformat -lswscale -lavcodec -lavutil -lcitro2d -lcitro3d -lopus -lexpat -lm -lcurl -lssl -lcrypto -lmbedtls -lmbedx509 -lmbedcrypto -lctru
+# additional libraries (freetype, png, bz2, zlib) are not provided for 3DS;
+# add them manually to LIBS if you later install them.
*** End Patch
PATCH

# patch http.c for stdio
patch -N -p0 <<'PATCH' || true
*** Begin Patch
*** Update File: libgamestream/http.c
@@
-#include <curl/curl.h>
-
-#include "errors.h"
+#include <curl/curl.h>
+#include <stdio.h>
+
+#include "errors.h"
*** End Patch
PATCH

# patch RSF to hide unsupported UseExtSaveData, SystemModeExt and other new3DS keys from older makerom
patch -N -p0 <<'PATCH' || true
*** Begin Patch
*** Update File: 3ds/res/app.rsf
@@
-  UseExtSaveData                : false # enables ExtData
+#  UseExtSaveData                : false # enables ExtData (unsupported in some makerom versions)
@@
-  SystemModeExt                 : 124MB # Legacy(Default)/124MB/178MB  Legacy:Use Old3DS SystemMode
+#  SystemModeExt                 : 124MB # New3DS setting (unsupported by older makerom)
@@
+#  EnableL2Cache                 : true # false(default)/true (unsupported by older makerom)
+#  CanAccessCore2                : true # New3DS core2 access
*** End Patch
PATCH

# build supporting libraries
for script in \
    3ds/build-expat.sh \
    3ds/build-opus.sh \
    3ds/build-openssl.sh \
    3ds/build-ffmpeg.sh \
    3ds/build-curl.sh \
    3ds/build-mbedtls.sh; do
    echo "==> running $script"
    bash "$script"
done

echo "==> dependencies finished. now building project"
make -j$(nproc)

echo "==> build finished. artifacts: moonlight.elf, moonlight.3dsx, etc."
