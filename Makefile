KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
BUILD_DIR := $(shell pwd)/build

USER_PROGRAM = user
TEST_PROGRAM = test

all: module user test

module:
	mkdir -p $(BUILD_DIR)
	cp src/stealthmem.c Makefile Kbuild $(BUILD_DIR)/
	$(MAKE) -C $(KERNEL_DIR) M=$(BUILD_DIR) modules
	@echo generated kernel module

user: test/user.c
	gcc -Wall -Wextra -O3 -o build/$(USER_PROGRAM) $<
	@echo generated test executable

test: test/test.c
	gcc -Wall -Wextra -O3 -o build/$(TEST_PROGRAM) $<
	@echo generated test executable

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(BUILD_DIR) clean
	rm -r $(BUILD_DIR)

load:
	sudo insmod $(BUILD_DIR)/stealthmem.ko
	sudo chmod 666 /dev/stealthmem

unload:
	sudo rmmod stealthmem

.PHONY: all module test clean load unload
