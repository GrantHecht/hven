#!/usr/bin/env python3
"""W3 in-tree scoring (design page s5.1-5.4): pure file processing over meas/. Prints every clause."""
import csv, os, re, sys
M = sys.argv[1]
def rows(name, key):
    p = os.path.join(M, name)
    return {r[key]: r for r in csv.DictReader(l for l in open(p) if not l.startswith("#"))}
def C(n): return rows(n, "cell_id")
def H(n): return rows(n, "hs")
WALL_HS = {"wall_median_s", "wall_min_s", "wall_max_s", "spread_pct", "median_se_pct"}
WALL_C = {"wall_s"}
out = []
def p(s=""): out.append(s)

def lost(off, arm, opt):
    return [k for k in off if off[k]["status"] == opt and arm.get(k, {}).get("status") != opt]
def diffcols(a, b, ex):
    d = []
    for k in a:
        for c in a[k]:
            if c in ex: continue
            if c not in b.get(k, {}) or a[k][c] != b[k][c]: d.append((k, c, a[k][c], b.get(k, {}).get(c)))
    return d
def isum(rs, c):
    return sum(int(float(r[c])) for r in rs.values() if r[c] not in ("", "-1", "null"))

# G-H1
for e in ("walk", "ssn"):
    a, b = H(f"hs-base-{e}-off.csv"), H(f"hs-head-{e}-off.csv")
    d = diffcols(a, b, WALL_HS)
    p(f"G-H1 HS {e}: base cols {len(next(iter(a.values())))}, rows {len(a)}/{len(b)}, differences {len(d)} {d[:5]}")
# G-H2
off, r5 = H("hs-head-ssn-off.csv"), H("hs-head-ssn-r5.csv")
moved = [k for k in off if off[k]["factorizations"] != r5[k]["factorizations"]]
exact = [k for k in moved if int(off[k]["factorizations"]) - int(r5[k]["factorizations"]) == int(off[k]["ssn_refinements"])]
p(f"G-H2 R5 HS rows whose factorizations moved: {len(moved)} (exactly OFF ssn_refinements on {len(exact)}) -> {'PASS' if moved else 'FAIL'}")
# G-S (farkas build OFF == head OFF)
d1 = diffcols(C("c44-head-off.csv"), C("c44-f4-off.csv"), WALL_C)
d2 = diffcols(H("hs-head-ssn-off.csv"), H("hs-f4-ssn-off.csv"), WALL_HS)
p(f"G-S farkas1e-4 OFF vs head OFF: corpus-44 diffs {len(d1)} {d1[:4]}; HS-27 diffs {len(d2)} {d2[:4]}")
p()
# R clauses
offC, offH = C("c44-head-off.csv"), H("hs-head-ssn-off.csv")
for arm in ("r1", "r4", "r5"):
    aC, aH = C(f"c44-head-{arm}.csv"), H(f"hs-head-ssn-{arm}.csv")
    la, lb = lost(offC, aC, "Optimal"), lost(offH, aH, "optimal")
    p(f"{arm.upper()}: F-a {'PASS' if not la else 'FAIL '+str(la)}; F-b {'PASS' if not lb else 'FAIL '+str(lb)}")
    p(f"    corpus-44 status Optimal off/arm {sum(r['status']=='Optimal' for r in offC.values())}/{sum(r['status']=='Optimal' for r in aC.values())}; "
      f"sum fact off/arm {isum(offC,'factorizations')}/{isum(aC,'factorizations')}; rows differing (non-wall) {len(set(k for k,*_ in diffcols(offC,aC,WALL_C)))}")
    p(f"    HS-27 optimal off/arm {sum(r['status']=='optimal' for r in offH.values())}/{sum(r['status']=='optimal' for r in aH.values())}; "
      f"sum fact off/arm {isum(offH,'factorizations')}/{isum(aH,'factorizations')}; rows differing (non-wall) {len(set(k for k,*_ in diffcols(offH,aH,WALL_HS)))}")
    for nm, o, a, key in (("corpus", offC, aC, "esc_infeasible_suspect"), ("HS", offH, aH, "esc_infeasible_suspect")):
        p(f"    {nm} sum esc_infeasible_suspect off/arm {isum(o,key)}/{isum(a,key)}; sum ssn_prox_updates off/arm {isum(o,'ssn_prox_updates')}/{isum(a,'ssn_prox_updates')}; sum esc_budget off/arm {isum(o,'esc_budget')}/{isum(a,'esc_budget')}")
    # descriptive predictions
    if arm == "r1":
        bad = [k for k in offH if offH[k]["ssn_prox_updates"] == "0" and any(offH[k][c] != aH[k][c] for c in offH[k] if c not in WALL_HS)]
        badc = [k for k in offC if offC[k]["ssn_prox_updates"] == "0" and any(offC[k][c] != aC[k][c] for c in offC[k] if c not in WALL_C)]
        p(f"    pred 'identical where OFF prox_updates==0': HS violations {bad}; corpus violations {badc}")
    if arm == "r4":
        bad = [k for k in offH if offH[k]["esc_infeasible_suspect"] == "0" and any(offH[k][c] != aH[k][c] for c in offH[k] if c not in WALL_HS)]
        badc = [k for k in offC if offC[k]["esc_infeasible_suspect"] == "0" and any(offC[k][c] != aC[k][c] for c in offC[k] if c not in WALL_C)]
        p(f"    pred 'identical where OFF suspect==0': HS violations {bad}; corpus violations {badc}")
    if arm == "r5":
        for nm, o, a, ex in (("HS", offH, aH, WALL_HS), ("corpus", offC, aC, WALL_C | {"qp_fact_per_qp"})):
            other = sorted(set(k for k, c, *_ in diffcols(o, a, ex) if c != "factorizations"))
            notexact = [k for k in o if o[k]["factorizations"] not in ("-1",) and int(o[k]["factorizations"]) - int(a[k]["factorizations"]) != int(o[k]["ssn_refinements"])]
            p(f"    pred R5 {nm}: rows moving beyond factorizations {other}; rows whose saving != OFF ssn_refinements {notexact}")
