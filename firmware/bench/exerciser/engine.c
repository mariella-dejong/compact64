// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors

#include "engine.h"

#include "exerciser.pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/time.h"

uint32_t ex_w0[EX_W0_WORDS];
uint32_t ex_w1[EX_W1_WORDS];
uint32_t ex_d[EX_D_WORDS];
uint32_t ex_s[EX_S_WORDS];
uint32_t ex_rx0[EX_S_WORDS];
uint32_t ex_rx1[EX_S_WORDS];

#define P0 pio0
#define P1 pio1
enum { SM_W0 = 0, SM_D = 1, SM_S0 = 2 };  // on PIO0
enum { SM_W1 = 0, SM_S1 = 1 };            // on PIO1
#define MASK_P0 ((1u << SM_W0) | (1u << SM_D) | (1u << SM_S0))
#define MASK_P1 ((1u << SM_W1) | (1u << SM_S1))

#define W0_PINS 0x00000000FFFFFF00ull                    // GPIO8 to GPIO31
#define W1_PINS ((uint64_t)EX_W1_OUTPUTS << 32)          // see engine.h
#define D_PINS  0x00000000000000FFull                    // GPIO0 to GPIO7

static uint off_w24, off_d8, off_s0, off_w16, off_s1;
static int ch_w0, ch_d, ch_s0, ch_w1, ch_s1, ch_rx0, ch_rx1;

uint32_t ex_sys_hz(void) {
    return clock_get_hz(clk_sys);
}

static void init_sm(PIO p, uint sm, uint off, pio_sm_config *c, uint out_base, uint out_count, int in_base) {
    if (out_count) {
        sm_config_set_out_pins(c, out_base, out_count);
    }
    if (in_base >= 0) {
        sm_config_set_in_pin_base(c, (uint)in_base);
        sm_config_set_in_shift(c, false, true, 16);  // shift left: pins land in bits 0-15
    }
    sm_config_set_out_shift(c, true, true, 32);
    if (in_base < 0) {
        sm_config_set_fifo_join(c, PIO_FIFO_JOIN_TX);  // eight words of slack
    }
    sm_config_set_clkdiv_int_frac8(c, 1, 0);
    pio_sm_init(p, sm, off, c);
}

static void dma_tx_config(int ch, PIO p, uint sm) {
    dma_channel_config c = dma_channel_get_default_config((uint)ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(p, sm, true));
    channel_config_set_high_priority(&c, true);
    dma_channel_set_config((uint)ch, &c, false);
    dma_channel_set_write_addr((uint)ch, &p->txf[sm], false);
}

static void dma_rx_config(int ch, PIO p, uint sm) {
    dma_channel_config c = dma_channel_get_default_config((uint)ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(p, sm, false));
    channel_config_set_high_priority(&c, true);
    dma_channel_set_config((uint)ch, &c, false);
    dma_channel_set_read_addr((uint)ch, &p->rxf[sm], false);
}

void ex_engine_init(void) {
    pio_set_gpio_base(P1, 16);

    off_w24 = pio_add_program(P0, &wave24_program);
    off_d8 = pio_add_program(P0, &data8_program);
    off_s0 = pio_add_program(P0, &sample16_program);
    off_w16 = pio_add_program(P1, &wave16_program);
    off_s1 = pio_add_program(P1, &sample16_program);
    pio_claim_sm_mask(P0, MASK_P0);
    pio_claim_sm_mask(P1, MASK_P1);

    for (uint g = 0; g < 48; g++) {
        const uint64_t bit = 1ull << g;
        if ((W0_PINS | D_PINS) & bit) {
            pio_gpio_init(P0, g);
        } else if (W1_PINS & bit) {
            pio_gpio_init(P1, g);
        } else {
            gpio_init(g);  // input: COLORD, I2C, sense wires, DUT trigger
        }
        gpio_disable_pulls(g);  // RP2350 erratum E9
        gpio_set_input_enabled(g, true);
        gpio_set_slew_rate(g, GPIO_SLEW_RATE_FAST);
        gpio_set_drive_strength(g, GPIO_DRIVE_STRENGTH_8MA);
    }

    pio_sm_config c;
    c = wave24_program_get_default_config(off_w24);
    init_sm(P0, SM_W0, off_w24, &c, 8, 24, -1);
    c = data8_program_get_default_config(off_d8);
    init_sm(P0, SM_D, off_d8, &c, 0, 8, -1);
    c = sample16_program_get_default_config(off_s0);
    init_sm(P0, SM_S0, off_s0, &c, 0, 0, 0);
    c = wave16_program_get_default_config(off_w16);
    init_sm(P1, SM_W1, off_w16, &c, 32, 16, -1);
    c = sample16_program_get_default_config(off_s1);
    init_sm(P1, SM_S1, off_s1, &c, 0, 0, 32);

    pio_sm_set_pindirs_with_mask64(P0, SM_W0, W0_PINS, W0_PINS);
    pio_sm_set_pindirs_with_mask64(P0, SM_D, 0, D_PINS);
    pio_sm_set_pindirs_with_mask64(P1, SM_W1, W1_PINS, W1_PINS);

    // Sample the inputs without the two-flip-flop synchronizer, so that the
    // sample time is exact to one clock. Bits are relative to the GPIO base.
    P0->input_sync_bypass = 0x0000FFFFu;            // GPIO0 to GPIO15
    P1->input_sync_bypass = 0x1FFu << (36 - 16);    // GPIO36 to GPIO44

    ch_w0 = dma_claim_unused_channel(true);
    ch_d = dma_claim_unused_channel(true);
    ch_s0 = dma_claim_unused_channel(true);
    ch_w1 = dma_claim_unused_channel(true);
    ch_s1 = dma_claim_unused_channel(true);
    ch_rx0 = dma_claim_unused_channel(true);
    ch_rx1 = dma_claim_unused_channel(true);
    dma_tx_config(ch_w0, P0, SM_W0);
    dma_tx_config(ch_d, P0, SM_D);
    dma_tx_config(ch_s0, P0, SM_S0);
    dma_tx_config(ch_w1, P1, SM_W1);
    dma_tx_config(ch_s1, P1, SM_S1);
    dma_rx_config(ch_rx0, P0, SM_S0);
    dma_rx_config(ch_rx1, P1, SM_S1);
}

