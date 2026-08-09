// =============================================================================
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// Derived from the PardisoSupport module of the Eigen linear algebra library
// (MPL-2.0) -- see the notice at the top of the matching header for exactly
// what is derived and what is not. MPL-2.0 applies to this file only; the
// remainder of hven is Apache-2.0. See notices/eigen-mpl2.txt.
// =============================================================================

#include "hven/detail/linear/pardiso_session.h"

#include "hven/detail/linear/fault_injection.h"

#include <algorithm>
#include <stdexcept>

#include <fmt/format.h>

#include <mkl_service.h>

namespace hven::linear::detail {

namespace {

// Applies a per-instance thread count for the duration of one backend call.
//
// mkl_set_num_threads_local sets a THREAD-LOCAL override; restoring it to 0
// hands control back to MKL's global setting. That is the whole point of
// using it: an hven instance configured for two threads must not reach out
// and change the thread count of every other MKL user in the process, and it
// must not depend on the environment to undo what it did.
class MklThreadScope {
  public:
    explicit MklThreadScope(int num_threads) : engaged_(num_threads > 0) {
        if (engaged_) {
            mkl_set_num_threads_local(num_threads);
        }
    }

    ~MklThreadScope() {
        if (engaged_) {
            mkl_set_num_threads_local(0);
        }
    }

    MklThreadScope(const MklThreadScope &) = delete;
    MklThreadScope &operator=(const MklThreadScope &) = delete;

