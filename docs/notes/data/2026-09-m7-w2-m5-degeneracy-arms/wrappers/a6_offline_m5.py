#!/usr/bin/env python3
"""M7 W2 M5 -- AMENDMENT A6's OFFLINE HALF, on YORKNET, from the COMMITTED
traces. Pre-registered in RULE.md section 8 step 2 and run BEFORE the dynamic
arm-a leg, so its prediction is on the record before the solver moves.

WHAT A6 PREDICTS. "OFFLINE -- replayed over the committed YORKNET traces, all
2,538 recorded core drops fail the scaled test (computable now, no solver; do
it in Task B's read-out as a pre-registered check)."

WHAT "FAILS THE SCALED TEST" MEANS, spelled out so the reading is not a matter
of taste. Arm A admits a drop candidate only when

        |lambda|  >  opt_tol * scale

with `scale` the pass's own published stationarity scale. Production's test is
the ABSOLUTE one, `|lambda| > opt_tol`. A candidate that production admitted and
the SCALED test would refuse is a drop arm A prevents. A6 predicts that EVERY
core drop in YORKNET's recorded walk is such a candidate -- i.e. that the
scaled test FAILS (refuses) on all of them.

Every input is read out of a flat trace this task did not produce: M4's own OFF
walks, the walks of the very rows M4b compared against. NO SOLVER RUNS HERE.

WHAT IS DERIVED, AND BY WHOM.
  * THE CORE DROP SET is `m4c_tails.py`'s, imported UNMODIFIED from this
    directory's BYTE COPY of M4c's own file. This script rebuilds the per-unit
    list with `m4c_tails`' OWN `unit_of`, its OWN `maximal_periodic_tail`, its
    OWN recurrence predicate and its OWN CORE predicate, and then CHECKS its
    per-solve core count against the `core_readd_units` that `m4c_tails.main()`
    itself writes into `tails.json`. A disagreement REFUSES. Nothing about the
    core set is derived by hand.
  * THE THREE NUMBERS THE TEST NEEDS are read from the trace row of the DROP
    minor: `decision_multiplier` (the candidate's |lambda|, which is the
    quantity production compared), `decision_tol` (the engine's own `opt_tol`
    at that pass) and `stat_scale` (the published stationarity scale, floored
    at 1 by `free_block_stationarity` itself). The first two are already on
    `m4c_tails`' unit as `lambda_abs` and `decision_tol`; `stat_scale` is
    joined back from the raw row by (solve, minor).

WHAT IT REPORTS, whatever the answer: the core count, how many core drops the
scaled test REFUSES, how many it still ADMITS, the distribution of the ratio
|lambda| / (opt_tol * scale), and the rows where `stat_scale` was not published
(NaN or < 1) -- which arm A's own `drop_scale()` degrades to 1.0 for, and which
this script therefore counts separately rather than dropping.

A6 IS A PREDICTION, NOT A GATE. A refutation here is REPORTED as a refutation
and changes nothing about the legs; RULE.md section 9 already says what a green
result would not license.

Usage: a6_offline_m5.py OUTDIR TRACE [TRACE ...]
"""

import json
import math
import os
import subprocess
import sys
from collections import OrderedDict

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import m4c_tails as mt  # noqa: E402
import classify_cause_fix1 as cc  # noqa: E402


def fnum(t):
    return mt.fnum(t)


