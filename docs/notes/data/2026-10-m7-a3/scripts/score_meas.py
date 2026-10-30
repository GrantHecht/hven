#!/usr/bin/env python3
"""A3 scoring (design page s5.1 G2/G2b/G4, s5.2 F-a..F-d + descriptive sums, s3.3 probe P).
Pure file processing. Usage: score_meas.py MEAS_DIR [OLD_E1_CSV]"""
import csv
import os
import sys

NAMED_RED = "e1_f7_n20000_af30_m1e-6"
ITER_GATE = 40
HARD_CAP = 60
ARMS = ["off", "s1", "s3", "k1", "kn"]
NEW = "ipqp_ladder_zero_trial_skips"


def rows(path):
    with open(path) as f:
        return list(csv.DictReader(l for l in f if not l.startswith("#")))


def key(r):
    return r.get("id") or (r.get("family", "") + "/" + r.get("id", ""))


def identity(a_path, b_path, label, out):
    a, b = rows(a_path), rows(b_path)
    cols = [c for c in a[0].keys() if c != "wall_s"]
    diffs = 0
    if len(a) != len(b):
        out.append(f"{label}: ROW COUNT {len(a)} vs {len(b)}")
        return 1
    for ra, rb in zip(a, b):
        for c in cols:
            if ra[c] != rb.get(c):
                diffs += 1
                out.append(f"  {label} DIFF {ra.get('id')} {c}: {ra[c]} -> {rb.get(c)}")
    extra = [c for c in b[0].keys() if c not in a[0]]
    nz = sum(1 for r in b for c in extra if c == NEW and r[c] != "0")
    out.append(f"{label}: {len(a)} rows x {len(cols)} base columns (wall_s excluded): {diffs} differences;"
               f" head-only columns {extra}, {NEW} non-zero on {nz} rows")
    return diffs + nz


def e1_score(path):
    rs = rows(path)
    red = sorted(r["id"] for r in rs if not (r["status"] == "optimal" and int(r["ipqp_iters"]) < ITER_GATE))
    constructed = [r for r in rs if r["layout"] != "anchor"]
    e2e = sum(1 for r in constructed if r["e2e_rule_a_missed"] == "0" and r["e2e_rule_a_false_positive"] == "0")
    tier = sum(1 for r in constructed if r["misclassified"] == "0")
    fac = sum(int(r["e2e_factorizations"]) for r in rs)
    iters = sum(int(r["ipqp_iters"]) for r in rs)
    skips = sum(int(r[NEW]) for r in rs)
    return dict(red=red, e2e=e2e, tier=tier, fac=fac, iters=iters, skips=skips, n=len(rs),
                constructed=len(constructed), by_id={r["id"]: r for r in rs})


def fam_score(path):
    rs = rows(path)
    by = {r["family"] + "/" + r["id"]: r for r in rs}
    hs_fac = sum(int(r["ipqp_factorizations"]) for r in rs if r["family"] == "hs")
    return dict(by=by, hs_fac=hs_fac, skips=sum(int(r[NEW]) for r in rs))


def lad_score(path):
    rs = rows(path)
    by = {r["id"]: r for r in rs}
    ok = {i: (r["escape"] in ("0", "3") and int(r["ipqp_iters"]) < HARD_CAP) for i, r in by.items()}
    fac = sum(int(r["ipqp_factorizations"]) for r in rs)
    return dict(by=by, ok=ok, fac=fac, skips=sum(int(r[NEW]) for r in rs))


def good(status):
    return status in ("optimal", "kOptimal")


