# CIA model tests

Host tests of the 6526 model that the CIA controllers U1501 and U1601 share
([`firmware/libraries/Compact64Cia`](../../firmware/libraries/Compact64Cia),
#182). They run without hardware:

```sh
make -C tools/cia/test          # all tests
make -C tools/cia/test unit     # unit tests only
make -C tools/cia/test lorenz   # Lorenz test suite only
```

| Test | What it checks |
| ---- | -------------- |
| `test/lorenz.c` | Runs the 18 CIA programs of Wolfgang Lorenz's C64 Emulator Test Suite on a minimal C64: floooh's cycle-stepped 6502 (`m6502.h`), 64 KB of RAM, both CIAs and the raster line. The programs check timers, interrupts, one-shot mode, force load and PB6/PB7 at single-cycle resolution against tables measured on a real C64. The KERNAL routines they call are traps or a few bytes of our own code; no ROM image is needed. |
| `test/unit.c` | What the suite does not cover: reset state, ports, `~PC`, `~FLAG`, time of day with alarm and latches, the serial port. |

`lorenz` takes options before the program files: `-v` prints each test's
screen output, `-k` answers the test's "press a key" after an error so that
it reports every mismatch, `-t` traces CIA 1 accesses with their cycle
numbers to stderr, and `-8521` runs the new CIA revision.

## Status

- **6526 (default):** all 18 Lorenz programs pass.
- **8521:** a setting of the model (`CIA_MODEL_8521`), with the interrupt
  one cycle earlier. The suite's tables are for the 6526, so 11 of 18 pass
  as expected; the 8521 differences are not verified yet.
- **Time of day, serial port:** tested against the data sheet only.

The third-party material is described in
[`third_party/README.md`](third_party/README.md).
