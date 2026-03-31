include config.mk

CC				:= emcc
DEV_PORT		?= 8082
PROD_PORT		?= 8081

#---------------------------------------------------------------------------------------------
TARGET_BUILD_DIR		:= $(call lc,$(BUILD_DIR)/web/$(BUILD_MODE))
TARGET_OUTPUT_DIR		:= $(call lc,$(OUTPUT_DIR)/web/$(BUILD_MODE))

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
LDFLAGS += -lwebsocket.js
LDFLAGS += --js-library $(SRC_DIR)/js/services.js
LDFLAGS += $(RAYLIB_PATH)/src/libraylib.web.a

#---------------------------------------------------------------------------------------------
# Web target html container
WEB_SHELL := $(SRC_DIR)/shell.html

OUTPUT := $(TARGET_OUTPUT_DIR)/index.html
ASSETS := $(SRC_DIR)/public/splash.png@splash.png

#---------------------------------------------------------------------------------------------
# Util variables
OBJS	:= $(SRC_FILES:$(SRC_DIR)/%=$(TARGET_BUILD_DIR)/%.o)
OBJS	+= $(TARGET_BUILD_DIR)/cJSON.o

#---------------------------------------------------------------------------------------------
# Specific targets

.PHONY: all serve-development serve-production

all:link

serve-development: all
	-fuser -k $(DEV_PORT)/tcp 2>/dev/null; sleep 0.3
	python3 -m http.server $(DEV_PORT) --directory $(TARGET_OUTPUT_DIR)

serve-production:
	make -f Web.mk serve-development BUILD_MODE=RELEASE DEV_PORT=$(PROD_PORT)

link: libraylib $(OBJS)
	@mkdir -p $(TARGET_OUTPUT_DIR)
	@cp $(SRC_DIR)/public/favicon.ico $(TARGET_OUTPUT_DIR)/favicon.ico
	$(CC) -o $(OUTPUT) $(OBJS) $(LDFLAGS) \
		-s USE_GLFW=3 \
		--shell-file $(WEB_SHELL) \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s INITIAL_MEMORY=67108864 \
		-s STACK_SIZE=16777216 \
		-s ASYNCIFY_STACK_SIZE=1048576 \
		--preload-file $(ASSETS)

$(TARGET_BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS)

$(TARGET_BUILD_DIR)/cJSON.o: $(CJSON_PATH)/cJSON.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS)

libraylib:
	make -j 8 -C $(RAYLIB_PATH)/src raylib \
		PLATFORM=PLATFORM_WEB \
		RAYLIB_BUILD_MODE=$(BUILD_MODE) \
		RAYLIB_LIBTYPE=STATIC \
		CFLAGS_EXTRA="-Wno-deprecated-pragma -Wno-tautological-compare"

clean:
	-rm -rf $(TARGET_BUILD_DIR) $(TARGET_OUTPUT_DIR)

# Remove when safe to
serve_development: serve-development
serve_production: serve-production
web: all