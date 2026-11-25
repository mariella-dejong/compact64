// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Bus loop of the RAM and ROM controller (U1401), following the timing notes
// in hardware/main-board/README.md (RAM and ROM replacement, Timing):
//
// - 6510 cycles (AEC high): the address is valid before the select falls.
//   Reads drive D0 to D7 until the select rises. Writes take the last sample
//   of D0 to D7 taken while PHI2 is still high, not the value at ~CASRAM.
// - VIC-II cycles (AEC low, ~COLORAM low throughout): A12 and A13 arrive on
//   VA4 and VA5 after ~CAS, so the address is taken when ~CASRAM falls (plus
//   bus_vic_sample_delay). The colour nibble for A0 to A9 goes out on COLORD0
//   to COLORD3 while ~CASRAM or ~CHAROM is low, so c-accesses get 12 bits.
// - Colour RAM in 6510 cycles: reads drive D0 to D3 only, writes take D0 to
//   D3 while GR/~W is low.
//
// Everything is timed from the select edges, never from fixed delays.

#include "bus.h"

#include "hardware/gpio.h"
#include "hardware/structs/busctrl.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/syscfg.h"
#include "hardware/sync.h"
#include "pico/platform.h"

#include <c64bus_pins.h>

#pragma GCC optimize("O3")

volatile bus_stats_t bus_stats;
uint8_t bus_ram[C64_RAM_SIZE] __attribute__((aligned(4)));
uint8_t bus_color[C64_COLOR_SIZE] __attribute__((aligned(4)));
uint8_t bus_rom[C64_ROM_SIZE] __attribute__((aligned(4)));
volatile uint32_t bus_vic_sample_delay = 0;
volatile uint32_t bus_aec_settle = 2;

// Bus inputs in GPIO_HI_IN.
#define HI_INPUTS (C64_HI_VA5 | C64_HI_VA14N | C64_HI_VA15N | C64_HI_AEC | C64_HI_PHI2 | C64_HI_CAS)

void bus_init_pins(void) {
    for (uint gpio = 0; gpio <= C64_GPIO_COLORD0 + 3; gpio++) {
        gpio_init(gpio);
        gpio_disable_pulls(gpio);  // RP2350 erratum E9
        gpio_set_input_enabled(gpio, true);
    }
    for (uint gpio = C64_GPIO_D0; gpio < C64_GPIO_D0 + 8; gpio++) {
        gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_12MA);
        gpio_set_slew_rate(gpio, GPIO_SLEW_RATE_FAST);
    }
    for (uint gpio = C64_GPIO_COLORD0; gpio < C64_GPIO_COLORD0 + 4; gpio++) {
        gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_12MA);
        gpio_set_slew_rate(gpio, GPIO_SLEW_RATE_FAST);
        gpio_set_dir(gpio, GPIO_OUT);  // only to the VIC-II's D8 to D11
    }
    const uint inputs[] = {C64_GPIO_PHI2, C64_GPIO_CAS};
    for (uint i = 0; i < 2; i++) {
        gpio_init(inputs[i]);
        gpio_disable_pulls(inputs[i]);
    }
    const uint outputs[] = {C64_GPIO_TRIG, C64_GPIO_HEARTBEAT};
    for (uint i = 0; i < 2; i++) {
        gpio_init(outputs[i]);
        gpio_disable_pulls(outputs[i]);
        gpio_set_dir(outputs[i], GPIO_OUT);
    }
    // The bus loop gets the bus fabric first when both cores want SRAM.
    busctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS;
}

// Skips the two-flip-flop synchronizer on the bus inputs: saves about two
// clocks per access, at the risk of a metastable read on an edge.
void bus_set_sync_bypass(bool on) {
    syscfg_hw->proc_in_sync_bypass = on ? 0xFFFFFFFFu : 0u;
    syscfg_hw->proc_in_sync_bypass_hi = on ? HI_INPUTS : 0u;
}

// About three clocks per loop.
static inline __attribute__((always_inline)) void delay_loops(uint32_t n) {
    if (n) {
        __asm volatile("1: subs %0, %0, #1\n bne 1b" : "+r"(n) : : "cc");
    }
}

