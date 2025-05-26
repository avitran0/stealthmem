KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
BUILD_DIR := $(shell pwd)/build

MEM_PROGRAM = mem
MOUSE_PROGRAM = mouse
KEY_PROGRAM = key

all: module mem mouse key

module:
	mkdir -p $(BUILD_DIR)
	cp src/stealthmem.c Makefile Kbuild $(BUILD_DIR)/
	$(MAKE) -C $(KERNEL_DIR) M=$(BUILD_DIR) modules
	@echo generated kernel module

mem: test/mem.c
	gcc -Wall -Wextra -O3 -o build/$(MEM_PROGRAM) $<
	@echo generated test executable

mouse: test/mouse.c
	gcc -Wall -Wextra -O3 -o build/$(MOUSE_PROGRAM) $<
	@echo generated mouse executable

key: test/key.c
	gcc -Wall -Wextra -O3 -o build/$(KEY_PROGRAM) $<
	@echo generated key executable

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(BUILD_DIR) clean
	rm -r $(BUILD_DIR)

load:
	sudo insmod $(BUILD_DIR)/stealthmem.ko
	sudo chmod 666 /dev/stealthmem

unload:
	sudo rmmod stealthmem

.PHONY: all module test clean load unload
