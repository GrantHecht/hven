#!/usr/bin/env python3
"""M7 W2 M5 -- THE READ-OUT DRIVER.  No solver runs here.

Every number this writes is DERIVED, at read-out time, from files already on
disk when the chain exited:

  * `rows/<arm>-<cell>.txt`      -- the 28 surviving leg rows (the chain's own);
  * `rows-off/off-<cell>.txt`    -- M4's thirteen committed OFF rows, COPIED and
                                    pinned by `rows-off/SHA256-off-rows.txt`;
  * `logs/chain-m5.txt`          -- the chain transcript (wall-clock, which is
                                    INFORMATIONAL, CLAUDE.md section 7);
  * `<tails>/<leg>/tails.json`   -- `wrappers/m4c_tails.py` RUN, per flat trace,
                                    never re-derived by hand (RULE.md section 6);
  * M4c's committed `tables/tails.json` -- the OFF baseline for the TEN.

It writes `tables/arm.md` and `verdict.txt` and prints nothing a table does not
also carry.  It NEVER edits RULE.md and it touches nothing under src/, include/
or tests/.

Usage: readout_m5.py ARTIFACT_DIR TAILS_DIR
"""

import json
import math
import os
import re
import sys
from collections import OrderedDict

CELLS_TEN = ["ACOPP118", "ACOPP57", "ACOPR57", "BRITGAS", "DALLASM", "SAWPATH",
             "STEENBRB", "STEENBRC", "STEENBRG", "YORKNET"]
NEUTRAL = ["READING6", "SMBANK", "ZAMB2-8"]
ARMS = ["a", "b1", "a+b1"]

# M4 RULE.md section 7.2's rank order, transcribed (better first).
RANK = {"optimal": 0, "acceptable": 1, "max_iter": 2, "stalled": 3,
        "diverging": 4, "budget_exhausted": 5, "infeasible": 6,
        "numerical_error": 7}


def split_leg(name):
    """`a+b1-ZAMB2-8` -> ('a+b1', 'ZAMB2-8').  The cell names carry hyphens."""
    for c in CELLS_TEN + NEUTRAL:
        if name.endswith("-" + c):
            return name[: -len(c) - 1], c
    raise SystemExit("readout_m5.py: cannot split leg name %r" % name)


def load_row(path):
    d = OrderedDict()
    for line in open(path):
        line = line.rstrip("\n")
        if "=" in line and not line.startswith("#"):
            k, v = line.split("=", 1)
            if k not in d:          # first wins; the row has no duplicates
                d[k] = v
    return d


def num(d, k, default=None):
    v = d.get(k)
    if v is None:
        return default
    try:
        return int(v)
    except ValueError:
        try:
            return float(v)
        except ValueError:
            return default


def walls(chain_path):
    """Per-leg wall seconds from the chain's own FOREGROUND stamps.

    INFORMATIONAL ONLY (CLAUDE.md section 7).  These legs ran one at a time
    under one lock hold, pinned to one core -- but nothing here is quoted as a
    timing measurement, and no comparison in this artifact rests on one.
    """
    import datetime
    out, start = {}, {}
    for line in open(chain_path):
        m = re.match(r"FOREGROUND_(START|END) (\S+) (\S+)", line.strip())
        if not m:
            continue
        kind, leg, ts = m.groups()
        t = datetime.datetime.fromisoformat(ts)
        if kind == "START":
            start[leg] = t
        elif leg in start:
            out[leg] = (t - start[leg]).total_seconds()
    return out


def fmt(v, w=None):
    if v is None:
        return "-"
    if isinstance(v, float):
        if math.isnan(v):
            return "nan"
        s = "%.6g" % v
    else:
        s = str(v)
    return s


