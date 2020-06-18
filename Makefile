TOOLCHAIN ?= /usr/bin
COMPILER ?= g++-8

CC := $(TOOLCHAIN)/$(COMPILER)

CC_FLAGS = -std=c++17 -O0

LDLIBS =

LDFLAGS =

SOURCE_DIR := src

MODULES := \
	main \

TARGET := safe_patchelf

SOURCES := $(foreach module,$(MODULES),$(SOURCE_DIR)/$(module).cpp)
OBJECTS := $(foreach module,$(MODULES),$(SOURCE_DIR)/$(module).o)

.PHONY: install clean $(OBJ_DIR) $(OUTPUT_DIR)

build: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(LDLIBS) -s $^ -o $@

$(OBJECTS): $(SOURCE_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	$(CC) $(CC_FLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECTS) $(TARGET)
