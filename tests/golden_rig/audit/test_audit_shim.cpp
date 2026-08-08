// The consumed-surface audit's own self-tests.
//
// THE PRE-REGISTERED COVERAGE TEST. The audit is required to find, through its
// runtime instrument and without being told it exists, the correctness rule
// that forces the iterative-refinement cap to zero around a phase-split solve
// and restores it afterwards. That rule is not an option; a grep over an
// option surface would never produce it; if the instrument cannot find it, the
// audit has a demonstrated coverage gap and its METHOD is wrong, not just its
// output. So it is asserted here, before any audit output is trusted.
//
// The test is written twice over: once against the library's own seam, so it
// runs on every commit in the default suite, and once against the old seam
// that the rule was originally read out of, when that checkout is configured
// in. The second is the real coverage claim; the first is what keeps the
// instrument working between derivation runs.

#include <algorithm>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "hven/linear/symmetric_factor.h"
#include "pardiso_recorder.h"
#include "recipes.h"
#include "seam.h"

#if defined(HVEN_RIG_HAVE_SQP_SEAM)
#include <tycho_sqp/kkt_system.h>
#include <tycho_sqp/types.h>
#endif

namespace hven::rig::audit {
namespace {

namespace hl = hven::linear;

constexpr int kRefinementCapEntry = 7;
constexpr int kFullSolvePhase = 33;
constexpr int kForwardPhase = 331;

bool contains(const std::vector<int> &v, int x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

// The instrument sees the backend at all: without this, every assertion below
// would pass vacuously on a build whose wrapper was not wired up.
TEST(AuditRuntimeShim, RecordsBackendPhasesAtAll) {
    const Fixture fx = pd_on_face_kkt(6, 2);

    RecordingWindow window;
    hl::SymmetricFactor::Options opts;
    opts.num_threads = 1;
    hl::SymmetricFactor engine(opts);
    engine.analyze(fx.K);
    ASSERT_EQ(engine.factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);
    Vec x(fx.K.rows());
    engine.solve(fx.rhs, x);

    const std::vector<int> phases = BackendRecorder::instance().phases();
    ASSERT_FALSE(phases.empty()) << "the runtime instrument recorded nothing -- either the "
                                    "wrapper is not linked in or recording never engaged";
    EXPECT_TRUE(contains(phases, 11)) << "a symbolic phase must have been observed";
    EXPECT_TRUE(contains(phases, 22)) << "a numeric phase must have been observed";
    EXPECT_TRUE(contains(phases, kFullSolvePhase)) << "a full solve must have been observed";
}

// Recording is genuinely scoped: a call outside a window leaves no trace, so a
// window's record cannot be contaminated by unrelated backend traffic in the
// same process.
TEST(AuditRuntimeShim, RecordsNothingOutsideAWindow) {
    BackendRecorder::instance().clear();
    const Fixture fx = pd_on_face_kkt(4, 1);
    hl::SymmetricFactor::Options opts;
    opts.num_threads = 1;
    hl::SymmetricFactor engine(opts);
    engine.analyze(fx.K);
    ASSERT_EQ(engine.factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);

    EXPECT_TRUE(BackendRecorder::instance().calls().empty());
}

// THE PRE-REGISTERED COVERAGE TEST, against the library's own seam.
TEST(AuditRuntimeShim, FindsTheRefinementCapRuleAroundAPhaseSplitSolve) {
    const Fixture fx = pd_on_face_kkt(6, 2);

    constexpr int kConfiguredCap = 2;
    hl::SymmetricFactor::Options opts;
    opts.num_threads = 1;
    opts.max_refinement_iters = kConfiguredCap;

    RecordingWindow window;
    hl::SymmetricFactor engine(opts);
    engine.analyze(fx.K);
    ASSERT_EQ(engine.factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);

    Vec x(fx.K.rows());
    engine.solve(fx.rhs, x); // a full solve, at the configured cap
    Vec y(fx.K.rows());
    engine.solve_partial(hl::SymmetricFactor::SolvePhase::kForward, fx.rhs, y);
    engine.solve(fx.rhs, x); // and another full solve, after the partial one

    const BackendRecorder &r = BackendRecorder::instance();

    // (a) The instrument found the entry WITHOUT being told which one to
    //     watch: it is simply the entry whose value moved between calls.
    EXPECT_TRUE(contains(r.entries_that_varied(), kRefinementCapEntry))
        << "the audit's runtime instrument did not notice the refinement cap changing between "
           "calls, which is the whole mechanism it is pre-registered to find";

    // (b) The zeroing half: the phase-split call ran with the cap at zero.
    EXPECT_EQ(r.parameter_at_phase(kForwardPhase, kRefinementCapEntry, -1), 0)
        << "a phase-split solve must run with the refinement cap at zero";

    // (c) The restore half: a full solve after the phase-split one is back at
    //     the configured cap. Only meaningful when the cap is nonzero, which
    //     is why this test configures one rather than riding a default.
    EXPECT_EQ(r.parameter_at_phase(kFullSolvePhase, kRefinementCapEntry, -1), kConfiguredCap)
        << "the refinement cap must be restored after a phase-split solve";
    const std::vector<BackendCall> &calls = r.calls();
    int last_full_solve_cap = -1;
    bool seen_partial = false;
    for (const BackendCall &c : calls) {
        if (c.phase == kForwardPhase) {
            seen_partial = true;
        } else if (c.phase == kFullSolvePhase && seen_partial) {
            last_full_solve_cap = c.parameters[kRefinementCapEntry];
        }
    }
    EXPECT_TRUE(seen_partial);
    EXPECT_EQ(last_full_solve_cap, kConfiguredCap)
        << "the full solve FOLLOWING the phase-split one must see the restored cap -- this is "
           "the half of the save/zero/restore rule that a missing restore would break";
}

#if defined(HVEN_RIG_HAVE_SQP_SEAM)
// THE PRE-REGISTERED COVERAGE TEST, against the old seam the rule was read out
// of. This is the claim the audit's method rests on: the instrument finds the
// rule in code the audit did not write.
TEST(AuditRuntimeShim, FindsTheRefinementCapRuleInTheOldSeam) {
    const Fixture fx = pd_on_face_kkt(6, 2);

    RecordingWindow window;
    tycho::sqp::KktSystem kkt{tycho::sqp::QpOptions{}};
    kkt.analyze(fx.K);
    kkt.factorize(fx.K);
    const Vec x_full = kkt.solve(fx.rhs);
    const Vec y = kkt.solve_forward(fx.rhs);
    const Vec x_again = kkt.solve(fx.rhs);
    (void)x_full;
    (void)y;
    (void)x_again;

    const BackendRecorder &r = BackendRecorder::instance();
    const int cap_at_full = r.parameter_at_phase(kFullSolvePhase, kRefinementCapEntry, -1);

    EXPECT_EQ(r.parameter_at_phase(kForwardPhase, kRefinementCapEntry, -1), 0)
        << "the old seam's phase-split solve must run with the refinement cap at zero";

    if (cap_at_full > 0) {
        EXPECT_TRUE(contains(r.entries_that_varied(), kRefinementCapEntry))
            << "the instrument must notice the old seam's cap moving between calls";
        int last_full_solve_cap = -1;
        bool seen_partial = false;
        for (const BackendCall &c : r.calls()) {
            if (c.phase == kForwardPhase) {
                seen_partial = true;
            } else if (c.phase == kFullSolvePhase && seen_partial) {
                last_full_solve_cap = c.parameters[kRefinementCapEntry];
            }
        }
        EXPECT_EQ(last_full_solve_cap, cap_at_full)
            << "the old seam must restore its refinement cap after a phase-split solve";
    } else {
        // The old seam configures no cap of its own -- it rides its library's
        // initializer -- so if that initializer's default is zero on this
        // build, the RESTORE half of the rule has nothing to restore and is
        // not observable here. Recorded rather than asserted away.
        GTEST_SKIP() << "the backend initializer's refinement cap is " << cap_at_full
                     << " on this build, so the restore half of the rule leaves no observable "
                        "trace through this seam; the zeroing half above still holds";
    }
}
#endif

} // namespace
} // namespace hven::rig::audit
