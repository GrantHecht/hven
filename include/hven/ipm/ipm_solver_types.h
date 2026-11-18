// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief What an interior-point solve returns (`IpmResult`). The options are in
///        ipm/ipm_options.h, which this header includes.

#include <array>
#include <iosfwd>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <hven/core/solver_counters.h>
#include <hven/core/types.h>
#include <hven/ipm/ipm_options.h>
#include <hven/ipm/ipm_phase_report.h>
#include <hven/linear/symmetric_factor.h>
#include <hven/solver/common_options.h>
#include <hven/solver/iteration_record.h>
#include <hven/solver/solve_result.h>

namespace hven {

/// @brief The four scalars the interior-point convergence test gates on, with
///        IpmIterate's definitions, so each is directly comparable against its
///        matching IpmOptions tolerance, plus their feasibility summary.
///
/// `stationarity` and `complementarity` carry IpmOptions::objective_scale;
/// `equality`, `inequality` and `feasibility` carry no scale, and nothing here
/// is unscaled on the way out. On a restoration-active exit they describe the
/// restoration subproblem, not the NLP. Last phase wins. NaN means UNMEASURED,
/// never "zero residual".
struct IpmConvergence {
    /// inf-norm dual infeasibility (z-form) at the reported iterate.
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    /// Complementarity (barrier) error at the reported iterate.
    double complementarity = std::numeric_limits<double>::quiet_NaN();
    /// inf-norm equality-constraint residual; 0 when there are no equalities.
    double equality = std::numeric_limits<double>::quiet_NaN();
    /// inf-norm slack-form inequality residual (rows and their slacks); 0 when none.
    double inequality = std::numeric_limits<double>::quiet_NaN();
    /// max(equality, inequality), NaN if either is. Not SqpConvergence::feasibility's
    /// norm (docs/migration-0.2.md, section G7).
    double feasibility = std::numeric_limits<double>::quiet_NaN();
};

/// @brief Where one interior-point call spent its wall-clock seconds.
///
/// INFORMATIONAL, NEVER ASSERTED -- counters are this project's currency of
/// correctness, not timings. Measured on std::chrono::steady_clock.
struct IpmTiming {
    /// Total wall-clock time of the call.
    double total_seconds = 0;
    /// Setup/preprocessing time.
    double preprocessing_seconds = 0;
    /// NLP evaluation callback time.
    double evaluation_seconds = 0;
    /// KKT assembly/factorization/solve time.
    double kkt_seconds = 0;
    /// Printing time.
    double print_seconds = 0;
    /// Solver-initialization time (measured before the main timer starts).
    double solver_initialization_seconds = 0;

    /// Total minus all categorized components (excluding `solver_init`, which
    /// is measured before the main timer starts): callback time, step
    /// application, convergence checks and the like.
    double other_seconds() const {
        return total_seconds - preprocessing_seconds - kkt_seconds - evaluation_seconds -
               print_seconds;
    }
};

/// @brief What one interior-point solve produced.
///
/// The base carries the answer in DECLARED space and CALLER units; everything
/// added here is this engine's own -- its measurements, its timings, its
/// per-phase account and its factorization snapshot.
///
/// PER CALL, all of it. This is a value returned by `solve()`, not an
/// accumulator a caller reads later.
struct IpmResult : SolveResult {
    /// @brief This call's counters.
    IpmCounters counters;

    /// @brief Which IpmOptions::fixed_variable_treatment this call actually
    ///        ran under.
    ///
    /// Always equals the option's value at the time the call ran --
    /// configure_variable_treatment runs the requested treatment or throws,
    /// never substitutes. Overwritten unconditionally at the start of every
    /// call. Recorded here because it decides whether the internal fixing rows
    /// below exist.
    IpmFixedVariableTreatment fixed_variable_treatment = IpmFixedVariableTreatment::kMakeParameter;

