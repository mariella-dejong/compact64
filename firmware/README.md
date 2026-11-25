# Firmware

Firmware for the microcontrollers of Compact 64, written in C against the
Raspberry Pi Pico SDK and built with the Arduino tools and the
[arduino-pico](https://github.com/earlephilhower/arduino-pico) core, which
contains the Pico SDK.

| Path | Contents |
| ---- | -------- |
| `u1401/` | RAM and ROM controller (U1401, #11). For now the bench build of #159 |
| `bench/exerciser/` | Bus exerciser for the U1401 bench test (#159), see [bench/README.md](bench/README.md) |
| `libraries/Compact64Bus/` | Header-only library shared by the sketches: U1401's pin map and the test patterns |
| `build.sh` | Builds and uploads a sketch with `arduino-cli` |

The host side of the bench test is in [`tools/bench`](../tools/bench).

## Building

You need `arduino-cli` (it is also inside the Arduino IDE 2) and the
arduino-pico core:

```sh
arduino-cli config add board_manager.additional_urls \
    https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli core update-index
arduino-cli core install rp2040:rp2040
```

Then, from the repository root:

```sh
firmware/build.sh u1401                    # compile
firmware/build.sh exerciser /dev/ttyACM0   # compile and upload
BOARD=weact firmware/build.sh u1401        # for a WeAct RP2350B board
```

Set `ARDUINO_CLI` if `arduino-cli` is not on your `PATH`. `TRACE_PINS=1`
builds U1401 with trace pins for the bench test's trace (bench/README.md).
The port may be a link such as `/dev/serial/by-id/...`. U1401 runs at
150 MHz, the RP2350's rated speed, unless `FREQ` (in MHz) says otherwise;
the bench needs about 300 (bench/README.md). The exerciser runs at
250 MHz, for 4 ns steps. Above 150 MHz both raise the core voltage from
1.10 to 1.20 V before they switch the clock (`c64bus_clock.h`): at 300 MHz
and 1.10 V, U1401 failed 5 to 17 % of its starts.
`BOARD` selects the board:

| `BOARD` | Board | FQBN |
| ------- | ----- | ---- |
| `rp2354b` (default) | An RP2354B board, as U1401 | `rp2040:rp2040:generic_rp2350:variantchip=RP2350B,flash=2097152_0` |
| `weact` | WeAct Studio RP2350B core board | `rp2040:rp2040:weact_rp2350b:flash=16777216_0` |

In the Arduino IDE, link or copy `libraries/Compact64Bus` into the
`libraries` folder of your sketchbook. For an RP2354B board select "Generic
RP2350" with chip variant RP2350B and 2 MB flash; then the CPU speed above,
and open the sketch.

The PIO programs are in `u1401/bus.pio` and `bench/exerciser/exerciser.pio`;
the Arduino tools do not assemble them, so the generated `.pio.h` files are
in the repository. After changing a program, run the `pioasm` that comes
with the core:

```sh
pioasm=$(echo ~/.arduino15/packages/rp2040/tools/pqt-pioasm/*/pioasm)
$pioasm -v 1 -o c-sdk firmware/u1401/bus.pio firmware/u1401/bus.pio.h
$pioasm -v 1 -o c-sdk firmware/bench/exerciser/exerciser.pio firmware/bench/exerciser/exerciser.pio.h
```

## Licence

Apache-2.0, see [LICENSE.md](../LICENSE.md).
