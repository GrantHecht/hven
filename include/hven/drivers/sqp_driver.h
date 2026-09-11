// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The SQP major loop: a trust-region globalization over nlp_model.h's
///        NLP, with an elastic reformulation for infeasible subproblems, one
///        second-order correction per rejected trial, and a restoration phase.
/// @see docs/notes/2026-09-header-prose-archive.md §sqp_driver.h

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/core/ledger.h>
#include <hven/detail/globalization/sqp/globalization.h>
#include <hven/detail/globalization/sqp/trust_region.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/qp/ssn_engine.h>
#include <hven/detail/qp/working_set.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/sqp_types.h>
#include <hven/drivers/trace.h>
#include <hven/model/nlp_model.h>
#include <hven/model/nlp_model_aggregate.h>
#include <hven/qp/qp_types.h>
#include <hven/warmstart/ipm_polish_extension.h>
#include <hven/warmstart/seeding.h>
#include <hven/warmstart/sqp_warm_start.h>
#include <hven/warmstart/warm_start_data.h>

namespace hven::solvers {

/// @brief Forward-declared, not included (M6 W5 T8.7 fix1, the lane's M4): only
/// two `unique_ptr` members name these types here, and including
/// `drivers/console_trace_sink.h` pulled `fmt/color.h` and both sink classes
/// into EVERY consumer of this header -- FORTY-NINE translation units in this
/// tree, all but a handful of which never name a console. The interior-point solver's header
/// already forward-declares them for the same reason. A `unique_ptr` member of an incomplete type
/// needs the owner's destructor (and any `reset()`) where the type is complete, which is why
/// `~SqpDriver` is declared below and DEFINED in src/drivers/sqp_driver.cpp; both sink classes are
/// non-copyable and non-movable, so nothing else in this header instantiates their deleters.
class ConsoleTraceSink;
class FanOutTraceSink;

/// @brief One model evaluation at one point: the five quantities a major
///        iteration reads, taken together so nothing is evaluated twice.
struct NlpEval {
    /// @brief Objective value at the evaluation point.
    double f = 0.0;
    /// @brief Objective gradient, equality- and inequality-constraint values;
    ///        sizes n(), me() and mi().
    Vec grad, ce, ci;
    /// @brief Equality and inequality constraint Jacobians; me() x n() and mi() x n().
    Eigen::SparseMatrix<double, Eigen::RowMajor> Je, Ji;

    /// @brief False if any of f/grad/cE/cI is NaN or infinite. Checked
    ///        explicitly: a running maximum would swallow a NaN silently.
    bool all_finite = true;
};

/// @brief Evaluates the model once at `x`: f, grad f, cE, cI, Je and Ji.
/// @param model The problem.
/// @param x     The point; must have size model.n().
/// @return The five quantities at `x`. Empty constraint blocks are skipped, so
///         the per-call count is exactly 2 + 2*[me>0] + 2*[mi>0].
/// @throws std::invalid_argument if `x` is mis-sized or any callback return's
///         shape contradicts the model's declared dimensions.
NlpEval eval_nlp(const NlpModel &model, const Vec &x);

/// @brief Evaluates f, cE and cI at `x` only -- no derivatives.
/// @param model The problem.
/// @param x     The point; must have size model.n().
/// @return An NlpEval whose grad/Je/Ji are correctly sized ZEROS -- n, me x n
///         and mi x n -- rather than empty, and whose `all_finite` covers
///         f/cE/cI only. Use it only where the derivatives go unread.
/// @throws std::invalid_argument if `x` is mis-sized, or if eval_values
///         returns a cE or cI whose size contradicts model.me()/model.mi().
NlpEval eval_nlp_values(const NlpModel &model, const Vec &x);

/// @brief Fills in the derivatives an eval_nlp_values() bundle is missing,
///        leaving its f/cE/cI untouched.
/// @param model The problem.
/// @param x     The point `ev` was taken at.
/// @param ev    The bundle to complete, in place.
/// @throws std::invalid_argument if eval_grad returns a size other than
///         model.n(), or eval_jac_e/eval_jac_i return a matrix whose shape
///         contradicts model.me()/model.mi() x model.n().
void upgrade_to_full(const NlpModel &model, const Vec &x, NlpEval &ev);

/// @brief The globalization violation measure h(x): the l1 norm of the
///        equality residuals plus the positive parts of the inequality rows.
/// @param ev An evaluation at the point of interest; bounds are NOT included.
/// @return h >= 0, or NaN if any constraint value is NaN -- non-finite values
///         propagate deliberately rather than being swallowed here.
double constraint_violation_l1(const NlpEval &ev);

/// @brief The KKT measurement of one NLP iterate.
struct SqpKkt {
    /// @brief inf-norm stationarity residual.
    double stationarity = 0.0;
    /// @brief inf-norm constraint violation, bounds included.
    double feasibility = 0.0;
    /// @brief inf-norm complementarity residual.
    double complementarity = 0.0;
    /// @brief grad f + Je^T lambda_e + Ji^T lambda_i (bound term NOT subtracted).
    Vec grad_lag;
    /// @brief Model-implied bound multipliers: grad_lag at an active bound, else 0.
    Vec z;

    /// @brief False if x, anything the model returned at x, or the resulting
    ///        grad_lag is NaN or infinite. A caller must branch on this before
    ///        reading the residuals as numbers.
    bool finite = true;

    /// @brief The single scalar the convergence and contraction tests read.
    /// @return max(stationarity, feasibility); NaN when `finite` is false.
    double residual() const { return std::max(stationarity, feasibility); }
};

/// @brief The SHARED declared diagnostics of one point, taken at the moment the
///        engine still holds the evaluation they are computed from.
///
/// THE WHOLE REASON THIS TYPE EXISTS is that the four shared diagnostics
/// (drivers/solve_result.h) must be computed from a STASHED evaluation and
/// never from a fresh one: a fresh evaluation at the exit would move
/// `evals_full` on every solve and break the identity proofs W5 rests on. Some
/// of this driver's exits return a point whose evaluation has already gone out
/// of scope by the time `finish` runs -- the budget-best iterate, and the
/// restored point, whose evaluation is a local of `enter_restoration`. Each such
/// site fills one of these while the evaluation is live.
///
/// It carries the four SCALARS plus copies of `ce`/`ci`, not an `NlpEval`: the
/// scalars are all the diagnostics need, and the two vectors are what the
/// result reports beside them. No sparse block is copied.
struct DeclaredDiagnosticsStash {
    /// False when no finite evaluation of the point existed. Every field below
    /// is then meaningless and the result reports NaN and empty blocks.
    bool measured = false;
    /// TRUE when the DUAL half -- `d.stationarity` and `d.complementarity` --
    /// was measured at prices this stash also names below (M6 W5 T8.4 fix1).
    /// False on an exit that cleared its multipliers AFTER the measurement:
    /// the primal half still describes the point, the dual half describes
    /// prices nothing will report.
    bool duals_measured = false;
    /// The four, over the declared problem.
    DeclaredDiagnostics d;
    /// The declared constraint residuals at the same point.
    Vec ce, ci;
    /// THE PRICES THE DUAL HALF WAS MEASURED AT (M6 W5 T8.4 fix1). `finish`
    /// compares them against the vectors it ends up EXPORTING -- the R6 sign
    /// sweep and the W0.2 scale map both run after a stash is taken -- and
    /// reports the dual half only when they are the same prices. Empty when
    /// `duals_measured` is false.
    Vec lambda_i_at, z_at;
};

/// @brief Fills a stash from an evaluation and the KKT measurement taken at the
///        same point.
/// @param ev       The evaluation at @p x.
/// @param kkt      The measurement at @p x, for its `grad_lag` and `z`.
/// @param x        The point.
/// @param lambda_i Inequality multipliers at @p x.
/// @param lo       Declared lower bounds.
/// @param up       Declared upper bounds.
/// @param duals_describe_the_measurement False where the caller has ALREADY
///                 replaced the multipliers `kkt.grad_lag` was measured at --
///                 the failed-restoration-evaluation arm clears them and
///                 returns zeros. The primal half (feasibility_e,
///                 feasibility_i, ce, ci) is still measured and still
///                 reported; the dual half is NaN and no prices are recorded.
/// @return The stash; `measured` false when @p kkt is not finite.
DeclaredDiagnosticsStash stash_declared_diagnostics(const NlpEval &ev, const SqpKkt &kkt,
                                                    const Vec &x, const Vec &lambda_i,
                                                    const Vec &lo, const Vec &up,
                                                    bool duals_describe_the_measurement = true);

namespace detail {

/// @brief The arithmetic of evaluate_kkt, over the five quantities it reads off
///        its first argument. Evaluates nothing: every model quantity arrives
///        in @p ev, and both public entries forward here.
/// @param n         Variable count.
/// @param me        Equality-row count.
/// @param mi        Inequality-row count.
/// @param lo        Lower bounds.
/// @param up        Upper bounds.
/// @param ev        An evaluation at @p x.
/// @param x         The iterate.
/// @param lambda_e  Equality multipliers.
/// @param lambda_i  Inequality multipliers.
/// @param bound_tol Geometric bound-activity tolerance.
/// @return The measurement; every residual is NaN when SqpKkt::finite is false.
SqpKkt evaluate_kkt_over(Index n, Index me, Index mi, const Vec &lo, const Vec &up,
                         const NlpEval &ev, const Vec &x, const Vec &lambda_e, const Vec &lambda_i,
                         double bound_tol);

} // namespace detail

/// @brief Measures (x, lambda_e, lambda_i) against the model, reusing an
///        evaluation already taken at the SAME x.
/// @param model     The problem.
/// @param ev        An evaluation at `x`.
/// @param x         The iterate.
/// @param lambda_e  Equality multipliers, size model.me().
/// @param lambda_i  Inequality multipliers, size model.mi().
/// @param bound_tol Geometric bound-activity tolerance (the driver passes
///                  SqpOptions::feas_tol).
/// @return The measurement; every residual is NaN when SqpKkt::finite is false.
SqpKkt evaluate_kkt(const NlpModel &model, const NlpEval &ev, const Vec &x, const Vec &lambda_e,
                    const Vec &lambda_i, double bound_tol);

/// @brief Convenience overload that evaluates the model itself; it costs one
///        extra model evaluation, so the driver never uses it.
/// @param model     The problem.
/// @param x         The iterate.
/// @param lambda_e  Equality multipliers.
/// @param lambda_i  Inequality multipliers.
/// @param bound_tol Geometric bound-activity tolerance.
/// @return The measurement.
/// @throws std::invalid_argument through eval_nlp, on a mis-sized `x` or a
///         callback return whose shape contradicts the declared dimensions.
SqpKkt evaluate_kkt(const NlpModel &model, const Vec &x, const Vec &lambda_e, const Vec &lambda_i,
                    double bound_tol);

/// @brief Builds the SQP subproblem at `x`, in the step variable p, from an
///        evaluation already taken at that x.
/// @param model     The problem.
/// @param ev        An evaluation at `x`; must be finite (the caller's
///                  obligation, not checked here).
/// @param x         The linearization point.
/// @param lambda_e  Equality multipliers the Hessian is formed at.
/// @param lambda_i  Inequality multipliers the Hessian is formed at.
/// @param obj_scale Objective scale applied to the gradient and the Hessian.
/// @return The pure linearization: min (obj_scale*grad f)^T p + 1/2 p^T W p
///         subject to Je p = -cE(x), Ji p <= -cI(x), l - x <= p <= u - x, with
///         W the exact Lagrangian Hessian. No trust region is folded into the
///         box, and the solved QpSolution's multipliers are the NLP's with no
///         sign flip.
QpProblem build_subproblem(const NlpModel &model, const NlpEval &ev, const Vec &x,
                           const Vec &lambda_e, const Vec &lambda_i, double obj_scale = 1.0);

/// @brief Convenience overload that evaluates the model itself; it costs one
///        extra model evaluation, so the driver never uses it.
/// @param model     The problem.
/// @param x         The linearization point.
/// @param lambda_e  Equality multipliers.
/// @param lambda_i  Inequality multipliers.
/// @param obj_scale Objective scale.
/// @return The subproblem.
/// @throws std::invalid_argument through eval_nlp, on a mis-sized `x` or a
///         callback return whose shape contradicts the declared dimensions.
QpProblem build_subproblem(const NlpModel &model, const Vec &x, const Vec &lambda_e,
                           const Vec &lambda_i, double obj_scale = 1.0);

/// @brief The QP model's predicted objective decrease along a step.
/// @param qp The subproblem the step came from.
/// @param p  The step; must have size qp.n().
/// @return delta_m_f(p) = -(g^T p + 1/2 p^T W p) over the subproblem's own
///         objective data; positive on a model that promises progress.
/// @throws std::invalid_argument if `p` is mis-sized.
double predicted_decrease(const QpProblem &qp, const Vec &p);

/// @brief Builds a cold-start active-set seed from the subproblem's own
///        geometry, naming the rows and bounds that are already tight at x0.
/// @param qp       The first subproblem of a cold solve.
/// @param feas_tol Activity tolerance; a row or bound within it counts as tight.
/// @param seed     Receives the seed, populated in place.
/// @param rows     Receives the count of seeded inequality rows.
/// @param bounds   Receives the count of seeded bounds.
/// @return True iff the seed names anything at all; a seed naming nothing is
///         not offered to the engine.
bool crash_basis_seed(const QpProblem &qp, double feas_tol, QpSolution &seed, Index &rows,
                      Index &bounds);

} // namespace hven::solvers

