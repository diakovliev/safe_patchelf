Q:=@

TOOLCHAIN ?= /usr/bin
COMPILER ?= g++-8

CC := $(TOOLCHAIN)/$(COMPILER)

INCLUDE_DIRS := -I include

BUILD_VARIANT ?= Release

ifeq (${BUILD_VARIANT},Release)
 DEBUG_CC_FLAGS ?= -O3 -DNDEBUG
 STRIP_OUTPUT ?= yes
else
 ifeq (${BUILD_VARIANT},Debug)
  DEBUG_CC_FLAGS ?= -O0 -DDEBUG
  STRIP_OUTPUT ?= no
 else
  $(error Unknown BUILD_VARIANT: $(BUILD_VARIANT)!)
 endif
endif


CC_FLAGS = -std=c++17 -g ${DEBUG_CC_FLAGS} ${INCLUDE_DIRS}

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
ifeq ($(STRIP_OUTPUT),yes)
	$(Q)echo STRIP $@
	$(Q)strip --strip-unneeded $@
endif

$(OBJECTS): $(SOURCE_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	$(Q)echo CPP $^
	$(Q)$(CC) $(CC_FLAGS) -c $< -o $@

clean:
	$(Q)echo RM $(OBJECTS) $(TARGET)
	$(Q)rm -rf $(OBJECTS) $(TARGET)
