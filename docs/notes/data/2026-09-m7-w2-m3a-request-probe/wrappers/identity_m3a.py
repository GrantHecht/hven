#!/usr/bin/env python3
"""M7 W2 M3a -- RULE.md section 4's CONTROL-IDENTITY CHECK, and nothing else.

The control legs run at tr_init = 1 and zeta = 0 -- both shipped defaults -- so
they are M2b's screen legs re-run on a library that has gained one counters
field and a probe that has gained one argument and thirteen printed keys. They
must reproduce M2b's committed screen rows.

MACHINE-WRITTEN: no number this produces is transcribed by hand.

  in   .scratch/m7w2m3a/out/stage1/<CELL>@tr1.txt        this task's control leg
       docs/notes/data/2026-09-m7-w2-m2b-proximity/rows/screen/<CELL>.txt
  out  stdout, and exit 0 (IDENTICAL on every cell) or 1 (a STOP)

The rule is RULE.md's, committed at dbf55675 before any leg ran; this script
implements section 4 and makes no judgement of its own. In particular the
exclusion lists below are RULE.md's, BY NAME, and a key absent from them is
compared whatever it is.
"""
import os
import sys

ROOT = "/home/ghecht/Projects/hven"
NEW = os.path.join(ROOT, ".scratch/m7w2m3a/out/stage1")
M2B = os.path.join(ROOT, "docs/notes/data/2026-09-m7-w2-m2b-proximity/rows/screen")

CELLS = ["STEENBRB", "YORKNET"]

# RULE.md section 4: this task's new keys, excluded BY NAME.
NEW_KEYS = {
    "tr_init",
    "first_qp_solved",
    "first_qp_status",
    "first_qp_elastic_applied",
    "first_qp_plain_status",
    "first_qp_verdict",
    "first_qp_tr_radius",
    "first_qp_tr_binding",
    "first_qp_restoration_seed_used",
    "history_rows",
    "elastic_from_ipqp_escape",
    "restoration_sub_status",
    "deadline_seconds",
    "deadline_fired",
}
# RULE.md section 4: TIME-VALUED fields, dropped BY NAME. None is printed by
# either probe today; the list is total so that a future key of this kind is
# excluded by the committed rule rather than by a later judgement call.
TIME_KEYS = {"wall_seconds", "elapsed", "elapsed_seconds"}

# RULE.md section 4: the residual class, the ONLY keys and columns a near-ulp
# gate may be applied to. Counters, statuses, integer- and enum-valued keys and
# x_k_source are byte equality, no tolerance, ever.
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
    """M2b's summarise_m2b.py reader, unchanged."""
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
    """RULE.md section 4's declared fallback. Returns (accepted, gap) or
    (False, None) when the pair is not even a pair of finite floats."""
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
    print("M7 W2 M3a -- CONTROL IDENTITY: the tr_init = 1, zeta = 0 legs vs M2b's")
    print("committed screen rows. RULE.md section 4. A DIFFERENCE IS A STOP, and a")
    print("stop ABORTS the chain before any raised-radius leg runs.")
    print("")
    print(f"excluded by name (this task's new keys): {len(NEW_KEYS)}")
    print(f"dropped by name (time-valued):           {sorted(TIME_KEYS)}")
    print(f"near-ulp gate, residual class only:      |a-b| <= {NEAR_ULP_REL:g} * max(1,|a|,|b|)")
    print("")
    for cell in CELLS:
        kv_new, rows_new, hdr_new = read_probe(os.path.join(NEW, cell + "@tr1.txt"))
        kv_old, rows_old, hdr_old = read_probe(os.path.join(M2B, cell + ".txt"))
        if kv_new is None or kv_old is None:
            print(f"{cell:10s} MISSING LEG -- new={kv_new is not None} m2b={kv_old is not None}")
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
            print(f"{cell:10s}   keys only in this probe (expected = the new ones): {only_new}")
        if only_old:
            print(f"{cell:10s}   keys only in M2b's row: {only_old}")
        for k, a, b in diffs + row_diffs:
            print(f"{cell:10s}   DIFF {k}: new={a}  m2b={b}")
        if diffs or row_diffs:
            worst = 1

    print("")
    print(f"NEAR_ULP_TOLERATED={tolerated}")
    print(f"IDENTITY_WORST={worst}")
    return worst


if __name__ == "__main__":
    sys.exit(main())
