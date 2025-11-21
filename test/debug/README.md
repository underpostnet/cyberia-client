# Cyberia MMO Client

MMO client built with C, Raylib, and WebSockets, compiled to WebAssembly using Emscripten.

## Architecture Overview

This project follows an **event-driven architecture**, with follow separation concerns:

```
┌─────────────────────────────────────────────────────────┐
│                        main.c                           │
│              (Application Entry Point)                  │
│                                                         │
│  • Initializes all subsystems                           │
│  • Manages main event loop                              │
│  • Coordinates render and client updates                │
└────────────┬────────────────────────────┬───────────────┘
             │                            │
             ▼                            ▼
    ┌────────────────┐          ┌─────────────────────┐
    │   render.c     │          │     client.c        │
    │   render.h     │          │     client.h        │
    │                │          │                     │
    │ • Raylib       │◄─────────│ • WebSocket control │
    │   rendering    │  Updates │ • Event handlers    │
    │ • Display text │   text   │ • Message buffering │
    │ • UI/Graphics  │          │                     │
    └────────────────┘          └──────────┬──────────┘
                                           │
                                           ▼
                                ┌──────────────────────┐
                                │     network.c        │
                                │     network.h        │
                                │                      │
                                │ • WebSocket API      │
                                │ • Low-level comms    │
                                │ • Event callbacks    │
                                └──────────┬───────────┘
                                           │
                                           ▼
                                ┌──────────────────────┐
                                │     config.c         │
                                │     config.h         │
                                │                      │
                                │ • WS_URL             │
                                │ • Configuration      │
                                └──────────────────────┘
```

## Event-Driven Architecture

The system uses callbacks for asynchronous event handling:

### WebSocket Events → Callbacks

```c
WebSocket Event          →  network.c handler   →  client.c callback    →  Action
─────────────────────────────────────────────────────────────────────────────
on_open (connected)      →  on_open_internal    →  on_websocket_open    →  Update UI
on_message (data recv)   →  on_message_internal →  on_websocket_message → Buffer & display
on_error (error)         →  on_error_internal   →  on_websocket_error   →  Show error
on_close (disconnected)  →  on_close_internal   →  on_websocket_close   →  Update status
```

## Building the Project

### Prerequisites

1. **Emscripten SDK** installed at `~/.emsdk`
2. **Raylib** compiled for Emscripten (should be in emsdk cache)
3. **Make** utility

### Build Commands

```bash
# Navigate to src_c directory
cd src_c

# Standard build
make

# Debug build (with symbols and assertions)
make debug

# Release build (optimized)
make release

# Clean build artifacts
make clean
```

### Build Output

After building, you'll have:
- `cyberia-client.html` - Entry HTML file
- `cyberia-client.js` - JavaScript glue code
- `cyberia-client.wasm` - WebAssembly binary

## Running the Application

### Using Emscripten's Built-in Server

```bash
emrun --no_browser --port 8081 .
```

Then open your browser to:
```
http://localhost:8081/cyberia-client.html
```

### Using Any HTTP Server

```bash
# Python
python3 -m http.server 8081

# Node.js (http-server)
npx http-server -p 8081

# PHP
php -S localhost:8081
```

## Configuration

### WebSocket Server URL

Edit `config.c` to change the WebSocket server:

```c
const char* WS_URL = "ws://localhost:8080/ws";
```

### Window Settings

Edit `main.c` constants:

```c
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define WINDOW_TITLE "Cyberia MMO Client"
```
