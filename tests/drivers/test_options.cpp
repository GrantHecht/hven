// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M6 W5 T8.3: options as values on both engines.
//
// What this file pins:
//   * validate(const IpmOptions &) refuses everything
//     IpmSolver::Settings::validate() refused -- one representative
//     per refusal class, enumerated from the old body.
//   * set_options() is TRANSACTIONAL on the interior-point engine: a rejected
//     replacement leaves the previous value in force and the solver usable.
//   * set_options() during a solve throws std::logic_error, leaves options()
//     unchanged, and clears the guard so a later solve still works.
//   * ipm_preset() returns a FULL validated value; an unknown name is refused.
//   * The ONE rule for the twelve ATTACH-ONLY fields, which are read once when
//     the program is attached: changing one on an attached solver is refused by
//     name rather than silently ignored -- one representative per type class --
//     and the executable release/replace/re-attach sequence that changes one
//     anyway.
//   * Neither solver object is copyable or movable (static_asserts), and both
//     options values are nothrow-move-assignable, which is what lets
//     SqpSolver::set_options() commit the options after the engine swap.
//   * The in-flight guard clears on an UNWIND as well as on a return, on both
//     engines, and an option replacement does not restart the SQP ledger's QP
//     label sequence.
//
// The SQP half of the same surface is pinned below it, and the hot-handle
// fingerprint lives in tests/sqp/test_warm_start.cpp beside the kHot chain it
// belongs to.

#include "hven/detail/globalization/sqp/globalization.h"
#include "hven/detail/model/fixed_variable_treatment.h"
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "hven/core/ledger.h"
#include "hven/detail/drivers/ipm_solver_presets.h"
#include "hven/detail/model/nlp_adapter.h"
#include "hven/detail/model/problem_access.h"
#include "hven/drivers/common_options.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/ipm_solver_types.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/drivers/sqp_solver_types.h"
#include "hven/qp/qp_types.h"

#include "sqp/support/hs_problems.h"
#include "support/hs071_problem.h"

namespace {

using hven::solvers::AcceptanceKind;
using hven::solvers::BarrierGovernorKind;
using hven::solvers::CommonOptions;
using hven::solvers::FixedVariableTreatment;
using hven::solvers::IpmOptions;
using hven::solvers::IpmSolver;

constexpr double kInf = std::numeric_limits<double>::infinity();

// The canonical HS071 lives in support/hs071_problem.h, shared with this
// directory's other live-solve suite.
using hven_drivers_tests::hs071_start;
using hven_drivers_tests::Hs071Problem;

// Silent, and otherwise default.
IpmOptions quiet() {
    IpmOptions o;
    o.common.verbosity = hven::solvers::Verbosity::kSilent;
    return o;
}

// A value validate() rejects, whichever field the caller is not looking at.
void expect_refused(const IpmOptions &o, const char *field) {
    try {
        hven::solvers::validate(o);
        FAIL() << "validate() accepted an options value it must refuse (" << field << ")";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find(field), std::string::npos)
            << "the refusal names the wrong field: " << e.what();
    }
}

} // namespace

// ---------------------------------------------------------------------------
// THE SOLVERS ARE MOVABLE HANDLES, NOT COPYABLE
// ---------------------------------------------------------------------------
//
// Both solvers are handles over a heap-allocated private implementation: they
// are movable (the implementation keeps its address, so the cross-references
// inside it survive a move) and not copyable. A moved-from solver may only be
// destroyed or assigned to; tests/drivers/test_solver_move.cpp pins the
// behaviour of a move, not just its traits.
//
// These are static_asserts rather than a TEST body deliberately: what is being
// pinned is a compile-time property, and a regression must fail the BUILD.
static_assert(!std::is_copy_constructible_v<hven::solvers::SqpSolver>,
              "SqpSolver must not be copy-constructible");
static_assert(!std::is_copy_assignable_v<hven::solvers::SqpSolver>,
              "SqpSolver must not be copy-assignable");
static_assert(std::is_nothrow_move_constructible_v<hven::solvers::SqpSolver>,
              "SqpSolver must be nothrow move-constructible");
static_assert(std::is_nothrow_move_assignable_v<hven::solvers::SqpSolver>,
              "SqpSolver must be nothrow move-assignable");

static_assert(!std::is_copy_constructible_v<IpmSolver>, "IpmSolver must not be copy-constructible");
static_assert(!std::is_copy_assignable_v<IpmSolver>, "IpmSolver must not be copy-assignable");
static_assert(std::is_nothrow_move_constructible_v<IpmSolver>,
              "IpmSolver must be nothrow move-constructible");
