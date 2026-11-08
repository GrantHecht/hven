#!/usr/bin/env python3
"""M8 DN-1 design page: the numbers behind docs/notes/2026-11-m8-dn1-design.md.

Read-only over DN-0's committed record and its raw replay data. Adds no judgement to DN-0's
rule; every table here is a design input, not a verdict.

Inputs
  DN0      docs/notes/data/2026-11-m8-dn0            (committed: cells.csv, raw/roster.tsv,
                                                      raw/dump/*.index.tsv.gz)
  RUN      <hven-bench-sd>/.scratch/dn0/run           (raw: replay/*.replay.csv, dump/*/mat_*.bin)

Parts
  A  replay inertia agreement and rho by KKT size bin, per engine, stratified by cause
  B  the same, cumulative over n <= N (the "selected sizes" reading)
  C  matrix-level re-factorization in numpy/scipy (Bunch-Kaufman via LAPACK ?sytrf, uplo='U';
     eigvalsh as the reference inertia): reproduces DN-0's dense column, classifies every
     disagreement, and simulates the threshold-fallback policy the page proposes
  D  engine call structure (Pardiso phases per cell from index.tsv) and the per-call cost,
     the dense projection, and the two per-call-overhead alternatives (F2, call trimming)

Usage: dn1_design_analysis.py DN0 RUN OUTDIR [DN0R1]
  DN0R1    docs/notes/data/2026-11-m8-dn0-ipm-r1      (committed: results/init-audit.csv, the IPM's
                                                      init-excluded bucket reading, informational)
Requires numpy and scipy for part C (BSD-licensed; an analysis tool, never linked into hven).
"""
import collections
import csv
import glob
import gzip
import math
import os
import struct
import sys

BINS = [(1, 8), (9, 16), (17, 32), (33, 64), (65, 128), (129, 10**9)]
CUTS = [8, 16, 24, 32, 48, 64, 96, 128, 10**9]


def bin_label(lo, hi):
    return f"n<={hi}" if lo == 1 else (f"{lo}-{hi}" if hi < 10**9 else f">{lo - 1}")


def load_roster(dn0):
    rows = []
    for line in open(os.path.join(dn0, "raw", "roster.tsv")):
        p = line.rstrip("\n").split("\t")
        if len(p) == 3:
            rows.append(tuple(p))
    return rows


def load_replay(run, roster):
    mats = []
    for pop, lane, prob in roster:
        tag = f"{pop}-{lane}-{prob}"
        path = os.path.join(run, "replay", tag + ".replay.csv")
        if not os.path.exists(path):
            continue
        for r in csv.DictReader(open(path)):
            r = {k: (int(v) if k not in ("matrix", "options", "density_full", "sparse_residual",
                                         "dense_residual") else v) for k, v in r.items()}
            r["pop"], r["lane"], r["prob"], r["tag"] = pop, lane, prob, tag
            r["fno"] = int(r["matrix"][4:8])
            mats.append(r)
    return mats


def cause(m):
    """Why a replayed matrix does or does not agree (sparse replay vs dense replay)."""
    if m["sparse_status"] != 0 or m["sparse_neg"] < 0:
        return "sparse_no_count"
    pert = m["sparse_perturbed"] > 0
    if m["dense_status"] != 0:
        return "dense_singular" + ("+perturbed" if pert else "")
    if m["neg_agree"] == 1:
        return "agree" + ("+perturbed" if pert else "")
    return "mismatch" + ("+perturbed" if pert else "")


