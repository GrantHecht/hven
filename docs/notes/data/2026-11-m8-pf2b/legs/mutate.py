#!/usr/bin/env python3
"""PF-2b: apply (or restore) one declared mutation of the solve-tally code. usage: mutate.py <id> | restore.
Ids and dispositions are declared in ../DECLARATION-PF-2b.md. Each anchor must occur the stated number of times."""
import subprocess
import sys

T = "src/linear/analysis_tally.cpp"
MKL = "src/linear/symmetric_factor_mkl.cpp"
DS = "src/linear/dense_session.cpp"
WS = "src/qp/walk/ws_independence.cpp"
EQ = "include/hven/detail/qp/walk/eqp_solve.h"
BE = "include/hven/detail/qp/walk/bordered_eqp.h"
SQP = "src/sqp/sqp_solver.cpp"
SCOPE = "    const hven::linear::SolveContextScope solve_context(hven::linear::SolveContext::%s);\n"
SCOPE_WS = "    const hven::linear::SolveContextScope solve_context(\n        hven::linear::SolveContext::kDetector); // PF-2b\n"

# id: (file, old, new, expected occurrences)
MUT = {
    "M1": (T, "        ++t->solves_[c];\n", "", 1),                       # solves never counted
    "M2": (T, "        t->rhs_columns_[c] += rhs_columns;\n", "", 1),    # columns never counted
    "M3": (T, "        t->refinement_steps_[c] += refinement_steps;\n", "", 1),  # refinement never counted
    "M4": (T, "    for (SolveTally *t = t_solve_innermost; t != nullptr; t = t->outer_) {",
              "    for (SolveTally *t = t_solve_innermost; t != nullptr; t = nullptr) {", 1),  # only the innermost tally
    "M5": (T, "SolveContextScope::~SolveContextScope() { t_solve_context = prev_; }",
              "SolveContextScope::~SolveContextScope() {}", 1),         # a scope never restores
    "S1": (WS, SCOPE_WS, "", 6),                                        # no detector scope
    "S2": (EQ, SCOPE % "kPolish", "", 1),                               # the eliminated polish unlabelled
    "S3": (BE, SCOPE % "kPolish", "", 1),                               # the bordered polish unlabelled
    "S4": (BE, SCOPE % "kBorderRefine", "", 1),                         # the bordered refinement unlabelled
    "A1": (MKL, "    const Index refined = static_cast<Index>(session.refinement_iters());\n    SolveTally::record(static_cast<Index>(X.cols()), refined);\n", "", 1),  # MKL multi-RHS solve unrecorded
    "A2": (MKL, "    SolveTally::record(1, static_cast<Index>(session.refinement_iters()));\n", "", 1),  # MKL single solve unrecorded
    "A3": (MKL, "    ++counters_.partial_solve_count;\n    SolveTally::record(1, 0);\n", "    ++counters_.partial_solve_count;\n", 1),  # MKL partial solve unrecorded
    "A4": (DS, "    SolveTally::record(static_cast<Index>(X.cols()), 0);\n", "", 1),  # dense multi-RHS unrecorded
    "A5": (DS, "    SolveTally::record(1, 0);\n", "", 1),                 # dense single unrecorded
}
for i, n in enumerate(["ordinary", "detector", "polish", "border_refine"]):
    pass
FAM = [("backend_solves_", "t.solves"), ("backend_solve_rhs_", "t.rhs_columns"), ("backend_solve_refine_", "t.refinement_steps")]
CTX = [("ordinary", "kOrdinary"), ("detector", "kDetector"), ("polish", "kPolish"), ("border_refine", "kBorderRefine")]
for fi, (pre, acc) in enumerate(FAM):
    for name, k in CTX:
        line = f"        c.{pre}{name} = {acc}(SolveContext::{k});\n"
        MUT[f"C{fi * 4 + CTX.index((name, k)) + 1}"] = (SQP, line, "", 1)   # that counter never reaches the result

ROOT = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()


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
