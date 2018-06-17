CC        := gcc
SRC_DIR   := src
BUILD_DIR := build
BINARY    := chad
CFLAGS    := -g -Wall -O3 -fPIC

SOURCES := $(shell find $(SRC_DIR) -name '*.c' | sort -k 1nr | cut -f2-)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS    := $(OBJECTS:.o=.d)

$(BINARY) : $(OBJECTS)
	$(CC) $(OBJECTS) -o $@

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) && mkdir $(BUILD_DIR)

-include $(DEPS)