// Included HERE rather than in the top block: these headers consume NlpEval,
// defined above, and must not include this one back. The position is load
// bearing -- moving them into the top group changes the definition order.
#include <hven/detail/drivers/aggregate_eval_seam.h>
#include <hven/detail/globalization/sqp/elastic.h>
#include <hven/detail/globalization/sqp/restoration.h>
#include <hven/detail/globalization/sqp/soc.h>

namespace hven::solvers {

/// @brief evaluate_kkt over a SEAM's dimensions and materialized box -- the
///        entry the driver's major loop uses.
/// @param seam      The aggregate seam; taken by const reference because this
///                  measurement evaluates nothing.
/// @param ev        An evaluation at `x`.
/// @param x         The iterate.
/// @param lambda_e  Equality multipliers.
/// @param lambda_i  Inequality multipliers.
/// @param bound_tol Geometric bound-activity tolerance.
/// @return The measurement, under the model-taking overload's contract.
SqpKkt evaluate_kkt(const AggregateEvalSeam &seam, const NlpEval &ev, const Vec &x,
                    const Vec &lambda_e, const Vec &lambda_i, double bound_tol);

/// @brief May the driver shrink the radius and re-solve after a failed
///        subproblem, rather than giving up?
/// @param qp        The subproblem that failed.
/// @param qs        Its result.
/// @param bound_tol Bound-activity tolerance, applied as a band OUTSIDE each
///                  bound (the driver passes SqpOptions::feas_tol).
/// @return True only for a kNumericalError or kMaxIter exit whose `x` is
///         correctly sized, finite and inside the box; false for kOptimal and
///         kInfeasible, which are answers rather than failures.
bool qp_failure_is_retryable(const QpProblem &qp, const QpSolution &qs, double bound_tol);

// --- The semismooth-Newton tier ---------------------------------------------
//
// SqpOptions::qp_mode selects the kernel each subproblem is solved on. kWalk is
// the default; kSsn routes the main subproblem of every major through
// ssn_engine.h and falls back to the walk, once, on any escape.
// See docs/notes/2026-09-header-prose-archive.md §sqp_driver.h.

/// @brief Trust-region slack the SSN certifying exit is allowed, as a multiple
///        of `fb_tol`; derived from ssn_engine.h's own constant rather than
///        restated as a literal.
inline const double kSsnTrViolationFactor = 2.0 * detail::kSsnComplementarityFactor;

/// @brief Does this SSN exit hand back something the funnel may use as a
///        trial step?
/// @param res    The kernel's result.
/// @param fb_tol The SsnOptions::fb_tol the solve ran at.
/// @return True only when the escape reason is kNone, the status is kOptimal,
///         and the point is finite and inside the trust region to within
///         kSsnTrViolationFactor * fb_tol.
bool ssn_exit_is_a_usable_step(const SsnResult &res, double fb_tol);

/// @brief Re-presents an SSN result as the QpSolution the driver's step and
///        warm-start paths consume.
/// @param res A usable SSN exit (see ssn_exit_is_a_usable_step).
/// @return The equivalent QpSolution, status kOptimal, with the activity
///         export carried across verbatim.
QpSolution ssn_result_to_qp_solution(const SsnResult &res);

/// @brief Clamp every strictly negative exported inequality price to 0.
/// @param lambda_i The exported inequality prices, swept IN PLACE.
/// @param counters Written, never read: `ssn_sign_swept` gains one per row
///        clamped, `ssn_sign_sweep_max` is raised to the largest magnitude
///        clamped. Both are cumulative across calls.
void sweep_negative_face_prices(Vec &lambda_i, SsnCounters &counters);

/// @brief Builds the SSN kernel's start object from the previous
///        subproblem's solution.
/// @param seed The prior QpSolution, or nullptr for no seed.
/// @return The start object; an empty one when `seed` is nullptr.
SsnStart ssn_start_from_qp_seed(const QpSolution *seed);

/// @brief The SSN kernel's `fb_tol` for a driver configured at these two
///        tolerances.
/// @param kkt_tol  SqpOptions::kkt_tol.
/// @param feas_tol SqpOptions::feas_tol.
/// @return The kernel tolerance derived from min(kkt_tol, feas_tol), so a
///         feas_tol tighter than kkt_tol is honoured.
double ssn_fb_tol_for(double kkt_tol, double feas_tol);

/// @brief Charges one SSN subproblem's cost to a solve's running totals.
/// @param total The solve's counters, updated in place.
/// @param res   The subproblem's result. Only factorizations and symbolic
///              analyses are folded -- `minor_iters` deliberately is not.
void charge_ssn_subproblem_cost(SqpCounters &total, const SsnResult &res);

/// @brief Charges a face refinement that was paid for and then refused.
/// @param total             The solve's counters, updated in place.
/// @param refined           The refinement's result.
/// @param ssn_budget_charge The probe budget, charged by the same
///                          factorization count.
void charge_refused_face_refinement(SqpCounters &total, const QpSolution &refined,
                                    Index &ssn_budget_charge);

/// @brief Writes the six mode-selection telemetry fields onto a history row,
///        from the QpSolution that is that major's answer.
///
/// Every definition is `SqpIterate`'s; nothing is decided here that is not
/// stated on those fields.
///
/// @param qp The subproblem `qs` solves; read for `Ai`, `bi`, `n` and `mi`.
///           Assumed validate()-consistent apart from the two sizes checked
///           here (`bi.size() == mi()`, `Ai.cols() == n()`).
/// @param qs This major's answer. A half whose activity vector does not match
///           @p qp contributes 0 to that half rather than being indexed.
/// @param prev_ineq_active The previous reporting major's row activity; empty
///                         or short means the empty set, which is what the
///                         first reporting major compares against.
/// @param prev_bound_state The previous reporting major's bound states; a
///                         missing entry reads `kFree`. The caller carries both
///                         forward unconditionally.
/// @param slack A caller-owned reused buffer, resized here.
/// @param row The row to write; the six fields are assigned, not accumulated,
///            except the three census counts, which are incremented from
///            whatever the row already holds.
/// @throws std::invalid_argument if @p qp is inconsistent in either size.
void census_major_activity(const QpProblem &qp, const QpSolution &qs,
                           const std::vector<bool> &prev_ineq_active,
                           const std::vector<BoundState> &prev_bound_state, Vec &slack,
                           SqpIterate &row);

/// @brief Folds one subproblem's SSN counters into a solve's running total.
/// @param total The running total, updated in place.
/// @param one   The subproblem's counters. The two peaks
///              (`ssn_uncertain_peak`, `ssn_sign_sweep_max`) aggregate by max;
///              every other field sums.
void accumulate_ssn_counters(SsnCounters &total, const SsnCounters &one);

/// @brief Folds one subproblem's IPQP counters into a solve's running total.
/// @param total The running total, updated in place.
/// @param one   The subproblem's counters. `ipqp_rho_demanded_max`,
///              `ipqp_restart_shift_max` and `ipqp_tier_retired_after` fold by
///              max -- the last is a major index, not a count --
///              `ipqp_alpha_p_min` and `ipqp_alpha_d_min` by min,
///              `ipqp_rho_demanded_last` and `ipqp_final_inertia_read` are
///              overwritten, and every other field sums.
void accumulate_ipqp_counters(IpqpCounters &total, const IpqpCounters &one);

/// @brief True iff this tier exit is a step the routing chain may use.
/// @param res   The tier's result.
/// @param opts  The QpOptions the tier ran under -- its tolerances.
/// @param iopts The IpqpOptions it ran under; `ipqp_converge_slack` is read.
bool ipqp_exit_is_a_usable_step(const IpqpResult &res, const QpOptions &opts,
                                const IpqpOptions &iopts);

/// @brief Maps a usable tier exit onto the QpSolution the driver routes.
/// @param res The tier's result, already judged usable.
QpSolution ipqp_result_to_qp_solution(const IpqpResult &res);

/// @brief Charges an abandoned tier subproblem's factorization cost.
/// @param total The solve's running counters, updated in place.
/// @param res   The abandoned tier result.
void charge_ipqp_subproblem_cost(SqpCounters &total, const IpqpResult &res);

/// @brief One ladder run's result: the subproblem it built, the rung it stopped
/// on, and the four-flag verdict on that rung. The history-row fields are
/// carried as values so a consumer needs no second read of `qs_e`.
struct ElasticLadderReport {
    ElasticQp elastic;       ///< The augmented problem, at the LAST rung's rho.
    QpSolution qs_e;         ///< The rung the ladder stopped on, AUGMENTED space.
    Vec p_elastic;           ///< Its original-variable block, or Zero(n) if not kOptimal.
    double slack_l1 = 0.0;   ///< l1 of the ACTUAL violations at qs_e; 0.0 when not kOptimal.
    bool closed = false;     ///< kOptimal && slack_l1 <= feas_tol: the relaxation shut.
    bool reduced = false;    ///< kOptimal && slack_l1 <= violation_l1 - feas_tol.
    bool promises_f = false; ///< kOptimal and predicted_decrease(qp, p_elastic) > 0.
    bool usable = false;     ///< kOptimal && (closed || reduced || promises_f).
    QpStatus qp_status = QpStatus::kOptimal; ///< qs_e.status, for the history row.
    Index qp_minor_iters = 0;                ///< qs_e's own count, for the row.
    Index qp_factorizations = 0;             ///< qs_e's own count, for the row.
    double step_norm = 0.0;                  ///< inf-norm of p_elastic, diagnostic.
    /// @brief The first rung's penalty, as the placement bound resolved it. Not
    ///        invertible from the report's `elastic` block once the ladder has
    ///        climbed, and the retry rule's own input.
    double rho_0 = 0.0;
    /// @brief True iff this ladder's placement was clamped -- by headroom or by
    ///        dual_mu -- i.e. the evidence priced the violation above the rung the
    ///        placement bound allows. Aggregated over a solve by
    ///        SqpCounters::elastic_rho0_ceiling_hits. On a certified-fallback
    ///        report it is the entry's clamp: a retry at the floor carries the
    ///        original attempt's flag.
    bool rho0_ceiling_hit = false;
};

/// @brief Where `run_elastic_ladder` takes its FIRST rung's seed from -- a SOURCE, not a mapped
/// point, because the mapping needs the `ElasticQp` the ladder builds inside itself. At most one
/// arm is set; both null is legal and means no hint. NON-OWNING: both must outlive the call.
struct ElasticSeedSource {
    /// The elastic tier's arm: THIS `qp`'s own kInfeasible solve, mapped index-for-index by
    /// `elastic_seed`. A size mismatch degrades to no hint rather than throwing.
    const QpSolution *failed = nullptr;
    /// The certified fallback's arm: the tier escaped with no QpSolution at all, so the seed is
    /// the WORKING SET at the evidence's least-infeasible point (spec 2.3 item 2's ratio rule on
    /// its retained duals, geometric activity when they are absent).
    const IpqpInfeasibilityEvidence *evidence = nullptr;
};

// Preconditions are documented, not validated: every arm of `seed` degrades to
// "no hint" on a size it cannot use or a block that never fired, and the ladder
// never reads it for a verdict.

/// @brief Runs the rho ladder to exhaustion and judges the rung it stopped on.
/// @param engine The engine every rung solves with.
/// @param qp     The subproblem to relax.
/// @param seed   Where the first rung's seed comes from; at most one arm set.
/// @param window The radius folded into the elastic box: nonnegative (0 is
///               legal) and finite on the shipped path.
/// @param opts   The driver's options -- the tolerances and the seed rule.
/// @param out    The running counters; every rung folds into them.
/// @param rho_0_override When set, the first rung's penalty, clamped into
///               [kElasticRhoInit, kElasticRhoMax]; it reads no evidence and
///               leaves `rho0_ceiling_hit` false. Otherwise the first rung is
///               rho_0 = max(kElasticRhoInit, min(evidence-priced start,
///               kElasticRhoMax / kElasticRhoFactor,
///               kElasticRhoDualMuSafety / opts.qp.dual_mu)), and either cap
///               binding sets `rho0_ceiling_hit`. A non-finite or non-positive
///               dual_mu disables that cap. The margin binds the first rung
///               only: the ladder still climbs to kElasticRhoMax.
/// @param sink   Optional trace sink; non-null makes each rung's walk write its
///               own `qp.mode` line at site `elastic_rung`.
/// @return The ladder's report.
/// @throws std::invalid_argument if `window` is negative or NaN, or if an
///         override is NaN or non-positive (+inf is legal and clamps).
ElasticLadderReport run_elastic_ladder(QpEngine &engine, const QpProblem &qp,
                                       const ElasticSeedSource &seed, double window,
                                       const SqpOptions &opts, SqpCounters &out,
                                       std::optional<double> rho_0_override = std::nullopt,
                                       TraceSink *sink = nullptr);

/// @brief The escape branch's single entry: a bounded two-rung ladder. Rung A is
///        the elastic QP at the evidence's own rho_0; rung B is a cold walk,
///        reached only on a rung A the engine declined. A declined rung A placed
///        above the floor is retried there once, and counts as one activation.
/// @param engine    The engine both rungs solve with.
/// @param qp        The subproblem.
/// @param ev        The NLP evaluation at the current iterate; unread today.
/// @param seed      The seed the ordinary walk would have had, or nullptr;
///                  unread today -- rung A seeds from the evidence, rung B is cold.
/// @param evidence  `fired` gates rung A, the least-infeasible point and its
///                  duals seed it, and `dual_norm_start` places rho_0; nothing
///                  else is read. Its two headline scalars are recorded on @p row.
/// @param overrides The walk's per-solve overrides, the caller's own levers.
/// @param opts      The driver's options -- rung A's tolerances and seed rule.
/// @param window    The radius this solve was given, folded into rung A's box.
/// @param out       The running counters; every rung of rung A folds into them.
/// @param row       This major's history row, annotated with the evidence.
/// @param fallback_report Rung A's report, engaged iff rung A owns the returned
///                  solution.
/// @param verdict   Overwritten on every entry, fired or not: this entry's own
///                  fallback verdict event, which the caller emits.
/// @param sink      Optional trace sink; non-null makes rung B's cold walk write
///                  a `qp.mode` line at site `fallback_rung_b` at both of its
///                  return sites, and is forwarded into rung A's ladder.
/// @return Rung A's step in the original variables, or rung B's walk solution
///         unchanged.
/// @throws std::invalid_argument if `window` is negative or NaN.
QpSolution certified_feasibility_fallback(QpEngine &engine, const QpProblem &qp, const NlpEval &ev,
                                          const QpSolution *seed,
                                          const IpqpInfeasibilityEvidence &evidence,
                                          const SolveOverrides &overrides, const SqpOptions &opts,
                                          double window, SqpCounters &out, SqpIterate &row,
                                          std::optional<ElasticLadderReport> &fallback_report,
                                          SqpFallbackVerdictTraceEvent &verdict,
                                          TraceSink *sink = nullptr);

// --- Adaptive dual regularization -------------------------------------------
//
// Caller-visible surface: SqpOptions::adaptive_mu and SqpIterate::mu
// (sqp_types.h). The schedule is mu_k = clamp(kAdaptiveMuKappa *
// ||KKT residual||^1.5, kAdaptiveMuMin, kAdaptiveMuMax), quantized to the
// nearest decade and read at the PREVIOUS major's measurement; the first major
// uses the ceiling. Disabled, the driver never touches SolveOverrides::dual_mu.
// See docs/notes/2026-09-header-prose-archive.md §sqp_driver.h.

/// @brief kappa_mu in the adaptive dual-regularization schedule above; named
///        so a future re-derivation has somewhere to change it.
inline constexpr double kAdaptiveMuKappa = 1.0;
/// @brief Floor of the schedule's dual regularization.
inline constexpr double kAdaptiveMuMin = 1e-12;
/// @brief Ceiling of the schedule's dual regularization; equals
///        QpOptions::dual_mu's own default.
inline constexpr double kAdaptiveMuMax = 1e-8;

// --- The seeded dual clamp --------------------------------------------------
//
// On a kSeeded ingest, and only there, an ingested lambda_i(j) still negative
// after the geometric complementarity clear is set to 0 when it lies within
// kSeededDualClampTol and degrades the whole object to kCold when it does not.
// The order -- geometric clear first, clamp second -- is normative.
// See docs/notes/2026-09-header-prose-archive.md §sqp_driver.h.
//
// THE CONSTANT ITSELF MOVED (M6 W5 T8.5) to `warmstart/seeding.h`, beside the
// IPM's own two seeding constants -- three policies, three derivations, one
// header, values DELIBERATELY NOT UNIFIED (design 2.4). It keeps this exact
// namespace-scope spelling, so nothing that names it had to change; this
// header pulls it in so every existing `#include <hven/drivers/sqp_driver.h>`
// still sees it.

/// @brief The SQP driver: a trust-region SQP major loop over one QpEngine.
/// Every member below -- and every free function declared above -- is defined
/// in src/drivers/sqp_driver.cpp; read that file's banner before changing this
/// class's structure. (The two constructors joined them in M6 W5 T8.7 fix1,
/// when the console members' types became forward declarations.)
class SqpDriver {
  public:
    /// @brief Constructs a driver over the given options.
    /// @param opts The options; validated here.
    /// @throws std::invalid_argument on an option validate_sqp_options rejects:
    ///         a non-positive or NaN kkt_tol/feas_tol, a negative max_iter, a
    ///         non-positive or NaN tr_init (0 is rejected outright; +inf is legal
    ///         and means no trust region), a tr_max below tr_init unless tr_init
    ///         is +inf, or a tr_min that is non-positive or above tr_init or
    ///         tr_max.
    ///
    /// DEFINED IN THE .cpp (M6 W5 T8.7 fix1), like the destructor and for the
    /// same reason: a constructor instantiates the `unique_ptr` members'
    /// deleters for its own unwind path, and the two console members' types are
    /// forward-declared here.
    ///
    /// VALIDATION PRECEDES CONSTRUCTION (M6 W5 T8.8 fix1): the options travel
    /// through `validated()` in the `opts_` mem-initializer, which is ordered
    /// before `engine_`, so a rejected value throws from `validate_sqp_options`
    /// and no engine -- and no backend session -- is ever built.
    explicit SqpDriver(const SqpOptions &opts);

