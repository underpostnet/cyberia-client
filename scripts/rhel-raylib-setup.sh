#!/bin/bash
# Setup raylib for Emscripten

LIB_DIR=lib/raylib

mkdir -p $LIB_DIR

git subrepo clone https://github.com/raysan5/raylib.git $LIB_DIR

cd $LIB_DIR

emcmake cmake . -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc)

emmake make install
