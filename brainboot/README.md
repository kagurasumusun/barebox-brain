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
* protocol-tested on the host (BootConfig, DevInfo, B000FF, ECEC, MBR/FAT16, CRC32)

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

## Boot menu

Serial (UARTDBG, 115200 8N1) and on-screen:

| key | action |
|---|---|
| 1 / Enter on item | WinCE NK from eMMC `0x120000` |
| 2 | DiagOS from eMMC `0x4120000` (size `0xB9437C`) |
| 3 | `EDSH6EXE.BIN` from SD |
| 4 | Linux `zImage` + DTB from SD |
| 5 | print DevInfo / BootConfig / CID |
| 6 | walking-bit DRAM test on `0x43000000–0x44FFFFFF` |
| 7 | watchdog reboot |

5 second autoboot. `EDSH6CFG.BIN` with `flags & 0x01` selects DiagOS as the
default.

Serial keys: `w`/`s` or `k`/`j` move, Enter selects, `1`–`7` jump, `q` cancels
the timer.

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
