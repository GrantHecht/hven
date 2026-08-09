// TEMPORARY, TEST-ONLY: the rig's arm over the interior-point engine's OLD
// linear seam -- Eigen::PardisoLDLT on MKL platforms, Eigen::AccelerateLDLTTPP
// on Apple, both from the sibling checkout named by the CMake option
// HVEN_RIG_PSIOPT_SEAM. Compiled only when that option is supplied; never
// installed, never linked into the hven library.
//
// THIS FILE IS DELETED WHEN THE INTERIOR-POINT ENGINE FINISHES MIGRATING onto
// hven::linear. It exists for exactly one purpose: to let a trace run against
// the seam that engine trusts today, so the expected tables are derived from
// observation of the trusted thing rather than asserted about the new one.
//
// THE ADAPTER REPORTS WHAT THE OLD SEAM SAYS, INCLUDING WHERE THE OLD SEAM IS
// WRONG. Two readings on the Apple branch below are fabrications the unified
// surface exists to fix -- a perturbed-pivot count hardcoded to zero on a
// backend with no such counter, and an inertia query whose failure is
// zero-filled rather than reported. Both are carried through here verbatim,
// because the traces that name them expect this arm to FAIL against the new
// semantics; smoothing them over here would delete the finding.

#include <memory>
#include <stdexcept>

#include <Eigen/SparseCore>

#if defined(__APPLE__)
#include <tycho/detail/solvers/linear/accelerate_interface.h>
#else
#include <mkl_service.h>

#include <tycho/detail/solvers/linear/pardiso_interface.h>
#endif

#include "seam.h"

namespace hven::rig {
namespace {

namespace hl = hven::linear;

#if defined(__APPLE__)
using OldSolver = Eigen::AccelerateLDLTTPP<SpMatRM, Eigen::Upper>;
#else
using OldSolver = Eigen::PardisoLDLT<SpMatRM, Eigen::Upper>;
#endif

// Pins the PROCESS thread count for this arm's lifetime and restores it
// afterwards. MKL-ONLY: its constructor and destructor bodies are both
// `#if !defined(__APPLE__)`, so on Apple this class is inert -- the real
// process-scoped control on that branch is solver_.set_num_threads(), called
// from configure() below, not this class. See thread_pin_mechanism() for why
// that is reported as kProcessGlobal on both branches despite the different
// mechanism underneath.
//
// Every asserted run must pin threads by the mechanism the seam under test
// possesses, and PardisoLDLT (the MKL branch's solver) possesses none. Its
// one thread-shaped field, PardisoLDLT::threads_ (written into iparm[33]), is
// the conditional-numerical-reproducibility slot rather than a thread count,
// and the interior-point engine only writes it when its CNR mode is on -- so
// pinning through it would be a claim this rig cannot support. The pin is
// therefore applied to the process, and the mechanism is REPORTED as
// process-global so the expected tables' provenance banner says what actually
// happened. Derivation runs are additionally invoked with MKL_NUM_THREADS=1 in
// the environment (docs/testing.md's rig section); this guard is the in-process
// belt that does not depend on when MKL first read the environment.
class ProcessThreadPin {
  public:
    explicit ProcessThreadPin(int n) : engaged_(n > 0) {
#if !defined(__APPLE__)
        if (engaged_) {
            previous_ = mkl_get_max_threads();
            mkl_set_num_threads(n);
        }
#endif
    }
    ~ProcessThreadPin() {
#if !defined(__APPLE__)
        if (engaged_) {
            mkl_set_num_threads(previous_);
        }
#endif
    }
    ProcessThreadPin(const ProcessThreadPin &) = delete;
    ProcessThreadPin &operator=(const ProcessThreadPin &) = delete;

  private:
    bool engaged_ = false;
    int previous_ = 0;
};

class PsioptSeam final : public SeamUnderTest {
  public:
    explicit PsioptSeam(const SeamOptions &opts) : opts_(opts), pin_(opts.num_threads) {
        configure();
    }

    SeamId id() const override { return SeamId::kPsioptOld; }

    Capabilities capabilities() const override {
        Capabilities c;
        // No phase-split solve on this seam at all: PardisoImpl's only solve
        // path is phase 33, and there is no composability predicate to gate
        // one with. Traces that need either SKIP on this arm.
        c.partial_solve = false;
        c.partial_solve_predicate = false;
        // No handle, no epoch, no adopt: this seam's factorization lives and
        // dies with the solver object, which is precisely the lifecycle gap
        // the unified surface's co-owning handle closes.
        c.share_handle = false;
        c.epoch = false;
        c.adopt = false;
        c.multi_rhs = true;
        c.reports_refinement_iters = kReportsRefinementIters;
        c.reports_perturbed_pivots = true;
        c.reports_inertia = true;
        return c;
    }

