#!/usr/bin/env python3
"""Compare two corpus replay CSVs on every non-wall column, joined on cell_id."""
import csv, sys

def load(path):
    rows, hdr = {}, None
    with open(path, newline="") as f:
        lines = [l for l in f if not l.startswith("#")]
    r = csv.reader(lines)
    hdr = next(r)
    for row in r:
        if not row or len(row) != len(hdr):
            continue
        rows[row[0]] = dict(zip(hdr, row))
    return hdr, rows

def main(a, b, label):
    ha, ra = load(a)
    hb, rb = load(b)
    if ha != hb:
        print(f"=== {label} === SCHEMA MISMATCH")
        print(" only-in-A:", [c for c in ha if c not in hb])
        print(" only-in-B:", [c for c in hb if c not in ha])
        return 2
    cols = [c for c in ha if c != "wall_s"]
    keys = sorted(set(ra) & set(rb))
    missing = sorted(set(ra) ^ set(rb))
    diffs = []
    for k in keys:
        for c in cols:
            if ra[k][c] != rb[k][c]:
                diffs.append((k, c, ra[k][c], rb[k][c]))
    print(f"=== {label} === cells compared: {len(keys)}  columns compared: {len(cols)}  "
          f"differences: {len(diffs)}" + (f"  UNMATCHED CELLS: {missing}" if missing else ""))
    for d in diffs[:200]:
        print(f"    {d[0]:32s} {d[1]:34s} A={d[2]}  B={d[3]}")
    return 1 if (diffs or missing) else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2], sys.argv[3]))
