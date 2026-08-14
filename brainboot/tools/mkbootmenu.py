#!/usr/bin/env python3
"""Build a 512-byte BOOTMENU.BIN (ones-complement checksum)."""
from __future__ import annotations

import argparse
import pathlib
import struct
import sys

MAGIC = b"SHARP E-DICTIONARY BOOT MENU"  # 28 bytes
FLAG_SHOW = 0x01
FLAG_VERBOSE = 0x02
FLAG_SHELL = 0x04


def build(flags: int, cookie: int = 0) -> bytes:
    buf = bytearray(512)
    buf[0 : len(MAGIC)] = MAGIC
    buf[0x20:0x24] = struct.pack("<I", flags)
    buf[0x28:0x2C] = struct.pack("<I", cookie)
    s = sum(buf[0:0x2C]) & 0xFFFF
    ns = (0xFFFF - s) & 0xFFFF
    buf[0x2C:0x2E] = struct.pack("<H", s)
    buf[0x2E:0x30] = struct.pack("<H", ns)
    return bytes(buf)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--output", type=pathlib.Path, default=pathlib.Path("BOOTMENU.BIN"))
    ap.add_argument("--show", action="store_true", help="set FLAG_SHOW (draw UI)")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--shell", action="store_true", help="set FLAG_SHELL (command line)")
    ap.add_argument("--cookie", type=lambda s: int(s, 0), default=0)
    args = ap.parse_args()
    flags = 0
    if args.show:
        flags |= FLAG_SHOW
    if args.verbose:
        flags |= FLAG_VERBOSE
    if args.shell:
        flags |= FLAG_SHELL
    data = build(flags, args.cookie)
    args.output.write_bytes(data)
    s = int.from_bytes(data[0x2C:0x2E], "little")
    print(f"wrote {args.output}  flags=0x{flags:x}  sum16=0x{s:04x}  cookie=0x{args.cookie:x}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
