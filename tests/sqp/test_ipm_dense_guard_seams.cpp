// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// THE IPM DENSE GUARD'S FAILED RE-RUN (DN-1b review F8, M8 DN-1d). On a numerically singular
// dense factorization the guard strips the ladder's shifts and re-runs the factorization; when
// that re-run leaves no numerics the step it was for must not be taken. No real backend fails
// there (the dense arm or Pardiso always produce numerics), so the re-run is failed through the
// dense-attempt and platform-factorize injectors (hven/detail/linear/fault_injection.h).
//
// THE BRANCH IS DEFENSIVE: factor_impl already reports `exhausted` when its last factorization
// failed (every rung then reads a wrong inertia), so this test pins the route -- the guard fired,
// its re-run failed, the solve ends unsuccessfully with a finite point -- rather than a
// difference the branch alone makes.
//
// THE FIXTURE IS A FAMILY, NOT ONE PROBLEM. Whether the step guard fires at all is decided by
// rounding: the step KKT [[2I, J^T], [J, 0]] does not depend on x, J's second row is `scale` x
// its first (consistent targets, so J is rank deficient to rounding), and dsytrf factors it either
// with an exactly zero pivot (the platform session takes it; no guard), with a pivot block of
// rounding-noise size and the expected inertia (the solve's residual ratio is O(1); the guard
// fires), or with the expected inertia and an accurate solve (only the multiplier estimate's own
// guard fires). Which of these a given scale lands in is the platform LAPACK's rounding: at
// scale 1.76 MKL and OpenBLAS factor the step KKT with a 2x2 pivot block of determinant ~1e-33
// (the guard fires); on macOS CI, Accelerate did not reach the step guard at 1.76 at all. So the
// test searches a fixed family of scales, 1.76 first, for a member whose step guard fires, and
// pins the route on the first one.
//
// A FACTORIZATION THAT LEFT NO NUMERICS IS NEVER A THROW (IpmNoNumericsSeam). The same two
// injectors, armed from a later pass, make every factorization from that pass on leave no numerics
// -- the dense attempt reads exactly singular and drops its numerics, and the platform session,
// which never factorized on a kAlways solve, returns the injected code (the one scenario
// FactorizeFaultInjector is faithful for). That is what a backend refusing every rung of the
// inertia-correction ladder looks like. What the solve then does depends on the code's kind
// (Wächter & Biegler 2006, Algorithm IC):
//   * a NUMERIC refusal (kNumericRefusal) abandons the direction and takes the exhausted-inertia
//     dispatch: elastic re-centre, then feasibility restoration; with neither available it stops
//     at an acceptable-tier iterate (kAcceptable) or ends kNumericalError (stop reason kNone) at
//     the committed iterate (return_best off; with it on, the best recorded one), with no step
//     taken -- at the step solve, at the PROBE predictor's
//     solve (which runs first), and at the dense guard's failed re-run;
//   * a HARD backend error (kHardBackendError) ends kNumericalError at once, restoration or not.
//
// THE SPARSE ARM IS NOT FAITHFULLY INJECTABLE HERE: on a sparse solve the platform session's
// initial factorization succeeds, and FactorizeFaultInjector is faithful only on a session that
// never factorized successfully (docs/testing.md). Every test below is a kAlways (dense-arm)
// solve for that reason; the sparse arm's no-numerics exit runs the same code after factor_impl.

#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <fmt/format.h>
#include <gtest/gtest.h>

#if defined(USE_ACCELERATE_SPARSE)
#include <Accelerate/Accelerate.h>
#endif

#include "../drivers/support/hs071_problem.h"
#include <hven/detail/linear/fault_injection.h>
#include <hven/drivers/ipm_enums.h>
#include <hven/drivers/ipm_iterate.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/trace.h>
#include <hven/model/nlp.h>
#include <hven/model/problem.h>