def size_table(mats, lane, out, cumulative=False):
    sel = [m for m in mats if m["lane"] == lane]
    groups = []
    if cumulative:
        for c in CUTS:
            groups.append((f"n<={c}" if c < 10**9 else "all", [m for m in sel if m["n"] <= c]))
    else:
        for lo, hi in BINS:
            groups.append((bin_label(lo, hi), [m for m in sel if lo <= m["n"] <= hi]))
    hdr = ("bin", "mats", "agree", "agree_rate", "perturbed", "dense_singular", "mismatch_unpert",
           "agree_unpert_rate", "unpert_mats", "rho", "rho_fs", "rho_engine_w")
    out.write("  " + " ".join(f"{h:>16}" for h in hdr) + "\n")
    rows = []
    for name, g in groups:
        if not g:
            continue
        c = collections.Counter(cause(m) for m in g)
        agree = sum(m["neg_agree"] for m in g)
        pert = sum(1 for m in g if m["sparse_perturbed"] > 0)
        dsing = sum(1 for m in g if m["dense_status"] != 0)
        unpert = [m for m in g if m["sparse_perturbed"] == 0 and m["sparse_status"] == 0]
        mism_unpert = sum(1 for m in unpert if m["neg_agree"] == 0)
        S = sum(m["sparse_total"] for m in g)
        D = sum(m["dense_total"] for m in g)
        Sfs = sum(m["sparse_factorize"] + m["sparse_solve"] for m in g)
        Dfs = sum(m["dense_densify"] + m["dense_factorize"] + m["dense_solve"] for m in g)
        r = dict(bin=name, mats=len(g), agree=agree, agree_rate=agree / len(g), perturbed=pert,
                 dense_singular=dsing, mismatch_unpert=mism_unpert,
                 agree_unpert_rate=(1 - mism_unpert / len(unpert)) if unpert else float("nan"),
                 unpert_mats=len(unpert), rho=D / S, rho_fs=Dfs / Sfs, causes=dict(c))
        rows.append(r)
        out.write("  " + " ".join(
            f"{v:>16.4f}" if isinstance(v, float) else f"{v:>16}" for v in
            (r["bin"], r["mats"], r["agree"], r["agree_rate"], r["perturbed"], r["dense_singular"],
             r["mismatch_unpert"], r["agree_unpert_rate"], r["unpert_mats"], r["rho"], r["rho_fs"],
             float("nan"))) + "\n")
    return rows


# ---------------------------------------------------------------- part C (matrix level)

def read_mat(path):
    with open(path, "rb") as f:
        buf = f.read()
    if buf[:7] != b"DN0MAT1":
        raise ValueError(f"{path}: bad magic")
    n, nnz, phase, mtype, err, ibase, neg, pos, pert = struct.unpack_from("<9i", buf, 8)
    off = 8 + 9 * 4 + 64 * 4
    ia = struct.unpack_from(f"<{n + 1}i", buf, off)
    off += (n + 1) * 4
    ja = struct.unpack_from(f"<{nnz}i", buf, off)
    off += nnz * 4
    a = struct.unpack_from(f"<{nnz}d", buf, off)
    base = 0 if ibase else 1
    return n, ia, ja, a, base


def dense_of(path, np):
    n, ia, ja, a, base = read_mat(path)
    M = np.zeros((n, n))
    for i in range(n):
        for k in range(ia[i] - base, ia[i + 1] - base):
            j = ja[k] - base
            M[i, j] = a[k]
            M[j, i] = a[k]
    return M


def bk_blocks(M, np, sla):
    """Bunch-Kaufman (LAPACK ?sytrf, uplo='U') block eigenvalues of D."""
    import warnings
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        _, d, _ = sla.ldl(M, lower=False, hermitian=True)
    n = M.shape[0]
    eigs = []
    k = 0
    while k < n:
        if k + 1 < n and d[k, k + 1] != 0.0:
            eigs.extend(np.linalg.eigvalsh(d[k:k + 2, k:k + 2]).tolist())
            k += 2
        else:
            eigs.append(float(d[k, k]))
            k += 1
    return eigs


def part_c(mats, run, out):
    try:
        import numpy as np
        import scipy.linalg as sla
    except ImportError:
        out.write("part C skipped: numpy/scipy not available\n")
        return None
    res = []
    for m in mats:
        path = os.path.join(run, "dump", m["tag"] + ".dump", m["matrix"])
        M = dense_of(path, np)
        n = M.shape[0]
        amax = float(np.max(np.abs(M))) if n else 0.0
        ev = np.linalg.eigvalsh(M)
        tol = n * np.finfo(float).eps * max(float(np.max(np.abs(ev))), 1e-300)
        true_neg = int(np.sum(ev < -tol))
        true_zero = int(np.sum(np.abs(ev) <= tol))
        blocks = bk_blocks(M, np, sla)
        babs = [abs(x) for x in blocks]
        bk_exact_zero = sum(1 for x in blocks if x == 0.0)
        bk_neg = sum(1 for x in blocks if x < 0.0)
        minpiv = min(babs) / amax if amax > 0 else 0.0
        res.append(dict(m=m, n=n, true_neg=true_neg, true_zero=true_zero, bk_neg=bk_neg,
                        bk_exact_zero=bk_exact_zero, minpiv_rel=minpiv,
                        min_abs_eig_rel=float(np.min(np.abs(ev))) / amax if amax > 0 else 0.0))
    return res


