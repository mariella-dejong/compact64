// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// 6510 traffic for the CIA bench test (#182): a cycle-exact 6502 core
// (floooh's m6502.h) runs random programs against the CIA under test, so
// that every access pattern on the bus is one a real 6510 makes, with its
// dummy reads, read-modify-write cycles, illegal opcodes, interrupt
// sequences and RDY stalls. Portable C: the exerciser uses it (gen_cia.c),
// and so does the host test tools/cia/test/busrules.c.
//
// Each cycle, cia_cpu_cycle() ticks the core once and says what it does on
// the bus. The caller performs the CIA side of the access (cia_peek and
// cia_read_effects, or cia_write, on its model), gives the core the data of
// a CIA read with cia_cpu_read_data(), steps the model and sets the
// interrupt line with cia_cpu_set_irq() for the next cycle.
//
// Memory: 64 KB of RAM with the CIA under test at $DC00 (CIA 1, ~IRQ) or
// $DD00 (CIA 2, ~NMI), its 16 registers mirrored through the page. The rest
// of $D000 to $DFFF is RAM too: the generator knows no other chip.
//
// Not covered, on purpose (firmware/bench/README.md, CIA mode): code run
// from $D000 to $DFFF, so from the CIA's registers, and DMA from the
// expansion port. An opcode fetch from there is replaced by a fetch from a
// random address in the code area, as if the instruction before had jumped
// there; the core never runs a single cycle of such code.

#ifndef EXERCISER_CIA_CPU_H
#define EXERCISER_CIA_CPU_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned unit;        // 1: CIA 1 at $DC00 on ~IRQ, 2: CIA 2 at $DD00 on ~NMI
    unsigned cia_pct;     // percent of memory operands aimed at the CIA
    bool badlines;        // RDY stalls like a VIC-II badline every 8 raster lines
    bool sprites;         // short RDY stalls like sprite DMA
} cia_cpu_cfg_t;

// One bus cycle.
typedef struct {
    uint16_t addr;   // the 6510's address (also when the CIA is not selected)
    bool rw;         // high: read
    bool aec;        // the 6510 has the bus in phi2; low: the VIC-II has it
    bool cia;        // the CIA is selected: AEC high and the address in its page
    uint8_t reg;     // CIA register, if selected
    uint8_t data;    // the data of a write
    bool sync;       // an opcode fetch
    bool rdy_wait;   // a read the core repeats because RDY is low
} cia_cpu_bus_t;

typedef struct {
    uint32_t cycles;
    uint32_t cia_reads, cia_writes;
    uint32_t rdy_repeats;   // CIA reads repeated in an RDY stall
    uint32_t redirects;     // opcode fetches moved out of $D000 to $DFFF
    uint32_t jams;          // JAM opcodes skipped (the core would stop)
    uint32_t interrupts;    // interrupt sequences started (vector fetches)
    uint32_t back_to_back;  // CIA reads in two cycles in a row
    uint32_t aec_writes;    // writes while AEC was low (a 6510 never makes them)
    uint32_t write_gap_min; // fewest cycles from a CIA write to the next CIA read
    uint32_t opcodes[8];    // bitmap of the opcodes executed
} cia_cpu_stats_t;

typedef struct cia_cpu cia_cpu_t;

// The state is large (64 KB of memory); the caller provides at least
// CIA_CPU_BYTES, word aligned (cia_cpu_size() is the exact size).
#define CIA_CPU_BYTES (65536u + 1024u)
uint32_t cia_cpu_size(void);
// Fills memory with a random program from seed and resets the core: the
// first 7 cycles are its reset sequence.
void cia_cpu_init(cia_cpu_t *c, const cia_cpu_cfg_t *cfg, uint32_t seed);
// The next bus cycle.
cia_cpu_bus_t cia_cpu_cycle(cia_cpu_t *c);
// The data the CIA returns for this cycle's read.
void cia_cpu_read_data(cia_cpu_t *c, uint8_t value);
// The CIA's interrupt output as the core sees it in the next cycle.
void cia_cpu_set_irq(cia_cpu_t *c, bool active);
const cia_cpu_stats_t *cia_cpu_stats(const cia_cpu_t *c);

#ifdef __cplusplus
}
#endif

#endif
