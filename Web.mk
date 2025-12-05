include config.mk

CC				:= emcc
DEV_PORT		?= 8081
PROD_PORT		?= 8081

#---------------------------------------------------------------------------------------------
BUILD_DIR		:= $(call lc,$(BUILD_DIR)/$(PLATFORM)/$(BUILD_MODE))
OUTPUT_DIR		:= $(call lc,$(OUTPUT_DIR)/$(PLATFORM)/$(BUILD_MODE))

#---------------------------------------------------------------------------------------------
# Specific compiler flags
CFLAGS += -DGRAPHICS_API_OPENGL_ES2
CFLAGS += -flto

ifneq ($(BUILD_MODE),RELEASE)
CFLAGS += --profiling
CFLAGS += -O0 # -Og doesnt not work in emcc
endif

CFLAGS		+= -DPLATFORM_WEB
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

.PHONY: web serve_development serve_production clean

web: $(PROJECT_NAME)

serve_development: web
	python3 -m http.server $(DEV_PORT) --directory $(OUTPUT_DIR)

serve_production:
	$(MAKE) -f Web.mk serve_development BUILD_MODE=RELEASE DEV_PORT=$(PROD_PORT)

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

$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BUILD_DIR)/cJSON.o: $(LIBS_DIR)/cJSON/cJSON.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS)

libraylib:
ifeq ($(OS),Windows_NT)
	@cp emar.bat emcc.bat $(RAYLIB_PATH)/src
endif
	make -j 8 -C $(RAYLIB_PATH)/src raylib \
		PLATFORM=PLATFORM_WEB \
		RAYLIB_BUILD_MODE=$(BUILD_MODE) \
		RAYLIB_LIBTYPE=STATIC \
		$(if $(filter $(BUILD_MODE),RELEASE), 2>/dev/null)
ifeq ($(OS),Windows_NT)
	@rm $(RAYLIB_PATH)/src/emar.bat $(RAYLIB_PATH)/src/emcc.bat
endif