  private:
    /// @brief Validates @p opts and returns it, so that validation happens
    ///        BEFORE any engine is constructed.
    ///
    /// M6 W5 T8.8 fix1 (astra I1 / the lane's I1). Both constructors build a
    /// `QpEngine` in their mem-initializer lists, and since T8.8 that engine
    /// applies `common.threads` to a `SymmetricFactor` -- which validates its
    /// own count and throws first, with ITS message, for a negative one. A body
    /// `validate_sqp_options(opts_)` therefore ran too late to be the refusal a
    /// caller saw. `opts_` is declared before `engine_`, so routing the options
    /// through here makes the mem-initializer order the guarantee: nothing is
    /// built until the whole SqpOptions value has been accepted. Pinned by
    /// `Threads.ANegativeThreadCountIsRefusedByTheDriverConstructor`.
    static const SqpOptions &validated(const SqpOptions &opts);

    /// The tag the restoration phase's own driver is constructed with. A TYPE
    /// rather than a `bool` parameter (M6 W5 T8.7 fix1, the lane's M2): the two
    /// facts about that driver are DIFFERENT facts, and one bool was carrying
    /// both. `allow_restoration_ == false` is "may not restore, so the
    /// recursion stops at one level"; `is_restoration_sub_driver_ == true` is
    /// "this driver is the restoration phase's own, and is the ONE driver that
    /// never prints". The console was keyed on the FIRST, which happens to
    /// select the same driver only because this constructor is private and the
    /// restoration phase is its only caller -- so the condition read as "a
    /// driver that may not restore does not print", which is not a rule this
    /// library has. Nothing outside this class can name the tag, so the
    /// sub-driver constructor stays private and the recursion bound is
    /// unchanged.
    struct RestorationSubDriverTag {
        explicit RestorationSubDriverTag() = default;
    };

    // The restoration phase's own driver is constructed through here: it may
    // not restore (which bounds the recursion at one level) and it never builds
    // a console of its own (the parent's console sees the nested pair at
    // depth 1 and renders none of it). Defined in the .cpp, with the other two.
    SqpDriver(const SqpOptions &opts, RestorationSubDriverTag);

  public:
    // NEITHER COPYABLE NOR MOVABLE, BY DECLARATION (M6 W5 T8.3 fix1). Design
    // §2.1 says a driver does not move; before this task the by-value QpEngine
    // member enforced half of that by accident (QpEngine declares no
    // assignment, so move ASSIGNMENT was implicitly deleted). Holding the
    // engine through a unique_ptr removed that accident and made
    // `a = std::move(b)` compile again -- and a moved-from driver would hold a
    // NULL engine_, which every use below dereferences without a check. The
    // four operations are deleted explicitly so the invariant is a property of
    // the class rather than of whichever member happens to be non-movable
    // today; the traits are pinned in tests/drivers/test_options.cpp.
    /// @brief Deleted: a driver is not copy-constructible.
    SqpDriver(const SqpDriver &) = delete;
    /// @brief Deleted: a driver is not copy-assignable.
    SqpDriver &operator=(const SqpDriver &) = delete;
    /// @brief Deleted: a driver is not move-constructible.
    SqpDriver(SqpDriver &&) = delete;
    /// @brief Deleted: a driver is not move-assignable.
    SqpDriver &operator=(SqpDriver &&) = delete;

