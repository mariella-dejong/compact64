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

static volatile bool bus_ready = false;
static uint32_t pattern_seed = 1;

static void fill_memory(uint32_t seed) {
    c64bench_fill(seed, bus_ram, bus_color, bus_rom);
    pattern_seed = seed;
}

static void print_stats() {
    Serial.printf("stats ram_read=%lu ram_write=%lu rom_read=%lu color_read=%lu color_write=%lu "
                  "vic_ram=%lu vic_char=%lu late=%lu\n",
                  (unsigned long)bus_stats.ram_read, (unsigned long)bus_stats.ram_write,
                  (unsigned long)bus_stats.rom_read, (unsigned long)bus_stats.color_read,
                  (unsigned long)bus_stats.color_write, (unsigned long)bus_stats.vic_ram,
                  (unsigned long)bus_stats.vic_char, (unsigned long)bus_stats.late);
}

static void print_info() {
    Serial.printf("info firmware=u1401-bench sys_hz=%lu seed=%lu vicdelay=%lu aecsettle=%lu\n",
                  (unsigned long)rp2040.f_cpu(), (unsigned long)pattern_seed,
                  (unsigned long)bus_vic_sample_delay, (unsigned long)bus_aec_settle);
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
    } else if (!strcmp(cmd, "vicdelay") && arg) {
        bus_vic_sample_delay = strtoul(arg, nullptr, 0);
        Serial.println("ok");
    } else if (!strcmp(cmd, "aecsettle") && arg) {
        bus_aec_settle = strtoul(arg, nullptr, 0);
        Serial.println("ok");
    } else if (!strcmp(cmd, "peek") && arg) {
        const uint32_t a = strtoul(arg, nullptr, 0) & 0xFFFFu;
        Serial.printf("peek addr=0x%04lx ram=0x%02x color=0x%x\n", (unsigned long)a, bus_ram[a],
                      bus_color[a & (C64_COLOR_SIZE - 1u)]);
    } else if (!strcmp(cmd, "info")) {
        print_info();
    } else {
        Serial.println("error commands: info stats clear fill [seed] sync bypass|normal vicdelay n aecsettle n peek addr");
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
    if (millis() - last_beat >= 500) {
        last_beat = millis();
        sio_hw->gpio_hi_togl = C64_HI_HEARTBEAT;
    }
}

void setup1() {
    while (!bus_ready) {
    }
    bus_init_pins();
    bus_set_sync_bypass(false);
}

void loop1() {
    bus_run();  // never returns
}
