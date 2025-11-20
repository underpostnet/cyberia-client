#!/bin/bash

set -e

cd /home/dd/cyberia-client/test

# ===============================
# 1. Configure EMSDK paths
# ===============================
EMSDK="$HOME/.emsdk"
INCLUDE="$EMSDK/upstream/emscripten/cache/sysroot/include"
LIB="$EMSDK/upstream/emscripten/cache/sysroot/lib/libraylib.a"
SRC_FILE="emscripten.test.c"

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
echo ""
echo "Run with:  emrun --no_browser --port 8080 ."