static_assert(std::is_nothrow_move_assignable_v<IpmSolver>,
              "IpmSolver must be nothrow move-assignable");

// The OPTIONS VALUES, by contrast, must move -- and SqpSolver::set_options()
// relies on more than that: it assigns `opts_ = std::move(o)` AFTER the engine
// swap has already committed, so a throwing move assignment there would leave
// the new engine standing beside the old options. This assert is what makes
// that argument checkable: add a field whose move assignment can throw and the
// build fails here, pointing at src/drivers/sqp_solver.cpp's set_options().
static_assert(std::is_nothrow_move_assignable_v<hven::solvers::SqpOptions>,
              "SqpSolver::set_options() commits the options after the engine swap on the "
              "strength of this");
static_assert(std::is_nothrow_move_assignable_v<IpmOptions>,
              "IpmSolver::set_options() assigns the value last, after every check");

// ---------------------------------------------------------------------------
// validate(const IpmOptions &): one representative per refusal class of the old
// Settings::validate() body, in that body's own order.
// ---------------------------------------------------------------------------

TEST(Options, IpmValidateRefusesWhatSettingsValidateRefused) {
    {
        // pos_int
        IpmOptions o;
        o.common.max_iterations = 0;
        expect_refused(o, "common.max_iterations");
    }
    {
        // the non-negative family (max_refac / max_ls_iters / max_soc /
        // ls_extended_iters / max_feas_rest share one condition and message)
        IpmOptions o;
        o.advanced.restoration_max_entries = -1;
        expect_refused(o, "advanced.restoration_max_entries");
    }
    {
        // combination guard 1: funnel/filter above an adaptive-only governor
        IpmOptions o;
        o.advanced.acceptance_strategy = AcceptanceKind::kFunnel;
        o.advanced.barrier_governor = BarrierGovernorKind::kClassicAdaptive;
        o.advanced.never_monotone = false;
        expect_refused(o, "advanced.acceptance_strategy");
    }
    {
        // combination guard 2: never_monotone against the monitored governor
        IpmOptions o;
        o.advanced.never_monotone = true;
        o.advanced.barrier_governor = BarrierGovernorKind::kMonitored;
        expect_refused(o, "advanced.never_monotone");
    }
    {
        // pos_finite
        IpmOptions o;
        o.common.optimality_tol = std::numeric_limits<double>::quiet_NaN();
        expect_refused(o, "common.optimality_tol");
    }
    {
        // cross-field: convergence tol above its acceptable tol
        IpmOptions o;
        o.termination.acceptable_equality_tol = o.common.feasibility_tol / 10.0;
        expect_refused(o, "termination.acceptable_equality_tol");
    }
    {
        // cross-field: acceptable tol above its divergence tol
        IpmOptions o;
        o.termination.divergence_kkt_tol = o.termination.acceptable_kkt_tol / 10.0;
        expect_refused(o, "termination.acceptable_kkt_tol");
    }
    {
        // barrier ordering: min_mu above max_mu
        IpmOptions o;
        o.min_barrier = 10.0 * o.max_barrier;
        expect_refused(o, "min_barrier");
    }
    {
        // barrier ordering: init_mu outside [min_mu, max_mu]
        IpmOptions o;
        o.initial_barrier = 10.0 * o.max_barrier;
        expect_refused(o, "initial_barrier");
    }
    {
        // in_open_unit
        IpmOptions o;
        o.fraction_to_boundary = 1.0;
        expect_refused(o, "fraction_to_boundary");
    }
    {
        // greater_than
        IpmOptions o;
        o.advanced.backtrack_divisor = 1.0;
        expect_refused(o, "advanced.backtrack_divisor");
    }
    {
        // in_open_interval
        IpmOptions o;
        o.bound_interval_push = 0.5;
        expect_refused(o, "bound_interval_push");
    }
    {
        // in_closed_interval
        IpmOptions o;
        o.bound_relax_factor = 2.0 * hven::solvers::kMaxBoundRelaxFactor;
        expect_refused(o, "bound_relax_factor");
    }
    {
        // check_fixed_variable_treatment (a closed-set enum still reaches the
        // struct as a plain field)
        IpmOptions o;
        o.fixed_variable_treatment = static_cast<FixedVariableTreatment>(42);
        expect_refused(o, "fixed_variable_treatment");
    }
    {
        // the QP integer family: non-negative
        IpmOptions o;
        o.linear.refinement_steps = -1;
        expect_refused(o, "linear.refinement_steps");
    }
    {
        // objective scale
        IpmOptions o;
        o.objective_scale = -1.0;
        expect_refused(o, "objective_scale");
    }
    {
        // the two fields that MOVED into CommonOptions keep the conditions
        // qp_threads_ and print_level_ faced.
        IpmOptions o;
        o.common.threads = 0;
        expect_refused(o, "common.threads");
    }
    {
        IpmOptions o;
        o.common.verbosity = static_cast<hven::solvers::Verbosity>(7);
        expect_refused(o, "common.verbosity");
    }
    // And the shipped default is accepted.
    EXPECT_NO_THROW(hven::solvers::validate(IpmOptions{}));
}

