#!/usr/bin/env python3
"""M8 walk cap (../DECLARATION.md, criterion I4): the fix's TRAIN identity and mover check on DN-3's 238 TRAIN cells.

Row equality is ../../2026-11-m8-dn3/r4/sl/flip_identity.py's, unchanged (compare() and load() are copied byte for
byte): every key=value line of the .out except `options_label` and `hven_pin_sha`, plus the lane exit code; integer and
string values byte-equal; residual-class values byte-equal or within hven-bench docs/corun.md's calibrated gate (rel
1e-12, abs 1e-13; in-gate non-byte-equal values listed); the .optdump byte-equal; a missing or capped row fails.

Per roster cell and arm (D = `default`, the library default kAuto; K = `never-default`, kNever):
  BASE  the row vs DN-3 r5's row of the same arm (../../2026-11-m8-dn3/r5/identity/cells/<tag>.<arm>, code
        030bae675's library): a differing row is a MOVER, printed with its direction (status rank optimal >
        acceptable > anything else; same rank: neutral, with iterations, objective and the reference error shown).
  DEV   (when DEV and MOVERS are given) the row vs the declared dev run's row (DEV/cells/<tag>.<arm>): must be EQUAL,
        and the mover set must equal MOVERS (one `<tag>.<arm>` per line, '#' comments) exactly.
Without DEV: report mode, exit 0 when every row is present. With DEV: exit 0 iff every row equals its dev row and the
mover set is exactly the declared one (I4_PASS), else 1.

Usage: train_check.py NEW_DIR DN3_DIR [DEV_DIR MOVERS.txt]
"""
import math
import os
import sys

SKIP = {"options_label", "hven_pin_sha"}
GATE_REL, GATE_ABS = 1e-12, 1e-13


def load(prefix):
    row, present = {}, False
    res = prefix + ".result"
    capped = os.path.exists(res) and "CAPPED" in open(res).read()
    if os.path.exists(res):  # the lane's exit code (cell.sh's CELL_RESULT rc=), compared as an exact key
        for tok in open(res).read().split():
            if tok.startswith("rc="):
                row["_cell_rc"] = tok[3:]
    if os.path.exists(prefix + ".out"):
        for line in open(prefix + ".out"):
            line = line.rstrip("\n")
            if "=" in line:
                k, v = line.split("=", 1)
                row[k] = v
                present = True
    return row, present and not capped and "status" in row


def is_int(v):
    try:
        int(v)
        return True
    except ValueError:
        return False


def as_float(v):
    try:
        return float(v)
    except ValueError:
        return None


def compare(new, old):
    """-> (exact differences, in-gate residual differences, out-of-gate residual differences)"""
    exact, ingate, outgate = [], [], []
    for k in sorted((set(new) | set(old)) - SKIP):
        a, b = new.get(k), old.get(k)
        if a == b:
            continue
        if a is None or b is None or is_int(a) or is_int(b):
            exact.append(f"{k}: {b} -> {a}")
            continue
        fa, fb = as_float(a), as_float(b)
        if fa is None or fb is None:
            exact.append(f"{k}: {b} -> {a}")
            continue
        if not (math.isfinite(fa) and math.isfinite(fb)):
            outgate.append(f"{k}: {b} -> {a} (non-finite)")
            continue
        if abs(fa - fb) <= max(GATE_REL * max(abs(fa), abs(fb)), GATE_ABS):
            ingate.append(f"{k}: {b} -> {a}")
        else:
            outgate.append(f"{k}: {b} -> {a}")
    return exact, ingate, outgate


def row_equal(pn, po):
    new, okn = load(pn)
    old, oko = load(po)
    if not (okn and oko):
        return None, new, old, [f"NO_ROW_OR_CAPPED (new {okn}, ref {oko})"], []
    ex, ig, og = compare(new, old)
    dn = open(pn + ".optdump").read() if os.path.exists(pn + ".optdump") else None
    do = open(po + ".optdump").read() if os.path.exists(po + ".optdump") else None
    if dn is None or dn != do:
        ex.append("OPTDUMP_DIFFERS")
    return not (ex or og), new, old, ex + og, ig


