#!/bin/bash

set -e

# ===============================
# 1. Configure EMSDK paths
# ===============================
EMSDK="$HOME/.emsdk"
INCLUDE="$EMSDK/upstream/emscripten/cache/sysroot/include"
LIB="$EMSDK/upstream/emscripten/cache/sysroot/lib/libraylib.a"
SRC_FILE="emscripten.raylib.c"
SRC_PATH="/home/dd/cyberia-client/test/raylib"

cd $SRC_PATH

echo "[INFO] Using EMSDK at: $EMSDK"

# ===============================
# 2. Download shell.html if missing
# ===============================
if [ ! -f shell.html ]; then
    echo "[INFO] Downloading shell.html..."
    wget -O shell.html https://raw.githubusercontent.com/raysan5/raylib/master/src/shell.html
else
    echo "[INFO] shell.html already exists."
fi

# ===============================
# 3. Compile into WebAssembly
# ===============================
echo "[INFO] Compiling $SRC_FILE -> $SRC_FILE.html (WebAssembly)..."

emcc -o $SRC_FILE.html $SRC_FILE \
    -Os \
    -I "$INCLUDE" \
    "$LIB" \
    -s USE_GLFW=3 \
    --shell-file shell.html \
    -DPLATFORM_WEB

echo ""
echo "====================================="
echo "   Build completed successfully!"
echo "   Files generated:"
echo "     • $SRC_FILE.html"
echo "     • $SRC_FILE.wasm"
echo "     • $SRC_FILE.js"
echo "====================================="

echo "Run with:  emrun --no_browser --port 8081 $SRC_PATH"
echo "Command copied to clipboard."

printf "emrun --no_browser --port 8081 $SRC_PATH" | xsel --clipboard --input
