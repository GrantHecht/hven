#!/usr/bin/env python3
"""TU-0 TRAIN identity check: the TU-0 code (arm B) against the declared base (arm A) on DN-3's 238 TRAIN cells x {D = `default`, K = `never-default`}.

Row equality is wa1's train_check.py (itself flip_identity.py's): every key=value line of the .out except `options_label` and
`hven_pin_sha`, plus the lane exit code; integer and string values byte-equal; residual-class values byte-equal or within
hven-bench docs/corun.md's calibrated gate (rel 1e-12, abs 1e-13); a missing or capped row fails.
ONE declared difference (DECLARATION-TU-0.md section 3): the .optdump of a hven-sqp row gains nine lines (the TU-0 options at their
defaults); B's optdump with those nine lines removed must equal A's byte for byte, and an ipm row's optdump must be byte-equal.
A further class, declared only by naming the rows in MOVERS (DECLARATION-TU-0.md section 3 declares none): a row may differ in the ledger-derived harness columns alone
(`side.harness_qp_ledger_*`, `side.harness_qpsum_*`: the lane sums the attached Ledger's records, and the restoration phase's QPs are now ledgered); a row whose
differences reach any other key is an OTHER mover and fails whether declared or not.
Exit 0 iff no row is missing/capped, no OTHER mover, and the LEDGER movers are exactly MOVERS (one `<tag>.<arm>` per line).

Usage: train_check.py B_DIR A_DIR DN3_DIR MOVERS.txt
"""
import math
import os
import sys

SKIP = {"options_label", "hven_pin_sha"}
NEW_OPTION_KEYS = ("row_scaling_power_of_two", "advanced.trust_region_growth", "advanced.ssn_farkas_residual_tol", "advanced.ssn_uncertain_tol",
                   "advanced.ssn_lm_sigma_c", "advanced.ssn_lm_sigma_exponent", "advanced.restoration_elastic_seed",
                   "advanced.elastic_rho0_headroom_rungs", "advanced.adaptive_dual_regularization_max")
GATE_REL, GATE_ABS = 1e-12, 1e-13


def load(prefix):
    row, present = {}, False
    res = prefix + ".result"
    capped = os.path.exists(res) and "CAPPED" in open(res).read()
    if os.path.exists(res):
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


def optdump_new_lines_removed(text):
    return "".join(l for l in text.splitlines(True) if not any(l.startswith(k + " ") or l.startswith(k + "=") for k in NEW_OPTION_KEYS))


def row_equal(pn, po, sqp):
    new, okn = load(pn)
    old, oko = load(po)
    if not (okn and oko):
        return None, new, old, [f"NO_ROW_OR_CAPPED (new {okn}, ref {oko})"], []
    ex, ig, og = compare(new, old)
    dn = open(pn + ".optdump").read() if os.path.exists(pn + ".optdump") else None
    do = open(po + ".optdump").read() if os.path.exists(po + ".optdump") else None
    if dn is None or do is None:
        ex.append("OPTDUMP_MISSING")
    elif sqp:
        added = len(dn.splitlines()) - len(do.splitlines())
        if added != len(NEW_OPTION_KEYS) or optdump_new_lines_removed(dn) != do:
            ex.append(f"OPTDUMP_DIFFERS_BEYOND_THE_NINE_NEW_LINES (lines added {added})")
    elif dn != do:
        ex.append("OPTDUMP_DIFFERS")
    return not (ex or og), new, old, ex + og, ig


def main():
    if len(sys.argv) != 5:
        print(__doc__)
        return 2
    bdir, adir, d3, movf = sys.argv[1:5]
    declared = set()
    for line in open(movf):
        line = line.split("#", 1)[0].strip()
        if line:
            declared.add(line)
    roster = [l.rstrip("\n").split("\t") for l in open(os.path.join(d3, "roster.tsv")) if l.strip()]
    if len(roster) != 238:
        print(f"ROSTER_COUNT {len(roster)}")
        return 4
    movers, missing, ingate_all, other = [], [], [], []
    for pop, lane, prob in roster:
        tag = f"{pop}-{lane}-{prob}"
        for arm in ("D", "K"):
            eq, new, old, diffs, ig = row_equal(os.path.join(bdir, "cells", f"{tag}.{arm}"),
                                                os.path.join(adir, "cells", f"{tag}.{arm}"), lane == "hven-sqp")
            ingate_all += [f"{tag}.{arm} {x}" for x in ig]
            if eq is None:
                missing.append(f"{tag}.{arm}: {diffs[0]}")
                continue
            if not eq:
                keys = [d.split(":")[0] for d in diffs]
                ledger_only = all(k.startswith("side.harness_qp") for k in keys)
                movers.append(f"{tag}.{arm}")
                if not ledger_only:
                    other.append(f"{tag}.{arm}")
                print(f"MOVER {'LEDGER' if ledger_only else 'OTHER'} {tag}.{arm}: {'; '.join(diffs[:6])}")
    print()
    print(f"SUMMARY rows {2 * len(roster)}; movers vs arm A {len(movers)}; missing/capped {len(missing)}")
    print(f"residual values equal within the gate but not byte-equal: {len(ingate_all)}")
    for x in ingate_all:
        print("  INGATE " + x)
    for x in missing:
        print("  FAIL MISSING " + x)
    und = sorted(set(movers) - declared)
    absent = sorted(declared - set(movers))
    for x in und:
        print("  FAIL UNDECLARED_MOVER " + x)
    for x in absent:
        print("  FAIL DECLARED_MOVER_DID_NOT_MOVE " + x)
    for x in other:
        print("  FAIL OTHER_MOVER " + x)
    ok = not missing and not und and not absent and not other
    print("TU0_TRAIN_PASS" if ok else "TU0_TRAIN_FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
