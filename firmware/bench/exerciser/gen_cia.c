// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// CIA mode of the exerciser (#182): plays the 6510 towards a CIA controller
// (firmware/cia, bench build) and checks it, cycle by cycle, against its own
// copy of the 6526 model (Compact64Cia).
//
// A cycle is PHI2 low (phi1), then PHI2 high (phi2). In phi1 the exerciser
// sets A0 to A3 and R/~W, then ~CS for an access, and precharges D to the
// inverse of the expected value before a read; in phi2 it drives D for a
// write. Its inputs to the CIA (CNT, SP, ~FLAG, the bench TOD pin and
// ~RESET) change in phi1, so the DUT samples them at the falling edge of
// PHI2 as the model sees them. Every cycle has one sample at the 6510's latch
// point: D for a read, and ~IRQ, which must show the model's interrupt
// output after the previous cycle.
//
// The DUT and the model step in the same order each cycle: inputs, the
// access (a write, or the side effects of a read), the TOD edge, the clock.

#include "gen_int.h"

#include <string.h>

#include <c64bus_bench.h>
#include <c64cia_pins.h>
#include <cia6526.h>

#define CB(g) (1ull << (g))
#define CM_A (0xFull << CIA_GPIO_A0)
#define CM_CNT CB(CIA_GPIO_CNT)
#define CM_SP CB(CIA_GPIO_SP)
#define CM_TOD CB(CIA_GPIO_TOD_BENCH)
#define CM_RESET CB(CIA_GPIO_RESET)
#define CM_FLAG CB(CIA_GPIO_FLAG)
#define CM_PHI2 CB(CIA_GPIO_PHI2)
#define CM_RW CB(CIA_GPIO_RW)
#define CM_CS CB(CIA_GPIO_CS)

// ~IRQ in the second sampler's word (GPIO16 to GPIO31 in CIA mode).
#define RX1_IRQ (CIA_GPIO_IRQ - 16)

#define RESET_CYCLES 3

static cia_t model;

static uint64_t cia_idle_pins(void) {
    return CM_CNT | CM_SP | CM_RESET | CM_FLAG | CM_RW | CM_CS;  // PHI2 low
}

void ex_cia_go_idle(void) {
    ex_set_idle(cia_idle_pins());
}

static uint8_t check_of(unsigned reg) {
    switch (reg) {
    case CIA_PRA:
    case CIA_PRB:
        return CK_CIA_PORT;
    case CIA_TALO:
    case CIA_TAHI:
    case CIA_TBLO:
    case CIA_TBHI:
        return CK_CIA_TIMER;
    case CIA_TOD10:
    case CIA_TODSEC:
    case CIA_TODMIN:
    case CIA_TODHR:
        return CK_CIA_TOD;
    case CIA_ICR:
        return CK_CIA_ICR;
    default:
        return CK_CIA_REG;
    }
}

// A value to write: timer latches mostly small, so that the timers
// underflow often in a batch.
static uint8_t write_value(unsigned reg) {
    const uint32_t r = rnd();
    switch (reg) {
    case CIA_TALO:
    case CIA_TBLO:
        return (r & 3u) ? (uint8_t)((r >> 8) & 0x1Fu) : (uint8_t)(r >> 16);
    case CIA_TAHI:
    case CIA_TBHI:
        return (r & 7u) ? 0 : (uint8_t)((r >> 8) & 3u);
    default:
        return (uint8_t)(r >> 8);
    }
}

enum { A_IDLE, A_READ, A_WRITE };

static int pick_access(void) {
    const int32_t w[3] = {p->cia_w_idle > 0 ? p->cia_w_idle : 0, p->cia_w_read > 0 ? p->cia_w_read : 0,
                          p->cia_w_write > 0 ? p->cia_w_write : 0};
    const int32_t total = w[0] + w[1] + w[2];
    if (total <= 0) {
        return A_IDLE;
    }
    int32_t x = (int32_t)(rnd() % (uint32_t)total);
    for (int i = 0; i < 3; i++) {
        if (x < w[i]) {
            return i;
        }
        x -= w[i];
    }
    return A_IDLE;
}