// Address of a VIC-II access: A0 to A11 from the bus, A12 and A13 from VA4
// and VA5, A14 and A15 from the inverted bank bits ~VA14 and ~VA15.
static inline __attribute__((always_inline)) uint32_t vic_address(uint32_t lo, uint32_t hi) {
    return ((lo >> C64_GPIO_A0) & 0x0FFFu)
         | ((lo >> (C64_GPIO_VA4 - 12)) & 0x1000u)
         | (((hi ^ (C64_HI_VA14N | C64_HI_VA15N)) & 7u) << 13);
}

#if BUS_TRACE_PINS
#define TP_VIC_ON()  (sio->gpio_hi_set = C64_HI_TRIG)
#define TP_VIC_OFF() (sio->gpio_hi_clr = C64_HI_TRIG)
#define TP_CPU_ON()  (sio->gpio_hi_set = C64_HI_HEARTBEAT)
#define TP_CPU_OFF() (sio->gpio_hi_clr = C64_HI_HEARTBEAT)
#define TP_LATE()    ((void)0)
#else
#define TP_VIC_ON()  ((void)0)
#define TP_VIC_OFF() ((void)0)
#define TP_CPU_ON()  ((void)0)
#define TP_CPU_OFF() ((void)0)
#define TP_LATE()    (sio->gpio_hi_togl = C64_HI_TRIG)
#endif

