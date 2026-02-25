#!/bin/bash
set -e

# Suppress verbose EMSDK environment messages
export EMSDK_QUIET=1

EMSDK_DIR="$HOME/.emsdk"

if [ ! -d "$EMSDK_DIR" ]; then
    echo "[ERROR] EMSDK directory not found: $EMSDK_DIR" >&2
    exit 1
fi

cd "$EMSDK_DIR"

./emsdk install latest
./emsdk activate latest

# Set up the environment for the current session
source ./emsdk_env.sh

# Use Python 3.11+ (required by latest Emscripten SDK)
# This must be set AFTER emsdk activate and source emsdk_env.sh,
# because both clear the EMSDK_PYTHON environment variable.
PYTHON_BIN=$(which python3.11 2>/dev/null || which python3 2>/dev/null || true)

if [ -z "$PYTHON_BIN" ]; then
    echo "[ERROR] python3.11 or python3 not found in PATH" >&2
    exit 1
fi

export EMSDK_PYTHON="$PYTHON_BIN"
