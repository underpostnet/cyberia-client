### Building the Test

```bash
cd test/network
make              # Compile the code
make serve        # Start a web server
# Then open http://localhost:8000/test.html in your browser
```

This will generate:
- `emscripten.network.html` - HTML page to run the test
- `emscripten.network.js` - JavaScript glue code
- `emscripten.network.wasm` - WebAssembly binary

### Detailed Event Logging

Each WebSocket event is logged with:
- Event type (on_open, on_message, on_error, on_close)
- Timestamp
- Connection status
- Message content (for on_message)
- Hex dump of message data
- Close codes and reasons (for on_close)

### Automatic Testing

The test automatically:
1. Connects to the WebSocket server
2. Sends a test message upon connection
3. Logs all received messages
4. Sends echo responses
5. Tracks message count
6. Reports connection status changes

### Close Code Interpretation

The test interprets WebSocket close codes:
- 1000: Normal Closure
- 1001: Going Away
- 1002: Protocol Error
- 1003: Unsupported Data
- 1006: Abnormal Closure
- 1007: Invalid Frame Payload Data
- 1008: Policy Violation
- 1009: Message Too Big
- 1011: Internal Server Error
