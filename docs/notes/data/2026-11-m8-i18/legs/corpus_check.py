#!/usr/bin/env python3
"""I18 corpus check: arm A rows vs arm B rows (corpus.sh), 146 dumps + the issue dump at 4 settings.

A row moves when status, minors or factorizations differ (counters exact; co-run at MKL_NUM_THREADS=1).
I18_CORPUS_PASS iff: both arms have 150 rows and no TIMEOUT_OR_FAIL row; the 4 issue rows of B are kOptimal (status=0);
no row kOptimal in A is non-optimal in B; the moved rows are exactly MOVERS (one row name per line, '#' comments);
minors and factorizations summed over the rows kOptimal in both arms rise by at most 3% (B over A).
usage: corpus_check.py A_ROWS B_ROWS MOVERS.txt   (MOVERS '-' = report only)"""
import sys


def load(p):
    rows = {}
    for line in open(p):
        parts = line.split()
        if not parts:
            continue
        kv = dict(t.split("=", 1) for t in parts[1:] if "=" in t)
        kv["_raw"] = " ".join(parts[1:])
        rows[parts[0]] = kv
    return rows


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    a, b = load(sys.argv[1]), load(sys.argv[2])
    declared = None
    if sys.argv[3] != "-":
        declared = set()
        for line in open(sys.argv[3]):
            line = line.split("#", 1)[0].strip()
            if line:
                declared.add(line)
    fails = []
    for name, rows in (("A", a), ("B", b)):
        if len(rows) != 150:
            fails.append(f"ROW_COUNT {name} {len(rows)}")
        bad = [k for k, v in rows.items() if "status" not in v]
        fails += [f"NO_ROW {name} {k}: {rows[k]['_raw']}" for k in bad]
    moved, lost, gained = [], [], []
    ma = mb = fa = fb = 0
    for k in sorted(set(a) & set(b)):
        ra, rb = a[k], b[k]
        if "status" not in ra or "status" not in rb:
            continue
        if (ra["status"], ra["minors"], ra["facts"]) != (rb["status"], rb["minors"], rb["facts"]):
            moved.append(k)
            print(f"MOVER {k}\n    A: {ra['_raw']}\n    B: {rb['_raw']}")
        if ra["status"] == "0" and rb["status"] != "0":
            lost.append(k)
        if ra["status"] != "0" and rb["status"] == "0":
            gained.append(k)
        if ra["status"] == "0" and rb["status"] == "0":
            ma += int(ra["minors"]); mb += int(rb["minors"]); fa += int(ra["facts"]); fb += int(rb["facts"])
    issue = [k for k in b if "I18_qp_m137" in k]
    if len(issue) != 4:
        fails.append(f"ISSUE_ROWS {len(issue)}")
    for k in issue:
        if b[k].get("status") != "0":
            fails.append(f"ISSUE_NOT_OPTIMAL {k}: {b[k]['_raw']}")
    fails += [f"LOST_OPTIMAL {k}" for k in lost]
    print(f"\nSUMMARY moved {len(moved)}; gained kOptimal {len(gained)} {gained}; lost kOptimal {len(lost)}")
    print(f"both-optimal minors {ma} -> {mb} ({100.0 * (mb - ma) / max(ma, 1):+.2f}%), "
          f"factorizations {fa} -> {fb} ({100.0 * (fb - fa) / max(fa, 1):+.2f}%)")
    if mb > 1.03 * ma or fb > 1.03 * fa:
        fails.append("COST_BOUND")
    if declared is not None:
        fails += [f"UNDECLARED_MOVER {k}" for k in sorted(set(moved) - declared)]
        fails += [f"DECLARED_MOVER_DID_NOT_MOVE {k}" for k in sorted(declared - set(moved))]
    for f in fails:
        print("  FAIL " + f)
    if declared is None:
        print("REPORT_ONLY")
        return 0 if not [f for f in fails if not f.startswith(("UNDECLARED", "DECLARED"))] else 1
    print("I18_CORPUS_PASS" if not fails else "I18_CORPUS_FAIL")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