def main(argv):
    d = argv[1]
    out = []
    out.append("== G1 is compare_replay's output (u0-exact-*.txt) ==")
    out.append("== G2 / G2b: OFF identity, head vs base (new toolchain, same session) ==")
    g2 = identity(f"{d}/e1-base-off.csv", f"{d}/e1-head-off.csv", "G2 e1 --gate OFF", out)
    g2b = identity(f"{d}/fam-base-off.csv", f"{d}/fam-head-off.csv", "G2b --families OFF", out)
    out.append(f"G2 verdict: {'PASS' if g2 == 0 else 'FAIL'}; G2b verdict: {'PASS' if g2b == 0 else 'FAIL'}")

    e1 = {a: e1_score(f"{d}/e1-head-{a}.csv") for a in ARMS}
    fam = {a: fam_score(f"{d}/fam-head-{a}.csv") for a in ARMS}
    lad = {a: lad_score(f"{d}/lad-head-{a}.csv") for a in ARMS}

    out.append("\n== G4 fire counter ==")
    g4 = []
    for a in ARMS:
        out.append(f"  {a}: {NEW} sums E1 {e1[a]['skips']}, families {fam[a]['skips']}, ladder {lad[a]['skips']}")
    g4.append(all(e1[a]["skips"] == 0 for a in ARMS))
    g4.append(e1["kn"]["skips"] == 0 and fam["kn"]["skips"] == 0 and lad["kn"]["skips"] == 0)
    g4.append(lad["off"]["skips"] > 0)
    out.append(f"G4: E1 zero under every arm {g4[0]}; KN zero everywhere {g4[1]}; OFF ladder non-vacuous {g4[2]}"
               f" -> {'PASS' if all(g4) else 'FAIL'}")

    out.append("\n== s5.2 fixture clauses (VETO-CLASS), vs OFF in the same build ==")
    o = e1["off"]
    out.append(f"OFF: E1 RED {o['red']}; e2e exact {o['e2e']}/{o['constructed']}; tier-contract {o['tier']}/{o['constructed']};"
               f" sum e2e_factorizations {o['fac']}; sum ipqp_iters {o['iters']}; families HS ipqp_factorizations"
               f" {fam['off']['hs_fac']}; ladder ipqp_factorizations {lad['off']['fac']}")
    for a in ARMS[1:]:
        x = e1[a]
        fa = set(x["red"]) <= {NAMED_RED}
        fb = x["e2e"] >= o["e2e"] and x["tier"] >= o["tier"]
        lost_f = [k for k, r in fam["off"]["by"].items() if good(r["status"]) and not good(fam[a]["by"][k]["status"])]
        fc = not lost_f
        lost_l = [k for k, okv in lad["off"]["ok"].items() if okv and not lad[a]["ok"][k]]
        fd = not lost_l
        out.append(f"\n-- arm {a} --")
        out.append(f"  F-a no new RED: RED {x['red']} -> {'PASS' if fa else 'FAIL'}")
        out.append(f"  F-b recovery: e2e {x['e2e']} (OFF {o['e2e']}), tier-contract {x['tier']} (OFF {o['tier']}) -> {'PASS' if fb else 'FAIL'}")
        out.append(f"  F-c families: lost {lost_f} -> {'PASS' if fc else 'FAIL'}")
        out.append(f"  F-d ladder: lost {lost_l} -> {'PASS' if fd else 'FAIL'}")
        out.append(f"  VETO-CLASS overall: {'PASS' if (fa and fb and fc and fd) else 'FAIL'}")
        out.append(f"  descriptive: E1 sum e2e_factorizations {x['fac']} (OFF {o['fac']}), sum ipqp_iters {x['iters']} (OFF {o['iters']});"
                   f" families HS ipqp_factorizations {fam[a]['hs_fac']} (OFF {fam['off']['hs_fac']});"
                   f" ladder ipqp_factorizations {lad[a]['fac']} (OFF {lad['off']['fac']})")
        moved = [i for i in o["by_id"] if any(o["by_id"][i][c] != x["by_id"][i][c] for c in o["by_id"][i] if c != "wall_s")]
        out.append(f"  E1 cells whose row moved vs OFF: {len(moved)}")

    out.append("\n== per-row tables ==")
    for a in ARMS:
        out.append(f"-- ladder {a}: id status escape iters facts retries elevated reclimbs skips")
        for i, r in lad[a]["by"].items():
            out.append(f"   {i} {r['status']} {r['escape']} {r['ipqp_iters']} {r['ipqp_factorizations']}"
                       f" {r['ipqp_inertia_retries']} {r['ipqp_iters_at_elevated_rho']} {r['ipqp_ladder_reclimbs']} {r[NEW]}")
        out.append(f"-- families {a}: key status iters facts skips")
        for k, r in fam[a]["by"].items():
            out.append(f"   {k} {r['status']} {r['ipqp_iters']} {r['ipqp_factorizations']} {r[NEW]}")
    for a in ARMS[1:]:
        out.append(f"-- E1 {a} vs OFF (moved cells): id status iters e2e_fac missed/fp tier-misclassified")
        for i in o["by_id"]:
            r0, r1 = o["by_id"][i], e1[a]["by_id"][i]
            if any(r0[c] != r1[c] for c in r0 if c != "wall_s"):
                out.append(f"   {i} {r0['status']}->{r1['status']} it {r0['ipqp_iters']}->{r1['ipqp_iters']}"
                           f" fac {r0['e2e_factorizations']}->{r1['e2e_factorizations']}"
                           f" e2e {r0['e2e_rule_a_missed']}/{r0['e2e_rule_a_false_positive']}->{r1['e2e_rule_a_missed']}/{r1['e2e_rule_a_false_positive']}"
                           f" mis {r0['misclassified']}->{r1['misclassified']}")
    for probe in ("pu", "pc"):
        lp, fp = f"{d}/lad-probe-{probe}.csv", f"{d}/fam-probe-{probe}.csv"
        if os.path.exists(lp) and os.path.exists(fp):
            L, F = lad_score(lp), fam_score(fp)
            lost_l = [k for k, okv in lad["off"]["ok"].items() if okv and not L["ok"][k]]
            lost_f = [k for k, r in fam["off"]["by"].items() if good(r["status"]) and not good(F["by"][k]["status"])]
            out.append(f"\n== probe {probe.upper()} (DESCRIPTIVE) ==")
            out.append(f"  ladder lost {lost_l}; families lost {lost_f}; ladder facts {L['fac']} (OFF {lad['off']['fac']});"
                       f" families HS facts {F['hs_fac']} (OFF {fam['off']['hs_fac']})")
            for i, r in L["by"].items():
                r0 = lad["off"]["by"][i]
                out.append(f"   {i} {r0['status']}/{r0['escape']} it {r0['ipqp_iters']} fac {r0['ipqp_factorizations']}"
                           f" -> {r['status']}/{r['escape']} it {r['ipqp_iters']} fac {r['ipqp_factorizations']}")
            for k, r in F["by"].items():
                r0 = fam["off"]["by"][k]
                out.append(f"   {k} {r0['status']} it {r0['ipqp_iters']} fac {r0['ipqp_factorizations']}"
                           f" -> {r['status']} it {r['ipqp_iters']} fac {r['ipqp_factorizations']}")
    if len(argv) > 2:
        out.append("\n== T-0 (DESCRIPTIVE): base new-toolchain E1 OFF vs the committed old-toolchain CSV ==")
        old = rows(argv[2])
        new = rows(f"{d}/e1-base-off.csv")
        cols = [c for c in old[0] if c != "wall_s" and c in new[0]]
        n = 0
        cells = set()
        for ro, rn in zip(old, new):
            for c in cols:
                if ro[c] != rn[c]:
                    n += 1
                    cells.add((ro["id"], c))
        out.append(f"  {len(old)} rows x {len(cols)} common columns: {n} differences")
        bycol = {}
        for _, c in cells:
            bycol[c] = bycol.get(c, 0) + 1
        for c, k in sorted(bycol.items()):
            out.append(f"    {c}: {k} cells")
    print("\n".join(out))


if __name__ == "__main__":
    main(sys.argv)
