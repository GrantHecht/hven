#!/usr/bin/env python3
"""W5 T8.9r fix3 -- THE SINGLE-ROW SOLVE BRACKET, read off the rows the measured
processes themselves wrote.

WHY THIS FILE EXISTS. attrib5 read the single-row leg's wall from `perf.csv`'s
`elapsed_s` -- the WHOLE PROCESS, as `perf stat` reports it -- and concluded from
0.995-1.009 that "the +2.5 % step does not appear in a single-row process at
all". astra's fix2 review is right that that is the wrong population to take
that conclusion on, and the settler's ruling R12 withdraws the conclusion:

  * `elapsed_s` is the process, start to exit. It carries `execve`, the dynamic
    loader, the model build, the CSV write and teardown OUTSIDE the solve, and
    its own within-arm spread on these rows is 0.2-1.2 % -- the size of the
    thing being looked for.
  * `wall_s` is the leg's OWN SOLVE BRACKET, written into the row by the same
    harness the 43-row leg uses. It is the narrower window and the quieter one:
    its within-arm spread is an order of magnitude smaller.

BOTH still contain MKL's `dsecnd()` first-call clock calibration -- it is called
from inside `solve()` (`src/drivers/interior_point_solver.cpp:5535`), so it is
inside the bracket too. The calibration is a WALL-TIMED busy-wait of fixed
duration, so it enters both arms equally and differences out of a ratio; what it
does do is make every ratio here a ratio of (row + ~0.95 s), which DAMPS the
step rather than inventing one. The figures below are therefore a LOWER BOUND on
the row's own relative step, not an upper one.

NOTHING HERE IS ASSERTED AS A TIMING (CLAUDE.md section 7). This leg carries no
R2' evidence at all -- its batch logs have no `PS_SNAPSHOT`/`CPUSTAT` bracket --
so both wall populations are INFORMATIONAL. They are printed because the reading
quoted one of them and drew a conclusion the other does not support.

Usage:  wall_bracket.py [<single-row dir>]        (default: this file's dir)
Reads only `raw/csv/<batch>/<arm>/<row>-r<n>.csv`, which are retained in full.
"""
import csv
import os
import statistics
import sys

ARMS = ("510a4bb", "9cebbbe", "control")
BATCHES = ("A", "B", "C")
ROUNDS = (1, 2, 3, 4, 5)

# section 11's eleven scored rows: four dual-binding F7 cells x three treatments
# MINUS `f7_n1000_bound_physics/MakeParameter`, which section 11's PRE-DECLARED
# positional rule excludes as the leg process's warm-up row. The twelfth is
# reported below the table, unscored, exactly as `classification.md` reports it.
SCORED = [
    ("f7_n1000_bound_physics", "MakeConstraint"),
    ("f7_n1000_bound_physics", "RelaxBounds"),
    ("f7_n5000_bound_physics", "MakeParameter"),
    ("f7_n5000_bound_physics", "MakeConstraint"),
    ("f7_n5000_bound_physics", "RelaxBounds"),
    ("f7_n10000_bound_neutral", "MakeParameter"),
    ("f7_n10000_bound_neutral", "MakeConstraint"),
    ("f7_n10000_bound_neutral", "RelaxBounds"),
    ("f7_n20000_bound_neutral", "MakeParameter"),
    ("f7_n20000_bound_neutral", "MakeConstraint"),
    ("f7_n20000_bound_neutral", "RelaxBounds"),
]
UNSCORED = ("f7_n1000_bound_physics", "MakeParameter")
SUBTRAHEND = [("hs071_x1_fixed", t) for t in ("MakeParameter", "MakeConstraint", "RelaxBounds")]


def wall_of(root, batch, arm, cell, treatment, rnd):
    path = os.path.join(root, "raw", "csv", batch, arm,
                        "%s-%s-r%d.csv" % (cell, treatment, rnd))
    with open(path, newline="") as fh:
        lines = [ln for ln in fh if not ln.startswith("#")]
    rows = list(csv.DictReader(lines))
    if len(rows) != 1:
        raise SystemExit("wall_bracket: %s carries %d rows, expected exactly 1" % (path, len(rows)))
    return float(rows[0]["wall_s"])


