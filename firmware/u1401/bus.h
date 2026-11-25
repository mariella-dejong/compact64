// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Bus loop of the RAM and ROM controller (U1401). The reads run in PIO
// state machines fed by DMA (bus.pio); core 1 runs with interrupts off and
// serves the rest of the accesses of the 6510 and the VIC-II (bus.c).

#ifndef U1401_BUS_H
#define U1401_BUS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Counters of the accesses core 1 serves, written by core 1 only. Reads
// served by the state machines are not counted.
typedef struct {
    uint32_t ram_write;
    uint32_t rom_read;     // BASIC and KERNAL
    uint32_t color_read;
    uint32_t color_write;
} bus_stats_t;

extern volatile bus_stats_t bus_stats;

// Memory served on the bus (layout in c64bus_pins.h).
extern uint8_t bus_ram[];
extern uint8_t bus_color[];
extern uint8_t bus_rom[];

// aec_settle: delay loops (about 3 clocks each) after AEC is seen high
// before a low ~COLORAM counts as a 6510 colour RAM access, so that the
// CPLD's ~COLORAM of the VIC-II cycle, which rises after AEC, is not taken
// for one.
extern volatile uint32_t bus_aec_settle;

// Debug build (BUS_TRACE_PINS=1, firmware/build.sh TRACE_PINS=1): GPIO45 is
// high while core 1 serves an access, from seeing the select to letting go
// of the bus. The exerciser's trace shows it. The heartbeat is off in this
// build.
#ifndef BUS_TRACE_PINS
#define BUS_TRACE_PINS 0
#endif

// Rebuilds the tables derived from bus_ram and bus_rom (the VIC-II's view
// of RAM, the character ROM table), after `fill`.
void bus_rebuild_tables(void);

void bus_init_pins(void);
void bus_set_sync_bypass(bool on);
void bus_run(void);  // never returns

#ifdef __cplusplus
}
#endif

#endif
