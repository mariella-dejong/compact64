# Main board

KiCad project of the c64-mini main board. Open `c64-mini.kicad_pro` in KiCad.

The design starts as an unmodified copy of the
[C64 250407 Replica](https://github.com/bwack/C64-250407-Replica-KiCad) by
Bwack: a reverse-engineered replica of the Commodore 64 assembly 250407
motherboard (PCB KU-14194HB). It will be modified step by step into the
c64-mini main board; see the [project README](../../README.md) for the
roadmap.

## Status

The schematic is under active redesign. The PCB layout is not updated along
with it: it includes the changes up to and including the HF module
replacement (#8), but not the `Power Supply` sheet (#10) or later changes.
Because the board will be laid out again from scratch for the smaller form
factor once the schematic is complete, intermediate schematic changes are
not carried over to the current layout. Expect the DRC schematic parity
check to report these differences, and do not use the current PCB files for
manufacturing.

The power supply is being converted step by step to a single 12 V DC input.
At this stage `+5V` comes from the 5 V buck converter on the `Power Supply`
sheet (#28), while `CAN+5V`, `+12V` and `+9V_UNREG` still come from the 9 V AC
of the original power brick. The schematic therefore needs both the 12 V DC
input and the 9 V AC input, and the power switch SW1 only switches the 9 V AC.
This intermediate state is not meant to be built.

## Licence

Copyright Bwack 2025.
Modifications copyright (c) 2026 Mariella de Jong and contributors.

The original design is licensed under the
[CERN Open Hardware Licence v1.2](../../LICENSES/CERN-OHL-1.2.txt). The
modified design is licensed under the
[CERN Open Hardware Licence Version 2 - Strongly Reciprocal](../../LICENSES/CERN-OHL-S-2.0.txt),
as permitted by section 3.4(e) of CERN-OHL v1.2.

Source Location: <https://github.com/leporidae-ca/c64-mini>

The HF module replacement sheet (`hf_module_replacement.kicad_sch`) is
derived from the
[C64 RF Modulator Replacement](https://github.com/mbarszcz-pcb/c64-rf-modulator-replacement),
Copyright (c) 2020 mbarszcz-pcb, licensed under the
[MIT License](../../LICENSES/MIT.txt). It is included in the design under
CERN-OHL-S-2.0, and its copyright and permission notice is kept in
[NOTICE.md](../../NOTICE.md) and on the sheet itself.

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

### HF module replacement

The circuit on the `HF Module Replacement` sheet replaces the original RF
modulator (M1) with composite and S-Video (luma/chroma) outputs on the A/V
connector. It is based on the schematic of the
[C64 RF Modulator Replacement](https://github.com/mbarszcz-pcb/c64-rf-modulator-replacement)
by mbarszcz-pcb.

| Item              | Value                                                                 |
| ----------------- | --------------------------------------------------------------------- |
| Upstream          | <https://github.com/mbarszcz-pcb/c64-rf-modulator-replacement>        |
| Upstream version  | Rev D                                                                 |
| Upstream commit   | `51bd665bfc97fd568f5d5c2ea828fc7b39c6541d` (2021-04-10)               |
| Licence           | MIT License                                                           |
| Imported on       | 2026-10-21                                                            |

Only the schematic (`C64 RF Modulator Replacement.sch`) was used, as the
basis for the sheet. The upstream PCB, Gerbers, BOM, images and footprint
library were not imported: the circuit is laid out on the main board itself.

The upstream design is a plug-in board for several C64 board revisions, set
up with jumpers. On this board it is fixed to the 250407 long board
configuration:

- Power comes from `+9V_UNREG` through the on-sheet 78L05 regulator. A
  47 Ohm series resistor (R501) in front of the regulator takes part of the
  voltage drop, because the circuit draws about 52 mA and the SMD (SOT-89)
  regulator can dissipate less heat than the TO-92 part upstream uses.
- The VIC-II luma pull-up is a fixed 470 Ohm resistor for the NMOS
  6567/6569 VIC-II. Upstream selects 470 Ohm or 1 kOhm with a jumper.
- The pinout selection jumpers for other board revisions are left out.
- The RF modulator's audio input is not used. Audio still goes directly to
  the A/V connector.

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
| 2026-10-23 | Powered the board's `+5V` from the 5 V buck converter instead of the 5 V of the power brick, and removed the brick's 5 V input filter (L5, C97, C100). SW1 pin 4, the switched 5 V of the brick, is no longer connected, so SW1 only switches the 9 V AC |
| 2026-10-23 | Replaced the global labels `PHI_COLOR` and `CAN+5V` on the 250407 sheets with hierarchical labels and sheet pins (`COLOR_CLOCK_OUT`, `COLOR_CLOCK_IN`, `COLOR_CLK_OUT` and `CAN+5V`), and brought the colour clock and `CAN+5V` up to the root sheet and into the `Power Supply` sheet (`COLOR_CLK_IN`, `CAN+5V`). The output of the 12 V DC input stage (formerly the local label `+12V_In`) now feeds the 5 V buck converter through its `VIN` sheet pin |
| 2026-10-23 | Synchronized the 5 V buck converter to the colour clock: U702 (74AHCT1G08) gates `PHI_COLOR` with the converter's PGOOD output (pulled up to `CAN+5V` by R706), U703 (74HC4040) divides it by 32 (447.4 kHz for NTSC), and C710 couples the result into EN/SYNC, which R705 holds at the input voltage; C709 and C711 decouple U702 and U703 |
| 2026-10-23 | Added a `5V Buck` sub-sheet to the `Power Supply` sheet with an LM61440 synchronous buck converter (U701) that makes 5 V from the 12 V DC input, based on a TI Webench design. Changed to JLC basic parts where available: feedback divider R702 120k and R703 30k (5.00 V), RT resistor R701 33k (about 400 kHz free-running), feedforward C707 15 pF with R704 1k, input capacitors C701 and C702 10 uF 50 V plus C703 and C704 220 nF, and three 22 uF 25 V output capacitors (C708, C712, C713) instead of one 47 uF; inductor L701 is a 15 uH Fangcheng FCH0650-150M. Annotated its parts from 700 |
| 2026-10-23 | Added a footprint for the Fangcheng FCH0650 power inductor (`L_Fangcheng_FCH0650`) to the component library in `hardware/components` |
| 2026-10-23 | Added the component library `hardware/components` (`c64-mini.kicad_sym`, `c64-mini.pretty`) with symbols for the LM61440 buck converter (LM61440AASQRJRRQ1, LM61440AANRJRR) and a footprint for its TI RJR0014A (VQFN-HR-14) package, and registered it in the project's symbol and footprint library tables as `c64-mini` |
| 2026-10-22 | Added a `Power Supply` sheet with a 12 V DC input stage that provides `+12V_In`: DC barrel jack J601 (5.5 x 2.1 mm), resettable fuse F601 (2.5 A hold), reverse polarity protection with two parallel P-channel MOSFETs (Q601, Q602) whose gate is pulled to ground by R601 and clamped by zener diode D601, TVS diode D602 and input capacitors C601 and C602; annotated its parts from 600. Not yet connected to the rest of the design and not yet on the PCB |
| 2026-10-22 | Added an attribution note for the C64 RF Modulator Replacement by mbarszcz-pcb (MIT License) to the `HF Module Replacement` sheet |
| 2026-10-22 | Added a 47 Ohm, 0.5 W series resistor (R501, 1210) between `+9V_UNREG` and the input of the HF module replacement regulator U501, to take part of the voltage drop off the SOT-89 78L05 |
| 2026-10-21 | Laid out the HF module replacement on the PCB in the area of the former RF modulator: parts on F.Cu, a `GND_RF` ground zone on both copper layers with stitching vias, and the solder-mask openings of the old modulator can removed |
| 2026-10-21 | Changed the HF module replacement parts to SMD where possible (0603/0805 resistors and capacitors, C0G for C505 and C506, BC847B in SOT-23, 1N4148W in SOD-123, 78L05 in SOT-89); kept the 3296W trimmers RV501/RV502 and the axial inductor L501 through-hole; added `LCSC Part #` fields to all fitted parts |
| 2026-10-21 | Replaced the upstream luma pull-up selector (two resistors and a jumper) with a fixed 470 Ohm resistor (R512) for the NMOS 6567 VIC-II |
| 2026-10-21 | Removed the RF modulator (M1), its G.PAL/I.PAL jumper (JP1) and its supply capacitors (C15, C16). Connected the HF module replacement to `+9V_UNREG`, the VIC-II luma and chroma signals (after FB15 and FB14) and A/V connector CN5 pins 1 (luma), 4 (composite) and 6 (chroma) through hierarchical labels; annotated its parts from 500 |
| 2026-10-21 | Added the `HF Module Replacement` sheet with a composite and S-Video output circuit derived from the C64 RF Modulator Replacement Rev D by mbarszcz-pcb (MIT License), fixed to the 250407 long board configuration |
| 2026-10-20 | Removed 17 footprints from `KU-14194HB-RevB-KiCad.pretty` that the schematic and PCB no longer use (including `DIP18`, `DIP24`, `DIP28` and `DIP40`) |
| 2026-10-20 | Fitted round-pin DIP sockets for the 19 chips LCSC does not sell (U1-U7, U9-U12, U17-U19, U21-U24, U32) using KiCad's standard `Package_DIP` socket footprints (0.8 mm holes, pads on the original positions), with notes on which chip to insert; moved the labels of U6 and U9-U12 and the reference texts of U21-U24 and U32 clear of the wider socket outlines; moved the sheet 2 note block above the title block |
| 2026-10-20 | Configured the clock circuit for NTSC (Commodore 251138 notes 2, 7 and 8): Y1 14.31818 MHz, C70 16 pF, R52 680 Ohm, R53 910 Ohm; E3 (NTSC link) fitted, E2 (PAL link), C203 and C204 not fitted; added NTSC note blocks (upper right of both sheets) with boxed markers |
| 2026-10-20 | Added `LCSC Part #` fields to the schematic symbols and PCB footprints of all parts with an exact LCSC match |
| 2026-10-20 | Restored the original designators of 14 sub-units on sheet 2 (`RP1A`-`RP1D`, `RP2A`-`RP2D`, `U8B`, `U15A`, `U15B`, `U27B`-`U27D`) that were renamed with a `1` suffix during the reorganization |
| 2026-10-20 | Renamed the project to `c64-mini` and moved both original schematic pages into a `250407` sub-sheet of a new root sheet |
| 2026-10-20 | Imported upstream V1.3 KiCad files unchanged (no modifications) |
