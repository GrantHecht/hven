// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// sqp_solver_types.h — the plain data types the SQP driver produces and consumes:
// status, options, counters, per-major history, solution. The driver itself
// (the loop, the subproblem construction and the KKT measure) lives in
// sqp_solver.h; nothing in this file does any work.
//
// This file is to sqp_solver.h what qp_types.h is to walk_engine.h.

#include <functional>
#include <iosfwd>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/qp/qp_types.h>
#include <hven/solver/common_options.h>
#include <hven/solver/iteration_record.h>
#include <hven/solver/solve_result.h>
#include <hven/solver/step_verdict.h>
#include <hven/sqp/sqp_options.h>
#include <hven/warmstart/sqp_warm_start.h>

namespace hven {

// The full-step-first warm rule's two constants. Both are watchdog thresholds:
// they bound how long the driver may keep taking undamped steps under
// funnel.h's full-step mode before restoring the best iterate it saw and
// handing the solve back to the funnel.
//
// kWarmResidualGrowthMax = 2 consecutive majors with a growing max(stationarity, feasibility,
// complementarity); kWarmFullStepWindow = 5 majors without a new best max(stationarity,
// feasibility, complementarity).
//
// Changing either is a behaviour change on every warm solve: the warm-start
// suite pins the majors of both a converging and a watchdog-restored run
// against these exact values.
// @see docs/notes/2026-09-header-prose-archive.md §sqp_solver_types.h

/// Watchdog threshold: consecutive majors with growing max(stationarity,
/// feasibility, complementarity) tolerated
/// under the full-step mode before it restores the best iterate.
inline constexpr Index kWarmResidualGrowthMax = 2;

/// Watchdog threshold: majors without a new best max(stationarity, feasibility,
/// complementarity) tolerated under
/// the full-step mode before it restores the best iterate.
inline constexpr Index kWarmFullStepWindow = 5;

/// @brief Relative margin below which an active row's multiplier -- or an
///        inactive row's slack -- counts as weak/near activity in the per-major
///        census.
///
/// A CONSTANT, NOT A SETTING: nothing in this library reads or branches on either
/// count, so there is no behaviour for a caller to tune.
///
/// It measures strict complementarity BY MAGNITUDE: a row is weakly active when
/// it is in the working set at a price negligible beside the largest one, and
/// nearly active when it is out of the working set at a slack negligible beside
/// the largest one.
///
/// It is NOT the SSN's uncertain set: that set is the dimensionless kink band
/// with leave hysteresis (ssn_engine.h), which reads a PURE state at `s == 0` for
/// any lambda and so never flags a small multiplier on an active row.
inline constexpr double kWeakActivityMargin = 1e-6;

/// @brief What the problem-scaling layer did to a solve, reported on its
///        solution.
///
/// A DIAGNOSTIC, NOT A COUNTER, and deliberately not a member of SqpCounters: a
/// unit factor neither sums nor peaks meaningfully across the restoration fold,
/// and the sub-solve runs unscaled by design. An INACTIVE report is the identity,
/// field for field, so a reader never has to branch on `active`.
struct SqpScalingReport {
    /// True when the ROW/OBJECTIVE layer ran, i.e. `SqpOptions::row_scaling` was
    /// on -- or `column_scaling == kAuto` fired, which engages it -- and factors were
    /// installed.
    bool active = false;
    /// The objective factor `sf`. Multiply a caller-scale objective by it to
    /// reach the engine's; divide to come back. 1.0 when inactive.
    double objective = 1.0;
    /// Largest and smallest constraint-row factor over the equality and
    /// inequality blocks together, or 1.0 when the problem has no rows (a
    /// bound-constrained problem has nothing to equilibrate). Their ratio is the
    /// row conditioning the layer removed.
    double row_max = 1.0;
    double row_min = 1.0;
    /// THE SCALED RESIDUAL THE CONVERGENCE TEST ACTUALLY READ, at the returned
    /// point -- the counterpart of SqpResult::kkt_residual, which is on the
    /// CALLER's scale. On a solve that ran in neither layer the two are the same
    /// number by construction; on a column-scaled one (`columns_active`) this is
    /// the residual in the scaled variables x~, whichever row layer ran. On a
    /// scaled one their difference is exactly the gap SqpOptions::row_scaling
    /// discloses, and reporting both is what makes it computable. NaN whenever
    /// SqpResult::kkt_residual is NaN, for the same reason: nothing was measured.
    double scaled_kkt_residual = std::numeric_limits<double>::quiet_NaN();
    /// THE COLUMN LAYER (SqpOptions::column_scaling). True when the solve
    /// ran in scaled variables. `column_min`/`column_max` are the installed factors'
    /// extremes over the columns that carry Jacobian data, 1.0 when none was
    /// installed. On a CARRIED decision (SqpWarmStart::column_factors) min, max and
    /// spread are over every variable, identity entries included, and a carried
    /// "unscaled" decision reports spread 1. `column_spread` is the spread the rule COMPUTED
    /// whenever it ran -- also when kAuto's gate did not fire, which is how a caller reads why --
    /// and 1.0 under kOff.
    bool columns_active = false;
    double column_min = 1.0;
    double column_max = 1.0;
    double column_spread = 1.0;
};

/// @brief Why an equality-basis split dropped nothing although it was computed
/// (`detail/sqp/equality_basis.h`, EqualityBasisRefusal, one for one).
enum class SqpEqualityBasisRefusal {
    kNone,        ///< No computation was refused.
    kSpectrum,    ///< No gap, a failed retained-set check, reconstruction or coefficient cap.
    kNonFinite,   ///< A norm, scaled entry, coefficient or Gram value was not finite.
    kLift,        ///< The lift system K failed its factorization checks or probe.
    kCertificate, ///< A certificate factorization's evidence was not clean.
    kSubspace,    ///< The null-basis iteration did not confirm the count.
    kFill,        ///< The Gram matrix would be too dense to form.
};

/// @brief SqpOptions::equality_basis's account of one solve (SqpResult::equality_basis).
/// Row indices are the model's equality rows. "Every computation" below means every
/// split the solve computed plus the warm structural probe's own, when it ran.
struct SqpEqualityBasisReport {
    /// Most dropped-row identities split_dropped_rows carries.
    static constexpr Index kSplitDroppedRowsCap = 64;

