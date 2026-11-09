// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors

#include "gen.h"

#include <math.h>
#include <string.h>

#include <c64bus_bench.h>
#include <c64bus_pins.h>

#pragma GCC optimize("O2")

// ---------------------------------------------------------------------------
// Pins, as 64-bit GPIO masks.

#define B(g)      (1ull << (g))
#define M_A       (0xFFFFull << C64_GPIO_A0)
#define M_CASRAM  B(C64_GPIO_CASRAM)
#define M_BASIC   B(C64_GPIO_BASIC)
#define M_KERNAL  B(C64_GPIO_KERNAL)
#define M_CHAROM  B(C64_GPIO_CHAROM)
#define M_COLORAM B(C64_GPIO_COLORAM)
#define M_RW      B(C64_GPIO_RW)
#define M_GRW     B(C64_GPIO_GRW)
#define M_VA4     B(C64_GPIO_VA4)
#define M_VA5     B(C64_GPIO_VA5)
#define M_VA14N   B(C64_GPIO_VA14N)
#define M_VA15N   B(C64_GPIO_VA15N)
#define M_AEC     B(C64_GPIO_AEC)
#define M_MARK    B(EX_GPIO_MARK)
#define M_PHI2    B(C64_GPIO_PHI2)
#define M_CAS     B(C64_GPIO_CAS)
#define M_SELECTS (M_CASRAM | M_BASIC | M_KERNAL | M_CHAROM | M_COLORAM)

// ---------------------------------------------------------------------------
// Parameters.

ex_params_t ex_params;

const char *const ex_check_names[CK_COUNT] = {
    "ram_read", "ram_readback", "basic", "kernal", "char", "color_read", "color_readback",
    "vic_ram", "vic_char", "cacc_ram", "cacc_color", "release",
};

#define P(name) {#name, &ex_params.name}
#define W(name, kind) {"w_" #name, &ex_params.weight[kind]}
const ex_param_ref_t ex_param_refs[] = {
    P(half_ns), P(t_cas), P(t_addr), P(t_vaddr), P(t_dv), P(t_setup),
    P(t_cas_rise), P(d_casram), P(t_col), P(t_pla), P(t_dhold),
    P(t_pre_start), P(t_pre_end), P(t_rel_drive), P(t_rel_sample),
    P(sample_mode), P(sample_off), P(speed), P(cycles), P(bank),
    W(v_refresh, K_V_REFRESH), W(v_ram, K_V_RAM), W(v_char, K_V_CHAR),
    W(ram_r, K_C_RAM_R), W(ram_w, K_C_RAM_W), W(basic, K_C_BASIC), W(kernal, K_C_KERNAL),
    W(char, K_C_CHAR), W(col_r, K_C_COL_R), W(col_w, K_C_COL_W), W(idle, K_C_IDLE),
    W(release, K_C_RELEASE), W(readback, K_C_READBACK), W(cacc, K_B_CACC),
};
const unsigned ex_param_count = sizeof(ex_param_refs) / sizeof(ex_param_refs[0]);

// Starting values: PAL (8565, 985.248 kHz). The edge times are estimates
// from the datasheets and the README timing notes, not measurements; set
// them from the prototype (#158) when it exists.
void ex_params_default_pal(void) {
    memset(&ex_params, 0, sizeof(ex_params));
    ex_params.half_ns = 507;
    ex_params.t_cas = 260;
    ex_params.t_addr = 100;
    ex_params.t_vaddr = 200;
    ex_params.t_dv = 200;
    ex_params.t_setup = 100;
    ex_params.t_cas_rise = 0;
    ex_params.d_casram = 25;
    ex_params.t_col = 10;
    ex_params.t_pla = 15;
    ex_params.t_dhold = 30;
    ex_params.t_pre_start = 60;
    ex_params.t_pre_end = 20;
    ex_params.t_rel_drive = 5;
    ex_params.t_rel_sample = 40;
    ex_params.sample_mode = 0;
    ex_params.sample_off = 100;
    ex_params.speed = 100;
    ex_params.cycles = 512;
    ex_params.bank = 4;
    const int32_t w[K_COUNT] = {
        [K_V_REFRESH] = 1, [K_V_RAM] = 6, [K_V_CHAR] = 2,
        [K_C_RAM_R] = 4, [K_C_RAM_W] = 3, [K_C_BASIC] = 1, [K_C_KERNAL] = 2, [K_C_CHAR] = 1,
        [K_C_COL_R] = 1, [K_C_COL_W] = 1, [K_C_IDLE] = 1, [K_C_RELEASE] = 0,
        [K_C_READBACK] = 2, [K_B_CACC] = 2,
    };
    memcpy(ex_params.weight, w, sizeof(w));
}

