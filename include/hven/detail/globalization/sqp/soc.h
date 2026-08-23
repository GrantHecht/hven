// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// soc.h -- the second-order-correction subproblem construction of the SQP
// driver. The SECOND-ORDER CORRECTION and MODEL EVALUATION notes cited below
// live at the top of drivers/sqp_driver.h (which includes this header); the
// body of the function declared here lives in
// src/globalization/sqp/soc_elastic_restoration.cpp, together with elastic.h's
// and restoration.h's definitions -- one TU for the three recovery tiers,
// because all three are reached from one place in SqpDriver::solve_impl.
//
// NOT SELF-CONTAINED BY DESIGN: `NlpEval` is defined in drivers/sqp_driver.h,
// which includes this header at the exact point the carved code stood -- after
// NlpEval's definition, before the driver class -- so no header cycle exists
// (this header never includes sqp_driver.h). Any other includer must have
// NlpEval complete first.

#include <stdexcept>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <hven/core/types.h>
#include <hven/detail/qp/qp_problem.h>

namespace hven::solvers {

/// @brief Builds the second-order-correction shifted QP: the whole of the
/// rhs-shift computation, factored out so it can be tested away from the
/// driver's loop and away from any engine.
///
/// Returns a COPY of `qp` with only `be`/`bi` touched -- H, g, Ae, Ai, lower,
/// upper are untouched, which is what lets the re-solve reuse qp_engine.h's
/// hot-start K0.
///
/// @param ev,ev_trial Model evaluations at the CURRENT iterate x and at the
///   REJECTED trial point x + p (both already in hand; this function makes no
///   model call of its own).
/// @param p The rejected step (qs.x).
/// @param ineq_active The rejected solve's own QpSolution::ineq_active (size
///   qp.mi()).
///
/// Shift rules:
///     be_soc = -cE(x) - (cE(x+p) - cE(x) - Je p)          (equalities)
///     bi_soc(j) = -cI(x)(j) - (cI(x+p)(j) - cI(x)(j) - Ji p(j)),
///         for j ACTIVE in `ineq_active` ONLY -- every other row's bi is
///         COPIED FROM qp.bi UNCHANGED. An inactive row's own linearization
///         was never the thing that failed, and shifting it risks
///         manufacturing a NEW active row the correction was never meant to
///         touch. (inequalities)
QpProblem build_soc_subproblem(const QpProblem &qp, const NlpEval &ev, const NlpEval &ev_trial,
                               const Vec &p, const std::vector<bool> &ineq_active);

} // namespace hven::solvers
