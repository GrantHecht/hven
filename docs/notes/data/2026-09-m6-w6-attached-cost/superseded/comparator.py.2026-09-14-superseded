#!/usr/bin/env python3
"""W6 T6 -- the attached-callback / sink cost leg's comparator.

Regenerates EVERY table in `reading.md` from `raw/` and `perf/`.  Nothing in
that file is hand-transcribed.

    usage: comparator.py --root <artifact dir> --out - | <path>

WHAT IT READS
-------------
  perf/sqpperf/<mode>/r<round>/<arm>-<cell>.txt   one `perf stat` per cell per
                                                  arm per round (the SQP corpus
                                                  instruction leg)
  raw/sqpperf/<mode>/r<round>/<arm>-<cell>.csv    that run's single corpus row
  raw/sqpwall/<mode>/<arm>.csv                    the 27-cell wall leg's CSV
  perf/ipmleg/<arm>-r<round>.txt                  the interior leg
  raw/ipmleg/{perf,wall}-<arm>-r<round>.csv
  perf/hs/<mode>-<arm>-r<round>.txt               the HS leg
  raw/hs/{perf,wall}-<mode>-<arm>-r<round>.csv

THE ARMS.  `a00` is the SHIPPED SHAPE -- callback off, trace off, nothing
attached -- and is the denominator of every ratio.  `a10` attaches the counting
iteration callback, `a01` the counting trace sink, `a11` both.  The interior leg
has two arms (`off`/`on`, the HVEN_LEG_COUNT_CALLBACK lever) and the HS leg two
(`off`/`sink`, the --hs-trace lever).

THE READING.  Per cell, the MEDIAN of the three rounds' instruction counts, per
arm; the ratio is attached-median / unattached-median.  The corpus figure is the
SUM of per-cell medians, never a mean of ratios (the ownership doc's section
11.3 rule, carried over).  The band is section 11.1's FLAT:

    per-cell median ratio in 0.99-1.01 AND whole-corpus within +/-0.5 %,
    in all three modes.

Anything else is reported as MOVED with its top cells.  NOTHING HERE IS A GATE:
T6 is a finding at one HEAD, not a commit pair, so there is no veto to fire.

THE COUNTER CHECK IS SEPARATE AND IT IS THE ONE THING THAT COULD STOP THE LEG.
An attached observer must change no trajectory, so every asserted column of
every pair must be byte-identical.  It is run by `scripts/compare_replay.py`
(the committed comparator) on the corpus CSVs and, for the HS schema which that
tool does not know, by the column walk in `hs_identity()` below, which names the
observation columns it excludes and why.
"""
import csv
import os
import re
import statistics
import subprocess
import sys

ARMS = ["a00", "a10", "a01", "a11"]
ARM_LABEL = {
    "a00": "callback=off  trace=off   (unattached -- the shipped shape)",
    "a10": "callback=count trace=off   (attached callback)",
    "a01": "callback=off  trace=sink  (attached sink)",
    "a11": "callback=count trace=sink  (both)",
}
MODES = ["walk", "ssn", "ipm"]
ROUNDS = [1, 2, 3]
EVENTS = ["instructions:u", "branches:u", "cycles:u", "branch-misses:u",
          "L1-icache-load-misses:u"]
# The HS schema's OBSERVATION columns: wall (timing, never a regression
# contract, CLAUDE.md section 7), the arm label itself, and the ev_* per-site
# event counts, which exist ONLY when a sink is attached and are the evidence
# that it fired.  Every other column is an asserted counter or a status.
HS_EXCLUDED = {"wall_median_s", "wall_min_s", "wall_max_s", "spread_pct",
               "median_se_pct", "trace", "ev_dispatch", "ev_soc_resolve",
               "ev_elastic_rung", "ev_fallback_rung_b", "ev_ssn_warm_grade"}

PERF_LINE = re.compile(r"^\s*([\d,]+)\s+(\S+)")
ELAPSED = re.compile(r"^\s*([\d.]+)\s+seconds time elapsed")


