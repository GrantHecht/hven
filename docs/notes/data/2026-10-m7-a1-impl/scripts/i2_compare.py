#!/usr/bin/env python3
"""A1 I2: compare E1 arm CSVs.

usage: i2_compare.py repro COMMITTED.csv OFF.csv
       i2_compare.py arm OFF.csv ARM.csv [LABEL]

repro: every column common to both files except wall_s must be byte-equal per cell.
arm:   the design page section 5 C1(b)/(c) readings of ARM against OFF.
"""
import csv
import sys

M6 = ["e1_f7_n4000_af01_m1e-6", "e1_f7_n4000_af10_m1e-6", "e1_f7_n4000_af30_m1e-6",
      "e1_f7_n20000_af01_m1e-6", "e1_f7_n20000_af10_m1e-6"]  # the 5 scattered non-RED m1e-6
RED = "e1_f7_n20000_af30_m1e-6"
MARGIN = ["ipqp_margin_suspects", "ipqp_margin_fallback_suspects", "ipqp_margin_extensions",
          "ipqp_margin_extension_iters", "ipqp_margin_demoted"]


def load(path):
    with open(path) as f:
        rows = list(csv.DictReader(l for l in f if not l.startswith("#")))
    return {r["id"]: r for r in rows}, rows


def repro(committed, off):
    c, _ = load(committed)
    o, _ = load(off)
    assert set(c) == set(o), (set(c) ^ set(o))
    cols = [k for k in next(iter(c.values())) if k in next(iter(o.values())) and k != "wall_s"]
    diffs = 0
    for cid in c:
        for k in cols:
            if c[cid][k] != o[cid][k]:
                diffs += 1
                print(f"DIFF {cid} {k}: committed={c[cid][k]} off={o[cid][k]}")
    print(f"REPRO cells={len(c)} columns_compared={len(cols)} differences={diffs}")
    return 0 if diffs == 0 else 1


def arm(off, armf, label):
    o, rows = load(off)
    a, _ = load(armf)
    assert set(o) == set(a)
    worse = []
    print(f"{'cell':40s} {'OFF m/f pol':>14s} {'ARM m/f pol':>14s} {'fact':>9s} "
          f"{'it':>7s} {'susp/fb/ext/eit/dem':>22s}")
    tot_o = tot_a = 0
    pol_o = pol_a = 0
    for r in rows:
        cid = r["id"]
        x, y = o[cid], a[cid]
        fo, fa = int(x["e2e_factorizations"]), int(y["e2e_factorizations"])
        tot_o += fo
        tot_a += fa
        mo, ffo = int(x["e2e_rule_a_missed"]), int(x["e2e_rule_a_false_positive"])
        ma, ffa = int(y["e2e_rule_a_missed"]), int(y["e2e_rule_a_false_positive"])
        flag = ""
        if mo >= 0 and (ma > mo or ffa > ffo):
            worse.append(cid)
            flag = "  WORSE"
        if cid in M6:
            pol_o += int(x["e2e_polished"])
            pol_a += int(y["e2e_polished"])
        mc = "/".join(y[k] for k in MARGIN)
        print(f"{cid:40s} {mo:4d}/{ffo:<5d}{x['e2e_polished']:>3s}  {ma:4d}/{ffa:<5d}{y['e2e_polished']:>3s} "
              f"{fo:4d}>{fa:<4d} {x['ipqp_iters']:>3s}>{y['ipqp_iters']:<3s} {mc:>22s}"
              f" {y['status']}{flag}")
    ratio = tot_a / tot_o
    print(f"\n[{label}] polish accepted on the 5 scattered non-RED m1e-6: {pol_a} of 5 (OFF {pol_o})")
    print(f"[{label}] cells worse than OFF END-TO-END (missed or false actives): "
          f"{len(worse)} {worse}")
    print(f"[{label}] tier+polish factorizations: {tot_a} / OFF {tot_o} = {ratio:.4f} (<= 1.10?)")
    red = a[RED]
    print(f"[{label}] RED cell: status={red['status']} iters={red['ipqp_iters']} "
          f"margin={'/'.join(red[k] for k in MARGIN)}")
    new_red = [cid for cid in a if cid != RED and (a[cid]["status"] != "optimal" or
                                                   int(a[cid]["ipqp_iters"]) >= 40)]
    print(f"[{label}] new RED cells (status != optimal or iters >= 40): {new_red}")
    ext = sum(int(a[c]["ipqp_margin_extensions"]) for c in a)
    print(f"[{label}] total ipqp_margin_extensions over the 29: {ext}")
    b1 = pol_a >= 4
    b2 = not worse
    b3 = ratio <= 1.10
    print(f"[{label}] C1(b) polish>=4/5: {'PASS' if b1 else 'FAIL'}; no-worse: "
          f"{'PASS' if b2 else 'FAIL'}; factorizations: {'PASS' if b3 else 'FAIL'}; "
          f"C1(d) no new RED: {'PASS' if not new_red else 'FAIL'}")
    return 0


def same(a_path, b_path):
    a, _ = load(a_path)
    b, _ = load(b_path)
    keys = [k for k in next(iter(b.values())) if k in next(iter(a.values()))]
    d = [(c, k, a[c][k], b[c][k]) for c in a for k in keys if a[c][k] != b[c][k]]
    print(f"SAME cells={len(a)} columns={len(keys)} differences={len(d)} {d[:5]}")
    return 0 if not d else 1


if __name__ == "__main__":
    if sys.argv[1] == "same":
        sys.exit(same(sys.argv[2], sys.argv[3]))
    if sys.argv[1] == "repro":
        sys.exit(repro(sys.argv[2], sys.argv[3]))
    sys.exit(arm(sys.argv[2], sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else "ARM"))
