# PW-AJ2 bootloader

The device-specific bootloader lives in [`brainboot/`](brainboot/README.md).

It is a small freestanding ARM926 image that can start Windows CE 6 and
Linux on the SHARP Brain PW-AJ2 (i.MX28, 128 MiB, eMMC). Build it with
`make -C brainboot`; do not confuse it with a full barebox `make` of this
tree.

The stock first-stage / SB stream is left untouched. First install path is
an SD-card `EDSH6EXE.BIN`.
