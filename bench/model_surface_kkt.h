#pragma once

// bench/model_surface_kkt.h — Task 4 (M4-Task5 plan, "the SQP driver's
// consumption of the Level 2 aggregate contract" --
// .superpowers/sdd/2026-08-21-m4-task5-sqp-level2-consumption/task-4-brief.md).
// An ENGINE-INDEPENDENT KKT residual scorer, computed ONLY off
// NlpAggregate::evaluate_candidate_first_order and AggregateDeclaration's own
// bound record -- no engine, no driver, no QP, no linear-algebra backend
// anywhere in the picture.
//
// THIS IS BENCH-LOCAL, NOT PART OF THE LIBRARY SURFACE -- same standing as
// bench/bench_cli.h and bench/corpus_cells.h: nothing in include/ or src/
// depends on it, and it is never installed.
//
// ENGINE-INDEPENDENT BY CONSTRUCTION, AND THE CHECK IS THIS FILE'S OWN
// INCLUDE LIST. This header may include ONLY model/ headers, core/ headers,
// Eigen, fmt, and the standard library -- never qp/, drivers/, kkt/, or any
// interior header. The compile is the proof: a later edit that pulls an
// engine header in here breaks that proof the moment it is added, and no
// runtime check could catch what a missing #include already prevents.
//
// WHY THIS EXISTS. bench/corpus_cells.h's model-level KKT gate (W1-W5,
// Task 6 of the Phase-7 corpus work) already recomputes stationarity,
// primal feasibility and complementarity from the model at the returned
// point -- but it does so through tests/sqp/support/nlp_kkt_check.h's
// `self_check_kkt`, which reads a raw NlpModel. This header answers the same
// three questions from the Level 2 aggregate surface instead, so a consumer
// that never held an NlpModel (a partitioned, multi-piece provider; a future
// consumer with no NlpModel behind it at all) still gets an independent
// checker, and so that the census hook below can ask "does the model surface
// agree with the engine's own recorded residuals at the point it returned?"
// without re-deriving the engine's answer through the engine.
//
// THE NORMS AND SCALE CONVENTIONS ARE self_check_kkt's, DELIBERATELY MIRRORED
// rather than re-derived, so a scorer-vs-recorded comparison is apples to
// apples:
//
//   stationarity    = max_i |grad f(x) + Je(x)^T lambda_e + Ji(x)^T lambda_i
//                            - z|_i        over i NOT declared-fixed
//   primal          = max(0, worst bound violation, worst cI(x), |cE(x)|_inf)
//   complementarity = max(max_j |lambda_i(j) * cI(j)(x)|,
//                          max_i |z(i) * dist(x(i), bounds(i))|)
//   dual_scale      = max(1, |lambda_e|_inf, |lambda_i|_inf, |z|_inf)
//   x_scale         = max(1, |x|_inf)
//
// which is bench/corpus_cells.h's own CorpusRow::{kkt_stationarity,
// kkt_primal, kkt_complementarity, dual_scale, x_scale} and W3's divisors,
// to the arithmetic -- see that header's kkt_gate_verdict for the rule these
// feed. dual_sign is measured by self_check_kkt but is NOT part of the W2
// gate (a telemetry-only split ruled in that header); this scorer follows the
// same split and does not compute it.
//
// ONE DELIBERATE DIVERGENCE FROM self_check_kkt, OWED TO THIS SURFACE'S OWN
// CONTRACT: model/nlp_aggregate.h's evaluate_candidate_first_order documents
// what "A SCORER OWES" over this surface -- a declared-fixed variable
// (materialized lower == upper) carries no degree of freedom, so its
// stationarity row is not a stationarity condition and must be EXCLUDED from
// the max-norm rather than read from whatever a provider leaves there.
// self_check_kkt has no such exclusion because it has never been run against
// a model with a declared-fixed variable; this scorer states the rule the
// contract requires regardless. The exclusion is stationarity-only: a fixed
// variable's dist-to-bound is exactly 0 by construction (x sits at both
// bounds at once), so its complementarity term is 0 whatever z holds there,
// and needs no exclusion of its own; likewise its bound-violation terms in
// primal are ordinary per-coordinate checks that apply to every variable, fixed
// or not.

