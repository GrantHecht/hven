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
"""

import argparse
import math
import os
import re
import sys

TOL_REL = 1e-12
WALL_CLOCK_PATTERNS = ("wall", "seconds_elapsed", "time")
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


def gate(p1_text, p2_text):
    """The whole gate for one leg. Returns a dict of everything it found."""
    f1, d1 = parse_row(p1_text)
    f2, d2 = parse_row(p2_text)
    common = sorted(set(f1) & set(f2))
    compared = [k for k in common if not is_wall_clock(k)]
    excluded = [k for k in common if is_wall_clock(k)]
    rows, differs, near_ulp = [], [], 0
    for k in compared:
        klass, verdict, delta = compare_field(f1[k], f2[k])
        rows.append((k, klass, verdict, f1[k], f2[k], delta))
        if verdict in ("DIFFERS", "EXCEEDS"):
            differs.append(k)
        elif verdict == "WITHIN":
            near_ulp += 1
    return {
        "compared": compared,
        "excluded_wall_clock": excluded,
        "absent_in_leg": sorted(set(f1) - set(f2)),
        "only_in_leg": sorted(set(f2) - set(f1)),
        "duplicates": sorted(set(d1) | set(d2)),
        "rows": rows,
        "differs": differs,
        "near_ulp_accepted": near_ulp,
        "reproduces": not differs and bool(compared),
    }


# --------------------------------------------------------------------------
# RULE-P2A section 5.5: the folds, on constructed inputs, before any leg
# --------------------------------------------------------------------------

BASE = ("PROBE_BEGIN\nstatus=numerical_error\nmajor_iters=7\nqp_minor_iters=7248\n"
        "f=41737.543107411046\nstationarity=0.6386400287759757\n"
        "feasibility_i=0\nrestoration_sub_status=not_run\nPROBE_END\n")


def folds(emit):
    ok = True

    def check(name, cond, detail=""):
        nonlocal ok
        emit(f"  {'PASS' if cond else 'FAIL'}  {name}{('  -- ' + detail) if detail else ''}")
        ok = ok and cond

    emit("--- RULE-P2A section 5.5 FOLDS (constructed inputs, no leg read)")
    r = gate(BASE, BASE)
    check("identical rows reproduce", r["reproduces"] and not r["differs"],
          f"{len(r['compared'])} fields compared")

    r = gate(BASE, BASE.replace("major_iters=7", "major_iters=8"))
    check("one changed integer counter STOPS and is named",
          not r["reproduces"] and r["differs"] == ["major_iters"])

    r = gate(BASE, BASE.replace("f=41737.543107411046", "f=41737.543107411050"))
    check("a float INSIDE the bar is WITHIN and accepted",
          r["reproduces"] and r["near_ulp_accepted"] == 1)

    r = gate(BASE, BASE.replace("stationarity=0.6386400287759757",
                                "stationarity=0.63864002870769243"))
    check("a float OUTSIDE the bar STOPS -- the case the smoke missed",
          not r["reproduces"] and r["differs"] == ["stationarity"],
          "delta 6.8e-11 on 0.64 exceeds 1e-12*max(1,|a|,|b|)")

    r = gate(BASE, BASE.replace("feasibility_i=0", "feasibility_i=nan"))
    check("nan against a number STOPS", not r["reproduces"] and r["differs"] == ["feasibility_i"])

    r = gate(BASE.replace("feasibility_i=0", "feasibility_i=nan"),
             BASE.replace("feasibility_i=0", "feasibility_i=nan"))
    check("nan against nan is EQUAL_BYTES and passes", r["reproduces"])

    r = gate(BASE, BASE.replace("stationarity=0.6386400287759757\n", ""))
    check("a field the leg does not print is ABSENT, never agreement",
          "stationarity" in r["absent_in_leg"] and "stationarity" not in r["compared"])

    r = gate(BASE, BASE.replace("status=numerical_error", "status=optimal"))
    check("a changed STATUS string STOPS", not r["reproduces"] and r["differs"] == ["status"])

    r = gate(BASE, "PROBE_BEGIN\nPROBE_END\n")
    check("an empty leg row cannot reproduce", not r["reproduces"])

    k1, _, _ = compare_field("0", "0")
    k2, _, _ = compare_field("0.5", "0.5")
    k3, _, _ = compare_field("not_run", "not_run")
    check("the class rule: '0' byte, '0.5' residual, 'not_run' byte",
          (k1, k2, k3) == ("byte", "residual", "byte"))

    emit(f"  FOLDS {'GREEN' if ok else 'RED'}")
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--p1-rows", required=True, help="the committed pass-1 rows directory")
    ap.add_argument("--leg-dir", required=True, help="where the arm rows were written")
    ap.add_argument("--cells", required=True, help="comma-separated cell names")
    ap.add_argument("--arms", default="A,B,C")
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
            p1 = os.path.join(args.p1_rows, f"{cell}.txt")
            leg = os.path.join(args.leg_dir, f"{arm}-{cell}.txt")
            emit("=" * 74)
            emit(f"ARM {arm}   CELL {cell}")
            emit("=" * 74)
            if not os.path.exists(p1):
                emit(f"  REFUSED: no committed pass-1 row at {p1}")
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
            r = gate(t1, t2)
            emit(f"  fields compared (the intersection, every member): {len(r['compared'])}")
            emit(f"  fields the leg does not print (ABSENT, never agreement): "
                 f"{len(r['absent_in_leg'])}")
            emit(f"    {','.join(r['absent_in_leg']) if r['absent_in_leg'] else '(none)'}")
            emit(f"  fields only the leg prints (not comparable): {len(r['only_in_leg'])}")
            emit(f"    {','.join(r['only_in_leg']) if r['only_in_leg'] else '(none)'}")
            emit(f"  wall-clock fields excluded by name: "
                 f"{','.join(r['excluded_wall_clock']) if r['excluded_wall_clock'] else '(none)'}")
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
                emit("  VERDICT: REPRODUCES the committed pass-1 row on every compared field")
                verdicts[(arm, cell)] = "REPRODUCES"
            else:
                emit(f"  VERDICT: DOES NOT REPRODUCE -- {','.join(r['differs'])}")
                verdicts[(arm, cell)] = "DIFFERS"
            emit()

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
