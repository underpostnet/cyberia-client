#!/bin/bash
# Set up Emscripten SDK

cd $HOME

cd .emsdk

./emsdk install latest
./emsdk activate latest

# Set up the environment for the current session
source ./emsdk_env.sh
