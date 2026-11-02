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

The SID is now an 8580 on a 9 V supply (#73), and the VIC-II an 8562 on 5 V
(#74), so the board no longer needs 12 V anywhere and the `+12V` rail is gone.

The VIC-II's colour and dot clocks now come from a clock synthesizer on the
`System Clocks` sheet (#76). It replaces the PLL of the original board, whose
MC4044 can no longer be bought, and the colour clock divider that
synchronized the buck converters.

What is left of the original 250407 circuit, the 6510 with the cassette
port and the expansion port, is now on one flat sheet, `CPU, Cassette and Expansion Port` (#84, #107), with SMD passives and
reference numbers in the 200 range, see
[CPU, cassette and expansion port](#cpu-cassette-and-expansion-port). Its
power-on reset timer is gone: the supervisor now resets the system at
start-up (#89), see [Soft reset](#soft-reset).

The PLA (U17), the 74LS139 (U15), the 7408 (U27) and the `~AEC` inverter of
the original board are replaced by a CPLD on the `PLA Replacement` sheet
(#83), see [PLA replacement](#pla-replacement).

The supervisor has a micro SD card slot (#95), from which it will load disk
and tape images (#13), see [SD card slot](#sd-card-slot).

The status LEDs (power, ready, activity, error) sit on a separate LED module
that connects to the supervisor with a short cable, so that they can be
mounted in the case (#54), see [Status LEDs](#status-leds). The original
power LED connector is gone.

## Compatibility

These changes make the c64-mini behave differently from an original C64.
Check them before using software or hardware that depends on them.

### CIA time-of-day clocks run from a crystal

- **Original C64:** both CIAs (U1 and U2) get a 50 Hz or 60 Hz signal on
  their TOD input (pin 19). It is derived from the 9 V AC of the power brick
  and buffered by gate A of U27, so the clocks follow the mains frequency.
- **c64-mini:** there is no 9 V AC (#37). Both CIAs are replaced by
  microcontrollers (#25, #68) that make the 50 Hz or 60 Hz reference from
  their own crystal, as selected by bit 7 of CRA (see
  [CIA 1, keyboard and joysticks](#cia-1-keyboard-and-joysticks) and
  [CIA 2, serial port and user port](#cia-2-serial-port-and-user-port)).
  U27 is gone as well (#83).
- **Not affected:** software that uses the time-of-day clocks, the TOD
  alarms or the CIA timers, provided the firmware implements them.
- **Affected:** only software that expects the TOD clocks to follow the
  mains frequency exactly. The crystal reference is more stable than the
  mains, so a TOD clock can drift a little differently from an original C64.

### User port pins 10 and 11 carry no 9 V AC

- **Original C64:** pins 10 and 11 of the user port (CN2) carry the 9 V AC of
  the power brick, one side through fuse F1.
- **c64-mini:** both pins are unconnected, and not grounded (#37). The user
  port is now J1602 on the `CIA2, Serial Port and User Port` sheet (#68).
- **Affected:** devices that take their power from these pins, such as some
  RS-232 interfaces and EPROM programmers. They need their own supply.
  Devices that only use the +5 V on pin 2 and the data lines are not
  affected.

### No 8-pin DIN A/V connector

- **Original C64:** the A/V connector (CN5) is an 8-pin DIN socket with luma
  (pin 1), ground (pin 2), audio out (pin 3), composite video (pin 4), audio
  in (pin 5) and chroma (pin 6).
- **c64-mini:** CN5 is removed (#24). The same signals are on a 4-pin
  mini-DIN S-Video socket (J501) and three RCA jacks (J502), see
  [Audio and video connectors](#audio-and-video-connectors).
- **Affected:** C64 A/V cables with a DIN plug, such as DIN to RCA, DIN to
  S-Video and DIN to SCART cables. Use standard S-Video and RCA cables
  instead.

### RAM and ROM are served by a microcontroller

- **Original C64:** eight 4164 DRAMs, the BASIC, KERNAL and character ROMs,
  and a 2114 colour RAM.
- **c64-mini:** a microcontroller (U1401) serves all of them from its own
  memory (#11), see [RAM and ROM replacement](#ram-and-rom-replacement). It
  loads the ROM images from an EEPROM. The ROM images are not part of this
  project.
- **Affected:**
  - The board does not start without the controller's firmware and ROM
    images.
  - ROM upgrades that replace a chip, such as a replacement KERNAL chip,
    cannot be fitted. Write a different image to the EEPROM instead.
  - The RAM contents at power-up are set by the firmware, not by the DRAMs'
    power-up pattern. The same goes for other details of DRAM behaviour,
    such as what ends up in RAM when the 6510 writes to $00 or $01.
- **Not affected:** bank switching, cartridges and the VIC-II bank
  selection. The CPLD that replaces the PLA (U301) decodes them as before.

### CIA 1 is emulated by a microcontroller, and the keyboard is USB

- **Original C64:** a 6526 CIA (U1) at $DC00 scans the keyboard matrix on
  connector CN1 and reads the two control ports (CN8, CN9).
- **c64-mini:** a microcontroller (U1501) takes the place of CIA 1 (#25),
  see [CIA 1, keyboard and joysticks](#cia-1-keyboard-and-joysticks). The
  keyboard is a USB keyboard on a USB-A socket (J1501), and the firmware
  presents it to the 6510 as the C64 keyboard matrix. RESTORE is a key on
  the USB keyboard chosen by the firmware. The control ports are new DE-9
  sockets (J1502, J1503) with the original pinout.
- **Affected:**
  - The board does not start without the controller's firmware.
  - An original C64 keyboard, or anything else made for the 20-pin keyboard
    connector, cannot be connected.
  - How closely the 6526 is matched (timers, interrupts, the serial shift
    register, keyboard ghosting) depends on the firmware. Programs that rely
    on exact CIA timing or on unusual keyboard matrix tricks need firmware
    that reproduces them.
  - The digital control port lines now have 10k pull-ups to +5V and a
    100 Ohm series resistor, instead of the 6526's internal pull-ups. The
    levels stay the same: a line idles at 5 V and a pressed switch pulls it
    to about 0.05 V.
- **Not affected:** paddles and the POT lines (the 4066 that switches them,
  now U1702, works as before), the light
  pen input of the VIC-II, cassette read and the serial bus SRQ on `~FLAG`,
  and `CNT` and `SP` on the user port.

### CIA 2 is emulated by a microcontroller

- **Original C64:** a 6526 CIA (U2) at $DD00 selects the VIC-II bank, drives
  and reads the serial bus through U8 (7406), and serves the user port
  (CN2).
- **c64-mini:** a microcontroller (U1601) takes the place of CIA 2 (#68), see
  [CIA 2, serial port and user port](#cia-2-serial-port-and-user-port). The
  serial port (J1601) and the user port (J1602) keep their original pinout.
- **Affected:**
  - The board does not start without the controller's firmware.
  - How closely the 6526 is matched (timers, NMI timing, the serial shift
    register, `~PC` and `~FLAG`) depends on the firmware. Fast loaders,
    RS-232 on the user port and demos that switch the VIC-II bank at exact
    cycles need firmware that reproduces the 6526 cycle by cycle.
  - The serial bus driver U1602 (SN74LVC06A) sinks up to 24 mA, the original
    7406 up to 40 mA. Every device on the bus adds its own pull-up of about
    1k (5 mA). The C64 and three such devices stay within the rating; more
    devices with 1k pull-ups may pull the low level up too far.
  - The user port lines have 4.7k pull-ups to +5V, a 100 Ohm series
    resistor and a TVS diode instead of the 6526's internal pull-ups. A line
    still idles at 5 V. An output that is set high is released to the
    pull-up instead of being driven, so its rising edge is a little slower
    (about 0.5 us with 100 pF of cable).
- **Not affected:** the VIC-II bank selection after reset (bank 0, also
  before the firmware runs), the serial bus pinout, the user port pinout
  except pins 10 and 11 (see above), and `CNT` and `SP` of CIA 1 on the user
  port.

### The CPU is an 8500

- **Original C64:** the 250407 board has an NMOS 6510 CPU (U7).
- **c64-mini:** the CPU socket (U201) takes the HMOS-II 8500 of the later
  C64C boards (#75), see
  [CPU, cassette and expansion port](#cpu-cassette-and-expansion-port). The
  8500 is the 6510 made in the newer process: same pinout, same 5 V supply,
  same bus timing and the same processor port, so the circuit is the same
  for both. It also runs cooler. A 6510 works in the socket as well. This
  README calls the CPU the 6510 when it describes its pins and bus cycles.
- **Affected:**
  - The unused bits 6 and 7 of the processor port ($01) keep a written value
    for a while after the port is switched to input, until the charge leaks
    away. This takes noticeably longer on an 8500 than on a 6510 (VICE
    assumes about 1.5 s against about 0.35 s), and on both chips it depends
    strongly on the chip and the temperature. A few copy protections and
    test programs measure it.
  - The unstable undocumented opcodes (such as ANE/XAA and LXA) use a
    "magic constant" that differs between chips, including between 6510 and
    8500 chips, and with temperature. Programs that rely on them may behave
    differently, as they also do between two original C64s.
  - Original 8500 and 6510 chips are only available second-hand.
- **Not affected:** the documented instruction set, the bus and interrupt
  timing, the processor port bits 0 to 5 (memory configuration and the
  cassette lines) and their pull-ups.
- **Not the 8502:** the 8502 is the C128's CPU and is not a drop-in
  replacement.

### The SID is an 8580

- **Original C64:** the 250407 board has a 6581 SID (U18) with 12 V on VDD
  and 470 pF filter capacitors.
- **c64-mini:** the SID socket (U1701) takes the 8580 of the later C64
  boards, with 9 V on VDD and 22 nF filter capacitors (#73), see
  [SID and audio output](#sid-and-audio-output).
- **Affected:**
  - Music sounds different. The 8580's filter is cleaner and more linear,
    it has less background noise and its combined waveforms differ. Music
    written for the 6581, which is most classic SID music, can sound
    noticeably different, especially where it uses the filter.
  - Samples played by writing the volume register ($D418) are almost silent
    on an 8580. A resistor from EXT IN to ground can bring them back; the
    board does not have one.
  - A 6581 must not be fitted: it would get 9 V instead of 12 V on VDD and
    the wrong filter capacitors.
- **Not affected:** the SID's registers, the paddles and the audio
  connectors. Modern SID replacements usually accept 9 V on VDD; check the
  specific product.

### The VIC-II is an 8562

- **Original C64:** the 250407 board has an NMOS 6567 VIC-II (U19) with 12 V
  on VDD (pin 13) and a 470 Ohm luma pull-up.
- **c64-mini:** the VIC-II socket (U504) takes the HMOS-II 8562 of the later
  NTSC C64 boards, with 5 V on pin 13 and a 1 kOhm luma pull-up (#74), see
  [VIC-II](#vic-ii).
- **Affected:**
  - Colours and luma levels differ slightly from the 6567.
  - The 8562 shows "grey dots" when some colour registers are written at
    certain cycles, which a few demos trigger.
  - A 6567 does not work in this socket: it needs 12 V on pin 13.
- **Not affected:** the video timing (65 cycles per line and 263 lines, the
  same as the 6567R8), the registers, sprites, raster interrupts and the
  light pen.

### The video clocks come from a clock synthesizer

- **Original C64:** a PLL on the 250407 board (Y1, U29 to U32) makes the
  colour clock from a 14.31818 MHz crystal and locks the dot clock to it.
  PAL or NTSC is set with the crystal, some component values and a jumper.
- **c64-mini:** an MS5351M clock synthesizer (Y1202) makes both clocks from
  one PLL and a 25 MHz crystal (#76), see [System clocks](#system-clocks).
  The supervisor programs it over I2C, so PAL or NTSC becomes a firmware
  setting; a PAL board still needs a PAL VIC-II.
- **Affected:**
  - Without supervisor firmware the VIC-II gets no clocks. The board already
    needs firmware for RAM, ROM and both CIAs.
- **Not affected:** the video timing and the 4/7 ratio between the dot clock
  and the colour clock, which the synthesizer makes exactly. The colour
  carrier is as accurate as the 25 MHz crystal (10 ppm).

### The PLA is a CPLD

- **Original C64:** a 906114-01 PLA (U17) decodes the memory map, a 74LS139
  (U15) the I/O area, and a 7408 (U27) and a 7406 gate (U8) make `RDY`,
  `CAEC`, `~COLORAM` and `~AEC`.
- **c64-mini:** one XC9572XL CPLD (U301) does all of this (#83), with logic
  written from the PLA's documented equations (#93), see
  [PLA replacement](#pla-replacement). The supervisor checks its design at
  start-up and programs it when needed (#86).
- **Affected:**
  - The board does not start without the supervisor firmware, which also
    enables the CPLD's outputs.
  - The chip selects come out about 10 ns after the inputs change, earlier
    than from the PLA and the 74LS139. Cartridges that are sensitive to PLA
    timing need testing on the prototype.
  - The chip selects on the expansion port (`~ROML`, `~ROMH`, `~I/O1`,
    `~I/O2`) are 3.3 V signals, at TTL levels like the original ones.
- **Not affected:** the memory map, bank switching through $01, the
  cartridge modes set by `~GAME` and `~EXROM` (including Ultimax), the I/O
  map and DMA through `~DMA`, provided the logic matches the PLA (#93).

## Power supply

The board is being converted to a single 12 V DC input (#10, #26). The rails
made from it so far are on the `Power Supply` sheet:

| Rail       | Made by                                                     | Rated for | Issue |
| ---------- | ----------------------------------------------------------- | --------- | ----- |
| `12V_PROT` | Input stage: barrel jack, polyfuse, reverse polarity and TVS protection | 2.5 A (polyfuse hold current) | #10   |
| `12V_MAIN` | Power switch (Q606, Q607), set by the power switch on the case; feeds `+3V3` and the input switch | 2.5 A | #109 |
| `12V_IN_SW` | Input switch (Q604, Q605), turned on by the supervisor; feeds every rail below except `+3V3` | 2.5 A | #47   |
| `+5V`      | LM61440 synchronous buck converter (U701), synchronized to the clock synthesizer | 2 A       | #28   |
| `CAN+5V`   | L78M05 linear regulator (U801) behind four M7 diodes        | 500 mA    | #34   |
| `+9V`      | CJ78M09 linear regulator (U902) from the switched 12 V input | 500 mA (regulator) | #36, #73 |
| `CASS_POWER` | TPS562208 synchronous buck converter (U1101), switched on and off by the 6510 | 2 A       | #41   |
| `+3V3`     | LM61440 synchronous buck converter (U1001), synchronized to the clock synthesizer; backed up from `+5V` by U1002 (#110) | 1 A budget (converter rated 4 A) | #44, #110 |

The HF module replacement (U501, a 78L05 behind R501) also runs from the
switched 12 V input: the `Power Supply` sheet passes `12V_IN_SW` on through a
sheet pin of the same name to the `VIC-II, HF Module Replacement and AV out`
sheet. The `5V Buck` sheet's output reaches the root sheet through the `5V_OUT` sheet
pin, where it connects to the `+5V` power symbol.

The `System Clocks` sheet supplies the sync clock for the switching parts,
see [System clocks](#system-clocks). U1201 (74AHCT1G08) passes output CLK2
of the clock synthesizer, about 445 kHz, only once `PGOOD` is high. It
reaches the 5 V and 3.3 V buck converters through the `SYNC_CLK_IN` sheet pin
of the `Power Supply` sheet. CLK2 comes from the same PLL as the colour and
dot clocks, so the converters' interference has a fixed relation to the
video timing. U1201 runs from `CAN+5V`.

`PGOOD` combines the open-drain PGOOD outputs of both buck converters (U701
and U1001) with the pull-ups R706 and R1003 (200k each) on `CAN+5V`: a
wired-AND. It leaves the `Power Supply` sheet through the `PGOOD` sheet pin.
It goes high once both outputs are in regulation, after the 1.5 ms to 2.5 ms
PGOOD delay. The supervisor reads it on GPIO3 through R1330 (#49), see [Chip
and rail checks](#chip-and-rail-checks). If either rail drops out of
regulation, the sync clock stops and both converters run unsynchronized at
about 400 kHz, set by R701 and R1002, until `PGOOD` returns.

### 12 V path

The 12 V input passes three stages, and each stage has its own net name:

```
J601 ── F601 ── Q601/Q602 ── 12V_PROT ── Q606/Q607 ── 12V_MAIN ── Q604/Q605 ── 12V_IN_SW
 jack   fuse    reverse         TVS D602   power switch   U1001 (+3V3)  input switch  all other rails
                polarity        C605       (case switch)                (supervisor)
```

| Net | Connected |
| --- | --------- |
| `12V_PROT` | After the reverse polarity protection: TVS D602, C605 (220 nF), the zener D601 of Q601/Q602, the power switch Q606/Q607 with R606 and C604 |
| `12V_MAIN` | After the power switch: C601, C602, 3V3 buck converter U1001 with its input capacitors and EN pull-up R1001, the input switch Q604/Q605 with R605 and C603 |
| `12V_IN_SW` | After the input switch: 5V buck converter U701 with R705, cassette buck converter U1101 with R1103, `CAN+5V` (D801 to U801), `+9V` (FB901 to U902), HF module replacement (R501) |

The converter sheets call their input `VIN`. That hierarchical label always
means "the input of this converter" and connects to `12V_MAIN` or
`12V_IN_SW` on the `Power Supply` sheet.

### Power switch

The power switch (#109) turns the whole board on and off, including
`+3V3` and the supervisor. A small switch in the case (on J602) steers a
P-channel MOSFET pair, so the switch itself only carries microamps.

```
12V_PROT ──┬───────────── Q606/Q607 (2x AO3401A, source on 12V_PROT) ── 12V_MAIN
           │                   │ gate (PWR_SW_GATE)
           ├── R606 100k ──────┤
           └── C604 47 nF ─────┤
                               └── R607 100k ── J602 pin 2 ── case switch ── J602 pin 1 ── GND
```

- **Off (switch open):** R606 holds the gate at the source, so Q606 and
  Q607 are off. They are mounted the other way round from Q601/Q602
  (source on the input side), so their body diodes block, and the board
  draws nothing.
- **On (switch closed):** R606 and R607 divide the gate to about half the
  input voltage: about -6 V gate-source at 12 V, and about -10.75 V at the
  21.5 V clamping voltage of D602, within the AO3401A's +/-12 V limit. Two
  in parallel carry the full 2.5 A at about 0.17 W, like the input switch.
- **Why a second pair:** Q601/Q602 (reverse polarity protection) cannot be
  reused. Their body diodes conduct in the normal direction, so the board
  would keep running through them while they are off.
- **C604 (47 nF):** slows the switch-on (time constant about 2.4 ms), so
  that C601, C602 and the converters' input capacitors charge gently, and
  keeps the gate at the source when the adapter is plugged in with the
  switch off.
- **ESD:** the switch sits in the case on a cable. R607 is on the gate side
  of J602, so a discharge into the cable reaches the gate only through
  100k, where C604 absorbs it. D602 and C605 sit before the switch, so the
  input stays protected while it is off.
- **Power-off order:** switching off looks exactly like unplugging the
  adapter. `+3V3` is backed up from `+5V` (#110), so it never drops while
  `+5V` is still up, see [Backup from `+5V`](#backup-from-5v-110).
- **Consequences:** with the switch off, the supervisor is off too, so the
  SD card cannot be reached over USB, and a firmware update cannot delay
  the switch-off; the firmware tolerates a sudden power loss (#111).
- **Reverse polarity:** Q601/Q602 still block a reversed input, and Q606
  and Q607 stay off.

| Ref | Part | LCSC | Package |
| --- | ---- | ---- | ------- |
| Q606, Q607 | AO3401A | C15127 | SOT-23 |
| R606, R607 | 100k | C25741 | 0402 |
| C604 | 47 nF | C1622 | 0603 |
| C605 | 220 nF | C21120 | 0603 |
| J602 | 1x2 pin header, 2.54 mm, for the case switch | C32713268 | THT |

The case switch itself is not on the board. Any small latching switch
works, for example a KCD11 mini rocker switch that snaps into a 10 x 15 mm
cut-out.

### Input switch

The supervisor (U1301) decides when the system may start (#47). It does this
with one switch in the 12 V input: every rail except `+3V3` runs from the
switched input `12V_IN_SW`, so a single signal turns them all on or off.
`+3V3` stays on `12V_MAIN`, before the input switch, because it powers the
supervisor.

**Circuit:**

- Q604 and Q605 (AO3401A, P-channel) in parallel, sources on `12V_MAIN`,
  drains on `12V_IN_SW`. Two in parallel halve the on-resistance to about
  28 mOhm, so they dissipate about 0.17 W at 2.5 A.
- R605 (100k) pulls their gates to the source, so they are off by default.
- Q603 (AO3400A, N-channel) pulls the gates down through R604 (100k). R604
  and R605 form a divider, so the gates go to about half the input voltage
  instead of to ground: about -6 V gate-source at 12 V, and about -10.75 V
  at the 21.5 V clamping voltage of D602, within the AO3401A's +/-12 V
  limit.
- Q603's gate is driven by `POWER_ON` through R602 (10k), with R603 (200k)
  to ground.
- C603 (47 nF) between the gates and `12V_MAIN` does two things. It keeps
  the gates at the source voltage when the adapter is plugged in, so the
  MOSFETs' gate-drain capacitance cannot turn them on briefly. And it slows
  down switching: the gates move with a time constant of about 2.4 ms when
  turning on and about 4.8 ms when turning off, which limits the inrush
  current into the input capacitors of the rails to well under 1 A.
- A PWR_FLAG on `12V_IN_SW` tells ERC that the MOSFETs drive this net.

**Operation** (12 V input):

| State | `POWER_ON` | Q603 gate | Q604/Q605 gate | Q604/Q605 gate-source | `12V_IN_SW` |
| ----- | ---------- | --------- | -------------- | --------------------- | ----------- |
| Off   | 0 V or not driven | 0 V | 12 V | 0 V | off |
| On    | 3.3 V      | 3.14 V    | 6 V            | -6 V                  | about 12 V  |

Q603 is fully on at 3.14 V (threshold at most 1.45 V); Q604 and Q605 are
fully on at -6 V (threshold at most -1.3 V). Q603 draws 60 uA through R604
and R605, and `POWER_ON` supplies 16 uA.

**Fail-safe default:** the rails stay off unless the supervisor's firmware
drives `POWER_ON` high.

- **Supervisor unpowered:** R603 holds Q603's gate at ground.
- **Supervisor in reset or booting:** RP2350 GPIOs come out of reset with
  the output disabled and the internal pull-down on. The boot ROM does not
  use GPIO2, whether it boots from flash or starts the USB bootloader.
- **Blank firmware:** the USB bootloader leaves GPIO2 alone, so the rails
  stay off. A board without supervisor firmware does not start.
- **Reset while running:** pressing RESET or a watchdog reset returns GPIO2
  to its reset state, which turns the rails off.

**Back-feeding:** while `12V_IN_SW` is off, only `+3V3`, the supervisor and
the RAM and ROM controller (U1401 to U1403) are powered.

- `PGOOD` is pulled up to `CAN+5V`, which is off, so it does not feed the
  unpowered gate U1201 on the `System Clocks` sheet.
- The sync clock reaches U1001's EN/SYNC through C1005, which blocks the DC
  on that pin from the unpowered gate U1201.
- The clock synthesizer (Y1202) runs from `+3V3` and stays powered. Its
  outputs stay disabled (low) until the supervisor enables them, and the
  74AHCT inputs of U1201 to U1203 tolerate 3.3 V while `CAN+5V` is off, see
  [System clocks](#system-clocks).
- U902 runs from `12V_IN_SW`, so `+9V` stays off as well.
- The supervisor's only connection to the legacy chips is `~RESET` (#70).
  Q1301 can only pull it low, and GPIO1 reads it through R1315, so neither
  feeds the unpowered chips, see [Soft reset](#soft-reset). Later issues that add more
  connections must keep them from feeding unpowered chips as well.
- The RAM and ROM controller connects to the C64 buses. It keeps all its bus
  pins as inputs while the rails are off, see
  [Power and back-feeding](#power-and-back-feeding).

**Side effects:**

- With the rails off, `PGOOD` is low and `+3V3` runs unsynchronized at about
  400 kHz. It locks to the sync clock once the other rails are up and the
  supervisor has enabled the synthesizer's outputs.
- The cassette motor supply U1101 is on the switched input, so it stays off
  until the supervisor starts the system. After that the 6510 controls it
  through P5, as before.
- All rails start together; there is no start-up order between them.

### `CAN+5V` current budget

`CAN+5V` powers the VIC-II (VCC and, since #74, VDD) and the three 5 V
gates on the `System Clocks` sheet (#76): the clock buffers U1202 and U1203
and the sync gate U1201.

| Load                                        | Typical             | Maximum  | Source                                                    |
| ------------------------------------------- | ------------------- | -------- | --------------------------------------------------------- |
| VIC-II 8562 (U504), VCC (pin 40)            | not specified       | 200 mA   | 6567 figure (preliminary datasheet, 3.3.7: ICC, outputs open), used as an upper bound |
| VIC-II 8562 (U504), VDD (pin 13)            | not specified       | 50 mA    | 6567 figure (3.3.7: IDD), used as an upper bound          |
| 74AHCT1G125 (U1202), colour clock           | about 2 mA          | about 3 mA | Cpd 14 pF plus about 15 pF load at 14.3 MHz, plus the change in supply current (1.35 mA with a 3.4 V input) for the half of the time the input is high (TI SN74AHCT1G125) |
| 74AHCT1G125 (U1203), dot clock              | about 2.6 mA        | about 3.5 mA | Cpd 14 pF plus about 50 pF load (C518, the VIC-II and the expansion port) at 8.2 MHz, plus the same change in supply current |
| 74AHCT1G08 (U1201), sync clock              | about 4.5 mA        | about 5.3 mA | Charging C710 and C1005 (1 nF each) at about 445 kHz, plus the same change in supply current (TI SN74AHCT1G08) |
| R706 and R1003 (PGOOD pull-ups)             | 0.05 mA             | 0.05 mA  | 5 V / 100 kOhm (two 200k in parallel)                     |
| R1331 and R1332 (rail check divider, #49)   | 0.34 mA             | 0.34 mA  | 5 V / 14.7 kOhm                                           |
| **Total**                                   | **about 9 mA plus the VIC-II** | **about 262 mA** |                                       |

No 8562 datasheet figures were found, so the 6567's maximum currents are used
as an upper bound; the HMOS-II 8562 draws considerably less in practice.
Measure the real `CAN+5V` current on the first prototype.

Datasheets:
[6567 VIC-II preliminary](https://archive.org/details/mos_6567_vic_ii_preliminary),
[SN74AHCT1G125](https://www.ti.com/lit/ds/symlink/sn74ahct1g125.pdf),
[SN74AHCT1G08](https://www.ti.com/lit/ds/symlink/sn74ahct1g08.pdf),
[L78M](https://www.st.com/resource/en/datasheet/l78m.pdf).

With this budget, the L78M05 (500 mA) runs at up to about 52 % of its rating.
The four M7 diodes in front of it drop about 3.2 V to 3.6 V and take part of
the heat:

| Load                       | Each diode | U801 at 12 V in | U801 at 12.6 V in |
| -------------------------- | ---------- | --------------- | ----------------- |
| About 200 mA               | about 0.16 W | about 0.75 W  | about 0.9 W       |
| About 260 mA (worst case)  | about 0.22 W | about 0.9 W   | about 1.1 W       |

At the lowest input voltage (about 11.2 V after the input stage) the regulator
still gets the 2 V headroom it needs. U801's tab is ground: connect it to a
large ground pour on both layers with vias, so it needs no heatsink.

### `+9V`

`+9V` feeds VDD of the SID (U1701, through FB1701) and the collector of the
audio output buffer Q1701, see [SID and audio output](#sid-and-audio-output).
The `9V Stabilized Rail` sheet makes it from the switched 12 V input (#73),
and the `+9V_STABILIZED` sheet pin brings it to the `+9V` power symbol on the
root sheet:

- **Input:** FB901 (100 Ohm at 100 MHz) from `12V_IN_SW` to `9V_IN`, with
  C904 (22 uF 25 V X5R, still about 10 uF at 12 V) and C905 (1 uF).
- **Regulator:** U902 (CJ78M09, 9 V, 500 mA, TO-252). The 78xx needs at
  least 0.33 uF at its input and 0.1 uF at its output, and is stable with
  ceramic capacitors.
- **Output:** C906 (100 nF) and C907 (10 uF 25 V X5R, about 5 uF at 9 V).
  On the SID sheet, C1707 and C1710 add 100 nF and 22 uF.

It replaces the `+12V` rail of #36, which needed a charge pump (U901 and
D901 to D903) because a 12 V regulator cannot run from a 12 V input. A 9 V
regulator can, so the charge pump and its clock (`PUMP_CLK`, Q5 of the colour
clock divider) are gone. Since #74 moved the VIC-II to 5 V, the `+12V` net is
gone as well.

#### `+9V` current budget

| Load                         | Typical               | Maximum       | Source                                                          |
| ---------------------------- | --------------------- | ------------- | --------------------------------------------------------------- |
| SID 8580 (U1701), VDD        | below 25 mA           | below 40 mA   | 6581 figures (preliminary datasheet, October 1982, page 8) used as an upper bound; the 8580 draws less |
| Q1701 (audio output buffer)  | about 4 mA            | about 5 mA    | Emitter follower into R1703 (1 kOhm), estimate                  |
| R1333 and R1334 (rail check divider, #49) | 0.6 mA       | 0.6 mA        | 9 V / 14.7 kOhm                                                 |
| **Total**                    | **below 30 mA**       | **below 46 mA** |                                                               |

U902 also draws about 5 mA of quiescent current from its input.

#### `+9V` headroom and dissipation

- **Headroom:** the CJ78M09 is specified with 2 V dropout at 350 mA, and
  needs less at about 45 mA. The lowest input after the input stage (about
  11.2 V) still leaves 2.2 V.
- **Dissipation:** about 3 V x 45 mA plus 12 V x 5 mA, so below 0.2 W. A
  TO-252 with a small copper pad (about 90 to 100 C/W) warms up by less than
  20 C. U902's tab is ground: connect it to the ground pour.
- **Ripple:** `12V_IN_SW` also carries the input ripple of the buck converters
  at about 447 kHz, which a 78xx filters poorly. FB901 with C904 and C905
  attenuates it. Listen for a whine in the audio on the prototype; if there
  is one, a larger ferrite bead or a few Ohm in series in front of C904
  helps.

Datasheets:
[6581 SID preliminary](https://archive.org/details/mos_6581_sid_preliminary_october_1982),
[L78M](https://www.st.com/resource/en/datasheet/l78m.pdf).

### Cassette motor supply

The `Cassette Power (6.2v)` sheet makes the supply of the Datasette motor
(`CASS_POWER`, through FB203 to pin C of the cassette port J202) from the 12 V
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
  synchronized to the sync clock; it only runs while the motor turns.
- **Motor control, as on the original board:** the 6510 drives Q1101 (BC847)
  from port bit P5 (`CASS_MOTOR`, U201 pin 24) through R1104 (10k), with
  R1105 (100k) pulling its base low. Until #100 these were 1k and 10k. The
  6510's NMOS port delivers little current when high, and with 1k the base
  current pulled P5 down to an estimated 1.2 to 1.7 V. That still switched
  Q1101, but was too low for the supervisor, which now reads P5 to pause
  the virtual Datasette. With 10k, P5 only delivers about 0.2 mA and stays
  near its rated high level. Q1101 switches only the 100k pull-up R1103, so
  about 0.1 mA of base current is still far more than it needs. With R1105
  at 100k, it turns on from about 0.7 V on P5, and stays off with P5 low (at
  most 0.4 V on P5 gives at most 0.36 V on the base). With P5 high, Q1101 pulls the enable input
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

### `+3V3`

The `3V3 Buck` sheet makes `+3V3` for the microcontrollers that are planned
for the board (#11, #25, #68, #12) and for other 3.3 V logic. It is a copy of the
5 V buck converter with the values changed for 3.3 V:

- **Converter:** U1001 (LM61440), fed from the 12 V DC input, with the same
  input capacitors (C1001 to C1004), VCC and bootstrap parts and RT resistor
  (R1002, 33k) as the 5 V buck converter.
- **Output voltage:** R1004 (51k) and R1005 (22k) set 1.0 V x (1 + 51/22) =
  3.32 V. TI's 100k/43.2k pair is not available as JLC basic parts. C1008
  (10 pF) with R1006 (1k) is the feedforward network; TI gives 4.7 pF for a
  100k top resistor, and 10 pF keeps the same corner frequency with 51k.
- **Inductor:** L1001 is a 10 uH FCH0650-100M, the same family and footprint
  as the 5 V buck converter's inductor. At about 445 kHz and 12 V in, its ripple
  current is about 0.54 A, 13 % of the LM61440's 4 A rating, within TI's
  range (at least 10 %).
- **Output capacitors:** four 22 uF ceramic capacitors (C1009 to C1012), TI's
  minimum for 3.3 V at about 400 kHz, plus C1013 (100 nF).
- **Synchronization:** C1005 couples the sync clock (`SYNC_CLK_IN`, from the
  `System Clocks` sheet) into EN/SYNC, which R1001 holds at the input
  voltage, the same way as for the 5 V buck converter. U1001's PGOOD joins
  `PGOOD` (see above).

The budget is 1 A for now: the four RP2354 microcontrollers (supervisor,
RAM and ROM, CIA 1, CIA 2) and some 3.3 V logic, with room to spare. The LM61440 can deliver up to 4 A, but the
inductor and output capacitors would need to be checked before raising the
budget much further. At 1 A the rail draws about 0.3 A from the 12 V input.

`+3V3` is the only rail that is always on. The supervisor (see below)
switches the 12 V input of all other rails, see [Input switch](#input-switch).

#### Backup from `+5V` (#110)

The RP2354s (supervisor, RAM and ROM, CIA 1, CIA 2) and the CPLD run from
`+3V3`, and many of their pins sit on 5 V nets: the C64 bus, `~RESET`,
`~IRQ`, `~NMI`, the serial bus and the user port. Their 5 V tolerance only
holds while their own supply is up. If `+3V3` dropped while `+5V` was still
up, for example when the adapter is unplugged, those pins would see 5 V
without supply. U1001 and the 5 V buck converter run from the same input,
so the order in which they drop out is not guaranteed by itself.

```
+5V ── U1002 (ME6211C33, 3.3 V LDO) ── D1001 (SS34) ──┬── +3V3
        C1014 1 uF in        C1015 1 uF out            │
U1001 (3V3 buck) ──────────────────────────────────────┘
```

- **Normal operation:** U1002 makes 3.3 V from `+5V`. After D1001 that is
  slightly below the 3.32 V of U1001, so the diode does not conduct and
  U1002 is idle; at most it shares a few mA with U1001 at the edges of the
  tolerances.
- **`+3V3` sags while `+5V` is up:** D1001 conducts and U1002 carries
  `+3V3` from `+5V`. U1002 is rated for 500 mA, enough for the whole
  `+3V3` load.
- **`+5V` falls as well:** U1002 goes into dropout, and `+3V3` follows
  `+5V` at about 0.5 V below it (dropout plus diode). A pin on a 5 V net is
  then never more than about 0.5 V above the 3.3 V supply.
- **`CAN+5V`:** its L78M05 sits behind four diodes and drops out at about
  10 V input, long before the 5 V buck converter (about 5.5 V), so it is
  always gone before `+5V`, and the backup from `+5V` covers it too. `+9V`
  only feeds the SID's VDD.
- **No back-feeding:**
  - With `+5V` off, D1001 blocks, so `+3V3` does not drain into U1002.
  - U1001's high-side MOSFET could only pass current back into its input
    if `+3V3` were above `12V_MAIN`, which cannot happen, because `+3V3`
    stays below `+5V`, which is made from that same input.
- **What it does not give:** no hold-up time. Once `+5V` collapses,
  `+3V3` follows within milliseconds, so the firmware must tolerate a
  sudden loss of power (#111).
- **Parts:** U1002 ME6211C33M5G-N (C82942, 500 mA, about 120 mV dropout
  at 100 mA, SOT-23-5, CE tied to VIN); D1001 SS34 (C8678, 3 A, 40 V
  Schottky, SMA); C1014 and C1015 (1 uF, C52923) on its input and output,
  as the LDO needs for stability. U1002 draws a few tens of uA from `+5V`
  while idle.
- **To check on the prototype:** unplug the adapter under full and light
  load and measure `+5V`, `CAN+5V` and `+3V3`: `+3V3` must never be more
  than about 0.5 V below `+5V` while `+5V` is above about 3.5 V.

Datasheet: [LM61440](https://www.ti.com/lit/ds/symlink/lm61440.pdf).

## System clocks

The `System Clocks` sheet (`clocks.kicad_sch`, page 12, parts in the 1200
range) makes the VIC-II's colour clock and dot clock and the sync clock of
the buck converters (#76). It replaces the PLL on the former
`250407 Sheet 1`, whose MC4044 (U32) can no longer be bought and whose
74LS629 (U31) is barely in stock, and the `Color Clock Divider` sheet.

| Removed | Function | Now |
| ------- | -------- | --- |
| Y1, U31 (74LS629) | Colour clock oscillator and dot clock VCO | Y1202 (MS5351M) with Y1201 (25 MHz) |
| U29, U30, U32, Q7 | Divider, phase comparator and loop filter that locked the dot clock to the colour clock | not needed: one PLL makes both clocks |
| E2, E3 | PAL/NTSC jumper | firmware setting |
| L2, FB19, C56, C65 to C68, C74, C82, C84 | `+Vc` supply of the PLL | not needed |
| R16, R17, R19, R26, R27, R52, R53, C70, C83, C85, C86, C107, C108 | Other parts of the PLL | not needed |
| FB16, FB17 | Ferrite beads in the colour and dot clock | FB1203, FB1202 |
| U1001, U1002, C1001, C1002 of the former `Color Clock Divider` sheet | `PGOOD` gate and divide by 32 for the buck converter sync | U1201 and CLK2 of Y1202 |

The reference numbers U1001, U1002, C1001 and C1002 now belong to the 3.3 V
buck converter, which moved to page 10.

### Frequencies

One PLL feeds all three outputs, so the clocks stay locked to each other:

| | PLL | Colour clock (CLK0) | Dot clock (CLK1) | Buck converter sync (CLK2) |
| --- | --- | --- | --- | --- |
| NTSC | 801.818 MHz (25 MHz x (32 + 4/55)) | PLL / 56 = 14.31818 MHz | PLL / 98 = 8.181818 MHz | PLL / 224 / 8 = 447.4 kHz |
| PAL | 851.255 MHz (25 MHz x (34 + 3137/62500)) | PLL / 48 = 17.734475 MHz | PLL / 108 = 7.881984 MHz | PLL / 240 / 8 = 443.4 kHz |

- **Exact ratios:** the PLL stays within its 600 MHz to 900 MHz range, and
  both multipliers are exact fractions, so the only frequency error is that
  of the crystal. The dot clock is exactly 4/7 (NTSC) or 4/9 (PAL) of the
  colour clock.
- **Low jitter:** all output dividers are even integers, which gives the
  lowest jitter and a 50 % duty cycle. The Si5351A datasheet gives a period
  jitter of about 70 ps, about 0.1 degrees of hue on the colour clock.
- **Sync clock:** outputs below about 500 kHz need the R divider after the
  multisynth, so CLK2 divides by 224 or 240 and then by 8. That keeps the
  sync at about 445 kHz for both standards, so the inductors, ripple and RT
  resistors of U701 and U1001 still fit. A fixed divide by 32 would have
  given 554 kHz on PAL.
- **SID pitch:** it depends only on the crystal, so sound is not affected.

### Circuit

- **Synthesizer:** Y1202, an MS5351M (a clone of the Si5351A-B-GT with the
  same pinout, which can be fitted instead), on `+3V3`, which is always on.
  VDD (pin 1) and VDDO (pin 7) each have their own 100 nF capacitor (C1201,
  C1202).
- **Crystal:** Y1201, 25 MHz with a 10 pF load. The synthesizer's internal
  load capacitance is set to 10 pF in its registers, so there are no external
  load capacitors.
- **I2C:** on the supervisor's I2C0 bus (`SDA`, `SCL`, pull-ups R1306 and
  R1312 on the `Supervisor and SD card slot` sheet) at address 0x60. The
  controllers on that bus must not use 0x60.
- **Colour clock:** CLK0 goes through U1202, R1203 and FB1203 to `COLOR_CLK`,
  which the root sheet connects to `COLOR_CLOCK_IN` of the VIC-II sheet
  (U504 pin 21).
- **Dot clock:** CLK1 goes through U1203, R1202 and FB1202 to `DOT_CLK`,
  which the root sheet connects to `DOT_CLOCK_IN` of the VIC-II sheet
  (U504 pin 22, with C518) and to `DOT_CLOCK_IN` of the
  `CPU, Cassette and Expansion Port` sheet (expansion port J201 pin 6).
- **Sync clock:** CLK2 goes through U1201, R1201 and FB1201 to `SYNC_CLK`,
  which the root sheet connects to `SYNC_CLK_IN` of the `Power Supply`
  sheet, and from there through C710 and C1005 into EN/SYNC of U701 and
  U1001. U1201 (74AHCT1G08) gates it with `PGOOD`, so the sync only starts
  once both buck converters are in regulation.
- **Buffers:** U1202 and U1203 (SN74AHCT1G125, /OE on ground) and U1201 run
  from `CAN+5V`, like the VIC-II, with C1203 to C1205 (100 nF).
  - Their TTL-level inputs (VIH 2.0 V) accept the synthesizer's 3.3 V
    outputs, and they give the VIC-II's clock inputs a full 5 V swing. The
    original 74LS629 output was TTL, about 3.4 V high.
  - Their inputs tolerate up to 5.5 V at any supply voltage, including 0 V:
    TI specifies the input leakage at 5.5 V for VCC = 0 V to 5.5 V. An
    unpowered buffer drives nothing, so no clock reaches the VIC-II while
    `CAN+5V` is off. Do not fit 74HCT parts instead: their input clamp diodes
    would feed `CAN+5V` from the 3.3 V clock.
  - `CAN+5V` is quieter than `+5V`, which keeps supply noise off the clock
    edges.
- **Series resistors and ferrite beads:** R1201 to R1203 (27 Ohm) terminate
  the buffer outputs. FB1202 and FB1203 replace FB16 and FB17. They sit
  before the clocks branch, and together with C518 (20 pF, formerly C36) they
  form the low-pass filter of the original dot clock. A bead can make a clock
  line ring with C518 and the VIC-II's input capacitance, at about 65 MHz,
  with only the 27 Ohm resistor and the bead's own losses as damping. If the
  prototype shows ringing, replace FB1202 and FB1203 with 0 Ohm. FB1201 is
  not needed for the sync; it only takes the high harmonics off its edges.
- **`PHI0`:** the VIC-II's `PHI0` output now only goes to the 6510. The PLL
  used it as feedback, which the synthesizer does not need.

### Parts

| Ref | Part | LCSC | Package |
| --- | ---- | ---- | ------- |
| Y1202 | MS5351M clock synthesizer | C1509083 | MSOP-10 |
| Y1201 | 25 MHz crystal, 10 pF load, 10 ppm (YXC X322525MMB4SI) | C70582 | 3225 |
| U1202, U1203 | SN74AHCT1G125DBVR | C7484 | SOT-23-5 |
| U1201 | 74AHCT1G08 | C113521 | SOT-23-5 |
| C1201 to C1205 | 100 nF 16 V X7R | C1525 | 0402 |
| R1201 to R1203 | 27 Ohm | C25190 | 0603 |
| FB1201 to FB1203 | 120 Ohm at 100 MHz, 2 A | C14709 | 0603 |

The Si5351A-B-GT (C529366) is the original part with the same pinout, but
it costs more and is hardly in stock.

### Firmware (supervisor)

- After power-up, and before `POWER_ON`, write the register set for NTSC (or
  PAL) with the outputs disabled. That is about 50 register writes.
- Keep the outputs' disable state at low (register 24 = 0x00, the Si5351
  default), not high impedance. Otherwise the buffer inputs float while
  `CAN+5V` comes up. If the MS5351M turns out to behave differently, add
  10k pull-downs at the buffer inputs.
- Enable the outputs once `CAN+5V` is up, before the controllers and the
  6510 are released, as part of the start-up order of #48.
- Report a failure to talk to the synthesizer: the board cannot run without
  its clocks.
- PAL or NTSC is a firmware setting. A PAL board still needs a PAL VIC-II.
- The PLL multiplier is fractional, so the firmware can also trim out the
  crystal's tolerance if needed.

### To check on the prototype

- **MS5351M:** its jitter, PLL range and register compatibility with the
  Si5351A are not confirmed by an independent source. Measure both clock
  frequencies and their jitter, and check the picture (colours, artefact
  colours, sprites) and the sound.
- **Clock edges:** check U504 pins 21 and 22 for ringing (see the ferrite
  beads above), and that the 8562 runs reliably from the buffered clocks.
- **Buck converter sync:** check that U701 and U1001 lock to CLK2 at about
  445 kHz on both standards. U1201 gives EN/SYNC the same 5 V swing through
  C710 and C1005 as the old divider did.

Datasheets:
[Si5351A/B/C-B](https://www.skyworksinc.com/-/media/Skyworks/SL/documents/public/data-sheets/Si5351-B.pdf),
[SN74AHCT1G125](https://www.ti.com/lit/ds/symlink/sn74ahct1g125.pdf),
[SN74AHCT1G08](https://www.ti.com/lit/ds/symlink/sn74ahct1g08.pdf).

## Supervisor microcontroller

The `Supervisor and SD card slot` sheet holds a microcontroller (U1301,
RP2354B) that will decide when the rest of the system may power up (#46). It
runs from `+3V3`, the one rail that is always on. Besides its supply, clock,
reset and USB it switches the other rails on and off (#47), and it can
program, reset and talk to the RAM and ROM controller (#11, see
[RAM and ROM replacement](#ram-and-rom-replacement)), the CIA 1 controller
(#25, see [CIA 1, keyboard and joysticks](#cia-1-keyboard-and-joysticks)) and
the CIA 2 controller (#68, see
[CIA 2, serial port and user port](#cia-2-serial-port-and-user-port)). The
same sheet holds the micro SD card slot (#95, see
[SD card slot](#sd-card-slot)). Its other functions follow in separate
issues:

| Function | Hardware | Firmware |
| -------- | -------- | -------- |
| Keep all rails except `+3V3` off until it is safe to start | #47 | #48 |
| Check the legacy chips (6510, VIC-II, SID) and their rails after a safe power-up | #49 | #50 |
| Link to the other microcontrollers | #51 | used by #52 and #53 |
| Check that all ROM images are present | uses #51 | #52 |
| Check that the other microcontrollers run up-to-date firmware | uses #51 | #53 |
| Status LEDs on a separate module (power, ready, activity, error) | #54 | #55 |
| Program the clock synthesizer and enable its outputs | #76 | #76, #48 |
| Reset the C64 without switching the rails off (soft reset) | #70 | #88 |
| Reset button in the case | #109 | #88 |
| Check and program the CPLD that replaces the PLA, and enable its outputs | #83 | #86, #48 |
| Load programs, disk images and tape images from a micro SD card (#13) | #95 | #96, #97 |

### Circuit

The circuit follows Raspberry Pi's reference design for the RP2350
([Hardware design with RP2350](https://datasheets.raspberrypi.com/rp2350/hardware-design-with-rp2350.pdf)).
The RP2354B is an RP2350B with 2 MB of flash in the package, so there is no
external flash chip.

- **Core regulator:**
  - L1301 (3.3 uH Abracon AOTA-B201610S3R3-101-T) between `VREG_LX` and the
    core supply, which is a local net (`+1V1`) on this sheet so that it can
    never join the core supply of another RP2354.
  - The inductor's polarity dot (pad 1) is on the core supply side, as in
    the RP2350 reference layout.
  - C1303 decouples `VREG_VIN`, C1304 and C1309 (4.7 uF) the output, C1307
    and C1310 the DVDD pins.
  - R1307 (33 Ohm) and C1308 (4.7 uF) filter `VREG_AVDD`.
- **Supplies:** the IOVDD, ADC_AVDD, USB_OTP_VDD and QSPI_IOVDD pins are on
  `+3V3`, with a 100 nF capacitor each (C1311 to C1321).
- **Clock:** Y1301 (12 MHz Abracon ABM8-272-T3) with C1301 and C1302 (15 pF)
  and R1303 (1k) in series with XOUT, as recommended for this crystal.
- **Reset and boot:** R1308 (200k) pulls RUN up and SW1302 (RESET) pulls it
  low. SW1301 (BOOTSEL) pulls `QSPI_SS` low through R1309 (1k).
- **USB:** J1301 is a USB-C receptacle used for data only. VBUS does not
  power the board.
  - R1310 and R1311 (27 Ohm) are in series with D+ and D-.
  - R1301 and R1302 (5.1k) on CC1 and CC2 identify the board as a device.
  - U1304 (TPD4E05U06DQA, a four-channel TVS array) protects D+, D-, CC1
    and CC2 against ESD, and D1305 (H5VL10BH) protects VBUS (#72).
  - R1304 (4.7k) and R1305 (6.8k) divide VBUS to about 3.0 V for GPIO13
    (`VBUS_DET`), so the firmware can detect a connected host; C1305 filters
    it and C1306 decouples VBUS.

### Programming

The supervisor is programmed through the USB-C connector with the RP2354's
built-in USB bootloader: hold BOOTSEL, press and release RESET, then release
BOOTSEL. The board then shows up as a USB drive that accepts a UF2 file. The
SWD pins (SWCLK, SWDIO) are not brought out.

### Pins

| GPIO | Pin | Net | Direction | Function |
| ---- | --- | --- | --------- | -------- |
| GPIO0 | 77 | Gate of Q1301 through R1313 | Output | High turns on Q1301, which pulls `~RESET` low (soft reset), see [Soft reset](#soft-reset) |
| GPIO1 | 78 | `~RESET` through R1315 | Input | Reads the shared reset line |
| GPIO2 | 79 | `PREFLIGHT_OK` (supervisor sheet), `POWER_ON` (power supply sheet) | Output | High turns on all rails except `+3V3`, see [Input switch](#input-switch) |
| GPIO3 | 80 | `PGOOD` through R1330 (1k), net `PGOOD_PROBE` | Input | Both buck converters in regulation and `CAN+5V` up, see [Chip and rail checks](#chip-and-rail-checks) |
| GPIO4 | 1 | `CAN5V_PROBE`, divider R1331/R1332 from `CAN+5V` | Input | High while `CAN+5V` is above about 3 V |
| GPIO5 | 2 | `9V_PROBE`, divider R1334/R1333 from `+9V` | Input | High while `+9V` is above about 6 V |
| GPIO6 | 3 | `PHI0_PROBE`, `PHI0` through R514 (1k) | Input | Clock from the VIC-II |
| GPIO7 | 4 | `PHI2_PROBE`, `PHI2` through R215 (1k) | Input | Clock from the 6510 |
| GPIO8 | 6 | `ATN` through R1335 (100 Ohm) | Input | Serial bus ATN, for the virtual drive (#98), see [Serial port](#serial-port) |
| GPIO9 | 7 | `CLK` through R1336 (100 Ohm) | Input | Serial bus CLK |
| GPIO10 | 8 | `DATA` through R1337 (100 Ohm) | Input | Serial bus DATA |
| GPIO11 | 9 | `SER_CLK_IN`, to U1602 gate E (pin 11) | Output | High pulls the serial bus CLK low; R1647 (10k) pulls the pin down |
| GPIO12 | 11 | `SER_DATA_IN`, to U1602 gate F (pin 13) | Output | High pulls the serial bus DATA low; R1646 (10k) pulls the pin down |
| GPIO24 | 25 | `CASS_RD` through R1338 (220 Ohm) | Open drain (firmware) | Plays tape pulses for the virtual Datasette (#100): low pulls the cassette read line low; never driven high |
| GPIO25 | 26 | `CASS_SENSE` through R1339 (220 Ohm) | Open drain (firmware) | Low makes the PLAY key look pressed; never driven high |
| GPIO26 | 27 | `CASS_MOTOR` through R1340 (1k) | Input | 6510 port bit P5: low means motor on |
| GPIO27 | 28 | `CASS_WRT` through R1341 (1k) | Input | 6510 port bit P3, the cassette write signal, for recording |
| GPIO13 | 12 | `VBUS_DET` | Input | USB host connected |
| GPIO15 | 14 | `RESET_REQ`, through R1344 (1k) to the reset button on J1304 | Input | High while the reset button is pressed; R1342 (4.7k) pulls it down, see [Reset button](#reset-button) |
| GPIO16 | 16 | `TDO` | Input | JTAG data from the CPLD (U301) |
| GPIO17 | 17 | `TDI` | Output | JTAG data to the CPLD |
| GPIO18 | 18 | `TCK` | Output | JTAG clock of the CPLD |
| GPIO19 | 19 | `TMS` | Output | JTAG mode select of the CPLD, R302 (10k) pull-up on the `PLA Replacement` sheet |
| GPIO20 | 20 | `EN_PLA` (sheet pin `EN_OUTPUT` on the `PLA Replacement` sheet) | Output | High enables the CPLD's outputs; low or not driven keeps them in high impedance (R301, 4.7k to ground), see [PLA replacement](#pla-replacement) |
| GPIO21 | 21 | `RDY_LED` through R1323 (150 Ohm) | Output | Ready LED (white) on the LED module; R1329 (4.7k) pulls the pin down |
| GPIO22 | 22 | `ERR_LED` through R1326 (220 Ohm) | Output | Error LED (amber) on the LED module; R1328 (4.7k) pulls the pin down |
| GPIO23 | 23 | `ACT_LED` through R1324 (150 Ohm) | Output | Activity LED (green) on the LED module; R1327 (4.7k) pulls the pin down |
| GPIO28 | 36 | `SWCLK` | Output | SWD clock of the CIA 2 controller |
| GPIO29 | 37 | `SWDAT` | Bidirectional | SWD data of the CIA 2 controller |
| GPIO30 | 38 | `RUN` | Output | High lets the CIA 2 controller run; low or not driven holds it in reset |
| GPIO33 | 42 | `SD_DET` | Input | Card detect switch of the SD card slot (J1302), R1322 (10k) pull-up |
| GPIO34 | 43 | `SCLK`, through R1316 (33 Ohm) `SD_CLK` | Output | SD card clock (SPI0 SCK) |
| GPIO35 | 44 | `SD_CMD` | Bidirectional | SD card command line; SPI0 TX (MOSI) in SPI mode |
| GPIO36 | 45 | `SD_DAT0` | Bidirectional | SD card data 0; SPI0 RX (MISO) in SPI mode |
| GPIO37 | 46 | `SD_DAT1` | Bidirectional | SD card data 1; not used in SPI mode |
| GPIO38 | 47 | `SD_DAT2` | Bidirectional | SD card data 2; not used in SPI mode |
| GPIO39 | 48 | `SD_DAT3` | Bidirectional | SD card data 3; chip select (CS) in SPI mode |
| GPIO40 | 49 | `SDA` | Bidirectional | I2C0 data to the RAM and ROM controller (U1401), the CIA 1 and CIA 2 controllers (U1501, U1601) and the clock synthesizer (Y1202, address 0x60) |
| GPIO41 | 52 | `SCL` | Output | I2C0 clock to U1401, U1501, U1601 and Y1202 |
| GPIO42 | 53 | `SWCLK` | Output | SWD clock of the CIA 1 controller |
| GPIO43 | 54 | `SWDAT` | Bidirectional | SWD data of the CIA 1 controller |
| GPIO44 | 55 | `RUN` | Output | High lets the CIA 1 controller run; low or not driven holds it in reset |
| GPIO45 | 56 | `RUN` | Output | High lets the RAM and ROM controller run; low or not driven holds it in reset |
| GPIO46 | 57 | `SWCLK` | Output | SWD clock of the RAM and ROM controller |
| GPIO47 | 58 | `SWDAT` | Bidirectional | SWD data of the RAM and ROM controller |

- **I2C:** R1306 and R1312 (4.7k) pull `SDA` and `SCL` up to `+3V3`.
- **State before the firmware runs:** all assigned pins come out of reset
  with the output disabled and the internal pull-down on. All three `RUN`
  lines therefore stay low and the three controllers stay in reset, see
  [Programming and start-up](#programming-and-start-up). GPIO0 is low as
  well, so Q1301 stays off and the C64 is not held in reset, and GPIO20 is
  low, so the CPLD's outputs stay off. R302 holds TMS high, so the CPLD's
  JTAG logic stays idle. The 10k pull-ups of the SD card lines override the
  internal pull-downs, so `SD_DAT3` (chip select) is high and the card is
  not selected. R1327 to R1329 hold GPIO21 to GPIO23 low, so the ready,
  activity and error LEDs stay off; only the power LED is on. GPIO3 to
  GPIO7 (chip and rail checks) are inputs; with the rails off they read
  low. R1646 and R1647 hold GPIO11 and GPIO12 low, so U1602 leaves the
  serial bus released; the firmware drives both low as its first action
  (RP2350 erratum E9, see [Serial port](#serial-port)). R1342 holds GPIO15
  low, so the reset button reads as released.
- **Back-feeding:** these pins only connect to the RAM and ROM controller,
  the CIA 1 and CIA 2 controllers, the clock synthesizer, the CPLD, the SD
  card and the LED module, which are all on `+3V3`, to the inputs of the
  chip and rail checks (GPIO3 to GPIO7), which the firmware must never drive,
  to the serial bus: GPIO8 to GPIO10 read it through 100 Ohm and GPIO11
  and GPIO12 only drive U1602, which runs from `+3V3` and only sinks, and to
  the cassette lines: GPIO24 and GPIO25 only pull low or release, and
  GPIO26 and GPIO27 are inputs, all behind series resistors, and to the
  reset button (GPIO15), which only switches `+3V3`, except GPIO0 and GPIO1, which reach
  `~RESET` through Q1301 and R1315, see [Soft reset](#soft-reset).

The other GPIOs are not assigned yet; the follow-up issues assign them. The
state of each assigned pin from power-up until the firmware runs, and with
blank firmware, is to be documented with those issues, because they must
not feed unpowered parts while the rails are off.

### Soft reset

The supervisor can reset the C64 without switching the rails off (#70), by
pulling the shared `~RESET` line low. The same circuit also makes the
power-on reset (#89), which used to come from a timer on the original
board.

```
GPIO0 ── R1313 1k ──┬── Q1301 gate (AO3400A)    drain: ~RESET, source: GND
                    └── R1314 4.7k to GND
GPIO1 ── R1315 4.7k ─── ~RESET                  (read back)
```

- **The line:** `~RESET` is wired-OR. R201 (1k, on the
  `CPU, Cassette and Expansion Port` sheet) pulls it up to `+5V`. Q1301 is the only driver on the board;
  devices on the user port, the serial bus and the expansion port can pull
  it low as well. It goes to the 6510, the CIA 1 and CIA 2 controllers, the
  SID, the user port, the serial bus and the expansion port.
- **Driver:** Q1301 (AO3400A, N-channel) is an open-drain driver, the same
  as the open-collector gates already on the line. GPIO0 high turns it on and
  pulls `~RESET` low; GPIO0 low releases it. GPIO0 is an ordinary push-pull
  output: whatever the firmware does with it, the supervisor cannot drive
  `~RESET` high.
- **Gate drive:** R1313 (1k) and R1314 (4.7k) give the gate about 2.7 V,
  above the 2.5 V at which the AO3400A's on-resistance is specified, so the
  line goes to practically 0 V, also with extra pull-ups in disk drives or
  cartridges. GPIO0 sees 5.7 kOhm to ground, less than the 8.2 kOhm that
  Raspberry Pi recommends for RP2350 erratum E9: with only the internal
  pull-down, a pin that was driven high can stay at about 2 V after a reset
  of the supervisor, which would keep Q1301 on.
- **Reading:** GPIO1 reads `~RESET` through R1315 (4.7k), so the firmware
  also sees resets from other sources. GPIO1 is 5 V tolerant while IOVDD is
  powered, which is always the case because the supervisor runs from
  `+3V3`. R1315 limits the current to about 1 mA if GPIO1 were driven by
  mistake.
- **Power-on reset (#89):** the original board reset the system with a
  timer when `+5V` came up: half B of U20 (556) with R50 and C105 as the
  trigger and R34 and C24 for a pulse of about 0.5 s, driving `~RESET`
  through gate D of U8. These parts are gone. The supervisor switches the
  rails on itself (#47), so its firmware is always running when `+5V` comes
  up. It drives GPIO0 high before `POWER_ON`, so that `~RESET` is already
  low while `+5V` rises (Q1301's gate drive comes from `+3V3`), and releases
  it once the rails are up, the clocks run and the other controllers are
  ready, after a minimum time for disk drives and cartridges (#88, #48). It
  can also reset the system if `PGOOD` drops (on GPIO3 since #49), which
  the timer could not.
- **Fail-safe:** in reset and with blank firmware, GPIO0 is an input with the
  internal pull-down on, and R1314 holds the gate low, so Q1301 is off and
  the supervisor never holds the system in reset. In both cases the rails
  stay off as well, so the missing hardware power-on reset does no harm.
- **Back-feeding:** with `+5V` off, Q1301 can only sink current (its body
  diode is reverse biased), and GPIO1 is an input behind R1315, so nothing
  feeds the unpowered chips.
- **What is reset:** everything on `~RESET`, including disk drives on the
  serial bus and a cartridge, as with a reset button on the user port. The
  RAM and ROM controller (U1401) does not see `~RESET`, so the RAM contents
  survive a soft reset, as on the original C64.
- **Firmware (#88):** GPIO0 is a push-pull output, high for reset. GPIO1 is
  an input with the internal pull-up and pull-down off. The power-on reset
  sequence, the pulse length and what triggers a soft reset are defined in
  #88.

| Ref | Part | LCSC | Package |
| --- | ---- | ---- | ------- |
| Q1301 | AO3400A | C20917 | SOT-23 |
| R1313 | 1k | C11702 | 0402 |
| R1314, R1315 | 4.7k | C25900 | 0402 |

### Reset button

A push button in the case (#109) asks the supervisor for a reset. It does
not touch `~RESET` itself: the supervisor reads the button and makes the
reset through Q1301, the same way as its other resets, see
[Soft reset](#soft-reset).

```
+3V3 ── R1343 220 ── J1304 pin 1 ── case button ── J1304 pin 2 ──┬── R1344 1k ── GPIO15 (RESET_REQ)
                                                                 └── R1342 4.7k to GND
```

- **Levels:** released, R1342 holds GPIO15 at 0 V. Pressed, R1343 and R1342
  give about 3.15 V at about 0.7 mA. R1342 (4.7k) is below the 8.2k that
  Raspberry Pi recommends for RP2350 erratum E9, so the pin cannot hang at
  about 2 V after a reset of the supervisor.
- **Why through the supervisor:** an earlier draft pulsed `~RESET` directly
  through a capacitor. A pulse like that is chopped up by the button's
  contact bounce, and the last, short pieces can reset some chips and not
  others. The supervisor debounces the button and then holds `~RESET` low
  for a fixed time, so every press gives one clean reset in the right order
  with the CPLD and the other controllers (#88, #48).
- **Protection:** the button sits in the case on a cable. R1344 (1k) is in
  series with GPIO15, so a discharge into the cable reaches the pin only
  through 1k. R1343 limits the current to about 15 mA if the cable shorts
  to ground.
- **Cable:** a push button has no polarity, so the 2-pin cable can be
  plugged in either way.
- **Not the supervisor's own reset:** SW1302 (RESET) on the board resets
  the supervisor itself, see [Circuit](#circuit-1); the case button only
  resets the C64.
- **Firmware (#88):** GPIO15 is an input with the internal pull-up and
  pull-down off. The debounce time, the reset pulse length and what the
  button does while the rails are off (ignore it, or switch the system on)
  are defined in #88.

| Ref | Part | LCSC | Package |
| --- | ---- | ---- | ------- |
| J1304 | 1x2 pin header, 2.54 mm, for the case button | C32713268 | THT |
| R1342 | 4.7k | C25900 | 0402 |
| R1343 | 220 Ohm | C25091 | 0402 |
| R1344 | 1k | C11702 | 0402 |

Any momentary push button works in the case, for example a tactile switch
with a printed cap or a panel-mount push button.

### Status LEDs

The status LEDs sit on a separate LED module (`hardware/led-module`) that is
mounted behind the front of the case and connects to J1303 with a short
cable (#54). Through-hole LEDs on a small board can sit exactly in the holes
of the case, independent of the main board layout. The original power LED
connector J203 (CN10 on the original board) is gone: its LED was lit from
`+5V`, which no longer means that the board has power.

| LED | Colour | Driven by | Meaning |
| --- | ------ | --------- | ------- |
| PWR | Red | `+3V3` through R1325 (220 Ohm) | The board has power and the supervisor is supplied. Works without firmware. |
| RDY | White | GPIO21 through R1323 (150 Ohm) | The checks passed, the rails are on and the C64 runs |
| ACT | Green | GPIO23 through R1324 (150 Ohm) | Supervisor activity: SD card, the emulated disk drive and Datasette, loading programs into RAM, transfers to the other controllers, programming the CPLD. Real drives and tapes do not light it. |
| ERR | Amber | GPIO22 through R1326 (220 Ohm) | Error or failed start-up |

The patterns of RDY, ACT and ERR (steady, blinking, codes) are defined by
the firmware (#55).

- **PWR in hardware:** `+5V` only comes up after the supervisor's firmware
  switches the rails on (#47), so a power LED on `+5V` would stay dark
  exactly when the firmware is missing or a check fails. The PWR LED runs
  from `+3V3` instead.
- **Connector:** J1303 is a 2×3 pin header with 2.54 mm pitch.

  | Pin | Signal |
  | --- | ------ |
  | 1 | GND |
  | 2 | `PWR_LED` |
  | 3 | Not connected (spare) |
  | 4 | `RDY_LED` |
  | 5 | `ACT_LED` |
  | 6 | `ERR_LED` |

  The LED module has the same header (J1), so a cable that keeps the pin
  numbers, for example a 6-way ribbon cable with IDC sockets, connects them
  pin for pin. The header is not keyed. A cable plugged in rotated by 180°
  does no harm: the LED cathodes then end up on `ERR_LED` instead of ground,
  so at most one LED lights dimly through two resistors in series, and the
  others are reverse-biased at no more than 3.3 V (they are rated for 5 V).
  No pin carries more than a few mA.
- **Resistors on the main board:** the current-limiting resistors sit at
  the supervisor, so a shorted cable cannot overload a GPIO or the rail.
  The module only carries the LEDs and the connector.
- **Currents:** at the low currents used here, the forward voltage of the
  red and amber LEDs is about 2.0 V and that of the white and green LEDs
  about 2.6 to 2.8 V. That gives about 6 mA for red and amber and about 3 to
  5 mA for white and green, within the RP2350's GPIO limits, and bright
  enough behind a case hole. The firmware can dim RDY, ACT and ERR with PWM
  if they differ too much in brightness.
- **Pull-downs:** R1327 to R1329 (4.7k) pull GPIO21 to GPIO23 to ground.
  RP2350 erratum E9 can leave a pin that was driven high at about 2 V after
  a reset, which would make the amber LED glow. Raspberry Pi recommends an
  external pull-down of at most 8.2k against this. With a pin driven high,
  each pull-down draws about 0.7 mA.
- **Standby:** the PWR LED draws about 6 mA from `+3V3` whenever the board
  has power. A larger resistor (330 to 470 Ohm) would halve that, if the
  standby current matters.

| Ref | Part | LCSC | Package |
| --- | ---- | ---- | ------- |
| J1303 | 2×3 pin header, 2.54 mm | C65114 | THT |
| R1323, R1324 | 150 Ohm | C22808 | 0603 |
| R1325, R1326 | 220 Ohm | C25091 | 0402 |
| R1327 to R1329 | 4.7k | C25900 | 0402 |

### Chip and rail checks

The supervisor checks the legacy chips in their sockets, the 6510 (U201),
the VIC-II (U504) and the SID (U1701), and the rails that feed them (#49).
The firmware is in #50.

Detecting the chips before power-up is not practical:
- **Signal pins:** almost every pin is shared with the controllers, the
  CPLD and pull-ups, and an unpowered NMOS chip only adds a few pF and
  protection diodes that conduct for negative voltages. Measuring through
  that would need test currents into unpowered chips.
- **Supply pins:** the decoupling capacitors hide the chip's own
  capacitance, and an NMOS chip draws no measurable current below about
  0.7 V.

Powering up with any of the three chips missing is harmless:
- **Without the VIC-II** there is no `PHI0`, so the 6510 and everything that
  runs on `PHI2` stands still.
- **Without the 6510** nothing writes on the bus, so there is no bus
  contention.
- **Without the SID** nothing reads its pins.
- **Wrong type:** a 6567 in the 8562 socket gets 5 V instead of 12 V on
  VDD, and a 6581 in the 8580 socket gets 9 V instead of 12 V. They do not
  work properly, but are not damaged.

The supervisor therefore powers up into a safe state (`~RESET` low, CPLD
outputs off) and then checks the rails and the clocks. The real risk is a
chip inserted the wrong way round or offset by a pin, which shorts its rail.
The rail checks let the supervisor switch the rails off within
milliseconds. That limits the damage, but cannot guarantee that the chip
survives, so the sockets must make the orientation obvious: a notch, a
pin-1 marking and an arrow in the silkscreen, and all three sockets facing
the same way.

| GPIO | Signal | Circuit | Shows |
| ---- | ------ | ------- | ----- |
| GPIO3 | `PGOOD` | R1330 (1k) in series | `+5V` and `+3V3` in regulation. The pull-ups R706 and R1003 run from `CAN+5V`, so it also reads low when `CAN+5V` collapses. |
| GPIO4 | `CAN5V_PROBE` | R1331 (4.7k) from `CAN+5V`, R1332 (10k) to ground: about 3.4 V at 5 V | `CAN+5V` (VIC-II) above about 3 V |
| GPIO5 | `9V_PROBE` | R1334 (10k) from `+9V`, R1333 (4.7k) to ground: about 2.9 V at 9 V | `+9V` (SID) above about 6 V |
| GPIO6 | `PHI0_PROBE` | R514 (1k) from `PHI0_IN`, on the 6510's side of FB501 (VIC-II sheet) | VIC-II running |
| GPIO7 | `PHI2_PROBE` | R215 (1k) from `PHI2`, on the bus side of FB205 (CPU sheet) | 6510 running |

- **What the checks show:**
  - a rail that does not come up, or drops, points at a chip that shorts
    it;
  - a dot clock without `PHI0` points at a missing or dead VIC-II;
  - `PHI0` without `PHI2` points at a missing or dead 6510;
  - the SID needs no hardware: the firmware tests it with a register test
    once the CPU runs (#50).
- **Clock taps:** the series resistors sit right at the tap, so the GPIO
  and its trace are a short, damped stub on the clock. Keep these traces
  short in the layout. `PHI2` reaches the controllers anyway; the
  supervisor's own tap keeps the check independent of their firmware.
- **Levels:** `PGOOD`, `PHI0` and `PHI2` swing to 5 V. GPIO3, GPIO6 and
  GPIO7 are 5 V tolerant while `+3V3` is up, which is always the case. The
  dividers keep GPIO4 and GPIO5 below 3.3 V.
- **With the rails off:**
  - `PGOOD` reads low through its pull-ups to the dead `CAN+5V`;
  - the dividers pull GPIO4 and GPIO5 low (about 3 to 5 kOhm, within the
    8.2k of RP2350 erratum E9);
  - the clock taps go through 1k into unpowered nets.

  None of these pins may ever be driven by the firmware.
- **Load:** the dividers draw 0.34 mA from `CAN+5V` and 0.6 mA from `+9V`.

| Ref | Part | LCSC | Package | Sheet |
| --- | ---- | ---- | ------- | ----- |
| R1330 | 1k | C11702 | 0402 | Supervisor and SD card slot |
| R1331, R1333 | 4.7k | C25900 | 0402 | Supervisor and SD card slot |
| R1332, R1334 | 10k | C25744 | 0402 | Supervisor and SD card slot |
| R514 | 1k | C21190 | 0603 | VIC-II, HF Module Replacement and AV out |
| R215 | 1k | C11702 | 0402 | CPU, Cassette and Expansion Port |

### SD card slot

J1302 is a micro SD card slot (#95). The supervisor will use it to load
programs, disk images (`.d64`) and tape images (`.t64`, `.tap`) for the C64
(#13). The firmware is in #96 and #97.

- **Why the supervisor:** it runs from `+3V3`, which is always on, so it can
  read the card, and offer it over USB (#96), while the rest of the board is
  off. It has free GPIOs and already talks to the RAM and ROM controller.
- **Socket:** J1302 is a Korean Hroparts Elec TF-01A (C91145), a push-push
  socket with a card detect switch, with the footprint
  `c64-mini:microSD_Hroparts_TF-01A` from the component library (#102). The
  card sticks out 2.65 mm past the front of the socket, so the socket has to
  sit at the board edge.
- **Supply:** the card runs from `+3V3`, which is within the 2.7 V to 3.6 V
  of the SD specification. The supervisor never asks for 1.8 V signalling
  (UHS-I), so the card lines stay at 3.3 V. C1322 (100 nF) and C1323 and
  C1324 (4.7 uF) decouple the socket, for the card's current peaks of up to
  about 200 mA.
- **Interface:** the wiring supports both SPI mode and 4-bit SD mode, so the
  firmware can start with SPI and change to SD mode later without a new
  board:
  - GPIO34 to GPIO36 are the hardware SPI0 pins (SCK, TX and RX), on CLK,
    CMD and DAT0. Chip select (DAT3, GPIO39) is driven as a plain GPIO.
  - DAT0 to DAT3 are on four consecutive GPIOs (GPIO36 to GPIO39), and CLK
    is two pins below DAT0, as PIO drivers for 4-bit SD mode expect. These
    pins are above GPIO31, so that PIO block needs its GPIO window to start
    at GPIO16.
- **Pull-ups:** R1317 to R1321 (10k) on CMD and DAT0 to DAT3. The SD
  specification requires them in SD mode; in SPI mode they keep the card
  deselected while the supervisor is in reset, keep MISO defined while the
  card does not drive it, and keep the unused DAT1 and DAT2 from floating.
- **Clock:** R1316 (33 Ohm) in series with CLK, close to U1301, damps
  ringing on the clock edges.
- **Card detect:** the switch connects `SD_DET` (pin 9) to the shell, which
  is on ground; R1322 (10k) pulls it up. The drawing does not say whether
  the switch closes or opens when a card is inserted; check this on the
  prototype before the firmware relies on it.
- **ESD protection:** two TPD4E05U06DQA arrays at the socket. U1302 protects
  DAT0 to DAT3, U1303 protects CLK (on the socket side of R1316), the card's
  `+3V3` supply, card detect and CMD.
- **Back-feeding:** the card and all its lines are on `+3V3`, like the
  supervisor itself, so they cannot feed the rails that are off.

| Pin | Name | Net | U1301 |
| --- | ---- | --- | ----- |
| 1 | DAT2 | `SD_DAT2` | GPIO38 |
| 2 | DAT3/CD | `SD_DAT3` | GPIO39 |
| 3 | CMD | `SD_CMD` | GPIO35 |
| 4 | VDD | `+3V3` | |
| 5 | CLK | `SD_CLK` | GPIO34, through R1316 |
| 6 | VSS | GND | |
| 7 | DAT0 | `SD_DAT0` | GPIO36 |
| 8 | DAT1 | `SD_DAT1` | GPIO37 |
| 9 | DET | `SD_DET` | GPIO33 |
| SH | Shell | GND | |

| Ref | Part | LCSC | Package |
| --- | ---- | ---- | ------- |
| J1302 | Korean Hroparts Elec TF-01A | C91145 | SMD |
| U1302, U1303 | TPD4E05U06DQA | C138714 | USON-10 |
| C1322 | 100 nF | C1525 | 0402 |
| C1323, C1324 | 4.7 uF | C19666 | 0603 |
| R1316 | 33 Ohm | C25105 | 0402 |
| R1317 to R1322 | 10k | C25744 | 0402 |

Datasheets:
[TF-01A](https://datasheet.lcsc.com/datasheet/pdf/7650eac21a54674b08ad4ba21e17fceb.pdf?productCode=C91145),
[TPD4E05U06](https://www.ti.com/lit/ds/symlink/tpd4e05u06.pdf).

### Notes

- **Stock:** LCSC lists the RP2354B (C39843328) with no stock at the time of
  writing; the RP2354A (C41378174, QFN-60) is in stock.
- **USB-C symbol:** J1301 uses the 14-pin USB 2.0 symbol on a 16-pin
  footprint, so the SBU pads (A8, B8) stay unconnected.
- **TVS footprint:** the H5VL10BH comes in a DFN1006-2L package and sits on
  a 0402 footprint of the same size.

## PLA replacement

The `PLA Replacement` sheet (`pla.kicad_sch`, page 3, parts in the 300
range) holds a CPLD (U301, XC9572XL) that takes over the address decoding
and the bus logic of the original board (#83). Users no longer have to find
a 906114-01 PLA or buy a replacement PLA.

| Removed | Function | Now |
| ------- | -------- | --- |
| U17 (906114-01 PLA) and its socket | Memory map: RAM, ROM, I/O and cartridge selects | U301 |
| U15 (74LS139) | I/O decoding into `~VIC`, `~SID`, `~COLOR`, `~CIA1`, `~CIA2`, `~I/O1` and `~I/O2` | U301 |
| U27 (7408) | `RDY`, `CAEC` and `~COLORAM`; gate A was unused | U301 |
| U8 gate B (7406) with R31 (1k) | `~AEC`, the inverted `AEC` | U301, with R303 (4.7k) |
| C31, C33, C46 | Decoupling of these chips | C301 to C304 at U301 |
| RP4 units F and H | 3k3 pull-ups on `~GAME` and `~EXROM` | R202, R203 (3k3) on the `CPU, Cassette and Expansion Port` sheet |

The reference numbers R301 and R302 were used before for the pull-ups on
`CIA1_PA6` and `CIA1_PA7`, which are now R1704 and R1705 on the SID sheet.

### Circuit

- **CPLD:** U301, an XC9572XL-10VQG64C (72 macrocells, 52 user I/O, about
  10 ns from pin to pin) on `+3V3`. Its I/O is 5 V tolerant, so the 5 V bus
  signals connect directly.
- **Supplies:** VCCINT (pins 3, 37) and VCCIO (pins 26, 55) are on `+3V3`,
  with one 100 nF capacitor per pin (C301 to C304). The symbol shows pins 37
  and 55 as hidden pins on top of pins 3 and 26; on the PCB each of the four
  pins needs its own capacitor close by.
- **Output enable:** GTS1 (pin 5) is on `EN_OUTPUT`, which the supervisor
  drives from GPIO20 (`EN_PLA` on the `Supervisor and SD card slot` sheet).
  R301 (4.7k) pulls it to ground. The design ties the output enable of every
  output to GTS1, active high: low puts all outputs in high impedance, high
  enables them.
  - In reset and with blank firmware, the supervisor's GPIO20 is an input
    with its internal pull-down, and R301 keeps the outputs off. R301 is
    below the 8.2 kOhm that RP2350 erratum E9 calls for.
  - The CPLD's bus-hold (about 50 kOhm) cannot override R301; the divider
    leaves GTS1 at about 0.3 V.
  - GTS2 (pin 2) is not used.
- **`~AEC`:** U301 now drives `~AEC` push-pull, so R303 (4.7k, formerly R31
  with 1k for the open-collector gate) only holds it high while the outputs
  are off. U505 and U506 then stay disabled. R303 goes to `+5V`, which the
  CPLD's 5 V tolerant I/O accepts.
- **JTAG:** TDI (pin 28), TMS (pin 29), TCK (pin 30) and TDO (pin 53) go to
  the supervisor's GPIO17, GPIO19, GPIO18 and GPIO16, so that it can check
  and program the CPLD (#86). R302 (10k) pulls TMS up to `+3V3`, which keeps
  the CPLD's JTAG logic idle while the supervisor does not drive it. There is
  no JTAG header.
- **Global pins:** the design is combinational, so the global clock and
  set/reset functions are not used. `~CAS` is on GCK2 in case a later design
  wants to latch on it; `R/~W` and `VA4` are on GCK1 and GCK3, and `A15` on
  GSR, as ordinary inputs.
- **Pull-ups:** on the `CPU, Cassette and Expansion Port` sheet, R202 and
  R203 pull up `~EXROM` and `~GAME`, R205 `~DMA` and R210 to R213 A12 to A15
  (all 3k3; they replace the resistor array RP4, #107). R1533 (3k3) on the
  CIA 1 sheet pulls up `~IRQ`.

### Pins

| Pin | Net | Direction | Original function |
| --- | --- | --------- | ----------------- |
| 7, 58, 59, 60 | `A8` to `A11` | In | U15 address inputs |
| 61 to 64 | `A12` to `A15` | In | PLA I8, I7, I6, I5 |
| 8 | `~VA14` | In | PLA I4 |
| 9, 10 | `~EXROM`, `~GAME` | In | PLA I12, I13 |
| 11, 12, 13 | `~HIRAM`, `~LORAM`, `~CHAREN` | In | PLA I2, I1, I3 |
| 15 | `R/~W` | In | PLA I11 |
| 16 | `~CAS` | In | PLA I0 |
| 17, 18 | `VA4`, `VA5` | In | PLA I15, I14 (A12 and A13 in VIC-II cycles) |
| 19 | `BA` | In | PLA I9, U27 |
| 20 | `AEC` | In | U8 gate B and U27; the PLA's `~AEC` input (I10) is now inverted inside the CPLD |
| 23 | `~DMA` | In | U27 |
| 5 | `EN_OUTPUT` | In | Output enable (GTS1) from the supervisor |
| 27 | `~CASRAM` | Out | PLA F0, to U1401 |
| 31, 32, 33 | `~BASIC`, `~KERNAL`, `~CHAROM` | Out | PLA F1 to F3, to U1401 |
| 34 | `GR/~W` | Out | PLA F4, to U1401 |
| 24, 25 | `~ROML`, `~ROMH` | Out | PLA F6, F7, to the expansion port |
| 22 | `~VIC` | Out | U15 1Y0, to U504 |
| 35 | `~SID` | Out | U15 1Y1, to U1701 |
| 36 | `~COLORAM` | Out | U27: `~COLOR` AND `AEC`, to U1401 |
| 38, 39 | `~CIA1`, `~CIA2` | Out | U15 2Y0, 2Y1, to U1501 and U1601 |
| 40, 42 | `~I/O1`, `~I/O2` | Out | U15 2Y2, 2Y3, to expansion port pins 7 and 10 |
| 43 | `~AEC` | Out | U8 gate B, to U505 and U506 |
| 44 | `CAEC` | Out | U27: `AEC` AND `~DMA`, to the 6510's AEC |
| 45 | `RDY` | Out | U27: `BA` AND `~DMA`, to the 6510's RDY |
| 28, 29, 30, 53 | `TDI`, `TMS`, `TCK`, `TDO` | JTAG | To the supervisor |

- **Internal signals:** `~I/O` (PLA F5), `~COLOR` and `~CIAS` (U15 1Y2 and
  1Y3) no longer leave the CPLD.
- **Free pins:** 1, 2, 4, 6, 46 to 52, 56 and 57 (13 user I/O). The CPLD's
  bus-hold keeps them at a defined level, so they are not connected.

### Levels

- **Inputs:** 5 V tolerant, also while the outputs are off.
- **Outputs:** 3.3 V LVTTL, at least 2.4 V high under load and close to
  3.3 V at the light loads on the board. That is above the 2.0 V input high
  level of the NMOS and TTL chips, and of the HCT multiplexers U505 and U506.
  The PLA and the 74LS139 only guaranteed about 2.7 V themselves, so
  cartridges already had to accept TTL levels on `~ROML`, `~ROMH`, `~I/O1`
  and `~I/O2`.

### Power and back-feeding

U301 is on `+3V3`, which is always on, so it is powered while the C64 chips
are not. This lets the supervisor check and program it before the rails come
up (#86).

- **Outputs:** off (high impedance) while `EN_OUTPUT` is low. The supervisor
  only raises it once `+5V` is up, and lowers it again before the rails go
  off.
- **Weak pull-ups:** the XC9500XL datasheet (DS054, table 5) gives every
  user pin a weak pull-up of about 50 kOhm to 3.3 V while the CPLD is blank,
  during programming and early in its power-up, and a bus-hold of the same
  strength in normal operation. Neither depends on GTS. With `+5V` off, a
  pin on a 5 V net can therefore feed up to about 66 uA into an unpowered
  chip. About 30 pins are on 5 V nets, so that is about 2 mA in total at
  worst, small enough for the rest of the `+5V` rail to absorb. This is
  accepted.
- **Blank or programming:** all outputs float high through these pull-ups.
  All chip selects are active low and `GR/~W` high means read, so nothing is
  selected. The supervisor programs the CPLD only with the rails off or with
  `~RESET` held low (#86).
- **Power-up:** VCCINT and VCCIO come up together with `+3V3`. Below 1 V on
  VCCINT the pins are undefined and may drive briefly, but `+5V` is off then.

### Timing

The CPLD decodes in one pass of about 10 ns, where the original board took
the PLA (about 20 to 35 ns) plus a 74LS139 stage for the I/O chip selects.
The chip selects therefore arrive earlier; the bus timing of the 6510 and the
VIC-II still sets the window. On the original board `~CASRAM` follows `~CAS`
by the PLA's delay, and U1401 times its RAM accesses from `~CASRAM`. The HDL
(#93) keeps that delay within the original range, or the U1401 firmware must
not depend on it.

### Parts

| Ref | Part | LCSC | Package |
| --- | ---- | ---- | ------- |
| U301 | XC9572XL-10VQG64C | C90569 | TQFP-64 (VQ64) |
| C301 to C304 | 100 nF | C1525 | 0402 |
| R301, R303 | 4.7k | C25900 | 0402 |
| R302 | 10k | C25744 | 0402 |
| R202, R203 (`CPU, Cassette and Expansion Port` sheet) | 3k3 | C25890 | 0402 |

### Logic and firmware

- **Logic (#93):** the HDL implements the 906114-01's equations from skoe's
  "The C64 PLA Dissected", plus U15, U27 and the `~AEC` inverter, with the
  output enables on GTS1. It is built with Xilinx ISE 14.7, the only tool
  for the XC9500XL. The source and the programming file go into the
  repository, so that nobody needs ISE to build the board.
- **Supervisor (#86):** at start-up, reads the IDCODE and the USERCODE over
  JTAG and programs the CPLD only when the design is missing or out of date.
  The XC9500XL keeps its design in on-chip flash (about 10,000 program
  cycles), so it is not programmed on every start-up.
- **Start-up order (#48, #88):** raise `EN_OUTPUT` once `+5V` is up and
  before `~RESET` is released; lower it before the rails go off. Without the
  CPLD's outputs the 6510 gets no `RDY` or `CAEC` and no chip is selected.

Datasheets:
[XC9500XL family (DS054)](https://docs.amd.com/v/u/en-US/ds054),
[XC9572XL (DS057)](https://docs.amd.com/v/u/en-US/ds057).

## RAM and ROM replacement

The `RAM and ROM replacement` sheet holds a microcontroller (U1401, RP2354B)
that takes the place of all memory chips of the original board (#11). It
keeps the RAM, the ROM images and the colour RAM in its own SRAM and answers
the bus cycles of the 6510 and the VIC-II.

| Removed       | Original part             | Now                              |
| ------------- | ------------------------- | -------------------------------- |
| U9 to U12, U21 to U24 | 4164 64K x 1 DRAM | 64 KB RAM in U1401             |
| U3            | 2364 BASIC ROM            | BASIC ROM image in U1401         |
| U4            | 2364 KERNAL ROM           | KERNAL ROM image in U1401        |
| U5            | 2332 character ROM        | Character ROM image in U1401     |
| U6            | 2114 1K x 4 static RAM    | Colour RAM in U1401              |
| U16 (U503 since #74) | 4066 colour data switch | U1401 drives D0 to D3 and the colour data lines itself (#81) |

Their support parts went with them: the decoupling capacitors C4 to C7,
C25 to C28 and C40 to C43, the 10 uF capacitor C14, the 33 Ohm resistor
arrays RP1 and RP2 between the multiplexed address bus and the DRAMs, the
`~CASRAM` series resistor R42 and the unfitted C204.

The address decoding stays as it was:

- The CPLD that replaces the PLA (U301, see
  [PLA replacement](#pla-replacement)) decodes every access with the PLA's
  logic, so bank switching through the 6510's port at $01, cartridges
  (`~GAME`, `~EXROM`) and the VIC-II bank selected by CIA 2 work as before.
  The controller only answers the chip selects the CPLD gives it, and does
  not need to track any of these.
- The address multiplexers (now U505, U502 and U506) and the latch (now
  U508) stay; they moved to the VIC-II sheet in #74. The VIC-II's register
  select, the CPLD's decoding of VIC-II cycles and the low address byte in
  VIC-II cycles all depend on them.
- The colour data switch (U16, later U503, a 4066) is gone (#81). It
  connected the colour data lines to D0 to D3 in 6510 cycles; U1401 now
  serves both sides itself, see [Timing](#timing-1).

### Circuit

The circuit is the same as the supervisor's (see [Circuit](#circuit)),
following Raspberry Pi's RP2350 reference design:

- **Core regulator:** L1401 (3.3 uH) with its own local core supply net
  (`+1V1`) and C1404, C1405, C1407 and C1408. R1402 (33 Ohm) and C1406
  (4.7 uF) filter `VREG_AVDD`; C1403 (4.7 uF) decouples the regulator input.
- **Supplies:** the IOVDD and other 3.3 V supply pins are on `+3V3`, with
  100 nF capacitors C1409 to C1419.
- **Clock:** Y1401 (12 MHz ABM8-272-T3) with C1401 and C1402 (15 pF) and
  R1401 (1k) in series with XOUT.
- **Flash:** the 2 MB flash in the RP2354B holds the firmware. The QSPI pins
  are unconnected; the internal pull-up on `QSPI_SS` makes it boot from
  flash.
- **No buttons and no USB:** the supervisor resets and programs it, see
  [Programming and start-up](#programming-and-start-up).

### Pins

| GPIO | Pin | Net | Function |
| ---- | --- | --- | -------- |
| GPIO0 to GPIO7 | 77 to 80, 1 to 4 | `D0` to `D7` | Data bus |
| GPIO8 to GPIO23 | 6 to 9, 11 to 14, 16 to 23 | `A0` to `A15` | Address bus |
| GPIO24 | 25 | `~CASRAM` | RAM select from the CPLD (U301), includes `~CAS` |
| GPIO25 | 26 | `~BASIC` | BASIC ROM select from the CPLD |
| GPIO26 | 27 | `~KERNAL` | KERNAL ROM select from the CPLD |
| GPIO27 | 28 | `~CHAROM` | Character ROM select from the CPLD |
| GPIO28 | 36 | `~COLORAM` | Colour RAM select from the CPLD |
| GPIO29 | 37 | `R/~W` | 6510 read/write |
| GPIO30 | 38 | `GR/~W` (sheet pin `COLORR/~W`) | Colour RAM write strobe from the CPLD |
| GPIO31 | 39 | `VA4` | Multiplexed address: A4, then A12 |
| GPIO32 | 40 | `VA5` | Multiplexed address: A5, then A13 |
| GPIO33 | 42 | `~VA14` | VIC-II bank, inverted A14 (CIA 2 PA0) |
| GPIO34 | 43 | `~VA15` | VIC-II bank, inverted A15 (CIA 2 PA1) |
| GPIO35 | 44 | `AEC` | High: 6510 cycle; low: VIC-II cycle |
| GPIO36 to GPIO39 | 45 to 48 | `COLORD0` to `COLORD3` (bus `COLORD[0..3]`) | Colour RAM data to the VIC-II's D8 to D11; GPIO36 plus n carries colour bit n |
| GPIO40 | 49 | `SDA` | I2C0 data, link to the supervisor |
| GPIO41 | 52 | `SCL` | I2C0 clock, link to the supervisor |
| GPIO42 | 53 | `EEPROM_SDA` | I2C1 data to the EEPROM |
| GPIO43 | 54 | `EEPROM_SCL` | I2C1 clock to the EEPROM |
| GPIO46 | 57 | `PHI2` through U1402 | 6510 clock |
| GPIO47 | 58 | `~CAS` through U1402 | Column address strobe from the VIC-II |

GPIO44 and GPIO45 (pins 55, 56) are not used.

### Level shifting

GPIO0 to GPIO39 are 5 V tolerant while the RP2354B's IOVDD is powered, so
the 5 V bus signals connect directly. GPIO40 to GPIO47 can also be ADC
inputs and are not 5 V tolerant. The two 5 V signals on those pins, `~CAS`
and `PHI2`, go through U1402 (74LVC2G17, dual Schmitt-trigger buffer) on
`+3V3` with C1420 (100 nF). Its inputs accept 5 V at a 3.3 V supply, and it
adds about 4 pF to `~CAS` and a few nanoseconds of delay.

In the other direction, the controller drives the data lines with 3.3 V.
That is above the 2.0 V input high level of the NMOS and TTL chips on the
bus.

5 V on the GPIOs is only allowed while `+3V3` is up. The input switch
ensures this: `+3V3` is always on, and all other rails are switched (see
[Input switch](#input-switch)).

### Addressing

| Cycle | `AEC` | A0 to A7 | A8 to A11 | A12, A13 | A14, A15 |
| ----- | ----- | -------- | --------- | -------- | -------- |
| 6510 | High | `A0` to `A7` | `A8` to `A11` | `A12`, `A13` | `A14`, `A15` |
| VIC-II | Low | `A0` to `A7` from latch U508 | `A8` to `A11` from the VIC-II | `VA4`, `VA5` while `~CAS` is low | `~VA14`, `~VA15`, inverted |

In VIC-II cycles, `A12` to `A15` read high through the pull-ups R210 to
R213 (3k3, formerly RP4).

| Memory | Address | Select | Notes |
| ------ | ------- | ------ | ----- |
| RAM | A0 to A15 | `~CASRAM` | Read when `R/~W` is high, write when low |
| BASIC ROM ($A000 to $BFFF) | A0 to A12 | `~BASIC` | |
| KERNAL ROM ($E000 to $FFFF) | A0 to A12 | `~KERNAL` | |
| Character ROM | A0 to A11 | `~CHAROM` | 6510 at $D000 to $DFFF, VIC-II at $1000 to $1FFF in banks 0 and 2 |
| Colour RAM | A0 to A9 | `~COLORAM` | Written with `GR/~W`; data on D0 to D3 for the 6510, on `COLORD0` to `COLORD3` for the VIC-II |

The PLA logic in the CPLD does not select RAM during the VIC-II's refresh
cycles, so they need no handling.

### ROM images

U1403 (AT24C256C, 32 KB) stores the ROM images. It is on I2C1 (GPIO42,
GPIO43) with R1407 and R1408 (4.7k) as pull-ups and C1421 (100 nF). Its
address pins are high (I2C address 0x57) and its write protect pin is low.
One set of ROM images (BASIC 8 KB, KERNAL 8 KB, character ROM 4 KB) takes
20 KB.

The original Commodore ROM images are not part of this project, see
[NOTICE.md](../../NOTICE.md#roms-and-firmware). They are to be supplied by
the user or replaced by images that are free of these rights.

### Programming and start-up

The supervisor (U1301) programs and controls the controller:

- **Reset:** the supervisor's GPIO45 drives `RUN` through R1404 (1k). R1403
  (10k) pulls `RUN` to ground, so the controller stays in reset until the
  supervisor drives it high (about 3.0 V). With the supervisor in reset,
  `RUN` stays below 0.6 V.
- **Programming:** the supervisor's GPIO46 and GPIO47 drive SWCLK and SWDIO
  through R1405 and R1406 (33 Ohm). The supervisor writes the firmware to
  the controller's flash over SWD; this also works on a blank chip.
- **Link:** both microcontrollers share I2C0 on GPIO40 and GPIO41.

Intended start-up order, for the firmware: the supervisor releases `RUN`
while the other rails are still off; the controller loads the ROM images
from the EEPROM (which is on `+3V3`) and reports over I2C that it is ready;
only then does the supervisor turn on the other rails. The 6510 then starts
with its memory already in place.

### Power and back-feeding

U1401 to U1403 are on `+3V3`, which is always on, so they are powered while
the C64 chips are not.

- In reset, all bus pins of U1401 are inputs with the internal pull-down on,
  so they do not feed the unpowered chips.
- Once it runs, the firmware must keep all bus pins as inputs until the
  rails are up, and turn the pull-downs off then (see below).
- U1402's inputs tolerate signals from unpowered chips; its outputs only go
  to U1401.

### Timing

The controller has to answer each access as fast as the original chips. The
tightest deadline is for RAM: the data must be on the bus about 135 ns
after `~CASRAM` falls, the CAS access time of a 200 ns 4164 DRAM. This is an
estimate, not a measured value. ROM and colour RAM accesses leave more time.

These timing requirements are for the firmware:

- Run the bus loop from SRAM, not from flash, on a core of its own with
  interrupts off.
- In 6510 cycles the full address is valid before `~CASRAM` falls, so the
  lookup can start early. In VIC-II cycles only A12 and A13 arrive late; the
  four possible bytes can be read beforehand, and one picked when
  `~CASRAM` falls.
- Time everything from the edges of `~CASRAM`, `~CAS` and `PHI2`, not from
  fixed delays after `PHI0`: the `~RAS` and `~CAS` timing of the 8562 differs
  slightly from the 6567's, and the delay from `~CAS` to `~CASRAM` is set by
  the CPLD (#93).
- Stop driving the data lines as soon as the select goes high.
- Latch write data late in the cycle, not when `~CASRAM` falls: the 6510
  can take up to about 200 ns into `PHI2` to put the data on the bus. Latch
  colour RAM writes on the rising edge of `GR/~W`.
- **Colour RAM** (#81): there is no switch between the CPU bus and the
  VIC-II's colour data inputs any more, so U1401 serves both sides itself,
  chosen by `AEC`:
  - **VIC-II cycles** (`AEC` low, `~COLORAM` low): put the nibble addressed
    by A0 to A9 on `COLORD0` to `COLORD3`. Never drive D0 to D3 for colour
    RAM, because the RAM data is on D0 to D7 at the same time; the VIC-II
    reads all 12 bits at once.
  - **6510 and DMA cycles** (`AEC` high, `~COLORAM` low): on a read, drive
    the nibble on D0 to D3 and leave D4 to D7 undriven, so the upper nibble
    reads back as whatever is left on the bus, as on the original board. On
    a write, latch D0 to D3 on the rising edge of `GR/~W`. A cartridge that
    does DMA (for example an REU) accesses colour RAM the same way.
  - `COLORD0` to `COLORD3` only go to the VIC-II's D8 to D11, which are
    inputs, so they can stay outputs all the time.
  - **Bit order:** `COLORDn` (GPIO36 plus n) carries colour bit n, the bit
    the 6510 sees on Dn: `COLORD0` goes to the VIC-II's D8. Before #81 the
    names ran the other way (`COLOR_D0` was D3 and D11), as on the replica.
  - Before U1401 runs, its pins are inputs with pull-downs, so the VIC-II
    reads colour 0. That is harmless, because the C64 is still in reset.
- Turn off the internal pull-downs of the bus pins (RP2350 erratum E9), and
  set the data lines to 12 mA drive strength.
- The RP2350 is rated for 150 MHz. Overclock it only if measurements show
  that this is not fast enough.

## CIA 1, keyboard and joysticks

The `CIA1, Keyboard and Joysticks` sheet holds a microcontroller (U1501,
RP2354B) that takes the place of CIA 1 (#25), with a USB-A socket for the
keyboard and the two control ports. CIA 2 is replaced the same way (#68),
see [CIA 2, serial port and user port](#cia-2-serial-port-and-user-port).

| Removed | Original part | Now |
| ------- | ------------- | --- |
| U1 | 6526 CIA 1 | U1501 |
| C2 | Decoupling capacitor of U1 | C1509 to C1519 at U1501 |
| CN1 | 20-pin keyboard connector | USB keyboard on J1501 |
| CN9 | Control port 1 (port B) | J1503 |
| CN8 | Control port 2 (port A) | J1502 |
| FB18 | Ferrite bead in the +5V of both control ports | FB1501 |
| U20 (556) with R33, R35, R41, C23 and C38 | RESTORE one-shot (half A); half B unused since #89 | U1501 pulls `~NMI` low itself (#91) |
| U8 (7406) | Gate C drove `~NMI` from the one-shot; the other gates were already gone | U1501 (#91) |
| C22, C39 | Decoupling of U8 and U20 | none needed |

These parts of the original circuit stay on the `CPU, Cassette and Expansion Port` sheet:

- **Chip select:** `~CIA1` for $DC00 to $DCFF now comes from the CPLD
  (U301) on the `PLA Replacement` sheet, which replaced U15 (74LS139) (#83).
- **Paddles:** a 4066 still switches the POT lines of control port 1 or 2
  to the SID. It moved to the SID sheet as U1702 (#73), see
  [SID and audio output](#sid-and-audio-output). The controller drives its
  control inputs, the former keyboard columns `COL6` and `COL7`, now
  `CIA1_PA6` and `CIA1_PA7`. R1704 and R1705 (10k) pull them up to +5V,
  because the 4066 needs at least 3.5 V there.
- **RESTORE:** the original board turned a falling edge on `~RESTORE` into
  a pulse of about 19 us on `~NMI`, with a one-shot (half A of U20, 556) and
  an open-collector gate (gate C of U8, 7406). The controller makes the
  pulse itself now, so these parts are gone (#91), see
  [RESTORE](#restore).
- **Pull-ups:** RP4 (3k3) pulled up `~IRQ` on the original board; R1533
  (3k3) on this sheet takes its place since #107, with the same value. RP3,
  which pulled up `CASS_RD`, `CNT1` and `SP1`, went with CIA 2 (#68); R1534,
  R1532 and R1531 (4.7k) on this sheet take its place.

### Circuit

The circuit is the same as the supervisor's (see [Circuit](#circuit)),
following Raspberry Pi's RP2350 reference design:

- **Core regulator:** L1501 (3.3 uH) with its own local core supply net
  (`+1V1`) and C1504, C1505, C1507 and C1508. R1503 (33 Ohm) and C1506
  (4.7 uF) filter `VREG_AVDD`; C1503 (4.7 uF) decouples the regulator input.
- **Supplies:** the IOVDD and other 3.3 V supply pins are on `+3V3`, with
  100 nF capacitors C1509 to C1519.
- **Clock:** Y1501 (12 MHz ABM8-272-T3) with C1501 and C1502 (15 pF) and
  R1501 (1k) in series with XOUT.
- **Flash:** the 2 MB flash in the RP2354B holds the firmware. The QSPI pins
  are unconnected.
- **No buttons:** the supervisor resets and programs it, see
  [Programming and start-up](#programming-and-start-up-1).

### Pins

| GPIO | Pin | Net | 6526 pin | Function |
| ---- | --- | --- | -------- | -------- |
| GPIO0 to GPIO7 | 77 to 80, 1 to 4 | `D0` to `D7` | D0 to D7 | Data bus |
| GPIO8 to GPIO11 | 6 to 9 | `A0` to `A3` | RS0 to RS3 | Register select |
| GPIO12 | 11 | `CNT1` (sheet pin `CNT`) | CNT | User port pin 4, on the CIA 2 sheet |
| GPIO13 | 12 | `SP1` (sheet pin `SP`) | SP | User port pin 5, on the CIA 2 sheet |
| GPIO21 | 21 | `~RESET` | ~RES | Reset input |
| GPIO22 | 22 | `~IRQ` | ~IRQ | Interrupt to the 6510, open drain |
| GPIO23 | 23 | `CASS_RD` (sheet pin `~FLAG`) | ~FLAG | Cassette read and serial bus SRQ |
| GPIO24 | 25 | `PHI2` | PH2 | 6510 clock |
| GPIO25 | 26 | `R/~W` | R/~W | 6510 read/write |
| GPIO26 | 27 | `~CIA1` (sheet pin `~SELECT`) | ~CS | Chip select from the CPLD (U301) |
| GPIO27 | 28 | `~NMI` | | RESTORE, open drain to the 6510's NMI |
| GPIO28 | 36 | `CIA1_PA6` | PA6 | Paddle select for control port 1 (U1702) |
| GPIO29 | 37 | `CIA1_PA7` | PA7 | Paddle select for control port 2 (U1702) |
| GPIO30 | 38 | to `JOYB3` | PA3 | Control port 2, right |
| GPIO31 | 39 | to `JOYB2` | PA2 | Control port 2, left |
| GPIO32 | 40 | to `JOYB1` | PA1 | Control port 2, down |
| GPIO33 | 42 | to `JOYBTNB` | PA4 | Control port 2, fire |
| GPIO34 | 43 | to `JOYB0` | PA0 | Control port 2, up |
| GPIO35 | 44 | to `JOYA3` | PB3 | Control port 1, right |
| GPIO36 | 45 | to `JOYA2` | PB2 | Control port 1, left |
| GPIO37 | 46 | to `JOYA1` | PB1 | Control port 1, down |
| GPIO38 | 47 | `~LP`, to `JOYBTNA` | PB4 | Control port 1, fire, and the VIC-II light pen input |
| GPIO39 | 48 | to `JOYA0` | PB0 | Control port 1, up |
| GPIO40 | 49 | `SDA` | | I2C0 data, link to the supervisor |
| GPIO41 | 52 | `SCL` | | I2C0 clock, link to the supervisor |
| GPIO46 | 57 | `USB_EN` | | Keyboard port power on |
| GPIO47 | 58 | `USB_FLT` | | Keyboard port overcurrent |
| USB_DP, USB_DM | 67, 66 | `USB_D+`, `USB_D-` through 27 Ohm | | USB keyboard |

GPIO14 to GPIO20 and GPIO42 to GPIO45 are not used. The joystick lines are
not in bit order on the GPIOs, so the firmware has to map them to port bits.
`~PC` and the TOD input of the 6526 have no counterpart: `~PC` was not
connected, and the controller makes the TOD reference itself.

PB5 to PB7 and PA5 only went to the keyboard connector. They exist only in
the firmware now, as part of the keyboard matrix. PB6 and PB7 as timer
outputs therefore have no pin either; on the original board they only
reached the keyboard connector as well.

### Control ports

J1502 (control port 2) and J1503 (control port 1) are 9-pin male D-sub
sockets (CONNFLY DS1037-09MNAKT74-0CC) with the original pinout. Their
shells are on ground.

- **Digital lines:** each of the ten joystick lines goes from the connector
  through a TVS channel to ground (TPD4E05U06DQA arrays, #72), a 100 Ohm
  series resistor and a 10k pull-up to +5V at the GPIO:

  | Line | Connector pin | TVS | Series | Pull-up | GPIO |
  | ---- | ------------- | --- | ------ | ------- | ---- |
  | `JOYA0` | J1503 pin 1 | U1504 | R1530 | R1520 | GPIO39 |
  | `JOYA1` | J1503 pin 2 | U1504 | R1528 | R1518 | GPIO37 |
  | `JOYA2` | J1503 pin 3 | U1504 | R1527 | R1517 | GPIO36 |
  | `JOYA3` | J1503 pin 4 | U1504 | R1526 | R1516 | GPIO35 |
  | `JOYBTNA` | J1503 pin 6 | U1507 | R1529 | R1519 | GPIO38 (`~LP`) |
  | `JOYB0` | J1502 pin 1 | U1505 | R1525 | R1515 | GPIO34 |
  | `JOYB1` | J1502 pin 2 | U1505 | R1523 | R1513 | GPIO32 |
  | `JOYB2` | J1502 pin 3 | U1505 | R1522 | R1512 | GPIO31 |
  | `JOYB3` | J1502 pin 4 | U1505 | R1521 | R1511 | GPIO30 |
  | `JOYBTNB` | J1502 pin 6 | U1506 | R1524 | R1514 | GPIO33 |

  The pull-ups replace the 6526's internal pull-ups, so the lines idle at
  5 V as before. A pressed switch pulls the GPIO to about 0.05 V. The series
  resistors and TVS diodes protect the controller against ESD and against
  devices that drive a line while the board is off.
- **Light pen:** `~LP` connects GPIO38 and the VIC-II's light pen input
  (pin 9) behind R1529. The fire button of control port 1 still triggers
  the VIC-II's light pen latch directly, as on the original board.
- **POT lines:** pins 5 and 9 go to the 4066 (U1702 on the SID sheet) as
  before. Since #72 they have TVS channels at the connector, so ESD cannot
  reach the SID's POT inputs through the 4066: U1507 for `POTAX` and `POTAY`
  (control port 1, together with `JOYBTNA`), U1506 for `POTBX` and `POTBY`
  (control port 2, together with `JOYBTNB`). The paddles swing from 0 V to
  5 V into the SID's 1 nF timing capacitors, so the channels' 0.5 pF do not
  matter.
- **Supply:** pin 7 of both ports gets +5V through FB1501 (100 Ohm at
  100 MHz, 800 mA), which replaces the through-hole ferrite bead FB18.
  D1501 (H5VL10BH) protects pin 7 on the connector side of FB1501.

### Keyboard port

J1501 is a USB-A socket (CONNFLY DS1095-WNR0). The controller's USB port
runs as a host at full or low speed, which covers USB keyboards.

- **Data:** R1507 and R1508 (27 Ohm) are in series with D+ and D-, and D1504
  (D+) and D1505 (D-) (H5VL10BH) protect them against ESD. The USB-A socket
  sits apart from the control ports, so it keeps single diodes instead of
  sharing an array with them. The host pull-downs on D+
  and D- are inside the RP2354.
- **VBUS:** U1502 (TPS2051C) switches `+5V` to `VBUS`. It is rated for
  0.5 A and limits the current to 0.67 to 1.01 A, so a short in the
  keyboard or cable does not pull down `+5V`. C1521 (100 nF) and C1522
  (4.7 uF) decouple its input. C1520 (150 uF) is the bulk capacitor that
  USB requires on a host port (at least 120 uF). D1503 protects `VBUS`
  against ESD.
- **Enable:** GPIO46 drives `USB_EN`. R1509 (10k) pulls it low, so the port
  stays off until the firmware turns it on. The firmware can also turn it
  off and on again to restart a keyboard that hangs.
- **Fault:** `USB_FLT` (open drain, active low) goes to GPIO47 with R1510
  (10k) as pull-up to `+3V3`, so the firmware can report an overcurrent.
- `VBUS` is only powered while `+5V` is on, see
  [Input switch](#input-switch).

### Level shifting

All 5 V signals are on GPIO0 to GPIO39, which are 5 V tolerant while
`+3V3` is up. GPIO40 to GPIO47 only carry 3.3 V signals (I2C, `USB_EN`,
`USB_FLT`).

Towards the board, only the data lines are driven high, with 3.3 V. That is
above the 2.0 V input high level of the 6510 and the other NMOS and TTL
chips on the bus, as for the RAM and ROM controller. All other outputs are
open drain against pull-ups to +5V, as on the 6526:

| Signal | Pull-up | Why |
| ------ | ------- | --- |
| `~IRQ` | R1533 (3k3) | Shared with the VIC-II and the expansion port; the only pull-up since RP4 is gone (#107) |
| `CASS_RD` | R1534 (4.7k) | Cassette read and serial bus SRQ, driven from outside |
| `CNT1`, `SP1` | R1532, R1531 (4.7k) | User port lines, also driven from outside |
| `CIA1_PA6`, `CIA1_PA7` | R1704, R1705 (10k) on the SID sheet | U1702 (4066) needs at least 3.5 V to switch on |
| `~NMI` | R1627 (4.7k) on the CIA 2 sheet | Shared with CIA 2 and the expansion port |
| Joystick lines and `~LP` | R1511 to R1520 (10k) | Switches and devices pull them to ground |

The firmware must never drive these lines high: a pressed switch or another
device on the line would short the output.

### RESTORE

The RESTORE key of the original C64 went to a one-shot that made a short
pulse on `~NMI`. On the c64-mini the RESTORE key is a key on the USB
keyboard, and U1501 drives `~NMI` directly from GPIO27 (#91). The one-shot
(half A of U20, 556, with R33, R35, R41, C23 and C38) and gate C of U8
(7406) are gone, and with them the rest of U20 and U8, whose other halves
and gates #89 and #83 had already freed.

- **Open drain:** `~NMI` is wired-OR with CIA 2 (U1601) and the expansion
  port, with R1627 (4.7k, on the CIA 2 sheet) as its pull-up to +5V.
  GPIO27 follows the same rule as `~IRQ`: pull low or release, never drive
  high.
- **Pulse:** the 6510 triggers NMI on the falling edge only. The firmware
  (#92) pulls `~NMI` low for about 20 us per key press and releases it,
  however long the key is held, so that a held key does not block NMIs from
  CIA 2 or a cartridge. If another source already holds `~NMI` low,
  RESTORE does nothing, as on the original C64.
- **Levels:** GPIO27 is 5 V tolerant while `+3V3` is up, which is always the
  case, so it can sit on the 5 V line directly.
- **Fail-safe:** in reset and with blank firmware, GPIO27 is an input with
  its internal pull-down, which R1627 overrides, so `~NMI` stays released.
- **Back-feeding:** GPIO27 only pulls low or releases, so it cannot feed
  `~NMI` while `+5V` is off.

### Programming and start-up

The supervisor (U1301) programs and controls the controller, the same way
as the RAM and ROM controller:

- **Reset:** the supervisor's GPIO44 drives `RUN` through R1502 (1k). R1504
  (10k) pulls `RUN` to ground, so the controller stays in reset until the
  supervisor drives it high.
- **Programming:** the supervisor's GPIO42 and GPIO43 drive SWCLK and SWDIO
  through R1505 and R1506 (33 Ohm).
- **Link:** the supervisor and both controllers share I2C0 on GPIO40 and
  GPIO41, so the two controllers need different I2C addresses.

The KERNAL needs CIA 1 to start: timer A drives the jiffy clock and the
keyboard scan. The supervisor therefore turns on the other rails only after
all controllers have reported over I2C that they are ready.

### Power and back-feeding

U1501 and U1502 are on `+3V3` and `+5V`; U1501 is powered while the C64
chips are not.

- In reset, all pins of U1501 are inputs with the internal pull-down on, so
  they do not feed the unpowered chips. `USB_EN` stays low and the keyboard
  port stays off.
- Once it runs, the firmware must keep all pins that lead to the C64 as
  inputs until the rails are up, and must not turn on the internal pull-ups
  of those pins: they would feed the unpowered `+5V` rail through the 10k
  pull-ups.

### Firmware

The firmware is not part of this issue. These requirements follow from the
circuit:

- **Bus:** answer only while `~CIA1` is low and `PHI2` is high. Drive the
  data lines only for a read, and release them as soon as `PHI2` falls,
  because the VIC-II and the RAM and ROM controller use the bus while
  `PHI2` is low. Latch write data at the falling edge of `PHI2`; the 6510
  holds it only briefly after the edge, so bypass the PIO input
  synchronizers or sample just before the edge.
- **6526 model:** step a cycle-exact model of the 6526 once per `PHI2`
  cycle. The timers count every cycle, also the ones the VIC-II takes from
  the 6510. Every bus cycle the controller sees is one a real 6526 would
  see, including the 6510's dummy reads, so read side effects such as the
  clearing of the interrupt flags come out right. Which revision to match
  (6526 or 6526A) is an open question of #25.
- **Ports:** compute what $DC00 and $DC01 return from the data and data
  direction registers, the USB keyboard and the levels on the joystick
  lines, including the ghosting of the keyboard matrix. Pull a joystick
  line low whenever the matching port bit is an output set to low, because
  the original port drove the connector pins directly. Shift Lock acts as a
  latched left Shift.
- **Light pen:** whenever port B bit 4 is low, from a key in the matrix or
  from an output, pull `~LP` low as well, so that the VIC-II's light pen
  latch reacts as on the original board.
- **RESTORE (#92):** pull `~NMI` low for about 20 us (the length of the
  old one-shot pulse) and release it, once per key press, whatever the key's
  hold time, see [RESTORE](#restore).
- **Time of day:** count 50 Hz or 60 Hz from the crystal, as selected by bit
  7 of CRA.
- **Pins:** turn off the internal pull-downs of the bus pins (RP2350
  erratum E9). The bus pins are in the PIO window from GPIO0 and the
  joystick lines in the window from GPIO16, so they need separate PIO
  programs or the processor's own GPIO access.

## CIA 2, serial port and user port

The `CIA2, Serial Port and User Port` sheet holds a microcontroller (U1601,
RP2354B) that takes the place of CIA 2 (#68), with the serial port and the
user port.

| Removed | Original part | Now |
| ------- | ------------- | --- |
| U2 | 6526 CIA 2 | U1601 |
| C3 | Decoupling capacitor of U2 | C1609 to C1619 at U1601 |
| U8 gates A, E and F | 7406 drivers for ATN, CLK and DATA | U1602 (SN74LVC06A) |
| CN4 | Serial port | J1601 |
| FB7, FB8, FB9 | Ferrite beads in ATN, DATA and CLK | FB1601, FB1603, FB1602 |
| R28, R29, R30 | 1k pull-ups on ATN, DATA and CLK | R1608, R1607, R1609 |
| CR100 to CR103 | Clamp diodes on CLK and DATA | U1607 (TVS array) |
| CN2 | User port | J1602 |
| C96 | 0.1 uF on +5V beside the user port | C1621 |
| RP3 | 3k3 pull-ups on `~FLAG2`, `~NMI`, `CASS_RD`, `CNT1`, `SP1`, `CNT2` and `SP2` | R1628, R1627, R1534, R1532, R1531, R1623 and R1624 (4.7k) |

These parts of the original circuit stay on the `CPU, Cassette and Expansion Port` sheet:

- **Chip select:** `~CIA2` for $DD00 to $DDFF now comes from the CPLD
  (U301) on the `PLA Replacement` sheet, which replaced U15 (74LS139) (#83).
- **VIC-II bank:** U502 (74F258) and U1401 still take `~VA14` and `~VA15`,
  and the CPLD (U301) takes `~VA14` in place of the PLA.
- **U8:** the 7406 is gone (#91). Gates A, E and F went with CIA 2, gate D
  with the old power-on reset (#89), gate B with the `AEC` inverter (#83)
  and gate C, the last one, with the RESTORE one-shot.

### Circuit

The circuit is the same as for CIA 1 (see [Circuit](#circuit-2)): core
regulator L1601 with its own local core supply net (`+1V1`), C1603 to C1608,
R1603 and C1606 for `VREG_AVDD`, 100 nF capacitors C1609 to C1619 on the
3.3 V supply pins, 12 MHz crystal Y1601 with C1601, C1602 and R1601, the
firmware in the internal flash, and no buttons or USB.

### Pins

| GPIO | Pin | Net | 6526 pin | Function |
| ---- | --- | --- | -------- | -------- |
| GPIO0 to GPIO7 | 77 to 80, 1 to 4 | `D0` to `D7` | D0 to D7 | Data bus |
| GPIO8 to GPIO11 | 6 to 9 | `A0` to `A3` | RS0 to RS3 | Register select |
| GPIO12 | 11 | `CNT` | CNT | User port pin 6 |
| GPIO13 | 12 | `SP` | SP | User port pin 7 |
| GPIO14 | 13 | `~PC` | ~PC | User port pin 8 |
| GPIO15 | 14 | `PA2` | PA2 | User port pin M |
| GPIO16 | 16 | `~VA14` | PA0 | VIC-II bank select |
| GPIO17 | 17 | `~VA15` | PA1 | VIC-II bank select |
| GPIO21 | 21 | `~RESET` | ~RES | Reset input |
| GPIO22 | 22 | `~NMI` (sheet pin `~IRQ`) | ~IRQ | Interrupt to the 6510's NMI, open drain |
| GPIO23 | 23 | `~FLAG` | ~FLAG | User port pin B |
| GPIO24 | 25 | `PHI2` | PH2 | 6510 clock |
| GPIO25 | 26 | `R/~W` | R/~W | 6510 read/write |
| GPIO26 | 27 | `~CIA2` (sheet pin `~SELECT`) | ~CS | Chip select from the CPLD (U301) |
| GPIO27 | 28 | `PB0` | PB0 | User port pin C |
| GPIO28 to GPIO32 | 36 to 40 | `PB1` to `PB5` | PB1 to PB5 | User port pins D, E, F, H and J |
| GPIO33, GPIO34 | 42, 43 | `PB6`, `PB7` | PB6, PB7 | User port pins K and L |
| GPIO35 | 44 | `DATA` through R1613 | PA7 | Serial bus DATA in |
| GPIO36 | 45 | `DATA_OUT` | PA5 | Serial bus DATA out, through U1602 |
| GPIO37 | 46 | `CLK` through R1614 | PA6 | Serial bus CLK in |
| GPIO38 | 47 | `CLK_OUT` | PA4 | Serial bus CLK out, through U1602 |
| GPIO39 | 48 | `ATN_OUT` | PA3 | Serial bus ATN out, through U1602 |
| GPIO40 | 49 | `SDA` | | I2C0 data, link to the supervisor |
| GPIO41 | 52 | `SCL` | | I2C0 clock, link to the supervisor |

GPIO18 to GPIO20, GPIO42 to GPIO47 and the USB port are not used. PB0 to PB7
are on eight consecutive GPIOs, so one PIO program can read and drive the
whole port, for example for parallel cables to disk drives.

### Serial port

J1601 is the 6-pin DIN socket of the original board (same part and
footprint as CN4) with the original pinout:

| Pin | Signal | TVS | Then |
| --- | ------ | --- | ---- |
| 1 | SRQ (`CASS_RD`) | U1607 | `~FLAG` of CIA 1 (U1501 GPIO23), pull-up R1534 on the CIA 1 sheet |
| 2 | Ground | | |
| 3 | ATN | U1607 | FB1601, then the ATN node |
| 4 | CLK | U1607 | FB1602, then the CLK node |
| 5 | DATA | U1607 | FB1603, then the DATA node |
| 6 | `~RESET` | D1602 | The shared reset line |

Each of ATN, CLK and DATA has a node with a 1k pull-up to +5V (R1608, R1609,
R1607) and an output of U1602 (SN74LVC06A), an inverting open-drain buffer:

- **Why a buffer:** every device on the serial bus has its own pull-up of
  about 1k, so a line that is pulled low sinks about 5 mA per device. That
  is more than an RP2350 pin can sink once two drives are connected. U1602
  sinks 24 mA at 0.55 V. It runs from `+3V3`, its outputs tolerate 5.5 V,
  and it does not feed `+5V` while the rail is off.
- **Polarity:** a high GPIO pulls the bus line low, the same as the 7406 on
  the original board.
- **State before the firmware runs:** R1610 to R1612 (10k) pull the buffer
  inputs low, so all three lines are released while U1601 is in reset, not
  programmed or being programmed.
- **Reading:** GPIO37 and GPIO35 read CLK and DATA through R1614 and R1613
  (100 Ohm), which limit the current into the pins. ATN is not read back, as
  on the original board.
- **Pull-ups:** the 1k pull-ups are the value of the original board and of
  the drives. Higher values would slow down the rising edges, which fast
  loaders depend on.
- **Supervisor as a drive (#98):** the supervisor (U1301) connects to the
  bus as an extra device, so that its firmware can act as a disk drive with
  images from the SD card (#99). Real drives on J1601 keep working next to
  it.
  - **Reading:** GPIO8, GPIO9 and GPIO10 read ATN, CLK and DATA at the bus
    nodes through R1335, R1336 and R1337 (100 Ohm, on the supervisor sheet).
    Unlike U1601, the supervisor reads ATN: a drive has to answer it within
    about 1 ms.
  - **Driving:** a drive pulls CLK and DATA low and never drives ATN. GPIO11
    drives gate E of U1602 (input pin 11, output pin 10 on CLK) and GPIO12
    drives gate F (input pin 13, output pin 12 on DATA), wired-OR with U1601
    and the drives. R1647 and R1646 (10k) pull the gate inputs low.
  - **PIO window:** GPIO8 to GPIO12 are five consecutive pins in one PIO
    window, for the cycle-exact timing of fast loaders.
- **Erratum E9:** after a reset, an RP2350 pin that was driven high can stay
  at about 2 V, which a 10k pull-down does not always override; Raspberry
  Pi recommends at most 8.2k. On the inputs of U1602 that is close to its
  2.0 V high level, so a latched pin could hold CLK, DATA or ATN low and
  stop the bus. The firmware of U1601 and of the supervisor therefore drives
  its U1602 inputs (U1601: GPIO36, GPIO38, GPIO39; supervisor: GPIO11,
  GPIO12) low as the very first action after a reset. The pull-downs
  R1610 to R1612, R1646 and R1647 stay at 10k; if the bus turns out to hang
  after resets on the prototype, change them to 4.7k.
- **Unused gate:** gate D of U1602 has its input (pin 9) on ground and its
  output (pin 8) unconnected. It is reserved for the virtual Datasette
  (#100), which needs open-drain drivers on the cassette lines.

### User port

J1602 is the user port edge connector with the original pinout. Pins 1, 12,
A and N are ground, pin 2 is +5V with C1621, and pins 10 and 11 are not
connected (#37). Every signal line has a TVS diode at the connector, then a
100 Ohm series resistor, then a 4.7k pull-up to +5V at the GPIO. The TVS
arrays are TPD4E05U06DQAR (four channels each):

| Pin | Signal | TVS | Series | Pull-up | Goes to |
| --- | ------ | --- | ------ | ------- | ------- |
| 3 | `~RESET` | D1601 | | R201 | The shared reset line |
| 4 | `CNT1` | U1604 | R1631 | R1532 | U1501 GPIO12 (CIA 1) |
| 5 | `SP1` | U1604 | R1632 | R1531 | U1501 GPIO13 (CIA 1) |
| 6 | `CNT2` | U1604 | R1633 | R1623 | GPIO12 |
| 7 | `SP2` | U1604 | R1634 | R1624 | GPIO13 |
| 8 | `~PC2` | U1605 | R1635 | R1625 | GPIO14 |
| 9 | ATN | U1605 | | R1608 | The serial bus ATN node |
| B | `~FLAG2` | U1605 | R1645 | R1628 | GPIO23 |
| C to F | PB0 to PB3 | U1606 | R1637 to R1640 | R1615 to R1618 | GPIO27 to GPIO30 |
| H to L | PB4 to PB7 | U1603 | R1641 to R1644 | R1619 to R1622 | GPIO31 to GPIO34 |
| M | PA2 | U1605 | R1636 | R1626 | GPIO15 |

The nets between the connector and the series resistors start with `UC_`,
those between the serial port and the ferrite beads with `SC_`. `CNT1` and
`SP1` reach the CIA 1 sheet through the `CIA1_CNT` and `CIA1_SP` sheet pins.

### Level shifting

As for CIA 1, all 5 V signals are on GPIO0 to GPIO39, which are 5 V tolerant
while `+3V3` is up, and only the data lines are driven high, with 3.3 V. All
other outputs are open drain against pull-ups to +5V:

| Signal | Pull-up | Why |
| ------ | ------- | --- |
| `~NMI` | R1627 (4.7k) | Shared with RESTORE (U1501 GPIO27) and the expansion port |
| `~VA14`, `~VA15` | R1629, R1630 (4.7k) | Select VIC-II bank 0 until the firmware runs, as a 6526 does after reset |
| ATN, CLK, DATA | R1608, R1609, R1607 (1k) | Serial bus, driven through U1602 |
| User port lines | R1615 to R1626, R1628 (4.7k) | Devices on the user port drive them too |

The firmware must never drive these lines high. `~VA14` and `~VA15` only go
to inputs (U502, the CPLD and U1401), but are open drain as well, so that they
do not drive 3.3 V against the 5 V pull-ups. Their rising edge takes about
0.1 us, short compared with a bus cycle of about 1 us.

### Programming and start-up

The supervisor (U1301) programs and controls the controller, the same way
as the other two controllers:

- **Reset:** the supervisor's GPIO30 drives `RUN` through R1602 (1k). R1604
  (10k) pulls `RUN` to ground, so the controller stays in reset until the
  supervisor drives it high.
- **Programming:** the supervisor's GPIO28 and GPIO29 drive SWCLK and SWDIO
  through R1605 and R1606 (33 Ohm).
- **Link:** I2C0 on GPIO40 and GPIO41, shared with U1401 and U1501, so it
  needs its own I2C address.

### Power and back-feeding

U1601 and U1602 are on `+3V3` and are powered while the C64 chips are not.
The same rules apply as for CIA 1 (see
[Power and back-feeding](#power-and-back-feeding-1)): keep all pins that
lead to the C64 as inputs until the rails are up, and never turn on their
internal pull-ups. The pull-downs R1610 to R1612 keep the serial bus
released in that time.

A drive that is switched on while the c64-mini is unpowered pulls its bus
lines to 5 V through its own pull-ups. U1602's outputs and the 100 Ohm
resistors in front of GPIO35 and GPIO37 limit what reaches U1601 then.

### Firmware

The firmware is not part of this issue. These requirements follow from the
circuit, in addition to the bus and 6526 model requirements of CIA 1 (see
[Firmware](#firmware)), with `~CIA2` as chip select:

- **Interrupt:** the 6526's interrupt output drives `~NMI`, which the 6510
  triggers on the falling edge. Pull `~NMI` low when an enabled interrupt
  occurs and release it when the interrupt control register is read, at the
  same cycle as a 6526.
- **VIC-II bank:** pull `~VA14` (PA0) and `~VA15` (PA1) low when the port bit
  is an output set to low, and release them otherwise. Apply writes at the
  falling edge of `PHI2`, so that bank switches in the middle of a line
  happen at the same cycle as on the original board.
- **Serial bus outputs:** on the original board, a PA3, PA4 or PA5 pin that
  is high, either as an output set to high or as an input held high by the
  6526's internal pull-up, pulls ATN, CLK or DATA low through the 7406. Set
  GPIO39, GPIO38 or GPIO36 high in exactly these cases. After reset all
  three port bits are inputs, so ATN, CLK and DATA are held low until the
  KERNAL sets the data direction register.
- **Serial bus inputs:** PA6 and PA7 read CLK and DATA without inversion. If
  software makes PA6 or PA7 an output set to low, the original port pulled
  the bus line low directly; do the same through GPIO38 or GPIO36.
- **User port:** pull PB0 to PB7 and PA2 low when the port bit is an output
  set to low, and release them otherwise. PB6 and PB7 carry the timer
  outputs when bit 1 of CRA or CRB is set. Pull `~PC` low for one cycle
  after each read or write of port B. A falling edge on `~FLAG` sets bit 4
  of the interrupt control register.
- **Time of day:** count 50 Hz or 60 Hz from the crystal, as selected by bit
  7 of CRA.
- **Pins:** turn off the internal pull-downs of the bus pins (RP2350
  erratum E9). The bus pins are in the PIO window from GPIO0; port B and the
  serial bus pins are in the window from GPIO16.

## VIC-II

The `VIC-II, HF Module Replacement and AV out` sheet (formerly `HF Module
Replacement and AV connectors`, now `vic_ii_hf_av_out.kicad_sch`) holds the
VIC-II with the parts that only serve it, next to the video output circuit
and the audio and video connectors (#74). The VIC-II is now the 8562.

| Removed | Original part | Now |
| ------- | ------------- | --- |
| U19 | 6567 VIC-II | U504 (8562) |
| U13, U25 | 74LS257 address multiplexers | U505, U506 (74HCT257) |
| U26 | 74LS373 address latch | U508 (74LS373) |
| U14 | 74LS258 multiplexer for VA6, VA7 and the bank | U502 (74F258) |
| U16 | 4066 colour data switch | U503 (CD4066B), then removed (#81): U1401 drives both sides |
| FB23 | Ferrite bead in `PHI0` | FB501 |
| FB15, FB14 | Ferrite beads in luma and chroma | FB502, FB503 |
| FB12, C54 | VDD (pin 13) filter on `+12V` | FB504, C510 on `CAN+5V` |
| C53, C94 | `+12V` decoupling | C512, C513 on `CAN+5V` |
| FB22, C60, C101 | VCC (pin 40) filter | FB505, C509, C511 |
| C36 | 20 pF load on the dot clock | C518 |
| C29, C30, C32, C44, C45 | Decoupling of U13, U14, U16, U25, U26 | C515, C507, C508 (removed with U503), C516, C517 |
| C203 | 82 pF on `~CAS`, PAL only, not fitted on NTSC | dropped |

These parts of the original circuit stay on the `CPU, Cassette and Expansion Port` sheet:

- **`~AEC`, chip select and bus arbitration:** these moved to the CPLD
  (U301) on the `PLA Replacement` sheet (#83). It inverts `AEC` for the
  multiplexers on this sheet (before: gate B of U8 with R31), decodes `~VIC`
  for $D000 to $D3FF (before: U15, 74LS139), and combines `AEC`, `BA` and
  `~DMA` for the 6510 (before: U27, 7408).
- **Clocks:** the dot and colour clock PLL on the former `250407 Sheet 1`
  (U29 to U32, FB16, FB17) stayed until #76 replaced it with the
  `System Clocks` sheet.

### Circuit

- **VIC-II:** U504, an 8562 in a DIP-40 socket. VCC (pin 40) and VDD
  (pin 13) both come from `CAN+5V`, through FB505 (with C509, C511) and FB504
  (with C510, C512 and C513). Unlike the 6567, the 8562 needs 5 V on pin 13;
  12 V there destroys it.
- **Bus:** `D[0..7]`, A8 to A11 of `A[0..15]`, `R/~W`, `~VIC_SELECT` (`~VIC`
  from U301), `~IRQ` and `BA`. `AEC` (pin 16) is high in 6510 cycles and low
  in VIC-II cycles.
- **Address path:**
  - U505 and U506 (74HCT257) put the 6510's address onto `VA0` to `VA7` in
    6510 cycles (`~OE` on `~AEC`), so that the VIC-II sees which register is
    accessed. `~CAS` selects the low or high address byte.
  - U508 (74LS373) latches `VA0` to `VA7` when `~RAS` falls and drives it
    onto A0 to A7 in VIC-II cycles (`~OE` on `AEC`), for character ROM and
    colour RAM fetches.
  - U502 (74F258) drives `VA6` and `VA7` in VIC-II cycles (`~OE` on
    `AEC`): the VIC-II's own A6 and A7 (`VA6'`, `VA7'`) or the bank bits
    `~VA14`/`~VA15`, selected by `~CAS`.
  - `~RAS` only connects U504 and U508, and `VA0` to `VA3`, `VA6` and `VA7`
    stay on this sheet. `VA4` and `VA5` go to U1401 and the CPLD (U301).
- **Colour data:** D8 to D11 (pins 38 to 35) come straight from U1401
  (`COLORD0` to `COLORD3`, bus `COLORD[0..3]`), which drives them in VIC-II
  cycles. The 4066 U503 that connected them to D0 to D3 in 6510 cycles is
  gone (#81); U1401 now puts colour RAM reads on D0 to D3 itself, see
  [RAM and ROM replacement](#ram-and-rom-replacement).
- **Clocks:** `COLOR_CLOCK_IN` (pin 21) and `DOT_CLOCK_IN` (pin 22, with C518)
  come from the `System Clocks` sheet (#76), see
  [System clocks](#system-clocks). `PHI0` (pin 17) leaves through FB501 to the
  6510 only (`PHI0_IN` on the `CPU, Cassette and Expansion Port` sheet).
- **Video:** luma (pin 15) through FB502 and chroma (pin 14) through FB503 go
  straight to the video output circuit on the same sheet. The luma pull-up
  R512 is 1 kOhm, the value for the 856x, see
  [HF module replacement](#hf-module-replacement).
- **Decoupling:** C507 and C515 to C517 (100 nF), one per logic chip.

### Parts

All parts on the sheet except the VIC-II itself are SMD. The ferrite beads
and C513 use the KiCad symbols `Device:FerriteBead_Small` and `Device:C_Small`;
the logic chips still use the symbols of the original design.

| Ref | Part | LCSC | Package |
| --- | ---- | ---- | ------- |
| C507, C509 to C512, C515 to C517 | 100 nF 16 V X7R | C1525 | 0402 |
| C513 | 10 uF 25 V X5R | C96446 | 0603 |
| C518 | 20 pF 50 V C0G | C1554 | 0402 |
| R514 | 1k (`PHI0` tap for the supervisor, #49) | C21190 | 0603 |
| FB501 to FB505 | 120 Ohm at 100 MHz, 2 A | C14709 | 0603 |
| U502 | SN74F258D | C1499704 | SOIC-16 |
| U505, U506 | SN74HCT257DR | C2872319 | SOIC-16 |
| U508 | SN74LS373DWR | C2867545 | SOIC-20 (7.5 mm) |
| U504 | 8562 in a DIP-40 socket | C2874018 (socket) | DIP-40 |

- **Logic families:** the 74LS257 and 74LS258 are not available in SOIC, so
  U505, U506 and U502 use other families with the same pinout and TTL input
  levels. The 6510 and the VIC-II drive TTL levels and U1601 drives `~VA14`
  and `~VA15` at 3.3 V, so 74HC parts, which need 3.5 V for a high input, are
  not an option.
  - U505 and U506 are 74HCT257. They switch about 20 ns slower than the
    74LS257, which does not matter here: they are only on in 6510 cycles,
    when they carry the register address to the VIC-II, which only uses it
    later in the cycle.
  - U502 is a 74F258, a bipolar part like the 74LS258, but faster. It is the
    only 258 in SOIC that is stocked, and only in small numbers.
  - U508 stays a 74LS373.
- **Ferrite beads:** the original beads are about 50 Ohm at 100 MHz. The
  120 Ohm beads are the nearest basic part. They are lower than the 600 Ohm
  beads on the SID sheet on purpose: FB502 and FB503 carry luma and chroma,
  and the larger inductance of a 600 Ohm bead would form a resonance with
  C505 (330 pF) near the video band. The 2 A rating covers the VIC-II's
  supply current through FB505.
- **C513:** a 10 uF ceramic capacitor replaces the electrolytic capacitor on
  `CAN+5V`. It keeps roughly half its capacitance at 5 V, which is enough
  next to the bulk capacitors of the `CAN+5V` regulator.

## SID and audio output

The `SID and internal audio output` sheet holds the SID, its audio output
stage and the paddle switch (#73). It replaces the SID circuit of
the former `250407 Sheet 2` and the 4066 of the former `250407 Sheet 1`,
with the SID changed from the 6581 to the 8580 and the other parts changed
to SMD.

| Removed | Original part | Now |
| ------- | ------------- | --- |
| U18 | 6581 SID | U1701 (8580) |
| C10, C11 | 470 pF filter capacitors | C1704, C1705 (22 nF C0G) |
| FB11, C51 | VDD filter (`+12V`) | FB1701, C1701 (`+9V`) |
| FB10, C50 | VCC filter | FB1702, C1702 |
| C52, C62 | `+12V` decoupling | C1707, C1710 (`+9V`) |
| C49, C34 | `+5V` decoupling | C1708, C1711 |
| R6 | Load on AUDIO | R1701 |
| FB13, R7, C37 | Feed to the audio buffer | FB1703, R1702, C1709 |
| Q8, R38, C205 | Audio buffer (2SC1815) | Q1701 (MMBT3904), R1703, C1712 |
| C12, C13 | Audio coupling capacitors | C1713, C1714 |
| C48, C93 | POTX and POTY timing capacitors | C1703, C1706 |
| U28, C47 | Paddle switch 4066 and its decoupling capacitor | U1702 (CD4066B), C1715 |
| R301, R302 | Pull-ups on `CIA1_PA6`, `CIA1_PA7` | R1704, R1705 |

The CPLD (U301) on the `PLA Replacement` sheet decodes `~SID` for $D400 to
$D7FF, as U15 (74LS139) did before (#83); it reaches U1701 through the
`~SELECT` sheet pin.

### Circuit

- **SID:** U1701, an 8580 in a DIP-28 socket, on the bus through `D[0..7]`,
  `A[0..15]` (A0 to A4), `R/~W`, `PHI2`, `~RESET` and `~SELECT`.
- **Supplies:** VDD (pin 28) from `+9V` through FB1701 with C1701, VCC
  (pin 25) from `+5V` through FB1702 with C1702. C1707 (100 nF) and C1710
  (22 uF) decouple `+9V` on this sheet, C1708 (100 nF) and C1711 (10 uF)
  `+5V`. See [`+9V`](#9v) for the regulator.
- **Filter capacitors:** C1704 and C1705, 22 nF C0G (the 8580 value). C0G
  keeps the filter's cutoff frequency independent of the signal voltage and
  temperature; X7R would distort and drift.
- **Audio output:** AUDIO (pin 27) has R1701 (1k) to ground as its load. FB1703
  and R1702 (10k) feed the emitter follower Q1701 (MMBT3904, collector on
  `+9V`), with C1709 (1 nF) forming a low-pass filter at about 16 kHz with
  R1702. R1703 (1k) is the emitter resistor and C1712 (220 pF) shunts RF to
  ground. C1714 (22 uF) blocks the DC level of the emitter, so `AUDIO_OUT`
  carries only the audio. With a 10k amplifier input, the low-frequency limit
  is below 2 Hz.
- **Audio input:** C1713 (22 uF) couples `AUDIO_IN` from the red RCA jack to
  EXT IN (pin 26), so the SID's internal bias on that pin is not disturbed.
  There is no resistor from EXT IN to ground, so samples played through the
  volume register stay quiet, see [The SID is an 8580](#the-sid-is-an-8580).
- **Paddles:** U1702 (CD4066B) connects the POT lines of control port 1
  (`POTAX`, `POTAY`, from J1503) or control port 2 (`POTBX`, `POTBY`, from
  J1502) to POTX and POTY (pins 24 and 23). `CIA1_PA6` and `CIA1_PA7` from the
  CIA 1 controller select the port, with R1704 and R1705 (10k) as pull-ups to
  +5V, because the 4066B needs at least 3.5 V on its control inputs. C1703 and
  C1706 (1.8 nF C0G) are the timing capacitors the SID measures; C0G keeps the
  paddle readings stable. C1715 decouples U1702.

### Parts

| Ref | Part | LCSC | Package |
| --- | ---- | ---- | ------- |
| C1701, C1707 | 100 nF 50 V X7R | C307331 | 0402 |
| C1702, C1708, C1715 | 100 nF 16 V X7R | C1525 | 0402 |
| C1703, C1706 | 1.8 nF 50 V C0G | C385028 | 0402 |
| C1704, C1705 | 22 nF 50 V C0G | C77069 | 0805 |
| C1709 | 1 nF 50 V C0G | C76947 | 0402 |
| C1710, C1713, C1714 | 22 uF 25 V X5R | C45783 | 0805 |
| C1711 | 10 uF 25 V X5R | C96446 | 0603 |
| C1712 | 220 pF 50 V X7R | C1530 | 0402 |
| R1701, R1703 | 1k 1 % | C11702 | 0402 |
| R1702, R1704, R1705 | 10k 1 % | C25744 | 0402 |
| FB1701 to FB1703 | 600 Ohm at 100 MHz, 200 mA | C1002 | 0603 |
| Q1701 | MMBT3904 | C20526 | SOT-23 |
| U1702 | CD4066BM96 | C54755 | SOIC-14 |
| U1701 | 8580 in a DIP-28 socket | C2874016 (socket) | DIP-28 |

- **Voltage ratings:** the capacitors on `+9V` are rated 25 V or 50 V, so
  they keep most of their capacitance at 9 V. The 22 uF coupling capacitors
  still give about 10 uF at the DC level of a few volts they see.
- **Ceramic coupling capacitors:** C1713 and C1714 replace electrolytic
  capacitors. With 22 uF only a few percent of the signal appears across
  them, even at 50 Hz, so the voltage dependence of X5R adds far less
  distortion than the SID itself has.
- **R1701:** carries a few volts DC, about 20 to 25 mW, within the 62.5 mW
  of an 0402 resistor.

## Audio and video connectors

The audio and video connectors are on the `VIC-II, HF Module Replacement and AV out`
sheet, next to the video output circuit that drives them (#24).
They replace the original 8-pin DIN A/V connector CN5.

| Connector | Contact               | Signal                                         |
| --------- | --------------------- | ---------------------------------------------- |
| J501      | Pin 1, pin 2          | Ground (luma and chroma)                       |
| J501      | Pin 3                 | Luma (`LUMA_OUT`)                              |
| J501      | Pin 4                 | Chroma (`CHROMA_OUT`)                          |
| J501      | Shield (`SH`)         | Ground                                         |
| J502      | Red jack (pin 1)      | Audio in to the SID `EXT IN` (`AUDIO_IN`)      |
| J502      | White jack (pin 3)    | Audio out from the SID buffer Q1701 (`AUDIO_OUT`) |
| J502      | Yellow jack (pin 5)   | Composite video (`COMP_OUT`)                   |
| J502      | Shells (pins 2, 4, 6) | Ground                                         |

- **J501:** CUI (Same Sky) MD-40SM 4-pin mini-DIN socket with the standard
  S-Video pinout.
- **J502:** CAX-AV-314-RWY-PHP block of three RCA jacks. The SID is mono, so
  one jack is the audio output and the spare one is the audio input that
  CN5 pin 5 used to carry.
- **Audio input protection:** the red jack is the only input on these
  connectors. A bidirectional 5 V TVS diode (D502, H5VL10BH) to ground at the
  jack and a 150 Ohm series resistor (R513) in front of the coupling
  capacitor C1713 protect the SID against ESD. There is no DC on the jack side
  of C1713, so a line-level signal stays well below the 5 V standoff voltage
  of D502.
- **Output protection (#72):** each output has a bidirectional H5VL10BH
  (5 V standoff, 8 V breakdown, 5.5 pF) to ground at its jack: D503 on
  `AUDIO_OUT`, D504 on `COMP_OUT`, D505 on `CHROMA_OUT` and D506 on
  `LUMA_OUT`.
  - **Video:** composite and luma stay between 0 V and about 2 V (about 1 V
    into a 75 Ohm load), and chroma swings about ±0.3 V around its level.
    5.5 pF is negligible at video frequencies with the low impedances of
    these lines. A larger TVS such as the SMF5.0CA, with hundreds of pF,
    would have softened the picture and the chroma.
  - **Audio:** `AUDIO_OUT` is AC-coupled through C1714 and swings about
    ±1 V. R502 (100k) to ground gives it a DC level of 0 V, so the coupling
    capacitor's charge drains away and plugging in an amplifier does not
    pop. At power-up the jack side briefly follows the emitter of Q1701,
    which rises to about 4 V, still below the diode's 5 V standoff.
- **Sheet pins:** `AUDIO_IN` and `AUDIO_OUT` connect to the
  `SID and internal audio output` sheet through the root sheet (#73).
  `LUMA_OUT`, `CHROMA_OUT` and `COMP_OUT` stay on the `VIC-II, HF Module Replacement and AV out`
  sheet.
- **Signal levels:** the video output circuit is unchanged from the HF module
  replacement (#8), see [HF module replacement](#hf-module-replacement).

## CPU, cassette and expansion port

The `CPU, Cassette and Expansion Port` sheet
(`cpu_cassette_expansion.kicad_sch`) holds what is left of the original
250407 circuit: the CPU, the cassette port and the expansion port. It used
to be the two sheets of the upstream design, `250407 Sheet 1` and `250407
Sheet 2`, which #84 merged into one flat sheet called `250407 Design`. #107
renamed it, renumbered its parts into the 200 range, replaced the
through-hole passives with SMD parts and tidied up the drawing. It keeps the
title block of the upstream design with its credits (C64 250407 PCB Replica
by Bwack, with help from Der Alte Bastler).

| Group | Parts |
| ----- | ----- |
| CPU | U201 (8500, or a 6510, in a DIP-40 socket), FB205 in `PHI2`, R207 to R209 (pull-ups on the port bits `~LORAM`, `~HIRAM`, `~CHAREN`), R204 (pull-up on `R/~W`), R215 (`PHI2` tap for the supervisor's chip check, #49) |
| Bus pull-ups | R202 and R203 (`~EXROM`, `~GAME`), R205 (`~DMA`), R210 to R213 (A15 to A12). `~IRQ` is pulled up by R1533 on the CIA 1 sheet. `RDY`, `CAEC`, `~AEC` and all chip selects come from the CPLD (U301, #83), see [PLA replacement](#pla-replacement) |
| Reset | R201 (1k pull-up on `~RESET`). The supervisor makes the power-on reset and the soft reset through Q1301 (#70, #89) |
| Cassette port | J202 (edge connector) with FB202 in its +5V (`CASS_5V`, with C202), FB203 in the motor supply (`CASS_POWER`), FB204 in `CASS_WRT`, and R206 (pull-up on `CASS_SENSE`). ESD protection at the connector (#72): U202 (TPD4E05U06DQA) on `CASS_SENSE`, `CASS_WRT'` and `CASS_RD`, D201 (H5VL10BH) on `CASS_5V` |
| Expansion port | J201 with FB201 in `~RESET` |
| Decoupling | C204 (100 nF), C201 and C203 (10 uF) on `+5V` |

- **Connections:** the sheet connects to the other sheets through
  hierarchical labels and sheet pins only.
- **Expansion port:** J201 gets `~RESET` through FB201 (pin C), the dot
  clock `DOT_CLK` from the `System Clocks` sheet through the `DOT_CLOCK_IN`
  sheet pin (pin 6), and `~ROML`, `~ROMH`, `~I/O1` and `~I/O2` from the CPLD
  (U301).
- **Cassette port:** read (`CASS_RD`, pin D-4) goes to CIA 1's `~FLAG`
  (U1501) and the serial bus SRQ, as on the original board; sense (P4),
  write (P3) and motor (P5) go to the 6510's port.
- **Virtual Datasette (#100):** the supervisor also connects to all four
  cassette lines, so that its firmware can play `.tap` images from the SD
  card (#101). A real Datasette on J202 keeps working next to it.

  | Line | Supervisor | Series resistor | Use |
  | ---- | ---------- | --------------- | --- |
  | `CASS_RD` | GPIO24 | R1338, 220 Ohm | Pull low for each tape pulse (open drain in firmware) |
  | `CASS_SENSE` | GPIO25 | R1339, 220 Ohm | Pull low to make PLAY look pressed (open drain in firmware) |
  | `CASS_MOTOR` | GPIO26 | R1340, 1k | Read: low means motor on, pause playback when high |
  | `CASS_WRT` | GPIO27 | R1341, 1k | Read: record what the C64 writes (optional) |

  - **Direct GPIOs instead of buffers:** unlike the serial bus, these lines
    have a single pull-up each (R1534 4.7k on `CASS_RD`, R206 3.3k on
    `CASS_SENSE`), so pulling them low takes only 1 to 1.5 mA, well within
    an RP2350 pin.
  - **Series resistors:** 220 Ohm on the outputs keeps the low level at
    about 0.22 V (`CASS_RD`) and 0.31 V (`CASS_SENSE`), well below the
    0.8 V that CIA 1's `~FLAG` and the 6510's P4 need. With 1k, the lows
    would have been 0.88 V and 1.16 V. 220 Ohm also limits the current to
    about 15 mA if the firmware ever drove a pin high while a real
    Datasette pulls the line low. The firmware must only pull these pins
    low or release them, never drive them high.
  - **Motor:** P5 is read through R1340. R1104 and R1105 were changed for
    this, see [Cassette motor supply](#cassette-motor-supply).
  - **Fail-safe:** in reset the GPIOs are inputs; R1534 and R206 keep the
    read and sense lines high, so the C64 sees no tape and no key. RP2350
    erratum E9 does not matter here, because these strong external pull-ups
    override a latched pin.
  - **Real Datasette:** both share the lines. Sense is pulled low by either
    one without conflict. The 1530's read output is believed to be open
    collector (the C64 provides the pull-up); this still has to be
    confirmed. The firmware only switches the virtual Datasette on while no
    real one is playing (#101).
  - **PIO window:** GPIO24 to GPIO27 are consecutive.
- **Sockets:** note B on the sheet lists the chips that are fitted in
  sockets: U201 (8500, or a 6510), and U504 and U1701 on their own sheets.
- **Removed so far:** #83 replaced U17, U15, U27 and gate B of U8 with a
  CPLD, and #91 removed U20 and the rest of U8 (RESTORE now comes from
  U1501). The 6510 (U201) is the only chip left on the sheet.
- **Symbols:** the passives use KiCad's standard symbols. The 6510, the
  expansion port, the cassette port and the ground symbols still use the
  symbols of the original design, and the connectors keep their original
  footprints.

### References

The original references still appear in the C64 schematics, service manuals
and the upstream replica:

| Original | Now | Part | Change |
| -------- | --- | ---- | ------ |
| U7 | U201 | 6510 in a DIP-40 socket | Now an 8500 (#75); a 6510 also works |
| CN6 | J201 | Expansion port | |
| CN3 | J202 | Cassette port (edge connector) | |
| CN10 | (J203) | Power LED connector | Removed (#54), see [Status LEDs](#status-leds) |
| FB4 | FB201 | Ferrite bead in `~RESET` to the expansion port | 100 Ohm 0805 |
| FB3 | FB202 | Ferrite bead in the cassette port's +5V | 120 Ohm 2 A 0603 |
| FB2 | FB203 | Ferrite bead in the cassette motor supply | 120 Ohm 2 A 0603 |
| FB1 | FB204 | Ferrite bead in `CASS_WRT` | 100 Ohm 0805 |
| FB5 | FB205 | Ferrite bead in `PHI2` | 100 Ohm 0805 |
| FB20 | (FB206) | Ferrite bead in the LED's ground | Removed (#54) |
| FB21 | (FB207) | Ferrite bead in the LED's supply | Removed (#54) |
| C17 | C201 | 10 uF on `+5V` | MLCC 0805 |
| C1 | C202 | 100 nF on the cassette port's +5V | 0402 |
| C8 | C203 | 10 uF on `+5V` | MLCC 0805 |
| C9 | C204 | 100 nF on `+5V` | 0402 |
| R51 | R204 | 1.5k pull-up on `R/~W` | 0402 |
| RP4 pin 10 | R205 | 3k3 pull-up on `~DMA` | 0402 |
| R1 | R206 | 3k3 pull-up on `CASS_SENSE` | 0402 |
| R45, R44, R43 | R207, R208, R209 | 3k3 pull-ups on `~LORAM`, `~HIRAM`, `~CHAREN` | 0402 |
| RP4 pins 4, 5, 6, 8 | R210 to R213 | 3k3 pull-ups on A15, A14, A13, A12 | 0402 |
| RP4 pin 3 | (none) | 3k3 pull-up on `~IRQ` | R1533 on the CIA 1 sheet is now 3k3 instead |
| R39 | (R214) | 390 Ohm in the power LED's supply | Removed (#54) |

R201 to R203 kept their numbers. The references C203 and C204 were used on
the original board for the PAL-only 82 pF capacitor on `~CAS` and an
unfitted capacitor near the DRAMs; both were dropped earlier, and the tables
of the VIC-II and RAM and ROM sections still use their original numbers.

### Parts

| Ref | Part | LCSC | Package |
| --- | ---- | ---- | ------- |
| U201 | 8500 (or 6510) in a DIP-40 socket | C2874018 (socket) | DIP-40 |
| J201 | Expansion port | C5173343 | THT |
| J202 | Cassette port (edge fingers on the PCB) | | |
| C201, C203 | 10 uF 25 V X5R | C15850 | 0805 |
| C202, C204 | 100 nF | C1525 | 0402 |
| FB201, FB204, FB205 | 100 Ohm at 100 MHz, 800 mA | C1015 | 0805 |
| FB202, FB203 | 120 Ohm at 100 MHz, 2 A | C14709 | 0603 |
| R201 | 1k | C11702 | 0402 |
| R202, R203, R205 to R213 | 3k3 | C25890 | 0402 |
| R204 | 1.5k | C25867 | 0402 |
| R215 | 1k (`PHI2` tap for the supervisor, #49) | C11702 | 0402 |
| U202 | TPD4E05U06DQA (cassette port ESD) | C138714 | USON-10 |
| D201 | H5VL10BH (`CASS_5V` ESD) | C42413893 | DFN1006-2L on an 0402 footprint |

- **Ferrite beads:** the beads in the cassette port's +5V and motor supply
  use the 2 A part, because the Datasette motor draws a few hundred mA and
  more when it starts, and tape adapters also run from the port's +5V. The
  other beads carry signals or the LED current.
- **10 uF capacitors:** MLCCs instead of electrolytics. The 0805 size keeps
  more of its capacitance at 5 V than an 0603.

## ESD protection

Every connector that a user plugs into gets ESD protection at the connector
(#72), before any series resistor, ferrite bead or pull-up. Two parts are
used throughout:

- **TPD4E05U06DQA** (C138714): four unidirectional TVS channels to ground,
  5.5 V working voltage, 0.5 pF, no supply pin (so it cannot back-feed a
  rail). Used where it covers at least two signal lines of the same
  connector. Unused channels stay unconnected.
- **H5VL10BH** (C42413893): a single bidirectional TVS, 5 V standoff, 8 V
  breakdown, 5.5 pF, in DFN1006-2L on an 0402 footprint. Used for single
  lines, for signals that swing below ground (audio, chroma), and for 5 V
  supply pins. Supply pins never share an array with signals.

| Connector | Signal lines | Supply pin |
| --------- | ------------ | ---------- |
| J601, 12 V input | | D602 (SMAJ13A) |
| J1301, supervisor USB-C | U1304: D+, D-, CC1, CC2 | D1305 (`VBUS`) |
| J1302, SD card | U1302, U1303: all card lines | U1303 (card `+3V3`) |
| J1501, USB keyboard | D1504 (D+), D1505 (D-) | D1503 (`VBUS`) |
| J1502, control port 2 | U1505: pins 1 to 4; U1506: fire, POT X, POT Y | D1501 (pin 7, shared) |
| J1503, control port 1 | U1504: pins 1 to 4; U1507: fire, POT X, POT Y | D1501 (pin 7, shared) |
| J1601, serial port | U1607: SRQ, ATN, CLK, DATA; D1602: `~RESET` | |
| J1602, user port | U1603 to U1606: all signal pins; D1601: `~RESET` | not protected (on the main `+5V` rail) |
| J202, cassette port | U202: sense, write, read | D201 (`CASS_5V`) |
| J501, S-Video | D506 (luma), D505 (chroma) | |
| J502, RCA jacks | D502 (audio in), D503 (audio out), D504 (composite) | |

Not protected, on purpose:
- **Expansion port (J201):** it would take about eleven arrays on the bus
  lines, right at the bus timing, and the original C64 had none either.
  Cartridges must never be plugged in or pulled out while the board is
  powered.
- **Cassette motor supply (`CASS_POWER`, J202 pin C):** it carries up to
  6.24 V, above the array's working voltage, and no basic or preferred TVS
  with a 6.5 V standoff was found. The Datasette motor on the other end and
  the converter's output capacitors (44 uF) absorb a discharge.
- **Internal connectors** (LED module J1303 and the debug headers of #112)
  are not touched by users. The case switch J602 and the reset button J1304
  are on cables inside the case and reach the board only through series
  resistors (R607, R1344).

## Licence

Copyright Bwack 2025.
Modifications copyright (c) 2026 Mariella de Jong and contributors.

The original design is licensed under the
[CERN Open Hardware Licence v1.2](../../LICENSES/CERN-OHL-1.2.txt). The
modified design is licensed under the
[CERN Open Hardware Licence Version 2 - Strongly Reciprocal](../../LICENSES/CERN-OHL-S-2.0.txt),
as permitted by section 3.4(e) of CERN-OHL v1.2.

Source Location: <https://github.com/leporidae-ca/c64-mini>

The HF module replacement circuit, on the sheet `vic_ii_hf_av_out.kicad_sch`
(formerly `hf_module_replacement.kicad_sch`), is derived from the
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

The video circuit on the `VIC-II, HF Module Replacement and AV out` sheet replaces
the original RF modulator (M1) with composite and S-Video (luma/chroma)
outputs, which go to the S-Video socket and the RCA jacks on the same sheet
(see [Audio and video connectors](#audio-and-video-connectors)). It is based on the schematic of the
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
- The VIC-II luma pull-up is a fixed resistor (R512). Upstream selects
  470 Ohm (NMOS 6567/6569) or 1 kOhm (HMOS-II 856x) with a jumper; R512 was
  470 Ohm for the 6567 and is 1 kOhm since the move to the 8562 (#74).
- The pinout selection jumpers for other board revisions are left out.
- The RF modulator's audio input is not used. Audio goes directly to the
  RCA jacks.

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
| 2026-11-02 | Added a reset button connector to the `Supervisor and SD card slot` sheet (#109): J1304 (1x2 pin header, C32713268) for a push button in the case, switching `+3V3` through R1343 (220 Ohm, C25091) to GPIO15 (`RESET_REQ`) through R1344 (1k, C11702), with R1342 (4.7k, C25900) pulling the line down (RP2350 erratum E9). The supervisor makes the reset itself through Q1301 |
| 2026-11-01 | Added the power switch (#109): P-channel MOSFETs Q606 and Q607 (AO3401A, C15127) between the reverse polarity protection and the rest of the board, with R606 and R607 (100k, C25741) as gate divider, C604 (47 nF, C1622) for a slow switch-on, and J602 (1x2 pin header, C32713268) for a switch in the case. Moved the TVS D602 before the power switch and added C605 (220 nF, C21120) there. Renamed the 12 V nets on the `Power Supply` sheet: `12V_IN` became `12V_PROT` (before the power switch) and `12V_MAIN` (after it); `12V_IN_SW` is unchanged |
| 2026-11-01 | Added a backup for `+3V3` from `+5V` to the `3V3 Buck` sheet (#110): LDO U1002 (ME6211C33M5G-N, C82942) from `+5V` with C1014 and C1015 (1 uF, C52923), feeding `+3V3` through Schottky diode D1001 (SS34, C8678), so that `+3V3` never drops while `+5V` is up. Added a note about it to the sheet |
| 2026-11-01 | Connected the supervisor to the cassette port lines for a virtual Datasette (#100): GPIO24 and GPIO25 to `CASS_RD` and `CASS_SENSE` through R1338 and R1339 (220 Ohm, C25091), GPIO26 and GPIO27 to `CASS_MOTOR` and `CASS_WRT` through R1340 and R1341 (1k, C11702). Changed R1104 from 1k to 10k (C25744) and R1105 from 10k to 100k (C25741), so that P5's high level stays readable. Renamed the hierarchical label `~CIA1_FLAG` on the CPU sheet to `~CASS_RD` |
| 2026-11-01 | Connected the supervisor to the serial bus as a virtual drive (#98): GPIO8, GPIO9 and GPIO10 read ATN, CLK and DATA through R1335 to R1337 (100 Ohm, C25076); GPIO11 and GPIO12 drive CLK and DATA through the previously unused gates E and F of U1602, with pull-downs R1647 and R1646 (10k, C25744). New sheet pins between the `Supervisor and SD card slot` and `CIA2, Serial Port and User Port` sheets |
| 2026-11-01 | Reworked the ESD protection (#72). Supervisor: D1301 to D1304 replaced by the array U1304 (TPD4E05U06DQA, C138714) on D+, D-, CC1 and CC2. CIA 1: the single diodes on the control ports replaced by U1504 (port 1, pins 1 to 4), U1505 (port 2, pins 1 to 4), U1506 and U1507 (fire button and both POT lines of port 2 and port 1; the POT lines were unprotected before); the keyboard port's D+ and D- now on D1504 and D1505; D1501 (H5VL10BH, C42413893) added on the control ports' +5V. Cassette port: U202 (TPD4E05U06DQA) on `CASS_SENSE`, `CASS_WRT'` and `CASS_RD`, D201 (H5VL10BH) on `CASS_5V`. AV outputs: D502 changed from SMF5.0CA to H5VL10BH, D503 to D506 (H5VL10BH) added on `AUDIO_OUT`, `COMP_OUT`, `CHROMA_OUT` and `LUMA_OUT`, and R502 (100k, C25741) from `AUDIO_OUT` to ground |
| 2026-11-01 | Changed the CPU to the 8500 (#75): U201's value is now `8500`, its description and note B on the `CPU, Cassette and Expansion Port` sheet say to fit an MOS 8500 and that a 6510 also works. No circuit change: the 8500 has the same pinout, supply, timing and processor port as the 6510. Corrected the reference in the `PHI2_PROBE` note on the same sheet (R215, formerly R214) |
| 2026-11-01 | Added the chip and rail checks for the supervisor (#49): `PGOOD` to GPIO3 through R1330 (1k, C11702); dividers from `CAN+5V` (R1331 4.7k, C25900, and R1332 10k, C25744) to GPIO4 and from `+9V` (R1334 10k and R1333 4.7k) to GPIO5; `PHI0` from the 6510's side of FB501 to GPIO6 through R514 (1k, C21190) on the `VIC-II, HF Module Replacement and AV out` sheet; `PHI2` from the bus side of FB205 to GPIO7 through R215 (1k, C11702) on the `CPU, Cassette and Expansion Port` sheet. New sheet pins `PGOOD`, `CAN+5V`, `PHI0_PROBE` and `PHI2_PROBE` on the `Supervisor and SD card slot` sheet, `PHI0_PROBE` on the VIC-II sheet and `PHI2_PROBE` on the CPU sheet; `+9V` is a global power net |
| 2026-11-01 | Added the status LED connector to the `Supervisor and SD card slot` sheet (#54): J1303 (2×3 pin header, C65114) for the new LED module, with R1325 (220 Ohm, C25091) from `+3V3` for the power LED, R1323 and R1324 (150 Ohm, C22808) from GPIO21 (ready) and GPIO23 (activity), R1326 (220 Ohm, C25091) from GPIO22 (error), and R1327 to R1329 (4.7k, C25900) pulling GPIO21 to GPIO23 down (RP2350 erratum E9). Removed the original power LED connector J203 with R214, FB206 and FB207 from the `CPU, Cassette and Expansion Port` sheet |
| 2026-11-01 | Cleaned up the remaining original circuit (#107): renamed the `250407 Design` sheet to `CPU, Cassette and Expansion Port` (file `cpu_cassette_expansion.kicad_sch`) and renumbered its parts into the 200 range (U7 to U201, CN6, CN3 and CN10 to J201 to J203, FB4, FB3, FB2, FB1, FB5, FB20 and FB21 to FB201 to FB207, C17, C1, C8 and C9 to C201 to C204, R51, R1, R45, R44, R43 and R39 to R204, R206 to R209 and R214). Replaced the through-hole passives with SMD parts: 10 uF MLCCs (C15850) for the electrolytics, 100 nF 0402 (C1525), ferrite beads C1015 and, in the cassette +5V and motor supply, C14709, resistors in 0402 (R214 in 0603). Replaced the resistor array RP4 with R205 (`~DMA`) and R210 to R213 (A12 to A15); its `~IRQ` pull-up is dropped and R1533 on the CIA 1 sheet changed from 4.7k to 3k3 (C25890) instead. Labelled the cassette port's +5V `CASS_5V`, updated note B (U201), and tidied up the drawing |
| 2026-11-01 | Let the CIA 1 controller drive `~NMI` for RESTORE (#91): U1501 GPIO27 now connects directly to `~NMI` through a `~NMI` sheet pin on the `CIA1, Keyboard and Joysticks` sheet, replacing `~RESTORE_NMI`. Removed the RESTORE one-shot from `250407 Design`: U20 (556, both halves) with R33, R35, R41, C23 and C38, U8 (7406, gate C was the last one in use) and the decoupling capacitors C22 and C39 |
| 2026-11-01 | Removed the colour data switch U503 (4066, formerly U16) and its decoupling capacitor C508 from the `VIC-II, HF Module Replacement and AV out` sheet (#81); U1401 now drives the colour RAM nibble on D0 to D3 in 6510 cycles itself. Corrected the bit order of the colour data lines: renamed `COLOR_D0` to `COLOR_D3` to `COLORD0` to `COLORD3` and connected `COLORDn` (U1401 GPIO36 plus n) to the VIC-II's D(8+n), so that it carries colour bit n. The two sheets now connect through a `COLORD[0..3]` bus |
| 2026-11-01 | Added a micro SD card slot to the `Supervisor` sheet and renamed the sheet to `Supervisor and SD card slot` (#95): J1302 (Korean Hroparts Elec TF-01A, C91145) on `+3V3` with C1322 (100 nF, C1525) and C1323, C1324 (4.7 uF, C19666). CLK, CMD and DAT0 to DAT3 go to GPIO34 to GPIO39 (CLK through R1316, 33 Ohm, C25105), card detect to GPIO33. R1317 to R1322 (10k, C25744) pull CMD, DAT0 to DAT3 and card detect up to `+3V3`. TVS arrays U1302 and U1303 (TPD4E05U06DQA, C138714) protect the card lines and the card supply at the socket |
| 2026-11-01 | Added a footprint for the micro SD socket Korean Hroparts Elec TF-01A (LCSC C91145) to the component library in `hardware/components`: `microSD_Hroparts_TF-01A`, for use with KiCad's `Connector:Micro_SD_Card_Det1` symbol (#102) |
| 2026-11-01 | Replaced the PLA (U17) with a CPLD (#83): added a `PLA Replacement` sheet (page 3) to the root sheet with U301 (XC9572XL-10VQG64C, C90569) on `+3V3`, C301 to C304 (100 nF, C1525, one per VCC pin), R301 (4.7k, C25900) from GTS1 (`EN_OUTPUT`) to ground, R302 (10k, C25744) from TMS to `+3V3` and R303 (4.7k, C25900) from `~AEC` to `+5V`. U301 also takes over U15 (74LS139), U27 (7408) and gate B of U8 (7406). Removed U17 and its socket, U15, U27, R31 and the decoupling capacitors C31, C33 and C46 from `250407 Design`; gate B of U8 is no longer placed. Replaced RP4 units F and H with R202 and R203 (3k3 0402, C25890) on `~EXROM` and `~GAME`. Connected the CPLD's JTAG pins to the supervisor's GPIO16 (TDO), GPIO17 (TDI), GPIO18 (TCK) and GPIO19 (TMS), and GTS1 to GPIO20 (`EN_PLA`), through new sheet pins on the `Supervisor` sheet. Wired the PLA's signals and `~DMA`, `CAEC`, `RDY`, `~COLORAM`, `~VIC`, `~SID`, `~CIA1`, `~CIA2`, `~I/O1`, `~I/O2` and `AEC`/`~AEC` to the CPLD through hierarchical labels and sheet pins on `250407 Design` and the root sheet; the VIC-II, RAM and ROM, CIA 1, CIA 2 and SID sheets keep their existing sheet pins. Removed U17 from note B |
| 2026-10-31 | Removed the power-on reset timer from `250407 Design` (#89): half B of U20 (556) no longer has a function, gate D of U8 (7406) is no longer placed, and R34, C24, R50 and C105 are gone; the supervisor now makes the power-on reset through Q1301. Tied off half B of U20: RST B (pin 10) and THR B (pin 12) to ground, TRIG B (pin 8) to `+5V`, OUT B, DIS B and CV B open; VCC and GND of U20 stay on unit B. Replaced the through-hole pull-up R36 (1k) on `~RESET` with R201 (1k 0402, C11702) |
| 2026-10-31 | Added a soft reset of the C64 to the `Supervisor` sheet (#70): GPIO0 (pin 77) drives the N-channel MOSFET Q1301 (AO3400A, C20917) through R1313 (1k, C11702), with R1314 (4.7k, C25900) from gate to ground, and Q1301 pulls `~RESET` low; GPIO1 (pin 78) reads `~RESET` through R1315 (4.7k). Added the hierarchical label `~{RESET}` to the sheet and wired the new `~{RESET}` sheet pin to the shared reset line on the root sheet |
| 2026-10-31 | Merged the note blocks of the former `250407 Sheet 1` and `250407 Sheet 2` into one note B on `250407 Design` (sockets for U7, U17, U504 and U1701), without the obsolete note 7 and its marker, and added the title block of the C64 250407 PCB Replica with its credits to `250407 Design` |
| 2026-10-31 | Reconnected the expansion port: pin C gets `~RESET` through FB4 again, and pin 6 gets the dot clock (`DOT_CLK` from the `System Clocks` sheet) through the new `DOT_CLOCK_IN` sheet pin of `250407 Design`. #76 had left both unconnected. Changed the `~RESET` sheet pin of `250407 Design` to bidirectional |
| 2026-10-31 | Merged `250407 Sheet 1` and `250407 Sheet 2` into the `250407 Design` sheet (`250407.kicad_sch`), which is now a single flat sheet, and removed `250407_sheet1.kicad_sch` and `250407_sheet2.kicad_sch`. All 43 parts keep their references and values. Replaced the global labels between the two sheets (`CAEC`, `RDY`, `~IRQ`, `~NMI`, `~LORAM`, `~HIRAM`, `~CHAREN`, `R/~W`, `DOT_CLOCK`, `~{RESET}`) with local connections. In the project settings, set the ERC connection grid to 25 mil and the pin-to-pin check to error |
| 2026-10-31 | Updated the notes for the new clock circuit: on `250407 Sheet 1`, removed notes 2 (clock circuit), 8 (dot-clock VCO) and A (PAL/NTSC jumper) and the U1, U2 and U32 entries of note B; on `250407 Sheet 2`, marked note 7 (CAS delay) as obsolete and removed the U3 to U6, U9 to U12 and U21 to U24 entries of note B, whose chips earlier changes removed; on the `Power Supply` sheet, changed the `PGOOD` note to R1003 and the sync gate U1201 on the `System Clocks` sheet |
| 2026-10-31 | Removed the colour and dot clock PLL from `250407 Sheet 1`: Y1, U29 to U32, Q7, L2, FB16, FB17, FB19, C56, C65 to C68, C70, C74, C82 to C86, C107, C108, R16, R17, R19, R26, R27, R52, R53 and the PAL/NTSC jumpers E2 and E3, with the `+Vc` and `jumper_select` nets. Removed the hierarchical labels `CAN+5V`, `COLOR_CLOCK_OUT` and `DOT_CLOCK_OUT` and the global labels `PHI0`, `DOT_CLOCK` and `~{RESET}` from that sheet, and the matching sheet pins on `250407 Design` and the root sheet; `PHI0_IN` now only connects FB501 and the 6510. The expansion port's dot clock (CN6 pin 6) and its reset through FB4 (CN6 pin C) lost their connection with these labels and are reconnected in a later change |
| 2026-10-31 | Removed the `Color Clock Divider` sub-sheet (U1001, U1002, C1001, C1002) from the `Power Supply` sheet. The `Power Supply` sheet now gets the sync clock through the new `SYNC_CLK_IN` sheet pin, which feeds C710 and C1005, and passes `PGOOD` out through a new `PGOOD` sheet pin; its `COLOR_CLK_IN` sheet pin is gone |
| 2026-10-31 | Moved the `3V3 Buck` sub-sheet to page 10 and renumbered its parts from the 1200 range to the 1000 range (U1201 to U1001, L1201 to L1001, C1201 to C1213 to C1001 to C1013, R1201 to R1206 to R1001 to R1006), and moved the other sheets to make room for the `System Clocks` sheet |
| 2026-10-31 | Added a `System Clocks` sheet (`clocks.kicad_sch`, page 12) to the root sheet that makes the VIC-II's colour and dot clocks and the buck converter sync clock with an MS5351M clock synthesizer (Y1202, C1509083) on `+3V3` and a 25 MHz crystal with a 10 pF load (Y1201, C70582), decoupled by C1201 and C1202. CLK0 and CLK1 drive `COLOR_CLK` and `DOT_CLK` through the 5 V buffers U1202 and U1203 (SN74AHCT1G125, C7484, on `CAN+5V`, /OE on ground), R1203 and R1202 (27 Ohm, C25190) and the ferrite beads FB1203 and FB1202 (120 Ohm, C14709). CLK2 drives `SYNC_CLK` through U1201 (74AHCT1G08, C113521), which gates it with `PGOOD`, R1201 and FB1201. C1203 to C1205 decouple U1201 to U1203. Connected the sheet to `CAN+5V`, `PGOOD`, the supervisor's I2C bus (`SDA`, `SCL`), `COLOR_CLOCK_IN` and `DOT_CLOCK_IN` of the VIC-II sheet, and `SYNC_CLK_IN` of the `Power Supply` sheet |
| 2026-10-31 | Changed the socket notes of U504 and U1701 to the 8562 VIC-II and the 8580 SID, saying that the 6567 and the 6581 do not work in these sockets. Replaced the U18 and U19 entries of note B on `250407 Sheet 2` with U1701 and U504, and added the 8562 to note 8 (dot-clock VCO) on `250407 Sheet 1` |
| 2026-10-31 | Changed the parts around the VIC-II on the `VIC-II, HF Module Replacement and AV out` sheet to SMD: C507 to C512 and C515 to C517 to 100 nF 0402 (C1525), the electrolytic capacitor C513 to 10 uF 0603 ceramic (C96446, `Device:C_Small`), C518 to 20 pF 0402 C0G (C1554), FB501 to FB505 to 120 Ohm 0603 ferrite beads (C14709, `Device:FerriteBead_Small`, with short wires to the old pin positions), U505 and U506 to SN74HCT257DR, U502 to SN74F258D, U503 to CD4066BM96 (SOIC-14) and U508 to SN74LS373DWR (SOIC-20). Added PWR_FLAG #FLG0502 and #FLG0503 on the filtered VCC and VDD of U504, and removed the `Y5U` note next to the old decoupling capacitors |
| 2026-10-30 | Removed the original VIC-II circuit from `250407 Sheet 2`: U19 (6567), U13, U14, U16, U25, U26, FB12, FB14, FB15, FB22, FB23, C29, C30, C32, C44, C45, C53, C54, C60, C94, C101 and C203, and C36 from `250407 Sheet 1`. Replaced the global label `PHI0` between both sheets with the hierarchical labels `PHI0_IN` and `DOT_CLOCK_OUT` on `250407 Sheet 1`. On `250407 Sheet 2`, removed the hierarchical labels `CAN+5V`, `COLOR_CLOCK_IN`, `COLOR_D0` to `COLOR_D3`, `VIC_II_LUMA`, `VIC_II_CHROMA`, `~LP` and `~VA15`, and added `BA`, `~AEC`, `~VIC` and a second `AEC`. Changed the sheet pins on `250407 Design` and the root sheet to match. The `+12V` net no longer exists |
| 2026-10-30 | Changed the VIC-II on the `VIC-II, HF Module Replacement and AV out` sheet from the 6567 to the 8562 (U504): VDD (pin 13) now comes from `CAN+5V` through FB504, with C510, C512 and C513, instead of from `+12V`. Changed the luma pull-up R512 from 470 Ohm to 1 kOhm (C21190), and left out the copy of C203 (82 pF on `~CAS`, PAL only) |
| 2026-10-30 | Renamed the `HF Module Replacement and AV connectors` sheet to `VIC-II, HF Module Replacement and AV out` (file `vic_ii_hf_av_out.kicad_sch`) and added copies of the VIC-II and the parts that only serve it: U504 (VIC-II), U505 and U506 (74LS257), U508 (74LS373), U502 (74LS258), U503 (4066), FB501 to FB505, C507 to C513 and C515 to C518. Connected them through new sheet pins `D[0..7]`, `A[0..15]`, `R/~W`, `~VIC_SELECT`, `~IRQ`, `BA`, `AEC`, `~AEC`, `~CAS`, `PHI0`, `COLOR_CLOCK_IN`, `DOT_CLOCK_IN`, `COLOR_D0` to `COLOR_D3`, `VA4`, `VA5`, `~VA14`, `~VA15`, `~LP` and `CAN+5V`, and joined luma and chroma to the video circuit on the same sheet, replacing the `LUMA_IN` and `CHROMA_IN` sheet pins. Annotated the new parts in the 500 range |
| 2026-10-30 | Removed the SID circuit from `250407 Sheet 2`: U18 (6581), C10, C11, C12, C13, C34, C37, C48, C49, C50, C51, C52, C62, C93, C205, R6, R7, R38, Q8, FB10, FB11 and FB13, and the paddle switch U28 with C47, R301 and R302 from `250407 Sheet 1`. Removed the global labels `POTX` and `POTY` between both sheets, and the hierarchical labels and sheet pins `POTAX`, `POTAY`, `POTBX`, `POTBY`, `CIA1_PA6`, `CIA1_PA7`, `AUDIO_IN` and `AUDIO_OUT` on the `250407` sheets, `250407 Design` and the root sheet. Added `~SID` as a hierarchical label on `250407 Sheet 2` and as a sheet pin on `250407 Design` and the root sheet |
| 2026-10-30 | Changed the SID on the `SID and internal audio output` sheet from the 6581 to the 8580 (U1701): VDD and the collector of the audio buffer on `+9V`, filter capacitors C1704 and C1705 22 nF C0G (C77069). Changed its other parts to SMD: C1701, C1702, C1707, C1708 and C1715 100 nF (C307331 on `+9V`, C1525 on `+5V`), C1703 and C1706 1.8 nF C0G, C1709 1 nF C0G, C1712 220 pF, the electrolytic capacitors C1710, C1713 and C1714 to 22 uF and C1711 to 10 uF ceramic, R1701 to R1703 to 0402, FB1701 to FB1703 to 600 Ohm 0603 ferrite beads (`Device:FerriteBead_Small`), Q1701 to an MMBT3904 (SOT-23) and U1702 to a CD4066BM96 (SOIC-14) |
| 2026-10-30 | Replaced the `12V Stabilized Rail` sub-sheet of the `Power Supply` sheet with a `9V Stabilized Rail` sub-sheet: removed the charge pump (U901, D901 to D903, C901 to C903) and its clock input `PUMP_CLK`, changed U902 from an L78M12 to a CJ78M09 (C9071) fed from `12V_IN_SW` through FB901, with C904 (22 uF) and C905 (1 uF) at its input on the net `9V_IN` (formerly `VPUMP`) and C906 and C907 at its output. Replaced the `+12V_STABILIZED` sheet pin with `+9V_STABILIZED` and the root sheet's `+12V` power symbol with `+9V`. Removed the `Q5` sheet pin of the `Color Clock Divider` sheet; U1002 pin 2 is now unconnected. The `+12V` net (VIC-II VDD through FB12, C53, C94) has no source until #74 |
| 2026-10-30 | Added a `SID and internal audio output` sheet to the root sheet with copies of the SID circuit (U1701, C1701 to C1714, R1701 to R1703, FB1701 to FB1703, Q1701) and of the paddle switch (U1702, C1715, R1704, R1705), connected through new sheet pins `D[0..7]`, `A[0..15]`, `R/~W`, `PHI2`, `~RESET`, `~SELECT`, `POTAX`, `POTAY`, `POTBX`, `POTBY`, `CIA1_PA6`, `CIA1_PA7`, `AUDIO_IN` and `AUDIO_OUT`. Annotated its parts from 1700 |
| 2026-10-30 | Changed the LCSC part number of the expansion port CN6 on `250407 Sheet 2` from C5173305 (WingTAT ED44BGFBK, vertical) to C5173343 (WingTAT ER44BGFBK, right angle), so that cartridges plug in parallel to the board as on the original C64. Both are 2 x 22 card-edge slots at 2.54 mm pitch for 1.57 mm cards and fit the existing `EXPANSION_PORT` footprint; the PCB footprint's LCSC field is not updated, as for the other schematic changes |
| 2026-10-30 | Removed CIA 2 (U2, 6526) and its decoupling capacitor C3, the serial port CN4 with FB7 to FB9, the pull-ups R28 to R30 and the clamp diodes CR100 to CR103, the user port CN2 with C96, the resistor array RP3, and gates A, E and F of U8 (the serial bus drivers) from `250407 Sheet 1`. Removed the hierarchical labels `CIA1_CNT`, `CIA1_SP`, `~VA14` and `~VA15` from `250407 Sheet 1` and `CIA1_CNT` and `CIA1_SP` from `250407 Design`, with their sheet pins. Added `~NMI` as a hierarchical label and sheet pin on `250407 Sheet 1`, `250407 Design` and the root sheet. Replaced the global label `~CIA2` between `250407 Sheet 1` and `250407 Sheet 2` with a hierarchical label on `250407 Sheet 2` and a sheet pin on `250407 Design` and the root sheet. Added pull-ups R1531 (`SP1`), R1532 (`CNT1`), R1533 (`~IRQ`) and R1534 (`CASS_RD`), 4.7k to +5V, to the `CIA1, Keyboard and Joysticks` sheet in place of RP3, and connected its `CNT` and `SP` sheet pins to the new `CIA1_CNT` and `CIA1_SP` sheet pins of the `CIA2, Serial Port and User Port` sheet |
| 2026-10-30 | Added the user port J1602 (original pinout, pins 10 and 11 not connected) to the `CIA2, Serial Port and User Port` sheet. Every signal line has a TVS at the connector (TPD4E05U06DQAR arrays U1603 to U1606, H5VL10BH D1601 for `~RESET`), a 100 Ohm series resistor (R1631 to R1645) and a 4.7k pull-up to +5V (R1615 to R1626, R1628) at the GPIO, except ATN and `~RESET`, which have no series resistor. PB0 to PB7 go to GPIO27 to GPIO34, `CNT`, `SP`, `~PC`, `PA2` and `~FLAG` to GPIO12 to GPIO15 and GPIO23, `CNT1` and `SP1` to U1501, and pin 9 to the serial bus ATN node. C1621 decouples +5V. Added R1627 (4.7k) on `~NMI`, and R1629 and R1630 (4.7k) on `~VA14` and `~VA15`, which go to GPIO16 and GPIO17 |
| 2026-10-30 | Added the serial port J1601 (6-pin DIN, the part of CN4) to the `CIA2, Serial Port and User Port` sheet with TVS array U1607 (TPD4E05U06DQAR) on SRQ, ATN, CLK and DATA, H5VL10BH D1602 on `~RESET`, ferrite beads FB1601 to FB1603 (100 Ohm at 100 MHz) and 1k pull-ups R1607 to R1609 to +5V. U1602 (SN74LVC06A on `+3V3`, C1620) drives ATN, CLK and DATA from GPIO39, GPIO38 and GPIO36, with pull-downs R1610 to R1612 (10k) on its inputs and its unused gates tied to ground; GPIO37 and GPIO35 read CLK and DATA through R1614 and R1613 (100 Ohm). SRQ joins `CASS_RD` through the new `~SERIAL_SRQIN` sheet pin |
| 2026-10-29 | Added a `CIA2, Serial Port and User Port` sheet to the root sheet with an RP2354B microcontroller (U1601) on `+3V3`, following Raspberry Pi's RP2350 reference design: core regulator inductor L1601 with its own local core supply net, 12 MHz crystal Y1601, and no buttons or USB. Connected it to the bus (`D[0..7]`, `A[0..15]`, `PHI2`, `R/~W`, `~RESET`, `~CIA2` as `~SELECT`, `~NMI` as `~IRQ`) through new sheet pins, and to the supervisor (U1301): GPIO30 to `RUN` through R1602 (1k) with R1604 (10k) to ground, GPIO28 and GPIO29 to SWCLK and SWDIO through R1605 and R1606 (33 Ohm), through new sheet pins on the `Supervisor` sheet, and I2C0 (GPIO40, GPIO41) shared with U1401 and U1501. Annotated its parts from 1600 |
| 2026-10-29 | Removed CIA 1 (U1, 6526) and its decoupling capacitor C2 from `250407 Sheet 1`. Connected its signals to U1501 through new hierarchical labels and sheet pins on `250407 Sheet 1`, `250407 Design` and the root sheet: `~CIA1` (sheet pin `~SELECT`), `~CIA1_FLAG` (`CASS_RD`), `~CIA1_IRQ` (`~IRQ`), `CIA1_CNT` (`CNT1`), `CIA1_SP` (`SP1`), `~RESET`, `CIA1_PA6` and `CIA1_PA7`. `R/~W`, `PHI2`, `A[0..15]` and `D[0..7]` reach the new sheet from the existing sheet pins on the root sheet. Replaced the global labels `~CIA1` and `BTNA\~LP` between `250407 Sheet 1` and `250407 Sheet 2` with hierarchical labels (`~LP`) |
| 2026-10-29 | Removed the keyboard connector CN1, the control ports CN8 and CN9 and their ferrite bead FB18 from `250407 Sheet 1`. Renamed the keyboard columns `COL6` and `COL7`, which also select the paddle port on U28, to `CIA1_PA6` and `CIA1_PA7` and added pull-ups R301 and R302 (10k) to +5V. Renamed `~RESTORE` to `~RESTORE_NMI` and connected it to U1501 GPIO27, so the controller triggers the RESTORE one-shot U20A. Brought `POTAX`, `POTAY`, `POTBX` and `POTBY` from U28 and `~LP` from the VIC-II out to the new control ports through hierarchical labels and sheet pins |
| 2026-10-29 | Added control ports J1502 (port 2) and J1503 (port 1) to the `CIA1, Keyboard and Joysticks` sheet: 9-pin male D-sub sockets (CONNFLY DS1037-09MNAKT74-0CC, `DE9_Pins_MountingHoles` with the shells on ground), +5V on pin 7 through FB1501 (100 Ohm at 100 MHz), POT lines to U28. Each of the ten digital lines has a TVS diode at the connector (D1504 to D1513, H5VL10BH), a 100 Ohm series resistor (R1521 to R1530) and a 10k pull-up to +5V at the GPIO (R1511 to R1520). The fire line of port 1 goes through R1529 to `~LP` (GPIO38 and the VIC-II light pen input) |
| 2026-10-28 | Added a USB-A keyboard socket J1501 (CONNFLY DS1095-WNR0) to the `CIA1, Keyboard and Joysticks` sheet on the USB port of U1501, with series resistors R1507 and R1508 (27 Ohm) and TVS diodes D1501 to D1503 (H5VL10BH) on D+, D- and `VBUS`. `VBUS` comes from `+5V` through current-limited power switch U1502 (TPS2051C) with C1521 and C1522 at its input and C1520 (150 uF) at its output; U1501 controls it with `USB_EN` (GPIO46, R1509 10k pull-down) and reads `USB_FLT` (GPIO47, R1510 10k pull-up) |
| 2026-10-28 | Added a `CIA1, Keyboard and Joysticks` sheet to the root sheet with an RP2354B microcontroller (U1501) on `+3V3`, following Raspberry Pi's RP2350 reference design: core regulator inductor L1501 with its own local core supply net, 12 MHz crystal Y1501, and no buttons. Connected it to the supervisor (U1301): GPIO44 to `RUN` through R1502 (1k) with R1504 (10k) to ground, GPIO42 and GPIO43 to SWCLK and SWDIO through R1505 and R1506 (33 Ohm), through new sheet pins on the `Supervisor` sheet, and I2C0 (GPIO40, GPIO41) shared with U1401. Annotated its parts from 1500 |
| 2026-10-28 | Added the LCSC part number C99395 to the embedded UCC27517DBV library symbol on the `12V Stabilized Rail` sheet. U901 already had it; no change to the circuit |
| 2026-10-28 | Added `PHI2` and `~CAS` inputs to the RAM and ROM controller: U1402 (74LVC2G17) with C1420 on `+3V3` buffers `~CAS` to GPIO47 and `PHI2` to GPIO46, which are not 5 V tolerant. Passed `~CAS` and `PHI2` to the `RAM and ROM replacement` sheet through new sheet pins |
| 2026-10-28 | Added EEPROM U1403 (AT24C256C) with pull-ups R1407 and R1408 (4.7k) and C1421 to the `RAM and ROM replacement` sheet, on I2C1 of U1401 (GPIO42, GPIO43), to store the ROM images |
| 2026-10-28 | Removed the eight 4164 DRAMs (U9 to U12, U21 to U24) from `250407 Sheet 2` with their decoupling capacitors C25 to C28 and C40 to C43, the 10 uF capacitor C14, the resistor arrays RP1 and RP2 with the `MA0` to `MA7` nets, the series resistor R42 and C204. Connected `VA4`, `VA5`, `~VA14`, `~VA15`, `AEC` and `~CASRAM` to U1401 through new hierarchical labels and sheet pins on `250407 Sheet 2`, `250407 Design` and the root sheet |
| 2026-10-27 | Removed the colour RAM U6 (2114) and its decoupling capacitor C7 from `250407 Sheet 2` and connected `~COLORAM`, `GR/~W` and `COLOR_D0` to `COLOR_D3` to U1401 through new hierarchical labels and sheet pins |
| 2026-10-27 | Removed the BASIC ROM U3 (2364), the KERNAL ROM U4 (2364) and the character ROM U5 (2332) with their decoupling capacitors C4 to C6 from `250407 Sheet 2`, and connected `~BASIC`, `~KERNAL`, `~CHAROM`, `R/~W`, the address bus `A[0..15]` and the data bus `D[0..7]` to U1401 through new hierarchical labels and sheet pins on `250407 Sheet 2`, `250407 Design` and the root sheet. Replaced the global labels `A[0..15]`, `D[0..7]`, `PHI2`, `~VA14` and `~VA15` between `250407 Sheet 1` and `250407 Sheet 2` with hierarchical labels and sheet pins on the `250407 Design` sheet. Added a memory map note to the `RAM and ROM replacement` sheet |
| 2026-10-27 | Added a `RAM and ROM replacement` sheet to the root sheet with an RP2354B microcontroller (U1401) on `+3V3`, following Raspberry Pi's RP2350 reference design: core regulator inductor L1401 with its own local core supply net, 12 MHz crystal Y1401, and no buttons or USB. Connected it to the supervisor (U1301): GPIO45 to `RUN` through R1404 (1k) with R1403 (10k) to ground, GPIO46 and GPIO47 to SWCLK and SWDIO through R1405 and R1406 (33 Ohm), and I2C0 (GPIO40, GPIO41) between both with pull-ups R1306 and R1312 (4.7k) on the `Supervisor` sheet. Annotated its parts from 1400 |
| 2026-10-27 | Added an input switch to the `Power Supply` sheet: P-channel MOSFETs Q604 and Q605 (AO3401A) in parallel between `12V_IN` and the new switched net `12V_IN_SW`, driven by N-channel MOSFET Q603 (AO3400A) through R602 to R605 and C603, with a PWR_FLAG on `12V_IN_SW`. Moved the 5 V buck converter, the cassette buck converter, `CAN+5V`, the `+12V` charge pump with D903 and the HF module replacement to `12V_IN_SW`; the 3.3 V buck converter stays on `12V_IN`. Replaced the `12V_INPUT` sheet pin of the `Power Supply` sheet with `12V_IN_SW` and renamed the `12V_IN` sheet pin of the `HF Module Replacement and AV connectors` sheet to `12V_IN_SW`. Connected GPIO2 of the supervisor (U1301) to the switch through a new `PREFLIGHT_OK` sheet pin on the `Supervisor` sheet and a `POWER_ON` sheet pin on the `Power Supply` sheet |
| 2026-10-27 | Removed the 8-pin DIN A/V connector CN5 from `250407 Sheet 2`, with the `LUMA_OUT`, `CHROMA_OUT` and `COMP_OUT` hierarchical labels and sheet pins that led to it. Composite, luma, chroma and audio are now on J501 and J502 |
| 2026-10-26 | Renamed the `HF Module Replacement` sheet to `HF Module Replacement and AV connectors` and added the audio and video connectors to it: S-Video socket J501 (MD-40SM) on `LUMA_OUT` and `CHROMA_OUT`, and triple RCA jack J502 (CAX-AV-314-RWY-PHP) with audio in on the red jack, audio out on the white jack and composite video on the yellow jack. Replaced the `AUDIO_IN` and `AUDIO_OUT` labels on `250407 Sheet 2` with hierarchical labels and passed them through the `250407 Design` sheet to the new sheet. Added TVS diode D502 (SMF5.0CA) and series resistor R513 (150 Ohm) to the audio input |
| 2026-10-26 | Added the S-Video connector, CUI (Same Sky) MD-40SM (LCSC C6224745), to the component library in `hardware/components`: the symbol `MD-40SM` (KiCad's `Connector:Mini-DIN-4` with a shield pin) and the footprint `Mini-DIN-4_CUI_MD-40SM_Horizontal` |
| 2026-10-26 | Added a triple RCA jack, CAX-AV-314-RWY-PHP (LCSC C55072367), to the component library in `hardware/components`: the symbol `CAX-AV-314-RWY-PHP` with one unit per jack and the footprint `Jack_RCA_x3_CAX_AV-314-RWY_Horizontal` |
| 2026-10-24 | Added a `Supervisor` sheet to the root sheet with an RP2354B microcontroller (U1301) on `+3V3`, following Raspberry Pi's RP2350 reference design: core regulator inductor L1301 with its own local core supply net, 12 MHz crystal Y1301, RESET (SW1302) and BOOTSEL (SW1301) buttons, and a USB-C receptacle (J1301) for programming with ESD protection and VBUS detection on GPIO13. Annotated its parts from 1300 |
| 2026-10-24 | Added an `RP2354B` symbol for the supervisor microcontroller to the component library in `hardware/components`, with the pin layout from the author's earlier RP2354 designs, the 0.3 mm thermal via QFN-80 footprint and LCSC part C39843328 |
| 2026-10-24 | Added three footprints for the RP2354 supervisor microcontroller to the component library in `hardware/components`: `QFN-80-1EP_10x10mm_P0.4mm_EP3.4x3.4mm_ThermalVias_0.3mm` (KiCad's QFN-80 with 0.3 mm thermal vias), `L_Abracon_AOTA-B201610` (polarity-marked core regulator inductor) and `USB_C_Receptacle_HCTL_HC-TYPE-C-16P-01A_Aligned` (KiCad's footprint with the origin moved 1.15 mm) |
| 2026-10-24 | Added a `3V3 Buck` sub-sheet to the `Power Supply` sheet that makes `+3V3` from the 12 V DC input with an LM61440 synchronous buck converter (U1201), a copy of the 5 V buck converter changed for 3.3 V: feedback divider R1204 51k and R1205 22k (3.32 V), feedforward C1208 10 pF with R1206 1k, inductor L1201 10 uH (Fangcheng FCH0650-100M) and four 22 uF output capacitors (C1209 to C1212) plus C1213. It is synchronized to Q4 of the colour clock divider through C1205, and `+3V3` reaches the root sheet through the `3V3_OUT` sheet pin. Annotated its parts from 1200 |
| 2026-10-24 | Combined the PGOOD outputs of the 5 V and 3.3 V buck converters into one `PGOOD` net (wired-AND) that gates the colour clock divider, replacing the divider's `5V_PGOOD` input. Changed the PGOOD pull-up R706 from 100k to 200k; with the new R1203 (200k) on the 3.3 V buck converter the combined pull-up stays 100k |
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
