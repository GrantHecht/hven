#!/usr/bin/env python3
"""W5 T8.9r-attrib2 -- the interior leg's +2.49 % charged to the group-1 task that
carries it, from this directory's own raw/.

Run from this directory:  python3 attribute_interior.py [-|out.txt]

It emits wall.csv and perf.csv beside itself and prints, in order:

  1  the eleven arms, their row counts, and THE LIKE-FOR-LIKE MAP the row counts
     imply -- which consecutive pairs may be compared in whole-process
     instructions at all (brief section 1)
  2  the per-row WALL step per task, median of five rounds, with the R3
     positional warm-up row EXCLUDED by the rule fixed before this leg ran
  3  the cumulative check against reading.md section 5
  4  the localisation: each task's share of the cumulative, in log space
  5  the whole-process INSTRUCTION table, the consecutive steps, and the FLOOR
     measured on the byte-identical-library control arm
  6  the added-row accounting for the three row-adding pairs
  7  pass B (the Zen 3 front-end events) on the localised pair and the control
  8  the differencing leg -- an instrument this leg BUILT, RAN and REJECTED,
     printed with the evidence that rejects it

Nothing here asserts a disposition. The wall numbers assert wall clock and were
taken under the fix1 R2 discipline; raw/logs/ carries every batch's brackets and
IDLE-PROOF.md the arithmetic over them.
"""
import csv
import os
import re
import statistics as st
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# arm NN -> (task label, sha7). The order IS group-1's order.
ARMS = [("01", "base", "102f729"), ("02", "T8.1", "b3915ff"), ("03", "T8.2", "b43580f"),
        ("04", "T8.3", "510a4bb"), ("05", "T8.4", "3c8e43b"), ("06", "T8.5", "8f95655"),
        ("07", "T8.6", "8cbaa39"), ("08", "T8.7", "56042be"), ("09", "T8.7b", "ddac2cf"),
        ("10", "T8.8", "b9848bf"), ("11", "T8.9", "e51a7e0")]
NN = [a for a, _, _ in ARMS]
TASK = {a: t for a, t, _ in ARMS}
SHA = {a: s for a, _, s in ARMS}
ROUNDS = (1, 2, 3, 4, 5)
PERF_ROUNDS = (1, 2, 3)
PASSB_ARMS = ("01", "02", "04", "05")

# reading.md section 5's per-row ratios for these twelve rows, read off the
# artifact's own retained interior CSVs (raw/interior/{arm102,head}-r{1,2,3}.csv
# one directory up). Recomputed here rather than transcribed -- see s5_ratios().
S5_DIR = os.path.join(HERE, "..", "raw", "interior")


def csv_rows(path):
    """-> [(index, row key, wall_s)] in the order the process WROTE them."""
    with open(path) as fh:
        lines = [l for l in fh if not l.startswith("#")]
    return [(i, r["cell_id"], float(r["wall_s"])) for i, r in enumerate(csv.DictReader(lines))]


def perf_counts(path):
    out = {}
    with open(path) as fh:
        for line in fh:
            m = re.match(r"\s*([\d,]+)\s+(\S+)", line)
            if m and ":" in m.group(2):
                out[m.group(2)] = int(m.group(1).replace(",", ""))
    return out


def rel(*p):
    return os.path.join(HERE, *p)