p()
# R4 1e-4 re-derivation
r4C, r4H = C("c44-head-r4.csv"), H("hs-head-ssn-r4.csv")
f4C, f4H = C("c44-f4-r4.csv"), H("hs-f4-ssn-r4.csv")
ec = lost(r4C, f4C, "Optimal") + lost(r4H, f4H, "optimal")
p(f"E-c {'PASS' if not ec else 'FAIL '+str(ec)}; corpus suspects 1e-6/1e-4 {isum(r4C,'esc_infeasible_suspect')}/{isum(f4C,'esc_infeasible_suspect')}; HS {isum(r4H,'esc_infeasible_suspect')}/{isum(f4H,'esc_infeasible_suspect')}; "
  f"rows differing corpus {len(set(k for k,*_ in diffcols(r4C,f4C,WALL_C)))} HS {len(set(k for k,*_ in diffcols(r4H,f4H,WALL_HS)))}")
def probe(fn):
    t = open(os.path.join(M, fn)).read()
    rec = re.search(r"R4 Farkas-gated\s+(\d+)\s+(\d+)\s+([\d.]+)", t)
    sym = re.search(r"shipped symptoms\s+(\d+)\s+(\d+)\s+([\d.]+)", t)
    fps = re.findall(r"R4 Farkas-gated\s+\d+\s+(\d+)\s+[\d.]+\s*%\s+(\d+)", t)
    return sym.groups(), rec.groups(), fps
for fn in ("probe-head-lever-farkas.txt", "probe-f4-lever-farkas.txt"):
    try:
        s, r, fp = probe(fn); p(f"probe {fn}: symptoms (QPs,diag,recall)={s} farkas={r} FP rows (falseKinf,refusals)={fp}")
    except Exception as ex: p(f"probe {fn}: PARSE {ex}")
p()
# tau
for t in ("tau10", "tau1e3"):
    la = lost(C("u0-head-walk.csv"), C(f"u0-{t}-walk.csv"), "Optimal")
    lb = lost(H("hs-head-walk-off.csv"), H(f"hs-{t}-walk.csv"), "optimal")
    du = len(set(k for k,*_ in diffcols(C("u0-head-walk.csv"), C(f"u0-{t}-walk.csv"), WALL_C)))
    dh = len(set(k for k,*_ in diffcols(H("hs-head-walk-off.csv"), H(f"hs-{t}-walk.csv"), WALL_HS)))
    p(f"{t}: T-a {'PASS' if not la else 'FAIL '+str(la)} (U0 rows differing {du}); T-b {'PASS' if not lb else 'FAIL '+str(lb)} (HS rows differing {dh})")
print("\n".join(out))
