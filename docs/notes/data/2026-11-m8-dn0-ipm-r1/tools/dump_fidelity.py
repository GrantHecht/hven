#!/usr/bin/env python3
"""Compare dump-build cell outputs against stat-r1 outputs on status,
iterations and objective. Usage: dump_fidelity.py <run_dir>"""
import sys, os, glob

def parse(path):
    d = {}
    for line in open(path):
        if "=" in line:
            k, v = line.rstrip("\n").split("=", 1)
            d.setdefault(k, v)
    return tuple(d.get(k) for k in ("status", "iterations", "objective"))

run = sys.argv[1]
names = sorted(os.path.basename(p) for p in glob.glob(os.path.join(run, "dump", "*.out")))
ok = 0
print("cell\tstatus\titerations\tobjective\tmatch")
for n in names:
    a = parse(os.path.join(run, "dump", n))
    bp = os.path.join(run, "stat-r1", n)
    b = parse(bp) if os.path.exists(bp) else None
    m = a == b
    ok += m
    print(f"{n[:-4]}\t{a[0]}\t{a[1]}\t{a[2]}\t{'MATCH' if m else 'MISMATCH ' + str(b)}")
print(f"IDENTICAL {ok}/{len(names)}")
