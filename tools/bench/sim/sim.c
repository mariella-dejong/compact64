// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Host simulation of the bench test (#159), without hardware. It links the
// exerciser's cycle generator and checker (gen.c) with a model of the PIO
// lists in place of engine.c, and a behavioural model of the U1401 firmware
// that sees every input `latency` clocks late and keeps driving that much
// longer. It checks that:
//
//   - an ideal firmware passes every check at every speed in sample mode 0;
//   - a slow firmware fails, and the sample point sweep finds its latency;
//   - the release check sees a firmware that lets go of D0 to D7 too late;
//   - with a firmware that never drives D, every read sees the precharge;
//   - the trace adds samples without moving the checked ones.
//
// Build and run: make -C tools/bench/sim

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c64bus_bench.h>
#include <c64bus_pins.h>

#include "engine.h"
#include "gen.h"

// ---------------------------------------------------------------------------
// Stand-in for engine.c.

uint32_t ex_w0[EX_W0_WORDS];
uint32_t ex_w1[EX_W1_WORDS];
uint32_t ex_d[EX_D_WORDS];
uint32_t ex_s[EX_S_WORDS];
uint32_t ex_rx0[EX_S_WORDS];
uint32_t ex_rx1[EX_S_WORDS];

static const uint32_t sys_hz = 250000000u;
static uint32_t latency = 2;  // DUT latency in exerciser clocks
static bool dut_silent;       // the DUT never drives D0 to D7

uint32_t ex_sys_hz(void) {
    return sys_hz;
}

void ex_engine_init(void) {
}

void ex_set_mark(bool on) {
    (void)on;
}

bool ex_mark_enabled(void) {
    return false;
}

void ex_set_idle(uint64_t pins) {
    (void)pins;
}

// U1401 model state.
static uint8_t dut_ram[C64_RAM_SIZE], dut_color[C64_COLOR_SIZE], dut_rom[C64_ROM_SIZE];

#define BIT(v, g) (((v) >> (g)) & 1u)

typedef struct {
    bool wpend, cpend;
    uint16_t waddr, caddr;
    uint8_t wdata, cdata;
} dut_state_t;

// One clock of the U1401 model: inputs as seen `latency` clocks ago.
static void dut_step(uint64_t in, uint8_t dbus, dut_state_t *s, uint8_t *oe, uint8_t *val, uint8_t *colord) {
    const bool aec = BIT(in, C64_GPIO_AEC), rw = BIT(in, C64_GPIO_RW), phi2 = BIT(in, C64_GPIO_PHI2);
    const bool casram = !BIT(in, C64_GPIO_CASRAM), coloram = !BIT(in, C64_GPIO_COLORAM);
    const bool basic = !BIT(in, C64_GPIO_BASIC), kernal = !BIT(in, C64_GPIO_KERNAL);
    const bool charom = !BIT(in, C64_GPIO_CHAROM), grw = !BIT(in, C64_GPIO_GRW);
    const uint32_t a = (uint32_t)(in >> C64_GPIO_A0) & 0xFFFFu;
    *oe = 0;

    if (s->wpend && !(aec && casram && !rw && phi2)) {
        dut_ram[s->waddr] = s->wdata;
        s->wpend = false;
    }
    if (s->cpend && !(aec && coloram && grw && phi2)) {
        dut_color[s->caddr] = s->cdata & 0x0Fu;
        s->cpend = false;
    }

    if (!aec) {
        *colord = dut_color[a & 0x3FFu];
        if (casram) {
            const uint32_t va = (a & 0x0FFFu) | (BIT(in, C64_GPIO_VA4) << 12) | (BIT(in, C64_GPIO_VA5) << 13) |
                                ((BIT(in, C64_GPIO_VA14N) ^ 1u) << 14) | ((BIT(in, C64_GPIO_VA15N) ^ 1u) << 15);
            *val = dut_ram[va];
            *oe = 0xFF;
        } else if (charom) {
            *val = dut_rom[C64_ROM_CHAR + (a & 0x0FFFu)];
            *oe = 0xFF;
        }
        return;
    }
    if (casram && rw) {
        *val = dut_ram[a];
        *oe = 0xFF;
    } else if (casram) {
        if (phi2) {
            s->wpend = true;
            s->waddr = (uint16_t)a;
            s->wdata = dbus;
        }
    } else if (coloram && rw) {
        *val = dut_color[a & 0x3FFu];
        *oe = 0x0F;
    } else if (coloram) {
        if (grw && phi2) {
            s->cpend = true;
            s->caddr = (uint16_t)(a & 0x3FFu);
            s->cdata = dbus;
        }
    } else if (rw && (kernal || basic || charom)) {
        *val = kernal ? dut_rom[C64_ROM_KERNAL + (a & 0x1FFFu)]
             : basic  ? dut_rom[C64_ROM_BASIC + (a & 0x1FFFu)]
                      : dut_rom[C64_ROM_CHAR + (a & 0x0FFFu)];
        *oe = 0xFF;
    }
}

