#!/usr/bin/env python3
"""
Static HTTP server for the Cyberia WASM client.

Modes
-----
  production (default)
      Silent per-request logging.
      Strong no-cache headers so browsers never serve stale WASM/JS.

  development
      Verbose per-request logging to stdout.
      Same no-cache headers (needed during iteration).

Usage
-----
  python3 server.py <port> <directory> [development|production]

Arguments
---------
  port        TCP port to listen on.
  directory   Directory to serve files from.
  mode        "development" or "production" (default: production).

Container status reporting
--------------------------
  Set CONTAINER_DEPLOY_ID in the instance env file (e.g.
  "dd-cyberia-mmo-client-development").  When set:
    - after a successful socket bind: underpost config set container-status
      <CONTAINER_DEPLOY_ID>-running-deployment
    - on any startup error:           underpost config set container-status error
  The call is fire-and-forget (background thread) and never blocks serving.
  If CONTAINER_DEPLOY_ID is unset the feature is silently disabled.
"""

import sys
import os
import subprocess
import threading
from http.server import HTTPServer, SimpleHTTPRequestHandler


def _run_underpost_status(status: str) -> None:
    """Call `underpost config set container-status <value>` in a background thread.

    value is either "error" or CONTAINER_DEPLOY_ID+"-"+status.
    Errors are only printed; they never propagate.
    """
    container_id = os.environ.get("CONTAINER_DEPLOY_ID", "")
    if not container_id:
        return
    value = "error" if status == "error" else f"{container_id}-{status}"

    def _run():
        try:
            result = subprocess.run(
                ["underpost", "config", "set", "container-status", value],
                capture_output=True,
                text=True,
            )
            if result.returncode != 0:
                print(
                    f"[status] underpost config set container-status {value} "
                    f"exited {result.returncode}: {result.stderr.strip()}",
                    flush=True,
                )
        except Exception as exc:
            print(f"[status] underpost config set container-status {value}: {exc}", flush=True)

    threading.Thread(target=_run, daemon=True).start()


class _BaseHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()


class ProductionHandler(_BaseHandler):
    """Suppresses per-request noise — suitable for container stdout."""
    def log_message(self, fmt, *args):
        pass


class DevelopmentHandler(_BaseHandler):
    """Full per-request logging — useful when iterating locally."""
    pass  # log_message default prints to stderr


if __name__ == "__main__":
    port      = int(sys.argv[1])            if len(sys.argv) > 1 else 8082
    directory = sys.argv[2]                 if len(sys.argv) > 2 else "."
    mode      = (sys.argv[3] or "").lower() if len(sys.argv) > 3 else "production"

    handler = DevelopmentHandler if mode == "development" else ProductionHandler

    os.chdir(directory)
    try:
        server = HTTPServer(("", port), handler)
    except Exception as exc:
        print(f"[server] bind failed on port {port}: {exc}", flush=True)
        _run_underpost_status("error")
        sys.exit(1)

    print(f"[server] http://0.0.0.0:{port}  mode={mode}  dir={directory}", flush=True)
    _run_underpost_status("running-deployment")

    try:
        server.serve_forever()
    except Exception as exc:
        print(f"[server] serve_forever error: {exc}", flush=True)
        _run_underpost_status("error")
        sys.exit(1)

