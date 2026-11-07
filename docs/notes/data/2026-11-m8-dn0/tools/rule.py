#!/usr/bin/env python3
"""M8 DN-0: per-cell table and the PRE-REGISTERED G-DENSE rule, applied mechanically.

Reads a run directory produced by legs.sh (stat-r1..3, record, dump, replay) and writes
  OUT/cells.csv     one row per cell (bucket table, totals, replay figures, caveats)
  OUT/rule.json     the four median cells per reading, every condition, the outcome
  OUT/rule.txt      the same, for a reader
The rule text is the DECLARATION's; this file is its only implementation and adds no judgement.

Usage: rule.py RUNDIR OUTDIR
"""
import csv
import glob
import json
import math
import os
import statistics
import sys

BUCKETS = ["B_sym", "B_fac", "B_kkt", "B_eval", "B_setup", "B_eng"]
DN1_FACT_SHARE = 0.40
DN1_RHO = 0.5
DN1_AGREE = 0.99
DN2_KKT_SHARE = 0.25
CAVEATS = {
    "ALLINITC": "CUTEst derivative check: stencil leaves the box",
    "FLETCHER": "CUTEst derivative check: stencil leaves the box",
    "HS30": "CUTEst derivative check: stencil leaves the box",
    "HS31": "CUTEst derivative check: stencil leaves the box",
    "S365": "CUTEst derivative check: stencil leaves the box",
    "S365MOD": "CUTEst derivative check: stencil leaves the box",
    "DEMBO7": "CUTEst derivative check: undiagnosed mismatch",
    "HS67": "CUTEst derivative check: undiagnosed mismatch",
    "HS70": "CUTEst derivative check: undiagnosed mismatch",
    "HS85": "CUTEst derivative check: undiagnosed mismatch",
}
LANE_CAVEATS = {("hven-sqp", "LOOTSMA"): "registered false-infeasibility candidate (infeasible at 2 iterations)"}


def cell_fields(path):
    fields, inside = {}, False
    if not os.path.exists(path):
        return fields
    for line in open(path, errors="replace"):
        line = line.rstrip("\n")
        if line == "CELL_BEGIN":
            inside = True
        elif line == "CELL_END":
            inside = False
        elif inside and "=" in line:
            k, _, v = line.partition("=")
            fields[k] = v
    return fields


def stat_counts(path):
    out = {}
    if os.path.exists(path):
        for line in open(path):
            parts = line.split(",")
            if len(parts) > 2 and parts[0].strip().isdigit():
                out[parts[2].split(":")[0]] = int(parts[0])
    return out


def read_index(path):
    rows = []
    if os.path.exists(path):
        for line in open(path):
            p = line.rstrip("\n").split("\t")
            rows.append({"call": int(p[0]), "fact": int(p[1]), "phase": int(p[2]), "pt": p[3],
                         "n": int(p[4]), "nnz": int(p[5]), "nrhs": int(p[6]), "error": int(p[7])})
    return rows


def engine_weights(index_rows):
    """For each dumped factorization number: (analyzed_before, solve_rhs_count) on its session.

    analyzed_before: a phase-11 call on the same pt since that pt's previous factorization.
    solve_rhs_count: rhs columns solved (phase 33 and partial 331-333, weighted by nrhs) on
    the same pt after it and before the pt's next factorization or release.
    """
    w = {}
    pending_an = {}
    current = {}
    for r in index_rows:
        pt, ph = r["pt"], r["phase"]
        if ph == 11:
            pending_an[pt] = True
        elif r["fact"] >= 0:
            w[r["fact"]] = [pending_an.pop(pt, False), 0]
            current[pt] = r["fact"]
        elif ph in (33, 331, 332, 333):
            f = current.get(pt)
            if f is not None:
                w[f][1] += max(1, r["nrhs"])
        elif ph in (-1, 0):
            current.pop(pt, None)
    return w


