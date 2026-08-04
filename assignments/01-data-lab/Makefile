CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -fwrapv
BUILD_DIR := ../../build/01-data-lab
TARGET := $(BUILD_DIR)/test_bits

.PHONY: all test clean

all: $(TARGET)

$(TARGET): bits.c bits.h test_bits.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) bits.c test_bits.c -o $(TARGET)

test: $(TARGET)
	$(TARGET)

clean:
	rm -rf $(BUILD_DIR)
