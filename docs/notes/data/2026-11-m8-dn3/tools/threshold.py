#!/usr/bin/env python3
"""M8 DN-3 (../DECLARATION-r1.md, criterion T; r0 section 1 T1-T3): the per-engine threshold from frozen data.

T_E = (the smallest n among engine E's replayed matrices with rho >= 1) - 1, or E's largest replayed n if none, where
rho = dense_total / sparse_total per row of docs/notes/data/2026-11-m8-dn1-design/matrices.csv.gz; a missing,
zero-denominator or non-finite value counts as rho >= 1. E is read from the cell name (hven-ipm / hven-sqp).
The input must hash to the declared sha256 (S-T0); the result must be T_SQP = 105, T_IPM = 167 (S-T1).
Density nnz/n^2 is not in this file (r0 T2 reads it from DN-0's index.tsv dumps): reported as NOT READ here.

Usage: threshold.py MATRICES_CSV_GZ   -> stdout; exit 0 iff the hash and both values are as declared.
"""
import csv
import gzip
import hashlib
import math
import sys

WANT_SHA = "f204be2c2a444b8fcdda8081a53f5940e04fc8ab8cf858b4c257f6e9b53b078c"
WANT = {"SQP": 105, "IPM": 167}


def main():
    path = sys.argv[1]
    sha = hashlib.sha256(open(path, "rb").read()).hexdigest()
    print(f"input {path}\nsha256 {sha}")
    if sha != WANT_SHA:
        print("S-T0: sha256 differs")
        sys.exit(2)
    rows = {"SQP": [], "IPM": []}
    with gzip.open(path, "rt") as f:
        for r in csv.DictReader(f):
            eng = "IPM" if "-hven-ipm-" in r["cell"] else "SQP" if "-hven-sqp-" in r["cell"] else None
            if eng is None:
                continue
            n = int(r["n"])
            try:
                d, s = float(r["dense_total"]), float(r["sparse_total"])
                rho = d / s if s != 0 else math.inf
            except (ValueError, KeyError):
                rho = math.inf
            if not math.isfinite(rho):
                rho = math.inf
            rows[eng].append((n, rho, r["cell"], r["matrix"]))
    ok = True
    for eng in ("SQP", "IPM"):
        v = rows[eng]
        bad = sorted((n, rho, c, m) for n, rho, c, m in v if rho >= 1)
        t = bad[0][0] - 1 if bad else max(n for n, *_ in v)
        first = [b for b in bad if b[0] == bad[0][0]] if bad else []
        print(f"{eng}: {len(v)} matrices; T = {t}; first rho >= 1 at n = {bad[0][0] if bad else '-'}: "
              + ", ".join(f"{c} {m} rho {rho:.3f}" for n, rho, c, m in first))
        if t != WANT[eng]:
            print(f"S-T1: T_{eng} = {t}, declared {WANT[eng]}")
            ok = False
    print("density (nnz/n^2): NOT READ by this instrument (not a criterion input)")
    print("THRESHOLD_AS_DECLARED" if ok else "THRESHOLD_MISMATCH")
    sys.exit(0 if ok else 3)


if __name__ == "__main__":
    main()
