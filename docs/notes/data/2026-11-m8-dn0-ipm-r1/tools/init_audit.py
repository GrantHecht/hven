#!/usr/bin/env python3
"""M8 DN-0 IPM r1 -- POST-HOC init audit (INFORMATIONAL; written after the data; not the rule, changes no criterion).

Finding it measures: after F1, hven::detail::ensure_solver_initialized() calls mkl_get_max_threads(), which runs
MKL_Read_ThreadENV_Count_Cores (reads /proc/cpuinfo) -> omp_get_num_procs -> __kmp_serial_initialize (the OpenMP
runtime's one-time serial initialization: environment parsing, settings dump, system-tick calibration). Under gdb
each of MKL_Read_ThreadENV_Count_Cores and __kmp_serial_initialize is entered exactly once per process, from
ensure_solver_initialized (results/init-audit.txt). perf's DWARF unwinder stops inside the statically linked MKL
(MKL_Read_ThreadENV_Count_Cores carries no CFI), so these samples never show ensure_solver_initialized: the frozen
bucket_map's leaf fallback files them as B_fac (leaf in libiomp5) or unmatched B_eng (leaf in libc), and the
declared secondary reading (marker ensure_solver_initialized) does not remove them.

Init marker (this audit): a sample whose chain contains any of
  hven::detail::ensure_solver_initialized, MKL_Read_ThreadENV_Count_Cores, __kmp_serial_initialize,
  __kmp_do_serial_initialize.
Per cell it reports the marked instructions per run and where the frozen rule put them, and two ALTERNATIVE readings:
  A  "init as B_setup": marked samples moved to B_setup (the brief's definition of B_setup: one-time solver
     construction); total unchanged; median cells as the primary reading's (S-median ranking).
  B  "steady state": marked samples removed from every bucket and from the total (the secondary reading's stated
     intent); median cells re-ranked by per-run steady-state instructions (sampled solver instructions minus marked).
rho and agreement are the cell's own (rule.py). The rule's thresholds are applied unchanged to both.

Usage: init_audit.py TOOLS_DIR RUNDIR CELLS_CSV OUT_CSV  (prints the reading table)
"""
import csv
import math
import os
import sys

MARKERS = ("hven::detail::ensure_solver_initialized", "MKL_Read_ThreadENV_Count_Cores",
           "__kmp_serial_initialize", "__kmp_do_serial_initialize")
B = ["B_sym", "B_fac", "B_kkt", "B_eval", "B_setup", "B_eng"]


def main():
    tools, run, cells_csv, out_csv = sys.argv[1:5]
    sys.path.insert(0, tools)
    import attribute as A
    rows = A.load_map(os.path.join(tools, "bucket_map.tsv"))
    cells = list(csv.DictReader(open(cells_csv)))
    out = []
    for c in cells:
        tag = f"{c['population']}-{c['engine']}-{c['problem']}"
        p = os.path.join(run, "record", tag + ".script.gz")
        tot = {b: 0 for b in B + ["B_proc"]}
        init = {b: 0 for b in B + ["B_proc"]}
        cache = {}
        for (comm, per), frames in A.samples(p):
            if comm != A.COMM:
                continue
            b, _ = A.classify(frames, rows, cache)
            tot[b] += per
            if any(A.normalize(s).endswith(MARKERS) for s, _ in frames):
                init[b] += per
        reps = int(c["rec_runs"]) or 1
        solver = sum(tot[b] for b in B)
        init_solver = sum(init[b] for b in B)
        r = {"population": c["population"], "problem": c["problem"], "capped": int(c["capped"]),
             "instr_median": int(c["instr_median"]) if c["instr_median"] else "",
             "solver_per_run": solver // reps, "init_per_run": init_solver // reps,
             "init_share_of_solver": init_solver / solver if solver else None,
             "init_in_B_fac": init["B_fac"] / solver if solver else None,
             "init_in_B_eng": init["B_eng"] / solver if solver else None,
             "init_in_B_setup": init["B_setup"] / solver if solver else None,
             "init_in_B_proc_per_run": init["B_proc"] // reps,
             "fact_primary": (tot["B_sym"] + tot["B_fac"]) / solver if solver else None,
             "fact_A": (tot["B_sym"] + tot["B_fac"] - init["B_sym"] - init["B_fac"]) / solver if solver else None,
             "fact_B": ((tot["B_sym"] + tot["B_fac"] - init["B_sym"] - init["B_fac"]) / (solver - init_solver)
                        if solver - init_solver else None),
             "kkt_A": (tot["B_kkt"] - init["B_kkt"]) / solver if solver else None,
             "kkt_B": (tot["B_kkt"] - init["B_kkt"]) / (solver - init_solver) if solver - init_solver else None,
             "ss_per_run": (solver - init_solver) // reps,
             "rho": float(c["rho"]) if c.get("rho") else None,
             "neg_agree": float(c["neg_agree"]) if c.get("neg_agree") else None}
        for b in B:
            r[f"A_{b}"] = (tot[b] - init[b] + (init_solver if b == "B_setup" else 0)) / solver if solver else None
            r[f"B_{b}"] = (tot[b] - init[b]) / (solver - init_solver) if solver - init_solver else None
        out.append(r)
    with open(out_csv, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(out[0].keys()))
        w.writeheader()
        for r in out:
            w.writerow({k: (f"{v:.6g}" if isinstance(v, float) else v) for k, v in r.items()})

    def median(cs, key):
        s = sorted(cs, key=lambda c: (1 if c["capped"] else 0, c[key] if not c["capped"] else 0, c["problem"]))
        k = math.ceil(len(s) / 2) - 1
        return s[k], k + 1, len(s)

    for label, rank_key, fk, kk, pre in (("primary (declared, for reference)", "instr_median", "fact_primary", None, None),
                                         ("A: init as B_setup", "instr_median", "fact_A", "kkt_A", "A_"),
                                         ("B: steady state, init excluded", "ss_per_run", "fact_B", "kkt_B", "B_")):
        print(f"== {label}")
        eng = True
        for pop in ("hs", "sd"):
            cs = [r for r in out if r["population"] == pop]
            c, rk, n = median(cs, rank_key)
            fac = c[fk]
            dn1 = fac is not None and fac >= 0.40 and c["rho"] is not None and c["rho"] <= 0.5 \
                and c["neg_agree"] is not None and c["neg_agree"] >= 0.99
            eng &= dn1
            extra = ""
            if pre:
                sh = {b: c[pre + b] for b in B}
                extra = " shares " + " ".join(f"{b}={sh[b]:.3f}" for b in B) + f" dominant={max(B, key=lambda b: sh[b])}"
                extra += f" DN2={'True' if c[kk] >= 0.25 else 'False'}"
            ge = sum(1 for r in cs if r[fk] is not None and r[fk] >= 0.40)
            print(f"  {pop}: median cell {c['problem']} (rank {rk}/{n} by {rank_key}={c[rank_key]}) "
                  f"(B_sym+B_fac)={fac:.4f} rho={c['rho']} neg_agree={c['neg_agree']} DN1_condition={dn1}{extra}")
            print(f"      init share of solver on this cell {c['init_share_of_solver']:.4f} "
                  f"(frozen rule filed it: B_fac {c['init_in_B_fac']:.4f}, B_eng {c['init_in_B_eng']:.4f}, "
                  f"B_setup {c['init_in_B_setup']:.4f}); population: (B_sym+B_fac)>=0.40 on {ge}/{len(cs)} cells")
        print(f"  IPM DN-1 under this reading: {eng}")


if __name__ == "__main__":
    main()
