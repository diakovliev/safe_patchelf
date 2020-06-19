Q:=@

TOOLCHAIN ?= /usr/bin
COMPILER ?= g++-8

CC := $(TOOLCHAIN)/$(COMPILER)

INCLUDE_DIRS := -I include

# DEBUG_CC_FLAGS ?= -O0 -g
DEBUG_CC_FLAGS ?= -O3

CC_FLAGS = -std=c++17 ${DEBUG_CC_FLAGS} ${INCLUDE_DIRS}

LDLIBS =

LDFLAGS =

SOURCE_DIR := src

MODULES := \
	FD \
	Args \
	main \

TARGET := safe_patchelf

SOURCES := $(foreach module,$(MODULES),$(SOURCE_DIR)/$(module).cpp)
OBJECTS := $(foreach module,$(MODULES),$(SOURCE_DIR)/$(module).o)

.PHONY: install clean $(OBJ_DIR) $(OUTPUT_DIR)

build: $(TARGET)

$(TARGET): $(OBJECTS)
	$(Q)echo LINK $@
	$(Q)$(CC) $(LDFLAGS) $(LDLIBS) -s $^ -o $@

$(OBJECTS): $(SOURCE_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	$(Q)echo CPP $^
	$(Q)$(CC) $(CC_FLAGS) -c $< -o $@

clean:
	$(Q)echo RM $(OBJECTS) $(TARGET)
	$(Q)rm -rf $(OBJECTS) $(TARGET)