def report_c(res, out):
    if res is None:
        return
    # 1. Does numpy's ?sytrf reproduce DN-0's MKL dsytrf column?
    same_status = sum(1 for r in res if (r["bk_exact_zero"] > 0) == (r["m"]["dense_status"] != 0))
    ok = [r for r in res if r["m"]["dense_status"] == 0 and r["bk_exact_zero"] == 0]
    same_neg = sum(1 for r in ok if r["bk_neg"] == r["m"]["dense_neg"])
    out.write(f"C1 reproduction of DN-0's dense column by numpy/scipy ?sytrf (uplo U): singular status "
              f"{same_status}/{len(res)}; negative count on both-nonsingular {same_neg}/{len(ok)}\n")
    # 2. Who is right in each disagreement class (reference = eigvalsh with tol n*eps*|lambda|max)
    out.write("C2 disagreement classes vs the eigvalsh reference inertia (per engine)\n")
    for lane in ("hven-ipm", "hven-sqp"):
        sub = [r for r in res if r["m"]["lane"] == lane]
        cls = collections.Counter()
        for r in sub:
            m = r["m"]
            c = cause(m)
            if c.startswith("agree"):
                continue
            if c.startswith("dense_singular"):
                key = c + (" truth_has_zero" if r["true_zero"] > 0 else " truth_nonsingular")
            elif c.startswith("mismatch"):
                dense_right = (m["dense_neg"] == r["true_neg"] and r["true_zero"] == 0)
                sparse_right = (m["sparse_neg"] == r["true_neg"] and r["true_zero"] == 0)
                key = c + (" dense_right" if dense_right else "") + (" sparse_right" if sparse_right else "") + \
                    (" truth_has_zero" if r["true_zero"] > 0 else "") + \
                    ("" if dense_right or sparse_right or r["true_zero"] > 0 else " neither")
            else:
                key = c
            cls[key] += 1
        for k, v in sorted(cls.items()):
            out.write(f"  {lane:9} {k:60} {v}\n")
    # 2b. Accuracy against the reference, cumulative by size: on matrices the reference calls nonsingular,
    #     how often each arm's negative count equals the reference's.
    out.write("C2b negative count vs the eigvalsh reference, on reference-nonsingular matrices, cumulative n <= N\n")
    out.write("  lane       cut   ref_nonsing  dense_ok_and_right  dense_rate  sparse_right  sparse_rate  "
              "sparse_unpert  sparse_unpert_rate  ref_singular  dense_exact_singular\n")
    for lane in ("hven-ipm", "hven-sqp"):
        for cut in CUTS:
            sub = [r for r in res if r["m"]["lane"] == lane and r["n"] <= cut]
            ns = [r for r in sub if r["true_zero"] == 0]
            d_ok = sum(1 for r in ns if r["m"]["dense_status"] == 0 and r["m"]["dense_neg"] == r["true_neg"])
            s_ok = sum(1 for r in ns if r["m"]["sparse_neg"] == r["true_neg"])
            unp = [r for r in ns if r["m"]["sparse_perturbed"] == 0]
            s_unp = sum(1 for r in unp if r["m"]["sparse_neg"] == r["true_neg"])
            sing = len(sub) - len(ns)
            dsing = sum(1 for r in sub if r["m"]["dense_status"] != 0)
            out.write(f"  {lane:9} {('all' if cut >= 10**9 else cut):>5} {len(ns):12} {d_ok:19} {d_ok / len(ns):11.4f} "
                      f"{s_ok:13} {s_ok / len(ns):12.4f} {len(unp):14} {s_unp / len(unp):19.4f} {sing:13} {dsing:21}\n")
    # 2c. Pardiso-perturbed matrices: what the dense arm reports there, against the reference
    out.write("C2c Pardiso-perturbed matrices (the walk reads these kSuspect today): dense vs reference\n")
    for lane in ("hven-ipm", "hven-sqp"):
        sub = [r for r in res if r["m"]["lane"] == lane and r["m"]["sparse_perturbed"] > 0]
        ns = [r for r in sub if r["true_zero"] == 0]
        d_ok = sum(1 for r in ns if r["m"]["dense_status"] == 0 and r["m"]["dense_neg"] == r["true_neg"])
        out.write(f"  {lane:9} perturbed {len(sub)}; reference-singular {len(sub) - len(ns)}; reference-nonsingular "
                  f"{len(ns)}, dense right on {d_ok}; dense exactly singular "
                  f"{sum(1 for r in sub if r['m']['dense_status'] != 0)}\n")
    # 3. Threshold fallback policy: dense takes the factorization iff min |BK block eig| > 10^-k * max|a_ij|,
    #    else the platform session takes it (agreement with the sparse replay is then by construction).
    out.write("C3 threshold-fallback policy: dense accepted iff min|D block eig| > 10^-k * max|a_ij| "
              "(and not exactly singular); fallback = Pardiso\n")
    out.write("  lane      k   cut      mats  fallback  fb_rate  acc_mats  acc_agree  acc_agree_rate  "
              "acc_unpert  acc_unpert_agree_rate  acc_pert  acc_vs_truth_rate\n")
    for lane in ("hven-ipm", "hven-sqp"):
        for k in (6, 8, 10, 12):
            for cut in CUTS:
                sub = [r for r in res if r["m"]["lane"] == lane and r["n"] <= cut]
                if not sub:
                    continue
                acc = [r for r in sub if r["bk_exact_zero"] == 0 and r["minpiv_rel"] > 10.0 ** (-k)]
                fb = len(sub) - len(acc)
                acc_agree = sum(1 for r in acc if r["bk_neg"] == r["m"]["sparse_neg"])
                acc_unp = [r for r in acc if r["m"]["sparse_perturbed"] == 0]
                acc_unp_agree = sum(1 for r in acc_unp if r["bk_neg"] == r["m"]["sparse_neg"])
                acc_pert = len(acc) - len(acc_unp)
                acc_truth = sum(1 for r in acc if r["bk_neg"] == r["true_neg"] and r["true_zero"] == 0)
                if cut in (16, 32, 64, 128, 10**9):
                    out.write(f"  {lane:9} {k:2} {('all' if cut >= 10**9 else cut):>5} {len(sub):8} {fb:9} "
                              f"{fb / len(sub):8.4f} {len(acc):9} {acc_agree:10} "
                              f"{(acc_agree / len(acc) if acc else float('nan')):15.4f} {len(acc_unp):11} "
                              f"{(acc_unp_agree / len(acc_unp) if acc_unp else float('nan')):22.4f} "
                              f"{acc_pert:9} {(acc_truth / len(acc) if acc else float('nan')):18.4f}\n")
    # 4. Remaining disagreements under k = 8 (all sizes): list them
    out.write("C4 accepted-but-disagreeing matrices at k = 8, all sizes (cell, matrix, n, sparse_neg, "
              "dense_neg, truth_neg, truth_zero, pardiso_perturbed, minpiv_rel, min|lambda|/amax)\n")
    for r in res:
        m = r["m"]
        if r["bk_exact_zero"] == 0 and r["minpiv_rel"] > 1e-8 and r["bk_neg"] != m["sparse_neg"]:
            out.write(f"  {m['tag']:34} {m['matrix']} n={r['n']:4} sp={m['sparse_neg']:4} bk={r['bk_neg']:4} "
                      f"true={r['true_neg']:4} z={r['true_zero']} pert={m['sparse_perturbed']} "
                      f"minpiv={r['minpiv_rel']:.2e} mineig={r['min_abs_eig_rel']:.2e}\n")
    # 5. Sticky-per-pattern cost: factorizations after a first fallback within the same analyzed pattern
    return res