    /// @brief Destroys the driver.
    ///
    /// DECLARED HERE AND DEFINED IN THE .cpp (M6 W5 T8.7 fix1): the two console
    /// members above are `unique_ptr`s to forward-declared types, and a
    /// destructor the compiler writes into each consumer would need those types
    /// complete there. Out of line, the deleter is instantiated once, in the
    /// one translation unit that includes `drivers/console_trace_sink.h`.
    ~SqpDriver();

    /// @brief Solves from the model's own start_point().
    /// @param model The problem; wrapped in a bridge built here.
    /// @return The solution.
    /// @throws std::invalid_argument on a model that cannot describe a
    ///         problem, through the same aggregate-declaration validation the
    ///         explicit-start-point overload below documents -- a malformed
    ///         start_point() included.
    SqpSolution solve(const NlpModel &model);

    /// @brief Attaches a ledger for instrumentation; nullptr (the default) is off.
    /// @param ledger       The ledger, or nullptr.
    /// @param label_prefix The label prefix for this driver's records.
    ///
    /// Emits exactly one SqpSolveRecord per public solve() on this instance,
    /// labeled `label_prefix_<n>`, and forwards the same Ledger to this driver's
    /// own QpEngine under `label_prefix_qp`. The restoration phase's nested
    /// driver is never given it, so a solve that restores still emits one record.
    void attach_ledger(Ledger *ledger, std::string label_prefix);

    /// @brief Attaches a trace sink; nullptr (the default) is off.
    ///
    /// THE SINK A SOLVE ACTUALLY WRITES TO IS COMPOSED AT SOLVE ENTRY (M6 W5
    /// T8.7). When `common.print_level` says printing is on, the driver builds
    /// its own `ConsoleTraceSink` and fans out over BOTH -- this sink first,
    /// the console second -- and hands the FAN-OUT to every emit site, to the
    /// IPQP engine and to the restoration sub-driver. So a caller's stream is
    /// byte-identical with printing on and off, its depth-1 lines included, and
    /// attaching a console never displaces this sink.
    ///
    /// LEGAL BETWEEN SOLVES ONLY, on `set_options`' rule and for its reason
    /// (M6 W5 T8.7): the effective sink is fixed for the duration of a solve,
    /// so a change made from inside a callback could not take effect in that
    /// solve anyway, and refusing says so instead of silently deferring.
    /// `InteriorPointSolver::attach_trace` refuses on the same rule since M6 W5
    /// T8.7 fix1.
    ///
    /// THE ONE DRIVER THAT NEVER PRINTS is the restoration phase's own
    /// sub-driver, which this class constructs for itself and no caller can
    /// name. Every other driver builds its console at the tiers
    /// `common.print_level` names -- including one constructed with restoration
    /// disabled, which is a different fact and no longer decides this one (M6
    /// W5 T8.7 fix1).
    ///
    /// @param sink The sink, or nullptr. Forwarded to the internal IpqpEngine;
    ///             this driver additionally emits `ipqp.route` and `qp.mode`
    ///             itself, in the kIpm dispatch arm.
    /// @throws std::logic_error if a solve is in flight on this driver.
    void attach_trace(TraceSink *sink);

    // --- The shared per-iteration callback (M6 W5 T8.6) ---
    /// @brief Installs the per-iteration callback both engines take.
    ///
    /// WHEN IT FIRES: once per HISTORY ROW, at the one site that emits them --
    /// after the `sqp.major` trace line and before the push -- so the trace
    /// stream, `SqpSolution::history` and the callback are the same rows, in
    /// the same order, in the same CALLER units. Every major records its
    /// iterate exactly once, rejected trials and the non-finite-start exit
    /// included, so the event count equals `history.size()`.
    ///
    /// WHAT THE EVENT DESCRIBES: the ROW BEING PUSHED -- the iterate that major
    /// stood on -- and not the pending commit. `IterationEvent::depth` is the
    /// restoration nesting: 0 in this solve, 1 inside a restoration sub-solve,
    /// whose events reach the SAME callback through a forwarder this driver
    /// installs.
    ///
    /// WHAT kStop DOES: the stop is LATCHED and honoured before the next
    /// subproblem is built, so no QP is solved for an answer the caller no
    /// longer wants; the solve then takes its ORDINARY terminal exit at the
    /// CURRENT iterate and reports kInterrupted. Precedence at that exit is
    /// converged > interrupted > probe-exhausted > cap: a stop on a CONVERGED
    /// row reports kOptimal (converged beats stop), and under
    /// SqpOptions::budget_mode an interrupt takes the ordinary exit rather than
    /// the budget-best one, exactly as a probe-budget stop does. A stop
    /// returned INSIDE a restoration sub-solve latches on this driver too, so
    /// no further top-level major runs whichever way that phase returns.
    /// kNumericalError and the QP-failure exit carry their own verdicts and are
    /// not demoted by a stop -- the row is terminal either way.
    ///
    /// The callback may throw; the exception propagates out of solve(), no
    /// `sqp.solve.end` event and no ledger record are emitted for the abandoned
    /// solve (the pair is deliberately non-RAII), the QP-engine records its
    /// subproblems already emitted STAY, and this driver remains usable.
    ///
    /// WHERE THE STOPPED SOLVE ENDS UP (M6 W5 T8.6 fix1). A stop returned on
    /// row `k` is honoured at row `k+1`'s exit conjunction, so the solve
    /// returns AFTER row `k+1`: `history.size() == k + 2`,
    /// `counters.major_iters == k + 1`, and the point returned is the one the
    /// LAST event describes -- the committed point row `k`'s accepted step
    /// reached, measured once and stepped from never. A stop returned on the
    /// PUBLIC call's own terminal row is a no-op: that row's exit and verdict
    /// were already decided when the callback was shown it.
    ///
    /// SETTING OR CLEARING FROM INSIDE THE CALLBACK IS SAFE (M6 W5 T8.6 fix1).
    /// Either call made while the callback is on the stack is DEFERRED: the
    /// stored std::function is left alone until the invocation returns, and the
    /// change is applied at that point (and, failing that -- a callback that
    /// left by throwing -- at the next solve entry). So a callback may disarm
    /// itself and go on touching its own captures.
    ///
    /// @param cb The callback. An empty std::function is the same as clearing.
    void set_iteration_callback(IterationCallback cb) {
        if (callback_in_flight_ || solve_in_flight_) {
            pending_callback_ = std::move(cb);
            // WHICH SAFE POINT APPLIES IT (M6 W5 T8.7b, the lane's Q5, taken on
            // both engines). From inside the callback: the statement after that
            // invocation returns -- T8.6's contract, unchanged. From anywhere
            // else while a solve runs, a SINK METHOD included: the next solve's
            // ENTRY, so the solve in progress is bitwise the solve it would
            // have been without the call.
            //
            // ONE SLOT FOR THIS SETTER AND clear_iteration_callback(), AND THE
            // LAST WRITE WINS (M6 W5 T8.7b fix1, the lane's M3): a sink-origin
            // park followed in the SAME solve by a callback-origin call keeps
            // only the second, applied when that callback returns. The most
            // recent request from the caller is the one that takes effect,
            // whoever made it; a queue would replay a stale value after a
            // newer one.
            //
            // THE RESTORATION SUB-DRIVER FORWARDS THROUGH THE PARENT. A set
            // made from inside a callback that the restoration sub-solve
            // forwarded goes through the PARENT's invoke_iteration_callback, so
            // `callback_in_flight_` is set on the parent for the duration and
            // the park is invocation-origin there -- T8.6's forwarded semantics
            // are unchanged by this task.
            pending_callback_at_entry_ = !callback_in_flight_;
            return;
        }
        // A DIRECT CALL SUPERSEDES ANY PENDING DEFERRAL (M6 W5 T8.7, the SQP
        // lane's M9). A callback that parked a deferral and then THREW never
        // reached the apply point, so the optional is still engaged; without
        // this reset the next solve entry would apply that stale value OVER the
        // callback just installed here.
        pending_callback_.reset();
        pending_callback_at_entry_ = false;
        iteration_callback_ = std::move(cb);
    }
    /// @brief Removes the per-iteration callback.
    ///
    /// Deferred to the safe point when called from INSIDE the callback; see
    /// set_iteration_callback(), whose ONE deferral slot this shares -- last
    /// write wins.
    void clear_iteration_callback() {
        if (callback_in_flight_ || solve_in_flight_) {
            pending_callback_ = IterationCallback{};
            pending_callback_at_entry_ = !callback_in_flight_;
            return;
        }
        // See set_iteration_callback(): a direct call supersedes a deferral.
        pending_callback_.reset();
        pending_callback_at_entry_ = false;
        iteration_callback_ = nullptr;
    }

    /// @brief Returns the options this driver runs under.
    ///
    /// READ-ONLY: there is no mutable accessor. Copy it, edit the copy, hand it
    /// back through set_options().
    const SqpOptions &options() const noexcept { return opts_; }

    /// @brief The thread count in force on the SSN tier's live factor, or -1
    ///        when this driver has not built that tier yet.
    ///
    /// AN OBSERVATION POINT (M6 W5 T8.8 fix1, astra I2 (a)). Both tier engines
    /// are LAZY: they are constructed at first use with `opts_.common.threads`,
    /// so a driver that has not run a solve in that tier's `qp_mode` holds no
    /// engine at all and this reads -1. After a solve that DID use the tier it
    /// reads the tier engine's own `num_threads()`, which reads through to the
    /// live backend session -- so it observes the driver-to-tier hand-off
    /// itself, not merely that an engine built with a count keeps it.
    int ssn_tier_num_threads() const {
        return ssn_engine_ == nullptr ? -1 : ssn_engine_->num_threads();
    }

    /// @brief The thread count in force on the IPQP tier's live KKT
    ///        factorization, or -1 when this driver has not built that tier yet.
    /// @see ssn_tier_num_threads(), whose discipline this shares.
    int ipqp_tier_num_threads() const {
        return ipqp_engine_ == nullptr ? -1 : ipqp_engine_->num_threads();
    }

    /// @brief Replaces the whole options value, rebuilding the QP engines.
    ///
    /// TRANSACTIONAL, in this order: validate(o) first; then a REPLACEMENT
    /// QpEngine is constructed into a temporary from `o.qp` and given the same
    /// ledger attachment (and the same solve counter, so the record labels keep
    /// counting rather than restarting at `<prefix>_qp_0`); only then is it
    /// swapped in, the lazily-built SSN and IPQP engines dropped (each holds its
    /// own COPY of the QpOptions, so dropping them is both necessary and
    /// sufficient), and the options adopted. A throw at any point -- validation
    /// or construction -- leaves the previous options AND the previous engines
    /// in force, and the driver usable.
    ///
    /// ONE RULE, NO FAST PATH: a replacement with IDENTICAL options rebuilds
    /// too. What the rebuild costs is this driver's cached K0 border; what it
    /// does NOT cost is a hot handle's reuse, which is keyed on the producing
    /// engine's OPTIONS FINGERPRINT rather than its identity, so a rebuild at
    /// identical options still adopts (detail/qp/qp_engine.h's HotState).
    ///
    /// Legal BETWEEN SOLVES only.
    ///
    /// @param o The replacement options.
    /// @throws std::invalid_argument if validate(o) rejects the value.
    /// @throws std::logic_error if a solve is in flight on this driver (a
    ///         replacement from inside a strategy factory or a callback).
    void set_options(SqpOptions o);

