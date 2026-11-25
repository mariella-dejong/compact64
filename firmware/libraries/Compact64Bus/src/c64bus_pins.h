// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// GPIO assignment of the RAM and ROM controller (U1401, RP2354B), as on the
// `RAM and ROM replacement` sheet (hardware/main-board/README.md, Pins).
// The bench exerciser uses the same numbers, so that every jumper between the
// two boards runs from GPIOn to GPIOn.
//
// "lo" bits are GPIO0 to GPIO31 (SIO GPIO_IN), "hi" bits are GPIO32 to GPIO47
// (SIO GPIO_HI_IN, bit n is GPIO32 + n).

#ifndef C64BUS_PINS_H
#define C64BUS_PINS_H

#define C64_GPIO_D0        0   // D0 to D7: GPIO0 to GPIO7
#define C64_GPIO_A0        8   // A0 to A15: GPIO8 to GPIO23
#define C64_GPIO_CASRAM    24  // ~CASRAM, RAM select (includes ~CAS)
#define C64_GPIO_BASIC     25  // ~BASIC
#define C64_GPIO_KERNAL    26  // ~KERNAL
#define C64_GPIO_CHAROM    27  // ~CHAROM
#define C64_GPIO_COLORAM   28  // ~COLORAM, low in every VIC-II cycle
#define C64_GPIO_RW        29  // R/~W
#define C64_GPIO_VA5       30  // A5, then A13 while ~CAS is low
#define C64_GPIO_VA4       31  // A4, then A12 while ~CAS is low
#define C64_GPIO_GRW       32  // GR/~W, colour RAM write strobe
#define C64_GPIO_VA14N     33  // ~VA14 (bank, inverted A14)
#define C64_GPIO_VA15N     34  // ~VA15 (bank, inverted A15)
#define C64_GPIO_AEC       35  // high: 6510 cycle, low: VIC-II cycle
#define C64_GPIO_COLORD0   36  // COLORD0 to COLORD3: GPIO36 to GPIO39
#define C64_GPIO_SDA       40  // I2C0 link to the supervisor
#define C64_GPIO_SCL       41
#define C64_GPIO_EE_SDA    42  // I2C1 to the EEPROM (U1403)
#define C64_GPIO_EE_SCL    43
#define C64_GPIO_TRIG      44  // free on the board, kept for a trigger pad (#112)
#define C64_GPIO_HEARTBEAT 45  // heartbeat LED D1401
#define C64_GPIO_PHI2      46  // PHI2 through U1402
#define C64_GPIO_CAS       47  // ~CAS through U1402

// Bit masks in GPIO_IN (GPIO0 to GPIO31).
#define C64_LO_D        0x000000FFu
#define C64_LO_A        0x00FFFF00u
#define C64_LO_CASRAM   (1u << C64_GPIO_CASRAM)
#define C64_LO_BASIC    (1u << C64_GPIO_BASIC)
#define C64_LO_KERNAL   (1u << C64_GPIO_KERNAL)
#define C64_LO_CHAROM   (1u << C64_GPIO_CHAROM)
#define C64_LO_COLORAM  (1u << C64_GPIO_COLORAM)
#define C64_LO_RW       (1u << C64_GPIO_RW)
#define C64_LO_VA5      (1u << C64_GPIO_VA5)
#define C64_LO_VA4      (1u << C64_GPIO_VA4)
#define C64_LO_SELECTS  (C64_LO_CASRAM | C64_LO_BASIC | C64_LO_KERNAL | C64_LO_CHAROM | C64_LO_COLORAM)
#define C64_LO_ROMS     (C64_LO_BASIC | C64_LO_KERNAL | C64_LO_CHAROM)

// Bit masks in GPIO_HI_IN (GPIO32 to GPIO47).
#define C64_HI_BIT(gpio) (1u << ((gpio) - 32))
#define C64_HI_GRW       C64_HI_BIT(C64_GPIO_GRW)
#define C64_HI_VA14N     C64_HI_BIT(C64_GPIO_VA14N)
#define C64_HI_VA15N     C64_HI_BIT(C64_GPIO_VA15N)
#define C64_HI_AEC       C64_HI_BIT(C64_GPIO_AEC)
#define C64_HI_COLORD    (0xFu << (C64_GPIO_COLORD0 - 32))
#define C64_HI_TRIG      C64_HI_BIT(C64_GPIO_TRIG)
#define C64_HI_HEARTBEAT C64_HI_BIT(C64_GPIO_HEARTBEAT)
#define C64_HI_PHI2      C64_HI_BIT(C64_GPIO_PHI2)
#define C64_HI_CAS       C64_HI_BIT(C64_GPIO_CAS)

// Memory sizes and the layout of the ROM image array.
#define C64_RAM_SIZE      0x10000u
#define C64_COLOR_SIZE    0x400u
#define C64_ROM_BASIC     0x0000u   // 8 KB at $A000
#define C64_ROM_KERNAL    0x2000u   // 8 KB at $E000
#define C64_ROM_CHAR      0x4000u   // 4 KB
#define C64_ROM_SIZE      0x5000u

#endif
