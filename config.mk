# RELEASE | DEBUG
BUILD_MODE      ?= DEBUG

# -- Master folders
ASSETS_DIR      ?= assets
BUILD_DIR       ?= build
LIBS_DIR		?= libs
OUTPUT_DIR      ?= bin
SRC_DIR         ?= src

#---------------------------------------------------------------------------------------------
# Define common compiler flags
#-------------------------------
CFLAGS = -I$(SRC_DIR)
CFLAGS += -Wextra
CFLAGS += -Wpointer-arith
CFLAGS += -fdiagnostics-color=always

# emscripten prefers gnu extensions, but let's try to reduce their usage
CFLAGS += -std=gnu11
CFLAGS += -pedantic

ifeq ($(BUILD_MODE),RELEASE)
CFLAGS += -DNDEBUG
CFLAGS += -O3
CFLAGS += -Wunused-result
CFLAGS += -Wunused-variable -Wunused-const-variable
# CFLAGS += -Werror
else
CFLAGS += -DCYBERIA_DEBUG -g
CFLAGS += -Wno-unused-parameter
endif

#------------------------------------------------
# Common sources to include
src_files = \
	$(wildcard $(SRC_DIR)/*.c) \
	$(wildcard $(SRC_DIR)/js/*.c) \
	$(wildcard $(SRC_DIR)/network/*.c) \
	$(wildcard $(SRC_DIR)/ui/*.c) \
	$(wildcard $(SRC_DIR)/input/*.c) \
	$(wildcard $(SRC_DIR)/domain/*.c)

#------------------------------------------------
# Raylib Dependendy
RAYLIB_PATH ?= $(LIBS_DIR)/raylib
CFLAGS += -I$(RAYLIB_PATH)/src -isystem$(RAYLIB_PATH)/src

#------------------------------------------------
# cJSON Dependency
CJSON_PATH := $(LIBS_DIR)/cJSON
CFLAGS += -I$(CJSON_PATH)

#------------------------------------------------
# CYBERIA config
WS_URL ?= ws://localhost:8081/ws
API_BASE ?= http://localhost:4005

CFLAGS  += -DWS_URL_OVERRIDE=$(WS_URL) -DAPI_BASE_URL_OVERRIDE=$(API_BASE)
ifeq ($(BUILD_MODE),RELEASE)
CFLAGS  += -DCYBERIA_LOG_LEVEL=3
else
CFLAGS += -DCYBERIA_LOG_LEVEL=4
endif