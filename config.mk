PROJECT_NAME	= project-name

# Build mode for project: DEBUG or RELEASE
BUILD_MODE      ?= DEBUG

ASSETS_DIR      ?= data
BUILD_DIR       ?= build
LIBS_DIR		?= libs
OUTPUT_DIR      ?= bin
SRC_DIR         ?= src
TOOLS_DIR       ?= tools

#------------------------------------------------
# Common sources to include
SRC_FILES = \
	$(wildcard $(SRC_DIR)/*.c)

#---------------------------------------------------------------------------------------------
# Define common compiler flags
#-------------------------------
#  -Werror=pointer-arith    catch unportable code that does direct arithmetic on void pointers
CFLAGS = -Wextra
CFLAGS += -Wunused-result
CFLAGS += -Wunused-variable
CFLAGS += -Wpointer-arith
CFLAGS += -fdiagnostics-color=always

ifeq ($(BUILD_MODE),RELEASE)
CFLAGS += -DNDEBUG
CFLAGS += -O3
CFLAGS += -pedantic
# CFLAGS += -Werror
else
CFLAGS += -D_DEBUG -g
endif

#------------------------------------------------
# Raylib Dependendy
RAYLIB_PATH := $(LIBS_DIR)/raylib
CFLAGS += -I$(RAYLIB_PATH)/src -isystem$(RAYLIB_PATH)/src

#------------------------------------------------
# cJSON Dependency
CJSON_PATH := $(LIBS_DIR)/cJSON
CFLAGS += -I$(CJSON_PATH)

#---------------------------------------------------------------------------------------------
# Common Targets
.PHONY: $(PROJECT_NAME) clean full-clean

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(OUTPUT_DIR)

full-clean: clean
	make -C $(RAYLIB_PATH)/src clean

#---------------------------------------------------------------------------------------------
# Util functions
lc = $(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$1))))))))))))))))))))))))))

ifeq ($(OS),Windows_NT)
DETECTED_OS := Windows
else
DETECTED_OS := $(shell uname)  # or "uname -s"
endif