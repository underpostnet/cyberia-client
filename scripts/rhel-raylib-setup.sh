#!/bin/bash
# Setup raylib for Emscripten

# cd $HOME
cd lib

git clone https://github.com/raysan5/raylib.git

cd raylib

emcmake cmake . -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc)

emmake make install
