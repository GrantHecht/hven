#!/usr/bin/env python3
"""PF-2d: apply (or restore) one declared mutation. usage: [MUT_TREE=<dir>] mutate.py <id> | restore.
Ids are declared in ../DECLARATION-PF-2d.md (and -r1). Each anchor must occur the stated number of times.
MUT_TREE defaults to the enclosing git worktree; the withheld L2 arm (branch pf2d-l2) is mutated in its own tree."""
import os
import subprocess
import sys

WS = "src/qp/walk/ws_independence.cpp"
WH = "include/hven/detail/qp/walk/ws_independence.h"
WE = "src/qp/walk/walk_engine.cpp"
GATE = "    if (target > 0.0 && !t.screen && res_norm > target) {\n"

# id: (file, old, new, expected occurrences)
MUT = {
    # L1: first-solve reuse
    "R1": (WS, "    if (first != nullptr && first->has_first) {\n", "    if (false && first != nullptr && first->has_first) {\n", 1),  # reuse disabled
    "R2": (WS, "        res_norm = first->first_res_norm;\n", "        res_norm = 0.0;\n", 1),  # reused residual norm zeroed
    "R3": (WS, "            t.first_y = std::move(y0);\n", "            t.first_y = y;\n", 1),  # stored solve taken after refinement
    "R4": (WS, "            t.has_first = true;\n", "            t.has_first = false;\n", 1),  # never held
    "R5": (WE, "detail::certificate_coefficients(qp, ws, view, a, opts, &t);", "detail::certificate_coefficients(qp, ws, view, a, opts, nullptr);", 1),  # the caller does not pass it
    # L3: first-solve decision
    "T1": (WH, "constexpr double kDepDecideFirstRho = 1e-2;", "constexpr double kDepDecideFirstRho = 1.0;", 1),
    "T2": (WH, "constexpr double kDepDecideFirstRho = 1e-2;", "constexpr double kDepDecideFirstRho = 1e-3;", 1),
    "T3": (WS, GATE, "    if (target > 0.0 && res_norm > target) {\n", 1),  # screen tests not excluded
    "T4": (WH, "return rho1 >= kDepDecideFirstRho;", "return rho1 > kDepDecideFirstRho;", 1),
    "T5": (WS, GATE, "    if (!t.screen && res_norm > target) {\n", 1),  # skip also in certificate mode
    "T6": (WS, "        t.stall_admit = !converged;\n", "        t.stall_admit = !converged && !decided_at_first;\n", 1),
    # L2 (withheld arm, tree pf2d-l2): the band
    "B1": (WH, "constexpr double kDepRereadBandHi = 1e-4;", "constexpr double kDepRereadBandHi = 1e-3;", 1),
    "B2": (WH, "constexpr double kDepRereadBandHi = 1e-4;", "constexpr double kDepRereadBandHi = 1e-5;", 1),
    "B3": (WH, "constexpr double kDepRereadBandLo = kDepTol;", "constexpr double kDepRereadBandLo = 1e-5;", 1),
    "B4": (WH, "rho <= kDepRereadBandHi;", "rho < kDepRereadBandHi;", 1),
    "B5": (WH, "return rho > kDepRereadBandLo &&", "return rho >= kDepRereadBandLo &&", 1),
    "B6": (WE, "        detail::in_reread_band(t.rho)) {", "        t.rho < detail::kDepUndecidable) {", 1),  # the re-read ignores the band
    "B7": (WS, "if (keep_first && converged && in_reread_band(t.rho)) {", "if (keep_first && converged && t.rho < kDepUndecidable) {", 1),
}

ROOT = os.environ.get("MUT_TREE") or subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()


def main():
    arg = sys.argv[1]
    if arg == "restore":
        files = sorted({v[0] for v in MUT.values()})
        subprocess.check_call(["git", "-C", ROOT, "checkout", "--"] + files)
        print("RESTORED")
        return 0
    f, old, new, want = MUT[arg]
    p = f"{ROOT}/{f}"
    s = open(p).read()
    n = s.count(old)
    if n != want:
        print(f"MUTATION_DOES_NOT_APPLY {arg}: anchor occurs {n} times (want {want}) in {f}")
        return 3
    open(p, "w").write(s.replace(old, new))
    print(f"APPLIED {arg} to {f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
