// The rig's NATIVE arm: hven::linear::SymmetricFactor driven through the
// SeamUnderTest surface. This is the product under test, and the only adapter
// that is permanent -- the two old-seam adapters beside it are deleted once
// the engine migrations close, at which point the traces stay as regression
// tests against this one.
//
// There is almost nothing to adapt: the surface the rig defines was drawn
// from this class's contract, so this file is mostly forwarding. What it does
// carry is the mapping from the rig's backend-neutral SeamOptions onto
// SymmetricFactor::Options, including the don't-write-by-default ordering and
// weighted-matching semantics, which the parity arms depend on being exact.

#include <memory>
#include <stdexcept>
#include <utility>

#include "seam.h"

namespace hven::rig {
namespace {

namespace hl = hven::linear;

hl::SymmetricFactor::Options to_native_options(const SeamOptions &o) {
    hl::SymmetricFactor::Options n;
    n.kind = hl::FactorKind::kLDLT;
    n.num_threads = o.num_threads;
    n.pivot_perturb_exp = o.pivot_perturb_exp;
    n.max_refinement_iters = o.max_refinement_iters;
    switch (o.ordering) {
    case SeamOptions::Ordering::kBackendDefault:
        n.ordering = hl::SymmetricFactor::Options::Ordering::kBackendDefault;
        break;
    case SeamOptions::Ordering::kMinimumDegree:
        n.ordering = hl::SymmetricFactor::Options::Ordering::kMinimumDegree;
        break;
    case SeamOptions::Ordering::kNestedDissection:
        n.ordering = hl::SymmetricFactor::Options::Ordering::kNestedDissection;
        break;
    case SeamOptions::Ordering::kParallelNestedDissection:
        n.ordering = hl::SymmetricFactor::Options::Ordering::kParallelNestedDissection;
        break;
    }
    n.weighted_matching = o.weighted_matching;
    return n;
}

class NativeHandle final : public SeamHandle {
  public:
    explicit NativeHandle(std::shared_ptr<const hl::Factorization> f) : f_(std::move(f)) {}

    hl::SolveInfo solve(const Vec &rhs, Vec &x) const override { return f_->solve(rhs, x); }
    hl::InertiaEvidence inertia() const override { return f_->inertia(); }
    std::uint64_t epoch() const override { return f_->epoch(); }
    std::uint64_t pattern_hash() const override { return f_->pattern_hash(); }

    const std::shared_ptr<const hl::Factorization> &native() const { return f_; }

  private:
    std::shared_ptr<const hl::Factorization> f_;
};

class NativeSeam final : public SeamUnderTest {
  public:
    explicit NativeSeam(const SeamOptions &opts) : opts_(opts), engine_(to_native_options(opts)) {}

    NativeSeam(const SeamOptions &opts, hl::SymmetricFactor engine)
        : opts_(opts), engine_(std::move(engine)) {}

    SeamId id() const override { return SeamId::kNative; }

    Capabilities capabilities() const override {
        Capabilities c;
        c.partial_solve = true;
        c.partial_solve_predicate = true;
        c.share_handle = true;
        c.epoch = true;
        c.adopt = true;
        c.multi_rhs = true;
        // Both evidence fields are SURFACED by this seam on every backend --
        // whether they carry a value is the backend's answer to give, and on
        // Accelerate `perturbed_pivots` is legitimately absent. The
        // capability says the surface exists; the evidence says what it holds.
        c.reports_refinement_iters = true;
        c.reports_perturbed_pivots = true;
        c.reports_inertia = true;
        return c;
    }

    // The mechanism this arm ACTUALLY pinned with, which is backend-split
    // even though the option that requests it is not.
    //
    // On MKL, Options::num_threads is applied at call scope around every
    // backend call and undone afterwards (src/linear/pardiso_session.cpp's
    // MklThreadScope) -- the per-instance mechanism the unified surface
    // specifies, never a process-global or environment setting.
    //
    // On Accelerate there is NO pin. The session stores num_threads so that
    // adopt() can round-trip Options faithfully and applies it to nothing
    // (hven/detail/linear/accelerate_session.h documents that as
    // best-effort-absent: Accelerate exposes no per-instance thread control,
    // and hven does not reach for the process-scoped one either). Reporting
    // kPerInstance here would stamp every Accelerate row -- including the
    // committed Mac float rows, whose whole claim to repeatability rests on
    // this column -- with a pin that no code applies. The value goes to 0 for
    // the same reason: `opts_.num_threads` is what was ASKED for, and naming
    // it would be naming a count nothing ran at.
    ThreadPinMechanism thread_pin_mechanism() const override {
#if defined(__APPLE__)
        return ThreadPinMechanism::kAbsent;
#else
        return ThreadPinMechanism::kPerInstance;
#endif
    }

    int thread_pin_value() const override {
#if defined(__APPLE__)
        return 0;
#else
        return opts_.num_threads;
#endif
    }