    ThreadPinMechanism thread_pin_mechanism() const override {
#if defined(__APPLE__)
        // NOT absent: configure() (below) calls solver_.set_num_threads(),
        // which resolves to accelerate_set_num_threads()
        // (tycho/detail/solvers/linear/accelerate_utils.h) -- BLASSetThreading
        // on macOS 15+, or else the VECLIB_MAXIMUM_THREADS environment
        // variable, read once at the first BLAS call. Either way it is a
        // real, process-scoped control, not the per-instance solver option
        // this class reports for other seams -- reporting kAbsent here would
        // itself be the fabrication this adapter exists to avoid. What is
        // genuinely missing is HARDWARE OBSERVATION: this branch has never
        // been compiled or run (see configuration_note() below), so the
        // mechanism is reported honestly while remaining unexercised.
        return ThreadPinMechanism::kProcessGlobal;
#else
        return ThreadPinMechanism::kProcessGlobal;
#endif
    }
    int thread_pin_value() const override { return opts_.num_threads; }

    std::string configuration_note() const override {
#if defined(__APPLE__)
        return "UNOBSERVED (never compiled or run): Accelerate LDLT-TPP with the "
               "interior-point engine's own order/refinement settings; thread control is "
               "process-scoped (BLASSetThreading on macOS 15+, else VECLIB_MAXIMUM_THREADS), "
               "not per-instance";
#else
        return "the interior-point engine's own set_params(): writes ~25 iparm entries "
               "unconditionally (including iparm[4]=2, whose only in-tree reader is dead code), "
               "so it has no don't-write-by-default state -- an ordering request of "
               "backend-default is applied as that engine's own METIS value. Raw Pardiso error "
               "codes are not surfaced by this seam; Eigen's ComputationInfo stands in.";
#endif
    }

    void analyze(const SpMatRM &A) override {
        solver_.analyze_pattern(A);
        ++counters_.analyze_count;
        dim_ = A.rows();
    }

    hl::FactorizeOutcome factorize(const SpMatRM &A) override {
        solver_.factorize(A);
        ++counters_.factorize_count;
        dim_ = A.rows();

        hl::FactorizeOutcome out;
        const Eigen::ComputationInfo info = solver_.info();
        out.status = (info == Eigen::Success) ? hl::FactorizeOutcome::Status::kOk
                                              : hl::FactorizeOutcome::Status::kBackendError;
        // This seam does not surface Pardiso's raw error code -- PardisoImpl
        // folds it into Eigen's ComputationInfo and discards the original. The
        // ComputationInfo value is recorded in its place, and the loss is the
        // finding rather than an adapter shortcut.
        out.backend_code = static_cast<int>(info);
        out.inertia = evidence();
        return out;
    }

    hl::SolveInfo solve(const Vec &rhs, Vec &x) override {
        x = solver_.solve(rhs);
        ++counters_.solve_count;
        return solve_info();
    }

    hl::SolveInfo solve_multi(const Mat &RHS, Mat &X) override {
        X = solver_.solve(RHS);
        ++counters_.solve_count;
        return solve_info();
    }

    hl::SolveInfo solve_partial(hl::SymmetricFactor::SolvePhase, const Vec &, Vec &) override {
        throw std::logic_error("psiopt old seam: no phase-split solve exists on this seam");
    }

    bool supports_partial_solve() const override {
        throw std::logic_error("psiopt old seam: no partial-solve predicate exists on this seam");
    }

    hl::InertiaEvidence inertia() const override { return evidence(); }
    Counters counters() const override { return counters_; }

    std::shared_ptr<const SeamHandle> share() override {
        throw std::logic_error("psiopt old seam: no shared factorization handle exists");
    }
    std::uint64_t epoch() const override {
        throw std::logic_error("psiopt old seam: no epoch exists on this seam");
    }
    std::unique_ptr<SeamUnderTest> adopt(std::shared_ptr<const SeamHandle>) const override {
        throw std::logic_error("psiopt old seam: no adopt path exists on this seam");
    }

  private:
#if defined(__APPLE__)
    static constexpr bool kReportsRefinementIters = false;
#else
    static constexpr bool kReportsRefinementIters = true;
#endif

