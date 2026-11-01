#!/usr/bin/env python3
"""PREVIEW only (not the record): run A merged and decided, before run B exists."""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import train_close as tc  # noqa: E402

cells, out, bench = sys.argv[1:4]
sets = {"hs": tc.roster(bench, "hs-27.json"), "cutest": tc.roster(bench, "cutest-nlp.json")}
tree = os.path.join(out, "tree")
for label, fams in tc.A2_LABELS.items():
    slot = "default" if label == "sqp-ipm" else label
    for s in fams:
        tc.merge(cells, label, s, "A", sets[s], os.path.join(tree, "hven-sqp", slot, f"{s}-run-A"))
r = subprocess.run(["python3", "-B", os.path.join(bench, "scripts", "w2_a4_decide.py"), tree],
                   capture_output=True, text=True)
print(r.stdout + r.stderr)