    std::string configuration_note() const override {
        // Every SeamOptions field this arm was given is applied verbatim --
        // the rig's neutral option set was drawn from this class's own --
        // with one backend-specific exception noted below (num_threads on
        // Accelerate), so the note only has to say what actually ends up in
        // force for each knob. The ordering method itself is cross-platform,
        // but WHAT IS
        // ACTUALLY IN FORCE per backend is not -- MKL writes (or, at
        // kBackendDefault, leaves alone) Pardiso's iparm[1], a genuine
        // don't-write-by-default control; Accelerate has no such sentinel for
        // ordering -- symmetric_factor_accelerate.cpp's config_from() always
        // assigns orderMethod, on every Ordering value including
        // kBackendDefault -- so the two backends report their own real
        // vocabulary here rather than a shared, Pardiso-only one that would
        // be fiction on the Accelerate arm. weighted_matching remains a
        // genuine don't-write-by-default knob on the MKL side only (Accelerate
        // rejects a true value at construction, so this note never runs with
        // it there).
        // One SeamOptions field is the exception to "applied verbatim" and
        // says so first: on Accelerate, num_threads is stored and never
        // applied (see thread_pin_mechanism() above), so the note leads with
        // the knob that is NOT in force rather than burying it.
#if defined(__APPLE__)
        std::string note = "applies every requested option EXCEPT num_threads, which this backend "
                           "stores without applying (no thread pin is in force); ordering=";
#else
        std::string note = "applies every requested option; ordering=";
#endif
#if defined(__APPLE__)
        switch (opts_.ordering) {
        case SeamOptions::Ordering::kBackendDefault:
            note += "backend-default (resolves to SparseOrderDefault, Apple's own default for "
                    "symmetric matrices, always assigned to orderMethod)";
            break;
        case SeamOptions::Ordering::kMinimumDegree:
            note += "minimum-degree (SparseOrderAMD)";
            break;
        case SeamOptions::Ordering::kNestedDissection:
            note += "nested-dissection (SparseOrderMetis)";
            break;
        case SeamOptions::Ordering::kParallelNestedDissection:
            note += "parallel-nested-dissection (SparseOrderMTMetis, downgraded to "
                    "SparseOrderMetis at runtime on a host that lacks it)";
            break;
        }
#else
        switch (opts_.ordering) {
        case SeamOptions::Ordering::kBackendDefault:
            note += "backend-default (iparm[1] not written)";
            break;
        case SeamOptions::Ordering::kMinimumDegree:
            note += "minimum-degree (iparm[1]=0)";
            break;
        case SeamOptions::Ordering::kNestedDissection:
            note += "nested-dissection (iparm[1]=2)";
            break;
        case SeamOptions::Ordering::kParallelNestedDissection:
            note += "parallel-nested-dissection (iparm[1]=3)";
            break;
        }
#endif
        note += opts_.weighted_matching ? ", weighted_matching=on (iparm[12]=1)"
                                        : ", weighted_matching=off (iparm[12] not written)";
        return note;
    }

    void analyze(const SpMatRM &A) override { engine_.analyze(A); }
    hl::FactorizeOutcome factorize(const SpMatRM &A) override { return engine_.factorize(A); }
    hl::SolveInfo solve(const Vec &rhs, Vec &x) override { return engine_.solve(rhs, x); }
    hl::SolveInfo solve_multi(const Mat &RHS, Mat &X) override { return engine_.solve(RHS, X); }

    hl::SolveInfo solve_partial(hl::SymmetricFactor::SolvePhase phase, const Vec &rhs,
                                Vec &x) override {
        return engine_.solve_partial(phase, rhs, x);
    }

    bool supports_partial_solve() const override { return engine_.supports_partial_solve(); }
    hl::InertiaEvidence inertia() const override { return engine_.inertia(); }

    Counters counters() const override {
        const hl::SymmetricFactor::Counters &c = engine_.counters();
        return Counters{c.analyze_count, c.factorize_count, c.solve_count, c.partial_solve_count};
    }

    std::shared_ptr<const SeamHandle> share() override {
        return std::make_shared<NativeHandle>(engine_.share());
    }

    std::uint64_t epoch() const override { return engine_.epoch(); }

    std::unique_ptr<SeamUnderTest> adopt(std::shared_ptr<const SeamHandle> handle) const override {
        const auto *nh = dynamic_cast<const NativeHandle *>(handle.get());
        if (nh == nullptr) {
            throw std::invalid_argument(
                "NativeSeam::adopt: handle was not emitted by the native seam");
        }
        // The adopting engine inherits the emitter's options from the session
        // itself (SymmetricFactor::adopt's own contract); opts_ is carried
        // here only so the adopted arm reports the same thread pin.
        return std::make_unique<NativeSeam>(opts_, hl::SymmetricFactor::adopt(nh->native()));
    }

  private:
    SeamOptions opts_;
    hl::SymmetricFactor engine_;
};

} // namespace

std::unique_ptr<SeamUnderTest> make_native_seam(const SeamOptions &opts) {
    return std::make_unique<NativeSeam>(opts);
}

} // namespace hven::rig
