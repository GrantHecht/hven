// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The interior-point engine's KKT factorization: the assembly buffer
// the engine writes its Newton system into, the SymmetricFactor consuming it,
// and the small evidence cache read back after every numeric factorization.
//
// The engine assembles directly into a matrix held by reference (hven::linear
// takes the matrix BY REFERENCE at analyze()/factorize()), and consumes plain
// integers plus an Eigen::ComputationInfo status while the linear layer
// reports what a factorization OBSERVED (inertia, optional perturbed-pivot
// count, factor size in backend-specific units) and never a verdict. This
// class projects one onto the other at a single point, deliberately faithful
// to pre-linear-layer shapes (absent pivot count reads 0; both factor-size
// observables land in one integer) so swapping backends is not an
// algorithm-policy change. Fidelity extends to the FAILURE paths, which are
// BACKEND-SPECIFIC: the replaced interfaces categorized their error codes
// differently, so the failed-factorization projection is written per backend.

#include <Eigen/Core>

#include "hven/core/types.h"
#include "hven/linear/symmetric_factor.h"

namespace hven::solvers {

class KktFactorization {
  public:
    using Options = hven::linear::SymmetricFactor::Options;
    using Counters = hven::linear::SymmetricFactor::Counters;

    /// @brief Whether a numeric factorization still has to verify that the
    ///        assembly buffer carries the analyzed sparsity pattern.
    ///
    /// The linear layer's own vocabulary, re-exported here so an engine that
    /// knows its buffer's pattern is fixed can say so without naming
    /// hven::linear at the call site. What passing kAssumeAnalyzed takes on
    /// is stated once, on SymmetricFactor::PatternCheck.
    using PatternCheck = hven::linear::SymmetricFactor::PatternCheck;

    /// Backend defaults. Nothing is analyzed or factorized until the engine
    /// configures this object and fills the matrix.
    KktFactorization();
    explicit KktFactorization(const Options &opts);

    KktFactorization(const KktFactorization &) = delete;
    KktFactorization &operator=(const KktFactorization &) = delete;

    /// The KKT assembly buffer. Every evaluation, barrier-Hessian and
    /// diagonal-perturbation site writes through this reference; the pattern
    /// itself is (re)laid out by NonLinearProgram::analyze_sparsity.
    SpMatRM &matrix() { return matrix_; }
    const SpMatRM &matrix() const { return matrix_; }

    /// Rebuild the factor under a new option set. Drops any existing symbolic
    /// analysis: the options are baked into the backend session that analysis
    /// lives in, so a new configuration needs a new session.
    void reconfigure(const Options &opts);

    /// Point the factor at a (possibly new) thread count, so a thread setting
    /// changed after the last reconfigure() still governs the backend calls.
    ///
    /// Costs nothing beyond the store: the linear surface applies the count
    /// per backend call, so moving it keeps the backend session, the symbolic
    /// analysis and the current numerics -- a solve that follows needs no
    /// re-analysis. Throws std::invalid_argument for a negative count, which
    /// is the linear surface's own rule.
    ///
    /// NOTE the deliberate asymmetry with `opts_.cnr_threads`: this setter
    /// refreshes `opts_.num_threads` on every solve entry (see the engine's
    /// run_phase_sequence() call site), but nothing analogous refreshes
    /// `cnr_threads` -- it is set exactly once, in set_qp_params() at
    /// transcribe time, and reconfigure() is the only thing that can move it
    /// afterward. This is fidelity-preserving, not an oversight: the
    /// pre-migration engine never re-derived CNR thread count outside of
    /// transcription either, so a mid-life qp_threads_ change under CNR mode
    /// (cnr_mode_ == true) leaves `cnr_threads` at whatever value was
    /// configured at the last transcribe, even though `num_threads` itself
    /// tracks the new setting immediately. Reproducibility-mode threading and
    /// ordinary solve threading are intentionally on different refresh
    /// cadences here -- and, unlike `num_threads`, `cnr_threads` genuinely IS
    /// baked into the session (it is a parameter-array entry the symbolic
    /// phase reads), so it could not be moved live even if the engine wanted
    /// to.
    void set_num_threads(int num_threads);

    /// The thread count the factor is currently configured with.
    int num_threads() const { return opts_.num_threads; }

    /// Drop the factorization, the symbolic analysis and the assembly buffer.
    void release();

    /// Symbolic analysis followed by a numeric factorization.
    ///
    /// A backend symbolic failure does NOT propagate: it is recorded in the
    /// reporting-only status and the call returns, leaving the object
    /// analyzable again -- the control flow the engine has always had on that
    /// path. A caller error (a matrix that is not compressed, not upper
    /// triangular, or missing a structural diagonal) is a different thing and
    /// still throws.
    void compute();

    /// @brief Numeric factorization into the existing symbolic analysis.
    /// @param check  whether the buffer's pattern is verified against the
    ///               analyzed one on this call, or declared to match it.
    ///
    /// Throws if there is no symbolic analysis, or -- under
    /// PatternCheck::kVerify -- if the buffer's sparsity pattern is not the
    /// analyzed one; a pattern change is a caller error here, not a numeric
    /// outcome, and its remedy is compute().
    ///
    /// The verification is an O(nnz) walk of the whole KKT matrix, and this
    /// call sits inside the inertia ladder, so on a solve it runs several
    /// times per iteration. kAssumeAnalyzed is for an engine that can name
    /// what keeps the pattern fixed between analyses -- the interior engine
    /// gates it on the model's structure epoch, which is the model's own
    /// signal that its structures have been re-laid.
    void refactorize(PatternCheck check = PatternCheck::kVerify);

    /// @brief The linear engine's call counters, including the pattern-guard
    ///        count that makes a skipped verification observable.
    const Counters &counters() const { return factor_.counters(); }

    /// Solve against the current factorization. `x` must already be sized.
    void solve(ConstVecRef rhs, VecRef x) const;

    // --- evidence from the last numeric factorization ---

    /// Positive and negative eigenvalue counts. Meaningful after a successful
    /// factorization; a failed one leaves the backend's own invalid counts,
    /// which the inertia ladder reads as a system to perturb.
    int peigs() const { return n_pos_; }
    int neigs() const { return n_neg_; }

    /// Pivots the backend perturbed to get through the factorization, or 0 on
    /// a backend that keeps no such counter.
    int ppivs() const { return perturbed_pivots_; }

    /// Reporting-only status of the last factorization. No control flow in the
    /// engine turns on it; it is surfaced in the exit statistics.
    Eigen::ComputationInfo info() const { return info_; }

    /// Factor size, in whichever unit this backend reports: an entry count on
    /// MKL Pardiso, a byte size on Apple Accelerate.
    int factor_mem() const { return factor_mem_; }

    /// Estimated factorization cost in Mflops, or 0 on a backend that reports
    /// no cost estimate.
    int factor_flops() const { return factor_flops_; }

  private:
    void record(const hven::linear::FactorizeOutcome &outcome);

    /// The evidence a factorization that did not produce one leaves behind.
    /// Backend-specific: see the implementation.
    void record_failed_factorization(const hven::linear::FactorizeOutcome &outcome);

    void clear_evidence();

    Options opts_;
    hven::linear::SymmetricFactor factor_;
    SpMatRM matrix_;

    int n_pos_ = 0;
    int n_neg_ = 0;
    int perturbed_pivots_ = 0;
    Eigen::ComputationInfo info_ = Eigen::Success;
    int factor_mem_ = 0;
    int factor_flops_ = 0;
};

} // namespace hven::solvers
