# Makefile for Cyberia Client (WebAssembly + Raylib + WebSocket)

# ============================================================================
# Project Configuration
# ============================================================================

PROJECT_NAME := cyberia-client
TARGET_NAME := index
BUILD_MODE ?= DEBUG
PLATFORM := PLATFORM_WEB

# Build directories
BUILD_DIR := build
OUTPUT_DIR := bin/$(PLATFORM)/$(BUILD_MODE)

# Source files
SOURCES := main.c render.c client.c network.c config.c game_state.c game_render.c message_parser.c input.c serial.c dev_ui.c modal.c modal_player.c ../lib/cJSON/cJSON.c
HEADERS := render.h client.h network.h config.h game_state.h game_render.h message_parser.h input.h serial.h dev_ui.h modal.h modal_player.h ../lib/cJSON/cJSON.h

# ============================================================================
# Emscripten SDK Configuration
# ============================================================================

EMSDK_PATH := $(HOME)/.emsdk
EMSDK_INCLUDE := $(EMSDK_PATH)/upstream/emscripten/cache/sysroot/include
EMSDK_LIB := $(EMSDK_PATH)/upstream/emscripten/cache/sysroot/lib/libraylib.a

# Shell template for fullscreen experience
SHELL_FILE := shell.html

# ============================================================================
# Compiler Flags
# ============================================================================

# Base compiler flags
EMCC_FLAGS := \
	-I $(EMSDK_INCLUDE) \
	-I ../lib/cJSON \
	$(EMSDK_LIB) \
	-s USE_GLFW=3 \
	-s ASYNCIFY \
	-lwebsocket.js \
	--shell-file $(SHELL_FILE) \
	-DPLATFORM_WEB \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-s TOTAL_MEMORY=128MB \
	-s ALLOW_MEMORY_GROWTH=1

# Build mode specific flags
ifeq ($(BUILD_MODE),DEBUG)
EMCC_FLAGS += -g -D_DEBUG
EMCC_FLAGS += -s ASSERTIONS=1
EMCC_FLAGS += -s SAFE_HEAP=1
EMCC_FLAGS += -s STACK_OVERFLOW_CHECK=2
EMCC_FLAGS += -O2 --profiling
endif

ifeq ($(BUILD_MODE),RELEASE)
EMCC_FLAGS += -Os -DNDEBUG
EMCC_FLAGS += --closure 1
EMCC_FLAGS += -s AGGRESSIVE_VARIABLE_ELIMINATION=1
endif

# ============================================================================
# Build Targets
# ============================================================================

.PHONY: all clean debug release help check-deps info serve

# Default target
all: check-deps $(OUTPUT_DIR)/$(TARGET_NAME).html
	@echo ""
	@echo "=========================================="
	@echo "  ✓ Build completed successfully!"
	@echo "=========================================="
	@echo ""
	@echo "Build Mode: $(BUILD_MODE)"
	@echo "Output Dir: $(OUTPUT_DIR)"
	@echo ""
	@echo "To run the application:"
	@echo "  make serve"
	@echo ""
	@echo "Or manually:"
	@echo "  emrun --no_browser --port 8081 $(OUTPUT_DIR)"
	@echo ""
	@echo "Then open in browser:"
	@echo "  http://localhost:8081/$(TARGET_NAME).html"
	@echo "=========================================="

# Serve the application locally
serve: check-deps
	@if [ ! -f "$(OUTPUT_DIR)/$(TARGET_NAME).html" ]; then \
		echo "========================================"; \
		echo "  Building project first..."; \
		echo "========================================"; \
		$(MAKE) all; \
		echo ""; \
	fi
	@echo "=========================================="
	@echo "  Starting local development server"
	@echo "=========================================="
	@echo ""
	@echo "Server running at:"
	@echo "  http://localhost:8081/$(TARGET_NAME).html"
	@echo ""
	@echo "Press Ctrl+C to stop the server"
	@echo "=========================================="
	@echo ""
	@emrun --no_browser --port 8081 $(OUTPUT_DIR)

# Main build target
$(OUTPUT_DIR)/$(TARGET_NAME).html: $(SOURCES) $(HEADERS) $(SHELL_FILE)
	@mkdir -p $(OUTPUT_DIR)
	@echo "=========================================="
	@echo "  Building $(PROJECT_NAME)"
	@echo "=========================================="
	@echo "Build Mode: $(BUILD_MODE)"
	@echo "Platform:   $(PLATFORM)"
	@echo "Output Dir: $(OUTPUT_DIR)"
	@echo ""
	@echo "Sources: $(SOURCES)"
	@echo "Headers: $(HEADERS)"
	@echo "Shell:   $(SHELL_FILE)"
	@echo ""
	@echo "Compiling to WebAssembly..."
	emcc -o $@ $(SOURCES) $(EMCC_FLAGS)
	@echo ""
	@echo "Generated files:"
	@echo "  • $(OUTPUT_DIR)/$(TARGET_NAME).html"
	@echo "  • $(OUTPUT_DIR)/$(TARGET_NAME).js"
	@echo "  • $(OUTPUT_DIR)/$(TARGET_NAME).wasm"

