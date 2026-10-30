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
NEW = os.path.join(NEW_ROOT, "f")
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

# ---------------------------------------------------------------------------
# fix1 (settler adjudication R2) -- THE STRUCTURAL REFUSALS.
#
# The review of this round found the three identity checkers VACUOUS IN SHAPE.
# They compared only the INTERSECTION of the two key sets; they printed a key
# present in the committed row but MISSING from the new one as an informational
# note; and they compared only the ladder columns present in BOTH headers. An
# empty, truncated or key-stripped new file therefore compared zero keys, found
# zero differences, and reported IDENTICAL. A check that passes on an empty file
# is not a check -- and here that would have meant the NEUTRALITY claim, the one
# claim addendum F exists to make, resting on nothing.
#
# The four refusals below close that hole. They are STRUCTURE, not numerics:
# nothing about what counts as a DIFFERENCE changes. The exclusion list, the
# residual class and the near-ulp constant are untouched, and a pair that passes
# these refusals is compared exactly as it was compared before.
#
#   (a) SENTINELS     each file's first non-empty line is PROBE_BEGIN and its
#                     last is PROBE_END, exactly one of each. A probe killed
#                     mid-write has no PROBE_END, and that must not read as
#                     agreement. On a `--ledger on` row PROBE_END still closes
#                     the file: the LEDGER block sits between LADDER_END and it.
#   (b) OLD KEY SET   the committed twin's key set is EXACTLY the pinned set for
#                     THAT twin -- cardinality AND names -- so a stripped or
#                     truncated COMMITTED file cannot pass either.
#   (c) NEW KEY SET   the F row's key set is EXACTLY that pinned set plus the
#                     keys named below. A key of the old row missing from the
#                     new one is now a FAILURE, never a note; so is a key in the
#                     new row the union does not name.
#   (d) LADDER HEADER each file's ladder header is EXACTLY LADDER_COLS, in
#                     order -- not merely a superset of the columns compared. A
#                     file with no LADDER block at all fails here.
#
# EXPECTED_OLD_KEYS pins EACH committed twin's OWN key set, read off the
# rows/stage1 files as they stand today and written out as literals precisely so
# that a later stripped or truncated file cannot pass by agreeing with itself.
# They are pins, not derivations. The two shapes differ by exactly
# `requesting_h`/`requesting_f`: the @tr1 controls enter a restoration phase and
# print the requesting pair, the @trinf controls do not enter one and print
# neither. Pinning the set means pinning THAT row's set, not a union of both.
_OLD_KEYS_TRINF = {
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
_OLD_KEYS_TR1 = _OLD_KEYS_TRINF | {"requesting_h", "requesting_f"}
EXPECTED_OLD_KEYS = {
    "STEENBRB@trinf.txt": _OLD_KEYS_TRINF,
    "YORKNET@trinf.txt": _OLD_KEYS_TRINF,
    "STEENBRB@tr1.txt": _OLD_KEYS_TR1,
    "YORKNET@tr1.txt": _OLD_KEYS_TR1,
}

# The F binary is ADDENDUM E's binary plus `--ledger`, so every F row also
# carries E's five printed keys. They are not new IN F -- RULE-F.md section 3.1
# names only `ledger`, because the E keys were already excluded upstream by
# RULE-E.md section 3 and are absent from the committed stage-1 twins for the
# same reason `ledger` is. Naming them here is what lets the new-side key set be
# pinned EXACTLY without re-scoping NEW_KEYS, which stays RULE-F.md's one key.
INHERITED_E_KEYS = {
    "qp_cap_retry", "qp_max_iter", "qp_cap_retries",
    "first_qp_minor_iters", "first_qp_factorizations",
}


def expected_new_keys(old_name):
    """The exact key set an F row must print: its twin's pinned set, plus the
    one key RULE-F.md adds, plus the five it inherits from addendum E."""
    return EXPECTED_OLD_KEYS[old_name] | NEW_KEYS | INHERITED_E_KEYS


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
    new_path = os.path.join(NEW, new_name + ".txt")
    old_path = os.path.join(OLD, old_name)
    kv_new, rows_new, hdr_new = read_probe(new_path)
    kv_old, rows_old, hdr_old = read_probe(old_path)
    if kv_new is None or kv_old is None:
        out.append(f"{tag:20s} MISSING LEG -- new={kv_new is not None} "
                   f"stage1={kv_old is not None}   STOP")
        return 1, 0

    withheld = kv_new.get("deadline_fired") == "1"

    # fix1 (a)-(d), BEFORE anything else. These apply to EVERY pair this
    # function reads, WITHHELD legs included: whether a row is truncated at the
    # solver deadline is a fact the row itself must be well-formed enough to
    # state, so a file that is empty, unsentinelled or ladderless cannot buy
    # itself a pass by printing deadline_fired=1.
    refusals = []
    refusals += sentinel_refusals(f"{tag} new", new_path)
    refusals += sentinel_refusals(f"{tag} stage1", old_path)
    refusals += ladder_refusals(f"{tag} new", hdr_new)
    refusals += ladder_refusals(f"{tag} stage1", hdr_old)
    if old_name not in EXPECTED_OLD_KEYS:
        refusals.append(f"REFUSED [old-key-set] {tag}: no pinned key set for twin "
                        f"{old_name!r} -- RULE-F.md section 2's table and the pins "
                        f"above have drifted apart")
    else:
        refusals += keyset_refusals(f"{tag} stage1", kv_old,
                                    EXPECTED_OLD_KEYS[old_name], "old-key-set")
        if not withheld:
            refusals += keyset_refusals(f"{tag} new", kv_new,
                                        expected_new_keys(old_name), "new-key-set")
        # A TRUNCATED F row's key set is NOT asserted, and the reason is the
        # instruction fix1 was given: verify, do not assume. NO committed F row
        # is truncated -- all five carry deadline_fired=0 -- so there is nothing
        # to read a pin off, and pinning a guess would be pinning a guess. The
        # OLD side above is still asserted, because a WITHHELD leg's twin is an
        # ordinary completed stage-1 row either way.
    for line in refusals:
        out.append(f"{tag:20s} {line}")
    if not refusals:
        out.append(f"{tag:20s} STRUCTURE OK (fix1): sentinels, key sets "
                   f"({len(kv_old)} committed"
                   + ("" if withheld else f" / {len(kv_new)} new")
                   + ") and ladder header, both files"
                   + (" [new-side key set not asserted: TRUNCATED]" if withheld else ""))

    # RULE-F.md section 5: a TRUNCATED leg's check is WITHHELD. An interrupted
    # solve is not the committed control's solve, and a difference there would
    # be the interruption, not the ledger. Withheld is not failed: the leg is
    # reported as TRUNCATED, with its numbers, and yields no verdict either.
    # fix1 leaves that a non-failure -- but only for a structurally sound row:
    # a refusal above still STOPS, because "withheld" is a statement about a
    # solve, and an unreadable file has not stated one.
    if withheld:
        out.append(f"{tag:20s} WITHHELD -- deadline_fired=1 (TRUNCATED). "
                   f"RULE-F.md section 5: an interrupted solve is not the control's "
                   f"solve, so no neutrality claim is taken from it.")
        return (1 if refusals else 0), 0

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

    # fix1: a structural refusal outranks the value comparison. Without this, a
    # key-stripped file compares zero keys, finds zero differences, and prints
    # IDENTICAL -- the exact vacuity the review found.
    verdict = ("REFUSED -- STOP" if refusals
               else "IDENTICAL" if not diffs and not row_diffs else "DIFFERS -- STOP")
    out.append(f"{tag:20s} vs {old_name:20s} keys {len(shared):3d} compared, "
               f"{len(diffs):3d} differing; rows {len(rows_new):3d} x {len(cols)} columns, "
               f"{len(row_diffs):3d} differing   {verdict}")
    if only_new:
        out.append(f"{tag:20s}   keys only in the rebuilt probe: {only_new}")
    if only_old:
        # fix1: this list is no longer informational -- a non-empty `only_old`
        # has already been refused by the new-key-set check above. It is printed
        # for the reader, not as a tolerance.
        out.append(f"{tag:20s}   keys only in the stage-1 row: {only_old}")
    for k, a, b in diffs + row_diffs:
        out.append(f"{tag:20s}   DIFF {k}: new={a}  stage1={b}")
    return (1 if (diffs or row_diffs or refusals) else 0), tolerated


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
    print(f"fix1 structural refusals:                   ON -- sentinels, the pinned "
          f"committed key set per twin, that set + {len(NEW_KEYS | INHERITED_E_KEYS)} "
          f"declared new/inherited keys on the new side, {len(LADDER_COLS)}-column "
          f"ladder header")
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
