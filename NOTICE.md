# Notices and attributions

c64-mini
Copyright (c) 2026 Mariella de Jong and contributors.
<https://github.com/leporidae-ca/c64-mini>

This file lists the people and projects whose work this project builds on,
the licences of third-party material included in this repository, and
trademark notices. Keep this file with any copy or derivative of this work
that you distribute. See [LICENSE.md](LICENSE.md) for the licences of the
project itself.

## C64 250407 Replica (main board)

The main board in [`hardware/main-board`](hardware/main-board) is derived
from the **C64 250407 Replica** KiCad project:

- Source: <https://github.com/bwack/C64-250407-Replica-KiCad>
- Imported version: V1.3, commit `9bf17bb868248b12fb12210d66be3a37ec4ab832`
  (2025-06-09)
- Copyright Bwack 2025
- Licence: CERN Open Hardware Licence v1.2
  ([`LICENSES/CERN-OHL-1.2.txt`](LICENSES/CERN-OHL-1.2.txt))

Original notice, reproduced from the upstream README:

> Copyright Bwack 2025
>
> This documentation describes Open Hardware and is licensed under the CERN
> OHL v. 1.2.
>
> You may redistribute and modify this documentation under the terms of the
> CERN OHL v.1.2. (http://ohwr.org/cernohl). This documentation is
> distributed WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING OF
> MERCHANTABILITY, SATISFACTORY QUALITY AND FITNESS FOR A PARTICULAR
> PURPOSE. Please see the CERN OHL v.1.2 for applicable conditions.

Credits from the upstream project:

- **Bwack** - layout, KiCad import, schematics and maintenance of the
  replica.
- **Michael K. (Der Alte Bastler)** - started the replica project in 2019
  and the reverse engineering of the PCB.
- **Langwell Cowan** - prototyping and testing of the board.
- **Gabriele Gorla (GGLABS)** - schematic symbol and ERC fixes (V1.2).
- **Fred Sauer** - PAL/NTSC jumper fix (V1.3).

## C64 RF Modulator Replacement (HF module replacement)

The HF module replacement circuit, on sheet
[`hardware/main-board/vic_ii_hf_av_out.kicad_sch`](hardware/main-board/vic_ii_hf_av_out.kicad_sch)
of the main board (formerly `hf_module_replacement.kicad_sch`), replaces the original RF modulator with composite and
S-Video (luma/chroma) outputs. It is derived from the schematic of the
**C64 RF Modulator Replacement** by mbarszcz-pcb:

- Source: <https://github.com/mbarszcz-pcb/c64-rf-modulator-replacement>
- Imported version: Rev D, commit `51bd665bfc97fd568f5d5c2ea828fc7b39c6541d`
  (2021-04-10)
- Copyright (c) 2020 mbarszcz-pcb
- Licence: MIT License ([`LICENSES/MIT.txt`](LICENSES/MIT.txt))

Only the schematic was used. The circuit was adapted to the 250407 long
board, redrawn as a sheet of the main board and laid out on the main board
itself. The changes are listed in the modification record in
[`hardware/main-board/README.md`](hardware/main-board/README.md).

Original licence notice, reproduced from the upstream repository:

> MIT License
>
> Copyright (c) 2020 mbarszcz-pcb
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

Credits from the upstream project: the design is based on the schematics of
similar designs, **Zero-X/OMS's Modified Commodore RF Modulator Design** and
**JMP$FCE2's S-Video only design**.

## 3D models

The main board includes 3D models that are used only for the 3D view in
KiCad. They are third-party material under their own licences:

- `hardware/main-board/3D-models/Bead_Axial_L5.1mm_D3.1mm_P12.50mm_Horizontal.wrl`
  Copyright (C) 2017, kicad StepUp. Licensed under the GNU General Public
  License v3 or later
  ([`LICENSES/GPL-3.0-or-later.txt`](LICENSES/GPL-3.0-or-later.txt)) with a
  special exception stating that using the model in a design does not by
  itself cause the design to be covered by the GPL. The full notice is in
  the header of the file.
- `hardware/main-board/3D-models/Resistors_THT_custom.3dshapes/*.wrl`
  Generated with `gen_model.py` by Bob Cousins (CC0) from the resistor body
  and colour ring models of
  [kicad3Dmodels](https://github.com/KammutierSpule/kicad3Dmodels),
  Copyright (c) 2017 Mario Luzeiro, licensed under an MIT-style licence
  ([`LICENSES/LicenseRef-kicad3Dmodels.txt`](LICENSES/LicenseRef-kicad3Dmodels.txt)).

Other 3D models referenced by the board (`${KISYS3DMOD}` paths) come from
the KiCad libraries installed with KiCad and are not included in this
repository.

## Original Commodore design

The C64 motherboard (assembly 250407, PCB KU-14194HB) was originally designed
by Commodore Business Machines. The replica is an independent re-creation of
that board's layout and schematics through reverse engineering; it does not
contain original Commodore design files.

## ROMs and firmware

This repository does not include the original Commodore 64 BASIC, KERNAL or
character generator ROM images, which are protected by copyright. ROM images
will only be included in this project with permission of the rights holder,
or when they are replacements that are free of these rights.

## Trademarks

Commodore, Commodore 64 and C64 are trademarks of their respective owners.
MOS Technology names and chip designations (such as 6510, VIC-II and SID)
are used only to identify the original components.
This project is independent and is not affiliated with, endorsed by or
sponsored by any of these trademark owners. Use of these names does not imply
any such relationship.
