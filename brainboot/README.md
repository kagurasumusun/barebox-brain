# brainboot

Lightweight dual-OS bootloader for **SHARP Brain PW-AJ2** (internal names
`ED-AA2` / `ED-SH6`, NXP i.MX28, 128 MiB DRAM, 8 GiB eMMC).

It boots:

* Windows CE 6 NK from eMMC (`0x120000`) or DiagOS (`0x4120000`)
* A B000FF `EDSH6EXE.BIN` from a FAT16/FAT32 microSD
* Linux `zImage` + `imx28-pwsh6.dtb` from microSD

## What this is / is not

This directory is a self-contained ARM926EJ-S freestanding image. It does
**not** replace the i.MX28 Boot ROM, and it does **not** re-initialise DDR.
The Boot ROM + first-stage SB stream must already have DRAM running, the
same way the stock second-stage is started.

It has been:

* compiled with `arm-none-eabi-gcc` for ARM926EJ-S
* protocol-tested on the host (BootConfig, DevInfo, B000FF, ECEC, MBR/FAT16
  read+write, `BRAINBOO.CFG` parse/format, CRC32)

It has **not** been executed on a physical PW-AJ2 in this workspace. Treat
the first install as a non-destructive SD-card payload (see below).

## Build

```
sudo apt install gcc-arm-none-eabi
cd brainboot
make            # ARM image + packed SD EXE
make test       # host protocol tests
```

Outputs in `build/`:

| file | use |
|---|---|
| `brainboot.elf` / `brainboot.bin` | raw image, linked at `0x40200000` |
| `EDSH6EXE.BIN` | B000FF wrapper, load address `0xA0200000` |
| `brainboot.ecec.bin` | same payload with an ECEC stamp at `+0x40` |

## Install (non-destructive, recommended first path)

The stock second-stage already understands B000FF on microSD.

1. Format a microSD as **MBR + FAT16**, partition type `0x0E`, boot flag
   `0x80`. The CHS start **and** end sector fields (low 6 bits) must be
   non-zero or the stock FAT driver rejects the card.
2. Copy `build/EDSH6EXE.BIN` to the root.
3. Optional: `EDSH6CFG.BIN` (see below), `zImage`, `imx28-pwsh6.dtb`.
4. Insert the card and power on. The stock loader copies the EXE to
   `0xA0200000` and jumps; brainboot takes over from there.

Do **not** overwrite the eMMC type-`0x53` SB partition until this SD path
has been confirmed on the unit.

## On-screen UI

854×480 dark console matching the dumped BRAINBOOT mockup: header
(`SHARP BRAIN PW-AJ2`), 3× `BRAINBOOT` title, left menu in a framed box,
right **SYSTEM STATUS** / **DEVICE INFO**, footer keycaps.

Status rows are **probes**, not wishful OK:

| row | meaning |
|---|---|
| OCRAM / DRAM probe | write/read signature at `0x18000` / `0x47E00000` |
| eMMC / SD | `mmc_init` / `fat_mount` result |
| Boot configuration | valid BootConfig checksum |
| OCOTP | raw `HW_OCOTP_LOCK` (not claimed as “Secure Boot”) |
| CPU clk | `480 MHz / CLKCTRL_CPU.DIV` (register-derived) |

No Sharp copyright string is drawn.

## Home menu

| item | action |
|---|---|
| Boot WinCE | eMMC NK `@ 0x120000` |
| Boot Linux | SD `zimage` + `dtb` from `BRAINBOO.CFG` |
| Diagnostics | DRAM test, CID/CSD, file list, key test |
| Boot Configuration | toggle stock flags, autoboot, default, save |
| Recovery Mode | DiagOS, `EDSH6EXE.BIN`, NK2 `@ 0x2120000`, file boot |
| Power Off | `HW_POWER_RESET.PWD` (`0x3e770001`) |

Serial: `w`/`s` or `k`/`j`, Enter, `1`–`6`, `q` cancels the timer.

## `BRAINBOO.CFG` (SD root, 8.3)

```
# brainboot cfg
autoboot=5
default=wince
cmdline=console=ttyAMA0,115200 console=tty1 root=/dev/mmcblk1p2 rw rootwait
zimage=ZIMAGE
dtb=IMX28-PWSH6.DTB
```

`default` is `wince` / `linux` / `diag` / `sdexe` / `none`. The Boot
Configuration screen can write this file and/or a stock `EDSH6CFG.BIN`
(ones-complement checksum). FAT16 write is implemented (create + overwrite).

## BootConfig / DevInfo

512-byte records. Checksum is `sum16(bytes[0..0x2B])` and the ones-complement
`nsum16 = 0xFFFF - sum16` at `+0x2E`.

BootConfig magic: `SHARP E-DICTIONARY BOOT CONFIG`

| offset | field |
|---|---|
| `0x20` | flags (`0x01` Diag, `0x02` CardBoot, `0x10` Engineer, `0x20` DevMode) |
| `0x24` | devflags |
| `0x28` | model |
| `0x2C` | sum16 |
| `0x2E` | nsum16 |

DevInfo lives at eMMC LBA 4, magic `SHARP E-DICTIONARY DEV INFO`, model
`EDSH6`, version 4 on this unit.

## Memory map

| region | address |
|---|---|
| DRAM | `0x40000000` … `+128 MiB` |
| this image / WinCE entry | `0x40200000` (`0xA0200000` uncached) |
| Linux DTB | `0x41000000` |
| Linux zImage / NK staging | `0x42000000` |
| framebuffer | `0x46000000` (854×480 RGB565) |
| copy trampoline | `0x47F00000` |
| SoC registers | `0x80000000` |
| UARTDBG | `0x80074000` |
| SSP0 eMMC (8-bit) | `0x80010000` |
| SSP1 SD (4-bit) | `0x80012000` |
| LCDIF | `0x80030000` |

WinCE NK is staged at `0x42000000` then a high-RAM trampoline copies it over
`0x40200000` and branches, because brainboot itself occupies that address
when started as a B000FF payload.

## Hardware sources

Board identity, OEM address table (128 MiB at `0x40000000`), and the CE
load address are taken from the device's own second-stage image and from
the project's existing eMMC/EBOOT notes.

Pinmux, LCD size (854×480 ILI9805, system-8080), SSP wiring, and the
key matrix follow the public `imx28-pwsh6` device tree / u-boot-brain
`pwsh6` port (same gen3_6 platform as ED-AA2 / ED-AJ2).

## License

GPL-2.0-or-later, same as barebox.
