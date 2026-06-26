#!/usr/bin/env python3
"""
Static HTTP server for the Cyberia WASM client.

Modes
-----
  RELEASE (default)
      Silent per-request logging.
      Strong no-cache headers so browsers never serve stale WASM/JS.

  DEBUG
      Verbose per-request logging to stdout.
      Rebuilds the WASM bundle from source before serving.
      Same no-cache headers (needed during iteration).

Usage
-----
  python3 server.py [RELEASE|DEBUG] [<port>] [<directory>]

  mode        "RELEASE" or "DEBUG" (default: RELEASE).
  port        TCP port (default: 8081 for RELEASE, 8082 for DEBUG).
  directory   Directory to serve (default: bin).

Examples
--------
  python3 server.py              # RELEASE, port 8081, dir bin
  python3 server.py DEBUG        # DEBUG, port 8082, dir bin
  python3 server.py DEBUG 9090   # DEBUG, port 9090, dir bin
  python3 server.py RELEASE 8080 # RELEASE, port 8080, dir bin

Container status reporting
--------------------------
  Set CONTAINER_DEPLOY_ID in the instance env file (e.g.
  "dd-cyberia-mmo-client-debug").  When set, each lifecycle transition is
  reported fire-and-forget (background thread) and never blocks serving:
    - container-status (runtime-status): set on EVERY transition, success or
      error. The deploy monitor reads it to detect failure.
        success: underpost config set container-status <CONTAINER_DEPLOY_ID>-running-deployment
        error:   underpost config set container-status error
    - start-container-status: the insulated readiness marker, set ONLY after a
      successful bind (<CONTAINER_DEPLOY_ID>-running-deployment) — never on
      error, so a later fault can't clear pod readiness.
  If CONTAINER_DEPLOY_ID is unset the feature is silently disabled.
"""

import sys
import os
import subprocess
import threading
from http.server import HTTPServer, SimpleHTTPRequestHandler


def _underpost_config_set(key: str, value: str) -> None:
    """Fire-and-forget `underpost config set <key> <value>` in a background thread.

    Errors are only printed; they never propagate.
    """

    def _run():
        try:
            result = subprocess.run(
                ["underpost", "config", "set", key, value],
                capture_output=True,
                text=True,
            )
            if result.returncode != 0:
                print(
                    f"[status] underpost config set {key} {value} "
                    f"exited {result.returncode}: {result.stderr.strip()}",
                    flush=True,
                )
        except Exception as exc:
            print(f"[status] underpost config set {key} {value}: {exc}", flush=True)

    threading.Thread(target=_run, daemon=True).start()


def _report_container_status(status: str) -> None:
    """Publish a lifecycle transition for this container.

    container-status is the dynamic runtime-status, set on every transition
    (success or error) so the deploy monitor can detect failure.
    start-container-status is the insulated readiness marker, set only once the
    server is running, so a later fault can never clear pod readiness.

    Silently disabled when CONTAINER_DEPLOY_ID is unset.
    """
    container_id = os.environ.get("CONTAINER_DEPLOY_ID", "")
    if not container_id:
        return
    value = "error" if status == "error" else f"{container_id}-{status}"
    _underpost_config_set("container-status", value)
    if status == "running-deployment":
        _underpost_config_set("start-container-status", value)


class _BaseHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()


class ReleaseHandler(_BaseHandler):
    """Suppresses per-request noise — suitable for container stdout."""
    def log_message(self, fmt, *args):
        pass


class DebugHandler(_BaseHandler):
    """Full per-request logging — useful when iterating locally."""
    pass  # log_message default prints to stderr


def _run_dev_build(build_mode: str) -> None:
    """Run the WASM build in the cyberia-client project root.

    Determines the project root by walking up from the current working
    directory until it finds ``Web.mk``, then executes::

        make -f Web.mk clean all BUILD_MODE=<build_mode> OUTPUT_DIR=bin/

    Exits the process on failure.
    """
    # Walk up from the serving directory to find the project root (where Web.mk lives).
    project_root = os.path.abspath(".")
    while not os.path.isfile(os.path.join(project_root, "Web.mk")):
        parent = os.path.dirname(project_root)
        if parent == project_root:
            print(
                "[build] Could not find Web.mk in any parent directory. "
                "Skipping dev build.",
                flush=True,
            )
            return
        project_root = parent

    print(f"[build] Rebuilding client (BUILD_MODE={build_mode}) in {project_root} ...", flush=True)

    # emscripten/emcc requires Python >= 3.10.  The system default python3 is
    # often 3.9 on older distros, but python3.11 is usually available.
    # The emcc shell script resolves python via $EMSDK_PYTHON first (line 18).
    build_env = os.environ.copy()
    for candidate in ("/usr/bin/python3.11", "/usr/bin/python3.10"):
        if os.path.isfile(candidate):
            build_env["EMSDK_PYTHON"] = candidate
            break

    result = subprocess.run(
        [
            "make", "-f", "Web.mk",
            "clean", "all",
            f"BUILD_MODE={build_mode}",
            "OUTPUT_DIR=bin/",
        ],
        cwd=project_root,
        env=build_env,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f"[build] FAILED (exit {result.returncode}):", flush=True)
        print(result.stdout, flush=True)
        print(result.stderr, flush=True)
        sys.exit(1)
    print("[build] Done.", flush=True)


def _parse_args() -> tuple:
    """Parse CLI arguments: [mode] [port] [directory].

    Returns (mode, port, directory).
    """
    mode = "RELEASE"
    port = None
    directory = "bin"

    # Collect all positional args
    args = sys.argv[1:]

    # First arg can be RELEASE/DEBUG
    if args and args[0].upper() in ("RELEASE", "DEBUG"):
        mode = args[0].upper()
        args = args[1:]

    # Second positional arg is port (if numeric)
    if args:
        try:
            port = int(args[0])
            args = args[1:]
        except ValueError:
            pass

    # Third positional arg is directory
    if args:
        directory = args[0]

    # Default port based on mode
    if port is None:
        port = 8082 if mode == "DEBUG" else 8081

    return mode, port, directory


if __name__ == "__main__":
    mode, port, directory = _parse_args()

    handler = DebugHandler if mode == "DEBUG" else ReleaseHandler

    # In DEBUG mode, rebuild the WASM bundle before serving.
    if mode == "DEBUG":
        _run_dev_build("DEBUG")

    os.chdir(directory)
    try:
        server = HTTPServer(("", port), handler)
    except Exception as exc:
        print(f"[server] bind failed on port {port}: {exc}", flush=True)
        _report_container_status("error")
        sys.exit(1)

    print(f"[server] http://0.0.0.0:{port}  mode={mode}  dir={directory}", flush=True)
    _report_container_status("running-deployment")

    try:
        server.serve_forever()
    except Exception as exc:
        print(f"[server] serve_forever error: {exc}", flush=True)
        _report_container_status("error")
        sys.exit(1)