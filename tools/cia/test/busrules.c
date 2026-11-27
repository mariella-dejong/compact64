// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Measures the bus rules the CIA firmware relies on (#182), on the 6510
// traffic of the bench exerciser's CPU mode (firmware/bench/exerciser/
// cia_cpu.c): floooh's cycle-exact 6502 core runs random programs, every
// opcode but the JAMs, against the 6526 model, with its interrupt output on
// ~IRQ (CIA 1) or ~NMI (CIA 2) and with badline and sprite RDY stalls.
//
// Checks that a CIA read never comes sooner than MIN_GAP cycles after a
// CIA write, that no write falls in a cycle in which the VIC-II has the
// bus, that every non-JAM opcode ran and that interrupts and RDY stalls
// happened; prints how often reads came in two cycles in a row.

#include <stdio.h>
#include <stdlib.h>

#include "cia6526.h"
#include "cia_cpu.h"

// A CIA write in cycle n: n + 1 fetches the next opcode, n + 2 and n + 3
// its operand bytes, so the earliest data read is in n + 4.
#define MIN_GAP 4u

static int jam(unsigned op) {
    return ((op & 0xFu) == 2u) && op != 0x82 && op != 0xA2 && op != 0xC2 && op != 0xE2;
}

int main(int argc, char **argv) {
    const unsigned seeds = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 0) : 200;
    const unsigned n = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 0) : 50000;
    cia_cpu_t *c = malloc(cia_cpu_size());
    if (!c) {
        return 1;
    }
    cia_cpu_stats_t tot = {0};
    tot.write_gap_min = UINT32_MAX;
    for (unsigned seed = 1; seed <= seeds; seed++) {
        const cia_cpu_cfg_t cfg = {.unit = 1u + (seed & 1u), .cia_pct = 20u + seed % 60u,
                                   .badlines = seed % 3u != 0, .sprites = seed % 4u != 0};
        cia_cpu_init(c, &cfg, seed);
        cia_t cia;
        cia_init(&cia, CIA_MODEL_6526);
        for (unsigned i = 0; i < n; i++) {
            const cia_cpu_bus_t b = cia_cpu_cycle(c);
            if (b.cia) {
                if (b.rw) {
                    cia_cpu_read_data(c, cia_read(&cia, b.reg));
                } else {
                    cia_write(&cia, b.reg, b.data);
                }
            }
            if (i % 997u == 0) {
                cia_tod_tick(&cia);
            }
            cia_clock(&cia);
            cia_cpu_set_irq(c, cia_irq(&cia));
        }
        const cia_cpu_stats_t *s = cia_cpu_stats(c);
        tot.cycles += s->cycles;
        tot.cia_reads += s->cia_reads;
        tot.cia_writes += s->cia_writes;
        tot.rdy_repeats += s->rdy_repeats;
        tot.redirects += s->redirects;
        tot.jams += s->jams;
        tot.interrupts += s->interrupts;
        tot.back_to_back += s->back_to_back;
        tot.aec_writes += s->aec_writes;
        if (s->write_gap_min < tot.write_gap_min) {
            tot.write_gap_min = s->write_gap_min;
        }
        for (unsigned w = 0; w < 8; w++) {
            tot.opcodes[w] |= s->opcodes[w];
        }
    }
    unsigned missing = 0;
    for (unsigned op = 0; op < 256; op++) {
        const int ran = (tot.opcodes[op >> 5] >> (op & 31u)) & 1u;
        if (!jam(op) && !ran) {
            printf("busrules: opcode %02X never ran\n", op);
            missing++;
        }
    }
    printf("busrules: %lu cycles, %lu CIA reads, %lu CIA writes, %lu reads in back-to-back cycles, "
           "%lu repeated in RDY stalls, %lu interrupt sequences, %lu fetches redirected from $D000-$DFFF, "
           "%lu JAMs skipped\n",
           (unsigned long)tot.cycles, (unsigned long)tot.cia_reads, (unsigned long)tot.cia_writes,
           (unsigned long)tot.back_to_back, (unsigned long)tot.rdy_repeats, (unsigned long)tot.interrupts,
           (unsigned long)tot.redirects, (unsigned long)tot.jams);
    printf("busrules: fewest cycles from a CIA write to the next CIA read: %lu (rule: at least %u)\n",
           (unsigned long)tot.write_gap_min, MIN_GAP);
    const int ok = tot.write_gap_min >= MIN_GAP && tot.aec_writes == 0 && !missing && tot.interrupts &&
                   tot.rdy_repeats && tot.back_to_back;
    if (tot.aec_writes) {
        printf("busrules: %lu writes while AEC was low\n", (unsigned long)tot.aec_writes);
    }
    printf(ok ? "busrules: pass\n" : "busrules: FAIL\n");
    free(c);
    return !ok;
}
