// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Pin map of the CIA controllers, U1501 (CIA 1) and U1601 (CIA 2), from the
// main board README ("CIA 1, keyboard and joysticks", "CIA 2, serial port and
// user port"). The bus pins are the same on both.

#ifndef C64CIA_PINS_H
#define C64CIA_PINS_H

// Shared bus pins.
#define CIA_GPIO_D0 0      // D0 to D7: GPIO0 to GPIO7
#define CIA_GPIO_A0 8      // A0 to A3 (RS0 to RS3): GPIO8 to GPIO11
#define CIA_GPIO_CNT 12    // CNT, open drain
#define CIA_GPIO_SP 13     // SP, open drain
#define CIA_GPIO_RESET 21  // ~RESET, input
#define CIA_GPIO_IRQ 22    // ~IRQ (CIA 2: ~NMI), open drain
#define CIA_GPIO_FLAG 23   // ~FLAG, input
#define CIA_GPIO_PHI2 24   // PHI2
#define CIA_GPIO_RW 25     // R/~W
#define CIA_GPIO_CS 26     // ~CS (~CIA1, ~CIA2)

// Bench only: TOD edges from the exerciser, on a pin both controllers leave
// free (#182). On the board the controller makes them from its crystal.
#define CIA_GPIO_TOD_BENCH 18

// CIA 1 (U1501). Port A bits 0 to 4: control port 2; port B bits 0 to 4:
// control port 1; not in bit order on the GPIOs.
#define CIA1_GPIO_NMI 27      // ~NMI, RESTORE pulse, open drain
#define CIA1_GPIO_PA6 28      // paddle select, open drain
#define CIA1_GPIO_PA7 29
#define CIA1_GPIO_LP 38       // ~LP: PB4, control port 1 fire and light pen
#define CIA1_GPIO_USB_EN 46   // keyboard port power, high = on
#define CIA1_GPIO_USB_FLT 47  // keyboard port overcurrent, low = fault
static const unsigned char cia1_pa_gpio[5] = {34, 31, 32, 30, 33};  // PA0..PA4
static const unsigned char cia1_pb_gpio[5] = {39, 37, 36, 35, 38};  // PB0..PB4

// CIA 2 (U1601).
#define CIA2_GPIO_PC 14       // ~PC, open drain
#define CIA2_GPIO_PA2 15      // user port M, open drain
#define CIA2_GPIO_VA14N 16    // PA0, VIC-II bank, open drain
#define CIA2_GPIO_VA15N 17    // PA1
#define CIA2_GPIO_PB0 27      // PB0 to PB7: GPIO27 to GPIO34, open drain
#define CIA2_GPIO_DATA_IN 35  // PA7
#define CIA2_GPIO_DATA_OUT 36 // PA5 through U1602, high pulls DATA low
#define CIA2_GPIO_CLK_IN 37   // PA6
#define CIA2_GPIO_CLK_OUT 38  // PA4 through U1602
#define CIA2_GPIO_ATN_OUT 39  // PA3 through U1602

// Shared.
#define CIA_GPIO_SDA 40
#define CIA_GPIO_SCL 41

#endif
