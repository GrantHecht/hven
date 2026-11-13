#!/usr/bin/env python3
"""M8 dense gate (../DECLARATION.md, criterion I4): the TRAIN lane, before (DN-3 r5's flip rows) and after (this code).

Per roster cell (DN-3's 238 TRAIN cells):
  I4K  the new `never-default` row (NEW/cells/<tag>.K.*, kNever explicit) equals r5's (R5/cells/<tag>.K.*): every
       key=value of the .out except `options_label` and `hven_pin_sha`, plus the lane exit code; integers and strings
       byte-equal; residual-class values byte-equal or within hven-bench docs/corun.md's calibrated gate
       |a-b| <= max(1e-12*max(|a|,|b|), 1e-13); the .optdump byte-equal. The change touches the dense arm only.
  I4D  the new `default` row (kAuto) vs r5's `default` row, the same equality: every cell that differs is in the
       declared mover list (MOVERS, one tag per line, '#' comments); a declared mover that does not move is reported.
A missing or capped row is a failure. Exit 0 iff I4K holds on all 238 cells and I4D's movers are all declared.

REPORTED, not gating -- the adoption tally, dense (D) vs sparse (K) outcome per cell, before (r5 D vs r5 K) and after
(new D vs new K): outcome = (reference_ok, converged), converged = status in {optimal, acceptable}; dense better /
same / worse by that pair; every cell whose D and K status differ is listed. And the dense-arm counter totals over the
SQP rows (side.dn3_dense_factorizations, side.dn1d_dense_flagged_factorizations, side.dn1d_dense_singular_refactors,
side.dn1d_dense_shifted_solve_refusals, side.dn3a_dense_solve_residual_refusals), before and after, per mover.

With a fifth argument DEV_DIR (the disclosed dev run), I4R: every new `default` row equals DEV_DIR's (the movers
reproduce). I4D then also requires every declared mover to move.

Usage: tally.py NEW_DIR R5_DIR ROSTER MOVERS [DEV_DIR]
"""
import math
import os
import sys

SKIP = {"options_label", "hven_pin_sha"}
GATE_REL, GATE_ABS = 1e-12, 1e-13
STRINGS = {"status", "outcome", "problem", "lane", "problem_set", "set_version", "source_path",
           "hessian_provenance"}
COUNTERS = ["side.dn3_dense_factorizations", "side.dn1d_dense_flagged_factorizations",
            "side.dn1d_dense_singular_refactors", "side.dn1d_dense_shifted_solve_refusals",
            "side.dn3a_dense_solve_residual_refusals"]
CONVERGED = {"optimal", "acceptable"}


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


def diffs(a, b):
    out = []
    for k in sorted(set(a) | set(b)):
        if k in SKIP:
            continue
        va, vb = a.get(k), b.get(k)
        if va == vb:
            continue
        if va is None or vb is None or k in STRINGS or k == "_cell_rc" or is_int(va) or is_int(vb):
            out.append((k, va, vb, "exact"))
            continue
        try:
            fa, fb = float(va), float(vb)
        except ValueError:
            out.append((k, va, vb, "exact"))
            continue
        if not (math.isfinite(fa) and math.isfinite(fb)):
            out.append((k, va, vb, "exact"))
            continue
        if abs(fa - fb) <= max(GATE_REL * max(abs(fa), abs(fb)), GATE_ABS):
            continue  # in gate
        out.append((k, va, vb, "residual"))
    return out


def outcome(r):
    return (r.get("reference_ok") == "1", r.get("status") in CONVERGED)


def tally(rows_d, rows_k, tags):
    better, worse, same, status_diff = [], [], [], []
    for t in tags:
        d, k = rows_d[t], rows_k[t]
        od, ok = outcome(d), outcome(k)
        (better if od > ok else worse if od < ok else same).append(t)
        if d.get("status") != k.get("status"):
            status_diff.append(f"{t}: D {d.get('status')}/{d.get('iterations')} K {k.get('status')}/{k.get('iterations')}")
    return better, worse, same, status_diff


