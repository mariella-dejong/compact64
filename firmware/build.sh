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
# Set ARDUINO_CLI to use an arduino-cli that is not on PATH.

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
cli=${ARDUINO_CLI:-arduino-cli}
board=rp2040:rp2040:weact_rp2350b

case ${1:-} in
    u1401)
        # U1401 runs at the RP2350's rated 150 MHz (README, Timing).
        sketch=$here/u1401
        fqbn=$board:freq=150
        ;;
    exerciser)
        # 250 MHz gives the exerciser 4 ns steps.
        sketch=$here/bench/exerciser
        fqbn=$board:freq=250
        ;;
    *)
        echo "usage: $0 u1401|exerciser [port]" >&2
        exit 2
        ;;
esac

out=$here/build/$1
"$cli" compile --fqbn "$fqbn" --warnings all \
    --library "$here/libraries/Compact64Bus" \
    --output-dir "$out" "$sketch"

if [ -n "${2:-}" ]; then
    "$cli" upload --fqbn "$fqbn" --port "$2" --input-dir "$out" "$sketch"
fi