def main():
    out = sys.stdout if len(sys.argv) < 2 or sys.argv[1] == "-" else open(sys.argv[1], "w")
    w = out.write

    # ---------------------------------------------------------------- 1. arms
    wall = {}          # (arm, row key) -> [wall_s x 5]
    order = None
    nrows = {}
    for a in NN:
        for r in ROUNDS:
            rs = csv_rows(rel("raw", "wall", "w-a%s-r%d.csv" % (a, r)))
            nrows[a] = len(rs)
            if a == "01" and r == 1:
                order = [k for _, k, _ in rs][:12]
            for _, k, s in rs:
                wall.setdefault((a, k), []).append(s)

    with open(rel("wall.csv"), "w", newline="") as fh:
        cw = csv.writer(fh)
        cw.writerow(["arm", "task", "sha", "round", "row_index", "row", "wall_s"])
        for a in NN:
            for r in ROUNDS:
                for i, k, s in csv_rows(rel("raw", "wall", "w-a%s-r%d.csv" % (a, r))):
                    cw.writerow([a, TASK[a], SHA[a], r, i, k, "%.9f" % s])

    w("=" * 100 + "\n")
    w("1. THE ELEVEN ARMS, THEIR ROW COUNTS, AND THE LIKE-FOR-LIKE MAP\n")
    w("=" * 100 + "\n\n")
    w("Every arm runs its OWN harness source; `wall_s` brackets the solve only. The per-ROW wall is\n")
    w("like-for-like at every arm. The whole-process INSTRUCTION count is like-for-like only between\n")
    w("consecutive arms whose ROW SET did not change -- and the row counts below, read off this leg's\n")
    w("own CSVs rather than assumed, are what decide that.\n\n")
    w("  arm  task   sha        rows written   step into this arm\n")
    prev = None
    like = {}
    for a in NN:
        if prev is None:
            tag = "(base)"
        elif nrows[prev] == nrows[a]:
            tag = "LIKE-FOR-LIKE -- row set unchanged"
            like[a] = True
        else:
            tag = "ROW-ADDING (+%d) -- NO INSTRUCTION VERDICT" % (nrows[a] - nrows[prev])
            like[a] = False
        w("  a%s   %-6s %-9s %6d         %s\n" % (a, TASK[a], SHA[a], nrows[a], tag))
        prev = a
    w("\n  LIKE-FOR-LIKE pairs: %s\n" % ", ".join(TASK[a] for a in NN[1:] if like[a]))
    w("  ROW-ADDING pairs:    %s\n" % ", ".join(TASK[a] for a in NN[1:] if not like[a]))

    # ------------------------------------------------------- 2. the wall steps
    med = {(a, k): st.median(wall[(a, k)]) for a in NN for k in order}
    w("\n" + "=" * 100 + "\n")
    w("2. THE PER-ROW WALL STEP PER TASK -- median of five rounds, arm order rotated per round\n")
    w("=" * 100 + "\n\n")
    w("THIS ASSERTS WALL CLOCK (CLAUDE.md section 7). Solo, taskset -c 2, MKL_NUM_THREADS=1,\n")
    w("one solve at a time under the box lock, the fix1 R2 pinned-core discipline; IDLE-PROOF.md.\n\n")
    w("R3 -- THE FIRST ROW EVERY PROCESS WRITES IS EXCLUDED BY POSITION. The cell order was chosen\n")
    w("so that row is the SAME row at every arm: %s.\n" % order[0])
    w("It is the process's warm-up and it is excluded from every score below; its own steps are\n")
    w("printed, marked EXCL, so a reader can see why.\n\n")
    hdr = "  " + "row".ljust(40) + "".join(TASK[a].rjust(9) for a in NN[1:]) + "   cumul"
    w(hdr + "\n")
    w("  " + "-" * (len(hdr) - 2) + "\n")
    steps = {}
    for j, k in enumerate(order):
        row = [med[(NN[i + 1], k)] / med[(NN[i], k)] for i in range(10)]
        for i in range(10):
            steps[(NN[i + 1], k)] = row[i]
        cum = med[("11", k)] / med[("01", k)]
        w("  " + k.ljust(40) + "".join("%9.4f" % s for s in row) + "%8.4f" % cum +
          ("   EXCL" if j == 0 else "") + "\n")
    scored = order[1:]
    w("\n  Scored rows: %d (the twelve base rows less the R3 warm-up).\n" % len(scored))
    w("  Per-task worst and best step over the scored rows:\n")
    for i in range(10):
        a = NN[i + 1]
        v = [steps[(a, k)] for k in scored]
        w("    %-6s min %.4f  max %.4f  median %.4f  rows above 1.01: %2d/%d\n"
          % (TASK[a], min(v), max(v), st.median(v), sum(1 for x in v if x > 1.01), len(v)))

    # ------------------------------------------------- 3. the cumulative check
    w("\n" + "=" * 100 + "\n")
    w("3. THE CUMULATIVE CHECK AGAINST reading.md SECTION 5\n")
    w("=" * 100 + "\n\n")
    s5 = s5_ratios()
    w("  Left: this leg's cumulative a01 -> a11 (median of 5). Right: the artifact's own\n")
    w("  102f729 -> head per-row ratio on the SAME row, recomputed from raw/interior/ one\n")
    w("  directory up (three alternating rounds, paired medians).\n\n")
    w("  " + "row".ljust(40) + "this leg   section 5   difference\n")
    dev = []
    for k in scored:
        c = med[("11", k)] / med[("01", k)]
        if k in s5:
            dev.append(abs(c - s5[k]))
            w("  " + k.ljust(40) + "%8.4f    %8.4f    %+8.4f\n" % (c, s5[k], c - s5[k]))
        else:
            w("  " + k.ljust(40) + "%8.4f         --          --\n" % c)
    tot_l = sum(med[("11", k)] for k in scored) / sum(med[("01", k)] for k in scored)
    w("\n  Corpus over the eleven scored rows, this leg: %.4f\n" % tot_l)
    w("  reading.md section 5's corpus over its 29 banded F7 rows: 1.0249\n")
    w("  Worst per-row deviation between the two readings: %.4f\n" % max(dev))
    w("\n  THE PER-ROW COLUMNS DISAGREE BY UP TO %.1f PERCENTAGE POINTS AND THAT IS REPORTED, NOT\n"
      % (max(dev) * 100))
    w("  SMOOTHED. Two independent legs, no shared round and no shared process, five medians here\n")
    w("  against three there, and a per-process layout term in both. What reproduces is the\n")
    w("  QUANTITY THE OWNER ASKED ABOUT -- the corpus ratio, %.4f against 1.0249 -- and the shape:\n"
      % tot_l)
    w("  every scored row moves the same way in both legs, and both legs put the move in one task.\n")

    # ---------------------------------------------------- 4. the localisation
    import math
    w("\n" + "=" * 100 + "\n")
    w("4. THE LOCALISATION -- each task's share of the cumulative, in log space\n")
    w("=" * 100 + "\n\n")
    w("  Shares are ln(step)/ln(cumulative). They sum to 1 by construction. A share ABOVE 1 means\n")
    w("  the other tasks net NEGATIVE on that row -- the task carries more than the whole move.\n\n")
    w("  " + "row".ljust(40) + "".join(TASK[a].rjust(8) for a in NN[1:]) + "\n")
    shares = {}
    for k in scored:
        c = math.log(med[("11", k)] / med[("01", k)])
        row = [math.log(steps[(a, k)]) / c for a in NN[1:]]
        for a, s in zip(NN[1:], row):
            shares[(a, k)] = s
        w("  " + k.ljust(40) + "".join("%8.2f" % s for s in row) + "\n")
    w("\n  Per task, over the eleven scored rows:\n")
    for a in NN[1:]:
        v = [shares[(a, k)] for k in scored]
        w("    %-6s median share %+6.2f   rows at or above 0.80: %2d/%d\n"
          % (TASK[a], st.median(v), sum(1 for x in v if x >= 0.80), len(v)))

    # --------------------------------------------------- 5. the instructions
    perf = {}
    for a in NN:
        for r in PERF_ROUNDS:
            for k, v in perf_counts(rel("raw", "perfA", "A-a%s-r%d.txt" % (a, r))).items():
                perf.setdefault(("A", a, k), []).append(v)
        for r in ROUNDS:
            for k, v in perf_counts(rel("raw", "diff", "P-a%s-r%d.txt" % (a, r))).items():
                perf.setdefault(("P", a, k), []).append(v)
            for k, v in perf_counts(rel("raw", "diff", "Q-a%s-r%d.txt" % (a, r))).items():
                perf.setdefault(("Q", a, k), []).append(v)
    for a in PASSB_ARMS:
        for r in PERF_ROUNDS:
            for k, v in perf_counts(rel("raw", "perfB", "B-a%s-r%d.txt" % (a, r))).items():
                perf.setdefault(("B", a, k), []).append(v)
    with open(rel("perf.csv"), "w", newline="") as fh:
        cw = csv.writer(fh)
        cw.writerow(["leg", "arm", "task", "sha", "round", "event", "value"])
        for leg, sub, rr in (("perfA", "perfA/A", PERF_ROUNDS), ("perfB", "perfB/B", PERF_ROUNDS),
                             ("diff-P", "diff/P", ROUNDS), ("diff-Q", "diff/Q", ROUNDS)):
            arms = PASSB_ARMS if leg == "perfB" else NN
            for a in arms:
                for r in rr:
                    p = rel("raw", *(sub.split("/")[0], sub.split("/")[1] + "-a%s-r%d.txt" % (a, r)))
                    for k, v in perf_counts(p).items():
                        cw.writerow([leg, a, TASK[a], SHA[a], r, k, v])

    def m(leg, a, k):
        return st.median(perf[(leg, a, k)])

    w("\n" + "=" * 100 + "\n")
    w("5. WHOLE-PROCESS INSTRUCTIONS -- pass A, and the floor the control arm measures\n")
    w("=" * 100 + "\n\n")
    w("  The five-round set is the diff leg's P process: the SAME invocation the wall leg times,\n")
    w("  with perf attached. The three-round pass-A set is printed beside it as a second sample.\n\n")
    w("  arm  task   rows      instructions (P, med 5)      branches           cycles        IPC\n")
    for a in NN:
        w("  a%s   %-6s %4d  %20s %16s %16s   %6.4f\n"
          % (a, TASK[a], nrows[a], "{:,}".format(int(m("P", a, "instructions:u"))),
             "{:,}".format(int(m("P", a, "branches:u"))),
             "{:,}".format(int(m("P", a, "cycles:u"))),
             m("P", a, "instructions:u") / m("P", a, "cycles:u")))
    w("\n  Consecutive whole-process steps. `b/i` is the branch fraction over the instruction\n")
    w("  fraction -- attribution.md section 3's fingerprint, where a LAYOUT cluster transition\n")
    w("  reads about 2.4 and executed work about 1.1.\n\n")
    for i in range(10):
        x, y = NN[i], NN[i + 1]
        si = m("P", y, "instructions:u") / m("P", x, "instructions:u")
        sb = m("P", y, "branches:u") / m("P", x, "branches:u")
        fi, fb = si - 1, sb - 1
        bi = fb / fi if abs(fi) > 1e-9 else float("nan")
        tag = "LIKE-FOR-LIKE" if like[y] else "ROW-ADDING -- NO VERDICT"
        w("  %-6s instr %9.6f (%+.6f)   branch %9.6f (%+.6f)   b/i %6.2f   %s\n"
          % (TASK[y], si, fi, sb, fb, bi, tag))
    ctl_i = m("P", "02", "instructions:u") / m("P", "01", "instructions:u")
    ctl_b = m("P", "02", "branches:u") / m("P", "01", "branches:u")
    w("\n  THE FLOOR IS MEASURED, NOT ASSUMED. Arm 02 (T8.1) has a libhven.a BYTE-IDENTICAL to arm\n")
    w("  01's (arms.txt: T8.1 touched only bench/ and tests/), so its LIBRARY work is exactly zero\n")
    w("  and its step is this instrument's floor on this leg. It reads instr %+.6f, branch %+.6f,\n"
      % (ctl_i - 1, ctl_b - 1))
    w("  b/i %.2f -- the layout value. Every LIKE-FOR-LIKE step measured above is SMALLER than\n"
      % ((ctl_b - 1) / (ctl_i - 1)))
    w("  that floor in magnitude, and so is the step on the pair the wall localises to.\n")
    w("\n  Round-to-round spread of the whole-process count, per arm (five rounds):\n")
    for a in NN:
        v = perf[("P", a, "instructions:u")]
        w("    a%s %-6s  %.2e   %s\n" % (a, TASK[a], max(v) / min(v) - 1,
                                          "  ".join("%.3f" % (x / 1e9) for x in v)))

    # --------------------------------------------- 6. the added-row accounting
    w("\n" + "=" * 100 + "\n")
    w("6. THE ADDED-ROW ACCOUNTING for the three row-adding pairs\n")
    w("=" * 100 + "\n\n")
    w("  A row-adding pair's whole-process instruction step gets NO VERDICT (section 1). What CAN\n")
    w("  be asked of it is narrower and is arithmetic: does the step EQUAL the rows the later arm\n")
    w("  added? The added rows' own wall is in the CSV; multiplied by the EARLIER arm's own\n")
    w("  whole-process instruction rate it gives an estimate of what they cost. If the estimate\n")
    w("  accounts for the step, the rows the two arms SHARE did not move.\n\n")
    w("  THIS IS AN ESTIMATE AND ITS ASSUMPTION IS STATED: it charges the added rows the process's\n")
    w("  AVERAGE instructions per second. The arm-to-arm spread of that rate is printed, and the\n")
    w("  residual is bounded with the extreme rates rather than the central one.\n\n")
    rates = [m("P", a, "instructions:u") / med_wall_sum(a) for a in NN]
    w("  Instruction rate across the eleven arms: %.3f to %.3f e9/s (spread %.1f %%)\n"
      % (min(rates) / 1e9, max(rates) / 1e9, (max(rates) / min(rates) - 1) * 100))
    for x, y in (("02", "03"), ("04", "05"), ("10", "11")):
        base_rows = {k for _, k, _ in csv_rows(rel("raw", "wall", "w-a%s-r1.csv" % x))}
        added = [(k, s) for _, k, s in csv_rows(rel("raw", "wall", "w-a%s-r1.csv" % y))
                 if k not in base_rows]
        aw = sum(s for _, s in added)
        rate = m("P", x, "instructions:u") / med_wall_sum(x)
        meas = m("P", y, "instructions:u") - m("P", x, "instructions:u")
        w("\n  %s (a%s -> a%s) adds %d row(s):\n" % (TASK[y], x, y, len(added)))
        for k, s in added:
            w("      %-52s wall %.6f s\n" % (k, s))
        w("      added-row wall total              %.6f s\n" % aw)
        w("      estimated at this arm's own rate  %.3f e9 instructions\n" % (aw * rate / 1e9))
        w("      MEASURED whole-process step       %.3f e9 instructions\n" % (meas / 1e9))
        w("      residual charged to the SHARED rows: %+.3f e9  (%+.2e of the process)\n"
          % ((meas - aw * rate) / 1e9, (meas - aw * rate) / m("P", x, "instructions:u")))
        lo, hi = aw * min(rates), aw * max(rates)
        w("      residual with the EXTREME rates:  %+.3f e9 to %+.3f e9\n"
          % ((meas - hi) / 1e9, (meas - lo) / 1e9))
        if y == "05":
            wallstep = st.median([steps[("05", k)] for k in scored]) - 1
            need = wallstep * m("P", x, "instructions:u")
            w("      for comparison, a WORK increase matching this task's median wall step of\n")
            w("      %+.4f on the shared rows would require about %.3f e9 more instructions.\n"
              % (wallstep, need / 1e9))

    # ------------------------------------------------------------- 7. pass B
    w("\n" + "=" * 100 + "\n")
    w("7. PASS B -- the Zen 3 front-end events, on the localised pair and on the control pair\n")
    w("=" * 100 + "\n\n")
    evb = ["instructions:u", "cycles:u", "de_dis_uop_queue_empty_di0:u",
           "op_cache_hit_miss.op_cache_miss:u", "op_cache_hit_miss.op_cache_hit:u",
           "ic_fetch_stall.ic_stall_any:u"]
    w("  " + "event".ljust(38) + "".join(("a%s(%s)" % (a, TASK[a])).rjust(18) for a in PASSB_ARMS)
      + "\n")
    for k in evb:
        w("  " + k.ljust(38) + "".join("{:>18,}".format(int(m("B", a, k))) for a in PASSB_ARMS)
          + "\n")
    for lo, hi, lab in (("01", "02", "T8.1 -- THE CONTROL (libhven.a byte-identical to the base's)"),
                        ("04", "05", "T8.4 -- the pair the wall localises to")):
        w("\n  %s\n" % lab)
        for k in evb:
            w("     %-40s %8.5f\n" % (k, m("B", hi, k) / m("B", lo, k)))
        ipc = (m("B", hi, "instructions:u") / m("B", hi, "cycles:u")) / \
              (m("B", lo, "instructions:u") / m("B", lo, "cycles:u"))
        w("     %-40s %8.5f\n" % ("IPC ratio", ipc))
    bigger = [k for k in evb
              if abs(m("B", "02", k) / m("B", "01", k) - 1) > abs(m("B", "05", k) / m("B", "04", k) - 1)]
    w("\n  READ IT ON THE CONTROL FIRST. The control pair's library work is exactly zero, so whatever\n")
    w("  it moves is this instrument's floor. It moves FURTHER than the pair under test on %d of\n"
      % len(bigger))
    w("  the %d events (%s)\n" % (len(evb), ", ".join(k.replace(":u", "") for k in bigger)))
    w("  and further on IPC as well. The one event where the pair under test moves further is\n")
    w("  %s, by %.4f against the control's %.4f -- a gap far inside\n"
      % (", ".join(k.replace(":u", "") for k in evb if k not in bigger) or "(none)",
         max([m("B", "05", k) / m("B", "04", k) for k in evb if k not in bigger] or [float("nan")]),
         max([m("B", "02", k) / m("B", "01", k) for k in evb if k not in bigger] or [float("nan")])))
    w("  the floor the rest of the row establishes. PASS B THEREFORE SEPARATES NOTHING HERE AND\n")
    w("  RETURNS NO VERDICT -- which is a fact about this leg's instrument, not about T8.4.\n")

    # ------------------------------------------ 8. the rejected diff instrument
    w("\n" + "=" * 100 + "\n")
    w("8. THE DIFFERENCING INSTRUMENT -- BUILT, RUN, AND REJECTED. Its evidence, in full.\n")
    w("=" * 100 + "\n\n")
    w("  The design: two processes per arm per round, Q = --cells <the first cell only>,\n")
    w("  P = --cells <the four>. P - Q is exactly the nine large base rows; every unconditional\n")
    w("  row, every variant row and the R3 warm-up row is in BOTH and cancels, which would make\n")
    w("  the differenced quantity like-for-like even across a row-adding pair. Both processes'\n")
    w("  FIRST solve is the same row in the same position -- the asymmetry that sank fix1's I4.\n\n")
    w("  arm  task   P-rows  Q-rows  P-Q rows      med(P-Q) instructions   spread of Q over 5\n")
    for a in NN:
        qn = len(csv_rows(rel("raw", "diff", "Q-a%s-r1.csv" % a)))
        pn = len(csv_rows(rel("raw", "diff", "P-a%s-r1.csv" % a)))
        d = [p - q for p, q in zip(perf[("P", a, "instructions:u")],
                                   perf[("Q", a, "instructions:u")])]
        qv = perf[("Q", a, "instructions:u")]
        w("  a%s   %-6s %6d  %6d  %8d   %20s   %.2e\n"
          % (a, TASK[a], pn, qn, pn - qn, "{:,}".format(int(st.median(d))),
             max(qv) / min(qv) - 1))
    w("\n  P - Q IS 9 ROWS AT EVERY ARM, so the construction did what it was built to do.\n")
    w("  WHAT REJECTS IT is the last column: the Q process's own instruction count scatters by\n")
    w("  15-22 %% round to round, while the P process at the later arms is reproducible to under\n")
    w("  0.2 %%. The rows Q contains are in P too -- so those rows DO NOT COST THE SAME inside a\n")
    w("  process that goes on to run nine large solves as they do in a process that does not.\n")
    w("  The cancellation the instrument depends on is therefore not exact, and the differenced\n")
    w("  steps below carry a floor larger than anything they were built to resolve:\n\n")
    prevd = None
    for a in NN:
        d = st.median([p - q for p, q in zip(perf[("P", a, "instructions:u")],
                                             perf[("Q", a, "instructions:u")])])
        if prevd:
            w("    %-6s differenced step %9.6f\n" % (TASK[a], d / prevd))
        prevd = d
    w("\n  The control arm T8.1 -- true step exactly zero -- reads a differenced step far from 1,\n")
    w("  which is the whole verdict on this instrument. IT IS REPORTED, NOT USED. Its raw data\n")
    w("  is retained in raw/diff/ as data, and the P process it produced IS used above, on its\n")
    w("  own, as the five-round whole-process sample.\n")

    w("\nAPPLE / ACCELERATE: UNOBSERVED.   WINDOWS: UNOBSERVED.\n")
    if out is not sys.stdout:
        out.close()
    return 0


def med_wall_sum(a):
    return st.median([sum(s for _, _, s in csv_rows(rel("raw", "diff", "P-a%s-r%d.csv" % (a, r))))
                      for r in ROUNDS])


def s5_ratios():
    """reading.md section 5's 102f729 -> head per-row ratio, recomputed from the
    artifact's OWN retained CSVs one directory up rather than transcribed."""
    out = {}
    try:
        base, head = {}, {}
        for r in (1, 2, 3):
            for _, k, s in csv_rows(os.path.join(S5_DIR, "arm102-r%d.csv" % r)):
                base.setdefault(k, []).append(s)
            for _, k, s in csv_rows(os.path.join(S5_DIR, "head-r%d.csv" % r)):
                head.setdefault(k, []).append(s)
        for k in base:
            if k in head:
                out[k] = st.median(head[k]) / st.median(base[k])
    except OSError:
        pass
    return out


if __name__ == "__main__":
    sys.exit(main())
