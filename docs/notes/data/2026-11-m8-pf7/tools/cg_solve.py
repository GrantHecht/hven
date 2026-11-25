#!/usr/bin/env python3
"""Inclusive costs of the named functions in one callgrind file (callgrind --inclusive), instruction counts.
usage: cg_solve.py <file.cg> [...] -> markdown rows.  net = Solver::solve inclusive - __kmp_do_serial_initialize inclusive (the once-per-process OpenMP bring-up, PF-6's)."""
import re, subprocess, sys
PATS = [("solve", r"hven::(Sqp|Ipm)Solver::solve\(hven::Problem&"),
        ("kmp_init", r"__kmp_do_serial_initialize"),
        ("dense_solve", r"hven::linear::detail::DenseSession::solve|hven::linear::DenseSymmetricFactor::solve"),
        ("dense_factorize", r"hven::linear::detail::DenseSession::(factorize|analyze)\(|hven::linear::DenseSymmetricFactor::(try_)?factorize\("),
        ("nancheck", r"LAPACKE_(dsy|dge)_nancheck"),
        ("set_threads_local", r"MKL_Set_Num_Threads_Local"),
        ("malloc_free", r"malloc\.c:(malloc|free|calloc|realloc)$"),
        ("core_count", r"hven::detail::get_core_count"),
        ("ensure_init", r"hven::detail::ensure_solver_initialized"),
        ]
def one(path):
    out = subprocess.run(["callgrind_annotate", "--inclusive=yes", path], capture_output=True, text=True).stdout
    rows = {}
    tot = 0
    for line in out.splitlines():
        m = re.match(r"^\s*([\d,]+) \(\s*[\d.]+%\)\s+(.*)$", line)
        if not m: continue
        n = int(m.group(1).replace(",", "")); rest = m.group(2)
        if "PROGRAM TOTALS" in rest: tot = n; continue
        for k, p in PATS:
            fn = rest.split(" [")[0]
            fname = fn.split(":", 1)[-1] if ":" in fn else fn
            if re.search(p, fn) or re.search(p, fname):
                rows[k] = rows.get(k, 0) + n if k in ("nancheck","set_threads_local","malloc_free","dense_solve","dense_factorize") else max(rows.get(k, 0), n)
    return tot, rows
K = [k for k, _ in PATS]
print("| cell | total | " + " | ".join(K) + " | net=solve-kmp |"); print("|---|---:|" + "---:|"*(len(K)+1))
for p in sys.argv[1:]:
    t, r = one(p)
    print(f"| {p.split('/')[-1][:-3]} | {t:,} | " + " | ".join(f"{r.get(k,0):,}" for k in K) + f" | {r.get('solve',0)-r.get('kmp_init',0):,} |")
