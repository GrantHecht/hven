// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// warm_start.h -- the interior-point CROSSOVER (`from_interior_point`).
//
// M6 W5 T8.5 PROMOTED THE SQP'S NATIVE WARM-START STRUCT OUT OF `detail/`: it
// lives, with its full contract, in `warmstart/sqp_warm_start.h` as
// `SqpWarmStart` -- the labelled SQP-only native identity beside the shared
// `WarmStartData` payload (design 2.4). This header KEPT the old spelling
// `WarmStart` as an alias so the ~70 call sites T8.5 did not touch went on
// compiling; M6 W5 T8.10 swept those call sites and REMOVED the alias.
//
// THE HEADER ITSELF STAYS, though the T8.5 entry, the plan and the T8.10 brief
// all said T8.10 would delete it. That premise was that nothing but the alias
// would be left here. Everything below is the crossover -- always this header's
// other half -- and group 2 is mapped renames only, so deleting the file would
// have RELOCATED functional code. Settler ruling, 2026-09-13 (T8.10 fix 1).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

#include <hven/core/start_level.h>
#include <hven/detail/qp/working_set.h>
#include <hven/qp/qp_types.h>
#include <hven/warmstart/sqp_warm_start.h>

namespace hven::solvers {

// THE INTERIOR-POINT CROSSOVER -- `from_interior_point`.
//
// Builds a SqpWarmStart from an INTERIOR-POINT-STYLE primal-dual iterate: the
// Knitro crossover pattern (an IP method runs to near-KKT, then hands off to
// an active-set method for the final polish). Unlike mesh_transfer.h's
// MeshTransfer (which maps a SqpWarmStart born from THIS driver's own solve onto
// a different mesh), this function's INPUT never went through this driver at
// all: there is no QpEngine working set, no funnel/trust-region history and,
// critically, no MODEL this function could hash -- so `structure_hash` is
// unconditionally 0 and `hot` is unconditionally null.
//
// SIGN CONVENTION -- READ BEFORE WIRING A CALLER. This project's own
// convention (this header's SIGN CONVENTIONS note above) is cI(x) <= 0,
// lambda_i >= 0 at an active row, and a SINGLE signed bound dual `z` (>= 0 at
// an active LOWER bound, <= 0 at an active UPPER bound). Interior-point
// codes conventionally report bound multipliers SEPARATELY, both
// non-negative (z_lower >= 0 for the constraint lower <= x, z_upper >= 0 for
// x <= upper), and a general-inequality SLACK s_i >= 0 with cI_i(x) + s_i = 0
// -- i.e. s_i = -cI_i(x). THIS FUNCTION TAKES `slack_i` AS THE cI(x) VALUES
// THEMSELVES (this project's own convention, matching every other slot in
// this header), NOT the IP solver's own non-negative s_i: a caller wiring an
// IPM- or Knitro-style s_i through must negate it first
// (slack_i = -s_i). Getting this backwards would flip every activity
// verdict below without producing any error. The two bound duals are
// converted to this project's single `z` internally as `z = z_lower -
// z_upper` -- a caller never has to perform that conversion itself.
//
// ACTIVITY INFERENCE (the Knitro-crossover pattern). An inequality row is
// judged ACTIVE iff BOTH
//   lambda_i(j) > dual_tol                       -- the multiplier is
//                                                   genuinely positive
//   lambda_i(j) >= kIpActivityFactor
//                  * max(mu_hat, ||lambda_i||inf * activity_rel_tol)
//                                                -- and the price is LIVE at
//                                                   the hand-off's OWN dual
//                                                   scale
// where `mu_hat` is the hand-off's own barrier level, estimated from the data
// it carries as the mean of lambda_i(j)*|slack_i(j)| over the PRICED rows
// (lambda_i(j) > 0), and `||lambda_i||inf` is the largest price among those
// same rows. A bound is judged active at its lower/upper side by the exact
// same test with lambda_i replaced by z_lower/z_upper and the row's residual
// replaced by the GAP (x(i) - lower(i), or upper(i) - x(i)); each side
// estimates its own `mu_hat` and its own dual norm.
//
// WHY A DUAL-SIDE TEST: the slack-side rule it replaced ("residual small
// relative to its own multiplier", with an absolute max(1, .) floor) degrades
// to "empty" on mesh-refinement sequences, where collocation multipliers are
// O(h) while an IP method's residual mu/lambda GROWS as prices shrink. The
// dual-side rule recovers the same fixtures' active sets across all mesh
// sizes. The slack is not ignored -- it is read AGGREGATELY as `mu_hat`,
// which also closes the extreme-dual trap: a hand-off pricing a row far
// above its own residual raises the threshold against itself instead of
// certifying the row ACTIVE.
//
// AMBIGUOUS rows -- a price at or below the noise floor, the degenerate case
// an IP method's numerical noise produces close to convergence -- are left
// FREE rather than guessed either way: the destination QP re-derives the
// correct working set from a real linearization, while a
// wrongly-forced-active row can only cost that QP work backing off a face
// the solution is not on. The `lambda_i(j) > dual_tol` conjunct also leaves
// a WRONG-SIGN dual free rather than certified active.
//
// THREE DELIBERATE LIMITS, stated so a reader does not have to find them by
// measurement:
//   * `dual_tol` (1e-6) is absolute. It answers "is this multiplier zero?"
//     against the driver's stationarity noise floor; a relative version
//     would certify degenerate tiny-price/tiny-slack rows. Past the point
//     where the rule's own relative term falls below it, `dual_tol` is the
//     sole false-active guard next to a quantity that scales -- so a caller
//     crossing over at a fine mesh should hand over at a barrier level mu no
//     looser than dual_tol * min|cI| over the strictly slack rows, or treat
//     the hint as slightly wrong (measured false-active fractions stay near
//     1 % even then).
//   * A FINE ENOUGH MESH MAKES THE WEAKLY-ACTIVE AND NOISE POPULATIONS
//     OVERLAP, and no dual-side rule survives that; the rule takes the
//     conservative side (missed rows, never invented ones).
//   * A LOOSE hand-off (mu large against its own dual scale) is a regime
//     where identification is not available from the hand-off at all, and
//     can be actively wrong rather than merely empty.
//
// A kFixed variable (lower(i) == upper(i)) is reported +1, the same
// arbitrary-but-consistent choice SqpWarmStart::bound_active's own note makes
// for a solve-derived object.
//
// NO FUNNEL/TRUST-REGION/REGULARIZATION STATE: funnel_width, tr_radius,
// primal_delta and dual_mu all stay at their "never populated" -1 sentinels
// -- an IP method's own trajectory carries none of these in a form this
// project's globalization understands, so the destination solve uses
// SqpOptions' plain defaults for all four rather than being handed a
// borrowed value with no justified scale.
//
// THE CROSSOVER'S VALUE: `structure_hash == 0` means the object resolves
// StartLevel::kSeeded on an ordinary 3-arg solve() -- never kWarm or kHot,
// and never authorized to reuse a factorization of a matrix nobody here has
// seen (that was always the point of the sentinel). What kSeeded changes
// concretely: `lambda_e`, `lambda_i` and the activity hint REACH the solve
// (the 3-arg call is no longer equivalent to the 2-arg one);
// `crossover.x` is taken FROM THE OBJECT, not from the caller's `x0`
// argument; the ingest-time clear applies, on this path a genuine
// improvement (interior-point duals are strictly positive on every row); and
// the seeded `lambda_i >= 0` clamp applies, the one defence this producer
// specifically needs (see this header's producer-inventory note).
//
// The activity rule's safety factor `nu`, NOT an option: it is one decade of
// margin over the larger of the two scales the rule compares against (the
// same "an order above the producer's own tolerance" step sqp_solver.h's
// kSeededDualClampTol derivation takes), fixed here rather than exposed
// because a caller has no data with which to choose it that the rule has not
// already read for itself. Its product with the default `activity_rel_tol`
// is 1e-3.
inline constexpr double kIpActivityFactor = 10.0;

struct IpCrossoverOptions {
    // eps_rel: how far below the LARGEST price a live price may sit, as a
    // fraction. 1e-4 against kIpActivityFactor = 10 puts the floor three
    // decades below ||lambda_i||inf -- measured on F7 at N = 100 as the
    // widest separation available between genuinely-active and
    // barrier-noise prices on that family.
    double activity_rel_tol = 1e-4;
    // Absolute "is this multiplier zero?" floor -- see this header's note
    // above on why this one stays absolute.
    double dual_tol = 1e-6;
};

namespace detail {

// The activity threshold of the rule documented above, for ONE dual/residual
// population (the inequality rows, or one side of the variable bounds):
//
//     kIpActivityFactor * max(mu_hat, ||dual||inf * eps_rel)
//
// with `mu_hat` the mean of dual(j)*|residual(j)| over the PRICED entries
// (dual(j) > 0) and `||dual||inf` the largest price among them. An empty
// population yields 0, which leaves the caller's `dual_tol` conjunct as the
// only gate.
//
// PRICED ENTRIES ONLY, on both statistics: a zero or negative price is the
// absence of a barrier-level statement, and averaging it in would drag
// `mu_hat` toward zero in proportion to how many rows were left unpriced,
// making the threshold depend on model SIZE rather than hand-off quality.
// The `> 0.0` form also skips a NaN PRICE. A NaN RESIDUAL against a real
// price does reach the mean and makes the threshold NaN, whereupon every
// `>=` below is false and the hint comes out EMPTY -- the safe direction.
//
// THE RESIDUAL IS A TEMPLATE PARAMETER so a caller can pass an Eigen
// EXPRESSION (`x - lower`) rather than a materialized vector: this function
// is reachable at n = 1e6 through crossover cells of scale corpora, and two
// n-sized temporaries per call is a cost with nothing to show for it.
template <typename Residual>
inline double ip_activity_threshold(const Vec &dual, const Residual &residual, double eps_rel) {
    double complementarity = 0.0;
    double dual_inf = 0.0;
    Index priced = 0;
    for (Index j = 0; j < dual.size(); ++j) {
        if (!(dual(j) > 0.0)) {
            continue;
        }
        complementarity += dual(j) * std::abs(residual(j));
        dual_inf = std::max(dual_inf, dual(j));
        ++priced;
    }
    if (priced == 0) {
        return 0.0;
    }
    const double mu_hat = complementarity / static_cast<double>(priced);
    return kIpActivityFactor * std::max(mu_hat, dual_inf * eps_rel);
}

} // namespace detail

/// @brief Builds a SqpWarmStart from an interior-point-style primal-dual point.
///
/// See this header's own note immediately above for the sign convention
/// `slack_i` must already be in (cI(x) VALUES, NOT an IP solver's own
/// non-negative slack), the activity-inference rule, and what this object's
/// fields do and do not carry. Dimensions: n = x.size(), me =
/// lambda_e.size(), mi = lambda_i.size(); slack_i must match mi; z_lower,
/// z_upper, lower and upper must each match n.
/// @throws std::invalid_argument On any size mismatch (offending sizes in
/// the message).
///
/// The body lives in src/warmstart/warm_start.cpp (runs at most once per
/// solve; never inlined at its call sites). detail::ip_activity_threshold
/// above STAYS HERE: it is a template on purpose, so this function can pass
/// it an Eigen expression rather than materialize two n-sized temporaries.
SqpWarmStart from_interior_point(const Vec &x, const Vec &lambda_e, const Vec &lambda_i,
                                 const Vec &slack_i, const Vec &z_lower, const Vec &z_upper,
                                 const Vec &lower, const Vec &upper,
                                 const IpCrossoverOptions &opts = {});

} // namespace hven::solvers
