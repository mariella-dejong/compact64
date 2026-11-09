# U1401 bench test

The bench test of #159 runs the RAM and ROM controller firmware (U1401,
[`../u1401`](../u1401)) against a bus exerciser on a second board, before the
6510, the VIC-II, the CPLD or the PCB exist. U1401 is risk 1 in the risk
register (#113).

- **Board under test (DUT):** an RP2350B board running the U1401 firmware,
  with U1401's pin map (main-board README, RAM and ROM replacement, Pins).
- **Exerciser:** a second RP2350B board that plays the 6510, the VIC-II and
  the CPLD, and checks the data that comes back.
- **Host:** [`tools/bench/bench.py`](../../tools/bench/bench.py) runs the
  tests and sweeps over USB serial.

## Boards

Both boards need all 48 GPIOs on headers, USB on the board, and no PSRAM:
the RP2350's PSRAM chip select can only be GPIO0, GPIO8, GPIO19 or GPIO47,
and U1401 uses all four. The sketches are built for the WeAct Studio RP2350B
core board. On that board:

- the LED is on GP25 (`~BASIC`), which only adds a small load;
- the user button is on GP23 (`A15`): do not press it during a test.

A Raspberry Pi Pico 2 does not work: its RP2350A has 30 GPIOs, and only 26
reach the headers. A spare Pico 2 can run a logic analyser firmware, for
example [LogicAnalyzer](https://github.com/gusmanb/logicanalyzer).

## Wiring

Every signal runs from GPIOn on one board to GPIOn on the other. Keep the
jumpers short (under 10 cm) and add at least six ground wires. Power each
board from its own USB port; do not connect `3V3` or `VBUS` between them.

| GPIO | Signal | Direction | Notes |
| ---- | ------ | --------- | ----- |
| 0 to 7 | D0 to D7 | both | 330 Ohm in series |
| 8 to 23 | A0 to A15 | exerciser to DUT | |
| 24 to 28 | `~CASRAM`, `~BASIC`, `~KERNAL`, `~CHAROM`, `~COLORAM` | exerciser to DUT | |
| 29, 30 | `R/~W`, `GR/~W` | exerciser to DUT | |
| 31, 32 | `VA4`, `VA5` | exerciser to DUT | |
| 33, 34 | `~VA14`, `~VA15` | exerciser to DUT | |
| 35 | `AEC` | exerciser to DUT | |
| 36 to 39 | `COLORD0` to `COLORD3` | DUT to exerciser | 330 Ohm in series |
| 44 | Trigger | DUT to exerciser | Optional; the DUT toggles it on a late answer |
| 46, 47 | `PHI2`, `~CAS` | exerciser to DUT | |
| 40 to 43, 45 | | | Not connected between the boards |

The 330 Ohm resistors keep both boards safe if both drive a data line at
once. They slow the edges by a few nanoseconds.

**Release check (optional):** connect the DUT side of the D0 resistor to
exerciser GPIO42 and the DUT side of the D7 resistor to exerciser GPIO43,
then set `w_release` above 0. After each such read the exerciser drives the
inverse data right after the select rises, and these two wires show whether
the DUT still holds the bus.

Exerciser GPIO45 is high during each batch, as a trigger for a logic
analyser.

## Running

1. Build and upload (see [../README.md](../README.md)):
   `firmware/build.sh u1401 <DUT port>` and
   `firmware/build.sh exerciser <exerciser port>`.
2. **Calibrate the exerciser** with the DUT disconnected. Jumper exerciser
   GPIO45 to GPIO0 and to GPIO36, then:

   ```sh
   tools/bench/bench.py cal 45 0 -4 12 --ex <port>
   tools/bench/bench.py cal 45 36 -4 12 --ex <port>
   ```

   The first offset with `high` equal to `reps` is the delay from an output
   change to the sampler seeing it, in exerciser clocks. Both should match;
   a difference is a skew between the two PIO blocks.
3. **Smoke test** with both boards wired:
   `tools/bench/bench.py run --ex <port> --dut <port> --batches 100`.
   Expect no errors at the default (PAL) timing.
4. **Latency per access type:** sample earlier and earlier after the select:

   ```sh
   tools/bench/bench.py sweep sample_off 20 200 4 --ex <port> --dut <port> \
       --set sample_mode=1 --csv latency.csv
   ```

   The first offset without errors is the DUT's latency plus the calibration
   offset. The budget for RAM reads is about 135 ns after `~CASRAM` (main
   board README, RAM and ROM replacement, Timing), an estimate; leave about
   20 to 25 ns for the real bus and U1402, which the bench does not have.
5. **Speed against sample point:**

   ```sh
   tools/bench/bench.py shmoo --ex <port> --dut <port> \
       --speeds 100:200:10 --offsets 40:200:8 --csv shmoo.csv
   ```

   The goal is no errors up to about 125 % speed at the real latch points.
6. **Soak:** `run --batches 100000`, at PAL and with `--ntsc`.

Other parameters to sweep: `d_casram` (`~CAS` to `~CASRAM`, 20 to 35 ns from
the CPLD, #93), `t_col` (column address after `~CAS`, #152), `t_dv` and
`t_dhold` (6510 write data). On the DUT, `sync bypass` skips the input
synchronizer and `vicdelay` takes the VIC-II address later.

## Exerciser commands

| Command | Does |
| ------- | ---- |
| `info` | Clock and seeds |
| `get` | All parameters |
| `set <name> <value>` | One parameter |
| `pal`, `ntsc` | Reset the parameters to PAL or NTSC |
| `fill [seed]` | Reset the memory model to a test pattern (also send it to the DUT) |
| `run [batches] [seed]` | Run batches; prints `result`, a `check` line per check, up to eight `fail` lines, and `end` |
| `cal <out> <in> <from> <to> [reps]` | Calibration, DUT disconnected |

`result` reports `moved` and `max_move_ns`: one list cannot change its pins
more often than every three clocks (12 ns), so when two edges of different
signals fall closer together the later one moves. Moves of one clock at the
half-cycle boundaries are normal. A run with `tx_stall`, `rx_stall`,
`timeout` or `overflow` is not valid.

## Parameters

Cycle times are in ns and scale with `speed`; chip delays are in ns and do
not. The defaults are PAL (8565): estimates from the datasheets and the
README timing notes, to be replaced by measurements on the prototype (#158).

| Parameter | Default | Meaning |
| --------- | ------- | ------- |
| `half_ns` | 507 | Half cycle (489 NTSC) |
| `t_cas` | 260 | `~CAS` falls, after the half starts |
| `t_addr` | 100 | 6510 address and `R/~W` valid |
| `t_vaddr` | 200 | VIC-II A0 to A11 valid |
| `t_dv` | 200 | 6510 write data valid |
| `t_setup` | 100 | Reader's setup time before the half ends (sample mode 0) |
| `t_cas_rise` | 0 | `~CAS` rises, relative to the end of the half (not scaled) |
| `d_casram` | 25 | `~CAS` to `~CASRAM`, both edges |
| `t_col` | 10 | `~CAS` to the column address on `VA4`, `VA5` |
| `t_pla` | 15 | Address to chip select |
| `t_dhold` | 30 | 6510 write data hold after the half |
| `t_pre_start`, `t_pre_end` | 60, 20 | Precharge of D to the inverse data before a read, so that a late answer cannot pass on the charge left on the bus |
| `t_rel_drive`, `t_rel_sample` | 5, 40 | Release check, after the select rises |
| `sample_mode`, `sample_off` | 0, 100 | 0: sample at the end of the half minus `t_setup`; 1: `sample_off` ns after the select falls |
| `speed` | 100 | Percent of the real cycle rate |
| `cycles` | 512 | Cycles per batch |
| `bank` | 4 | VIC-II bank 0 to 3; 4 picks one per batch |
| `w_*` | | Weights of the half-cycle kinds (`gen.h`) |

## Board under test commands

`info`, `stats`, `clear`, `fill [seed]`, `sync bypass|normal`,
`vicdelay <loops>`, `aecsettle <loops>`, `peek <addr>`.

## Simulation

`make -C tools/bench/sim` runs the exerciser's generator and checker against
a model of the PIO lists and of the U1401 firmware, without hardware. It
checks that an ideal firmware passes, that a slow one fails, that the sweep
finds its latency, and that the release check works.

## Not covered

- U1402 and the 5 V side of the bus: everything here runs at 3.3 V.
- The real timing of the 6510, the VIC-II and the CPLD; the margins measured
  here say how much of it the firmware can absorb.
- The EEPROM and the I2C link to the supervisor.
