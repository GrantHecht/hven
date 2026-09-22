#!/usr/bin/env python3
"""M7 W2 M3c RULE-P2A section 5 -- the identity gate, on EVERY field, and section
6's mechanical outcome.

The pass-2 smoke checked eight integer counters on one cell and reported the seam
neutral while `f`, `stationarity` and `feasibility_e` already differed in the same
transcript. This gate cannot repeat that: it does not choose its fields. It takes
the INTERSECTION of the two rows' own key=value field sets, compares every member,
and names every field it could not compare.

  byte class      exact byte equality, no tolerance
  residual class  |a - b| <= 1e-12 * max(1, |a|, |b|)   (RULE-P2 section 5.1's own
                  constant, unchanged), `nan` matched as a BYTE fact

A field is residual class iff EITHER row's value text parses as a float AND
carries one of `.`, `e`, `E`, `nan`, `inf`; everything else is byte class.

No solver runs here. The folds of section 5.5 run on constructed inputs before any
leg is read, and this script exits non-zero if any of them does not behave.

FIX ROUND 1 (settler adjudication X1) -- THE GATE IS NOW STRUCTURALLY REFUSING,
not merely structurally honest. The version that produced `logs/gate-p2a.txt`
and `logs/gate-p2b.txt` compared the intersection and called a leg a
reproduction whenever that intersection was NON-EMPTY and carried no
difference. Codex sol's review is right that this can pass on ONE surviving
shared field, and that a missing field or a conflicting duplicate key was
reported but did not FAIL. Three refusals are added, and each is fold-proved on
constructed inputs below:

  R-EXPECT  an EXPECTED field set is declared per cell -- `EXPECTED_MANIFEST`
            intersected with the COMMITTED PASS-1 ROW's own key set -- and any
            expected field the leg does not print is a REFUSAL, not an
            "ABSENT" note. The manifest is pinned in this file; the checker
            never learns its field set from the leg it is judging.
  R-FLOOR   the compared (shared, non-wall-clock) set must have at least
            `MIN_SHARED` = 58 members, the P2A/P2B comparable surface. A
            one-field intersection can no longer pass.
  R-DUP     a key repeated inside one row with CONFLICTING values REFUSES, even
            when the row's last value would have agreed.

`EXPECTED_MANIFEST` names a field the pass-1 row itself does not carry -> that
field is reported ABSENT-IN-PASS-1 and is NOT required of the leg (a pass-1 row
that never ran a first QP, e.g. `NET3`, prints no `first_*` field). Such a cell
then has fewer than 58 comparable fields and is refused by R-FLOOR, which is
the conservative direction: it is declared, never silently passed.

The committed originals `logs/gate-p2a.txt` / `logs/gate-p2b.txt` are NOT
edited: they are what the pre-hardening checker printed at the time the legs
ran. `logs/gate-p2a-v2.txt` / `logs/gate-p2b-v2.txt` are this hardened
checker's POST-HOC REPLAY over the same committed rows. No leg was re-run: no
solver ran in either pass.
"""

import argparse
import math
import os
import re
import sys

TOL_REL = 1e-12
WALL_CLOCK_PATTERNS = ("wall", "seconds_elapsed", "time")

