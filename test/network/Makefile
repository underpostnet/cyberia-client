# Makefile for Cyberia Client WebSocket Test
# Compiles C code to WebAssembly using Emscripten

# Compiler
CC = emcc

# Source files
SRC_DIR = ../../src_c
TEST_DIR = .
SOURCES = $(SRC_DIR)/config.c $(SRC_DIR)/network.c $(TEST_DIR)/emscripten.network.c

# Output files
OUTPUT = emscripten.network.js
OUTPUT_WASM = emscripten.network.wasm

# Compiler flags
CFLAGS = -Wall -Wextra -O2
EMFLAGS = -s WASM=1 \
          -s USE_PTHREADS=0 \
          -s ALLOW_MEMORY_GROWTH=1 \
          -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
          -s EXPORTED_FUNCTIONS='["_main"]' \
          -s ENVIRONMENT='web' \
          -s MODULARIZE=1 \
          -s EXPORT_NAME='CyberiaClient' \
          -lwebsocket.js

# Include directories
INCLUDES = -I$(SRC_DIR)

# Default target
all: $(OUTPUT)

# Build the WebSocket test
$(OUTPUT): $(SOURCES)
	@echo "Compiling WebSocket test with Emscripten..."
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCES) -o $(OUTPUT) $(EMFLAGS)
	@echo "Build complete!"
	@echo "Generated files:"
	@echo "  - $(OUTPUT)"
	@echo "  - $(OUTPUT_WASM)"
	@echo ""
	@echo "To run the test:"
	@echo "  1. Start a local web server: python3 -m http.server 8000"
	@echo "  2. Open http://localhost:8000/test.html in your browser"
	@echo "  3. Click 'Run WebSocket Test' button"
	@echo "  4. Check the console output on the page"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(OUTPUT) $(OUTPUT_WASM)
	@echo "Clean complete!"

# Run a local server (requires Python 3)
serve:
	@echo "Starting local web server on port 8000..."
	@echo "Open http://localhost:8000/test.html in your browser"
	python3 -m http.server 8000

# Build with debug symbols
debug: CFLAGS += -g -DDEBUG
debug: EMFLAGS += -s ASSERTIONS=1 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=1
debug: clean $(OUTPUT)
	@echo "Debug build complete!"

# Show help
help:
	@echo "Cyberia Client WebSocket Test - Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  make          - Build the WebSocket test (default)"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make debug    - Build with debug symbols and assertions"
	@echo "  make serve    - Start a local web server to test"
	@echo "  make help     - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  - Emscripten SDK (emcc compiler)"
	@echo "  - Python 3 (for local web server)"
	@echo ""
	@echo "WebSocket Server:"
	@echo "  Make sure you have a WebSocket server running at:"
	@echo "  ws://localhost:8080/ws"

.PHONY: all clean serve debug help
