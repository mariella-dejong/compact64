// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Bus loop of the RAM and ROM controller (U1401). Runs on core 1 with
// interrupts off and answers every access of the 6510 and the VIC-II.

#ifndef U1401_BUS_H
#define U1401_BUS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Counters, written by core 1 only. `late` counts reads whose select had
// already gone high again by the time the data was on the bus.
typedef struct {
    uint32_t ram_read;
    uint32_t ram_write;
    uint32_t rom_read;
    uint32_t color_read;
    uint32_t color_write;
    uint32_t vic_ram;
    uint32_t vic_char;
    uint32_t late;
} bus_stats_t;

extern volatile bus_stats_t bus_stats;

// Memory served on the bus (layout in c64bus_pins.h).
extern uint8_t bus_ram[];
extern uint8_t bus_color[];
extern uint8_t bus_rom[];

// Tuning, read by core 1 at the start of every VIC-II phase.
// vic_sample_delay: delay loops (about 3 clocks each) between seeing
//   ~CASRAM fall in a VIC-II cycle and taking the column address (#152).
// aec_settle: delay loops after AEC rises before a low ~COLORAM counts as a
//   6510 colour RAM access, so the CPLD's late ~COLORAM is not mistaken.
extern volatile uint32_t bus_vic_sample_delay;
extern volatile uint32_t bus_aec_settle;

void bus_init_pins(void);
void bus_set_sync_bypass(bool on);
void bus_run(void);  // never returns

#ifdef __cplusplus
}
#endif

#endif
