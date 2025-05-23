KDIR ?= /lib/modules/$(shell uname -r)/build
BUILD_DIR := $(shell pwd)/build

TEST_PROGRAM = stealthmem_test

all: module test

module:
	mkdir -p $(BUILD_DIR)
	cp src/stealthmem.c Makefile Kbuild $(BUILD_DIR)/
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) modules
	@echo generated kernel module

test: test/stealthmem_test.c
	gcc -Wall -O3 -o build/$(TEST_PROGRAM) $<
	@echo generated test executable

clean:
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) clean
	rm -r $(BUILD_DIR)

load:
	sudo insmod $(BUILD_DIR)/stealthmem.ko
	sudo chmod 666 /dev/stealthmem

unload:
	sudo rmmod stealthmem

.PHONY: all module test clean load unload
