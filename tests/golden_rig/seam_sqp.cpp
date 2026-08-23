// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// TEMPORARY, TEST-ONLY: the rig's arm over the SQP engine's OLD linear seam --
// tycho::sqp::KktSystem (MKL Pardiso, phase-numbered) from the sibling
// checkout named by the CMake option HVEN_RIG_SQP_SEAM, pinned to that
// checkout's phase-7-close tag (see tests/golden_rig/CMakeLists.txt, which
// records the tag's commit hash and verifies it at configure time).
//
// THIS FILE IS DELETED WHEN THE SQP ENGINE FINISHES MIGRATING onto
// hven::linear. Compiled only when the CMake option is supplied; never
// installed, never linked into the hven library.
//
// WHAT THIS SEAM DOES AND DOES NOT CONFIGURE, since it drives the parity
// arms: it calls its library's initializer once and then writes exactly ONE
// backend parameter (zero-based CSR indexing), riding the initializer's own
// values for ordering, static pivot perturbation, weighted matching, and the
// iterative-refinement cap. hven writes the refinement cap explicitly and its
// frozen default is zero, so this seam's FULL solves refine and hven's
// default-configured ones do not. That is a configuration difference, not a
// defect on either side, and it is why the arm table carries a
// native-sqp-parity arm whose refinement cap is read from a live initializer
// probe rather than assumed.
//
// The one backend parameter this seam writes DURING a solve is the
// correctness rule the audit's runtime shim is pre-registered to find: the
// refinement cap is forced to zero and restored around every phase-split
// solve, because the backend produces a silently wrong answer otherwise. It is
// not an option and no options grep would find it -- see audit/.

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <Eigen/SparseCore>
#include <mkl_service.h>

#include <tycho_sqp/kkt_system.h>
#include <tycho_sqp/types.h>

#include "seam.h"

namespace hven::rig {
namespace {

namespace hl = hven::linear;

static_assert(std::is_same_v<tycho::sqp::SpMatU, SpMatRM>,
              "the sqp old seam's matrix type must be hven's own, or the rig would be comparing "
              "two different storage conventions");

// See seam_psiopt.cpp's twin for why the pin is applied to the process: this
// seam has no thread control whatsoever -- it never writes a thread-shaped
// backend parameter -- so "pin by the mechanism the seam possesses" resolves
// to none, and the rig pins the process instead and says so in the provenance
// banner.
class ProcessThreadPin {
  public:
    explicit ProcessThreadPin(int n) : engaged_(n > 0) {
        if (engaged_) {
            previous_ = mkl_get_max_threads();
            mkl_set_num_threads(n);
        }
    }
    ~ProcessThreadPin() {
        if (engaged_) {
            mkl_set_num_threads(previous_);
        }
    }
    ProcessThreadPin(const ProcessThreadPin &) = delete;
    ProcessThreadPin &operator=(const ProcessThreadPin &) = delete;

  private:
    bool engaged_ = false;
    int previous_ = 0;
};

class SqpSeam final : public SeamUnderTest {
  public:
    explicit SqpSeam(const SeamOptions &opts)
        : opts_(opts), pin_(opts.num_threads), kkt_(tycho::sqp::QpOptions{}) {}

    SeamId id() const override { return SeamId::kSqpOld; }

    Capabilities capabilities() const override {
        Capabilities c;
        c.partial_solve = true;
        c.partial_solve_predicate = true;
        // No handle/epoch/adopt: this seam's factorization is owned by the
        // object and cannot outlive it or be co-owned, which is the gap the
        // unified surface's epoch-stamped handle closes.
        c.share_handle = false;
        c.epoch = false;
        c.adopt = false;
        // Its solve takes one right-hand side and returns a fresh vector.
        c.multi_rhs = false;
        // Its parameter array is private, so the refinement-step readback is
        // genuinely unreachable through this seam. Absent, not zero.
        c.reports_refinement_iters = false;
        c.reports_perturbed_pivots = true;
        c.reports_inertia = true;
        return c;
    }

    ThreadPinMechanism thread_pin_mechanism() const override {
        return ThreadPinMechanism::kProcessGlobal;
    }
    int thread_pin_value() const override { return opts_.num_threads; }

    std::string configuration_note() const override {
        return "writes exactly one backend parameter (zero-based CSR indexing) and rides its "
               "library initializer for ordering, pivot perturbation, weighted matching and the "
               "refinement cap; forces the refinement cap to zero and restores it around every "
               "phase-split solve. None of the rig's four backend-parameter options is "
               "applicable to this seam -- its full solves refine up to the initializer's own "
               "cap regardless of what was requested, which the native-sqp-parity arm mirrors "
               "from a live probe of that same initializer.";
    }

    void analyze(const SpMatRM &A) override {
        kkt_.analyze(A);
        ++counters_.analyze_count;
        dim_ = A.rows();
    }

