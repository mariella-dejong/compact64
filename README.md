# c64-mini

A smaller form-factor Commodore 64 that is built around the **original MOS
6510 CPU, VIC-II and SID chips**.

The project starts from a faithful KiCad replica of the C64 "250407"
motherboard and evolves it step by step: the original chips stay, while the
rest of the board is either kept as-is or replaced with present-day parts and
modern solutions where that makes the machine smaller, easier to build or more
convenient to use.

> **Status:** work in progress. The schematic is being redesigned step by
> step, starting from the 250407 replica. The PCB layout is not kept in sync
> with these changes and reflects an earlier state of the design: the board
> will be laid out again from scratch for the smaller form factor once the
> schematic is complete. Do not use the current PCB files for manufacturing.
> Nothing here has been built or tested yet in its modified form.

## Goals

- Keep the heart of the C64 genuine: the original 6510, VIC-II and SID chips
  run the show.
- Shrink the board to a smaller form factor.
- Make it buildable today: use parts that can actually be sourced, and put
  chips that can only be salvaged from old machines in sockets.
- Replace parts of the design that are obsolete, hard to source or
  inconvenient with modern equivalents.
- Stay open: publish all design files in their native, editable format.

## Roadmap

The work is tracked in the issue tracker. In broad strokes:

**Phase 1 - Hardware: a clean, modern base design**

- Import the reverse-engineered 250407 design (#1)
- Upgrade the design to KiCad 10 (#2)
- Restructure the hierarchical schematic (#3)
- Make schematic and PCB fully consistent (clean ERC and DRC) (#4)
- Add LCSC part numbers for sourceable components, NTSC variants first (#5)
- Use IC sockets for chips that can no longer be sourced new (#6)
- Replace obsolete passive and other non-chip components (#14, #15)

**Phase 2 - Hardware: modernisation**

- Replace the RF modulator with an on-board composite and S-Video circuit
  (#8), and add S-Video and audio outputs (#24, #9)
- Power the board from a single 12 V DC input (#10) and generate the supply
  rails from it (#26), starting with the 5 V buck converter (#28),
  `CAN+5V` (#34), `+12V` (#36) and the cassette motor supply (#41)
- Replace RAM and ROM with a microcontroller-based solution (#11)
- Replace CIA1 and CIA2 with a microcontroller-based solution (#25)
- Replace the keyboard connector with a USB keyboard to keyboard-matrix
  bridge (#12)

**Phase 3 - Firmware and software**

- Wireless (Bluetooth or Wi-Fi) media access, for example an emulated disk
  drive fed by a PC application (#13)
- Firmware for the microcontroller-based subsystems above

## Repository layout

| Path                  | Contents                                                        |
| --------------------- | --------------------------------------------------------------- |
| `hardware/main-board` | KiCad project of the main board (starts as the 250407 replica)  |
| `hardware/components` | KiCad symbols and footprints that KiCad's own libraries lack    |
| `LICENSES/`           | Full texts of all licences used in this repository              |
| `LICENSE.md`          | Which licence applies to which part of the repository           |
| `NOTICE.md`           | Credits, third-party attributions and trademark notices         |

Firmware and software will get their own top-level directories once that work
starts.

## Tools

The hardware is designed in [KiCad](https://www.kicad.org/). The imported
replica was last saved with KiCad 9.0; the design will be moved to KiCad 10
(#2).

## Credits

This project would not exist without the C64 250407 replica by **Bwack** and
the people who helped create it, most notably **Michael K. (Der Alte
Bastler)**, who started the reverse engineering effort. See
[NOTICE.md](NOTICE.md) for the full list of credits and third-party
attributions.

The composite and S-Video output circuit that replaces the RF modulator is
derived from the C64 RF Modulator Replacement by **mbarszcz-pcb**.

- Upstream project: <https://github.com/bwack/C64-250407-Replica-KiCad>
- RF modulator replacement: <https://github.com/mbarszcz-pcb/c64-rf-modulator-replacement>

## Licence

In short:

- **Hardware** design files (`hardware/`) are licensed under the
  [CERN Open Hardware Licence Version 2 - Strongly Reciprocal](LICENSES/CERN-OHL-S-2.0.txt)
  (CERN-OHL-S-2.0).
- **Firmware and software** will be licensed under the
  [Apache License 2.0](LICENSES/Apache-2.0.txt).
- **Documentation** outside `hardware/` is licensed under
  [Creative Commons Attribution 4.0 International](LICENSES/CC-BY-4.0.txt)
  (CC-BY-4.0).

See [LICENSE.md](LICENSE.md) for the details, including how to attribute this
work when you reuse it.

## Disclaimer

This is an independent hobby project. It is not affiliated with, endorsed by
or sponsored by the owners of the Commodore, Commodore 64 or C64 trademarks,
or by MOS Technology or its successors. See [NOTICE.md](NOTICE.md).

The design files are provided as-is, without any warranty. Building
electronics involves risks, including working with mains-powered equipment
and soldering; you build and use anything from this repository at your own
risk.
