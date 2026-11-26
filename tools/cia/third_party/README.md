# Third-party test material

Used only by the host tests in [`../test`](../test); none of it is built into
the firmware. Each part keeps its own licence, see also
[NOTICE.md](../../../NOTICE.md).

| Folder | What | Origin | Licence |
| ------ | ---- | ------ | ------- |
| [`chips/`](chips) | `m6502.h`, a cycle-stepped 6502/6510 emulator by Andre Weissflog (floooh) | <https://github.com/floooh/chips>, file `chips/m6502.h` at commit `7ef8776f637a6424ac615da64de6b968b06de791` | zlib ([`LICENSES/Zlib.txt`](../../../LICENSES/Zlib.txt)); the notice is in the file itself |
| [`lorenz-2.15/`](lorenz-2.15) | The CIA programs of Wolfgang Lorenz's C64 Emulator Test Suite 2.15 | <https://6502.org/tools/emu/>; these copies from <https://github.com/floooh/chips-test>, folder `tests/testsuite-2.15` at commit `66e33887371ef7b2e4daa21e8e0902f676d0b72a` | Public domain ("Public Domain, no Copyright") |

Both are unmodified copies. To update them, fetch the files again from the
same places and record the new commits here.