// NTSC (8562, 1.023 MHz).
void ex_params_ntsc(void) {
    ex_params.half_ns = 489;
}

// ---------------------------------------------------------------------------
// Memory model.

static uint8_t model_ram[C64_RAM_SIZE];
static uint8_t model_color[C64_COLOR_SIZE];
static uint32_t model_seed = 1;

void ex_model_fill(uint32_t seed) {
    model_seed = seed;
    for (uint32_t a = 0; a < C64_RAM_SIZE; a++) {
        model_ram[a] = c64bench_byte(seed, C64BENCH_REGION_RAM, a);
    }
    for (uint32_t a = 0; a < C64_COLOR_SIZE; a++) {
        model_color[a] = c64bench_byte(seed, C64BENCH_REGION_COLOR, a) & 0x0Fu;
    }
}

uint8_t ex_model_ram(uint16_t addr) {
    return model_ram[addr];
}

static uint8_t rom_byte(uint32_t offset) {
    return c64bench_byte(model_seed, C64BENCH_REGION_ROM, offset);
}

// Recent writes, for read-backs.
static uint16_t wq_ram[16], wq_col[16];
static unsigned wq_ram_n, wq_col_n, wq_ram_i, wq_col_i;

// ---------------------------------------------------------------------------
// Random numbers (xorshift32).

static uint32_t rng = 1;

