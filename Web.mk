include config.mk

CC				:= emcc
DEV_PORT		?= 8082
PROD_PORT		?= 8081

#---------------------------------------------------------------------------------------------
target_build_dir	:= $(call lc,$(BUILD_DIR)/web/$(BUILD_MODE))
target_output_dir	:= $(call lc,$(OUTPUT_DIR)/web/$(BUILD_MODE))

#---------------------------------------------------------------------------------------------
# Specific compiler flags
CFLAGS	+= -DPLATFORM_WEB
CFLAGS	+= -DGRAPHICS_API_OPENGL_ES2
CFLAGS	+= -flto
CFLAGS	+= -Wno-unused-parameter

ifneq ($(BUILD_MODE),RELEASE)
CFLAGS	+= --profiling
CFLAGS	+= -O0 # -Og doesnt not work in emcc
endif

#---------------------------------------------------------------------------------------------
# Linking flags
LDFLAGS = -lidbfs.js
LDFLAGS += -s 'EXPORTED_RUNTIME_METHODS=["writeArrayToMemory","setValue","allocateUTF8"]'
LDFLAGS += -sASYNCIFY
LDFLAGS += -lwebsocket.js
LDFLAGS += --js-library $(SRC_DIR)/js/services.js
LDFLAGS += --js-library $(SRC_DIR)/js/interact_overlay.js
LDFLAGS += --js-library $(SRC_DIR)/js/notify_badge.js
LDFLAGS += -sEXPORTED_FUNCTIONS='["_main","_c_send_ws_message","_c_open_dialogue_from_js","_c_interact_overlay_did_close"]'

#---------------------------------------------------------------------------------------------
# Web target html container
WEB_SHELL := $(SRC_DIR)/shell.html

OUTPUT := $(target_output_dir)/index.html
ASSETS := $(SRC_DIR)/public/splash.png@splash.png

#---------------------------------------------------------------------------------------------
# Util variables
OBJS	:= $(src_files:$(SRC_DIR)/%=$(target_build_dir)/%.o)
OBJS	+= $(target_build_dir)/cJSON.o
OBJS	+= $(target_build_dir)/libraylib.web.a

#---------------------------------------------------------------------------------------------
# Platform Specific targets

.PHONY: all serve-development serve-production libraylib web

all: link

libraylib: $(target_build_dir)/libraylib.web.a

web: all

serve-development: all
	-fuser -k $(DEV_PORT)/tcp 2>/dev/null; sleep 0.3
	python3 scripts/serve.py $(DEV_PORT) $(target_output_dir)

serve-production:
	make -f Web.mk serve-development BUILD_MODE=RELEASE DEV_PORT=$(PROD_PORT)

link: $(OBJS)
	@mkdir -p $(target_output_dir)
	@cp $(SRC_DIR)/public/favicon.ico $(target_output_dir)/favicon.ico
	$(CC) -o $(OUTPUT) $(OBJS) $(LDFLAGS) \
		-s USE_GLFW=3 \
		--shell-file $(WEB_SHELL) \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s INITIAL_MEMORY=67108864 \
		-s STACK_SIZE=16777216 \
		-s ASYNCIFY_STACK_SIZE=1048576 \
		--preload-file $(ASSETS)

$(target_build_dir)/%.c.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS) -MMD -MP

$(target_build_dir)/cJSON.o: $(CJSON_PATH)/cJSON.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS)

# Raylib dep
$(target_build_dir)/libraylib.web.a:
	@mkdir -p $(target_build_dir)
	make -j 8 -C $(RAYLIB_PATH)/src raylib \
		PLATFORM=PLATFORM_WEB \
		RAYLIB_BUILD_MODE=$(BUILD_MODE) \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=$(CURDIR)/$(target_build_dir) \
		CFLAGS_EXTRA="-Wno-deprecated-pragma -Wno-tautological-compare -D__EMSCRIPTEN_major__=__EMSCRIPTEN_MAJOR__ -D__EMSCRIPTEN_minor__=__EMSCRIPTEN_MINOR__ -D__EMSCRIPTEN_tiny__=__EMSCRIPTEN_TINY__"
	make -C $(RAYLIB_PATH)/src clean

clean:
	-rm -rf $(BUILD_DIR) $(OUTPUT_DIR)
