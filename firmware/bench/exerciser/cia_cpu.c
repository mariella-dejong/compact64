// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// 6510 traffic for the CIA bench test, see cia_cpu.h.

#include "cia_cpu.h"

#include <string.h>

#define CHIPS_IMPL
#include "m6502.h"

// Code runs from these; data operands go to DATA_LO to DATA_HI, or to the
// CIA. The vectors at $FFFA to $FFFF point into the code area.
#define CODE1_LO 0x0200u
#define CODE1_HI 0xC000u
#define CODE2_LO 0xE000u
#define CODE2_HI 0xFFFAu
#define DATA_LO 0xC000u
#define DATA_HI 0xD000u
#define IO_LO 0xD000u
#define IO_HI 0xE000u

// VIC-II-like RDY stalls (PAL): a badline every 8 raster lines of 63
// cycles; BA is low for 43 cycles, AEC for the last 40. Sprite DMA: BA low
// 3 cycles ahead of 2 cycles with AEC low.
#define BADLINE_PERIOD (8u * 63u)
#define BADLINE_BA 43u
#define STALL_AEC_DELAY 3u
#define SPRITE_BA 5u
#define SPRITE_ONE_IN 300u

struct cia_cpu {
    m6502_t cpu;
    uint64_t pins;
    cia_cpu_cfg_t cfg;
    uint32_t rng;
    uint16_t base;     // the CIA's page
    uint32_t cycle;
    uint32_t ba_start, ba_len;  // the RDY stall in progress, or the next badline
    uint32_t next_badline;
    uint32_t last_write;
    bool have_write;
    bool last_cia_read;  // the previous cycle was a CIA read
    cia_cpu_stats_t st;
    uint8_t mem[65536];
};

_Static_assert(sizeof(struct cia_cpu) <= CIA_CPU_BYTES, "CIA_CPU_BYTES");

uint32_t cia_cpu_size(void) {
    return sizeof(cia_cpu_t);
}

static uint32_t rnd(cia_cpu_t *c) {
    uint32_t x = c->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    c->rng = x;
    return x;
}

// Addressing modes of all 256 opcodes, illegal ones included.
enum { IMP, IMM, ZP, ZPX, ZPY, IZX, IZY, ABS, ABX, ABY, IND, REL, JAM };

static unsigned mode_of(uint8_t op) {
    const bool odd = (op >> 4) & 1u;
    switch (op & 0xFu) {
    case 0x0:
        if (odd) {
            return REL;
        }
        return op == 0x20 ? ABS : (op == 0x00 || op == 0x40 || op == 0x60) ? IMP : IMM;
    case 0x1:
    case 0x3:
        return odd ? IZY : IZX;
    case 0x2:
        return (odd || op == 0x02 || op == 0x22 || op == 0x42 || op == 0x62) ? JAM : IMM;
    case 0x4:
    case 0x5:
        return odd ? ZPX : ZP;
    case 0x6:
    case 0x7:
        if (!odd) {
            return ZP;
        }
        return (op == 0x96 || op == 0xB6 || op == 0x97 || op == 0xB7) ? ZPY : ZPX;
    case 0x8:
    case 0xA:
        return IMP;
    case 0x9:
    case 0xB:
        return odd ? ABY : IMM;
    case 0xC:
        return odd ? ABX : (op == 0x6C ? IND : ABS);
    case 0xD:
        return odd ? ABX : ABS;
    default:  // 0xE, 0xF
        if (!odd) {
            return ABS;
        }
        return (op == 0x9E || op == 0xBE || op == 0x9F || op == 0xBF) ? ABY : ABX;
    }
}

static const uint8_t mode_len[] = {
    [IMP] = 1, [IMM] = 2, [ZP] = 2, [ZPX] = 2, [ZPY] = 2, [IZX] = 2, [IZY] = 2,
    [ABS] = 3, [ABX] = 3, [ABY] = 3, [IND] = 3, [REL] = 2, [JAM] = 1,
};

static bool in_io(uint16_t a) {
    return a >= IO_LO && a < IO_HI;
}

static uint16_t code_addr(cia_cpu_t *c) {
    const uint32_t r = rnd(c);
    if (r & 1u) {
        return (uint16_t)(CODE1_LO + (r >> 8) % (CODE1_HI - CODE1_LO));
    }
    return (uint16_t)(CODE2_LO + (r >> 8) % (CODE2_HI - CODE2_LO));
}

