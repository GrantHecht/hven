#!/usr/bin/env python3
"""M7 W2 M4b -- `tables/neutrality-descriptive.md`, RULE.md section 5.2.

Written from M4's ALREADY-COMMITTED rows
`../2026-09-m7-w2-m4-anticycling-arm/rows/{off,E}-<CELL>.txt`.  **No leg is
re-run for it and it carries no verdict.**  The consensus is machine-read from
M3c's committed `tables/census.md` and is never recomputed.  No solver runs
here.
"""
import os
import re
import sys
from collections import OrderedDict

CELLS = ("READING6", "SMBANK", "ZAMB2-8")


def parse(path):
    d = OrderedDict()
    for ln in open(path):
        ln = ln.strip()
        if "=" in ln and not ln.endswith("_BEGIN") and not ln.endswith("_END"):
            k, v = ln.split("=", 1)
            d[k] = v
    return d


def main():
    m4_rows, census_path, out = sys.argv[1:4]
    census = open(census_path).read()
    R = {}
    for c in CELLS:
        for a in ("off", "E"):
            R[(a, c)] = parse(os.path.join(m4_rows, "%s-%s.txt" % (a, c)))
    w = open(out, "w").write
    w("# M7 W2 M4b — `tables/neutrality-descriptive.md`\n\n")
    w("`RULE.md` §5.2's DESCRIPTIVE neutrality report, written by\n")
    w("`wrappers/table_neutrality_m4b.py` from M4's already-committed rows\n")
    w("`../2026-09-m7-w2-m4-anticycling-arm/rows/{off,E}-<CELL>.txt`.\n")
    w("**No leg was re-run for it. It carries no verdict.** §5.2 says so in\n")
    w("terms: \"These three cells receive no verdict. The report states what\n")
    w("happened on them and moves on. They are context for reading the ten,\n")
    w("not a gate in front of them.\"\n\n")
    w("**Why it exists.** M4 §5.5 required an arm to reproduce its own `off`\n")
    w("leg BIT FOR BIT on these three cells. Arm E failed that on all three,\n")
    w("and M4's close-out established from the traces WHY: E does not only\n")
    w("decide ties — it admits constraints against a slowly growing\n")
    w("feasibility bound and takes a largest-pivot second pass, so it changes\n")
    w("which constraint enters even where nothing is tied. A bit-equality bar\n")
    w("is unmeetable by an arm of that shape for reasons that have nothing to\n")
    w("do with whether the arm is any good, so `RULE.md` §5 REPLACED it with\n")
    w("this report — which asserts nothing about equality, cannot be passed by\n")
    w("accident, and must be carried whatever it says.\n\n")
    w("**The subject is arm E as implemented in the pinned M4 binary**, never\n")
    w("\"EXPAND\" and never \"canonical EXPAND\".\n\n")

    w("## §5.2's four items, per cell\n\n")
    w("| | `READING6` | `SMBANK` | `ZAMB2-8` |\n|---|---|---|---|\n")

    def row(label, fn):
        w("| %s | %s | %s | %s |\n" % (label, fn("READING6"), fn("SMBANK"),
                                       fn("ZAMB2-8")))

    row("**1. terminal status** — off",
        lambda c: "`%s`" % R[("off", c)]["status"])
    row("**1. terminal status** — E",
        lambda c: "`%s`" % R[("E", c)]["status"])
    row("**2. `f`** — off", lambda c: "`%s`" % R[("off", c)]["f"])
    row("**2. `f`** — E", lambda c: "`%s`" % R[("E", c)]["f"])
    row("**2. `f` vs M3c consensus**",
        lambda c: ("**ABSENT** — `%s` appears nowhere in M3c's committed "
                   "`census.md`" % c) if c not in census else "(see census)")
    row("**2. `\\|f_E − f_off\\| / max(1,\\|f_off\\|)`**",
        lambda c: "`%.3e`" % (abs(float(R[("E", c)]["f"]) - float(R[("off", c)]["f"]))
                              / max(1.0, abs(float(R[("off", c)]["f"])))))
    row("**3. `max(feasibility_e, feasibility_i)`** — off",
        lambda c: "`%.6e`" % max(float(R[("off", c)]["feasibility_e"]),
                                 float(R[("off", c)]["feasibility_i"])))
    row("**3. `max(feasibility_e, feasibility_i)`** — E",
        lambda c: "`%.6e`" % max(float(R[("E", c)]["feasibility_e"]),
                                 float(R[("E", c)]["feasibility_i"])))
    row("**3. that leg's own printed `opt_feas_tol`**",
        lambda c: "**ABSENT** — this probe does not print it")
    row("**4. minors off**", lambda c: "`%s`" % R[("off", c)]["qp_minor_iters"])
    row("**4. minors E**", lambda c: "`%s`" % R[("E", c)]["qp_minor_iters"])
    row("**4. minors ratio E / off**",
        lambda c: "**%.4f**" % (int(R[("E", c)]["qp_minor_iters"])
                                / int(R[("off", c)]["qp_minor_iters"])))
    w("\n")

    w("### Item 2 — the consensus is ABSENT for all three, and that is not a gap\n\n")
    w("§5.2 requires `f` under E to be read against **M3c's three-lane\n")
    w("consensus, machine-read from M3c's committed `census.md`, never\n")
    w("recomputed**. M3c's census is a census of the cells that FAIL at DEFAULT\n")
    w("options. These three do not fail — that is why M4 chose them as\n")
    w("neutrality cells — so the census has no row for any of them. A grep of\n")
    w("the committed file for the three names returns **0 lines**. No consensus\n")
    w("is read, none is computed, and the item is reported ABSENT rather than\n")
    w("filled from somewhere else.\n\n")
    w("### Item 3 — `opt_feas_tol` is ABSENT from this probe's row\n\n")
    w("§5.2 requires the leg's OWN printed `opt_feas_tol`. This probe does not\n")
    w("print the `OPTIONS` block at all: `opt_feas_tol` is one of the 127\n")
    w("fields M4's own OFF-control gate lists under \"fields the leg does not\n")
    w("print (ABSENT, never agreement)\"\n")
    w("(`../2026-09-m7-w2-m4-anticycling-arm/logs/gate-off.txt`). The\n")
    w("feasibility values are printed above without a bar beside them, and no\n")
    w("bar is substituted from M3c's row or typed in from the rule.\n\n")

    w("## §6.6's arm counters on these three, and what they say\n\n")
    w("| counter | `READING6` | `SMBANK` | `ZAMB2-8` |\n|---|---|---|---|\n")
    for k in ("arm_min_steps", "arm_unpinned_adds", "arm_pivot_switches",
              "arm_lex_drop_switches", "arm_lex_ratio_switches", "arm_resets",
              "arm_reset_nontrivial_moves"):
        w("| `%s` | `%s` | `%s` | `%s` |\n"
          % (k, R[("E", "READING6")][k], R[("E", "SMBANK")][k],
             R[("E", "ZAMB2-8")][k]))
    w("\n* `arm_min_steps == arm_unpinned_adds` on all three, so the ride's\n")
    w("  re-pin defect (README, departure 2) was **not exercised** on any of\n")
    w("  them — which is exactly what the Codex review of M4 reported, and is\n")
    w("  why M4's data could not see it. Five of this task's ten cells do\n")
    w("  exercise it; `tables/arm.md` §3 counts them.\n")
    w("* `arm_resets` is 0 on all three, so no reset ran on them either, and\n")
    w("  M4's data could not see the reset off-by-one (README, departure 1).\n")
    w("  Two of this task's ten cells reached a reset.\n")
    w("* `arm_lex_drop_switches` and `arm_lex_ratio_switches` are 0, as they\n")
    w("  must be: E touches no lexicographic rule.\n\n")

    w("## §5.3's DECLARED CAUTION, held against what the ten actually did\n\n")
    w("§5.3 was written BEFORE the ten ran and said, from these three cells:\n")
    w("**\"E can lengthen a walk as readily as shorten one.\"** The ratios it\n")
    w("declared were `READING6` 0.8137, `SMBANK` 0.9820, `ZAMB2-8` 1.4922.\n\n")
    w("**The caution HELD.** On the ten, the minors ratio E/off runs from\n")
    w("0.7630 (`BRITGAS`) to 1.3387 (`YORKNET`) — E shortened seven walks,\n")
    w("lengthened two, and left one (`ACOPP57`) at exactly 1.0000. Neither a\n")
    w("short leg nor a long one was a surprise, and neither is read as the\n")
    w("arm's general property. See `tables/arm.md` §1 for the ten ratios.\n\n")
    w("One qualification the caution did not anticipate and this task must\n")
    w("add: `SAWPATH`'s 0.7737 is **not a saving**. Both its legs fired the\n")
    w("300 s deadline, so the two minor counts are two truncations of two\n")
    w("different walks at the same wall-clock stop, not two completed walks.\n")
    w("`verdict.txt` records the same point.\n")
    print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