    hl::FactorizeOutcome factorize(const SpMatRM &A) override {
        // This seam re-runs its own symbolic analysis inside factorize()
        // whenever the pattern changed. That implicit analysis is real work
        // and is attributed to analyze_count here -- observed through the
        // seam's own public pattern predicate, before the call, so the count
        // reflects what the backend did rather than what the rig wished for.
        const bool implicit_analyze = !kkt_.pattern_matches(A);

        hl::FactorizeOutcome out;
        try {
            kkt_.factorize(A);
        } catch (const std::runtime_error &) {
            // This seam reports a backend failure by throwing and discards the
            // raw error code in the message. There is no outcome object to
            // read, so the rig records the failure as a state and leaves the
            // code at its unknown-sentinel; the loss is the finding.
            if (implicit_analyze) {
                ++counters_.analyze_count;
            }
            ++counters_.factorize_count;
            out.status = hl::FactorizeOutcome::Status::kBackendError;
            out.backend_code = 0;
            return out;
        }

        if (implicit_analyze) {
            ++counters_.analyze_count;
        }
        ++counters_.factorize_count;
        dim_ = A.rows();

        out.status = hl::FactorizeOutcome::Status::kOk;
        out.backend_code = 0;
        out.inertia = evidence();
        return out;
    }

    hl::SolveInfo solve(const Vec &rhs, Vec &x) override {
        x = kkt_.solve(rhs);
        ++counters_.solve_count;
        // Absent, never zero: this seam's parameter array is private.
        return hl::SolveInfo{};
    }

    hl::SolveInfo solve_multi(const Mat &, Mat &) override {
        throw std::logic_error("sqp old seam: no multi-RHS solve exists on this seam");
    }

    hl::SolveInfo solve_partial(hl::SymmetricFactor::SolvePhase phase, const Vec &rhs,
                                Vec &x) override {
        switch (phase) {
        case hl::SymmetricFactor::SolvePhase::kForward:
            x = kkt_.solve_forward(rhs);
            break;
        case hl::SymmetricFactor::SolvePhase::kDiagonal:
            x = kkt_.solve_diagonal(rhs);
            break;
        case hl::SymmetricFactor::SolvePhase::kBackward:
            x = kkt_.solve_backward(rhs);
            break;
        }
        ++counters_.partial_solve_count;
        return hl::SolveInfo{};
    }

    bool supports_partial_solve() const override { return kkt_.supports_partial_solve(); }

    hl::InertiaEvidence inertia() const override { return evidence(); }
    Counters counters() const override { return counters_; }

    std::shared_ptr<const SeamHandle> share() override {
        throw std::logic_error("sqp old seam: no shared factorization handle exists");
    }
    std::uint64_t epoch() const override {
        throw std::logic_error("sqp old seam: no epoch exists on this seam");
    }
    std::unique_ptr<SeamUnderTest> adopt(std::shared_ptr<const SeamHandle>) const override {
        throw std::logic_error("sqp old seam: no adopt path exists on this seam");
    }

  private:
    hl::InertiaEvidence evidence() const {
        hl::InertiaEvidence e;

        // NO PRE-FACTORIZATION GUARD, deliberately, and for the same reason
        // the Apple branch of the other old-seam adapter has none: this seam
        // DEFINES an answer before anything is factorized. Its parameter array
        // is zero-filled at construction and its three accessors are plain
        // reads of it, so asking for an inertia before a factorization returns
        // zeros -- reported as though observed, with no state that could say
        // "nothing here". That is a real, reachable instance of the
        // fabrication class the unified surface's explicit kUnavailable
        // exists to close, and this adapter reports it rather than
        // substituting the new surface's honest answer. An adapter that
        // answered kUnavailable here would make this seam's
        // before-factorization rows indistinguishable from the native arm's,
        // which is precisely the reading a derivation must not be handed.
        //
        // The zero class is DERIVED, as it must be -- this seam offers only
        // the positive and negative counts -- and the flag says so.
        e.state = hl::InertiaEvidence::State::kObserved;
        e.n_pos = static_cast<Index>(kkt_.num_pos_eigs());
        e.n_neg = static_cast<Index>(kkt_.num_neg_eigs());
        e.n_zero = dim_ - e.n_pos - e.n_neg;
        e.zero_is_derived = true;
        e.perturbed_pivots = static_cast<Index>(kkt_.num_perturbed_pivots());
        return e;
    }

    SeamOptions opts_;
    ProcessThreadPin pin_;
    tycho::sqp::KktSystem kkt_;
    Counters counters_;
    Index dim_ = 0;
};

} // namespace

std::unique_ptr<SeamUnderTest> make_sqp_seam(const SeamOptions &opts) {
    return std::make_unique<SqpSeam>(opts);
}

} // namespace hven::rig