// A CIA address: mostly the 16 registers; also mirrors, the end of the page
// (indexed accesses cross into the next one, after a dummy read in this
// one) and the end of the page before (indexed accesses land in this one).
static uint16_t cia_addr(cia_cpu_t *c) {
    const uint32_t r = rnd(c);
    const uint16_t lo = (uint16_t)((r >> 8) & 0xFu);
    switch (r & 7u) {
    case 4:
        return (uint16_t)(c->base | ((r >> 8) & 0xFFu));
    case 5:
        return (uint16_t)(c->base | 0xF0u | lo);
    case 6:
        return (uint16_t)((c->base - 0x100u) | 0xF0u | lo);
    default:
        return (uint16_t)(c->base | lo);
    }
}

static uint16_t operand_addr(cia_cpu_t *c) {
    if (rnd(c) % 100u < c->cfg.cia_pct) {
        return cia_addr(c);
    }
    return (uint16_t)(DATA_LO + rnd(c) % (DATA_HI - DATA_LO));
}

static void put16(cia_cpu_t *c, uint32_t a, uint16_t v) {
    c->mem[a & 0xFFFFu] = (uint8_t)v;
    c->mem[(a + 1u) & 0xFFFFu] = (uint8_t)(v >> 8);
}

// A random instruction at a, at most `room` bytes; returns its length.
static unsigned gen_instruction(cia_cpu_t *c, uint32_t a, uint32_t room) {
    uint8_t op;
    unsigned mode;
    do {
        op = (uint8_t)rnd(c);
        mode = mode_of(op);
    } while (mode == JAM || mode_len[mode] > room);
    c->mem[a] = op;
    const uint32_t r = rnd(c);
    switch (mode) {
    case IMM:
        // Small values now and then, so that LDX/LDY # keep indexed
        // accesses near their base.
        c->mem[a + 1] = (r & 1u) ? (uint8_t)((r >> 8) & 0x1Fu) : (uint8_t)(r >> 8);
        break;
    case ZP:
    case ZPX:
    case ZPY:
        c->mem[a + 1] = (uint8_t)(r >> 8);
        break;
    case IZX:
    case IZY:
        c->mem[a + 1] = (uint8_t)((r >> 8) & 0xFEu);  // the pointers in zero page
        break;
    case REL:
        c->mem[a + 1] = (r & 1u) ? (uint8_t)((r >> 8) & 0x0Fu) : (uint8_t)(r >> 8);
        break;
    case ABS:
        put16(c, a + 1, (op == 0x20 || op == 0x4C) ? code_addr(c) : operand_addr(c));
        break;
    case ABX:
    case ABY:
        put16(c, a + 1, operand_addr(c));
        break;
    case IND:
        // JMP (ind): a pointer in the data area, which holds code addresses
        // until the program overwrites them.
        put16(c, a + 1, (uint16_t)(DATA_LO + rnd(c) % (DATA_HI - DATA_LO - 1u)));
        break;
    default:
        break;
    }
    return mode_len[mode];
}

static void gen_code(cia_cpu_t *c, uint32_t lo, uint32_t hi) {
    for (uint32_t a = lo; a < hi;) {
        a += gen_instruction(c, a, hi - a);
    }
}

void cia_cpu_init(cia_cpu_t *c, const cia_cpu_cfg_t *cfg, uint32_t seed) {
    memset(c, 0, sizeof(*c));
    c->cfg = *cfg;
    c->rng = seed * 2654435761u | 1u;
    c->base = cfg->unit == 2 ? 0xDD00u : 0xDC00u;

    // Zero page: pointers for (zp,X) and (zp),Y. Stack, data area and
    // everything else in $D000 to $DFFF: bytes in $02 to $BF, so that a
    // pointer or return address made of any two of them is in the code
    // area.
    for (unsigned a = 0; a < 0x100; a += 2) {
        put16(c, a, operand_addr(c));
    }
    for (uint32_t a = 0x100; a < 0x200; a++) {
        c->mem[a] = (uint8_t)(0x02u + rnd(c) % 0xBEu);
    }
    for (uint32_t a = DATA_LO; a < IO_HI; a++) {
        c->mem[a] = (uint8_t)(0x02u + rnd(c) % 0xBEu);
    }
    gen_code(c, CODE1_LO, CODE1_HI);
    gen_code(c, CODE2_LO, CODE2_HI);
    for (uint32_t v = 0xFFFAu; v < 0x10000u; v += 2) {
        put16(c, v, code_addr(c));
    }

    m6502_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    c->pins = m6502_init(&c->cpu, &desc);
    c->st.write_gap_min = UINT32_MAX;
    c->next_badline = 20u + rnd(c) % BADLINE_PERIOD;
    c->ba_start = UINT32_MAX;
}

