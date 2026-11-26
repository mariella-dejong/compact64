# Firmware

Firmware for the microcontrollers of Compact 64, written in C against the
Raspberry Pi Pico SDK and built with the Arduino tools and the
[arduino-pico](https://github.com/earlephilhower/arduino-pico) core, which
contains the Pico SDK.

| Path | Contents |
| ---- | -------- |
| `u1401/` | RAM and ROM controller (U1401, #11). For now the bench build of #159 |
| `bench/exerciser/` | Bus exerciser for the bench tests of U1401 (#159) and the CIA controllers (#182), see [bench/README.md](bench/README.md) |
| `libraries/Compact64Bus/` | Header-only library shared by the sketches: U1401's pin map and the test patterns |
| `libraries/Compact64Cia/` | The 6526 model of the CIA controllers (#182), also used by the exerciser; host tests in [`tools/cia`](../tools/cia/README.md) |
| `build.sh` | Builds and uploads a sketch with `arduino-cli` |
| `azure-pipelines.yml` | Azure Pipeline: builds every sketch and publishes the images, see [Continuous integration](#continuous-integration) |

The host side of the bench test is in [`tools/bench`](../tools/bench).

## Building

You need `arduino-cli` (it is also inside the Arduino IDE 2) and version
6.3.2 of the arduino-pico core, the one the pipeline uses:

```sh
arduino-cli config add board_manager.additional_urls \
    https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli core update-index
arduino-cli core install rp2040:rp2040@6.3.2
```

Then, from the repository root:

```sh
firmware/build.sh u1401                    # compile
firmware/build.sh exerciser /dev/ttyACM0   # compile and upload
BOARD=weact firmware/build.sh u1401        # for a WeAct RP2350B board
```

Set `ARDUINO_CLI` if `arduino-cli` is not on your `PATH`. `CLEAN=1` builds
from scratch instead of reusing `arduino-cli`'s build cache. `TRACE_PINS=1`
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

In the Arduino IDE, link or copy `libraries/Compact64Bus` and
`libraries/Compact64Cia` into the `libraries` folder of your sketchbook. For
an RP2354B board select "Generic RP2350" with chip variant RP2350B and 2 MB
flash; then the CPU speed above, and open the sketch.

The PIO programs are in `u1401/bus.pio` and `bench/exerciser/exerciser.pio`;
the Arduino tools do not assemble them, so the generated `.pio.h` files are
in the repository. After changing a program, run the `pioasm` that comes
with the core:

```sh
pioasm=$(echo ~/.arduino15/packages/rp2040/tools/pqt-pioasm/*/pioasm)
$pioasm -v 1 -o c-sdk firmware/u1401/bus.pio firmware/u1401/bus.pio.h
$pioasm -v 1 -o c-sdk firmware/bench/exerciser/exerciser.pio firmware/bench/exerciser/exerciser.pio.h
```

## Continuous integration

[`azure-pipelines.yml`](azure-pipelines.yml) builds every sketch on pushes to
`main` and on pull requests that change `firmware/`, on the self-hosted
`linux-x64` agents, with arduino-pico 6.3.2. It:

- checks that the generated `.pio.h` files match their `.pio` sources;
- builds U1401 at 150 and 300 MHz and the exerciser at 250 MHz, each for the
  RP2354B board and the WeAct board, with `CLEAN=1 build.sh`, and fails on any
  compiler warning;
- publishes two pipeline artifacts, `u1401-firmware` and
  `exerciser-firmware`: the `.uf2` and `.elf` files named after the board,
  the clock and the build number (for example
  `u1401-weact-300mhz-20261125.1.uf2`), a `BUILD_INFO.txt` with the commit,
  and `SHA256SUMS`.

### Setting it up in Azure DevOps

1. **New pipeline:** GitHub, repository `mariella-dejong/compact64` (service
   connection `mariella-dejong`), existing Azure Pipelines YAML file,
   path `firmware/azure-pipelines.yml`.
2. **No builds from forks.** The repository is public, and a pull request
   from a fork runs the fork's version of `azure-pipelines.yml` and
   `build.sh` on our agents. Nothing in the repository can prevent that, so
   it is switched off in Azure DevOps:
   - in the pipeline: Edit, the "..." menu, Triggers, Pull request
     validation: clear "Build pull requests from forks of this repository";
   - for the whole project as well: Project settings, Pipelines, Settings,
     "Limit building pull requests from forked GitHub repositories": "Do not
     build pull requests from forks".

   As a second line, the pipeline skips its stage when
   `System.PullRequest.IsFork` is true. A maintainer builds a fork's change
   after review by pushing it to a branch of this repository.

## Licence

Apache-2.0, see [LICENSE.md](../LICENSE.md).
