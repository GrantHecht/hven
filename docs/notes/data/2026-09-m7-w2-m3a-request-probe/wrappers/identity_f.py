#!/usr/bin/env python3
"""M7 W2 M3a ADDENDUM F -- RULE-F.md section 3's TWO CHECKS, and nothing else.

The probe gained ONE argument (`--ledger on|off`, default off) and one printed
key, plus a LEDGER block emitted after LADDER_END. Two different things must be
proved about it, and this script is both:

  IDENTITY   (RULE-F.md section 3.1) -- the new argument is INERT AT ITS DEFAULT.
             `STEENBRB --tr-init inf --ledger off` vs the committed
             rows/stage1/STEENBRB@trinf.txt. A failure ABORTS the chain: it
             would mean the rebuilt binary is not stage 1's solve.

  NEUTRALITY (RULE-F.md section 3.2) -- THE ATTACHED LEDGER MOVES NO SOLVER
             COUNTER. Each `--ledger on` leg vs ITS OWN committed stage-1 twin,
             on the identical machinery. If any solver counter moves with the
             ledger attached, the addendum STOPS: that is a finding ABOUT hven,
             not a nuisance to work around.

  usage  identity_f.py identity        the identity leg alone
         identity_f.py neutral-first   STEENBRB@F-trinf alone (the chain's gate)
         identity_f.py neutrality      all four measured legs
  exit   0 = every compared pair IDENTICAL (or WITHHELD), 1 = a STOP

MACHINE-WRITTEN: no number this produces is transcribed by hand. The exclusion
list is RULE-F.md section 3.1's, BY NAME, and a key absent from it is compared
whatever it is. The near-ulp fallback is RULE.md section 4's, carried over
verbatim through RULE-E.md section 3 and unchanged again here.
"""
import os
import sys

ROOT = "/home/ghecht/Projects/hven"
ART = os.path.join(ROOT, "docs/notes/data/2026-09-m7-w2-m3a-request-probe")
NEW = os.path.join(ROOT, ".scratch/m7w2m3a/out/f")
OLD = os.path.join(ART, "rows/stage1")

# RULE-F.md section 2's table, verbatim: leg tag -> (cell, its committed twin).
IDENTITY_PAIRS = [("STEENBRB@identity-F", "STEENBRB@trinf.txt")]
NEUTRALITY_PAIRS = [
    ("STEENBRB@F-trinf", "STEENBRB@trinf.txt"),
    ("STEENBRB@F-tr1", "STEENBRB@tr1.txt"),
    ("YORKNET@F-trinf", "YORKNET@trinf.txt"),
    ("YORKNET@F-tr1", "YORKNET@tr1.txt"),
]

# RULE-F.md section 3.1: THIS ADDENDUM's one new key, excluded BY NAME.
NEW_KEYS = {"ledger"}
# RULE.md section 4's TIME-VALUED list, carried over unchanged and total. None
# is printed by either binary today; the list is stated in full so a future key
# of this kind is excluded by a committed rule rather than a later judgement
# call.
TIME_KEYS = {"wall_seconds", "elapsed", "elapsed_seconds"}

# RULE.md section 4's residual class -- the ONLY keys and columns a near-ulp
# gate may touch. Counters, statuses, integer- and enum-valued keys and
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
    """identity_e.py's reader with ONE change, and it is the change addendum F
    needs: key/value lines are collected only from BEFORE `LADDER_BEGIN`.

    The F probe emits a LEDGER block AFTER `LADDER_END` whose lines contain
    `=`; a reader that swept the whole file would fold those into the key set
    and then report them as differences against a committed row that has no
    such block. RULE-F.md section 3.1 says the LEDGER block is not part of the
    key block and is not compared -- this is that rule, in code. On the
    committed stage-1 rows, which carry nothing after LADDER_END but PROBE_END,
    this reader and identity_e.py's produce the SAME key set.
    """
    kv, rows, header = {}, [], None
    state = "keys"
    if not os.path.exists(path):
        return None, None, None
    for line in open(path):
        line = line.rstrip("\n")
        if line == "LADDER_BEGIN":
            state = "ladder"
            continue
        if line == "LADDER_END":
            state = "after"
            continue
        if state == "ladder":
            if header is None:
                header = line.split(",")
            else:
                rows.append(dict(zip(header, line.split(","))))
            continue
        if state == "after":
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


