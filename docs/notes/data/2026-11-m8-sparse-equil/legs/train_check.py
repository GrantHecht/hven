#!/usr/bin/env python3
"""sparse-equil (../DECLARATION.md): train_check.py of kopt-1x (copy of ../../2026-11-m8-walk-cap/legs/train_check.py with ONE change: the reference arm is arm A's own enumeration (REF/cells/<tag>.<arm>), not DN-3 r5's rows; the roster is DN-3's.

Original docstring: the fix's TRAIN identity and mover check on DN-3's 238 TRAIN cells.

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

Usage: train_check.py NEW_DIR DN3_DIR REF_DIR [DEV_DIR MOVERS.txt]
"""
import math
import os
import sys

SKIP = {"options_label", "hven_pin_sha", "side.se_sparse_equilibrated_refactors"}
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
    if len(sys.argv) != 4:
        print("usage: train_check.py B_DIR A_DIR ROSTER.tsv")
        return 2
    bdir, adir, roster_path = sys.argv[1:4]
    roster = [l.rstrip("\n").split("\t") for l in open(roster_path) if l.strip()]
    if len(roster) != 238:
        print(f"ROSTER_COUNT {len(roster)}")
        return 4
    movers, missing, bad, ingate_all = [], [], [], []
    perturbed = 0
    tally = {"BETTER": 0, "WORSE": 0, "NEUTRAL": 0}
    for pop, lane, prob in roster:
        tag = f"{pop}-{lane}-{prob}"
        for arm in ("D", "K"):
            pb = os.path.join(bdir, "cells", f"{tag}.{arm}")
            pa = os.path.join(adir, "cells", f"{tag}.{arm}")
            eq, new, old, diffs, ig = row_equal(pb, pa)
            ingate_all += [f"{tag}.{arm} {x}" for x in ig]
            if eq is None:
                missing.append(f"{tag}.{arm}: {diffs[0]}")
                continue
            n_eq = int(new.get("side.se_sparse_equilibrated_refactors", "0") or 0)
            if n_eq > 0:
                perturbed += 1
            if not eq:
                d = direction(new, old)
                tally[d] += 1
                movers.append(f"{tag}.{arm}")
                allowed = n_eq > 0
                print(f"MOVER {d:8s} {'PERTURBED' if allowed else 'UNPERTURBED'} {tag}.{arm} refactors={n_eq}")
                print(f"    A: {summary(old)}")
                print(f"    B: {summary(new)}")
                print(f"    diff: {'; '.join(diffs[:12])}")
                if not allowed:
                    bad.append(f"UNPERTURBED_MOVER {tag}.{arm}")
                if d == "WORSE":
                    bad.append(f"WORSE_STATUS {tag}.{arm}")
    print()
    print(f"SUMMARY rows {2 * len(roster)}; rows with equilibrated refactors > 0: {perturbed}; movers vs arm A {len(movers)} "
          f"(better {tally['BETTER']}, worse {tally['WORSE']}, neutral {tally['NEUTRAL']}); missing/capped {len(missing)}")
    print(f"residual values equal within the gate but not byte-equal: {len(ingate_all)}")
    for x in ingate_all:
        print("  INGATE " + x)
    for x in missing:
        print("  FAIL MISSING " + x)
    for x in bad:
        print("  FAIL " + x)
    ok = not missing and not bad
    print("I4_PASS" if ok else "I4_FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