// The defaults each engine's `common` carries. Silent on BOTH engines (owner
// decision 4); the iteration cap and thread count are each engine's own.
TEST(Options, CommonDefaultsAreEachEnginesOwn) {
    using hven::solvers::Verbosity;
    EXPECT_EQ(CommonOptions{}.threads, 0);
    EXPECT_EQ(CommonOptions{}.verbosity, Verbosity::kSilent);
    EXPECT_EQ(CommonOptions{}.max_iterations, 100);
    EXPECT_EQ(CommonOptions{}.optimality_tol, 1e-6);
    EXPECT_EQ(CommonOptions{}.feasibility_tol, 1e-6);
    EXPECT_FALSE(CommonOptions{}.max_factorizations.has_value());
    EXPECT_FALSE(CommonOptions{}.time_limit_seconds.has_value());
    EXPECT_EQ(CommonOptions{}.infinite_bound, 1e20);
    EXPECT_EQ(CommonOptions{}.start_level, hven::solvers::StartLevel::kWarm);

    EXPECT_EQ(IpmOptions{}.common.threads, HVEN_DEFAULT_QP_THREADS);
    EXPECT_EQ(IpmOptions{}.common.verbosity, Verbosity::kSilent);
    EXPECT_EQ(IpmOptions{}.common.max_iterations, 1000);
    EXPECT_EQ(IpmOptions{}.common.start_level, hven::solvers::StartLevel::kWarm);
}

// ---------------------------------------------------------------------------
// set_options() on the interior-point engine
// ---------------------------------------------------------------------------

TEST(Options, IpmSetOptionsIsTransactional) {
    IpmSolver solver(quiet());
    const int good_iters = 123;
    {
        IpmOptions o = solver.options();
        o.common.max_iterations = good_iters;
        solver.set_options(std::move(o));
    }
    ASSERT_EQ(solver.options().common.max_iterations, good_iters);

    IpmOptions bad = solver.options();
    bad.common.max_iterations = 0;
    bad.common.optimality_tol = 1.0e-9;
    EXPECT_THROW(solver.set_options(bad), std::invalid_argument);

    // Neither field moved: the value is taken whole or not at all.
    EXPECT_EQ(solver.options().common.max_iterations, good_iters);
    EXPECT_EQ(solver.options().common.optimality_tol, IpmOptions{}.common.optimality_tol);
}

// EVERY FIELD IS ACCEPTED, and the transcription-time twelve are not silently
// inert (M6 W5 T8.4).
//
// T8.3's rule was a REFUSAL: twelve fields were read exactly once, inside
// set_qp_params(), which ran from set_nlp(), so changing one on an attached
// solver would have been silently inert until the next attach. There is no
// attachment now -- the program is an argument of solve() and set_qp_params()
// runs from the solve that transcribes -- so the refusal has nothing to refuse
// against and is gone. What replaced it is a staleness mark: the change is
// taken, and the analysis laid under the old value stops counting as this
// program's.
TEST(Options, IpmEveryBackendFieldIsAcceptedAndTakesEffectAtTheNextSolve) {
    // M6 W5 T8.9: the program is transcribed once, here, and handed to every
    // solve below as an argument -- the wrapper's lazy transcribe() is gone.
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    ASSERT_NE(program, nullptr);
    IpmSolver solver;
    {
        IpmOptions o = solver.options();
        o.common.verbosity = hven::solvers::Verbosity::kSilent;
        solver.set_options(std::move(o));
    }
    hven::solvers::IpmResult result =
        solver.solve(hven::solvers::detail::problem_ref(*program), hs071_start());
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);

    // Each edit is applied to the value in force, attempted alone, ACCEPTED,
    // and required to invalidate the analysis. qp_matching is left out and
    // pinned on its own below, with the live re-transcription.
    // WRITTEN, CHECKED, AND PUT BACK. The change is asserted accepted and
    // asserted to have invalidated the analysis; then the previous value is
    // restored and a solve re-establishes the precondition for the next field.
    //
    // The restore is not tidiness: a value the backend refuses at transcription
    // would make every later step of this test throw out of set_qp_params()
    // instead of testing what it is for.
    const auto accepts = [&](const char *field, auto edit) {
        ASSERT_TRUE(solver.kkt_pattern_is_analyzed(hven::solvers::detail::problem_ref(*program)))
            << "precondition for " << field;
        const IpmOptions before = solver.options();
        IpmOptions changed = before;
        edit(changed);
        EXPECT_NO_THROW(solver.set_options(changed)) << field;
        EXPECT_FALSE(solver.kkt_pattern_is_analyzed(hven::solvers::detail::problem_ref(*program)))
            << "a transcription-time change must not leave the old analysis standing: " << field;
        solver.set_options(before);
        result = solver.solve(hven::solvers::detail::problem_ref(*program), hs071_start());
        ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    };

    accepts("qp_ref_steps", [](IpmOptions &o) { o.linear.refinement_steps = 1; }); // int
    accepts("qp_ord", [](IpmOptions &o) {                                          // enum
        o.linear.ordering = hven::solvers::KktOrdering::kMinimumDegree;
    });
    accepts("cnr_mode",
            [](IpmOptions &o) { o.linear.conditional_reproducibility = true; }); // the CNR flag
