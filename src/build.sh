#!/bin/bash
set -e

SRC="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SRC")"
BIN="$ROOT/bin"
mkdir -p "$BIN"

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
    LLVM_LIBS=$($LLVM_CONFIG --link-static --libs all 2>/dev/null)
fi

# Build mioc
echo "Building mioc..."
# On Linux, GNU ld needs --start-group to resolve circular deps between
# LLVM/LLD static libraries. macOS linker handles this automatically.
if [ "$(uname -s)" = "Linux" ]; then
    WS="-Wl,--start-group"
    WE="-Wl,--end-group"
    GC="-Wl,--gc-sections"
else
    WS=""
    WE=""
    GC="-Wl,-dead_strip"
fi
"$CXX" -std=c++17 \
    -ffunction-sections -fdata-sections \
    -I"$INC" \
    -L"$LIB" \
    "$SRC/main.cpp" \
    -o "$BIN/mioc" \
    $WS \
    $LLVM_LIBS \
    -llldCommon -llldCOFF -llldELF -llldMachO \
    $WE \
    $GC \
    -lz -lzstd \
    $(pkg-config --libs libxml-2.0 2>/dev/null || echo "") \
    $(pkg-config --libs libzstd 2>/dev/null || echo "")

# Strip debug symbols to reduce binary size
echo "Stripping..."
strip "$BIN/mioc" 2>/dev/null || true

# Bundle LLD dylibs for portable distribution on macOS
if [ "$(uname -s)" = "Darwin" ]; then
	echo "Bundling LLD dylibs..."
	# Find where lld dylibs are
	for candidate in "$LLD_PREFIX/lib" "$LIB"; do
		if [ -f "$candidate/liblldCommon.dylib" ]; then
			LLD_DYLIB_DIR="$candidate"
			break
		fi
	done
	if [ -z "$LLD_DYLIB_DIR" ]; then
		echo "Warning: LLD dylibs not found, skipping bundle"
	else
		for lib in liblldCommon.dylib liblldCOFF.dylib liblldELF.dylib liblldMachO.dylib; do
			if [ -f "$LLD_DYLIB_DIR/$lib" ]; then
				cp "$LLD_DYLIB_DIR/$lib" "$BIN/"
				install_name_tool -id "@loader_path/$lib" "$BIN/$lib"
			fi
		done
		# Fix inter-dylib references
		for dylib in "$BIN"/liblld*.dylib; do
			otool -L "$dylib" | tail -n +2 | grep 'liblld.*dylib' | awk '{print $1}' | while read -r ref; do
				libname=$(basename "$ref")
				install_name_tool -change "$ref" "@loader_path/$libname" "$dylib"
			done
		done
		# Fix mioc references to LLD dylibs
		otool -L "$BIN/mioc" | tail -n +2 | grep 'liblld' | awk '{print $1}' | while read -r ref; do
			libname=$(basename "$ref")
			install_name_tool -change "$ref" "@loader_path/$libname" "$BIN/mioc"
		done
		echo "Bundled LLD dylibs into bin/"
	fi
fi

echo "Build successful: $BIN/mioc"