    /// A split was computed at least once.
    bool computed = false;
    /// The first split was refused (no pivot gap, a failed check) and dropped nothing.
    bool undecidable = false;
    /// Computations that were refused (kUndecidable), over every computation.
    Index undecidable_count = 0;
    /// The reason of the last refused computation (kNone when none was).
    SqpEqualityBasisRefusal last_refusal = SqpEqualityBasisRefusal::kNone;
    /// |D| of the last split the solve computed (the probe's excluded), whatever
    /// happened to it afterwards (a switch-off, hand-off or refusal clears
    /// dropped_rows, not this).
    Index split_dropped_count = 0;
    /// That split's dropped rows, increasing, the first kSplitDroppedRowsCap of them.
    std::vector<Index> split_dropped_rows;
    /// Rows dropped by the split in force at exit.
    std::vector<Index> dropped_rows;
    /// Largest number of rows any split dropped.
    Index dropped_peak = 0;
    /// Splits computed after the first one.
    Index recomputes = 0;
    /// Numeric factorizations of the equilibrated Gram matrix over every split
    /// computed (a warm probe's included): 1 per full-rank split on the certificate's
    /// fast path, 2 when full rank is certified at the lower shift, 4 per reducing split.
    Index gram_factorizations = 0;
    /// Perturbed pivots the backend reported, summed over every factorization
    /// gram_factorizations counts (a nonzero count refuses its split with
    /// kCertificate, so 0 is the expectation and the IPARM-SURFACE evidence).
    Index gram_perturbed_pivots = 0;
    /// Of those factorizations, the ones whose backend reported no perturbed-pivot
    /// count (0 on MKL; Accelerate UNOBSERVED).
    Index gram_perturbation_unobserved = 0;
    /// Three consecutive majors recomputed; the QPs carried every row thereafter.
    bool switched_off = false;
    /// Splits refused because a norm, coefficient or Gram value was not finite.
    Index nonfinite_refusals = 0;
    /// Lift/projection failures: splits refused because K = [I C; C^T -I] failed
    /// its factorization checks or probe (kLift), plus every lift or projection solve
    /// that failed its forward-error estimate mid-solve (the SOC and QP projections, the
    /// two commit lifts, the SSN prox-centre lift), plus warm_probe_refusals; after a
    /// mid-solve failure the QPs carry every row.
    Index lift_refusals = 0;
    /// The warm structural probe's own split refused with kLift, or its projection
    /// failed its estimate (0 or 1; counted in lift_refusals too). THE WARM LEVEL
    /// DEGRADES: the probe QP stays unreduced, so its structural hash cannot match a
    /// warm start taken from a reducing solve, and the start resolves below kWarm
    /// (kSeeded when the values are ingestible). Safe, never silent.
    Index warm_probe_refusals = 0;
    /// lift_refusals by site; the five plus warm_probe_refusals sum to lift_refusals.
    struct LiftRefusalSites {
        Index compute = 0;        ///< kLift refusals of the solve's own splits.
        Index qp_projection = 0;  ///< The QP subproblem's rhs projection.
        Index soc_projection = 0; ///< The SOC subproblem's rhs projection.
        Index commit_lift = 0;    ///< The two commit lifts (direct and SOC acceptance).
        Index prox_centre = 0;    ///< The SSN prox-centre lift.
    };
    LiftRefusalSites lift_refusal_sites;
    /// Majors at which the QPs' equality rows changed; each dropped the warm state
    /// held in row coordinates (the QP seed, the IPQP carry, a staged IPQP seed).
    Index split_changes = 0;
    /// A zero step at a suspected inconsistency returned every row to the QPs for
    /// the rest of the solve.
    bool handed_off = false;
    /// The dropped row found inconsistent on two-iterate evidence (the exit is
    /// kInfeasible), or -1. Evidence, not a proof: identical Jacobian values at two
    /// iterates do not establish that the rows are linear.
    Index inconsistent_row = -1;
    /// Its combination gap, in the solver's units.
    double inconsistent_gap = 0.0;
    /// The retained rows its reconstruction uses.
    std::vector<Index> inconsistent_support;
    /// The two majors whose evidence found it.
    Index evidence_major_first = -1;
    Index evidence_major_second = -1;
    /// Largest |gap| of a dropped row at any major, in the solver's units.
    double max_gap = 0.0;
};

/// @brief Where one SQP call spent its wall-clock seconds; `IpmTiming`'s counterpart.
///
/// INFORMATIONAL, NEVER ASSERTED -- counters, not timings, are this project's
/// currency of correctness. Measured on std::chrono::steady_clock.
struct SqpTiming {
    /// Seconds inside this solve's engine alone -- never model construction, the
    /// bridge or seam lay, the staged-value ingest or the ledger bookkeeping. The
    /// base's `wall_seconds` is the whole public call. 0 until a solve returns.
    double engine_seconds = 0.0;
};

/// @brief The SQP engine's own terminal KKT measurement, taken at the RETURNED
///        (x, lambda_e, lambda_i) by the same evaluate_kkt call the convergence
///        test read.
///
/// These are in the space the engine's convergence test gated in; a tolerance
/// comparison must be made against the number the test actually read. The
/// shared SolveResult diagnostics are a different, declared-space measurement.
///
/// All four are NaN on the non-finite-iterate kNumericalError exit: nothing was
/// measured there. On a certified kInfeasible exit they measure the problem's
/// own KKT conditions at the returned (infeasible) point, so `feasibility` is
/// large by construction and `stationarity` is the ordinary grad-L measure, not
/// the subgradient certificate's residual.
///
/// One qualification, on the engine's own columns: when
/// `counters.ssn.ssn_sign_swept > 0` the sign sweep clamped negative inequality
/// prices AFTER this measurement, so these describe the PRE-SWEEP multipliers
/// while `lambda` holds the swept ones; `stationarity` is then optimistic by at
/// most `ssn_sign_sweep_max * ||Ji||inf`, and `complementarity` can only be
/// over-stated. Not on a column-scaled solve (`scaling.columns_active`): there
/// the four are re-measured on the caller's model at the returned, post-sweep
/// multipliers. The sweep does not reach the shared base: where it moved a price
/// after the measurement the base's `stationarity` and `complementarity` are
/// NaN rather than describing prices the caller is not being handed.
struct SqpConvergence {
    /// Reduced/projected ||grad L||inf at the returned point.
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    /// max(||cE||inf, max(cI)+, bound violation) at the returned point.
    double feasibility = std::numeric_limits<double>::quiet_NaN();
    /// max_j |lambda_i(j) * cI_j(x)| at the returned point. Gated at common.optimality_tol on
    /// a kOptimal exit -- see IterationRecord::complementarity.
    double complementarity = std::numeric_limits<double>::quiet_NaN();
    /// max(stationarity, feasibility). Not the whole convergence test:
    /// complementarity is gated separately.
    double kkt_residual = std::numeric_limits<double>::quiet_NaN();
};

/// @brief Result of a whole SQP solve: the base's answer plus this engine's own members.
///
/// MULTIPLIERS. `lambda` is one vector over the problem's rows, in the sign
/// convention documented at SolveResult::lambda, and is the NLP's multiplier vector at the returned
/// point with no flip. `z` is the model-implied bound multiplier at the returned
/// point, computed from the Lagrangian gradient there.
///
/// On a non-kOptimal exit `x` is the FINAL ITERATE reached and the multipliers
/// are whatever the last successful subproblem priced -- NOT cleared, because they
/// are the caller's evidence about where the solve stopped.
///
/// On a certified kInfeasible exit the multipliers mean something stronger and
/// DIFFERENT, and must not be read as prices of the NLP's own constraints: they
/// are the RESTORATION problem's multipliers, a SUBGRADIENT CERTIFICATE that the
/// returned x is a stationary point of h(x) = the sum of the rows' violations
/// (the l1 norm of the equality residuals plus the positive parts of the
/// inequality ones). Over the rows: the Jacobian-transpose product of `lambda`
/// minus `z` is zero; an equality row's multiplier lies in [-1, 1] and equals the
/// sign of its residual wherever that is nonzero; an inequality row's multiplier
/// lies in [0, 1], is 1 where the row is violated and 0 where it is slack. There
/// is no objective-gradient term, which is what makes it a certificate of
/// infeasibility rather than of optimality. `z` on this exit is likewise the
/// restoration problem's bound price.
///
/// The one exception is the non-finite-iterate kNumericalError exit, where
/// `lambda` and `z` are cleared: at a NaN iterate nothing was measured. `f` is
/// still reported as the model returned it, which is to say possibly NaN.
///
/// `history` describes every iterate visited; whether the last row is an iterate
/// row or a failing-subproblem row depends on the exit -- see SqpCounters.
struct SqpResult : SolveResult {
    // The base carries the ANSWER -- status, message, x, lambda, z, f, the
    // shared declared diagnostics, g, iterations, work, wall_seconds and the
    // warm-start snapshot -- in the problem's row space and caller units
    // (solver/solve_result.h). Everything below is this engine's own.