#ifdef USE_ACCELERATE_SPARSE
    // UNOBSERVED on this box: this is declared and read only on an
    // Accelerate build, and nothing here has run on Mac hardware.
    accepts("accel_zero_tolerance",
            [](IpmOptions &o) { o.linear.accelerate_zero_tolerance = 1e-12; });
#endif

    // A value the backend does accept, left in force and carried through a live
    // re-transcription -- so this test shows the whole route, not only the
    // staleness mark. qp_matching is inert on BOTH backends at the value
    // written (MKL reads it as `weighted_matching = qp_matching != 0`,
    // Accelerate does not read it at all), so the re-transcription cannot throw
    // out of set_qp_params() on either.
    {
        const hven::Index analyses_before = result.kkt_analyses_total;
        IpmOptions changed = solver.options();
        changed.linear.matching = changed.linear.matching != 0 ? 0 : 1;
        EXPECT_NO_THROW(solver.set_options(changed));
        EXPECT_FALSE(solver.kkt_pattern_is_analyzed(hven::solvers::detail::problem_ref(*program)));
        result = solver.solve(hven::solvers::detail::problem_ref(*program), hs071_start());
        ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
        EXPECT_EQ(result.kkt_analyses_total, analyses_before + 1);
        EXPECT_EQ(result.kkt_analyses_this_call, 1);
        EXPECT_EQ(solver.options().linear.matching, changed.linear.matching);
        EXPECT_TRUE(solver.kkt_pattern_is_analyzed(hven::solvers::detail::problem_ref(*program)));
    }

    // THE COUNTER-EXAMPLE, unchanged: a PER-SOLVE field is accepted too and
    // does NOT invalidate the analysis -- fixed_variable_treatment excepted,
    // which changes the problem's dimensions and is part of the identity token.
    IpmOptions ok = solver.options();
    ok.common.max_iterations = 77;
    EXPECT_NO_THROW(solver.set_options(std::move(ok)));
    EXPECT_EQ(solver.options().common.max_iterations, 77);
    EXPECT_TRUE(solver.kkt_pattern_is_analyzed(hven::solvers::detail::problem_ref(*program)))
        << "a per-solve field does not touch the analysis";

    IpmOptions treatment = solver.options();
    treatment.fixed_variable_treatment = FixedVariableTreatment::kMakeConstraint;
    EXPECT_NO_THROW(solver.set_options(std::move(treatment)));
    EXPECT_FALSE(solver.kkt_pattern_is_analyzed(hven::solvers::detail::problem_ref(*program)))
        << "the treatment is part of the identity token: a change re-analyses";
}

// The T8.3 pin this replaced -- Options.IpmAnAttachOnlyFieldChangesThroughRelease
// ReplaceReattach, which executed the release() -> set_options() -> set_nlp()
// recovery sequence the refusal pointed at -- is GONE with the three methods it
// used. There is no attachment to release and no re-attach to make, and the
// case it covered (a transcription-time field changed on a solver that had
// already transcribed) is the qp_matching leg of the test above, which now ends
// in a live re-transcription rather than in a recovery dance.

