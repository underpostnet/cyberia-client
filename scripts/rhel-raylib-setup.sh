#!/bin/bash
# Setup raylib for Emscripten

# Use Python 3.11+ (required by latest Emscripten SDK)
if command -v python3.11 &>/dev/null; then
  export EMSDK_PYTHON=$(which python3.11)
fi

LIB_DIR=lib/raylib

if [ -d "$LIB_DIR" ] && [ "$(ls -A $LIB_DIR)" ]; then
  echo "Directory '$LIB_DIR' already exists, skipping clone..."
else
  mkdir -p $LIB_DIR
  git subrepo clone https://github.com/raysan5/raylib.git $LIB_DIR
fi

BUILD_DIR="$LIB_DIR/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

emcmake cmake .. \
  -DPLATFORM=Web \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_GAMES=OFF

emmake make -j$(nproc)

emmake make install
