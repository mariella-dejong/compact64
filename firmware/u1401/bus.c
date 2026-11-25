// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Bus loop of the RAM and ROM controller (U1401), following the timing notes
// in hardware/main-board/README.md (RAM and ROM replacement, Timing).
//
// The CPU is too slow to sit between the address and the data: every SIO
// or PIO register access costs it several clocks (#159). So reads run
// without the CPU (bus.pio): PIO1 watches the address bus and hands
// pointers to DMA, which copies table entries into the RX registers of the
// PIO0 drivers, or to PIO2 for COLORD:
//
// | Access                    | Select     | Table, from               | Driver     |
// | ------------------------- | ---------- | ------------------------- | ---------- |
// | 6510 RAM read             | ~CASRAM    | bus_ram[A0..A15]          | ram_drive  |
// | VIC-II RAM read           | ~CASRAM    | vic_tab[bank][A0..A11]: 4 candidates, picked by VA4, VA5 | ram_drive |
// | Character ROM, both       | ~CHAROM    | char_tab[A0..A15] (A0..A11 repeated) | char_drive |
// | Colour for the VIC-II     | always     | bus_color[A0..A9]         | colord_out |
//
// The RAM, character and colour watchers run the same program, which looks
// up a changed address only after a second sample: the DUT's own D edges
// can glitch the address lines. The colour watcher reads A0 to A9 only
// (IN_COUNT 10, the other bits read 0), so colour RAM starts a 64 KB block.
//
// The CPU serves the rest: 6510 reads of the BASIC and KERNAL ROMs and of
// colour RAM (through cpu_drive), and all writes, which it also copies into
// vic_tab.

#include "bus.h"

#include "bus.pio.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/structs/busctrl.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/syscfg.h"
#include "hardware/sync.h"
#include "pico/platform.h"

#include <c64bus_pins.h>

#pragma GCC optimize("O3")

volatile bus_stats_t bus_stats;
// The watchers build pointers from the upper bits, so the tables are aligned
// to their size.
uint8_t bus_ram[C64_RAM_SIZE] __attribute__((aligned(0x10000)));
static uint32_t vic_tab[4 * 4096] __attribute__((aligned(0x10000)));
static uint8_t char_tab[0x10000] __attribute__((aligned(0x10000)));
static struct {
    uint8_t color[C64_COLOR_SIZE];  // first: the colour watcher's table
    uint8_t rom[C64_ROM_SIZE];
} color_rom __attribute__((aligned(0x10000)));
uint8_t *const bus_color = color_rom.color;
uint8_t *const bus_rom = color_rom.rom;
volatile uint32_t bus_aec_settle = 2;

// Bus inputs in GPIO_HI_IN.
#define HI_INPUTS (C64_HI_GRW | C64_HI_VA14N | C64_HI_VA15N | C64_HI_AEC | C64_HI_PHI2 | C64_HI_CAS)

// State machines.
#define SM_RAM_DRIVE  0u  // PIO0
#define SM_CHAR_DRIVE 1u
#define SM_CPU_DRIVE  2u
#define SM_W_RAM      0u  // PIO1
#define SM_W_CHAR     1u
#define SM_W_COLOR    2u
#define SM_W_VIC      3u
#define SM_BANK       0u  // PIO2
#define SM_COLORD     1u
#define DRIVE_D   0xFF00u  // cpu_drive: drive D0 to D7
#define DRIVE_D03 0x0F00u  // cpu_drive: drive D0 to D3

// vic_tab: one word per bank (~VA15, ~VA14 as on the pins, so 3 - bank) and
// A0 to A11; byte A12 + 2 * A13 of it is RAM at that address, the number
// ram_drive reads from VA4 and VA5.
static inline __attribute__((always_inline)) uint8_t *vic_byte(uint32_t a) {
    const uint32_t word = ((3u - (a >> 14)) << 12) | (a & 0x0FFFu);
    return (uint8_t *)&vic_tab[word] + ((a >> 12) & 3u);
}

void bus_rebuild_tables(void) {
    for (uint32_t a = 0; a < C64_RAM_SIZE; a++) {
        *vic_byte(a) = bus_ram[a];
        char_tab[a] = bus_rom[C64_ROM_CHAR + (a & 0x0FFFu)];
    }
}

