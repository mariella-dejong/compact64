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

The board now runs from a single 12 V DC input (#10, #26). All supply rails
are made from it on the `Power Supply` sheet. The original power brick input
is gone (#37): the power connector CN7, the 9 V AC and `+9V_UNREG`, and the
power switch SW1, so the board has no power switch.

## Compatibility

These changes make the c64-mini behave differently from an original C64.
Check them before using software or hardware that depends on them.

### CIA time-of-day clocks do not run

- **Original C64:** both CIAs (U1 and U2) get a 50 Hz or 60 Hz signal on
  their TOD input (pin 19). It is derived from the 9 V AC of the power brick
  and buffered by gate A of U27.
- **c64-mini:** there is no 9 V AC, so both TOD inputs are tied to ground
  (#37). The time-of-day clocks of both CIAs stand still at the time last
  written to them, and TOD alarms never trigger.
- **Not affected:** the CIA timers, and the KERNAL jiffy clock with BASIC's
  `TI` and `TI$`, which run from timer A of CIA 1.
- **Affected:** software that reads the time-of-day clock, waits for it to
  change or sets a TOD alarm. Such programs show a frozen clock, or can hang
  while they wait. Examples are programs with a real-time clock display and
  programs that use the TOD clock for timing or to tell 50 Hz from 60 Hz
  mains.
- **Possible fix:** a 50 Hz or 60 Hz reference can be added later. Gate A of
  U27 is still in place with pin 1 on +5V; its input pin 2 (now on ground)
  and output pin 3 (now unconnected) can take the new reference and drive
  the TOD inputs again. Replacing the CIAs (#25) can also provide it.

### User port pins 10 and 11 carry no 9 V AC

- **Original C64:** pins 10 and 11 of the user port (CN2) carry the 9 V AC of
  the power brick, one side through fuse F1.
- **c64-mini:** both pins are unconnected, and not grounded (#37).
- **Affected:** devices that take their power from these pins, such as some
  RS-232 interfaces and EPROM programmers. They need their own supply.
  Devices that only use the +5 V on pin 2 and the data lines are not
  affected.

## Power supply

The board is being converted to a single 12 V DC input (#10, #26). The rails
made from it so far are on the `Power Supply` sheet:

| Rail       | Made by                                                     | Rated for | Issue |
| ---------- | ----------------------------------------------------------- | --------- | ----- |
| `+12V_IN`  | Input stage: barrel jack, polyfuse, reverse polarity and TVS protection | 2.5 A (polyfuse hold current) | #10   |
| `+5V`      | LM61440 synchronous buck converter (U701), synchronized to the colour clock | 2 A       | #28   |
| `CAN+5V`   | L78M05 linear regulator (U801) behind four M7 diodes        | 500 mA    | #34   |
| `+12V`     | Charge pump driven by a UCC27517 (U901), followed by an L78M12 linear regulator (U902) | about 150 mA (limit of the charge pump driver) | #36   |
| `CASS_POWER` | TPS562208 synchronous buck converter (U1101), switched on and off by the 6510 | 2 A       | #41   |

The HF module replacement (U501, a 78L05 behind R501) also runs from the 12 V
DC input: the `Power Supply` sheet passes it on through its `12V_INPUT` sheet
pin to the `12V_IN` sheet pin of the `HF Module Replacement` sheet. The
`5V Buck` sheet's output reaches the root sheet through the `5V_OUT` sheet
pin, where it connects to the `+5V` power symbol.

The `Color Clock Divider` sheet supplies the clocks for the switching parts.
U1001 (74AHCT1G08) passes the colour clock (`PHI_COLOR`, 14.31818 MHz for
NTSC) only once the buck converter's PGOOD output is high, and U1002
(74HC4040) divides it: Q4 (/32, 447.4 kHz) synchronizes the 5 V buck
converter and Q5 (/64, 223.7 kHz) clocks the `+12V` charge pump. Because
both are locked to the colour clock, their interference has a fixed relation
to the video timing. The divider runs from `CAN+5V`.

### `CAN+5V` current budget

`CAN+5V` powers the VIC-II, the dot and colour clock circuit (`+Vc`, through
L2 and FB19) and the colour clock divider.

| Load                                        | Typical             | Maximum  | Source                                                    |
| ------------------------------------------- | ------------------- | -------- | --------------------------------------------------------- |
| VIC-II 6567 (U19), Vcc                      | not specified       | 200 mA   | 6567 preliminary datasheet, 3.3.7: ICC, outputs open      |
| 74LS629 (U31), VCC and OSC VCC              | 35 mA               | 55 mA    | TI SDLS186                                                |
| 74LS193 (U30)                               | 19 mA               | 34 mA    | TI SDLS074                                                |
| MC4044 (U32)                                | 17 mA               | 40 mA    | Motorola MC4344/MC4044 datasheet (85 mW typical)          |
| 74LS74A (U29)                               | 4 mA                | 8 mA     | TI SDLS119                                                |
| R16, R53 and R27, Q7 (around the PLL)       | about 3 mA          | about 7 mA | Estimate from the circuit                               |
| 74AHCT1G08 (U1001)                          | about 2.5 mA        | about 3.5 mA | Cpd 18 pF at 14.3 MHz, plus the change in supply current with a 3.4 V input (TI SN74AHCT1G08) |
| 74HC4040 (U1002)                            | about 8.5 mA        | about 8.5 mA | Cpd 88 pF at 14.3 MHz, plus charging C710 (1 nF) at 447 kHz (TI SN74HC4040) |
| R706 (PGOOD pull-up)                        | 0.05 mA             | 0.05 mA  | 5 V / 100 kOhm                                            |
| **Total**                                   | **about 90 mA plus the VIC-II** | **about 360 mA** |                                       |

The VIC-II datasheet only gives a maximum. Measure the real `CAN+5V` current
on the first prototype.

Datasheets:
[6567 VIC-II preliminary](https://archive.org/details/mos_6567_vic_ii_preliminary),
[SN74LS629](https://www.ti.com/lit/ds/symlink/sn74ls629.pdf),
[SN74LS193](https://www.ti.com/lit/ds/symlink/sn74ls193.pdf),
[MC4344/MC4044](https://www.radioradar.net/en/datasheets_search/M/C/4/MC4344_MotorolaSemiconductor.pdf.html),
[SN74LS74A](https://www.ti.com/lit/ds/symlink/sn74ls74a.pdf),
[SN74AHCT1G08](https://www.ti.com/lit/ds/symlink/sn74ahct1g08.pdf),
[SN74HC4040](https://www.ti.com/lit/ds/symlink/sn74hc4040.pdf),
[L78M](https://www.st.com/resource/en/datasheet/l78m.pdf).

With this budget, the L78M05 (500 mA) runs at up to about 70 % of its rating.
The four M7 diodes in front of it drop about 3.2 V to 3.6 V and take part of
the heat:

| Load                       | Each diode | U801 at 12 V in | U801 at 12.6 V in |
| -------------------------- | ---------- | --------------- | ----------------- |
| About 200 mA               | about 0.16 W | about 0.75 W  | about 0.9 W       |
| About 360 mA (worst case)  | about 0.3 W  | about 1.2 W   | about 1.4 W       |

At the lowest input voltage (about 11.2 V after the input stage) the regulator
still gets the 2 V headroom it needs. U801's tab is ground: connect it to a
large ground pour on both layers with vias, so it needs no heatsink.

### `+12V`

`+12V` feeds VDD of the VIC-II (through FB12) and the SID (through FB11), and
the audio output buffer Q8. The `12V Stabilized Rail` sheet makes it from the
12 V DC input:

- **Charge pump:** while U901 (UCC27517 gate driver) holds the bottom of the
  flying capacitor C903 at ground, C903 charges from `+5V` through D901. When
  U901 lifts it to the 12 V input, D902 passes the charge on to the reservoir
  capacitor C904 (`VPUMP`, about 15 V). U901 runs from the 12 V input through
  FB901 (decoupled by C901 and C902) and switches at 223.7 kHz (`PUMP_CLK`).
- **Regulator:** U902 (L78M12) regulates `VPUMP` to 12 V. C905 decouples its
  input, C906 and C907 its output.
- **Pre-charge:** D903 charges `VPUMP` straight from the 12 V input. Before
  the pump runs, `+12V` therefore already sits at about 10 V, and the first
  pump cycles do not have to fill an empty reservoir. Once the pump runs, D903
  is reverse biased.

The pump clock only runs once the buck converter's PGOOD is high, so `+12V`
reaches 12 V a few ms after `+5V`. Neither the 6567 nor the 6581 datasheet
specifies a power-up order between VDD and Vcc, and the power-on reset (U20B,
about 0.5 s) keeps the CPU, the CIAs and the SID in reset well beyond that.

The board supports the 6581 SID only. The 8580 is not supported: it needs
9 V on VDD instead of 12 V, and the rail has no 9 V option.

#### `+12V` current budget

| Load                         | Typical                        | Maximum      | Source                                                                    |
| ---------------------------- | ------------------------------ | ------------ | ------------------------------------------------------------------------- |
| VIC-II 6567 (U19), VDD       | not specified                  | 50 mA        | 6567 preliminary datasheet, 3.3.7: IDD with the dot clock low             |
| SID 6581 (U18), VDD          | 25 mA                          | 40 mA        | 6581 preliminary datasheet (October 1982), page 8                         |
| Q8 (audio output buffer)     | about 5.5 mA                   | about 6 mA   | Emitter follower into R38 (1 kOhm) with its emitter at about 5.3 V        |
| **Total**                    | **about 31 mA plus the VIC-II** | **about 96 mA** |                                                                       |

Datasheets:
[6567 VIC-II preliminary](https://archive.org/details/mos_6567_vic_ii_preliminary),
[6581 SID preliminary](https://archive.org/details/mos_6581_sid_preliminary_october_1982),
[UCC27517](https://www.ti.com/lit/ds/symlink/ucc27517.pdf),
[L78M](https://www.st.com/resource/en/datasheet/l78m.pdf).

#### `+12V` headroom and dissipation

Most of the pump's loss is in U901's pull-up resistance (5 Ohm typical,
7.5 Ohm maximum), which carries about twice the load current. Estimates at
the maximum load of 96 mA:

| Condition                                         | `VPUMP`     | `+12V`                     |
| ------------------------------------------------- | ----------- | -------------------------- |
| 12 V input, typical driver resistance             | about 15.0 V | 12.0 V                    |
| 11.2 V input, maximum driver resistance           | about 13.6 V | about 11.9 V (in dropout) |

The L78M12 is only specified to regulate from 14.5 V input. In the worst-case
corner it drops out, but its output stays within the 11.4 V to 12.6 V VDD
range of the VIC-II and the SID. If the prototype shows too little headroom,
a low-dropout regulator such as the LM2940-12 can take U902's place. A lower
pump frequency would not help: below 223.7 kHz the capacitors add to the loss.

U902 dissipates about 0.3 W to 0.35 W and U901 about 0.1 W to 0.16 W. The
ripple on `VPUMP` is about 55 mV at full load. U902's tab is ground, like
U801's: connect it to the ground pour, so it needs no heatsink.

### Cassette motor supply

The `Cassette Power (6.2v)` sheet makes the supply of the Datasette motor
(`CASS_POWER`, through FB2 to pin C of the cassette port CN3) from the 12 V
DC input. It replaces the original linear circuit (a 7.5 V zener and a
Darlington pass transistor), which turned everything above about 6.2 V into
heat in a TO-220 transistor (about 0.9 W at 150 mA from 12 V) and drew about
8 mA from its input while the motor was off.

- **Converter:** U1101 (TPS562208) with L1101 (4.7 uH), C1101 and C1102 at
  the input, C1103 as bootstrap capacitor and C1104 and C1105 (22 uF each) at
  the output, the values TI recommends for this output voltage. R1101 (39k)
  and R1102 (5.6k) set the output to 6.12 V (6.0 V to 6.24 V with the 2 %
  reference tolerance), the closest JLC basic resistor pair to the original
  6.2 V.
- **Fixed frequency:** the TPS562208 always switches at about 580 kHz, also
  at the light load of a running motor. The TPS562201 variant skips pulses at
  light load, which would make its frequency vary. The converter is not
  synchronized to the colour clock; it only runs while the motor turns.
- **Motor control, as on the original board:** the 6510 drives Q1101 (BC847)
  from port bit P5 (`CASS_MOTOR`, U7 pin 24) through R1104 (1k), with R1105
  (10k) pulling its base low. With P5 high, Q1101 pulls the enable input
  (`CASS_EN`) low and the converter is off; it then draws about 0.13 mA,
  mostly through the enable pull-up R1103 (100k). With P5 low, R1103 pulls
  the enable input up and the converter starts with a 1 ms soft start. During
  reset, before the KERNAL sets P5, the motor supply is on, like the
  original.
- **Protection:** the converter has a cycle-by-cycle current limit (2.4 A to
  4 A) and thermal shutdown, so a short on the cassette port no longer
  overloads a transistor.

Commodore gives no motor current. Expect roughly 100 mA to 200 mA while the
motor runs and several hundred mA at start-up. At 1 A the inductor peak
current is about 1.6 A, well below L1101's 3.2 A saturation current. In a
hard short the current limit can exceed that before it trips; a 4 A inductor
of the same size (such as the FXL0420-4R7-M) fits if that margin is wanted.
The loss is estimated at about 0.2 W to 0.3 W at 150 mA. Measure the real
motor current and output voltage with a Datasette on the first prototype.

Datasheets:
[TPS562208](https://www.ti.com/lit/ds/symlink/tps562208.pdf),
[SWPA4030S](https://datasheet.lcsc.com/lcsc/1811081011_Sunlord-SWPA4030S4R7MT_C57269.pdf).

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
| 2026-10-24 | Removed the 9 V AC input and `+9V_UNREG` from sheet 1: power connector CN7, power switch SW1, fuse F1, choke L4, bridge rectifier CR4 and capacitors C19, C20, C21, C95, C98 and C99. User port pins 10 and 11 (CN2) are no longer connected. The board has no power switch |
| 2026-10-24 | Removed the 50/60 Hz time-of-day reference (R5, R100, R37, zener diode CR1 and C200) from sheet 1 and tied the TOD inputs of U1 and U2 (pin 19) to ground. Kept gate A of U27, which carries U27's supply pins, with pin 1 on +5V, pin 2 on ground and pin 3 unconnected, and its decoupling capacitor C46 |
| 2026-10-24 | Fed the HF module replacement from the 12 V DC input instead of `+9V_UNREG`, through a new `12V_INPUT` sheet pin on the `Power Supply` sheet and a `12V_IN` sheet pin on the `HF Module Replacement` sheet; R501 (47 Ohm) is unchanged |
| 2026-10-24 | Moved the `+5V` input capacitors C91 (100 uF) and C92 (100 nF) from sheet 1 to the `5V Buck` sheet as C714 (100 uF SMD electrolytic, 5 x 5.4 mm) and C709 (100 nF, 0402), and moved the `+5V` power symbol from the `Power Supply` sheet to the root sheet, fed through a new `5V_OUT` sheet pin |
| 2026-10-24 | Removed the original cassette motor supply from sheet 1: R2, the 7.5 V zener CR2, C18, the Darlington pair Q1 and Q3 and the motor switch Q2 with R3 and R4. `CASS_MOTOR` (U7 pin 24) now leaves sheet 1, and `CASS_POWER` arrives at FB2, through hierarchical labels and sheet pins via `250407 Design`, the root sheet and the `Power Supply` sheet. `+9V_UNREG` now only feeds the HF module replacement |
| 2026-10-24 | Added a `Cassette Power (6.2v)` sub-sheet to the `Power Supply` sheet that makes the cassette motor supply from the 12 V DC input with a TPS562208 synchronous buck converter (U1101, L1101 4.7 uH, C1101-C1105), set to 6.12 V by R1101 (39k) and R1102 (5.6k). Q1101 (BC847) with R1104 and R1105 switches its enable input from the 6510's P5 like the original Q2; R1103 pulls the enable input up. Annotated its parts from 1100 |
| 2026-10-24 | Removed the original `+12V` regulator VR1 (7812), its charge pump on the 9 V AC (CR5, CR6, C90) and its input and output capacitors (C88, C89, C57, C59) from sheet 1. `+12V` now comes from the `12V Stabilized Rail` sheet through the `+12V_STABILIZED` sheet pin, which the root sheet connects to the `+12V` power symbol. `+9V_UNREG` still feeds the cassette motor supply and the HF module replacement |
| 2026-10-24 | Added a `12V Stabilized Rail` sub-sheet to the `Power Supply` sheet that makes `+12V` from the 12 V DC input: a charge pump with flying capacitor C903, charged from `+5V` through D901 and lifted by gate driver U901 (UCC27517, supplied through FB901 with C901 and C902), D902 into reservoir C904, pre-charge diode D903 from the 12 V input, and an L78M12 linear regulator (U902, DPAK) with C905 at its input and C906 and C907 at its output. D901-D903 are B5819W Schottky diodes. Annotated its parts from 900 |
| 2026-10-24 | Moved the colour clock divider from the `5V Buck` sheet to a new `Color Clock Divider` sub-sheet of the `Power Supply` sheet and annotated its parts from 1000: U702, U703, C709 and C711 became U1001, U1002, C1001 and C1002. Q4 (447.4 kHz) reaches the 5 V buck converter through its new `CLOCK_IN` sheet pin, the converter's PGOOD leaves through its `PGOOD` sheet pin, and Q5 (223.7 kHz) clocks the `+12V` charge pump (`PUMP_CLK`) |
| 2026-10-24 | Added a symbol for the UCC27517 gate driver (`UCC27517DBV`, SOT-23-5) to the component library in `hardware/components` |
| 2026-10-23 | Named the output of the 12 V DC input stage `+12V_IN` with a label on the `Power Supply` sheet and added a PWR_FLAG to it |
| 2026-10-23 | Removed the original `CAN+5V` regulator VR2 (7805) and its output capacitors C102 and C103 from sheet 1. `CAN+5V` now comes from the `CAN+5V Supply` sheet through hierarchical labels and sheet pins; `+9V_UNREG` still feeds the cassette motor supply, the `+12V` charge pump and the HF module replacement |
| 2026-10-23 | Added a `CAN+5V Supply` sub-sheet to the `Power Supply` sheet that makes `CAN+5V` from the 12 V DC input with an L78M05 linear regulator (U801, DPAK). Four M7 diodes in series (D801-D804) drop about 3.2 to 3.6 V in front of the regulator to share its heat; C801 decouples the regulator input, C802 and C803 its output. Annotated its parts from 800 |
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
