#!/bin/bash
# Set up Emscripten SDK

cd $HOME

cd .emsdk

# Use Python 3.11+ (required by latest Emscripten SDK)
export EMSDK_PYTHON=$(which python3.11)

./emsdk install latest
./emsdk activate latest

# Set up the environment for the current session
source ./emsdk_env.sh
