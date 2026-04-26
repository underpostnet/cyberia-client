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

Examples
--------
  python3 server.py 8081 bin/web/release               # production
  python3 server.py 8082 bin/web/debug  development    # development
"""

import sys
import os
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


def main():
    port      = int(sys.argv[1])      if len(sys.argv) > 1 else 8082
    directory = sys.argv[2]           if len(sys.argv) > 2 else "."
    mode      = (sys.argv[3] or "").lower() if len(sys.argv) > 3 else "production"

    handler = DevelopmentHandler if mode == "development" else ProductionHandler

    os.chdir(directory)
    server = HTTPServer(("", port), handler)
    print(f"[server] http://0.0.0.0:{port}  mode={mode}  dir={directory}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
