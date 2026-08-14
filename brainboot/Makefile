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
         src/ident.c src/bootmenu.c src/policy.c src/shell.c \
         src/i2c.c src/ebl.c src/eboot.c \
         src/net_proto.c src/rndis_proto.c src/enet_fec.c \
         src/usb_xport.c src/nand_ipl.c src/xport.c
S_SRC := src/start.S

C_OBJ := $(patsubst src/%.c,build/%.o,$(C_SRC))
S_OBJ := $(patsubst src/%.S,build/%.o,$(S_SRC))
OBJ   := $(S_OBJ) $(C_OBJ)

.PHONY: all clean test host images sd emmc splash

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

# Path A: SD Card-BOOT EXE (stock first-stage stays). Also emits CFG/DEV.
images: build/brainboot.bin
	python3 tools/mkimage.py --bin build/brainboot.bin --outdir build

sd: images

# Path B: replace the eMMC type-0x53 SB stream (i.MX28 ROM / qemu-brain).
emmc: build/brainboot.bin
	python3 tools/mksb.py --bin build/brainboot.bin --out build/brainboot.sb
	python3 tools/mk_emmc_boot.py --sb build/brainboot.sb --out build/emmc_brainboot.img

host: build/host_test
	./build/host_test

build/host_test: tests/host_test.c src/string.c src/bootconfig.c src/fat.c src/config.c \
                 src/ident.c src/bootmenu.c src/policy.c src/keyboard.c \
                 src/net_proto.c src/rndis_proto.c
	@mkdir -p build
	$(HOSTCC) -std=gnu11 -Wall -Wextra -DHOST_BUILD -Iinclude -no-pie \
	    -o $@ tests/host_test.c src/string.c src/bootconfig.c src/fat.c src/config.c \
	        src/ident.c src/bootmenu.c src/policy.c src/keyboard.c \
	        src/net_proto.c src/rndis_proto.c

build/render_splash: tests/render_splash.c src/splash.c src/font.c
	@mkdir -p build
	$(HOSTCC) -std=gnu11 -Wall -Wextra -DHOST_BUILD -Iinclude -no-pie \
	    -o $@ tests/render_splash.c src/splash.c src/font.c

build/splash.ppm: build/render_splash
	./build/render_splash $@
	-convert $@ build/splash.png

splash: build/splash.ppm

test: host splash
	python3 tests/test_protocol.py

clean:
	rm -rf build
