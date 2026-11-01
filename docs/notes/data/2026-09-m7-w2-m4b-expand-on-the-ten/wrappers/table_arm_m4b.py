#!/usr/bin/env python3
"""M7 W2 M4b -- `tables/arm.md`, written mechanically from this task's own rows.

Every number is read out of `rows/E-<CELL>.txt` and `rows-off/off-<CELL>.txt`,
the exact files the chain wrote and this directory committed.  Nothing is
retyped.  The consensus column is machine-read from M3c's committed
`tables/census.md` and is never recomputed.  No solver runs here.

Derived from M4's `table_arm_m4.py`, which is committed beside it unmodified;
the differences are the cell set, the two arms, and the per-cell consensus and
verdict columns this task's `RULE.md` section 12 asks for.
"""
import os
import re
import sys
from collections import OrderedDict

TEN = ("ACOPP57", "DALLASM", "STEENBRB", "ACOPP118", "ACOPR57", "STEENBRC",
       "STEENBRG", "YORKNET", "SAWPATH", "BRITGAS")

S71 = ("status", "major_iters", "qp_minor_iters", "factorizations", "f",
       "stationarity", "feasibility_e", "feasibility_i", "steps_accepted",
       "restoration_entries", "restoration_sub_status", "restoration_iters")
S71_ABSENT = ("qp_failures_in_a_row_peak", "restoration_h_in",
              "restoration_h_out", "restoration_f_in", "restoration_f_out")
ARMK = ("arm_min_steps", "arm_unpinned_adds", "arm_pivot_switches",
        "arm_lex_drop_switches", "arm_lex_ratio_switches", "arm_resets",
        "arm_reset_nontrivial_moves", "arm_delta_at_exit",
        "arm_expand_iter_at_exit")
TIES = ("first_drop_ties", "first_ratio_ties", "first_degenerate_fraction",
        "qp_maxiter_exits", "qp_numerical_exits", "qp_failures_routed",
        "deadline_fired")

# M4 RULE.md section 7.2's rank order.  `interrupted` is NOT RANKED.
RANK = {"optimal": 0, "acceptable": 1, "max_iter": 2, "stalled": 3,
        "diverging": 4, "budget_exhausted": 5, "infeasible": 6,
        "numerical_error": 7}


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
    hdr, rows = None, []
    for l in lines[i + 1:j]:
        if l.startswith("solve,"):
            hdr = l.split(",")
            continue
        if hdr and l and l[0].isdigit():
            rows.append(dict(zip(hdr, l.split(","))))
    return rows


def consensus(census_path):
    """Machine-read M3c's per-cell CONSENSUS line.  Never recomputed."""
    out = {}
    cur = None
    for l in open(census_path):
        m = re.match(r"^#{2,4}\s+`?([A-Za-z0-9_\-]+)`?\b", l.strip())
        if m and m.group(1) in TEN:
            cur = m.group(1)
        if "**CONSENSUS**" in l and cur and cur not in out:
            out[cur] = l.strip().split("**CONSENSUS**", 1)[1].strip()
    return out