namespace {

using hven::linear::DensePolicy;
using hven::linear::detail::testing::DenseFactorizeFaultInjector;
using hven::linear::detail::testing::FactorizeFaultInjector;

// test_dense_arm_engines.cpp's NearDuplicateRowsProblem with its scale as a parameter: the second
// row is `scale` x the first.
class NearDuplicateRows final : public hven::Nlp {
  public:
    explicit NearDuplicateRows(double scale) : scale_(scale) {}
    hven::NlpDimensions dimensions() const override { return {2, 2, 4, 2}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl << -inf, -inf;
        xu << inf, inf;
        gl << 1.0, scale_;
        gu << 1.0, scale_;
    }
    double eval_f(hven::ConstVecRef x) const override {
        return (x[0] - 1.0) * (x[0] - 1.0) + (x[1] - 2.0) * (x[1] - 2.0);
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        g[0] = 2.0 * (x[0] - 1.0);
        g[1] = 2.0 * (x[1] - 2.0);
    }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        g[0] = x[0] + 0.3 * x[1];
        g[1] = scale_ * x[0] + (scale_ * 0.3) * x[1];
    }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 0, 1, 1;
        c << 0, 1, 0, 1;
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jacobian(hven::ConstVecRef, hven::VecRef v) const override {
        v << 1.0, 0.3, scale_, scale_ * 0.3;
    }
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v << 2.0 * obj_factor, 2.0 * obj_factor;
    }
    std::string name() const override { return "NearDuplicateRows"; }

  private:
    double scale_;
};

// 1.76 first (the scale the Linux backends reach the route on), then every other hundredth from
// 0.50 to 4.99. Of those 450 scales, MKL and OpenBLAS factor 336 step KKTs with an exactly zero
// pivot and 65 with a rounding-noise pivot block (1.76's class).
std::vector<double> scale_family() {
    std::vector<double> scales{1.76};
    for (int k = 50; k < 500; ++k) {
        if (k != 176) {
            scales.push_back(k / 100.0);
        }
    }
    return scales;
}

// The platform's codes for the two kinds (src/interior/kkt_factorization.cpp's categorization):
// a numeric refusal (Eigen::NumericalIssue) and a hard backend error (Eigen::InvalidInput).
#if defined(USE_ACCELERATE_SPARSE)
constexpr int kNumericRefusal = static_cast<int>(SparseFactorizationFailed);
constexpr int kHardBackendError = static_cast<int>(SparseInternalError);
#else
constexpr int kNumericRefusal = -4;   // Pardiso: zero pivot
constexpr int kHardBackendError = -1; // Pardiso: input inconsistent
#endif

void disarm() {
    DenseFactorizeFaultInjector::active = false;
    FactorizeFaultInjector::active = false;
}

struct Armed {
    explicit Armed(int pass, int backend_code = kNumericRefusal) {
        DenseFactorizeFaultInjector::pass = pass;
        DenseFactorizeFaultInjector::active = true;
        FactorizeFaultInjector::injected_backend_code = backend_code;
        FactorizeFaultInjector::active = true;
    }
    ~Armed() { disarm(); }
    Armed(const Armed &) = delete;
    Armed &operator=(const Armed &) = delete;
};

bool success(hven::SolveStatus s) {
    return s == hven::SolveStatus::kOptimal || s == hven::SolveStatus::kAcceptable;
}

// A solve recording the x of every iteration event: the last is the iterate the final iteration
// started from, which a solve that takes no step there returns unchanged. `on_event` (optional) may
// act on the event too -- the restoration recovery test disarms the injectors from it. Every
// `ipm.iter` row is kept as well.
struct RecordedSolve {
    hven::IpmResult result;
    hven::IpmStopReason stop_reason = hven::IpmStopReason::kNone;
    std::vector<Eigen::VectorXd> event_x;
    std::vector<hven::IpmIterate> rows;
};

class RowSink final : public hven::TraceSink {
  public:
    explicit RowSink(std::vector<hven::IpmIterate> &rows) : rows_(rows) {}
    void on_ipm_iter(const hven::IpmIterTraceEvent &event) override {
        rows_.push_back(event.iterate);
    }

  private:
    std::vector<hven::IpmIterate> &rows_;
};

using EventHook = std::function<void(const hven::IterationEvent &)>;

