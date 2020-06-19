Q:=@

TARGET := safe_patchelf

TOOLCHAIN ?= /usr/bin
COMPILER ?= g++-8

INCLUDE_DIRS := -I include

BUILD_VARIANT ?= Release
USE_CCACHE ?= yes

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

CPP_FLAGS = -std=c++17 -g -Wall -Wextra ${DEBUG_CC_FLAGS} ${INCLUDE_DIRS}

LD := $(TOOLCHAIN)/$(COMPILER)
CPP := $(TOOLCHAIN)/$(COMPILER)

ifeq ($(USE_CCACHE),yes)
 CPP := ccache $(CPP)
endif

LDLIBS =

LDFLAGS =

SOURCE_DIR := src

HEADERS := \
	include/elf/elf.h \
	include/$(TARGET)/commons.h \
	include/$(TARGET)/FD.h \
	include/$(TARGET)/Args.h \
	include/$(TARGET)/Elf.h \


MODULES := \
	FD \
	Args \
	main \


SOURCES := $(foreach module,$(MODULES),$(SOURCE_DIR)/$(module).cpp)
OBJECTS := $(foreach module,$(MODULES),$(SOURCE_DIR)/$(module).o)

.PHONY: install clean $(OBJ_DIR) $(OUTPUT_DIR)

build: $(TARGET)

$(TARGET): $(OBJECTS)
	$(Q)echo LINK $@
	$(Q)$(LD) $(LDFLAGS) $(LDLIBS) -s $^ -o $@
ifeq ($(STRIP_OUTPUT),yes)
	$(Q)echo STRIP $@
	$(Q)strip --strip-unneeded $@
endif

$(OBJECTS): $(SOURCE_DIR)/%.o: $(SOURCE_DIR)/%.cpp $(HEADERS)
	$(Q)echo CPP $<
	$(Q)$(CPP) $(CPP_FLAGS) -c $< -o $@

clean:
	$(Q)echo RM $(OBJECTS) $(TARGET)
	$(Q)rm -rf $(OBJECTS) $(TARGET)
