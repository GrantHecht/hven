#!/usr/bin/env python3
"""Self-cost classes of one callgrind file (callgrind_annotate flat output), instruction counts.
usage: cg_classes.py <file.cg> [...]  -> one markdown row per file.  Classes by function/dso (self cost, no tree):
loader (ld-linux), kmp (libiomp5 incl. init), mkl-kernel (mkl_* / DSYTRS...), lapacke-wrapper (LAPACKE_*), malloc (libc malloc/free/new/delete),
libc-stdio (printf/scanf/stdio, the harness's own output), eigen/hven (the rest of the binary: hven:: , Eigen::, harness models), libc-other."""
import re, subprocess, sys, collections
def classes(path):
    out = subprocess.run(["callgrind_annotate", path], capture_output=True, text=True).stdout
    c = collections.Counter(); tot = 0
    for line in out.splitlines():
        m = re.match(r"^\s*([\d,]+) \(\s*[\d.]+%\)\s+(.*)$", line)
        if not m: continue
        n = int(m.group(1).replace(",", "")); rest = m.group(2)
        if "PROGRAM TOTALS" in rest: tot = n; continue
        if "ld-linux" in rest: k = "loader"
        elif "libiomp5" in rest: k = "kmp"
        elif re.search(r":(LAPACKE_|DSYTR|DSYTF|DSYCON|dsytr|dsytf)", rest): k = "lapacke_wrapper"
        elif re.search(r":(mkl_|MKL_|DSYTRS|dsytrs)", rest) or "mkl" in rest.split("[")[0].lower(): k = "mkl"
        elif re.search(r"malloc|free\b|operator (new|delete)|new_op|del_op|_int_|calloc|realloc", rest): k = "malloc"
        elif re.search(r"stdio|printf|scanf|getc|fgets|iogetline|libio|strops|vfscanf", rest): k = "libc_stdio"
        elif "libc.so" in rest or "libstdc++" in rest or "libm.so" in rest: k = "libc_other"
        else: k = "hven_eigen_harness"
        c[k] += n
    return tot, c
KEYS = ["loader","kmp","mkl","lapacke_wrapper","malloc","libc_stdio","libc_other","hven_eigen_harness"]
print("| cell | total Ir | " + " | ".join(KEYS) + " |"); print("|---|---:|" + "---:|"*len(KEYS))
for p in sys.argv[1:]:
    tot, c = classes(p)
    print(f"| {p.split('/')[-1][:-3]} | {tot:,} | " + " | ".join(f"{c[k]:,}" for k in KEYS) + " |")
