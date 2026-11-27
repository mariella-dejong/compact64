// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// The 6526 model without its fast paths (CIA_FAST_PATHS 0), under other
// names, as the reference for quiet.c.

#define CIA_FAST_PATHS 0
#define cia_init ref_cia_init
#define cia_reset ref_cia_reset
#define cia_peek ref_cia_peek
#define cia_read_effects ref_cia_read_effects
#define cia_write ref_cia_write
#define cia_tod_tick ref_cia_tod_tick
#define cia_clock_full ref_cia_clock_full
#define cia_clock_quiet ref_cia_clock_quiet
#define cia_clock ref_cia_clock
#define cia_read ref_cia_read
#define cia_irq ref_cia_irq
#define cia_pa_out ref_cia_pa_out
#define cia_pb_out ref_cia_pb_out
#define cia_pb_dir ref_cia_pb_dir

#include "cia6526.c"
