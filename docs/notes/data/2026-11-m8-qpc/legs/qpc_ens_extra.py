#!/usr/bin/env python3
"""QPC: the three ensemble readings DECLARATION-QPC needs beyond `ens.py read`, computed with the reader's own loaders and
class rule (hven-bench ens 27c02687), never with re-implemented ones.

  qpc_ens_extra.py power <bench> <store> <BINARIES> <freeze.json> <arm>
      P-POWER (section 4): baseline-STABLE cells strict-optimal in every baseline member (p_base == 1), per family and
      corpus, on which the ARM's member 0 shows converged_refinement_arms > 0 / certificate_gate_refusals > 0. (`ens.py power`
      needs the event column in the BASELINE rows; R has none.)
  qpc_ens_extra.py vstab <bench> <store> <BINARIES> <freeze.json> <arm> [--m M] [--kind x0]
      V-STAB (section 5.2, C553 Q1): the arm's classes from its OWN ensemble (ens.load_ensemble + ens.classes_of, the rule
      the freeze applied to R); per corpus the number of distinct problem NAMES with >= 1 STATUS-UNSTABLE cell, arm vs the
      frozen R classes. GATE: arm <= R + 2 in every corpus. rc 0 pass, 1 fail. Also prints the class counts per family and
      the R-class x arm-class transition table (reported).
  qpc_ens_extra.py qm <bench> <store> <BINARIES> <freeze.json> <arm> [--m M] [--kind x0]
      Reported: per seed the pooled geomean of (QM_arm+1)/(QM_R+1) on cells strict-optimal in both and the same solution
      (one term per problem name, the reader's same-solution rule and dedup), the median over seeds; and the S1 robust-loss
      count on the optimal+acceptable reading (p_R >= robust_base_min and p_arm <= robust_arm_max, the reader's params).
"""
import argparse
import collections
import math
import os
import statistics
import sys
from fractions import Fraction

NEW = ("converged_refinement_arms", "certificate_gate_refusals")


def setup(bench, binaries):
    sys.path.insert(0, os.path.join(os.path.realpath(bench), "tu"))
    import ens
    import i19_cells
    import i19_rule
    import i19_run
    spec = i19_run.load_spec()
    i19_cells.register_binary_arms(spec, i19_cells.read_declared_binaries(binaries))
    return ens, i19_cells, i19_rule, i19_run, spec


def corpora_of(frozen):
    return frozen["source"]["corpora"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=("power", "vstab", "qm"))
    ap.add_argument("bench"), ap.add_argument("store"), ap.add_argument("binaries"), ap.add_argument("freeze"), ap.add_argument("arm")
    ap.add_argument("--m", type=int, default=None)
    ap.add_argument("--kind", default="x0")
    a = ap.parse_args()
    ens, i19_cells, i19_rule, i19_run, spec = setup(a.bench, a.binaries)
    frozen = ens.load_freeze(a.freeze)
    params = ens.params_from_json(frozen["params"])
    corpora = corpora_of(frozen)
    m = a.m if a.m is not None else params["members"]
    classes_r = frozen["classes"]          # 'corpus|set|problem|base' -> class
    p_base = frozen["p_base"]
    if a.cmd == "power":
        d0 = i19_run.load_arm_data(a.store, a.arm, corpora)
        tally = collections.defaultdict(lambda: [0, 0, 0])   # group -> [cells, C1 hits, C2 hits]
        for cid, rec in d0.items():
            k = ens.ckey(cid)
            if classes_r.get(k) != ens.STABLE or Fraction(p_base.get(k, "0")) != 1:
                continue
            for g in (cid[0], i19_rule.family_of(cid), "pooled"):
                t = tally[g]
                t[0] += 1
                for i, c in enumerate(NEW):
                    v = i19_rule.col(rec, c)
                    if v in (None, "", "null"):
                        raise SystemExit("STOP: %s %s lacks %s" % (a.arm, k, c))
                    t[1 + i] += float(v) > 0
        for g, (n, c1, c2) in sorted(tally.items()):
            print("POWER %-12s stable_solved=%4d C1_exercised=%4d C2_exercised=%4d" % (g, n, c1, c2))
        return 0
    arm_ens = ens.load_ensemble(a.store, a.arm, corpora, m, a.kind, spec=spec)
    if a.cmd == "vstab":
        classes_a = {ens.ckey(c): k for c, k in ens.classes_of(arm_ens, params).items()}
        ok = True
        for corpus in corpora:
            names = lambda cl: {k.split("|")[2] for k, v in cl.items() if k.startswith(corpus + "|") and v == ens.STATUS_UNSTABLE}
            nr, na = names(classes_r), names(classes_a)
            verdict = "PASS" if len(na) <= len(nr) + 2 else "FAIL"
            ok &= verdict == "PASS"
            print("VSTAB %-11s R=%3d arm=%3d (bound R+2) %s  new=%s gone=%s" % (corpus, len(nr), len(na), verdict,
                                                                                sorted(na - nr), sorted(nr - na)))
        trans = collections.Counter((classes_r.get(k), v) for k, v in classes_a.items())
        for (r, x), n in sorted(trans.items(), key=str):
            print("TRANSITION R=%s arm=%s %d" % (r, x, n))
        print("CLASS-COUNTS arm %s" % ens.class_counts({c: classes_a[ens.ckey(c)] for c in arm_ens.ids}, by=i19_rule.family_of))
        print("VSTAB %s" % ("PASS" if ok else "FAIL"))
        return 0 if ok else 1
    # qm
    base = ens.load_ensemble(a.store, "H0", corpora, m, a.kind, spec=spec)
    ids = sorted(set(base.ids) & set(arm_ens.ids))
    geos = []
    for j in range(m + 1):
        h, x = base.data[j], arm_ens.data[j]
        groups = collections.defaultdict(list)
        for cid in ids:
            o = ens.cell_outcome(h[cid], x[cid], params)
            if o and o["solution"] == "same":
                groups[ens.name_of(cid)].append(cid)
        q = lambda r: float(i19_rule.col(r, "qp_minor_iters"))
        terms = [sum(math.log((q(x[c]) + 1) / (q(h[c]) + 1)) for c in cs) / len(cs) for cs in groups.values()]
        g = math.exp(sum(terms) / len(terms)) if terms else 1.0
        geos.append(g)
        print("QM seed %d terms=%d geomean=%.4f" % (j, len(terms), g))
    print("QM median geomean %.4f (reported)" % statistics.median(geos))
    lost = []
    for cid in ids:
        pr = Fraction(sum(i19_rule.is_acc(r) for r in base.recs(cid)), m + 1)
        pa = Fraction(sum(i19_rule.is_acc(r) for r in arm_ens.recs(cid)), m + 1)
        if pr >= params["robust_base_min"] and pa <= params["robust_arm_max"]:
            lost.append((ens.ckey(cid), str(pr), str(pa)))
    for l in lost:
        print("ROBUST-LOSS-ACCEPTABLE %s p_R=%s p_arm=%s" % l)
    print("ROBUST-LOSSES optimal+acceptable: %d" % len(lost))
    return 1 if lost else 0


if __name__ == "__main__":
    sys.exit(main())
