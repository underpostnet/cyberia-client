# Makefile for Cyberia MMO Client (Emscripten + Raylib + WebSocket)

# --- Configuration ---
EMSDK_PATH := $(HOME)/.emsdk
EMSDK_INCLUDE := $(EMSDK_PATH)/upstream/emscripten/cache/sysroot/include
EMSDK_LIB := $(EMSDK_PATH)/upstream/emscripten/cache/sysroot/lib/libraylib.a

# Project configuration
PROJECT_NAME := cyberia-client
TARGET_NAME := $(PROJECT_NAME)

# Source files
SOURCES := main.c render.c client.c network.c config.c
HEADERS := render.h client.h network.h config.h

# Shell template
SHELL_FILE := ../test/raylib/shell.html

# --- Compiler Flags ---
EMCC_FLAGS := \
	-Os \
	-I $(EMSDK_INCLUDE) \
	$(EMSDK_LIB) \
	-s USE_GLFW=3 \
	-s ASYNCIFY \
	-lwebsocket.js \
	--shell-file $(SHELL_FILE) \
	-DPLATFORM_WEB \
	-Wall \
	-Wextra

# Additional flags for debugging (optional)
DEBUG_FLAGS := -g -s ASSERTIONS=1 -s SAFE_HEAP=1

# Additional flags for production (optional)
RELEASE_FLAGS := -Os --closure 1

# --- Targets ---

.PHONY: all clean debug release help check-shell

all: check-shell $(TARGET_NAME).html

# Main build target
$(TARGET_NAME).html: $(SOURCES) $(HEADERS) $(SHELL_FILE)
	@echo "================================================"
	@echo "  Building Cyberia MMO Client (WebAssembly)"
	@echo "================================================"
	@echo "[INFO] Compiling sources..."
	@echo "  Sources: $(SOURCES)"
	emcc -o $@ $(SOURCES) $(EMCC_FLAGS)
	@echo ""
	@echo "================================================"
	@echo "   ✓ Build completed successfully!"
	@echo "================================================"
	@echo "  Generated files:"
	@echo "    • $(TARGET_NAME).html"
	@echo "    • $(TARGET_NAME).wasm"
	@echo "    • $(TARGET_NAME).js"
	@echo "================================================"
	@echo ""
	@echo "To run the application:"
	@echo "  emrun --no_browser --port 8081 ."
	@echo ""
	@echo "Then open in browser:"
	@echo "  http://localhost:8081/$(TARGET_NAME).html"
	@echo "================================================"

# Debug build with extra diagnostics
debug: EMCC_FLAGS += $(DEBUG_FLAGS)
debug: $(TARGET_NAME).html
	@echo "[DEBUG] Build completed with debugging symbols"

# Release build with optimizations
release: EMCC_FLAGS += $(RELEASE_FLAGS)
release: $(TARGET_NAME).html
	@echo "[RELEASE] Build completed with optimizations"

# Check if shell.html exists, download if needed
check-shell:
	@if [ ! -f "$(SHELL_FILE)" ]; then \
		echo "[INFO] shell.html not found, downloading..."; \
		mkdir -p ../test/raylib; \
		wget -O $(SHELL_FILE) https://raw.githubusercontent.com/raysan5/raylib/master/src/shell.html; \
	fi

# Clean build artifacts
clean:
	@echo "[INFO] Cleaning build artifacts..."
	rm -f $(TARGET_NAME).html $(TARGET_NAME).js $(TARGET_NAME).wasm
	rm -f $(TARGET_NAME).data
	@echo "[INFO] Clean complete"

# Help target
help:
	@echo "Cyberia MMO Client - Makefile Help"
	@echo "===================================="
	@echo ""
	@echo "Available targets:"
	@echo "  make           - Build the project (default)"
	@echo "  make all       - Same as 'make'"
	@echo "  make debug     - Build with debugging flags"
	@echo "  make release   - Build with optimization flags"
	@echo "  make clean     - Remove build artifacts"
	@echo "  make help      - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  - Emscripten SDK installed at: $(EMSDK_PATH)"
	@echo "  - Raylib library at: $(EMSDK_LIB)"
	@echo ""
	@echo "Configuration:"
	@echo "  Sources: $(SOURCES)"
	@echo "  Headers: $(HEADERS)"
	@echo ""
