// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Bus interface of the CIA controllers, see cia_bus.h.

#if defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2040)

#include "cia_bus.h"

#include "cia_bus.pio.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/structs/busctrl.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/syscfg.h"
#include "hardware/sync.h"
#include "pico.h"

#pragma GCC optimize("O3")

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

void __not_in_flash_func(cia_bus_image)(const cia_t *cia, uint32_t out[4]) {
    const uint8_t *tod = cia->tod_latched ? cia->tod_latch : cia->tod;
    out[0] = (uint32_t)cia->pa_pins | ((uint32_t)cia->pb_pins << 8) |
             ((uint32_t)cia->ddra << 16) | ((uint32_t)cia->ddrb << 24);
    out[1] = (uint32_t)cia->ta.counter | ((uint32_t)cia->tb.counter << 16);
    out[2] = (uint32_t)tod[0] | ((uint32_t)tod[1] << 8) | ((uint32_t)tod[2] << 16) |
             ((uint32_t)tod[3] << 24);
    out[3] = (uint32_t)cia->sdr | ((uint32_t)(cia->icr | (cia->irq_out ? 0x80 : 0)) << 8) |
             ((uint32_t)cia->ta.cr << 16) | ((uint32_t)cia->tb.cr << 24);
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

static inline void write_image(const cia_t *cia, bool all) {
    uint32_t img[4];
    cia_bus_image(cia, img);
    io_rw_32 *rx = pio0->rxf_putget[SM_READ];
    for (unsigned i = 0; i < 4; i++) {
        if (all || img[i] != shown[i]) {
            rx[i] = img[i];
            shown[i] = img[i];
        }
    }
    // Ready: the read driver may drive this cycle's byte now.
    pio0->irq_force = 1u << CIA_PIO_IRQ_READY;
}

// Cortex-M33 cycle counter (DWT), per core.
#define DEMCR (*(volatile uint32_t *)0xE000EDFCu)
#define DWT_CTRL (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)

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

    void (*const ports)(cia_t *, uint32_t) = board->ports;
    void (*const reset)(cia_t *) = board->reset;
    ports(cia, 0);
    write_image(cia, true);
    pio->fdebug = stall;

    for (;;) {
        while (pio->fstat & empty) {
        }
        const uint32_t s = pio->rxf[SM_CYCLE];
        const uint32_t t_start = DWT_CYCCNT;
        const bool tod_rise = (s & ~prev) & CIA_S_TOD_BENCH;
        prev = s;
        if (pio->fdebug & stall) {
            pio->fdebug = stall;
            cia_bus_stats.overruns++;
        }
        cia_bus_stats.cycles++;

        if (!(s & CIA_S_RESET)) {
            if (!in_reset) {
                cia_bus_stats.resets++;
                in_reset = true;
            }
            cia_reset(cia);
            if (cia_bus_irq_push_pull) {
                sio_hw->gpio_set = 1u << CIA_GPIO_IRQ;
            } else {
                sio_hw->gpio_oe_clr = 1u << CIA_GPIO_IRQ;
            }
            reset(cia);
            write_image(cia, false);
            continue;
        }
        in_reset = false;

        cia->cnt_in = s & CIA_S_CNT;
        cia->sp_in = s & CIA_S_SP;
        cia->flag_in = s & CIA_S_FLAG;
        if (!(s & CIA_S_CS)) {
            const unsigned reg = cia_s_reg(s);
            if (s & CIA_S_RW) {
                cia_read_effects(cia, reg);
                cia_bus_stats.reads++;
            } else {
                cia_write(cia, reg, (uint8_t)(s & CIA_S_D));
                cia_bus_stats.writes++;
            }
        }
        if (cia_bus_tod_pin) {
            if (tod_rise) {
                cia_tod_tick(cia);
            }
        } else if (cia_bus_tod_edges != tod_seen) {
            tod_seen++;
            cia_tod_tick(cia);
        }
        cia_clock(cia);

        // ~IRQ: open drain, low while the model's output is active.
        if (cia_bus_irq_push_pull) {
            sio_hw->gpio_oe_set = 1u << CIA_GPIO_IRQ;
            if (cia_irq(cia)) {
                sio_hw->gpio_clr = 1u << CIA_GPIO_IRQ;
            } else {
                sio_hw->gpio_set = 1u << CIA_GPIO_IRQ;
            }
        } else if (cia_irq(cia)) {
            sio_hw->gpio_oe_set = 1u << CIA_GPIO_IRQ;
        } else {
            sio_hw->gpio_oe_clr = 1u << CIA_GPIO_IRQ;
        }
        ports(cia, s);
        write_image(cia, false);
        const uint32_t dt = DWT_CYCCNT - t_start;
        cia_bus_stats.loop_sum += dt;
        if (dt > cia_bus_stats.loop_max) {
            cia_bus_stats.loop_max = dt;
        }
    }
}

#endif
