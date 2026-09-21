#!/usr/bin/env python3
"""M7 W2 M3a ADDENDUM E -- RULE-E.md section 3's IDENTITY CHECK, and nothing else.

The probe gained two arguments and five printed keys. Both arguments default to
the SHIPPED value (`qp_cap_retry` off, `qp.max_iter` the size-derived sentinel),
so a leg passing NEITHER is stage 1's own solve. This script proves it on one
control before any E leg runs.

  in   .scratch/m7w2m3a/out/e/STEENBRB@identity.txt   the rebuilt probe, defaults
       docs/notes/data/.../rows/stage1/STEENBRB@trinf.txt   the committed control
  out  stdout, and exit 0 (IDENTICAL) or 1 (a STOP)

MACHINE-WRITTEN: no number this produces is transcribed by hand. The exclusion
lists below are RULE-E.md section 3's, BY NAME, and a key absent from them is
compared whatever it is. The near-ulp fallback is RULE.md section 4's, carried
over verbatim.

An identity failure ABORTS the chain: it would mean the two new arguments are
not inert at their defaults, and no E row taken with that binary could be
believed.
"""
import os
import sys

ROOT = "/home/ghecht/Projects/hven"
ART = os.path.join(ROOT, "docs/notes/data/2026-09-m7-w2-m3a-request-probe")
NEW = os.path.join(ROOT, ".scratch/m7w2m3a/out/e")
OLD = os.path.join(ART, "rows/stage1")

# RULE-E.md section 3: the identity leg is STEENBRB at tr_init = inf with both
# new options at their defaults, against that cell's committed stage-1 @trinf row.
PAIRS = [("STEENBRB", "STEENBRB@identity.txt", "STEENBRB@trinf.txt")]

# RULE-E.md section 3: THIS ADDENDUM's new keys, excluded BY NAME.
NEW_KEYS = {
    "qp_cap_retry",
    "qp_max_iter",
    "qp_cap_retries",
    "first_qp_minor_iters",
    "first_qp_factorizations",
}
# RULE.md section 4's TIME-VALUED list, carried over unchanged and total. None is
# printed by either binary today; the list is stated in full so a future key of
# this kind is excluded by a committed rule rather than a later judgement call.
TIME_KEYS = {"wall_seconds", "elapsed", "elapsed_seconds"}

# RULE.md section 4's residual class -- the ONLY keys and columns a near-ulp gate
# may touch. Counters, statuses, integer- and enum-valued keys and x_k_source are
# byte equality, no tolerance, ever.
RESIDUAL_KEYS = {
    "f", "stationarity", "feasibility_e", "feasibility_i", "complementarity",
    "restoration_h_in", "restoration_h_out", "restoration_f_in",
    "restoration_f_out", "requesting_h", "requesting_f",
}
RESIDUAL_COLS = {"f_event", "radius", "step_norm", "f_main", "h_main",
                 "dx_inf", "prox_rel"}
NEAR_ULP_REL = 1.0e-12

LADDER_COLS = ["depth", "event_iteration", "f_event", "radius", "step_norm",
               "f_main", "h_main", "dx_inf", "prox_rel"]


def read_probe(path):
    """identity_m3a.py's reader, unchanged."""
    kv, rows, in_ladder, header = {}, [], False, None
    if not os.path.exists(path):
        return None, None, None
    for line in open(path):
        line = line.rstrip("\n")
        if line == "LADDER_BEGIN":
            in_ladder = True
            continue
        if line == "LADDER_END":
            in_ladder = False
            continue
        if in_ladder:
            if header is None:
                header = line.split(",")
            else:
                rows.append(dict(zip(header, line.split(","))))
            continue
        if "=" in line and not line.startswith(("PROBE_", "#")):
            k, _, v = line.partition("=")
            kv[k] = v
    return kv, rows, header