static dut_state_t dut;
static uint8_t dut_colord;

void ex_run(const ex_lists_t *l, uint64_t first, ex_run_result_t *r) {
    r->tx_stall = r->rx_stall = r->timeout = false;
    const uint32_t n = l->total_ticks + 1;
    uint64_t *out = malloc(n * sizeof(uint64_t));
    uint8_t *dv = calloc(n, 1), *dd = calloc(n, 1), *bus = calloc(n, 1), *dside = calloc(n, 1), *cd = calloc(n, 1);
    if (!out || !dv || !dd || !bus || !dside || !cd) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }

    // W0 and W1: the word taken at tick t sets the pins from t on.
    for (uint32_t t = 0; t < n; t++) {
        out[t] = first;
    }
    uint32_t t = 0;
    for (uint32_t i = 0; i < l->n_w0; i++) {
        const uint32_t dur = (ex_w0[i] >> 24) + 3u;
        const uint64_t v = (uint64_t)(ex_w0[i] & 0xFFFFFFu) << 8;
        for (uint32_t k = t; k < t + dur && k < n; k++) {
            out[k] = (out[k] & ~(0xFFFFFFull << 8)) | v;
        }
        t += dur;
    }
    if (t != l->total_ticks - 1) {
        printf("  W0 list lasts %u clocks, expected %u\n", t, l->total_ticks - 1);
    }
    t = 0;
    for (uint32_t i = 0; i < l->n_w1; i++) {
        const uint32_t dur = (ex_w1[i] >> 16) + 3u;
        const uint64_t v = (uint64_t)(ex_w1[i] & 0xFFFFu) << 32;
        for (uint32_t k = t; k < t + dur && k < n; k++) {
            out[k] = (out[k] & ~(0xFFFFull << 32)) | v;
        }
        t += dur;
    }
    if (t != l->total_ticks - 1) {
        printf("  W1 list lasts %u clocks, expected %u\n", t, l->total_ticks - 1);
    }
    // data8 runs `out pins` one clock before `out pindirs`: for that clock
    // the new values go out with the old directions.
    t = 0;
    uint8_t dirs = 0;
    for (uint32_t i = 0; i < l->n_d; i++) {
        const uint32_t dur = (ex_d[i] >> 16) + 4u;
        for (uint32_t k = t; k < t + dur && k < n; k++) {
            dv[k] = (uint8_t)ex_d[i];
            dd[k] = k == t ? dirs : (uint8_t)(ex_d[i] >> 8);
        }
        dirs = (uint8_t)(ex_d[i] >> 8);
        t += dur;
    }
    if (t != l->total_ticks - 1) {
        printf("  D list lasts %u clocks, expected %u\n", t, l->total_ticks - 1);
    }

    // The bus: the exerciser wins where it drives (its side of the series
    // resistors), then the DUT, else the bus keeps its charge.
    uint8_t prev = 0;
    for (uint32_t k = 0; k < n; k++) {
        uint8_t oe = 0, val = 0;
        const uint32_t kin = k >= latency ? k - latency : 0;
        dut_step(out[kin], bus[kin], &dut, &oe, &val, &dut_colord);
        if (dut_silent) {
            oe = 0;
        }
        uint8_t b = (uint8_t)((prev & ~(oe | dd[k])) | (val & oe & ~dd[k]) | (dv[k] & dd[k]));
        bus[k] = b;
        dside[k] = (uint8_t)((val & oe) | (b & ~oe));
        cd[k] = dut_colord;
        prev = b;
    }

    // Samplers.
    uint32_t s = 0;
    for (uint32_t i = 0; i < l->n_s; i++) {
        const uint32_t ts = s + ex_s[i] + 2u;
        const uint32_t k = ts < n ? ts : n - 1;
        ex_rx0[i] = bus[k] | (uint32_t)((out[k] >> 8) & 0xFFu) << 8;
        ex_rx1[i] = (uint32_t)(cd[k] << 4) | ((dside[k] & 1u) << 10) | (((dside[k] >> 7) & 1u) << 11);
        s = ts + 1u;
    }
    free(out);
    free(dv);
    free(dd);
    free(bus);
    free(dside);
    free(cd);
}

