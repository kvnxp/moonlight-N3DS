#!/usr/bin/env bash

# build.sh - helper to configure the MSYS2/devkitPro environment and
# perform a project build.  It is intended to be run from the workspace
# root (the same directory as this script).
#
# This script exports the usual 3DS toolchain variables, ensures the
# bannertool directory is on PATH, prints out the relevant environment
# state and invokes make.  You can edit or extend the exports below if
# you need additional customisation.

set -e

# ------------------------------------------------------------------
# 1. prepare devkitPro & MSYS2 environment
# ------------------------------------------------------------------

# if running under the default MSYS shell, the mingw64 binaries aren't on
# the path; add them if they exist so tools like ffmpeg can be used.
if [ -d "/mingw64/bin" ]; then
    export PATH="/mingw64/bin:$PATH"
fi

if [ -f /etc/profile.d/devkit-env.sh ]; then
    # this script is provided by devkitPro and sets DEVKITPRO/DEVKITARM
    # and adds the toolchain to PATH; it also exports PORTLIBS etc.
    source /etc/profile.d/devkit-env.sh
else
    echo "[warning] /etc/profile.d/devkit-env.sh not found."
    echo "  Make sure DEVKITPRO and DEVKITARM are set in your shell."
fi

# ------------------------------------------------------------------
# 2. add bannertool (and other helper tools) to PATH if available
# ------------------------------------------------------------------

# MSYS2 installation path - adjust if your devkitPro is located elsewhere
if [ -d "/msys64/opt/devkitpro/tools/bin" ]; then
    export PATH="$PATH:/msys64/opt/devkitpro/tools/bin"
fi

# ------------------------------------------------------------------
# 3. optional: additional exports (uncomment / modify as needed)
# ------------------------------------------------------------------

# export PKG_CONFIG_PATH="$PORTLIBS/3ds/lib/pkgconfig:$PKG_CONFIG_PATH"
# export CFLAGS="-g -O2 -Wall"
# export CXXFLAGS="$CFLAGS -fno-rtti -fexceptions -std=gnu++17"

# ------------------------------------------------------------------
# 4. show key environment values for debugging
# ------------------------------------------------------------------

echo "===> build environment"
echo "DEVKITPRO=$DEVKITPRO"
echo "DEVKITARM=$DEVKITARM"
echo "PORTLIBS=$PORTLIBS"
echo "PATH contains bannertool?" $(command -v bannertool || echo "no")
echo "PATH contains makerom?" $(command -v makerom || echo "no")
echo

# ------------------------------------------------------------------
# 5. sanity checks
# ------------------------------------------------------------------

# builder tools required by the Makefile
if ! command -v makerom >/dev/null 2>&1; then
    cat <<'MSG'
[error] makerom not found in PATH.
You need the 3ds-tools package from devkitPro which provides makerom,
2dstool, etc.  Install it via MSYS2:  pacman -S 3ds-tools
or using the devkitPro package scripts, and re-open your shell.
MSG
    exit 1
fi

# host-side utilities used during asset generation
if ! command -v ffmpeg >/dev/null 2>&1; then
    cat <<'MSG'
[error] host ffmpeg not found.
The makefile uses ffmpeg to convert PNG graphics to BGR format.  Install
it in MSYS2 with: pacman -S mingw-w64-x86_64-ffmpeg
MSG
    exit 1
fi

# ------------------------------------------------------------------
# 6. actually build
# ------------------------------------------------------------------

cd "$(dirname "$0")"

# clean previous output to avoid stale objects
make clean

# parallel build using all CPUs
make -j$(nproc)

echo "===> build completed. check the build/ directory for artifacts."

# if the 3dslink client is available, automatically upload the generated
# 3dsx to a connected console.  3dslink binaries are usually installed
# alongside the toolchain or provided by `pacman -S 3dslink`.
read -p "Press any key to upload... "
if command -v 3dslink >/dev/null 2>&1; then
    echo "===> uploading moonlight.3dsx via 3dslink (-a)"
    3dslink -a 192.168.1.241 moonlight.3dsx || echo "[warning] 3dslink returned non-zero"
else
    echo "===> 3dslink not found; skipping automatic upload"
fi
