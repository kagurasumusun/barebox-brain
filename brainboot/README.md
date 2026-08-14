# brainboot

Lightweight dual-OS second-stage for the SHARP Brain i.MX28 family
(PW-AJ2 / PW-SH6 / PW-AA2, gen3_6). It does **not** hard-code the
retail name: DevInfo.model is looked up at runtime.

It boots:

* Windows CE 6 NK from eMMC (`0x120000`) or DiagOS (`0x4120000`)
* A B000FF `EDSH6EXE.BIN` (or the model-derived `EDxxxxEXE.BIN`) from FAT16/32
* Linux `zImage` + DTB from microSD

Default behaviour matches stock EBOOT: **no graphical UI**. The mockup
screen appears only when a flag + checksum says so (see BOOTMENU.BIN).

## What this is / is not

This directory is a self-contained ARM926EJ-S freestanding image living
in the `barebox-brain` tree. It does **not** replace the i.MX28 Boot ROM
and it does **not** re-initialise DDR.

`EBL.bin` on this device is the WinCE OAL (`OEMInit`, RTC, I2C, EDNA2,
codec, clock) at VA `0x81E80000`. It is **not** the NK-loading EBOOT
described in `BOOT.md`. brainboot covers the EBOOT decision path and
prints the same serial strings. It does **not** reimplement the CE OAL,
and it is not honest to call that “100% EBL coverage”.

It has been:

* compiled with `arm-none-eabi-gcc` for ARM926EJ-S
* protocol-tested on the host (BootConfig, DevInfo, BOOTMENU.BIN, B000FF,
  ECEC, MBR/FAT16 read+write, identity lookup, silent/menu policy, CRC32)
* executed on **qemu-brain** (`-M brain`, QEMU 11.0.3, release
  `qemu-brain-build-2026-08-14`): silent NK path, `Fast Diag Boot!!!`,
  and BOOTMENU.BIN SHOW all produced the documented serial lines

It has **not** been executed on a physical PW-AJ2 in this workspace.

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

`python3 tools/mkbootmenu.py --show -o BOOTMENU.BIN` builds the UI gate file.

## Silent path (stock EBOOT)

On every boot, serial prints the EBOOT lines from `BOOT.md`:

1. `DevInfo OK!!` (eMMC LBA 4) or a checksum failure
2. `Find Boot Config File!!` / `Boot Config OK!!` from SD `EDxxxxCFG.BIN`
   or eMMC LBA 2; otherwise `Check Card BOOT`
3. No UI unless one of:
   * `BOOTMENU.BIN` present, checksums, and `FLAG_SHOW`
   * BootConfig Engineer (`0x10`)
   * Enter / Esc held, or a UART character during probe
   * `BRAINBOO.CFG` has `menu=on`
4. Otherwise: DiagBoot (`0x01`) → `Fast Diag Boot!!!` → DiagOS;
   else NK at `0x120000`. Linux only if `default=linux`.

## `BOOTMENU.BIN`

No `bootmenu.bin` exists in the kagurasumusun trees or inside `EBL.bin`.
The record is defined here so the UI can be gated on a flag + the same
ones-complement checksum as BootConfig.

| offset | field |
|---|---|
| `0x00` | `"SHARP E-DICTIONARY BOOT MENU"` (28 bytes) |
| `0x20` | flags (`0x01` SHOW, `0x02` VERBOSE, `0x04` SHELL) |
| `0x28` | cookie (optional, `0` = unused) |
| `0x2C` | `sum16(bytes[0..0x2B])` |
| `0x2E` | `nsum16 = (0xFFFF - sum16) & 0xFFFF` |

`FLAG_SHELL` drops to the command line without drawing the LCD.

## Identity

`ident.c` maps DevInfo.model through the ResetKit / lilo table
(`EDSH6` → `PW-SH6` / `gen3_6`, `EDAJ2` → `PW-AJ2`, …). An unknown or
missing record stays `UNKNOWN`; it is not forced to PW-AJ2.

