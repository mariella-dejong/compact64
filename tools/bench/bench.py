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
  bench.py cal 47 0 -4 12 --ex /dev/ttyACM0
  bench.py trace --ex /dev/ttyACM0 --dut /dev/ttyACM1 --cycles 4
  bench.py timings --ex /dev/ttyACM0 --dut /dev/ttyACM1 --batches 2 --csv timings.csv

Requires pyserial.
"""

import argparse
import bisect
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


# Trace columns: name, GPIO, active low. Exerciser outputs come from `out`,
# the DUT's pins from the samples (rx0: GPIO0 to 15, rx1: GPIO32 to 47).
TRACE_OUT = [("PHI2", 46, False), ("AEC", 35, False), ("~CAS", 47, True), ("~CASRAM", 24, True),
             ("~BASIC", 25, True), ("~KERNAL", 26, True), ("~CHAROM", 27, True),
             ("~COLORAM", 28, True), ("R/~W", 29, False), ("GR/~W", 32, False)]
SELECT_OF = {"ram_read": 24, "ram_readback": 24, "release": 24, "vic_ram": 24, "cacc_ram": 24,
             "basic": 25, "kernal": 26, "char": 27, "vic_char": 27,
             "color_read": 28, "color_readback": 28}
D_MASK = {"color_read": 0x0F, "color_readback": 0x0F}


def parse_trace(lines):
    rows = []
    for line in lines:
        f = fields(line)
        if f["_"] != "trace":
            continue
        rows.append({"tick": int(f["tick"]), "cycle": int(f["cycle"]), "out": int(f["out"], 16),
                     "d": int(f["d"], 16), "rx0": int(f["rx0"], 16), "rx1": int(f["rx1"], 16),
                     "kind": f["kind"], "addr": int(f["addr"], 16), "exp_d": int(f["exp_d"], 16),
                     "exp_c": int(f["exp_c"], 16)})
    return rows


def answer_times(rows, skew, hold=2):
    """For each checked read: when its select fell and rose (as the pins saw
    them, `skew` clocks after the exerciser set them), when D first held the
    expected value for `hold` entries in a row while the select was low or
    just after, and when the DUT's GPIO44 and GPIO45 were first high."""
    out = []
    for i, r in enumerate(rows):
        gpio = SELECT_OF.get(r["kind"])
        if gpio is None:
            continue
        kf = None
        for k in range(i, 0, -1):
            if not (rows[k]["out"] >> gpio) & 1 and (rows[k - 1]["out"] >> gpio) & 1:
                kf = k
                break
        if kf is None:
            continue
        kr = next((k for k in range(kf, len(rows)) if (rows[k]["out"] >> gpio) & 1), len(rows) - 1)
        fall, rise = rows[kf]["tick"] + skew, rows[kr]["tick"] + skew
        mask = D_MASK.get(r["kind"], 0xFF)
        window = [k for k in range(kf, len(rows)) if rows[k]["tick"] >= fall and rows[k]["tick"] <= rise + 2 * skew]
        answer = None
        for n, k in enumerate(window):
            if all(not ((rows[m]["rx0"] ^ r["exp_d"]) & mask) for m in window[n:n + hold]):
                answer = rows[k]["tick"]
                break
        first = lambda bit: next((rows[k]["tick"] for k in window if (rows[k]["rx1"] >> bit) & 1), None)
        out.append({"cycle": r["cycle"], "kind": r["kind"], "addr": r["addr"], "fall": fall, "rise": rise,
                    "sample": r["tick"], "answer": answer, "g44": first(12), "g45": first(13),
                    "ok": not ((r["rx0"] ^ r["exp_d"]) & mask)})
    return out


