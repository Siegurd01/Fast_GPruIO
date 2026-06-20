#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
FW="${FAST_GPRUIO_FW:-$ROOT/build/fast_gpruio_pru1_fw.out}"
CMD="$ROOT/userspace/build/fast_gpruio_cmd"
PIN="${1:-}"
PULSE_US="${2:-800}"

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: run this hardware test as root (sudo)." >&2
    exit 1
fi
if [ ! -e /dev/mem ]; then
    echo "ERROR: /dev/mem is unavailable; the userspace library cannot map the PRU1 mailbox." >&2
    exit 1
fi
if [ ! -f "$FW" ]; then
    echo "ERROR: firmware not found at $FW" >&2
    echo "Copy the release .out file there or set FAST_GPRUIO_FW." >&2
    exit 1
fi

make -C "$ROOT/userspace"
"$ROOT/scripts/load_pru1_fast_gpruio.sh" "$FW"
"$CMD" stat
"$CMD" calc 8 115200 25 10

if [ -n "$PIN" ]; then
    echo "Starting a $PULSE_US us pulse on $PIN"
    "$CMD" pulse "$PIN" "$PULSE_US"
    sleep 1
    "$CMD" stat
else
    echo "No GPIO pulse was generated. To test one:"
    echo "  sudo ./scripts/verify_on_target.sh P2.17 800"
fi
