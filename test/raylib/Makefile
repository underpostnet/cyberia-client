# Makefile for Emscripten Raylib project

# --- Configuration ---
EMSDK_PATH := $(HOME)/.emsdk
EMSDK_INCLUDE := $(EMSDK_PATH)/upstream/emscripten/cache/sysroot/include
EMSDK_LIB := $(EMSDK_PATH)/upstream/emscripten/cache/sysroot/lib/libraylib.a

SOURCE_FILE := emscripten.raylib.c
TARGET_NAME := $(basename $(SOURCE_FILE))

# --- Emscripten Flags ---
EMCC_FLAGS := \
	-Os \
	-I $(EMSDK_INCLUDE) \
	$(EMSDK_LIB) \
	-s USE_GLFW=3 \
	--shell-file shell.html \
	-DPLATFORM_WEB

# --- Targets ---

.PHONY: all clean shell.html

all: $(TARGET_NAME).html

$(TARGET_NAME).html: $(SOURCE_FILE) shell.html
	@echo "[INFO] Compiling $(SOURCE_FILE) -> $@ (WebAssembly)..."
	emcc -o $@ $(SOURCE_FILE) $(EMCC_FLAGS)
	@echo ""
	@echo "====================================="
	@echo "   Build completed successfully!"
	@echo "   Files generated:"
	@echo "     • $(TARGET_NAME).html"
	@echo "     • $(TARGET_NAME).wasm"
	@echo "     • $(TARGET_NAME).js"
	@echo "====================================="
	@echo "Run with: emrun --no_browser --port 8081 ."
	@echo "Command copied to clipboard."
	@printf "emrun --no_browser --port 8081 ." | xsel --clipboard --input || true

shell.html:
	@echo "[INFO] Downloading shell.html..."
	wget -O $@ https://raw.githubusercontent.com/raysan5/raylib/master/src/shell.html

clean:
	@echo "[INFO] Cleaning up build files..."
	rm -f $(TARGET_NAME).html $(TARGET_NAME).js $(TARGET_NAME).wasm
	rm -f shell.html
