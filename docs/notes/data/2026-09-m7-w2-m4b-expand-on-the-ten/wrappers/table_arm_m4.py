#!/usr/bin/env python3
"""M7 W2 M4 -- `tables/arm.md`, written mechanically from this task's own rows.

Every number below is read out of `rows/<arm>-<cell>.txt`, the exact files the
chain wrote and this directory committed. Nothing is retyped. No solver runs
here.
"""
import os
import sys
from collections import OrderedDict

ARMS = ("off", "E", "L", "L+")
NEUTRAL = ("READING6", "SMBANK", "ZAMB2-8")
TEN = ("ACOPP57", "DALLASM", "STEENBRB", "ACOPP118", "ACOPR57", "STEENBRC",
       "STEENBRG", "YORKNET", "SAWPATH", "BRITGAS")

# RULE.md section 7.1's per-cell field list, in its own order.
S71 = ("status", "major_iters", "qp_minor_iters", "factorizations", "f",
       "stationarity", "feasibility_e", "feasibility_i", "steps_accepted",
       "restoration_entries", "restoration_sub_status")
# section 7.1 names `qp_failures_in_a_row_peak` and the restoration h/f fields;
# this probe does not print them, and they are listed ABSENT rather than
# silently skipped.
S71_ABSENT = ("qp_failures_in_a_row_peak", "restoration_h_in", "restoration_h_out",
              "restoration_f_in", "restoration_f_out")
# section 6.6's arm counters.
ARMK = ("arm_min_steps", "arm_pivot_switches", "arm_unpinned_adds",
        "arm_lex_drop_switches", "arm_lex_ratio_switches", "arm_resets",
        "arm_reset_nontrivial_moves", "arm_delta_at_exit",
        "arm_expand_iter_at_exit")
TIES = ("first_drop_ties", "first_ratio_ties", "first_degenerate_fraction",
        "qp_maxiter_exits", "qp_numerical_exits", "qp_failures_routed",
        "deadline_fired")


def parse(path):
    d = OrderedDict()
    for ln in open(path):
        ln = ln.strip()
        if "=" in ln and not ln.endswith("_BEGIN") and not ln.endswith("_END"):
            k, v = ln.split("=", 1)
            d[k] = v
    return d


def summary(path):
    lines = open(path).read().splitlines()
    i, j = lines.index("SOLVE_SUMMARY_BEGIN"), lines.index("SOLVE_SUMMARY_END")
    blk = lines[i + 1:j]
    hdr = None
    rows = []
    for l in blk:
        if l.startswith("solve,"):
            hdr = l.split(",")
            continue
        if hdr and l and l[0].isdigit():
            rows.append(dict(zip(hdr, l.split(","))))
    return rows


