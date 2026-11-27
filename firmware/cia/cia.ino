// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Firmware of the CIA controllers: U1501 (CIA 1, #183) or U1601 (CIA 2,
// #184), chosen with CIA_UNIT (build.sh cia1|cia2). Both run the shared 6526
// model and bus interface of the Compact64Cia library (#182).
//
// This first version is the bench build for #182: it starts at once and
// reports over USB serial. On the board the controller has no USB serial
// (U1501's USB port is the keyboard host); the link to the supervisor (I2C)
// and the rule that every pin stays released until the rails are up come
// later (#48).
//
// Core 0: serial commands, the TOD edges, heartbeat. Core 1: the bus loop.

#include <Compact64Bus.h>
#include <Compact64Cia.h>

#include "board.h"
#include "hardware/structs/sio.h"
#include "hardware/vreg.h"

C64BUS_CORE_VOLTAGE()

static cia_t cia;
static volatile bool core1_ready = false;
static cia_model_t model = CIA_MODEL_6526;
static unsigned tod_hz = 0;  // 0: from the bench pin (GPIO18)
static repeating_timer_t tod_timer;

static bool tod_edge(repeating_timer_t *) {
    cia_bus_tod_edges = cia_bus_tod_edges + 1;
    return true;
}

static void start_tod(unsigned hz) {
    cancel_repeating_timer(&tod_timer);
    tod_hz = hz;
    cia_bus_tod_pin = hz == 0;
    if (hz) {
        add_repeating_timer_us(-(int64_t)(1000000 / hz), tod_edge, nullptr, &tod_timer);
    }
}

static void print_info() {
    Serial.printf("info firmware=%s-bench sys_hz=%lu vreg=%u model=%s tod=%s%s irq=%s\n", board_name,
                  (unsigned long)rp2040.f_cpu(), (unsigned)vreg_get_voltage(),
                  model == CIA_MODEL_6526 ? "6526" : "8521", tod_hz ? (tod_hz == 50 ? "50" : "60") : "pin",
                  tod_hz ? "hz" : "", cia_bus_irq_push_pull ? "pp" : "od");
}

static void print_stats() {
    const uint32_t n = cia_bus_stats.cycles;
    Serial.printf("stats cycles=%lu reads=%lu writes=%lu resets=%lu quiet=%lu overruns=%lu lagging=%lu "
                  "ready_max=%lu ready_avg=%lu loop_max=%lu loop_avg=%lu\n",
                  (unsigned long)n, (unsigned long)cia_bus_stats.reads,
                  (unsigned long)cia_bus_stats.writes, (unsigned long)cia_bus_stats.resets,
                  (unsigned long)cia_bus_stats.quiet, (unsigned long)cia_bus_stats.overruns,
                  (unsigned long)cia_bus_stats.lagging,
                  (unsigned long)cia_bus_stats.ready_max,
                  (unsigned long)(n ? cia_bus_stats.ready_sum / n : 0),
                  (unsigned long)cia_bus_stats.loop_max,
                  (unsigned long)(n ? cia_bus_stats.loop_sum / n : 0));
}

// `prof`: clocks per call of the parts of the bus loop, on core 0 with
// interrupts off, on copies of the model (core 1 keeps running). For
// finding where the loop's time goes; not for the board.
#define DEMCR (*(volatile uint32_t *)0xE000EDFCu)
#define DWT_CTRL (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)

