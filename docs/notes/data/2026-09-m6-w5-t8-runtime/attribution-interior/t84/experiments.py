#!/usr/bin/env python3
"""W5 T8.9r-attrib3 -- the two Step-B experiments, from this directory's own raw/.

Run from the artifact directory:  python3 experiments.py [-|out.txt]

Four arms measured in ONE set of five rounds, so every pair is paired inside every
batch:
  e1  parent   8ae1618            unpatched
  e2  culprit  9cebbbe            unpatched
  e3  E1       9cebbbe + experiment-1.patch  (hot members back at the PARENT's offsets)
  e4  E2       8ae1618 + experiment-2.patch  (hot members at the CULPRIT's offsets)

RECOVERY (E1)      = 1 - (step(e3/e1) - 1) / (step(e2/e1) - 1)
REPRODUCTION (E2)  =     (step(e4/e1) - 1) / (step(e2/e1) - 1)
Both are reported per row and as the median over the eleven scored rows, and both
are computed in LOG space as well, which is what the leg quotes.

THIS ASSERTS WALL CLOCK (CLAUDE.md section 7): fix1's R2 discipline, five rounds,
arm order rotated, the R3 positional warm-up row excluded, IDLE-PROOF-X.md.
"""
import csv, math, os, statistics as st, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ARMS = [("e1", "parent"), ("e2", "culprit"), ("e3", "E1"), ("e4", "E2")]
ROUNDS = (1, 2, 3, 4, 5)

def rel(*p): return os.path.join(HERE, *p)

def csv_rows(path):
    with open(path) as fh:
        lines = [l for l in fh if not l.startswith("#")]
    return [(i, r["cell_id"], float(r["wall_s"])) for i, r in enumerate(csv.DictReader(lines))]

def main():
    out = sys.stdout if len(sys.argv) < 2 or sys.argv[1] == "-" else open(sys.argv[1], "w")
    w = out.write
    wall, order = {}, None
    for a, _ in ARMS:
        for r in ROUNDS:
            rs = csv_rows(rel("raw", "xwall", "x-%s-r%d.csv" % (a, r)))
            if a == "e1" and r == 1:
                order = [k for _, k, _ in rs][:12]
            for _, k, s in rs:
                wall.setdefault((a, k), []).append(s)
    with open(rel("experiments.csv"), "w", newline="") as fh:
        cw = csv.writer(fh)
        cw.writerow(["arm", "kind", "round", "row_index", "row", "wall_s"])
        for a, kind in ARMS:
            for r in ROUNDS:
                for i, k, s in csv_rows(rel("raw", "xwall", "x-%s-r%d.csv" % (a, r))):
                    cw.writerow([a, kind, r, i, k, "%.9f" % s])
    med = {(a, k): st.median(wall[(a, k)]) for a, _ in ARMS for k in order}
    scored = order[1:]

    w("=" * 104 + "\n")
    w("THE TWO EXPERIMENTS -- per-row wall, median of five rounds, R3 warm-up row EXCLUDED\n")
    w("=" * 104 + "\n\n")
    w("  " + "row".ljust(42) + "  culprit/parent      E1/parent      E2/parent   E1 recov   E2 repro\n")
    w("  " + "-" * 100 + "\n")
    rec, rep = [], []
    for j, k in enumerate(order):
        c = med[("e2", k)] / med[("e1", k)]
        a1 = med[("e3", k)] / med[("e1", k)]
        a2 = med[("e4", k)] / med[("e1", k)]
        lc = math.log(c)
        r1 = 1 - math.log(a1) / lc if abs(lc) > 1e-12 else float("nan")
        r2 = math.log(a2) / lc if abs(lc) > 1e-12 else float("nan")
        if j > 0:
            rec.append(r1); rep.append(r2)
        w("  " + k.ljust(42) + "%15.4f%15.4f%15.4f%11.2f%11.2f%s\n"
          % (c, a1, a2, r1, r2, "   EXCL" if j == 0 else ""))
    w("\n  Scored rows: %d\n" % len(scored))
    tot = lambda a: sum(med[(a, k)] for k in scored)
    w("\n  CORPUS over the eleven scored rows (sum of the medians):\n")
    for a, kind in ARMS:
        w("    %-8s %-8s  %.6f s   ratio to parent %.4f\n" % (a, kind, tot(a), tot(a) / tot("e1")))
    lc = math.log(tot("e2") / tot("e1"))
    w("\n    the step under test (culprit/parent)  %+.4f %%\n" % ((tot("e2") / tot("e1") - 1) * 100))
    w("    E1 corpus RECOVERY   %.1f %%   (1 - ln(E1/parent)/ln(culprit/parent))\n"
      % ((1 - math.log(tot("e3") / tot("e1")) / lc) * 100))
    w("    E2 corpus REPRODUCTION %.1f %%   (ln(E2/parent)/ln(culprit/parent))\n"
      % ((math.log(tot("e4") / tot("e1")) / lc) * 100))
    w("\n  Per-row medians: E1 recovery %.2f, E2 reproduction %.2f\n" % (st.median(rec), st.median(rep)))
    w("  E1 recovery >= 0.80 on %d of %d rows; E2 reproduction >= 0.80 on %d of %d rows.\n"
      % (sum(1 for x in rec if x >= 0.80), len(rec), sum(1 for x in rep if x >= 0.80), len(rep)))
    w("\n  Round-to-round spread of each arm's scored-row total (max/min - 1):\n")
    for a, kind in ARMS:
        t = [sum(wall[(a, k)][i] for k in scored) for i in range(5)]
        w("    %-8s %-8s %.4f   %s\n" % (a, kind, max(t) / min(t) - 1, "  ".join("%.4f" % x for x in t)))
    w("\nAPPLE / ACCELERATE: UNOBSERVED.   WINDOWS: UNOBSERVED.\n")
    if out is not sys.stdout:
        out.close()
    return 0

if __name__ == "__main__":
    sys.exit(main())