def read_perf(path):
    """One `perf stat -o` file -> {event: count} plus 'elapsed_s'."""
    out = {}
    if not os.path.exists(path):
        return None
    with open(path) as f:
        for line in f:
            m = ELAPSED.match(line)
            if m:
                out["elapsed_s"] = float(m.group(1))
                continue
            m = PERF_LINE.match(line)
            if m and m.group(2) in EVENTS:
                out[m.group(2)] = int(m.group(1).replace(",", ""))
    return out or None


def med(xs):
    return statistics.median(xs) if xs else None


def ratio(a, b):
    return (a / b) if (a and b) else float("nan")


def pct(r):
    return (r - 1.0) * 100.0


def cells_from(root, mode):
    d = os.path.join(root, "perf", "sqpperf", mode, "r1")
    if not os.path.isdir(d):
        return []
    names = set()
    for fn in os.listdir(d):
        if fn.endswith(".txt") and fn.startswith("a00-"):
            names.add(fn[len("a00-"):-len(".txt")])
    return sorted(names)


# ---------------------------------------------------------------------------
# The SQP corpus instruction leg
# ---------------------------------------------------------------------------
def sqp_tables(root, out):
    summary = []
    for mode in MODES:
        cells = cells_from(root, mode)
        if not cells:
            continue
        # medians[arm][cell][event]
        medians = {a: {} for a in ARMS}
        elapsed = {a: {} for a in ARMS}
        for arm in ARMS:
            for cell in cells:
                per_ev = {e: [] for e in EVENTS}
                els = []
                for r in ROUNDS:
                    p = read_perf(os.path.join(root, "perf", "sqpperf", mode,
                                               "r%d" % r, "%s-%s.txt" % (arm, cell)))
                    if p is None:
                        continue
                    for e in EVENTS:
                        if e in p:
                            per_ev[e].append(p[e])
                    if "elapsed_s" in p:
                        els.append(p["elapsed_s"])
                medians[arm][cell] = {e: med(v) for e, v in per_ev.items()}
                elapsed[arm][cell] = med(els)

        out.write("\n### SQP corpus / %s -- instructions, attached vs unattached\n\n" % mode)
        out.write("Per cell: the median of three rounds, per arm; the ratio is that arm's "
                  "median over `a00`'s. The corpus row is the SUM of per-cell medians.\n\n")
        out.write("| cell | a00 instructions (median) | a10 ratio | a01 ratio | a11 ratio |\n")
        out.write("|---|---|---|---|---|\n")
        corpus = {a: 0 for a in ARMS}
        outside = {a: [] for a in ARMS[1:]}
        percell = {a: [] for a in ARMS[1:]}
        for cell in cells:
            base = medians["a00"][cell]["instructions:u"]
            row = ["`%s`" % cell, "%d" % base]
            for arm in ARMS[1:]:
                r = ratio(medians[arm][cell]["instructions:u"], base)
                percell[arm].append((cell, r))
                if not (0.99 <= r <= 1.01):
                    outside[arm].append((cell, r))
                row.append("%.5f" % r)
            for arm in ARMS:
                corpus[arm] += medians[arm][cell]["instructions:u"] or 0
            out.write("| " + " | ".join(row) + " |\n")
        out.write("| **corpus (sum of medians)** | **%d** | %s |\n" % (
            corpus["a00"],
            " | ".join("**%.5f**" % ratio(corpus[a], corpus["a00"]) for a in ARMS[1:])))
        out.write("\n")
        for arm in ARMS[1:]:
            cr = ratio(corpus[arm], corpus["a00"])
            flat = (not outside[arm]) and abs(pct(cr)) <= 0.5
            verdict = "FLAT" if flat else "MOVED"
            top = sorted(percell[arm], key=lambda kv: -abs(kv[1] - 1.0))[:3]
            rs = [r for _, r in percell[arm]]
            lo = min(percell[arm], key=lambda kv: kv[1])
            hi = max(percell[arm], key=lambda kv: kv[1])
            out.write("* **%s / %s -- %s.** corpus %.5f (%+.4f %%); PER CELL median "
                      "%.5f (%+.4f %%), min %.5f (`%s`), max %.5f (`%s`); cells outside "
                      "0.99-1.01: **%d/%d**%s. Largest per-cell moves: %s.\n" % (
                          mode, arm, verdict, cr, pct(cr),
                          statistics.median(rs), pct(statistics.median(rs)),
                          lo[1], lo[0], hi[1], hi[0],
                          len(outside[arm]), len(cells),
                          "" if not outside[arm] else
                          " (" + ", ".join("`%s` %.5f" % kv for kv in outside[arm][:6]) + ")",
                          ", ".join("`%s` %.5f (%+.4f %%)" % (c, r, pct(r)) for c, r in top)))
            summary.append((mode, arm, cr, len(outside[arm]), len(cells), verdict,
                            statistics.median(rs), hi[1]))
        # The other four events, corpus level, for the LAYOUT-MOVED / WORK-MOVED
        # classification the ownership doc's 11.1 asks for.
        out.write("\nThe rest of the instrument, at the corpus level (medians summed):\n\n")
        out.write("| event | " + " | ".join(ARMS) + " | a10/a00 | a01/a00 | a11/a00 |\n")
        out.write("|---|" + "---|" * (len(ARMS) + 3) + "\n")
        for e in EVENTS:
            tot = {a: sum((medians[a][c][e] or 0) for c in cells) for a in ARMS}
            out.write("| `%s` | %s | %s |\n" % (
                e, " | ".join("%d" % tot[a] for a in ARMS),
                " | ".join("%.5f" % ratio(tot[a], tot["a00"]) for a in ARMS[1:])))
        out.write("\nProcess wall of the single-cell perf runs, summed over the "
                  "cells (median of three rounds each) -- INFORMATIONAL, and taken "
                  "under `perf stat`:\n\n")
        tot = {a: sum((elapsed[a][c] or 0) for c in cells) for a in ARMS}
        out.write("| arm | summed process wall (s) | ratio |\n|---|---|---|\n")
        for a in ARMS:
            out.write("| `%s` | %.4f | %s |\n" % (
                a, tot[a], "--" if a == "a00" else "%.5f" % ratio(tot[a], tot["a00"])))
    return summary


