#include "hven/detail/interior/kkt_factorization.h"

#include <stdexcept>

#include <fmt/format.h>

#ifdef USE_ACCELERATE_SPARSE
// For the backend's own status enumerators. The failure categorization below
// is written in terms of those names rather than their numeric values: the two
// backends' error codes overlap numerically and mean entirely different
// things, which is precisely the confusion a raw comparison would introduce.
#include <Accelerate/Accelerate.h>
#endif

namespace hven::solvers {

namespace {

// The reporting-only status category the engine has always recorded for a
// given backend outcome. Both branches reproduce the categorization the
// engine's previous backend interface performed, verbatim -- this is a
// reporting surface, so a code landing in a different category than before
// would change which warnings the ladder prints.
#ifdef USE_ACCELERATE_SPARSE
Eigen::ComputationInfo computation_info_of(int backend_code) {
    switch (static_cast<SparseStatus_t>(backend_code)) {
    case SparseStatusOK:
        return Eigen::Success;
    case SparseFactorizationFailed:
    case SparseMatrixIsSingular:
        // A numeric refusal, which is a normal outcome while the ladder probes
        // perturbations -- recorded, not printed.
        return Eigen::NumericalIssue;
    case SparseInternalError:
    case SparseParameterError:
    case SparseStatusReleased:
    default:
        return Eigen::InvalidInput;
    }
}
#else
Eigen::ComputationInfo computation_info_of(int backend_code) {
    if (backend_code == 0) {
        return Eigen::Success;
    }
    // Pardiso's -4 / -7 (zero or near-zero pivot) is the ordinary outcome of
    // probing a perturbation during inertia correction, so it is recorded
    // without printing; anything else is a hard error.
    return (backend_code == -4 || backend_code == -7) ? Eigen::NumericalIssue : Eigen::InvalidInput;
}
#endif

} // namespace

KktFactorization::KktFactorization() : KktFactorization(Options{}) {}

KktFactorization::KktFactorization(const Options &opts) : opts_(opts), factor_(opts) {}

void KktFactorization::reconfigure(const Options &opts) {
    opts_ = opts;
    factor_ = hven::linear::SymmetricFactor(opts);
    clear_evidence();
}

bool KktFactorization::set_num_threads(int num_threads) {
    if (num_threads == opts_.num_threads) {
        return false;
    }
    Options opts = opts_;
    opts.num_threads = num_threads;
    reconfigure(opts);
    return true;
}

void KktFactorization::release() {
    factor_ = hven::linear::SymmetricFactor(opts_);
    matrix_.resize(0, 0);
    matrix_.data().squeeze();
    clear_evidence();
}

void KktFactorization::compute() {
    try {
        factor_.analyze(matrix_);
    } catch (const std::runtime_error &) {
        // A BACKEND symbolic failure, and the engine's long-standing response
        // to one: record the status and return. The previous interface did
        // exactly this -- it stored the backend's error code in the
        // reporting-only status and let the caller carry on -- and the engine
        // above is written against that control flow, so the exception does
        // not escape here. The evidence is projected the same way a failed
        // numeric factorization's is, since neither produced a factorization
        // to describe.
        //
        // A caller error is a different thing and is NOT caught: the linear
        // surface throws std::invalid_argument (a logic error, not a runtime
        // one) for a matrix that is not compressed, not upper triangular, or
        // missing a structural diagonal, and that is a bug in the assembly
        // above, not a numeric outcome to absorb.
        //
        // NOT COVERED BY A FIXTURE, only by fault injection: no matrix this
        // engine can assemble makes either backend's symbolic phase fail.
        //
        // The status is categorized as an input-level failure rather than a
        // numeric one because the symbolic phase is where a backend reports
        // reordering and sizing problems, which is exactly what both previous
        // interfaces categorized this way. The backend's own code is not
        // available here -- the linear surface reports a symbolic failure as a
        // formatted exception rather than a code -- and reading it back out of
        // a message is not a thing this layer will do.
        info_ = Eigen::InvalidInput;
        record_failed_factorization(hven::linear::FactorizeOutcome{});
        return;
    }
    record(factor_.factorize(matrix_));
}

void KktFactorization::refactorize() { record(factor_.factorize(matrix_)); }

void KktFactorization::solve(ConstVecRef rhs, VecRef x) const { factor_.solve(rhs, x); }

void KktFactorization::clear_evidence() {
    n_pos_ = 0;
    n_neg_ = 0;
    perturbed_pivots_ = 0;
    info_ = Eigen::Success;
    factor_mem_ = 0;
    factor_flops_ = 0;
}

void KktFactorization::record(const hven::linear::FactorizeOutcome &outcome) {
    info_ = computation_info_of(outcome.backend_code);

    if (outcome.status != hven::linear::FactorizeOutcome::Status::kOk) {
        record_failed_factorization(outcome);
        return;
    }

    n_pos_ = static_cast<int>(outcome.inertia.n_pos);
    n_neg_ = static_cast<int>(outcome.inertia.n_neg);
    // An absent perturbed-pivot count reads as the 0 the engine saw on a
    // backend that keeps no such counter.
    perturbed_pivots_ = static_cast<int>(outcome.inertia.perturbed_pivots.value_or(0));

    // Factor size. The backends report structurally different quantities -- an
    // entry count on one, a byte size on the other -- and the linear layer
    // keeps them in separate fields rather than folding them under a shared
    // name. Each branch requires ITS OWN backend's field: a missing value is a
    // defect in this projection, not a quantity to substitute from elsewhere.
#ifdef USE_ACCELERATE_SPARSE
    if (!outcome.factor.factor_size_bytes.has_value()) {
        throw std::logic_error("KktFactorization: the factorization reported no factor byte size");
    }
    factor_mem_ = static_cast<int>(*outcome.factor.factor_size_bytes);
    // This backend reports no factorization-cost estimate at all, which the
    // engine's own "> 0" print guard is what suppresses.
    factor_flops_ = 0;
#else
    if (!outcome.factor.factor_nonzeros.has_value()) {
        throw std::logic_error(
            "KktFactorization: the factorization reported no factor entry count");
    }
    factor_mem_ = static_cast<int>(*outcome.factor.factor_nonzeros);

    // The cost estimate is reported only where it was requested; requesting it
    // and not getting it is a defect.
    if (opts_.collect_factor_mflops) {
        if (!outcome.factor.factor_mflops.has_value()) {
            throw std::logic_error(
                fmt::format("KktFactorization: the factorization cost estimate was requested but "
                            "not reported (factor size {} was)",
                            factor_mem_));
        }
        factor_flops_ = static_cast<int>(*outcome.factor.factor_mflops);
    } else {
        factor_flops_ = 0;
    }
#endif
}

void KktFactorization::record_failed_factorization(
    [[maybe_unused]] const hven::linear::FactorizeOutcome &outcome) {
#ifdef USE_ACCELERATE_SPARSE
    // The engine's previous Accelerate interface reset its inertia counts to
    // zero and zeroed both factor metrics on every path that abandoned a
    // factorization -- a failed symbolic analysis, a failed numeric
    // factorization, a failed refactorization. Those were DEFINED values the
    // engine went on to read, so they are reproduced here rather than replaced
    // by the linear layer's honest invalid counts. That the honest answer is
    // available at this boundary and deliberately not adopted is the same
    // decision every other row of this projection makes.
    n_pos_ = 0;
    n_neg_ = 0;
    perturbed_pivots_ = 0;
    factor_mem_ = 0;
    factor_flops_ = 0;
#else
    // The MKL side has no such values to preserve: the previous interface read
    // the parameter array unconditionally after a failed backend call, so what
    // it presented was whatever the backend happened to leave there. The
    // linear layer's own invalid counts pass through instead, and the ladder
    // reads them as a system to perturb -- the right response to a
    // factorization that did not happen. The factor metrics are left at their
    // last reported values for the same reason: there is no defined previous
    // behavior to match, and the engine only reads them after a successful
    // entry factorization.
    //
    // Unreachable in practice for a real symmetric indefinite matrix: with
    // static pivot perturbation on, this backend perturbs its way through
    // rather than refusing.
    n_pos_ = static_cast<int>(outcome.inertia.n_pos);
    n_neg_ = static_cast<int>(outcome.inertia.n_neg);
    perturbed_pivots_ = static_cast<int>(outcome.inertia.perturbed_pivots.value_or(0));
#endif
}

} // namespace hven::solvers
