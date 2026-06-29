#!/usr/bin/env python3
"""
Static HTTP server for the Cyberia WASM client.

Silent per-request logging.  Strong no-cache headers so browsers never serve
stale WASM/JS.

Usage
-----
  python3 docker-driver.py <port> [<directory>]

  port        TCP port
  directory   Directory to serve (default: current directory)

Container status reporting
--------------------------
  Set CONTAINER_DEPLOY_ID in the instance env file (e.g.
  "dd-cyberia-mmo-client-debug").  When set, each lifecycle transition is
  reported synchronously (blocks until `underpost config set` returns):
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
from http.server import HTTPServer, SimpleHTTPRequestHandler


def call_underpost(key: str, value: str) -> None:
    try:
        r = subprocess.run(
            ["underpost", "config", "set", key, value],
            capture_output=True,
            text=True,
        )
        if 0 != r.returncode:
            print(
                f"[status] underpost config set {key} {value} "
                f"exited {r.returncode}: {r.stderr.strip()}",
                flush=True,
            )
    except Exception as exc:
        print(f"[status] underpost config set {key} {value}: {exc}", flush=True)


def report_container_status(container_id: str, status: str) -> None:
    """Calls underpost CLI to report container status - Disabled if CONTAINER_DEPLOY_ID unset"""

    if container_id:
        value = "error" if status == "error" else f"{container_id}-{status}"

        call_underpost("container-status", value)
        if status == "running-deployment":
            call_underpost("start-container-status", value)


class CyberiaHandler(SimpleHTTPRequestHandler):
    """Suppresses per-request noise — suitable for container stdout."""

    def end_headers(self):
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()

    def log_message(self, fmt, *args):
        pass


if __name__ == "__main__":
    # port required; directory optional, defaults to CWD
    port = int(sys.argv[1])
    directory = sys.argv[2] if len(sys.argv) > 2 else "."
    container_id = os.environ.get("CONTAINER_DEPLOY_ID", "")

    os.chdir(directory)

    try:
        server = HTTPServer(("", port), CyberiaHandler)
    except Exception as exc:
        print(f"[server] bind failed on port {port}: {exc}", flush=True)
        report_container_status(container_id, "error")
        sys.exit(1)

    print(f"[server] http://{server.server_address[0]}:{server.server_address[1]}  dir={directory}", flush=True)
    report_container_status(container_id, "running-deployment")

    try:
        server.serve_forever()
    except Exception as exc:
        print(f"[server] serve_forever error: {exc}", flush=True)
        report_container_status(container_id, "error")
        sys.exit(1)