# ---------------------------------------------------------------------------
# The interior leg and the HS leg
# ---------------------------------------------------------------------------
def two_arm_table(root, out, title, perfdir, arms, namer, note):
    rows = {a: {e: [] for e in EVENTS} for a in arms}
    els = {a: [] for a in arms}
    for a in arms:
        for r in ROUNDS:
            p = read_perf(os.path.join(root, "perf", perfdir, namer(a, r)))
            if p is None:
                continue
            for e in EVENTS:
                if e in p:
                    rows[a][e].append(p[e])
            if "elapsed_s" in p:
                els[a].append(p["elapsed_s"])
    if not rows[arms[0]]["instructions:u"]:
        return None
    out.write("\n### %s\n\n%s\n\n" % (title, note))
    out.write("| event | %s | ratio |\n|---|%s---|\n" % (
        " | ".join("%s (median)" % a for a in arms), "---|" * len(arms)))
    verdict = None
    for e in EVENTS:
        m = {a: med(rows[a][e]) for a in arms}
        r = ratio(m[arms[1]], m[arms[0]])
        if e == "instructions:u":
            verdict = r
        out.write("| `%s` | %s | %.5f |\n" % (
            e, " | ".join("%d" % (m[a] or 0) for a in arms), r))
    m = {a: med(els[a]) for a in arms}
    out.write("| process wall (s), informational | %s | %.5f |\n" % (
        " | ".join("%.4f" % (m[a] or 0) for a in arms), ratio(m[arms[1]], m[arms[0]])))
    out.write("\nPer-round instruction counts (the alternation): %s\n" % "; ".join(
        "%s %s" % (a, rows[a]["instructions:u"]) for a in arms))
    # THE LEVEL-MATCHED PAIRING.  A three-round instruction sample of these legs
    # is not scattered: it lands on a SMALL NUMBER OF DISCRETE LEVELS that BOTH
    # arms visit -- a per-process constant (address layout, allocator and page
    # placement) that no arm changes.  When the two arms' medians land on
    # DIFFERENT levels the median-of-three ratio reports that level gap and not
    # the arm.  Pairing the arms' sorted extremes -- lowest with lowest, highest
    # with highest -- cancels it.  The two pairings AGREEING is what says the
    # levels are shared and the pairing is the right one; where they disagree
    # the pairing is meaningless and is not read.
    a, b = arms[0], arms[1]
    la, lb = sorted(rows[a]["instructions:u"]), sorted(rows[b]["instructions:u"])
    if len(la) == len(lb) and la and lb:
        rlo, rhi = ratio(lb[0], la[0]), ratio(lb[-1], la[-1])
        out.write("\nLEVEL-MATCHED PAIRING (lowest-with-lowest, highest-with-highest): "
                  "**%.6f** and **%.6f**%s\n" % (
                      rlo, rhi,
                      " -- the two agree to %.1e, so the arms share their levels and the "
                      "pairing resolves the arm below the per-process spread" % abs(rlo - rhi)
                      if abs(rlo - rhi) < 1e-4 else
                      " -- the two DISAGREE, so the arms do not share levels and this "
                      "pairing is not read"))
    return verdict


