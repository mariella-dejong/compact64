// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Internals of the exerciser's generators, shared by gen.c (U1401, #159) and
// gen_cia.c (the CIA controllers, #182): the lists the state machines play,
// the event queue that fills them, the sample records and the timing.

#ifndef EXERCISER_GEN_INT_H
#define EXERCISER_GEN_INT_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "engine.h"
#include "gen.h"

#ifdef __cplusplus
extern "C" {
#endif

// A list word holds a state and how long it lasts.
typedef struct {
    uint32_t *buf;
    uint32_t cap, n;
    uint32_t last;   // tick at which `state` started
    uint32_t state;  // pending state
    uint32_t mask, min, max, bias;
    uint8_t shift;
} lst_t;

// What a sample checks.
typedef struct {
    uint16_t addr;
    uint8_t check;
    uint8_t exp_d, mask_d, exp_c, mask_c, exp_s, mask_s;
    uint32_t cycle;
} rec_t;

enum { EV_W, EV_D, EV_S };

typedef struct {
    uint32_t tick;
    uint8_t type;
    uint64_t mask, val;
    rec_t rec;
} ev_t;

#define QCAP 256

extern lst_t l_w0, l_w1, l_d;
extern uint32_t s_n, s_start;
extern uint32_t moved, max_move;
extern bool full;
extern ev_t q[QCAP];
extern unsigned qn;
extern rec_t recs[EX_S_WORDS];
extern uint32_t n_recs;
extern uint64_t pins;
extern uint32_t dstate;
extern uint32_t d_end;
extern float ks, kd;  // ticks per ns: cycle times (scaled), chip delays
extern uint32_t rng;

// Random numbers (xorshift32).
static inline uint32_t rnd(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

#define p (&ex_params)

static inline uint32_t at(uint32_t base, int32_t ns, float k) {
    return (uint32_t)((int32_t)base + (int32_t)lroundf((float)ns * k));
}
#define SC(base, ns) at((base), (ns), ks)
#define DL(base, ns) at((base), (ns), kd)

void lists_begin(uint32_t seed, uint64_t first);
void lists_end(uint32_t t_end, ex_lists_t *l);
void q_flush(uint32_t limit);
void w_set(uint32_t t, uint64_t mask, uint64_t val);
void w_low(uint32_t t, uint64_t mask);
void w_high(uint32_t t, uint64_t mask);
void d_set(uint32_t t, uint32_t vals, uint32_t dirs, uint32_t bits);
void sample(uint32_t t, const rec_t *r);

#ifdef __cplusplus
}
#endif

#endif
