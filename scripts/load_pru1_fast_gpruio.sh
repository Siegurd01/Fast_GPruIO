#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
FW_PATH="${1:-$ROOT/build/fast_gpruio_pru1_fw.out}"
FW_NAME="${FW_NAME:-fast_gpruio_pru1_fw.out}"

find_pru1_remoteproc()
{
    for rp in /sys/class/remoteproc/remoteproc*; do
        [ -d "$rp" ] || continue
        name="$(cat "$rp/name" 2>/dev/null || true)"
        case "$name" in
            *pru1*|*4a338000*)
                printf '%s\n' "$rp"
                return 0
                ;;
        esac
    done

    [ -d /sys/class/remoteproc/remoteproc2 ] && {
        printf '%s\n' /sys/class/remoteproc/remoteproc2
        return 0
    }
    return 1
}

if [ "$(id -u)" -ne 0 ]; then
    echo "Run as root." >&2
    exit 1
fi

if [ -z "${PRU1_REMOTEPROC:-}" ]; then
    PRU1_REMOTEPROC="$(find_pru1_remoteproc)" || {
        echo "PRU1 remoteproc not found under /sys/class/remoteproc" >&2
        exit 1
    }
fi
[ -d "$PRU1_REMOTEPROC" ] || {
    echo "PRU1 remoteproc not found: $PRU1_REMOTEPROC" >&2
    exit 1
}
[ -f "$FW_PATH" ] || {
    echo "Firmware not found: $FW_PATH" >&2
    exit 1
}

echo "Using PRU1 remoteproc: $PRU1_REMOTEPROC"
echo "Copying firmware: $FW_PATH -> /lib/firmware/$FW_NAME"
cp "$FW_PATH" "/lib/firmware/$FW_NAME"
chmod 644 "/lib/firmware/$FW_NAME"

STATE="$(cat "$PRU1_REMOTEPROC/state" 2>/dev/null || true)"
if [ "$STATE" = "running" ]; then
    echo "Stopping PRU1"
    echo stop > "$PRU1_REMOTEPROC/state"
    sleep 1
fi

echo "$FW_NAME" > "$PRU1_REMOTEPROC/firmware"
echo start > "$PRU1_REMOTEPROC/state"
sleep 0.2

echo "PRU1 state: $(cat "$PRU1_REMOTEPROC/state")"
echo "PRU1 firmware: $(cat "$PRU1_REMOTEPROC/firmware")"