RANK = {"optimal": 2, "acceptable": 1}


def direction(new, old):
    rn, ro = RANK.get(new.get("status"), 0), RANK.get(old.get("status"), 0)
    if rn > ro:
        return "BETTER"
    if rn < ro:
        return "WORSE"
    return "NEUTRAL"


def summary(r):
    keys = ("status", "iterations", "objective", "reference_abs_err", "side.elastic_activations",
            "side.restoration_iters", "side.qp_minor_iters")
    return " ".join(f"{k.replace('side.', '')}={r.get(k, '-')}" for k in keys)


def main():
    if len(sys.argv) not in (3, 5):
        print(__doc__)
        return 2
    ndir, d3 = sys.argv[1], sys.argv[2]
    dev = sys.argv[3] if len(sys.argv) == 5 else None
    declared = set()
    if dev:
        for line in open(sys.argv[4]):
            line = line.split("#", 1)[0].strip()
            if line:
                declared.add(line)
    roster = [l.rstrip("\n").split("\t") for l in open(os.path.join(d3, "roster.tsv")) if l.strip()]
    if len(roster) != 238:
        print(f"ROSTER_COUNT {len(roster)}")
        return 4
    movers, missing, dev_fail, ingate_all = [], [], [], []
    tally = {"BETTER": 0, "WORSE": 0, "NEUTRAL": 0}
    for pop, lane, prob in roster:
        tag = f"{pop}-{lane}-{prob}"
        for arm in ("D", "K"):
            pn = os.path.join(ndir, "cells", f"{tag}.{arm}")
            po = os.path.join(d3, "r5", "identity", "cells", f"{tag}.{arm}")
            eq, new, old, diffs, ig = row_equal(pn, po)
            ingate_all += [f"BASE {tag}.{arm} {x}" for x in ig]
            if eq is None:
                missing.append(f"{tag}.{arm}: {diffs[0]}")
                continue
            if not eq:
                d = direction(new, old)
                tally[d] += 1
                movers.append(f"{tag}.{arm}")
                print(f"MOVER {d:8s} {tag}.{arm}")
                print(f"    base: {summary(old)}")
                print(f"    new:  {summary(new)}")
                print(f"    diff: {'; '.join(diffs[:12])}")
            if dev:
                deq, _, _, ddiffs, dig = row_equal(pn, os.path.join(dev, "cells", f"{tag}.{arm}"))
                ingate_all += [f"DEV {tag}.{arm} {x}" for x in dig]
                if not deq:
                    dev_fail.append(f"{tag}.{arm}: " + "; ".join(ddiffs[:8]))
    print()
    print(f"SUMMARY rows {2 * len(roster)}; movers vs DN-3 r5 {len(movers)} (better {tally['BETTER']}, worse "
          f"{tally['WORSE']}, neutral {tally['NEUTRAL']}); missing/capped {len(missing)}")
    print(f"residual values equal within the gate but not byte-equal: {len(ingate_all)}")
    for x in ingate_all:
        print("  INGATE " + x)
    for x in missing:
        print("  FAIL MISSING " + x)
    ok = not missing
    if dev:
        und = sorted(set(movers) - declared)
        absent = sorted(declared - set(movers))
        for x in und:
            print("  FAIL UNDECLARED_MOVER " + x)
        for x in absent:
            print("  FAIL DECLARED_MOVER_DID_NOT_MOVE " + x)
        for x in dev_fail:
            print("  FAIL DEV_ROW_DIFFERS " + x)
        ok = ok and not und and not absent and not dev_fail
        print("I4_PASS" if ok else "I4_FAIL")
    else:
        print("REPORT_ONLY (dev run)" if ok else "REPORT_ONLY_WITH_MISSING_ROWS")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
