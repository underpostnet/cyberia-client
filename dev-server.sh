#!/usr/bin/env bash
# dev-server.sh — Local development entry-point for the Cyberia WASM client.
#
# What it does
# ────────────
#  1. Verifies / installs all host packages needed by the toolchain
#     (RHEL/Fedora / Debian-Ubuntu families are both supported).
#  2. Installs or updates the Emscripten SDK (emsdk) when missing.
#  3. Activates the EMSDK environment for the current shell session.
#  4. Builds src/ with Web.mk in DEBUG mode.
#  5. Starts server.py on DEV_PORT in development mode.
#
# Usage
# ─────
#   ./dev-server.sh [port]          # default port = 8082
#
# Environment overrides
# ─────────────────────
#   EMSDK_ROOT      Path to emsdk checkout    (default: $HOME/.emsdk)
#   MAKE_JOBS       Parallel make jobs        (default: nproc)
#   DEV_PORT        HTTP port                 (default: 8082)

set -euo pipefail

# ── Helpers ────────────────────────────────────────────────────────────────────
ts()      { date -u +"%Y-%m-%dT%H:%M:%SZ"; }
log()     { printf '[%s] %s\n' "$(ts)" "$*"; }
section() { printf '\n[%s] ══ %s ══\n' "$(ts)" "$*"; }
have()    { command -v "$1" >/dev/null 2>&1; }

# ── Config ─────────────────────────────────────────────────────────────────────
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EMSDK_ROOT="${EMSDK_ROOT:-$HOME/.emsdk}"
MAKE_JOBS="${MAKE_JOBS:-$(nproc)}"
DEV_PORT="${1:-${DEV_PORT:-8082}}"
export EMSDK_QUIET=1

# ── 1. Host packages ───────────────────────────────────────────────────────────
section "Checking host packages"

RHEL_PKGS=(
  epel-release cmake make gcc-c++ pkgconfig git wget unzip
  python3 python3.11
  alsa-lib-devel mesa-libGL-devel mesa-libGLU-devel
  libX11-devel libXrandr-devel libXi-devel libXcursor-devel
  libXinerama-devel libXfixes-devel freeglut-devel glfw-devel
  libatomic
)
DEB_PKGS=(
  build-essential cmake git wget unzip
  python3 python3.11
  libasound2-dev libgl1-mesa-dev libglu1-mesa-dev
  libx11-dev libxrandr-dev libxi-dev libxcursor-dev
  libxinerama-dev libxfixes-dev freeglut3-dev libglfw3-dev
  libatomic1
)

install_packages_rhel() {
  local missing=()
  for pkg in "${RHEL_PKGS[@]}"; do
    rpm -q "${pkg}" >/dev/null 2>&1 || missing+=("${pkg}")
  done
  if [[ ${#missing[@]} -gt 0 ]]; then
    log "Installing missing RHEL packages: ${missing[*]}"
    sudo dnf install -y "${missing[@]}"
  else
    log "All RHEL packages already installed."
  fi
}

install_packages_deb() {
  local missing=()
  for pkg in "${DEB_PKGS[@]}"; do
    dpkg -s "${pkg}" >/dev/null 2>&1 || missing+=("${pkg}")
  done
  if [[ ${#missing[@]} -gt 0 ]]; then
    log "Installing missing Debian packages: ${missing[*]}"
    sudo apt-get update -qq
    sudo apt-get install -y "${missing[@]}"
  else
    log "All Debian packages already installed."
  fi
}

if have dnf; then
  install_packages_rhel
elif have apt-get; then
  install_packages_deb
else
  log "WARN: Unknown package manager — skipping package check."
fi

# ── 2. git-subrepo (needed if libs/ subrepos are not yet present) ──────────────
if ! have git-subrepo && [[ ! -f "$HOME/.local/git-subrepo/.rc" ]]; then
  section "Installing git-subrepo"
  git clone --depth=1 https://github.com/ingydotnet/git-subrepo "$HOME/.local/git-subrepo"
  # shellcheck disable=SC1090
  source "$HOME/.local/git-subrepo/.rc"
elif [[ -f "$HOME/.local/git-subrepo/.rc" ]]; then
  # shellcheck disable=SC1090
  source "$HOME/.local/git-subrepo/.rc"
fi

# ── 3. Emscripten SDK ──────────────────────────────────────────────────────────
section "Checking Emscripten SDK"

if [[ ! -d "$EMSDK_ROOT" ]]; then
  log "Cloning emsdk into $EMSDK_ROOT"
  git clone --depth=1 https://github.com/emscripten-core/emsdk.git "$EMSDK_ROOT"
fi

cd "$EMSDK_ROOT"
./emsdk install latest
./emsdk activate latest
# shellcheck disable=SC1090,SC1091
source ./emsdk_env.sh
cd "$PROJECT_ROOT"

# Re-pin EMSDK_PYTHON after emsdk_env.sh clears it
PYTHON_BIN="$(command -v python3.11 2>/dev/null || command -v python3 2>/dev/null || true)"
if [[ -z "$PYTHON_BIN" ]]; then
  log "ERROR: python3 not found after package install."
  exit 1
fi
export EMSDK_PYTHON="$PYTHON_BIN"
export PATH="$EMSDK_ROOT:$EMSDK_ROOT/upstream/emscripten:$PATH"

log "EMSDK activated — emcc: $(command -v emcc)"

# ── 4. Build ───────────────────────────────────────────────────────────────────
section "Building Cyberia Client (DEBUG)"
cd "$PROJECT_ROOT"
make -j"$MAKE_JOBS" -f Web.mk all BUILD_MODE=DEBUG

# ── 5. Serve ───────────────────────────────────────────────────────────────────
section "Starting development server on port $DEV_PORT"
exec python3 "$PROJECT_ROOT/server.py" "$DEV_PORT" "$PROJECT_ROOT/bin/web/debug" development
