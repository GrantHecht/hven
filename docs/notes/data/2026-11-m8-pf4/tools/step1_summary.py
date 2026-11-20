#!/usr/bin/env python3
"""PF-4 step 1: one line per probe cell (step1/probe/*.stderr + .stdout) -> stdout.
Batch columns: border batches that landed >= 1 border (sync_borders), mean batch size k, the k = 1 single-solve
instructions (no batching possible), and for k >= 2 the instructions of the k single solves vs the one multi-RHS solve
(perf_event instructions:u around each), the saving, and that saving as a share of the cell's PF-3 instruction total.
Bit identity: columns whose multi-RHS solution equals the single solve bitwise. Analyses: sparse SymmetricFactor::analyze
calls, and repeats (a pattern hash already analyzed earlier in the process)."""
import glob, json, os, re, sys
here = os.path.dirname(os.path.abspath(__file__))
pf3 = os.path.join(here, "../../2026-11-m8-pf3/out")
d = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "../step1/probe")
print(f"{'cell':9s} {'status':15s} {'batches':>7s} {'mean_k':>6s} {'k1_instr':>9s} {'single_k2+':>10s} {'multi_k2+':>10s} {'m/s':>5s} "
      f"{'saving':>9s} {'%cell':>5s} {'cols_ident':>10s} {'cols_diff':>9s} {'max_rel':>7s} {'analyses':>8s} {'repeats':>7s} {'rep%':>4s}")
for f in sorted(glob.glob(os.path.join(d, "*.stderr"))):
    P = os.path.basename(f)[:-7]
    txt = open(f).read()
    if "PF4PROBE_BEGIN" not in txt:
        continue
    S = M = K1 = cols = bat = ident = diff = 0
    mx = 0.0
    for m in re.finditer(r"PF4K k=(\d+) batches=(\d+) cols=(\d+) instr_single=(\d+) instr_multi=(\d+) .* identical=(\d+) differ=(\d+) max_rel=(\S+)", txt):
        k, b, c, s, mu, i, dd, r = m.groups()
        bat += int(b); cols += int(c)
        if int(k) == 1:
            K1 += int(s)
            continue
        S += int(s); M += int(mu); ident += int(i); diff += int(dd); mx = max(mx, float(r))
    a = re.search(r"analyses=(\d+) repeat_of_last=(\d+) repeat_of_any_seen=(\d+)", txt).groups()
    tot = None
    for sub in ("cells", "cells-sd"):
        p = os.path.join(pf3, sub, f"hven-sqp__{P}.json")
        if os.path.exists(p):
            tot = json.load(open(p)).get("instructions_total")
    st = re.search(r"^status=(\S+)", open(f[:-7] + ".stdout").read(), re.M).group(1)
    sav = S - M
    print(f"{P:9s} {st:15s} {bat:7d} {cols / max(1, bat):6.1f} {K1:9.2e} {S:10.3e} {M:10.3e} {M / max(1, S):5.3f} {sav:9.2e} "
          f"{(100 * sav / tot) if tot else float('nan'):5.1f} {ident:10d} {diff:9d} {mx:7.1e} {int(a[0]):8d} {int(a[2]):7d} "
          f"{100 * int(a[2]) / max(1, int(a[0])):4.0f}")
    r2 = re.search(r"PF4R repeat_same_instance=(\d+) in_qp_run=(\d+) repeat_within_qp_run=(\d+)", txt)
    if r2:
        print(f"{'':9s} probe2: repeats on the same SymmetricFactor instance {r2.group(1)}; analyses inside a QpEngine::run "
              f"{r2.group(2)}, of which repeats within the same run {r2.group(3)}")
