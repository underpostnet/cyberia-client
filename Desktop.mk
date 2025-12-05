include config.mk

CC	:= gcc
CXX	:= g++

#---------------------------------------------------------------------------------------------
BUILD_DIR		:= $(call lc,$(BUILD_DIR)/$(DETECTED_OS)/$(BUILD_MODE))
OUTPUT_DIR		:= $(call lc,$(OUTPUT_DIR)/$(DETECTED_OS)/$(BUILD_MODE))

#---------------------------------------------------------------------------------------------
# Specific compiler flags
ifeq ($(BUILD_MODE),RELEASE)
CFLAGS += -O3
endif

CFLAGS		+= -DPLATFORM_DESKTOP
CXXFLAGS	:= $(CFLAGS) #-std=c++23
# CFLAGS	+= -std=gnu23

#---------------------------------------------------------------------------------------------
# Linking flags
LDFLAGS = $(RAYLIB_PATH)/src/libraylib.a

ifeq ($(DETECTED_OS),Windows)
LDFLAGS += -lopengl32 -lgdi32 -lwinmm
else
LDFLAGS = -lGL -lm -lpthread -ldl -lrt
LDFLAGS += -lX11 -lXrandr -lXinerama -lXi -lXxf86vm -lXcursor
endif

ifeq ($(BUILD_MODE),RELEASE)
ifeq ($(DETECTED_OS),Windows)
LDFLAGS += -Wl,--subsystem windows
endif
endif

# Util variables
OBJS	:= $(addprefix $(BUILD_DIR),$(SRC_FILES:$(SRC_DIR)/%=$(OBJ_DIR)/%.o))

ifeq ($(DETECTED_OS),Windows)
DOT_EXE := .exe
endif
#---------------------------------------------------------------------------------------------
# Specific targets

$(PROJECT_NAME): libraylib $(OBJS)
	@mkdir -p $(OUTPUT_DIR)
	$(CXX) $(OBJS) -o $(OUTPUT_DIR)/$(PROJECT_NAME)$(DOT_EXE) $(LDFLAGS)

$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BUILD_DIR)/%.cpp.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) -c $< -o $@ $(CXXFLAGS)

libraylib:
	make -j 8 -C $(RAYLIB_PATH)/src raylib \
		PLATFORM=PLATFORM_DESKTOP \
		RAYLIB_BUILD_MODE=$(BUILD_MODE) \
		RAYLIB_LIBTYPE=STATIC \
		$(if $(filter $(BUILD_MODE),RELEASE), 2>/dev/null)