# Debug build with extra diagnostics
debug:
	@$(MAKE) BUILD_MODE=DEBUG all
	@echo ""
	@echo "[DEBUG BUILD] Completed with debugging symbols"
	@echo "Use browser DevTools for debugging"

# Release build with optimizations
release:
	@$(MAKE) BUILD_MODE=RELEASE all
	@echo ""
	@echo "[RELEASE BUILD] Completed with optimizations"
	@echo "Files are minified and optimized for production"

# Check if shell.html exists
check-deps:
	@if [ ! -f "$(SHELL_FILE)" ]; then \
		echo "[ERROR] $(SHELL_FILE) not found!"; \
		echo "The shell file is required for building."; \
		exit 1; \
	fi
	@echo "[✓] Dependencies check passed"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(OUTPUT_DIR)
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Show project information
info:
	@echo "=========================================="
	@echo "  Cyberia Client - Info"
	@echo "=========================================="
	@echo ""
	@echo "Project:     $(PROJECT_NAME)"
	@echo "Target:      $(TARGET_NAME)"
	@echo "Build Mode:  $(BUILD_MODE)"
	@echo "Platform:    $(PLATFORM)"
	@echo ""
	@echo "Build Dir:   $(BUILD_DIR)"
	@echo "Output Dir:  $(OUTPUT_DIR)"
	@echo ""
	@echo "Sources:"
	@for src in $(SOURCES); do echo "  • $$src"; done
	@echo ""
	@echo "Headers:"
	@for hdr in $(HEADERS); do echo "  • $$hdr"; done
	@echo ""
	@echo "Emscripten SDK: $(EMSDK_PATH)"
	@echo "Raylib:         $(EMSDK_LIB)"
	@echo "Shell:          $(SHELL_FILE)"
	@echo ""
	@echo "Available targets:"
	@echo "  make              - Build the project (default)"
	@echo "  make serve        - Build and start local server"
	@echo "  make debug        - Build with debugging symbols"
	@echo "  make release      - Build with optimizations"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make info         - Show this information"
	@echo "  make help         - Show detailed help"
	@echo ""
	@echo "Build mode can be overridden:"
	@echo "  make BUILD_MODE=DEBUG"
	@echo "  make BUILD_MODE=RELEASE"
	@echo "=========================================="

# Help target with detailed instructions
help:
	@echo "=========================================="
	@echo "  Cyberia Client - Help"
	@echo "=========================================="
	@echo ""
	@echo "DESCRIPTION:"
	@echo "  This Makefile builds the Cyberia client"
	@echo "  as a WebAssembly application using Emscripten and Raylib."
	@echo ""
	@echo "PREREQUISITES:"
	@echo "  1. Emscripten SDK installed at: $(EMSDK_PATH)"
	@echo "  2. Raylib library compiled for Emscripten"
	@echo "  3. Shell template file: $(SHELL_FILE)"
	@echo ""
	@echo "USAGE:"
	@echo "  make [target]"
	@echo ""
	@echo "TARGETS:"
	@echo "  all       Build the project (default)"
	@echo "  serve     Build and start local development server"
	@echo "  debug     Build with debugging enabled"
	@echo "  release   Build with optimizations"
	@echo "  clean     Remove all build artifacts"
	@echo "  info      Display project information"
	@echo "  help      Display this help message"
	@echo ""
	@echo "EXAMPLES:"
	@echo "  # Standard build"
	@echo "  make"
	@echo ""
	@echo "  # Debug build for development"
	@echo "  make debug"
	@echo ""
	@echo "  # Optimized build for production"
	@echo "  make release"
	@echo ""
	@echo "  # Clean and rebuild"
	@echo "  make clean && make"
	@echo ""
	@echo "RUNNING THE APPLICATION:"
	@echo "  1. Build and serve (easiest):"
	@echo "     make serve"
	@echo ""
	@echo "  2. Or build then serve manually:"
	@echo "     make"
	@echo "     emrun --no_browser --port 8081 $(OUTPUT_DIR)"
	@echo ""
	@echo "  3. Then open in your browser:"
	@echo "     http://localhost:8081/$(TARGET_NAME).html"
	@echo ""
	@echo "TROUBLESHOOTING:"
	@echo "  • If build fails, check that Emscripten is activated:"
	@echo "    source $(EMSDK_PATH)/emsdk_env.sh"
	@echo ""
	@echo "  • Verify Raylib is available:"
	@echo "    ls -la $(EMSDK_LIB)"
	@echo ""
	@echo "  • Check shell.html exists in current directory"
	@echo ""
	@echo "=========================================="
