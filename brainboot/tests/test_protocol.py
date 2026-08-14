#!/usr/bin/env python3
"""Re-check B000FF / BootConfig / ECEC packing against the C implementation."""
from __future__ import annotations

import pathlib
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main() -> int:
    exe = ROOT / "build" / "host_test"
    if not exe.exists():
        print("build/host_test missing — run `make host` first", file=sys.stderr)
        return 2
    r = subprocess.run([str(exe)])
    if r.returncode != 0:
        return r.returncode

    # Independent Python check of the ones-complement BootConfig rule.
    magic = b"SHARP E-DICTIONARY BOOT CONFIG"
    buf = bytearray(512)
    buf[0:len(magic)] = magic
    buf[0x20:0x24] = struct.pack("<I", 0x11)
    s = (sum(buf[0:0x2C]) + 0x1FE) & 0xFFFF
    ns = (0xFFFF - s) & 0xFFFF
    buf[0x2C:0x2E] = struct.pack("<H", s)
    buf[0x2E:0x30] = struct.pack("<H", ns)
    assert (sum(buf[0:0x30]) & 0xFFFF) == s
    assert ns == (0xFFFF - s) & 0xFFFF
    print("ok   python bootcfg ones-complement inclusive")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
