// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Cycle-exact model of the MOS 6526 CIA, shared by the CIA 1 (U1501) and
// CIA 2 (U1601) controllers (#182). Portable C without Pico SDK headers, so
// that the same code runs in the firmware, in the bench exerciser and in the
// host tests (tools/cia/test).
//
// One PHI2 cycle at a time. In every cycle the caller:
//   1. sets the inputs that changed (cia_set_pa_pins, cia_set_pb_pins,
//      cia->cnt_in, cia->sp_in, cia->flag_in, cia_tod_tick);
//   2. performs the 6510's access of this cycle, if the chip is selected:
//      cia_read() (or cia_peek() and later cia_read_effects()) or
//      cia_write();
//   3. calls cia_clock(), which ends the cycle;
//   4. reads the outputs: cia_irq(), cia_pa_out(), cia_pb_out(), cia->pc,
//      cia->cnt_out, cia->sp_out.
// A read returns the state at the start of the cycle; a write takes effect
// through the same pipelines as in the chip, so its first visible effect is
// in a later cycle.

#ifndef CIA6526_H
#define CIA6526_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CIA_PRA = 0x0,
    CIA_PRB = 0x1,
    CIA_DDRA = 0x2,
    CIA_DDRB = 0x3,
    CIA_TALO = 0x4,
    CIA_TAHI = 0x5,
    CIA_TBLO = 0x6,
    CIA_TBHI = 0x7,
    CIA_TOD10 = 0x8,
    CIA_TODSEC = 0x9,
    CIA_TODMIN = 0xA,
    CIA_TODHR = 0xB,
    CIA_SDR = 0xC,
    CIA_ICR = 0xD,
    CIA_CRA = 0xE,
    CIA_CRB = 0xF,
};

// Interrupt sources (ICR bits 0 to 4).
enum {
    CIA_INT_TA = 0x01,
    CIA_INT_TB = 0x02,
    CIA_INT_ALARM = 0x04,
    CIA_INT_SP = 0x08,
    CIA_INT_FLAG = 0x10,
};

typedef enum {
    CIA_MODEL_6526 = 0,  // "old" CIA, as on the 250407 board (default)
    CIA_MODEL_8521 = 1,  // "new" CIA (6526A), as on later C64C boards
} cia_model_t;

typedef struct {
    uint16_t counter;
    uint16_t latch;
    uint8_t cr;     // control register as written, START kept current
    uint8_t pipe;   // delay pipeline, see cia6526.c
    bool underflow; // underflowed in the last cycle
    bool toggle;    // PB6/PB7 toggle flip-flop
} cia_timer_t;

typedef struct {
    cia_model_t model;

    // Ports. pa_pins/pb_pins are the levels on the pins, set by the caller;
    // a read of PRA/PRB returns them.
    uint8_t pra, prb, ddra, ddrb;
    uint8_t pa_pins, pb_pins;

    cia_timer_t ta, tb;
    bool ta_underflow_prev;  // timer B sees A's underflows one cycle later

    // Interrupts.
    uint8_t icr;       // pending sources, bits 0 to 4
    uint8_t imr;       // mask
    uint8_t irq_pipe;  // interrupt output delay
    bool irq_out;      // ICR bit 7: an enabled interrupt is pending
    bool irq_prev;     // bit 7 at the end of the previous cycle
    bool irq_pin;      // interrupt output, one cycle after bit 7

    // Time of day, BCD: tenths, seconds, minutes, hours (bit 7 PM).
    uint8_t tod[4];
    uint8_t alarm[4];
    uint8_t tod_latch[4];
    bool tod_latched;
    bool tod_stopped;
    uint8_t tod_div;
    bool tod_match;

    // Serial port.
    uint8_t sdr;          // data register
    uint8_t sr;           // shift register
    uint8_t sr_count;     // bits (output: CNT half periods) left
    bool sdr_full;        // output mode: a byte waits in SDR
    bool cnt_out, sp_out; // levels driven in output mode

    // Inputs, set by the caller before cia_clock().
    bool cnt_in;  // CNT pin level (pulled up when nothing drives it)
    bool sp_in;   // SP pin level
    bool flag_in; // ~FLAG pin level

    // Edge detection on the inputs.
    bool cnt_prev, flag_prev;

    // ~PC is low for the cycle after a port B access.
    bool pc;
    bool pc_next;
} cia_t;

void cia_init(cia_t *cia, cia_model_t model);
void cia_reset(cia_t *cia);

// Access of the 6510 in this cycle (reg 0 to 15).
uint8_t cia_peek(const cia_t *cia, unsigned reg);
void cia_read_effects(cia_t *cia, unsigned reg);
void cia_write(cia_t *cia, unsigned reg, uint8_t value);

static inline uint8_t cia_read(cia_t *cia, unsigned reg) {
    const uint8_t v = cia_peek(cia, reg);
    cia_read_effects(cia, reg);
    return v;
}

// One TOD pin edge (50 or 60 Hz).
void cia_tod_tick(cia_t *cia);

// End of the cycle.
void cia_clock(cia_t *cia);

// Outputs.
static inline bool cia_irq(const cia_t *cia) { return cia->irq_pin; }

// Port levels the chip drives: 1 where the bit is an input (the 6526 pulls
// it up) or an output set to 1, 0 where it is an output set to 0.
static inline uint8_t cia_pa_out(const cia_t *cia) {
    return (uint8_t)(cia->pra | (uint8_t)~cia->ddra);
}
uint8_t cia_pb_out(const cia_t *cia);
// Port B direction, including PB6 and PB7 when they carry a timer output.
uint8_t cia_pb_dir(const cia_t *cia);

#ifdef __cplusplus
}
#endif

#endif
