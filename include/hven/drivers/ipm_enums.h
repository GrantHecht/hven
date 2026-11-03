// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The interior-point engine's option enums, including the phase
///        vocabulary `IpmPhase`. Reached through drivers/ipm_options.h; a header of
///        its own so that the trace interface can name them without the options
///        struct.

namespace hven::solvers {

/// @brief One phase of an interior-point solve, as `IpmOptions::phases` names it.
///
/// Declaration order is contractual: it is what `static_cast<int>` and any packed
/// diagnostic column print.
enum class IpmPhase {
    /// The optimality phase.
    kOptimize = 0,
    /// The feasibility ("solve the equations") phase, run in
    /// `IpmAdvancedOptions::solve_phase_mode`.
    kSolve = 1,
};

/// @brief How a primal variable whose declared lower and upper bounds are equal
///        is handed to the solver.
///
/// All three reach the same solution on a well-posed problem; they differ in the
/// size of the factorized system and in how exactly the variable sits at its
/// value.
enum class FixedVariableTreatment {
    /// Removed from the optimization and pinned at its value: the system is one
    /// row and column narrower per fixed variable, and the value is exact. The
    /// default.
    kMakeParameter,
    /// Kept free and held by one internal equality row per fixed variable,
    /// appended after every declared row: the system is wider, every declared
    /// row keeps its index, and the value is met to the feasibility tolerance.
    kMakeConstraint,
    /// Kept two-sided with its bounds pushed apart by `bound_relax_factor`
    /// (which must then be > 0) and held near its value by the barrier.
    kRelaxBounds,
};

/// @brief Fill-reducing ordering of the KKT matrix.
enum class KktOrdering {
    kMinimumDegree,
    /// Serial nested dissection (METIS). The default.
    kNestedDissection,
    /// Parallel nested dissection; downgraded to the serial variant on a host
    /// that lacks it.
    kParallelNestedDissection,
};

/// @brief Pivoting for the symmetric indefinite KKT factorization.
enum class KktPivoting {
    /// 1x1 pivots only.
    kOneByOne,
    /// 1x1 and 2x2 Bunch-Kaufman pivots. The default.
    kTwoByTwo,
};

/// @brief Parallelism of the forward and backward solves.
enum class KktSolveParallelism {
    /// Partition the matrix for a single right-hand side, otherwise parallelize
    /// over right-hand sides. The default.
    kAdaptive,
    /// Sequential.
    kSequential,
};

/// @brief The algorithm mode a phase runs in.
enum class AlgorithmMode { kOpt, kOptNo, kSoe, kInit };

/// @brief Barrier-parameter update rule.
enum class BarrierMode { kProbe, kLoqo };

/// @brief Merit function of the classic line search.
enum class LineSearchMode { kAugLang, kLang, kL1, kNoLs };

/// @brief Primal-dual step-length rule.
enum class PrimalDualStepStrategy { kPrimSlackEqSplitIq, kAllMinimum, kPrimSlackSplitEqIq, kMaxEq };

/// @brief Step-acceptance test.
enum class AcceptanceKind {
    /// The fused classic backtracking merit line search.
    kClassicMerit,
    /// The modernized merit family; its penalty rule is `merit_penalty_rule`.
    kMerit,
    /// The scalar-funnel test on the Waechter-Biegler switching skeleton.
    kFunnel,
    /// The (theta, phi) filter test on the same skeleton. The default.
    kFilter,
};

/// @brief Penalty-parameter rule of the modernized merit family.
enum class MeritPenaltyRule {
    /// Waltz, Morales, Nocedal and Orban (2006), section 3.1.
    kWmno,
    /// Curtis and Nocedal (2008): a penalty interval.
    kFlexible,
};

/// @brief Barrier-parameter governor.
enum class BarrierGovernorKind {
    /// The free-mode PROBE/LOQO update on its own.
    kClassicAdaptive,
    /// Free mode with a monotone (Fiacco-McCormick) fallback when progress
    /// stalls. The default.
    kMonitored,
};

/// @brief Feasibility restoration.
enum class RestorationMode {
    /// None. The default.
    kOff,
    /// Swap the objective for a proximal term until infeasibility is reduced.
    kProximalSwitch,
    /// A condensed nested l1 elastic restoration phase.
    kL1Nested,
};

/// @brief KKT inertia correction.
enum class InertiaMode {
    /// The on-demand inertia ladder. The default.
    kClassic,
    /// A persistent decaying primal shift and a barrier-scaled dual shift, with
    /// the ladder on top.
    kProximalRegularization,
};

/// @brief How `IpmOptions::return_best` scores iterates.
enum class BestIterateCriterion {
    /// Smallest equality residual. The default.
    kEquality,
    /// Smallest inequality residual.
    kInequality,
    /// Smallest KKT residual.
    kKkt,
    /// Smallest objective.
    kObjective,
};

} // namespace hven::solvers
