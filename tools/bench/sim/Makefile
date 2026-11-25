# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Compact 64 contributors
#
# Host simulation of the bench test: make -C tools/bench/sim

ROOT := ../../..
EX := $(ROOT)/firmware/bench/exerciser
LIB := $(ROOT)/firmware/libraries/Compact64Bus/src
CFLAGS ?= -O2 -g -Wall -Wextra -std=c11
CPPFLAGS += -I$(EX) -I$(LIB)

all: run

sim: sim.c $(EX)/gen.c $(EX)/gen.h $(EX)/engine.h $(LIB)/c64bus_bench.h $(LIB)/c64bus_pins.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ sim.c $(EX)/gen.c -lm

run: sim
	./sim

clean:
	rm -f sim

.PHONY: all run clean
