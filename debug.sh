#!/bin/bash

# simple wrapper to launch the ARM GDB with the built moonlight ELF
# optionally connects to a remote gdbserver if host:port is given

GDB=arm-none-eabi-gdb
ELF="${1:-moonlight.elf}"

if ! command -v "$GDB" >/dev/null 2>&1; then
    echo "error: $GDB not found in PATH" >&2
    exit 1
fi

if [ ! -f "$ELF" ]; then
    echo "error: ELF '$ELF' not found; build the project first" >&2
    exit 1
fi

TARGET=""
if [ $# -gt 1 ]; then
    TARGET="$2"
fi

if [ -n "$TARGET" ]; then
    echo "Starting gdb ($GDB) on $ELF and connecting to $TARGET..."
    exec "$GDB" "$ELF" -ex "target remote $TARGET"
else
    echo "Starting gdb ($GDB) on $ELF"    
    exec "$GDB" "$ELF"
fi
