# Cyberia Immersive Client

A clean, immersive WebAssembly-based game client built with Raylib and WebSocket support. This client provides a fullscreen, responsive gaming experience with no UI clutter - just pure immersion.

## 🎯 Features

- **Immersive Fullscreen Experience**: No banners, status bars, or UI elements - only the game canvas
- **Responsive Design**: Automatically adapts to browser window size changes
- **WebSocket Network Layer**: Real-time communication with game server
- **Event-Driven Architecture**: Clean separation of concerns with callback-based networking
- **JavaScript Debug Logging**: Comprehensive console logging for all WebSocket events:
  - `on_open` - Connection established
  - `on_message` - Messages received (with JSON parsing)
  - `on_error` - Connection errors
  - `on_close` - Connection closed (with reason codes)
- **Visual Indicators**:
  - Red circle in center (always centered, scales responsively)
  - Blue border around canvas edges (for responsive testing)

## 📁 Project Structure

```
src_c/
├── main.c           # Application entry point and main loop
├── render.c         # Rendering subsystem (Raylib)
├── render.h         # Rendering interface
├── client.c         # Client logic and WebSocket event handlers
├── client.h         # Client interface
├── network.c        # WebSocket network layer implementation
├── network.h        # WebSocket network interface
├── config.c         # Configuration values
├── config.h         # Configuration interface
├── shell.html       # HTML shell template (immersive design)
├── Makefile         # Build system
└── README.md        # This file
```

## 🛠️ Architecture

### Modular Design

The client is built with a clean modular architecture following industry best practices:

1. **Main Module** (`main.c`)
   - Application initialization and shutdown
   - Main event loop (Emscripten-compatible)
   - Subsystem coordination

2. **Render Module** (`render.c/h`)
   - Raylib initialization and management
   - Frame rendering with responsive canvas
   - Visual elements (circle, border)

3. **Client Module** (`client.c/h`)
   - High-level client logic
   - WebSocket event callback implementations
   - Message handling and state management

4. **Network Module** (`network.c/h`)
   - Low-level WebSocket abstraction
   - Event-driven networking with callbacks
   - Connection lifecycle management

5. **Config Module** (`config.c/h`)
   - Centralized configuration
   - WebSocket URL and application settings

### Event-Driven Networking

The client uses an event-driven architecture for network communication:

```
Server Event → Emscripten WebSocket → Network Layer → Client Callbacks → Application Logic
```

All WebSocket events are logged to the JavaScript console for debugging.

## 📋 Prerequisites

1. **Emscripten SDK** (v3.1.0 or later)
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh
   ```

2. **Raylib for Emscripten**
   - Typically comes with Emscripten SDK
   - Located at: `~/.emsdk/upstream/emscripten/cache/sysroot/lib/libraylib.a`

## 🚀 Building

### Standard Build

```bash
cd src_c
make
```

### Debug Build (with assertions and debugging symbols)

```bash
make debug
```

### Release Build (optimized for production)

```bash
make release
```

### Clean Build Artifacts

```bash
make clean
```

### View Project Information

```bash
make info
```

## 🌐 Running

1. **Build the project**:
   ```bash
   make
   ```

2. **Start a local web server**:
   ```bash
   emrun --no_browser --port 8081 .
   ```

3. **Open in your browser**:
   ```
   http://localhost:8081/cyberia-client.html
   ```

## 🔧 Configuration

Edit `config.c` to change the WebSocket server URL:

```c
const char* WS_URL = "ws://localhost:8080/ws";
```

For production, use a secure WebSocket connection:

```c
const char* WS_URL = "wss://game.example.com/ws";
```

## 🐛 Debugging

### Browser Console Logs

All WebSocket events are logged to the browser console with color-coded messages:

- **🟢 Green** - `on_open`: Connection established
- **🔵 Blue** - `on_message`: Message received
- **🔴 Red** - `on_error`: Error occurred
- **🟠 Orange** - `on_close`: Connection closed

### C Console Logs

All C `printf` statements are redirected to the browser console with `[MODULE]` prefixes.

### Example Console Output

```
[INIT] Cyberia Client - Starting initialization...
[INIT] Step 1/2 - Initializing renderer...
[RENDER] Initialized successfully (800x600)
[INIT] Step 2/2 - Initializing network client...
[CLIENT] Connecting to ws://localhost:8080/ws
[NETWORK] WebSocket initialized for URL: ws://localhost:8080/ws
[CLIENT] Initialization complete
[INIT] All subsystems initialized - Ready!
[MAIN] Running in WebAssembly mode
[MODULE] Application started

