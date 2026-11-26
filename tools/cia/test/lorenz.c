// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Runs the CIA programs of Wolfgang Lorenz's C64 test suite (public domain,
// third_party/lorenz-2.15) against the 6526 model, on floooh's cycle-stepped
// 6502 core (zlib licence, third_party/chips/m6502.h).
//
//   lorenz [-v] <program> [...]
//
// A minimal C64: 64 KB of RAM, CIA 1 at $DC00 (IRQ), CIA 2 at $DD00 (NMI),
// and the VIC-II raster line in $D011/$D012. No ROM image: the KERNAL
// routines the tests call are traps or a few bytes of our own code. A test
// passes when it jumps to LOAD ($E16F) for the next program; it fails when it
// waits for a key ($FFE4) after printing an error.

#define CHIPS_IMPL
#include "../third_party/chips/m6502.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cia6526.h"

#define PAL_CYCLES_PER_LINE 63
#define PAL_LINES 312
#define TOD_CYCLES (985248 / 50)
#define MAX_CYCLES 400000000ull

static uint8_t mem[1 << 16];
static cia_t cia1, cia2;
static char out[1 << 20];
static size_t out_len;

static void put(uint16_t addr, const uint8_t *code, size_t n) {
    memcpy(&mem[addr], code, n);
}

// Our own stand-ins for the KERNAL entry points the tests use.
static void install_kernal(void) {
    // Hardware vectors.
    mem[0xFFFA] = 0x43; mem[0xFFFB] = 0xFE;  // NMI   -> $FE43
    mem[0xFFFC] = 0x00; mem[0xFFFD] = 0x08;  // RESET (unused)
    mem[0xFFFE] = 0x48; mem[0xFFFF] = 0xFF;  // IRQ   -> $FF48
    // IRQ/BRK entry: save A, X, Y; BRK flag decides between ($0316), ($0314).
    static const uint8_t irq[] = {0x48, 0x8A, 0x48, 0x98, 0x48, 0xBA, 0xBD, 0x04, 0x01, 0x29,
                                  0x10, 0xF0, 0x03, 0x6C, 0x16, 0x03, 0x6C, 0x14, 0x03};
    put(0xFF48, irq, sizeof(irq));
    // NMI entry: SEI, JMP ($0318).
    static const uint8_t nmi[] = {0x78, 0x6C, 0x18, 0x03};
    put(0xFE43, nmi, sizeof(nmi));
    // Default NMI handler: save registers, acknowledge CIA 2, restore, RTI.
    static const uint8_t nmi_default[] = {0x48, 0x8A, 0x48, 0x98, 0x48, 0xAD, 0x0D, 0xDD,
                                          0x68, 0xA8, 0x68, 0xAA, 0x68, 0x40};
    put(0xFE47, nmi_default, sizeof(nmi_default));
    // End of the default IRQ handler: acknowledge CIA 1, restore, RTI.
    static const uint8_t irq_default[] = {0xAD, 0x0D, 0xDC, 0x68, 0xA8, 0x68, 0xAA, 0x68, 0x40};
    put(0xEA31, irq_default, sizeof(irq_default));
    // IOINIT ($FDA3): the CIA set-up of the KERNAL, including the 60 Hz
    // jiffy timer (PAL latch $4025) with its interrupt.
    static const uint8_t ioinit[] = {
        0xA9, 0x7F, 0x8D, 0x0D, 0xDC, 0x8D, 0x0D, 0xDD, 0x8D, 0x00, 0xDC,  // ICRs off, PRA
        0xA9, 0x08, 0x8D, 0x0E, 0xDC, 0x8D, 0x0E, 0xDD, 0x8D, 0x0F, 0xDC,  // CRs: one shot
        0x8D, 0x0F, 0xDD,
        0xA2, 0x00, 0x8E, 0x03, 0xDC, 0x8E, 0x03, 0xDD, 0xCA, 0x8E, 0x02, 0xDC,  // DDRs
        0xA9, 0x07, 0x8D, 0x00, 0xDD, 0xA9, 0x3F, 0x8D, 0x02, 0xDD,            // CIA 2 port A
        0xA9, 0x25, 0x8D, 0x04, 0xDC, 0xA9, 0x40, 0x8D, 0x05, 0xDC,            // TA latch
        0xA9, 0x81, 0x8D, 0x0D, 0xDC, 0xA9, 0x11, 0x8D, 0x0E, 0xDC,            // IRQ on, start
        0x60};
    put(0xFDA3, ioinit, sizeof(ioinit));
    // Trapped entry points: an RTS, with the action done at the fetch.
    mem[0xFFD2] = 0x60;  // CHROUT
    mem[0xFD15] = 0x60;  // RESTOR
    mem[0xFFE4] = 0x60;  // GETIN: only reached after an error
    mem[0xE16F] = 0x60;  // LOAD: the next test, so this one passed
    mem[0xFE66] = 0x60;  // BRK without a handler
    mem[0x8000] = 0x60;  // exits
    mem[0xA474] = 0x60;
}