RecordedSolve record_solve(const hven::IpmOptions &o, std::shared_ptr<hven::Nlp> nlp,
                           const Eigen::VectorXd &x0, const EventHook &on_event = {}) {
    hven::IpmSolver solver(o);
    RecordedSolve out;
    RowSink sink(out.rows);
    solver.attach_trace(&sink);
    solver.set_iteration_callback([&out, &on_event](const hven::IterationEvent &e) {
        out.event_x.emplace_back(e.x);
        if (on_event) {
            on_event(e);
        }
        return hven::CallbackAction::kContinue;
    });
    hven::Problem problem(std::move(nlp));
    out.result = solver.solve(problem, x0);
    out.stop_reason = solver.last_stop_reason();
    return out;
}

hven::IpmOptions dense_options() {
    hven::IpmOptions o;
    o.common.verbosity = hven::Verbosity::kSilent;
    o.linear.dense_factorization = DensePolicy::kAlways;
    return o;
}

RecordedSolve solve_dense(double scale) {
    Eigen::VectorXd x0(2);
    x0 << 3.0, -1.0;
    return record_solve(dense_options(), std::make_shared<NearDuplicateRows>(scale), x0);
}

hven::IpmOptions hs071_options(hven::IpmBarrierMode barrier_mode) {
    hven::IpmOptions o = dense_options();
    o.advanced.optimize_barrier_mode = barrier_mode;
    o.advanced.solve_barrier_mode = barrier_mode;
    return o;
}

// HS071 on the dense arm.
RecordedSolve solve_hs071_dense(const hven::IpmOptions &o, const EventHook &on_event = {}) {
    return record_solve(o, std::make_shared<hven_drivers_tests::Hs071Problem>(),
                        hven_drivers_tests::hs071_start(), on_event);
}

RecordedSolve solve_hs071_dense(hven::IpmBarrierMode barrier_mode) {
    return solve_hs071_dense(hs071_options(barrier_mode));
}

// x returned unmoved from the iterate the failing iteration started from.
void expect_x_unmoved(const RecordedSolve &run) {
    ASSERT_FALSE(run.event_x.empty());
    const Eigen::VectorXd &start = run.event_x.back();
    ASSERT_EQ(run.result.x.size(), start.size());
    EXPECT_TRUE(run.result.x.allFinite());
    EXPECT_LE((run.result.x - start).lpNorm<Eigen::Infinity>(),
              1e-12 * (1.0 + start.lpNorm<Eigen::Infinity>()))
        << "a step was taken from the iterate the failing iteration started from";
}

// The contract every no-numerics abort keeps: a status with no stop reason of its own, the
// ladder's words, the last factorization's failure recorded, and -- with return_best off, the
// default -- the iterate the failing iteration started from returned unmoved (the return_best test
// covers the other case).
void expect_ladder_exit(const RecordedSolve &run, hven::Index pass) {
    const hven::IpmResult &r = run.result;
    EXPECT_EQ(r.status, hven::SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(run.stop_reason, hven::IpmStopReason::kNone);
    EXPECT_NE(r.message.find("inertia-correction ladder at iteration"), std::string::npos)
        << r.message;
    EXPECT_NE(r.message.find("ended on a failed KKT factorization (numerical issue)"),
              std::string::npos)
        << r.message;
    EXPECT_NE(r.message.find("neither an elastic re-centre nor feasibility restoration was "
                             "available"),
              std::string::npos)
        << r.message;
    EXPECT_NE(r.last_kkt_info, Eigen::Success);
    EXPECT_FALSE(r.returned_best_iterate);
    EXPECT_EQ(r.kkt_factor_counters.dense_factorize_count, pass);
    EXPECT_GT(r.kkt_factor_counters.dense_fallback_count, 0);
    expect_x_unmoved(run);
}

} // namespace