def main():
    run, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    roster = [l.rstrip("\n").split("\t") for l in open(os.path.join(run, "roster.tsv"))]
    rows = []
    for pop, lane, prob in roster:
        tag = f"{pop}-{lane}-{prob}"
        r = {"population": pop, "engine": lane, "problem": prob}
        f1 = cell_fields(os.path.join(run, "stat-r1", tag + ".out"))
        res1 = open(os.path.join(run, "stat-r1", tag + ".result")).read() if os.path.exists(
            os.path.join(run, "stat-r1", tag + ".result")) else ""
        r["capped"] = 1 if "CAPPED" in res1 else 0
        r["status"] = "dnf_wall" if r["capped"] else f1.get("status", "error")
        r["iterations"] = f1.get("iterations", "")
        r["objective"] = f1.get("objective", f1.get("f", ""))
        ins = []
        for k in (1, 2, 3):
            c = stat_counts(os.path.join(run, f"stat-r{k}", tag + ".stat"))
            r[f"instr_r{k}"] = c.get("instructions", "")
            if "instructions" in c:
                ins.append(c["instructions"])
        r["instr_median"] = int(statistics.median(ins)) if ins else ""
        aj = os.path.join(run, "record", tag + ".attr.json")
        a = json.load(open(aj)) if os.path.exists(aj) else None
        if a:
            r["rec_samples"] = a["samples"]
            r["rec_solver_instr"] = a["instr_sampled_solver"]
            for b in BUCKETS:
                r[f"share_{b}"] = a["share_of_solver"].get(b)
            r["share_proc_of_all"] = a["proc_share_of_all"]
            r["share_unmatched"] = a["unmatched_share_of_solver"]
            r["share_fallback"] = (a["fallback_instr"] / a["instr_sampled_solver"]
                                   if a["instr_sampled_solver"] else None)
            r["chains_reaching_main"] = a["chains_reaching_main"]
            r["init_share"] = a["init_instr"] / a["instr_sampled_solver"] if a["instr_sampled_solver"] else None
            r["ss_instr"] = a["steady_state_instr"]
            for b in BUCKETS:
                r[f"ss_share_{b}"] = a["steady_state_share"].get(b)
        runs_file = os.path.join(run, "record", tag + ".runs")
        r["rec_runs"] = sum(1 for _ in open(runs_file)) if os.path.exists(runs_file) else 0
        if a and r["rec_runs"]:
            # sampled totals are summed over the repeats; per-run figures divide by their count
            r["rec_solver_instr"] = a["instr_sampled_solver"] // r["rec_runs"]
            r["ss_instr"] = a["steady_state_instr"] // r["rec_runs"]
        r["rec_lost_any"] = int(any("LOST" in l for l in open(runs_file))) if os.path.exists(runs_file) else ""
        idx = read_index(os.path.join(run, "dump", tag + ".dump", "index.tsv"))
        r["factorizations_engine"] = sum(1 for x in idx if x["fact"] >= 0)
        rp = os.path.join(run, "replay", tag + ".replay.csv")
        vp = os.path.join(run, "replay", tag + ".verify.csv")
        mats = list(csv.DictReader(open(rp))) if os.path.exists(rp) else []
        ver = list(csv.DictReader(open(vp))) if os.path.exists(vp) else []
        r["replayed"] = len(mats)
        r["iparm_verified"] = (sum(1 for v in ver if v["iparm_match"] == "1"), len(ver))
        if mats:
            S = sum(int(m["sparse_total"]) for m in mats)
            D = sum(int(m["dense_total"]) for m in mats)
            Dd = sum(int(m["dense_densify"]) for m in mats)
            r["rho"] = D / S if S else None
            r["rho_incl_densify"] = (D + Dd) / S if S else None
            w = engine_weights(idx)
            Sw = Dw = 0
            for m in mats:
                fno = int(m["matrix"][4:8])
                an, k = w.get(fno, [True, 1])
                Sw += (int(m["sparse_analyze"]) if an else 0) + int(m["sparse_factorize"]) + k * int(m["sparse_solve"])
                Dw += int(m["dense_factorize"]) + k * int(m["dense_solve"])
            r["rho_engine_weighted"] = Dw / Sw if Sw else None
            r["neg_agree"] = sum(int(m["neg_agree"]) for m in mats) / len(mats)
            r["neg_agree_engine_vs_dense"] = sum(
                1 for m in mats if int(m["dense_neg"]) >= 0 and int(m["engine_neg"]) == int(m["dense_neg"])) / len(mats)
            r["replay_fidelity"] = sum(
                1 for m in mats if int(m["sparse_neg"]) >= 0 and int(m["sparse_neg"]) == int(m["engine_neg"])) / len(mats)
            r["kkt_n_max"] = max(int(m["n"]) for m in mats)
            r["kkt_density_min"] = min(float(m["density_full"]) for m in mats)
        r["caveat"] = "; ".join(x for x in (CAVEATS.get(prob), LANE_CAVEATS.get((lane, prob))) if x)
        rows.append(r)
    cols = []
    for r in rows:
        for k in r:
            if k not in cols:
                cols.append(k)
    with open(os.path.join(out, "cells.csv"), "w", newline="") as f:
        wtr = csv.DictWriter(f, fieldnames=cols)
        wtr.writeheader()
        for r in rows:
            wtr.writerow({k: (f"{v[0]}/{v[1]}" if isinstance(v, tuple) else
                              (f"{v:.6g}" if isinstance(v, float) else v)) for k, v in r.items()})

    def median_cell(cells, key):
        def rank(c):
            v = c.get(key)
            inf = c["capped"] == 1 or v in ("", None)
            return (1 if inf else 0, v if not inf else 0, c["problem"])
        s = sorted(cells, key=rank)
        return s[math.ceil(len(s) / 2) - 1], s.index(s[math.ceil(len(s) / 2) - 1]) + 1, len(s)

    def evaluate(reading, share_prefix, rank_key):
        res = {"reading": reading, "cells": {}, "engines": {}}
        for lane in ("hven-ipm", "hven-sqp"):
            eng_dn1, eng_dn2 = True, True
            for pop in ("hs", "sd"):
                cs = [c for c in rows if c["engine"] == lane and c["population"] == pop]
                c, rk, n = median_cell(cs, rank_key)
                sh = {b: c.get(share_prefix + b) for b in BUCKETS}
                fac = (sh["B_sym"] or 0) + (sh["B_fac"] or 0) if sh["B_fac"] is not None else None
                rho = c.get("rho")
                agree = c.get("neg_agree")
                dn1 = (fac is not None and fac >= DN1_FACT_SHARE and rho is not None and rho <= DN1_RHO
                       and agree is not None and agree >= DN1_AGREE)
                dn2 = sh["B_kkt"] is not None and sh["B_kkt"] >= DN2_KKT_SHARE
                dom = max(BUCKETS, key=lambda b: sh[b] or 0)
                res["cells"][f"{lane}/{pop}"] = {
                    "median_cell": c["problem"], "rank": rk, "of": n, "rank_key": rank_key,
                    "rank_value": c.get(rank_key), "status": c["status"], "shares": sh,
                    "fact_share": fac, "rho": rho, "rho_incl_densify": c.get("rho_incl_densify"),
                    "rho_engine_weighted": c.get("rho_engine_weighted"), "neg_agree": agree,
                    "replayed": c.get("replayed"), "DN1_condition": dn1, "DN2_condition": dn2,
                    "dominant_bucket": dom, "caveat": c["caveat"]}
                eng_dn1 &= dn1
                eng_dn2 &= dn2
            res["engines"][lane] = {"DN1": eng_dn1, "DN2_provisional": eng_dn2}
        dn1 = any(v["DN1"] for v in res["engines"].values())
        dn2 = any(v["DN2_provisional"] for v in res["engines"].values())
        res["DN1_fires"] = dn1
        res["DN2_fires"] = dn2
        res["outcome"] = "DN-1" if dn1 else ("DN-2" if dn2 else "no dense build")
        if not dn1 and not dn2:
            doms = {k: v["dominant_bucket"] for k, v in res["cells"].items()}
            res["dominant_by_median_cell"] = doms
            res["registered_levers"] = sorted({d for d in doms.values() if d in ("B_setup", "B_eval")})
        return res

    primary = evaluate("primary (process total, one-time init included)", "share_", "instr_median")
    secondary = evaluate("secondary (steady state, process-once solver init excluded)", "ss_share_", "ss_instr")
    json.dump({"primary": primary, "secondary": secondary}, open(os.path.join(out, "rule.json"), "w"), indent=1)
    with open(os.path.join(out, "rule.txt"), "w") as f:
        for res in (primary, secondary):
            f.write(f"== {res['reading']}\n")
            for k, v in res["cells"].items():
                sh = " ".join(f"{b}={v['shares'][b]:.3f}" if v["shares"][b] is not None else f"{b}=NA" for b in BUCKETS)
                f.write(f"{k}: median cell {v['median_cell']} (rank {v['rank']}/{v['of']} by {v['rank_key']}={v['rank_value']}), "
                        f"status {v['status']}\n    {sh}\n    (B_sym+B_fac)={v['fact_share']} rho={v['rho']} "
                        f"rho_incl_densify={v['rho_incl_densify']} rho_engine_weighted={v['rho_engine_weighted']} "
                        f"neg_agree={v['neg_agree']} replayed={v['replayed']}\n    DN1={v['DN1_condition']} "
                        f"DN2={v['DN2_condition']} dominant={v['dominant_bucket']} {v['caveat']}\n")
            f.write(f"engines: {json.dumps(res['engines'])}\nOUTCOME: {res['outcome']}\n")
            if "registered_levers" in res:
                f.write(f"dominant by median cell: {res['dominant_by_median_cell']}\nregistered levers: {res['registered_levers']}\n")
            f.write("\n")
    print(open(os.path.join(out, "rule.txt")).read())


if __name__ == "__main__":
    main()
