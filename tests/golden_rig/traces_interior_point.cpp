// The interior-point half of the trace set: P1-P6, each pinning a seam
// behaviour the interior-point engine depends on.
//
// Two of these (P4 and P5) are the FABRICATION cases. They assert the unified
// surface's semantics -- perturbation evidence ABSENT rather than zero on a
// backend that has no such counter; a failed inertia query reported as its own
// state rather than zero-filled -- which means they are EXPECTED TO FAIL on
// the old seam that fabricates those readings. That failure is the finding,
// and it is what a docket entry is written from; it is not a bug in the trace.

#include <cctype>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "recipes.h"
#include "trace_support.h"

namespace hven::rig {
namespace {

namespace hl = hven::linear;

std::string interior_point_arm_suffix(const testing::TestParamInfo<ArmSpec> &info) {
    std::string s = info.param.name;
    for (char &c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0) {
            c = '_';
        }
    }
    return s;
}

class InteriorPointTrace : public testing::TestWithParam<ArmSpec> {};

INSTANTIATE_TEST_SUITE_P(Arms, InteriorPointTrace, testing::ValuesIn(arms()),
                         interior_point_arm_suffix);

// --- P1 -- iterate-loop lifecycle --------------------------------------------
//
// The interior-point pattern: analyze once when the structure is fixed, then
// per iteration change the values (the barrier diagonal moves as the barrier
// parameter falls), refactorize, solve. The twin of the SQP side's value-only
// refactorization trace, kept separate because the matrix class differs -- this
// one carries a barrier diagonal, the other is bound-eliminated.

TEST_P(InteriorPointTrace, P1_IterateLoopLifecycle) {
    TraceRun run("P1", GetParam());
    constexpr Index kIterates = 3;

    const Fixture first = barrier_chain_kkt(chain_nodes(), /*iterate=*/1);
    run.note("fixture " + first.name + ": " + first.provenance);

    SeamUnderTest &seam = run.seam();
    seam.analyze(first.K);

    Vec x(first.K.rows());
    hl::SolveInfo info;
    for (Index k = 1; k <= kIterates; ++k) {
        const Fixture fx = barrier_chain_kkt(chain_nodes(), k);
        ASSERT_EQ(fx.K.nonZeros(), first.K.nonZeros())
            << "the barrier rung must change values only, never the pattern";
        ASSERT_EQ(seam.factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);
        info = seam.solve(fx.rhs, x);
        run.record(Observation::real("P1", run.label(),
                                     "relative_residual_iterate_" + std::to_string(k),
                                     relative_residual(fx.K, x, fx.rhs), 1e-8));
    }

    const Counters c = seam.counters();
    EXPECT_EQ(c.analyze_count, 1) << "the structure is fixed, so one analysis serves every iterate";
    EXPECT_EQ(c.factorize_count, kIterates);
    EXPECT_EQ(c.solve_count, kIterates);

    run.record_counters(c);
    run.record_inertia(seam.inertia());
    run.record_solve_info(info);
    run.record_vector_head("x_final", x, 4, TraceRun::kDefaultTolerance);
}

// --- P2 -- inertia-correction ladder replay ----------------------------------
//
// A KKT with the wrong inertia, factorized under a ladder of increasing dual
// regularization until the accepting condition holds -- as many negative
// eigenvalues as there are constraint rows, and no zero class. Recorded per
// rung: the inertia triple, the perturbation count, and which rung accepted.
// The ladder's own arithmetic belongs to the driver; what this trace pins is
// that the EVIDENCE the driver reads is there, rung by rung, and that the whole
// ladder costs one symbolic analysis.

TEST_P(InteriorPointTrace, P2_InertiaCorrectionLadderReplay) {
    TraceRun run("P2", GetParam());
    SeamUnderTest &seam = run.seam();
    RIG_REQUIRE(run, seam.capabilities().reports_inertia, "an inertia readback");

    const std::vector<double> rungs = {0.0, 1e-10, 1e-8, 1e-6, 1e-4};
    const Fixture first = duplicated_equality_kkt(/*primal_reg=*/1e-8, rungs.front());
    run.note("fixture " + first.name + ": " + first.provenance);
    run.note("ladder rungs (dual regularization): 0, 1e-10, 1e-8, 1e-6, 1e-4");

    seam.analyze(first.K);

    const Index m_face = first.n_dual;
    Index accepting_rung = -1;
    for (std::size_t r = 0; r < rungs.size(); ++r) {
        const Fixture fx = duplicated_equality_kkt(/*primal_reg=*/1e-8, rungs[r]);
        ASSERT_EQ(fx.K.nonZeros(), first.K.nonZeros())
            << "a ladder rung must change values only, never the pattern";
        const hl::FactorizeOutcome out = seam.factorize(fx.K);
        const std::string prefix = "rung" + std::to_string(r) + "_";
        run.record(Observation::state(
            "P2", run.label(), prefix + "factorize_status",
            out.status == hl::FactorizeOutcome::Status::kOk ? "kOk" : "kBackendError"));
        const hl::InertiaEvidence e = seam.inertia();
        run.record_inertia(e, prefix);
        if (accepting_rung < 0 && e.state == hl::InertiaEvidence::State::kObserved &&
            e.n_neg == m_face && e.n_zero == 0) {
            accepting_rung = static_cast<Index>(r);
        }
    }

    const Counters c = seam.counters();
    EXPECT_EQ(c.analyze_count, 1) << "the whole ladder runs on one symbolic analysis";
    EXPECT_EQ(c.factorize_count, static_cast<Index>(rungs.size()));

    run.record_counters(c);
    // Recorded, not asserted: WHICH rung accepts is a property of the backend's
    // arithmetic and is exactly what the derivation is for.
    run.record(Observation::counter("P2", run.label(), "accepting_rung", accepting_rung));
}

// --- P3 -- the singular-KKT verdict and its negative control -----------------
//
// A genuinely rank-deficient KKT, and beside it the control that must NOT read
// as singular however large its bound-curvature term gets.
//
// THE DEFICIENT CASE ASSERTS NOTHING ABOUT A ZERO CLASS. The assumption that a
// singular matrix surfaces as a nonzero zero class is known to be wrong on at
// least one backend: static pivot perturbation replaces the zero pivot with a
// signed one, so the counts sum to the dimension, the derived zero class reads
// zero, and the perturbation count is what actually carries the signal. The
// expected table therefore carries BOTH the inertia triple and the
// perturbation count as recorded quantities, and this trace records reality
// rather than pinning the superseded assumption.

TEST_P(InteriorPointTrace, P3_SingularVerdictAndControl) {
    TraceRun run("P3", GetParam());
    RIG_REQUIRE(run, run.seam().capabilities().reports_inertia, "an inertia readback");

    {
        const Fixture fx = duplicated_equality_kkt(/*primal_reg=*/1e-8, /*dual_reg=*/0.0);
        run.note("deficient case, fixture " + fx.name + ": " + fx.provenance);
        std::unique_ptr<SeamUnderTest> s = run.another_seam();
        s->analyze(fx.K);
        const hl::FactorizeOutcome out = s->factorize(fx.K);
        run.record(Observation::state(
            "P3", run.label(), "deficient_factorize_status",
            out.status == hl::FactorizeOutcome::Status::kOk ? "kOk" : "kBackendError"));
        run.record_inertia(s->inertia(), "deficient_");
    }

    {
        // The curvature term is eight orders above the scale at which the
        // original pin calls it large, which is the regime the misread lived
        // in.
        const Fixture fx = active_bound_curvature_kkt(/*sigma=*/1e8);
        run.note("control case, fixture " + fx.name + ": " + fx.provenance);
        std::unique_ptr<SeamUnderTest> s = run.another_seam();
        s->analyze(fx.K);
        ASSERT_EQ(s->factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);
        const hl::InertiaEvidence e = s->inertia();
        if (e.state == hl::InertiaEvidence::State::kObserved) {
            EXPECT_EQ(e.n_zero, 0) << "active-bound curvature must never read as a zero class";
            // A REASONED EXTENSION BEYOND THE AUTHORITY, flagged as such. The
            // pin this control comes from says only that the matrix must not
            // read as singular. The perturbation count is the other half of
            // the same misread on this backend -- a perturbed pivot here is
            // the signal a driver would act on, and reading a zero class while
            // ignoring it is exactly the mistake the pin exists to prevent --
            // so the trace asserts it too. The cost if a future backend
            // perturbs benignly here: this trace FAILS rather than records,
            // and the right response is to demote this line to an
            // observation, not to widen it.
            if (e.perturbed_pivots.has_value()) {
                EXPECT_EQ(*e.perturbed_pivots, 0)
                    << "active-bound curvature must never provoke a perturbed pivot either -- "
                       "that is the other half of the same misread";
            }
        }
        run.record_inertia(e, "control_");
    }
}

// --- P4 -- perturbation evidence absent on a backend that has no counter -----
//
// The first fabrication case. On a backend with a perturbed-pivot counter the
// field is PRESENT; on one without, it is ABSENT -- not zero. A zero here
// would mean "the backend counted, and the answer was none", which is a
// different and false claim.
//
// The old seam this trace runs against on Apple returns a hardcoded zero from
// its perturbed-pivot accessor, so THIS TRACE FAILS BY DESIGN on that arm.
// That failure is the docket entry.

TEST_P(InteriorPointTrace, P4_PerturbationEvidencePresenceIsBackendHonest) {
    TraceRun run("P4", GetParam());
    SeamUnderTest &seam = run.seam();
    RIG_REQUIRE(run, seam.capabilities().reports_inertia, "an inertia readback");

    const Fixture fx = barrier_chain_kkt(chain_nodes(), /*iterate=*/1);
    run.note("fixture " + fx.name + ": " + fx.provenance);

    seam.analyze(fx.K);
    ASSERT_EQ(seam.factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);
    const hl::InertiaEvidence e = seam.inertia();

#if defined(__APPLE__)
    // The native arm compiles and passes in every macOS CI run. The psiopt-old
    // Apple arm remains unobserved; this is the assertion its hardcoded zero fails.
    EXPECT_FALSE(e.perturbed_pivots.has_value())
        << "this backend has no perturbed-pivot counter, so the evidence must be ABSENT; a zero "
           "would be a fabricated reading";
#else
    EXPECT_TRUE(e.perturbed_pivots.has_value())
        << "this backend counts perturbed pivots, so the evidence must be present";
#endif

    run.record_inertia(e);
}

// --- P5 -- an inertia query with nothing to report says so -------------------
//
// The second fabrication case, in the slice that is reachable without a
// backend fault: an inertia asked for before any numeric factorization. The
// answer must be an explicit "cannot report", with the counts left INVALID --
// never zero-filled, because a zero-filled triple is indistinguishable from a
// real reading and is precisely what the old seam produces when its query
// fails.
//
// The other half of this trace's subject -- a query that RUNS and FAILS -- has
// no input that provokes it on any backend, and is covered by the library's own
// fault-injection suite instead (that is what the fault-injection seam exists
// for). The non-LDLT factorization kinds, the authority's other named path, are
// unreachable here for a different reason: constructing one throws, so no
// object of that kind exists to query.
//
// THIS TRACE FAILS BY DESIGN ON ANY SEAM THAT ZERO-FILLS, which is what makes
// it a docket source rather than a formality. Known so far: the SQP old seam
// on every platform (its parameter array is zero-filled at construction and
// its accessors are plain reads of it, so it answers a real-looking triple
// before anything is factorized), and the interior-point old seam on Apple
// (same shape, different mechanism). The interior-point seam on MKL passes,
// and for an honest reason -- its counts are genuinely indeterminate there, so
// its adapter reports the absence of a defined state rather than inventing
// one. A failure here is the finding; it is not a broken trace.

TEST_P(InteriorPointTrace, P5_InertiaBeforeFactorizationIsAnExplicitState) {
    TraceRun run("P5", GetParam());
    SeamUnderTest &seam = run.seam();
    RIG_REQUIRE(run, seam.capabilities().reports_inertia, "an inertia readback");

    const hl::InertiaEvidence e = seam.inertia();

    EXPECT_NE(e.state, hl::InertiaEvidence::State::kObserved)
        << "nothing has been factorized, so there is nothing to have observed";
    EXPECT_FALSE(e.n_pos == 0 && e.n_neg == 0 && e.n_zero == 0)
        << "counts with nothing behind them must stay invalid, never be zero-filled -- a "
           "zero-filled triple reads exactly like a real one";

    run.record_inertia(e, "before_factorize_");
    run.note("the query-ran-and-failed half of this case has no provoking input on any backend "
             "and is covered by the library's fault-injection suite; the non-LDLT factorization "
             "kinds are unreachable because constructing one throws.");
}

// --- P6 -- refinement-step evidence ------------------------------------------
//
// One factorize and solve with iterative refinement turned on, on a poorly
// scaled matrix -- the case where refinement has something to do. The
// refinement cap is an OVERRIDE of this arm's own configuration, which the
// report records, because the point of the trace is the knob.

TEST_P(InteriorPointTrace, P6_RefinementStepEvidence) {
    TraceRun run("P6", GetParam());
    RIG_REQUIRE(run, run.seam().capabilities().reports_refinement_iters,
                "a refinement-step readback");

    const Fixture fx = brutally_scaled_kkt();
    run.note("fixture " + fx.name + ": " + fx.provenance);

    SeamOptions o = GetParam().options;
    o.max_refinement_iters = 2;
    std::unique_ptr<SeamUnderTest> seam =
        run.seam_with(o, "refinement cap raised to 2, which is this trace's subject");

    seam->analyze(fx.K);
    ASSERT_EQ(seam->factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);
    Vec x(fx.K.rows());
    const hl::SolveInfo info = seam->solve(fx.rhs, x);

    // RIG_REQUIRE above already skips any arm whose capabilities() reports
    // reports_refinement_iters == false -- the psiopt old seam correctly
    // does this on Apple (seam_psiopt.cpp's kReportsRefinementIters is
    // false there), so the only arm that reaches this point on Apple is
    // native, whose capability is unconditionally true on every backend (the
    // std::optional surface exists) even though Accelerate never populates
    // it. Per the frozen contract (SolveInfo::refinement_iters's doc
    // comment in include/hven/linear/symmetric_factor.h), refinement_iters
    // is present with Pardiso semantics on MKL and legitimately absent --
    // never zero-filled -- on Accelerate, which has no such counter.
    // Asserting ABSENCE below is itself the fabrication guard for that
    // backend.
#if defined(__APPLE__)
    EXPECT_FALSE(info.refinement_iters.has_value())
        << "Accelerate has no refinement-step counter; a present value here would be a "
           "fabrication, not evidence";
#else
    EXPECT_TRUE(info.refinement_iters.has_value())
        << "this seam reports refinement steps, so the field must be present when refinement is "
           "configured on";
    if (info.refinement_iters.has_value()) {
        EXPECT_GE(*info.refinement_iters, 0);
    }
#endif

    run.record_solve_info(info);
    run.record_inertia(seam->inertia());
    run.record_counters(seam->counters());
    run.record(Observation::real("P6", run.label(), "relative_residual",
                                 relative_residual(fx.K, x, fx.rhs), 1e-6));
}

} // namespace
} // namespace hven::rig