static cia_t prof_cia;
static uint32_t prof_img[4];
static void __not_in_flash_func(p_none)(void) {}
static void __not_in_flash_func(p_clock)(void) { cia_clock_full(&prof_cia); }
static void __not_in_flash_func(p_clock_q)(void) { cia_clock(&prof_cia); }
static void __not_in_flash_func(p_image)(void) { cia_bus_image(&prof_cia, prof_img); }
static void __not_in_flash_func(p_inputs)(void) {
    (void)board.inputs(&prof_cia, true, sio_hw->gpio_in & board.in_mask, sio_hw->gpio_hi_in & board.in_mask_hi);
}
static void __not_in_flash_func(p_write)(void) { cia_write(&prof_cia, CIA_TALO, 0x10); }
static void __not_in_flash_func(p_read_icr)(void) { cia_read_effects(&prof_cia, CIA_ICR); }
static void __not_in_flash_func(p_sio)(void) { (void)sio_hw->gpio_in; }
// 64 ALU instructions, and 32 load/add pairs, from SRAM and from flash.
#define ALU8 "adds r0, #1\n adds r1, #1\n adds r2, #1\n adds r3, #1\n adds r0, #1\n adds r1, #1\n adds r2, #1\n adds r3, #1\n"
#define LD4 "ldr r1, [%0]\n adds r0, r1\n ldr r2, [%0, #4]\n adds r0, r2\n ldr r3, [%0, #8]\n adds r0, r3\n ldr r1, [%0, #12]\n adds r0, r1\n"
static void __not_in_flash_func(p_alu_ram)(void) {
    asm volatile(ALU8 ALU8 ALU8 ALU8 ALU8 ALU8 ALU8 ALU8 ::: "r0", "r1", "r2", "r3", "cc");
}
static void p_alu_flash(void) {
    asm volatile(ALU8 ALU8 ALU8 ALU8 ALU8 ALU8 ALU8 ALU8 ::: "r0", "r1", "r2", "r3", "cc");
}
#define BR4 "b 1f\n 1: b 2f\n 2: b 3f\n 3: b 4f\n 4:\n"
static void __not_in_flash_func(p_br_ram)(void) { asm volatile(BR4 BR4 BR4 BR4 BR4 BR4 BR4 BR4); }
static void p_br_flash(void) { asm volatile(BR4 BR4 BR4 BR4 BR4 BR4 BR4 BR4); }
static void __scratch_y("prof") p_br_scy(void) { asm volatile(BR4 BR4 BR4 BR4 BR4 BR4 BR4 BR4); }
#define LS4 "ldrb r1, [%0]\n strb r1, [%0, #1]\n ldrb r2, [%0, #2]\n strb r2, [%0, #3]\n"
static void __not_in_flash_func(p_ls_ram)(void) {
    asm volatile(LS4 LS4 LS4 LS4 LS4 LS4 LS4 LS4 ::"r"(prof_img) : "r1", "r2", "memory");
}
static void __not_in_flash_func(p_ld_ram)(void) {
    asm volatile(LD4 LD4 LD4 LD4 LD4 LD4 LD4 LD4 ::"r"(prof_img) : "r0", "r1", "r2", "r3", "cc", "memory");
}
static void p_ld_flash(void) {
    asm volatile(LD4 LD4 LD4 LD4 LD4 LD4 LD4 LD4 ::"r"(prof_img) : "r0", "r1", "r2", "r3", "cc", "memory");
}

static uint32_t __not_in_flash_func(prof_time)(void (*fn)(void), unsigned n) {
    uint32_t total = 0;
    for (unsigned i = 0; i < n; i++) {
        const uint32_t t0 = DWT_CYCCNT;
        fn();
        total += DWT_CYCCNT - t0;
    }
    return total;
}

static void prof() {
    DEMCR |= 1u << 24;
    DWT_CTRL |= 1u;
    struct {
        const char *name;
        void (*fn)(void);
        uint8_t cra, crb;  // control registers to start from (0x11: load and start)
        bool live;         // start from a copy of the live model
    } const cases[] = {
        {"none", p_none, 0, 0, false},         {"clock_idle", p_clock, 0, 0, false},
        {"clock_ta", p_clock, 0x11, 0, false}, {"clock_ta_tb", p_clock, 0x11, 0x11, false},
        {"clock_live", p_clock, 0, 0, true},   {"clock_q_ta_tb", p_clock_q, 0x11, 0x11, false},   {"image", p_image, 0, 0, true},
        {"inputs_live", p_inputs, 0, 0, true},   {"write_talo", p_write, 0, 0, false},
        {"read_icr", p_read_icr, 0x11, 0, false}, {"sio_in", p_sio, 0, 0, false},
        {"alu64_ram", p_alu_ram, 0, 0, false}, {"alu64_flash", p_alu_flash, 0, 0, false},
        {"ld32_ram", p_ld_ram, 0, 0, false},   {"ld32_flash", p_ld_flash, 0, 0, false},
        {"br32_ram", p_br_ram, 0, 0, false},   {"br32_flash", p_br_flash, 0, 0, false},
        {"br32_scratch_y", p_br_scy, 0, 0, false}, {"ldst32_ram", p_ls_ram, 0, 0, false},
    };
    const unsigned n = 1000;
    for (const auto &c : cases) {
        if (c.live) {
            memcpy(&prof_cia, &cia, sizeof(cia));
        } else {
            cia_init(&prof_cia, model);
            cia_write(&prof_cia, CIA_TALO, 0x00);
            cia_write(&prof_cia, CIA_TAHI, 0x10);
            cia_write(&prof_cia, CIA_TBLO, 0x00);
            cia_write(&prof_cia, CIA_TBHI, 0x10);
            cia_write(&prof_cia, CIA_ICR, 0x83);
            cia_write(&prof_cia, CIA_CRA, c.cra);
            cia_write(&prof_cia, CIA_CRB, c.crb);
            for (unsigned i = 0; i < 4; i++) {
                cia_clock(&prof_cia);
            }
        }
        const uint32_t irq = save_and_disable_interrupts();
        const uint32_t t = prof_time(c.fn, n);
        restore_interrupts(irq);
        Serial.printf("prof %s clocks=%lu.%02lu\n", c.name, (unsigned long)(t / n),
                      (unsigned long)((t % n) / 10));
    }
    Serial.println("end");
}

