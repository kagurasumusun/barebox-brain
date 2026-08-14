# PW-AJ2 bootloader

The device-specific bootloader lives in [`brainboot/`](brainboot/README.md).

It is a small freestanding ARM926 image that can start Windows CE 6 and
Linux on the SHARP Brain i.MX28 family. The retail name is read from
DevInfo at runtime (not compiled in as PW-AJ2).

Build it with `make -C brainboot`. That is independent of a full barebox
`make` of this tree. The command line inside brainboot uses the same
verbs as barebox (`md`, `mw`, `detect`, `boot`, `reset`, `poweroff`);
SoC bases match `include/mach/mxs/imx28-regs.h`.

The graphical UI is off unless `BOOTMENU.BIN` (flag + ones-complement
checksum), a held key, or `menu=on` says otherwise. Engineer is DiagApp,
not this UI. Silent path prints the stock EBOOT strings
(`DevInfo OK!!`, `Fast Diag Boot!!!`).

Two install images: SD `EDSH6EXE.BIN` (Card BOOT, stock first-stage
stays) and `make emmc` (type-`0x53` SB replace).

Executed on qemu-brain (`-M brain`) with synthetic eMMC/SD images; not
yet run on the physical unit.