#include <algorithm>
#include <cmath>

#include <Eigen/Core>

#include <fmt/format.h>

#include "hven/core/types.h"
#include "hven/model/aggregate_declaration.h"
#include "hven/model/candidate_point.h"
#include "hven/model/nlp_aggregate.h"

namespace hven::solvers {

/// The three W2-gated residuals plus the two W3 scale denominators, all in
/// bench/corpus_cells.h's own convention -- see this file's banner. Deliberately
/// NOT `dual_sign`: the gate this struct feeds does not gate it either (W2).
struct ModelSurfaceKktResiduals {
    double stationarity_ = 0.0;
    double complementarity_ = 0.0;
    double primal_ = 0.0;
    double dual_scale_ = 1.0; // max(1, |lambda_e|inf, |lambda_i|inf, |z|inf)
    double x_scale_ = 1.0;    // max(1, |x|inf)
};

/// Scores (x, lambda_e, lambda_i, z) against `aggregate`'s declared problem,
/// using ONLY evaluate_candidate_first_order's output and declaration() bounds
/// -- no engine state, no provider internals, and no knowledge of which
/// treatment a provider was configured with (nlp_aggregate.h's own claim about
/// this surface).
///
/// z is the model-implied bound multiplier, full length n, in the seam
/// stationarity convention grad f + Je^T lambda_e + Ji^T lambda_i - z = 0
/// (model/candidate_point.h's banner). A CALLER WITH NO z PASSES A ZERO VECTOR
/// OF LENGTH n -- there is no "absent" spelling here, unlike the CandidatePoint
/// multiplier blocks, because a scorer with no z to compare against would be
/// scoring the raw Lagrangian gradient's norm rather than a stationarity
/// residual, and silently reinterpreting the request that way is exactly the
/// kind of substitution this project's error rules refuse to make silently.
///
/// OFF THE HOT PATH BY DESIGN: this call allocates its own CandidateFirstOrder
/// storage (five vectors/one scalar sized off the declaration) on every
/// invocation and never caches it. A census hook calling this once per corpus
/// cell is exactly the intended use; a per-minor caller is not.
///
/// @throws std::invalid_argument if x/lambda_e/lambda_i/z are not sized to the
///         declaration (x and z at n; lambda_e at me; lambda_i at mi) --
///         x/lambda_e/lambda_i are checked by evaluate_candidate_first_order's
///         own entry (model/nlp_aggregate.h), z is checked here since it is not
///         part of that contract's own vocabulary.
inline ModelSurfaceKktResiduals model_surface_kkt_residuals(NlpAggregate &aggregate, const Vec &x,
                                                            const Vec &lambda_e,
                                                            const Vec &lambda_i, const Vec &z) {
    const AggregateDeclaration &declared = aggregate.declaration();
    const Eigen::Index n = declared.primal_vars_;
    const Eigen::Index me = declared.equality_rows_;
    const Eigen::Index mi = declared.inequality_rows_;

    if (z.size() != n) {
        throw std::invalid_argument(
            fmt::format("model_surface_kkt_residuals: z has {0} rows, but the aggregate declares "
                        "{1} primal variables; a caller with no z to compare against passes a zero "
                        "vector of length {1}, not an empty one",
                        z.size(), n));
    }

    // The candidate-surface call: objective value and both residual blocks are
    // written, but this scorer reads only the two blocks the residuals below
    // need (cE for `primal`, cI for both `primal` and `complementarity`) plus
    // the two gradient blocks. ASSIGNED, not accumulated -- see
    // evaluate_candidate_first_order's own contract text.
    double objective = 0.0;
    Vec equality_residuals = Vec::Zero(me);
    Vec inequality_residuals = Vec::Zero(mi);
    Vec objective_gradient = Vec::Zero(n);
    Vec constraint_adjoint_gradient = Vec::Zero(n);

    CandidatePoint point{x, lambda_e, lambda_i, /*objective_scale_=*/1.0};
    CandidateValues values{objective, equality_residuals, inequality_residuals};
    CandidateFirstOrder first_order{values, objective_gradient, constraint_adjoint_gradient};
    aggregate.evaluate_candidate_first_order(point, first_order);

    // The declared-fixed exclusion set, computable from declaration data alone
    // (nlp_aggregate.h's own "WHAT A SCORER OWES" note).
    const std::vector<VariableBound> bounds = declared.materialize_variable_bounds();

    ModelSurfaceKktResiduals out;

    // stationarity: max_i |grad f + adjoint_grad - z|_i, over non-fixed i.
    for (Eigen::Index i = 0; i < n; ++i) {
        const VariableBound &bound = bounds[static_cast<std::size_t>(i)];
        if (bound.lower_ == bound.upper_) {
            continue; // declared-fixed: no stationarity condition at this row.
        }
        const double g = objective_gradient(i) + constraint_adjoint_gradient(i) - z(i);
        out.stationarity_ = std::max(out.stationarity_, std::abs(g));
    }

    // primal: worst constraint/bound violation, over EVERY coordinate/row --
    // feasibility applies to a fixed variable exactly as it does to a free one.
    if (me > 0) {
        out.primal_ = std::max(out.primal_, equality_residuals.lpNorm<Eigen::Infinity>());
    }
    for (Eigen::Index j = 0; j < mi; ++j) {
        out.primal_ = std::max(out.primal_, std::max(0.0, inequality_residuals(j)));
    }
    for (Eigen::Index i = 0; i < n; ++i) {
        const VariableBound &bound = bounds[static_cast<std::size_t>(i)];
        out.primal_ = std::max(out.primal_, std::max(0.0, bound.lower_ - x(i)));
        out.primal_ = std::max(out.primal_, std::max(0.0, x(i) - bound.upper_));
    }

    // complementarity: inequality rows, plus bound complementarity over every
    // coordinate with at least one finite side (self_check_kkt's own skip for
    // a variable free on both sides, where dist is +/-inf and z is otherwise
    // required to be exactly 0 -- see that header's note; a fixed coordinate's
    // dist is exactly 0 here regardless, so it needs no separate exclusion).
    for (Eigen::Index j = 0; j < mi; ++j) {
        out.complementarity_ =
            std::max(out.complementarity_, std::abs(lambda_i(j) * inequality_residuals(j)));
    }
    for (Eigen::Index i = 0; i < n; ++i) {
        const VariableBound &bound = bounds[static_cast<std::size_t>(i)];
        if (std::isfinite(bound.lower_) || std::isfinite(bound.upper_)) {
            const double dist = std::min(x(i) - bound.lower_, bound.upper_ - x(i));
            out.complementarity_ = std::max(out.complementarity_, std::abs(z(i) * dist));
        }
    }

    // The two W3 scale denominators, corpus_cells.h's own convention.
    if (lambda_e.size() > 0) {
        out.dual_scale_ = std::max(out.dual_scale_, lambda_e.lpNorm<Eigen::Infinity>());
    }
    if (lambda_i.size() > 0) {
        out.dual_scale_ = std::max(out.dual_scale_, lambda_i.lpNorm<Eigen::Infinity>());
    }
    if (z.size() > 0) {
        out.dual_scale_ = std::max(out.dual_scale_, z.lpNorm<Eigen::Infinity>());
    }
    out.x_scale_ = x.size() > 0 ? std::max(1.0, x.lpNorm<Eigen::Infinity>()) : 1.0;

    return out;
}

} // namespace hven::solvers