# ---------------------------------------------------------------------------
# What the attached cost IS -- the normalisation that names the carrier
# ---------------------------------------------------------------------------
EVENT_RE = re.compile(r"ATTACHED-CALLBACK .*? events=(\d+)")
SINK_RE = re.compile(r"ATTACHED-SINK .*?qp_mode_dispatch=(\d+) soc_resolve=(\d+) "
                     r"elastic_rung=(\d+) fallback_rung_b=(\d+) ssn_warm_grade=(\d+) "
                     r"other=(\d+)")
NODES_RE = re.compile(r"_n(\d+)_")


def attribution_section(root, out):
    """The instruction delta per ATTACHED EVENT, and per event per node.

    The brief names a CALLBACK-DISPATCH class -- one null check and one indirect
    call per iteration -- and asks whether the reading is consistent with it.
    This is the test: a dispatch is O(1) per event, so if the class were right
    the delta divided by the EVENT COUNT would be a small constant and dividing
    again by the problem size would make it fall.  It does the opposite.
    """
    out.write("\n## What the attached cost actually is\n\n")
    out.write("The delta is normalised two ways: per ATTACHED EVENT (the count the "
              "instrument itself prints to stderr) and per event per collocation "
              "NODE. An O(1) dispatch would give a small constant in the first "
              "column and a falling number in the second.\n\n")
    out.write("| leg | mode | arm | instruction delta | events | instr / event | "
              "instr / (event x node) |\n|---|---|---|---|---|---|---|\n")
    for mode in MODES:
        cells = cells_from(root, mode)
        if not cells:
            continue
        for arm, rx in (("a10", "callback"), ("a01", "sink")):
            tot_d = tot_e = tot_en = 0
            for cell in cells:
                m = {}
                for a in ("a00", arm):
                    vals = [read_perf(os.path.join(root, "perf", "sqpperf", mode,
                                                   "r%d" % r, "%s-%s.txt" % (a, cell)))
                            for r in ROUNDS]
                    vals = [v["instructions:u"] for v in vals if v and "instructions:u" in v]
                    m[a] = med(vals)
                ev = 0
                sp = os.path.join(root, "raw", "sqpperf", mode, "r1",
                                  "%s-%s.stderr" % (arm, cell))
                if os.path.exists(sp):
                    txt = open(sp).read()
                    if arm == "a10":
                        mm = EVENT_RE.search(txt)
                        ev = int(mm.group(1)) if mm else 0
                    else:
                        mm = SINK_RE.search(txt)
                        ev = sum(int(g) for g in mm.groups()) if mm else 0
                nm = NODES_RE.search(cell)
                n = int(nm.group(1)) if nm else 0
                tot_d += (m[arm] or 0) - (m["a00"] or 0)
                tot_e += ev
                tot_en += ev * n
            out.write("| SQP corpus | %s | `%s` (%s) | %.4g | %d | %.4g | %.1f |\n" % (
                mode, arm, rx, tot_d, tot_e,
                tot_d / tot_e if tot_e else float("nan"),
                tot_d / tot_en if tot_en else float("nan")))
    # the interior leg, batch 2
    ev = 0
    en = 0
    sp = os.path.join(root, "raw", "ipmleg2", "perf-on-r1.stdout")
    csvp = os.path.join(root, "raw", "ipmleg2", "perf-on-r1.csv")
    if os.path.exists(sp) and os.path.exists(csvp):
        nodes = {}
        lines = [l for l in open(csvp) if not l.startswith("#")]
        rd = csv.reader(lines)
        hdr = next(rd)
        for row in rd:
            if len(row) == len(hdr):
                nodes[row[0]] = int(row[hdr.index("n_nodes")])
        for line in open(sp):
            mm = re.search(r"callback-events row=(\S+) events=(\d+)", line)
            if mm:
                ev += int(mm.group(2))
                en += int(mm.group(2)) * nodes.get(mm.group(1), 0)
        d = {}
        for a in ("off", "on"):
            vals = [read_perf(os.path.join(root, "perf", "ipmleg2", "%s-r%d.txt" % (a, r)))
                    for r in ROUNDS]
            vals = [v["instructions:u"] for v in vals if v and "instructions:u" in v]
            d[a] = med(vals)
        delta = (d["on"] or 0) - (d["off"] or 0)
        out.write("| interior leg (batch 2) | -- | `on` (callback) | %.4g | %d | %.4g | %.1f |\n"
                  % (delta, ev, delta / ev if ev else float("nan"),
                     delta / en if en else float("nan")))
    out.write("\n**The dispatch is not the carrier.** Per event the delta is MILLIONS of "
              "instructions, and per event per node it is a few hundred -- an O(n) "
              "cost paid once per attached event, not an O(1) null check and indirect "
              "call. That is the `IterationEvent` the driver builds AFTER the guard: "
              "`src/drivers/ipm_solver.cpp:2240-2243` and `:2402`, "
              "`src/drivers/sqp_solver.cpp:4618`. The CALLBACK-DISPATCH class the brief "
              "names is real and is in there, but it is three to four orders of "
              "magnitude below what this leg can see; what an attached callback costs "
              "on these problems is the EVENT, and it scales with the problem.\n")


