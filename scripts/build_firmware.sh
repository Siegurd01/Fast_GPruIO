#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRU_CGT="${PRU_CGT:-$ROOT/.tools/ti-cgt-pru_2.3.3}"
CLPRU="${CLPRU:-$PRU_CGT/bin/clpru}"
BUILD="$ROOT/build"
OBJ="$BUILD/fast_gpruio_fw.obj"
OUT="$BUILD/fast_gpruio_pru1_fw.out"

if [ "${1:-}" = "--clean" ]; then
    rm -rf "$BUILD"
fi
if [ ! -x "$CLPRU" ]; then
    echo "ERROR: clpru not found at $CLPRU." >&2
    echo "Run ./scripts/install_pru_cgt.sh or set PRU_CGT/CLPRU." >&2
    exit 1
fi

mkdir -p "$BUILD"
echo "Using $CLPRU"
"$CLPRU" --compiler_revision

"$CLPRU" \
    -v3 \
    -O2 \
    --endian=little \
    -I"$PRU_CGT/include" \
    -I"$ROOT/include" \
    -c "$ROOT/firmware/fast_gpruio_fw.c" \
    --output_file="$OBJ"

"$CLPRU" \
    -v3 \
    -z "$ROOT/firmware/AM335x_PRU.cmd" \
    "$OBJ" \
    -o "$OUT" \
    --entry_point=main

echo "Built: $OUT"
