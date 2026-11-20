#!/usr/bin/env python3
"""Sum callgrind call counts per callee name.  usage: pf3_callgrind.py <callgrind.out> -> JSON on stdout"""
import json, re, sys, collections
names = {}
calls = collections.Counter()
pending = None
for line in open(sys.argv[1], errors="replace"):
    m = re.match(r"^c?fn=\((\d+)\)(?: (.*))?$", line)
    if m:
        if m.group(2):
            names[m.group(1)] = m.group(2).strip()
        if line.startswith("cfn="):
            pending = m.group(1)
        continue
    if line.startswith("calls=") and pending is not None:
        calls[names.get(pending, "?" + pending)] += int(line.split("=")[1].split()[0])
        pending = None
KEYS = ["FactorSession::analyze", "FactorSession::factorize", "FactorSession::solve(", "FactorSession::solve_partial",
        "DenseSession::factorize", "DenseSession::solve", "QpEngine::run(", "SchurComplement::add_border", "SchurComplement::solve",
        "SchurComplement::append_border", "SymmetricFactor::solve_single", "SymmetricFactor::factorize(", "detail::solve_vec",
        "mkl_pds_lp64_pardiso", "LAPACKE_dsytrf_rook_work", "LAPACKE_dsytrs_rook_work", "LAPACKE_dsytrs_work", "LAPACKE_dsytrf_work"]
out = {k: sum(v for n, v in calls.items() if k in n) for k in KEYS}
print(json.dumps(out, indent=1))