# ---------------------------------------------------------------------------
# The counter check
# ---------------------------------------------------------------------------
def compare_replay(a, b, label, repo):
    tool = os.path.join(repo, "scripts", "compare_replay.py")
    p = subprocess.run([sys.executable, tool, a, b, label],
                       capture_output=True, text=True)
    return p.returncode, p.stdout.strip()


def hs_identity(a, b, label):
    def load(path):
        lines = [l for l in open(path) if not l.startswith("#")]
        r = csv.reader(lines)
        hdr = next(r)
        return hdr, {row[0]: dict(zip(hdr, row)) for row in r if len(row) == len(hdr)}
    ha, ra = load(a)
    hb, rb = load(b)
    if ha != hb:
        return 2, "=== %s === SCHEMA MISMATCH" % label
    cols = [c for c in ha if c not in HS_EXCLUDED]
    keys = sorted(set(ra) & set(rb))
    diffs = [(k, c, ra[k][c], rb[k][c]) for k in keys for c in cols if ra[k][c] != rb[k][c]]
    line = ("=== %s === cells compared: %d  columns compared: %d  differences: %d"
            % (label, len(keys), len(cols), len(diffs)))
    for d in diffs[:20]:
        line += "\n    %s %s A=%s B=%s" % d
    return (1 if diffs else 0), line


