// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Cycle-exact model of the MOS 6526 CIA, see cia6526.h.
//
// Timers. Each timer has a small delay pipeline, as in the chip, after the
// cycle analysis in Wolfgang Lorenz's C64 test suite (Test_Suite_2_15.txt,
// programs CIA1TB123, CIA1TAB, FLIPOS, LOADTH, CNTO2):
//   - START, with PHI2 as the input, reaches the counter two cycles later
//     (COUNT2, then COUNT3); the counter decrements in a cycle in which
//     COUNT3 is set. A CNT edge or a timer A underflow sets COUNT3 directly.
//   - A force load (CR bit 4) loads the latch into the counter at the end of
//     the cycle after the write (FLOAD, then LOAD1). A write to the latch high
//     byte of a stopped timer loads at the end of the same cycle.
//   - Timer B counts an underflow of timer A one cycle after it (CIA1TAB).
//   - At an underflow the counter is reloaded, and the cycle after a load
//     does not count, so a timer with latch N underflows every N + 1 cycles.
//     The reload lasts into that next cycle: a latch written then is still
//     taken (CIA1TA, CIA2TA).
//   - One-shot mode stops the timer at an underflow if the one-shot bit is
//     set in this or the previous cycle.

#include "cia6526.h"

#include <string.h>

// Control register bits.
#define CR_START 0x01
#define CR_PBON 0x02
#define CR_OUTMODE 0x04
#define CR_ONESHOT 0x08
#define CR_LOAD 0x10
#define CRA_INMODE 0x20
#define CRA_SPMODE 0x40
#define CRA_TODIN 0x80
#define CRB_INMODE 0x60
#define CRB_ALARM 0x80

// Timer pipeline bits.
#define P_COUNT2 0x01
#define P_COUNT3 0x02
#define P_FLOAD 0x04
#define P_LOAD1 0x08
#define P_ONESHOT0 0x10
#define P_RELOAD 0x20

static void timer_reset(cia_timer_t *t) {
    t->counter = 0xFFFF;
    t->latch = 0xFFFF;
    t->cr = 0;
    t->pipe = 0;
    t->underflow = false;
    t->toggle = false;
}

void cia_init(cia_t *cia, cia_model_t model) {
    memset(cia, 0, sizeof(*cia));
    cia->model = model;
    cia_reset(cia);
}

void cia_reset(cia_t *cia) {
    const cia_model_t model = cia->model;
    memset(cia, 0, sizeof(*cia));
    cia->model = model;
    timer_reset(&cia->ta);
    timer_reset(&cia->tb);
    cia->pa_pins = 0xFF;
    cia->pb_pins = 0xFF;
    cia->tod[3] = 0x01;  // 1:00:00.0 AM
    cia->cnt_in = true;  // CNT and ~FLAG are pulled up
    cia->cnt_prev = true;
    cia->sp_in = true;
    cia->flag_in = true;
    cia->flag_prev = true;
    cia->cnt_out = true;
    cia->sp_out = true;
}

uint8_t cia_pb_out(const cia_t *cia) {
    uint8_t v = (uint8_t)(cia->prb | (uint8_t)~cia->ddrb);
    if (cia->ta.cr & CR_PBON) {
        const bool b = (cia->ta.cr & CR_OUTMODE) ? cia->ta.toggle : cia->ta.underflow;
        v = (uint8_t)((v & ~0x40) | (b ? 0x40 : 0));
    }
    if (cia->tb.cr & CR_PBON) {
        const bool b = (cia->tb.cr & CR_OUTMODE) ? cia->tb.toggle : cia->tb.underflow;
        v = (uint8_t)((v & ~0x80) | (b ? 0x80 : 0));
    }
    return v;
}

uint8_t cia_pb_dir(const cia_t *cia) {
    uint8_t d = cia->ddrb;
    if (cia->ta.cr & CR_PBON) {
        d |= 0x40;
    }
    if (cia->tb.cr & CR_PBON) {
        d |= 0x80;
    }
    return d;
}