def main():
    off_dir, e_dir, census_path, out = sys.argv[1:5]
    R = {}
    for c in TEN:
        R[("off", c)] = parse(os.path.join(off_dir, "off-%s.txt" % c))
        R[("E", c)] = parse(os.path.join(e_dir, "E-%s.txt" % c))
    CONS = consensus(census_path)

    w = open(out, "w").write
    w("# M7 W2 M4b — `tables/arm.md`\n\n")
    w("Written by `wrappers/table_arm_m4b.py` out of `rows/E-<CELL>.txt` and\n")
    w("`rows-off/off-<CELL>.txt`. **No solver ran here and no number is\n")
    w("retyped.** The consensus column is machine-read from M3c's committed\n")
    w("`../2026-09-m7-w2-m3c-default-census/tables/census.md`.\n\n")
    w("**The subject is arm E AS IMPLEMENTED IN THE PINNED M4 BINARY**\n")
    w("(`RULE.md` §3.1, sha256 `c1be17d2…fe50a720`), never \"EXPAND\" and never\n")
    w("\"canonical EXPAND\". Two departures from the paper live in that binary\n")
    w("and are disclosed in `README.md`; the columns they bear on are flagged\n")
    w("in §3 below.\n\n")

    # ---- 1. the headline table -----------------------------------------
    w("## 1. The ten cells, E against its copied OFF control\n\n")
    w("`RULE.md` §12's asked-for columns. `minors E/off` is printed to four\n")
    w("places. `f vs OFF` is the relative move `|f_E − f_off| / max(1,|f_off|)`;\n")
    w("`f vs consensus` is `|f_E − consensus| / max(1,|consensus|)` where a\n")
    w("consensus exists.\n\n")
    w("| cell | status off → E | majors off → E | minors off → E | minors E/off |"
      " cap exits off → E | restoration iters off → E | `arm_min_steps` |"
      " `arm_unpinned_adds` | `arm_pivot_switches` | `arm_resets` |"
      " f vs OFF | f vs consensus | verdict |\n")
    w("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n")
    for c in TEN:
        o, e = R[("off", c)], R[("E", c)]
        mo, me = int(o["qp_minor_iters"]), int(e["qp_minor_iters"])
        fo, fe = float(o["f"]), float(e["f"])
        dfo = abs(fe - fo) / max(1.0, abs(fo))
        cons = CONS.get(c, "(cell not in census)")
        mcon = re.match(r"consensus ([-0-9.eE+]+)", cons)
        if mcon:
            cv = float(mcon.group(1))
            dfc = "%.3e" % (abs(fe - cv) / max(1.0, abs(cv)))
        else:
            dfc = "ABSENT (no consensus)"
        if e["deadline_fired"] == "1":
            verdict = "**TRUNCATED**"
        else:
            ro, re_ = RANK[o["status"]], RANK[e["status"]]
            verdict = ("CONVERTED-or-improved?" if re_ < ro
                       else ("**HARM**" if re_ > ro else "none"))
        w("| %s | `%s` → `%s` | %s → %s | %s → %s | **%.4f** | %s → %s | %s → %s |"
          " %s | %s | %s | %s | %.3e | %s | %s |\n"
          % (c, o["status"], e["status"], o["major_iters"], e["major_iters"],
             mo, me, me / mo, o["qp_maxiter_exits"], e["qp_maxiter_exits"],
             o["restoration_iters"], e["restoration_iters"],
             e["arm_min_steps"], e["arm_unpinned_adds"],
             e["arm_pivot_switches"], e["arm_resets"], dfo, dfc, verdict))
    w("\nThe `verdict` column is `verdict.txt`'s, applied here by the same\n")
    w("rank order (M4 `RULE.md` §7.2) so the table and the verdict file cannot\n")
    w("drift. `none` means rank(E) == rank(OFF): not `CONVERTED`, not `HARM`,\n")
    w("not `improved, unverified`.\n\n")
    w("The consensus strings, verbatim from M3c's census:\n\n")
    for c in TEN:
        w("* `%s` — %s\n" % (c, CONS.get(c, "(cell not in census)")))
    w("\n")

    # ---- 2. every §7.1 field, per cell ----------------------------------
    w("## 2. Every M4 §7.1 field, plus §6.6's arm counters, per cell\n\n")
    w("`<<` marks a value that differs from that cell's OFF control.\n\n")
    for c in TEN:
        w("### %s\n\n" % c)
        w("| field | off | E |\n|---|---|---|\n")
        for k in S71 + TIES + ARMK:
            a, b = R[("off", c)].get(k, "(absent)"), R[("E", c)].get(k, "(absent)")
            w("| `%s` | `%s` | `%s`%s |\n" % (k, a, b, " <<" if a != b else ""))
        w("\n")
        w("Solve-summary aggregates, summed over the run's own per-solve table\n"
          "in the same row file:\n\n")
        w("| | off | E |\n|---|---|---|\n")
        agg = {}
        for arm, p in (("off", os.path.join(off_dir, "off-%s.txt" % c)),
                       ("E", os.path.join(e_dir, "E-%s.txt" % c))):
            rs = summary(p)
            agg[arm] = {
                "QP solves": len(rs),
                "minors": sum(int(r["minors"]) for r in rs),
                "degenerate minors": sum(int(r["degenerate"]) for r in rs),
                "rides": sum(int(r["rides"]) for r in rs),
                "drops": sum(int(r["drops"]) for r in rs),
                "adds": sum(int(r["adds"]) for r in rs),
            }
        for k in ("QP solves", "minors", "degenerate minors", "rides", "drops",
                  "adds"):
            base, arm = agg["off"][k], agg["E"][k]
            w("| %s | %d | %d%s |\n" % (k, base, arm,
                                        " <<" if arm != base else ""))
        w("\n")

    # ---- 3. the two disclosed implementation departures -----------------
    w("## 3. The two departures in the pinned binary, and the columns they bite\n\n")
    w("Both were found by the Codex review of M4\n")
    w("(`.superpowers/m7-w2-m4arm-review-sol.md` §1a, §4). They are properties\n")
    w("of the binary every row above came from, so they are printed here beside\n")
    w("the counters they bear on rather than only argued in prose.\n\n")
    w("**(1) The EXPAND reset fires one iteration early.** `begin_minor()`\n")
    w("returns true at `expand_iter >= K` and the caller resets *before* that\n")
    w("minor's ratio test, so the K-th iteration runs at `delta_0` and not at\n")
    w("the expanded `delta`; the paper performs K expanded iterations and resets\n")
    w("afterwards. M4's neutrality legs never reached a reset, so M4's data\n")
    w("could not see this. **THESE LEGS DID.**\n\n")
    w("| cell | `arm_resets` | `arm_reset_nontrivial_moves` |"
      " `arm_expand_iter_at_exit` | `arm_delta_at_exit` | did the reset path run? |\n")
    w("|---|---|---|---|---|---|\n")
    for c in TEN:
        e = R[("E", c)]
        n = int(e["arm_resets"])
        w("| %s | %s | %s | %s | %s | %s |\n"
          % (c, e["arm_resets"], e["arm_reset_nontrivial_moves"],
             e["arm_expand_iter_at_exit"], e["arm_delta_at_exit"],
             "**YES — %d time(s), under the off-by-one**" % n if n else "no"))
    w("\n**(2) The negative-curvature ride calls the armed ratio test but its\n")
    w("add path always re-pins the blocking bound exactly**, defeating the\n")
    w("`alpha_min` floor on that path. `arm_min_steps` counts every minor whose\n")
    w("step was the floor; `arm_unpinned_adds` counts those on which the exact\n")
    w("re-pin was actually skipped. **Equality means the ride floor path was\n")
    w("never exercised on that cell. Inequality means it was, and the\n")
    w("difference is the number of floor steps whose displacement was silently\n")
    w("re-pinned away.**\n\n")
    w("| cell | `arm_min_steps` | `arm_unpinned_adds` | difference ="
      " HIDDEN RE-PINS | ride floor path exercised? |\n")
    w("|---|---|---|---|---|\n")
    tot = 0
    for c in TEN:
        e = R[("E", c)]
        ms, ua = int(e["arm_min_steps"]), int(e["arm_unpinned_adds"])
        tot += ms - ua
        w("| %s | %d | %d | **%d** | %s |\n"
          % (c, ms, ua, ms - ua,
             "**YES**" if ms != ua else "no (equal)"))
    w("| **total** | | | **%d** | |\n\n" % tot)
    print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
