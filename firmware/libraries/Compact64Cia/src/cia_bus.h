// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// Bus interface of the CIA controllers (U1501, U1601) on the RP2354B: the
// 6510 reads and writes the 6526 model (cia6526.h) through two PIO state
// machines, and core 1 steps the model once per PHI2 cycle.
//
//   PIO0 SM0 cia_read   drives D0 to D7 for a read, from a register image
//                       the CPU writes into its RX FIFO registers
//   PIO0 SM1 cia_cycle  pushes one sample of the bus pins per cycle, taken
//                       just before PHI2 falls
//
// Per cycle, core 1 takes the sample, applies a write or the side effects of
// a read, steps the model, lets the controller set its pins (board->ports)
// and writes the register image for the next cycle. All of that has to be
// done before PHI2 rises again, about half a cycle later.

#ifndef CIA_BUS_H
#define CIA_BUS_H

#include <stdbool.h>
#include <stdint.h>

#include "c64cia_pins.h"
#include "cia6526.h"

#ifdef __cplusplus
extern "C" {
#endif

// PIO0 interrupt flag the CPU sets when the register image for the next
// cycle is written; the read driver waits for it before it drives.
#define CIA_PIO_IRQ_READY 0

// Bits of the cycle sample (GPIO0 to GPIO26).
#define CIA_S_D 0x000000FFu
#define CIA_S_CNT (1u << CIA_GPIO_CNT)
#define CIA_S_SP (1u << CIA_GPIO_SP)
#define CIA_S_RESET (1u << CIA_GPIO_RESET)
#define CIA_S_FLAG (1u << CIA_GPIO_FLAG)
#define CIA_S_TOD_BENCH (1u << CIA_GPIO_TOD_BENCH)
#define CIA_S_RW (1u << CIA_GPIO_RW)
#define CIA_S_CS (1u << CIA_GPIO_CS)
static inline unsigned cia_s_reg(uint32_t s) { return (s >> CIA_GPIO_A0) & 0xFu; }

typedef struct {
    // Called once per cycle, after the model has stepped. Sets the
    // controller's output pins from the model (ports, ~PC, extra lines) and
    // cia->pa_pins and cia->pb_pins to what reads of PRA and PRB return in
    // the next cycle. sample: the bus pins of the cycle that just ended.
    void (*ports)(cia_t *cia, uint32_t sample);
    // Called once when ~RESET is low, after the model has been reset:
    // release every output.
    void (*reset)(cia_t *cia);
} cia_board_t;

typedef struct {
    uint32_t cycles;
    uint32_t reads;
    uint32_t writes;
    uint32_t resets;
    uint32_t overruns;  // a cycle sample was lost: the loop was too slow
    uint32_t max_lag;   // most samples waiting at once
    uint32_t loop_max;  // longest pass of the loop, in clocks, from taking a sample
    uint64_t loop_sum;  // sum of all passes, for the average
} cia_bus_stats_t;

extern volatile cia_bus_stats_t cia_bus_stats;

// TOD pin edges (50 or 60 Hz) to apply: core 0 increments it, core 1
// passes each increment to the model.
extern volatile uint32_t cia_bus_tod_edges;

// Bench settings (#182). tod_pin: TOD edges are the rising edges of GPIO18,
// driven by the exerciser, instead of cia_bus_tod_edges, so that the
// exerciser's model sees them in the same cycle. irq_push_pull: drive ~IRQ
// high as well as low, since the bench has no 3k3 pull-up on it.
extern volatile bool cia_bus_tod_pin;
extern volatile bool cia_bus_irq_push_pull;

// Pins and PIO. Call from core 1 before cia_bus_run().
void cia_bus_init(void);

// Bypass the input synchronizers of the bus pins (saves two clocks, at the
// risk of a metastable read on an edge).
void cia_bus_set_sync_bypass(bool on);

// The bus loop, on core 1, with interrupts off. Never returns.
void cia_bus_run(cia_t *cia, const cia_board_t *board) __attribute__((noreturn));

// The four RX FIFO words of the read driver for the model's state.
void cia_bus_image(const cia_t *cia, uint32_t out[4]);

#ifdef __cplusplus
}
#endif

#endif