TEST(IpmDenseGuardSeam, AFailedGuardReRunEndsTheSolveWithoutItsStep) {
    // What each member did, reported if none reaches the route: the next platform's observations.
    std::string survey;
    bool found = false;
    for (const double scale : scale_family()) {
        const hven::IpmResult clean = solve_dense(scale).result;
        const hven::Index attempts = clean.kkt_factor_counters.dense_factorize_count;
        survey += fmt::format("\n  scale {:.2f}: clean {} triggers {} dense {} fallbacks {}", scale,
                              success(clean.status) ? "ok" : "FAILED",
                              clean.dense_residual_ratio_triggers, attempts,
                              clean.kkt_factor_counters.dense_fallback_count);
        // A member qualifies when its clean solve succeeds with every factorization on the dense
        // arm (so dense attempts and dense_factorize_count count the same thing) and some guard
        // fired.
        if (!success(clean.status) || clean.kkt_factor_counters.dense_fallback_count != 0 ||
            clean.dense_residual_ratio_triggers == 0) {
            continue;
        }

        // Every dense attempt from `pass` on fails (dense read singular, platform refuses), and
        // `pass < attempts`, so the injector always fires: a run identical to the clean one is
        // never mistaken for the route. EVERY injected run returns without an exception, at a
        // finite point: restoration is off (the default), so nothing can resolve the refusal, and
        // a run whose failure no guard absorbs ends at the initial factorization or at an
        // iteration's inertia-correction ladder (kNumericalError, its message naming which) or
        // stops at an acceptable-tier iterate (kAcceptable); the route is the run whose message
        // names the step guard's failed re-run.
        for (int pass = 0; pass < static_cast<int>(attempts); ++pass) {
            SCOPED_TRACE(fmt::format("scale {:.2f}, pass {}", scale, pass));
            RecordedSolve run;
            {
                const Armed armed(pass);
                ASSERT_NO_THROW(run = solve_dense(scale));
            }
            const hven::IpmResult &r = run.result;
            EXPECT_TRUE(r.status == hven::SolveStatus::kNumericalError ||
                        r.status == hven::SolveStatus::kAcceptable)
                << r.message;
            EXPECT_TRUE(r.x.allFinite());
            if (r.message.find("dense step guard's refactorization") == std::string::npos) {
                continue;
            }
            found = true;
            // The member and pass the route was pinned on (Linux MKL and OpenBLAS: 1.76, 3).
            ::testing::Test::RecordProperty("route",
                                            fmt::format("scale {:.2f} pass {}", scale, pass));
            EXPECT_EQ(r.status, hven::SolveStatus::kNumericalError) << r.message;
            EXPECT_EQ(run.stop_reason, hven::IpmStopReason::kNone);
            EXPECT_NE(r.message.find("failed (numerical issue)"), std::string::npos) << r.message;
            EXPECT_GE(r.dense_residual_ratio_triggers, 1);
            EXPECT_EQ(r.kkt_factor_counters.dense_factorize_count, pass);
            EXPECT_GT(r.kkt_factor_counters.dense_fallback_count, 0);
            EXPECT_NE(r.last_kkt_info, Eigen::Success);
            // The step the guard rejected is discarded: x is the iterate its iteration started
            // from.
            expect_x_unmoved(run);
            break;
        }
        if (found) {
            break;
        }
        survey += " -- no injected run reached the step guard's re-run";
    }
    EXPECT_TRUE(found) << "no member of the family reached the guard's failed re-run:" << survey;
}

// The step solve's site: from the first iteration's ladder on, every factorization leaves no
// numerics (pass 1: only the initial factorization succeeds), so the ladder ends on a failed one
// and the step solve has nothing to solve on. Restoration is off (the default), so the dispatch has
// nothing to route to, and the start is not acceptable: kNumericalError.
TEST(IpmNoNumericsSeam, ALadderEndingWithoutNumericsIsAStatusAtTheFirstIteration) {
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = solve_hs071_dense(hven::IpmBarrierMode::kLoqo));
    }
    expect_ladder_exit(run, 1);
    EXPECT_NE(run.result.message.find("at iteration 0 "), std::string::npos) << run.result.message;
}