// Loads `value` into the OSR (and Y, if `y`) of a state machine.
static void sm_load(PIO pio, uint sm, uint32_t value, bool y) {
    pio_sm_put_blocking(pio, sm, value);
    pio_sm_exec(pio, sm, pio_encode_pull(false, true));
    if (y) {
        pio_sm_exec(pio, sm, pio_encode_mov(pio_y, pio_osr));
    }
}

static void init_sm(PIO pio, uint sm, uint offset, pio_sm_config *c) {
    sm_config_set_clkdiv_int_frac8(c, 1, 0);
    pio_sm_init(pio, sm, offset, c);
}

// A watcher's pointer goes from its RX FIFO to `ch_data`'s read trigger;
// `ch_data` copies one `size` entry to `dest`. `high`: DMA high priority,
// for the lookups that race the selects.
static void lookup_chain(PIO pio, uint sm, volatile void *dest, enum dma_channel_transfer_size size, bool high) {
    const uint ch_data = (uint)dma_claim_unused_channel(true);
    const uint ch_ptr = (uint)dma_claim_unused_channel(true);
    dma_channel_config d = dma_channel_get_default_config(ch_data);
    channel_config_set_transfer_data_size(&d, size);
    channel_config_set_read_increment(&d, false);
    channel_config_set_write_increment(&d, false);
    channel_config_set_dreq(&d, DREQ_FORCE);
    channel_config_set_high_priority(&d, high);
    dma_channel_configure(ch_data, &d, dest, bus_ram, 1, false);
    dma_channel_config p = dma_channel_get_default_config(ch_ptr);
    channel_config_set_transfer_data_size(&p, DMA_SIZE_32);
    channel_config_set_read_increment(&p, false);
    channel_config_set_write_increment(&p, false);
    channel_config_set_dreq(&p, pio_get_dreq(pio, sm, false));
    channel_config_set_high_priority(&p, high);
    dma_channel_configure(ch_ptr, &p, &dma_hw->ch[ch_data].al3_read_addr_trig, &pio->rxf[sm],
                          dma_encode_endless_transfer_count(), true);
}

