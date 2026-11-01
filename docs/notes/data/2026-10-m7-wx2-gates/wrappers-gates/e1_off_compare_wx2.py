#!/usr/bin/env python3
"""M7 WX gates: the E1 OFF comparison. usage: e1_off_compare_wx2.py BASE.csv HEAD.csv ADDED_COLUMNS   (comma list; "" = the head adds no column)

COPIED from e1_off_compare_a3.py (NOT edited in place); the ONE change: the declared added-column set may be EMPTY (WX adds no E1 column).

Derived from docs/notes/data/2026-10-m7-a1-impl/scripts/i2_compare.py `repro` (same csv reader, same rule: every column common to both
files except wall_s is byte-equal per cell), plus the WX treatment of the columns the head adds:
  * the set of head-only columns (head header minus base header) must EQUAL the declared set exactly;
  * the base must have no column the head lacks;
  * every declared added column must be the string "0" on every row (WX is identity at the shipped defaults on E1).
Exit 0 only if all hold. Comment lines (#) are ignored."""
import csv
import sys


def load(path):
    with open(path) as f:
        rows = list(csv.DictReader(l for l in f if not l.startswith("#")))
    return {r["id"]: r for r in rows}, rows


def main():
    base_p, head_p, declared = sys.argv[1], sys.argv[2], set(c for c in sys.argv[3].split(",") if c)
    b, _ = load(base_p)
    h, _ = load(head_p)
    ok = True
    if set(b) != set(h):
        print("CELL SET DIFFERS:", sorted(set(b) ^ set(h)))
        return 1
    bcols = list(next(iter(b.values())))
    hcols = list(next(iter(h.values())))
    added = [c for c in hcols if c not in bcols]
    missing = [c for c in bcols if c not in hcols]
    print(f"ADDED_COLUMNS {added} DECLARED {sorted(declared)}")
    if set(added) != declared or len(added) != len(declared):
        print("ADDED COLUMN SET IS NOT THE DECLARED ONE")
        ok = False
    if missing:
        print("COLUMNS THE HEAD LACKS:", missing)
        ok = False
    common = [c for c in bcols if c in hcols and c != "wall_s"]
    diffs = 0
    for cid in b:
        for k in common:
            if b[cid][k] != h[cid][k]:
                diffs += 1
                print(f"DIFF {cid} {k}: base={b[cid][k]} head={h[cid][k]}")
    nonzero = [(cid, c, h[cid][c]) for cid in h for c in declared if c in hcols and h[cid][c] != "0"]
    for t in nonzero:
        print("NONZERO ADDED COLUMN", *t)
    if nonzero:
        ok = False
    print(f"E1_OFF cells={len(b)} columns_compared={len(common)} differences={diffs} added_columns={len(added)} added_nonzero_rows={len(nonzero)}")
    if diffs:
        ok = False
    return 0 if ok else 1


sys.exit(main())