# ---------------------------------------------------------------- part D (engine call structure)

def read_index(dn0, tag):
    path = os.path.join(dn0, "raw", "dump", tag + ".index.tsv.gz")
    if not os.path.exists(path):
        return []
    rows = []
    with gzip.open(path, "rt") as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            rows.append(dict(call=int(p[0]), fact=int(p[1]), phase=int(p[2]), n=int(p[4]),
                             nrhs=int(p[6])))
    return rows


INIT_MARKERS = ("ensure_solver_initialized", "MKL_Read_ThreadENV_Count_Cores", "__kmp_serial_initialize",
                "__kmp_do_serial_initialize")  # DN-0 r1 init_audit.py's marker, applied to the SQP cells


def sqp_init_share(run, tag):
    """Share of a cell's solver instructions in the process-once OpenMP init (r1's marker), from the raw
    perf script; None when the raw record is absent. Process (loader) samples are excluded as DN-0 does."""
    import json
    sp = os.path.join(run, "record", tag + ".script.gz")
    ap = os.path.join(run, "record", tag + ".attr.json")
    if not (os.path.exists(sp) and os.path.exists(ap)):
        return None
    a = json.load(open(ap))
    init = 0
    with gzip.open(sp, "rt", errors="replace") as f:
        block = []
        for line in f:
            if line.strip():
                block.append(line)
                continue
            if block and any(mk in l for l in block for mk in INIT_MARKERS):
                init += 1
            block = []
        if block and any(mk in l for l in block for mk in INIT_MARKERS):
            init += 1
    return init * a["period"] / a["instr_sampled_solver"] if a["instr_sampled_solver"] else None