static void init_pio_dma(void) {
    // PIO0: drivers on D0 to D7.
    pio_claim_sm_mask(pio0, (1u << SM_RAM_DRIVE) | (1u << SM_CHAR_DRIVE) | (1u << SM_CPU_DRIVE));
    uint off = pio_add_program(pio0, &ram_drive_program);
    pio_sm_config c = ram_drive_program_get_default_config(off);
    sm_config_set_in_pins(&c, C64_GPIO_COLORAM);
    sm_config_set_in_pin_count(&c, 4);
    sm_config_set_in_shift(&c, false, false, 32);
    sm_config_set_out_pins(&c, C64_GPIO_D0, 8);
    sm_config_set_out_shift(&c, true, false, 32);
    sm_config_set_jmp_pin(&c, C64_GPIO_COLORAM);
    init_sm(pio0, SM_RAM_DRIVE, off, &c);

    off = pio_add_program(pio0, &char_drive_program);
    c = char_drive_program_get_default_config(off);
    sm_config_set_out_pins(&c, C64_GPIO_D0, 8);
    sm_config_set_out_shift(&c, true, false, 32);
    sm_config_set_jmp_pin(&c, C64_GPIO_CHAROM);
    init_sm(pio0, SM_CHAR_DRIVE, off, &c);

    off = pio_add_program(pio0, &cpu_drive_program);
    c = cpu_drive_program_get_default_config(off);
    sm_config_set_out_pins(&c, C64_GPIO_D0, 8);
    sm_config_set_out_shift(&c, true, true, 16);
    init_sm(pio0, SM_CPU_DRIVE, off, &c);
    pio_sm_set_pindirs_with_mask(pio0, SM_CPU_DRIVE, 0, C64_LO_D);

    // PIO1: watchers on A0 to A15.
    pio_claim_sm_mask(pio1, 0xFu);
    const uint w16 = pio_add_program(pio1, &watch16_program);
    const uint wv = pio_add_program(pio1, &vic_watch_program);
    const struct {
        uint sm, offset;
        pio_sm_config c;
        uint32_t osr;
    } w[] = {
        {SM_W_RAM, w16, watch16_program_get_default_config(w16), (uint32_t)(uintptr_t)bus_ram >> 16},
        {SM_W_CHAR, w16, watch16_program_get_default_config(w16), (uint32_t)(uintptr_t)char_tab >> 16},
        {SM_W_COLOR, w16, watch16_program_get_default_config(w16), (uint32_t)(uintptr_t)bus_color >> 16},
        {SM_W_VIC, wv, vic_watch_program_get_default_config(wv), 0},
    };
    for (unsigned i = 0; i < 4; i++) {
        pio_sm_config cc = w[i].c;
        sm_config_set_in_pins(&cc, C64_GPIO_A0);
        if (w[i].sm == SM_W_COLOR) {
            sm_config_set_in_pin_count(&cc, 10);  // A0 to A9; A10 to A15 read 0
        }
        sm_config_set_in_shift(&cc, false, false, 32);
        sm_config_set_out_shift(&cc, true, false, 32);
        init_sm(pio1, w[i].sm, w[i].offset, &cc);
        pio_sm_exec(pio1, w[i].sm, pio_encode_mov_not(pio_y, pio_null));
        if (w[i].sm != SM_W_VIC) {
            sm_load(pio1, w[i].sm, w[i].osr, false);
        }
    }
    // vic_watch starts with bank 3 (~VA14, ~VA15 low) in X, until bank_watch
    // sends the real one.
    sm_load(pio1, SM_W_VIC, ((uint32_t)(uintptr_t)vic_tab >> 16) << 2, false);
    pio_sm_exec(pio1, SM_W_VIC, pio_encode_mov(pio_x, pio_osr));

    // PIO2 (GPIO16 to GPIO47): the VIC-II bank, COLORD.
    pio_set_gpio_base(pio2, 16);
    pio_claim_sm_mask(pio2, (1u << SM_BANK) | (1u << SM_COLORD));
    off = pio_add_program(pio2, &bank_watch_program);
    c = bank_watch_program_get_default_config(off);
    sm_config_set_in_pins(&c, C64_GPIO_VA14N);
    sm_config_set_in_shift(&c, false, false, 32);
    init_sm(pio2, SM_BANK, off, &c);
    pio_sm_exec(pio2, SM_BANK, pio_encode_mov_not(pio_y, pio_null));
    sm_load(pio2, SM_BANK, (uint32_t)(uintptr_t)vic_tab >> 16, false);
    off = pio_add_program(pio2, &colord_out_program);
    c = colord_out_program_get_default_config(off);
    sm_config_set_out_pins(&c, C64_GPIO_COLORD0, 4);
    sm_config_set_out_shift(&c, true, false, 32);
    init_sm(pio2, SM_COLORD, off, &c);
    for (uint gpio = C64_GPIO_COLORD0; gpio < C64_GPIO_COLORD0 + 4; gpio++) {
        pio_gpio_init(pio2, gpio);
    }
    pio_sm_set_consecutive_pindirs(pio2, SM_COLORD, C64_GPIO_COLORD0, 4, true);

    // Lookups: pointers from PIO1 to the tables, entries to the drivers.
    lookup_chain(pio1, SM_W_RAM, &pio0->rxf_putget[SM_RAM_DRIVE][1], DMA_SIZE_8, true);
    lookup_chain(pio1, SM_W_VIC, &pio0->rxf_putget[SM_RAM_DRIVE][0], DMA_SIZE_32, true);
    lookup_chain(pio1, SM_W_CHAR, &pio0->rxf_putget[SM_CHAR_DRIVE][0], DMA_SIZE_8, false);
    lookup_chain(pio1, SM_W_COLOR, &pio2->txf[SM_COLORD], DMA_SIZE_8, false);
    // The bank watcher's words go straight to vic_watch.
    const uint ch_bank = (uint)dma_claim_unused_channel(true);
    dma_channel_config b = dma_channel_get_default_config(ch_bank);
    channel_config_set_transfer_data_size(&b, DMA_SIZE_32);
    channel_config_set_read_increment(&b, false);
    channel_config_set_write_increment(&b, false);
    channel_config_set_dreq(&b, pio_get_dreq(pio2, SM_BANK, false));
    dma_channel_configure(ch_bank, &b, &pio1->txf[SM_W_VIC], &pio2->rxf[SM_BANK],
                          dma_encode_endless_transfer_count(), true);

    pio_set_sm_mask_enabled(pio2, (1u << SM_BANK) | (1u << SM_COLORD), true);
    pio_set_sm_mask_enabled(pio1, 0xFu, true);
    pio_set_sm_mask_enabled(pio0, (1u << SM_RAM_DRIVE) | (1u << SM_CHAR_DRIVE) | (1u << SM_CPU_DRIVE), true);
}

