#!/usr/bin/env python3
"""A1 I3 (ipm arm at ON) / I3b: compare two --f7-path CSVs of hven_sqp_ipqp_e1_arm.
usage: f7_compare.py OFF.csv ON.csv LABEL"""
import csv
import sys

M = ["ipqp_margin_suspects", "ipqp_margin_fallback_suspects", "ipqp_margin_extensions",
     "ipqp_margin_extension_iters", "ipqp_margin_demoted"]


def load(p):
    with open(p) as f:
        return list(csv.DictReader(l for l in f if not l.startswith("#")))


off, on = load(sys.argv[1]), load(sys.argv[2])
label = sys.argv[3]
assert [r["id"] for r in off] == [r["id"] for r in on]
cols = [k for k in off[0] if k not in ("wall_s",)]
fired = 0
lost = []
print(f"{'cell':34s} {'status off>on':>22s} {'facts':>11s} {'uncert':>9s} {'susp/fb/ext/eit/dem':>20s} diffs")
for a, b in zip(off, on):
    nd = sum(1 for k in cols if a[k] != b[k] and k not in M)
    mc = "/".join(b[k] for k in M)
    if int(b["ipqp_margin_extensions"]) > 0 or int(b["ipqp_margin_suspects"]) > 0:
        fired += 1
    if a["status"] == "optimal" and b["status"] != "optimal":
        lost.append(a["id"])
    print(f"{a['id']:34s} {a['status']:>10s}>{b['status']:<11s} {a['factorizations']:>5s}>{b['factorizations']:<5s} "
          f"{a['ipqp_face_uncertain']:>4s}>{b['ipqp_face_uncertain']:<4s} {mc:>20s} {nd}")
print(f"[{label}] cells with any fire counter > 0: {fired} of {len(on)}; status lost vs OFF: {lost}")
