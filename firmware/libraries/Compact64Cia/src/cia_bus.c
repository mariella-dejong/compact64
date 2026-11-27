// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Bus interface of the CIA controllers, see cia_bus.h.

#if defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2040)

#include "cia_bus.h"

#include <stddef.h>
#include <string.h>

#include "cia_bus.pio.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/structs/busctrl.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/syscfg.h"
#include "hardware/sync.h"
#include "pico.h"

#pragma GCC optimize("O3")

// Pass times by kind in cia_bus_stats (a few clocks per pass).
#ifndef CIA_BUS_KINDS
#define CIA_BUS_KINDS 1
#endif

#define SM_READ 0u
#define SM_CYCLE 1u

volatile cia_bus_stats_t cia_bus_stats;
volatile uint32_t cia_bus_tod_edges;
volatile bool cia_bus_tod_pin;
volatile bool cia_bus_irq_push_pull;

// Bus inputs: A0 to A3, CNT, SP, ~RESET, ~FLAG, PHI2, R/~W, ~CS.
#define BUS_INPUTS                                                                  \
    ((0xFu << CIA_GPIO_A0) | (1u << CIA_GPIO_CNT) | (1u << CIA_GPIO_SP) |           \
     (1u << CIA_GPIO_RESET) | (1u << CIA_GPIO_FLAG) | (1u << CIA_GPIO_PHI2) |       \
     (1u << CIA_GPIO_RW) | (1u << CIA_GPIO_CS))

// Registers 0 to 3 and the TOD bytes are words in cia_t (cia6526.h).
_Static_assert(offsetof(cia_t, pa_pins) % 4 == 0 && offsetof(cia_t, pb_pins) == offsetof(cia_t, pa_pins) + 1 &&
                   offsetof(cia_t, ddra) == offsetof(cia_t, pa_pins) + 2 &&
                   offsetof(cia_t, ddrb) == offsetof(cia_t, pa_pins) + 3,
               "PRA, PRB, DDRA, DDRB in register order");
_Static_assert(offsetof(cia_t, tod) % 4 == 0 && offsetof(cia_t, tod_latch) % 4 == 0, "TOD words");

static inline uint32_t word_at(const uint8_t *p) {
    uint32_t w;
    memcpy(&w, p, sizeof(w));  // one load; little endian, register 4n in bits 0 to 7
    return w;
}

static inline uint32_t image_word(const cia_t *cia, unsigned i) {
    switch (i) {
    case 0:
        return word_at(&cia->pa_pins);
    case 1:
        return (uint32_t)cia->ta.counter | ((uint32_t)cia->tb.counter << 16);
    case 2:
        return word_at(cia->tod_latched ? cia->tod_latch : cia->tod);
    default:
        return (uint32_t)cia->sdr | ((uint32_t)(cia->icr | (cia->irq_out ? 0x80 : 0)) << 8) |
               ((uint32_t)cia->ta.cr << 16) | ((uint32_t)cia->tb.cr << 24);
    }
}

void __not_in_flash_func(cia_bus_image)(const cia_t *cia, uint32_t out[4]) {
    for (unsigned i = 0; i < 4; i++) {
        out[i] = image_word(cia, i);
    }
}

void cia_bus_set_sync_bypass(bool on) {
    syscfg_hw->proc_in_sync_bypass = on ? BUS_INPUTS | 0xFFu : 0u;
    pio0->input_sync_bypass = on ? BUS_INPUTS | 0xFFu : 0u;
}

void cia_bus_init(void) {
    // Every bus pin an input without pulls (erratum E9); D0 to D7 to PIO0,
    // released. ~IRQ: open drain, released.
    for (uint gpio = 0; gpio <= CIA_GPIO_CS; gpio++) {
        if (gpio >= 14 && gpio <= 20 && gpio != CIA_GPIO_TOD_BENCH) {
            continue;  // controller pins, set up by the controller
        }
        gpio_init(gpio);
        gpio_disable_pulls(gpio);
        gpio_set_input_enabled(gpio, true);
    }
    for (uint gpio = CIA_GPIO_D0; gpio < CIA_GPIO_D0 + 8; gpio++) {
        gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_4MA);
        pio_gpio_init(pio0, gpio);
    }
    gpio_put(CIA_GPIO_IRQ, false);
    gpio_set_dir(CIA_GPIO_IRQ, GPIO_IN);  // released; output enable pulls low

    pio_claim_sm_mask(pio0, (1u << SM_READ) | (1u << SM_CYCLE));

    uint off = pio_add_program(pio0, &cia_read_program);
    pio_sm_config c = cia_read_program_get_default_config(off);
    sm_config_set_in_pins(&c, CIA_GPIO_A0);
    sm_config_set_in_pin_count(&c, CIA_GPIO_CS - CIA_GPIO_A0 + 1);
    sm_config_set_in_shift(&c, false, false, 32);
    sm_config_set_out_pins(&c, CIA_GPIO_D0, 8);
    sm_config_set_out_shift(&c, true, false, 32);
    sm_config_set_clkdiv_int_frac8(&c, 1, 0);
    pio_sm_init(pio0, SM_READ, off, &c);
    pio_sm_set_pindirs_with_mask(pio0, SM_READ, 0, 0xFFu << CIA_GPIO_D0);

    off = pio_add_program(pio0, &cia_cycle_program);
    c = cia_cycle_program_get_default_config(off);
    sm_config_set_in_pins(&c, 0);
    sm_config_set_in_pin_count(&c, CIA_GPIO_CS + 1);
    sm_config_set_in_shift(&c, false, false, 32);
    sm_config_set_jmp_pin(&c, CIA_GPIO_PHI2);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv_int_frac8(&c, 1, 0);
    pio_sm_init(pio0, SM_CYCLE, off, &c);

    pio_set_sm_mask_enabled(pio0, (1u << SM_READ) | (1u << SM_CYCLE), true);

    // The bus loop gets the bus fabric first.
    busctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS;
}