static void handle_command(char *line) {
    char *cmd = strtok(line, " \t");
    char *arg = strtok(nullptr, " \t");
    if (!cmd) {
        return;
    }
    if (!strcmp(cmd, "info")) {
        print_info();
    } else if (!strcmp(cmd, "stats")) {
        print_stats();
    } else if (!strcmp(cmd, "clear")) {
        // Core 1 may count while this runs; the bench clears between runs.
        memset((void *)&cia_bus_stats, 0, sizeof(cia_bus_stats));
        Serial.println("ok");
    } else if (!strcmp(cmd, "kinds")) {
        static const char *const names[7] = {"idle", "idle_full", "read", "read_full", "write", "write_full",
                                             "stretch"};
        for (unsigned k = 0; k < 7; k++) {
            const uint32_t n = cia_bus_stats.kind_n[k];
            Serial.printf("kind %s n=%lu avg=%lu max=%lu\n", names[k], (unsigned long)n,
                          (unsigned long)(n ? cia_bus_stats.kind_sum[k] / n : 0),
                          (unsigned long)cia_bus_stats.kind_max[k]);
        }
        Serial.println("end");
    } else if (!strcmp(cmd, "prof")) {
        prof();
    } else if (!strcmp(cmd, "regs")) {
        // A snapshot taken while core 1 runs: for debugging only.
        Serial.print("regs");
        for (unsigned r = 0; r < 16; r++) {
            Serial.printf(" %02x", cia_peek(&cia, r));
        }
        Serial.printf(" irq=%d\n", cia_irq(&cia));
    } else if (!strcmp(cmd, "tod") && arg) {
        const unsigned hz = !strcmp(arg, "pin") ? 0 : strtoul(arg, nullptr, 0);
        if (hz == 0 || hz == 50 || hz == 60) {
            start_tod(hz);
            Serial.println("ok");
        } else {
            Serial.println("error tod 50|60|pin");
        }
    } else if (!strcmp(cmd, "irq") && arg) {
        cia_bus_irq_push_pull = !strcmp(arg, "pp");
        Serial.println("ok");
    } else if (!strcmp(cmd, "sync") && arg) {
        cia_bus_set_sync_bypass(!strcmp(arg, "bypass"));
        Serial.println("ok");
    } else {
        Serial.println("error commands: info stats clear regs tod 50|60|pin irq od|pp sync bypass|normal");
    }
}

void setup() {
    Serial.begin(115200);
    gpio_init(board_heartbeat_gpio);
    gpio_set_dir(board_heartbeat_gpio, GPIO_OUT);
    start_tod(tod_hz);
}

void loop() {
    static char line[96];
    static size_t len = 0;
    static uint32_t last_beat = 0;

    while (Serial.available()) {
        const int c = Serial.read();
        if (c == '\n' || c == '\r') {
            line[len] = '\0';
            if (len) {
                handle_command(line);
            }
            len = 0;
        } else if (len < sizeof(line) - 1) {
            line[len++] = (char)c;
        }
    }
    if (core1_ready && millis() - last_beat >= 500) {
        last_beat = millis();
        gpio_xor_mask64(1ull << board_heartbeat_gpio);
    }
}

void setup1() {
    cia_bus_irq_push_pull = true;  // bench: no pull-up on ~IRQ
    cia_init(&cia, model);
    board_init();
    cia_bus_init();
    core1_ready = true;
}

void loop1() {
    cia_bus_run(&cia, &board);  // never returns
}
