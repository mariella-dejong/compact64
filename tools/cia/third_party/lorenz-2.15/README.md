# C64 Emulator Test Suite 2.15 (CIA part)

By Wolfgang Lorenz. From its documentation:

> C64 Emulator Test Suite - Public Domain, no Copyright

- Original: <https://6502.org/tools/emu/>
- These copies: <https://github.com/floooh/chips-test>, folder
  `tests/testsuite-2.15`, commit `66e33887371ef7b2e4daa21e8e0902f676d0b72a`.

Only the parts the 6526 model is tested with are included, unmodified:

| Path | What |
| ---- | ---- |
| `bin/` | The 18 CIA test programs, as C64 program files (load address first) |
| `src/` | Their assembler sources, for reading what a failing test does |
| `Test_Suite_2_15-CIA.txt` | The introduction of the suite's documentation and the descriptions of these programs, with the cycle tables they check |

The programs were measured on the author's C64-I (PAL), which has 6526
("old") CIAs. [`tools/cia/test`](../../test) runs them.