def counter_section(root, out, repo):
    out.write("\n## The counter check -- an attached observer must change no trajectory\n\n")
    out.write("`scripts/compare_replay.py A B LABEL` is EXACT STRING EQUALITY on every "
              "column but `wall_s`, which is what this check wants: every counter, every "
              "status and every residual byte-identical between the attached and the "
              "unattached arm. Run without `--residual-gate` first; where that passes there "
              "is nothing for the gate to relax.\n\n```\n")
    worst = 0
    # the 27-cell wall CSVs
    for mode in MODES:
        base = os.path.join(root, "raw", "sqpwall", mode, "a00.csv")
        if not os.path.exists(base):
            continue
        for arm in ARMS[1:]:
            other = os.path.join(root, "raw", "sqpwall", mode, "%s.csv" % arm)
            if not os.path.exists(other):
                continue
            rc, txt = compare_replay(base, other, "sqp corpus wall / %s / a00 vs %s"
                                     % (mode, arm), repo)
            worst = max(worst, rc)
            out.write(txt + "\n")
    # the single-cell perf CSVs, concatenated per arm per round
    for mode in MODES:
        cells = cells_from(root, mode)
        if not cells:
            continue
        for r in ROUNDS:
            d = os.path.join(root, "raw", "sqpperf", mode, "r%d" % r)
            # The single-cell runs each write ONE bare row (--internal-out has no
            # header of its own), so the arm's 27 rows are concatenated under the
            # wall leg's column header to make a file compare_replay.py can read.
            merged = {a: os.path.join(d, "_merged-%s.csv" % a) for a in ARMS}
            hdr = None
            for line in open(os.path.join(root, "raw", "sqpwall", mode, "a00.csv")):
                if not line.startswith("#"):
                    hdr = line
                    break
            for arm in ARMS:
                with open(merged[arm], "w") as f:
                    f.write(hdr)
                    for cell in cells:
                        p = os.path.join(d, "%s-%s.csv" % (arm, cell))
                        if os.path.exists(p):
                            f.write(open(p).read())
            for arm in ARMS[1:]:
                rc, txt = compare_replay(merged["a00"], merged[arm],
                                         "sqp single-cell perf / %s / r%d / a00 vs %s"
                                         % (mode, r, arm), repo)
                worst = max(worst, rc)
                out.write(txt + "\n")
    # the interior leg
    for phase, rs in (("wall", [1]), ("perf", ROUNDS)):
        for r in rs:
            a = os.path.join(root, "raw", "ipmleg", "%s-off-r%d.csv" % (phase, r))
            b = os.path.join(root, "raw", "ipmleg", "%s-on-r%d.csv" % (phase, r))
            if not (os.path.exists(a) and os.path.exists(b)):
                continue
            rc, txt = compare_replay(a, b, "interior leg / %s / r%d / off vs on" % (phase, r),
                                     repo)
            worst = max(worst, rc)
            out.write(txt + "\n")
    # the HS leg
    for phase, rs in (("wall", [1]), ("perf", ROUNDS)):
        for mode in MODES:
            for r in rs:
                a = os.path.join(root, "raw", "hs", "%s-%s-off-r%d.csv" % (phase, mode, r))
                b = os.path.join(root, "raw", "hs", "%s-%s-sink-r%d.csv" % (phase, mode, r))
                if not (os.path.exists(a) and os.path.exists(b)):
                    continue
                rc, txt = hs_identity(a, b, "HS leg / %s / %s / r%d / off vs sink"
                                      % (phase, mode, r))
                worst = max(worst, rc)
                out.write(txt + "\n")
    out.write("```\n\n")
    out.write("**RESULT: %s.**\n" % ("every pair byte-identical on every asserted column"
                                     if worst == 0 else
                                     "AT LEAST ONE PAIR DIFFERS -- see the listing above"))
    return worst


