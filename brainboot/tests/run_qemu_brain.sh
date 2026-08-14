#!/bin/sh
# Run brainboot on qemu-brain with the synthetic images from mk_qemu_images.py.
# Usage: tests/run_qemu_brain.sh [silent|diag|menu] [qemu-system-arm]
set -e
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
MODE=${1:-silent}
QEMU=${2:-qemu-system-arm}
IMG=${TMPDIR:-/tmp}/bb-qemu
BIN=$ROOT/build/brainboot.bin

if [ ! -x "$QEMU" ]; then
    echo "qemu-system-arm not found: $QEMU" >&2
    exit 2
fi
if [ ! -f "$BIN" ]; then
    echo "build brainboot first (make -C brainboot)" >&2
    exit 2
fi
python3 "$ROOT/tests/mk_qemu_images.py" "$IMG"

case "$MODE" in
    silent) EMMC=$IMG/emmc.img; SD=$IMG/sd_silent.img ;;
    diag)   EMMC=$IMG/emmc_diag.img; SD=$IMG/sd_silent.img ;;
    menu)   EMMC=$IMG/emmc.img; SD=$IMG/sd_menu.img ;;
    *) echo "mode: silent|diag|menu" >&2; exit 2 ;;
esac

# -kernel uses the Linux loader (lands at 0x20000). Deposit at the link address.
exec "$QEMU" -M brain \
    -device loader,file="$BIN",addr=0x40200000,force-raw=on,cpu-num=0 \
    -drive if=sd,file="$EMMC",format=raw \
    -drive if=sd,file="$SD",format=raw,index=1 \
    -display none -serial stdio