static void stop_all(void) {
    pio_set_sm_mask_enabled(P0, MASK_P0, false);
    pio_set_sm_mask_enabled(P1, MASK_P1, false);
}

static void reset_sm(PIO p, uint sm, uint off) {
    pio_sm_clear_fifos(p, sm);
    pio_sm_restart(p, sm);
    pio_sm_exec(p, sm, pio_encode_jmp(off));
}

void ex_set_idle(uint64_t pins) {
    stop_all();
    pio_sm_set_pins_with_mask64(P0, SM_W0, pins, W0_PINS);
    pio_sm_set_pins_with_mask64(P1, SM_W1, pins, W1_PINS);
    pio_sm_set_pindirs_with_mask64(P0, SM_D, 0, D_PINS);
}

static void start_tx(int ch, const uint32_t *buf, uint32_t n) {
    dma_channel_set_read_addr((uint)ch, buf, false);
    dma_channel_set_trans_count((uint)ch, n, true);
}

static void start_rx(int ch, uint32_t *buf, uint32_t n) {
    dma_channel_set_write_addr((uint)ch, buf, false);
    dma_channel_set_trans_count((uint)ch, n, true);
}

static bool tx_busy(void) {
    return dma_channel_is_busy((uint)ch_w0) || dma_channel_is_busy((uint)ch_d) ||
           dma_channel_is_busy((uint)ch_s0) || dma_channel_is_busy((uint)ch_w1) ||
           dma_channel_is_busy((uint)ch_s1);
}

static void wait_fill(PIO p, uint sm, int ch) {
    while (!pio_sm_is_tx_fifo_full(p, sm) && dma_channel_is_busy((uint)ch)) {
    }
}

void ex_run(const ex_lists_t *l, uint64_t first_pins, ex_run_result_t *r) {
    r->tx_stall = r->rx_stall = r->timeout = false;

    ex_set_idle(first_pins);
    reset_sm(P0, SM_W0, off_w24);
    reset_sm(P0, SM_D, off_d8);
    reset_sm(P0, SM_S0, off_s0);
    reset_sm(P1, SM_W1, off_w16);
    reset_sm(P1, SM_S1, off_s1);
    P0->fdebug = 0xFFFFFFFFu;
    P1->fdebug = 0xFFFFFFFFu;

    start_rx(ch_rx0, ex_rx0, l->n_s);
    start_rx(ch_rx1, ex_rx1, l->n_s);
    start_tx(ch_w0, ex_w0, l->n_w0);
    start_tx(ch_d, ex_d, l->n_d);
    start_tx(ch_s0, ex_s, l->n_s);
    start_tx(ch_w1, ex_w1, l->n_w1);
    start_tx(ch_s1, ex_s, l->n_s);
    wait_fill(P0, SM_W0, ch_w0);
    wait_fill(P0, SM_D, ch_d);
    wait_fill(P0, SM_S0, ch_s0);
    wait_fill(P1, SM_W1, ch_w1);
    wait_fill(P1, SM_S1, ch_s1);

    // All five state machines start on the same clock.
    pio_enable_sm_multi_mask_in_sync(P1, MASK_P0, MASK_P1, 0);

    const uint32_t mhz = ex_sys_hz() / 1000000u;
    const uint64_t deadline = time_us_64() + l->total_ticks / (mhz ? mhz : 1u) + 2000u;

    // When the last words are in the FIFOs, every list still holds its padding
    // (several microseconds), so a stall flag set now means a list ran dry
    // during the run.
    while (tx_busy()) {
        if (time_us_64() > deadline) {
            r->timeout = true;
            break;
        }
    }
    r->tx_stall = ((P0->fdebug >> PIO_FDEBUG_TXSTALL_LSB) & MASK_P0) != 0 ||
                  ((P1->fdebug >> PIO_FDEBUG_TXSTALL_LSB) & MASK_P1) != 0;

    while (!r->timeout && (dma_channel_is_busy((uint)ch_rx0) || dma_channel_is_busy((uint)ch_rx1))) {
        if (time_us_64() > deadline) {
            r->timeout = true;
        }
    }
    r->rx_stall = ((P0->fdebug >> PIO_FDEBUG_RXSTALL_LSB) & (1u << SM_S0)) != 0 ||
                  ((P1->fdebug >> PIO_FDEBUG_RXSTALL_LSB) & (1u << SM_S1)) != 0;

    stop_all();
    if (r->timeout) {
        const int chans[] = {ch_w0, ch_d, ch_s0, ch_w1, ch_s1, ch_rx0, ch_rx1};
        for (unsigned i = 0; i < sizeof(chans) / sizeof(chans[0]); i++) {
            dma_channel_abort((uint)chans[i]);
        }
    }
    pio_sm_set_pindirs_with_mask64(P0, SM_D, 0, D_PINS);
}
