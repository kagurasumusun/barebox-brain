# SPDX-License-Identifier: GPL-2.0-or-later
CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size
HOSTCC  ?= gcc

CFLAGS := -std=gnu11 -Wall -Wextra -Werror=implicit-function-declaration \
          -Wno-unused-parameter \
          -fno-builtin -ffreestanding -fno-stack-protector -fno-pic \
          -marm -march=armv5te -mtune=arm926ej-s -msoft-float \
          -O2 -g -Iinclude
LIBGCC  := $(shell $(CC) -print-libgcc-file-name)
LDFLAGS := -nostdlib -static -T ldscripts/brainboot.ld -Wl,--nmagic

C_SRC := src/string.c src/delay.c src/uart.c src/gpio.c \
         src/clock.c src/pinmux.c src/ssp_mmc.c src/fat.c src/bootconfig.c \
         src/boot_wince.c src/boot_linux.c src/font.c src/lcd.c src/splash.c \
         src/keyboard.c src/menu.c src/main.c src/ui.c src/config.c \
         src/ident.c src/bootmenu.c src/policy.c src/shell.c
S_SRC := src/start.S

C_OBJ := $(patsubst src/%.c,build/%.o,$(C_SRC))
S_OBJ := $(patsubst src/%.S,build/%.o,$(S_SRC))
OBJ   := $(S_OBJ) $(C_OBJ)

.PHONY: all clean test host images

all: build/brainboot.bin images

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.S
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/brainboot.elf: $(OBJ) ldscripts/brainboot.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LIBGCC)
	$(SIZE) $@

build/brainboot.bin: build/brainboot.elf
	$(OBJCOPY) -O binary $< $@
	@ls -l $@

images: build/brainboot.bin
	python3 tools/mkimage.py --bin build/brainboot.bin --outdir build

host: build/host_test
	./build/host_test

build/host_test: tests/host_test.c src/string.c src/bootconfig.c src/fat.c src/config.c \
                 src/ident.c src/bootmenu.c src/policy.c
	@mkdir -p build
	$(HOSTCC) -std=gnu11 -Wall -Wextra -DHOST_BUILD -Iinclude -no-pie \
	    -o $@ tests/host_test.c src/string.c src/bootconfig.c src/fat.c src/config.c \
	        src/ident.c src/bootmenu.c src/policy.c

test: host
	python3 tests/test_protocol.py

clean:
	rm -rf build
