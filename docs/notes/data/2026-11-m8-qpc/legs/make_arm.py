#!/usr/bin/env python3
"""QPC component arms (DECLARATION-QPC section 2): apply ONE component revert to an H tree, in place.

  make_arm.py <tree> c1only   the certificate's data half reverted to R's certified exit (C1 only)
  make_arm.py <tree> c2only   the converged-refinement arming reverted to R's dual_mu rung (C2 only)

The caller commits the result on branch qpc-c1 / qpc-c2 (never merged). Every edit's anchor must occur exactly once;
anything else stops with a non-zero exit, so a moved H is noticed rather than half-reverted.
"""
import sys

tree, which = sys.argv[1], sys.argv[2]
W = f"{tree}/src/qp/walk/walk_engine.cpp"
WH = f"{tree}/include/hven/detail/qp/walk/walk_engine.h"


def once(s, old, new, what):
    if s.count(old) != 1:
        sys.exit(f"ANCHOR {what}: {s.count(old)} occurrences, want 1")
    return s.replace(old, new)


s = open(W).read()
if which == "c1only":
    head = "                            // THE CERTIFICATE'S DATA HALF (walk_engine.h, section 5; M8 QPC):\n"
    tail = "                                status = QpStatus::kNumericalError;\n                            }\n"
    if s.count(head) != 1:
        sys.exit("ANCHOR c1only head")
    i = s.index(head)
    j = s.index(tail, i) + len(tail)
    s = s[:i] + ("                            // QPC-C1 ARM (C2 reverted, the round's component arm; never shipped as\n"
                 "                            // such): no data half, R's certified exit.\n"
                 "                            status = QpStatus::kOptimal;\n") + s[j:]
    open(W, "w").write(s)
elif which == "c2only":
    s = once(s, """        // footprint. Every EQP solve from here on converges its mu-free refinement (eqp_solve.h),
        // at the same dual_mu and on the same factor state; once per solve.
        if (released_row_out && in_working(ws, dropped.idx) &&
            eqp_refinement_ == EqpRefinement::kModel && eff_opts.dual_regularization > 0.0) {
            ++counters.converged_refinement_arms;
            eqp_refinement_ = EqpRefinement::kConverged;
        }""", """        // footprint. QPC-C2 ARM (C1 reverted, the round's component arm; never shipped as such):
        // R's dual_mu rung, one decade pair down and the factor state discarded, at most twice.
        if (released_row_out && in_working(ws, dropped.idx) &&
            counters.dual_regularization_decreases < detail::kMaxDualRegDecreases &&
            eff_opts.dual_regularization > 0.0) {
            ++counters.dual_regularization_decreases;
            eff_opts.dual_regularization *= detail::kDualRegDecreaseFactor;
            discard_factor_state();
        }""", "c2only arming")
    open(W, "w").write(s)
    h = open(WH).read()
    h = once(h, "constexpr double kDualRegDecreaseFactor = 1e-2;\n",
             "constexpr double kDualRegDecreaseFactor = 1e-2;\n"
             "constexpr Index kMaxDualRegDecreases = 2; // QPC-C2 arm: the release re-admission's rung restored\n",
             "c2only constant")
    open(WH, "w").write(h)
else:
    sys.exit("usage: make_arm.py <tree> c1only|c2only")
print("applied", which)