// The image the read driver holds; only changed words are written.
static uint32_t shown[4];

static inline void put_word(io_rw_32 *rx, unsigned i, uint32_t w) {
    if (w != shown[i]) {
        rx[i] = w;
        shown[i] = w;
    }
}

// The counters (word 1) change in most cycles; the rest only after an
// access, a TOD edge, a full model cycle or a change on the port pins.
static inline void write_image(const cia_t *cia, bool all) {
    io_rw_32 *rx = pio0->rxf_putget[SM_READ];
    put_word(rx, 1, image_word(cia, 1));
    if (all) {
        put_word(rx, 0, image_word(cia, 0));
        put_word(rx, 2, image_word(cia, 2));
        put_word(rx, 3, image_word(cia, 3));
    }
}

// Cortex-M33 cycle counter (DWT), per core.
#define DEMCR (*(volatile uint32_t *)0xE000EDFCu)
#define DWT_CTRL (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)

// ~IRQ: open drain, low while the model's output is active; push-pull on
// the bench (cia_bus_irq_push_pull).
static inline void set_irq(bool active, bool push_pull) {
    if (push_pull) {
        sio_hw->gpio_oe_set = 1u << CIA_GPIO_IRQ;
        if (active) {
            sio_hw->gpio_clr = 1u << CIA_GPIO_IRQ;
        } else {
            sio_hw->gpio_set = 1u << CIA_GPIO_IRQ;
        }
    } else if (active) {
        sio_hw->gpio_oe_set = 1u << CIA_GPIO_IRQ;
    } else {
        sio_hw->gpio_oe_clr = 1u << CIA_GPIO_IRQ;
    }
}

// Reads with side effects on the image: the TOD latch and ICR.
#define READ_EFFECTS_IMAGE ((1u << CIA_TOD10) | (1u << CIA_TODHR) | (1u << CIA_ICR))
// Writes that can change the port outputs: the ports, their directions and
// the control registers (PB6, PB7 timer outputs).
#define WRITE_OUTPUTS ((1u << CIA_PRA) | (1u << CIA_PRB) | (1u << CIA_DDRA) | (1u << CIA_DDRB) | \
                       (1u << CIA_CRA) | (1u << CIA_CRB))

// Counters kept in core 1's registers and stack, and published to
// cia_bus_stats every PUBLISH passes, in a pass with time to spare.
#define PUBLISH 256u

typedef struct {
    uint32_t passes, reads, writes, quiet, lagging;
    uint32_t ready_sum, ready_max, loop_sum, loop_max;
#if CIA_BUS_KINDS
    uint32_t kind_n[7], kind_sum[7], kind_max[7];
#endif
} acc_t;

static void __not_in_flash_func(publish)(acc_t *a, pio_hw_t *pio, uint32_t stall) {
    volatile cia_bus_stats_t *const st = &cia_bus_stats;
    st->cycles += a->passes;
    st->reads += a->reads;
    st->writes += a->writes;
    st->quiet += a->quiet;
    st->lagging += a->lagging;
    st->ready_sum += a->ready_sum;
    st->loop_sum += a->loop_sum;
    if (a->ready_max > st->ready_max) {
        st->ready_max = a->ready_max;
    }
    if (a->loop_max > st->loop_max) {
        st->loop_max = a->loop_max;
    }
#if CIA_BUS_KINDS
    for (unsigned k = 0; k < 7; k++) {
        st->kind_n[k] += a->kind_n[k];
        st->kind_sum[k] += a->kind_sum[k];
        if (a->kind_max[k] > st->kind_max[k]) {
            st->kind_max[k] = a->kind_max[k];
        }
    }
#endif
    // The PIO's stall flag is sticky: one or more samples were lost since
    // the last look.
    if (pio->fdebug & stall) {
        pio->fdebug = stall;
        st->overruns++;
    }
    for (uint32_t *w = (uint32_t *)a; w < (uint32_t *)(a + 1); w++) {
        *w = 0;
    }
}

