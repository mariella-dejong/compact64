// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// PIO and DMA engine of the bus exerciser. Five state machines play four
// lists, all started on the same clock:
//
//   PIO0 (GPIO base 0)   SM0 wave24   GPIO8 to GPIO31: A0-A15, selects, R/~W, VA4, VA5
//                        SM1 data8    GPIO0 to GPIO7: D0-D7, values and directions
//                        SM2 sample16 reads GPIO0 to GPIO15
//   PIO1 (GPIO base 16)  SM0 wave16   GPIO32 to GPIO47: GR/~W, ~VA14, ~VA15, AEC, PHI2, ~CAS,
//                                     and MARK when enabled
//                        SM1 sample16 reads GPIO32 to GPIO47: COLORD, sense wires, DUT trigger

#ifndef EXERCISER_ENGINE_H
#define EXERCISER_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Exerciser-only pins (the firmware under test leaves these free on the bench).
#define EX_GPIO_SENSE_D0 42  // D0 on the DUT side of its series resistor
#define EX_GPIO_SENSE_D7 43  // D7 on the DUT side of its series resistor
#define EX_GPIO_MARK     45  // high during each batch, for a logic analyser; off by default,
                             // because the DUT drives its heartbeat LED on GPIO45

// Outputs driven by wave16 (PIO1), as GPIO_HI bits.
#define EX_W1_OUTPUTS ((1u << (35 - 32)) | (1u << (34 - 32)) | (1u << (33 - 32)) | (1u << (32 - 32)) | \
                       (1u << (46 - 32)) | (1u << (47 - 32)))

// List capacities, in words.
#define EX_W0_WORDS 16384
#define EX_W1_WORDS 8192
#define EX_D_WORDS  4096
#define EX_S_WORDS  2048

extern uint32_t ex_w0[EX_W0_WORDS];
extern uint32_t ex_w1[EX_W1_WORDS];
extern uint32_t ex_d[EX_D_WORDS];
extern uint32_t ex_s[EX_S_WORDS];   // sampler hold times, shared by both samplers
extern uint32_t ex_rx0[EX_S_WORDS]; // GPIO0 to GPIO15 at each sample
extern uint32_t ex_rx1[EX_S_WORDS]; // GPIO32 to GPIO47 at each sample

typedef struct {
    uint32_t n_w0, n_w1, n_d, n_s;
    uint32_t total_ticks;  // run length, including the padding at the end
} ex_lists_t;

typedef struct {
    bool tx_stall;  // a list ran dry before its end: timing invalid
    bool rx_stall;  // a sampler could not push: timing invalid
    bool timeout;   // the run did not finish
} ex_run_result_t;

void ex_engine_init(void);
// CIA mode (#182): only A0 to A3, CNT, SP, the bench TOD pin (GPIO18),
// ~RESET, ~FLAG, PHI2, R/~W and ~CS are outputs (plus D0 to D7 as the lists
// say); every other pin is an input with its pull-up, as the CIA
// controller's lines are open drain; the second sampler reads GPIO16 to
// GPIO31 instead of GPIO32 to GPIO47. Off: the U1401 set-up of #159.
void ex_engine_set_cia(bool on);
bool ex_engine_cia(void);
// Drives MARK (GPIO45) or leaves it an input. Only enable it when GPIO45 is
// not connected to the DUT.
void ex_set_mark(bool on);
bool ex_mark_enabled(void);
uint32_t ex_sys_hz(void);
// Sets all exerciser outputs to an idle state: `pins` holds the values for
// GPIO0 to GPIO47, D0 to D7 are inputs.
void ex_set_idle(uint64_t pins);
void ex_run(const ex_lists_t *lists, uint64_t first_pins, ex_run_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
