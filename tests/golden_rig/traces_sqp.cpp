// The SQP half of the trace set: T1-T8 plus T2b and T4b, each pinning one
// clause of the interface contract the SQP engine's migration has to
// reproduce.
//
// Every trace runs on every arm this build has. What a trace ASSERTS here is
// only what its naming authority states as a contract property -- "the
// symbolic is analyzed once across this sequence", "the predicate is false
// under perturbation", "the handle's epoch stays at its emission value". The
// numbers (solutions, inertia counts, perturbed-pivot counts, refinement
// steps) are RECORDED against the expected tables, which at this point carry
// header rows and unfilled slots; deriving them is a separate step that reads
// this suite's own report.
//
// An arm that cannot do what a trace needs SKIPS, naming the missing
// capability. That is how the old seams' real gaps -- no phase-split solve on
// one, no shared handle or epoch on either -- reach the record instead of
// being papered over with an emulation.

#include <cctype>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "hven/linear/dense_symmetric_factor.h"
#include "recipes.h"
#include "trace_support.h"

namespace hven::rig {
namespace {

namespace hl = hven::linear;

std::string sqp_arm_suffix(const testing::TestParamInfo<ArmSpec> &info) {
    std::string s = info.param.name;
    for (char &c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0) {
            c = '_';
        }
    }
    return s;
}

// Full precision on the way out: a record-only value is still the raw material
// for a docket entry, and rounding it here would lose the only digits that
// distinguish "the two settings agreed" from "they agreed to eleven places".
std::string format_deviation(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

class SqpTrace : public testing::TestWithParam<ArmSpec> {};

INSTANTIATE_TEST_SUITE_P(Arms, SqpTrace, testing::ValuesIn(arms()), sqp_arm_suffix);

// --- T1 -- phase-split lifecycle -------------------------------------------
//
// analyze once, factorize once, solve repeatedly, on a collocation-chain K0
// with an empty path-constraint window. The clause: the symbolic analysis
// happens exactly once across the whole sequence, and the counters make that
// checkable rather than merely claimed.

TEST_P(SqpTrace, T1_PhaseSplitLifecycle) {
    TraceRun run("T1", GetParam());
    const Fixture fx = collocation_chain_kkt(chain_nodes(), /*value_seed=*/11u);
    run.note("fixture " + fx.name + ": " + fx.provenance);

    SeamUnderTest &seam = run.seam();
    seam.analyze(fx.K);
    const hl::FactorizeOutcome out = seam.factorize(fx.K);
    ASSERT_EQ(out.status, hl::FactorizeOutcome::Status::kOk);

    constexpr Index kSolves = 3;
    Vec x(fx.K.rows());
    hl::SolveInfo info;
    for (Index k = 0; k < kSolves; ++k) {
        info = seam.solve(fx.rhs, x);
    }

    const Counters c = seam.counters();
    // The contract clause this trace exists for.
    EXPECT_EQ(c.analyze_count, 1) << "the symbolic must be analyzed exactly once";
    EXPECT_EQ(c.factorize_count, 1);
    EXPECT_EQ(c.solve_count, kSolves);

    run.record_counters(c);
    run.record_inertia(seam.inertia());
    run.record_solve_info(info);
    run.record_vector_head("x", x, 4, TraceRun::kDefaultTolerance);
    run.record(Observation::real("T1", run.label(), "relative_residual",
                                 relative_residual(fx.K, x, fx.rhs), 1e-10));
}

// --- T2 -- persistent-oracle interleaving ----------------------------------
//
// Two factorizations alive at once -- the sparse oracle and a small dense
// border factor -- with their solves interleaved. The clause: the sparse
// factorization stays valid and NUMERICALLY UNDISTURBED across the dense
// factor's whole lifecycle.
//
// The dense side is hven's own dense factor on every arm. It is not the seam
// under test: what is under test is whether the sparse seam tolerates another
// factorization living beside it, so using one dense implementation across all
// arms is what keeps the comparison about the sparse side.

TEST_P(SqpTrace, T2_PersistentOracleInterleaving) {
    TraceRun run("T2", GetParam());
    const Fixture fx = hs76_kkt({0});
    run.note("fixture " + fx.name + ": " + fx.provenance);

    SeamUnderTest &seam = run.seam();
    seam.analyze(fx.K);
    ASSERT_EQ(seam.factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);

    Vec x_before(fx.K.rows());
    seam.solve(fx.rhs, x_before);

    Vec x_after(fx.K.rows());
    {
        hl::DenseSymmetricFactor dense;
        const Mat B = dense_border_block(5);
        dense.factorize(B);
        ASSERT_TRUE(dense.factorized());

        Mat rhs_b = Mat::Ones(5, 1);
        Mat y(5, 1);
        dense.solve(rhs_b, y);

        // Interleaved: sparse, dense, sparse, all with both factorizations
        // live.
        seam.solve(fx.rhs, x_after);
        dense.solve(rhs_b, y);
        seam.solve(fx.rhs, x_after);
    }

    Vec x_outlived(fx.K.rows());
    seam.solve(fx.rhs, x_outlived);

    // The clause, as a 0-ULP comparison between two observations of ONE
    // pinned-thread run -- the only place the comparison policy permits
    // bitwise equality.
    EXPECT_TRUE(bitwise_equal(x_before, x_after))
        << "the sparse factorization's solve changed while a dense factor lived beside it";
    EXPECT_TRUE(bitwise_equal(x_before, x_outlived))
        << "the sparse factorization's solve changed after the dense factor was destroyed";

    const Counters c = seam.counters();
    EXPECT_EQ(c.analyze_count, 1);
    EXPECT_EQ(c.factorize_count, 1);
    EXPECT_EQ(c.solve_count, 4) << "only the sparse seam's own solves are counted";

    run.record_counters(c);
    run.record_vector_head("x", x_before, 4, TraceRun::kDefaultTolerance);
    run.record(Observation::real("T2", run.label(), "relative_residual",
                                 relative_residual(fx.K, x_before, fx.rhs), 1e-10));
}

// --- T2b -- the composability predicate -------------------------------------
//
// A factorization whose pivots were perturbed must report that its phase-split
// solves cannot be trusted to compose, and a caller that honours the predicate
// must be visible in the counters as having fallen back to a full solve. The
// Apple arm asserts the conservative rung: with no perturbation evidence at
// all, the predicate is false because composability is unverifiable, never a
// fabricated true.

TEST_P(SqpTrace, T2b_PartialSolvePredicateUnderPerturbation) {
    TraceRun run("T2b", GetParam());
    SeamUnderTest &seam = run.seam();
    RIG_REQUIRE(run, seam.capabilities().partial_solve_predicate, "a partial-solve predicate");

    const Fixture fx = perturbing_singular_kkt();
    run.note("fixture " + fx.name + ": " + fx.provenance);

    seam.analyze(fx.K);
    const hl::FactorizeOutcome out = seam.factorize(fx.K);
    ASSERT_EQ(out.status, hl::FactorizeOutcome::Status::kOk);

    const hl::InertiaEvidence e = seam.inertia();
    const bool gate = seam.supports_partial_solve();

    // Both implications the predicate's design law states, asserted as
    // implications rather than as a fixed expected value -- which arm and
    // which backend decides which one applies.
    if (e.perturbed_pivots.has_value() && *e.perturbed_pivots > 0) {
        EXPECT_FALSE(gate) << "pivots were perturbed, so the composition is not trustworthy";
    }
    if (!e.perturbed_pivots.has_value()) {
        EXPECT_FALSE(gate) << "with no perturbation evidence, composability is unverifiable and "
                              "the predicate must be a conservative false";
    }

    // The fallback a border stack takes when the gate is closed, made
    // counter-visible: one full solve, no partial solves.
    Vec x(fx.K.rows());
    if (gate) {
        Vec y(fx.K.rows());
        Vec z(fx.K.rows());
        seam.solve_partial(hl::SymmetricFactor::SolvePhase::kForward, fx.rhs, y);
        seam.solve_partial(hl::SymmetricFactor::SolvePhase::kDiagonal, y, z);
        seam.solve_partial(hl::SymmetricFactor::SolvePhase::kBackward, z, x);
    } else {
        seam.solve(fx.rhs, x);
    }

    const Counters c = seam.counters();
    if (!gate) {
        EXPECT_EQ(c.partial_solve_count, 0) << "the gate was closed, so no partial solve may run";
        EXPECT_EQ(c.solve_count, 1) << "the fallback is exactly one full solve";
    }

    run.record(Observation::boolean("T2b", run.label(), "supports_partial_solve", gate));
    run.record_counters(c);
    run.record_inertia(e);
}

// --- T3 -- value-only refactorization ---------------------------------------
//
// The same sparsity pattern with different values, refactorized numerically
// with NO re-analysis. This is the mechanism a value-latch regression turns
// on, and the counter is the only thing that distinguishes "reused the
// symbolic" from "quietly re-analyzed and got the same answer".

TEST_P(SqpTrace, T3_ValueOnlyRefactorization) {
    TraceRun run("T3", GetParam());
    const Fixture a = collocation_chain_kkt(chain_nodes(), /*value_seed=*/11u);
    const Fixture b = collocation_chain_kkt(chain_nodes(), /*value_seed=*/29u);
    run.note("fixture pair " + a.name + " (two value seeds, one pattern): " + a.provenance);
    ASSERT_EQ(a.K.nonZeros(), b.K.nonZeros()) << "the recipe must vary values, not the pattern";

    SeamUnderTest &seam = run.seam();
    seam.analyze(a.K);
    ASSERT_EQ(seam.factorize(a.K).status, hl::FactorizeOutcome::Status::kOk);
    Vec xa(a.K.rows());
    seam.solve(a.rhs, xa);

    ASSERT_EQ(seam.factorize(b.K).status, hl::FactorizeOutcome::Status::kOk);
    Vec xb(b.K.rows());
    seam.solve(a.rhs, xb);

    const Counters c = seam.counters();
    EXPECT_EQ(c.analyze_count, 1) << "a value change must not cost a re-analysis";
    EXPECT_EQ(c.factorize_count, 2);
    EXPECT_FALSE(bitwise_equal(xa, xb))
        << "the second factorization used different values and must give a different solve";

    run.record_counters(c);
    run.record_vector_head("x_first", xa, 4, TraceRun::kDefaultTolerance);
    run.record_vector_head("x_second", xb, 4, TraceRun::kDefaultTolerance);
    run.record(Observation::real("T3", run.label(), "relative_residual_second",
                                 relative_residual(b.K, xb, a.rhs), 1e-10));
}

// --- T4 -- handle store and share -------------------------------------------
//
// Factorize, emit a handle, destroy the engine that made it, solve through the
// handle. The clause: the handle co-owns the backend session, so it outlives
// its emitter, and its solve is the SAME factorization -- bitwise, within this
// one pinned-thread run.

TEST_P(SqpTrace, T4_HandleOutlivesItsEmitter) {
    TraceRun run("T4", GetParam());
    RIG_REQUIRE(run, run.seam().capabilities().share_handle, "a shared factorization handle");

    const Fixture fx = collocation_chain_kkt(chain_nodes(), /*value_seed=*/11u);
    run.note("fixture " + fx.name + ": " + fx.provenance);

    std::shared_ptr<const SeamHandle> handle;
    Vec x_engine(fx.K.rows());
    Counters engine_counters;
    {
        std::unique_ptr<SeamUnderTest> engine = run.another_seam();
        engine->analyze(fx.K);
        ASSERT_EQ(engine->factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);
        engine->solve(fx.rhs, x_engine);
        handle = engine->share();
        engine_counters = engine->counters();
    } // the emitting engine is destroyed here

    ASSERT_NE(handle, nullptr);
    Vec x_handle(fx.K.rows());
    handle->solve(fx.rhs, x_handle);

    EXPECT_TRUE(bitwise_equal(x_engine, x_handle))
        << "the handle must be the same factorization, not a copy or a re-solve";

    run.record_counters(engine_counters);
    run.record_vector_head("x", x_handle, 4, TraceRun::kDefaultTolerance);
    run.record(Observation::counter("T4", run.label(), "handle_epoch",
                                    static_cast<Index>(handle->epoch())));
    run.record_inertia(handle->inertia());
}

// --- T4b -- the staleness contract ------------------------------------------
//
// Emit a handle, let the ORIGINATOR refactorize with new values, then adopt
// the handle. Numeric reuse must be refused on the epoch mismatch while
// symbolic reuse stays legal. The epoch discriminator is the point: counter
// deltas alone cannot distinguish "refused the stale numerics and factorized
// fresh" from "quietly re-shared the mutated session", because both produce
// the same counts.

TEST_P(SqpTrace, T4b_AdoptRefusesStaleNumerics) {
    TraceRun run("T4b", GetParam());
    const Capabilities caps = run.seam().capabilities();
    RIG_REQUIRE(run, caps.share_handle && caps.epoch && caps.adopt,
                "a shared handle with an epoch and an adopt path");

    const Fixture a = collocation_chain_kkt(chain_nodes(), /*value_seed=*/11u);
    const Fixture b = collocation_chain_kkt(chain_nodes(), /*value_seed=*/29u);
    run.note("fixture pair " + a.name + " (two value seeds, one pattern): " + a.provenance);

    std::unique_ptr<SeamUnderTest> originator = run.another_seam();
    originator->analyze(a.K);
    ASSERT_EQ(originator->factorize(a.K).status, hl::FactorizeOutcome::Status::kOk);

    std::shared_ptr<const SeamHandle> handle = originator->share();
    const std::uint64_t emission_epoch = handle->epoch();

    // The originator moves on. The handle now names numerics nobody holds.
    ASSERT_EQ(originator->factorize(b.K).status, hl::FactorizeOutcome::Status::kOk);
    const std::uint64_t live_epoch = originator->epoch();

    EXPECT_EQ(handle->epoch(), emission_epoch) << "a handle's epoch is fixed at emission";
    EXPECT_GT(live_epoch, emission_epoch) << "a successful refactorization must advance the epoch";

    std::unique_ptr<SeamUnderTest> adopter = originator->adopt(handle);

    // Numeric reuse refused ...
    Vec x(a.K.rows());
    EXPECT_THROW(adopter->solve(a.rhs, x), std::runtime_error)
        << "solving through an engine that adopted stale numerics must be refused, not served";

    // ... while symbolic reuse stays legal: the adopter's first factorization
    // costs no analysis.
    ASSERT_EQ(adopter->factorize(b.K).status, hl::FactorizeOutcome::Status::kOk);
    adopter->solve(a.rhs, x);

    const Counters ac = adopter->counters();
    EXPECT_EQ(ac.analyze_count, 0) << "the adopted symbolic must be reused, not re-derived";
    EXPECT_EQ(ac.factorize_count, 1);
    EXPECT_EQ(ac.solve_count, 1);

    run.record_counters(ac);
    run.record(Observation::counter("T4b", run.label(), "handle_epoch_at_emission",
                                    static_cast<Index>(emission_epoch)));
    run.record(Observation::counter("T4b", run.label(), "originator_epoch_after_refactorize",
                                    static_cast<Index>(live_epoch)));
    run.record_vector_head("x_after_adopt_refactorize", x, 4, TraceRun::kDefaultTolerance);
}

// --- T5 -- inertia evidence --------------------------------------------------
//
// Three factorizations with different inertia characters: a saddle (negative
// curvature present), a semidefinite boundary, and a clean positive-definite
// face. Evidence is recorded for all three; only the properties the naming
// authority states outright are asserted. Presence of the perturbation field
// is recorded SEPARATELY from its value, because an absent field and a zero
// are different readings and a table that stored only the number could not
// tell them apart.

TEST_P(SqpTrace, T5_InertiaEvidence) {
    TraceRun run("T5", GetParam());
    SeamUnderTest &seam = run.seam();
    RIG_REQUIRE(run, seam.capabilities().reports_inertia, "an inertia readback");

    {
        const Fixture fx = saddle_kkt();
        run.note("case a, fixture " + fx.name + ": " + fx.provenance);
        std::unique_ptr<SeamUnderTest> s = run.another_seam();
        s->analyze(fx.K);
        ASSERT_EQ(s->factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);
        const hl::InertiaEvidence e = s->inertia();
        if (e.state == hl::InertiaEvidence::State::kObserved) {
            EXPECT_GE(e.n_neg, 1) << "the saddle's negative curvature must be visible";
        }
        run.record_inertia(e, "a_");
    }

    {
        const Fixture fx = semidefinite_boundary_kkt();
        run.note("case b, fixture " + fx.name + ": " + fx.provenance);
        std::unique_ptr<SeamUnderTest> s = run.another_seam();
        s->analyze(fx.K);
        const hl::FactorizeOutcome out = s->factorize(fx.K);
        // Deliberately unasserted: whether a backend reports a zero class here
        // or perturbs its way past it is exactly what the derivation records.
        run.record(Observation::state(
            "T5", run.label(), "b_factorize_status",
            out.status == hl::FactorizeOutcome::Status::kOk ? "kOk" : "kBackendError"));
        run.record_inertia(s->inertia(), "b_");
    }

    {
        constexpr Index kFree = 6;
        constexpr Index kFace = 2;
        const Fixture fx = pd_on_face_kkt(kFree, kFace);
        run.note("case c, fixture " + fx.name + ": " + fx.provenance);
        std::unique_ptr<SeamUnderTest> s = run.another_seam();
        s->analyze(fx.K);
        ASSERT_EQ(s->factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);
        const hl::InertiaEvidence e = s->inertia();
        if (e.state == hl::InertiaEvidence::State::kObserved) {
            EXPECT_EQ(e.n_pos, kFree) << "a positive-definite face has exactly one positive "
                                         "eigenvalue per free variable";
            EXPECT_EQ(e.n_neg, kFace) << "and exactly one negative eigenvalue per face row";
        }
        run.record_inertia(e, "c_");
    }
}

// --- T6 -- refusal and failure as explicit state -----------------------------
//
// A rank-deficient face. What is asserted is the HONESTY RULE rather than a
// particular outcome: a factorization reports an explicit status, and an
// inertia query that failed leaves its counts invalid rather than zero-filled
// -- a zero-filled count is indistinguishable from a real reading, which is
// the defect class this trace exists for.

TEST_P(SqpTrace, T6_RankDeficientFaceReportsAState) {
    TraceRun run("T6", GetParam());
    const Fixture fx = duplicated_equality_kkt(/*primal_reg=*/1e-8, /*dual_reg=*/0.0);
    run.note("fixture " + fx.name + ": " + fx.provenance);

    SeamUnderTest &seam = run.seam();
    seam.analyze(fx.K);
    const hl::FactorizeOutcome out = seam.factorize(fx.K);
    const hl::InertiaEvidence e = seam.inertia();

    // The honesty rule, in both halves.
    if (e.state == hl::InertiaEvidence::State::kQueryFailed) {
        EXPECT_LT(e.n_pos, 0) << "a failed query must leave its counts invalid, never zero-filled";
        EXPECT_LT(e.n_neg, 0);
        EXPECT_LT(e.n_zero, 0);
    }
    if (e.state == hl::InertiaEvidence::State::kObserved) {
        EXPECT_GE(e.n_pos, 0);
        EXPECT_GE(e.n_neg, 0);
    }

    run.record(Observation::state(
        "T6", run.label(), "factorize_status",
        out.status == hl::FactorizeOutcome::Status::kOk ? "kOk" : "kBackendError"));
    run.record(Observation::counter("T6", run.label(), "backend_code", out.backend_code));
    run.record_inertia(e);
    run.record_counters(seam.counters());
}

// --- T7 -- the backend-parameter surface floor -------------------------------
//
// One factorize and one solve on a brutally scaled matrix, reading the two
// backend-derived fields both engines consume. GOVERNANCE NOTE, attached here
// because this is the trace that would catch it: any change to the Pardiso
// parameter surface in a migration diff requires explicit human review, and
// this trace's recorded presence-and-value rows are what make such a change
// visible as a diff in an artifact rather than only in source.

TEST_P(SqpTrace, T7_BackendParameterSurfaceFloor) {
    TraceRun run("T7", GetParam());
    const Fixture fx = brutally_scaled_kkt();
    run.note("fixture " + fx.name + ": " + fx.provenance);
    run.note("GOVERNANCE: a backend-parameter change in a migration diff triggers human review; "
             "this trace's rows are the artifact such a change shows up in.");

    SeamUnderTest &seam = run.seam();
    const Capabilities caps = seam.capabilities();
    seam.analyze(fx.K);
    const hl::FactorizeOutcome out = seam.factorize(fx.K);
    ASSERT_EQ(out.status, hl::FactorizeOutcome::Status::kOk);

    Vec x(fx.K.rows());
    const hl::SolveInfo info = seam.solve(fx.rhs, x);
    const hl::InertiaEvidence e = seam.inertia();

    // A seam that says it surfaces a field must actually surface it -- but
    // "surfaces" and "populates" are different claims on Accelerate.
    // seam_native.cpp's capabilities() sets both flags unconditionally on
    // every backend on purpose: the capability says the std::optional
    // surface exists; whether it carries a value is the backend's own
    // answer. Per the frozen contract (include/hven/linear/symmetric_factor.h's
    // InertiaEvidence::perturbed_pivots and SolveInfo::refinement_iters doc
    // comments), both are present with Pardiso semantics on MKL and
    // legitimately absent -- never zero-filled -- on Accelerate, which has
    // neither counter. Asserting ABSENCE on Accelerate below is itself the
    // fabrication guard: it is what would fail loudly if a future change
    // ever zero-filled either field there instead of reporting it honestly
    // absent. (No arm that reaches this trace on Apple reports either
    // capability as false -- the SQP old seam has no Apple build at all, and
    // native's capability is unconditionally true -- so this is a pure
    // backend split, not a seam one.)
    if (caps.reports_perturbed_pivots) {
#if defined(__APPLE__)
        EXPECT_FALSE(e.perturbed_pivots.has_value())
            << "Accelerate has no perturbed-pivot counter; a present value here would be a "
               "fabrication, not evidence";
#else
        EXPECT_TRUE(e.perturbed_pivots.has_value())
            << "this seam reports a perturbed-pivot count, so the field must be present";
#endif
    }
    if (caps.reports_refinement_iters) {
#if defined(__APPLE__)
        EXPECT_FALSE(info.refinement_iters.has_value())
            << "Accelerate has no refinement-step counter; a present value here would be a "
               "fabrication, not evidence";
#else
        EXPECT_TRUE(info.refinement_iters.has_value())
            << "this seam reports a refinement-step count, so the field must be present";
#endif
    }

    run.record_inertia(e);
    run.record_solve_info(info);
    run.record_counters(seam.counters());
    run.record_vector_head("x", x, 4, 1e-8);
}

// --- T8 -- thread-count control ----------------------------------------------
//
// The same solve at a pinned single thread and at the backend's own default.
//
// WHAT IS ASSERTED HERE AND WHAT IS NOT. The counters must be identical --
// integer call counts reproduce under any thread setting, and that is an
// unconditional claim. The cross-thread VALUE agreement is RECORDED, not
// asserted, and deliberately so: the comparison policy permits a
// multithreaded run only as unasserted smoke, and this trace's amendment says
// in as many words that the stronger claim is retained only if a derivation
// run demonstrates it for this trace's own matrix and that otherwise the
// observed deviation is recorded as documentation. No derivation has run yet,
// so asserting a tolerance here would be inventing the very expectation this
// task is not allowed to invent. What IS asserted about the values is that
// they are finite -- a crash-or-garbage guard, which is what "smoke" means.
//
// Measured while writing this trace, and worth knowing before the derivation
// reads the row: on this collocation class the deviation is not always zero.
// It is the number the amendment asks to be recorded.
//
// THE SMOKE LEG ASKS FOR A THREAD COUNT EXPLICITLY. Asking for "the backend's
// default" would have made this trace measure nothing under the very
// invocation the derivation uses, which exports a single-thread setting for
// the whole process -- both legs would resolve to one thread and the deviation
// would be identically zero for a reason that has nothing to do with
// threading. The count is recorded beside the deviation so the row can be
// read.

TEST_P(SqpTrace, T8_ThreadCountControl) {
    TraceRun run("T8", GetParam());
    const Fixture fx = collocation_chain_kkt(chain_nodes(), /*value_seed=*/11u);
    run.note("fixture " + fx.name + ": " + fx.provenance);

    // The smoke leg runs FIRST and the asserted leg second, deliberately: every
    // row this trace records belongs to the pinned leg, and the run's recorded
    // thread pin is the one of the last engine it built. Running them the other
    // way round would stamp the asserted rows with the smoke leg's setting,
    // which is precisely the provenance the comparison policy refuses.
    //
    // The smoke leg requests an EXPLICIT count above one rather than the
    // backend's own default -- see smoke_thread_count() for why the default
    // makes this whole trace vacuous under the derivation invocation.
    const int smoke_threads = smoke_thread_count();
    SeamOptions smoke_options = GetParam().options;
    smoke_options.num_threads = smoke_threads;

    // The smoke engine outlives its block: its thread pin is what stamps the
    // cross-thread rows below, and asking a destroyed engine for it would put
    // the rig in the business of remembering configurations rather than
    // reading them off the thing that ran.
    std::unique_ptr<SeamUnderTest> smoke = run.seam_with(
        smoke_options, "the unasserted-smoke leg, " + std::to_string(smoke_threads) + " threads");
    Vec x_default(fx.K.rows());
    smoke->analyze(fx.K);
    ASSERT_EQ(smoke->factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);
    smoke->solve(fx.rhs, x_default);
    const Counters default_counters = smoke->counters();

    Counters pinned_counters;
    Vec x_pinned(fx.K.rows());
    {
        SeamOptions o = GetParam().options;
        o.num_threads = 1;
        std::unique_ptr<SeamUnderTest> s = run.seam_with(o, "the pinned leg, threads=1");
        s->analyze(fx.K);
        ASSERT_EQ(s->factorize(fx.K).status, hl::FactorizeOutcome::Status::kOk);
        s->solve(fx.rhs, x_pinned);
        pinned_counters = s->counters();
    }

    EXPECT_EQ(pinned_counters.analyze_count, default_counters.analyze_count);
    EXPECT_EQ(pinned_counters.factorize_count, default_counters.factorize_count);
    EXPECT_EQ(pinned_counters.solve_count, default_counters.solve_count);

    const double diff = (x_pinned - x_default).lpNorm<Eigen::Infinity>();
    const double scale = std::max(1.0, x_pinned.lpNorm<Eigen::Infinity>());
    EXPECT_TRUE(std::isfinite(diff)) << "the unasserted-smoke leg produced a non-finite solve";
    EXPECT_TRUE(x_pinned.allFinite()) << "the pinned leg produced a non-finite solve";

    run.record_counters(pinned_counters);
    run.record_vector_head("x_pinned", x_pinned, 4, TraceRun::kDefaultTolerance);
    // THE THREE CROSS-THREAD QUANTITIES ARE RECORD-ONLY, and the kind is doing
    // real work rather than labelling. Each describes a comparison BETWEEN two
    // thread configurations, so no single thread pin describes it: stamped
    // with the asserted leg's pin they would read as ordinary pinned rows, and
    // a derivation following the copy-the-report-rows workflow would commit a
    // run-to-run-nondeterministic number at a tight tolerance with the
    // reader's pin-refusal unable to catch it, because the row would look
    // perfectly pinned. The record-only kind makes that structurally
    // impossible -- the reader REFUSES it in a committed table -- and the rows
    // additionally carry the SMOKE leg's own mechanism and count, so the
    // artifact says what was actually measured.
    const std::string measured_between =
        "measured between two thread settings (" + std::to_string(smoke_threads) +
        " and 1), so no single thread pin describes it; documentation, never an expectation";
    run.record_only("smoke_leg_thread_count", std::to_string(smoke_threads), measured_between,
                    *smoke);
    run.record_only("cross_thread_relative_deviation", format_deviation(diff / scale),
                    measured_between, *smoke);
    run.record_only("bitwise_identical_across_thread_settings",
                    bitwise_equal(x_pinned, x_default) ? "true" : "false", measured_between,
                    *smoke);
}

} // namespace
} // namespace hven::rig
