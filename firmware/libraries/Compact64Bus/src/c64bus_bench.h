// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Test patterns for the bench test (#159). The firmware under test and the
// exerciser both fill their memory from these functions, so the exerciser
// knows every byte the firmware should return. The ROM patterns stand in for
// the Commodore ROM images, which are not part of this project.

#ifndef C64BUS_BENCH_H
#define C64BUS_BENCH_H

#include <stdint.h>
#include "c64bus_pins.h"

// Pattern regions, so that RAM, colour RAM and each ROM differ.
#define C64BENCH_REGION_RAM    0u
#define C64BENCH_REGION_COLOR  1u
#define C64BENCH_REGION_ROM    2u

// A 32-bit integer hash (lowbias32 by Chris Wellons, public domain).
static inline uint32_t c64bench_hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// Byte `addr` of `region` for pattern `seed`.
static inline uint8_t c64bench_byte(uint32_t seed, uint32_t region, uint32_t addr) {
    return (uint8_t)c64bench_hash32((seed * 0x9e3779b9u) ^ (region << 20) ^ addr);
}

// Fills RAM (64 KB), colour RAM (1 KB, one nibble per byte) and the ROM image
// array (20 KB, layout in c64bus_pins.h).
static inline void c64bench_fill(uint32_t seed, uint8_t *ram, uint8_t *color, uint8_t *rom) {
    for (uint32_t a = 0; a < C64_RAM_SIZE; a++) {
        ram[a] = c64bench_byte(seed, C64BENCH_REGION_RAM, a);
    }
    for (uint32_t a = 0; a < C64_COLOR_SIZE; a++) {
        color[a] = c64bench_byte(seed, C64BENCH_REGION_COLOR, a) & 0x0Fu;
    }
    for (uint32_t a = 0; a < C64_ROM_SIZE; a++) {
        rom[a] = c64bench_byte(seed, C64BENCH_REGION_ROM, a);
    }
}

#endif
