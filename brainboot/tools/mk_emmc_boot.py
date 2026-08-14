#!/usr/bin/env python3
"""Build a power-of-two eMMC image that the i.MX28 ROM / qemu-brain boots.

MBR type 0x53 at LBA 256 (stock P1) → BCB 0x00112233 → SB stream.
LBA 2 / LBA 4 keep Diag-compatible BootConfig / DevInfo so the loaded
image can identify the device the same way stock EBOOT does.
"""
from __future__ import annotations

import argparse
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
import mksb  # noqa: E402

sys.path.insert(0, str(ROOT.parent / "tests"))
import mk_qemu_images as mq  # noqa: E402

PART_LBA = 256  # stock type-0x53 start
SB_LBA = 257    # first sector after BCB


def put32(b: bytearray, off: int, v: int) -> None:
    b[off : off + 4] = struct.pack("<I", v)


def main() -> int:
    ap = argparse.ArgumentParser()
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--bin", type=pathlib.Path, help="raw ARM payload")
    src.add_argument("--sb", type=pathlib.Path, help="already-built .sb")
    ap.add_argument("--out", required=True, type=pathlib.Path)
    ap.add_argument("--size", type=int, default=8 * 1024 * 1024,
                    help="image size, must be a power of two (qemu eMMC)")
    ap.add_argument("--flags", type=lambda s: int(s, 0), default=0)
    args = ap.parse_args()

    if args.size < 2 * 1024 * 1024 or (args.size & (args.size - 1)):
        print("size must be a power of two and >= 2 MiB", file=sys.stderr)
        return 2

    if args.sb:
        sb = args.sb.read_bytes()
    else:
        payload = args.bin.read_bytes()
        if not payload:
            print("empty payload", file=sys.stderr)
            return 1
        sb = mksb.build_sb(payload)
    info = mksb.parse_sb(sb)
    if not any(op[0] == "JUMP" for op in info["ops"]):
        print("SB has no JUMP", file=sys.stderr)
        return 1

    img = bytearray(args.size)
    img[510] = 0x55
    img[511] = 0xAA
    # P1 type 0x53 @ LBA 256, CHS sector != 0 (EBOOT FATInitDisk rule)
    img[446] = 0x80
    img[446 + 2] = 1
    img[446 + 4] = 0x53
    img[446 + 6] = 0x3F
    put32(img, 446 + 8, PART_LBA)
    put32(img, 446 + 12, (args.size // 512) - PART_LBA)

    sb_secs = (len(sb) + 511) // 512
    img[512 * PART_LBA : 512 * PART_LBA + 512] = mksb.build_bcb(SB_LBA, sb_secs)
    img[512 * SB_LBA : 512 * SB_LBA + len(sb)] = sb

    img[512 * 2 : 512 * 3] = mq.make_bootcfg(args.flags)
    img[512 * 4 : 512 * 5] = mq.make_devinfo("EDSH6", 4)
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

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(img)
    print(
        f"wrote {args.out}  {len(img)} bytes  "
        f"0x53@{PART_LBA}  SB@{SB_LBA} ({len(sb)} B)  flags=0x{args.flags:x}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