// The same site mid-solve: only the clean solve's last factorization fails, so the solve has taken
// steps before it, and the iterate its last iteration started from is already at the acceptable
// tier. With nothing to route to, the solve stops there -- kAcceptable, as the un-evaluable
// dispatch's exit does -- returning that iterate unmoved and no exception.
TEST(IpmNoNumericsSeam, ALadderEndingWithoutNumericsAtAnAcceptableIterateStopsThere) {
    const RecordedSolve clean = solve_hs071_dense(hven::IpmBarrierMode::kLoqo);
    ASSERT_EQ(clean.result.status, hven::SolveStatus::kOptimal) << clean.result.message;
    ASSERT_EQ(clean.result.kkt_factor_counters.dense_fallback_count, 0);
    const hven::Index attempts = clean.result.kkt_factor_counters.dense_factorize_count;
    ASSERT_GT(clean.event_x.size(), 2U) << "the fixture must take steps before the failure";
    RecordedSolve run;
    {
        const Armed armed(static_cast<int>(attempts - 1));
        ASSERT_NO_THROW(run = solve_hs071_dense(hven::IpmBarrierMode::kLoqo));
    }
    const hven::IpmResult &r = run.result;
    EXPECT_EQ(r.status, hven::SolveStatus::kAcceptable) << r.message;
    EXPECT_EQ(run.stop_reason, hven::IpmStopReason::kNone);
    EXPECT_NE(r.last_kkt_info, Eigen::Success);
    EXPECT_EQ(r.kkt_factor_counters.dense_factorize_count, attempts - 1);
    EXPECT_GT(run.event_x.size(), 2U);
    expect_x_unmoved(run);
}

// The PROBE predictor's site: under the PROBE barrier mode the governor solves on the iteration's
// factorization before the step solve does, so it must not run on one that left no numerics.
TEST(IpmNoNumericsSeam, ALadderEndingWithoutNumericsIsAStatusBeforeTheProbePredictor) {
    const RecordedSolve clean = solve_hs071_dense(hven::IpmBarrierMode::kProbe);
    ASSERT_TRUE(success(clean.result.status)) << clean.result.message;
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = solve_hs071_dense(hven::IpmBarrierMode::kProbe));
    }
    expect_ladder_exit(run, 1);
}

// return_best: the abort hands back the best iterate it recorded, flagged as such, at a finite
// point.
TEST(IpmNoNumericsSeam, ALadderEndingWithoutNumericsReturnsTheBestIterateUnderReturnBest) {
    hven::IpmOptions o = hs071_options(hven::IpmBarrierMode::kLoqo);
    o.return_best = true;
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = solve_hs071_dense(o));
    }
    const hven::IpmResult &r = run.result;
    EXPECT_EQ(r.status, hven::SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(run.stop_reason, hven::IpmStopReason::kNone);
    EXPECT_TRUE(r.returned_best_iterate);
    ASSERT_EQ(r.x.size(), 4);
    EXPECT_TRUE(r.x.allFinite());
}

// Restoration configured: the numeric refusal at the first iteration enters it, with no step and
// no throw. The injectors stay armed, so every later factorization -- restoration's own -- leaves
// no numerics too; restoration is then already active, nothing is left to route to, and the solve
// ends kNumericalError.
class IpmNoNumericsRestorationSeam : public ::testing::TestWithParam<hven::IpmRestorationMode> {};

TEST_P(IpmNoNumericsRestorationSeam, ANumericRefusalEntersRestoration) {
    hven::IpmOptions o = hs071_options(hven::IpmBarrierMode::kLoqo);
    o.advanced.restoration_mode = GetParam();
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = solve_hs071_dense(o));
    }
    const hven::IpmResult &r = run.result;
    EXPECT_EQ(r.last_feasibility_restoration_entries.value_or(0), 1) << r.message;
    EXPECT_EQ(r.status, hven::SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(run.stop_reason, hven::IpmStopReason::kNone);
    EXPECT_NE(r.message.find("(numerical issue)"), std::string::npos) << r.message;
    EXPECT_GT(run.event_x.size(), 1U) << "the solve continued past the iteration that entered it";
    EXPECT_TRUE(r.x.allFinite());
}

