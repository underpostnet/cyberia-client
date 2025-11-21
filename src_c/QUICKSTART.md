# Quick Start Guide - Cyberia Immersive Client

Get up and running in 5 minutes!

## 🚀 Quick Setup

### 1. Prerequisites

Make sure you have Emscripten SDK installed and activated:

```bash
# Check if Emscripten is available
which emcc

# If not found, activate it:
source ~/.emsdk/emsdk_env.sh
```

### 2. Build the Client

```bash
cd src_c
make
```

### 3. Run the Client

```bash
# Start the web server
emrun --no_browser --port 8081 .

# Open in your browser:
# http://localhost:8081/cyberia-client.html
```

### 4. Open Browser Console

Press `F12` (or `Cmd+Option+I` on Mac) to open Developer Tools and view the console logs.

## 🎯 What You Should See

### In the Browser:
- **Black fullscreen canvas** covering the entire browser window
- **Red circle** in the center (scales with window size)
- **Blue border** around all edges (5px wide)
- **Loading spinner** briefly during initialization

### In the Console:
```
[INIT] Cyberia Client - Starting initialization...
[INIT] Step 1/2 - Initializing renderer...
[RENDER] Initialized successfully (800x600)
[INIT] Step 2/2 - Initializing network client...
[CLIENT] Connecting to ws://localhost:8080/ws
[NETWORK] WebSocket initialized for URL: ws://localhost:8080/ws
[MODULE] Application started

[WS] on_open (or on_error if server not available)
```

## 🔧 Configuration

### Change WebSocket Server URL

Edit `config.c`:

```c
const char* WS_URL = "ws://your-server:port/ws";
```

Then rebuild:
```bash
make clean && make
```

## 🧪 Testing WebSocket Connection

### Option 1: Use a Test Server

```bash
# Install wscat (Node.js WebSocket client/server)
npm install -g wscat

# Start a simple WebSocket server
wscat -l 8080
```

### Option 2: Use Python WebSocket Server

```python
# simple_ws_server.py
import asyncio
import websockets
import json

async def handler(websocket, path):
    print(f"Client connected: {path}")
    
    async for message in websocket:
        print(f"Received: {message}")
        
        # Echo back a response
        response = json.dumps({
            "type": "echo",
            "message": message,
            "timestamp": "now"
        })
        await websocket.send(response)

async def main():
    async with websockets.serve(handler, "localhost", 8080):
        print("WebSocket server running on ws://localhost:8080")
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())
```

Run it:
```bash
pip install websockets
python simple_ws_server.py
```

## 📊 Understanding the Console Output

### WebSocket Events

| Log Prefix | Color | Meaning |
|------------|-------|---------|
| `[WS] on_open` | 🟢 Green | Successfully connected to server |
| `[WS] on_message` | 🔵 Blue | Received message from server |
| `[WS] on_error` | 🔴 Red | Connection error occurred |
| `[WS] on_close` | 🟠 Orange | Connection closed |

### C Module Logs

| Log Prefix | Meaning |
|------------|---------|
| `[INIT]` | Initialization process |
| `[RENDER]` | Rendering subsystem |
| `[CLIENT]` | Client logic |
| `[NETWORK]` | Network layer |
| `[MODULE]` | Emscripten module |

## 🐛 Troubleshooting

### Build Fails

**Problem**: `emcc: command not found`

**Solution**:
```bash
source ~/.emsdk/emsdk_env.sh
```

---

**Problem**: `shell.html not found`

**Solution**: Make sure you're in the `src_c` directory:
```bash
cd cyberia-client/src_c
ls -la shell.html
```

---

**Problem**: `libraylib.a not found`

**Solution**: Raylib should be in Emscripten cache. Try:
```bash
# Check if Raylib exists
ls -la ~/.emsdk/upstream/emscripten/cache/sysroot/lib/libraylib.a

# If not, update Emscripten SDK
cd ~/.emsdk
./emsdk install latest
./emsdk activate latest
```

### Runtime Issues

**Problem**: Canvas not responsive

**Solution**: 
- Check browser console for JavaScript errors
- Make sure the canvas has no parent containers with fixed dimensions
- Verify CSS is not being overridden

---

**Problem**: WebSocket connection fails

**Solution**:
1. Check if server is running: `netstat -an | grep 8080`
2. Verify URL in `config.c` is correct
3. Check browser console for specific error messages
4. Try connecting with a WebSocket client tool:
   ```bash
   wscat -c ws://localhost:8080/ws
   ```

---

**Problem**: Red circle not visible

**Solution**:
- Check canvas dimensions in browser DevTools
- Verify WebGL is working (check for WebGL errors in console)
- Try resizing browser window

## 🎨 Customization Quick Tips

### Change Circle Color

Edit `render.c`:
```c
#define CIRCLE_COLOR BLUE  // Change from RED to BLUE
```

### Change Circle Size

Edit `render.c`:
```c
float radius = (current_width < current_height ? current_width : current_height) * 0.2f;  // 20% instead of 10%
```

### Change Border Color/Width

Edit `render.c`:
```c
#define BORDER_WIDTH 10  // Change from 5 to 10
#define BORDER_COLOR (Color){255, 0, 0, 255}  // Red instead of blue
```

### Add FPS Counter

Edit `render.c` in `render_update()`:
```c
// Before EndDrawing();
DrawFPS(10, 10);
```

## 📚 Next Steps

- Read the full [README.md](README.md) for detailed documentation
- Explore the modular architecture
- Implement game-specific rendering
- Add custom WebSocket message handlers
- Build your game logic!

## 💡 Pro Tips

1. **Use Debug Build During Development**
   ```bash
   make debug
   ```
   Provides better error messages and stack traces.

2. **Monitor Network Tab**
   Open browser DevTools → Network tab → WS filter
   See all WebSocket frames in real-time.

3. **Hot Reload**
   After making changes:
   ```bash
   make && echo "Build complete! Refresh browser."
   ```

4. **Test Responsiveness**
   - Resize browser window while running
   - Blue border should always be visible
   - Red circle should stay centered

5. **Check Performance**
   Open Chrome DevTools → Performance tab
   Record and analyze frame rendering.

## 🎓 Learning Resources

- [Raylib Cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html)
- [Emscripten Documentation](https://emscripten.org/docs/)
- [WebSocket API (MDN)](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket)
- [WebAssembly Concepts](https://developer.mozilla.org/en-US/docs/WebAssembly/Concepts)

---

**Happy coding! 🚀**