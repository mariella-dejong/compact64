# Components

KiCad library with the symbols and footprints of the c64-mini that are not in
the standard KiCad libraries. It uses the current KiCad file formats (KiCad
10): `c64-mini.kicad_sym` for the symbols and `c64-mini.pretty/` for the
footprints.

The main board project (`hardware/main-board`) has the library registered in
its symbol and footprint library tables under the nickname `c64-mini`, with a
path relative to the project (`${KIPRJMOD}/../components/`). Other projects
under `hardware/` can register it the same way.

## Contents

| Symbol              | Footprint                                  | LCSC Part # | Notes                                                    |
| ------------------- | ------------------------------------------ | ----------- | -------------------------------------------------------- |
| `LM61440AASQRJRRQ1` | `Texas_VQFN-HR-14_4x3.5mm_P0.5mm_RJR0014A` | C3038666    | Spread spectrum, turned off while synchronized (EN/SYNC) |
| `LM61440AANRJRR`    | `Texas_VQFN-HR-14_4x3.5mm_P0.5mm_RJR0014A` | C3188034    | No spread spectrum; derived from `LM61440AASQRJRRQ1`     |

### LM61440

3 V to 36 V, 4 A synchronous step-down converter from Texas Instruments
([datasheet](https://www.ti.com/lit/ds/symlink/lm61440.pdf)). The switching
frequency is set with a resistor on RT, or synchronized to an external clock
on EN/SYNC.

The pin layout of the symbol follows the symbol in the TI Webench design, so
a Webench schematic can be copied pin by pin: supply and control pins on the
left, outputs, bootstrap, feedback and grounds on the right.

### RJR0014A footprint

TI VQFN-HR package, 14 pins, 4 mm x 3.5 mm body, 0.5 mm pitch. KiCad has no
footprint for this package. It is drawn from the LM61440 datasheet
(SNVSBD6C):

- Copper pads from the land pattern example (page 55). The L-shaped pads 1,
  5, 9 and 11 are custom pads made of two rectangles; the inner corner radius
  of the TI drawing (R0.12) is left out. The other pads have the 0.05 mm
  corner radius of the drawing.
- Solder mask: non solder mask defined, as TI prefers. The pads use the
  board's default solder mask expansion.
- Solder paste from the stencil design example (page 56). The L-shaped pads
  get a slightly narrower aperture and the SW pad (10) gets two apertures of
  1.1 mm x 0.4 mm. These are separate paste-only pads without a number.
- The pads extend 0.2 mm past the body on all sides, so the silkscreen only
  has a pin 1 marker and no body outline. The body outline is on the
  fabrication layer.
- There is no 3D model.

## Adding parts

- Use the KiCad 10 formats, not the legacy `.lib` format.
- Take footprints from the land pattern in the manufacturer's datasheet, and
  write down the source (document number and page) in the footprint
  description and in this README.
- Fill in the datasheet link, the description and the `LCSC Part #` field,
  the field name the main board uses.
- Check the pin numbers of the symbol against the footprint and the
  datasheet.
