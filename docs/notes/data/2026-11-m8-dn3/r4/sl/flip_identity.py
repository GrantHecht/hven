#!/usr/bin/env python3
"""M8 DN-3 r4 (../../DECLARATION-r4.md, criterion I4): the flip's counter identity on DN-3's 238 TRAIN cells.

Per roster cell (roster order, ../../roster.tsv):
  I4a  the flip binary's `default` row (F/cells/<tag>.D.*: the lane sets no dense policy, so the library default reads)
       equals DN-3's `auto-default` row (../../enum/cells/<tag>.A.*, the implementation head 96ac1a27d at kAuto);
  I4b  the flip binary's `never-default` row (F/cells/<tag>.K.*: kNever set explicitly) equals DN-3's `default` row
       (../../enum/cells/<tag>.N.*, the implementation head at its then-default kNever).
Equality: every key=value line of the .out, except `options_label` and `hven_pin_sha` (they name the label and the
binary's pin by construction), plus the lane's exit code from the .result line (`_cell_rc`, exact):
  EXACT keys (every value that parses as an integer, plus status / outcome / problem / lane identity strings): byte-equal;
  RESIDUAL-class keys (every other value: objective, kkt_*, side.ipm_*_inf, reference_abs_err, ...): byte-equal, or
  within hven-bench docs/corun.md's calibrated gate |a-b| <= max(1e-12*max(|a|,|b|), 1e-13) (CLAUDE.md s7: MKL is
  address-sensitive across processes); every in-gate non-byte-equal value is listed. Non-finite values byte-equal only.
The .optdump of each row is byte-equal to its DN-3 counterpart (the same options reached the solver).
A missing / capped row on either side is a failure. Exit 0 iff I4a and I4b hold on all 238 cells.

Usage: flip_identity.py FLIP_DIR DN3_DIR
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


def main():
    fdir, ddir = sys.argv[1], sys.argv[2]
    roster = [l.rstrip("\n").split("\t") for l in open(os.path.join(ddir, "roster.tsv")) if l.strip()]
    if len(roster) != 238:
        print(f"ROSTER_COUNT {len(roster)}")
        return 4
    fails = {"I4a": [], "I4b": []}
    ingate_all = []
    counts = {"I4a": 0, "I4b": 0}
    dense_rows = 0
    for pop, lane, prob in roster:
        tag = f"{pop}-{lane}-{prob}"
        for crit, mine, theirs in (("I4a", "D", "A"), ("I4b", "K", "N")):
            pn = os.path.join(fdir, "cells", f"{tag}.{mine}")
            po = os.path.join(ddir, "enum", "cells", f"{tag}.{theirs}")
            new, okn = load(pn)
            old, oko = load(po)
            if not (okn and oko):
                fails[crit].append(f"{tag}: NO_ROW_OR_CAPPED (flip {okn}, dn3 {oko})")
                continue
            ex, ig, og = compare(new, old)
            dn = open(pn + ".optdump").read() if os.path.exists(pn + ".optdump") else None
            do = open(po + ".optdump").read() if os.path.exists(po + ".optdump") else None
            if dn is None or dn != do:
                ex.append("OPTDUMP_DIFFERS")
            verdict = "EQUAL"
            if ex or og:
                fails[crit].append(f"{tag}: " + "; ".join((ex + og)[:8]))
                verdict = "DIFFERS"
            else:
                counts[crit] += 1
            ingate_all += [f"{crit} {tag} {x}" for x in ig]
            dense = int(new.get("side.dn3_dense_factorizations", "0"))
            if crit == "I4a" and dense > 0:
                dense_rows += 1
            print(f"{crit} {tag:44s} {new.get('status', '-'):16s} it {new.get('iterations', '-'):>5s} "
                  f"fac {new.get('factorizations', '-'):>6s} dense {dense:>6d} {verdict}")
    print()
    print(f"I4a (flip default == DN-3 auto-default): {counts['I4a']} of 238 equal; flip-default rows with dense "
          f"factorizations > 0: {dense_rows}")
    print(f"I4b (flip never-default == DN-3 default): {counts['I4b']} of 238 equal")
    print(f"residual values equal within the gate but not byte-equal: {len(ingate_all)}")
    for x in ingate_all:
        print("  INGATE " + x)
    for crit in ("I4a", "I4b"):
        for x in fails[crit]:
            print(f"  FAIL {crit} {x}")
    ok = not fails["I4a"] and not fails["I4b"]
    print("I4_PASS" if ok else "I4_FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