def cmd_trace(args):
    ex, dut = prepare(args)
    hz = int(fields(ex.command("info", until=("info",))[-1])["sys_hz"])
    ns = 1e9 / hz
    if dut:
        print(dut.command("info", until=("info",))[-1])
    # The same batch with the sample phase shifted by one clock per pass;
    # merged, the passes give one sample per clock.
    passes = args.passes if args.passes else args.step
    rows, bad = [], 0
    for phase in range(passes):
        lines = ex.command(f"trace {args.cycles} {args.step} {phase % args.step} {args.seed_run}")
        for line in lines:
            f = fields(line)
            if f["_"] == "result" and not valid(f):
                bad += 1
                print(line)
            if f["_"] == "fail" and phase == 0:
                print(line)
        rows += parse_trace(lines)
    if bad:
        print(f"{bad} of {passes} passes stalled; their samples are not valid")
    rows.sort(key=lambda r: r["tick"])
    dedup = []
    for r in rows:
        if dedup and dedup[-1]["tick"] == r["tick"]:
            if r["kind"] != "-":
                dedup[-1] = r  # a checked sample is the same in every pass
            continue
        dedup.append(r)
    rows = dedup
    if not rows:
        print("no trace entries (stall or timeout?)")
        return 1
    t0 = rows[0]["tick"]
    if args.csv:
        write_csv(args.csv, [dict(r, ns=round((r["tick"] - t0) * ns, 1)) for r in rows])

    # Exerciser outputs are shown `skew` clocks late, as the pins see them.
    ticks = [r["tick"] for r in rows]
    names = [n for n, _, _ in TRACE_OUT]
    head = f"{'ns':>7} {'cyc':>3} " + " ".join(f"{n:>{max(len(n), 1)}}" for n in names)
    head += "   exD  dir |  D  COL G44 G45 | sample"
    print(head)
    last = None
    for r in rows:
        k = bisect.bisect_right(ticks, r["tick"] - args.skew) - 1
        o = rows[k] if k >= 0 else rows[0]
        outs = [(o["out"] >> g) & 1 for _, g, _ in TRACE_OUT]
        exd, exdir = o["d"] & 0xFF, o["d"] >> 8
        din, col = r["rx0"] & 0xFF, (r["rx1"] >> 4) & 0xF
        g44, g45 = (r["rx1"] >> 12) & 1, (r["rx1"] >> 13) & 1
        state = (tuple(outs), exd if exdir else None, exdir, din, col, g44, g45)
        checked = r["kind"] != "-"
        if not args.all and state == last and not checked:
            continue
        last = state
        line = f"{(r['tick'] - t0) * ns:7.0f} {r['cycle']:3} "
        line += " ".join(f"{v:>{len(n)}}" for v, n in zip(outs, names))
        line += f"   {exd:02x}   {exdir:02x} | {din:02x}  {col:x}   {g44}   {g45} |"
        if checked:
            mask = D_MASK.get(r["kind"], 0xFF)
            good = not ((din ^ r["exp_d"]) & mask)
            line += f" {r['kind']} {r['addr']:04x} exp {r['exp_d']:02x} {'ok' if good else 'BAD'}"
        print(line)

    print()
    print("reads, in ns after the select fell at the pins (- = not seen while it was low):")
    print(f"  {'cycle':>5} {'check':<15} {'addr':4} {'low for':>7} {'answer':>6} {'gpio44':>6} {'gpio45':>6} "
          f"{'sample':>6}")
    fmt = lambda t, a: f"{(t - a['fall']) * ns:6.0f}" if t is not None else f"{'-':>6}"
    for a in answer_times(rows, args.skew):
        print(f"  {a['cycle']:5} {a['kind']:<15} {a['addr']:04x} {(a['rise'] - a['fall']) * ns:7.0f} "
              f"{fmt(a['answer'], a)} {fmt(a['g44'], a)} {fmt(a['g45'], a)} {fmt(a['sample'], a)} "
              f"{'ok' if a['ok'] else 'BAD'}")


# Access types for `timings`: the check, and the half-cycle kinds that make
# it (refresh and idle halves fill the rest).
TIMING_KINDS = [
    ("ram_read", {"w_ram_r": 4}),
    ("ram_readback", {"w_ram_w": 2, "w_readback": 2}),
    ("basic", {"w_basic": 4}),
    ("kernal", {"w_kernal": 4}),
    ("char", {"w_char": 4}),
    ("color_read", {"w_col_r": 4}),
    ("color_readback", {"w_col_w": 2, "w_readback": 2}),
    ("vic_ram", {"w_v_ram": 4}),
    ("vic_char", {"w_v_char": 4}),
    ("cacc_ram", {"w_cacc": 4}),
    ("cacc_color", {"w_cacc": 4}),
]
WEIGHTS = ["w_v_refresh", "w_v_ram", "w_v_char", "w_ram_r", "w_ram_w", "w_basic", "w_kernal", "w_char",
           "w_col_r", "w_col_w", "w_idle", "w_release", "w_readback", "w_cacc"]