// One cycle from s0 (PHI2 falls, phi1 starts) to s2 (the next fall).
static void gen_cycle(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t c) {
    const bool in_reset = c < RESET_CYCLES;

    // Inputs as they will be at the end of this cycle.
    const bool tod = p->cia_tod > 0 && (c % (uint32_t)p->cia_tod) == 0;
    const bool flag_low = p->cia_flag > 0 && (c % (uint32_t)p->cia_flag) == 0;
    const bool cnt = p->cia_cnt > 0 ? ((c / (uint32_t)p->cia_cnt) & 1u) == 0 : true;
    const uint32_t mid1 = DL(s0, 150);
    w_set(mid1, CM_TOD | CM_FLAG | CM_CNT | CM_RESET,
          (tod ? CM_TOD : 0) | (flag_low ? 0 : CM_FLAG) | (cnt ? CM_CNT : 0) | (in_reset ? 0 : CM_RESET));

    w_set(s1, CM_PHI2, CM_PHI2);
    w_set(s2, CM_PHI2, 0);

    // ~CS and the address: released shortly after the previous cycle,
    // then set for this one.
    w_set(DL(s0, 10), CM_CS, CM_CS);
    const int acc = in_reset ? A_IDLE : pick_access();
    const unsigned reg = rnd() & 0xFu;
    const uint32_t t_a = SC(s0, p->t_addr);
    w_set(t_a, CM_A | CM_RW, ((uint64_t)reg << CIA_GPIO_A0) | (acc == A_WRITE ? 0 : CM_RW));
    if (acc != A_IDLE) {
        w_set(DL(t_a, p->t_pla), CM_CS, 0);
    }

    rec_t r = {.check = CK_CIA_IRQ, .addr = (uint16_t)reg, .cycle = c};
    model.cnt_in = cnt;
    model.sp_in = true;
    model.flag_in = !flag_low;
    if (in_reset) {
        cia_reset(&model);
        return;  // no checks while ~RESET is low
    }
    r.exp_c = cia_irq(&model) ? 0 : 1;  // ~IRQ: low while active
    r.mask_c = 1;

    if (acc == A_READ) {
        const uint8_t v = cia_peek(&model, reg);
        r.check = check_of(reg);
        r.exp_d = v;
        r.mask_d = (r.check == CK_CIA_PORT && !p->cia_ports) ? 0 : 0xFF;
        // Precharge D to the inverse in phi1, released before PHI2 rises.
        const uint32_t pre = DL(s0, p->t_pre_start);
        const uint32_t pre_end = DL(s1, -p->t_pre_end);
        if ((int32_t)(pre_end - pre) >= 8 && (int32_t)(pre - (d_end + 4)) >= 0) {
            d_set(pre, (uint8_t)~v, 0xFF, 0xFF);
            d_set(pre_end, 0, 0, 0xFF);
        }
        cia_read_effects(&model, reg);
    } else if (acc == A_WRITE) {
        const uint8_t v = write_value(reg);
        d_set(SC(s1, p->t_dv), v, 0xFF, 0xFF);
        d_set(DL(s2, p->t_dhold), 0, 0, 0xFF);
        cia_write(&model, reg, v);
    }
    // The DUT sees a TOD edge when the pin is high in this cycle's sample
    // and was low in the last one; the pin goes low again next cycle.
    if (tod) {
        cia_tod_tick(&model);
    }
    cia_clock(&model);

    const uint32_t t_s = p->sample_mode ? DL(s1, p->sample_off) : SC(s2, -p->t_setup);
    sample(t_s, &r);
}

