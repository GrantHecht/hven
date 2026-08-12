#include "hven/detail/interior/kkt_factorization.h"

#include <stdexcept>

#include <fmt/format.h>

namespace hven::solvers {

KktFactorization::KktFactorization() : KktFactorization(Options{}) {}

KktFactorization::KktFactorization(const Options &opts)
    : opts_(opts), factor_(opts), expect_factor_mflops_(opts.collect_factor_mflops) {}

void KktFactorization::reconfigure(const Options &opts) {
    opts_ = opts;
    expect_factor_mflops_ = opts.collect_factor_mflops;
    factor_ = hven::linear::SymmetricFactor(opts);
    clear_evidence();
}

void KktFactorization::release() {
    factor_ = hven::linear::SymmetricFactor(opts_);
    matrix_.resize(0, 0);
    matrix_.data().squeeze();
    clear_evidence();
}

void KktFactorization::compute() {
    factor_.analyze(matrix_);
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
    // The inertia counts and the perturbed-pivot projection are taken the same
    // way on both outcomes: on success they are the backend's observation; on a
    // backend error the linear layer reports no observation at all, and the
    // invalid counts it leaves behind make the ladder's own singularity test
    // fire, which is the right response to a factorization that did not happen.
    n_pos_ = static_cast<int>(outcome.inertia.n_pos);
    n_neg_ = static_cast<int>(outcome.inertia.n_neg);
    perturbed_pivots_ = static_cast<int>(outcome.inertia.perturbed_pivots.value_or(0));

    if (outcome.status != hven::linear::FactorizeOutcome::Status::kOk) {
        // Pardiso's -4 / -7 (zero or near-zero pivot) is the ordinary outcome
        // of probing a perturbation during inertia correction, so it maps to
        // the same NumericalIssue category the engine has always recorded
        // without printing; anything else is a hard error.
        info_ = (outcome.backend_code == -4 || outcome.backend_code == -7) ? Eigen::NumericalIssue
                                                                           : Eigen::InvalidInput;
        return;
    }

    info_ = Eigen::Success;

    // Factor size. The two backends report structurally different quantities --
    // an entry count on one, a byte size on the other -- and the linear layer
    // keeps them in separate, individually optional fields rather than folding
    // them under a shared name. Exactly one of the two is present after a
    // successful factorization; neither being present means the backend
    // reported no size at all, which no supported backend does.
    if (outcome.factor.factor_nonzeros.has_value()) {
        factor_mem_ = static_cast<int>(*outcome.factor.factor_nonzeros);
    } else if (outcome.factor.factor_size_bytes.has_value()) {
        factor_mem_ = static_cast<int>(*outcome.factor.factor_size_bytes);
    } else {
        throw std::logic_error(
            "KktFactorization: the factorization reported neither a factor entry count nor a "
            "factor byte size, so there is no factor size to report");
    }

    // Factorization cost. Requested wherever the backend can produce it, and
    // its absence there is a defect rather than a backend limitation; a backend
    // with no cost estimate at all leaves the engine's own "> 0" print guard to
    // suppress the line.
    if (expect_factor_mflops_) {
        if (!outcome.factor.factor_mflops.has_value()) {
            throw std::logic_error(fmt::format(
                "KktFactorization: the factorization cost estimate was requested but not "
                "reported (factor size {} was)",
                factor_mem_));
        }
        factor_flops_ = static_cast<int>(*outcome.factor.factor_mflops);
    } else {
        factor_flops_ = 0;
    }
}

} // namespace hven::solvers
