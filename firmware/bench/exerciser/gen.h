// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Bus cycle generator and checker of the exerciser. A batch is a run of C64
// cycles, each a VIC-II half (PHI2 low) and a 6510 half (PHI2 high). Every
// edge is placed from a timing model in nanoseconds; the cycle times scale
// with `speed`, the chip delays do not. Reads are checked against a model of
// the memory, which starts from the shared test pattern (c64bus_bench.h).

#ifndef EXERCISER_GEN_H
#define EXERCISER_GEN_H

#include <stdbool.h>
#include <stdint.h>

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

// Kinds of half cycle; the weights choose how often each one occurs.
enum {
    K_V_REFRESH,  // VIC-II half without a select (refresh, idle)
    K_V_RAM,      // VIC-II reads RAM (A12, A13 late on VA4, VA5)
    K_V_CHAR,     // VIC-II reads the character ROM
    K_C_RAM_R,    // 6510 reads RAM
    K_C_RAM_W,    // 6510 writes RAM
    K_C_BASIC,    // 6510 reads the BASIC ROM
    K_C_KERNAL,   // 6510 reads the KERNAL ROM
    K_C_CHAR,     // 6510 reads the character ROM
    K_C_COL_R,    // 6510 reads colour RAM (D0 to D3)
    K_C_COL_W,    // 6510 writes colour RAM
    K_C_IDLE,     // 6510 accesses I/O: no select, sometimes data on the bus
    K_C_RELEASE,  // 6510 reads RAM, then the exerciser checks the release (sense wires)
    K_C_READBACK, // 6510 reads back a recent RAM or colour RAM write
    K_B_CACC,     // bad line: VIC-II c-access in the 6510 half (AEC low)
    K_COUNT
};

// Checks, as reported.
enum {
    CK_RAM_R, CK_RAM_RB, CK_BASIC, CK_KERNAL, CK_CHAR, CK_COL_R, CK_COL_RB,
    CK_VIC_RAM, CK_VIC_CHAR, CK_CACC_RAM, CK_CACC_COL, CK_RELEASE, CK_COUNT
};
extern const char *const ex_check_names[CK_COUNT];

typedef struct {
    // Cycle times, scaled by 100 / speed (ns).
    int32_t half_ns;      // half cycle: 507 PAL, 489 NTSC
    int32_t t_cas;        // ~CAS falls, after the half starts
    int32_t t_addr;       // 6510 address and R/~W valid
    int32_t t_vaddr;      // VIC-II A0 to A11 valid (after ~RAS and the latch)
    int32_t t_dv;         // 6510 write data valid
    int32_t t_setup;      // reader's setup time before the half ends (sample mode 0)
    // Chip delays, not scaled (ns).
    int32_t t_cas_rise;   // ~CAS rises, relative to the end of the half
    int32_t d_casram;     // ~CAS to ~CASRAM, both edges (CPLD, #93)
    int32_t t_col;        // ~CAS to the column address on VA4 and VA5
    int32_t t_pla;        // address to chip select (CPLD)
    int32_t t_dhold;      // 6510 write data hold after the half ends
    int32_t t_pre_start;  // precharge of D to the inverse data, after the half starts
    int32_t t_pre_end;    // precharge released this long before the select falls
    int32_t t_rel_drive;  // release check: drive D this long after the select rises
    int32_t t_rel_sample; // release check: sample the sense wires this long after it rises
    int32_t sample_mode;  // 0: at the end of the half minus t_setup; 1: select fall + sample_off
    int32_t sample_off;   // ns, sample mode 1
    int32_t speed;        // percent of the real cycle rate
    int32_t cycles;       // cycles per batch
    int32_t bank;         // VIC-II bank 0 to 3, or 4 for a random bank per batch
    int32_t weight[K_COUNT];
} ex_params_t;

typedef struct {
    const char *name;
    int32_t *value;
} ex_param_ref_t;

extern ex_params_t ex_params;
extern const ex_param_ref_t ex_param_refs[];
extern const unsigned ex_param_count;

void ex_params_default_pal(void);
void ex_params_ntsc(void);

typedef struct {
    uint32_t n[CK_COUNT];
    uint32_t err[CK_COUNT];
    uint32_t batches, cycles, samples, errors;
    uint32_t moved;      // edges moved because they were closer than a list allows
    uint32_t max_move;   // largest such move, in clocks (at most 3)
    uint32_t overflow;   // batches cut short because a list was full
    uint32_t tx_stall, rx_stall, timeout;
} ex_stats_t;

typedef struct {
    uint32_t batch, cycle;
    uint8_t check;
    uint16_t addr;
    uint8_t exp_d, got_d, mask_d, exp_c, got_c, mask_c, exp_s, got_s, mask_s;
} ex_failure_t;

#define EX_MAX_FAILURES 8
extern ex_failure_t ex_failures[EX_MAX_FAILURES];
extern unsigned ex_failure_count;

// Resets the memory model to the test pattern `seed` (as `fill` on the DUT).
void ex_model_fill(uint32_t seed);
uint8_t ex_model_ram(uint16_t addr);

// Puts the bus in its idle state: selects high, AEC high, D0 to D7 released.
void ex_go_idle(void);

// Builds, runs and checks `batches` batches, adding to `stats`.
void ex_run_batches(uint32_t batches, uint32_t seed, ex_stats_t *stats);

// Calibration with the DUT disconnected and a jumper from `out_gpio` to
// `in_gpio`: drives a rising edge and samples `offset` clocks later. Returns
// how many of `reps` samples read high.
uint32_t ex_calibrate(uint32_t out_gpio, uint32_t in_gpio, int32_t offset, uint32_t reps, bool *ok);

#ifdef __cplusplus
}
#endif

#endif