def near_ulp(a, b):
    """RULE.md section 4's declared fallback, unchanged."""
    try:
        x, y = float(a), float(b)
    except (TypeError, ValueError):
        return False, None
    if x != x or y != y:          # NaN == NaN is a byte fact, not a numeric one
        return False, None
    if x in (float("inf"), float("-inf")) or y in (float("inf"), float("-inf")):
        return False, None
    gap = abs(x - y)
    bar = NEAR_ULP_REL * max(1.0, abs(x), abs(y))
    return gap <= bar, gap


def main():
    worst = 0
    tolerated = 0
    print("M7 W2 M3a ADDENDUM E -- IDENTITY: the REBUILT probe at BOTH new options'")
    print("defaults vs the committed stage-1 @trinf row. RULE-E.md section 3.")
    print("A DIFFERENCE IS A STOP, and a stop ABORTS the chain before any E leg runs.")
    print("")
    print(f"excluded by name (this addendum's new keys): {sorted(NEW_KEYS)}")
    print(f"dropped by name (time-valued):               {sorted(TIME_KEYS)}")
    print(f"near-ulp gate, residual class only:          |a-b| <= {NEAR_ULP_REL:g} * max(1,|a|,|b|)")
    print("")
    for cell, new_name, old_name in PAIRS:
        kv_new, rows_new, hdr_new = read_probe(os.path.join(NEW, new_name))
        kv_old, rows_old, hdr_old = read_probe(os.path.join(OLD, old_name))
        if kv_new is None or kv_old is None:
            print(f"{cell:10s} MISSING LEG -- new={kv_new is not None} stage1={kv_old is not None}")
            worst = 1
            continue

        shared = (set(kv_new) & set(kv_old)) - NEW_KEYS - TIME_KEYS
        only_new = sorted(set(kv_new) - set(kv_old))
        only_old = sorted(set(kv_old) - set(kv_new))
        diffs = []
        for k in sorted(shared):
            a, b = kv_new[k], kv_old[k]
            if a == b:
                continue
            if k in RESIDUAL_KEYS:
                ok, gap = near_ulp(a, b)
                if ok:
                    tolerated += 1
                    print(f"{cell:10s} NEAR-ULP ACCEPTED key {k}: {a} vs {b}  gap={gap:.3e}")
                    continue
            diffs.append((k, a, b))

        cols = [c for c in LADDER_COLS if c in (hdr_new or []) and c in (hdr_old or [])]
        row_diffs = []
        if len(rows_new) != len(rows_old):
            row_diffs.append(("<row count>", str(len(rows_new)), str(len(rows_old))))
        else:
            for i, (ra, rb) in enumerate(zip(rows_new, rows_old)):
                for c in cols:
                    a, b = ra.get(c), rb.get(c)
                    if a == b:
                        continue
                    if c in RESIDUAL_COLS:
                        ok, gap = near_ulp(a, b)
                        if ok:
                            tolerated += 1
                            print(f"{cell:10s} NEAR-ULP ACCEPTED row {i} col {c}: "
                                  f"{a} vs {b}  gap={gap:.3e}")
                            continue
                    row_diffs.append((f"row {i} col {c}", a, b))

        verdict = "IDENTICAL" if not diffs and not row_diffs else "DIFFERS -- STOP"
        print(f"{cell:10s} keys {len(shared):3d} compared, {len(diffs):3d} differing; "
              f"rows {len(rows_new):3d} x {len(cols)} columns, {len(row_diffs):3d} differing   "
              f"{verdict}")
        if only_new:
            print(f"{cell:10s}   keys only in the rebuilt probe (expected = the new ones): {only_new}")
        if only_old:
            print(f"{cell:10s}   keys only in the stage-1 row: {only_old}")
        for k, a, b in diffs + row_diffs:
            print(f"{cell:10s}   DIFF {k}: new={a}  stage1={b}")
        if diffs or row_diffs:
            worst = 1

    print("")
    print(f"NEAR_ULP_TOLERATED={tolerated}")
    print(f"IDENTITY_E_WORST={worst}")
    return worst


if __name__ == "__main__":
    sys.exit(main())