    /// @brief Solves from an explicit start point.
    /// @param model The problem; wrapped in a bridge built here.
    /// @param x0    The starting point.
    /// @return The solution.
    /// @throws std::invalid_argument on a model that cannot describe a problem.
    ///         Wrapping it in a bridge runs the aggregate declaration's own
    ///         validation (model/aggregate_declaration.h) before the loop starts,
    ///         which refuses a crossed or NaN box, a start_point() whose size is
    ///         not n(), an eval_hess entry below the diagonal, and a Jacobian or
    ///         Hessian whose dimensions contradict me()/mi()/n(); and, per
    ///         evaluation, a stored-element count or coordinate set that moves
    ///         with x.
    SqpSolution solve(const NlpModel &model, const Vec &x0);

    /// @brief Solves from an explicit start point under a caller's work
    ///        ceiling.
    /// @param model  The problem; wrapped in a bridge built here.
    /// @param x0     The starting point.
    /// @param budget The caller's work ceiling, on exactly the terms the
    ///               warm-start overload below states: `minor_budget` is the
    ///               probe budget and `max_iterations`, when non-zero, gives
    ///               an effective major cap of min(it, SqpOptions::max_iter)
    ///               that governs the exit conjunction, the restoration
    ///               refusal and the restoration sub-driver alike.
    /// @return The solution.
    /// @throws std::invalid_argument on the classes the 2-argument overload
    ///         above enumerates.
    ///
    /// ADDED IN M6 W5 T8.4 fix1: design section 2.2 puts the budget on EVERY
    /// public overload, and the cold and staged entries hardcoded
    /// `SolveBudget{}`. With staging retired in T8.5 every warm route is an
    /// overload of its own, and each of them takes a budget.
    SqpSolution solve(const NlpModel &model, const Vec &x0, SolveBudget budget);

    /// @brief Solves against an already-built bridge -- the primary path every
    ///        NlpModel-taking overload wraps.
    /// @param bridge The aggregate to solve over; caller-owned. It owes one
    ///               operation at a time, structural mutation included.
    /// @param x0     The starting point.
    /// @return The solution.
    ///
    /// A COLD solve, unconditionally: with staging retired (M6 W5 T8.5) this
    /// overload has no warm-start source at all, and `x0` is simply the start.
    /// @throws std::invalid_argument only through `bridge` itself -- this entry
    ///         does not re-check the box, which the bridge validated when it laid
    ///         its structures.
    SqpSolution solve(NlpModelAggregate &bridge, const Vec &x0);

    /// @brief Solves against an already-built bridge under a caller's work
    ///        ceiling -- the primary budgeted path every other overload reaches.
    /// @param bridge The aggregate to solve over; caller-owned.
    /// @param x0     The starting point.
    /// @param budget The caller's work ceiling; see the model-taking form
    ///               above.
    /// @return The solution.
    /// @throws std::invalid_argument on the classes the 2-argument bridge
    ///         overload above enumerates.
    SqpSolution solve(NlpModelAggregate &bridge, const Vec &x0, SolveBudget budget);

    /// @brief Warm-start ingest against a model, wrapped in a bridge built here.
    /// @param model        The problem to solve.
    /// @param x0           The starting point; ignored whenever `warm` resolves
    ///                     above kCold, where the solve starts from `warm.x`.
    /// @param warm         A prior solve's warm-start object. Never throws on a
    ///                     stale or foreign value: an unusable hash resolves
    ///                     kSeeded and still contributes its values, and a failed
    ///                     ingest gate degrades to kCold.
    /// @param budget      The caller's work ceiling (M6 W5 T8.4; this argument
    ///                     was `Index minor_budget` before).
    ///                     `budget.minor_budget` is that probe budget verbatim:
    ///                     0 (the default) is no budget, and a positive value
    ///                     stops the solve at the top of the first major that
    ///                     finds qp_minor_iters >= it without having converged,
    ///                     reporting kMaxIter and counting the stop in
    ///                     counters.probe_budget_stops. Convergence always wins,
    ///                     the test runs between majors only, and under kSsn the
    ///                     budget also charges the SSN and refinement
    ///                     factorizations.
    ///                     `budget.max_iterations`, when non-zero, gives an
    ///                     EFFECTIVE MAJOR CAP of min(it, SqpOptions::max_iter)
    ///                     -- a caller may tighten this engine's own limit,
    ///                     never loosen it -- and RESTORATION IS BUDGETED FROM
    ///                     THAT CAP, so it cannot spend past the caller's
    ///                     ceiling.
    /// @return The solution.
    /// @throws std::invalid_argument on a model that cannot describe a problem --
    ///         the classes the 2-argument model-taking overload enumerates --
    ///         checked before `warm` is looked at. NOTHING ELSE: the
    ///         two-warm-start-sources refusal this entry used to name went with
    ///         staging (M6 W5 T8.5). A call names exactly one warm-start
    ///         source, which is its own argument, so two sources for one solve
    ///         is no longer a state that can be reached.
    SqpResult solve(const NlpModel &model, const Vec &x0, const SqpWarmStart &warm,
                    SolveBudget budget = {});

    /// @brief Warm-start ingest against an already-built bridge, on the same
    ///        footing as the 2-argument bridge overload above.
    /// @param bridge       The aggregate to solve over; caller-owned.
    /// @param x0           The starting point; ignored whenever `warm` resolves
    ///                     above kCold.
    /// @param warm         The prior solve's warm-start object; the model-taking
    ///                     overload just above carries the whole ingest contract.
    /// @param budget      The caller's work ceiling, on the terms that overload
    ///                     states.
    /// @return The solution.
    /// @throws std::invalid_argument only through `bridge` itself (this entry
    ///         does not re-check the box). The two-warm-start-sources refusal
    ///         this entry used to name is gone with staging (M6 W5 T8.5) --
    ///         see the model-taking overload just above.
    SqpResult solve(NlpModelAggregate &bridge, const Vec &x0, const SqpWarmStart &warm,
                    SolveBudget budget = {});

    // --- The PAYLOAD route: the shared protocol, on both engines -----------
    //
    // ONE PROTOCOL, TWO IDENTITIES (design 2.4). The two overloads above take
    // this engine's OWN native object (`warmstart/sqp_warm_start.h`), which is
    // the LABELLED SQP-ONLY entry -- the exact counterpart of the IPM-only KKT
    // hook -- and is the only route to kWarm/kHot. The two below take the
    // SHARED declared-space currency `WarmStartData`, which the interior-point
    // engine's own `solve(model, x0, warm, budget)` also takes, which
    // `SolveResult::export_warm_start()` hands back, and which is what travels
    // between engines and across a serialization boundary.
    //
    // THE RULE ON THIS ROUTE: identity mismatch REFUSES; pattern and value
    // defects DEGRADE.
    //   * IDENTITY = the block lengths and the `DeclarationKey` stamp, checked
    //     at solve entry against the problem this call binds, both
    //     `std::invalid_argument`, IN EVERY `SqpOptions::qp_mode` -- kWalk,
    //     kSsn AND kIpm alike (M6 W5 T8.5, owner ruling; M5 ruling 4's
    //     mode-local cold grade under kIpm is RETIRED).
    //   * PATTERN = the structural hash of the assembled QP matrices. A payload
    //     never saw a model and so always carries hash 0: the level a payload
    //     resolves to is CAPPED AT kSeeded, by construction, never by refusal.
    //   * VALUE DEFECTS = the seeded dual clamp band (`kSeededDualClampTol`,
    //     warmstart/seeding.h) -- an inequality price a shade negative is
    //     clamped and counted, a badly negative one degrades the object to
    //     kCold. Unchanged.
    //     ONE VALUE DEFECT DOES NOT DEGRADE, AND SAYING SO MATTERS (M6 W5
    //     T8.5 fix1): a NON-FINITE entry in a PAYLOAD's core blocks is
    //     REFUSED at the hand-over, not graded down. Only the NATIVE route
    //     degrades a non-finite value (to kCold, unchanged). A payload is
    //     currency that crossed an engine or a serialization boundary, and a
    //     NaN in one is a broken value rather than a stale one.
    //
    // THE MULTIPLIERS-ONLY SEED. A payload whose `primal_` is EMPTY carries
    // multipliers and nothing else; `x0` is the start, and the object still
    // resolves kSeeded (it did NOT before T8.5 -- an empty primal failed the
    // plausibility gate and silently cold-started, dropping the multipliers).
    // Its hand-over rule is `primal_` and `bound_lmults_` EITHER both empty OR
    // both at the declared primal width. A polish extension on a seed is
    // IGNORED and COUNTED (`SqpCounters::polish_ignored`): its bound duals are
    // stated at the exporter's point, and this solve stands at `x0`.
    //
    // `z` IS NEVER INGESTED FROM ANY SEED on this engine, payload or native --
    // today's rule, restated here because the payload carries a `bound_lmults_`
    // block that looks ingestable and is not.
    //
    // AMBIGUITY, stated once: an lvalue of `SqpWarmStart` or of `WarmStartData`
    // selects its own overload unambiguously (neither converts to the other,
    // and `SolveBudget` converts from neither). The ONE ambiguous SPELLING is a
    // BRACED third argument -- `solve(bridge, x0, {})` would match
    // `SolveBudget`, `WarmStartData` and `SqpWarmStart` alike. Spell the type.

    /// @brief Warm-start ingest from the SHARED payload, against a model.
    /// @param model  The problem to solve.
    /// @param x0     The starting point. Used whenever `warm` does not supply
    ///               one -- the multipliers-only form at every level, and any
    ///               form that degrades to kCold.
    /// @param warm   The payload; read, never retained.
    /// @param budget The caller's work ceiling, on the terms the native
    ///               model-taking overload above states.
    /// @return The solution.
    /// @throws std::invalid_argument on a model that cannot describe a problem
    ///         (checked first); if any core block of `warm` holds a non-finite
    ///         value; if `primal_` and `bound_lmults_` are neither both empty
    ///         nor one length; if the `"hven.ipm.polish.v1"` extension is
    ///         duplicated, unreadable, not at the core's widths, or holds a
    ///         non-finite or negative bound dual; if any block is not at this
    ///         problem's declared dimensions; or if the declaration stamp is
    ///         not this problem's -- the last two IN EVERY qp_mode.
    SqpResult solve(const NlpModel &model, const Vec &x0, const WarmStartData &warm,
                    SolveBudget budget = {});

    /// @brief Warm-start ingest from the SHARED payload, against a bridge.
    /// @param bridge The aggregate to solve over; caller-owned.
    /// @param x0     The starting point; see the model-taking form above.
    /// @param warm   The payload; read, never retained.
    /// @param budget The caller's work ceiling.
    /// @return The solution.
    /// @throws std::invalid_argument on the classes the model-taking payload
    ///         overload above enumerates, less the model-box class this entry
    ///         does not re-check.
    SqpResult solve(NlpModelAggregate &bridge, const Vec &x0, const WarmStartData &warm,
                    SolveBudget budget = {});