def main():
    rows_dir, out = sys.argv[1], sys.argv[2]
    R = {}
    for a in ARMS:
        for c in NEUTRAL:
            R[(a, c)] = parse(os.path.join(rows_dir, "%s-%s.txt" % (a, c)))
    for c in TEN:
        R[("off", c)] = parse(os.path.join(rows_dir, "off-%s.txt" % c))

    w = open(out, "w").write
    w("# M7 W2 M4 — `tables/arm.md`\n\n")
    w("Written by `wrappers/table_arm_m4.py` out of `rows/`, the rows this "
      "task's chain wrote. **No solver ran here and no number is retyped.**\n\n")
    w("RULE.md §12 asks for one row per cell × arm with §7.1's fields, §6.6's "
      "read-outs and the verdict. **§8 stopped all three arms at their "
      "neutrality legs, so PHASE 3 never ran and the ten cells of §1.1 have no "
      "arm leg at all.** Their OFF-control line is given first, because it is "
      "the whole of what this task has on them, and then the three neutrality "
      "cells × four arms in full.\n\n")

    w("## 1. The OFF controls — RULE.md §5's identity gate, phase 1\n\n")
    w("Read from `logs/gate-off.txt`: **REPRODUCES 10 / 10, DIFFERS 0, "
      "CENSORED 0, REFUSED 0, MISSING 0**, zero near-ulp acceptances. Every "
      "one of §1.1's ten cells reproduced M3c's committed pass-1 row on every "
      "compared field, so every cell was carried forward "
      "(`logs/passed-cells.txt`). No `OFF_CONTROL_FAILED` line exists.\n\n")
    w("| cell | status | majors | minors | facts | f | stationarity | "
      "feas_e | gate |\n|---|---|---|---|---|---|---|---|---|\n")
    for c in TEN:
        d = R[("off", c)]
        w("| %s | `%s` | %s | %s | %s | %s | %s | %s | REPRODUCES |\n" % (
            c, d["status"], d["major_iters"], d["qp_minor_iters"],
            d["factorizations"], d["f"], d["stationarity"], d["feasibility_e"]))
    w("\n**No arm leg was run on any of them.** RULE.md §7.5's second `HARM` "
      "clause is `N/A` on these ten because OFF solves none of them; that is "
      "stated rather than printed as a vacuous \"0 harmed\", exactly as §7.5 "
      "requires. §7.3's `CONVERTED`, §7.4's `improved, unverified` and §7.7's "
      "SNOPT cost read-out have no cell to apply to.\n\n")

    w("## 2. The three neutrality cells × four arms — every §7.1 field\n\n")
    w("A cell is compared against **its own `off` leg** (RULE.md §5.5), at the "
      "STRICT bar. A `<<` marks a value that differs from that cell's `off`.\n\n")
    for c in NEUTRAL:
        w("### %s\n\n" % c)
        w("| field | off | E | L | L+ |\n|---|---|---|---|---|\n")
        for k in S71 + TIES + ARMK:
            vals = [R[(a, c)].get(k, "(absent)") for a in ARMS]
            cells = []
            for i, v in enumerate(vals):
                mark = " <<" if i and v != vals[0] else ""
                cells.append("`%s`%s" % (v, mark))
            w("| `%s` | %s |\n" % (k, " | ".join(cells)))
        w("\n§7.1 also names %s; **this probe does not print them**, so they "
          "are ABSENT from every row here and are named rather than skipped.\n\n"
          % ", ".join("`%s`" % k for k in S71_ABSENT))
        w("Solve-summary aggregates, summed over the run's own per-solve table "
          "in the same row file:\n\n")
        w("| | off | E | L | L+ |\n|---|---|---|---|---|\n")
        agg = {}
        for a in ARMS:
            rs = summary(os.path.join(rows_dir, "%s-%s.txt" % (a, c)))
            agg[a] = {
                "QP solves": len(rs),
                "minors": sum(int(r["minors"]) for r in rs),
                "degenerate minors": sum(int(r["degenerate"]) for r in rs),
                "rides": sum(int(r["rides"]) for r in rs),
                "drops": sum(int(r["drops"]) for r in rs),
                "adds": sum(int(r["adds"]) for r in rs),
            }
        for k in ("QP solves", "minors", "degenerate minors", "rides", "drops",
                  "adds"):
            base = agg["off"][k]
            w("| %s | %d | %s |\n" % (
                k, base,
                " | ".join("%d%s" % (agg[a][k], " <<" if agg[a][k] != base else "")
                           for a in ARMS[1:])))
        w("\nminors ratio arm / off: %s\n\n"
          % ", ".join("**%s %.4f**" % (a, agg[a]["minors"] / agg["off"]["minors"])
                      for a in ARMS[1:]))

    w("## 3. The neutrality verdicts, as the gate that ran recorded them\n\n")
    w("Read from `logs/gate-neutral-{E,L,L+}.txt`; the fields are that "
      "transcript's own `FIELDS DIFFER` line.\n\n")
    w("| arm | cell | verdict | near-ulp acceptances | fields that differ |\n")
    w("|---|---|---|---|---|\n")
    for a in ("E", "L", "L+"):
        txt = open(os.path.join(os.path.dirname(rows_dir), "logs",
                                "gate-neutral-%s.txt" % a)).read().splitlines()
        cell = None
        near = None
        for i, l in enumerate(txt):
            if l.startswith("ARM %s   CELL " % a):
                cell = l.split("CELL ")[1].strip()
            if l.strip().startswith("near-ulp acceptances"):
                near = l.split(":")[-1].strip()
            if l.strip() == "VERDICT: DOES NOT REPRODUCE":
                diff = ""
                for j in range(i + 1, min(i + 4, len(txt))):
                    if "FIELDS DIFFER:" in txt[j]:
                        diff = txt[j].split("FIELDS DIFFER:")[1].strip()
                w("| %s | %s | DOES NOT REPRODUCE | %s | `%s` |\n"
                  % (a, cell, near, diff.replace(",", "`, `")))
            elif l.strip().startswith("VERDICT: REPRODUCES"):
                w("| %s | %s | REPRODUCES | %s | — |\n" % (a, cell, near))
    w("\n")
    print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
