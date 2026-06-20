#!/usr/bin/env bash
set -euo pipefail

VERSION="2.3.3"
INSTALLER="ti_cgt_pru_${VERSION}_linux_installer_x86.bin"
URL="${PRU_CGT_URL:-https://software-dl.ti.com/codegen/esd/cgt_public_sw/PRU/${VERSION}/${INSTALLER}}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_PREFIX="${PRU_CGT_PREFIX:-$ROOT/.tools}"
TOOLCHAIN_DIR="$INSTALL_PREFIX/ti-cgt-pru_${VERSION}"
CACHE_DIR="${PRU_CGT_CACHE:-$ROOT/.cache}"
INSTALLER_PATH="${PRU_CGT_INSTALLER:-$CACHE_DIR/$INSTALLER}"

case "$(uname -m)" in
    x86_64|amd64|i?86) ;;
    *)
        echo "ERROR: TI's Linux installer is an x86 host tool and cannot run on $(uname -m)." >&2
        echo "Build firmware on an x86 Linux/Windows host, then copy the .out file to PocketBeagle." >&2
        exit 1
        ;;
esac

if [ -x "$TOOLCHAIN_DIR/bin/clpru" ]; then
    echo "TI PRU CGT is already installed at $TOOLCHAIN_DIR"
    "$TOOLCHAIN_DIR/bin/clpru" --compiler_revision
    exit 0
fi

mkdir -p "$CACHE_DIR" "$INSTALL_PREFIX"
if [ ! -f "$INSTALLER_PATH" ]; then
    echo "Downloading TI PRU CGT $VERSION from:"
    echo "$URL"
    if command -v curl >/dev/null 2>&1; then
        curl --fail --location --retry 3 --output "$INSTALLER_PATH" "$URL"
    elif command -v wget >/dev/null 2>&1; then
        wget --output-document="$INSTALLER_PATH" "$URL"
    else
        echo "ERROR: install curl or wget first." >&2
        exit 1
    fi
fi

chmod +x "$INSTALLER_PATH"
"$INSTALLER_PATH" --mode unattended --prefix "$INSTALL_PREFIX"

if [ ! -x "$TOOLCHAIN_DIR/bin/clpru" ]; then
    echo "ERROR: installation finished but clpru was not found in $TOOLCHAIN_DIR/bin." >&2
    exit 1
fi

echo "Installed TI PRU CGT $VERSION"
"$TOOLCHAIN_DIR/bin/clpru" --compiler_revision