    // Reproduce the configuration the interior-point engine actually ships,
    // then let the rig's own SeamOptions override the four knobs the unified
    // surface names. Everything else is left at that engine's value so this
    // arm is the seam as the engine drives it, not a rig-flavoured variant.
    void configure() {
#if defined(__APPLE__)
        // UNOBSERVED: this branch has never been compiled or run. Its call
        // shapes are taken from the engine's own set_qp_params() and from
        // accelerate_interface.h's declarations; it first executes on the Mac
        // hardware leg.
        solver_.set_num_threads(opts_.num_threads);
        solver_.set_iterative_refinement(opts_.max_refinement_iters > 0);
        solver_.set_iterative_refinement_iterations(opts_.max_refinement_iters);
        // Every SeamOptions::Ordering value is representable on this seam via
        // set_order() (which applies accelerate_supported_order()'s own
        // OS-availability downgrade for kParallelNestedDissection), so it is
        // applied explicitly here -- never left as a silent no-op the way an
        // unrepresentable option would have to be.
        solver_.set_order(ordering_value());
#else
        // The engine's settings, verbatim: METIS ordering, 2x2 pivoting,
        // static pivot perturbation 10^-8, maximum weighted matching ON, no
        // scaling, classic algorithm, serial forward/backward solve, silent.
        // iparm[33] (the CNR slot the class calls `threads_`) is left at 0
        // because the engine only writes it when CNR mode is on.
        solver_.ord_ = ordering_value();
        solver_.pivotstrat_ = 1;
        solver_.pivotpert_ = opts_.pivot_perturb_exp;
        solver_.matching_ = opts_.weighted_matching ? 1 : 0;
        solver_.scaling_ = 0;
        solver_.iterref_ = opts_.max_refinement_iters;
        solver_.alg_ = 0;
        solver_.msglvl_ = 0;
        solver_.threads_ = 0;
        solver_.parsolve_ = 0;
        solver_.set_params();
#endif
    }

#if defined(__APPLE__)
    // Every SeamOptions::Ordering value maps onto a real Accelerate order
    // method (mirroring hven::linear's own accelerate_ordering_code()), so
    // this seam represents all four -- unlike weighted_matching, ordering is
    // not a Pardiso-only concept here either.
    //
    // SparseOrderMTMetis is only DECLARED starting in the macOS 26 SDK --
    // exactly the same SDK-version gap
    // src/linear/symmetric_factor_accelerate.cpp's HVEN_HAS_MTMETIS guards.
    // This seam already includes
    // tycho/detail/solvers/linear/accelerate_interface.h (above), which pulls
    // in accelerate_utils.h's own TYCHO_HAS_MTMETIS / accelerate_supported_order()
    // -- the very guard that hven's production file was written to mirror --
    // so this arm reuses psiopt's own guard directly rather than duplicating
    // it, with the same fallback semantics: on a pre-SDK-26 Apple build,
    // SparseOrderMTMetis is not declared at all, and this case downgrades to
    // SparseOrderMetis at compile time instead.
    SparseOrder_t ordering_value() const {
        switch (opts_.ordering) {
        case SeamOptions::Ordering::kMinimumDegree:
            return SparseOrderAMD;
        case SeamOptions::Ordering::kNestedDissection:
            return SparseOrderMetis;
        case SeamOptions::Ordering::kParallelNestedDissection:
#ifdef TYCHO_HAS_MTMETIS
            // set_order() (accelerate_interface.h) applies
            // accelerate_supported_order() again on this value before it
            // reaches Accelerate, so this call is a compile-time SDK
            // declaration guard first and a belt-and-suspenders runtime
            // downgrade second -- never a correctness gap.
            return accelerate_supported_order(SparseOrderMTMetis);
#else
            // This SDK does not declare SparseOrderMTMetis at all -- the same
            // state a host older than macOS 26 downgrades to at runtime via
            // accelerate_supported_order(), reached here at compile time
            // instead.
            return SparseOrderMetis;
#endif
        case SeamOptions::Ordering::kBackendDefault:
            break;
        }
        // This seam has no don't-write-by-default state: order_ is a plain
        // data member with a class default (SparseOrderMetis), always applied
        // via set_order() above. kBackendDefault therefore maps to the value
        // the interior-point engine itself ships (METIS), the same
        // non-representability recorded for the MKL branch below, and in the
        // arm table's parity note.
        return SparseOrderMetis;
    }
#else
    int ordering_value() const {
        switch (opts_.ordering) {
        case SeamOptions::Ordering::kMinimumDegree:
            return 0;
        case SeamOptions::Ordering::kNestedDissection:
            return 2;
        case SeamOptions::Ordering::kParallelNestedDissection:
            return 3;
        case SeamOptions::Ordering::kBackendDefault:
            break;
        }
        // This seam has no don't-write-by-default state: set_params() always
        // assigns iparm[1]. kBackendDefault therefore maps to the value the
        // interior-point engine itself ships (METIS = 2), and the
        // non-representability is recorded in the arm table's parity note
        // rather than papered over with a silent zero.
        return 2;
    }
#endif