    // --- The treatment's own rows, reported rather than dropped ---
    //
    // Under MakeConstraint a bound-fixed variable stays in the problem and is
    // held by an INTERNAL equality row `x - value = 0` appended after the
    // declared rows. Those rows are the treatment's, not the declaration's, so
    // they are taken OFF the base's ce/lambda_e -- whose contract is the
    // DECLARED problem -- and reported here instead. Empty under every other
    // treatment, and empty under MakeConstraint on a problem with no
    // bound-fixed variables.
    //
    // The fixing row's multiplier is also what prices that variable's box in
    // declared space: the base's z carries -lambda_fix at that coordinate, the
    // sign the stationarity convention grad f + J'lambda - z = 0 forces.
    /// @brief Residuals of the internal fixing rows, one per bound-fixed
    ///        variable, in the order of
    ///        NonLinearProgram::fixed_variable_indices().
    Vec internal_fixed_ce;
    /// @brief Multipliers of those same rows, same order.
    Vec internal_fixed_lambda_e;

    /// @brief One `IterationRecord` per counted iteration of every phase, in order (`ipm` block
    ///        set): the depth-0 `iter` trace stream row for row. Always on, sink or not.
    std::vector<IterationRecord> history;

    /// @brief The engine's own terminal measurements: what its convergence test
    ///        gated on, in its space. NOT the base's shared diagnostics.
    IpmConvergence convergence;

    /// @brief Where the call spent its wall-clock seconds.
    IpmTiming timing;

    // --- Factorization stats ---
    /// @brief Memory reported by the last factorization.
    Index factor_memory = 0;
    /// @brief Flops reported by the last factorization.
    Index factor_flops = 0;

    // --- The per-phase account ---
    /// @brief One entry per phase in `IpmOptions::phases`, in that order.
    ///
    /// The base's `status` is the LAST RAN phase's status and the base's
    /// `iterations` is the sum over the phases that ran.
    std::vector<IpmPhaseReport> phases;

    // --- The factorization SNAPSHOT ---
    //
    // Taken as this result was built, so they describe the solve that produced
    // them and cannot go stale behind a caller's back.
    /// @brief How many times the producing solver had laid and analyzed the KKT
    ///        sparsity pattern over its LIFETIME, as of this result.
    Index kkt_analyses_total = 0;
    /// @brief The KKT factor's linear-layer call counters as of this result,
    ///        including the pattern-guard count.
    hven::linear::SymmetricFactor::Counters kkt_factor_counters;
};

/// @brief Writes every counter @p r holds to @p os, one `name = value` line each: the twin of
///        `dump_options`, generated from the same X-macro table as `solve.end.counters`.
///
/// Lines, in order: the `IpmCounters` fields in declaration order (the same table the trace's
/// `counters` object is written from), `kkt_analyses_total`, then every field of
/// `kkt_factor_counters` under that prefix (`kkt_factor_counters.analyze_count`, ...).
/// `name = value`, `\n`-terminated, no header or footer. An integer prints as such; a double
/// at its shortest round-trip form; an enum by its `to_string` spelling;
/// `recovery_depth_histogram` as `[a, b, c, d, e]`; a counter the table declares absent for its
/// value (a "never measured" sentinel) prints `null`, the trace's spelling.
///
/// Counters only. A sum a consumer computes over a ledger, or a residual, is not here. The
/// caller's stream state (flags, precision, width, locale) reaches no byte of the output and is
/// not modified.
void dump_counters(const IpmResult &r, std::ostream &os);

/// @brief The last phase of `r.phases` that ran, or nullptr when none did.
///
/// The phase the base's `status` is the verdict of; the reading the retired
/// `IpmResult::last_*` fields stood for.
inline const IpmPhaseReport *last_ran_phase(const IpmResult &r) {
    for (auto it = r.phases.rbegin(); it != r.phases.rend(); ++it) {
        if (it->ran) {
            return &*it;
        }
    }
    return nullptr;
}

} // namespace hven