// The same entry with the backend recovered once restoration has begun (the injectors disarmed at
// the first event after the failing iteration; every later dense attempt is a real one):
// restoration carries the solve on, and it succeeds. The clean solve never enters restoration, so
// every entry is the refusal's doing (the nested strategy may enter once more on its own way back).
TEST_P(IpmNoNumericsRestorationSeam, ANumericRefusalIsRecoveredThroughRestoration) {
    hven::IpmOptions o = hs071_options(hven::IpmBarrierMode::kLoqo);
    o.advanced.restoration_mode = GetParam();
    const RecordedSolve clean = solve_hs071_dense(o);
    ASSERT_TRUE(success(clean.result.status)) << clean.result.message;
    ASSERT_EQ(clean.result.last_feasibility_restoration_entries.value_or(0), 0);
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = solve_hs071_dense(o, [](const hven::IterationEvent &e) {
                            if (e.iteration >= 1) {
                                disarm();
                            }
                        }));
    }
    const hven::IpmResult &r = run.result;
    EXPECT_GE(r.last_feasibility_restoration_entries.value_or(0), 1) << r.message;
    EXPECT_TRUE(success(r.status)) << r.message;
    EXPECT_TRUE(r.x.allFinite());
}

INSTANTIATE_TEST_SUITE_P(BothStrategies, IpmNoNumericsRestorationSeam,
                         ::testing::Values(hven::IpmRestorationMode::kProximalSwitch,
                                           hven::IpmRestorationMode::kL1Nested),
                         [](const ::testing::TestParamInfo<hven::IpmRestorationMode> &info) {
                             return info.param == hven::IpmRestorationMode::kProximalSwitch
                                        ? std::string("ProximalSwitch")
                                        : std::string("L1Nested");
                         });

// A hard backend error (a code the factorization layer reads as InvalidInput, not a numeric
// refusal) is not routed: the solve ends kNumericalError at once, restoration configured and never
// entered.
TEST(IpmNoNumericsSeam, AHardBackendErrorEndsTheSolveWithoutRestoration) {
    hven::IpmOptions o = hs071_options(hven::IpmBarrierMode::kLoqo);
    o.advanced.restoration_mode = hven::IpmRestorationMode::kProximalSwitch;
    RecordedSolve run;
    {
        const Armed armed(1, kHardBackendError);
        ASSERT_NO_THROW(run = solve_hs071_dense(o));
    }
    const hven::IpmResult &r = run.result;
    EXPECT_EQ(r.status, hven::SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(run.stop_reason, hven::IpmStopReason::kNone);
    EXPECT_NE(r.message.find("the inertia-correction ladder at iteration 0 ended on a failed KKT "
                             "factorization (backend error); no step can be computed"),
              std::string::npos)
        << r.message;
    EXPECT_EQ(r.message.find("neither"), std::string::npos) << r.message;
    EXPECT_EQ(r.last_kkt_info, Eigen::InvalidInput);
    EXPECT_EQ(r.last_feasibility_restoration_entries.value_or(0), 0);
    expect_x_unmoved(run);
}

// The aborted iteration's row carries its own barrier objective at the mu it was handed: with the
// barrier governor skipped (its PROBE mode would solve), the governor's tail still runs at the
// current mu, so the row's barr_obj is the bound term PLUS the inequality slack's -mu*log(s) --
// the account a restoration entry from the same point reads. HS071's bounds are 1 <= x_i <= 5, so
// the bound term is recomputed here from the iterate and the row's mu.
TEST(IpmNoNumericsSeam, TheAbortedIterationsRowCarriesItsBarrierObjective) {
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = solve_hs071_dense(hven::IpmBarrierMode::kLoqo));
    }
    ASSERT_EQ(run.result.status, hven::SolveStatus::kNumericalError) << run.result.message;
    ASSERT_FALSE(run.rows.empty());
    ASSERT_FALSE(run.event_x.empty());
    const hven::IpmIterate &row = run.rows.back();
    const Eigen::VectorXd &x = run.event_x.back();
    double bound_term = 0.0;
    for (Eigen::Index k = 0; k < x.size(); ++k) {
        bound_term += -row.mu * (std::log(x[k] - 1.0) + std::log(5.0 - x[k]));
    }
    EXPECT_EQ(row.iter, 0);
    ASSERT_TRUE(std::isfinite(row.barr_obj));
    // The slack term -mu*log(s): on this box s is ~mu = 1e-3 at the start, so the term is ~6.9e-3;
    // without the tail the difference is rounding (the row carries the bound term alone).
    EXPECT_GT(row.barr_obj - bound_term, 1e-4)
        << "barr_obj " << row.barr_obj << ", bound term " << bound_term << ", mu " << row.mu;
}
