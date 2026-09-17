// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// M7 W0 T4 -- THE DISPROVING VEHICLE.
//
// WHY THIS EXISTS. `ipqp_suspicion_disproved` counts fallback entries whose rung
// A CLOSED the slacks -- a suspicion the tier raised and the elastic relaxation
// then refuted. The counter has never been moved by a DRIVER fixture. M6 W2's
// two unit tests reach it by handing `certified_feasibility_fallback` a
// HAND-BUILT `IpqpInfeasibilityEvidence` block (test_sqp_solver.cpp's
// `P2ClosedSlacksDisproveTheSuspicionAndCarryTheMultipliers`,
// `P4bTheCeilingDeclineIsGONEUnderThePlacementBound` and
// `ThePartitionOverFallbackEntriesIsCOMPLETEAndSumsToTheFiredOnes`), never by
// letting the tier itself
// escape on a subproblem the fallback then solves; M6 W1 T7's calibration reads
// 0 disproved over the whole HS corpus, and every frozen F7 artifact in
// docs/notes/data/ reads 0 in the column. So the M6 ledger's Q-S3 (a) ruling --
// "the K = 3 retirement tally is charged on a DISPROVAL too, because a disproved
// suspicion is evidence that the tier's DETECTOR is unsuited to this problem" --
// is a decision with no pin behind it. That is T8's registered rider, and this
// header is the vehicle it needs.
//
// WHAT IT IS, AND WHERE THE SHAPE COMES FROM. M7 W0 T3 measured the elastic rung
// on the interior-point tier and found the tier's section 6.3 detector
// FALSE-DECLINING 3 of 5 rungs of an elastic QP that is FEASIBLE BY CONSTRUCTION
// (docs/notes/2026-09-m7-w0-t3-design.md section (ix)). That is the shape this
// pin needs -- a consistent subproblem the detector suspects -- so the vehicle
// carries the ELASTIC QP'S OWN SHAPE into a PRIMARY subproblem:
//
//   * `ne` "row" variables with curvature 2 and a finite box, the elastic QP's
//     original block;
//   * `ns` ZERO-CURVATURE penalty columns priced `rho * sigma` linearly and
//     boxed `[0, cap]` -- structurally empty rows and columns of H, exactly as
//     `build_elastic_subproblem` leaves the slack block
//     (src/globalization/sqp/soc_elastic_restoration.cpp);
//   * `ns` equality rows `x_(k mod ne) + sigma * s_k = target`, the elastic
//     form's `A_e` augmented by its scaled slack column;
//   * `nc` "pulled" variables with curvature 2 and gradient `-pull`, whose only
//     job is to make the solve TRUST-REGION-LIMITED so that a LATER major still
//     enters the tier after the first major's disproval. Without them the cell
//     solves in one major and there is no next entry to observe.
//
// THE ROWS ARE CONSISTENT BY CONSTRUCTION: `x_k = target, s = 0` satisfies every
// one of them, and with `|target| < tr_init <= box` that witness is inside every
// window the driver can build. So a `kInfeasibleSuspect` escape on this
// subproblem is a FALSE POSITIVE by construction, and rung A's elastic copy
// closes its slacks on it -- which is what `ipqp_suspicion_disproved` counts.
//
// WHY THE DETECTOR FIRES, in the terms of its own two conjuncts
// (src/qp/ipqp_engine.cpp's `primal_flat_on_floor` and the growth test): the
// huge linear price on the zero-curvature columns drives the dual norm past
// `kSsnDualGrowthFactor = 1e4` within the stall window -- MEASURED 1e6 on the
// shipped cell -- while the primal residual is still above the feasibility
// target and NOT improving (measured `primal_improvement < 0`, i.e. worse). Both
// conjuncts hold, so the tier escapes `kInfeasibleSuspect`. No test seam, no
// fault injection, no `HVEN_TESTING` point: the detector fires on its own terms.
//
// NOT A CORPUS CELL. Deliberately: no bench cell, no CSV column, no schema move.

#include <cmath>
#include <stdexcept>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/core/types.h>
#include <hven/detail/qp/qp_problem.h>

#include "qp_as_nlp_model.h"

