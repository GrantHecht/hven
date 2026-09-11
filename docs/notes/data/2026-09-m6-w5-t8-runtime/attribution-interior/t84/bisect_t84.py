#!/usr/bin/env python3
"""W5 T8.9r-attrib3 -- the intra-T8.4 per-row wall bisect, from this leg's own raw/.

Run from the artifact directory (docs/notes/data/.../attribution-interior/t84/):
    python3 bisect_t84.py [-|out.txt]

It emits wall.csv beside itself and prints:
  1  the six arms, their binaries' identity relations, and the row counts
  2  the per-row WALL step per commit, median of five rounds, R3 warm-up row EXCLUDED
  3  the cumulative check against reading.md section 11 / section 5
  4  the localisation -- each commit's share of the cumulative, in log space
  5  the two ZERO CONTROLS, read as this instrument's floor

The wall numbers assert wall clock and were taken under the fix1 R2 discipline;
logs/ carries every batch's brackets and IDLE-PROOF.md the arithmetic over them.
"""
import csv, math, os, statistics as st, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ARMS = [("01", "T8.3",    "510a4bb"), ("02", "8ae1618", "8ae1618"),
        ("03", "9cebbbe", "9cebbbe"), ("04", "9ce9bb2", "9ce9bb2"),
        ("05", "fix1",    "5124aa1"), ("06", "T8.4",    "3c8e43b")]
NN   = [a for a, _, _ in ARMS]
TASK = {a: t for a, t, _ in ARMS}
SHA  = {a: s for a, _, s in ARMS}
ROUNDS = (1, 2, 3, 4, 5)

def rel(*p): return os.path.join(HERE, *p)

def csv_rows(path):
    with open(path) as fh:
        lines = [l for l in fh if not l.startswith("#")]
    return [(i, r["cell_id"], float(r["wall_s"])) for i, r in enumerate(csv.DictReader(lines))]