// ---------------------------------------------------------------------------
// Scenarios.

static void reset_all(void) {
    c64bench_fill(1, dut_ram, dut_color, dut_rom);
    memset(&dut, 0, sizeof(dut));
    ex_model_fill(1);
}

static ex_stats_t run(uint32_t batches, uint32_t seed) {
    ex_stats_t st;
    memset(&st, 0, sizeof(st));
    ex_failure_count = 0;
    ex_run_batches(batches, seed, &st);
    return st;
}

static void print_stats(const char *title, const ex_stats_t *st) {
    printf("%s: batches=%u cycles=%u samples=%u errors=%u moved=%u max_move=%u overflow=%u\n", title,
           st->batches, st->cycles, st->samples, st->errors, st->moved, st->max_move, st->overflow);
    for (unsigned c = 0; c < CK_COUNT; c++) {
        if (st->n[c]) {
            printf("    %-15s n=%-6u err=%u\n", ex_check_names[c], st->n[c], st->err[c]);
        }
    }
}

static int failures;

static void expect(bool ok, const char *what) {
    printf("  %s: %s\n", ok ? "PASS" : "FAIL", what);
    failures += !ok;
}

int main(void) {
    ex_params_default_pal();
    ex_params.weight[K_C_RELEASE] = 1;

    // 1. An ideal firmware (2 clocks, 8 ns) passes at 100 % and 150 %.
    for (int speed = 100; speed <= 150; speed += 50) {
        reset_all();
        latency = 2;
        ex_params.speed = speed;
        char title[64];
        snprintf(title, sizeof(title), "ideal DUT, speed %d %%", speed);
        const ex_stats_t st = run(8, 1);
        print_stats(title, &st);
        expect(st.errors == 0 && st.max_move <= 2 && st.overflow == 0 && st.samples > 1000,
               "no errors, edges moved by at most 2 clocks");
    }

    // 2. A firmware with 120 ns latency (30 clocks) fails RAM reads that are
    // sampled 100 ns after ~CASRAM, and passes when sampled at 140 ns.
    ex_params.speed = 100;
    ex_params.sample_mode = 1;
    ex_params.weight[K_C_RELEASE] = 0;
    for (int off = 100; off <= 140; off += 40) {
        reset_all();
        latency = 30;
        ex_params.sample_off = off;
        char title[64];
        snprintf(title, sizeof(title), "120 ns DUT, sample %d ns after the select", off);
        const ex_stats_t st = run(4, 2);
        print_stats(title, &st);
        if (off == 100) {
            expect(st.err[CK_RAM_R] == st.n[CK_RAM_R] && st.n[CK_RAM_R] > 0, "every RAM read fails");
        } else {
            expect(st.err[CK_RAM_R] == 0 && st.err[CK_VIC_RAM] == 0, "RAM reads pass");
        }
    }

    // 3. The sweep finds the latency: the first passing offset is within a
    // clock or two of 120 ns.
    reset_all();
    latency = 30;
    int first_pass = -1;
    for (int off = 80; off <= 160; off += 4) {
        ex_params.sample_off = off;
        const ex_stats_t st = run(1, 3);
        if (st.err[CK_RAM_R] == 0 && st.n[CK_RAM_R] > 0) {
            first_pass = off;
            break;
        }
    }
    printf("sweep: first passing sample offset %d ns for a 120 ns DUT\n", first_pass);
    expect(first_pass >= 116 && first_pass <= 132, "sweep finds the latency");

    // 4. Release check: 8 ns passes; 80 ns is still driving at 40 ns.
    ex_params.sample_mode = 0;
    ex_params.weight[K_C_RELEASE] = 4;
    for (uint32_t lat = 2; lat <= 20; lat += 18) {
        reset_all();
        latency = lat;
        const ex_stats_t st = run(4, 4);
        char title[64];
        snprintf(title, sizeof(title), "release check, DUT latency %u ns", lat * 4);
        print_stats(title, &st);
        if (lat == 2) {
            expect(st.err[CK_RELEASE] == 0 && st.n[CK_RELEASE] > 0, "release passes");
        } else {
            expect(st.err[CK_RELEASE] == st.n[CK_RELEASE] && st.n[CK_RELEASE] > 0, "release fails");
        }
    }

    // 5. At 200 % the 120 ns DUT fails in sample mode 0 (latch point).
    reset_all();
    latency = 30;
    ex_params.speed = 200;
    ex_params.weight[K_C_RELEASE] = 0;
    {
        const ex_stats_t st = run(4, 5);
        print_stats("120 ns DUT at 200 %", &st);
        expect(st.err[CK_RAM_R] > 0, "fails at double speed");
    }

    // 6. A DUT that never drives D: every read fails and sees the inverse of
    // the expected data, which the precharge left on the bus.
    reset_all();
    latency = 2;
    dut_silent = true;
    ex_params.speed = 100;
    {
        const ex_stats_t st = run(4, 6);
        print_stats("silent DUT", &st);
        bool inverse = ex_failure_count > 0;
        for (unsigned i = 0; i < ex_failure_count; i++) {
            const ex_failure_t *f = &ex_failures[i];
            inverse &= ((f->got_d ^ (uint8_t)~f->exp_d) & f->mask_d) == 0;
        }
        expect(st.err[CK_RAM_R] == st.n[CK_RAM_R] && st.n[CK_RAM_R] > 0, "every RAM read fails");
        expect(inverse, "the bus holds the inverse of the expected data");
    }
    dut_silent = false;

    // 7. The trace adds samples without moving the checked ones: one batch
    // with and without it gives the same checks, and the entries are in time
    // order.
    reset_all();
    latency = 2;
    ex_params.sample_mode = 0;
    {
        const ex_stats_t plain = run(1, 7);
        reset_all();
        ex_stats_t st;
        memset(&st, 0, sizeof(st));
        ex_failure_count = 0;
        ex_trace_batch(7, 3, 4, 0, &st);
        print_stats("trace, 4 cycles every 3 clocks", &st);
        bool ordered = true, checked = false;
        for (unsigned i = 1; i < ex_trace_n; i++) {
            ordered &= (int32_t)(ex_trace[i].tick - ex_trace[i - 1].tick) >= 3;
            checked |= ex_trace[i].kind != EX_TRACE_EXTRA;
        }
        printf("    %u entries\n", ex_trace_n);
        expect(st.errors == 0 && st.samples == plain.samples && st.moved == plain.moved,
               "checks as without the trace");
        expect(ex_trace_n > 250 && ordered && checked, "entries in time order, with the checked samples");
    }

    printf("%s (%d failed)\n", failures ? "FAILED" : "all passed", failures);
    return failures ? 1 : 0;
}
