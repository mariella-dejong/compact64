# Contributing to Compact 64

Thank you for your interest in the Compact 64. Contributions are welcome:
reviews of design decisions, corrections, sourcing information, tests and
pull requests.

The project is a work in progress. The schematic is being redesigned step by
step, and nothing has been built yet. See the [README](README.md) for the
goals and the status.

## Where help is most useful

- **Reviewing the design:** the [risk register (#113)](https://github.com/mariella-dejong/compact64/issues/113)
  lists the least proven parts of the design. Knowledge of the VIC-II, the
  6526 and the bus timing of the C64 is especially valuable.
- **Open issues:** look for issues that describe a change but have no pull
  request yet.
- **Sourcing:** parts that are out of stock, better alternatives, and
  second sources.
- **Testing:** test benches such as the U1401 bus exerciser (#159), and
  later the prototype.

## Before you start

- **Discuss first.** Open an issue, or comment on an existing one, before
  you start a larger change. That avoids duplicate work and changes that do
  not fit the design.
- **One issue per change.** Each pull request addresses one issue. A larger
  topic gets an umbrella issue with smaller issues under it.
- **Questions** are welcome in [Discussions](https://github.com/mariella-dejong/compact64/discussions).

## Hardware changes

- **KiCad 10.** The project files are saved with KiCad 10.0. Do not save them
  with a newer major version, which older versions cannot open.
- **Schematic only, for now.** The PCB layout is not kept in sync with the
  schematic and will be laid out again from scratch. Do not change the PCB
  file.
- **Libraries.** Use KiCad's standard symbols and footprints where they fit.
  Anything else goes into the project library in `hardware/components`
  (nickname `compact64`), documented in its README.
- **Parts:**
  - Give every fitted part an `LCSC Part #`, and check its stock before you
    choose it.
  - Parts that LCSC does not sell, such as the original MOS chips, go in a
    socket and are noted on the sheet.
- **Documentation:**
  - Describe the change in the README of the board (for the main board,
    `hardware/main-board/README.md`): the circuit, the parts, and what the
    firmware or the prototype has to check.
  - Add a row to its **Modifications** table, newest first. CERN-OHL-S
    requires a dated record of every change to the design.
- **Checks:** compare the netlist with `main` by reference and pin, so that
  only the intended connections change, and run ERC. A new ERC item needs a
  reason in the pull request.

## Pull requests

- **Branch:** name it after the issue, for example `127-power-test-points`.
- **Commit title:** a short sentence in the imperative, followed by
  ` * #<issue>`, for example
  `Add test points for the power rails and ground * #127`.
- **Description:**
  - what changes and why;
  - which checks you ran and their results (netlist, ERC);
  - what is left for later.

  Close the issue with `Closes #<issue>` when the pull request completes it.
- **Images:** for schematic changes, add before and after images of the
  changed area.
- **Merging:** pull requests are reviewed and squash-merged into `main`. Keep
  your branch up to date with `main` and resolve conflicts by rebasing.

## Licensing of contributions

By submitting a contribution, you agree that it is licensed under the licence
of the part of the repository it belongs to (see [LICENSE.md](LICENSE.md)):

- hardware design files under `hardware/`: CERN-OHL-S-2.0;
- firmware and software: Apache-2.0;
- documentation outside `hardware/`: CC-BY-4.0.

You also confirm that you have the right to submit it under that licence.

- **Third-party material:** include it only if its licence is compatible,
  keep its copyright and licence notices, and add it to
  [NOTICE.md](NOTICE.md).
- **Not accepted:** ROM images and other copyrighted firmware, datasheets,
  or material whose licence does not allow redistribution. Link to such
  material instead.

## Conduct

Be respectful and constructive. Critique designs, not people. Everyone here
contributes in their own time.