// A replacement from inside the solve is a logic_error, the options do not
// move, and the guard clears on the unwind so the next solve still runs.
TEST(Options, IpmSetOptionsDuringASolveThrowsLogicError) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    IpmSolver solver;
    {
        IpmOptions o = solver.options();
        o.common.verbosity = hven::solvers::Verbosity::kSilent;
        solver.set_options(std::move(o));
    }
    const IpmOptions before = solver.options();

    int attempts = 0;
    bool saw_logic_error = false;
    IpmSolver *engine = &solver;
    // M6 W5 T8.6: the shared iteration callback, in place of the late callback
    // this test used to arm. Same assertions, same solve.
    engine->set_iteration_callback([&](const hven::solvers::IterationEvent &) {
        ++attempts;
        IpmOptions o = engine->options();
        o.common.max_iterations = 3;
        try {
            engine->set_options(std::move(o));
        } catch (const std::logic_error &) {
            saw_logic_error = true;
        }
        return hven::solvers::CallbackAction::kContinue;
    });

    ASSERT_EQ(solver.solve(hven::solvers::detail::problem_ref(*program), hs071_start()).status,
              hven::solvers::SolveStatus::kOptimal);
    ASSERT_GT(attempts, 0);
    EXPECT_TRUE(saw_logic_error);
    EXPECT_EQ(solver.options().common.max_iterations, before.common.max_iterations);

    // The guard cleared on the way out: a replacement between calls works, and
    // so does the next solve.
    IpmOptions after = solver.options();
    after.common.max_iterations = 400;
    EXPECT_NO_THROW(solver.set_options(std::move(after)));
    EXPECT_EQ(solver.options().common.max_iterations, 400);
    engine->clear_iteration_callback();
    EXPECT_EQ(solver.solve(hven::solvers::detail::problem_ref(*program), hs071_start()).status,
              hven::solvers::SolveStatus::kOptimal);
}

// THE SAME GUARD, CLEARED ON AN UNWIND RATHER THAN A RETURN (fix round 1). The
// test above catches its logic_error INSIDE the hook, so the solve returns
// normally and the guard's destructor runs on an ordinary exit. This one lets
// the exception LEAVE the solve, which is the path the RAII guard exists for:
// run_phase_sequence() unwinds, SolveInFlightGuard's destructor clears the
// flag, and the solver is usable again.
TEST(Options, IpmTheInFlightGuardClearsWhenAnExceptionLeavesTheSolve) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    IpmSolver solver;
    {
        IpmOptions o = solver.options();
        o.common.verbosity = hven::solvers::Verbosity::kSilent;
        solver.set_options(std::move(o));
    }
    IpmSolver *engine = &solver;
    engine->set_iteration_callback(
        [](const hven::solvers::IterationEvent &) -> hven::solvers::CallbackAction {
            throw std::logic_error("a hook that leaves the solve by throwing");
        });
    EXPECT_THROW(solver.solve(hven::solvers::detail::problem_ref(*program), hs071_start()),
                 std::logic_error);
    engine->clear_iteration_callback();

    // The guard did not survive the unwind: a replacement is accepted and the
    // next solve runs.
    IpmOptions after = engine->options();
    after.common.max_iterations = 400;
    EXPECT_NO_THROW(engine->set_options(std::move(after)));
    EXPECT_EQ(engine->options().common.max_iterations, 400);
    EXPECT_EQ(solver.solve(hven::solvers::detail::problem_ref(*program), hs071_start()).status,
              hven::solvers::SolveStatus::kOptimal);
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------

TEST(Options, PresetsReturnFullValues) {
    // Every shipped preset name produces a value validate() accepts, and the
    // value is FULL -- every field outside the preset's nine is the default.
    for (const auto &entry : hven::solvers::kInteriorPointSolverPresets) {
        const IpmOptions o = hven::solvers::ipm_preset(entry.name_);
        EXPECT_NO_THROW(hven::solvers::validate(o)) << "preset " << entry.name_;
        EXPECT_EQ(o.advanced.acceptance_strategy, entry.fields_.acceptance_strategy_)
            << entry.name_;
        EXPECT_EQ(o.advanced.merit_penalty_rule, entry.fields_.merit_penalty_rule_) << entry.name_;
        EXPECT_EQ(o.advanced.barrier_governor, entry.fields_.barrier_governor_) << entry.name_;
        EXPECT_EQ(o.advanced.never_monotone, entry.fields_.never_monotone_) << entry.name_;
        EXPECT_EQ(o.advanced.restoration_mode, entry.fields_.restoration_mode_) << entry.name_;
        EXPECT_EQ(o.advanced.inertia_mode, entry.fields_.inertia_mode_) << entry.name_;
        EXPECT_EQ(o.advanced.max_second_order_corrections, entry.fields_.max_soc_) << entry.name_;
        EXPECT_EQ(o.advanced.line_search_extended_backtracks, entry.fields_.ls_extended_iters_)
            << entry.name_;
        EXPECT_EQ(o.advanced.watchdog, entry.fields_.watchdog_) << entry.name_;
        // Outside the nine, a default value.
        EXPECT_EQ(o.common.max_iterations, IpmOptions{}.common.max_iterations) << entry.name_;
        EXPECT_EQ(o.common.verbosity, IpmOptions{}.common.verbosity) << entry.name_;
    }
}