    // --- Warm-start currency ---
    //
    // The same two entries the interior-point engine carries. This driver binds
    // nothing until a solve() call names a problem, so the currency's two checks
    // -- the block sizes and the stamp -- are made at solve entry, not at
    // staging.
    /// @brief The warm-start value of the last completed solve, in DECLARED
    ///        space.
    /// @return The captured value, by copy. Blocks, all at declared dimensions:
    ///         `primal_` is SqpSolution::x, `eq_lmults_` its lambda_e,
    ///         `iq_lmults_` its lambda_i and `bound_lmults_` its z, verbatim --
    ///         model space is declared space on this engine. `z` already is the
    ///         currency's z = zL - zU. The stamp is the bridge's declaration key
    ///         as of that solve's completion. No extensions: this engine produces
    ///         none, so `extensions_` reads empty.
    /// @throws std::logic_error if no solve has COMPLETED on this instance --
    ///         completed meaning a public solve() that returned.
    WarmStartData export_warm_start() const;

    // stage_warm_start() and the staged_warm_ / warm_staged_ pair it armed were
    // REMOVED in M6 W5 T8.5. `stage(p); solve(m, x0)` becomes `solve(m, x0, p)`
    // -- the payload overloads above -- which is the same solve with the same
    // checks in the same order, minus the one-shot state between the two calls.
    // The table is in docs/notes/2026-09-m6-w5-migration-guide.md.

  private:
    // The ledger-recording tail every public solve() overload shares: record
    // exactly one SqpSolveRecord if a ledger is attached, and return the result
    // unchanged. `wall_seconds` is measured by each caller around solve_impl.
    SqpSolution record_solve(SqpSolution out, double wall_seconds);

    // refuse_two_warm_sources() is gone with staging (M6 W5 T8.5): a call's
    // warm-start source is now exactly its own argument, so there is no second
    // source to contradict.

    // THE PAYLOAD'S ONE INGEST, shared by both bridge-taking payload overloads:
    // the against-the-problem checks (block lengths, then the declaration
    // stamp, both refusing in EVERY qp_mode), the kIpm tier seed, and the
    // translation of the declared-space currency into the native `SqpWarmStart`
    // the rest of this engine speaks. `x0` is read for the MULTIPLIERS-ONLY
    // form, whose `primal_` is empty and whose start is therefore the call's
    // own. Renamed from consume_staged_warm_start, which read a member.
    WarmStart consume_payload(const AggregateEvalSeam &seam, const NlpModelAggregate &bridge,
                              const Vec &x0, const WarmStartData &data);

    // The export's one capture at completion, taken after record_solve returns.
    // A failed internal-consistency check skips the capture and clears the marker
    // rather than throwing; none of those conditions is reachable today.
    void capture_completed_warm_start(SqpSolution &out, const AggregateEvalSeam &seam,
                                      const NlpModelAggregate &bridge);

    /// @brief The solve, wrapped: validates the arguments, writes
    ///        `sqp.solve.begin`, runs the body, writes `sqp.solve.end` from the
    ///        result, and returns it.
    ///
    /// Validation precedes `begin`, so a refused call writes nothing at all; an
    /// exception from the body skips the `end` emit by construction.
    ///
    /// @param seam         Every model quantity this loop reads arrives through
    ///                     it.
    /// @param bridge       Rides alongside `seam` because the restoration phase
    ///                     builds a different NlpModel around the model behind
    ///                     it.
    /// @param x0           The start point.
    /// @param warm         The ingested warm start.
    /// @param budget `budget.minor_budget <= 0` means no probe budget; the 4-argument `solve()`
    ///                     carries that contract.
    /// @return The assembled solution.
    SqpSolution solve_impl(AggregateEvalSeam &seam, NlpModelAggregate &bridge, const Vec &x0,
                           const WarmStart &warm, SolveBudget budget);

    /// @brief The major loop itself, entered only with validated arguments and
    ///        the strategy its caller built.
    ///
    /// Nothing here emits the whole-solve events.
    ///
    /// @param seam         The solve's evaluation seam.
    /// @param bridge       The model bridge the restoration phase rebuilds
    ///                     around.
    /// @param x0           The start point.
    /// @param warm         The ingested warm start.
    /// @param budget The caller's work ceiling; both members 0 means none.
    /// @param strategy     The globalization strategy, consumed.
    /// @return The assembled solution.
    SqpSolution solve_impl_body(AggregateEvalSeam &seam, NlpModelAggregate &bridge, const Vec &x0,
                                const WarmStart &warm, SolveBudget budget,
                                std::unique_ptr<GlobalizationStrategy> strategy);

    /// @brief The solve-scope state `solve_impl_body` owns, DEFINED IN THE .cpp.
    ///
    /// Forward-declared and nothing more: a `.cpp`-internal shape, not a surface,
    /// and no consumer of this header can name it.
    struct SolveState;

    /// @brief The pre-loop setup.
    ///
    /// Initialises `st` in place -- it never returns state, because the bundle
    /// holds a pointer into itself.
    ///
    /// @param st       The solve-scope state, written in place.
    /// @param seam     The solve's evaluation seam.
    /// @param x0       The start point.
    /// @param warm     The ingested warm start.
    /// @param strategy The globalization strategy, consumed.
    void prepare_solve(SolveState &st, AggregateEvalSeam &seam, const Vec &x0,
                       const WarmStart &warm, std::unique_ptr<GlobalizationStrategy> strategy);

    /// @brief The per-major routing bundle, DEFINED IN THE .cpp.
    ///
    /// Forward-declared and nothing more, exactly as `SolveState` above.
    struct MajorState;

    /// @brief The kSsn dispatch arm.
    ///
    /// Solves this major's subproblem on the semismooth-Newton tier and either
    /// writes `mj.qs` or hands the subproblem to the walk. It emits its own
    /// `qp.mode` line before returning, and it owns the deferred face
    /// refinement's single producer and both of its consumers.
    ///
    /// @param st The solve-scope state.
    /// @param mj This major's routing bundle; `mj.qs` is written on a usable
    ///           SSN exit.
    void route_through_ssn_tier(SolveState &st, MajorState &mj);

    /// @brief The kIpm dispatch arm -- the interior-point routing chain.
    /// @param st              The solve-scope state.
    /// @param mj              This major's routing bundle.
    /// @param seam            The evaluation seam, for the symbolic hoist's epoch.
    /// @param iter            This major's index, for the trace and the escape
    ///                        ladder.
    /// @param tr_shrink_retry True on a pass that did not rebuild the subproblem:
    ///                        it neither re-centres the cross-major carry nor
    ///                        charges the ladder.
    /// @param overrides       The caller's own walk levers, which the certified
    ///                        feasibility fallback runs with.
    ///
    /// This major's row and its `row_qp_mode` are on `mj`, not parameters. It
    /// emits every one of its own trace lines before returning.
    void route_through_ipqp_tier(SolveState &st, MajorState &mj, AggregateEvalSeam &seam,
                                 Index iter, bool tr_shrink_retry, const SolveOverrides &overrides);

    /// @brief The walk invocation -- the one shared successor of the dispatch,
    ///        reached from the kWalk arm, an IPQP retirement, an IPQP domain
    ///        decline, or an SSN hand-off.
    ///
    /// Not the elastic ladder's walk, which runs inside the kIpm arm with
    /// `walk_owns_this_qp` false and stays there.
    ///
    /// @param st        The solve-scope state.
    /// @param mj        This major's routing bundle; `mj.qs` is written here.
    /// @param warm      The ingested warm start.
    /// @param overrides The caller's own walk levers.
    /// @param offer_hot True to offer the retained hot handle to the walk.
    /// @param use_crash True to seed the first working set from the crash basis.
    void solve_with_walk(SolveState &st, MajorState &mj, const WarmStart &warm,
                         const SolveOverrides &overrides, bool offer_hot, bool use_crash);

    /// @brief What ONE MAJOR decided, and the only thing that crosses back out of
    ///        `run_major`.
    ///
    /// Payload-free by construction: no solution, vector, evaluation, KKT record,
    /// objective, warm start or restoration flag travels in an outcome. The
    /// finish provenance is in the tag because the four terminal exits select
    /// different activity sources.
    enum class MajorOutcome {
        /// The major is over and the loop advances; every update it owns
        /// (a shrink, a re-seed, a restoration resume) is already done.
        kContinue,
        /// The step was accepted and its row is already emitted. The caller
        /// runs the growth evidence, the radius growth, the SOC or direct
        /// commit, the counters and the re-centred seed.
        kCommitAccepted,
        /// The non-finite-KKT exit, which assembled `st.out` by hand: the
        /// caller checks and MOVES it, bypassing `finish`.
        kFinishManual,
        /// The ordinary terminal exit -- kOptimal or kMaxIter by
        /// `mj.converged` -- reporting the current iterate.
        kFinishCurrent,
        /// Budgeted mode's best-by-(h, f) iterate from `st.mb`.
        kFinishBudgetBest,
        /// A QP failure with no retry left: this QP's status and activity.
        kFinishQpFailure,
        /// A restoration exit whose requester has no usable QP activity (the
        /// elastic ladder's answer is in the augmented variables), so the
        /// PRIOR SEED is the fallback.
        kFinishRestorationSeed,
        /// A restoration exit whose requester's QP is in the original
        /// variables and still describes x unless restoration moved it.
        kFinishRestorationQp,
    };

    /// @brief What the restoration phase decided.
    ///
    /// A TAG, and nothing else: the exit payload it wrote lives on
    /// `SolveState::resto`, which is its one authoritative copy.
    enum class RestorationOutcome {
        /// A pre-run gate refused: no sub-solve ran.
        kRefused,
        /// The sub-solve reached a feasible enough point and the main loop
        /// resumes from it; every piece of solve state is already updated.
        kResumed,
        /// Every post-run terminal path, including the numerical-error one
        /// that adopts no point at all.
        kExited,
    };

    /// @brief What a restoration request site OFFERS as a start point.
    ///
    /// The point and, when the site has already measured it, its values-only
    /// bundle there. A null `values_ev` is the unmeasured route, the only arm
    /// that may query the model. Both are temporary borrows.
    struct RestorationCandidate {
        const Vec *x = nullptr;
        NlpEval *values_ev = nullptr;
    };

    /// @brief The restoration phase.
    ///
    /// The requesting row is pushed by the caller, after this returns, at all
    /// four call sites: `row.restoration_seed_used` is set inside and must be in
    /// the row that is emitted.
    ///
    /// @param st     The solve-scope state.
    /// @param mj     This major's routing bundle.
    /// @param seam   The solve's evaluation seam.
    /// @param bridge The model bridge the sub-solve's NlpModel is built around.
    /// @param iter   This major's index.
    /// @param cand   The point to restore from, and its values-only bundle when
    ///               the site has already measured it.
    /// @return What the phase decided.
    RestorationOutcome enter_restoration(SolveState &st, MajorState &mj, AggregateEvalSeam &seam,
                                         NlpModelAggregate &bridge, Index iter,
                                         RestorationCandidate cand = {nullptr, nullptr});

    /// @brief One major: the KKT measurement of the iterate it starts at,
    ///        through to the history row it emits.
    ///
    /// Holds all ten push sites, and every one of them returns its `MajorOutcome`
    /// from inside this function, so no caller-side effect ever precedes the
    /// return. The terminal check is the caller's.
    ///
    /// @param st           The solve-scope state.
    /// @param mj           This major's routing bundle.
    /// @param seam         The solve's evaluation seam.
    /// @param bridge       The model bridge.
    /// @param warm         The ingested warm start.
    /// @param budget The caller's work ceiling; both members 0 means none.
    /// @param iter         This major's index.
    /// @return What this major decided.
    MajorOutcome run_major(SolveState &st, MajorState &mj, AggregateEvalSeam &seam,
                           NlpModelAggregate &bridge, const WarmStart &warm, SolveBudget budget,
                           Index iter);

