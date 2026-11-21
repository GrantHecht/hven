#!/usr/bin/env python3
"""PF-4: apply (or restore) one declared mutation of the PF-4 code. usage: mutate.py <id> | restore.
Ids and dispositions are declared in ../DECLARATION-PF-4.md; each anchor must occur exactly once in its file."""
import subprocess
import sys

S = "include/hven/detail/kkt/schur_complement.h"
K = "src/kkt/kkt_factor.cpp"
T = "src/linear/analysis_tally.cpp"
AK = "include/hven/detail/linear/analysis_key.h"
MKL = "src/linear/symmetric_factor_mkl.cpp"
SQP = "src/sqp/sqp_solver.cpp"

MUT = {
    # the batch
    "B1": (S, "                detail::solve_mat(kkt_, rhs, sol);\n                batched = true;",
              "                detail::solve_mat(kkt_, rhs, sol);\n                batched = false;"),          # never use the batch solve
    "B2": (S, "        if (solve_failure) {\n            std::rethrow_exception(solve_failure);\n        }",
              "        (void)solve_failure;"),                                                            # the solve throw swallowed
    "B3": (S, "                const auto sj = static_cast<std::size_t>(j);\n                Vec k0inv_v = batched",
              "                const auto sj = static_cast<std::size_t>(k - 1 - j);\n                Vec k0inv_v = batched"),  # lands in reverse order
    "B4": (S, "Vec k0inv_v = batched ? Vec(sol.col(j)) :", "Vec k0inv_v = batched ? Vec(sol.col(0)) :"),  # every border gets column 0
    "B5": (S, "            pending_d_.push_back(d);\n            // C is owed a factorization from here on: nothing may read the\n            // old one (end_batch() refactorizes even if nothing lands).\n            singular_ = false;\n            evidence_.reset();\n",
              "            pending_d_.push_back(d);\n            // C is owed a factorization from here on: nothing may read the\n            // old one (end_batch() refactorizes even if nothing lands).\n"),  # stale evidence readable inside a batch
    "B6": (S, "            evidence_.reset();\n            factor_pending_ = true;\n            return;",
              "            evidence_.reset();\n            return;"),                                          # a recorded add owes no factorization (a batch that lands nothing leaves C unfactorized)
    "B7": (S, "    Index dim() const { return static_cast<Index>(v_.size() + pending_v_.size()); }",
              "    Index dim() const { return static_cast<Index>(v_.size()); }"),                          # dim() blind to recorded adds
    # the tally
    "T2": (MKL, "    AnalysisTally::record(detail::analysis_key(pattern_hash_, A, opts_));\n", ""),             # the adapter records nothing
    "T3": (T, "            if (!t->seen_->keys.insert(key).second) {", "            if (false && !t->seen_->keys.insert(key).second) {"),  # repeats never counted
    "T4": (AK, "    mix(static_cast<std::uint64_t>(o.ordering));\n", ""),                                   # ordering left out of the key
    "T5": (T, "    for (AnalysisTally *t = t_innermost; t != nullptr; t = t->outer_) {", "    for (AnalysisTally *t = t_innermost; t != nullptr; t = nullptr) {"),  # only the innermost tally sees an analysis
    "T6": (SQP, "    out.counters.backend_symbolic_analyses = analysis_tally.analyses();\n", ""),            # the count never reaches the result
    # solve_mat's retake arms (recorded: no fixture lands a border batch on retake numerics)
    "R1": (K, "        for (Index j = 0; j < x.cols(); ++j) {\n            x.col(j).array() *= k.retake_scale.array();\n        }\n", ""),
    "R2": (K, "    case PlatformRetake::kMatched:\n        k.matched_session->solve(rhs, x);\n        break;\n    }\n}\n\nhven::linear::InertiaEvidence",
              "    case PlatformRetake::kMatched:\n        k.factor.solve(rhs, x);\n        break;\n    }\n}\n\nhven::linear::InertiaEvidence"),
}

ROOT = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()


def main():
    arg = sys.argv[1]
    if arg == "restore":
        files = sorted({f for f, _, _ in MUT.values()})
        subprocess.check_call(["git", "-C", ROOT, "checkout", "--"] + files)
        print("RESTORED")
        return 0
    f, old, new = MUT[arg]
    p = f"{ROOT}/{f}"
    s = open(p).read()
    n = s.count(old)
    if n != 1:
        print(f"MUTATION_DOES_NOT_APPLY {arg}: anchor occurs {n} times in {f}")
        return 3
    open(p, "w").write(s.replace(old, new))
    print(f"APPLIED {arg} to {f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
