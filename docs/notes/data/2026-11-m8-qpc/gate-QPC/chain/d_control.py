#!/usr/bin/env python3
"""V-PDIV reproduction control (DECLARATION section 5.2 / leg D): a diagnostic build's H0 rows on the sample must equal its NON-diagnostic
counterpart's rows (status, iterations, factorizations, eval_f, eval_jac, objective; the pdiv study's own control, tools/verify.py).
usage: d_control.py <runs dir (<name>.row files)> <tasks.tsv ...> -- <store> <arm: H0 for the R side, QPC for the QPC side>
Counterpart rows = the ensemble store's member-0 rows of that arm (R: the census H0 rows; QPC: the E-A member-0 rows). rc 0 iff every
H0 task (seed '-', no trust_region_initial key) with a store row matches; NOSTORE cells are listed (not a pass)."""
import csv, glob, os, sys
a = sys.argv[1:]; i = a.index("--"); rd, tasks, (store, arm) = a[0], a[1:i], a[i + 1:]
rows = {}
for c in ("TRAIN-238", "CAP-238-1", "CAP-238-01", "NLP-46"):
    for f in glob.glob(f"{store}/{c}/{arm}/*/*/*/results.csv"):
        for r in csv.DictReader(l for l in open(f) if not l.startswith("#")):
            rows[(r["problem"], r["options_label"])] = r
ok = bad = nos = 0
for t in tasks:
    for line in open(t):
        if not line.strip() or line.startswith("#"): continue
        name, prob, label, bf, seed = line.rstrip("\n").split("\t")
        if seed != "-" or "trust_region_initial" in label or not name.endswith("__H0"): continue
        p = f"{rd}/{name}.row"
        if not os.path.exists(p): print("NOROW", name); bad += 1; continue
        r = dict(l.rstrip("\n").split("=", 1) for l in open(p) if "=" in l)
        s = rows.get((prob, label))
        if s is None: print("NOSTORE", name); nos += 1; continue
        d = [(k, s[k], r.get(k)) for k in ("status", "iterations", "factorizations", "eval_f", "eval_jac", "objective") if s[k] != r.get(k)]
        if d: bad += 1; print("MISMATCH", name, d)
        else: ok += 1
print("reproduction control", arm, "verified", ok, "mismatch", bad, "nostore", nos)
sys.exit(0 if bad == 0 and ok > 0 else 1)
