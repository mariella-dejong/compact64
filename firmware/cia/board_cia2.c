// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// CIA 2 (U1601): VIC-II bank, serial bus and user port (#184). All port
// lines are open drain against pull-ups, except the three inputs of the
// serial bus driver U1602, where high pulls the bus line low. Main board
// README, "CIA 2, serial port and user port".

#include "board.h"

#if CIA_UNIT == 2

#include "hardware/gpio.h"
#include "hardware/structs/sio.h"
#include "pico.h"

const char board_name[] = "cia2";
const unsigned board_heartbeat_gpio = 47;

// Open-drain lines in GPIO0 to GPIO31 and GPIO32 to GPIO47.
#define LO_PORTS ((1u << CIA2_GPIO_PC) | (1u << CIA2_GPIO_PA2) | (1u << CIA2_GPIO_VA14N) | \
                  (1u << CIA2_GPIO_VA15N) | (0x1Fu << CIA2_GPIO_PB0))
#define HI_PORTS 0x7u  // PB5 to PB7 on GPIO32 to GPIO34
#define HI_BUS_OUT ((1u << (CIA2_GPIO_ATN_OUT - 32)) | (1u << (CIA2_GPIO_CLK_OUT - 32)) | \
                    (1u << (CIA2_GPIO_DATA_OUT - 32)))

void board_init(void) {
    // The serial bus driver's inputs first, low: the bus released (erratum
    // E9 can leave a pin near 2 V after a reset).
    const unsigned outs[] = {CIA2_GPIO_ATN_OUT, CIA2_GPIO_CLK_OUT, CIA2_GPIO_DATA_OUT};
    for (unsigned i = 0; i < 3; i++) {
        gpio_init(outs[i]);
        gpio_put(outs[i], false);
        gpio_set_dir(outs[i], GPIO_OUT);
    }
    const unsigned ins[] = {CIA2_GPIO_PC, CIA2_GPIO_PA2, CIA2_GPIO_VA14N, CIA2_GPIO_VA15N,
                            CIA2_GPIO_DATA_IN, CIA2_GPIO_CLK_IN};
    for (unsigned i = 0; i < 6; i++) {
        gpio_init(ins[i]);
        gpio_disable_pulls(ins[i]);
        gpio_put(ins[i], false);  // open drain: the output enable pulls low
    }
    for (unsigned g = CIA2_GPIO_PB0; g < CIA2_GPIO_PB0 + 8; g++) {
        gpio_init(g);
        gpio_disable_pulls(g);
        gpio_put(g, false);
    }
}

static inline void set_outputs(cia_t *cia, uint8_t pa, uint8_t pb);
static inline void read_inputs(cia_t *cia, uint8_t pa);

// What the outputs were last set from; they only change with these.
static uint32_t last_out = 0xFFFFFFFFu;

static void __not_in_flash_func(ports)(cia_t *cia, uint32_t sample) {
    (void)sample;
    const uint8_t pa = cia_pa_out(cia);
    const uint8_t pb = cia_pb_out(cia);
    const uint32_t out = pa | ((uint32_t)pb << 8) | ((uint32_t)(cia->ddra & 0xC0) << 16) |
                         ((uint32_t)(cia->pra & 0xC0) << 18) | (cia->pc ? 1u << 31 : 0);
    if (out != last_out) {
        last_out = out;
        set_outputs(cia, pa, pb);
    }
    read_inputs(cia, pa);
}

static inline void set_outputs(cia_t *cia, uint8_t pa, uint8_t pb) {
    // Open drain: pull low where the port drives 0.
    uint32_t lo = ((uint32_t)(~pb & 0x1Fu) << CIA2_GPIO_PB0);
    lo |= (pa & 0x01) ? 0 : 1u << CIA2_GPIO_VA14N;
    lo |= (pa & 0x02) ? 0 : 1u << CIA2_GPIO_VA15N;
    lo |= (pa & 0x04) ? 0 : 1u << CIA2_GPIO_PA2;
    lo |= cia->pc ? 1u << CIA2_GPIO_PC : 0;
    sio_hw->gpio_oe_clr = LO_PORTS & ~lo;
    sio_hw->gpio_oe_set = lo;
    const uint32_t hi = (uint32_t)(~pb >> 5) & HI_PORTS;
    sio_hw->gpio_hi_oe_clr = HI_PORTS & ~hi;
    sio_hw->gpio_hi_oe_set = hi;

    // Serial bus through U1602 (inverting): a high PA3 to PA5 pulls ATN,
    // CLK, DATA low, and so does PA6 or PA7 as an output set to low.
    const bool clk_low = (pa & 0x10) || ((cia->ddra & 0x40) && !(cia->pra & 0x40));
    const bool data_low = (pa & 0x20) || ((cia->ddra & 0x80) && !(cia->pra & 0x80));
    uint32_t bus = (pa & 0x08) ? 1u << (CIA2_GPIO_ATN_OUT - 32) : 0;
    bus |= clk_low ? 1u << (CIA2_GPIO_CLK_OUT - 32) : 0;
    bus |= data_low ? 1u << (CIA2_GPIO_DATA_OUT - 32) : 0;
    sio_hw->gpio_hi_clr = HI_BUS_OUT & ~bus;
    sio_hw->gpio_hi_set = bus;
}

static inline void read_inputs(cia_t *cia, uint8_t pa) {
    // What PRA and PRB read: the pins. PA3 to PA5 only drive U1602, so they
    // read as the port drives them; PA6, PA7 read CLK and DATA.
    const uint32_t in = sio_hw->gpio_in;
    const uint32_t in_hi = sio_hw->gpio_hi_in;
    uint8_t ra = (uint8_t)(pa & 0x38);
    ra |= (in >> CIA2_GPIO_VA14N) & 1u ? 0x01 : 0;
    ra |= (in >> CIA2_GPIO_VA15N) & 1u ? 0x02 : 0;
    ra |= (in >> CIA2_GPIO_PA2) & 1u ? 0x04 : 0;
    ra |= (in_hi >> (CIA2_GPIO_CLK_IN - 32)) & 1u ? 0x40 : 0;
    ra |= (in_hi >> (CIA2_GPIO_DATA_IN - 32)) & 1u ? 0x80 : 0;
    cia->pa_pins = ra;
    cia->pb_pins = (uint8_t)(((in >> CIA2_GPIO_PB0) & 0x1Fu) | ((in_hi & 0x7u) << 5));
}

static void reset(cia_t *cia) {
    last_out = 0xFFFFFFFFu;
    ports(cia, 0);
}

const cia_board_t board = {ports, reset};

#endif