  private:
    bool engaged_;
};

// A one-line gloss for the Pardiso error codes hven can plausibly hit, so a
// thrown message says something a reader can act on without the manual open.
// Unknown codes are reported as-is rather than guessed at.
const char *pardiso_error_text(MKL_INT code) {
    switch (code) {
    case -1:
        return "input inconsistent";
    case -2:
        return "not enough memory";
    case -3:
        return "reordering problem";
    case -4:
        return "zero pivot, numerically singular or indefinite matrix";
    case -5:
        return "unclassified internal error";
    case -6:
        return "reordering failed";
    case -7:
        return "diagonal matrix is singular";
    case -8:
        return "32-bit integer overflow";
    case -9:
        return "not enough memory for out-of-core";
    case -10:
        return "error opening out-of-core files";
    case -11:
        return "read/write error with out-of-core files";
    case -12:
        return "out-of-core mode: parameter error";
    case -13:
        return "interrupted by the mkl_progress function";
    default:
        return "unrecognized Pardiso error code";
    }
}

} // namespace

FactorSession::FactorSession(const PardisoConfig &cfg, std::uint64_t initial_epoch)
    : cfg_(cfg), epoch_(initial_epoch) {
    pt_.fill(nullptr);
    iparm_.fill(0);
}

FactorSession::~FactorSession() { release(); }

MKL_INT FactorSession::run_phase(MKL_INT phase, bool use_matrix, MKL_INT nrhs, const double *b,
                                 double *x) const {
    MKL_INT maxfct = 1;
    MKL_INT mnum = 1;
    MKL_INT mtype = static_cast<MKL_INT>(cfg_.mtype);
    MKL_INT msglvl = 0;
    MKL_INT error = 0;
    MKL_INT n = n_;

    const void *a = use_matrix ? static_cast<const void *>(matrix_.valuePtr()) : nullptr;
    const MKL_INT *ia = use_matrix ? matrix_.outerIndexPtr() : nullptr;
    const MKL_INT *ja = use_matrix ? matrix_.innerIndexPtr() : nullptr;

    // Pardiso's C prototype takes the right-hand side as `void *` because it
    // overwrites it when asked to (iparm[5] == 1). hven never asks -- the
    // solution goes to `x` and `b` is left untouched -- so the buffer is
    // const on this side and cast at the boundary, exactly as the Eigen
    // module this is derived from does.
    void *b_ptr = const_cast<double *>(b);

    const MklThreadScope threads(cfg_.num_threads);
    ::pardiso(pt_.data(), &maxfct, &mnum, &mtype, &phase, &n, a, ia, ja, perm_.data(), &nrhs,
              iparm_.data(), &msglvl, b_ptr, x, &error);
    return error;
}

void FactorSession::release() noexcept {
    if (!active_) {
        return;
    }
    // Phase -1 frees Pardiso's internal memory. It is not expected to fail,
    // and this runs from the destructor, so its error code is deliberately
    // discarded rather than turned into a throw from a noexcept path.
    run_phase(/*phase=*/-1, /*use_matrix=*/false, /*nrhs=*/0, nullptr, nullptr);
    active_ = false;
    has_numerics_ = false;
}

void FactorSession::analyze(const SpMatRM &A) {
    release();

    n_ = static_cast<MKL_INT>(A.rows());
    perm_.assign(static_cast<std::size_t>(n_), 0);

    // Start from Pardiso's own defaults for this matrix type, then override
    // only the entries hven's option surface names. Every override is listed
    // in the block below; nothing else is touched.
    pt_.fill(nullptr);
    iparm_.fill(0);
    MKL_INT mtype = static_cast<MKL_INT>(cfg_.mtype);
    pardisoinit(pt_.data(), &mtype, iparm_.data());

    // iparm[34] = 1: zero-based (C-style) CSR indexing, so Eigen's index
    // arrays feed Pardiso directly with no reindexing pass.
    iparm_[34] = 1;
    // iparm[7]: cap on iterative-refinement steps for a full solve. Forced
    // to zero around partial solves (see solve_partial).
    iparm_[7] = static_cast<MKL_INT>(cfg_.max_refinement_iters);
    // iparm[9]: static pivot perturbation exponent -- pivots too small to
    // use are replaced by ones of magnitude ~10^-k and counted in iparm[13].
    iparm_[9] = static_cast<MKL_INT>(cfg_.pivot_perturb_exp);
    // iparm[1]: fill-in reordering. Left untouched (pardisoinit's own value
    // survives) when cfg_.ordering is nullopt; a present value (0 = minimum
    // degree via AMD, 2 = nested dissection via METIS, 3 = its OpenMP-
    // parallel variant) overrides it -- see
    // SymmetricFactor::Options::Ordering.
    //
    // THE ONE TEST-ONLY RECORD IN THIS FILE, and why it is here rather than at
    // the adapter boundary where this project prefers to instrument: the fact
    // under test is whether the assignment below EXECUTES, and a skipped
    // assignment leaves no trace anywhere outside this function. On an MKL
    // whose pardisoinit default for this entry happens to equal one of the
    // values Ordering can request -- which is the case on the toolchain this
    // was written against -- reading the array afterwards cannot tell "left it
    // alone" from "wrote exactly that value", so the boundary has nothing to
    // observe and the rule would rest on inspecting this `if`. The record goes
    // where the fact is. It compiles to nothing outside the fault-injection
    // test target (fault_injection.h is empty without HVEN_TESTING), and the
    // deviation is argued in docs/testing.md.
    if (cfg_.ordering.has_value()) {
        iparm_[1] = static_cast<MKL_INT>(*cfg_.ordering);
#ifdef HVEN_TESTING
        testing::PardisoIparmObserver::ordering_was_written = true;
        testing::PardisoIparmObserver::ordering_written_value = static_cast<int>(iparm_[1]);
#endif
    }
    // iparm[12]: maximum weighted matching. Left untouched unless requested
    // -- `false` does not write 0, it writes nothing at all, same as
    // iparm[1] above -- see SymmetricFactor::Options::weighted_matching.
    if (cfg_.weighted_matching) {
        iparm_[12] = 1;
#ifdef HVEN_TESTING
        testing::PardisoIparmObserver::weighted_matching_was_written = true;
        testing::PardisoIparmObserver::weighted_matching_written_value =
            static_cast<int>(iparm_[12]);
#endif
    }

    matrix_ = A;

    const MKL_INT error =
        run_phase(/*phase=*/11, /*use_matrix=*/true, /*nrhs=*/0, nullptr, nullptr);
    if (error != 0) {
        throw std::runtime_error(
            fmt::format("SymmetricFactor::analyze: symbolic analysis failed, backend error {} ({})",
                        static_cast<int>(error), pardiso_error_text(error)));
    }

    active_ = true;
    has_numerics_ = false;
}

int FactorSession::factorize(const SpMatRM &A) {
    if (!active_) {
        throw std::runtime_error(
            "SymmetricFactor::factorize: no symbolic analysis is available in this session");
    }
    if (static_cast<MKL_INT>(A.rows()) != n_ || A.nonZeros() != matrix_.nonZeros()) {
        throw std::invalid_argument(fmt::format(
            "SymmetricFactor::factorize: matrix is {}x{} with {} stored entries but the analyzed "
            "structure is {}x{} with {} stored entries",
            A.rows(), A.cols(), A.nonZeros(), matrix_.rows(), matrix_.cols(), matrix_.nonZeros()));
    }

    // Values only: the pattern is the analyzed one (the caller verified its
    // hash), so the index arrays Pardiso already holds stay exactly as they
    // were and only the numbers change.
    std::copy(A.valuePtr(), A.valuePtr() + A.nonZeros(), matrix_.valuePtr());

    // A factorization attempt invalidates the previous numerics up front: if
    // this one fails, the session must not look like it still holds a usable
    // factorization from before. Every co-owner sees that at once -- an
    // engine's solves and a shared handle's solves both gate on
    // has_numerics().
    //
    // NOT COVERED BY A TEST, AND DELIBERATELY NOT. Reaching the failure
    // return below needs this backend to refuse a numeric factorization, and
    // for a real symmetric indefinite matrix it does not: with static pivot
    // perturbation on it perturbs its way through an exactly singular matrix
    // and reports success (the suite's own singular fixture is exactly that).
    // Any fixture contrived to provoke a failure here would pin an accident
    // of the backend rather than this contract, so none is written. That
    // makes the three properties below guaranteed by inspection of this one
    // function -- keep them that way: the invalidation stays ahead of the
    // call, the epoch is incremented in exactly one place after the error
    // check, and the only other writes to either are unconditional teardown or
    // replacement (release() and analyze() set has_numerics_ = false; the
    // constructor seeds epoch_ forward) -- none of which can make a failed
    // factorization look usable.
    has_numerics_ = false;

    const MKL_INT error =
        run_phase(/*phase=*/22, /*use_matrix=*/true, /*nrhs=*/0, nullptr, nullptr);
    if (error != 0) {
        return static_cast<int>(error);
    }

    // iparm[21] / iparm[22]: positive and negative eigenvalue counts.
    // iparm[13]: pivots perturbed during this factorization.
    n_pos_ = iparm_[21];
    n_neg_ = iparm_[22];
    perturbed_pivots_ = iparm_[13];

    has_numerics_ = true;
    ++epoch_;
    return 0;
}

void FactorSession::solve(const double *b, double *x, Index nrhs) const {
    const MKL_INT error =
        run_phase(/*phase=*/33, /*use_matrix=*/true, static_cast<MKL_INT>(nrhs), b, x);
    if (error != 0) {
        throw std::runtime_error(
            fmt::format("SymmetricFactor::solve: backend solve failed, backend error {} ({})",
                        static_cast<int>(error), pardiso_error_text(error)));
    }
    // iparm[6]: refinement steps actually performed.
    refinement_iters_ = iparm_[6];
}

void FactorSession::solve_partial(int phase, const double *b, double *x) const {
    // Pardiso documents that a step-by-step (phase 33x) solve requires the
    // refinement cap at zero, "otherwise PARDISO produces wrong result" --
    // silently, with no error code. Saved and restored around the call so a
    // configured refinement setting neither leaks into partial solves nor is
    // lost from the full ones.
    const MKL_INT saved_refinement_cap = iparm_[7];
    iparm_[7] = 0;
    const MKL_INT error =
        run_phase(static_cast<MKL_INT>(phase), /*use_matrix=*/true, /*nrhs=*/1, b, x);
    iparm_[7] = saved_refinement_cap;

    if (error != 0) {
        throw std::runtime_error(fmt::format("SymmetricFactor::solve_partial: backend solve (phase "
                                             "{}) failed, backend error {} ({})",
                                             phase, static_cast<int>(error),
                                             pardiso_error_text(error)));
    }
    refinement_iters_ = iparm_[6];
}

} // namespace hven::linear::detail
