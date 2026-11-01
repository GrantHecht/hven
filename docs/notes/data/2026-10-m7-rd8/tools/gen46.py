#!/usr/bin/env python3
"""Emit the 46-cell leg list: LANE OUTDIR PROBLEM LABEL CAP ENV. Caps: 300 s for the cells RETAKE18 capped,
590 s for all others. usage: gen46.py LANE OUTDIR ENV [ONLY=P-L,P-L]"""
import json
import sys
R18 = "/home/ghecht/Projects/hven-bench/runs/2026-10-m7-w2-retake18/measured/walls.txt"
lane, od, env = sys.argv[1:4]
only = set(sys.argv[4].split(",")) if len(sys.argv) > 4 else None
pin = json.load(open("/home/ghecht/Projects/hven-bench/sets/cutest-nlp.json"))
probs = sorted(p["problem"] for p in pin["problems"] if p["split"] == "TRAIN")
capped = set()
for line in open(R18):
    p = line.split()
    if len(p) >= 2 and p[1] == "CAPPED":
        capped.add(p[0])
for p in probs:
    for lab in ("default", "sqp-qn"):
        c = f"{p}-{lab}"
        if only is not None and c not in only:
            continue
        cap = 300 if c in capped else 590
        print(f"{lane} {od} {p} {lab} {cap} {env}")