def main():
    new, r5, roster, movers_file = sys.argv[1:5]
    dev = sys.argv[5] if len(sys.argv) > 5 else None
    declared = set()
    for line in open(movers_file):
        line = line.split("#", 1)[0].strip()
        if line:
            declared.add(line)
    tags = []
    for line in open(roster):
        pop, lane, prob = line.rstrip("\n").split("\t")
        tags.append(f"{pop}-{lane}-{prob}")
    ok = True
    rows = {("new", "D"): {}, ("new", "K"): {}, ("r5", "D"): {}, ("r5", "K"): {}}
    for t in tags:
        for side, base in (("new", new), ("r5", r5)):
            for arm in "DK":
                r, present = load(f"{base}/cells/{t}.{arm}")
                if not present:
                    print(f"MISSING_OR_CAPPED {side} {t}.{arm}")
                    ok = False
                rows[(side, arm)][t] = r
    if not ok:
        print("I4_FAIL (rows)")
        return 1
    # I4K
    k_bad = 0
    for t in tags:
        d = diffs(rows[("r5", "K")][t], rows[("new", "K")][t])
        dump_a = open(f"{r5}/cells/{t}.K.optdump", "rb").read()
        dump_b = open(f"{new}/cells/{t}.K.optdump", "rb").read()
        if d or dump_a != dump_b:
            k_bad += 1
            print(f"I4K_DIFF {t}: {d[:6]} optdump_equal={dump_a == dump_b}")
    print(f"I4K (kNever rows vs r5): {len(tags) - k_bad}/{len(tags)} equal")
    # I4D
    moved = []
    for t in tags:
        d = diffs(rows[("r5", "D")][t], rows[("new", "D")][t])
        dump_a = open(f"{r5}/cells/{t}.D.optdump", "rb").read()
        dump_b = open(f"{new}/cells/{t}.D.optdump", "rb").read()
        if dump_a != dump_b:
            print(f"I4D_OPTDUMP_DIFF {t}")
            k_bad += 1
        if d:
            moved.append(t)
            keys = ", ".join(f"{k} {a}->{b}" for k, a, b, _ in d if k in COUNTERS or k in
                             ("status", "iterations", "objective", "reference_ok", "_cell_rc"))
            print(f"I4D_MOVER {t}{' (declared)' if t in declared else ' UNDECLARED'}: {len(d)} keys; {keys}")
    undeclared = [t for t in moved if t not in declared]
    unmoved = sorted(declared - set(moved))
    print(f"I4D (kAuto rows vs r5): {len(moved)} movers, {len(undeclared)} undeclared; declared but unmoved: "
          f"{len(unmoved)} {unmoved}")
    r_bad = 0
    if dev is not None:
        if unmoved:
            k_bad += 1
        for t in tags:
            rd, present = load(f"{dev}/cells/{t}.D")
            d = diffs(rd, rows[("new", "D")][t]) if present else [("missing", None, None, "exact")]
            if d:
                r_bad += 1
                print(f"I4R_DIFF {t}: {d[:6]}")
        print(f"I4R (kAuto rows vs the dev run): {len(tags) - r_bad}/{len(tags)} equal")
        k_bad += r_bad
    # tallies (reported)
    for side in ("r5", "new"):
        b, w, s, sd = tally(rows[(side, "D")], rows[(side, "K")], tags)
        name = "BEFORE (r5 flip)" if side == "r5" else "AFTER (dense gate)"
        print(f"TALLY {name}: dense better {len(b)}, same {len(s)}, dense worse {len(w)}")
        print(f"  better: {b}")
        print(f"  worse: {w}")
        for line in sd:
            print(f"  status differs  {line}")
        tot = {c: sum(int(rows[(side, 'D')][t].get(c, '0') or 0) for t in tags) for c in COUNTERS}
        print(f"  D-arm counter totals: {tot}")
    for t in moved:
        before = {c.split('.')[-1]: rows[("r5", "D")][t].get(c, "-") for c in COUNTERS}
        after = {c.split('.')[-1]: rows[("new", "D")][t].get(c, "-") for c in COUNTERS}
        print(f"MOVER_COUNTERS {t}: before {before} after {after}")
    if k_bad == 0 and not undeclared:
        print("I4_PASS")
        return 0
    print("I4_FAIL")
    return 1


if __name__ == "__main__":
    sys.exit(main())