def series(root, batch, cell, treatment):
    return {arm: [wall_of(root, batch, arm, cell, treatment, r) for r in ROUNDS] for arm in ARMS}


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
    print("W5 T8.9r fix3 -- the single-row SOLVE BRACKET (`wall_s`), medians of five rounds")
    print("source: raw/csv/<batch>/<arm>/<row>-r<n>.csv -- the rows the measured processes wrote")
    print("parent 510a4bb   culprit 9cebbbe   control = 510a4bb's executable at a second path")
    print("INFORMATIONAL ONLY (CLAUDE.md section 7); this leg carries no R2' evidence.")
    print()

    for batch, what in (("A", "PASS A -- the instruction pass; the SAME processes perf.csv's "
                              "elapsed_s comes from"),
                        ("B", "PASS B -- the Zen 3 front-end pass, an INDEPENDENT round set"),
                        ("C", "PASS C -- the memory pass, an INDEPENDENT round set")):
        print("=== %s ===" % what)
        print("%-44s %9s %9s %9s %9s %9s %7s %7s" % (
            "row", "parent", "culprit", "control", "c/p", "x/p", "c>p", "x>p"))
        sp = sc = sx = 0.0
        for cell, treat in SCORED:
            s = series(root, batch, cell, treat)
            p = statistics.median(s["510a4bb"])
            c = statistics.median(s["9cebbbe"])
            x = statistics.median(s["control"])
            sp += p
            sc += c
            sx += x
            nc = sum(1 for i in range(5) if s["9cebbbe"][i] > s["510a4bb"][i])
            nx = sum(1 for i in range(5) if s["control"][i] > s["510a4bb"][i])
            print("%-44s %9.6f %9.6f %9.6f %9.6f %9.6f %5d/5 %5d/5" % (
                cell + "/" + treat, p, c, x, c / p, x / p, nc, nx))
        print("%-44s %9.6f %9.6f %9.6f %9.6f %9.6f" % (
            "CORPUS (sum of the eleven medians)", sp, sc, sx, sc / sp, sx / sp))
        s = series(root, batch, *UNSCORED)
        p = statistics.median(s["510a4bb"])
        c = statistics.median(s["9cebbbe"])
        x = statistics.median(s["control"])
        nc = sum(1 for i in range(5) if s["9cebbbe"][i] > s["510a4bb"][i])
        print("%-44s %9.6f %9.6f %9.6f %9.6f %9.6f %5d/5   (12th, NOT SCORED)" % (
            UNSCORED[0] + "/" + UNSCORED[1], p, c, x, c / p, x / p, nc))
        print()

    print("=== PASS S -- the subtrahend process (hs071_x1_fixed, ~0.00018 s of solve) ===")
    print("Its bracket is ~0.96 s: that is the calibration busy-wait, and it is INSIDE `wall_s`.")
    print("%-44s %9s %9s %9s %9s %9s" % ("row", "parent", "culprit", "control", "c/p", "x/p"))
    for cell, treat in SUBTRAHEND:
        s = series(root, "S", cell, treat)
        p = statistics.median(s["510a4bb"])
        c = statistics.median(s["9cebbbe"])
        x = statistics.median(s["control"])
        print("%-44s %9.6f %9.6f %9.6f %9.6f %9.6f" % (
            cell + "/" + treat, p, c, x, c / p, x / p))
    print()

    print("=== THE PER-ROUND PAIRS, pass A (culprit/parent, round by round) ===")
    for cell, treat in SCORED:
        s = series(root, "A", cell, treat)
        pr = [s["9cebbbe"][i] / s["510a4bb"][i] for i in range(5)]
        xr = [s["control"][i] / s["510a4bb"][i] for i in range(5)]
        print("%-44s c/p %s" % (cell + "/" + treat, " ".join("%.5f" % v for v in pr)))
        print("%-44s x/p %s" % ("", " ".join("%.5f" % v for v in xr)))

    print()
    print("=== WITHIN-ARM SPREAD, pass A: the bracket against the whole process ===")
    print("(max-min)/median per arm, in percent, on the eleven scored rows. The BRACKET")
    print("column is `wall_s` from raw/csv/A/; the PROCESS column is `elapsed_s` from")
    print("perf.csv, pass A -- the same five rounds of the same processes. THE TWO WINDOWS ARE")
    print("EQUALLY NOISY: neither population is quieter than the other, so the disagreement")
    print("between their ratios is NOT a resolution difference. What it is, is the table below.")
    el = {}
    with open(os.path.join(root, "perf.csv"), newline="") as fh:
        for r in csv.DictReader(fh):
            if r["pass_"] == "A" and r["event"] == "elapsed_s" and r["value"] != "":
                el.setdefault((r["cell"], r["treatment"], r["arm"]),
                              [None] * 5)[int(r["round"]) - 1] = float(r["value"])
    print("%-40s %18s %18s" % ("row", "BRACKET wall_s %", "PROCESS elapsed_s %"))
    print("%-40s %8s %9s %8s %9s" % ("", "parent", "culprit", "parent", "culprit"))
    for cell, treat in SCORED:
        s = series(root, "A", cell, treat)
        b = [100.0 * (max(s[a]) - min(s[a])) / statistics.median(s[a]) for a in ARMS]
        e = [100.0 * (max(el[(cell, treat, a)]) - min(el[(cell, treat, a)]))
             / statistics.median(el[(cell, treat, a)]) for a in ARMS]
        print("%-40s %8.4f %9.4f %8.4f %9.4f" % (cell + "/" + treat, b[0], b[1], e[0], e[1]))
    print()
    print("=== THE WHOLE-PROCESS ELAPSED, pass A, beside the bracket (both informational) ===")
    print("%-40s %10s %10s %10s %10s" % ("row", "proc c/p", "proc x/p", "brkt c/p", "brkt x/p"))
    sp = sc = sx = 0.0
    ep = ec_ = ex = 0.0
    for cell, treat in SCORED:
        s = series(root, "A", cell, treat)
        bp, bc, bx = (statistics.median(s[a]) for a in ARMS)
        pp, pc, px = (statistics.median(el[(cell, treat, a)]) for a in ARMS)
        sp += bp; sc += bc; sx += bx
        ep += pp; ec_ += pc; ex += px
        print("%-40s %10.6f %10.6f %10.6f %10.6f" % (
            cell + "/" + treat, pc / pp, px / pp, bc / bp, bx / bp))
    print("%-40s %10.6f %10.6f %10.6f %10.6f" % (
        "CORPUS (sum of the eleven medians)", ec_ / ep, ex / ep, sc / sp, sx / sp))

    print()
    print("=== AND WHY THEY DISAGREE: THE TIME OUTSIDE THE BRACKET, pass A ===")
    print("outside = elapsed_s - wall_s, per round, per row: execve, the loader, the corpus")
    print("harness, `ipm.transcribe()` (which runs immediately BEFORE the bracket opens --")
    print("`bench/ipm_corpus_leg.cpp:387-391`, and those lines are byte-identical at the two")
    print("arms), the CSV write and teardown. Medians of five, then summed over the eleven.")
    print("%-40s %9s %9s %9s %9s %9s %9s" % (
        "row", "p brkt", "c brkt", "p outsd", "c outsd", "brkt c/p", "outsd c/p"))
    T = {a: [0.0, 0.0, 0.0] for a in ARMS}
    for cell, treat in SCORED:
        s_ = series(root, "A", cell, treat)
        m = {}
        for a in ARMS:
            b = s_[a]
            e = [el[(cell, treat, a)][i] for i in range(5)] if isinstance(
                el[(cell, treat, a)], dict) else el[(cell, treat, a)]
            o = [e[i] - b[i] for i in range(5)]
            m[a] = (statistics.median(e), statistics.median(b), statistics.median(o))
            T[a][0] += m[a][0]
            T[a][1] += m[a][1]
            T[a][2] += m[a][2]
        print("%-40s %9.5f %9.5f %9.5f %9.5f %9.6f %9.6f" % (
            cell + "/" + treat, m["510a4bb"][1], m["9cebbbe"][1],
            m["510a4bb"][2], m["9cebbbe"][2],
            m["9cebbbe"][1] / m["510a4bb"][1], m["9cebbbe"][2] / m["510a4bb"][2]))
    p_, c_, x_ = T["510a4bb"], T["9cebbbe"], T["control"]
    print()
    print("%-40s %9s %9s %9s" % ("TOTAL over the eleven rows", "process", "bracket", "outside"))
    for name, v in (("parent 510a4bb", p_), ("culprit 9cebbbe", c_), ("control", x_)):
        print("%-40s %9.5f %9.5f %9.5f" % (name, v[0], v[1], v[2]))
    print("%-40s %9.6f %9.6f %9.6f" % ("culprit / parent", c_[0] / p_[0], c_[1] / p_[1],
                                       c_[2] / p_[2]))
    print("%-40s %9.6f %9.6f %9.6f" % ("control / parent", x_[0] / p_[0], x_[1] / p_[1],
                                       x_[2] / p_[2]))
    print("%-40s %+9.4f %+9.4f %+9.4f" % ("culprit - parent (seconds)", c_[0] - p_[0],
                                          c_[1] - p_[1], c_[2] - p_[2]))
    print("%-40s %+9.4f %+9.4f %+9.4f" % ("control - parent (seconds)", x_[0] - p_[0],
                                          x_[1] - p_[1], x_[2] - p_[2]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