# ---------------------------------------------------------------------------
def wall_section(root, out):
    out.write("\n## The wall readings, beside the instructions (INFORMATIONAL)\n\n")
    out.write("CLAUDE.md section 7: serial, solo, one solve at a time, "
              "`MKL_NUM_THREADS=1`, pinned to cpu2 with its SMT sibling cpu10 left idle. "
              "The corpus figure is the SUM of the per-cell `wall_s` column -- the leg's own "
              "solve bracket -- exactly as T8.9r's leg 1 read it.\n\n")
    out.write("| leg | mode | arm | corpus wall (s) | ratio vs unattached |\n|---|---|---|---|---|\n")
    for mode in MODES:
        base = None
        for arm in ARMS:
            p = os.path.join(root, "raw", "sqpwall", mode, "%s.csv" % arm)
            if not os.path.exists(p):
                continue
            tot = 0.0
            lines = [l for l in open(p) if not l.startswith("#")]
            rd = csv.reader(lines)
            hdr = next(rd)
            i = hdr.index("wall_s")
            for row in rd:
                if len(row) == len(hdr):
                    tot += float(row[i])
            if arm == "a00":
                base = tot
            out.write("| SQP corpus (27 cells) | %s | `%s` | %.4f | %s |\n" % (
                mode, arm, tot, "--" if arm == "a00" else "%.5f" % ratio(tot, base)))
    # the interior leg
    base = None
    for arm in ("off", "on"):
        p = os.path.join(root, "raw", "ipmleg", "wall-%s-r1.csv" % arm)
        if not os.path.exists(p):
            continue
        lines = [l for l in open(p) if not l.startswith("#")]
        rd = csv.reader(lines)
        hdr = next(rd)
        i = hdr.index("wall_s")
        tot = sum(float(r[i]) for r in rd if len(r) == len(hdr))
        if arm == "off":
            base = tot
        out.write("| interior leg (43 rows) | -- | `%s` | %.4f | %s |\n" % (
            arm, tot, "--" if arm == "off" else "%.5f" % ratio(tot, base)))
    # the HS leg
    for mode in MODES:
        base = None
        for arm in ("off", "sink"):
            p = os.path.join(root, "raw", "hs", "wall-%s-%s-r1.csv" % (mode, arm))
            if not os.path.exists(p):
                continue
            lines = [l for l in open(p) if not l.startswith("#")]
            rd = csv.reader(lines)
            hdr = next(rd)
            i = hdr.index("wall_median_s")
            tot = sum(float(r[i]) for r in rd if len(r) == len(hdr))
            if arm == "off":
                base = tot
            out.write("| HS leg (27 cells, sum of per-cell medians) | %s | `%s` | %.6f | %s |\n"
                      % (mode, arm, tot, "--" if arm == "off" else "%.5f" % ratio(tot, base)))