uint8_t cia_peek(const cia_t *cia, unsigned reg) {
    switch (reg & 0xF) {
    case CIA_PRA:
        return cia->pa_pins;
    case CIA_PRB:
        return cia->pb_pins;
    case CIA_DDRA:
        return cia->ddra;
    case CIA_DDRB:
        return cia->ddrb;
    case CIA_TALO:
        return (uint8_t)cia->ta.counter;
    case CIA_TAHI:
        return (uint8_t)(cia->ta.counter >> 8);
    case CIA_TBLO:
        return (uint8_t)cia->tb.counter;
    case CIA_TBHI:
        return (uint8_t)(cia->tb.counter >> 8);
    case CIA_TOD10:
    case CIA_TODSEC:
    case CIA_TODMIN:
    case CIA_TODHR: {
        const unsigned i = (reg & 0xF) - CIA_TOD10;
        return cia->tod_latched ? cia->tod_latch[i] : cia->tod[i];
    }
    case CIA_SDR:
        return cia->sdr;
    case CIA_ICR:
        return (uint8_t)(cia->icr | (cia->irq_out ? 0x80 : 0));
    case CIA_CRA:
        return cia->ta.cr;
    default:
        return cia->tb.cr;
    }
}

void cia_read_effects(cia_t *cia, unsigned reg) {
    switch (reg & 0xF) {
    case CIA_PRB:
        cia->pc_next = true;
        break;
    case CIA_TOD10:
        cia->tod_latched = false;
        break;
    case CIA_TODHR:
        if (!cia->tod_latched) {
            memcpy(cia->tod_latch, cia->tod, sizeof(cia->tod));
            cia->tod_latched = true;
        }
        break;
    case CIA_ICR:
        cia->icr = 0;
        cia->irq_out = false;
        cia->irq_pipe = 0;
        break;
    default:
        break;
    }
}

static void write_cr(cia_timer_t *t, uint8_t value) {
    if (!(t->cr & CR_START) && (value & CR_START)) {
        t->toggle = true;  // a rising edge of START sets the PB toggle
    }
    if (value & CR_LOAD) {
        t->pipe |= P_FLOAD;
    }
    t->cr = (uint8_t)(value & ~CR_LOAD);
}

static void write_tod(cia_t *cia, unsigned i, uint8_t value) {
    static const uint8_t masks[4] = {0x0F, 0x7F, 0x7F, 0x9F};
    value &= masks[i];
    if (cia->tb.cr & CRB_ALARM) {
        cia->alarm[i] = value;
    } else {
        if (i == 3) {
            // The 6526 inverts AM/PM when 12 is written to the hours.
            if ((value & 0x1F) == 0x12) {
                value ^= 0x80;
            }
            cia->tod_stopped = true;
        } else if (i == 0) {
            cia->tod_stopped = false;
            cia->tod_div = 0;
        }
        cia->tod[i] = value;
    }
    const bool match = memcmp(cia->tod, cia->alarm, sizeof(cia->tod)) == 0;
    if (match && !cia->tod_match) {
        cia->icr |= CIA_INT_ALARM;
    }
    cia->tod_match = match;
}