TEST(Options, AnUnknownIpmPresetIsRefusedAndListsTheValidNames) {
    try {
        hven::solvers::ipm_preset("no-such-preset");
        FAIL() << "an unknown preset name must be refused";
    } catch (const std::invalid_argument &e) {
        const std::string what(e.what());
        EXPECT_NE(what.find("no-such-preset"), std::string::npos);
        EXPECT_NE(what.find(hven::solvers::kInteriorPointSolverPresets[0].name_),
                  std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// The SQP engine's half of the same surface
// ---------------------------------------------------------------------------

namespace {

using hven::solvers::SqpOptions;
using hven::solvers::SqpSolver;

// Whatever the driver reads at construction, at the SQP engine's own defaults.
SqpOptions sqp_default() { return SqpOptions{}; }

} // namespace

// The SQP engine's `common` defaults are the struct's own: 0 threads (leave the
// backend alone), silent, 100 iterations, kWarm.
TEST(Options, SqpCommonDefaultsAreTheStructsOwn) {
    const SqpOptions o;
    EXPECT_EQ(o.common.threads, 0);
    EXPECT_EQ(o.common.verbosity, hven::solvers::Verbosity::kSilent);
    EXPECT_EQ(o.common.max_iterations, 100);
    EXPECT_EQ(o.common.start_level, hven::solvers::StartLevel::kWarm);
}

TEST(Options, SqpValidateRefusesTheCommonFieldsAndWhatItAlwaysRefused) {
    {
        SqpOptions o;
        o.common.max_iterations = -1;
        // The pre-T8.3 spelling was a one-line forwarder onto this same body;
        // T8.10 removed it, so there is one name and one call to make.
        EXPECT_THROW(hven::solvers::validate(o), std::invalid_argument);
    }
    {
        SqpOptions o;
        o.common.threads = -1;
        try {
            hven::solvers::validate(o);
            FAIL() << "a negative thread count must be refused";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("common.threads"), std::string::npos);
        }
    }
    {
        SqpOptions o;
        o.common.verbosity = static_cast<hven::solvers::Verbosity>(-1);
        try {
            hven::solvers::validate(o);
            FAIL() << "an out-of-range verbosity must be refused";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("common.verbosity"), std::string::npos);
        }
    }
    EXPECT_NO_THROW(hven::solvers::validate(SqpOptions{}));
}

TEST(Options, SqpPresetsReturnFullValues) {
    const SqpOptions d = hven::solvers::sqp_preset("default");
    EXPECT_NO_THROW(hven::solvers::validate(d));
    EXPECT_EQ(d.common.max_iterations, SqpOptions{}.common.max_iterations);
    EXPECT_EQ(d.common.optimality_tol, SqpOptions{}.common.optimality_tol);
    EXPECT_EQ(d.common.verbosity, SqpOptions{}.common.verbosity);
    try {
        hven::solvers::sqp_preset("no-such-preset");
        FAIL() << "an unknown preset name must be refused";
    } catch (const std::invalid_argument &e) {
        const std::string what(e.what());
        EXPECT_NE(what.find("no-such-preset"), std::string::npos);
        EXPECT_NE(what.find("default"), std::string::npos);
    }
}

TEST(Options, SqpSetOptionsIsTransactional) {
    SqpSolver d(sqp_default());
    {
        SqpOptions o = d.options();
        o.common.max_iterations = 42;
        d.set_options(std::move(o));
    }
    ASSERT_EQ(d.options().common.max_iterations, 42);

    SqpOptions bad = d.options();
    bad.common.max_iterations = -1;
    bad.common.optimality_tol = 1.0e-9;
    EXPECT_THROW(d.set_options(bad), std::invalid_argument);
    EXPECT_EQ(d.options().common.max_iterations, 42);
    EXPECT_EQ(d.options().common.optimality_tol, SqpOptions{}.common.optimality_tol);

    // The old engines are still in force too: the driver solves.
    const auto p = hven::solvers::test_support::make_hs(7);
    EXPECT_NO_THROW(
        d.solve(hven::solvers::detail::problem_ref(*p.model), (*p.model).start_point()));
}

// The replacement is refused while a solve is in flight -- reached through
// SqpOptions::make_strategy, which the driver calls once per solve from inside
// solve_impl. The options do not move and the guard clears on the way out.
TEST(Options, SqpSetOptionsDuringASolveThrowsLogicError) {
    SqpSolver d(sqp_default());
    bool attempted = false;
    bool refused = false;
    {
        SqpOptions o = d.options();
        o.advanced.make_strategy = [&]() -> std::unique_ptr<hven::solvers::GlobalizationStrategy> {
            attempted = true;
            SqpOptions replacement = d.options();
            replacement.common.max_iterations = 5;
            try {
                d.set_options(std::move(replacement));
            } catch (const std::logic_error &) {
                refused = true;
            }
            return std::make_unique<hven::solvers::FunnelStrategy>();
        };
        d.set_options(std::move(o));
    }
    const auto p = hven::solvers::test_support::make_hs(7);
    const auto sol =
        d.solve(hven::solvers::detail::problem_ref(*p.model), (*p.model).start_point());
    (void)sol;
    ASSERT_TRUE(attempted) << "the strategy factory never ran";
    EXPECT_TRUE(refused) << "a replacement under an in-flight solve must be a logic_error";
    EXPECT_EQ(d.options().common.max_iterations, SqpOptions{}.common.max_iterations);

    // The guard cleared: a replacement between calls goes through, AND so does
    // the next solve -- the IPM pin above asserts both and this one now does
    // too (fix round 1).
    SqpOptions after = d.options();
    after.advanced.make_strategy = {};
    after.common.max_iterations = 11;
    EXPECT_NO_THROW(d.set_options(std::move(after)));
    EXPECT_EQ(d.options().common.max_iterations, 11);
    EXPECT_NO_THROW(
        d.solve(hven::solvers::detail::problem_ref(*p.model), (*p.model).start_point()));
}

// THE SAME GUARD, CLEARED ON AN UNWIND RATHER THAN A RETURN (fix round 1), the
// SQP half of the IPM pin above: the hook throws OUT of the solve, solve_impl
// unwinds, SolveInFlightGuard's destructor clears the flag, and the driver is
// usable again.
TEST(Options, SqpTheInFlightGuardClearsWhenAnExceptionLeavesTheSolve) {
    SqpSolver d(sqp_default());
    {
        SqpOptions o = d.options();
        o.advanced.make_strategy = []() -> std::unique_ptr<hven::solvers::GlobalizationStrategy> {
            throw std::logic_error("a hook that leaves the solve by throwing");
        };
        d.set_options(std::move(o));
    }
    const auto p = hven::solvers::test_support::make_hs(7);
    EXPECT_THROW(d.solve(hven::solvers::detail::problem_ref(*p.model), (*p.model).start_point()),
                 std::logic_error);

    SqpOptions after = d.options();
    after.advanced.make_strategy = {};
    after.common.max_iterations = 11;
    EXPECT_NO_THROW(d.set_options(std::move(after)));
    EXPECT_EQ(d.options().common.max_iterations, 11);
    EXPECT_NO_THROW(
        d.solve(hven::solvers::detail::problem_ref(*p.model), (*p.model).start_point()));
}

// THE LEDGER LABEL SEQUENCE CONTINUES ACROSS A REBUILD (A3; fix round 1).
//
// set_options() replaces the QpEngine, and a fresh engine's own solve_counter_
// starts at 0 -- so without the carry the QP records after a replacement would
// restart at `<prefix>_qp_0` beside the one already in the ledger, and two
// different subproblems would share a label. The carry is exact rather than
// merely sufficient because QpEngine advances that counter ONLY inside its
// `ledger_ != nullptr` branch: with no ledger attached both engines sit at 0.
//
// The pin is over the WHOLE sequence, not a count: every label is
// `<prefix>_qp_<i>` for its own position i, and all of them are distinct.
TEST(Options, SqpAnOptionReplacementDoesNotRestartTheLedgerLabelSequence) {
    SqpSolver d(sqp_default());
    hven::solvers::Ledger ledger;
    d.attach_ledger(&ledger, "chain");

    const auto p = hven::solvers::test_support::make_hs(7);

    d.solve(hven::solvers::detail::problem_ref(*p.model), (*p.model).start_point());
    const std::size_t after_first = ledger.records().size();
    ASSERT_GT(after_first, 0u) << "the fixture must produce QP records to pin";

    d.set_options(d.options()); // the identical-options rebuild
    d.solve(hven::solvers::detail::problem_ref(*p.model), (*p.model).start_point());
    const std::size_t after_second = ledger.records().size();
    ASSERT_GT(after_second, after_first);

    SqpOptions changed = d.options(); // and a CHANGED-options rebuild
    changed.qp.schur_cap = d.options().qp.schur_cap + 1;
    d.set_options(std::move(changed));
    d.solve(hven::solvers::detail::problem_ref(*p.model), (*p.model).start_point());
    const std::size_t after_third = ledger.records().size();
    ASSERT_GT(after_third, after_second);

    std::vector<std::string> labels;
    labels.reserve(ledger.records().size());
    for (const auto &rec : ledger.records()) {
        labels.push_back(rec.label);
    }
    for (std::size_t i = 0; i < labels.size(); ++i) {
        EXPECT_EQ(labels[i], "chain_qp_" + std::to_string(i))
            << "the label sequence restarted or skipped at position " << i;
    }
    std::vector<std::string> sorted = labels;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(std::distance(sorted.begin(), std::unique(sorted.begin(), sorted.end())),
              static_cast<std::ptrdiff_t>(labels.size()))
        << labels.size() << " QP records across three solves and two rebuilds must carry "
        << labels.size() << " distinct labels";
}

// ---------------------------------------------------------------------------
// The hot-handle options fingerprint (M6 W5 T8.3)
// ---------------------------------------------------------------------------

// EVERY field of QpOptions, plus the thread count, must move the hash. A field
// left out is a hole in the hot-handle reuse gate: an engine built under one
// value would adopt a K0 factorized under another. This test flips each of the
// fields ALONE from a default value and requires the fingerprint to move;
// add a field to QpOptions and this test is where the omission surfaces.
TEST(Options, TheOptionsFingerprintCoversEveryQpOptionsField) {
    using hven::solvers::options_fingerprint;
    using hven::solvers::QpOptions;
    using hven::solvers::WorkingSetLinearAlgebra;

    const QpOptions base;
    const std::uint64_t h0 = options_fingerprint(base, 0);

    // Stable: the same value hashes the same, every time and from an
    // independently constructed struct.
    EXPECT_EQ(options_fingerprint(base, 0), h0);
    EXPECT_EQ(options_fingerprint(QpOptions{}, 0), h0);

    const auto moved = [&](auto edit) {
        QpOptions o = base;
        edit(o);
        return options_fingerprint(o, 0) != h0;
    };
    EXPECT_TRUE(moved([](QpOptions &o) { o.primal_delta *= 2.0; })) << "primal_delta";
    EXPECT_TRUE(moved([](QpOptions &o) { o.dual_mu *= 2.0; })) << "dual_mu";
    EXPECT_TRUE(moved([](QpOptions &o) { o.feas_tol *= 2.0; })) << "feas_tol";
    EXPECT_TRUE(moved([](QpOptions &o) { o.opt_tol *= 2.0; })) << "opt_tol";
    EXPECT_TRUE(moved([](QpOptions &o) { o.max_iter = o.max_iter.value_or(0) + 1; })) << "max_iter";
    EXPECT_TRUE(moved([](QpOptions &o) { o.schur_cap += 1; })) << "schur_cap";
    EXPECT_TRUE(moved([](QpOptions &o) { o.schur_cond_max *= 2.0; })) << "schur_cond_max";
    EXPECT_TRUE(moved([](QpOptions &o) { o.ws_algebra = WorkingSetLinearAlgebra::kRefactorize; }))
        << "ws_algebra";
    EXPECT_TRUE(moved([](QpOptions &o) { o.tr_radius = 1.0; })) << "tr_radius";
    EXPECT_TRUE(moved([](QpOptions &o) { o.independent_working_set = false; }))
        << "independent_working_set";
    EXPECT_TRUE(moved([](QpOptions &o) { o.hessian_known_convex = true; }))
        << "hessian_known_convex";

    // And the thread count, which is not a QpOptions field but IS part of what a
    // factorization was built under. Carried and not applied by the SQP engine
    // until T8.8; hashed from T8.3 so this pin's meaning does not change then.
    EXPECT_NE(options_fingerprint(base, 1), h0) << "threads";
    EXPECT_NE(options_fingerprint(base, 2), options_fingerprint(base, 1)) << "threads";
}
