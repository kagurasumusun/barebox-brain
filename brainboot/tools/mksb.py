#!/usr/bin/env python3
"""Build a cleartext i.MX28 SB v1.1 bootstream (OTP key = 0).

Header and instruction layout match qemu-brain hw/arm/mxs.c:
  SBHeader.signature \"STMP\" at +20
  instruction: opcode at +1, addr@+4, count@+8, arg@+12
One LOAD of the payload at 0x40200000 followed by JUMP.
"""
from __future__ import annotations

import argparse
import pathlib
import struct

SB_BLOCK = 16
SB_BOOTABLE = 1
SB_CLEARTEXT = 2
SB_INST_LOAD, SB_INST_JUMP = 2, 4
ENTRY = 0x40200000
HDR_BLOCKS = 6  # sizeof(SBHeader) == 96


def pad16(b: bytes) -> bytes:
    if len(b) % SB_BLOCK:
        b += b"\x00" * (SB_BLOCK - (len(b) % SB_BLOCK))
    return b


def inst(opcode: int, addr: int = 0, count: int = 0, arg: int = 0) -> bytes:
    raw = bytearray(16)
    raw[1] = opcode & 0xFF
    struct.pack_into("<I", raw, 4, addr)
    struct.pack_into("<I", raw, 8, count)
    struct.pack_into("<I", raw, 12, arg)
    return bytes(raw)


def build_sb(payload: bytes, dest: int = ENTRY) -> bytes:
    body = inst(SB_INST_LOAD, dest, len(payload)) + pad16(payload)
    body += inst(SB_INST_JUMP, dest, 0, 0)
    body = pad16(body)

    sh_blocks = 1
    data_off = HDR_BLOCKS + sh_blocks
    data_blocks = len(body) // SB_BLOCK
    image_blocks = data_off + data_blocks

    hdr = bytearray(HDR_BLOCKS * SB_BLOCK)
    hdr[20:24] = b"STMP"
    hdr[24] = 1  # major
    hdr[25] = 1  # minor
    struct.pack_into("<I", hdr, 28, image_blocks)
    struct.pack_into("<I", hdr, 32, 0)  # first_boot_tag_off
    struct.pack_into("<I", hdr, 36, 0)  # first_boot_sec_id
    struct.pack_into("<H", hdr, 40, 0)  # nr_keys (OTP-zero cleartext)
    struct.pack_into("<H", hdr, 42, 0)  # key_dict_off
    struct.pack_into("<H", hdr, 44, HDR_BLOCKS)
    struct.pack_into("<H", hdr, 46, 1)  # nr_sections
    struct.pack_into("<H", hdr, 48, 1)  # sec_hdr_size in blocks

    sh = struct.pack(
        "<IIII", 0, data_off, data_blocks, SB_BOOTABLE | SB_CLEARTEXT
    )
    sh = pad16(sh)
    return bytes(hdr) + sh + body


def parse_sb(image: bytes) -> dict:
    """Walk the stream the same way qemu-brain mxs_rom_load_sb does."""
    if len(image) < 96 or image[20:24] != b"STMP":
        raise ValueError("no STMP signature")
    image_blocks = struct.unpack_from("<I", image, 28)[0]
    hdr_blocks = struct.unpack_from("<H", image, 44)[0]
    nr_sections = struct.unpack_from("<H", image, 46)[0]
    nr_keys = struct.unpack_from("<H", image, 40)[0]
    if image_blocks * SB_BLOCK > len(image):
        raise ValueError("truncated SB image")
    ops = []
    for i in range(nr_sections):
        shoff = hdr_blocks * SB_BLOCK + i * 16
        ident, off, size, flags = struct.unpack_from("<IIII", image, shoff)
        if not (flags & SB_BOOTABLE):
            continue
        if not (flags & SB_CLEARTEXT):
            raise ValueError("encrypted section (need OTP key)")
        data = image[off * SB_BLOCK : (off + size) * SB_BLOCK]
        pos = 0
        while pos + 16 <= len(data):
            opcode = data[pos + 1]
            addr = struct.unpack_from("<I", data, pos + 4)[0]
            count = struct.unpack_from("<I", data, pos + 8)[0]
            arg = struct.unpack_from("<I", data, pos + 12)[0]
            if opcode == SB_INST_LOAD:
                payload = (count + 15) & ~15
                ops.append(("LOAD", addr, count))
                pos += 16 + payload
            elif opcode == SB_INST_JUMP:
                ops.append(("JUMP", addr, arg))
                pos += 16
            else:
                ops.append((f"OP{opcode}", addr, count))
                pos += 16
    return {
        "image_blocks": image_blocks,
        "nr_sections": nr_sections,
        "nr_keys": nr_keys,
        "ops": ops,
    }


def build_bcb(sb_sector: int, sb_blocks_512: int, tag: int = 1) -> bytes:
    """BCB 0x00112233, one drive copy at sb_sector (must be non-zero)."""
    if sb_sector == 0:
        raise ValueError("sb_sector must be non-zero")
    sec = bytearray(512)
    struct.pack_into("<I", sec, 0, 0x00112233)
    struct.pack_into("<I", sec, 4, tag)   # primary_tag
    struct.pack_into("<I", sec, 12, 1)    # copies
    struct.pack_into("<I", sec, 16 + 8, tag)
    struct.pack_into("<I", sec, 16 + 12, sb_sector)
    struct.pack_into("<I", sec, 16 + 16, sb_blocks_512)
    return bytes(sec)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True, type=pathlib.Path)
    ap.add_argument("--out", required=True, type=pathlib.Path)
    args = ap.parse_args()
    payload = args.bin.read_bytes()
    if not payload:
        print("empty payload")
        return 1
    sb = build_sb(payload)
    info = parse_sb(sb)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(sb)
    print(
        f"wrote {args.out}  {len(sb)} bytes  "
        f"LOAD+JUMP 0x{ENTRY:08x}  ops={info['ops']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
