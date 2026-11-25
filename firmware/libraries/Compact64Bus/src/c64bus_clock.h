// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Core voltage for overclocked builds. The RP2350 is rated for 150 MHz at
// its default 1.10 V, and arduino-pico only raises the voltage for the
// RP2040. On the bench, U1401 at 300 MHz and 1.10 V failed 5 to 17 % of its
// starts (USB not coming up, or core 1 stopping in its init); at 1.15 V and
// 1.20 V none of 250 starts failed, at 150 MHz none either.
//
// C64BUS_CORE_VOLTAGE() defines a constructor that sets 1.20 V when F_CPU is
// above 150 MHz. Constructors run after the SDK has started the clocks at
// 150 MHz and before main() switches to F_CPU. Use it once per sketch.

#ifndef C64BUS_CLOCK_H
#define C64BUS_CLOCK_H

#include "hardware/vreg.h"
#include "pico/platform.h"

#define C64BUS_CORE_VOLTAGE()                                                   \
    __attribute__((constructor)) static void c64bus_core_voltage(void) {        \
        if (F_CPU > 150000000 && vreg_get_voltage() < VREG_VOLTAGE_1_20) {      \
            vreg_set_voltage(VREG_VOLTAGE_1_20);                                \
            busy_wait_at_least_cycles(150000); /* 1 ms to settle at 150 MHz */ \
        }                                                                       \
    }

#endif