# The P2A/P2B COMPARABLE SURFACE, pinned. These 58 names are the fields the
# committed pass-1 rows and the pass-2 probe both print; they are written here
# so the checker carries its own expectation into every leg instead of reading
# one off the leg. A leg that drops any of them REFUSES.
EXPECTED_MANIFEST = frozenset((
    "border_refine_steps", "complementarity", "deadline_fired",
    "deadline_seconds", "f", "factorizations", "feasibility_e",
    "feasibility_i", "first_border_refine_steps",
    "first_degenerate_fraction", "first_degenerate_run_max",
    "first_degenerate_steps", "first_distinct_bound_added",
    "first_distinct_ineq_added", "first_drop_ties",
    "first_eqp_refine_steps", "first_factorizations", "first_k0_reused",
    "first_label", "first_minor_iters", "first_qp_record_status",
    "first_ratio_ties", "first_schur_updates", "first_shift_adds",
    "first_suspect_escalations", "first_symbolic_analyses",
    "first_verdict_refine_steps", "first_warm", "first_ws_adds",
    "first_ws_adds_bound", "first_ws_adds_minus_drops", "first_ws_drops",
    "first_ws_drops_bound", "first_ws_net_change",
    "infeasibility_certified", "ledger", "ledger_attached",
    "ledger_qp_records", "ledger_sqp_records", "ledger_sqp_status",
    "major_iters", "me", "mi", "n", "problem", "qp_failures_routed",
    "qp_maxiter_exits", "qp_minor_iters", "qp_numerical_exits",
    "rejected_steps", "restoration_entries", "restoration_iters",
    "restoration_sub_status", "stationarity", "status", "steps_accepted",
    "symbolic_analyses", "tr_init",
))
MIN_SHARED = 58
KV_RE = re.compile(r"^([A-Za-z_][A-Za-z_0-9]*)=(.*)$")


def parse_row(text):
    """key=value fields of a probe row, with duplicates reported."""
    fields, dups = {}, []
    for line in text.split("\n"):
        m = KV_RE.match(line)
        if not m:
            continue
        k, v = m.group(1), m.group(2)
        if k in fields and fields[k] != v:
            dups.append(k)
        fields[k] = v
    return fields, dups


def is_wall_clock(key):
    return any(p in key for p in WALL_CLOCK_PATTERNS)


def as_float(text):
    try:
        return float(text)
    except (TypeError, ValueError):
        return None


def residual_class(a, b):
    for t in (a, b):
        if as_float(t) is None:
            continue
        low = t.lower()
        if any(c in low for c in (".", "e", "nan", "inf")):
            return True
    return False


def compare_field(a, b):
    """Return (klass, verdict, delta). Verdicts: EQUAL_BYTES / WITHIN / DIFFERS."""
    if a == b:
        return ("residual" if residual_class(a, b) else "byte"), "EQUAL_BYTES", 0.0
    if not residual_class(a, b):
        return "byte", "DIFFERS", None
    fa, fb = as_float(a), as_float(b)
    if fa is None or fb is None:
        return "residual", "DIFFERS", None
    if math.isnan(fa) or math.isnan(fb):
        # `nan` is a BYTE fact: unequal text with a nan on either side differs.
        return "residual", "DIFFERS", None
    d = abs(fa - fb)
    bar = TOL_REL * max(1.0, abs(fa), abs(fb))
    return "residual", ("WITHIN" if d <= bar else "EXCEEDS"), d