    /// @brief Builds ONE IterationEvent from the row being pushed and hands it
    ///        to the installed per-iteration callback (M6 W5 T8.6).
    ///
    /// CALLED FROM `push_history` AND NOWHERE ELSE, which is what the
    /// `events_fired == rows_pushed` identity there checks.
    ///
    /// EVERYTHING IT HANDS OUT IS IN CALLER UNITS. `exported` arrives already
    /// mapped; the three price blocks and the Lagrangian gradient behind the
    /// four shared diagnostics are mapped HERE, by problem_scaling.h's
    /// engine->caller multiplier map -- `lambda * s_row / sf`, `z / sf`,
    /// `grad_lag / sf`, `c / s_row` -- so the event describes the problem the
    /// caller posed, as design section 2.5's "multipliers and z have the scaling
    /// map applied" requires. (Not one of section 2.7's numbered behaviour
    /// changes: its (2) is kInterrupted and its (3) is the interior-point
    /// engine's moved dispatch -- corrected at fix1.)
    ///
    /// IT EVALUATES NOTHING, and it must not: an evaluation here would move
    /// `evals_full` on every row of every solve with a callback attached, which
    /// is exactly the identity the corpus leg's callback-attached run proves.
    /// On a SCALED solve that makes the event's four diagnostics the engine's
    /// own measurement MAPPED BACK, where `finish` re-measures the returned
    /// point unscaled; the two agree to floating-point rounding rather than
    /// bitwise, and the result's is the one the contract states.
    ///
    /// The four vector views the callback sees alias LOCALS of this function.
    ///
    /// EVERY VECTOR AND EVERY DIAGNOSTIC COMES FROM THE ROW'S OWN SNAPSHOT (M6
    /// W5 T8.6 fix1). `measure_iterate` takes the WHOLE measurement the event
    /// needs -- the point, the three price blocks in caller units and the four
    /// declared diagnostics -- at the moment the row is measured, and this
    /// function reads nothing else. Four of the ten push sites sit after an
    /// `enter_restoration` that has ALREADY replaced `st.x` and, on the resumed
    /// route, `st.ev`; reading either live there produced an event whose point
    /// and constraint residuals were the RESTORED iterate's while its `f`,
    /// `stationarity`, `step_norm` and prices were the stalled row's -- one
    /// event describing two points.
    ///
    /// @param st        The solve state; its latch and event count are written.
    /// @param exported  The row being pushed, already in caller units.
    /// @param mj        The major, for the snapshot taken at measure time.
    void fire_iteration_event(SolveState &st, const SqpIterate &exported, const MajorState &mj);

    /// @brief Invokes the installed callback with the in-flight guard held.
    ///
    /// THE ONE PLACE THE CALLABLE IS CALLED, so that set_iteration_callback()
    /// and clear_iteration_callback() called from INSIDE it can be deferred to
    /// the statement after it returns rather than destroying the callable
    /// during its own invocation (M6 W5 T8.6 fix1, the SQP lane's M1). Used by
    /// fire_iteration_event and by the restoration forwarder alike.
    CallbackAction invoke_iteration_callback(const IterationEvent &event);

    /// @brief Applies a set/clear deferred by invoke_iteration_callback.
    ///
    /// Called at the statement after the callback returns, and again at every
    /// solve entry -- the second is what catches a deferral left standing by a
    /// callback that departed by throwing.
    /// Applies a parked callback if this is the safe point it belongs to.
    /// @param at_solve_entry True at a public entry, which applies every parked
    ///        value; false after a callback invocation returns, which applies
    ///        only what that invocation itself parked.
    void apply_pending_iteration_callback(bool at_solve_entry = false);

    // --- The clock the delegating public overloads carry (M6 W5 T8.6 fix1) ---
    // ONE ENTRY POINT OWNS THE CLOCK AND THE DELEGATES RECEIVE IT. A
    // model-taking overload lays a bridge -- real work, inside the boundary
    // SqpResult::wall_seconds states -- and then delegates; before this, the
    // bridge-taking overload it delegated to overwrote `entry_time_` with its
    // own later stamp, so every IterationEvent::elapsed_seconds omitted the
    // bridge lay. These three carry the outer stamp down: `outer_entry` is
    // std::nullopt at a bridge-taking public entry (which then takes its own,
    // after its own refusals, exactly where it always did) and holds the
    // model-taking entry's stamp otherwise.
    SqpSolution solve_from_entry(NlpModelAggregate &bridge, const Vec &x0, SolveBudget budget,
                                 std::optional<std::chrono::steady_clock::time_point> outer_entry);
    SqpSolution solve_from_entry(NlpModelAggregate &bridge, const Vec &x0, const SqpWarmStart &warm,
                                 SolveBudget budget,
                                 std::optional<std::chrono::steady_clock::time_point> outer_entry);
    SqpSolution solve_from_entry(NlpModelAggregate &bridge, const Vec &x0,
                                 const WarmStartData &warm, SolveBudget budget,
                                 std::optional<std::chrono::steady_clock::time_point> outer_entry);

    // Reached only after the one-shot retry has been spent, and never with
    // kInfeasible (the elastic tier consumes that status upstream), which is why
    // the kInfeasible arm is kept only to keep the mapping total.
    static SolveStatus map_status(QpStatus qp_status);

    // The two halves of the shrink rule, kept as one pair so the floor test and
    // the shrink itself can never disagree about what the next radius is. A +inf
    // radius does NOT hit the floor -- its next value is tr_max.
    bool shrink_hits_floor(double delta) const;
    double shrunk_radius(double delta) const;

    // The radius the main loop resumes at after a restoration, and the floor under
    // the radius the restoration phase itself is given: a fraction of the caller's
    // own starting radius, +inf resolved to tr_max, never below the floor.
    double restoration_restart_radius() const;

    // The SSN engine, constructed on first use. `opts_.qp` is the SAME QpOptions
    // the walk was constructed with, so the two kernels are regularized
    // identically by construction.
    SsnEngine &ssn_engine();

    // The SsnOptions ONE subproblem is solved under. `fb_tol` tracks the tighter
    // of the driver's own kkt_tol and feas_tol, through `ssn_fb_tol_for`; every
    // other field is ssn_engine.h's own default. `prox_sigma_init` is the one
    // field this driver sets, and only on the first subproblem of a solve that
    // ingested a proximal carry (warm_start.h's `prox_sigma`).
    SsnOptions ssn_options(double prox_sigma_init) const;

    // The interior-point tier's engine, constructed on first use, on
    // `ssn_engine_`'s discipline. `opts_.qp` is the SAME QpOptions the walk and
    // the SSN tier were constructed with, so all three kernels read one
    // tolerance pair and one regularization pair.
    IpqpEngine &ipqp_engine();

    // The IpqpOptions ONE subproblem is solved under: forwarded, not derived,
    // with one exception. `ipqp_hoist_symbolic` is forced false for one entry
    // when the model's structure epoch has moved since the analysis the engine
    // holds -- a narrowing, never a widening; a caller who set the kill switch
    // false keeps it false on every entry.
    IpqpOptions ipqp_options(bool structure_epoch_moved) const;

    /// @brief Routes one subproblem to the SSN tier, warm-graded from a
    ///        finished IPQP solve.
    /// @param qp       the subproblem.
    /// @param ires     the tier's result, the warm grade's source.
    /// @param delta    this trial's trust-region radius.
    /// @param ssn_prox_ingested the solve's one-shot proximal carry, CONSUMED
    ///        (set to 0) whether or not the ladder used it.
    /// @param ssn_budget_charge the probe budget's SSN accumulator, charged.
    /// @param counters the solve's running counters, updated.
    /// @param qs       written with the SSN's step iff one was usable.
    /// @return true iff the walk owns this subproblem after all (the SSN exit
    ///         was not usable), i.e. `walk_owns_this_qp`.
    bool route_through_ssn_warm_grade(const QpProblem &qp, const IpqpResult &ires, double delta,
                                      double &ssn_prox_ingested, Index &ssn_budget_charge,
                                      SqpCounters &counters, QpSolution &qs);

    /// @brief Builds the WarmStart every exit of solve_impl attaches to
    ///        SqpSolution::warm_start.
    ///
    /// @param seam              The solve's evaluation seam.
    /// @param activity          The best-known QpSolution whose activity still
    ///                          describes the point being returned, or nullptr.
    /// @param qp                The subproblem the activity belongs to.
    /// @param qp_built          False with a non-null `probe_ev`/`probe_x` pays
    ///                          one extra eval_hess to hash a probe subproblem
    ///                          at the exit point.
    /// @param probe_ev          The probe evaluation, or nullptr to emit a cold
    ///                          object instead.
    /// @param probe_x           The point the probe is taken at.
    /// @param delta             The trust-region radius to record.
    /// @param dual_mu_eff       The effective dual regularization.
    /// @param primal_delta_eff  The effective primal regularization.
    /// @param strategy          The globalization strategy, read for its state.
    /// @param hot               The hot handle to carry, if any.
    /// @return The warm start to attach to `SqpSolution::warm_start`.
    /// @see docs/notes/2026-09-header-prose-archive.md §sqp_driver.h
    static WarmStart make_warm_start(AggregateEvalSeam &seam, const QpSolution *activity,
                                     const QpProblem &qp, bool qp_built, const NlpEval *probe_ev,
                                     const Vec *probe_x, double delta, double dual_mu_eff,
                                     double primal_delta_eff, const GlobalizationStrategy *strategy,
                                     std::shared_ptr<const HotState> hot);

    /// @brief Assembles the final SqpSolution at an exit -- status, x,
    ///        multipliers, KKT record, f, warm start -- and maps every exported
    ///        quantity back to the caller's units when the solve ran scaled.
    /// @param seam The solve's evaluation seam; carries the installed factors.
    /// @param out      The partially assembled solution, consumed.
    /// @param status   The exit status to report.
    /// @param x        The iterate to report.
    /// @param lambda_e The equality multipliers to report.
    /// @param lambda_i The inequality multipliers to report.
    /// @param kkt      The KKT record to report.
    /// @param f        The objective value to report.
    /// @param warm     The warm start to attach, consumed.
    /// @param multipliers_are_caller_scale True only at the restoration exit that
    ///        adopts the sub-solve's own multipliers and bound prices, which are
    ///        already in the caller's units; every other exit leaves it false.
    /// @return The finished solution.
    /// @param stash The SHARED declared diagnostics of the point being
    ///              returned, taken where its evaluation was live. Never null;
    ///              a stash with `measured == false` is how an exit says the
    ///              point could not be measured, and the result then reports
    ///              NaN and empty blocks rather than zeros. On a SCALED solve
    ///              this is ignored in favour of the caller-scale
    ///              re-measurement `finish` already takes.
    SqpSolution finish(AggregateEvalSeam &seam, SqpSolution out, SolveStatus status, const Vec &x,
                       const Vec &lambda_e, const Vec &lambda_i, const SqpKkt &kkt, double f,
                       WarmStart warm, const DeclaredDiagnosticsStash &stash,
                       bool multipliers_are_caller_scale = false);

