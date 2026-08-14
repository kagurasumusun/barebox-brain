#!/usr/bin/env python3
"""Build tiny eMMC + FAT16 SD images for qemu-brain (no stock firmware)."""
from __future__ import annotations

import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import mkbootmenu  # noqa: E402


def put16(b: bytearray, off: int, v: int) -> None:
    b[off : off + 2] = struct.pack("<H", v)


def put32(b: bytearray, off: int, v: int) -> None:
    b[off : off + 4] = struct.pack("<I", v)


def sum16(data: bytes) -> int:
    return sum(data) & 0xFFFF


def ones_record(magic: bytes, payload20: bytes) -> bytes:
    """512-byte record: magic at 0, payload at 0x20 (12 bytes used), sum at 0x2C."""
    sec = bytearray(512)
    sec[0 : len(magic)] = magic
    sec[0x20 : 0x20 + len(payload20)] = payload20
    s = (sum16(sec[0:0x2C]) + 0x1FE) & 0xFFFF
    ns = (0xFFFF - s) & 0xFFFF
    put16(sec, 0x2C, s)
    put16(sec, 0x2E, ns)
    return bytes(sec)


def make_devinfo(model: str = "EDSH6", version: int = 4) -> bytes:
    extra = bytearray(12)
    extra[0:8] = model.encode("ascii")[:8]
    extra[8:12] = struct.pack("<I", version)
    return ones_record(b"SHARP E-DICTIONARY DEV INFO", extra)


def make_bootcfg(flags: int = 0) -> bytes:
    extra = bytearray(12)
    extra[0:4] = struct.pack("<I", flags)
    return ones_record(b"SHARP E-DICTIONARY BOOT CONFIG", extra)


def make_emmc(path: pathlib.Path, flags: int = 0) -> None:
    size = 8 * 1024 * 1024
    img = bytearray(size)
    # MBR
    img[510] = 0x55
    img[511] = 0xAA
    img[446] = 0x80
    img[446 + 2] = 1
    img[446 + 4] = 0x53
    img[446 + 6] = 0x3F
    put32(img, 446 + 8, 1)
    put32(img, 446 + 12, (size // 512) - 1)
    img[512 * 2 : 512 * 3] = make_bootcfg(flags)
    img[512 * 4 : 512 * 5] = make_devinfo()
    img[512 * 16] = 0x80
    img[512 * 16 + 4] = ord("7")
    img[512 * 16 + 5] = ord("0")
    img[512 * 17 + 0] = ord("7")
    img[512 * 17 + 1] = ord("0")
    img[512 * 18 + 0] = ord("7")
    img[512 * 18 + 1] = ord("0")
    img[512 * 64 + 0] = ord("7")
    img[512 * 64 + 1] = ord("0")
    img[512 * 71 : 512 * 71 + 30] = b"SHARP E-DICTIONARY CHARGE INFO"
    # Fake NK at 0x120000: ARM `b .`
    nk = 0x120000
    img[nk : nk + 4] = struct.pack("<I", 0xEAFFFFFE)
    path.write_bytes(img)
    print(f"wrote {path}  {path.stat().st_size} bytes  flags=0x{flags:x}")


def make_fat16_sd(path: pathlib.Path, files: dict[str, bytes]) -> None:
    """64 MiB raw image, MBR + FAT16 type 0x0E starting LBA 2048, CHS sector != 0."""
    sec = 512
    part_lba = 2048
    total = 64 * 1024 * 1024
    img = bytearray(total)
    nsec = total // sec
    part_secs = nsec - part_lba

    img[510] = 0x55
    img[511] = 0xAA
    img[446] = 0x80
    img[446 + 2] = 1  # start CHS sector
    img[446 + 4] = 0x0E
    img[446 + 6] = 0x3F
    put32(img, 446 + 8, part_lba)
    put32(img, 446 + 12, part_secs)

    vbr_off = part_lba * sec
    vbr = memoryview(img)[vbr_off : vbr_off + 512]
    vbr[0:3] = b"\xeb\x3c\x90"
    vbr[3:11] = b"MSWIN4.1"
    put16(img, vbr_off + 11, 512)
    img[vbr_off + 13] = 4  # 4 sectors/cluster = 2 KiB
    put16(img, vbr_off + 14, 1)  # reserved
    img[vbr_off + 16] = 2  # FATs
    put16(img, vbr_off + 17, 512)  # root entries = 16 KiB = 32 sectors
    img[vbr_off + 21] = 0xF8
    fat_secs = 128
    put16(img, vbr_off + 22, fat_secs)
    put16(img, vbr_off + 24, 32)
    put16(img, vbr_off + 26, 2)
    put32(img, vbr_off + 32, part_secs)
    img[vbr_off + 38] = 0x29
    img[vbr_off + 43:vbr_off + 54] = b"BRAINBOOT  "
    img[vbr_off + 54:vbr_off + 62] = b"FAT16   "
    img[vbr_off + 510] = 0x55
    img[vbr_off + 511] = 0xAA

    fat0 = vbr_off + 512
    img[fat0] = 0xF8
    img[fat0 + 1] = 0xFF
    img[fat0 + 2] = 0xFF
    img[fat0 + 3] = 0xFF
    fat1 = fat0 + fat_secs * sec
    img[fat1 : fat1 + 4] = img[fat0 : fat0 + 4]

    root = fat1 + fat_secs * sec
    data = root + 32 * sec  # 512 entries * 32 = 16384 = 32 sectors
    next_cl = 2

    def fat16_set(cl: int, val: int) -> None:
        for base in (fat0, fat1):
            put16(img, base + cl * 2, val)

    def to_83(name: str) -> bytes:
        stem, _, ext = name.upper().partition(".")
        return (stem[:8].ljust(8) + ext[:3].ljust(3)).encode("ascii")

    for i, (name, payload) in enumerate(files.items()):
        ncl = max(1, (len(payload) + 2047) // 2048)
        first = next_cl
        for k in range(ncl):
            cl = first + k
            nxt = 0xFFFF if k == ncl - 1 else cl + 1
            fat16_set(cl, nxt)
            src = payload[k * 2048 : (k + 1) * 2048]
            off = data + (cl - 2) * 2048
            img[off : off + len(src)] = src
        next_cl = first + ncl
        ent = root + i * 32
        img[ent : ent + 11] = to_83(name)
        img[ent + 11] = 0x20
        put16(img, ent + 26, first)
        put32(img, ent + 28, len(payload))

    path.write_bytes(img)
    print(f"wrote {path}  {path.stat().st_size} bytes  files={list(files)}")


def main() -> int:
    out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "/tmp/bb-qemu")
    out.mkdir(parents=True, exist_ok=True)
    make_emmc(out / "emmc.img", flags=0)
    make_emmc(out / "emmc_diag.img", flags=0x01)
    make_fat16_sd(out / "sd_silent.img", {})
    make_fat16_sd(out / "sd_menu.img", {"BOOTMENU.BIN": mkbootmenu.build(mkbootmenu.FLAG_SHOW)})
    make_fat16_sd(out / "sd_shell.img", {"BOOTMENU.BIN": mkbootmenu.build(mkbootmenu.FLAG_SHELL)})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
