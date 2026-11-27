// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Compact 64 contributors
//
// The controller-specific part of the CIA firmware: CIA 1 (U1501,
// board_cia1.c) or CIA 2 (U1601, board_cia2.c), chosen with CIA_UNIT at
// build time (build.sh cia1|cia2).

#ifndef BOARD_H
#define BOARD_H

#include "cia_bus.h"

#ifndef CIA_UNIT
#define CIA_UNIT 2
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern const cia_board_t board;
extern const char board_name[];
extern const unsigned board_heartbeat_gpio;

// Pins of the controller, all released. Before cia_bus_init().
void board_init(void);

#ifdef __cplusplus
}
#endif

#endif
