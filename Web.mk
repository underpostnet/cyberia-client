include config.mk

CC				:= emcc
DEV_PORT		?= 8082
PROD_PORT		?= 8081

#---------------------------------------------------------------------------------------------
target_build_dir	:= $(BUILD_DIR)/web/$(BUILD_MODE)

#---------------------------------------------------------------------------------------------
# EMSCRIPTEN needed flags that I hate
CFLAGS += -Wno-gnu-zero-variadic-macro-arguments

# Endpoint injection — override on the command line:
#   make -f Web.mk all WS_URL='wss://server.example.com/ws' \
#                      API_BASE_URL='https://www.example.com'
# RELEASE picks the production URLs by default; DEBUG keeps localhost.
ifeq ($(BUILD_MODE),RELEASE)
WS_URL       ?= wss://server.cyberiaonline.com/ws
API_BASE_URL ?= https://www.cyberiaonline.com
CFLAGS  += -DCYBERIA_LOG_LEVEL=3
LDFLAGS_RELEASE := --closure 1
else
WS_URL       ?= ws://localhost:8081/ws
API_BASE_URL ?= http://localhost:4005
CFLAGS += --profiling
CFLAGS += -O0 # -Og does not work in emcc
CFLAGS += -DCYBERIA_LOG_LEVEL=4
endif
CFLAGS += -DWS_URL_LITERAL='"$(WS_URL)"'
CFLAGS += -DAPI_BASE_URL_LITERAL='"$(API_BASE_URL)"'

#---------------------------------------------------------------------------------------------
# Linking flags
LDFLAGS = -lidbfs.js
LDFLAGS += -lwebsocket.js
LDFLAGS += -s 'EXPORTED_RUNTIME_METHODS=["writeArrayToMemory","setValue","allocateUTF8"]'
# ASYNCIFY removed — the main loop is driven by emscripten_set_main_loop and
# nothing calls emscripten_sleep / coroutine helpers. Keeping the flag paid
# the ~20% code-size and 10-30% runtime overhead for nothing.
LDFLAGS += -sFETCH=1
LDFLAGS += --js-library $(SRC_DIR)/js/services.js
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

# Auto-generated header dependency files produced by -MMD -MP.
# Must be included AFTER OBJS is defined so Make tracks .h changes.
DEPS	:= $(OBJS:%.o=%.d)
-include $(DEPS)
OBJS	+= $(target_build_dir)/cJSON.o
OBJS	+= $(target_build_dir)/libraylib.web.a

#---------------------------------------------------------------------------------------------
# Platform Specific targets

.PHONY: all clean serve-development serve-production

all: link

serve-development: all
	-fuser -k $(DEV_PORT)/tcp 2>/dev/null; sleep 0.3
	python3 server.py $(DEV_PORT) $(OUTPUT_DIR)

serve-production:
	make -f Web.mk serve-development BUILD_MODE=RELEASE DEV_PORT=$(PROD_PORT)

link: $(OBJS)
	@mkdir -p $(OUTPUT_DIR)
	@cp $(ASSETS_DIR)/favicon.ico $(OUTPUT_DIR)/favicon.ico
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
