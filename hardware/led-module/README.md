# LED module

KiCad project of the Compact 64 status LED module (#54). The module carries
four 3 mm through-hole LEDs that sit in holes in the front of the case, and
connects to the main board's supervisor with a short cable. Open
`led-module.kicad_pro` in KiCad.

The PCB layout is not done yet.

## LEDs

| Ref | LED | Colour | Part | LCSC |
| --- | --- | ------ | ---- | ---- |
| D1 | PWR | Red | XINGLIGHT XL-304SURD | C2895471 |
| D2 | RDY | White | XINGLIGHT XL-304UWC | C2895459 |
| D3 | ERR | Amber | XINGLIGHT XL-304UYD | C2895473 |
| D4 | ACT | Green | XINGLIGHT XL-304UGD | C2895477 |

All four come from the same 3 mm series (footprint `LED_THT:LED_D3.0mm`),
so they have the same body and height and line up in the case. What the LEDs
mean is described in the main board README, see
[Status LEDs](../main-board/README.md#status-leds).

The module has no resistors. The current-limiting resistors sit on the main
board, next to the supervisor, so that a shorted cable cannot overload a
GPIO or the rail.

## Connector

J1 is a 2×3 pin header with 2.54 mm pitch (C65114), the same as J1303 on the
main board. A cable that keeps the pin numbers, for example a 6-way ribbon
cable with IDC sockets, connects the two pin for pin.

| Pin | Signal | LED |
| --- | ------ | --- |
| 1 | GND | Cathodes of D1 to D4 |
| 2 | `PWR` | D1 (red) |
| 3 | Not connected (spare) | |
| 4 | `RDY` | D2 (white) |
| 5 | `ACT` | D4 (green) |
| 6 | `ERR` | D3 (amber) |

## Mounting

H1 to H4 are 2.5 mm mounting holes (`MountingHole_2.5mm`) to screw the
module behind the front of the case.

## Licence

Copyright (c) 2026 Mariella de Jong and contributors.

Licensed under the
[CERN Open Hardware Licence Version 2 - Strongly Reciprocal](../../LICENSES/CERN-OHL-S-2.0.txt)
(CERN-OHL-S-2.0). See [LICENSE.md](../../LICENSE.md) for how the licences
apply to this repository.

## Modifications

| Date       | Description |
| ---------- | ----------- |
| 2026-11-01 | New design: four status LEDs (D1 to D4), the cable connector J1 and four mounting holes (#54) |
