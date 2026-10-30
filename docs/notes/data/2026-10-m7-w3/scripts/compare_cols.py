#!/usr/bin/env python3
"""Compare two CSVs row by row on the REFERENCE's columns (minus --exclude), keyed by --key.
Usage: compare_cols.py REF NEW --key K [--exclude c1,c2] [--title T]. Exit 0 iff 0 differences."""
import argparse, csv, sys
ap = argparse.ArgumentParser()
ap.add_argument("ref"); ap.add_argument("new")
ap.add_argument("--key", required=True); ap.add_argument("--exclude", default="")
ap.add_argument("--title", default="")
a = ap.parse_args()
def load(p):
    rows = list(csv.DictReader(l for l in open(p) if not l.startswith("#")))
    return rows[0].keys() if rows else [], {r[a.key]: r for r in rows}
rc, R = load(a.ref); nc, N = load(a.new)
ex = set(x for x in a.exclude.split(",") if x)
cols = [c for c in rc if c not in ex]
missing_cols = [c for c in cols if c not in nc]
nd = 0; lines = []
for k in R:
    if k not in N:
        lines.append(f"{k}: MISSING in new"); nd += 1; continue
    for c in cols:
        if c in missing_cols: continue
        if R[k][c] != N[k][c]:
            nd += 1; lines.append(f"{k}.{c}: {R[k][c]} -> {N[k][c]}")
for k in N:
    if k not in R: lines.append(f"{k}: EXTRA in new"); nd += 1
print(f"== {a.title}")
print(f"ref={a.ref}\nnew={a.new}\nrows ref={len(R)} new={len(N)} compared_cols={len(cols)} excluded={sorted(ex)} ref_cols_missing_in_new={missing_cols}")
for l in lines: print("  " + l)
print(f"DIFFERENCES {nd}")
sys.exit(0 if nd == 0 and not missing_cols else 1)