namespace hven::solvers::test_support {

/// @brief The disproving vehicle's parameters. The defaults ARE the shipped
///        cell -- every pin and the T4 measurement use them unchanged.
struct DisprovedEntryCell {
    Index ne = 2;         ///< Curved "row" variables.
    Index ns = 2;         ///< Zero-curvature penalty columns (and equality rows).
    Index nc = 2;         ///< Pulled variables, the reason the solve takes majors.
    double rho = 1.0e4;   ///< The penalty columns' price, with `sigma`.
    double sigma = 1.0e2; ///< The scaled slack column's coefficient.
    double target = 0.1;  ///< The equality rows' right-hand side.
    double pull = 1.0e1;  ///< The pulled variables' linear gradient, negated.
    double box = 1.0e1;   ///< The symmetric box on the curved and pulled blocks.
    double cap = 1.0e3;   ///< The penalty columns' upper bound.
};

/// @brief The vehicle as a `QpProblem`, in the driver's own subproblem shape.
///
/// Validated at the boundary on the same terms library code is (CLAUDE.md
/// section 4) -- a fixture that silently built a degenerate QP would make every
/// pin below vacuous rather than failing.
///
/// @throws std::invalid_argument if any block count is < 1, if `sigma`, `box`,
///         `cap` or `rho` is not > 0, or if `|target| >= box` (which would put
///         the consistent witness outside the box and make the suspicion TRUE).
inline QpProblem disproved_entry_qp(const DisprovedEntryCell &c = DisprovedEntryCell{}) {
    if (c.ne < 1 || c.ns < 1 || c.nc < 1) {
        throw std::invalid_argument(fmt::format(
            "disproved_entry_qp: ne/ns/nc are {}/{}/{}, each must be >= 1", c.ne, c.ns, c.nc));
    }
    if (!(c.sigma > 0.0) || !(c.box > 0.0) || !(c.cap > 0.0) || !(c.rho > 0.0)) {
        throw std::invalid_argument(
            fmt::format("disproved_entry_qp: rho/sigma/box/cap are {}/{}/{}/{}, each must be > 0",
                        c.rho, c.sigma, c.box, c.cap));
    }
    if (!(std::abs(c.target) < c.box)) {
        throw std::invalid_argument(fmt::format(
            "disproved_entry_qp: |target| ({}) must be < box ({}) -- the whole point of this "
            "vehicle is that the equality rows are CONSISTENT INSIDE THE BOX, so the tier's "
            "suspicion is a false positive and rung A can close the slacks",
            c.target, c.box));
    }
    const Index n = c.ne + c.ns + c.nc;
    QpProblem qp;
    qp.H = SpMatRM(n, n);
    for (Index i = 0; i < c.ne; ++i) {
        qp.H.insert(i, i) = 2.0;
    }
    // The `ns` penalty columns are DELIBERATELY ABSENT from H -- structurally
    // empty rows and columns, which is what the elastic subproblem's slack block
    // is and half of why the tier struggles on this shape.
    for (Index i = 0; i < c.nc; ++i) {
        qp.H.insert(c.ne + c.ns + i, c.ne + c.ns + i) = 2.0;
    }
    qp.H.makeCompressed();

    qp.g = Vec::Zero(n);
    qp.g.segment(c.ne, c.ns) = Vec::Constant(c.ns, c.rho * c.sigma);
    qp.g.tail(c.nc) = Vec::Constant(c.nc, -c.pull);

    qp.Ae = SpMatRM(c.ns, n);
    for (Index k = 0; k < c.ns; ++k) {
        qp.Ae.insert(k, k % c.ne) = 1.0;
        qp.Ae.insert(k, c.ne + k) = c.sigma;
    }
    qp.Ae.makeCompressed();
    qp.be = Vec::Constant(c.ns, c.target);

    qp.Ai = SpMatRM(0, n);
    qp.bi = Vec(0);

    qp.lower = Vec::Zero(n);
    qp.upper = Vec::Zero(n);
    qp.lower.head(c.ne) = Vec::Constant(c.ne, -c.box);
    qp.upper.head(c.ne) = Vec::Constant(c.ne, c.box);
    qp.lower.segment(c.ne, c.ns) = Vec::Zero(c.ns);
    qp.upper.segment(c.ne, c.ns) = Vec::Constant(c.ns, c.cap);
    qp.lower.tail(c.nc) = Vec::Constant(c.nc, -c.box);
    qp.upper.tail(c.nc) = Vec::Constant(c.nc, c.box);
    return qp;
}

/// @brief The vehicle as an `NlpModel`, started at the origin.
///
/// `QpAsNlpModel` is exact by construction, so the driver's subproblem at the
/// start point IS `disproved_entry_qp()` shifted to `p` -- the tier sees the
/// shape above and nothing else.
inline QpAsNlpModel disproved_entry_model(const DisprovedEntryCell &c = DisprovedEntryCell{}) {
    const QpProblem qp = disproved_entry_qp(c);
    return QpAsNlpModel(qp, Vec::Zero(qp.n()));
}

} // namespace hven::solvers::test_support
