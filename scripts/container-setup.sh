#!/usr/bin/env bash
# Script to set up the Cyberia Client development environment inside a container.
# It handles repository cloning, secrets synchronization, toolchain installation,

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/container-setup.sh <environment> [-- additional args]
Example: scripts/container-setup.sh development
EOF
}

if [ $# -lt 1 ]; then
  usage
  exit 1
fi

ENVIRONMENT_INPUT="$1"
shift
ENVIRONMENT="${ENVIRONMENT_INPUT}"
ENVIRONMENT_SLUG="$(printf '%s' "$ENVIRONMENT" | tr '[:upper:]' '[:lower:]' | tr -cs '[:alnum:]-' '-')"

PROJECT_ROOT="${PROJECT_ROOT:-/home/dd/cyberia-client}"
PROJECT_PARENT="$(dirname "$PROJECT_ROOT")"
REPO_SLUG="${REPO_SLUG:-underpostnet/cyberia-client}"
SECRETS_FILE="${SECRETS_FILE:-/etc/config/.env.${ENVIRONMENT_SLUG}}"
EMSDK_ROOT="${EMSDK_ROOT:-$HOME/.emsdk}"
MAKE_JOBS="${MAKE_JOBS:-$(nproc)}"
CONTAINER_STATUS_VALUE="${CONTAINER_STATUS_VALUE:-dd-cyberia-mmo-client-${ENVIRONMENT_SLUG}-running-deployment}"

timestamp() {
  date -u +"%Y-%m-%dT%H:%M:%SZ"
}

log() {
  printf '[%s] %s\n' "$(timestamp)" "$*"
}

warn() {
  printf '[%s] WARN: %s\n' "$(timestamp)" "$*" >&2
}

section() {
  printf '\n[%s] === %s ===\n' "$(timestamp)" "$*"
}

run() {
  log "RUN $*"
  "$@"
}

ensure_repo() {
  if [ -d "$PROJECT_ROOT/.git" ]; then
    log "Repository already present at $PROJECT_ROOT"
    return
  fi

  section "Cloning repository $REPO_SLUG"
  if ! command -v underpost >/dev/null 2>&1; then
    log "ERROR: underpost CLI is required to clone $REPO_SLUG"
    exit 1
  fi

  run bash -c "cd \"$PROJECT_PARENT\" && underpost clone \"$REPO_SLUG\""
}

sync_secrets() {
  section "Syncing secrets for environment: $ENVIRONMENT_SLUG"
  if [ ! -f "$SECRETS_FILE" ]; then
    warn "Secrets file $SECRETS_FILE not found; skipping secret sync"
    return
  fi

  if command -v underpost >/dev/null 2>&1; then
    run underpost secret underpost --create-from-file "$SECRETS_FILE"
  else
    warn "underpost CLI not found; cannot sync secrets"
  fi
}

install_toolchain() {
  section "Installing host toolchain prerequisites"
  (
    cd "$PROJECT_ROOT"
    run chmod +x ./scripts/rhel-emscripten-setup.sh
    run ./scripts/rhel-emscripten-setup.sh
  )

  section "Installing and activating EMSDK"
  (
    cd "$PROJECT_ROOT"
    run chmod +x ./scripts/rhel-emscripten-env.sh
    run ./scripts/rhel-emscripten-env.sh
  )
}

ensure_emsdk_env() {
  section "Activating EMSDK environment for current shell"
  if [ ! -f "$EMSDK_ROOT/emsdk_env.sh" ]; then
    log "ERROR: EMSDK env script not found at $EMSDK_ROOT/emsdk_env.sh"
    exit 1
  fi

  # shellcheck disable=SC1090
  source "$EMSDK_ROOT/emsdk_env.sh"
  export PATH="$EMSDK_ROOT:$EMSDK_ROOT/upstream/emscripten:$PATH"

  if ! command -v emcmake >/dev/null 2>&1; then
    log "ERROR: emcmake is still missing after sourcing EMSDK"
    exit 1
  fi

  log "EMSDK activated (emcmake: $(command -v emcmake))"
}

build_raylib() {
  section "Building raylib (Web / Release)"
  (
    cd "$PROJECT_ROOT/lib/raylib"
    run emcmake cmake . -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release
    run emmake make -j"$MAKE_JOBS"
    run emmake make install
  )
}

build_src_c() {
  section "Building src_c artifacts"
  (
    cd "$PROJECT_ROOT/src_c"
    run make clean
    run make
  )
}

update_container_status() {
  section "Updating container status in underpost config"
  if command -v underpost >/dev/null 2>&1; then
    run underpost config set container-status "$CONTAINER_STATUS_VALUE"
  else
    warn "underpost CLI not found; skipping container status update"
  fi
}

start_server() {
  section "Launching development server"
  (
    cd "$PROJECT_ROOT/src_c"
    run make serve
  )
}

main() {
  section "Target environment: $ENVIRONMENT (slug: $ENVIRONMENT_SLUG)"
  sync_secrets
  ensure_repo
  install_toolchain
  ensure_emsdk_env
  build_raylib
  build_src_c
  update_container_status
  start_server
}

main "$@"
