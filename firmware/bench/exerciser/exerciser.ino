// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Bus exerciser for the U1401 bench test (#159). Plays the 6510, the VIC-II
// and the CPLD towards a board running the U1401 firmware, with every jumper
// from GPIOn to GPIOn, and checks the data it returns. Line commands over USB
// serial; tools/bench/bench.py drives the sweeps.

#include <Compact64Bus.h>

#include "engine.h"
#include "gen.h"

static uint32_t next_seed = 1;
static uint32_t pattern_seed = 1;

static void print_params() {
    for (unsigned i = 0; i < ex_param_count; i++) {
        Serial.printf("param %s=%ld\n", ex_param_refs[i].name, (long)*ex_param_refs[i].value);
    }
    Serial.println("end");
}

static bool set_param(const char *name, const char *value) {
    for (unsigned i = 0; i < ex_param_count; i++) {
        if (!strcmp(ex_param_refs[i].name, name)) {
            *ex_param_refs[i].value = (int32_t)strtol(value, nullptr, 0);
            return true;
        }
    }
    return false;
}

static void run(uint32_t batches, uint32_t seed) {
    ex_stats_t st;
    memset(&st, 0, sizeof(st));
    ex_failure_count = 0;
    ex_run_batches(batches, seed, &st);
    Serial.printf("result batches=%lu cycles=%lu samples=%lu errors=%lu moved=%lu max_move_ns=%lu overflow=%lu "
                  "tx_stall=%lu rx_stall=%lu timeout=%lu speed=%ld sample_mode=%ld sample_off=%ld seed=%lu\n",
                  (unsigned long)st.batches, (unsigned long)st.cycles, (unsigned long)st.samples,
                  (unsigned long)st.errors, (unsigned long)st.moved,
                  (unsigned long)((uint64_t)st.max_move * 1000000000u / ex_sys_hz()), (unsigned long)st.overflow,
                  (unsigned long)st.tx_stall, (unsigned long)st.rx_stall, (unsigned long)st.timeout,
                  (long)ex_params.speed, (long)ex_params.sample_mode, (long)ex_params.sample_off,
                  (unsigned long)seed);
    for (unsigned c = 0; c < CK_COUNT; c++) {
        if (st.n[c]) {
            Serial.printf("check name=%s n=%lu err=%lu\n", ex_check_names[c], (unsigned long)st.n[c],
                          (unsigned long)st.err[c]);
        }
    }
    for (unsigned i = 0; i < ex_failure_count; i++) {
        const ex_failure_t *f = &ex_failures[i];
        Serial.printf("fail batch=%lu cycle=%lu check=%s addr=0x%04x exp_d=0x%02x got_d=0x%02x mask_d=0x%02x "
                      "exp_c=0x%x got_c=0x%x mask_c=0x%x exp_s=%u got_s=%u mask_s=%u\n",
                      (unsigned long)f->batch, (unsigned long)f->cycle, ex_check_names[f->check], f->addr,
                      f->exp_d, f->got_d, f->mask_d, f->exp_c, f->got_c, f->mask_c, f->exp_s, f->got_s,
                      f->mask_s);
    }
    Serial.println("end");
}

static void calibrate(uint32_t out, uint32_t in, int32_t from, int32_t to, uint32_t reps) {
    for (int32_t k = from; k <= to; k++) {
        bool ok;
        const uint32_t high = ex_calibrate(out, in, k, reps, &ok);
        if (!ok) {
            Serial.printf("error cal out=%lu in=%lu offset=%ld\n", (unsigned long)out, (unsigned long)in, (long)k);
            break;
        }
        Serial.printf("cal offset=%ld high=%lu reps=%lu\n", (long)k, (unsigned long)high, (unsigned long)reps);
    }
    Serial.println("end");
}

static void handle_command(char *line) {
    char *argv[6] = {};
    int argc = 0;
    for (char *t = strtok(line, " \t"); t && argc < 6; t = strtok(nullptr, " \t")) {
        argv[argc++] = t;
    }
    if (!argc) {
        return;
    }
    const char *cmd = argv[0];
    if (!strcmp(cmd, "info")) {
        Serial.printf("info firmware=exerciser sys_hz=%lu pattern_seed=%lu next_seed=%lu\n",
                      (unsigned long)ex_sys_hz(), (unsigned long)pattern_seed, (unsigned long)next_seed);
    } else if (!strcmp(cmd, "get")) {
        print_params();
    } else if (!strcmp(cmd, "set") && argc == 3) {
        Serial.println(set_param(argv[1], argv[2]) ? "ok" : "error unknown parameter");
    } else if (!strcmp(cmd, "pal")) {
        ex_params_default_pal();
        Serial.println("ok");
    } else if (!strcmp(cmd, "ntsc")) {
        ex_params_default_pal();
        ex_params_ntsc();
        Serial.println("ok");
    } else if (!strcmp(cmd, "fill")) {
        pattern_seed = argc > 1 ? strtoul(argv[1], nullptr, 0) : 1;
        ex_model_fill(pattern_seed);
        Serial.println("ok");
    } else if (!strcmp(cmd, "run")) {
        const uint32_t batches = argc > 1 ? strtoul(argv[1], nullptr, 0) : 1;
        const uint32_t seed = argc > 2 ? strtoul(argv[2], nullptr, 0) : next_seed;
        next_seed = seed + batches;
        run(batches, seed);
    } else if (!strcmp(cmd, "cal") && argc >= 5) {
        calibrate(strtoul(argv[1], nullptr, 0), strtoul(argv[2], nullptr, 0), strtol(argv[3], nullptr, 0),
                  strtol(argv[4], nullptr, 0), argc > 5 ? strtoul(argv[5], nullptr, 0) : 64);
    } else {
        Serial.println("error commands: info get set <name> <value> pal ntsc fill [seed] "
                       "run [batches] [seed] cal <out> <in> <from> <to> [reps]");
    }
}

void setup() {
    ex_params_default_pal();
    ex_model_fill(pattern_seed);
    ex_engine_init();
    ex_go_idle();  // selects inactive, D0 to D7 released
    Serial.begin(115200);
}

void loop() {
    static char line[128];
    static size_t len = 0;
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
}