def core_units_of(path):
    """m4c_tails' OWN construction, called through m4c_tails' own functions.

    Returns (cell, {solve: [ (unit, drop_raw_row) ]}).
    """
    header, raws, typed = cc.load_trace(path)
    cell = header.get("cell", os.path.basename(path))
    n = int(header["n"])
    me = int(header["me"])
    by = OrderedDict()
    for r in raws:
        by.setdefault(int(r["solve"]), []).append(r)
    out = OrderedDict()
    for k, rs in by.items():
        keys = [tuple(cc.bitkey(c, r[c]) for c in cc.F0) for r in rs]
        p, t0, _nfail = mt.maximal_periodic_tail(keys)
        N = len(rs)
        if p is None:
            continue
        tl = N - t0
        if not ((tl >= 0.10 * N) and (tl >= 8 * p)):
            continue
        tail = rs[t0:]
        units, add_actions, drop_rows = [], [], []
        for i in range(len(tail) - 1):
            if tail[i]["action"] in mt.DROP and tail[i + 1]["post_drop"] == "1":
                units.append(mt.unit_of(tail[i], tail[i + 1], n, me))
                add_actions.append(tail[i + 1]["action"])
                drop_rows.append(tail[i])
        core = [(u, d) for u, d, act in zip(units, drop_rows, add_actions)
                if act in mt.ADD and u["added"] == u["released"]
                and u["add_route"] == "none"]
        if core:
            out[k] = core
    return cell, out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    outdir = os.path.abspath(sys.argv[1])
    traces = [os.path.abspath(p) for p in sys.argv[2:]]
    os.makedirs(outdir, exist_ok=True)
    lines = []

    def emit(m=""):
        lines.append(m)
        print(m)

    emit("M7 W2 M5 -- AMENDMENT A6, THE OFFLINE HALF. No solver ran here.")
    emit("the scaled test admits iff |lambda| > opt_tol * scale; A6 predicts it")
    emit("REFUSES every recorded core drop.")
    emit()

    # --- 1. m4c_tails' OWN tails.json, produced by m4c_tails' OWN main() ----
    tails_dir = os.path.join(outdir, "m4c-tails")
    os.makedirs(tails_dir, exist_ok=True)
    cmd = [sys.executable, os.path.join(_HERE, "m4c_tails.py"), tails_dir] + traces
    emit("--- m4c_tails.py, run as itself (the authoritative core count)")
    emit("    " + " ".join(cmd))
    rc = subprocess.run(cmd, capture_output=True, text=True)
    emit("    rc=%d" % rc.returncode)
    for ln in rc.stdout.strip().split("\n"):
        emit("    | " + ln)
    if rc.returncode != 0:
        emit("REFUSED: m4c_tails.py did not run; nothing below can be checked "
             "against it")
        emit(rc.stderr.strip()[:2000])
        with open(os.path.join(outdir, "a6-offline.txt"), "w") as fh:
            fh.write("\n".join(lines) + "\n")
        return 91
    with open(os.path.join(tails_dir, "tails.json")) as fh:
        tails = json.load(fh)
    authoritative = {(r["cell"], r["solve"]): r.get("core_readd_units", 0)
                     for r in tails}
    emit()

    total_core = 0
    total_refused = 0
    total_admitted = 0
    total_unpublished = 0
    ratios = []
    per_cell = OrderedDict()
    for path in traces:
        cell, solves = core_units_of(path)
        emit("=" * 70)
        emit("CELL %s   trace %s" % (cell, path))
        emit("=" * 70)
        c_core = c_ref = c_adm = c_unpub = 0
        for solve, core in solves.items():
            want = authoritative.get((cell, solve))
            if want is None or want != len(core):
                emit("REFUSED: solve %d -- this script counts %d core unit(s) and "
                     "m4c_tails' own tails.json says %s. The core set is "
                     "m4c_tails', not this script's, and a disagreement is a "
                     "refusal rather than a choice." % (solve, len(core), want))
                with open(os.path.join(outdir, "a6-offline.txt"), "w") as fh:
                    fh.write("\n".join(lines) + "\n")
                return 92
            for u, d in core:
                c_core += 1
                lam = u["lambda_abs"]
                tol = u["decision_tol"]
                scale_txt = d.get("stat_scale", "")
                scale = fnum(scale_txt)
                published = (scale is not None and not math.isnan(scale)
                             and scale >= 1.0)
                if not published:
                    # arm A's OWN degradation: `drop_scale()` returns 1.0 and
                    # counts the pass. Counted apart AND carried at 1.0, so the
                    # total is the arm's behaviour and not a filtered subset.
                    c_unpub += 1
                    scale = 1.0
                bar = tol * scale
                admits = lam > bar
                if admits:
                    c_adm += 1
                else:
                    c_ref += 1
                if bar > 0.0 and lam is not None:
                    ratios.append(lam / bar)
            emit("  solve %-5d core units %-6d (m4c_tails agrees: %d)"
                 % (solve, len(core), want))
        emit("  CORE DROPS                 %d" % c_core)
        emit("  the scaled test REFUSES    %d" % c_ref)
        emit("  the scaled test ADMITS     %d" % c_adm)
        emit("  no scale published (arm A degrades to 1.0, counted here) %d" % c_unpub)
        per_cell[cell] = (c_core, c_ref, c_adm, c_unpub)
        total_core += c_core
        total_refused += c_ref
        total_admitted += c_adm
        total_unpublished += c_unpub
        emit()

    emit("=" * 70)
    emit("A6 OFFLINE -- THE PREDICTION AGAINST THE TRACES")
    emit("=" * 70)
    emit("  core drops examined              %d" % total_core)
    emit("  the scaled test REFUSES          %d" % total_refused)
    emit("  the scaled test STILL ADMITS     %d" % total_admitted)
    emit("  passes with no published scale   %d" % total_unpublished)
    if ratios:
        rs = sorted(ratios)
        def q(f):
            return rs[min(len(rs) - 1, int(f * (len(rs) - 1)))]
        emit("  |lambda| / (opt_tol*scale):  min %.6g  p50 %.6g  p90 %.6g  "
             "max %.6g" % (rs[0], q(0.5), q(0.9), rs[-1]))
    emit()
    if total_core == 0:
        emit("  VERDICT: NO CORE DROP was found in these traces. A6's offline half")
        emit("           is UNTESTED by this trajectory -- not confirmed and not")
        emit("           refuted -- and it is reported as untested.")
    elif total_admitted == 0:
        emit("  VERDICT: A6's offline prediction HOLDS on these traces -- the scaled")
        emit("           test refuses every one of the %d recorded core drops." % total_core)
    else:
        emit("  VERDICT: A6's offline prediction is REFUTED on these traces -- %d of"
             % total_admitted)
        emit("           %d core drops are STILL ADMITTED by the scaled test."
             % total_core)
        emit("           Reported as a refutation. It changes nothing about the legs;")
        emit("           RULE.md section 9 says what a result does not license.")
    emit()
    emit("  This is an OFFLINE replay over committed traces. It says what arm A's")
    emit("  test would have decided at each recorded drop; it does not say what a")
    emit("  walk that took different drops would then have done. That is the")
    emit("  DYNAMIC half, and it is the leg that follows.")
    with open(os.path.join(outdir, "a6-offline.txt"), "w") as fh:
        fh.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