DRAM size is read with the same `DRAM_CTL29/31` formula as barebox
`imx28_get_memsize()`. EDNA2 doorbell (`0x400EA03C`) and I2C0 CTRL are
probed and printed; a zero is reported as a zero.

## On-screen UI

Only after the gate above. 854×480 dark console matching the dumped
mockup. Header retail name comes from identity. Status rows are probes.

| item | action |
|---|---|
| Boot WinCE | eMMC NK `@ 0x120000` |
| Boot Linux | SD `zimage` + `dtb` from `BRAINBOO.CFG` |
| Diagnostics | DRAM test, CID/CSD, file list, key test, command line |
| Boot Configuration | flags, autoboot, `BRAINBOO.CFG` / `EDSH6CFG.BIN` / `BOOTMENU.BIN` |
| Recovery Mode | DiagOS, SD EXE, NK2 `@ 0x2120000`, file boot |
| Power Off | `HW_POWER_RESET.PWD` (`0x3e770001`) |

Serial: `w`/`s` or `k`/`j`, Enter, `1`–`6`, `q` cancels the timer, `c` / `` ` `` opens the command line.

## barebox-style command line

Same verbs as barebox: `help`, `version`, `detect`, `ident`, `md`, `mw`,
`boot wince|linux|diag|sdexe|nk2`, `go`, `reset`, `poweroff`, `exit`.
Register bases match `include/mach/mxs/imx28-regs.h` in this tree.

## `BRAINBOO.CFG` (SD root, 8.3)

```
# brainboot cfg
autoboot=5
default=wince
menu=off
cmdline=console=ttyAMA0,115200 console=tty1 root=/dev/mmcblk1p2 rw rootwait
zimage=ZIMAGE
dtb=IMX28-PWSH6.DTB
```

`default` is `wince` / `linux` / `diag` / `sdexe` / `none`. `menu=on`
forces the UI even without `BOOTMENU.BIN`.

## qemu-brain

Prebuilt `qemu-system-arm` from wince release `qemu-brain-build-2026-08-14`.
`-kernel` lands at `0x20000` (Linux loader), so the image is deposited
with the generic loader instead:

```
python3 tests/mk_qemu_images.py /tmp/bb-qemu
qemu-system-arm -M brain \
  -device loader,file=build/brainboot.bin,addr=0x40200000,force-raw=on,cpu-num=0 \
  -drive if=sd,file=/tmp/bb-qemu/emmc.img,format=raw \
  -drive if=sd,file=/tmp/bb-qemu/sd_silent.img,format=raw,index=1 \
  -display none -serial stdio
```

`sd_menu.img` carries `BOOTMENU.BIN` SHOW. `emmc_diag.img` has DiagBoot.
These images are synthetic (no SB stream, no real NK/DiagOS). A full
stock `emmc.img` was not downloaded for this run.

## Install (non-destructive)

1. Format microSD as **MBR + FAT16**, type `0x0E`, boot flag `0x80`,
   CHS start **and** end sector fields non-zero.
2. Copy `build/EDSH6EXE.BIN` to the root.
3. Optional: `BOOTMENU.BIN --show` if you want the UI, plus
   `EDSH6CFG.BIN` / `zImage` / DTB.
4. Power on. Stock EBOOT Card BOOT loads the EXE to `0xA0200000`.

Do **not** overwrite the eMMC type-`0x53` SB partition until the SD
path has been confirmed on the unit.

## Memory map

| region | address |
|---|---|
| DRAM | `0x40000000` … `+128 MiB` |
| this image / WinCE entry | `0x40200000` (`0xA0200000` uncached) |
| Linux DTB | `0x41000000` |
| Linux zImage / NK staging | `0x42000000` |
| framebuffer | `0x46000000` (854×480 RGB565) |
| copy trampoline | `0x47F00000` |
| EDNA2 mailbox | `0x400EA000` |
| UARTDBG | `0x80074000` |
| SSP0 eMMC / SSP1 SD | `0x80010000` / `0x80012000` |

## License

GPL-2.0-or-later, same as barebox.
