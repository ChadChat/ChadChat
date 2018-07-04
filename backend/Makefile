CC        := gcc
SRC_DIR   := src
BUILD_DIR := build
SUB_DIR	  := utils
BINARY    := chad
LIBS	  := -lcrypto -lm
PREPROC	  := -D DEBUG_ON
CFLAGS    := -g -Wall -O3 -fPIC

# -lm = math library
# -lcrypto = crypto library for b64 and sha1

SOURCES := $(shell find $(SRC_DIR) -name '*.c' | sort -k 1nr | cut -f2-)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS    := $(OBJECTS:.o=.d)

$(BINARY) : $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $@

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(PREPROC) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) && mkdir $(BUILD_DIR) && mkdir $(BUILD_DIR)/$(SUB_DIR)

-include $(DEPS)