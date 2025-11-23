PROJECT_NAME	?= cyberia-client
BUILD_MODE		?= DEBUG
CC				?= emcc
PLATFORM		= PLATFORM_WEB

RAYLIB_PATH     ?= $(HOME)/raylib
EMSDK_PATH		?= $(HOME)/.emsdk
export PATH		:= $(EMSDK_PATH):$(PATH)

ASSETS_DIR      ?= data
BUILD_DIR       ?= build
OUTPUT_DIR      ?= bin/$(PLATFORM)/$(BUILD_MODE)
SRC_DIR         ?= .

#------------------------------------------------
# SRC FILES
SRC_FILES = \
	$(SRC_DIR)/main.c

#---------------------------------------------------------------------------------------------
# Define Compiler FLAGS
#-------------------------------
#  -Werror=pointer-arith    catch unportable code that does direct arithmetic on void pointers
CFLAGS = -Wextra
CFLAGS += -Wunused-result
CFLAGS += -Wunused-variable
CFLAGS += -Wpointer-arith
# CFLAGS += -I$(RAYLIB_PATH)/src
# CFLAGS += -isystem$(RAYLIB_PATH)/src

#-------------------------
# Debug/Release Flags
ifeq ($(BUILD_MODE),DEBUG)
CFLAGS += -D_DEBUG -g
#CFLAGS += -O0
CFLAGS += -O2 --profiling
endif
ifeq ($(BUILD_MODE),RELEASE)
CFLAGS += -DNDEBUG
# CFLAGS += -Werror
CFLAGS += -Os
endif

#---------------------------------------------------------------------------------------------
# Configure EMSCRIPTEN
CFLAGS += -std=gnu17
CFLAGS += -DGRAPHICS_API_OPENGL_ES2
CFLAGS += -flto

#---------------------------------------------------------------------------------------------
# Linking
#LDFLAGS = $(BUILD_DIR)/libraylib.a
LDFLAGS = -lidbfs.js
LDFLAGS += -s EXPORTED_RUNTIME_METHODS=['writeArrayToMemory','setValue']
LDFLAGS += -sASYNCIFY

WEB_SHELL	:= shell.html

#---------------------------------------------------------------------------------------------
# Targets
OBJS	:= $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC_FILES))

.PHONY:

all: $(PROJECT_NAME)

$(PROJECT_NAME): $(OBJS)
	@mkdir -p $(OUTPUT_DIR)
	"$(CC)" -o $(OUTPUT_DIR)/$(PROJECT_NAME).html $(OBJS) $(LDFLAGS) \
		-s USE_GLFW=3 \
		--shell-file $(WEB_SHELL) \
		-s ALLOW_MEMORY_GROWTH=1
#		--preload-file $(LOCAL_ASSETS)@res

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	"$(CC)" -c $< -o $@ -D$(PLATFORM) $(CFLAGS)

.PHONY: clean
clean:
	rm -rf $(OUTPUT_DIR)
	rm -rf $(BUILD_DIR)
#	make -C $(RAYLIB_PATH)/src clean