void __not_in_flash_func(bus_run)(void) {
    sio_hw_t *const sio = sio_hw;
    uint8_t *const ram = bus_ram;
    uint8_t *const color = bus_color;
    const uint8_t *const rom = bus_rom;
    uint32_t cd = 0;  // nibble on COLORD0 to COLORD3

    sio->gpio_hi_out = sio->gpio_hi_out & ~C64_HI_COLORD;
    (void)save_and_disable_interrupts();

    for (;;) {
        uint32_t lo;
        // Wait for any select. ~COLORAM is low throughout VIC-II cycles.
        do {
            lo = sio->gpio_in;
        } while ((lo & C64_LO_SELECTS) == C64_LO_SELECTS);

        if ((lo & (C64_LO_CASRAM | C64_LO_COLORAM | C64_LO_RW)) == (C64_LO_COLORAM | C64_LO_RW)) {
            // 6510 RAM read. ~COLORAM high means AEC is high.
            sio->gpio_out = ram[(lo >> C64_GPIO_A0) & 0xFFFFu];
            sio->gpio_oe_set = C64_LO_D;
            TP_CPU_ON();
            if (sio->gpio_in & C64_LO_CASRAM) {
                bus_stats.late++;
                TP_LATE();
            }
            while (!(sio->gpio_in & C64_LO_CASRAM)) {
            }
            sio->gpio_oe_clr = C64_LO_D;
            TP_CPU_OFF();
            bus_stats.ram_read++;
            continue;
        }

        if (!(lo & C64_LO_COLORAM)) {
            uint32_t hi = sio->gpio_hi_in;
            if (hi & C64_HI_AEC) {
                TP_CPU_ON();
                // 6510 colour RAM access, A0 to A9.
                const uint32_t a = (lo >> C64_GPIO_A0) & (C64_COLOR_SIZE - 1u);
                if (lo & C64_LO_RW) {
                    sio->gpio_out = color[a];
                    sio->gpio_oe_set = 0x0Fu;  // D4 to D7 stay undriven
                    do {
                        lo = sio->gpio_in;
                        hi = sio->gpio_hi_in;
                    } while (!(lo & C64_LO_COLORAM) && (hi & C64_HI_AEC));
                    sio->gpio_oe_clr = 0x0Fu;
                    bus_stats.color_read++;
                } else {
                    uint32_t d = 0;
                    bool seen = false;
                    for (;;) {
                        lo = sio->gpio_in;
                        hi = sio->gpio_hi_in;
                        if ((lo & C64_LO_COLORAM) || !(hi & C64_HI_AEC)) {
                            break;
                        }
                        if (!(lo & C64_LO_GRW) && (hi & C64_HI_PHI2)) {
                            d = lo;
                            seen = true;
                        } else if (seen) {
                            break;  // GR/~W rose: d is the last sample before
                        }
                    }
                    if (seen) {
                        color[a] = (uint8_t)(d & 0x0Fu);
                        bus_stats.color_write++;
                    }
                    while (!(sio->gpio_in & C64_LO_COLORAM) && (sio->gpio_hi_in & C64_HI_AEC)) {
                    }
                }
                TP_CPU_OFF();
                continue;
            }

            // VIC-II phase: runs until AEC rises.
            TP_VIC_ON();
            const uint32_t vdelay = bus_vic_sample_delay;
            for (;;) {
                lo = sio->gpio_in;
                hi = sio->gpio_hi_in;
                uint32_t sel;
                if (!(lo & C64_LO_CASRAM)) {
                    if (vdelay) {
                        delay_loops(vdelay);
                        lo = sio->gpio_in;
                        hi = sio->gpio_hi_in;
                    }
                    sio->gpio_out = ram[vic_address(lo, hi)];
                    sel = C64_LO_CASRAM;
                    bus_stats.vic_ram++;
                } else if (!(lo & C64_LO_CHAROM)) {
                    sio->gpio_out = rom[C64_ROM_CHAR + ((lo >> C64_GPIO_A0) & 0x0FFFu)];
                    sel = C64_LO_CHAROM;
                    bus_stats.vic_char++;
                } else if (hi & C64_HI_AEC) {
                    break;
                } else {
                    continue;
                }
                sio->gpio_oe_set = C64_LO_D;
                // Colour for c-accesses, from A0 to A9, while the select is low.
                do {
                    lo = sio->gpio_in;
                    const uint32_t c = color[(lo >> C64_GPIO_A0) & (C64_COLOR_SIZE - 1u)];
                    sio->gpio_hi_togl = (c ^ cd) << (C64_GPIO_COLORD0 - 32);
                    cd = c;
                } while (!(lo & sel));
                sio->gpio_oe_clr = C64_LO_D;
            }
            // AEC is high again; let a stale ~COLORAM from the CPLD settle.
            TP_VIC_OFF();
            delay_loops(bus_aec_settle);
            continue;
        }

        if (!(lo & C64_LO_CASRAM)) {
            // 6510 RAM write: keep the last sample of D0 to D7 taken while
            // ~CASRAM is low and PHI2 is still high.
            TP_CPU_ON();
            const uint32_t a = (lo >> C64_GPIO_A0) & 0xFFFFu;
            uint32_t d = lo;
            for (;;) {
                const uint32_t x = sio->gpio_in;
                const uint32_t h = sio->gpio_hi_in;
                if ((x & C64_LO_CASRAM) || !(h & C64_HI_PHI2)) {
                    break;
                }
                d = x;
            }
            ram[a] = (uint8_t)d;
            while (!(sio->gpio_in & C64_LO_CASRAM)) {
            }
            TP_CPU_OFF();
            bus_stats.ram_write++;
            continue;
        }

        // ROM read by the 6510 (writes to ROM addresses go to RAM, so a ROM
        // select with R/~W low is never driven).
        uint32_t sel;
        uint32_t v;
        const uint32_t a = lo >> C64_GPIO_A0;
        if (!(lo & C64_LO_KERNAL)) {
            sel = C64_LO_KERNAL;
            v = rom[C64_ROM_KERNAL + (a & 0x1FFFu)];
        } else if (!(lo & C64_LO_BASIC)) {
            sel = C64_LO_BASIC;
            v = rom[C64_ROM_BASIC + (a & 0x1FFFu)];
        } else {
            sel = C64_LO_CHAROM;
            v = rom[C64_ROM_CHAR + (a & 0x0FFFu)];
        }
        if (lo & C64_LO_RW) {
            sio->gpio_out = v;
            sio->gpio_oe_set = C64_LO_D;
            TP_CPU_ON();
            if (sio->gpio_in & sel) {
                bus_stats.late++;
                TP_LATE();
            }
            while (!(sio->gpio_in & sel)) {
            }
            sio->gpio_oe_clr = C64_LO_D;
            bus_stats.rom_read++;
        } else {
            TP_CPU_ON();
            while (!(sio->gpio_in & sel)) {
            }
        }
        TP_CPU_OFF();
    }
}