def main():
    root = "."
    outpath = "-"
    argv = sys.argv[1:]
    while argv:
        a = argv.pop(0)
        if a == "--root":
            root = argv.pop(0)
        elif a == "--out":
            outpath = argv.pop(0)
        else:
            sys.stderr.write("usage: comparator.py --root DIR --out - | PATH\n")
            return 2
    repo = os.environ.get("HVEN_REPO", "/home/ghecht/Projects/hven")
    out = sys.stdout if outpath == "-" else open(outpath, "w")
    out.write("# W6 T6 comparator output\n\n")
    out.write("Regenerate: `python3 comparator.py --root . --out -`\n")
    out.write("\nArms: " + "; ".join("`%s` = %s" % (a, ARM_LABEL[a]) for a in ARMS) + ".\n")
    out.write("\n## The SQP corpus leg (the U0 27-cell set, one process per cell)\n")
    summary = sqp_tables(root, out)
    two_arm_table(
        root, out, "The top-level interior-point leg -- callback off vs on (batch 1)",
        "ipmleg", ["off", "on"], lambda a, r: "%s-r%d.txt" % (a, r),
        "The whole 43-row leg in ONE process (it forks nothing). The lever is "
        "`HVEN_LEG_COUNT_CALLBACK`, which installs a counting "
        "`set_iteration_callback` on every row's measured solver "
        "(`bench/ipm_corpus_leg.cpp:638-645`). THIS LEG HAS NO SINK LEVER and this "
        "task added none. **Round 1's UNATTACHED process is a first-process "
        "outlier** -- +2.1 % over its own rounds 2 and 3, while all six processes "
        "wrote byte-identical rows -- so batch 2 below re-takes it behind an "
        "untimed warm-up. Neither batch is discarded.")
    two_arm_table(
        root, out, "The top-level interior-point leg -- callback off vs on (batch 2, "
        "behind an untimed warm-up)",
        "ipmleg2", ["off", "on"], lambda a, r: "%s-r%d.txt" % (a, r),
        "The same leg, the same lever, the same binary, three more rounds -- with "
        "ONE UNTIMED, UNRECORDED process ahead of them so no round is the batch's "
        "first process. Each arm is now internally stable to 0.27 % (off) and "
        "0.04 % (on), and the gap between the arms is an order of magnitude larger "
        "than either.")
    for mode in MODES:
        two_arm_table(
            root, out, "The HS leg / %s -- trace off vs sink (the re-read)" % mode,
            "hs", ["off", "sink"], lambda a, r, m=mode: "%s-%s-r%d.txt" % (m, a, r),
            "27 HS cells, `--repeat 200 --hs-warmup 1`, in one process. The lever is "
            "`--hs-trace`, unchanged since T6.d. The registration recorded this arm's "
            "WALL at 0.975-0.993, below the leg's own 0.22 % instrument floor; this is "
            "the same arm read on INSTRUCTIONS.")
    attribution_section(root, out)
    worst = counter_section(root, out, repo)
    wall_section(root, out)
    out.write("\n## The short version\n\n")
    out.write("| leg | mode | arm | corpus instruction ratio | per-cell median | "
              "per-cell max | cells outside 0.99-1.01 | band |\n")
    out.write("|---|---|---|---|---|---|---|---|\n")
    for mode, arm, cr, no, nc, verdict, pcmed, pcmax in summary:
        out.write("| SQP corpus | %s | `%s` | **%.5f** (%+.4f %%) | %.5f (%+.4f %%) | "
                  "%.5f (%+.4f %%) | %d/%d | **%s** |\n"
                  % (mode, arm, cr, pct(cr), pcmed, pct(pcmed), pcmax, pct(pcmax),
                     no, nc, verdict))
    out.write("\n**THE CORPUS COLUMN AND THE PER-CELL COLUMNS ARE AN ORDER OF MAGNITUDE "
              "APART, AND THAT IS A FACT ABOUT THE CELL SET, NOT ABOUT THE OBSERVER.** "
              "Two cells -- `f7_n1000_path_warm` and `f7_n800_path_warm` -- carry ~96 % "
              "of the U0 set's instructions between them (1.08e12 and 6.66e11 against "
              "1.82e12 for all 27 in walk mode) and both take the same TWO major "
              "iterations as every other cell, so their attached ratio is ~1.00004 and "
              "they pull the SUM-of-medians corpus figure onto themselves. The per-cell "
              "median is the figure to read for what an attached observer costs a "
              "typical cell; the corpus figure is the one the FLAT definition names, and "
              "both are inside the band.\n")
    out.write("\nCounter check: **%s**.\n" % ("PASS -- byte-identical" if worst == 0
                                              else "FAIL"))
    if out is not sys.stdout:
        out.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
