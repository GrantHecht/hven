#!/usr/bin/env python3
"""M8 DN-3 (../DECLARATION-r1.md, criterion E): the enumeration leg's reading.

Per roster cell (EV/cells/<tag>.N.* = `default`, kNever; <tag>.A.* = `auto-default`, kAuto; one serial `run` each):
  candidate      the A row's side.dn3_dense_factorizations > 0: some analyzed pattern of the solve had order <= the
                 engine's threshold and was factorized on the dense arm (design Q8: side factors by their own n);
  E1 N_ZERO      every N row reads side.dn3_dense_factorizations == 0 (kNever takes no dense factorization);
  E2 COLUMNS     every row carries the column and a status (no CAPPED / THREW / NO_ROW cell on either arm unless both);
  E3 IDENTITY    every NON-candidate's A row equals its N row in EVERY key=value line except options_label
                 (status, iterations, objective as printed, every counter and side column);
Writes EV/candidates.tsv (pop<TAB>lane<TAB>problem, roster order). Exit 0 iff E1-E3 hold.

Usage: enum_analyze.py EV_ENUM_DIR
"""
import collections
import os
import sys


def load(prefix):
    row = {"_capped": False, "_present": False}
    res = prefix + ".result"
    if os.path.exists(res):
        row["_capped"] = "CAPPED" in open(res).read()
    out = prefix + ".out"
    if os.path.exists(out):
        for line in open(out):
            line = line.rstrip("\n")
            if "=" in line:
                k, v = line.split("=", 1)
                row[k] = v
                row["_present"] = True
    return row


def main():
    ev = sys.argv[1]
    roster = [l.rstrip("\n").split("\t") for l in open(os.path.join(ev, "..", "roster.tsv")) if l.strip()]
    cand = []
    bad = collections.defaultdict(list)
    per = collections.Counter()
    print(f"{'cell':44s} {'N status':16s} {'A status':16s} {'it N/A':>9s} {'fac N/A':>11s} {'dense A':>8s} class")
    for pop, lane, prob in roster:
        tag = f"{pop}-{lane}-{prob}"
        n = load(os.path.join(ev, "cells", tag + ".N"))
        a = load(os.path.join(ev, "cells", tag + ".A"))
        eng = "IPM" if lane == "hven-ipm" else "SQP"
        fam = "HS" if pop == "hs" else "SD"
        for arm, r in (("N", n), ("A", a)):
            if r["_capped"] or not r["_present"] or "status" not in r:
                bad["E2_NO_ROW_OR_CAPPED"].append(f"{tag}.{arm}")
            elif "side.dn3_dense_factorizations" not in r:
                bad["E2_COLUMN_MISSING"].append(f"{tag}.{arm}")
        dn = int(n.get("side.dn3_dense_factorizations", "-1"))
        da = int(a.get("side.dn3_dense_factorizations", "-1"))
        if dn != 0:
            bad["E1_N_NONZERO"].append(f"{tag} ({dn})")
        is_cand = da > 0
        if is_cand:
            cand.append((pop, lane, prob))
            per[(eng, fam, "candidate")] += 1
            fac = a.get("factorizations", "?")
            cls = f"dense {da} of factorizations {fac}"
        else:
            per[(eng, fam, "non-candidate")] += 1
            cls = "all-sparse"
            keys = (set(n) | set(a)) - {"options_label", "_capped", "_present"}
            diff = sorted(k for k in keys if n.get(k) != a.get(k))
            if diff:
                bad["E3_NONCANDIDATE_MOVED"].append(f"{tag}: {', '.join(diff[:6])}")
        print(f"{tag:44s} {n.get('status', '-'):16s} {a.get('status', '-'):16s} "
              f"{n.get('iterations', '-'):>4s}/{a.get('iterations', '-'):<4s} "
              f"{n.get('factorizations', '-'):>5s}/{a.get('factorizations', '-'):<5s} {da:8d} {cls}")
    with open(os.path.join(ev, "candidates.tsv"), "w") as f:
        for c in cand:
            f.write("\t".join(c) + "\n")
    print()
    print("== COUNTS (engine, family, class)")
    for k in sorted(per):
        print(f"  {k[0]} {k[1]} {k[2]}: {per[k]}")
    for eng in ("IPM", "SQP"):
        c = sum(v for k, v in per.items() if k[0] == eng and k[2] == "candidate")
        t = sum(v for k, v in per.items() if k[0] == eng)
        print(f"  {eng}: {c} candidates of {t} cells")
    print()
    print("== VERDICTS")
    ok = True
    for name in ("E1_N_NONZERO", "E2_NO_ROW_OR_CAPPED", "E2_COLUMN_MISSING", "E3_NONCANDIDATE_MOVED"):
        if bad[name]:
            ok = False
            print(f"{name}: {len(bad[name])} -- " + "; ".join(bad[name][:40]))
        else:
            print(f"{name}: none")
    print(f"candidates: {len(cand)} (written to candidates.tsv)")
    print("ENUM_PASS" if ok else "ENUM_FAIL")
    sys.exit(0 if ok else 6)


if __name__ == "__main__":
    main()
