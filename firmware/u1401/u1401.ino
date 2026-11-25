// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Firmware of the RAM and ROM controller (U1401, RP2354B, #11).
//
// This first version is the bench build for #159: it fills RAM, colour RAM
// and the ROM image array from the test pattern shared with the exerciser and
// reports over USB serial. On the board, U1401 has no USB; loading the ROM
// images from the EEPROM (U1403) and the I2C link to the supervisor come
// later.
//
// Core 0: setup, serial commands, heartbeat. Core 1: the bus loop (bus.c).

#include <Compact64Bus.h>

#include "bus.h"
#include "hardware/structs/sio.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/vreg.h"

C64BUS_CORE_VOLTAGE()

static volatile bool bus_ready = false;
static uint32_t pattern_seed = 1;

static void fill_memory(uint32_t seed) {
    c64bench_fill(seed, bus_ram, bus_color, bus_rom);
    bus_rebuild_tables();
    pattern_seed = seed;
}

static void print_stats() {
    Serial.printf("stats ram_write=%lu rom_read=%lu color_read=%lu color_write=%lu\n",
                  (unsigned long)bus_stats.ram_write, (unsigned long)bus_stats.rom_read,
                  (unsigned long)bus_stats.color_read, (unsigned long)bus_stats.color_write);
}

static void print_info() {
    Serial.printf("info firmware=u1401-bench sys_hz=%lu vreg=%u seed=%lu aecsettle=%lu trace_pins=%d\n",
                  (unsigned long)rp2040.f_cpu(), (unsigned)vreg_get_voltage(), (unsigned long)pattern_seed,
                  (unsigned long)bus_aec_settle, BUS_TRACE_PINS);
}

static void handle_command(char *line) {
    char *cmd = strtok(line, " \t");
    char *arg = strtok(nullptr, " \t");
    if (!cmd) {
        return;
    }
    if (!strcmp(cmd, "stats")) {
        print_stats();
    } else if (!strcmp(cmd, "clear")) {
        // Core 1 may count while this runs; the bench clears between runs.
        memset((void *)&bus_stats, 0, sizeof(bus_stats));
        Serial.println("ok");
    } else if (!strcmp(cmd, "fill")) {
        fill_memory(arg ? strtoul(arg, nullptr, 0) : 1);
        Serial.println("ok");
    } else if (!strcmp(cmd, "sync")) {
        bus_set_sync_bypass(arg && !strcmp(arg, "bypass"));
        Serial.println("ok");
    } else if (!strcmp(cmd, "aecsettle") && arg) {
        bus_aec_settle = strtoul(arg, nullptr, 0);
        Serial.println("ok");
    } else if (!strcmp(cmd, "peek") && arg) {
        const uint32_t a = strtoul(arg, nullptr, 0) & 0xFFFFu;
        Serial.printf("peek addr=0x%04lx ram=0x%02x color=0x%x\n", (unsigned long)a, bus_ram[a],
                      bus_color[a & (C64_COLOR_SIZE - 1u)]);
    } else if (!strcmp(cmd, "drive") && arg) {
        // Drive strength (mA) and slew of D0 to D7 and COLORD0 to COLORD3.
        const char *slew = strtok(nullptr, " \t");
        const unsigned ma = strtoul(arg, nullptr, 0);
        const enum gpio_drive_strength s = ma >= 12 ? GPIO_DRIVE_STRENGTH_12MA
                                         : ma >= 8 ? GPIO_DRIVE_STRENGTH_8MA
                                         : ma >= 4 ? GPIO_DRIVE_STRENGTH_4MA
                                                   : GPIO_DRIVE_STRENGTH_2MA;
        const bool fast = !slew || strcmp(slew, "slow");
        // Optional mask: bits 0 to 7 for D0 to D7, 8 to 11 for COLORD0 to
        // COLORD3; default all twelve.
        const char *m = strtok(nullptr, " \t");
        const uint32_t bits = m ? strtoul(m, nullptr, 0) & 0xFFFu : 0xFFFu;
        const uint64_t mask = (bits & 0xFFull) | ((uint64_t)(bits >> 8) << C64_GPIO_COLORD0);
        for (uint g = 0; g < 40; g++) {
            if (mask & (1ull << g)) {
                gpio_set_drive_strength(g, s);
                gpio_set_slew_rate(g, fast ? GPIO_SLEW_RATE_FAST : GPIO_SLEW_RATE_SLOW);
            }
        }
        Serial.println("ok");
    } else if (!strcmp(cmd, "regs")) {
        for (uint sm = 0; sm < 3; sm++) {
            Serial.printf("regs pio0 sm%u exec=%08lx shift=%08lx pin=%08lx pc=%lu\n", sm,
                          (unsigned long)pio0->sm[sm].execctrl, (unsigned long)pio0->sm[sm].shiftctrl,
                          (unsigned long)pio0->sm[sm].pinctrl, (unsigned long)pio0->sm[sm].addr);
        }
        Serial.println("ok");
    } else if (!strcmp(cmd, "info")) {
        print_info();
    } else {
        Serial.println("error commands: info stats clear fill [seed] sync bypass|normal aecsettle n drive mA fast|slow [mask] peek addr regs");
    }
}

void setup() {
    fill_memory(1);
    bus_ready = true;
    Serial.begin(115200);
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
    if (!BUS_TRACE_PINS && millis() - last_beat >= 500) {
        last_beat = millis();
        sio_hw->gpio_hi_togl = C64_HI_HEARTBEAT;
    }
}

void setup1() {
    while (!bus_ready) {
    }
    // Bench: the I2C pins (GPIO40 to GPIO43) are unused; inputs without
    // pulls, so that a straight cable to the exerciser does not load them.
    for (uint gpio = C64_GPIO_SDA; gpio <= C64_GPIO_EE_SCL; gpio++) {
        gpio_init(gpio);
        gpio_disable_pulls(gpio);
    }
    bus_init_pins();
    bus_set_sync_bypass(false);
}

void loop1() {
    bus_run();  // never returns
}
