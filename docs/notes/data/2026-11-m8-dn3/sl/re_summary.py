#!/usr/bin/env python3
"""M8 DN-3 (../DECLARATION-r1.md R-e, informational, never gating): the four median cells at the engines' default threads
(`sl/adopt_run.sh T`: `t0-` labels, MKL variables unset, unpinned). Per cell and arm: status, raw = the median of the three
stat rounds, s_init / s_proc from the T-leg record, solver = raw x (1 - s_init - s_proc), and A/N on both.

Usage: re_summary.py EV   -> stdout
"""
import json
import os
import statistics
import sys

CELLS = ["hs-hven-ipm-hs026", "sd-hven-ipm-HS67", "hs-hven-sqp-hs043", "sd-hven-sqp-HS84"]


def instr(p):
    for line in open(p):
        parts = line.split(",")
        if len(parts) > 2 and parts[2].startswith("instructions"):
            return int(parts[0])
    return None


def main():
    ev = sys.argv[1]
    d = os.path.join(ev, "threads")
    print("cell | arm | status | raw | s_init | s_proc | solver")
    for c in CELLS:
        sv = {}
        for a in ("N", "A"):
            raw = int(statistics.median(instr(os.path.join(d, f"{c}.{a}.r{k}.stat")) for k in (1, 2, 3)))
            out = open(os.path.join(d, f"{c}.{a}.r1.out")).read().splitlines()
            st = next((l.split("=", 1)[1] for l in out if l.startswith("status=")), None)
            if st is None:  # a row with no status threw: report the exception, not a measurement
                st = "THREW: " + next((l.split("=", 1)[1] for l in out if l.startswith("exception=")), "?")
            j = json.load(open(os.path.join(d, f"{c}.{a}.attr.json")))
            solver = raw * (1 - j["s_init"] - j["s_proc"])
            sv[a] = (raw, solver)
            print(f"{c} | {a} | {st} | {raw} | {j['s_init']:.4f} | {j['s_proc']:.4f} | {solver:.0f}")
        print(f"{c} | A/N | raw {sv['A'][0] / sv['N'][0]:.3f} | solver {sv['A'][1] / sv['N'][1]:.3f}")


if __name__ == "__main__":
    main()
