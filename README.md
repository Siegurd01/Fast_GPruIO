# Fast_GPruIO

> **NEAR-MICROSECOND GPIO CONTROL ON ANY GPIO-CAPABLE POCKETBEAGLE HEADER PIN**

Fast_GPruIO lets PRU1 generate precisely timed, non-blocking GPIO pulses for
applications such as RS485 DIR/DE control. It is not limited to the small set
of pins connected directly to PRU `R30`: it can drive any PocketBeagle header
pin that the AM335x exposes as a GPIO.

## Why Fast_GPruIO?

Controlling GPIO directly from Linux userspace does not provide deterministic timing. Process scheduling, system calls, filesystem interfaces, and kernel activity can introduce unpredictable delays.

This becomes critical in timing-sensitive applications such as RS485 direction control. After transmitting the final byte, the DIR/DE pin must return to the receive state as quickly and predictably as possible. A delayed transition may corrupt or truncate an incoming response.

Fast_GPruIO moves pulse timing to the PRU, providing fast and predictable GPIO control while allowing the Linux application to remain non-blocking.

https://github.com/user-attachments/assets/accc500e-cb64-40a7-869a-c573f52b7f97

Pulse a pin by name; the last argument is the pulse duration in microseconds:

```bash
sudo ./userspace/build/fast_gpruio_cmd pulse P2.17 800
sudo ./userspace/build/fast_gpruio_cmd pulse P1.30 25
sudo ./userspace/build/fast_gpruio_cmd pulse GPIO2.1 100
```

Change `800`, `25`, or `100` to change the duration. Pin names may use board
notation (`P2.17`, `P1.30`) or raw GPIO notation (`GPIO65`, `GPIO2.1`).

## Quick start with the prebuilt firmware

Run these commands directly on PocketBeagle:

```bash
sudo apt update
sudo apt install git curl

git clone https://github.com/Siegurd01/Fast_GPruIO.git
cd Fast_GPruIO

mkdir -p build
curl -fL \
  https://github.com/Siegurd01/Fast_GPruIO/releases/latest/download/fast_gpruio_pru1_fw.out \
  -o build/fast_gpruio_pru1_fw.out
curl -fL \
  https://github.com/Siegurd01/Fast_GPruIO/releases/latest/download/fast_gpruio_cmd \
  -o build/fast_gpruio_cmd

chmod +x scripts/*.sh build/fast_gpruio_cmd
sudo ./scripts/load_pru1_fast_gpruio.sh build/fast_gpruio_pru1_fw.out
sudo ./build/fast_gpruio_cmd pulse P2.17 800 --idle 0 --active 1
```

The last command drives `P2.17` high for 800 microseconds and then returns it
low. Change `P2.17` to another GPIO-capable pin name and change `800` to the
required duration in microseconds. For an active-low pulse, use:

```bash
sudo ./build/fast_gpruio_cmd pulse P2.17 800 --idle 1 --active 0
```


## Download on Windows

