#!/bin/bash
# Set up Emscripten SDK

cd $HOME

cd .emsdk

./emsdk install latest
./emsdk activate latest

# Set up the environment for the current session
source ./emsdk_env.sh

# Use Python 3.11+ (required by latest Emscripten SDK)
# This must be set AFTER emsdk activate and source emsdk_env.sh,
# because both clear the EMSDK_PYTHON environment variable.
export EMSDK_PYTHON=$(which python3.11)
