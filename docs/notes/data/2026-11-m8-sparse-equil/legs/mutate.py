#!/usr/bin/env python3
"""Apply (or restore) one declared mutation. usage: mutate.py <id> | restore.
M1 no-refactor:    a perturbed sparse factorization is not refactored (sparse_equilibrated_response returns the outcome as is).
M2 no-D-on-solve:  solve_vec does not apply D (the refactored factor answers D K D's system unscaled).
M3 gate-removed:   KktFactor::gated() ignores sparse_equilibrated (no D-metric gate, no keep-while-falling refinement).
M4 no-count:       sparse_equilibrated_refactors is never incremented.
M5 marker-kept:    the next factorization does not retire sparse_equilibrated.
M6 raw-refactor:   the 'equilibrated' refactor factors K itself (D applied on solve only)."""
import subprocess
import sys

CPP = "src/kkt/kkt_calls.cpp"
HDR = "include/hven/detail/kkt/kkt_calls.h"
MUT = {
    "M1": (CPP, "    if (!perturbed_sparse(outcome)) {\n        return outcome;", "    if (true) {\n        return outcome;"),
    "M2": (CPP, "    if (k.sparse_equilibrated) {\n        const Vec &d = k.dense_scale;", "    if (false && k.sparse_equilibrated) {\n        const Vec &d = k.dense_scale;"),
    "M3": (HDR, "bool gated() const { return dense_shifted || sparse_equilibrated; }", "bool gated() const { return dense_shifted; }"),
    "M4": (CPP, "    ++k.sparse_equilibrated_refactors;\n", ""),
    "M5": (CPP, "    k.dense_shifted = false;\n    k.sparse_equilibrated = false;\n", "    k.dense_shifted = false;\n"),
    "M6": (CPP, "    const SpMatRM KD = congruent_scaled(K, d);", "    const SpMatRM KD = K;"),
}
arg = sys.argv[1]
if arg == "restore":
    subprocess.check_call(["git", "checkout", "--", CPP, HDR])
    print("RESTORED")
    sys.exit(0)
path, old, new = MUT[arg]
s = open(path).read()
if s.count(old) != 1:
    sys.exit(f"MUTATION_{arg}_DOES_NOT_APPLY (count {s.count(old)})")
open(path, "w").write(s.replace(old, new))
print(f"APPLIED {arg}")
