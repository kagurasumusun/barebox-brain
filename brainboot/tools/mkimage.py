#!/usr/bin/env python3
"""Pack brainboot.bin as B000FF (SD EXE) and as an ECEC-style raw image.

Does not copy or embed any third-party bootloader binary.
"""
from __future__ import annotations

import argparse
import pathlib
import struct
import sys

B000FF = b"B000FF\n"
ECEC = 0x43454345
NK_LOAD = 0xA0200000
EBL_LOAD_VA = 0x81E80000


def sum32(data: bytes) -> int:
    return sum(data) & 0xFFFFFFFF


def write_b000ff(payload: bytes, path: pathlib.Path, load: int = NK_LOAD) -> None:
    hdr = B000FF
    hdr += struct.pack("<I", load)
    hdr += struct.pack("<I", len(payload))
    hdr += struct.pack("<I", load)
    hdr += struct.pack("<I", len(payload))
    hdr += struct.pack("<I", sum32(payload))
    path.write_bytes(hdr + payload)
    print(f"wrote {path}  {path.stat().st_size} bytes  load=0x{load:08x}  sum=0x{sum32(payload):08x}")


def write_ecec(payload: bytes, path: pathlib.Path, load_va: int = NK_LOAD) -> None:
    """Stamp ECEC at +0x40 if the first word is an ARM B and the slot is free."""
    img = bytearray(payload)
    if len(img) < 0x50:
        img.extend(b"\x00" * (0x50 - len(img)))
    # Only write ECEC if the reserved header is still zero (our start.S placeholder).
    existing = struct.unpack_from("<I", img, 0x40)[0]
    if existing == 0:
        struct.pack_into("<I", img, 0x40, ECEC)
        struct.pack_into("<I", img, 0x44, load_va + 0x4524)
        struct.pack_into("<I", img, 0x48, 0x4524)
    path.write_bytes(bytes(img))
    print(f"wrote {path}  {path.stat().st_size} bytes  ECEC load_va=0x{load_va:08x}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True, type=pathlib.Path)
    ap.add_argument("--outdir", required=True, type=pathlib.Path)
    args = ap.parse_args()
    payload = args.bin.read_bytes()
    if not payload:
        print("empty payload", file=sys.stderr)
        return 1
    args.outdir.mkdir(parents=True, exist_ok=True)
    write_b000ff(payload, args.outdir / "EDSH6EXE.BIN", NK_LOAD)
    write_ecec(payload, args.outdir / "brainboot.ecec.bin", NK_LOAD)
    (args.outdir / "brainboot.bin").write_bytes(payload)

    # Same 8.3 trio stock EBOOT looks for on SD.
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "mk_qemu_images", pathlib.Path(__file__).resolve().parents[1] / "tests" / "mk_qemu_images.py"
    )
    mq = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mq)
    (args.outdir / "EDSH6CFG.BIN").write_bytes(mq.make_bootcfg(0))
    (args.outdir / "EDSH6DEV.BIN").write_bytes(mq.make_devinfo("EDSH6", 4))
    print(f"wrote {args.outdir / 'EDSH6CFG.BIN'}  flags=0")
    print(f"wrote {args.outdir / 'EDSH6DEV.BIN'}  model=EDSH6")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