void cia_write(cia_t *cia, unsigned reg, uint8_t value) {
    switch (reg & 0xF) {
    case CIA_PRA:
        cia->pra = value;
        break;
    case CIA_PRB:
        cia->prb = value;
        cia->pc_next = true;
        break;
    case CIA_DDRA:
        cia->ddra = value;
        break;
    case CIA_DDRB:
        cia->ddrb = value;
        break;
    case CIA_TALO:
        cia->ta.latch = (uint16_t)((cia->ta.latch & 0xFF00) | value);
        break;
    case CIA_TAHI:
        cia->ta.latch = (uint16_t)((cia->ta.latch & 0x00FF) | (value << 8));
        if (!(cia->ta.cr & CR_START)) {
            cia->ta.pipe |= P_LOAD1;
        }
        break;
    case CIA_TBLO:
        cia->tb.latch = (uint16_t)((cia->tb.latch & 0xFF00) | value);
        break;
    case CIA_TBHI:
        cia->tb.latch = (uint16_t)((cia->tb.latch & 0x00FF) | (value << 8));
        if (!(cia->tb.cr & CR_START)) {
            cia->tb.pipe |= P_LOAD1;
        }
        break;
    case CIA_TOD10:
    case CIA_TODSEC:
    case CIA_TODMIN:
    case CIA_TODHR:
        write_tod(cia, (reg & 0xF) - CIA_TOD10, value);
        break;
    case CIA_SDR:
        cia->sdr = value;
        if (cia->ta.cr & CRA_SPMODE) {
            cia->sdr_full = true;
        }
        break;
    case CIA_ICR:
        if (value & 0x80) {
            cia->imr |= value & 0x1F;
        } else {
            cia->imr &= (uint8_t)~value;
        }
        break;
    case CIA_CRA:
        if ((value ^ cia->ta.cr) & CRA_SPMODE) {
            // Switching the serial port direction ends a transfer.
            cia->sr_count = 0;
            cia->sdr_full = false;
            cia->cnt_out = true;
        }
        write_cr(&cia->ta, value);
        break;
    default:
        write_cr(&cia->tb, value);
        break;
    }
}

static uint8_t bcd_inc(uint8_t v) {
    v++;
    if ((v & 0x0F) == 0x0A) {
        v = (uint8_t)((v & 0xF0) + 0x10);
    }
    return v;
}

void cia_tod_tick(cia_t *cia) {
    if (cia->tod_stopped) {
        return;
    }
    if (++cia->tod_div < ((cia->ta.cr & CRA_TODIN) ? 5 : 6)) {
        return;
    }
    cia->tod_div = 0;
    uint8_t *t = cia->tod;
    t[0] = (uint8_t)((t[0] + 1) & 0x0F);
    if (t[0] == 10) {
        t[0] = 0;
        t[1] = (uint8_t)(t[1] == 0x59 ? 0 : bcd_inc(t[1]));
        if (t[1] == 0) {
            t[2] = (uint8_t)(t[2] == 0x59 ? 0 : bcd_inc(t[2]));
            if (t[2] == 0) {
                const uint8_t h = t[3] & 0x1F;
                const uint8_t pm = t[3] & 0x80;
                if (h == 0x11) {
                    t[3] = (uint8_t)(0x12 | (pm ^ 0x80));
                } else if (h == 0x12) {
                    t[3] = (uint8_t)(0x01 | pm);
                } else {
                    t[3] = (uint8_t)(bcd_inc(h) | pm);
                }
            }
        }
    }
    const bool match = memcmp(cia->tod, cia->alarm, sizeof(cia->tod)) == 0;
    if (match && !cia->tod_match) {
        cia->icr |= CIA_INT_ALARM;
    }
    cia->tod_match = match;
}

// One cycle of a timer. phi2: counts PHI2; step: a CNT edge or timer A
// underflow to count in this cycle.
static void timer_clock(cia_timer_t *t, bool phi2, bool step) {
    const uint8_t pipe = t->pipe;
    if ((pipe & P_COUNT3) && t->counter) {
        t->counter--;
    }
    const bool start = t->cr & CR_START;
    uint8_t next = 0;
    if (start && phi2) {
        next |= P_COUNT2;
    }
    if ((pipe & P_COUNT2) || (start && step)) {
        next |= P_COUNT3;
    }
    if (pipe & P_FLOAD) {
        next |= P_LOAD1;
    }
    if (t->cr & CR_ONESHOT) {
        next |= P_ONESHOT0;
    }
    bool load = pipe & P_LOAD1;
    if (pipe & P_RELOAD) {
        // Second cycle of an underflow reload: takes a latch written in it.
        t->counter = t->latch;
    }
    t->underflow = false;
    if (t->counter == 0 && (next & P_COUNT3)) {
        t->underflow = true;
        t->toggle = !t->toggle;
        load = true;
        next |= P_RELOAD;
        if ((t->cr & CR_ONESHOT) || (pipe & P_ONESHOT0)) {
            t->cr &= (uint8_t)~CR_START;
            next &= (uint8_t)~P_COUNT2;
        }
    }
    if (load) {
        t->counter = t->latch;
        next &= (uint8_t)~P_COUNT3;
    }
    t->pipe = next;
}

