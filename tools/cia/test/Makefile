# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Compact 64 contributors
#
# Host tests of the 6526 model (firmware/libraries/Compact64Cia).
#   make          build and run all tests
#   make unit     only the unit tests (unit.c)
#   make lorenz   only the Lorenz test suite programs
#   make quiet    only the check of the model's fast paths (quiet.c)
#   make busrules only the bus rules of 6510 traffic (busrules.c)

CIA := ../../../firmware/libraries/Compact64Cia/src
EX := ../../../firmware/bench/exerciser
CHIPS := ../third_party/chips
SUITE := ../third_party/lorenz-2.15/bin
CFLAGS ?= -O2 -g -Wall -Wextra -Werror -std=c11
CPPFLAGS += -I$(CIA)

PROGRAMS := cia1ta cia1tb cia1tab cia1tb123 cia1pb6 cia1pb7 \
            cia2ta cia2tb cia2tb123 cia2pb6 cia2pb7 \
            cntdef cnto2 flipos icr01 imr loadth oneshot

all: unit lorenz quiet busrules

unit: build/unit
	./build/unit

build/unit: unit.c $(CIA)/cia6526.c $(CIA)/cia6526.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ unit.c $(CIA)/cia6526.c

lorenz: build/lorenz
	./build/lorenz $(addprefix $(SUITE)/,$(PROGRAMS))

build/lorenz: lorenz.c $(CIA)/cia6526.c $(CIA)/cia6526.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-parameter -o $@ lorenz.c $(CIA)/cia6526.c

quiet: build/quiet
	./build/quiet

build/quiet: quiet.c ref_cia.c $(CIA)/cia6526.c $(CIA)/cia6526.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ quiet.c ref_cia.c $(CIA)/cia6526.c

busrules: build/busrules
	./build/busrules

build/busrules: busrules.c $(EX)/cia_cpu.c $(EX)/cia_cpu.h $(CHIPS)/m6502.h $(CIA)/cia6526.c $(CIA)/cia6526.h | build
	$(CC) $(CPPFLAGS) -I$(EX) -I$(CHIPS) $(CFLAGS) -Wno-unused-parameter -o $@ busrules.c $(EX)/cia_cpu.c $(CIA)/cia6526.c

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all unit lorenz quiet busrules clean