def compare(tag, new_name, old_name, out):
    """One pair. Returns (worst, tolerated). `out` collects the printed lines."""
    kv_new, rows_new, hdr_new = read_probe(os.path.join(NEW, new_name + ".txt"))
    kv_old, rows_old, hdr_old = read_probe(os.path.join(OLD, old_name))
    if kv_new is None or kv_old is None:
        out.append(f"{tag:20s} MISSING LEG -- new={kv_new is not None} "
                   f"stage1={kv_old is not None}   STOP")
        return 1, 0

    # RULE-F.md section 5: a TRUNCATED leg's check is WITHHELD. An interrupted
    # solve is not the committed control's solve, and a difference there would
    # be the interruption, not the ledger. Withheld is not failed: the leg is
    # reported as TRUNCATED, with its numbers, and yields no verdict either.
    if kv_new.get("deadline_fired") == "1":
        out.append(f"{tag:20s} WITHHELD -- deadline_fired=1 (TRUNCATED). "
                   f"RULE-F.md section 5: an interrupted solve is not the control's "
                   f"solve, so no neutrality claim is taken from it.")
        return 0, 0

    tolerated = 0
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
                out.append(f"{tag:20s} NEAR-ULP ACCEPTED key {k}: {a} vs {b}  gap={gap:.3e}")
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
                        out.append(f"{tag:20s} NEAR-ULP ACCEPTED row {i} col {c}: "
                                   f"{a} vs {b}  gap={gap:.3e}")
                        continue
                row_diffs.append((f"row {i} col {c}", a, b))

    verdict = "IDENTICAL" if not diffs and not row_diffs else "DIFFERS -- STOP"
    out.append(f"{tag:20s} vs {old_name:20s} keys {len(shared):3d} compared, "
               f"{len(diffs):3d} differing; rows {len(rows_new):3d} x {len(cols)} columns, "
               f"{len(row_diffs):3d} differing   {verdict}")
    if only_new:
        out.append(f"{tag:20s}   keys only in the rebuilt probe: {only_new}")
    if only_old:
        out.append(f"{tag:20s}   keys only in the stage-1 row: {only_old}")
    for k, a, b in diffs + row_diffs:
        out.append(f"{tag:20s}   DIFF {k}: new={a}  stage1={b}")
    return (1 if (diffs or row_diffs) else 0), tolerated


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "identity"
    if mode == "identity":
        pairs, title = IDENTITY_PAIRS, (
            "IDENTITY (RULE-F.md section 3.1): the REBUILT probe at the new argument's\n"
            "DEFAULT (--ledger off) vs the committed stage-1 @trinf row.\n"
            "A DIFFERENCE IS A STOP, and a stop ABORTS the chain before any F leg runs.")
    elif mode == "neutral-first":
        pairs, title = NEUTRALITY_PAIRS[:1], (
            "NEUTRALITY, THE GATING LEG (RULE-F.md section 3.2): the FIRST --ledger on leg\n"
            "vs its own committed stage-1 twin. If any solver counter moved with the ledger\n"
            "ATTACHED, the chain ABORTS here and that is a finding ABOUT hven.")
    elif mode == "neutrality":
        pairs, title = NEUTRALITY_PAIRS, (
            "NEUTRALITY (RULE-F.md section 3.2): every --ledger on leg vs its own committed\n"
            "stage-1 twin, on the identity check's machinery. The compared set covers the\n"
            "terminal status, f, the feasibilities and every printed SqpCounters field --\n"
            "'solver counters, status, f and h' stated by rule, not by a hand-picked subset.")
    else:
        print(f"usage: identity_f.py [identity|neutral-first|neutrality]")
        return 2

    worst, tolerated = 0, 0
    out = []
    print("M7 W2 M3a ADDENDUM F -- " + title)
    print("")
    print(f"excluded by name (this addendum's new key): {sorted(NEW_KEYS)}")
    print(f"dropped by name (time-valued):              {sorted(TIME_KEYS)}")
    print(f"near-ulp gate, residual class only:         |a-b| <= {NEAR_ULP_REL:g} * max(1,|a|,|b|)")
    print("the LEDGER block is NOT part of the key block and is NOT compared "
          "(RULE-F.md section 3.1)")
    print("")
    for tag, old_name in pairs:
        w, t = compare(tag, tag, old_name, out)
        worst = max(worst, w)
        tolerated += t
    for line in out:
        print(line)
    print("")
    print(f"NEAR_ULP_TOLERATED={tolerated}")
    print(f"IDENTITY_F_WORST={worst}")
    return worst


if __name__ == "__main__":
    sys.exit(main())
