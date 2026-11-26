#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Compact 64 contributors
#
# Builds, and optionally uploads, one of the firmware sketches with arduino-cli
# and the arduino-pico core.
#
#   firmware/build.sh u1401|exerciser            compile
#   firmware/build.sh u1401|exerciser <port>     compile and upload, for
#                                                example /dev/ttyACM0
#
# BOARD selects the target board:
#   rp2354b  (default) an RP2354B board, as U1401 itself: the generic RP2350
#            target with the 48-GPIO RP2350B and the 2 MB of stacked flash
#   weact    the WeAct Studio RP2350B core board
#
# FREQ sets U1401's clock in MHz (default 150, the RP2350's rating; the
# Arduino core offers overclocks up to 300).
#
# TRACE_PINS=1 builds U1401 with the trace pins (BUS_TRACE_PINS in
# u1401/bus.h): GPIO44 and GPIO45 show where the bus loop is.
#
# CLEAN=1 rebuilds from scratch (arduino-cli --clean): arduino-cli keeps a copy
# of the sketch in a build cache, where a deleted source file would still be
# compiled. The pipeline (azure-pipelines.yml) always sets it.
#
# Set ARDUINO_CLI to use an arduino-cli that is not on PATH. The sketches are
# built with arduino-pico 6.3.2 (README.md).

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
cli=${ARDUINO_CLI:-arduino-cli}

case ${BOARD:-rp2354b} in
    rp2354b)
        board=rp2040:rp2040:generic_rp2350:variantchip=RP2350B,flash=2097152_0
        ;;
    weact)
        board=rp2040:rp2040:weact_rp2350b:flash=16777216_0
        ;;
    *)
        echo "unknown BOARD '${BOARD}': use rp2354b or weact" >&2
        exit 2
        ;;
esac

case ${1:-} in
    u1401)
        # U1401 runs at the RP2350's rated 150 MHz (README, Timing).
        sketch=$here/u1401
        fqbn=$board,freq=${FREQ:-150}
        ;;
    exerciser)
        # 250 MHz gives the exerciser 4 ns steps.
        sketch=$here/bench/exerciser
        fqbn=$board,freq=250
        ;;
    *)
        echo "usage: [BOARD=rp2354b|weact] $0 u1401|exerciser [port]" >&2
        exit 2
        ;;
esac

out=$here/build/${BOARD:-rp2354b}/$1
if [ "$1" = u1401 ] && [ "${FREQ:-150}" != 150 ]; then
    out=$out-${FREQ}mhz
fi
extra=()
if [ "${CLEAN:-0}" = 1 ]; then
    extra+=(--clean)
fi
if [ "${TRACE_PINS:-0}" = 1 ]; then
    out=$out-trace
    extra+=(--build-property "compiler.c.extra_flags=-DBUS_TRACE_PINS=1"
            --build-property "compiler.cpp.extra_flags=-DBUS_TRACE_PINS=1")
fi
"$cli" compile --fqbn "$fqbn" --warnings all ${extra[@]+"${extra[@]}"} \
    --library "$here/libraries/Compact64Bus" \
    --library "$here/libraries/Compact64Cia" \
    --output-dir "$out" "$sketch"

if [ -n "${2:-}" ]; then
    # arduino-cli does not follow links such as /dev/serial/by-id/...
    port=$(readlink -f "$2")
    "$cli" upload --fqbn "$fqbn" --port "$port" --input-dir "$out" "$sketch"
fi
