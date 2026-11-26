// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// CIA 1 (U1501): control ports and paddle select (#183). The joystick lines
// are open drain against 10k pull-ups and not in bit order on the GPIOs.
// Main board README, "CIA 1, keyboard and joysticks".
//
// Not yet: the keyboard matrix (with ghosting and its interaction with the
// joystick lines), the USB keyboard (#12) and RESTORE (#92). Until then PRA
// and PRB read the joystick lines, and the bits without a pin read as the
// port drives them.

#include "board.h"

#if CIA_UNIT == 1

#include "hardware/gpio.h"
#include "hardware/structs/sio.h"
#include "pico.h"

const char board_name[] = "cia1";
const unsigned board_heartbeat_gpio = 45;

static const unsigned char other_pins[] = {CIA1_GPIO_PA6, CIA1_GPIO_PA7, CIA1_GPIO_NMI};

static uint32_t lo_mask, hi_mask;

void board_init(void) {
    gpio_init(CIA1_GPIO_USB_EN);  // keyboard port off
    gpio_put(CIA1_GPIO_USB_EN, false);
    gpio_set_dir(CIA1_GPIO_USB_EN, GPIO_OUT);
    gpio_init(CIA1_GPIO_USB_FLT);
    for (unsigned i = 0; i < 5; i++) {
        const unsigned g[2] = {cia1_pa_gpio[i], cia1_pb_gpio[i]};
        for (unsigned j = 0; j < 2; j++) {
            gpio_init(g[j]);
            gpio_disable_pulls(g[j]);
            gpio_put(g[j], false);  // open drain: the output enable pulls low
            if (g[j] < 32) {
                lo_mask |= 1u << g[j];
            } else {
                hi_mask |= 1u << (g[j] - 32);
            }
        }
    }
    for (unsigned i = 0; i < sizeof(other_pins); i++) {
        gpio_init(other_pins[i]);
        gpio_disable_pulls(other_pins[i]);
        gpio_put(other_pins[i], false);
    }
    lo_mask |= (1u << CIA1_GPIO_PA6) | (1u << CIA1_GPIO_PA7);
}

static inline void pull(uint32_t *lo, uint32_t *hi, unsigned gpio) {
    if (gpio < 32) {
        *lo |= 1u << gpio;
    } else {
        *hi |= 1u << (gpio - 32);
    }
}

static inline bool level(uint32_t in, uint32_t in_hi, unsigned gpio) {
    return gpio < 32 ? (in >> gpio) & 1u : (in_hi >> (gpio - 32)) & 1u;
}

static void __not_in_flash_func(ports)(cia_t *cia, uint32_t sample) {
    (void)sample;
    const uint8_t pa = cia_pa_out(cia);
    const uint8_t pb = cia_pb_out(cia);
    uint32_t lo = 0, hi = 0;
    for (unsigned i = 0; i < 5; i++) {
        if (!(pa & (1u << i))) {
            pull(&lo, &hi, cia1_pa_gpio[i]);
        }
        if (!(pb & (1u << i))) {
            pull(&lo, &hi, cia1_pb_gpio[i]);  // PB4 also pulls ~LP (GPIO38)
        }
    }
    lo |= (pa & 0x40) ? 0 : 1u << CIA1_GPIO_PA6;
    lo |= (pa & 0x80) ? 0 : 1u << CIA1_GPIO_PA7;
    sio_hw->gpio_oe_clr = lo_mask & ~lo;
    sio_hw->gpio_oe_set = lo;
    sio_hw->gpio_hi_oe_clr = hi_mask & ~hi;
    sio_hw->gpio_hi_oe_set = hi;

    const uint32_t in = sio_hw->gpio_in;
    const uint32_t in_hi = sio_hw->gpio_hi_in;
    uint8_t ra = (uint8_t)(pa & 0x20);
    uint8_t rb = (uint8_t)(pb & 0xE0);
    for (unsigned i = 0; i < 5; i++) {
        ra |= level(in, in_hi, cia1_pa_gpio[i]) ? 1u << i : 0;
        rb |= level(in, in_hi, cia1_pb_gpio[i]) ? 1u << i : 0;
    }
    ra |= (in >> CIA1_GPIO_PA6) & 1u ? 0x40 : 0;
    ra |= (in >> CIA1_GPIO_PA7) & 1u ? 0x80 : 0;
    cia->pa_pins = ra;
    cia->pb_pins = rb;
}

static void reset(cia_t *cia) {
    ports(cia, 0);
}

const cia_board_t board = {ports, reset};

#endif
