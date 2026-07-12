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
    # Remove optional libraries that may not be installed (Polly, etc.)
    LLVM_LIBS=$(echo "$LLVM_LIBS" | tr ' ' '\n' | grep -v -E '^-lPolly(ISL)?$' | tr '\n' ' ')
fi

# Build libxml2 stub
echo "Building libxml2 stub..."
"$CC" -c "$SRC/libxml2_stub.c" -o "$SRC/libxml2_stub.o"
ar rcs "$SRC/libxml2_stub.a" "$SRC/libxml2_stub.o"
rm -f "$SRC/libxml2_stub.o"

# Build Polly stub (always, in case libLLVMPolly.a doesn't contain getPollyPluginInfo)
cat >"$SRC/polly_stub.cpp" <<'EOF'
extern "C" void* getPollyPluginInfo(){return nullptr;}
EOF
"$CXX" -std=c++17 -c "$SRC/polly_stub.cpp" -o "$SRC/polly_stub.o"

# Build mioc
echo "Building mioc..."
# On Linux, GNU ld needs --start-group to resolve circular deps between
# LLVM/LLD static libraries. macOS linker handles this automatically.
if [ "$(uname -s)" = "Linux" ]; then
    WS="-Wl,--start-group"
    WE="-Wl,--end-group"
else
    WS=""
    WE=""
fi
"$CXX" -std=c++17 \
    -I"$INC" \
    -L"$LIB" \
    "$SRC/main.cpp" \
    "$SRC/polly_stub.o" \
    -o "$SRC/mioc" \
    $WS \
    $LLVM_LIBS \
    -llldCommon -llldCOFF -llldELF -llldMachO \
    "$SRC/libxml2_stub.a" \
    $WE \
    -lz -lzstd \
    $(pkg-config --libs libxml-2.0 2>/dev/null || echo "")

# Cleanup
rm -f "$SRC/libxml2_stub.a" "$SRC/polly_stub.cpp" "$SRC/polly_stub.o"

echo "Build successful: $SRC/mioc"