void bus_init_pins(void) {
    for (uint gpio = 0; gpio <= C64_GPIO_COLORD0 + 3; gpio++) {
        gpio_init(gpio);
        gpio_disable_pulls(gpio);  // RP2350 erratum E9
        gpio_set_input_enabled(gpio, true);
    }
    // D and COLORD at 2 mA with slow edges, for the bench: faster ones
    // glitched the address lines that the watchers sample (0.03 to 4 % wrong
    // VIC-II bytes over the ribbon cable), and the light load there makes
    // these cost no measurable time. On the board, D0 to D7 drive the C64
    // data bus directly and COLORD0 to COLORD3 the VIC-II, a much larger
    // load: set the strength from the prototype. `drive` changes it for tests.
    for (uint gpio = C64_GPIO_D0; gpio < C64_GPIO_D0 + 8; gpio++) {
        gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_2MA);
        gpio_set_slew_rate(gpio, GPIO_SLEW_RATE_SLOW);
        pio_gpio_init(pio0, gpio);
    }
    for (uint gpio = C64_GPIO_COLORD0; gpio < C64_GPIO_COLORD0 + 4; gpio++) {
        gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_2MA);
        gpio_set_slew_rate(gpio, GPIO_SLEW_RATE_SLOW);
    }
    const uint inputs[] = {C64_GPIO_PHI2, C64_GPIO_CAS};
    for (uint i = 0; i < 2; i++) {
        gpio_init(inputs[i]);
        gpio_disable_pulls(inputs[i]);
    }
    const uint outputs[] = {C64_GPIO_TRIG, C64_GPIO_HEARTBEAT};
    for (uint i = 0; i < 2; i++) {
        gpio_init(outputs[i]);
        gpio_disable_pulls(outputs[i]);
        gpio_set_dir(outputs[i], GPIO_OUT);
    }
    bus_rebuild_tables();
    init_pio_dma();
    // The DMA lookups get the bus fabric first, then the bus loop.
    busctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_R_BITS | BUSCTRL_BUS_PRIORITY_DMA_W_BITS |
                           BUSCTRL_BUS_PRIORITY_PROC1_BITS;
}

// Skips the two-flip-flop synchronizer on the bus inputs, for the CPU and
// the PIO blocks: saves about two clocks per access, at the risk of a
// metastable read on an edge.
void bus_set_sync_bypass(bool on) {
    syscfg_hw->proc_in_sync_bypass = on ? 0xFFFFFFFFu : 0u;
    syscfg_hw->proc_in_sync_bypass_hi = on ? HI_INPUTS : 0u;
    pio0->input_sync_bypass = on ? 0xFFFFFF00u : 0u;
    pio1->input_sync_bypass = on ? 0xFFFFFF00u : 0u;
    pio2->input_sync_bypass = on ? (3u << (C64_GPIO_VA14N - 16)) : 0u;
}

// About three clocks per loop.
static inline __attribute__((always_inline)) void delay_loops(uint32_t n) {
    if (n) {
        __asm volatile("1: subs %0, %0, #1\n bne 1b" : "+r"(n) : : "cc");
    }
}

#if BUS_TRACE_PINS
#define TP_CPU_ON()  (sio->gpio_hi_set = C64_HI_HEARTBEAT)
#define TP_CPU_OFF() (sio->gpio_hi_clr = C64_HI_HEARTBEAT)
#else
#define TP_CPU_ON()  ((void)0)
#define TP_CPU_OFF() ((void)0)
#endif

