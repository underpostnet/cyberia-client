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
  python3 server.py <port> <directory> [development|production] [ready_cmd]

Arguments
---------
  port        TCP port to listen on.
  directory   Directory to serve files from.
  mode        "development" or "production" (default: production).
  ready_cmd   Optional shell command executed once the server is listening.
              Runs in a background thread so serving starts immediately.
              Typical use: signal a container-status to the orchestrator.

Examples
--------
  python3 server.py 8081 bin/web/release
  python3 server.py 8082 bin/web/debug development
  python3 server.py 8081 bin/web/release production \\
      "underpost config set container-status my-deployment-running"
"""

import sys
import os
import subprocess
import threading
from http.server import HTTPServer, SimpleHTTPRequestHandler


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


def _run_ready_cmd(cmd: str) -> None:
    """Execute *cmd* via the shell and print its exit code."""
    try:
        result = subprocess.run(cmd, shell=True, check=False)
        print(f"[server] ready_cmd exited {result.returncode}: {cmd}", flush=True)
    except Exception as exc:
        print(f"[server] ready_cmd error: {exc}", flush=True)


def main():
    port      = int(sys.argv[1])            if len(sys.argv) > 1 else 8082
    directory = sys.argv[2]                 if len(sys.argv) > 2 else "."
    mode      = (sys.argv[3] or "").lower() if len(sys.argv) > 3 else "production"
    ready_cmd = sys.argv[4]                 if len(sys.argv) > 4 else None

    handler = DevelopmentHandler if mode == "development" else ProductionHandler

    os.chdir(directory)
    server = HTTPServer(("", port), handler)
    print(f"[server] http://0.0.0.0:{port}  mode={mode}  dir={directory}", flush=True)

    if ready_cmd:
        threading.Thread(target=_run_ready_cmd, args=(ready_cmd,), daemon=True).start()

    server.serve_forever()


if __name__ == "__main__":
    main()
