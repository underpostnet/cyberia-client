# Cyberia Client Makefile
# Supports: Web (Emscripten), Linux, Windows
#
# Usage:
#   make -f Web.mk                # Build for Web (default)
#   make -f Web.mk desktop        # Build for Desktop
#   make -f Web.mk clean          # Clean artifacts
#
# Configuration:
#   PLATFORM    : PLATFORM_WEB (default) or PLATFORM_DESKTOP
#   BUILD_MODE  : DEBUG (default) or RELEASE

include config.mk

# ============================================================================
# Configuration & Defaults
# ============================================================================

# Override generic project name if needed
ifeq ($(PROJECT_NAME),project-name)
    PROJECT_NAME := cyberia-client
endif

PLATFORM ?= PLATFORM_WEB
BUILD_MODE ?= DEBUG

# Directories
SRC_DIR ?= src
LIBS_DIR ?= libs
BUILD_DIR ?= build
OUTPUT_DIR ?= bin

# Raylib
RAYLIB_PATH ?= $(LIBS_DIR)/raylib

# Source Files
# Auto-discover sources in src/ directory
SRC_CPP := $(wildcard $(SRC_DIR)/*.cpp)
SRC_C   := $(wildcard $(SRC_DIR)/*.c)
# Add cJSON
SRC_C   += $(LIBS_DIR)/cJSON/cJSON.c

# Object Files
OBJ_DIR := $(BUILD_DIR)/$(PLATFORM)/$(BUILD_MODE)
OBJS    := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(filter $(SRC_DIR)/%.cpp, $(SRC_CPP))) \
           $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(filter $(SRC_DIR)/%.c, $(SRC_C))) \
           $(patsubst $(LIBS_DIR)/cJSON/%.c, $(OBJ_DIR)/cJSON.o, $(filter $(LIBS_DIR)/cJSON/%.c, $(SRC_C)))

# Output Paths
OUT_DIR := $(OUTPUT_DIR)/$(PLATFORM)/$(BUILD_MODE)

# ============================================================================
# Platform Specific Settings
# ============================================================================

# ----------------------------------------------------------------------------
# WEB (Emscripten)
# ----------------------------------------------------------------------------
ifeq ($(PLATFORM),PLATFORM_WEB)
    CC = emcc
    CXX = em++
    EXT = .html

    # Web Flags
    CFLAGS += -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2
    CXXFLAGS += -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2

    # Emscripten Linker Flags
    LDFLAGS += -s USE_GLFW=3 \
               -s ASYNCIFY \
               -s ASYNCIFY_IMPORTS='["js_fetch_object_layer","js_fetch_binary"]' \
               -lwebsocket.js \
               --js-library $(SRC_DIR)/js/services.js \
               --shell-file $(SRC_DIR)/shell.html \
               -s TOTAL_MEMORY=128MB \
               -s ALLOW_MEMORY_GROWTH=1 \
               -s 'EXPORTED_RUNTIME_METHODS=["writeArrayToMemory","setValue"]' \
               -lidbfs.js

    # Raylib for Web (Static build)
    # We expect libraylib.a to be built in the raylib src directory
    RAYLIB_LIB_NAME = libraylib.web.a
    RAYLIB_LIB = $(RAYLIB_PATH)/src/$(RAYLIB_LIB_NAME)
    LDFLAGS += $(RAYLIB_LIB)

    ifeq ($(BUILD_MODE),RELEASE)
        LDFLAGS += -O3
    else
        LDFLAGS += -O0 --profiling
    endif
endif

# ----------------------------------------------------------------------------
# DESKTOP (Linux / Windows)
# ----------------------------------------------------------------------------
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    # Detect OS
    ifeq ($(OS),Windows_NT)
        PLATFORM_OS = WINDOWS
        EXT = .exe
        CC = gcc
        CXX = g++

        # Windows Libraries
        LDLIBS = -lraylib -lopengl32 -lgdi32 -lwinmm -static-libgcc -static-libstdc++ -Wl,--allow-multiple-definition
    else
        UNAME_S := $(shell uname -s)
        ifeq ($(UNAME_S),Linux)
            PLATFORM_OS = LINUX
            EXT =
            CC = gcc
            CXX = g++

            # Linux Libraries
            LDLIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
        endif
        ifeq ($(UNAME_S),Darwin)
            PLATFORM_OS = OSX
            EXT =
            CC = clang
            CXX = clang++
            LDLIBS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo
        endif
    endif

    CFLAGS += -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE
    CXXFLAGS += -DPLATFORM_DESKTOP -D_DEFAULT_SOURCE

    # Linker
    LDFLAGS += -L$(RAYLIB_PATH)/src
endif

# ============================================================================
# Common Flags
# ============================================================================

COMMON_FLAGS = -Wall -Wextra -Wno-unused-parameter -D_GNU_SOURCE -I$(SRC_DIR) -I$(LIBS_DIR)/cJSON -I$(RAYLIB_PATH)/src

ifeq ($(BUILD_MODE),DEBUG)
    COMMON_FLAGS += -g -D_DEBUG
else
    COMMON_FLAGS += -O2 -DNDEBUG
endif

CFLAGS += $(COMMON_FLAGS) -std=c99
CXXFLAGS += $(COMMON_FLAGS) -std=c++11

# ============================================================================
# Targets
# ============================================================================

TARGET = $(OUT_DIR)/$(PROJECT_NAME)$(EXT)

.PHONY: all clean web desktop serve info libraylib

all: $(TARGET)

# Link
$(TARGET): $(OBJS) $(if $(filter PLATFORM_WEB,$(PLATFORM)),libraylib)
	@mkdir -p $(dir $@)
	@echo "Linking $(TARGET)..."
ifeq ($(PLATFORM),PLATFORM_WEB)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Copying assets..."
	@if [ -d "$(SRC_DIR)/public" ]; then cp -r $(SRC_DIR)/public/* $(OUT_DIR)/; fi
else
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)
endif
	@echo "Build successful!"

# Compile C++
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(CXXFLAGS)

# Compile C
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

# Compile cJSON
$(OBJ_DIR)/cJSON.o: $(LIBS_DIR)/cJSON/cJSON.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

# Build Raylib for Web
libraylib:
ifeq ($(PLATFORM),PLATFORM_WEB)
	@if [ ! -f "$(RAYLIB_LIB)" ]; then \
		echo "Building Raylib for Web..."; \
		if [ "$(OS)" = "Windows_NT" ]; then \
			cp emar.bat emcc.bat $(RAYLIB_PATH)/src; \
		fi; \
		$(MAKE) -j 8 -C $(RAYLIB_PATH)/src raylib \
			PLATFORM=PLATFORM_WEB \
			RAYLIB_BUILD_MODE=$(BUILD_MODE) \
			RAYLIB_LIBTYPE=STATIC; \
		if [ "$(OS)" = "Windows_NT" ]; then \
			rm $(RAYLIB_PATH)/src/emar.bat $(RAYLIB_PATH)/src/emcc.bat; \
		fi; \
		if [ -f "$(RAYLIB_PATH)/src/libraylib.a" ]; then \
			mv $(RAYLIB_PATH)/src/libraylib.a $(RAYLIB_LIB); \
		fi; \
	fi
endif

# Clean
clean:
	rm -rf $(BUILD_DIR) $(OUTPUT_DIR)
	@echo "Cleaned build artifacts."
ifeq ($(PLATFORM),PLATFORM_WEB)
	@if [ -f "$(RAYLIB_LIB)" ]; then rm $(RAYLIB_LIB); fi
endif

# Aliases
web:
	$(MAKE) -f Web.mk PLATFORM=PLATFORM_WEB

desktop:
	$(MAKE) -f Web.mk PLATFORM=PLATFORM_DESKTOP

# Serve (Web only)
serve:
ifeq ($(PLATFORM),PLATFORM_WEB)
	@echo "Starting web server..."
	@echo "Open http://localhost:8080/$(PROJECT_NAME).html"
	python3 -m http.server 8080 --directory $(OUT_DIR)
else
	@echo "Serve target is only for Web builds. Use 'make -f Web.mk web serve' or manually run the executable."
endif

info:
	@echo "Project:  $(PROJECT_NAME)"
	@echo "Platform: $(PLATFORM)"
	@echo "Mode:     $(BUILD_MODE)"
	@echo "Output:   $(TARGET)"
	@echo "Sources:  $(SRC_CPP) $(SRC_C)"
