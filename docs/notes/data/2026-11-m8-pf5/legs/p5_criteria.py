#!/usr/bin/env python3
"""PF-5 (../DECLARATION-PF-5.md) P5: the asserted instruction lines. usage: p5_criteria.py <instr-train.txt> <nlp A> <nlp B>
TRAIN: `TOTAL hven-sqp` B/A <= 1.000, `TOTAL hven-ipm` B/A in [0.995, 1.005] (from instr_compare.py's table);
nlp-35: ACOPP118 B/A <= 0.90, YORKNET B/A <= 0.95 (per-process instructions:u of the co-run, instr_compare.instr()).
Prints P5_PASS / P5_FAIL."""
import importlib.util
import os
import sys

spec = importlib.util.spec_from_file_location("ic", os.path.join(os.path.dirname(os.path.abspath(__file__)), "instr_compare.py"))
ic = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ic)

ok = True
tot = {}
for line in open(sys.argv[1]):
    p = line.split()
    if len(p) > 2 and p[0] == "TOTAL" and p[1] in ("hven-sqp", "hven-ipm"):
        tot[p[1]] = float(p[-1])
for lane, lo, hi in (("hven-sqp", 0.0, 1.000), ("hven-ipm", 0.995, 1.005)):
    v = tot.get(lane)
    good = v is not None and lo <= v <= hi
    ok &= good
    print(f"TRAIN {lane} B/A {v} in [{lo}, {hi}]: {'ok' if good else 'FAIL'}")
for cell, hi in (("ACOPP118", 0.90), ("YORKNET", 0.95)):
    a = ic.instr(os.path.join(sys.argv[2], f"nlp-hven-sqp-{cell}.D"))
    b = ic.instr(os.path.join(sys.argv[3], f"nlp-hven-sqp-{cell}.D"))
    r = b / a if a and b else None
    good = r is not None and r <= hi
    ok &= good
    print(f"nlp-35 {cell} A {a} B {b} B/A {r} <= {hi}: {'ok' if good else 'FAIL'}")
print("P5_PASS" if ok else "P5_FAIL")
sys.exit(0 if ok else 1)
