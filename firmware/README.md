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
```

Set `ARDUINO_CLI` if `arduino-cli` is not on your `PATH`. The script builds
for the WeAct Studio RP2350B core board (`rp2040:rp2040:weact_rp2350b`):
U1401 at 150 MHz, the RP2350's rated speed; the exerciser at 250 MHz, for
4 ns steps.

In the Arduino IDE, link or copy `libraries/Compact64Bus` into the
`libraries` folder of your sketchbook, select "WeAct Studio RP2350B Core
Board" and the CPU speed above, and open the sketch.

The exerciser's PIO programs are in `exerciser.pio`; the Arduino tools do not
assemble them, so the generated `exerciser.pio.h` is in the repository.
After changing the programs, run the `pioasm` that comes with the core:

```sh
~/.arduino15/packages/rp2040/tools/pqt-pioasm/*/pioasm -v 1 -o c-sdk \
    firmware/bench/exerciser/exerciser.pio firmware/bench/exerciser/exerciser.pio.h
```

## Licence

Apache-2.0, see [LICENSE.md](../LICENSE.md).