[NETWORK] WebSocket connection opened
[WS] on_open
[WS] WebSocket connection successfully established
[WS] Ready to send and receive messages

[CLIENT] Sending message (68 bytes): {"type":"handshake",...}
[NETWORK] Message sent successfully (68 bytes)

[NETWORK] Received text message (156 bytes)
[WS] on_message
[WS] Message #1 received (156 bytes)
[WS] Content: {"type":"welcome","status":"connected"}
[WS] Parsed JSON: {type: "welcome", status: "connected"}
```

## 🎨 Visual Elements

### Red Circle
- **Location**: Center of canvas
- **Size**: 10% of the smaller canvas dimension (responsive)
- **Purpose**: Visual indicator always visible regardless of canvas size

### Blue Border
- **Location**: All four edges of canvas
- **Width**: 5 pixels
- **Purpose**: Visual indicator for responsive testing and canvas boundaries

## 🔌 WebSocket Protocol

The client automatically sends a handshake message on connection:

```json
{
  "type": "handshake",
  "client": "cyberia-immersive",
  "version": "1.0.0"
}
```

All incoming messages are logged to the console with automatic JSON parsing for better visualization.

## 📦 Output Files

After building, you'll have three files:

1. **cyberia-client.html** - The main HTML page (immersive design)
2. **cyberia-client.js** - Generated JavaScript code
3. **cyberia-client.wasm** - WebAssembly binary module

## 🎯 Design Philosophy

### Immersive Experience
- No UI elements except the game canvas
- Fullscreen, responsive design
- Minimal visual distractions

### Clean Code
- Modular architecture with clear separation of concerns
- Comprehensive documentation and comments
- Industry-standard practices (DRY, SOLID principles)

### Developer-Friendly
- Extensive logging for debugging
- Clear error messages
- Helpful build system with `make help`

## 🔄 WebSocket Event Flow

```
Connection Lifecycle:
1. Client calls ws_init()
2. Emscripten creates WebSocket
3. on_open_internal() → on_websocket_open() → Log to console
4. Client sends handshake
5. Server sends messages → on_message_internal() → on_websocket_message() → Log to console
6. Connection closed → on_close_internal() → on_websocket_close() → Log to console
```

## 📝 Development Guidelines

### Adding New Features

1. **New Rendering**: Add to `render.c`
2. **New Network Events**: Add callbacks to `client.c`
3. **New Configuration**: Add to `config.c/h`

### Code Style

- Use clear, descriptive variable names
- Add comments for complex logic
- Follow existing naming conventions
- Keep functions focused and small

### Testing

- Test in multiple browsers (Chrome, Firefox, Safari)
- Test with different screen sizes
- Test with slow/unstable network connections
- Monitor browser console for errors

## 🤝 Integration with Server

The client expects a WebSocket server at the configured URL. The server should:

1. Accept WebSocket connections
2. Handle handshake messages
3. Send/receive JSON messages
4. Close connections gracefully

Example server setup (for testing):
```bash
# Start your WebSocket server on port 8080
# Example: Node.js, Python, Go, etc.
```

## 📄 License

This project follows the same license as the parent Cyberia Client project.

## 🙏 Acknowledgments

- Built with [Raylib](https://www.raylib.com/) - Amazing game development library
- Powered by [Emscripten](https://emscripten.org/) - C/C++ to WebAssembly compiler
- Uses WebSocket API for real-time communication

---

**Note**: This is an immersive client designed for fullscreen gaming experiences. For development with UI elements, see the debug client in `test/debug/`.