def cmd_timings(args):
    """Per access type: the sample offset after the select from which reads
    pass (minus the calibration), and pass or fail at the latch point."""
    ex, dut = prepare(args)
    offsets = span(args.offsets)
    speeds = span(args.speeds)
    rows = []

    def run_check(check, batches):
        _, checks, _, _ = run_step(ex, dut, batches)
        return checks.get(check, (0, 0))

    for check, weights in TIMING_KINDS:
        if args.only and check not in args.only.split(","):
            continue
        ex.command("ntsc" if args.ntsc else "pal")
        apply_sets(ex, args.set)
        for w in WEIGHTS:
            ex.command(f"set {w} {weights.get(w, 1 if w in ('w_v_refresh', 'w_idle') else 0)}")
        if check == "vic_char":
            ex.command("set bank 0")  # the character ROM is in banks 0 and 2
        ex.command("set sample_mode 1")
        results = []
        for off in offsets:
            ex.command(f"set sample_off {off}")
            n, err = run_check(check, args.batches)
            if n:
                results.append((off, n, err))
        # The first window with fewer errors than --max-rate: reads pass from
        # its start until the DUT lets go after the select rises.
        ok = lambda n, e: e <= n * args.max_rate
        first_ok = next((o for o, n, e in results if ok(n, e)), None)
        last_err = max((o for o, n, e in results if not ok(n, e) and (first_ok is None or o < first_ok)),
                       default=None)
        ok_until, win_n, win_err = None, 0, 0
        if first_ok is not None:
            for o, n, e in results:
                if o >= first_ok:
                    if not ok(n, e):
                        break
                    ok_until, win_n, win_err = o, win_n + n, win_err + e
        ex.command("set sample_mode 0")
        latch = []
        for sp in speeds:
            ex.command(f"set speed {sp}")
            n, err = run_check(check, args.batches)
            latch.append((sp, n, err))
        ex.command("set speed 100")
        cal = args.cal_ns
        row = {"check": check,
               "errors_until": None if last_err is None else last_err - cal,
               "ok_from": None if first_ok is None else first_ok - cal,
               "ok_until": None if ok_until is None else ok_until - cal,
               "window_errors": f"{win_err}/{win_n}"}
        for sp, n, err in latch:
            row[f"latch_{sp}"] = f"{err}/{n}"
        rows.append(row)
        print(f"{check:<15} last error {fmt_ns(row['errors_until'])}  error free {fmt_ns(row['ok_from'])} "
              f"to {fmt_ns(row['ok_until'])} ({win_err}/{win_n})  "
              + "  ".join(f"{sp}%:{err}/{n}" for sp, n, err in latch), flush=True)
    write_csv(args.csv, rows)


def fmt_ns(v):
    return f"{v:4d} ns" if v is not None else "   - ns"


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
    tm = sub.add_parser("timings", parents=[common])
    tm.add_argument("--offsets", default="0:400:8", help="from:to:step, ns after the select")
    tm.add_argument("--speeds", default="100:150:25", help="from:to:step, percent, at the latch point")
    tm.add_argument("--cal-ns", type=int, default=12, help="calibration offset in ns, subtracted")
    tm.add_argument("--only", help="comma-separated checks")
    tm.add_argument("--max-rate", type=float, default=0.005, help="error rate that still counts as passing")
    tp = sub.add_parser("trace", parents=[common])
    tp.add_argument("--cycles", type=int, default=4, help="cycles to trace")
    tp.add_argument("--step", type=int, default=6, help="exerciser clocks between samples in one pass (6 or more)")
    tp.add_argument("--passes", type=int, default=0, help="passes with shifted phase (default: step, one sample per clock)")
    tp.add_argument("--seed-run", type=int, default=1, help="batch seed")
    tp.add_argument("--skew", type=int, default=3, help="calibration offset in exerciser clocks")
    tp.add_argument("--all", action="store_true", help="print every sample, not only changes")
    args = ap.parse_args()
    handler = {"info": cmd_info, "run": cmd_run, "sweep": cmd_sweep, "shmoo": cmd_shmoo, "cal": cmd_cal,
               "trace": cmd_trace, "timings": cmd_timings}
    return handler[args.cmd](args) or 0


if __name__ == "__main__":
    sys.exit(main())