def gate(p1_text, p2_text, expected=None, min_shared=MIN_SHARED, strict=False,
         exclude_prefixes=()):
    """The whole gate for one leg. Returns a dict of everything it found.

    M7 W2 M4 adds `strict`. In STRICT mode a `WITHIN` -- a residual-class field
    that passes only by the near-ulp bar -- is a DIFFERENCE, not a pass. RULE.md
    section 5.5 requires it on the NEUTRALITY legs and only there: those legs
    compare two runs of the SAME binary on the SAME cell at MKL_NUM_THREADS=1,
    which differ in nothing, and an arm claiming to be inert where no tie occurs
    owes bit equality rather than a tolerance. The OFF controls keep the
    ordinary mode, because they compare against a row produced by a DIFFERENTLY
    BUILT binary and CLAUDE.md section 7's near-ulp allowance is exactly the
    case for that.

    `expected` is the declared field set (default: `EXPECTED_MANIFEST`). The
    EXPECTED SET FOR THIS LEG is that set intersected with the committed
    pass-1 row's own keys; every member of it must be present in the leg or
    the leg REFUSES. `min_shared` is the floor on the compared set.
    """
    f1, d1 = parse_row(p1_text)
    f2, d2 = parse_row(p2_text)
    expected = EXPECTED_MANIFEST if expected is None else frozenset(expected)
    expected_here = sorted(expected & set(f1))
    expected_absent_in_p1 = sorted(expected - set(f1))
    missing_expected = sorted(k for k in expected_here if k not in f2)
    common = sorted(set(f1) & set(f2))

    def excluded_by_name(k):
        # RULE.md AMENDMENT 1 (section 5.5a): on a NEUTRALITY leg the arm's own
        # `arm` / `arm_*` keys are excluded BY NAME and the exclusion printed,
        # exactly as section 5.3 treats wall-clock, and for the same reason --
        # they describe the LEG, not the solve. Empty by default, so the OFF
        # controls are untouched.
        return any(k == pre or k.startswith(pre + "_") for pre in exclude_prefixes)

    compared = [k for k in common if not is_wall_clock(k) and not excluded_by_name(k)]
    excluded = [k for k in common if is_wall_clock(k)]
    excluded_by_prefix = [k for k in common if excluded_by_name(k)]
    rows, differs, near_ulp = [], [], 0
    for k in compared:
        klass, verdict, delta = compare_field(f1[k], f2[k])
        rows.append((k, klass, verdict, f1[k], f2[k], delta))
        if verdict in ("DIFFERS", "EXCEEDS"):
            differs.append(k)
        elif verdict == "WITHIN":
            near_ulp += 1
            if strict:
                differs.append(k)
    dups = sorted(set(d1) | set(d2))
    refusals = []
    if missing_expected:
        refusals.append("R-EXPECT: the leg does not print %d expected field(s): %s"
                        % (len(missing_expected), ",".join(missing_expected)))
    if len(compared) < min_shared:
        refusals.append("R-FLOOR: only %d compared field(s), below the declared "
                        "minimum of %d" % (len(compared), min_shared))
    if dups:
        refusals.append("R-DUP: key(s) repeated with CONFLICTING values inside one "
                        "row: %s" % ",".join(dups))
    _ = excluded_by_prefix
    return {
        "compared": compared,
        "excluded_wall_clock": excluded,
        "excluded_by_prefix": excluded_by_prefix,
        "absent_in_leg": sorted(set(f1) - set(f2)),
        "only_in_leg": sorted(set(f2) - set(f1)),
        "duplicates": dups,
        "expected": expected_here,
        "expected_absent_in_p1": expected_absent_in_p1,
        "missing_expected": missing_expected,
        "min_shared": min_shared,
        "refusals": refusals,
        "rows": rows,
        "differs": differs,
        "near_ulp_accepted": near_ulp,
        "reproduces": not differs and not refusals and bool(compared),
    }


# --------------------------------------------------------------------------
# RULE-P2A section 5.5: the folds, on constructed inputs, before any leg
# --------------------------------------------------------------------------

BASE = ("PROBE_BEGIN\nstatus=numerical_error\nmajor_iters=7\nqp_minor_iters=7248\n"
        "f=41737.543107411046\nstationarity=0.6386400287759757\n"
        "feasibility_i=0\nrestoration_sub_status=not_run\nPROBE_END\n")
# the constructed rows' own surface: the folds declare their expectation the
# same way a real leg does, from the pass-1 side, never from the leg.
BASE_KEYS = frozenset(("status", "major_iters", "qp_minor_iters", "f",
                       "stationarity", "feasibility_i",
                       "restoration_sub_status"))
BASE_N = len(BASE_KEYS)