// Quiet stretch. Once a quiet model cycle (cia_clock_quiet) has settled
// ~IRQ and ~PC, the quiet cycles after it change nothing but the counters
// of the timers that count PHI2. While nothing else happens (no access, no
// TOD edge, no change on CNT, ~FLAG, ~RESET or the port pins) and the model
// allows it (cia->quiet), the loop keeps both counters in one word,
// subtracts and shows it, and writes them back to the model when the
// stretch ends.
#define STRETCH_WATCH (CIA_S_RESET | CIA_S_CS | CIA_S_CNT | CIA_S_FLAG | CIA_S_TOD_BENCH)

void __not_in_flash_func(cia_bus_run)(cia_t *cia, const cia_board_t *board) {
    (void)save_and_disable_interrupts();
    DEMCR |= 1u << 24;  // TRCENA
    DWT_CTRL |= 1u;     // CYCCNTENA
    pio_hw_t *const pio = pio0;
    const uint32_t empty = 1u << (PIO_FSTAT_RXEMPTY_LSB + SM_CYCLE);
    const uint32_t stall = 1u << (PIO_FDEBUG_RXSTALL_LSB + SM_CYCLE);
    uint32_t tod_seen = cia_bus_tod_edges;
    uint32_t prev = 0;
    bool in_reset = false;
    bool out_prev = true;   // outputs were due in the last cycle
    bool irq_shown = false;
    // Settings from core 0, read again when the counters are published.
    bool irq_pp = cia_bus_irq_push_pull;
    bool tod_pin = cia_bus_tod_pin;
    acc_t acc;
    for (uint32_t *w = (uint32_t *)&acc; w < (uint32_t *)(&acc + 1); w++) {
        *w = 0;
    }

    void (*const outputs)(cia_t *) = board->outputs;
    bool (*const inputs)(cia_t *, bool, uint32_t, uint32_t) = board->inputs;
    void (*const reset)(cia_t *) = board->reset;
    const uint32_t in_mask = board->in_mask, in_mask_hi = board->in_mask_hi;
    bool stretch = false;
    uint32_t st_w1 = 0, st_dec = 0, st_q = 0, st_in = 0, st_in_hi = 0;
    outputs(cia);
    inputs(cia, true, sio_hw->gpio_in & in_mask, sio_hw->gpio_hi_in & in_mask_hi);
    io_rw_32 *const rx = pio->rxf_putget[SM_READ];
    for (unsigned i = 0; i < 4; i++) {
        shown[i] = image_word(cia, i);  // all four, whatever the PIO held
        rx[i] = shown[i];
    }
    set_irq(false, irq_pp);
    pio->irq_force = 1u << CIA_PIO_IRQ_READY;
    pio->fdebug = stall;

    for (;;) {
        // A sample already waiting: the last pass ended after the next
        // PHI2 fall.
        if (!(pio->fstat & empty)) {
            acc.lagging++;
        } else {
            while (pio->fstat & empty) {
            }
        }
        const uint32_t s = pio->rxf[SM_CYCLE];
        const uint32_t t_start = DWT_CYCCNT;

        if (stretch) {
            if (!((s ^ prev) & STRETCH_WATCH) && (s & CIA_S_CS) && st_q &&
                (tod_pin || cia_bus_tod_edges == tod_seen) && (sio_hw->gpio_in & in_mask) == st_in &&
                (sio_hw->gpio_hi_in & in_mask_hi) == st_in_hi) {
                st_q--;
                st_w1 -= st_dec;
                rx[1] = st_w1;
                pio->irq_force = 1u << CIA_PIO_IRQ_READY;
                const uint32_t dt = DWT_CYCCNT - t_start;
                acc.passes++;
                acc.quiet++;
                acc.ready_sum += dt;
                acc.loop_sum += dt;
#if CIA_BUS_KINDS
                acc.kind_n[6]++;
                acc.kind_sum[6] += dt;
                if (dt > acc.kind_max[6]) {
                    acc.kind_max[6] = dt;
                }
#endif
                if (acc.passes >= PUBLISH) {
                    publish(&acc, pio, stall);
                    irq_pp = cia_bus_irq_push_pull;
                    tod_pin = cia_bus_tod_pin;
                    set_irq(irq_shown, irq_pp);
                }
                continue;
            }
            // The stretch ends: the model takes the counters back.
            cia->ta.counter = (uint16_t)st_w1;
            cia->tb.counter = (uint16_t)(st_w1 >> 16);
            cia->quiet = (uint16_t)st_q;
            shown[1] = st_w1;
            stretch = false;
        }

        const bool tod_rise = (s & ~prev) & CIA_S_TOD_BENCH;
        prev = s;
        acc.passes++;

        if (!(s & CIA_S_RESET)) {
            if (!in_reset) {
                cia_bus_stats.resets++;
                in_reset = true;
            }
            cia_reset(cia);
            irq_shown = false;
            set_irq(false, irq_pp);
            reset(cia);
            inputs(cia, true, sio_hw->gpio_in & in_mask, sio_hw->gpio_hi_in & in_mask_hi);
            write_image(cia, true);
            pio->irq_force = 1u << CIA_PIO_IRQ_READY;
            out_prev = true;
            continue;
        }
        in_reset = false;

        cia->cnt_in = s & CIA_S_CNT;
        cia->sp_in = s & CIA_S_SP;
        cia->flag_in = s & CIA_S_FLAG;
        unsigned kind = 0;   // see cia_bus_stats_t
        bool image = false;  // more than the counters may have changed
        bool out = false;    // the outputs may have changed
        if (!(s & CIA_S_CS)) {
            const unsigned reg = cia_s_reg(s);
            if (s & CIA_S_RW) {
                cia_read_effects(cia, reg);
                kind = 2;
                image = (READ_EFFECTS_IMAGE >> reg) & 1u;
                out = reg == CIA_PRB;  // ~PC
                acc.reads++;
            } else {
                cia_write(cia, reg, (uint8_t)(s & CIA_S_D));
                kind = 4;
                image = true;
                out = (WRITE_OUTPUTS >> reg) & 1u;
                acc.writes++;
            }
        }
        if (tod_pin) {
            if (tod_rise) {
                cia_tod_tick(cia);
                image = true;
            }
        } else if (cia_bus_tod_edges != tod_seen) {
            tod_seen++;
            cia_tod_tick(cia);
            image = true;
        }
        if (cia_clock_quiet(cia)) {
            acc.quiet++;
        } else {
            cia_clock_full(cia);
            kind++;
            image = true;
            // PB6, PB7: a timer output changes when the timer underflows.
            out |= (cia->ta.underflow && (cia->ta.cr & 0x02)) || (cia->tb.underflow && (cia->tb.cr & 0x02));
        }
        const uint32_t in = sio_hw->gpio_in & in_mask;
        const uint32_t in_hi = sio_hw->gpio_hi_in & in_mask_hi;
        image |= inputs(cia, out, in, in_hi);
        write_image(cia, image);
        const bool irq = cia_irq(cia);
        if (irq != irq_shown) {
            irq_shown = irq;
            set_irq(irq, irq_pp);
        }
        // Ready: the read driver may drive this cycle's byte now.
        pio->irq_force = 1u << CIA_PIO_IRQ_READY;
        const uint32_t t_ready = DWT_CYCCNT;

        // Off the critical path: the output pins (the port B timer outputs
        // and ~PC also go back in the cycle after), then the counters.
        if (out || out_prev) {
            outputs(cia);
        }
        out_prev = out;
        const uint32_t dr = t_ready - t_start;
        const uint32_t dt = DWT_CYCCNT - t_start;
        acc.ready_sum += dr;
        acc.loop_sum += dt;
        if (dr > acc.ready_max) {
            acc.ready_max = dr;
        }
        if (dt > acc.loop_max) {
            acc.loop_max = dt;
        }
#if CIA_BUS_KINDS
        acc.kind_n[kind]++;
        acc.kind_sum[kind] += dt;
        if (dt > acc.kind_max[kind]) {
            acc.kind_max[kind] = dt;
        }
#else
        (void)kind;
#endif
        // A quiet cycle with ~IRQ and ~PC settled starts a stretch.
        if (kind == 0 && !out && !out_prev && cia->quiet && !cia->pc && !cia->pc_next &&
            cia->irq_pin == cia->irq_out && cia->irq_prev == cia->irq_out) {
            stretch = true;
            st_w1 = shown[1];
            st_dec = cia->quiet_dec_a | ((uint32_t)cia->quiet_dec_b << 16);
            st_q = cia->quiet;
            st_in = in;
            st_in_hi = in_hi;
        }
        if (acc.passes >= PUBLISH && kind == 0) {
            publish(&acc, pio, stall);
            irq_pp = cia_bus_irq_push_pull;
            tod_pin = cia_bus_tod_pin;
            set_irq(irq_shown, irq_pp);
        }
    }
}

#endif