static void restor(void) {
    mem[0x0314] = 0x31; mem[0x0315] = 0xEA;  // IRQ
    mem[0x0316] = 0x66; mem[0x0317] = 0xFE;  // BRK
    mem[0x0318] = 0x47; mem[0x0319] = 0xFE;  // NMI
}

static char petscii(uint8_t c) {
    if (c == 13) {
        return '\n';
    }
    if (c >= 0x41 && c <= 0x5A) {
        return (char)(c + 0x20);  // unshifted letters print as lower case
    }
    if (c >= 0xC1 && c <= 0xDA) {
        return (char)(c - 0x80);
    }
    if (c >= 0x20 && c < 0x7F) {
        return (char)c;
    }
    return '?';
}

static uint64_t cycle;
static bool trace;
static bool keep_going;  // answer the test's "press a key" and continue
static unsigned errors;

static uint8_t bus_read(uint16_t addr) {
    switch (addr >> 8) {
    case 0xDC:
        if (trace) {
            fprintf(stderr, "%llu r %04x %02x\n", (unsigned long long)cycle, addr, cia_peek(&cia1, addr));
        }
        return cia_read(&cia1, addr);
    case 0xDD:
        return cia_read(&cia2, addr);
    case 0xD0: {
        const unsigned line = (unsigned)((cycle / PAL_CYCLES_PER_LINE) % PAL_LINES);
        if ((addr & 0x3F) == 0x11) {
            return (uint8_t)(0x1B | ((line >> 1) & 0x80));
        }
        if ((addr & 0x3F) == 0x12) {
            return (uint8_t)line;
        }
        return 0xFF;
    }
    default:
        return mem[addr];
    }
}

static void bus_write(uint16_t addr, uint8_t v) {
    switch (addr >> 8) {
    case 0xDC:
        if (trace) {
            fprintf(stderr, "%llu w %04x %02x\n", (unsigned long long)cycle, addr, v);
        }
        cia_write(&cia1, addr, v);
        break;
    case 0xDD:
        cia_write(&cia2, addr, v);
        break;
    case 0xD0:
        break;
    default:
        mem[addr] = v;
        break;
    }
}

typedef enum { RUNNING, PASSED, FAILED, TIMEOUT } result_t;

