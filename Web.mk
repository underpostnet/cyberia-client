include config.mk

PLATFORM		:= PLATFORM_WEB
CC				:= emcc

#---------------------------------------------------------------------------------------------
BUILD_DIR		:= $(call lc,$(BUILD_DIR)/$(PLATFORM)/$(BUILD_MODE))
OUTPUT_DIR		:= $(call lc,$(OUTPUT_DIR)/$(PLATFORM)/$(BUILD_MODE))

#---------------------------------------------------------------------------------------------
# Specific compiler flags
CFLAGS += -DGRAPHICS_API_OPENGL_ES2
CFLAGS += -flto

ifneq ($(BUILD_MODE),RELEASE)
CFLAGS += --profiling
CFLAGS += -O0 # -Og doesn not work in emcc
endif

CXXFLAGS := $(CFLAGS) #-std=c++23
# CFLAGS	+= -std=gnu23

#---------------------------------------------------------------------------------------------
# Linking flags
LDFLAGS = -lidbfs.js
LDFLAGS += -s 'EXPORTED_RUNTIME_METHODS=["writeArrayToMemory","setValue"]'
LDFLAGS += -sASYNCIFY
LDFLAGS += -s 'ASYNCIFY_IMPORTS=["js_fetch_object_layer", "js_fetch_binary"]'
LDFLAGS += -lwebsocket.js
LDFLAGS += --js-library $(SRC_DIR)/js/services.js
LDFLAGS += $(RAYLIB_PATH)/src/libraylib.web.a

#---------------------------------------------------------------------------------------------
# Web target html container
WEB_SHELL := $(SRC_DIR)/shell.html

WEB_ARTIFACTS := $(OUTPUT_DIR)/index.html
ARTIFACTS_ARCHIVES := --preload-file $(SRC_DIR)/public/splash.png@splash.png

#---------------------------------------------------------------------------------------------
# Util variables
OBJS	:= $(addprefix $(BUILD_DIR),$(SRC_FILES:$(SRC_DIR)/%=$(OBJ_DIR)/%.o))
OBJS	+= $(BUILD_DIR)/cJSON.o

#---------------------------------------------------------------------------------------------
# Specific targets

.PHONY: web serve clean

web: $(PROJECT_NAME)

serve: web
	python3 -m http.server 8080 --directory $(OUTPUT_DIR)

clean:
	rm -rf $(BUILD_DIR) $(OUTPUT_DIR)

$(PROJECT_NAME): libraylib $(OBJS)
	@mkdir -p $(OUTPUT_DIR)
	@cp $(SRC_DIR)/public/favicon.ico $(OUTPUT_DIR)/favicon.ico
	$(CC) -o $(WEB_ARTIFACTS) $(OBJS) $(LDFLAGS) \
		-s USE_GLFW=3 \
		--shell-file $(WEB_SHELL) \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s INITIAL_MEMORY=67108864 \
		-s STACK_SIZE=16777216 \
		-s ASYNCIFY_STACK_SIZE=1048576 \
		$(ARTIFACTS_ARCHIVES)
#		--preload-file $(LOCAL_ASSETS)@res

$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ -D$(PLATFORM) $(CFLAGS)

$(BUILD_DIR)/%.cpp.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ -D$(PLATFORM) $(CXXFLAGS)

$(BUILD_DIR)/cJSON.o: $(LIBS_DIR)/cJSON/cJSON.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ -D$(PLATFORM) $(CFLAGS)

libraylib:
ifeq ($(OS),Windows_NT)
	@cp emar.bat emcc.bat $(RAYLIB_PATH)/src
endif
	make -j 8 -C $(RAYLIB_PATH)/src raylib \
		PLATFORM=PLATFORM_WEB \
		RAYLIB_BUILD_MODE=$(BUILD_MODE) \
		RAYLIB_LIBTYPE=STATIC
ifeq ($(OS),Windows_NT)
	@rm $(RAYLIB_PATH)/src/emar.bat $(RAYLIB_PATH)/src/emcc.bat
endif
