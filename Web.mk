include config.mk

CC				:= emcc

#---------------------------------------------------------------------------------------------
target_build_dir	:= $(BUILD_DIR)/web/$(BUILD_MODE)

#---------------------------------------------------------------------------------------------
# EMSCRIPTEN needed flags that I hate
CFLAGS += -Wno-gnu-zero-variadic-macro-arguments

ifneq ($(BUILD_MODE),RELEASE)
CFLAGS += --profiling
CFLAGS += -O0 # -Og does not work in emcc
CFLAGS += -DCYBERIA_LOG_LEVEL=4
endif

#---------------------------------------------------------------------------------------------
# Linking flags
LDFLAGS = -lidbfs.js
LDFLAGS += -lwebsocket.js
LDFLAGS += -s 'EXPORTED_RUNTIME_METHODS=["writeArrayToMemory","setValue","allocateUTF8"]'
LDFLAGS += -sASYNCIFY
LDFLAGS += -sFETCH=1
LDFLAGS += --js-library $(SRC_DIR)/js/interact_overlay.js
LDFLAGS += -sEXPORTED_FUNCTIONS='["_main","_c_send_chat_binary"]'

#---------------------------------------------------------------------------------------------
# Web target html container
WEB_SHELL := $(SRC_DIR)/shell.html

OUTPUT := $(OUTPUT_DIR)/index.html
ASSETS := $(ASSETS_DIR)/splash.png@splash.png

#---------------------------------------------------------------------------------------------
# Util variables
OBJS	:= $(src_files:$(SRC_DIR)/%=$(target_build_dir)/%.o)

# TODO: confirm if DEPS is really working, if not, remove it
# Auto-generated header dependency files produced by -MMD -MP.
# Must be included AFTER OBJS is defined so Make tracks .h changes.
DEPS	:= $(OBJS:%.o=%.d)
-include $(DEPS)
OBJS	+= $(target_build_dir)/cJSON.o
OBJS	+= $(target_build_dir)/libraylib.web.a

#---------------------------------------------------------------------------------------------
# Platform Specific targets

.PHONY: all clean

all: link

link: $(OBJS)
	@mkdir -p $(OUTPUT_DIR)/fonts
	@cp $(ASSETS_DIR)/favicon.ico $(OUTPUT_DIR)/favicon.ico
	@cp $(ASSETS_DIR)/fonts/Jersey15-Regular.ttf $(OUTPUT_DIR)/fonts/Jersey15-Regular.ttf
	$(CC) -o $(OUTPUT) $(OBJS) $(LDFLAGS) $(LDFLAGS_RELEASE) \
		-s USE_GLFW=3 \
		--shell-file $(WEB_SHELL) \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s INITIAL_MEMORY=67108864 \
		-s STACK_SIZE=16777216 \
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
		RAYLIB_RELEASE_PATH=$(CURDIR)/$(target_build_dir)
	make -C $(RAYLIB_PATH)/src clean

clean:
	-rm -rf $(BUILD_DIR) $(OUTPUT_DIR)
