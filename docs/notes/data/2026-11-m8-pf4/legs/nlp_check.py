#!/usr/bin/env python3
"""PF-4 P4b check over nlp_instr.sh's rows (train_check.py's row equality; NEW_KEYS skipped where arm A lacks them).
usage: nlp_check.py <evidence-dir> [<dev evidence-dir> <movers file>]
Report mode (one argument): every A-vs-B row difference printed with its direction. Gate mode: every B row equals its
dev B row (the gate's arm A rows are also compared with the dev arm A rows: arm A is unchanged code, so they must be
equal), the set of A-vs-B movers equals the declared set exactly, and no mover is WORSE (status rank, train_check.py's).
-> NLP_AS_DECLARED / NLP_FAIL (gate mode), NLP_REPORT (report mode)."""
import glob, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import train_check as T

ev = sys.argv[1]
dev = sys.argv[2] if len(sys.argv) == 4 else None
declared = set()
if dev:
    for line in open(sys.argv[3]):
        line = line.split("#", 1)[0].strip()
        if line:
            declared.add(line)
movers, fails, n = [], [], 0
for f in sorted(glob.glob(os.path.join(ev, "B", "*.out"))):
    name = os.path.basename(f)[:-4]
    n += 1
    eq, new, old, diffs, ig = T.row_equal(os.path.join(ev, "B", name), os.path.join(ev, "A", name))
    if eq is None:
        fails.append(f"{name}: {diffs[0]}")
        continue
    if not eq:
        d = T.direction(new, old)
        movers.append(name)
        print(f"MOVER {d:8s} {name}")
        print(f"    A: {T.summary(old)}")
        print(f"    B: {T.summary(new)}")
        print(f"    diff: {'; '.join(diffs[:10])}")
        if d == "WORSE":
            fails.append(f"WORSE {name}")
    if dev:
        for arm in ("A", "B"):
            deq, _, _, dd, _ = T.row_equal(os.path.join(ev, arm, name), os.path.join(dev, arm, name))
            if not deq:
                fails.append(f"DEV_ROW_DIFFERS {arm} {name}: {'; '.join(dd[:6])}")
print(f"rows {n}; movers A vs B {len(movers)}")
if dev:
    for x in sorted(set(movers) - declared):
        fails.append("UNDECLARED_MOVER " + x)
    for x in sorted(declared - set(movers)):
        fails.append("DECLARED_MOVER_DID_NOT_MOVE " + x)
if n != 15:
    fails.append(f"ROW_COUNT {n}")
for x in fails:
    print("  FAIL " + x)
if dev:
    print("NLP_AS_DECLARED" if not fails else "NLP_FAIL")
    sys.exit(0 if not fails else 1)
print("NLP_REPORT" + (" (with missing/WORSE rows)" if fails else ""))