def folds(emit):
    ok = True

    def check(name, cond, detail=""):
        nonlocal ok
        emit(f"  {'PASS' if cond else 'FAIL'}  {name}{('  -- ' + detail) if detail else ''}")
        ok = ok and cond

    def g(p1, leg, expected=BASE_KEYS, min_shared=BASE_N):
        return gate(p1, leg, expected=expected, min_shared=min_shared)

    emit("--- RULE-P2A section 5.5 FOLDS (constructed inputs, no leg read)")
    emit("    the constructed folds declare their own expected set (the 7 fields of")
    emit("    the BASE row) and their own floor; the REAL legs below use the pinned")
    emit("    %d-name manifest and the floor of %d." % (len(EXPECTED_MANIFEST), MIN_SHARED))
    r = g(BASE, BASE)
    check("identical rows reproduce", r["reproduces"] and not r["differs"],
          f"{len(r['compared'])} fields compared")

    r = g(BASE, BASE.replace("major_iters=7", "major_iters=8"))
    check("one changed integer counter STOPS and is named",
          not r["reproduces"] and r["differs"] == ["major_iters"])

    r = g(BASE, BASE.replace("f=41737.543107411046", "f=41737.543107411050"))
    check("a float INSIDE the bar is WITHIN and accepted",
          r["reproduces"] and r["near_ulp_accepted"] == 1)

    r = g(BASE, BASE.replace("stationarity=0.6386400287759757",
                             "stationarity=0.63864002870769243"))
    check("a float OUTSIDE the bar STOPS -- the case the smoke missed",
          not r["reproduces"] and r["differs"] == ["stationarity"],
          "delta 6.8e-11 on 0.64 exceeds 1e-12*max(1,|a|,|b|)")

    r = g(BASE, BASE.replace("feasibility_i=0", "feasibility_i=nan"))
    check("nan against a number STOPS", not r["reproduces"] and r["differs"] == ["feasibility_i"])

    r = g(BASE.replace("feasibility_i=0", "feasibility_i=nan"),
          BASE.replace("feasibility_i=0", "feasibility_i=nan"))
    check("nan against nan is EQUAL_BYTES and passes", r["reproduces"])

    r = g(BASE, BASE.replace("stationarity=0.6386400287759757\n", ""), min_shared=1)
    check("a field the leg does not print is ABSENT, never agreement",
          "stationarity" in r["absent_in_leg"] and "stationarity" not in r["compared"])

    r = g(BASE, BASE.replace("status=numerical_error", "status=optimal"))
    check("a changed STATUS string STOPS", not r["reproduces"] and r["differs"] == ["status"])

    r = g(BASE, "PROBE_BEGIN\nPROBE_END\n")
    check("an empty leg row cannot reproduce", not r["reproduces"])

    # -- M7 W2 M4: THE STRICT MODE, proved to REJECT what the ordinary mode
    # -- accepts. A mode that behaved identically to the one beside it would be
    # -- a label, not a bar (RULE.md section 5.6).
    emit("    -- M7 W2 M4: the STRICT neutrality bar (RULE.md section 5.5)")
    within = BASE.replace("f=41737.543107411046", "f=41737.543107411050")
    r_loose = gate(BASE, within, expected=BASE_KEYS, min_shared=BASE_N, strict=False)
    r_strict = gate(BASE, within, expected=BASE_KEYS, min_shared=BASE_N, strict=True)
    check("ordinary mode ACCEPTS a WITHIN", r_loose["reproduces"] and
          r_loose["near_ulp_accepted"] == 1)
    check("STRICT mode REJECTS the SAME WITHIN, and names the field",
          not r_strict["reproduces"] and r_strict["differs"] == ["f"],
          "the two modes disagree on this input, which is what makes strict a bar")
    r_strict_same = gate(BASE, BASE, expected=BASE_KEYS, min_shared=BASE_N, strict=True)
    check("STRICT mode still ACCEPTS two byte-identical rows",
          r_strict_same["reproduces"] and r_strict_same["near_ulp_accepted"] == 0,
          "strict is a tolerance bar, not a refusal of everything")

    # -- M7 W2 M4 AMENDMENT 1 (RULE.md section 5.5a): the by-name exclusion
    # -- excludes what it says and NOTHING ELSE. A prefix rule that also ate a
    # -- neighbouring field would hide a real difference behind a label.
    emit("    -- M7 W2 M4: the arm-key exclusion (RULE.md AMENDMENT 1, section 5.5a)")
    armed = BASE.replace("PROBE_BEGIN\n",
                         "PROBE_BEGIN\narm=E\narm_min_steps=41\narmour=1\n")
    plain = BASE.replace("PROBE_BEGIN\n",
                         "PROBE_BEGIN\narm=off\narm_min_steps=0\narmour=1\n")
    keys2 = frozenset(BASE_KEYS)
    r_no = gate(plain, armed, expected=keys2, min_shared=BASE_N, strict=True)
    check("WITHOUT the exclusion the arm keys DIFFER and stop the leg",
          not r_no["reproduces"] and set(r_no["differs"]) == {"arm", "arm_min_steps"},
          "which is the defect AMENDMENT 1 exists to fix")
    r_ex = gate(plain, armed, expected=keys2, min_shared=BASE_N, strict=True,
                exclude_prefixes=("arm",))
    check("WITH it the leg reproduces and the two keys are reported excluded",
          r_ex["reproduces"] and
          set(r_ex["excluded_by_prefix"]) == {"arm", "arm_min_steps"})
    check("the prefix does NOT eat a neighbouring field whose name merely starts the same",
          "armour" in r_ex["compared"] and "armour" not in r_ex["excluded_by_prefix"],
          "`armour` is neither `arm` nor `arm_...`")
    r_ex_bad = gate(plain, armed.replace("major_iters=7", "major_iters=8"),
                    expected=keys2, min_shared=BASE_N, strict=True,
                    exclude_prefixes=("arm",))
    check("and with it a REAL difference beside the excluded keys still STOPS",
          not r_ex_bad["reproduces"] and r_ex_bad["differs"] == ["major_iters"])

    k1, _, _ = compare_field("0", "0")
    k2, _, _ = compare_field("0.5", "0.5")
    k3, _, _ = compare_field("not_run", "not_run")
    check("the class rule: '0' byte, '0.5' residual, 'not_run' byte",
          (k1, k2, k3) == ("byte", "residual", "byte"))

    # -- FIX ROUND 1 (settler adjudication X1): the three new REFUSALS, each
    # -- proved ALONE, on constructed inputs, before any leg is read.
    emit("    -- fix round 1 (X1): the refusals that make the gate non-vacuous")
    one_field = "PROBE_BEGIN\nstatus=numerical_error\nPROBE_END\n"
    r = g(BASE, one_field, expected=frozenset(("status",)))
    check("R-FLOOR: a ONE-FIELD intersection is REFUSED, not a reproduction",
          not r["reproduces"] and not r["differs"] and not r["missing_expected"]
          and len(r["refusals"]) == 1 and r["refusals"][0].startswith("R-FLOOR"),
          "1 compared field against a floor of %d -- the exact defect the review "
          "found" % BASE_N)

    r = g(BASE, BASE.replace("stationarity=0.6386400287759757\n", ""), min_shared=1)
    check("R-EXPECT: an EXPECTED field the leg does not print is REFUSED",
          not r["reproduces"] and not r["differs"]
          and r["missing_expected"] == ["stationarity"]
          and len(r["refusals"]) == 1 and r["refusals"][0].startswith("R-EXPECT"),
          "the same row the ABSENT fold above reports: reported AND refused")

    dup_leg = BASE.replace("major_iters=7\n", "major_iters=9\nmajor_iters=7\n")
    r = g(BASE, dup_leg)
    check("R-DUP: a CONFLICTING duplicate key is REFUSED even when the last "
          "value agrees",
          not r["reproduces"] and not r["differs"] and not r["missing_expected"]
          and r["duplicates"] == ["major_iters"]
          and len(r["refusals"]) == 1 and r["refusals"][0].startswith("R-DUP"),
          "the leg prints major_iters twice, 9 then 7; 7 agrees with pass 1")

    r = g(BASE, BASE, expected=BASE_KEYS | frozenset(("elastic_activations",)))
    check("an expected field the PASS-1 ROW ITSELF lacks stays ABSENT, not a "
          "refusal",
          r["reproduces"] and r["expected_absent_in_p1"] == ["elastic_activations"]
          and not r["missing_expected"],
          "RULE-P2 section 5.1's two unprinted byte fields are this case")

    check("the pinned manifest and the declared floor agree",
          len(EXPECTED_MANIFEST) == MIN_SHARED == 58,
          "manifest %d names, floor %d" % (len(EXPECTED_MANIFEST), MIN_SHARED))

    emit(f"  FOLDS {'GREEN' if ok else 'RED'}")
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--p1-rows", required=True, help="the committed pass-1 rows directory")
    ap.add_argument("--leg-dir", required=True, help="where the arm rows were written")
    ap.add_argument("--cells", required=True, help="comma-separated cell names")
    ap.add_argument("--arms", default="A,B,C")
    ap.add_argument("--rule-outcome", default="on", choices=["on", "off"],
                    help="print RULE-P2A section 6's three-arm outcome block. `off` is for a "
                         "run that is NOT the three-arm experiment (the pass-2 re-take under "
                         "arm C's build): there the per-leg verdicts are the whole answer and "
                         "an arm comparison would be a category error. The per-leg gate, the "
                         "field set, the classes and the folds are untouched by this flag.")
    ap.add_argument("--min-shared", type=int, default=MIN_SHARED,
                    help="the floor on the number of COMPARED fields; a leg with "
                         "fewer REFUSES (fix round 1, X1). Default: the 58 of P2A.")
    ap.add_argument("--note", default=None,
                    help="a line printed in the transcript header, for a replay "
                         "that must say what it is.")
    # M7 W2 M4. The reference a leg is compared against is normally the
    # COMMITTED pass-1 row; on a NEUTRALITY leg it is that cell's OWN `off` leg
    # from this same chain (RULE.md section 5.5), which lives in the leg
    # directory under the `off-` prefix. Naming it as a PATTERN rather than
    # hard-coding the prefix keeps the gate honest about what it read.
    ap.add_argument("--reference-dir", default=None,
                    help="compare against rows in THIS directory instead of --p1-rows")
    ap.add_argument("--reference-pattern", default="{cell}.txt",
                    help="the reference row's basename, with {cell} and {arm}")
    ap.add_argument("--exclude-prefix", action="append", default=[],
                    help="exclude fields named X or beginning X_ (RULE.md AMENDMENT 1, "
                         "section 5.5a). Repeatable. The exclusion is PRINTED per leg")
    ap.add_argument("--strict", action="store_true",
                    help="a WITHIN is a DIFFERENCE (RULE.md section 5.5's neutrality bar)")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    lines = []

    def emit(msg=""):
        lines.append(msg)
        print(msg)

    emit("M7 W2 M3c RULE-P2A section 5 -- THE IDENTITY GATE, and section 6's outcome")
    emit("generated by wrappers/gate_p2a.py; no solver ran here.")
    emit(f"near-ulp bar: |a-b| <= {TOL_REL:g} * max(1,|a|,|b|)   (RULE-P2 section 5.1's constant)")
    emit(f"wall-clock field names excluded by pattern: {list(WALL_CLOCK_PATTERNS)}")
    emit(f"EXPECTED field set (pinned in this file): {len(EXPECTED_MANIFEST)} names, "
         f"intersected per cell with the committed pass-1 row's own keys.")
    emit(f"REFUSALS (fix round 1, X1): R-EXPECT an expected field the leg does not "
         f"print; R-FLOOR fewer than {args.min_shared} compared fields; R-DUP a key "
         f"repeated with conflicting values.")
    if args.note:
        emit()
        for ln in args.note.split("\n"):
            emit(ln)
    emit()
    if not folds(emit):
        emit("REFUSED: the gate's own folds are not green; no leg is read.")
        with open(args.out, "w") as fh:
            fh.write("\n".join(lines) + "\n")
        return 97
    emit()

    cells = [c for c in args.cells.split(",") if c]
    arms = [a for a in args.arms.split(",") if a]
    verdicts = {}
    for arm in arms:
        for cell in cells:
            ref_dir = args.reference_dir or args.p1_rows
            p1 = os.path.join(ref_dir,
                              args.reference_pattern.format(cell=cell, arm=arm))
            leg = os.path.join(args.leg_dir, f"{arm}-{cell}.txt")
            emit("=" * 74)
            emit(f"ARM {arm}   CELL {cell}")
            emit("=" * 74)
            if not os.path.exists(p1):
                emit(f"  REFUSED: no reference row at {p1}")
                verdicts[(arm, cell)] = "REFUSED"
                continue
            if not os.path.exists(leg) or os.path.getsize(leg) == 0:
                emit(f"  CENSORED: no leg row at {leg} -- RULE-P2A section 4 fills nothing in later")
                verdicts[(arm, cell)] = "CENSORED"
                continue
            with open(p1) as fh:
                t1 = fh.read()
            with open(leg) as fh:
                t2 = fh.read()
            r = gate(t1, t2, min_shared=args.min_shared, strict=args.strict,
                     exclude_prefixes=tuple(args.exclude_prefix))
            emit(f"  fields compared (the intersection, every member): {len(r['compared'])}"
                 f"   (declared minimum: {r['min_shared']})")
            emit(f"  EXPECTED of this leg (manifest AND in the pass-1 row): "
                 f"{len(r['expected'])}")
            emit(f"  expected fields the PASS-1 ROW itself lacks (ABSENT, not required "
                 f"of the leg): {len(r['expected_absent_in_p1'])}")
            emit(f"    {','.join(r['expected_absent_in_p1']) if r['expected_absent_in_p1'] else '(none)'}")
            emit(f"  expected fields the LEG does not print (a REFUSAL): "
                 f"{len(r['missing_expected'])}")
            emit(f"    {','.join(r['missing_expected']) if r['missing_expected'] else '(none)'}")
            emit(f"  fields the leg does not print (ABSENT, never agreement): "
                 f"{len(r['absent_in_leg'])}")
            emit(f"    {','.join(r['absent_in_leg']) if r['absent_in_leg'] else '(none)'}")
            emit(f"  fields only the leg prints (not comparable): {len(r['only_in_leg'])}")
            emit(f"    {','.join(r['only_in_leg']) if r['only_in_leg'] else '(none)'}")
            emit(f"  wall-clock fields excluded by name: "
                 f"{','.join(r['excluded_wall_clock']) if r['excluded_wall_clock'] else '(none)'}")
            emit(f"  fields excluded by declared prefix {args.exclude_prefix} "
                 f"(RULE.md section 5.5a; they describe the LEG, not the solve, and "
                 f"are REPORTED below, never discarded): "
                 f"{','.join(r['excluded_by_prefix']) if r['excluded_by_prefix'] else '(none)'}")
            if args.exclude_prefix:
                fl, _dl = parse_row(t2)
                fr, _dr = parse_row(t1)
                for k in sorted(r["excluded_by_prefix"]):
                    emit(f"      {k:<32} reference={fr.get(k, '(absent)'):<22} leg={fl.get(k, '(absent)')}")
            if r["duplicates"]:
                emit(f"  DUPLICATE KEYS in a row (reported, not silently folded): "
                     f"{','.join(r['duplicates'])}")
            emit("  field                          class     verdict      pass-1 / leg")
            for k, klass, verdict, a, b, delta in r["rows"]:
                if verdict == "EQUAL_BYTES":
                    emit(f"    {k:<30} {klass:<9} {verdict:<12} {a}")
                else:
                    d = "" if delta is None else f"   d={delta!r}"
                    emit(f"    {k:<30} {klass:<9} {verdict:<12} p1={a}  leg={b}{d}")
            emit(f"  near-ulp acceptances (passed by tolerance, not by bytes): "
                 f"{r['near_ulp_accepted']}")
            if r["reproduces"]:
                emit(f"  VERDICT: REPRODUCES the committed pass-1 row on every compared "
                     f"field, with every expected field present, no conflicting "
                     f"duplicate, and {len(r['compared'])} >= {r['min_shared']} fields "
                     f"compared")
                verdicts[(arm, cell)] = "REPRODUCES"
            else:
                why = list(r["refusals"])
                if r["differs"]:
                    why.append("FIELDS DIFFER: " + ",".join(r["differs"]))
                if not why:
                    why.append("nothing comparable")
                emit("  VERDICT: DOES NOT REPRODUCE")
                for reason in why:
                    emit(f"    {reason}")
                verdicts[(arm, cell)] = "DIFFERS"
            emit()

    if args.rule_outcome == "off":
        emit("=" * 74)
        emit("PER-LEG VERDICTS (RULE-P2A section 6's ARM outcome is not printed: this run is")
        emit("not the three-arm experiment, so an arm comparison would be a category error)")
        emit("=" * 74)
        for arm in arms:
            for cell in cells:
                emit(f"  {arm}-{cell:<10} {verdicts.get((arm, cell), 'MISSING')}")
        vals = [verdicts.get((a, c), "MISSING") for a in arms for c in cells]
        emit(f"  REPRODUCES {vals.count('REPRODUCES')} / {len(vals)}   "
             f"DIFFERS {vals.count('DIFFERS')}   CENSORED {vals.count('CENSORED')}   "
             f"REFUSED {vals.count('REFUSED')}   MISSING {vals.count('MISSING')}")
        emit()
        emit("  A cell that DIFFERS is a STOP for that cell: its traces are not read as traces")
        emit("  of the pass-1 row, and nothing about it is relabelled.")
        emit("  Nothing above licenses a behaviour change, a default change, or a line of code.")
        with open(args.out, "w") as fh:
            fh.write("\n".join(lines) + "\n")
        return 0
    emit("=" * 74)
    emit("RULE-P2A section 6 -- THE PRE-REGISTERED OUTCOME, applied mechanically")
    emit("=" * 74)
    arm_ok = {}
    for arm in arms:
        vs = [verdicts.get((arm, c), "MISSING") for c in cells]
        emit(f"  ARM {arm}: " + "  ".join(f"{c}={v}" for c, v in zip(cells, vs)))
        if all(v == "REPRODUCES" for v in vs):
            arm_ok[arm] = True
        elif any(v in ("CENSORED", "REFUSED", "MISSING") for v in vs):
            arm_ok[arm] = None
        else:
            arm_ok[arm] = False
    for arm, v in arm_ok.items():
        emit(f"  ARM {arm} REPRODUCES: "
             f"{'YES' if v else ('INCOMPLETE' if v is None else 'NO')}")
    a, b, c = arm_ok.get("A"), arm_ok.get("B"), arm_ok.get("C")
    if None in (a, b, c):
        outcome = ("INCOMPLETE -- an arm has a censored or missing cell; RULE-P2A section 6 "
                   "never calls such an arm a reproduction")
    elif a and b and c:
        outcome = ("OUTCOME 1 -- flags-and-compiler were the whole cause; the seam is neutral "
                   "compiled in AND armed on whole default runs. The ten pass-2 legs are "
                   "RE-TAKEN under arm C's build, as a new chain with the identity gate armed.")
    elif a and not b:
        outcome = ("OUTCOME 2 -- arm A reproduces and arm B does not: compiling the seam IN is "
                   "not record-only on the default path. STOP; a finding about hven for the "
                   "settler.")
    elif a and b and not c:
        outcome = ("OUTCOME 3 -- arms A and B reproduce and arm C does not: ARMING the observer "
                   "is not neutral. STOP; pass 2's instrument cannot answer RULE-P2 section 0's "
                   "question by whole-run capture.")
    else:
        outcome = ("OUTCOME 4 -- arm A does not reproduce: the replay is still not the install's "
                   "build, or the difference lies elsewhere. STOP and report the residual; arms "
                   "B and C are uninterpretable.")
    emit()
    emit(f"  {outcome}")
    emit()
    emit("  Nothing above licenses a behaviour change, a default change, or a line of code "
         "(RULE-P2A section 7).")
    with open(args.out, "w") as fh:
        fh.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