def part_d(dn0, mats, out, res, r1=None, run=None):
    cells = {(r["engine"], r["population"], r["problem"]): r for r in
             csv.DictReader(open(os.path.join(dn0, "results", "cells.csv")))}
    # The IPM's DN-0 B shares are the dsecnd()-spin reading; use DN-0 r1's init-excluded reading
    # (init-audit.csv, reading B) instead. Informational only (ruling: no projected-saving weight).
    ipm_b = {}
    if r1:
        for r in csv.DictReader(open(os.path.join(r1, "results", "init-audit.csv"))):
            ss = float(r["ss_per_run"])
            ipm_b[(r["population"], r["problem"])] = (ss, float(r["B_B_fac"]) * ss, float(r["B_B_sym"]) * ss)
    by_tag = collections.defaultdict(list)
    for m in mats:
        by_tag[m["tag"]].append(m)
    pol = {"singular-only": {}, "threshold-k8": {}}
    if res is not None:
        for r in res:
            key = (r["m"]["tag"], r["m"]["matrix"])
            pol["singular-only"][key] = r["m"]["dense_status"] == 0
            pol["threshold-k8"][key] = (r["bk_exact_zero"] == 0 and r["minpiv_rel"] > 1e-8)
    out.write("D1 engine call structure per cell (Pardiso phases from index.tsv; B_fac from cells.csv)\n")
    agg = collections.defaultdict(list)
    sticky = {k: [0, 0, 0] for k in pol}  # [fallbacks, after-fallback-in-pattern, of which dense-acceptable]
    forks = {k: [0, 0.0, 0.0] for k in pol}  # [forks, fork analyze instr, all-sparse-equivalent instr of cells hit]
    fb_cells = {k: collections.Counter() for k in pol}
    for (lane, pop, prob), c in sorted(cells.items()):
        tag = f"{pop}-{lane}-{prob}"
        idx = read_index(dn0, tag)
        if not idx:
            continue
        ph = collections.Counter(x["phase"] for x in idx)
        calls = len(idx)
        n_an, n_fac, n_sol, n_rel = ph[11], ph[22], ph[33], ph[-1]
        rhs = sum(x["nrhs"] for x in idx if x["phase"] == 33)
        try:
            if lane == "hven-ipm" and (pop, prob) in ipm_b:
                solver, bfac, bsym = ipm_b[(pop, prob)]
            else:
                solver = float(c["rec_solver_instr"])
                bfac = float(c["share_B_fac"]) * solver
                bsym = float(c["share_B_sym"]) * solver
        except ValueError:
            continue
        rep = by_tag.get(tag, [])
        # replay per-phase means for this cell (isolated, warm, fresh session each matrix)
        if rep:
            an = sum(m["sparse_analyze"] for m in rep) / len(rep)
            fa = sum(m["sparse_factorize"] for m in rep) / len(rep)
            so = sum(m["sparse_solve"] for m in rep) / len(rep)
            dfa = sum(m["dense_densify"] + m["dense_factorize"] for m in rep) / len(rep)
            dso = sum(m["dense_solve"] for m in rep) / len(rep)
        else:
            an = fa = so = dfa = dso = float("nan")
        pred_sparse = n_an * an + n_fac * fa + rhs * so
        pred_dense = n_fac * dfa + rhs * dso
        # Sticky-per-pattern cost on the replayed window: once a factorization in a pattern group falls back,
        # the rest of that group runs sparse.
        if rep and res is not None:
            group = -1
            fact_group = {}
            for x in idx:
                if x["phase"] == 11:
                    group += 1
                if x["fact"] >= 0:
                    fact_group[x["fact"]] = group
            for pname, acc in pol.items():
                # Per-factorization fallback; the fallback session is reused iff the previous factorization of
                # the engine was also taken by it (its epoch is then the engine's), else forked (one analysis).
                prev_fb = False
                prev_group = None
                for m in sorted(rep, key=lambda z: z["fno"]):
                    g = fact_group.get(m["fno"], -1)
                    ok = acc.get((tag, m["matrix"]), True)
                    if not ok:
                        fb_cells[pname][tag] += 1
                        if not (prev_fb and prev_group == g):
                            forks[pname][0] += 1
                            forks[pname][1] += m["sparse_analyze"]
                    prev_fb, prev_group = (not ok), g
                fell = {}
                for m in sorted(rep, key=lambda z: z["fno"]):
                    g = fact_group.get(m["fno"], -1)
                    ok = acc.get((tag, m["matrix"]), True)
                    if not ok:
                        sticky[pname][0] += 1
                    if fell.get(g):
                        sticky[pname][1] += 1
                        if ok:
                            sticky[pname][2] += 1
                    elif not ok:
                        fell[g] = True
        init_sh = sqp_init_share(run, tag) if (run and lane == "hven-sqp") else None
        rec = dict(init_share=init_sh, lane=lane, pop=pop, prob=prob, calls=calls, an=n_an, fac=n_fac, sol=n_sol, rel=n_rel,
                   rhs=rhs, solver=solver, bfac=bfac, bsym=bsym, per_call=(bfac + bsym) / calls if calls else 0,
                   sol_per_fac=n_sol / n_fac if n_fac else float("nan"), pred_sparse=pred_sparse,
                   pred_dense=pred_dense, an_i=an, fa_i=fa, so_i=so, dfa_i=dfa, dso_i=dso, status=c["status"])
        agg[(lane, pop)].append(rec)
    for key in sorted(agg):
        recs = agg[key]
        lane, pop = key

        def med(f):
            v = sorted(f(r) for r in recs if not math.isnan(f(r)))
            return v[len(v) // 2] if v else float("nan")
        out.write(f"  {lane}/{pop}: cells {len(recs)}; median per cell: calls {med(lambda r: r['calls']):.0f}, "
                  f"analyze {med(lambda r: r['an']):.0f}, factorize {med(lambda r: r['fac']):.0f}, "
                  f"solve {med(lambda r: r['sol']):.0f}, solves/factorization {med(lambda r: r['sol_per_fac']):.1f}, "
                  f"(B_sym+B_fac)/call {med(lambda r: r['per_call']):.0f} instr\n")
        out.write(f"      replay per-phase medians (isolated): analyze {med(lambda r: r['an_i']):.0f}, "
                  f"factorize {med(lambda r: r['fa_i']):.0f}, solve {med(lambda r: r['so_i']):.0f}; dense "
                  f"densify+factorize {med(lambda r: r['dfa_i']):.0f}, solve {med(lambda r: r['dso_i']):.0f}\n")
        # Projection of the three alternatives on each cell, as shares of the cell's solver instructions.
        # measured B = (B_sym + B_fac). Replay-predicted sparse split -> fraction of B in each phase.
        sav_dense, sav_f2, sav_trim, calib = [], [], [], []
        sav_f2_half, sav_f2_third, sav_f2_after_dense, resid_dense = [], [], [], []
        sav_dense_noinit, init_shares = [], []
        for r in recs:
            B = r["bfac"] + r["bsym"]
            if not B or math.isnan(r["pred_sparse"]) or r["pred_sparse"] <= 0:
                continue
            calib.append(B / r["pred_sparse"])
            scale = B / r["pred_sparse"]  # apportion the measured B by the replay's phase split
            b_an = r["an"] * r["an_i"] * scale
            b_fa = r["fac"] * r["fa_i"] * scale
            b_so = r["rhs"] * r["so_i"] * scale
            # dense arm: analysis gone; factorize and solve at the replay's dense/sparse ratio per phase.
            dense_B = r["fac"] * r["dfa_i"] * scale + r["rhs"] * r["dso_i"] * scale
            sav_dense.append((B - dense_B) / r["solver"])
            # F2 upper bound: every solve beyond one per factorization removed (solves -> factorizations).
            f2_B = b_an + b_fa + min(r["rhs"], r["fac"]) * r["so_i"] * scale
            sav_f2.append((B - f2_B) / r["solver"])
            # Call trimming upper bound: every analyze whose pattern repeats an earlier one avoided is not
            # observable from index.tsv; bound = all analyses after the first removed.
            trim_B = B - max(r["an"] - 1, 0) * r["an_i"] * scale
            sav_trim.append((B - trim_B) / r["solver"])
            # F2 realistic: half / a third of the solves removed.
            sav_f2_half.append(0.5 * b_so / r["solver"])
            sav_f2_third.append(b_so / 3.0 / r["solver"])
            # F2 after the dense arm: half of the (now dense) solves removed.
            sav_f2_after_dense.append(0.5 * r["rhs"] * r["dso_i"] * scale / r["solver"])
            resid_dense.append(dense_B / r["solver"])
            if r["init_share"] is not None:
                # Process-once OpenMP init removed from the solver total and (conservatively) from B: a dense
                # arm calling threaded MKL pays it too, so it is not a saving the arm can claim.
                ii = r["init_share"] * r["solver"]
                init_shares.append(r["init_share"])
                sav_dense_noinit.append((B - ii - dense_B) / (r["solver"] - ii))

        def q(v, p):
            v = sorted(v)
            return v[min(len(v) - 1, int(p * len(v)))] if v else float("nan")
        out.write(f"      measured/replay-predicted B calibration: median {q(calib, .5):.2f} "
                  f"(q25 {q(calib, .25):.2f}, q75 {q(calib, .75):.2f})\n")
        out.write(f"      projected saving, share of solver instructions (median, q25, q75):\n")
        out.write(f"        dense arm (no analysis; dense factorize+solve)  {q(sav_dense, .5):.3f} "
                  f"{q(sav_dense, .25):.3f} {q(sav_dense, .75):.3f}\n")
        out.write(f"        F2 bound (solves cut to one per factorization)  {q(sav_f2, .5):.3f} "
                  f"{q(sav_f2, .25):.3f} {q(sav_f2, .75):.3f}\n")
        out.write(f"        trim bound (every analysis after the first)     {q(sav_trim, .5):.3f} "
                  f"{q(sav_trim, .25):.3f} {q(sav_trim, .75):.3f}\n")
        out.write(f"        F2 at half the solves removed                   {q(sav_f2_half, .5):.3f} "
                  f"{q(sav_f2_half, .25):.3f} {q(sav_f2_half, .75):.3f}\n")
        out.write(f"        F2 at a third of the solves removed             {q(sav_f2_third, .5):.3f} "
                  f"{q(sav_f2_third, .25):.3f} {q(sav_f2_third, .75):.3f}\n")
        out.write(f"        F2 (half) on top of the dense arm               {q(sav_f2_after_dense, .5):.3f} "
                  f"{q(sav_f2_after_dense, .25):.3f} {q(sav_f2_after_dense, .75):.3f}\n")
        out.write(f"        residual (B_sym+B_fac) after the dense arm      {q(resid_dense, .5):.3f} "
                  f"{q(resid_dense, .25):.3f} {q(resid_dense, .75):.3f}\n")
        if init_shares:
            out.write(f"      process-once OpenMP init share of solver (r1 marker, raw perf scripts): median "
                      f"{q(init_shares, .5):.3f} (q25 {q(init_shares, .25):.3f}, q75 {q(init_shares, .75):.3f}, "
                      f"max {max(init_shares):.3f})\n")
            out.write(f"        dense arm, init excluded from solver and from B {q(sav_dense_noinit, .5):.3f} "
                      f"{q(sav_dense_noinit, .25):.3f} {q(sav_dense_noinit, .75):.3f}\n")
    for pname, (nf, fa, _) in forks.items():
        out.write(f"D3 policy {pname}: per-factorization fallback forks {nf} fresh sessions on the replayed window "
                  f"(sum of their analyses {fa:.0f} instructions); cells with any fallback: "
                  f"{dict(sorted(fb_cells[pname].items())) if pname == 'singular-only' else len(fb_cells[pname])}\n")
    for pname, (nfb, after, lost) in sticky.items():
        out.write(f"D2 policy {pname}: {nfb} fallbacks on the replayed window; a sticky-per-pattern variant would "
                  f"run {after} later factorizations of the same pattern sparse, {lost} of them dense-acceptable\n")
    return agg


def main():
    dn0, run, outdir = sys.argv[1:4]
    r1 = sys.argv[4] if len(sys.argv) > 4 else None
    os.makedirs(outdir, exist_ok=True)
    roster = load_roster(dn0)
    mats = load_replay(run, roster)
    with open(os.path.join(outdir, "results.txt"), "w") as out:
        out.write(f"DN-1 design analysis over DN-0 TRAIN: {len(roster)} cells in roster, "
                  f"{len(mats)} replayed matrices\n\n")
        for lane in ("hven-ipm", "hven-sqp"):
            out.write(f"A  {lane}: by KKT size bin (sparse replay vs dense replay; 'unpert' = Pardiso perturbed 0; "
                      f"rho = sum dense_total / sum sparse_total incl. a fresh analyze; rho_fs = (densify+factorize+"
                      f"solve) / (factorize+solve), analysis excluded)\n")
            size_table(mats, lane, out)
            out.write(f"B  {lane}: cumulative n <= N\n")
            size_table(mats, lane, out, cumulative=True)
            c = collections.Counter(cause(m) for m in mats if m["lane"] == lane)
            out.write(f"   causes (all sizes): {dict(sorted(c.items()))}\n\n")
        out.write("A' hven-sqp, small-dense population only, by KKT size bin\n")
        size_table([m for m in mats if m["pop"] == "sd"], "hven-sqp", out)
        out.write("\n")
        res = part_c(mats, run, out)
        report_c(res, out)
        out.write("\n")
        part_d(dn0, mats, out, res, r1, run)
        if res is not None:
            import io
            with io.TextIOWrapper(gzip.GzipFile(os.path.join(outdir, "matrices.csv.gz"), "wb", mtime=0),
                                  newline="") as f:
                w = csv.writer(f)
                w.writerow(["cell", "matrix", "n", "sparse_neg", "sparse_perturbed", "dense_status", "dense_neg",
                            "bk_neg_scipy", "bk_exact_zero_scipy", "minpiv_rel", "true_neg", "true_zero",
                            "min_abs_eig_rel", "sparse_total", "dense_total"])
                for r in res:
                    m = r["m"]
                    w.writerow([m["tag"], m["matrix"], r["n"], m["sparse_neg"], m["sparse_perturbed"],
                                m["dense_status"], m["dense_neg"], r["bk_neg"], r["bk_exact_zero"],
                                f"{r['minpiv_rel']:.3e}", r["true_neg"], r["true_zero"],
                                f"{r['min_abs_eig_rel']:.3e}", m["sparse_total"], m["dense_total"]])


if __name__ == "__main__":
    main()
