#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Compact 64 contributors
"""Drives the U1401 bench test (#159) over USB serial.

The exerciser plays the bus; the DUT runs the U1401 firmware. Both print
`key=value` lines (firmware/bench/README.md). Examples:

  bench.py run --ex /dev/ttyACM0 --dut /dev/ttyACM1 --batches 100
  bench.py sweep sample_off 40 200 4 --ex /dev/ttyACM0 --dut /dev/ttyACM1 \\
      --set sample_mode=1 --batches 20 --csv sweep.csv
  bench.py shmoo --ex /dev/ttyACM0 --dut /dev/ttyACM1 --speeds 100:200:10 \\
      --offsets 40:200:8 --batches 10 --csv shmoo.csv
  bench.py cal 45 0 -4 12 --ex /dev/ttyACM0

Requires pyserial.
"""

import argparse
import csv
import sys
import time

import serial


class Port:
    """A line-based command port."""

    def __init__(self, device):
        self.s = serial.Serial(device, 115200, timeout=60)
        time.sleep(0.2)
        self.s.reset_input_buffer()

    def command(self, line, until=("end",)):
        """Sends a command and returns its output lines, up to the first line
        that is `ok`, starts with `error`, or starts with one of `until`."""
        self.s.write((line + "\n").encode())
        out = []
        while True:
            raw = self.s.readline()
            if not raw:
                raise TimeoutError(f"no answer to {line!r}")
            text = raw.decode(errors="replace").strip()
            if not text:
                continue
            out.append(text)
            if text == "ok" or text.startswith("error") or text.split(" ", 1)[0] in until:
                if text.startswith("error"):
                    raise RuntimeError(f"{line!r}: {text}")
                return out


def fields(line):
    """Parses `word key=value key=value ...`."""
    word, _, rest = line.partition(" ")
    d = {"_": word}
    for item in rest.split():
        k, _, v = item.partition("=")
        d[k] = v
    return d


def run_step(ex, dut, batches):
    """Runs `batches` batches; returns the summary, per-check counts,
    failures and the DUT's late count for this step."""
    if dut:
        dut.command("clear")
    lines = ex.command(f"run {batches}")
    summary, checks, fails = {}, {}, []
    for line in lines:
        f = fields(line)
        if f["_"] == "result":
            summary = f
        elif f["_"] == "check":
            checks[f["name"]] = (int(f["n"]), int(f["err"]))
        elif f["_"] == "fail":
            fails.append(line)
    late = None
    if dut:
        stats = fields(dut.command("stats", until=("stats",))[-1])
        late = int(stats.get("late", 0))
    return summary, checks, fails, late


def valid(summary):
    return all(summary.get(k, "0") == "0" for k in ("tx_stall", "rx_stall", "timeout", "overflow"))


def apply_sets(ex, sets):
    for item in sets or []:
        name, _, value = item.partition("=")
        ex.command(f"set {name} {value}")


def prepare(args):
    ex = Port(args.ex)
    dut = Port(args.dut) if args.dut else None
    ex.command("ntsc" if args.ntsc else "pal")
    ex.command(f"fill {args.seed}")
    if dut:
        dut.command(f"fill {args.seed}")
    apply_sets(ex, args.set)
    return ex, dut


def span(text):
    a, b, step = (int(x) for x in text.split(":"))
    return list(range(a, b + 1, step))


def cmd_info(args):
    ex = Port(args.ex)
    print(ex.command("info", until=("info",))[-1])
    print("\n".join(ex.command("get")[:-1]))
    if args.dut:
        print(Port(args.dut).command("info", until=("info",))[-1])


def cmd_run(args):
    ex, dut = prepare(args)
    summary, checks, fails, late = run_step(ex, dut, args.batches)
    print(" ".join(f"{k}={v}" for k, v in summary.items() if k != "_"))
    for name, (n, err) in checks.items():
        print(f"  {name:15} n={n:<8} err={err}")
    for line in fails:
        print("  " + line)
    if late is not None:
        print(f"  dut late={late}")
    return 0 if valid(summary) and summary.get("errors") == "0" else 1