Install [Git for Windows](https://git-scm.com/download/win), open Command
Prompt or a Windows Terminal Command Prompt tab, and run:

```batch
git clone https://github.com/Siegurd01/Fast_GPruIO.git
cd Fast_GPruIO
```

Alternatively, use **Code > Download ZIP** on the repository page and extract
the archive.

## Download on Linux

Install Git using the distribution package manager, then clone the project:

```bash
sudo apt update
sudo apt install git
git clone https://github.com/Siegurd01/Fast_GPruIO.git
cd Fast_GPruIO
```

On non-Debian distributions, install Git with that distribution's package
manager. A ZIP archive from **Code > Download ZIP** works as well.

Fast_GPruIO is PRU1 firmware and a small C userspace library for scheduling
non-blocking GPIO pulses on PocketBeagle and other AM335x boards. A Linux
process registers a GPIO once and submits short pulse commands through a PRU1
local-RAM mailbox. PRU1 changes the GPIO through AM335x GPIO registers and
returns the pin to its idle level at the requested deadline.

The repository is self-contained: it includes firmware sources, the shared
memory ABI, the Linux library and command-line tool, scripts that download the
TI PRU compiler, build scripts for Windows and Linux, deployment helpers, and a
target verification script. Generated toolchains and build products are not
stored in Git.

## Important hardware constraints

- PocketBeagle GPIO is 3.3 V only. Do not apply 5 V to a header pin.
- This firmware owns PRU1. Stop or replace any other PRU1 application first.
- This firmware owns the PRU-ICSS IEP timer. Do not run another PRUSS feature
  that assumes it owns IEP unless timer ownership is redesigned.
- It does not use PRU0 registers or PRU Shared RAM, so an independent PRU0
  application can run concurrently if it does not use IEP or the same GPIOs.
- The Linux library accesses `/dev/mem` and remoteproc. Normal operation
  therefore requires root privileges or an equivalent system design.
- Verify the selected header pin and wiring before running the pulse test. A
  logic analyzer or oscilloscope is strongly recommended.

## Repository layout

```text
firmware/                 PRU1 C source and AM335x linker command file
include/                  Shared firmware/userspace mailbox ABI
userspace/                Linux C library, CLI, and Makefile
scripts/install_*.{cmd,sh} TI PRU CGT download and installation
scripts/build_*.{cmd,sh}  Firmware builds for Windows and Linux
scripts/load_*.sh         PRU1 remoteproc deployment on the target
scripts/verify_*.sh       Target smoke test
```

## Host and target roles

There are two different build products:

1. `build/fast_gpruio_pru1_fw.out` is PRU firmware. Build it on Windows or an
   x86 Linux host with Texas Instruments PRU Code Generation Tools 2.3.3.
2. `userspace/build/fast_gpruio_cmd` is a Linux executable. Build it directly
   on PocketBeagle, or cross-compile it for ARM hard-float on an x86 Linux host.

The TI Linux compiler installer is an x86 host program. It is not intended to
run on the ARM PocketBeagle itself. Native Windows cannot run the Linux
userspace tool; use PocketBeagle, Linux, or WSL plus an ARM cross-compiler.

## Toolchain

Firmware builds are pinned to **TI PRU Code Generation Tools 2.3.3**. The
install scripts download the unmodified installer from TI's official software
server:

- [TI PRU CGT 2.3.3 download](https://www.ti.com/tool/download/PRU-CGT/2.3.3])

Running a TI installer is subject to the license displayed/provided by Texas
Instruments. The toolchain is installed locally under `.tools/` and is ignored
by Git. Override `PRU_CGT` when using an existing installation.

## Build firmware on Windows

Requirements:

- 64-bit Windows 10 or later;
- `cmd.exe` (Command Prompt or a Windows Terminal Command Prompt tab);
- internet access to `software-dl.ti.com` for the first installation.

From Command Prompt in the repository root:

```batch
scripts\install_pru_cgt.cmd
scripts\build_firmware.cmd --clean
```

To use an already installed compiler:

```batch
set "PRU_CGT=C:\ti\ti-cgt-pru_2.3.3"
scripts\build_firmware.cmd
```

The build creates:

```text
build/fast_gpruio_fw.obj
build/fast_gpruio_pru1_fw.out
```

## Build firmware on Linux

Use an x86 or x86-64 Linux host. On Debian/Ubuntu, install basic host tools:

```bash
sudo apt update
sudo apt install ca-certificates curl make gcc
```

Then install the compiler and build:

```bash
chmod +x scripts/*.sh
./scripts/install_pru_cgt.sh
make firmware
```

For an existing compiler installation:

```bash
PRU_CGT=/opt/ti/ti-cgt-pru_2.3.3 make firmware
```

If the TI installer needs additional compatibility libraries on a particular
distribution, run the installer from a terminal and install the library named
in its loader error. The compiler itself is invoked only by
`scripts/build_firmware.sh`; no Code Composer Studio installation is required.

## Build Linux userspace

### Directly on PocketBeagle

Install a C compiler and make using the package manager provided by the board
image, then run:

```bash
make -C userspace clean all
./userspace/build/fast_gpruio_cmd calc 8 115200 25 10
```

The calculation test should print `720`. It does not access hardware and does
not need root.

### Cross-compile on Debian/Ubuntu x86-64

```bash
sudo apt update
sudo apt install make gcc-arm-linux-gnueabihf
make -C userspace clean all CC=arm-linux-gnueabihf-gcc
file userspace/build/fast_gpruio_cmd
```

Copy the resulting ARM executable, the firmware `.out`, and the repository
scripts to the target. The userspace program has no third-party library
dependency beyond the Linux C runtime.

## Install and verify on PocketBeagle

The target image must provide:

- Linux remoteproc support for AM335x PRU-ICSS;
- a writable `/lib/firmware`;
- `/dev/mem` access;
- `make` and `gcc` if userspace is built on the target;
- `config-pin` for automatic header-pin pinmux configuration (recommended).

Copy or clone the repository onto PocketBeagle. Place the compiled firmware at
`build/fast_gpruio_pru1_fw.out`, then run:

```bash
chmod +x scripts/*.sh
sudo ./scripts/verify_on_target.sh
```

The script builds userspace, discovers PRU1 under `/sys/class/remoteproc`,
loads the firmware, prints mailbox status, and runs the non-hardware `calc`
test. It does not toggle a GPIO unless a pin is explicitly supplied.

After checking the board schematic and connecting a logic analyzer, generate
an 800 microsecond active-high pulse:

```bash
sudo ./scripts/verify_on_target.sh P2.17 800
```

Expected results:

- remoteproc reports `running`;
- `stat` prints magic `0x4750494F`, version `1`, and PRU frequency
  `200000000`;
- after the pulse, `accepted` and `expired` increase;
- the measured pulse is close to the requested duration, subject to GPIO and
  measurement propagation delay.

You can deploy without the combined smoke test:

```bash
sudo ./scripts/load_pru1_fast_gpruio.sh build/fast_gpruio_pru1_fw.out
make -C userspace
sudo ./userspace/build/fast_gpruio_cmd stat
sudo ./userspace/build/fast_gpruio_cmd pulse P2.17 800
```

Set `PRU1_REMOTEPROC=/sys/class/remoteproc/remoteprocN` when automatic
discovery does not match a custom kernel's naming.

## Supported pin names

When `config-pin` is installed, board pins such as `P2.17` are resolved using
`config-pin -i`, switched to GPIO mode, and registered in the next PRU slot.
If the pin exposes no GPIO mode, registration fails.

Without `config-pin`, the library uses its embedded PocketBeagle pin table. It
can resolve known header pins but cannot change pinmux, so pinmux must already
be configured by the device tree or another startup mechanism.

Raw identifiers are also accepted, but never change pinmux:

```text
GPIO65  -> Linux GPIO 65 -> GPIO2.1
GPIO2.1 -> AM335x GPIO bank 2, bit 1
GPIO2_1 -> same as GPIO2.1
```

Inspect the current target image with:

```bash
./scripts/scan_pocketbeagle_pin_functions.sh \
  --out pb_pin_functions.tsv \
  --raw-out pb_pin_functions_raw.txt

./scripts/probe_pocketbeagle_pins.sh --gpio-only --out pb_gpio_pins.txt
```

## C API example

```c
#include "fast_gpruio_lib.h"

struct fast_gpruio_ctx gpio = {0};
uint32_t pin_id;
uint32_t seq;

if (fast_gpruio_init(&gpio, NULL, 2000) != 0)
    return 1;
if (fast_gpruio_register_pin(&gpio, "P2.17", 0, &pin_id) != 0)
    return 1;
if (fast_gpruio_start_pulse(&gpio, pin_id, 800, 1, &seq) != 0)
    return 1;

fast_gpruio_close(&gpio);
```

`fast_gpruio_start_pulse()` waits only until PRU1 accepts or rejects the
command. It does not wait for the pulse to finish. Reusing a busy `pin_id`
returns `FAST_GPRUIO_STATUS_PIN_BUSY`; other registered pins remain available.
Up to 16 physical GPIOs can be registered, and the command ring has 16 slots.

Pin registration lives in PRU1 RAM and persists across Linux process restarts
while PRU1 remains running. Registering the same physical GPIO again reuses its
existing `pin_id`.

For UART 8N1, a byte occupies 10 wire bits. Calculate a DIR/DE pulse including
a guard interval with:

```c
uint32_t us = fast_gpruio_calc_pulse_length_us(tx_len, 10, 115200, 25);
```

## Implementation details

- PRU clock: 200 MHz (`200` cycles per microsecond).
- Timer: PRU-ICSS IEP at `0x0002E000`.
- PRU1 local mailbox: PRU address `0x00001000`.
- ARM physical mailbox address: `0x4A303000`.
- Mailbox ABI: `include/fast_gpruio_shared.h`.
- GPIO writes use AM335x `SETDATAOUT` and `CLEARDATAOUT` registers.
- A duration must be nonzero and less than `0x80000000` PRU cycles, or roughly
  10.7 seconds at 200 MHz.

## Troubleshooting

**`clpru` not found**

Run the matching install script, or set `PRU_CGT` to a directory containing
`bin/clpru` (`bin/clpru.exe` on Windows).

**PRU1 remoteproc not found**

Check `ls /sys/class/remoteproc` and each `remoteproc*/name`. Confirm that the
kernel/device tree enables PRU-ICSS. Override `PRU1_REMOTEPROC` if needed.

**`open /dev/mem: Permission denied`**

Run the target command as root. Some hardened kernels disable `/dev/mem`; this
implementation then requires a kernel/UIO abstraction before it can run.

**`config-pin` missing or pinmux failure**

Install the board image's pinmux utility, or configure the pin in the device
tree and use a verified raw GPIO identifier. Never assume a header pin is in
GPIO mode merely because the numeric GPIO exists.

**Firmware loads but mailbox magic is wrong**

Confirm that PRU1, not PRU0, loaded `fast_gpruio_pru1_fw.out`, and that no
service immediately replaces it. Check kernel messages with `dmesg`.

**Pulse starts but never expires correctly**

Stop other PRU applications that configure the IEP timer. IEP is a shared PRUSS
resource and Fast_GPruIO assumes exclusive ownership.

## TODO

- Add and document BeagleBone Black support, including pin mapping and hardware
  timing verification.
- Port to PocketBeagle 2 and verify its PRU, GPIO, pinmux, remoteproc, and
  userspace memory-access interfaces.

## License

Fast_GPruIO is distributed under the MIT License. See [LICENSE](LICENSE).
