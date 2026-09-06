// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// tests/sqp/support/ipqp_test_support.h -- test-support only, NOT part of the public library
// surface. Shared IPQP counter predicates; AssertionResult, not a direct assert, so the checker
// stays mutation-pinnable by test_ipqp_counters.cpp. Requirement: `.superpowers/w1-t2-report.md`.
// It also holds the M6 W2 tier fixtures three suites share (W5 T5).

#include <limits>

#include <Eigen/SparseCore>
#include <gtest/gtest.h>

#include <hven/core/solver_counters.h>
#include <hven/core/types.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/drivers/sqp_types.h>
#include <hven/drivers/trace.h>
#include <hven/model/nlp_model.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers::test_support {

/// Asserts the five-way escape census sums to `ipqp_escapes` (plan section 7
/// note b: escape-COUNT only -- the dropped stall-reason sub-counters play no
/// part in this invariant).
inline ::testing::AssertionResult assert_ipqp_escape_census_sums(const IpqpCounters &c) {
    const Index sum = c.ipqp_escape_budget + c.ipqp_escape_stall + c.ipqp_escape_indefinite +
                      c.ipqp_escape_numerical + c.ipqp_escape_infeasible_suspect;
    if (sum != c.ipqp_escapes) {
        return ::testing::AssertionFailure()
               << "assert_ipqp_escape_census_sums: census sum " << sum
               << " (budget=" << c.ipqp_escape_budget << " stall=" << c.ipqp_escape_stall
               << " indefinite=" << c.ipqp_escape_indefinite
               << " numerical=" << c.ipqp_escape_numerical
               << " infeasible_suspect=" << c.ipqp_escape_infeasible_suspect << ") != ipqp_escapes "
               << c.ipqp_escapes;
    }
    return ::testing::AssertionSuccess();
}

/// Asserts the section 2.3 ROUTING PARTITION IS CLOSED: every subproblem the
/// routing chain disposed of went to exactly one FIRST destination, and the
/// three `ipqp_to_*` counters say which.
///
/// @param c            the solve's folded IPQP counters.
/// @param tier_entries subproblems that ENTERED the tier (neither retired-past nor declined);
///        the counters cannot express it, so the caller supplies it. Driver fixtures derive it
///        as `ipqp_symbolic_analyses + ipqp_pattern_verifies` -- plan section 7 note (k).
///
/// FOUR IDENTITIES, and the first is the closed one:
///
///   `to_refine + escape_indefinite + (to_walk - declined_pinned)`
///       `== tier_entries`
///       -- one FIRST destination each: the tier-3 refinement, the SSN warm grade
///          (saddle-suspect), or the cold walk. A decline also lands in `to_walk`
///          without the tier having run, so it is subtracted.
///   `to_refine == refine_accepted + refine_refused`
///       -- the refinement's two outcomes, so `to_refine` counts hand-offs.
///   `to_ssn == refine_refused + escape_indefinite`
///       -- item 4's TWO feeders; a refusal reaches SSN SECOND, so `to_ssn` is not in the sum.
///   `to_walk >= declined_pinned`.
///
/// The closed form needs `to_refine`: the two-term version is FALSE on the converged-`kBudget`
/// row. Derivation: `.superpowers/w1-t6-report.md` section R2.
inline ::testing::AssertionResult assert_ipqp_routing_partition(const IpqpCounters &c,
                                                                Index tier_entries) {
    if (c.ipqp_to_refine != c.ipqp_refine_accepted + c.ipqp_refine_refused) {
        return ::testing::AssertionFailure()
               << "assert_ipqp_routing_partition: ipqp_to_refine " << c.ipqp_to_refine
               << " != refine_accepted " << c.ipqp_refine_accepted << " + refine_refused "
               << c.ipqp_refine_refused;
    }
    if (c.ipqp_to_ssn != c.ipqp_refine_refused + c.ipqp_escape_indefinite) {
        return ::testing::AssertionFailure()
               << "assert_ipqp_routing_partition: ipqp_to_ssn " << c.ipqp_to_ssn
               << " != refine_refused " << c.ipqp_refine_refused << " + escape_indefinite "
               << c.ipqp_escape_indefinite;
    }
    if (c.ipqp_to_walk < c.ipqp_declined_pinned) {
        return ::testing::AssertionFailure()
               << "assert_ipqp_routing_partition: ipqp_to_walk " << c.ipqp_to_walk
               << " is below ipqp_declined_pinned " << c.ipqp_declined_pinned
               << ", but every decline routes to the walk";
    }
    const Index first_destinations =
        c.ipqp_to_refine + c.ipqp_escape_indefinite + (c.ipqp_to_walk - c.ipqp_declined_pinned);
    if (first_destinations != tier_entries) {
        return ::testing::AssertionFailure()
               << "assert_ipqp_routing_partition: first destinations " << first_destinations
               << " (to_refine=" << c.ipqp_to_refine
               << " escape_indefinite=" << c.ipqp_escape_indefinite << " to_walk=" << c.ipqp_to_walk
               << " declined_pinned=" << c.ipqp_declined_pinned << ") != tier entries "
               << tier_entries;
    }
    return ::testing::AssertionSuccess();
}