void cia_clock(cia_t *cia) {
    const bool cnt_rise = cia->cnt_in && !cia->cnt_prev;
    cia->cnt_prev = cia->cnt_in;

    // Timer A: PHI2 or CNT.
    const bool ta_cnt = cia->ta.cr & CRA_INMODE;
    timer_clock(&cia->ta, !ta_cnt, ta_cnt && cnt_rise);

    // Timer B: PHI2, CNT, timer A underflows, or those while CNT is high.
    bool tb_phi2 = false, tb_step = false;
    switch (cia->tb.cr & CRB_INMODE) {
    case 0x00:
        tb_phi2 = true;
        break;
    case 0x20:
        tb_step = cnt_rise;
        break;
    case 0x40:
        tb_step = cia->ta_underflow_prev;
        break;
    default:
        tb_step = cia->ta_underflow_prev && cia->cnt_in;
        break;
    }
    timer_clock(&cia->tb, tb_phi2, tb_step);
    cia->ta_underflow_prev = cia->ta.underflow;

    if (cia->ta.underflow) {
        cia->icr |= CIA_INT_TA;
    }
    if (cia->tb.underflow) {
        cia->icr |= CIA_INT_TB;
    }

    // Serial port.
    if (cia->ta.cr & CRA_SPMODE) {
        if (cia->sr_count == 0 && cia->sdr_full) {
            cia->sr = cia->sdr;
            cia->sdr_full = false;
            cia->sr_count = 16;
        }
        if (cia->sr_count && cia->ta.underflow) {
            cia->cnt_out = !cia->cnt_out;
            if (!cia->cnt_out) {
                cia->sp_out = cia->sr & 0x80;
                cia->sr = (uint8_t)(cia->sr << 1);
            }
            if (--cia->sr_count == 0) {
                cia->icr |= CIA_INT_SP;
            }
        }
    } else if (cnt_rise) {
        cia->sr = (uint8_t)((cia->sr << 1) | (cia->sp_in ? 1 : 0));
        if (++cia->sr_count == 8) {
            cia->sdr = cia->sr;
            cia->sr_count = 0;
            cia->icr |= CIA_INT_SP;
        }
    }

    // ~FLAG: falling edge.
    if (!cia->flag_in && cia->flag_prev) {
        cia->icr |= CIA_INT_FLAG;
    }
    cia->flag_prev = cia->flag_in;

    // Interrupt output: the 6526 sets ICR bit 7 one cycle after the source,
    // the 8521 in the same cycle. The pin follows bit 7 one cycle later, as
    // it was at the end of that cycle: a read of ICR does not take back an
    // interrupt that bit 7 has already shown (ICR01, IMR, CIA2TA).
    cia->irq_pin = cia->irq_prev;
    if (cia->irq_pipe) {
        cia->irq_out = true;
    }
    cia->irq_pipe = 0;
    if (!cia->irq_out && (cia->icr & cia->imr)) {
        if (cia->model == CIA_MODEL_8521) {
            cia->irq_out = true;
        } else {
            cia->irq_pipe = 1;
        }
    }
    cia->irq_prev = cia->irq_out;

    cia->pc = cia->pc_next;
    cia->pc_next = false;
}
