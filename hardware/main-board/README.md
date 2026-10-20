# Main board

KiCad project of the c64-mini main board. Open `250407_.kicad_pro` in KiCad.

The design starts as an unmodified copy of the
[C64 250407 Replica](https://github.com/bwack/C64-250407-Replica-KiCad) by
Bwack: a reverse-engineered replica of the Commodore 64 assembly 250407
motherboard (PCB KU-14194HB). It will be modified step by step into the
c64-mini main board; see the [project README](../../README.md) for the
roadmap.

## Licence

Copyright Bwack 2025.
Modifications copyright (c) 2026 Mariella de Jong and contributors.

The original design is licensed under the
[CERN Open Hardware Licence v1.2](../../LICENSES/CERN-OHL-1.2.txt). The
modified design is licensed under the
[CERN Open Hardware Licence Version 2 - Strongly Reciprocal](../../LICENSES/CERN-OHL-S-2.0.txt),
as permitted by section 3.4(e) of CERN-OHL v1.2.

Source Location: <https://github.com/leporidae-ca/c64-mini>

The 3D models in `3D-models/` are third-party material under their own
licences. See [NOTICE.md](../../NOTICE.md) for all attributions, and
[LICENSE.md](../../LICENSE.md) for how the licences apply.

## Provenance

| Item              | Value                                                                 |
| ----------------- | --------------------------------------------------------------------- |
| Upstream          | <https://github.com/bwack/C64-250407-Replica-KiCad>                   |
| Upstream version  | PCB and schematics V1.3                                               |
| Upstream commit   | `9bf17bb868248b12fb12210d66be3a37ec4ab832` (2025-06-09)               |
| Last saved with   | KiCad 9.0.1                                                           |
| Imported on       | 2026-10-20                                                            |

### What was imported

Only the files that make up the KiCad project were imported, unchanged:

- Project, schematics and PCB: `250407_.kicad_pro`, `250407_.kicad_sch`,
  `250407_sheet2.kicad_sch`, `250407_.kicad_pcb`
- Library tables: `fp-lib-table`, `sym-lib-table`
- Project libraries: `KU-14194HB-RevB-KiCad.pretty/`,
  `KU-schematics-library.lib`/`.dcm`, `250407_-rescue.lib`/`.dcm`
- 3D models referenced by the PCB: `3D-models/`

### What was left out

- Fabrication outputs and exports: Gerbers and drill files, the schematic
  PDF, the BOM CSV, the interactive BOM, the board SVG and the screenshots.
  These can be regenerated from the KiCad files.
- KiCad local and legacy files: `250407_.kicad_prl` (local user settings) and
  `250407_-cache.lib` (KiCad 5 symbol cache; symbols are embedded in the
  schematics since KiCad 6).
- `KU-14194HB-RevB-KiCad.pretty/photofootprint.kicad_mod`: a 15 MB traced
  photo of the original board used as a layout aid. It is not used on the
  PCB.
- The 3D model generator (`gen_model.py`) and its source models
  (`RESAD780W55L630D240B.wrl`, `ring_*.wrl`, `unitary_ring.fcstd`), and
  unused generated resistor models. Only models referenced by the PCB were
  kept.

## Known issues inherited from upstream

From the upstream V1.3 change log:

- Updating the PCB from the schematic requires annotating the schematics,
  which renames the resistor array sub-parts (for example `R1A` becomes
  `R1A1`) and turns them into new footprints on the PCB. Upstream therefore
  did not sync schematic and PCB in V1.3. This is addressed by #4.
- KiCad asks to refill the copper zones because of changed fill constraints;
  upstream did not refill them in V1.3.
- Some component values in the schematics depend on the clock circuit (PAL
  or NTSC) and the PLA model, and were left blank. They can be found in the
  table next to the original 250407 schematics at
  <http://www.zimmers.net/anonftp/pub/cbm/schematics/computers/c64/index.html>.

## Modifications

CERN-OHL requires a dated record of changes made to the original design.
Add an entry here for every change to the design files, newest first.

| Date       | Description                                                   |
| ---------- | ------------------------------------------------------------- |
| 2026-10-20 | Configured the clock circuit for NTSC (Commodore 251138 notes 2, 7 and 8): Y1 14.31818 MHz, C70 16 pF, R52 680 Ohm, R53 910 Ohm; E3 (NTSC link) fitted, E2 (PAL link), C203 and C204 not fitted; added NTSC note blocks (upper right of both sheets) with boxed markers |
| 2026-10-20 | Added `LCSC Part #` fields to the schematic symbols and PCB footprints of all parts with an exact LCSC match |
| 2026-10-20 | Restored the original designators of 14 sub-units on sheet 2 (`RP1A`-`RP1D`, `RP2A`-`RP2D`, `U8B`, `U15A`, `U15B`, `U27B`-`U27D`) that were renamed with a `1` suffix during the reorganization |
| 2026-10-20 | Renamed the project to `c64-mini` and moved both original schematic pages into a `250407` sub-sheet of a new root sheet |
| 2026-10-20 | Imported upstream V1.3 KiCad files unchanged (no modifications) |