    hl::InertiaEvidence evidence() const {
        hl::InertiaEvidence e;

        // THE PRE-FACTORIZATION ANSWER IS DIFFERENT ON THE TWO BACKENDS, and
        // this adapter reports each one as it actually is. Its job is
        // FIDELITY TO EACH SEAM'S REAL BEHAVIOUR -- never smoothing either of
        // them toward the semantics of the surface that replaces them. A
        // single shared guard here would be exactly that smoothing, and it
        // would delete a finding on the Apple side.
#if defined(__APPLE__)
        // UNOBSERVED, and deliberately faithful to three fabrications:
        //
        //  (1) ppivs() on this seam is `return 0;` -- a hardcoded literal on a
        //      backend with no perturbed-pivot counter. Carried through as a
        //      PRESENT zero, which is exactly what the unified surface's
        //      absent optional now forbids, and exactly what the trace that
        //      names this case asserts against.
        //  (2) an inertia query that FAILS is zero-filled by this seam
        //      (cacheInertia's else branch sets all three counts to 0) with no
        //      way for a caller to tell that from a real reading, so this
        //      adapter can only ever report kObserved. The missing state is
        //      the finding.
        //  (3) NO PRE-FACTORIZATION GUARD ON THIS BRANCH, deliberately. This
        //      seam's count members are DEFINED here -- zero-initialized at
        //      construction -- so asking it for an inertia before anything has
        //      been factorized has a real answer, and that answer is a
        //      zero-filled triple reported as though observed. That is the
        //      same fabrication class as (2), reachable without provoking any
        //      backend fault, and it is the slice this rig can actually drive.
        //      Substituting kUnavailable here (as the MKL branch below must,
        //      for a different and genuine reason) would report the NEW
        //      surface's honest answer in place of the OLD seam's real one,
        //      and the fail-by-design this arm exists to produce would
        //      silently never happen.
        e.state = hl::InertiaEvidence::State::kObserved;
        e.n_pos = static_cast<Index>(solver_.peigs());
        e.n_neg = static_cast<Index>(solver_.neigs());
        e.n_zero = static_cast<Index>(solver_.zeigs());
        e.zero_is_derived = false;
        e.perturbed_pivots = static_cast<Index>(solver_.ppivs());
#else
        // The MKL branch DOES need the guard, and for a reason that is a
        // finding in its own right rather than a convenience: this seam has no
        // defined pre-factorization inertia state at all. Its count members
        // are plain ints the constructor never initializes (it zeroes the
        // parameter array and nothing else), so reading them before a
        // factorization is an indeterminate read -- there is no answer to
        // report faithfully and no way for the seam to say so. kUnavailable is
        // the only representation that is not an invention, and the absence of
        // a defined state is itself the observation. Note the asymmetry with
        // the Apple branch above is a property of the two seams, not of this
        // adapter's convenience.
        if (counters_.factorize_count == 0) {
            return e; // kUnavailable, counts left at their -1 sentinel
        }
        e.state = hl::InertiaEvidence::State::kObserved;
        e.n_pos = static_cast<Index>(solver_.peigs());
        e.n_neg = static_cast<Index>(solver_.neigs());
        e.n_zero = dim_ - e.n_pos - e.n_neg;
        e.zero_is_derived = true;
        e.perturbed_pivots = static_cast<Index>(solver_.ppivs());
#endif
        return e;
    }

    hl::SolveInfo solve_info() const {
        hl::SolveInfo s;
#if !defined(__APPLE__)
        // iparm[6]: refinement steps actually performed. PardisoLDLT exposes
        // the whole parameter array publicly, so this is a read of the seam's
        // own state, not a reconstruction.
        s.refinement_iters = static_cast<Index>(solver_.iparm_[6]);
#endif
        return s;
    }

    SeamOptions opts_;
    ProcessThreadPin pin_;
    OldSolver solver_;
    Counters counters_;
    Index dim_ = 0;
};

} // namespace

std::unique_ptr<SeamUnderTest> make_psiopt_seam(const SeamOptions &opts) {
    return std::make_unique<PsioptSeam>(opts);
}

} // namespace hven::rig