// ===========================================================================
// The M6 W2 tier fixtures, shared by the driver, dispatch and trace suites.
// ===========================================================================

// A subproblem the tier DECLINES pre-solve, by construction rather than by luck:
// variable 0's declared bounds are EQUAL, so the effective box has a zero-width
// pair at index 0 whatever the radius is, which is exactly the domain gate's rule.
class PinnedVariableModel : public NlpModel {
  public:
    // `pin` false widens variable 0's box and nothing else, so the SAME model
    // exercises the declining and the non-declining case -- the mutation the
    // decline pin needs to be non-vacuous.
    explicit PinnedVariableModel(bool pin) : pin_(pin) {}

    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        return 0.5 * ((x(0) - 1.0) * (x(0) - 1.0) + (x(1) - 2.0) * (x(1) - 2.0));
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << x(0) - 1.0, x(1) - 2.0;
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        // x0 + x1 - 3 <= 0, in the tree's own "ci(x) <= 0" convention.
        Vec c(1);
        c << x(0) + x(1) - 3.0;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 1.0;
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return pin_ ? pinned_lower() : free_lower(); }
    const Vec &upper() const override { return pin_ ? pinned_upper() : free_upper(); }
    Vec start_point() const override { return Vec::Constant(2, 0.25); }

  private:
    static const Vec &pinned_lower() {
        static const Vec v = (Vec(2) << 0.5, -5.0).finished();
        return v;
    }
    static const Vec &pinned_upper() {
        static const Vec v = (Vec(2) << 0.5, 5.0).finished();
        return v;
    }
    static const Vec &free_lower() {
        static const Vec v = (Vec(2) << -5.0, -5.0).finished();
        return v;
    }
    static const Vec &free_upper() {
        static const Vec v = (Vec(2) << 5.0, 5.0).finished();
        return v;
    }
    bool pin_;
};

/// An equality the BOX cannot reach: `x0 + x1 = 5` on `[-b, b]^2`. The tier escapes it as an
/// infeasible suspect from iterates pressed against the upper bounds, so its least-infeasible
/// point carries a working set the elastic solution shares -- both bounds tight.
inline QpProblem w2_box_blocked_qp(double b) {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 2.0;
    qp.H.insert(1, 1) = 2.0;
    qp.H.makeCompressed();
    qp.g = Vec::Zero(2);
    qp.Ae = SpMatRM(1, 2);
    qp.Ae.insert(0, 0) = 1.0;
    qp.Ae.insert(0, 1) = 1.0;
    qp.Ae.makeCompressed();
    qp.be = Vec(1);
    qp.be << 5.0;
    qp.Ai = SpMatRM(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -b);
    qp.upper = Vec::Constant(2, b);
    return qp;
}

/// Two antiparallel EQUALITY rows -- plan section 6's F-1 shape: `x0 + x1 = 1` against
/// `-x0 - x1 = 1`, inconsistent at every point of a box that reaches both.
inline QpProblem w2_antiparallel_eq_qp() {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 2.0;
    qp.H.insert(1, 1) = 2.0;
    qp.H.makeCompressed();
    qp.g = Vec::Zero(2);
    qp.Ae = SpMatRM(2, 2);
    qp.Ae.insert(0, 0) = 1.0;
    qp.Ae.insert(0, 1) = 1.0;
    qp.Ae.insert(1, 0) = -1.0;
    qp.Ae.insert(1, 1) = -1.0;
    qp.Ae.makeCompressed();
    qp.be = Vec(2);
    qp.be << 1.0, 1.0;
    qp.Ai = SpMatRM(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

/// The tier's own escape on `qp`, with its section 6.3 evidence block filled. `radius` is the
/// WINDOW the solve ran in: +inf disables it, a finite one clamps every bound to `c +- radius`
/// and so gives zl/zu to faces the original problem may not have at all. EXACTLY ONE `solve()`
/// per call, and `sink` -- attached for that one solve -- is how a test can see that.
inline IpqpResult w2_escaped(const QpProblem &qp,
                             double radius = std::numeric_limits<double>::infinity(),
                             TraceSink *sink = nullptr) {
    QpOptions qopts;
    qopts.tr_radius = radius;
    IpqpEngine tier(qopts);
    if (sink != nullptr) {
        tier.attach_trace(sink);
    }
    return tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
}

} // namespace hven::solvers::test_support
