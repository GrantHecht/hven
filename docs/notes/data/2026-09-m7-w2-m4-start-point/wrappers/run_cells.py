#!/usr/bin/env python3
"""M7 W2 M4's own serial cell runner for the CUTEst TRAIN 23, SQP lane.

WHY NOT `runners/run_serial.py`. That runner's stamp trap refuses a prefix whose
`HVEN_PIN.txt` sha is not `cmake/hven_pin.cmake`'s, which is exactly the case
here: this task measures an hven AHEAD of the harness pin (3904a477), and the
re-pin is the settler's own declared event. Nothing in the harness tree is
touched. This runner keeps every term the H3 run was taken under -- one cell,
one process, serial, `MKL_NUM_THREADS=1`, a 300 s wall ceiling
(`sets/cutest-nlp.json` `run_terms.budget_wall_s`), `--budget-iters 0` -- and
records the cell binary's own key=value output verbatim.

ONE ARM, ONE LOCK ACQUISITION -- the SAME GRANULARITY the other lane's sweep
uses on this box (`scripts/w2_a4_sweep.sh <label> <lane> <set> <run>` takes the
lock for a whole arm), so the two interleave arm by arm instead of one lane
starving the other. Inside the lock, before each cell, a `pgrep` refuses to
proceed if a FOREIGN cell binary is alive -- which would mean something is
running outside the lock, the exact contamination this protocol exists to
prevent. There is no timeout and no fallback on the lock itself.

WALL IS INFORMATIONAL. The asserted columns are the status, the outcome and the
counters. `dnf_wall` is the one WALL-DEPENDENT outcome, and it is honest here
because the cell holds the box lock alone for its whole 300 s.
"""

import argparse
import csv
import os
import subprocess
import sys
import time

TRAIN23 = [
    "ACOPP118", "ACOPP57", "ACOPR118", "ACOPR57", "BIGBANK", "BRITGAS", "DALLASM",
    "MSS2", "MSS3", "NET3", "READING6", "READING8", "SAWPATH", "SMBANK", "STEENBRB",
    "STEENBRC", "STEENBRD", "STEENBRE", "STEENBRG", "TAX213322", "TAX53322",
    "YORKNET", "ZAMB2-8",
]
WALL_CEILING_S = 300.0
# THE CELL RUNS WITH THE HARNESS ROOT AS ITS CWD. A decoded CUTEst problem's
# files are found at `.scratch/h3/decoded/<PROBLEM>` -- a path RELATIVE to the
# harness root (`lanes/cutest/cutest_model.cpp:123`, and its header says so:
# "Where a decoded problem's files live, relative to the harness root"). A cell
# started anywhere else throws "cannot enter .scratch/h3/decoded/<P>" and writes
# a row with `threw=1`, which is what a first pass of this runner produced on
# all 23 cells. Reading that tree is all this does; nothing in it is written.
HARNESS_ROOT = "/home/ghecht/Projects/hven-bench"

def foreign_cells(my_binary_real):
    """Any hven_bench_cell process that is not this arm's own binary.

    BY PROCESS NAME, NOT BY COMMAND LINE: this file's own text contains the
    string `hven_bench_cell`, so a cmdline match would find the python process
    running it (the declared self-match). `pgrep -x` matches the executable's
    name only, and each hit is resolved through /proc to its real path.
    """
    found = []
    proc = subprocess.run(["pgrep", "-x", "hven_bench_cell"],
                          capture_output=True, text=True)
    for pid in proc.stdout.split():
        try:
            exe = os.path.realpath(os.readlink("/proc/{}/exe".format(pid)))
        except OSError:
            continue
        if exe != my_binary_real:
            found.append("{}:{}".format(pid, exe))
    return found


def run_one(binary, problem, raw_dir):
    """One cell, one process, inside the arm's lock. The ceiling is on the CELL."""
    my_real = os.path.realpath(binary)
    foreign = foreign_cells(my_real)
    if foreign:
        raise SystemExit(
            "ABORTED before {}: a foreign cell binary is alive while this arm holds "
            "the box lock: {}".format(problem, " ".join(foreign)))

    env = dict(os.environ)
    env["MKL_NUM_THREADS"] = "1"
    env["OMP_NUM_THREADS"] = "1"
    cmd = [binary, "--lane", "hven-sqp", "--problem", problem,
           "--options", "default", "--budget-iters", "0"]
    t0 = time.time()
    killed = False
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, env=env,
                              cwd=HARNESS_ROOT, timeout=WALL_CEILING_S)
        out, err, rc = proc.stdout, proc.stderr, proc.returncode
    except subprocess.TimeoutExpired as exc:
        killed = True
        out = exc.stdout.decode() if isinstance(exc.stdout, bytes) else (exc.stdout or "")
        err = exc.stderr.decode() if isinstance(exc.stderr, bytes) else (exc.stderr or "")
        rc = None
    wall = time.time() - t0

    with open(os.path.join(raw_dir, problem + ".txt"), "w") as handle:
        handle.write("$ " + " ".join(cmd) + "\n")
        handle.write("# wall_s {:.3f}  rc {}  ceiling_kill {}\n".format(wall, rc, killed))
        handle.write(out)
        if err:
            handle.write("--- stderr ---\n" + err)

    row = {"problem": problem, "wall_s": "{:.3f}".format(wall), "cell_rc": str(rc)}
    for line in out.splitlines():
        if "=" not in line or line.startswith(("CELL_BEGIN", "CELL_END")):
            continue
        key, _, value = line.partition("=")
        row[key] = value
    if row.get("threw") == "1":
        # A CELL THAT THREW IS NOT A MEASUREMENT AND IS NOT QUIETLY RECORDED.
        # The harness normalises an exception to `error`, which is a legitimate
        # OUTCOME of a solve -- but the exceptions this runner can cause are
        # about its own setup (a missing decoded problem), and a run that
        # recorded 23 of those as "error" would read as a catastrophic
        # regression. Refuse and say which.
        raise SystemExit("ABORTED on {}: the cell threw -- {}".format(
            problem, row.get("exception", "<no message>")))
    if "outcome" not in row:
        # The set's DNF CONDITION, never a measurement (schema/v1.md).
        row["outcome"] = "dnf_wall" if killed else "no_row"
        row.setdefault("status", "null")
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", required=True)
    ap.add_argument("--tag", required=True, help="e.g. off-r1")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    binary = os.path.abspath(args.binary)
    raw_dir = os.path.join(args.out, "raw", args.tag)
    os.makedirs(raw_dir, exist_ok=True)

    rows = []
    for problem in TRAIN23:
        print("[{}] {} ...".format(args.tag, problem), flush=True)
        row = run_one(binary, problem, raw_dir)
        print("    {} {}  wall={}".format(row.get("outcome"), row.get("status", ""),
                                          row["wall_s"]), flush=True)
        rows.append(row)

    keys = []
    for row in rows:
        for key in row:
            if key not in keys:
                keys.append(key)
    path = os.path.join(args.out, "cells-{}.csv".format(args.tag))
    with open(path, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=keys, restval="")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
    print("wrote", path, flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