// BA low (RDY) and AEC in this cycle.
static void stall(cia_cpu_t *c, bool *ba, bool *aec) {
    const uint32_t n = c->cycle;
    if (c->ba_start != UINT32_MAX && n >= c->ba_start + c->ba_len) {
        c->ba_start = UINT32_MAX;
    }
    if (c->ba_start == UINT32_MAX) {
        if (c->cfg.badlines && n >= c->next_badline) {
            c->ba_start = n;
            c->ba_len = BADLINE_BA;
            c->next_badline += BADLINE_PERIOD;
        } else if (c->cfg.sprites && rnd(c) % SPRITE_ONE_IN == 0) {
            c->ba_start = n;
            c->ba_len = SPRITE_BA;
        }
    }
    *ba = c->ba_start != UINT32_MAX;
    *aec = !*ba || n < c->ba_start + STALL_AEC_DELAY;
}

cia_cpu_bus_t cia_cpu_cycle(cia_cpu_t *c) {
    bool ba, aec;
    stall(c, &ba, &aec);
    uint64_t in = c->pins;
    in = ba ? (in | M6502_RDY) : (in & ~M6502_RDY);
    const bool frozen = ba && (in & M6502_RW);  // the core repeats this read
    uint64_t pins = m6502_tick(&c->cpu, in);
    uint16_t addr = M6502_GET_ADDR(pins);

    if ((pins & M6502_SYNC) && !frozen) {
        // No code from $D000 to $DFFF, and no JAM: fetch from the code area
        // instead (cia_cpu.h).
        if (in_io(addr) || mode_of(c->mem[addr]) == JAM) {
            if (in_io(addr)) {
                c->st.redirects++;
            } else {
                c->st.jams++;
            }
            do {
                addr = code_addr(c);
            } while (mode_of(c->mem[addr]) == JAM);
            pins = (pins & ~0xFFFFULL) | addr;
            m6502_set_pc(&c->cpu, addr);
        }
        const uint8_t op = c->mem[addr];
        c->st.opcodes[op >> 5] |= 1u << (op & 31u);
    }

    cia_cpu_bus_t b;
    b.addr = addr;
    b.rw = (pins & M6502_RW) != 0;
    b.aec = aec;
    b.cia = aec && (addr & 0xFF00u) == c->base;
    b.reg = (uint8_t)(addr & 0xFu);
    b.sync = (pins & M6502_SYNC) != 0;
    b.rdy_wait = frozen;
    b.data = M6502_GET_DATA(pins);

    if (b.rw) {
        M6502_SET_DATA(pins, b.cia ? 0xFF : c->mem[addr]);  // a CIA read: cia_cpu_read_data()
        if (aec && (addr == 0xFFFAu || addr == 0xFFFEu)) {
            c->st.interrupts++;
        }
    } else {
        if (!aec) {
            c->st.aec_writes++;
        }
        if (!b.cia) {
            c->mem[addr] = b.data;
        }
    }

    if (b.cia) {
        if (b.rw) {
            c->st.cia_reads++;
            if (frozen) {
                c->st.rdy_repeats++;
            }
            if (c->last_cia_read) {
                c->st.back_to_back++;
            }
            if (c->have_write && c->cycle - c->last_write < c->st.write_gap_min) {
                c->st.write_gap_min = c->cycle - c->last_write;
            }
        } else {
            c->st.cia_writes++;
            c->last_write = c->cycle;
            c->have_write = true;
        }
    }
    c->last_cia_read = b.cia && b.rw;
    c->pins = pins;
    c->cycle++;
    c->st.cycles++;
    return b;
}

void cia_cpu_read_data(cia_cpu_t *c, uint8_t value) {
    M6502_SET_DATA(c->pins, value);
}

void cia_cpu_set_irq(cia_cpu_t *c, bool active) {
    const uint64_t line = c->cfg.unit == 2 ? M6502_NMI : M6502_IRQ;
    c->pins = active ? (c->pins | line) : (c->pins & ~line);
}

const cia_cpu_stats_t *cia_cpu_stats(const cia_cpu_t *c) {
    return &c->st;
}
