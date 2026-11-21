#!/usr/bin/env python3
"""I18 nlp-35 check: arm B rows vs arm A rows (nlp35.sh), row equality and direction as train_check.py (imported).
I18_NLP35_PASS iff all 42 rows present and uncapped in both arms, no WORSE mover, and the movers are exactly MOVERS
(one `<lane>-<problem>` per line, '#' comments).  usage: nlp35_check.py B_DIR A_DIR MOVERS.txt   (MOVERS '-' = report only)"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import train_check as tc  # noqa: E402


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    bdir, adir = sys.argv[1], sys.argv[2]
    cells = [l.strip().split(",") for l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "nlp35-cells.csv")) if l.strip()]
    declared = None
    if sys.argv[3] != "-":
        declared = {l.split("#", 1)[0].strip() for l in open(sys.argv[3])} - {""}
    movers, missing, worse = [], [], []
    for lane, prob, _mode in cells:
        tag = f"{lane}-{prob}"
        eq, new, old, diffs, _ig = tc.row_equal(os.path.join(bdir, "cells", tag), os.path.join(adir, "cells", tag))
        if eq is None:
            missing.append(f"{tag}: {diffs[0]}")
            continue
        if not eq:
            d = tc.direction(new, old)
            movers.append(tag)
            if d == "WORSE":
                worse.append(tag)
            print(f"MOVER {d:8s} {tag}\n    A: {tc.summary(old)}\n    B: {tc.summary(new)}\n    diff: {'; '.join(diffs[:12])}")
    print(f"\nSUMMARY rows {len(cells)}; movers {len(movers)}; worse {len(worse)}; missing/capped {len(missing)}")
    fails = [f"MISSING {m}" for m in missing] + [f"WORSE_MOVER {w}" for w in worse]
    if declared is not None:
        fails += [f"UNDECLARED_MOVER {m}" for m in sorted(set(movers) - declared)]
        fails += [f"DECLARED_MOVER_DID_NOT_MOVE {m}" for m in sorted(declared - set(movers))]
    for f in fails:
        print("  FAIL " + f)
    if declared is None:
        print("REPORT_ONLY")
        return 0 if not missing else 1
    print("I18_NLP35_PASS" if not fails else "I18_NLP35_FAIL")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