    /// @brief Work spent by this solve, restoration folded in.
    SqpCounters counters;
    /// One `IterationRecord` per iterate visited (`sqp` block set), always on: the depth-0
    /// `iter` trace stream row for row. Whether the last row is an iterate row or a
    /// failing-subproblem row depends on the exit -- see SqpCounters.
    std::vector<IterationRecord> history;

    /// @brief The engine's own terminal KKT measurement; see SqpConvergence.
    SqpConvergence convergence;

    /// @brief Where the solve spent its wall-clock seconds (informational).
    SqpTiming timing;

    /// @brief What the problem-scaling layer did to this solve; the identity
    ///        when it was off, which is the shipped default.
    ///
    /// READ IT BEFORE COMPARING `f`, the multiplier blocks or the four terminal
    /// residuals against another solve's: all of those are reported on the
    /// CALLER's scale whatever this says, so they are directly comparable -- but
    /// `scaled_kkt_residual` is the number the convergence test gated on, and on
    /// an active solve it is the one that was required to be <= common.optimality_tol.
    SqpScalingReport scaling;

    /// What SqpOptions::equality_basis did on this solve.
    SqpEqualityBasisReport equality_basis;

    /// @brief SQP ONLY: the solve's exit state as this engine's own native
    ///        warm-start object, for a LATER solve of a nearby problem to feed
    ///        back through the labelled native entry
    ///        `SqpSolver::solve(problem, x0, const SqpWarmStart &)`.
    ///
    /// Populated on EVERY exit of SqpSolver::solve(), including a failed one,
    /// from the best-known iterate. Populated is NOT the same as
    /// `sqp_warm_start.valid()`: one exit -- a start point the model could not
    /// evaluate -- holds a state for the engine's own bookkeeping but reports
    /// valid() == false, because feeding that point back would override the
    /// caller's own corrected x0.
    ///
    /// The base's `warm_start` carries the SHARED currency (WarmStart) taken at
    /// the same exit; this is the engine-native one, and only this engine reads
    /// it.
    SqpWarmStart sqp_warm_start{SqpWarmStart::Cold{}};
};

/// @brief Writes every counter @p r holds to @p os, one `name = value` line each: the twin of
///        `dump_options`, generated from the same X-macro tables as `solve.end.counters`.
///
/// Lines, in order: the direct `SqpCounters` fields in declaration order, then the `ssn` block's
/// fields under `ssn.` and the `ipqp` block's under `ipqp.`. The text format is the one stated
/// at `dump_counters(const IpmResult &, std::ostream &)` (ipm/ipm_solver_types.h).
///
/// Counters only: the per-QP sums a harness takes over a `Ledger` are not here.
void dump_counters(const SqpResult &r, std::ostream &os);

} // namespace hven
