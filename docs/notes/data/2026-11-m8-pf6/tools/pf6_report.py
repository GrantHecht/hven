#!/usr/bin/env python3
"""PF-6 report: B/A (sequential over intel) instruction table from out/stat/*.json.  usage: pf6_report.py <statdir> <out.md>"""
import glob, json, sys, collections, csv
d = collections.defaultdict(dict)
for p in glob.glob(sys.argv[1] + "/*.json"):
    j = json.load(open(p)); d[(j["lane"], j["problem"])][(j["layer"], j["rep"])] = j
rows = []; bad = []; nondet = []
for (lane, prob), m in sorted(d.items()):
    ok = all(k in m and m[k]["instructions"] for k in [("intel", 0), ("intel", 1), ("sequential", 0), ("sequential", 1)])
    if not ok: bad.append(f"{lane} {prob}: missing/failed run"); continue
    ia = [m[("intel", r)]["instructions"] for r in (0, 1)]; sa = [m[("sequential", r)]["instructions"] for r in (0, 1)]
    cnt = {k: (m[k]["status"], m[k]["iterations"], m[k]["factorizations"]) for k in m}
    same_counters = len(set(cnt.values())) == 1
    SPREAD = 0.012  # PF-3 measured max cross-run spread 1.175e-2 (out/determinism.txt); instructions:u are NOT exactly reproducible
    det_i = abs(ia[0] - ia[1]) / max(ia) <= SPREAD; det_s = abs(sa[0] - sa[1]) / max(sa) <= SPREAD
    spr_i = abs(ia[0] - ia[1]) / max(ia); spr_s = abs(sa[0] - sa[1]) / max(sa)
    if not (det_i and det_s): nondet.append((lane, prob, ia, sa))
    mi = sum(ia) / 2; ms = sum(sa) / 2
    rows.append((lane, prob, mi, ms, ms / mi, same_counters, det_i, det_s, ia, sa, cnt[("intel", 0)], spr_i, spr_s))
out = open(sys.argv[2], "w")
P = lambda s="": print(s, file=out)
P("| lane | problem | intel | sequential | seq/intel | counters equal | spread intel | spread seq |"); P("|---|---|---:|---:|---:|---|---:|---:|")
for r in sorted(rows, key=lambda r: r[2]):
    P(f"| {r[0]} | {r[1]} | {r[2]:,.0f} | {r[3]:,.0f} | {r[4]:.4f} | {r[5]} | {r[11]:.1e} | {r[12]:.1e} |")
good = [r for r in rows if r[6] and r[7] and r[5]]
P(); P(f"cells {len(rows)}; asserted (counters equal across layers and reps, both layers reproduce): {len(good)}; missing/failed: {len(bad)}; counter mismatches: {sum(not r[5] for r in rows)}; spread > 1.2%: {len(nondet)}; max spread intel {max(r[11] for r in rows):.1e} seq {max(r[12] for r in rows):.1e}")
for b in bad: P("- " + b)
for n in nondet: P(f"- NONDET {n}")
for lab, sel in (("ALL", good), ("HS", [r for r in good if r[1].isupper() is False or True])):
    pass
ti = sum(r[2] for r in good); ts = sum(r[3] for r in good)
P(f"suite (asserted cells, mean of 2 reps) intel {ti:,.0f} sequential {ts:,.0f} ratio {ts/ti:.5f} delta {ts-ti:,.0f} ({(ts-ti)/ti*100:+.3f}%)")
import statistics
rr = [r[4] for r in good]
P(f"per-cell ratio median {statistics.median(rr):.4f} min {min(rr):.4f} max {max(rr):.4f}; seq<intel in {sum(x<1 for x in rr)} of {len(rr)}")
sm = sorted(good, key=lambda r: r[2])[:8]
P("smallest 8 cells (per-process bring-up): " + "; ".join(f"{r[1]}/{r[0]} delta {r[3]-r[2]:,.0f}" for r in sm))
dl = sorted(r[3] - r[2] for r in good)
P(f"per-cell delta median {statistics.median(dl):,.0f}")
out.close()
