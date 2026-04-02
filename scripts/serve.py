#!/usr/bin/env python3
"""
Static HTTP server with no-cache headers for WASM development.

Prevents browsers from serving stale WASM/JS binaries after a rebuild.
Usage:
    python3 scripts/serve.py <port> <directory>
"""

import sys
import os
from http.server import HTTPServer, SimpleHTTPRequestHandler


class NoCacheHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()

    def log_message(self, format, *args):
        # Suppress per-request noise; keep it quiet for terminal readability
        pass


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8082
    directory = sys.argv[2] if len(sys.argv) > 2 else "."

    os.chdir(directory)
    server = HTTPServer(("", port), NoCacheHandler)
    print(f"[serve] http://localhost:{port}  (no-cache, dir={directory})")
    server.serve_forever()