static uint32_t build_cia_batch(uint32_t seed, ex_lists_t *l, uint64_t *first) {
    rng = c64bench_hash32(seed) | 1u;
    *first = cia_idle_pins() & ~CM_RESET;
    lists_begin(seed, *first);
    rng = c64bench_hash32(seed) | 1u;
    cia_init(&model, p->cia_model ? CIA_MODEL_8521 : CIA_MODEL_6526);

    const uint32_t t0 = DL(0, 2000);
    const float half = (float)p->half_ns * ks;
    uint32_t c = 0;
    for (; c < (uint32_t)p->cycles; c++) {
        if (full || l_w0.n + 32 > l_w0.cap || l_d.n + 16 > l_d.cap || s_n + 4 > EX_S_WORDS) {
            break;
        }
        const uint32_t s0 = t0 + (uint32_t)lroundf((float)(2 * c) * half);
        const uint32_t s1 = t0 + (uint32_t)lroundf((float)(2 * c + 1) * half);
        const uint32_t s2 = t0 + (uint32_t)lroundf((float)(2 * c + 2) * half);
        gen_cycle(s0, s1, s2, c);
        q_flush(s1);
    }
    const uint32_t t_end = t0 + (uint32_t)lroundf((float)(2 * c) * half) + DL(0, 300);
    w_set(t_end, CM_CS | CM_RW | CM_TOD | CM_FLAG | CM_CNT, CM_CS | CM_RW | CM_FLAG | CM_CNT);
    d_set(t_end, 0, 0, 0xFF);
    lists_end(t_end, l);
    return c;
}

static void check_cia_batch(uint32_t batch, ex_stats_t *st) {
    for (uint32_t i = 0; i < n_recs; i++) {
        const rec_t *r = &recs[i];
        if (r->check == EX_TRACE_EXTRA) {
            continue;
        }
        const uint8_t got_d = (uint8_t)(ex_rx0[i] & 0xFFu);
        const uint8_t got_c = (uint8_t)((ex_rx1[i] >> RX1_IRQ) & 1u);
        const bool bad_d = ((got_d ^ r->exp_d) & r->mask_d) != 0;
        const bool bad_c = ((got_c ^ r->exp_c) & r->mask_c) != 0;
        st->samples++;
        if (r->check != CK_CIA_IRQ) {
            st->n[r->check]++;
            st->err[r->check] += bad_d;
        }
        st->n[CK_CIA_IRQ]++;
        st->err[CK_CIA_IRQ] += bad_c;
        if (bad_d || bad_c) {
            st->errors++;
            if (ex_failure_count < EX_MAX_FAILURES) {
                ex_failures[ex_failure_count++] = (ex_failure_t){
                    .batch = batch, .cycle = r->cycle, .check = bad_d ? r->check : CK_CIA_IRQ,
                    .addr = r->addr, .exp_d = r->exp_d, .got_d = got_d, .mask_d = r->mask_d,
                    .exp_c = r->exp_c, .got_c = got_c, .mask_c = r->mask_c};
            }
        }
    }
}

void ex_cia_run_batches(uint32_t batches, uint32_t seed, ex_stats_t *st) {
    ex_lists_t l;
    uint64_t first;
    ex_run_result_t res;
    for (uint32_t b = 0; b < batches; b++) {
        const uint32_t cycles = build_cia_batch(seed + b, &l, &first);
        st->moved += moved;
        if (max_move > st->max_move) {
            st->max_move = max_move;
        }
        if (full || cycles < (uint32_t)p->cycles) {
            st->overflow++;
        }
        if (full) {
            continue;
        }
        ex_run(&l, first, &res);
        st->batches++;
        st->cycles += cycles;
        st->tx_stall += res.tx_stall;
        st->rx_stall += res.rx_stall;
        st->timeout += res.timeout;
        if (!res.timeout && !res.tx_stall && !res.rx_stall) {
            check_cia_batch(seed + b, st);
        }
    }
    ex_cia_go_idle();
}
