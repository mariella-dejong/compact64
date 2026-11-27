// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Checks the fast paths of the 6526 model (quiet cycles, cia_clock_quiet,
// and the usual timer cases in cia_clock_full): runs random accesses and
// inputs through the model and through a reference build without them
// (ref_cia.c), and compares the whole state, every register and every
// output after each cycle. The accesses favour what
// makes timers, interrupts and the serial port busy: small latches, starts
// and stops in every mode, one-shot, force loads, interrupt masks, CNT and
// ~FLAG edges, TOD ticks and alarms.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cia6526.h"

void ref_cia_init(cia_t *cia, cia_model_t model);
uint8_t ref_cia_peek(const cia_t *cia, unsigned reg);
void ref_cia_read_effects(cia_t *cia, unsigned reg);
void ref_cia_write(cia_t *cia, unsigned reg, uint8_t value);
void ref_cia_tod_tick(cia_t *cia);
void ref_cia_clock_full(cia_t *cia);
uint8_t ref_cia_pb_out(const cia_t *cia);

static uint32_t rng;

static uint32_t rnd(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static uint8_t value_for(unsigned reg) {
    const uint32_t r = rnd();
    switch (reg) {
    case CIA_TALO:
    case CIA_TBLO:
        return (r & 3u) ? (uint8_t)((r >> 8) & 0x0Fu) : (uint8_t)(r >> 8);
    case CIA_TAHI:
    case CIA_TBHI:
        return (r & 7u) ? 0 : (uint8_t)((r >> 8) & 1u);
    case CIA_CRA:
    case CIA_CRB:
        // Mostly started; TOD bits rarely, as they change TOD writes.
        return (uint8_t)(((r >> 8) & 0x7Fu) | ((r & 0x10u) ? 0x01 : 0) | ((r & 0x3E0u) ? 0 : 0x80));
    default:
        return (uint8_t)(r >> 8);
    }
}

// Copies of the state without the quiet-cycle fields, which only the model
// under test uses.
static void strip(const cia_t *c, cia_t *out) {
    memcpy(out, c, sizeof(*out));
    out->quiet = 0;
    out->quiet_dec_a = 0;
    out->quiet_dec_b = 0;
}

static int compare(const cia_t *a, const cia_t *b, unsigned seed, unsigned cycle) {
    cia_t sa, sb;
    strip(a, &sa);
    strip(b, &sb);
    if (memcmp(&sa, &sb, sizeof(sa)) != 0) {
        printf("seed %u cycle %u: state differs\n", seed, cycle);
        return 1;
    }
    for (unsigned r = 0; r < 16; r++) {
        if (cia_peek(a, r) != ref_cia_peek(b, r)) {
            printf("seed %u cycle %u: register %u differs\n", seed, cycle, r);
            return 1;
        }
    }
    if (cia_irq(a) != b->irq_pin || cia_pb_out(a) != ref_cia_pb_out(b) || a->pc != b->pc) {
        printf("seed %u cycle %u: outputs differ\n", seed, cycle);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const unsigned seeds = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 0) : 400;
    const unsigned n = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 0) : 20000;
    unsigned long quiet = 0, total = 0;
    int failed = 0;
    for (unsigned seed = 1; seed <= seeds && !failed; seed++) {
        rng = seed * 2654435761u | 1u;
        // Each seed has its own mix: how busy the bus is and how often the
        // inputs move, so that some runs stay quiet for long stretches.
        const uint32_t access_in = 2u + (rnd() % 64u);
        const uint32_t edge_in = 4u + (rnd() % 400u);
        const cia_model_t model = (seed & 1) ? CIA_MODEL_6526 : CIA_MODEL_8521;
        cia_t a, b;
        cia_init(&a, model);
        ref_cia_init(&b, model);
        bool cnt = true, flag = true;
        for (unsigned c = 0; c < n && !failed; c++) {
            if (rnd() % edge_in == 0) {
                cnt = !cnt;
            }
            flag = rnd() % edge_in != 0;
            a.cnt_in = b.cnt_in = cnt;
            a.flag_in = b.flag_in = flag;
            a.sp_in = b.sp_in = rnd() & 1u;
            if (rnd() % access_in == 0) {
                const unsigned reg = rnd() & 0xFu;
                if (rnd() & 1u) {
                    const uint8_t v = value_for(reg);
                    cia_write(&a, reg, v);
                    ref_cia_write(&b, reg, v);
                } else {
                    if (cia_peek(&a, reg) != ref_cia_peek(&b, reg)) {
                        printf("seed %u cycle %u: read of %u differs\n", seed, c, reg);
                        failed = 1;
                    }
                    cia_read_effects(&a, reg);
                    ref_cia_read_effects(&b, reg);
                }
            }
            if (rnd() % (edge_in * 4u) == 0) {
                cia_tod_tick(&a);
                ref_cia_tod_tick(&b);
            }
            if (cia_clock_quiet(&a)) {
                quiet++;
            } else {
                cia_clock_full(&a);
            }
            ref_cia_clock_full(&b);
            total++;
            failed |= compare(&a, &b, seed, c);
        }
    }
    printf("quiet: %lu of %lu cycles took the fast path (%.0f %%)\n", quiet, total,
           total ? 100.0 * (double)quiet / (double)total : 0.0);
    printf(failed ? "quiet: FAIL\n" : "quiet: all cycles match the reference\n");
    return failed;
}
