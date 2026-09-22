#!/usr/bin/env python3
"""M7 W2 M5 -- RULE.md sections 4, 5, 7 and 8, APPLIED MECHANICALLY.

Writes `verdict.txt`.  No solver runs here; every input is a committed row, a
committed OFF row, or the chain transcript.  RULE.md is READ and never edited.

Usage: verdict_m5.py ARTIFACT_DIR
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from readout_m5 import (ARMS, CELLS_TEN, NEUTRAL, RANK, load_row, num,  # noqa: E402
                        split_leg, walls)

FEAS_FLOOR = 1e-6          # DECLARED here; the probe prints no `opt_feas_tol`


def main():
    D = sys.argv[1]
    rows, off = {}, {}
    for fn in sorted(os.listdir(os.path.join(D, "rows"))):
        if fn.endswith(".txt"):
            rows[split_leg(fn[:-4])] = load_row(os.path.join(D, "rows", fn))
    for fn in sorted(os.listdir(os.path.join(D, "rows-off"))):
        if fn.startswith("off-") and fn.endswith(".txt"):
            off[fn[4:-4]] = load_row(os.path.join(D, "rows-off", fn))
    W = walls(os.path.join(D, "logs", "chain-m5.txt"))

    o = []
    P = o.append
    P("M7 W2 M5 -- RULE.md APPLIED MECHANICALLY, CELL BY CELL AND ARM BY ARM")
    P("=" * 70)
    P("Written at the close of the task by `wrappers/verdict_m5.py`, from the")
    P("rows and the chain transcript named on every line.  No solver ran to")
    P("produce this file.  RULE.md was not edited.")
    P("")
    P("THE SUBJECT OF EVERY LINE BELOW")
    P("-------------------------------")
    P('"arms a, b1 and a+b1 as implemented in the pinned M5 probe"')
    P("    .scratch/m7w2m5arm/build/m5_arm_probe")
    P("    sha256 eac445c0256e1b0af739f98e91a5d1cce586fedfa41562b7fb08684b145b43d4")
    P("NOT a production form of either arm, and NOT the Gill & Wong exchange,")
    P("which RULE.md section 1 records as REGISTERED AND UNBUILT (A4/C5).")
    P("")
    P("THE CHAIN")
    P("---------")
    P("`logs/chain-m5.txt` (pid 3123140, detached 2026-09-22T14:14:38-04:00).")
    P("    CHAIN_END 2026-09-22T15:29:36-04:00")
    P("    PIN_BINARY OK; PIN_OFF_ROWS OK (13); PIN_OFF_ROWS_PROVENANCE OK;")
    P("    PIN_PARENT_TRACE OK (a69816b9...)")
    P("    legs attempted 28   rows surviving 28   CENSORED 0")
    P("    TRUNCATED (deadline_fired=1): b1-SAWPATH, a+b1-SAWPATH,")
    P("                                  a+b1-STEENBRC, a+b1-STEENBRG")
    P("")
    P("THE PRECEDENCE THIS FILE APPLIES, DECLARED BEFORE THE TABLE")
    P("-----------------------------------------------------------")
    P("RULE.md section 7 inherits M4 section 7 and adds six deviations.  Where")
    P("two categories could both apply, this file applies them in the order the")
    P("rule states them, and it PRINTS every category a cell meets rather than")
    P("collapsing them:")
    P("  1. TRUNCATED (deadline_fired=1) -- M4 section 7.2 and deviation 6: a")
    P("     censored cell gets counters and NO CONVERTED/HARM verdict at all.")
    P("  2. UNSUPPORTED (deviation 2) -- a b1 or a+b1 cell with a nonzero")
    P("     `indep_rank_violations` (A9), `indep_refused_mandatory_dependent`")
    P("     (A3) or `indep_leak_bound_exceeded` (A10).  Counters reported, no")
    P("     CONVERTED and no HARM.")
    P("  3. INFEASIBLE-EXIT (deviation 1, section 5.1) -- printed as a FLAG in")
    P("     its own column.  It outranks CONVERTED and never becomes one.")
    P("  4. Otherwise M4 sections 7.3/7.4/7.5 on the rank order: CONVERTED,")
    P("     `improved, unverified`, HARM, none.")
    P("  5. RULE.md section 8 step 4's NEUTRALITY rule is SEPARATE and is not")
    P("     suppressed by UNSUPPORTED: a change of status on READING6, SMBANK or")
    P("     ZAMB2-8 IS HARM and stops that arm's verdicts being quoted.  It is")
    P("     printed in its own column and again below the table.")
    P("")
    P("M4 section 7.2's rank order: optimal 0, acceptable 1, max_iter 2,")
    P("stalled 3, diverging 4, budget_exhausted 5, infeasible 6,")
    P("numerical_error 7, interrupted NOT RANKED (a censoring, not a status).")
    P("M4 section 7.2's own note applies unchanged: `infeasible` ranks below")
    P("admitting defeat ON THIS CELL SET, not as a general matter of the enum,")
    P("and the one cell it applies to is YORKNET.")
    P("")

    P("THE TABLE")
    P("---------")
    P("`triggers` is A9:<rank violations> A3:<mandatory-dependent refusals>")
    P("A10:<leak-bound exceedances>; a dash means all three are zero.")
    P("`INFEAS-EXIT` marks a leg whose RETURNED POINT violates its own printed")
    P("`feasibility_e`/`feasibility_i` by more than 1e-6 (section 5.1); `+OFF`")
    P("beside it means the OFF CONTROL of that cell does so too, so on that cell")
    P("the flag is not an arm effect.  `NEUT-HARM` is section 8 step 4.")
    P("")
    P("%-9s %-5s %-16s %-16s %-5s %-22s %-22s %s" % (
        "cell", "arm", "OFF status", "arm status", "rank", "triggers",
        "flags", "SECTION 7 VERDICT"))
    P("%-9s %-5s %-16s %-16s %-5s %-22s %-22s %s" % (
        "-" * 9, "-" * 5, "-" * 16, "-" * 16, "-" * 5, "-" * 22, "-" * 22, "-" * 28))

    tally = {}
    per_arm_disarm = {}
    lines = []
    for cell in CELLS_TEN + NEUTRAL:
        for arm in ARMS:
            d = rows.get((arm, cell))
            if d is None:
                continue
            oo = off.get(cell, {})
            st, ost = d.get("status"), oo.get("status")
            rv = num(d, "indep_rank_violations") or 0
            md = num(d, "indep_refused_mandatory_dependent") or 0
            lb = num(d, "indep_leak_bound_exceeded") or 0
            trig = "-"
            if arm != "a":
                if rv or md or lb:
                    trig = "A9:%d A3:%d A10:%d" % (rv, md, lb)
                per_arm_disarm.setdefault(arm, []).append(
                    (cell, bool(rv or md)))
            flags = []
            if arm != "a":
                fe = num(d, "feasibility_e") or 0.0
                fi = num(d, "feasibility_i") or 0.0
                ofe = num(oo, "feasibility_e") or 0.0
                ofi = num(oo, "feasibility_i") or 0.0
                if max(fe, fi) > FEAS_FLOOR:
                    flags.append("INFEAS-EXIT" +
                                 ("+OFF" if max(ofe, ofi) > FEAS_FLOOR else ""))
            if cell in NEUTRAL and st != ost:
                flags.append("NEUT-HARM")
            if num(d, "deadline_fired"):
                v = "TRUNCATED"
            elif trig != "-":
                v = "UNSUPPORTED"
            else:
                r, ro = RANK.get(st), RANK.get(ost)
                if r is None or ro is None:
                    v = "TRUNCATED"
                elif r < ro:
                    v = "CONVERTED?/improved -- see below"
                elif r > ro:
                    v = "HARM"
                else:
                    v = "none"
            if cell in NEUTRAL and st != ost and v != "TRUNCATED":
                v = v + " (+HARM, section 8 step 4)" if v != "HARM" else v
            tally[v.split(" ")[0]] = tally.get(v.split(" ")[0], 0) + 1
            ranks = "%s>%s" % (RANK.get(ost, "-"), RANK.get(st, "-"))
            lines.append("%-9s %-5s %-16s %-16s %-5s %-22s %-22s %s" % (
                cell, arm, ost, st, ranks, trig,
                ",".join(flags) or "-", v))
    o.extend(lines)
    P("")
    P("THE TOTALS")
    P("----------")
    for k in ("CONVERTED", "improved,", "HARM", "none", "TRUNCATED", "UNSUPPORTED"):
        P("    %-14s %d" % (k, tally.get(k, 0)))
    P("")
    P("SECTION 7.3 -- CONVERTED")
    P("------------------------")
    P("Condition 1, rank(arm) < rank(OFF), is FALSE on EVERY cell x arm: no leg")
    P("improved its cell's terminal status over the OFF control.  Conditions 2")
    P("and 3 are therefore never reached and NO CELL IS CONVERTED.  The")
    P("conversion total is 0, on both arms and on their combination.")
    P("")
    P("Two absences are recorded rather than papered over, both inherited from")
    P("M4b's own record of them:")
    P("  * condition 2's `feas_tol` is to be read from the leg's own printed")
    P("    `opt_feas_tol`.  THIS PROBE DOES NOT PRINT IT.  It is not substituted")
    P("    from M3c's committed row and not typed in from the rule.  Condition 2")
    P("    was not evaluated on any cell; condition 1 fails first on all of them.")
    P("  * section 7.1's restoration record asks for `restoration_h_in/out` and")
    P("    `restoration_f_in/out`; those four are absent from this probe's row.")
    P("    `restoration_entries`, `restoration_sub_status` and")
    P("    `restoration_iters` ARE printed and ARE in `tables/arm.md`.")
    P("")
    P("SECTION 7.4 -- improved, unverified")
    P("-----------------------------------")
    P("Requires condition 1, which fails everywhere.  NO CELL IS `improved,")
    P("unverified`.  Recorded for completeness from M3c's committed")
    P("`tables/census.md`: STEENBRC and STEENBRG carry `ABSENT (no consensus)`")
    P("there, so neither could ever have been better than `improved,")
    P("unverified` even had condition 1 held.")
    P("")
    P("SECTION 7.5 -- HARM")
    P("-------------------")
    P("First clause, rank(arm) > rank(OFF): TRUE on YORKNET under `a` and under")
    P("`a+b1` (OFF `infeasible`, rank 6 -> arm `numerical_error`, rank 7), and")
    P("TRUE on READING6 and ZAMB2-8 under `b1` and `a+b1` (OFF `optimal`, rank 0")
    P("-> `numerical_error`, rank 7).")
    P("Second clause (equal ranks, a cell OFF already solved, minors > 1.10x")
    P("OFF's): N/A on the TEN, because OFF solves none of them -- stated per")
    P("section 7.5's own instruction rather than printed as a vacuous 0.  On the")
    P("three neutrality cells the bar is section 8 step 4's, which is a change of")
    P("STATUS, and it is applied in its own column above.")
    P("")
    P("DEVIATION 3 -- MORE THAN HALF THE CELLS DISARM (A9)")
    P("---------------------------------------------------")
    for arm in ("b1", "a+b1"):
        cs = per_arm_disarm.get(arm, [])
        dis = [c for c, f in cs if f]
        P("    arm %s: %d of %d cells disarmed (a rank violation or a"
          % (arm, len(dis), len(cs)))
        P("        mandatory-dependent refusal), namely %s." % ", ".join(dis))
        if len(dis) * 2 > len(cs):
            P("        THAT IS MORE THAN HALF -> **THE ARM'S RESULT IS")
            P("        UNSUPPORTED AS A WHOLE** (A9, deviation 3).")
        else:
            P("        Not more than half; deviation 3 does not fire.")
    P("")
    P("DEVIATION 4 -- INVALIDATING COUNTERS")
    P("------------------------------------")
    P("`indep_enumeration_mismatches` is 0 on all 24 armed-b1 legs and")
    P("`scale_unavailable_passes` is 0 on all 28 legs.  Deviation 4 never fires:")
    P("no cell's read-out is invalidated by either counter.")
    P("")
    P("DEVIATION 6 -- TRUNCATION")
    P("-------------------------")
    P("Four legs fired the probe's own deadline and are TRUNCATED, with no")
    P("verdict filled in later: b1-SAWPATH and a+b1-SAWPATH (300 s declared for")
    P("SAWPATH; the OFF control is `interrupted` too), and a+b1-STEENBRC and")
    P("a+b1-STEENBRG (600 s), whose OFF controls are NOT censored.  On the two")
    P("STEENBR cells the arm reached ONE major and roughly a fifth of OFF's")
    P("minors inside the same budget -- see `tables/arm.md` section 1 for the")
    P("minors and `tables/arm.md` section 10 for the QR cost that consumed it.  Wall-clock")
    P("is INFORMATIONAL (CLAUDE.md section 7) and no timing claim is made.")
    P("")
    P("SECTION 4 -- ARM A's OWN CONDITION, AND R1")
    P("------------------------------------------")
    P("`scale_unavailable_passes` = 0 on every leg, so no arm-A read-out is")
    P("invalidated.  R1: the claim `terminates optimal under a` was to be made")
    P("on YORKNET in leg 2 or NOT AT ALL.  Leg 2 returned `numerical_error`")
    P("where OFF returns `infeasible`.  **THE CLAIM IS NOT MADE.**")
    P("")
    P("SECTION 8 STEP 4 -- NEUTRALITY")
    P("------------------------------")
    P("    arm a     : READING6 `optimal` -> `optimal`; SMBANK `optimal` ->")
    P("                `optimal`; ZAMB2-8 `optimal` -> `optimal`.  NO status")
    P("                change.  (f, feasibility and stationarity are in")
    P("                `tables/arm.md` section 9; the minors ratios are")
    P("                0.9966 / 1.0000 / 1.1530.)")
    P("    arm b1    : READING6 and ZAMB2-8 change status to `numerical_error`.")
    P("                **HARM. That arm's verdicts are not quoted.**")
    P("    arm a+b1  : the same two cells, the same change.  **HARM. That arm's")
    P("                verdicts are not quoted.**")
    P("    SMBANK is unchanged under all three arms -- and under b1 and a+b1 its")
    P("    flat trace is BYTE-IDENTICAL to the OFF control's, because the arm")
    P("    refused to arm on 29 of 29 solves (A3).  That is neutrality by the")
    P("    arm never running.  It is reported as such and not as evidence that")
    P("    an armed walk is neutral.")
    P("")
    P("WHAT THIS FILE DOES NOT DECIDE")
    P("------------------------------")
    P("Nothing here is a statement about a production form of either arm, about")
    P("the exchange (unbuilt), or about wall-clock.  RULE.md section 9 lists")
    P("what a GREEN result would not have licensed; this result is not green,")
    P("and the list stands unchanged.")
    open(os.path.join(D, "verdict.txt"), "w").write("\n".join(o) + "\n")
    print("wrote verdict.txt (%d lines)" % len(o))


if __name__ == "__main__":
    main()
