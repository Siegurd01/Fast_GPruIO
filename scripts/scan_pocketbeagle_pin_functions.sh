#!/bin/sh
set -eu

OUT=""
RAW_OUT=""

usage() {
    echo "Usage: $0 [--out FILE] [--raw-out FILE]" >&2
}

while [ $# -gt 0 ]; do
    case "$1" in
        --out)
            if [ $# -lt 2 ]; then
                usage
                exit 2
            fi
            OUT="$2"
            shift 2
            ;;
        --raw-out)
            if [ $# -lt 2 ]; then
                usage
                exit 2
            fi
            RAW_OUT="$2"
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

line_value() {
    label="$1"
    sed -n "s/^${label}:[[:space:]]*//p" | head -n 1
}

tsv_escape() {
    tr '\t\r\n' '   '
}

scan_pin() {
    pin="$1"
    info="$(config-pin -i "$pin" 2>&1 || true)"
    query="$(config-pin -q "$pin" 2>&1 || true)"

    if [ -n "$RAW_OUT" ]; then
        {
            echo "===== $pin ====="
            echo "$query"
            echo "$info"
            echo
        } >> "$RAW_OUT"
    fi

    pin_name="$(printf '%s\n' "$info" | line_value "Pin name" | tsv_escape)"
    default_fn="$(printf '%s\n' "$info" | line_value "Function if no cape loaded" | tsv_escape)"
    cape_fns="$(printf '%s\n' "$info" | line_value "Function if cape loaded" | tsv_escape)"
    fn_info="$(printf '%s\n' "$info" | line_value "Function information" | tsv_escape)"
    kernel_gpio="$(printf '%s\n' "$info" | line_value "Kernel GPIO id" | tsv_escape)"
    pru_gpio="$(printf '%s\n' "$info" | line_value "PRU GPIO id" | tsv_escape)"

    current_mode="$(printf '%s\n' "$query" |
        sed -n 's/^.* Mode:[[:space:]]*\([^[:space:]]*\).*$/\1/p' |
        head -n 1 | tsv_escape)"

    gpio_capable=0
    gpio_bank_bit=""
    if printf '%s\n' "$cape_fns" | grep -q 'gpio'; then
        gpio_capable=1
        gpio_bank_bit="$(printf '%s\n' "$fn_info" |
            sed -n 's/^.*\(gpio[0-3]_[0-9][0-9]*\).*$/\1/p' |
            head -n 1 | tsv_escape)"
    fi

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$pin" "$current_mode" "$gpio_capable" "$gpio_bank_bit" \
        "$kernel_gpio" "$pru_gpio" "$default_fn" "$cape_fns" "$fn_info"
}

run_scan() {
    printf 'pin\tcurrent_mode\tgpio_capable\tgpio_bank_bit\tkernel_gpio_id\tpru_gpio_id\tdefault_function\tcape_functions\tfunction_information\n'

    header=1
    while [ "$header" -le 2 ]; do
        idx=1
        while [ "$idx" -le 36 ]; do
            pin="$(printf 'P%d.%02d' "$header" "$idx")"
            scan_pin "$pin"
            idx=$((idx + 1))
        done
        header=$((header + 1))
    done
}

if [ -n "$RAW_OUT" ]; then
    : > "$RAW_OUT"
fi

if [ -n "$OUT" ]; then
    run_scan > "$OUT"
    echo "Wrote $OUT"
    if [ -n "$RAW_OUT" ]; then
        echo "Wrote $RAW_OUT"
    fi
else
    run_scan
fi
