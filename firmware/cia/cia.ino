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
    Serial.printf("stats cycles=%lu reads=%lu writes=%lu resets=%lu overruns=%lu max_lag=%lu "
                  "loop_max=%lu loop_avg=%lu\n",
                  (unsigned long)n, (unsigned long)cia_bus_stats.reads,
                  (unsigned long)cia_bus_stats.writes, (unsigned long)cia_bus_stats.resets,
                  (unsigned long)cia_bus_stats.overruns, (unsigned long)cia_bus_stats.max_lag,
                  (unsigned long)cia_bus_stats.loop_max,
                  (unsigned long)(n ? cia_bus_stats.loop_sum / n : 0));
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
