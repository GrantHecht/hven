// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// INVERTED-EXPECTATION CONTROLS for the traces that are supposed to fail.
//
// Two traces in this rig assert the unified surface's honesty rules against
// old seams known to break them, so a FAILURE on those arms is the finding --
// it is what a docket entry gets written from. That arrangement has a failure
// mode of its own, and it is the quiet one: if an adapter ever regresses to
// smoothing its seam's behaviour toward the new surface's, the fail-by-design
// stops firing, the suite goes green, and nothing anywhere says the finding
// was lost. A green suite would then mean "the old seam is honest" when what
// actually happened is "the rig stopped looking".
//
// These tests close that by asserting the OPPOSITE of what the traces assert:
// the old seam really does produce the dishonest evidence, in exactly the
// shape documented. If an adapter regresses to smoothing, the trace goes green
// and THESE GO RED, so the regression is loud either way and neither direction
// can pass silently.
//
// They live beside the traces rather than inside them because they are not
// traces: they pin no contract clause and derive no expectation. They are a
// guard on the instrument.

#include <gtest/gtest.h>

#include "hven/linear/symmetric_factor.h"
#include "seam.h"

namespace hven::rig {
namespace {

#if defined(HVEN_RIG_HAVE_SQP_SEAM)
namespace hl = hven::linear;
// The SQP old seam's parameter array is zero-filled at construction and its
// three inertia accessors are plain reads of it, so asking for an inertia
// before anything has been factorized returns a complete, real-looking triple
// of zeros with no state that could say "nothing here". That is the
// fabrication class the unified surface's explicit kUnavailable exists to
// close, and it is what the corresponding trace fails on.
//
// This asserts the failure's PRECONDITION rather than the failure itself: the
// adapter must still be reporting the seam's own answer. The day it reports
// kUnavailable instead, this test goes red and names the reason.
TEST(FailByDesignControl, SqpSeamStillZeroFillsItsPreFactorizationInertia) {
    SeamOptions opts;
    opts.num_threads = 1;
    std::unique_ptr<SeamUnderTest> seam = make_seam(SeamId::kSqpOld, opts);

    const hl::InertiaEvidence e = seam->inertia();

    EXPECT_EQ(e.state, hl::InertiaEvidence::State::kObserved)
        << "the SQP old seam reports a real-looking inertia before any factorization -- if this "
           "now reads kUnavailable, either the seam changed or its adapter started smoothing it "
           "toward the new surface's semantics, and the fail-by-design trace this guards has "
           "stopped finding anything";
    EXPECT_EQ(e.n_pos, 0);
    EXPECT_EQ(e.n_neg, 0);
    EXPECT_EQ(e.n_zero, 0)
        << "the zero-filled triple IS the finding: it is indistinguishable from a real reading, "
           "which is precisely why the unified surface refuses to produce one";
}

// The same guard, one layer up: the trace that consumes this evidence must
// actually be reaching the dishonest answer, not skipping past it. Asserted
// through the same public predicate the trace uses, so a change to either the
// adapter or the capability declaration is caught.
TEST(FailByDesignControl, SqpSeamDeclaresTheInertiaSurfaceTheFailingTraceNeeds) {
    SeamOptions opts;
    opts.num_threads = 1;
    std::unique_ptr<SeamUnderTest> seam = make_seam(SeamId::kSqpOld, opts);
    EXPECT_TRUE(seam->capabilities().reports_inertia)
        << "if this seam stopped declaring an inertia surface, the fail-by-design trace would "
           "SKIP rather than fail, and the docket entry would silently never file";
}
#endif // HVEN_RIG_HAVE_SQP_SEAM

#if defined(HVEN_RIG_HAVE_PSIOPT_SEAM) && defined(__APPLE__)
// UNOBSERVED: never compiled or run anywhere. The interior-point seam's Apple
// branch has the same shape by a different mechanism -- its count members are
// zero-initialized, so its pre-factorization answer is a defined zero-filled
// triple, and its perturbed-pivot accessor is a hardcoded literal zero on a
// backend with no such counter. This control first executes on the Mac leg,
// alongside the fail-by-design it guards.
TEST(FailByDesignControl, PsioptSeamStillZeroFillsItsPreFactorizationInertiaOnApple) {
    SeamOptions opts;
    opts.num_threads = 1;
    std::unique_ptr<SeamUnderTest> seam = make_seam(SeamId::kPsioptOld, opts);

    const hl::InertiaEvidence e = seam->inertia();
    EXPECT_EQ(e.state, hl::InertiaEvidence::State::kObserved);
    EXPECT_EQ(e.n_pos, 0);
    EXPECT_EQ(e.n_neg, 0);
    EXPECT_EQ(e.n_zero, 0);
    EXPECT_TRUE(e.perturbed_pivots.has_value())
        << "this seam's perturbed-pivot accessor returns a hardcoded zero on a backend with no "
           "such counter -- a PRESENT zero is the fabrication, and its absence here would mean "
           "the adapter had started reporting the new surface's honest answer instead";
}
#endif

// A note for the case where neither old seam is configured in, so the file is
// not silently empty: the controls above guard arms that only exist in a
// three-seam build, and their absence here is a property of the build rather
// than of the rig.
TEST(FailByDesignControl, ControlsArePresentForWhicheverOldSeamsThisBuildHas) {
    const bool sqp = seam_available(SeamId::kSqpOld);
    const bool psiopt = seam_available(SeamId::kPsioptOld);
    if (!sqp && !psiopt) {
        GTEST_SKIP() << "no old seam is configured into this build, so there is no "
                        "fail-by-design to guard -- configure a seam path to run the controls";
    }
    SUCCEED() << "old seams present: sqp=" << sqp << " psiopt=" << psiopt;
}

} // namespace
} // namespace hven::rig