def main():
    out = sys.stdout if len(sys.argv) < 2 or sys.argv[1] == "-" else open(sys.argv[1], "w")
    w = out.write
    wall, nrows, order = {}, {}, None
    for a in NN:
        for r in ROUNDS:
            rs = csv_rows(rel("raw", "wall", "w-b%s-r%d.csv" % (a, r)))
            nrows[a] = len(rs)
            if a == "01" and r == 1:
                order = [k for _, k, _ in rs][:12]
            for _, k, s in rs:
                wall.setdefault((a, k), []).append(s)
    with open(rel("wall.csv"), "w", newline="") as fh:
        cw = csv.writer(fh)
        cw.writerow(["arm", "commit", "sha", "round", "row_index", "row", "wall_s"])
        for a in NN:
            for r in ROUNDS:
                for i, k, s in csv_rows(rel("raw", "wall", "w-b%s-r%d.csv" % (a, r))):
                    cw.writerow([a, TASK[a], SHA[a], r, i, k, "%.9f" % s])

    w("=" * 100 + "\n1. THE SIX ARMS\n" + "=" * 100 + "\n\n")
    w("  arm  commit    sha       rows   exe sha256 (first 16)   lib sha256 (first 16)\n")
    import hashlib
    def h(p):
        return hashlib.sha256(open(p, "rb").read()).hexdigest()
    for a in NN:
        e = rel("raw", "bin", "%s.exe.sha256" % SHA[a])
        l = rel("raw", "bin", "%s.lib.sha256" % SHA[a])
        es = open(e).read().split()[0] if os.path.exists(e) else "?" * 64
        ls = open(l).read().split()[0] if os.path.exists(l) else "?" * 64
        w("  b%s   %-8s  %s  %4d   %s       %s\n" % (a, TASK[a], SHA[a], nrows[a], es[:16], ls[:16]))

    w("\n" + "=" * 100 + "\n")
    w("2. THE PER-ROW WALL STEP PER COMMIT -- median of five rounds, arm order rotated per round\n")
    w("=" * 100 + "\n\n")
    w("THIS ASSERTS WALL CLOCK (CLAUDE.md section 7). Solo, taskset -c 2, MKL_NUM_THREADS=1,\n")
    w("one solve at a time under the box lock, the fix1 R2 pinned-core discipline; IDLE-PROOF.md.\n\n")
    w("R3 -- THE FIRST ROW EVERY PROCESS WRITES IS EXCLUDED BY POSITION (predeclaration.txt,\n")
    w("hashed at 18:50:35 UTC, before the first wall batch's FOREGROUND_START at 18:51:1x UTC).\n")
    w("It is the SAME row at every arm: %s.\n\n" % order[0])
    med = {(a, k): st.median(wall[(a, k)]) for a in NN for k in order}
    hdr = "  " + "row".ljust(42) + "".join(TASK[a].rjust(10) for a in NN[1:]) + "    cumul"
    w(hdr + "\n  " + "-" * (len(hdr) - 2) + "\n")
    steps = {}
    for j, k in enumerate(order):
        row = [med[(NN[i + 1], k)] / med[(NN[i], k)] for i in range(5)]
        for i in range(5):
            steps[(NN[i + 1], k)] = row[i]
        cum = med[("06", k)] / med[("01", k)]
        w("  " + k.ljust(42) + "".join("%10.4f" % s for s in row) + "%9.4f" % cum +
          ("   EXCL" if j == 0 else "") + "\n")
    scored = order[1:]
    w("\n  Scored rows: %d (the twelve base F7 rows less the R3 warm-up).\n" % len(scored))
    w("  Per-commit worst/best/median step over the scored rows:\n")
    for i in range(5):
        a = NN[i + 1]
        v = [steps[(a, k)] for k in scored]
        w("    %-8s min %.4f  max %.4f  median %.4f  rows above 1.01: %2d/%d  below 0.99: %2d/%d\n"
          % (TASK[a], min(v), max(v), st.median(v),
             sum(1 for x in v if x > 1.01), len(v), sum(1 for x in v if x < 0.99), len(v)))
    w("\n  Round-to-round spread of each arm's own scored-row total (max/min - 1):\n")
    for a in NN:
        tot = [sum(wall[(a, k)][r] for k in scored) for r in range(5)]
        w("    b%s %-8s  %.4f   %s\n" % (a, TASK[a], max(tot) / min(tot) - 1,
                                          "  ".join("%.4f" % t for t in tot)))

    w("\n" + "=" * 100 + "\n3. THE CUMULATIVE CHECK\n" + "=" * 100 + "\n\n")
    tot_l = sum(med[("06", k)] for k in scored) / sum(med[("01", k)] for k in scored)
    w("  Corpus over the eleven scored rows, b01 -> b06 (510a4bb -> 3c8e43b): %.4f\n" % tot_l)
    w("  reading.md section 11's T8.4 column, median share row:               +1.03 of a 1.0250 cumulative\n")
    w("  reading.md section 11's T8.4 per-row steps ran 1.0147 to 1.0425; this leg's b01 -> b06\n")
    w("  per-row cumulative runs %.4f to %.4f.\n"
      % (min(med[("06", k)] / med[("01", k)] for k in scored),
         max(med[("06", k)] / med[("01", k)] for k in scored)))

    w("\n" + "=" * 100 + "\n4. THE LOCALISATION -- each commit's share of the cumulative, in log space\n")
    w("=" * 100 + "\n\n")
    w("  " + "row".ljust(42) + "".join(TASK[a].rjust(10) for a in NN[1:]) + "\n")
    shares = {}
    for k in scored:
        c = math.log(med[("06", k)] / med[("01", k)])
        row = [math.log(steps[(a, k)]) / c for a in NN[1:]]
        for a, s in zip(NN[1:], row):
            shares[(a, k)] = s
        w("  " + k.ljust(42) + "".join("%10.2f" % s for s in row) + "\n")
    w("\n  Per commit, over the eleven scored rows:\n")
    for a in NN[1:]:
        v = [shares[(a, k)] for k in scored]
        w("    %-8s median share %+6.2f   rows at or above 0.80: %2d/%d\n"
          % (TASK[a], st.median(v), sum(1 for x in v if x >= 0.80), len(v)))
    carrier = max(NN[1:], key=lambda a: st.median([shares[(a, k)] for k in scored]))
    ms = st.median([shares[(carrier, k)] for k in scored])
    w("\n  CARRIER (the pre-declared rule: largest median share, and >= 0.80 to be called the\n")
    w("  carrier of at least 80 %% of the step): %s, median share %+.2f -- %s\n"
      % (TASK[carrier], ms, "CARRIER" if ms >= 0.80 else "NO SINGLE CARRIER"))

    w("\n" + "=" * 100 + "\n5. THE TWO ZERO CONTROLS\n" + "=" * 100 + "\n\n")
    w("  b01 -> b02 (510a4bb -> 8ae1618) and b05 -> b06 (5124aa1 -> 3c8e43b) each produced a\n")
    w("  BYTE-IDENTICAL hven_sqp_corpus (raw/bin/*.sha256, and the cmp in logs/B3-identity.log).\n")
    w("  Their true step is EXACTLY ZERO on every row, so whatever they read is this\n")
    w("  instrument's floor and can be nothing else.\n\n")
    for a in ("02", "06"):
        v = [steps[(a, k)] for k in scored]
        w("    %-8s min %.4f  max %.4f  median %.4f  |ln| median %.5f\n"
          % (TASK[a], min(v), max(v), st.median(v), st.median([abs(math.log(x)) for x in v])))
    w("\n  And the floor to compare the carrier against, stated as a ratio:\n")
    fl = max(st.median([abs(math.log(steps[(a, k)])) for k in scored]) for a in ("02", "06"))
    ca = st.median([abs(math.log(steps[(carrier, k)])) for k in scored])
    w("    worst zero-control median |ln step| %.5f ; %s's %.5f ; ratio %.1fx\n"
      % (fl, TASK[carrier], ca, ca / fl))
    w("\nAPPLE / ACCELERATE: UNOBSERVED.   WINDOWS: UNOBSERVED.\n")
    if out is not sys.stdout:
        out.close()
    return 0

if __name__ == "__main__":
    sys.exit(main())
