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

fix1 (settler adjudication R2) added the STRUCTURAL REFUSALS documented at
`EXPECTED_OLD_KEYS` below, and ONE input knob: the new-side root is read from
the environment variable `M3A_NEW_ROOT` when it is set, and is otherwise
exactly the `.scratch` path it has always been. That knob exists for one
purpose only -- re-running the hardened checker against the COMMITTED rows,
which mirror the scratch layout byte for byte, so that the refusals can be
exercised and the verdicts re-derived without a solver. It changes no rule,
no exclusion list and no tolerance.
"""
import os
import sys

ROOT = "/home/ghecht/Projects/hven"
ART = os.path.join(ROOT, "docs/notes/data/2026-09-m7-w2-m3a-request-probe")
# fix1: the new-side root. Unset -- which is what the chain does -- this is the
# same `.scratch/m7w2m3a/out` it has always been, so a chain run is unchanged.
NEW_ROOT = os.environ.get("M3A_NEW_ROOT", os.path.join(ROOT, ".scratch/m7w2m3a/out"))
NEW = os.path.join(NEW_ROOT, "e")
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

# ---------------------------------------------------------------------------
# fix1 (settler adjudication R2) -- THE STRUCTURAL REFUSALS.
#
# The review of this round found the three identity checkers VACUOUS IN SHAPE.
# They compared only the INTERSECTION of the two key sets; they printed a key
# present in the committed row but MISSING from the new one as an informational
# note; and they compared only the ladder columns present in BOTH headers. An
# empty, truncated or key-stripped new file therefore compared zero keys, found
# zero differences, and reported IDENTICAL. A check that passes on an empty file
# is not a check, and an identity claim resting on it would be resting on
# nothing -- which here means every E row would be believed on no evidence.
#
# The four refusals below close that hole. They are STRUCTURE, not numerics:
# nothing about what counts as a DIFFERENCE changes. The exclusion lists, the
# residual class and the near-ulp constant are untouched, and a pair that passes
# these refusals is compared exactly as it was compared before.
#
#   (a) SENTINELS     each file's first non-empty line is PROBE_BEGIN and its
#                     last is PROBE_END, exactly one of each. A probe killed
#                     mid-write has no PROBE_END, and that must not read as
#                     agreement.
#   (b) OLD KEY SET   the committed twin's key set is EXACTLY the pinned set
#                     below -- cardinality AND names -- so a stripped or
#                     truncated COMMITTED file cannot pass either.
#   (c) NEW KEY SET   the rebuilt probe's key set is EXACTLY that pinned set
#                     plus this addendum's declared NEW_KEYS. A key of the old
#                     row missing from the new one is now a FAILURE, never a
#                     note; so is a key in the new row the union does not name.
#   (d) LADDER HEADER each file's ladder header is EXACTLY LADDER_COLS, in
#                     order -- not merely a superset of the columns compared. A
#                     file with no LADDER block at all fails here.
#
# EXPECTED_OLD_KEYS is THE COMMITTED STAGE-1 @trinf ROW'S OWN KEY SET, read off
# rows/stage1/STEENBRB@trinf.txt as it stands today and written out here as a
# literal precisely so that a later stripped or truncated file cannot pass by
# agreeing with itself. It is a pin, not a derivation. It carries no
# `requesting_h`/`requesting_f`: the @trinf control enters no restoration phase,
# so the probe prints neither, and pinning the set means pinning THAT row's set.
EXPECTED_OLD_KEYS = {
    "border_refine_steps", "budget_iters_argument", "callback",
    "complementarity", "deadline_fired", "deadline_seconds",
    "elastic_activations", "elastic_escalations", "elastic_floor_retries",
    "elastic_from_ipqp_escape", "elastic_rho0_ceiling_hits", "f",
    "factorizations", "feasibility_e", "feasibility_i",
    "first_qp_elastic_applied", "first_qp_plain_status",
    "first_qp_restoration_seed_used", "first_qp_solved", "first_qp_status",
    "first_qp_tr_binding", "first_qp_tr_radius", "first_qp_verdict",
    "history_rows", "infeasibility_certified", "major_iters", "me", "mi",
    "n", "problem", "proximity", "qp_failures_in_a_row_peak",
    "qp_failures_routed", "qp_maxiter_exits", "qp_minor_iters",
    "qp_numerical_exits", "rejected_steps", "restoration_entries",
    "restoration_f_in", "restoration_f_out", "restoration_h_in",
    "restoration_h_out", "restoration_iters", "restoration_sub_status",
    "stationarity", "status", "steps_accepted", "symbolic_analyses",
    "tr_init", "x_k_source",
}
# What the REBUILT probe must print: the committed set plus the five keys
# RULE-E.md section 3 excludes BY NAME, and nothing else.
EXPECTED_NEW_KEYS = EXPECTED_OLD_KEYS | NEW_KEYS


def sentinel_refusals(label, path):
    """fix1 refusal (a). Returns refusal lines; an empty list means it passed."""
    if not os.path.exists(path):
        return [f"REFUSED [sentinels] {label}: no such file: {path}"]
    body = [ln.rstrip("\n") for ln in open(path) if ln.strip()]
    if not body:
        return [f"REFUSED [sentinels] {label}: the file is EMPTY -- there is nothing "
                f"to compare, and an empty file must never read as agreement ({path})"]
    msgs = []
    if body[0] != "PROBE_BEGIN":
        msgs.append(f"REFUSED [sentinels] {label}: first non-empty line is "
                    f"{body[0]!r}, expected 'PROBE_BEGIN'")
    if body[-1] != "PROBE_END":
        msgs.append(f"REFUSED [sentinels] {label}: last non-empty line is "
                    f"{body[-1]!r}, expected 'PROBE_END' -- the row is TRUNCATED")
    n_begin, n_end = body.count("PROBE_BEGIN"), body.count("PROBE_END")
    if n_begin != 1 or n_end != 1:
        msgs.append(f"REFUSED [sentinels] {label}: expected exactly one PROBE_BEGIN "
                    f"and one PROBE_END, found {n_begin} and {n_end}")
    return msgs


def keyset_refusals(label, kv, expected, what):
    """fix1 refusals (b) and (c). Cardinality AND names, both directions."""
    got = set(kv)
    if got == expected:
        return []
    missing, extra = sorted(expected - got), sorted(got - expected)
    msgs = [f"REFUSED [{what}] {label}: key set has {len(got)} keys, the pinned "
            f"set has {len(expected)}"]
    if missing:
        msgs.append(f"REFUSED [{what}] {label}:   MISSING from this file: {missing}")
    if extra:
        msgs.append(f"REFUSED [{what}] {label}:   UNEXPECTED in this file: {extra}")
    return msgs


def ladder_refusals(label, hdr):
    """fix1 refusal (d). The header is pinned in order, not merely contained."""
    if hdr is None:
        return [f"REFUSED [ladder-header] {label}: the file has NO LADDER block -- "
                f"there is no ladder to compare"]
    if hdr != LADDER_COLS:
        return [f"REFUSED [ladder-header] {label}: header is {hdr}, expected "
                f"exactly {LADDER_COLS}"]
    return []


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
    print(f"fix1 structural refusals:                    ON -- sentinels, "
          f"{len(EXPECTED_OLD_KEYS)}-key committed set, {len(EXPECTED_NEW_KEYS)}-key new set, "
          f"{len(LADDER_COLS)}-column ladder header")
    print("")
    for cell, new_name, old_name in PAIRS:
        new_path = os.path.join(NEW, new_name)
        old_path = os.path.join(OLD, old_name)
        kv_new, rows_new, hdr_new = read_probe(new_path)
        kv_old, rows_old, hdr_old = read_probe(old_path)
        if kv_new is None or kv_old is None:
            print(f"{cell:10s} MISSING LEG -- new={kv_new is not None} stage1={kv_old is not None}")
            worst = 1
            continue

        # fix1 (a)-(d), BEFORE any value is compared. The comparison below is
        # only meaningful on two files that are shaped like probe rows; these
        # refusals are what establishes that, and any one of them is a STOP.
        # The comparison still runs afterwards, so the report keeps its summary
        # line and shows whatever else differs.
        refusals = []
        refusals += sentinel_refusals(f"{cell} new", new_path)
        refusals += sentinel_refusals(f"{cell} stage1", old_path)
        refusals += keyset_refusals(f"{cell} stage1", kv_old, EXPECTED_OLD_KEYS, "old-key-set")
        refusals += keyset_refusals(f"{cell} new", kv_new, EXPECTED_NEW_KEYS, "new-key-set")
        refusals += ladder_refusals(f"{cell} new", hdr_new)
        refusals += ladder_refusals(f"{cell} stage1", hdr_old)
        for line in refusals:
            print(f"{cell:10s} {line}")
        if not refusals:
            print(f"{cell:10s} STRUCTURE OK (fix1): sentinels, key sets "
                  f"({len(kv_old)} committed / {len(kv_new)} new) and ladder header, both files")

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

        # fix1: a structural refusal outranks the value comparison. Without
        # this, a key-stripped file compares zero keys, finds zero differences,
        # and prints IDENTICAL -- the exact vacuity the review found.
        verdict = ("REFUSED -- STOP" if refusals
                   else "IDENTICAL" if not diffs and not row_diffs else "DIFFERS -- STOP")
        print(f"{cell:10s} keys {len(shared):3d} compared, {len(diffs):3d} differing; "
              f"rows {len(rows_new):3d} x {len(cols)} columns, {len(row_diffs):3d} differing   "
              f"{verdict}")
        if only_new:
            print(f"{cell:10s}   keys only in the rebuilt probe (expected = the new ones): {only_new}")
        if only_old:
            # fix1: this list is no longer informational -- a non-empty
            # `only_old` has already been refused by the new-key-set check
            # above. It is printed for the reader, not as a tolerance.
            print(f"{cell:10s}   keys only in the stage-1 row: {only_old}")
        for k, a, b in diffs + row_diffs:
            print(f"{cell:10s}   DIFF {k}: new={a}  stage1={b}")
        if diffs or row_diffs or refusals:
            worst = 1

    print("")
    print(f"NEAR_ULP_TOLERATED={tolerated}")
    print(f"IDENTITY_E_WORST={worst}")
    return worst


if __name__ == "__main__":
    sys.exit(main())