static result_t run(const char *path, cia_model_t model) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return FAILED;
    }
    memset(mem, 0, sizeof(mem));
    uint8_t hdr[2];
    if (fread(hdr, 1, 2, f) != 2) {
        fclose(f);
        return FAILED;
    }
    const uint16_t load = (uint16_t)(hdr[0] | (hdr[1] << 8));
    const size_t n = fread(&mem[load], 1, sizeof(mem) - load, f);
    fclose(f);
    (void)n;

    install_kernal();
    restor();
    cia_init(&cia1, model);
    cia_init(&cia2, model);
    out_len = 0;
    cycle = 0;
    errors = 0;

    m6502_t cpu;
    m6502_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    uint64_t pins = m6502_init(&cpu, &desc);
    // Start where the program's BASIC line "POKE 780,0:SYS <addr>" jumps.
    uint16_t start = 0;
    for (unsigned a = load; a < load + 32u; a++) {
        if (mem[a] == 0x9E) {  // SYS token
            for (a++; mem[a] >= '0' && mem[a] <= '9'; a++) {
                start = (uint16_t)(start * 10 + (mem[a] - '0'));
            }
            break;
        }
    }
    if (!start) {
        fprintf(stderr, "%s: no SYS line\n", path);
        return FAILED;
    }
    pins = M6502_SYNC;
    M6502_SET_ADDR(pins, start);
    M6502_SET_DATA(pins, mem[start]);
    m6502_set_pc(&cpu, start);
    m6502_set_s(&cpu, 0xF0);
    m6502_set_p(&cpu, 0x24);

    result_t result = RUNNING;
    while (result == RUNNING) {
        if (cycle >= MAX_CYCLES) {
            result = TIMEOUT;
            break;
        }
        pins = m6502_tick(&cpu, pins);
        const uint16_t addr = M6502_GET_ADDR(pins);
        if (pins & M6502_SYNC) {
            switch (addr) {
            case 0xFFD2:
                if (out_len < sizeof(out) - 1) {
                    out[out_len++] = petscii(cpu.A);
                }
                break;
            case 0xFD15:
                restor();
                break;
            case 0xE16F:
                result = PASSED;
                break;
            case 0xFFE4:
                if (keep_going && errors < 2000) {
                    errors++;
                    m6502_set_a(&cpu, 0x0D);
                    m6502_set_p(&cpu, (uint8_t)(cpu.P & ~0x02));  // Z clear: a key
                    break;
                }
                result = FAILED;
                break;
            case 0xFE66:
            case 0x8000:
            case 0xA474:
                result = FAILED;
                break;
            default:
                break;
            }
        }

        // Port pins: no keyboard, nothing on the user port.
        cia1.pa_pins = cia_pa_out(&cia1);
        cia1.pb_pins = cia_pb_out(&cia1);
        cia2.pa_pins = cia_pa_out(&cia2);
        cia2.pb_pins = cia_pb_out(&cia2);

        if (pins & M6502_RW) {
            M6502_SET_DATA(pins, bus_read(addr));
        } else {
            bus_write(addr, M6502_GET_DATA(pins));
        }
        if (cycle % TOD_CYCLES == 0) {
            cia_tod_tick(&cia1);
            cia_tod_tick(&cia2);
        }
        cia_clock(&cia1);
        cia_clock(&cia2);
        cycle++;

        pins &= ~(M6502_IRQ | M6502_NMI);
        if (cia_irq(&cia1)) {
            pins |= M6502_IRQ;
        }
        if (cia_irq(&cia2)) {
            pins |= M6502_NMI;
        }
    }
    out[out_len] = 0;
    return result;
}

int main(int argc, char **argv) {
    bool verbose = false;
    cia_model_t model = CIA_MODEL_6526;
    int failed = 0, total = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) {
            verbose = true;
            continue;
        }
        if (!strcmp(argv[i], "-k")) {
            keep_going = true;
            continue;
        }
        if (!strcmp(argv[i], "-t")) {
            trace = true;  // CIA 1 accesses with their cycle, to stderr
            continue;
        }
        if (!strcmp(argv[i], "-8521")) {
            model = CIA_MODEL_8521;
            continue;
        }
        const result_t r = run(argv[i], model);
        const char *name = strrchr(argv[i], '/') ? strrchr(argv[i], '/') + 1 : argv[i];
        const char *s = r == PASSED ? "PASS" : r == TIMEOUT ? "TIMEOUT" : "FAIL";
        printf("%-10s %s (%llu cycles)\n", name, s, (unsigned long long)cycle);
        if (r != PASSED || verbose) {
            printf("%s\n", out);
        }
        total++;
        failed += r != PASSED;
    }
    printf("%d of %d passed\n", total - failed, total);
    return failed ? 1 : 0;
}