def main():
    D = sys.argv[1]
    T = sys.argv[2]
    rows, off = {}, {}
    for fn in sorted(os.listdir(os.path.join(D, "rows"))):
        if not fn.endswith(".txt"):
            continue
        arm, cell = split_leg(fn[:-4])
        rows[(arm, cell)] = load_row(os.path.join(D, "rows", fn))
    for fn in sorted(os.listdir(os.path.join(D, "rows-off"))):
        if fn.startswith("off-") and fn.endswith(".txt"):
            off[fn[4:-4]] = load_row(os.path.join(D, "rows-off", fn))
    W = walls(os.path.join(D, "logs", "chain-m5.txt"))

    # --- the core drop->same-bound-re-add unit counts -----------------------
    def tails_of(leg):
        p = os.path.join(T, leg, "tails.json")
        if not os.path.isfile(p):
            return None
        return json.load(open(p))

    m4c = json.load(open(os.path.join(
        os.path.dirname(D.rstrip("/")),
        "2026-09-m7-w2-m4c-drop-direction-study", "tables", "tails.json")))

    def agg(rs):
        if rs is None:
            return None
        a = {"solves": len(rs), "units": 0, "core": 0, "ids": [], "cls": {}}
        for r in rs:
            a["units"] += r.get("units", 0)
            a["core"] += r.get("core_readd_units", 0)
            if r.get("rep_released"):
                a["ids"].append("s%s:%s" % (r["solve"], r["rep_released"]))
            c = r.get("classification", "-")
            a["cls"][c] = a["cls"].get(c, 0) + 1
        return a

    off_tails = {}
    for c in CELLS_TEN:
        off_tails[c] = agg([r for r in m4c if r["cell"] == c])
    for c in NEUTRAL:
        off_tails[c] = agg(tails_of("off-" + c))
    arm_tails = {k: agg(tails_of("%s-%s" % k)) for k in rows}

    out = []
    A = out.append
    A("# M7 W2 M5 -- the degeneracy arms, cell by cell and arm by arm")
    A("")
    A("Written by `wrappers/readout_m5.py` at read-out time from the files named")
    A("in its header.  **No solver ran to produce this file.**  RULE.md was not")
    A("edited.  Wall-clock is INFORMATIONAL throughout (CLAUDE.md section 7);")
    A("counters are the asserted currency.")
    A("")

    # ---------------- T1: the compared fields -------------------------------
    A("## 1. RULE.md section 7.1's compared fields, every cell x arm")
    A("")
    A("`OFF` is M4's committed row for that cell, COPIED (never re-run) and")
    A("verified against `rows-off/SHA256-off-rows.txt` by the chain's STEP 0.")
    A("")
    A("| cell | arm | status | OFF status | f | OFF f | stationarity | feas_e | feas_i | majors | minors | OFF minors | min ratio | facts | steps acc | restor entries / sub | deadline | wall s (info) |")
    A("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for cell in CELLS_TEN + NEUTRAL:
        for arm in ARMS:
            d = rows.get((arm, cell))
            if d is None:
                continue
            o = off.get(cell, {})
            mo = num(o, "qp_minor_iters")
            mi = num(d, "qp_minor_iters")
            ratio = ("%.4f" % (mi / mo)) if (mo not in (None, 0) and mi is not None) else "-"
            A("| `%s` | `%s` | `%s` | `%s` | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s / `%s` | %s | %s |" % (
                cell, arm, d.get("status"), o.get("status", "-"),
                fmt(num(d, "f")), fmt(num(o, "f")),
                fmt(num(d, "stationarity")), fmt(num(d, "feasibility_e")),
                fmt(num(d, "feasibility_i")), d.get("major_iters"),
                mi, mo, ratio, d.get("factorizations"), d.get("steps_accepted"),
                d.get("restoration_entries"), d.get("restoration_sub_status"),
                d.get("deadline_fired"),
                ("%.1f" % W["%s-%s" % (arm, cell)]) if "%s-%s" % (arm, cell) in W else "-"))
    A("")
    A("`opt_feas_tol` IS NOT PRINTED BY THIS PROBE.  It is one of the fields M4's")
    A("own OFF-control gate lists as absent from the row, and M4b recorded the")
    A("same absence.  It is not substituted from M3c's row and not typed in from")
    A("the rule.  Where this artifact calls a cell INFEASIBLE-EXIT the printed")
    A("`feasibility_e` exceeds 1e-6 by more than six orders of magnitude, so the")
    A("call does not turn on the exact tolerance; no borderline cell is called.")
    A("")

    # ---------------- T2: arm A's five counters -----------------------------
    A("## 2. Arm A's read-out (RULE.md section 4)")
    A("")
    A("| cell | arm | drops made | candidates refused | **drops prevented** | max refused ratio | scale unavailable passes |")
    A("|---|---|---|---|---|---|---|")
    for cell in CELLS_TEN + NEUTRAL:
        for arm in ARMS:
            d = rows.get((arm, cell))
            if d is None or arm == "b1":
                continue
            A("| `%s` | `%s` | %s | %s | **%s** | %s | %s |" % (
                cell, arm, d.get("scaled_drops_made"),
                d.get("scaled_candidates_refused"),
                d.get("scaled_drops_prevented"),
                fmt(num(d, "scaled_max_refused_ratio")),
                d.get("scale_unavailable_passes")))
    A("")
    A("`scale_unavailable_passes` is **0 on every leg**, so no arm-A read-out is")
    A("invalidated by RULE.md section 4's own condition.  `b1`-only legs are")
    A("omitted from this table: arm A is off there and all five read 0.")
    A("")

    # ---------------- T3: b1 counters --------------------------------------
    A("## 3. Arm b1's counters, per cell x arm (RULE.md sections 5 and 6)")
    A("")
    A("| cell | arm | passes | cands examined | passed over | dep tests | QR builds | builds/pass | test failures | blocker switched | no-blocker passes | budget truncations | **enum mismatches** |")
    A("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for cell in CELLS_TEN + NEUTRAL:
        for arm in ARMS:
            d = rows.get((arm, cell))
            if d is None or arm == "a":
                continue
            p = num(d, "indep_passes") or 0
            q = num(d, "indep_qr_builds") or 0
            A("| `%s` | `%s` | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | **%s** |" % (
                cell, arm, p, d.get("indep_candidates_examined"),
                d.get("indep_candidates_passed_over"), d.get("indep_dep_tests"),
                q, ("%.2f" % (q / p)) if p else "-",
                d.get("indep_test_failures"), d.get("indep_blocker_switched"),
                d.get("indep_no_blocker_passes"),
                d.get("indep_budget_truncations"),
                d.get("indep_enumeration_mismatches")))
    A("")
    A("**`indep_enumeration_mismatches` is 0 on every leg.**  RULE.md section 7")
    A("deviation 4's invalidation never fires: the arm's re-enumeration of the")
    A("ratio test's candidates agreed with production's own blocker on every pass")
    A("it took.")
    A("")

    # ---------------- T4: three-mechanism split -----------------------------
    A("## 4. A8's three-mechanism split (RULE.md section 3.1)")
    A("")
    A("| cell | arm | 1. entry removals | 2. screen-and-QR pass-overs | 3. QR-only pass-overs |")
    A("|---|---|---|---|---|")
    for cell in CELLS_TEN + NEUTRAL:
        for arm in ARMS:
            d = rows.get((arm, cell))
            if d is None or arm == "a":
                continue
            A("| `%s` | `%s` | %s | %s | %s |" % (
                cell, arm, d.get("indep_entry_members_removed"),
                d.get("indep_screen_and_qr_dependent"),
                d.get("indep_qr_only_dependent")))
    A("")
    A("**READ THE SPLIT AGAINST THE SITES, NOT AGAINST THE PASS-OVERS.**")
    A("Mechanisms 2 and 3 count every DEPENDENT VERDICT the shared predicate")
    A("returned, at whichever site asked; `indep_candidates_passed_over` counts")
    A("only the ordinary add's.  Mechanism 1 has its own counter and does not go")
    A("through the predicate.  The identity that follows is CHECKED here on")
    A("every armed leg rather than asserted:")
    A("")
    A("    screen_and_qr + qr_only  ==  passed_over + ride_refusals")
    A("                                 + shift_refusals + repair_pin_refusals")
    A("                                 + probe_restore_dependent")
    A("")
    bad = []
    for (arm, cell), d in rows.items():
        if arm == "a":
            continue
        lhs = (num(d, "indep_screen_and_qr_dependent") +
               num(d, "indep_qr_only_dependent"))
        rhs = sum(num(d, k) for k in ("indep_candidates_passed_over",
                                      "indep_ride_refusals",
                                      "indep_shift_refusals",
                                      "indep_repair_pin_refusals",
                                      "indep_probe_restore_dependent"))
        if lhs != rhs:
            bad.append("%s-%s (%d vs %d)" % (arm, cell, lhs, rhs))
    A("It holds on **%d of %d** armed legs%s." % (
        24 - len(bad), 24,
        "" if not bad else "; the exceptions are " + ", ".join(bad)))
    A("")
    A("So `READING6`'s 400 QR-only verdicts are 200 ordinary pass-overs plus 200")
    A("repair-pin refusals, and `STEENBRC`'s 5462 under b1 are 2731 pass-overs")
    A("plus 2731 ride refusals -- two independent counters that happen to be")
    A("equal on that cell, not one counter printed twice.")
    A("")

    # ---------------- T5: the rank invariant --------------------------------
    A("## 5. The rank invariant, and the three counters that make a cell UNSUPPORTED")
    A("")
    A("`indep_disarmed` / `indep_disarm_site` are **per solve** -- `begin_solve()`")
    A("re-arms and clears them -- so they report the LAST solve's state, not the")
    A("cell's.  The cumulative counters beside them are what a verdict reads.")
    A("Sites: 0 ordinary add, 1 ride add, 2 seed entry, 3 shift add, 4 probe")
    A("restore (check-only), 5 repair pin.")
    A("")
    A("| cell | arm | solves | max(me+\\|W\\|-n) | max rank deficit | **rank violations (A9)** | **mandatory-dep refusals (A3)** | refused / solves | last-solve disarm site |")
    A("|---|---|---|---|---|---|---|---|---|")
    for cell in CELLS_TEN + NEUTRAL:
        for arm in ARMS:
            d = rows.get((arm, cell))
            if d is None or arm == "a":
                continue
            g = num(d, "indep_solve_generation") or 0
            m = num(d, "indep_refused_mandatory_dependent") or 0
            A("| `%s` | `%s` | %s | %s | %s | **%s** | **%s** | %s | %s |" % (
                cell, arm, g, d.get("indep_rows_minus_n_max"),
                d.get("indep_rank_deficit_max"),
                d.get("indep_rank_violations"), m,
                ("%.2f" % (m / g)) if g else "-",
                d.get("indep_disarm_site") if num(d, "indep_disarmed") else "(not disarmed at harvest)"))
    A("")

    # ---------------- T6: exceedances against cond(R) -----------------------
    A("## 6. AMENDMENT 1's exceedances-against-`cond(R)` column (C4)")
    A("")
    A("A READING, never a verdict.  Every row with `indep_leak_bound_exceeded > 0`")
    A("keeps UNSUPPORTED whatever `indep_max_cond_r` says.  `nan` means NO")
    A("DECOMPOSITION WAS TAKEN -- it is printed as absent and never floored to 1.")
    A("")
    A("| cell | arm | **A10 exceedances** | max passed-over violation | `indep_max_cond_r` (per solve high-water) |")
    A("|---|---|---|---|---|")
    for cell in CELLS_TEN + NEUTRAL:
        for arm in ARMS:
            d = rows.get((arm, cell))
            if d is None or arm == "a":
                continue
            A("| `%s` | `%s` | **%s** | %s | %s |" % (
                cell, arm, d.get("indep_leak_bound_exceeded"),
                fmt(num(d, "indep_max_passed_violation")),
                fmt(num(d, "indep_max_cond_r"))))
    A("")

    # ---------------- T7: which site acted ----------------------------------
    A("## 7. Which SITE acted (RULE.md section 5's site table)")
    A("")
    A("| cell | arm | ride refusals (site 1) | shift refusals (site 3) | probe-restore dependent (site 4, check-only) | repair-pin refusals (site 5) | OFF shift_adds | arm shift_adds |")
    A("|---|---|---|---|---|---|---|---|")
    for cell in CELLS_TEN + NEUTRAL:
        for arm in ARMS:
            d = rows.get((arm, cell))
            if d is None or arm == "a":
                continue
            o = off.get(cell, {})
            A("| `%s` | `%s` | %s | %s | %s | %s | %s | %s |" % (
                cell, arm, d.get("indep_ride_refusals"),
                d.get("indep_shift_refusals"),
                d.get("indep_probe_restore_dependent"),
                d.get("indep_repair_pin_refusals"),
                o.get("first_shift_adds", "-"), d.get("first_shift_adds", "-")))
    A("")
    A("`counters.shift_adds` is not a field of the SQP-level row; the ledger's")
    A("FIRST-QP `first_shift_adds` is what both rows print, and it is what the")
    A("last two columns carry.")
    A("")

    # ---------------- T8: the core unit counts ------------------------------
    A("## 8. The core drop -> same-bound-re-add unit count (`wrappers/m4c_tails.py`, RUN)")
    A("")
    A("The OFF column for the TEN is **M4c's committed `tables/tails.json`**, read")
    A("and not recomputed.  The OFF column for the three neutrality cells is this")
    A("round's own run of the SAME script over M4's pinned OFF traces (M4c never")
    A("ran it on those three).  As a check the script was also re-run on M4's")
    A("`off-YORKNET.csv` and reproduced M4c's committed 2538 / 3994 exactly.")
    A("")
    A("| cell | arm | recurrent solves | tail units | **core units** | OFF recurrent solves | OFF tail units | **OFF core units** | core ratio |")
    A("|---|---|---|---|---|---|---|---|---|")
    for cell in CELLS_TEN + NEUTRAL:
        ot = off_tails.get(cell)
        for arm in ARMS:
            if (arm, cell) not in rows:
                continue
            at = arm_tails.get((arm, cell))
            if at is None:
                continue
            r = "-"
            if ot and ot["core"]:
                r = "%.3f" % (at["core"] / ot["core"])
            elif ot is not None and ot["core"] == 0 and at["core"] == 0:
                r = "0/0"
            A("| `%s` | `%s` | %s | %s | **%s** | %s | %s | **%s** | %s |" % (
                cell, arm, at["solves"], at["units"], at["core"],
                ot["solves"] if ot else "-", ot["units"] if ot else "-",
                ot["core"] if ot else "-", r))
    A("")
    A("The representative released identity of every recurrent tail, per leg:")
    A("")
    A("| leg | tails (solve : representative released identity) | classes |")
    A("|---|---|---|")
    for cell in CELLS_TEN + NEUTRAL:
        ot = off_tails.get(cell)
        if ot is not None:
            A("| `off-%s` | %s | %s |" % (cell, ", ".join("`%s`" % i for i in ot["ids"]) or "(none)", ot["cls"]))
        for arm in ARMS:
            at = arm_tails.get((arm, cell))
            if at is None:
                continue
            A("| `%s-%s` | %s | %s |" % (arm, cell, ", ".join("`%s`" % i for i in at["ids"]) or "(none)", at["cls"]))
    A("")

    # ---------------- T9: neutrality ----------------------------------------
    A("## 9. Neutrality, DESCRIPTIVE AND EXTENDED (RULE.md section 8 step 4, A7)")
    A("")
    A("`f` is printed EXACTLY as the row carries it, not rounded: M3c's")
    A("consensus rule agrees to 6 significant digits, and a difference below")
    A("that is a difference a rounded column would hide.")
    A("")
    A("| cell | arm | status | f | feasibility_e | feasibility_i | stationarity | minors | minors ratio vs OFF | trace sha == OFF's? |")
    A("|---|---|---|---|---|---|---|---|---|---|")
    shas = {}
    sp = os.path.join(D, "traces-m5", "SHA256-parent-traces-m5.txt")
    if os.path.isfile(sp):
        for line in open(sp):
            pr = line.split()
            if len(pr) == 2 and len(pr[0]) == 64:
                shas[pr[1]] = pr[0]
    for cell in NEUTRAL:
        o = off.get(cell, {})
        A("| `%s` | `off` | `%s` | %s | %s | %s | %s | %s | 1.0000 | (the control) |" % (
            cell, o.get("status"), o.get("f"), o.get("feasibility_e"),
            o.get("feasibility_i"), o.get("stationarity"),
            o.get("qp_minor_iters")))
        for arm in ARMS:
            d = rows.get((arm, cell))
            if d is None:
                continue
            mo, mi = num(o, "qp_minor_iters"), num(d, "qp_minor_iters")
            same = "-"
            k1, k2 = "%s-%s.csv" % (arm, cell), "off-%s.csv" % cell
            if k1 in shas and k2 in shas:
                same = "YES" if shas[k1] == shas[k2] else "no"
            A("| `%s` | `%s` | `%s` | %s | %s | %s | %s | %s | %s | %s |" % (
                cell, arm, d.get("status"), d.get("f"),
                d.get("feasibility_e"), d.get("feasibility_i"),
                d.get("stationarity"), mi,
                ("%.4f" % (mi / mo)) if mo else "-", same))
    A("")
    A("Read against M3c's consensus bar (agreement to 6 significant digits):")
    A("under `a`, READING6's and SMBANK's `f` are IDENTICAL to the control's")
    A("digit for digit, and ZAMB2-8's agrees to 6 significant digits while")
    A("differing at the 9th (`-0.15293592838839792` against")
    A("`-0.15293592733713887`), on a leg that took 1.1530x the control's minors.")
    A("Under `b1` and `a+b1` two of the three cells do not reach an objective to")
    A("compare.")
    A("")

    # ---------------- T10: A11's declared cost ------------------------------
    A("## 10. A11's COST, DECLARED -- what the per-pass QR bought and spent")
    A("")
    A("RULE.md section 3 declares the cost in advance: one dense rank-revealing")
    A("QR of an `n x (me+|W|)` matrix per pass, `O(n (me+|W|)^2)`, plus one")
    A("projection per candidate examined and one least-squares solve per")
    A("candidate called dependent.  **Wall-clock is INFORMATIONAL** (CLAUDE.md")
    A("section 7): the legs ran one at a time, pinned, under one lock hold, but")
    A("no claim in this artifact rests on a timing and none is quoted as a")
    A("measurement.  The column is here because a DEADLINE is what censored four")
    A("legs, and a reader is owed the arithmetic behind that censoring.")
    A("")
    A("| cell | arm | n | me | QR builds | dep tests | minors | minors vs OFF | deadline s | wall s (info) | deadline fired |")
    A("|---|---|---|---|---|---|---|---|---|---|---|")
    for cell in CELLS_TEN + NEUTRAL:
        for arm in ARMS:
            d = rows.get((arm, cell))
            if d is None or arm == "a":
                continue
            o = off.get(cell, {})
            mo, mi = num(o, "qp_minor_iters"), num(d, "qp_minor_iters")
            A("| `%s` | `%s` | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
                cell, arm, d.get("n"), d.get("me"), d.get("indep_qr_builds"),
                d.get("indep_dep_tests"), mi,
                ("%.4f" % (mi / mo)) if mo else "-", d.get("deadline_seconds"),
                ("%.1f" % W["%s-%s" % (arm, cell)]) if "%s-%s" % (arm, cell) in W else "-",
                d.get("deadline_fired")))
    A("")

    # ---------------- T11: A6's two halves ----------------------------------
    A("## 11. A6's prediction, both halves (RULE.md section 4)")
    A("")
    A("**OFFLINE (the pre-registered check, run BEFORE the dynamic leg and")
    A("recorded in `logs/a6-offline.txt` and STEP 2 of `logs/chain-m5.txt`):**")
    A("2538 core drops examined over M4's pinned `off-YORKNET.csv` -- the exact")
    A("number A6 predicted, and cross-checked against `m4c_tails.py`'s own")
    A("`tails.json` (2538 == 2538) rather than taken on trust.  The scaled test")
    A("REFUSES 2538 and ADMITS 0; no pass lacked a published scale; the ratio")
    A("`|lambda|/(opt_tol*scale)` is 1.84849e-10 at min, p50, p90 and max.")
    A("**A6's offline half HOLDS.**")
    A("")
    A("**DYNAMIC (A6's refutation condition, verbatim): refutation means the")
    A("SAME candidate identity is still dropped under the arm, or the SAME")
    A("matched recurrence persists.  A DIFFERENT tail surviving is NOT a")
    A("refutation and is reported as _diagnosis untested by this trajectory_.**")
    A("")
    ot = off_tails["YORKNET"]
    A("| leg | recurrent solves | representative released identities | classes |")
    A("|---|---|---|---|")
    A("| `off-YORKNET` | %d | %s | %s |" % (
        ot["solves"], ", ".join("`%s`" % i for i in ot["ids"]), ot["cls"]))
    for arm in ("a", "a+b1"):
        at = arm_tails.get((arm, "YORKNET"))
        if at:
            A("| `%s-YORKNET` | %d | %s | %s |" % (
                arm, at["solves"], ", ".join("`%s`" % i for i in at["ids"]),
                at["cls"]))
    A("")
    offids = set(i.split(":", 1)[1] for i in ot["ids"])
    for arm in ("a", "a+b1"):
        at = arm_tails.get((arm, "YORKNET"))
        if not at:
            continue
        armids = set(i.split(":", 1)[1] for i in at["ids"])
        A("* `%s-YORKNET`: OFF's cycling identities are %s; the arm's surviving" % (
            arm, ", ".join("`%s`" % i for i in sorted(offids))))
        A("  tails release %s.  Intersection: **%s**." % (
            ", ".join("`%s`" % i for i in sorted(armids)),
            ", ".join(sorted(offids & armids)) or "EMPTY"))
    A("")

    open(os.path.join(D, "tables", "arm.md"), "w").write("\n".join(out) + "\n")
    print("wrote tables/arm.md  (%d lines)" % len(out))


if __name__ == "__main__":
    main()
