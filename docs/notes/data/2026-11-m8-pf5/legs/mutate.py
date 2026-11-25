#!/usr/bin/env python3
# PF-5 (../DECLARATION-PF-5.md): the PF-4 r2 mutate.py's driver (../../2026-11-m8-pf4/r2/legs/mutate.py) with PF-5's own
# mutation table.
"""PF-5: apply (or restore) one declared mutation of the PF-5 code. usage: mutate.py <id> | restore.
Ids and dispositions are declared in ../DECLARATION-PF-5.md; each anchor must occur exactly once in its file."""
import subprocess
import sys

K = "src/kkt/kkt_factor.cpp"
W = "src/qp/walk/walk_engine.cpp"
N = "src/qp/ssn/ssn_engine.cpp"
SQP = "src/sqp/sqp_solver.cpp"

MUT = {
    # the key: a hit on the hash, order and nnz alone (no exact pattern comparison)
    "X1": (K, "    return std::equal(e.outer.begin(), e.outer.end(), K.outerIndexPtr()) &&\n           std::equal(e.inner.begin(), e.inner.end(), K.innerIndexPtr());",
              "    return true;"),
    # the discard: a swapped-in session whose refactorization fails is kept
    "D1": (K, "        if (in_ && k_.factor.session_id() == in_->session_id && k_.factor.epoch() == in_->epoch) {",
              "        if (false && in_ && k_.factor.session_id() == in_->session_id && k_.factor.epoch() == in_->epoch) {"),
    # free at QP-solve exit: the walk's K0 factor / the SSN engine's factor keep their lists
    "F1": (W, "                detail::release_retained_analyses(border->kkt);\n", ""),
    "F2": (N, "            detail::release_retained_analyses(kkt);\n", ""),
    # the MRU cap: one too many, one too few, and the eviction taking the most recently used entry
    "M1": (K, "    if (k.retained.size() > kRetainedAnalyses) {", "    if (k.retained.size() > kRetainedAnalyses + 1) {"),
    "M2": (K, "    if (k.retained.size() > kRetainedAnalyses) {", "    if (k.retained.size() >= kRetainedAnalyses) {"),
    "M3": (K, "    k.retained.insert(k.retained.begin(),", "    k.retained.insert(k.retained.end(),"),
    # the counter carry across a swap
    "C1": (K, "             std::vector<SpMatRM::StorageIndex> &inner) noexcept {\n    accumulate(k.departed_counters, k.factor.counters(), +1);\n",
              "             std::vector<SpMatRM::StorageIndex> &inner) noexcept {\n"),
    # the tally: only the innermost scope sees a hit; the walk's QpCounters never get it; the SQP's never get it
    "T1": (K, "    for (AnalysisReuseTally *t = t_reuse_innermost; t != nullptr; t = t->outer_) {",
              "    for (AnalysisReuseTally *t = t_reuse_innermost; t != nullptr; t = nullptr) {"),
    "Q1": (W, "    s.counters.symbolic_analysis_reuses = reuses.reuses();\n", ""),
    "S1": (SQP, "    out.counters.symbolic_analysis_reuses = reuse_tally.reuses();\n", ""),
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
