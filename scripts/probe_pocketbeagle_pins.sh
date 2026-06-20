#!/bin/sh
set -eu

GPIO_ONLY=0
OUT=""

usage() {
    echo "Usage: $0 [--gpio-only] [--out FILE]" >&2
}

while [ $# -gt 0 ]; do
    case "$1" in
        --gpio-only)
            GPIO_ONLY=1
            shift
            ;;
        --out)
            if [ $# -lt 2 ]; then
                usage
                exit 2
            fi
            OUT="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage
            exit 2
            ;;
    esac
done

if ! command -v config-pin >/dev/null 2>&1; then
    echo "config-pin not found" >&2
    exit 1
fi

emit_pin() {
    pin="$1"
    info="$(config-pin -i "$pin" 2>&1 || true)"

    if [ "$GPIO_ONLY" -eq 1 ]; then
        echo "$info" | grep -q 'Function if cape loaded:.*gpio' || return 0
    fi

    {
        echo "===== $pin ====="
        echo "$info"
        echo
    }
}

run_probe() {
    header=1
    while [ "$header" -le 2 ]; do
        idx=1
        while [ "$idx" -le 36 ]; do
            pin="$(printf 'P%d.%02d' "$header" "$idx")"
            emit_pin "$pin"
            idx=$((idx + 1))
        done
        header=$((header + 1))
    done
}

if [ -n "$OUT" ]; then
    run_probe > "$OUT"
    echo "Wrote $OUT"
else
    run_probe
fi