def cmd_sweep(args):
    ex, dut = prepare(args)
    rows = []
    for value in range(args.start, args.stop + 1, args.step):
        ex.command(f"set {args.param} {value}")
        summary, checks, _, late = run_step(ex, dut, args.batches)
        row = {args.param: value, "valid": int(valid(summary)), "dut_late": late}
        for name, (n, err) in checks.items():
            row[f"{name}_n"] = n
            row[f"{name}_err"] = err
        rows.append(row)
        bad = {k: v[1] for k, v in checks.items() if v[1]}
        print(f"{args.param}={value:<6} valid={row['valid']} errors={summary.get('errors')} {bad or ''}")
    write_csv(args.csv, rows)


def cmd_shmoo(args):
    """Error rate of the chosen checks over speed (rows) and sample offset
    after the select (columns)."""
    ex, dut = prepare(args)
    ex.command("set sample_mode 1")
    speeds, offsets = span(args.speeds), span(args.offsets)
    wanted = args.checks.split(",")
    rows, grid = [], {}
    for speed in speeds:
        ex.command(f"set speed {speed}")
        for off in offsets:
            ex.command(f"set sample_off {off}")
            summary, checks, _, late = run_step(ex, dut, args.batches)
            n = sum(checks.get(c, (0, 0))[0] for c in wanted)
            err = sum(checks.get(c, (0, 0))[1] for c in wanted)
            rate = err / n if n and valid(summary) else None
            grid[speed, off] = rate
            rows.append({"speed": speed, "sample_off": off, "n": n, "err": err,
                         "valid": int(valid(summary)), "dut_late": late})
    print(f"error rate of {args.checks}: rows speed %, columns ns after the select")
    print("speed " + "".join(f"{o:>5}" for o in offsets))
    for speed in speeds:
        cells = []
        for off in offsets:
            r = grid[speed, off]
            cells.append("    ?" if r is None else "    ." if r == 0 else "    #" if r == 1 else f"{r:5.2f}")
        print(f"{speed:>5} " + "".join(cells))
    print("legend: . no errors, # all wrong, ? invalid run (stall, timeout or overflow)")
    write_csv(args.csv, rows)


def cmd_cal(args):
    ex = Port(args.ex)
    for line in ex.command(f"cal {args.out} {args.inp} {args.start} {args.stop} {args.reps}")[:-1]:
        print(line)


def write_csv(path, rows):
    if not path or not rows:
        return
    keys = []
    for row in rows:
        keys += [k for k in row if k not in keys]
    with open(path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=keys)
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {path}")


def main():
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--ex", required=True, help="serial port of the exerciser")
    common.add_argument("--dut", help="serial port of the board under test")
    common.add_argument("--seed", type=int, default=1, help="test pattern seed (fill)")
    common.add_argument("--ntsc", action="store_true", help="NTSC timing instead of PAL")
    common.add_argument("--set", action="append", metavar="NAME=VALUE", help="exerciser parameter")
    common.add_argument("--batches", type=int, default=20)
    common.add_argument("--csv", help="write the results to this file")
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("info", parents=[common])
    sub.add_parser("run", parents=[common])
    sp = sub.add_parser("sweep", parents=[common])
    sp.add_argument("param")
    sp.add_argument("start", type=int)
    sp.add_argument("stop", type=int)
    sp.add_argument("step", type=int)
    sh = sub.add_parser("shmoo", parents=[common])
    sh.add_argument("--speeds", default="100:200:10", help="from:to:step, percent")
    sh.add_argument("--offsets", default="40:200:8", help="from:to:step, ns after the select")
    sh.add_argument("--checks", default="ram_read,ram_readback,vic_ram,cacc_ram")
    cp = sub.add_parser("cal", parents=[common])
    cp.add_argument("out", type=int)
    cp.add_argument("inp", type=int)
    cp.add_argument("start", type=int)
    cp.add_argument("stop", type=int)
    cp.add_argument("--reps", type=int, default=64)
    args = ap.parse_args()
    handler = {"info": cmd_info, "run": cmd_run, "sweep": cmd_sweep, "shmoo": cmd_shmoo, "cal": cmd_cal}
    return handler[args.cmd](args) or 0


if __name__ == "__main__":
    sys.exit(main())