static inline uint32_t rnd(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

// ---------------------------------------------------------------------------
// Lists. A list word holds a state and how long it lasts; words are written
// when the next change is known.

typedef struct {
    uint32_t *buf;
    uint32_t cap, n;
    uint32_t last;   // tick at which `state` started
    uint32_t state;  // pending state
    uint32_t mask, min, max, bias;
    uint8_t shift;
} lst_t;

static lst_t l_w0, l_w1, l_d;
static uint32_t s_n, s_start;  // sampler list
static uint32_t moved, max_move;  // edges moved to keep a list's minimum hold
static bool full;

static void note_move(uint32_t ticks) {
    moved++;
    if (ticks > max_move) {
        max_move = ticks;
    }
}

static void lst_put(lst_t *l, uint32_t state, uint32_t dur) {
    if (l->n >= l->cap) {
        full = true;
        return;
    }
    l->buf[l->n++] = (state & l->mask) | ((dur - l->bias) << l->shift);
}

static void lst_emit(lst_t *l, uint32_t state, uint32_t dur) {
    while (dur > l->max) {
        uint32_t chunk = l->max;
        if (dur - chunk < l->min) {
            chunk = dur - l->min;
        }
        lst_put(l, state, chunk);
        dur -= chunk;
    }
    lst_put(l, state, dur);
}

static void lst_change(lst_t *l, uint32_t tick, uint32_t state) {
    state &= l->mask;
    if (state == l->state) {
        return;
    }
    if ((int32_t)(tick - l->last) <= 0) {
        if (tick != l->last) {
            note_move(l->last - tick);  // behind an edge that was itself moved
        }
        l->state = state;  // same tick: merges with the pending change
        return;
    }
    uint32_t dur = tick - l->last;
    if (dur < l->min) {
        note_move(l->min - dur);
        dur = l->min;
        tick = l->last + dur;
    }
    lst_emit(l, l->state, dur);
    l->last = tick;
    l->state = state;
}

// Sampler: block k waits x + 2 clocks after the previous block ends, then
// samples, and ends one clock later.
static void sampler_add(uint32_t tick) {
    if (s_n >= EX_S_WORDS) {
        full = true;
        return;
    }
    if ((int32_t)(tick - (s_start + 2)) < 0) {
        note_move(s_start + 2 - tick);
        tick = s_start + 2;
    }
    ex_s[s_n++] = tick - s_start - 2;
    s_start = tick + 1;
}

// ---------------------------------------------------------------------------
// Events, sorted by time.

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
static ev_t q[QCAP];
static unsigned qn;
static rec_t recs[EX_S_WORDS];
static uint32_t n_recs;
static uint64_t pins;      // exerciser outputs on GPIO8 to GPIO47
static uint32_t dstate;    // D0 to D7: values | directions << 8
static uint32_t d_end;     // latest D event so far

static void q_add(const ev_t *e) {
    if (qn >= QCAP) {
        full = true;
        return;
    }
    unsigned i = qn++;
    while (i > 0 && (int32_t)(q[i - 1].tick - e->tick) > 0) {
        q[i] = q[i - 1];
        i--;
    }
    q[i] = *e;
}

// Applies every event before `limit`.
static void q_flush(uint32_t limit) {
    unsigned i = 0;
    for (; i < qn && (int32_t)(q[i].tick - limit) < 0; i++) {
        const ev_t *e = &q[i];
        switch (e->type) {
        case EV_W:
            pins = (pins & ~e->mask) | (e->val & e->mask);
            lst_change(&l_w0, e->tick, (uint32_t)(pins >> 8));
            lst_change(&l_w1, e->tick, (uint32_t)(pins >> 32));
            break;
        case EV_D:
            dstate = (dstate & ~(uint32_t)e->mask) | ((uint32_t)e->val & (uint32_t)e->mask);
            lst_change(&l_d, e->tick, dstate);
            break;
        default:
            if (n_recs < EX_S_WORDS) {
                recs[n_recs++] = e->rec;
            }
            sampler_add(e->tick);
            break;
        }
    }
    memmove(q, q + i, (qn - i) * sizeof(ev_t));
    qn -= i;
}

static void w_set(uint32_t t, uint64_t mask, uint64_t val) {
    ev_t e = {.tick = t, .type = EV_W, .mask = mask, .val = val};
    q_add(&e);
}

static void w_low(uint32_t t, uint64_t mask) {
    w_set(t, mask, 0);
}

static void w_high(uint32_t t, uint64_t mask) {
    w_set(t, mask, mask);
}

static void a_set(uint32_t t, uint32_t addr) {
    w_set(t, M_A, (uint64_t)(addr & 0xFFFFu) << C64_GPIO_A0);
}

static void va_set(uint32_t t, uint32_t va4, uint32_t va5) {
    w_set(t, M_VA4 | M_VA5, ((va4 & 1u) ? M_VA4 : 0) | ((va5 & 1u) ? M_VA5 : 0));
}

// Sets the bits `bits` of D0 to D7: values and directions (1 = drive).
static void d_set(uint32_t t, uint32_t vals, uint32_t dirs, uint32_t bits) {
    bits &= 0xFFu;
    ev_t e = {.tick = t, .type = EV_D, .mask = bits | (bits << 8),
              .val = (vals & bits) | ((dirs & bits) << 8)};
    q_add(&e);
    if ((int32_t)(t - d_end) > 0) {
        d_end = t;
    }
}

static void sample(uint32_t t, const rec_t *r) {
    ev_t e = {.tick = t, .type = EV_S, .rec = *r};
    q_add(&e);
}

// ---------------------------------------------------------------------------
// Timing.

static float ks, kd;  // ticks per ns: cycle times (scaled), chip delays
static uint32_t bank;
#define p (&ex_params)

static inline uint32_t at(uint32_t base, int32_t ns, float k) {
    return (uint32_t)((int32_t)base + (int32_t)lroundf((float)ns * k));
}
#define SC(base, ns) at((base), (ns), ks)
#define DL(base, ns) at((base), (ns), kd)

static uint32_t sample_tick(uint32_t end, uint32_t fall) {
    return p->sample_mode ? DL(fall, p->sample_off) : SC(end, -p->t_setup);
}

// Drives D to the inverse of the expected value before a read, so that a
// late answer cannot pass on the charge left on the bus.
static void precharge(uint32_t start_of_half, uint32_t fall, uint8_t expected, uint32_t bits) {
    uint32_t start = DL(start_of_half, p->t_pre_start);
    if ((int32_t)(start - (d_end + 4)) < 0) {
        start = d_end + 4;
    }
    const uint32_t end = DL(fall, -p->t_pre_end);
    if ((int32_t)(end - start) < 8) {
        return;  // no room at this speed
    }
    d_set(start, ~(uint32_t)expected, bits, bits);
    d_set(end, 0, 0, bits);
}

// ---------------------------------------------------------------------------
// Half cycles.

static void gen_vic_half(uint32_t s, uint32_t e, int kind, bool phi2, uint32_t cycle) {
    w_set(s, M_PHI2 | M_AEC, phi2 ? M_PHI2 : 0);
    w_high(DL(s, p->t_pla), M_RW);      // the 6510 lets go of R/~W
    w_low(DL(s, p->t_pla), M_COLORAM);  // ~COLORAM is low while AEC is low
    a_set(s, (rnd() & 0x0FFFu) | 0xF000u);  // A12 to A15 float high (pull-ups)
    const uint32_t cas = SC(s, p->t_cas);
    w_low(cas, M_CAS);
    w_high(DL(e, p->t_cas_rise), M_CAS);
    const uint32_t col = DL(cas, p->t_col);

    uint32_t addr;
    if (kind == K_V_CHAR && !(bank & 1u)) {
        addr = (bank << 14) | 0x1000u | (rnd() & 0x0FFFu);  // character ROM in banks 0 and 2
    } else {
        if (kind == K_V_CHAR) {
            kind = K_V_RAM;
        }
        addr = (bank << 14) | (rnd() & 0x3FFFu);
        if (!(bank & 1u) && (addr & 0x3000u) == 0x1000u) {
            addr ^= 0x2000u;  // the character ROM window is not RAM for the VIC-II
        }
    }
    const uint32_t va = SC(s, p->t_vaddr);
    a_set(va, (addr & 0x0FFFu) | 0xF000u);
    va_set(va, addr >> 4, addr >> 5);    // row: A4, A5
    va_set(col, addr >> 12, addr >> 13); // column: A12, A13
    if (kind == K_V_REFRESH) {
        return;
    }

    rec_t r = {.addr = (uint16_t)addr, .mask_d = 0xFF, .cycle = cycle};
    uint32_t fall, rise;
    uint64_t sel;
    if (kind == K_V_CHAR) {
        fall = DL(col, p->t_pla);
        rise = DL(e, p->t_pla);
        sel = M_CHAROM;
        r.exp_d = rom_byte(C64_ROM_CHAR + (addr & 0x0FFFu));
        r.check = CK_VIC_CHAR;
    } else {
        fall = DL(cas, p->d_casram);
        rise = DL(DL(e, p->t_cas_rise), p->d_casram);
        sel = M_CASRAM;
        r.exp_d = model_ram[addr];
        r.check = CK_VIC_RAM;
    }
    if (kind == K_B_CACC) {
        r.check = CK_CACC_RAM;
        r.exp_c = model_color[addr & (C64_COLOR_SIZE - 1u)];
        r.mask_c = 0x0F;
    }
    w_low(fall, sel);
    w_high(rise, sel);
    precharge(s, fall, r.exp_d, 0xFF);
    sample(sample_tick(e, fall), &r);
}

static void gen_cpu_half(uint32_t s, uint32_t e, int kind, uint32_t cycle) {
    w_set(s, M_PHI2 | M_AEC, M_PHI2 | M_AEC);
    w_high(DL(s, p->t_pla), M_COLORAM);
    a_set(s, rnd());  // the bus floats until the 6510 drives it
    const uint32_t cas = SC(s, p->t_cas);
    w_low(cas, M_CAS);
    w_high(DL(e, p->t_cas_rise), M_CAS);
    const uint32_t r_va = rnd();
    va_set(DL(cas, p->t_col), r_va, r_va >> 1);  // the VIC-II's own address, ignored
    const uint32_t ta = SC(s, p->t_addr);

    // A read-back reads an address written earlier, so that writes are checked.
    int rb = -1;
    if (kind == K_C_READBACK) {
        if (wq_col_n && (!wq_ram_n || (rnd() & 1u))) {
            kind = K_C_COL_R;
            rb = 0xD800 | wq_col[rnd() % wq_col_n];
        } else {
            kind = K_C_RAM_R;
            if (wq_ram_n) {
                rb = wq_ram[rnd() % wq_ram_n];
            }
        }
    }

    uint32_t addr;
    bool rw = true;
    uint8_t check = CK_RAM_R;
    switch (kind) {
    case K_C_RAM_W:
        addr = rnd() & 0xFFFFu;
        rw = false;
        break;
    case K_C_BASIC:
        addr = 0xA000u | (rnd() & 0x1FFFu);
        check = CK_BASIC;
        break;
    case K_C_KERNAL:
        addr = 0xE000u | (rnd() & 0x1FFFu);
        check = CK_KERNAL;
        break;
    case K_C_CHAR:
        addr = 0xD000u | (rnd() & 0x0FFFu);
        check = CK_CHAR;
        break;
    case K_C_COL_R:
    case K_C_COL_W:
        addr = 0xD800u | (rnd() & 0x03FFu);
        rw = kind == K_C_COL_R;
        check = CK_COL_R;
        break;
    case K_C_IDLE:
        addr = 0xD000u | (rnd() & 0x03FFu);  // VIC-II registers: no select
        rw = rnd() & 1u;
        break;
    default:  // K_C_RAM_R, K_C_RELEASE
        addr = rnd() & 0xFFFFu;
        break;
    }
    if (rb >= 0) {
        addr = (uint32_t)rb;
        check = kind == K_C_COL_R ? CK_COL_RB : CK_RAM_RB;
    }
    a_set(ta, addr);
    w_set(ta, M_RW, rw ? M_RW : 0);

    uint8_t data = 0;
    if (!rw) {
        data = (uint8_t)rnd();
        uint32_t start = ta;
        if ((int32_t)(start - (d_end + 4)) < 0) {
            start = d_end + 4;
        }
        d_set(start, rnd(), 0xFF, 0xFF);  // not valid yet
        d_set(SC(s, p->t_dv), data, 0xFF, 0xFF);
        d_set(DL(e, p->t_dhold), 0, 0, 0xFF);
    }

    rec_t r = {.addr = (uint16_t)addr, .check = check, .mask_d = 0xFF, .cycle = cycle};
    uint32_t fall, rise;
    switch (kind) {
    case K_C_RAM_R:
    case K_C_RAM_W:
    case K_C_RELEASE:
        fall = DL(cas, p->d_casram);
        rise = DL(DL(e, p->t_cas_rise), p->d_casram);
        w_low(fall, M_CASRAM);
        w_high(rise, M_CASRAM);
        if (kind == K_C_RAM_W) {
            model_ram[addr] = data;
            wq_ram[wq_ram_i++ & 15u] = (uint16_t)addr;
            if (wq_ram_n < 16) {
                wq_ram_n++;
            }
            return;
        }
        r.exp_d = model_ram[addr];
        precharge(s, fall, r.exp_d, 0xFF);
        sample(sample_tick(e, fall), &r);
        if (kind == K_C_RELEASE) {
            // Drive the inverse right after the select rises; the sense wires
            // show whether the DUT still holds its side of the resistors.
            const uint32_t pd = DL(rise, p->t_rel_drive);
            const uint32_t ps = DL(rise, p->t_rel_sample);
            const uint8_t inv = (uint8_t)~r.exp_d;
            d_set(pd, inv, 0xFF, 0xFF);
            rec_t rr = {.addr = (uint16_t)addr, .check = CK_RELEASE, .cycle = cycle,
                        .exp_s = (uint8_t)((inv & 1u) | ((inv >> 6) & 2u)), .mask_s = 3};
            sample(ps, &rr);
            d_set(DL(ps, 20), 0, 0, 0xFF);
        }
        return;
    case K_C_BASIC:
    case K_C_KERNAL:
    case K_C_CHAR: {
        const uint64_t sel = kind == K_C_BASIC ? M_BASIC : kind == K_C_KERNAL ? M_KERNAL : M_CHAROM;
        const uint32_t base = kind == K_C_BASIC ? C64_ROM_BASIC : kind == K_C_KERNAL ? C64_ROM_KERNAL : C64_ROM_CHAR;
        const uint32_t size = kind == K_C_CHAR ? 0x1000u : 0x2000u;
        fall = DL(ta, p->t_pla);
        rise = DL(e, p->t_pla);
        w_low(fall, sel);
        w_high(rise, sel);
        r.exp_d = rom_byte(base + (addr & (size - 1u)));
        precharge(s, fall, r.exp_d, 0xFF);
        sample(sample_tick(e, fall), &r);
        return;
    }
    case K_C_COL_R:
    case K_C_COL_W: {
        // ~COLORAM stays low into the next VIC-II half; AEC tells them apart.
        const uint32_t idx = addr & (C64_COLOR_SIZE - 1u);
        fall = DL(ta, p->t_pla);
        w_low(fall, M_COLORAM);
        if (kind == K_C_COL_W) {
            w_low(DL(cas, p->d_casram), M_GRW);
            w_high(e, M_GRW);
            model_color[idx] = data & 0x0Fu;
            wq_col[wq_col_i++ & 15u] = (uint16_t)idx;
            if (wq_col_n < 16) {
                wq_col_n++;
            }
            return;
        }
        r.exp_d = model_color[idx];
        r.mask_d = 0x0F;  // D4 to D7 are not driven
        precharge(s, fall, r.exp_d, 0x0F);
        sample(sample_tick(e, fall), &r);
        return;
    }
    default:
        return;
    }
}

// ---------------------------------------------------------------------------
// Batches.

static uint64_t idle_pins(void) {
    uint64_t v = M_SELECTS | M_RW | M_GRW | M_A | M_AEC | M_CAS;
    if (!(bank & 1u)) {
        v |= M_VA14N;
    }
    if (!(bank & 2u)) {
        v |= M_VA15N;
    }
    return v;
}

void ex_go_idle(void) {
    ex_set_idle(idle_pins());
}

static void lists_begin(uint32_t seed, uint64_t first) {
    rng = c64bench_hash32(seed) | 1u;
    moved = 0;
    max_move = 0;
    full = false;
    qn = 0;
    n_recs = 0;
    s_n = 0;
    s_start = 0;
    d_end = 0;
    pins = first;
    dstate = 0;
    const float mhz = (float)ex_sys_hz() / 1e6f;
    kd = mhz / 1000.0f;
    ks = kd * 100.0f / (float)(p->speed > 0 ? p->speed : 100);
    l_w0 = (lst_t){.buf = ex_w0, .cap = EX_W0_WORDS, .state = (uint32_t)(first >> 8) & 0xFFFFFFu,
                   .mask = 0xFFFFFFu, .min = 3, .max = 255 + 3, .bias = 3, .shift = 24};
    l_w1 = (lst_t){.buf = ex_w1, .cap = EX_W1_WORDS, .state = (uint32_t)(first >> 32) & 0xFFFFu,
                   .mask = 0xFFFFu, .min = 3, .max = 65535 + 3, .bias = 3, .shift = 16};
    l_d = (lst_t){.buf = ex_d, .cap = EX_D_WORDS, .state = 0,
                  .mask = 0xFFFFu, .min = 4, .max = 65535 + 4, .bias = 4, .shift = 16};
}

// Flushes all events, holds every list to a common end and pads it, so that
// a stall flag can be read before any list runs out (engine.c).
static void lists_end(uint32_t t_end, ex_lists_t *l) {
    q_flush(t_end + 1u);
    uint32_t last = t_end;
    if ((int32_t)(l_w0.last - last) > 0) last = l_w0.last;
    if ((int32_t)(l_w1.last - last) > 0) last = l_w1.last;
    if ((int32_t)(l_d.last - last) > 0) last = l_d.last;
    if ((int32_t)(s_start - last) > 0) last = s_start;
    const uint32_t fin = last + DL(0, 20000);  // 20 us of padding
    lst_emit(&l_w0, l_w0.state, fin - l_w0.last);
    lst_emit(&l_w1, l_w1.state, fin - l_w1.last);
    lst_emit(&l_d, l_d.state, fin - l_d.last);
    sampler_add(fin);  // one extra sample at the end, not checked
    l->n_w0 = l_w0.n;
    l->n_w1 = l_w1.n;
    l->n_d = l_d.n;
    l->n_s = s_n;
    l->total_ticks = fin + 1u;
}

static int pick(const int *kinds, unsigned n) {
    int32_t total = 0;
    for (unsigned i = 0; i < n; i++) {
        total += p->weight[kinds[i]] > 0 ? p->weight[kinds[i]] : 0;
    }
    if (total <= 0) {
        return kinds[0];
    }
    int32_t x = (int32_t)(rnd() % (uint32_t)total);
    for (unsigned i = 0; i < n; i++) {
        const int32_t w = p->weight[kinds[i]] > 0 ? p->weight[kinds[i]] : 0;
        if (x < w) {
            return kinds[i];
        }
        x -= w;
    }
    return kinds[n - 1];
}

static const int vic_kinds[] = {K_V_REFRESH, K_V_RAM, K_V_CHAR};
static const int cpu_kinds[] = {K_C_RAM_R, K_C_RAM_W, K_C_BASIC, K_C_KERNAL, K_C_CHAR, K_C_COL_R,
                                K_C_COL_W, K_C_IDLE, K_C_RELEASE, K_C_READBACK, K_B_CACC};

// Returns the number of cycles built.
static uint32_t build_batch(uint32_t seed, ex_lists_t *l, uint64_t *first) {
    rng = c64bench_hash32(seed) | 1u;
    bank = p->bank >= 0 && p->bank < 4 ? (uint32_t)p->bank : (rnd() & 3u);
    *first = idle_pins();
    lists_begin(seed, *first);

    const uint32_t t0 = DL(0, 2000);
    w_high(t0, M_MARK);
    const float half = (float)p->half_ns * ks;
    uint32_t c = 0;
    for (; c < (uint32_t)p->cycles; c++) {
        if (full || l_w0.n + 64 > l_w0.cap || l_w1.n + 32 > l_w1.cap || l_d.n + 32 > l_d.cap ||
            s_n + 8 > EX_S_WORDS) {
            break;
        }
        const uint32_t s0 = t0 + (uint32_t)lroundf((float)(2 * c) * half);
        const uint32_t s1 = t0 + (uint32_t)lroundf((float)(2 * c + 1) * half);
        const uint32_t s2 = t0 + (uint32_t)lroundf((float)(2 * c + 2) * half);
        const int kv = pick(vic_kinds, sizeof(vic_kinds) / sizeof(vic_kinds[0]));
        const int kc = pick(cpu_kinds, sizeof(cpu_kinds) / sizeof(cpu_kinds[0]));
        gen_vic_half(s0, s1, kv, false, c);
        if (kc == K_B_CACC) {
            gen_vic_half(s1, s2, K_B_CACC, true, c);  // AEC stays low, PHI2 high
        } else {
            gen_cpu_half(s1, s2, kc, c);
        }
        q_flush(s1);  // no later half places an edge before s1
    }
    uint32_t t_end = t0 + (uint32_t)lroundf((float)(2 * c) * half) + DL(0, 300);
    if (qn && (int32_t)(q[qn - 1].tick + 4u - t_end) > 0) {
        t_end = q[qn - 1].tick + 4u;  // after the last edge of the last cycle
    }
    w_set(t_end, M_SELECTS | M_RW | M_GRW | M_CAS | M_MARK, M_SELECTS | M_RW | M_GRW | M_CAS);
    d_set(t_end, 0, 0, 0xFF);
    lists_end(t_end, l);
    return c;
}

static void check_batch(uint32_t batch, ex_stats_t *st) {
    for (uint32_t i = 0; i < n_recs; i++) {
        const rec_t *r = &recs[i];
        const uint8_t got_d = (uint8_t)(ex_rx0[i] & 0xFFu);
        const uint8_t got_c = (uint8_t)((ex_rx1[i] >> (C64_GPIO_COLORD0 - 32)) & 0x0Fu);
        const uint8_t got_s = (uint8_t)(((ex_rx1[i] >> (EX_GPIO_SENSE_D0 - 32)) & 1u) |
                                        (((ex_rx1[i] >> (EX_GPIO_SENSE_D7 - 32)) & 1u) << 1));
        const bool bad_d = ((got_d ^ r->exp_d) & r->mask_d) != 0;
        const bool bad_c = ((got_c ^ r->exp_c) & r->mask_c) != 0;
        const bool bad_s = ((got_s ^ r->exp_s) & r->mask_s) != 0;
        st->samples++;
        if (r->check == CK_CACC_RAM) {
            st->n[CK_CACC_RAM]++;
            st->n[CK_CACC_COL]++;
            st->err[CK_CACC_RAM] += bad_d;
            st->err[CK_CACC_COL] += bad_c;
        } else {
            st->n[r->check]++;
            st->err[r->check] += bad_d || bad_c || bad_s;
        }
        if (bad_d || bad_c || bad_s) {
            st->errors++;
            if (ex_failure_count < EX_MAX_FAILURES) {
                ex_failures[ex_failure_count++] = (ex_failure_t){
                    .batch = batch, .cycle = r->cycle, .check = r->check, .addr = r->addr,
                    .exp_d = r->exp_d, .got_d = got_d, .mask_d = r->mask_d,
                    .exp_c = r->exp_c, .got_c = got_c, .mask_c = r->mask_c,
                    .exp_s = r->exp_s, .got_s = got_s, .mask_s = r->mask_s};
            }
        }
    }
}

ex_failure_t ex_failures[EX_MAX_FAILURES];
unsigned ex_failure_count;

void ex_run_batches(uint32_t batches, uint32_t seed, ex_stats_t *st) {
    ex_lists_t l;
    uint64_t first;
    ex_run_result_t res;
    for (uint32_t b = 0; b < batches; b++) {
        const uint32_t cycles = build_batch(seed + b, &l, &first);
        st->moved += moved;
        if (max_move > st->max_move) {
            st->max_move = max_move;
        }
        if (full || cycles < (uint32_t)p->cycles) {
            st->overflow++;
        }
        if (full) {
            continue;  // lists are incomplete; the model may now differ from the DUT
        }
        ex_run(&l, first, &res);
        st->batches++;
        st->cycles += cycles;
        st->tx_stall += res.tx_stall;
        st->rx_stall += res.rx_stall;
        st->timeout += res.timeout;
        if (!res.timeout && !res.tx_stall && !res.rx_stall) {
            check_batch(seed + b, st);
        }
    }
    ex_set_idle(idle_pins());
}

// ---------------------------------------------------------------------------
// Calibration.

uint32_t ex_calibrate(uint32_t out_gpio, uint32_t in_gpio, int32_t offset, uint32_t reps, bool *ok) {
    const bool out_d = out_gpio < 8;
    const bool out_w = (out_gpio >= 8 && out_gpio < 32) ||
                       (out_gpio >= 32 && out_gpio < 48 && ((EX_W1_OUTPUTS >> (out_gpio - 32)) & 1u));
    const bool in_ok = in_gpio < 16 || (in_gpio >= 32 && in_gpio < 48);
    *ok = (out_d || out_w) && in_ok && offset >= -20 && offset <= 40 && reps > 0 && reps <= 1024;
    if (!*ok) {
        return 0;
    }
    bank = 0;
    uint64_t first = idle_pins() & ~(out_w ? B(out_gpio) : 0);
    lists_begin(1, first);
    const uint32_t t0 = DL(0, 2000);
    for (uint32_t i = 0; i < reps; i++) {
        const uint32_t t = t0 + i * 64u;
        if (out_d) {
            d_set(t, 1u << out_gpio, 1u << out_gpio, 1u << out_gpio);
            d_set(t + 24u, 0, 1u << out_gpio, 1u << out_gpio);
        } else {
            w_high(t, B(out_gpio));
            w_low(t + 24u, B(out_gpio));
        }
        const rec_t r = {0};
        sample((uint32_t)((int32_t)t + offset), &r);
        q_flush(t + 32u);
    }
    const uint32_t t_end = t0 + reps * 64u;
    d_set(t_end, 0, 0, 0xFF);
    ex_lists_t l;
    lists_end(t_end, &l);
    ex_run_result_t res;
    ex_run(&l, first, &res);
    ex_set_idle(idle_pins());
    if (full || res.timeout || res.tx_stall || res.rx_stall) {
        *ok = false;
        return 0;
    }
    uint32_t high = 0;
    for (uint32_t i = 0; i < n_recs; i++) {
        const uint32_t v = in_gpio < 16 ? (ex_rx0[i] >> in_gpio) : (ex_rx1[i] >> (in_gpio - 32));
        high += v & 1u;
    }
    return high;
}
