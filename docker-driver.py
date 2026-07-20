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

Multi-instance routing
----------------------
  One image serves every world instance; the instance is the first path
  segment (/FOREST/, /TEST/, ...).  The bundle lives flat in <directory>, so
  the leading instance segment is stripped before the file lookup and the same
  index.html/wasm is served under every instance path.

  The browser keeps the original URL, which is what the client reads to resolve
  its instance.  These env vars parameterise that resolution and are injected
  into index.html as window.* globals:

    CYBERIA_WS_ORIGIN          websocket origin, e.g. wss://server.cyberiaonline.com
    CYBERIA_ENGINE_API_ORIGIN  engine REST origin, e.g. https://www.cyberiaonline.com
    CYBERIA_DEFAULT_INSTANCE   instance used when served from the root path

  Unset vars are simply not injected and the client falls back to the values
  compiled into the WASM.

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
import io
import json
import posixpath
import subprocess
from http.server import HTTPServer, SimpleHTTPRequestHandler

RUNTIME_CONFIG_ENV_KEYS = (
    "CYBERIA_WS_ORIGIN",
    "CYBERIA_ENGINE_API_ORIGIN",
    "CYBERIA_DEFAULT_INSTANCE",
)


def runtime_config_script() -> bytes:
    """<script> assigning the configured env vars to window.*, or b'' if none."""

    assignments = "".join(
        f"window.{key}={json.dumps(os.environ[key])};"
        for key in RUNTIME_CONFIG_ENV_KEYS
        if os.environ.get(key)
    )
    if not assignments:
        return b""
    return f"<script>{assignments}</script>".encode("utf-8")


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
    """Serves the flat bundle under any /<instance>/ prefix. Silent logging."""

    def end_headers(self):
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()

    def log_message(self, fmt, *args):
        pass

    def translate_path(self, path):
        """Drops a leading /<instance>/ segment so the flat bundle resolves.

        A segment is treated as an instance prefix only when it does not name an
        existing file, so real bundle paths always win.
        """

        stripped = path.split("?", 1)[0].split("#", 1)[0]
        segments = [s for s in posixpath.normpath(stripped).split("/") if s and s != "."]
        if segments and not os.path.exists(os.path.join(os.getcwd(), segments[0])):
            path = "/" + "/".join(segments[1:])
        return super().translate_path(path)

    def send_head(self):
        """Injects runtime config into index.html; everything else unchanged."""

        served = self.translate_path(self.path)
        if os.path.isdir(served):
            served = os.path.join(served, "index.html")
        if not served.endswith("index.html") or not os.path.isfile(served):
            return super().send_head()

        script = runtime_config_script()
        with open(served, "rb") as handle:
            body = handle.read()
        if script:
            marker = b"</head>"
            if marker in body:
                body = body.replace(marker, script + marker, 1)
            else:
                body = script + body

        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        return io.BytesIO(body)


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
