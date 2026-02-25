#!/usr/bin/env bash
# Script to set up the Cyberia Client development environment inside a container.
# It handles repository cloning, secrets synchronization, toolchain installation,
# and building the project using the new Web.mk build system.

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

# Check for double dash to separate additional args
if [ "$#" -gt 0 ] && [ "$1" == "--" ]; then
  shift
fi

# Determine Build Mode based on Environment
if [[ "$ENVIRONMENT_SLUG" == "production" ]]; then
  BUILD_MODE="RELEASE"
else
  BUILD_MODE="DEBUG"
fi

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
    if [ -f "./scripts/rhel-emscripten-setup.sh" ]; then
        run chmod +x ./scripts/rhel-emscripten-setup.sh
        run ./scripts/rhel-emscripten-setup.sh
    else
        warn "Toolchain setup script not found at ./scripts/rhel-emscripten-setup.sh"
    fi
  )

  section "Installing and activating EMSDK"
  (
    cd "$PROJECT_ROOT"
    if [ -f "./scripts/rhel-emscripten-env.sh" ]; then
        run chmod +x ./scripts/rhel-emscripten-env.sh
        run ./scripts/rhel-emscripten-env.sh
    else
        warn "EMSDK env setup script not found at ./scripts/rhel-emscripten-env.sh"
    fi
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

  # emsdk activate clears EMSDK_PYTHON; re-export it so emcc uses Python 3.11+
  local python_bin=""
  if command -v python3.11 >/dev/null 2>&1; then
    python_bin="$(command -v python3.11)"
  elif command -v python3 >/dev/null 2>&1; then
    python_bin="$(command -v python3)"
  fi

  # If no python3 found, attempt to install it
  if [ -z "$python_bin" ]; then
    warn "python3 not found; attempting to install via dnf"
    if command -v dnf >/dev/null 2>&1; then
      dnf install -y python3 || true
    elif command -v yum >/dev/null 2>&1; then
      yum install -y python3 || true
    elif command -v apt-get >/dev/null 2>&1; then
      apt-get update && apt-get install -y python3 || true
    fi

    # Re-check after install attempt
    if command -v python3.11 >/dev/null 2>&1; then
      python_bin="$(command -v python3.11)"
    elif command -v python3 >/dev/null 2>&1; then
      python_bin="$(command -v python3)"
    fi
  fi

  if [ -n "$python_bin" ]; then
    export EMSDK_PYTHON="$python_bin"
    log "Set EMSDK_PYTHON=$EMSDK_PYTHON"
  else
    log "ERROR: python3.11 or python3 not found and could not be installed"
    exit 1
  fi

  if ! command -v emcc >/dev/null 2>&1; then
    log "ERROR: emcc is still missing after sourcing EMSDK"
    exit 1
  fi

  log "EMSDK activated (emcc: $(command -v emcc))"
}

build_raylib() {
  section "Building Raylib (Web / $BUILD_MODE)"
  # The Web.mk file handles building raylib, but we can explicitly invoke the target
  # to ensure it's built before the main application.
  (
    cd "$PROJECT_ROOT"
    run make -f Web.mk libraylib PLATFORM=PLATFORM_WEB BUILD_MODE="$BUILD_MODE"
  )
}

build_client() {
  section "Building Cyberia Client (Web / $BUILD_MODE)"
  (
    cd "$PROJECT_ROOT"
    run make -f Web.mk clean
    run make -f Web.mk web BUILD_MODE="$BUILD_MODE"
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
  section "Launching server ($ENVIRONMENT_SLUG)"

  # Parse arguments to handle space-separated variables (e.g., "PORT 8080" -> "PORT=8080")
  local make_args=()
  while [[ $# -gt 0 ]]; do
    local key="$1"
    shift
    # Heuristic: If arg is all uppercase/digits/underscores and next arg exists, treat as VAR=VAL
    if [[ "$key" =~ ^[A-Z0-9_]+$ ]] && [[ $# -gt 0 ]] && [[ "$1" != *=* ]]; then
      make_args+=("$key=$1")
      shift
    else
      make_args+=("$key")
    fi
  done

  (
    cd "$PROJECT_ROOT"
    if [[ "$ENVIRONMENT_SLUG" == "production" ]]; then
      run make -f Web.mk serve_production "${make_args[@]}"
    else
      run make -f Web.mk serve_development BUILD_MODE="$BUILD_MODE" "${make_args[@]}"
    fi
  )
}

main() {
  section "Target environment: $ENVIRONMENT (slug: $ENVIRONMENT_SLUG)"
  sync_secrets
  ensure_repo
  install_toolchain
  ensure_emsdk_env
  build_raylib
  build_client
  update_container_status
  start_server "$@"
}

main "$@"