void __not_in_flash_func(bus_run)(void) {
    sio_hw_t *const sio = sio_hw;
    pio_hw_t *const pio = pio0_hw;
    uint8_t *const ram = bus_ram;
    uint8_t *const color = bus_color;
    const uint8_t *const rom = bus_rom;

    (void)save_and_disable_interrupts();

    for (;;) {
        uint32_t lo;
        do {
            lo = sio->gpio_in;
        } while ((lo & C64_LO_SELECTS) == C64_LO_SELECTS);

        if (!(lo & C64_LO_CASRAM)) {
            if ((lo & C64_LO_COLORAM) && !(lo & C64_LO_RW)) {
                // 6510 RAM write: keep the last sample of D0 to D7 taken
                // while ~CASRAM is low and PHI2 is still high.
                TP_CPU_ON();
                const uint32_t a = (lo >> C64_GPIO_A0) & 0xFFFFu;
                uint32_t d = lo;
                for (;;) {
                    const uint32_t x = sio->gpio_in;
                    const uint32_t h = sio->gpio_hi_in;
                    if ((x & C64_LO_CASRAM) || !(h & C64_HI_PHI2)) {
                        break;
                    }
                    d = x;
                }
                ram[a] = (uint8_t)d;
                *vic_byte(a) = (uint8_t)d;
                bus_stats.ram_write++;
                TP_CPU_OFF();
            }
            // Reads: ram_drive.
            while (!(sio->gpio_in & C64_LO_CASRAM)) {
            }
            continue;
        }

        if (!(lo & (C64_LO_BASIC | C64_LO_KERNAL))) {
            continue;  // both low: a glitch
        }
        if ((lo & (C64_LO_BASIC | C64_LO_KERNAL)) != (C64_LO_BASIC | C64_LO_KERNAL)) {
            // 6510 BASIC or KERNAL read (the CPLD selects ROMs for reads only).
            TP_CPU_ON();
            const uint32_t a = (lo >> C64_GPIO_A0) & 0x1FFFu;
            const uint32_t sel = ~lo & (C64_LO_BASIC | C64_LO_KERNAL);
            pio->txf[SM_CPU_DRIVE] = rom[(sel & C64_LO_BASIC ? C64_ROM_BASIC : C64_ROM_KERNAL) + a] | DRIVE_D;
            while (!(sio->gpio_in & sel)) {
            }
            pio->txf[SM_CPU_DRIVE] = 0;
            bus_stats.rom_read++;
            TP_CPU_OFF();
            continue;
        }

        if (!(lo & C64_LO_COLORAM)) {
            uint32_t hi = sio->gpio_hi_in;
            if (!(hi & C64_HI_AEC)) {
                continue;  // VIC-II cycle: ~COLORAM is low throughout
            }
            // AEC rises before the CPLD's ~COLORAM of the VIC-II cycle does:
            // only a ~COLORAM still low after bus_aec_settle is the 6510's.
            delay_loops(bus_aec_settle);
            lo = sio->gpio_in;
            if (lo & C64_LO_COLORAM) {
                continue;
            }
            // 6510 colour RAM access, A0 to A9.
            TP_CPU_ON();
            const uint32_t a = (lo >> C64_GPIO_A0) & (C64_COLOR_SIZE - 1u);
            if (lo & C64_LO_RW) {
                pio->txf[SM_CPU_DRIVE] = color[a] | DRIVE_D03;  // D4 to D7 stay undriven
                do {
                    lo = sio->gpio_in;
                    hi = sio->gpio_hi_in;
                } while (!(lo & C64_LO_COLORAM) && (hi & C64_HI_AEC));
                pio->txf[SM_CPU_DRIVE] = 0;
                bus_stats.color_read++;
            } else {
                // Write: D0 to D3 while GR/~W is low.
                uint32_t d = 0;
                bool seen = false;
                for (;;) {
                    const uint32_t x = sio->gpio_in;
                    const uint32_t h = sio->gpio_hi_in;
                    if ((x & C64_LO_COLORAM) || !(h & C64_HI_AEC)) {
                        break;
                    }
                    if (!(h & C64_HI_GRW) && (h & C64_HI_PHI2)) {
                        d = x;
                        seen = true;
                    } else if (seen) {
                        break;  // GR/~W rose: d is the last sample before
                    }
                }
                if (seen) {
                    color[a] = (uint8_t)(d & 0x0Fu);
                    bus_stats.color_write++;
                }
                while (!(sio->gpio_in & C64_LO_COLORAM) && (sio->gpio_hi_in & C64_HI_AEC)) {
                }
            }
            TP_CPU_OFF();
            continue;
        }

        // ~CHAROM: char_drive.
        while (!(sio->gpio_in & C64_LO_CHAROM)) {
        }
    }
}