    /// @brief Computes this solve's scaling factors and installs them on @p seam,
    ///        or leaves the seam unscaled when `opts_.enable_scaling` is false.
    ///
    /// Called after solve_impl's argument validation and before the major loop,
    /// so a mis-sized or non-finite x0 is still refused as it always was. Costs
    /// one extra model evaluation, and only when the toggle is on.
    ///
    /// @param seam The solve's evaluation seam, unscaled on entry.
    /// @param x0   The start point the factors are read at.
    /// @return The count of extra derivative evaluations spent (0 or 1), for the
    ///         caller to charge to its own counters.
    Index install_solve_scaling(AggregateEvalSeam &seam, const Vec &x0) const;

    SqpOptions opts_;
    // ONE engine for the whole driver, deliberately: it is what makes warm seeding
    // possible across majors, and it makes SqpDriver exactly as thread-unsafe as
    // QpEngine -- use one driver per thread.
    //
    // HELD BY unique_ptr since M6 W5 T8.3, and only because set_options() has to
    // REPLACE it: QpEngine owns a live backend session and declares no
    // assignment, so the transactional swap needs a pointer. Never null between
    // constructor and destructor; every use dereferences without a check.
    std::unique_ptr<QpEngine> engine_;
    // The semismooth-Newton tier's engine, LAZILY CONSTRUCTED and never touched
    // at the shipped default: an SsnEngine owns a live KktFactor, so a plain
    // member would allocate a backend session on every driver. One engine per
    // driver is what keeps its symbolic analysis reusable across majors.
    std::unique_ptr<SsnEngine> ssn_engine_;
    // The interior-point tier's engine, on `ssn_engine_`'s discipline and for the
    // same two reasons: a live backend session per driver, and one symbolic
    // analysis per solve.
    std::unique_ptr<IpqpEngine> ipqp_engine_;

    // --- The console composition (M6 W5 T8.7) ---
    //
    // ON THE DRIVER, NOT ON AN ENGINE, and that matters: `set_options` drops
    // both lazy engines, so anything held there would be rebuilt behind the
    // caller's back. Neither sink type is copyable or movable and neither is
    // `SqpDriver`, so the pointers `fanout_` holds into this object are stable.
    //
    // Both are REBUILT at every solve entry from `opts_.common.print_level`,
    // `user_trace_` and `is_restoration_sub_driver_`, and both stay null on a
    // driver that never prints -- every driver at the SQP's default
    // `print_level` of 3, and the restoration sub-driver at any level.
    std::unique_ptr<ConsoleTraceSink> console_;
    std::unique_ptr<FanOutTraceSink> fanout_;
    // The trace sink, held here because `ipqp_engine_` is lazy; `ipqp_engine()`
    // applies it at first-use construction.
    // THE SINK THE CALLER ATTACHED (M6 W5 T8.7). `attach_trace` writes THIS
    // one; `ipqp_trace_` below is what a solve actually writes to.
    TraceSink *user_trace_ = nullptr;
    // THE EFFECTIVE SINK: `user_trace_` alone, this driver's console alone, or
    // a fan-out over both. Composed at solve entry and read by every emit site,
    // by the lazily-created IPQP engine and by the restoration sub-driver -- so
    // a nested solve's depth-1 lines reach the caller's sink exactly as they do
    // with no console attached. The name predates the composition and is kept
    // so the fifteen read sites did not have to move for a rename.
    //
    // WHAT IT HOLDS WHEN (lane review, the "Smaller" note): BETWEEN solves it
    // holds the USER's pointer, which `attach_trace` writes here as well so a
    // caller reading the effect of its own call sees what it attached; only
    // DURING a solve does it hold the effective sink. Read it as "the sink the
    // current solve writes to", never as "the composition".
    TraceSink *ipqp_trace_ = nullptr;

    // The two driver-owned emit sites: `ipqp.route` and `qp.mode`, the kIpm
    // dispatch arm's own facts.
    void emit_trace_route(const IpqpTraceRouteEvent &event) const;
    void emit_trace_qp_mode(const QpModeTraceEvent &event) const;
    void emit_trace_fallback_verdict(const SqpFallbackVerdictTraceEvent &event) const;
    void emit_trace_sqp_major(const SqpMajorTraceEvent &event) const;
    // The FIRST kIpm subproblem's seed, built from the staged currency's
    // unflattened zL/zU/mu. Empty when nothing is staged, the mode is not kIpm,
    // or the grade came out cold.
    std::optional<IpqpSeed> ipqp_staged_seed_;
    // The proximal level to EXPORT on this solve's WarmStart, and the point it was
    // reached at (warm_start.h's `prox_sigma`/`prox_center_*` block). A member
    // rather than a local because solve_impl has a dozen exits and
    // make_warm_start is static; reset at the top of every solve_impl call.
    double ssn_prox_sigma_out_ = 0.0;
    // The CENTRE's own high-water mark, which is NOT ssn_prox_sigma_out_: the
    // level is taken over every SSN subproblem, the centre only over CERTIFYING
    // ones.
    double ssn_prox_center_sigma_out_ = 0.0;
    Vec ssn_prox_center_x_out_, ssn_prox_center_lambda_out_;
    // False on the driver the RESTORATION PHASE constructs for itself, which
    // bounds the recursion at one level. See the private constructor.
    bool allow_restoration_ = true;

    // True on that same driver and on NO other (M6 W5 T8.7 fix1): the ONE
    // driver in this library that never prints, whatever `common.print_level`
    // says. Read only where the effective sink is composed, at solve entry.
    // Every other driver -- restoration allowed or not -- builds its console at
    // the tiers the print level names.
    bool is_restoration_sub_driver_ = false;

    // Is a public solve on THIS driver currently running? Set by an RAII guard
    // at solve_impl()'s entry -- the ONE place all four public solve() overloads
    // funnel through exactly once, since they NEST -- and cleared on every exit,
    // a throw included. Read by set_options() and attach_trace(), which refuse
    // to replace the options -- and the sink -- a solve is running under. The
    // restoration phase builds a DISTINCT nested driver of this class, so it
    // never re-enters this object's guard. (The INTERIOR-POINT engine's
    // restoration is not like this: it runs in place on the one solver. Its own
    // guard comment says so.)
    bool solve_in_flight_ = false;

    // See attach_ledger's doc comment above for the whole contract;
    // nullptr (ledger_) is "off", exactly like QpEngine's own ledger_.
    Ledger *ledger_ = nullptr;
    std::string label_prefix_;
    Index solve_counter_ = 0;

    // --- Warm-start currency state ---
    // Both per-instance: the restoration phase's nested driver shares neither.
    //
    // The value captured at the end of the last COMPLETED solve, valid only while
    // solve_completed_ is true.
    WarmStartData completed_warm_;
    // True once a public solve() has RETURNED on this instance. A call that
    // threw never reaches the capture and so does not arm this; convergence
    // is NOT required (the caller reads the verdict from SqpSolution::status).
    bool solve_completed_ = false;
    // staged_warm_ / warm_staged_ went with stage_warm_start (M6 W5 T8.5).
    //
    // THE PAYLOAD'S ONE PIECE OF CALL-SCOPE STATE, in their place: how many
    // polish extensions the ingest DROPPED because the payload was the
    // multipliers-only form. It is a member rather than a local because
    // consume_payload runs in the public entry's frame, one call above
    // solve_impl, while the counter it feeds is written by record_solve -- the
    // ONE point every public overload funnels through. Reset at the top of
    // every consume (payload and cold alike), so nothing leaks between solves.
    //
    // READ TWICE (M6 W5 T8.5 fix1), in solve_impl just before the
    // `sqp.solve.end` trace emission and again in record_solve. The event
    // carries the whole counters object, so a single write in record_solve --
    // which runs ABOVE the emission -- traced 0 where the result and the
    // ledger reported 1. Nothing between the two reads can move the member.
    Index payload_polish_ignored_ = 0;

    // --- The shared per-iteration callback (M6 W5 T8.6) ---
    // No arming flag beside it: an installed std::function is armed and an
    // empty one is not, which is the whole of clear_iteration_callback().
    // NOT carried into the restoration sub-driver's `opts_`; the sub-driver is
    // handed a FORWARDER instead (see enter_restoration), which stamps the
    // depth and latches a stop on THIS driver.
    IterationCallback iteration_callback_;

    // THE IN-FLIGHT GUARD AND THE DEFERRED CHANGE (M6 W5 T8.6 fix1, the SQP
    // lane's M1). `callback_in_flight_` is true for exactly the duration of one
    // invocation of `iteration_callback_`; a set_/clear_iteration_callback()
    // made in that window parks its new value here instead of assigning the
    // std::function that is running, which would destroy the callable's storage
    // -- and its captures -- underneath itself. An ENGAGED optional holding an
    // EMPTY function is a deferred clear; engaged and non-empty is a deferred
    // replacement; disengaged is "nothing pending".
    bool callback_in_flight_ = false;
    std::optional<IterationCallback> pending_callback_;
    // WHICH SAFE POINT the parked value belongs to (M6 W5 T8.7b). False: the
    // statement after the running callback returns. True: the next solve's
    // entry, which is where a value parked by a SINK method is applied.
    bool pending_callback_at_entry_ = false;

    // When this solve's public entry was taken, for
    // IterationEvent::elapsed_seconds. Written ONCE PER CALL, by the outermost
    // public overload the caller entered (M6 W5 T8.6 fix1): a model-taking
    // entry stamps it before it lays its bridge and carries the stamp down
    // through solve_from_entry, so the bridge lay is INSIDE the boundary, on
    // the same footing as SqpResult::wall_seconds. A bridge-taking entry stamps
    // it after its own refusals, where it always did. INFORMATIONAL, never
    // asserted (CLAUDE.md section 7).
    std::chrono::steady_clock::time_point entry_time_{};
};

// Defined in src/drivers/sqp_print.cpp alongside format_iteration_table.
/// @brief Human-readable name of a step verdict, for the iteration printer.
/// @param v The verdict.
/// @return A static string; nothing in the driver branches on it.
const char *to_string(StepVerdict v);

/// @brief Renders `sol.history` as an fmt-based iteration table, one row per
///        SqpIterate.
/// @param sol A finished solve.
/// @return The table, ready to print.
std::string format_iteration_table(const SqpSolution &sol);

// THE FIVE PIECES `format_iteration_table` IS BUILT FROM (M6 W5 T8.7), also
// defined in src/drivers/sqp_print.cpp. `ConsoleTraceSink` renders the SAME
// table LIVE from the event stream and reaches each piece as its event
// arrives, so the two renderers share one copy of every field width instead of
// each holding its own -- which is what lets the console be pinned byte for
// byte against `format_iteration_table` on a live solve.

/// @brief The table's header line plus its rule, both newline-terminated.
std::string sqp_iteration_table_head();

/// @brief One table row, newline-terminated. A row with `qp_solved == false`
///        renders "-" in the verdict and the two QP columns.
/// @param row The history row.
std::string sqp_iteration_table_row(const SqpIterate &row);

/// @brief The trailer's `Status` line, with its leading blank line.
/// @param status The solve's verdict.
std::string sqp_iteration_table_status_line(SolveStatus status);

/// @brief The trailer's `Start Level` line.
/// @param level `SqpCounters::start_level_used` -- the RESOLVED level.
std::string sqp_iteration_table_start_level_line(StartLevel level);

/// @brief The trailer's `Scaling` line; "Scaling: off" when inactive.
/// @param active              `SqpScalingReport::active`.
/// @param obj                 The objective factor.
/// @param row_min             The smallest constraint-row factor.
/// @param row_max             The largest constraint-row factor.
/// @param scaled_kkt_residual The residual the convergence test read.
std::string sqp_iteration_table_scaling_line(bool active, double obj, double row_min,
                                             double row_max, double scaled_kkt_residual);

} // namespace hven::solvers
