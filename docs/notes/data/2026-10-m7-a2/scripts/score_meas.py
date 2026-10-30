#!/usr/bin/env python3
"""A2 scoring of the hven-side instruments (design page s6.1 G2/G4, s6.2 E1-a..d, s6.3 T-a/T-b).
Pure file processing. Usage: score_meas.py MEAS_DIR COMMITTED_A4_CSV"""
import csv
import sys

RED = "e1_f7_n20000_af30_m1e-6"


def rows(path):
    return list(csv.DictReader(l for l in open(path) if not l.startswith("#")))


def i(r, k):
    return int(float(r[k]))


def main(argv):
    d, committed = argv[1], argv[2]
    out = []
    # G2: OFF vs the committed T10b gate CSV on that file's columns, wall_s excluded.
    ref = {r["id"]: r for r in rows(committed)}
    off = {r["id"]: r for r in rows(f"{d}/e1-off.csv")}
    cols = [c for c in rows(committed)[0].keys() if c != "wall_s"]
    nd = 0
    for k, r in ref.items():
        for c in cols:
            if off[k].get(c) != r[c]:
                nd += 1
                out.append(f"G2 DIFF {k} {c}: {r[c]} -> {off[k].get(c)}")
    out.append(f"G2 E1 OFF vs committed: {len(ref)} cells x {len(cols)} columns, {nd} differences")
    arms = {a: {r["id"]: r for r in rows(f"{d}/e1-{a}.csv")} for a in ("off", "m0", "mf", "m0mf")}
    base = arms["off"]

    def stat(rs):
        bad = [k for k, r in rs.items() if not (r["status"] == "optimal" and i(r, "ipqp_iters") < 40)]
        cons = [k for k, r in rs.items() if r["layout"] != "anchor"]
        e2e = sum(1 for k in cons if i(rs[k], "e2e_rule_a_missed") == 0
                  and i(rs[k], "e2e_rule_a_false_positive") == 0)
        tc = sum(1 for k in cons if i(rs[k], "misclassified") == 0)
        facts = sum(i(r, "e2e_factorizations") for r in rs.values())
        return bad, e2e, tc, facts

    b_bad, b_e2e, b_tc, b_facts = stat(base)
    out.append(f"E1 OFF: red={b_bad} e2e={b_e2e} tier_contract={b_tc} tier+polish facts={b_facts}")
    for a in ("m0", "mf", "m0mf"):
        rs = arms[a]
        bad, e2e, tc, facts = stat(rs)
        ea = all(k == RED for k in bad)
        eb = facts <= b_facts
        ec = e2e >= b_e2e and tc >= b_tc
        ed = rs[RED]["status"] == "optimal"
        der = sum(i(r, "ipqp_mu0_derived") for r in rs.values())
        fb = sum(i(r, "ipqp_mu0_fallbacks") for r in rs.values())
        binds = sum(i(r, "ipqp_mu_floor_binds") for r in rs.values())
        out.append(f"E1 {a}: red={bad} e2e={e2e} tier_contract={tc} facts={facts} | "
                   f"E1-a {'PASS' if ea else 'FAIL'}  E1-b {'PASS' if eb else 'FAIL'}  "
                   f"E1-c {'PASS' if ec else 'FAIL'}  E1-d {'PASS' if ed else 'FAIL'} | "
                   f"derived={der} fallbacks={fb} floor_binds={binds} | RED cell "
                   f"{rs[RED]['status']} iters={rs[RED]['ipqp_iters']}")
        worse = []
        for k, r in rs.items():
            if r["layout"] == "anchor":
                continue
            o = base[k]
            if (i(r, "e2e_rule_a_missed"), i(r, "e2e_rule_a_false_positive")) != \
               (i(o, "e2e_rule_a_missed"), i(o, "e2e_rule_a_false_positive")):
                worse.append(f"{k} e2e {o['e2e_rule_a_missed']}/{o['e2e_rule_a_false_positive']}"
                             f" -> {r['e2e_rule_a_missed']}/{r['e2e_rule_a_false_positive']}"
                             f" res_comp {o['res_comp']}->{r['res_comp']} iters {o['ipqp_iters']}->{r['ipqp_iters']}")
        out.append(f"   cells whose end-to-end reading moved under {a}: {len(worse)}")
        out.extend("     " + w for w in worse)
    # G4 on OFF
    g4 = sum(i(r, c) for r in base.values()
             for c in ("ipqp_mu0_derived", "ipqp_mu0_fallbacks", "ipqp_mu_floor_binds"))
    out.append(f"G4 E1 OFF: sum of the three A2 counters = {g4}")
    # Families
    fam = {a: {r["id"]: r for r in rows(f"{d}/fam-{a}.csv")} for a in ("off", "m0", "mf", "m0mf")}
    fo = fam["off"]
    out.append("FAMILIES (status, ipqp_iters, ipqp_factorizations, derived/fallbacks/binds):")
    for k in fo:
        out.append("  " + k + " :: " + " | ".join(
            f"{a} {fam[a][k]['status']} {fam[a][k]['ipqp_iters']} {fam[a][k]['ipqp_factorizations']} "
            f"{fam[a][k]['ipqp_mu0_derived']}/{fam[a][k]['ipqp_mu0_fallbacks']}/{fam[a][k]['ipqp_mu_floor_binds']}"
            for a in ("off", "m0", "mf", "m0mf")))
    okstat = ("optimal", "kOptimal")
    hs = [k for k in fo if k.startswith("hs")]
    hs_off = sum(i(fo[k], "ipqp_factorizations") for k in hs)
    for a in ("m0", "mf", "m0mf"):
        lost = [k for k in fo if fo[k]["status"] in okstat and fam[a][k]["status"] not in okstat]
        hs_a = sum(i(fam[a][k], "ipqp_factorizations") for k in hs)
        out.append(f"T {a}: T-a {'PASS' if not lost else 'FAIL ' + str(lost)}  "
                   f"T-b HS facts {hs_a} vs OFF {hs_off}: {'PASS' if hs_a <= hs_off else 'FAIL'}")
    g4f = sum(i(r, c) for r in fo.values()
              for c in ("ipqp_mu0_derived", "ipqp_mu0_fallbacks", "ipqp_mu_floor_binds"))
    out.append(f"G4 families OFF: sum of the three A2 counters = {g4f}")
    print("\n".join(out))


if __name__ == "__main__":
    main(sys.argv)
