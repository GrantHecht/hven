#!/usr/bin/env python3
"""RD8 mutants for the padded twin. Mutates the WORKTREE source one at a time, builds the Release
hven_sqp_tests, runs the filter, restores (and touches) the file. Run under leg.sh.
usage: mutants.py [M#...]"""
import os
import subprocess
import sys

W = "/home/ghecht/Projects/hven/.claude/worktrees/agent-af791fbc982c20d3f"
C = W + "/.scratch/rd8"
FILTER = "PaddedKkt.*:QpEngineBorder.*:KktFactor.*"
MUT = {
    "M1": ("src/kkt/kkt_calls.cpp", "    return unpad(k.factor.inertia(), k.pad_rows.empty() ? 0 : k.pad_extra_pos);",
           "    return k.factor.inertia();"),
    "M2": ("src/kkt/kkt_calls.cpp", "        rp(k.pad_rows[static_cast<std::size_t>(r)]) = rhs(r);", "        rp(r) = rhs(r);"),
    "M3": ("include/hven/detail/kkt/kkt_assembly.h", "it.valueRef() = (r == c) ? 1.0 : 0.0;", "it.valueRef() = 0.0;"),
    "M4": ("include/hven/detail/kkt/kkt_assembly.h", "            if (pinned(r) || pinned(c)) {", "            if (pinned(r) && pinned(c)) {"),
    "M5": ("include/hven/detail/kkt/kkt_assembly.h", "    return opts.ws_algebra == WorkingSetLinearAlgebra::kSchurBorder &&",
           "    return false && opts.ws_algebra == WorkingSetLinearAlgebra::kSchurBorder &&"),
    "M6": ("src/kkt/kkt_calls.cpp", "    k.pad_rows.clear();\n    k.pad_extra_pos = 0;\n", ""),
    "M7": ("src/kkt/kkt_calls.cpp", "    outcome.inertia = unpad(outcome.inertia, extra_pos);\n", ""),
    "M8": ("include/hven/detail/qp/eqp_solve.h", "    if (detail::padded_assembly_applies(qp, ws, opts)) {", "    if (true) {"),
    "M9": ("src/qp/qp_engine.cpp", "    verdict = detail::inertia_verdict(detail::kkt_inertia(kkt), ws.num_free(),",
           "    verdict = detail::inertia_verdict(kkt.factor.inertia(), ws.num_free(),"),
    "M10": ("src/kkt/kkt_calls.cpp", "    if (reduced == 0 || extra_pos < 0 || reduced + extra_pos != K_pad.rows()) {",
            "    if (reduced == 0 || extra_pos < 0) {"),
}
which = sys.argv[1:] or sorted(MUT, key=lambda s: int(s[1:]))
for m in which:
    path, a, b = MUT[m]
    full = os.path.join(W, path)
    orig = open(full).read()
    if orig.count(a) != 1:
        print(f"MUTANT {m} ANCHOR x{orig.count(a)}", flush=True)
        continue
    open(full, "w").write(orig.replace(a, b))
    try:
        rb = subprocess.run(["bash", C + "/build.sh", "release", "hven_sqp_tests"], capture_output=True, text=True)
        if rb.returncode != 0:
            print(f"MUTANT {m} KILLED (build) {rb.stdout[-300:]}", flush=True)
            continue
        rt = subprocess.run([C + "/build-release/tests/sqp/hven_sqp_tests", f"--gtest_filter={FILTER}"],
                            capture_output=True, text=True, env=dict(os.environ, MKL_NUM_THREADS="1"))
        failed = [l for l in rt.stdout.splitlines() if l.startswith("[  FAILED  ]") and "listed below" not in l]
        print(f"MUTANT {m} {'KILLED' if rt.returncode != 0 else 'SURVIVED'} rc={rt.returncode} {failed[:4]}", flush=True)
    finally:
        open(full, "w").write(orig)
        os.utime(full, None)
rb = subprocess.run(["bash", C + "/build.sh", "release", "hven_sqp_tests"], capture_output=True, text=True)
print("RESTORED_BUILD rc", rb.returncode, flush=True)
