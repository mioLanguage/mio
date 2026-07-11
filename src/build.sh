#!/bin/bash
set -e

SRC="$(cd "$(dirname "$0")" && pwd)"

# LLVM_DIR can be set via environment variable or found automatically
if [ -z "$LLVM_DIR" ]; then
    # Try common LLVM installation paths
    for candidate in \
        "$SRC/llvm-project" \
        /usr/lib/llvm-22 \
        /usr/lib/llvm-21 \
        /usr/lib/llvm-20 \
        /usr/lib/llvm-19 \
        /usr/lib/llvm-18 \
        /usr/local/llvm; do
        if [ -f "$candidate/bin/llvm-config" ]; then
            LLVM_DIR="$candidate"
            break
        fi
    done
fi

if [ -z "$LLVM_DIR" ] || [ ! -f "$LLVM_DIR/bin/llvm-config" ]; then
    echo "Error: LLVM not found."
    echo "Set LLVM_DIR environment variable, e.g.:"
    echo "  export LLVM_DIR=/usr/lib/llvm-22"
    exit 1
fi

echo "Using LLVM at: $LLVM_DIR"

CXX="$LLVM_DIR/bin/clang++"
CC="$LLVM_DIR/bin/clang"
INC="$LLVM_DIR/include"
LIB="$LLVM_DIR/lib"

# Check for llvm-config
LLVM_CONFIG="$LLVM_DIR/bin/llvm-config"
if [ ! -f "$LLVM_CONFIG" ]; then
    echo "Warning: llvm-config not found, using static library list"
    LLVM_LIBS="-lLLVMCore -lLLVMSupport -lLLVMTargetParser -lLLVMBinaryFormat -lLLVMRemarks"
else
    LLVM_LIBS=$($LLVM_CONFIG --libs all --link-static 2>/dev/null || $LLVM_CONFIG --libs all)
fi

# Build libxml2 stub
echo "Building libxml2 stub..."
"$CC" -c "$SRC/libxml2_stub.c" -o "$SRC/libxml2_stub.o"
ar rcs "$SRC/libxml2_stub.a" "$SRC/libxml2_stub.o"
rm -f "$SRC/libxml2_stub.o"

# Build mioc
echo "Building mioc..."
"$CXX" -std=c++17 \
    -I"$INC" \
    -L"$LIB" \
    "$SRC/main.cpp" \
    -o "$SRC/mioc" \
    $LLVM_LIBS \
    -llldCommon -llldCOFF -llldELF -llldMachO \
    "$SRC/libxml2_stub.a" \
    $(pkg-config --libs libxml-2.0 2>/dev/null || echo "")

# Cleanup
rm -f "$SRC/libxml2_stub.a"

echo "Build successful: $SRC/mioc"