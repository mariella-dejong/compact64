# Licensing

Copyright (c) 2026 Mariella de Jong and contributors.

This repository contains different kinds of work, each under a licence that
suits it. All licences used here are open licences: you are free to use,
study, modify and share the work, provided that you give proper credit when
you distribute it. The full licence texts are in the [`LICENSES/`](LICENSES)
directory.

| Part of the repository                | Licence                                              | SPDX identifier  |
| ------------------------------------- | ---------------------------------------------------- | ---------------- |
| Hardware design files (`hardware/`)   | CERN Open Hardware Licence v2 - Strongly Reciprocal  | `CERN-OHL-S-2.0` |
| Firmware and software (when added)    | Apache License 2.0                                   | `Apache-2.0`     |
| Documentation and everything else     | Creative Commons Attribution 4.0 International       | `CC-BY-4.0`      |

If a file or directory carries its own licence notice, that notice takes
precedence over this table.

## Hardware: CERN-OHL-S-2.0

Everything under `hardware/` is licensed under the
[CERN Open Hardware Licence Version 2 - Strongly Reciprocal](LICENSES/CERN-OHL-S-2.0.txt).

**Source Location:** <https://github.com/mariella-dejong/compact64>

In plain words (this summary is not a substitute for the licence text):

- You may use, copy, modify and manufacture the design, also commercially.
- You must keep all copyright, attribution and licence notices intact.
- If you distribute a modified design, you must mark it as modified (with a
  date and a short description of the changes) and license it under
  CERN-OHL-S-2.0 as well.
- If you distribute boards or products built from the design, you must give
  the recipients the design files, or tell them where to find them.

### Why a reciprocal licence for the hardware?

The main board is derived from the
[C64 250407 Replica](https://github.com/bwack/C64-250407-Replica-KiCad) by
Bwack, which is licensed under the
[CERN Open Hardware Licence v1.2](LICENSES/CERN-OHL-1.2.txt) (CERN-OHL-1.2).
That licence requires modified versions to be licensed under the CERN-OHL
v1.2 or a later version issued by CERN (section 3.4(e)). CERN-OHL-S-2.0 is
that later version: v1.2 has no variant identifier, and CERN-OHL v2 states
that designs without a variant identifier are treated as CERN-OHL-S
(section 8.3).

The upstream files as originally published remain available under
CERN-OHL-1.2; our modifications and the combined design are licensed under
CERN-OHL-S-2.0. Upstream notices are kept as required; see
[`hardware/main-board/README.md`](hardware/main-board/README.md) for the
provenance of the design and a record of the modifications made to it.

The HF module replacement circuit on the main board is derived from the
[C64 RF Modulator Replacement](https://github.com/mbarszcz-pcb/c64-rf-modulator-replacement)
by mbarszcz-pcb, which is licensed under the [MIT License](LICENSES/MIT.txt).
The MIT License allows that work to be included in the design under
CERN-OHL-S-2.0, as long as its copyright and permission notice is kept. The
notice is reproduced in [NOTICE.md](NOTICE.md).

## Firmware and software: Apache-2.0

Firmware, software, scripts and other code in this repository will be
licensed under the [Apache License 2.0](LICENSES/Apache-2.0.txt), unless a
file states otherwise. When you redistribute the code, in source or binary
form, you must include a copy of the licence and the attribution notices from
[NOTICE.md](NOTICE.md), and mark files you changed.

The Apache License 2.0 does not extend to the hardware design files, and the
CERN-OHL-S-2.0 does not extend to the firmware and software in this
repository.

## Documentation: CC-BY-4.0

Documentation, images and other content that is not hardware design files or
code is licensed under
[Creative Commons Attribution 4.0 International](LICENSES/CC-BY-4.0.txt).

## How to give attribution

When you redistribute this work or something based on it, please credit it
like this:

> Based on Compact 64 by Mariella de Jong (<https://github.com/mariella-dejong/compact64>),
> which is derived from the C64 250407 Replica by Bwack
> (<https://github.com/bwack/C64-250407-Replica-KiCad>) and includes a circuit
> derived from the C64 RF Modulator Replacement by mbarszcz-pcb
> (<https://github.com/mbarszcz-pcb/c64-rf-modulator-replacement>).

and keep the notices in [NOTICE.md](NOTICE.md) with your copy.

## No warranty

All work in this repository is provided "as is", without warranty of any
kind. See the disclaimer and limitation of liability sections of the
respective licences.

## Third-party material and trademarks

Some material in this repository comes from third parties and remains under
its own licence. Commodore 64 related trademarks and copyrights belong to
their respective owners. See [NOTICE.md](NOTICE.md) for details.
