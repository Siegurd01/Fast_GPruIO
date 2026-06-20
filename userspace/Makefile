CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra
BUILD_DIR := build

.PHONY: all clean

all: $(BUILD_DIR)/fast_gpruio_cmd

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/fast_gpruio_cmd: fast_gpruio_cmd.c fast_gpruio_lib.c fast_gpruio_lib.h ../include/fast_gpruio_shared.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I../include -I. fast_gpruio_cmd.c fast_gpruio_lib.c -o $@

clean:
	rm -rf $(BUILD_DIR)
