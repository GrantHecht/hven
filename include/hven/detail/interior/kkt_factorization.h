#pragma once

// The interior-point engine's KKT factorization: the assembly buffer the
// engine writes its Newton system into, the hven::linear::SymmetricFactor that
// consumes it, and the small evidence cache the engine reads back after every
// numeric factorization.
//
// WHY A BUFFER OF ITS OWN. The engine assembles values -- and, when the
// variable treatment changes, a whole new sparsity pattern -- directly into a
// matrix it holds a reference to. hven::linear takes the matrix BY REFERENCE at
// analyze() and factorize() instead of handing out an internal one, so the
// buffer moves here and every assembly site writes into it exactly as before.
//
// WHY AN EVIDENCE CACHE. hven::linear reports what a factorization observed
// (an inertia state, an optional perturbed-pivot count, a factor size that is
// an entry count on one backend and a byte size on another) and never a
// verdict. The engine above consumes plain integers and an
// Eigen::ComputationInfo status, so this class projects the one onto the other
// at a single point. The projection is deliberately faithful to what the
// engine saw before the linear layer was replaced -- an absent perturbed-pivot
// count reads as 0, the two structurally different factor-size observables both
// land in one integer -- so that swapping the backend is not also an
// algorithm-policy change. Adopting the honest evidence shapes inside the
// engine is a separate piece of work with its own justification.

#include <Eigen/Core>

#include "hven/core/types.h"
#include "hven/linear/symmetric_factor.h"

namespace hven::solvers {

class KktFactorization {
  public:
    using Options = hven::linear::SymmetricFactor::Options;

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

    /// Drop the factorization, the symbolic analysis and the assembly buffer.
    void release();

    /// Symbolic analysis followed by a numeric factorization.
    void compute();

    /// Numeric factorization into the existing symbolic analysis. Throws if
    /// there is none, or if the buffer's sparsity pattern is not the analyzed
    /// one -- a pattern change is a caller error here, not a numeric outcome,
    /// and its remedy is compute().
    void refactorize();

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
    void clear_evidence();

    Options opts_;
    hven::linear::SymmetricFactor factor_;
    SpMatRM matrix_;

    /// True once a factorization requesting the Mflop estimate has been
    /// configured, which makes the estimate's absence a defect rather than a
    /// backend that simply does not report one.
    bool expect_factor_mflops_ = false;

    int n_pos_ = 0;
    int n_neg_ = 0;
    int perturbed_pivots_ = 0;
    Eigen::ComputationInfo info_ = Eigen::Success;
    int factor_mem_ = 0;
    int factor_flops_ = 0;
};

} // namespace hven::solvers
