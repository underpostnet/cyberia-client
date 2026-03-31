<p align="center">
  <img src="https://www.cyberiaonline.com/assets/splash/apple-touch-icon-precomposed.png" alt="CYBERIA online"/>
</p>

<div align="center">

<h1>cyberia client</h1>

</div>

Web client for [Cyberia](https://www.cyberiaonline.com), built with C, Raylib, and Emscripten (WebAssembly).

Connects to the Go game server via WebSocket using a binary AOI protocol. Receives entity positions, directions, modes, colors, and item stacks. Renders entities using atlas sprite sheets (fetched from the Engine REST API) or solid RGBA colors when no sprites are available.

## Prerequisites

- **GNU Make**
- **[Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)**
- **[git-subrepo](https://github.com/ingydotnet/git-subrepo)**

Libraries (Raylib, cJSON) are already checked in under `lib/` via git-subrepo.

### RHEL/Rocky Linux setup

```bash
scripts/rhel-emscripten-setup.sh   # installs build deps + emsdk
source ~/.bashrc                    # loads emsdk_env.sh
```

## Configuration

Connection URLs are compile-time constants in `src/config.h`:

```c
// Development
static const char* WS_URL = "ws://localhost:8081/ws";
static const char* API_BASE_URL = "http://localhost:4005";

// Production
static const char* WS_URL = "wss://server.cyberiaonline.com/ws";
static const char* API_BASE_URL = "https://www.cyberiaonline.com";
```

Edit `src/config.h` before building to switch between environments.

## Build

Activate Emscripten, then:

```bash
source ~/.emsdk/emsdk_env.sh

# Debug (default)
make -f Web.mk web

# Release
make -f Web.mk web BUILD_MODE=RELEASE
```

Output goes to `bin/web/debug/` or `bin/web/release/`.

## Development

```bash
# 1. Edit src/config.h → set WS_URL = "ws://localhost:8081/ws"
# 2. Build and serve
source ~/.emsdk/emsdk_env.sh
make -f Web.mk clean && make -f Web.mk web
make -f Web.mk serve-development   # http://localhost:8082
```

Requires the Go game server running on `:8081` (see [cyberia-server](../cyberia-server/README.md)).

### Full local stack

```bash
# Terminal 1: Engine (gRPC :50051 + REST :4005)
cd /home/dd/engine && npm run dev

# Terminal 2: Go server (WS :8081)
cd /home/dd/engine/cyberia-server && go run main.go

# Terminal 3: C/WASM client (HTTP :8082)
cd /home/dd/engine/cyberia-client
source ~/.emsdk/emsdk_env.sh
make -f Web.mk serve-development
```

### Dev port summary

| Component      | Port  | Protocol  |
| -------------- | ----- | --------- |
| Engine Express | 4005  | HTTP      |
| Engine gRPC    | 50051 | gRPC      |
| Go server      | 8081  | HTTP + WS |
| WASM client    | 8082  | HTTP      |

## Production

```bash
# 1. Edit src/config.h → set WS_URL = "wss://server.cyberiaonline.com/ws"
# 2. Build release
source ~/.emsdk/emsdk_env.sh
make -f Web.mk clean && make -f Web.mk web BUILD_MODE=RELEASE
make -f Web.mk serve-production    # http://localhost:8081
```

### Serving from Go server

The Go server can serve the WASM client directly:

```bash
cp -r bin/web/release/* /home/dd/engine/cyberia-server/public/
# Then: STATIC_DIR=./public go run main.go → serves WS + WASM on :8081
```

### Container deployment

```bash
scripts/container-setup.sh production   # builds RELEASE, serves :8081
scripts/container-setup.sh development  # builds DEBUG, serves :8082
```

## Ports

| Target              | Port | BUILD_MODE |
| ------------------- | ---- | ---------- |
| `serve-development` | 8082 | DEBUG      |
| `serve-production`  | 8081 | RELEASE    |

Override with `DEV_PORT=` or `PROD_PORT=`.

## Update Dependencies

```bash
git subrepo pull lib/raylib
git subrepo pull lib/cJSON
```

## Project Structure

```
src/           C source code
src/config.h   Connection URLs (WS_URL, API_BASE_URL)
src/js/        JavaScript interop (Emscripten)
src/public/    Static assets (favicon, splash)
lib/           External libraries (git-subrepo)
scripts/       Setup and deploy scripts
Web.mk         Build configuration
config.mk      Shared variables
```
