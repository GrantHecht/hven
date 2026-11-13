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
//     dispatch in the un-evaluable dispatch's order: an acceptable-tier iterate OUTSIDE
//     restoration stops there (kAcceptable; inside restoration a row's residuals are the
//     subproblem's and never stop the solve), then the elastic re-centre of an active nested
//     phase, then feasibility restoration; with none available it ends kNumericalError (stop
//     reason kNone) at the committed iterate (return_best off; with it on, the best recorded one),
//     with no step taken -- at the step solve, at the PROBE predictor's solve (which runs first),
//     and at the dense guard's failed re-run;
//   * a HARD backend error (kHardBackendError) ends kNumericalError at once, restoration or not.
//
// THE SPARSE ARM IS NOT FAITHFULLY INJECTABLE HERE: on a sparse solve the platform session's
// initial factorization succeeds, and FactorizeFaultInjector is faithful only on a session that
// never factorized successfully (docs/testing.md). Every test below is a kAlways (dense-arm)
// solve for that reason; the sparse arm's no-numerics exit runs the same code after factor_impl.

#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
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
#include <hven/core/errors.h>
#include <hven/detail/drivers/ipm_dense_testing.h>
#include <hven/detail/globalization/recovery_chain.h>
#include <hven/detail/linear/fault_injection.h>
#include <hven/drivers/ipm_enums.h>
#include <hven/drivers/ipm_iterate.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/trace.h>
#include <hven/model/nlp.h>
#include <hven/model/problem.h>

namespace {

using hven::detail::testing::IpmDenseSparseFallbackOverride;
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
    hven::StopReason stop_reason = hven::StopReason::kNone;
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

// Which singular policy the IPM's dense arm runs under. The production policy is kReport (M8
// DN-3b); the tests written before it pin routes through the platform session, which only
// kSparseFallback reaches (FactorizeFaultInjector fails it there), so they keep that policy through
// the IPM's own seam (hven/detail/drivers/ipm_dense_testing.h).
enum class SingularPolicy { kSparseFallback, kReport };

struct PolicyScope {
    explicit PolicyScope(SingularPolicy p) {
        IpmDenseSparseFallbackOverride::active = p == SingularPolicy::kSparseFallback;
    }
    ~PolicyScope() { IpmDenseSparseFallbackOverride::active = false; }
    PolicyScope(const PolicyScope &) = delete;
    PolicyScope &operator=(const PolicyScope &) = delete;
};

RecordedSolve record_solve(const hven::IpmOptions &o, std::shared_ptr<hven::Nlp> nlp,
                           const Eigen::VectorXd &x0, const EventHook &on_event = {},
                           SingularPolicy policy = SingularPolicy::kSparseFallback) {
    const PolicyScope scope(policy);
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
    out.stop_reason = out.result.stop_reason;
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
    EXPECT_EQ(run.stop_reason, hven::StopReason::kNone);
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

// The engine's acceptable-tier test on one row (ipm_solver.cpp's
// interior_point_iterate_acceptable), for the preconditions of the restoration tests: their
// fixtures are meaningful only if the row the solve ends on would pass it.
bool row_acceptable(const hven::IpmIterate &row, const hven::IpmOptions &o) {
    return row.kkt_inf < o.termination.acceptable_kkt_tol &&
           row.econ_inf < o.termination.acceptable_equality_tol &&
           row.icon_inf < o.termination.acceptable_inequality_tol &&
           row.barr_inf < o.termination.acceptable_barrier_tol;
}

// The iteration a no-numerics message names ("... at iteration <i> ..."), or -1.
int named_iteration(const std::string &message) {
    const std::size_t at = message.find(" at iteration ");
    int i = -1;
    if (at == std::string::npos ||
        std::sscanf(message.c_str() + at + 14, "%d", &i) != 1) { // NOLINT(cert-err34-c)
        return -1;
    }
    return i;
}

// Where IpmDenseGuardSeam.AFailedGuardReRunEndsTheSolveWithoutItsStep pins the route, located the
// same way (restoration off): the first family member and pass whose injected run ends at the
// step guard's failed re-run, and the iteration that re-run belonged to.
struct GuardRoute {
    double scale = 0.0;
    int pass = 0;
    int iteration = -1;
};

std::optional<GuardRoute> find_guard_route() {
    for (const double scale : scale_family()) {
        const hven::IpmResult clean = solve_dense(scale).result;
        if (!success(clean.status) || clean.kkt_factor_counters.dense_fallback_count != 0 ||
            clean.dense_residual_ratio_triggers == 0) {
            continue;
        }
        const int attempts = static_cast<int>(clean.kkt_factor_counters.dense_factorize_count);
        for (int pass = 0; pass < attempts; ++pass) {
            RecordedSolve run;
            {
                const Armed armed(pass);
                run = solve_dense(scale);
            }
            if (run.result.message.find("dense step guard's refactorization") !=
                std::string::npos) {
                return GuardRoute{scale, pass, named_iteration(run.result.message)};
            }
        }
    }
    return std::nullopt;
}

// HS071 whose model refuses (EvaluationError) every evaluation away from `safe` once `refusing` is
// set: the line search's trial points are refused while the committed point -- which the
// iteration callback hands over as `safe` -- still evaluates. A model that cannot be evaluated at
// any trial step is the un-evaluable dispatch's real scenario.
class TrialRefusingHs071 final : public hven::Nlp {
  public:
    bool refusing = false;
    Eigen::VectorXd safe;

    hven::NlpDimensions dimensions() const override { return inner_.dimensions(); }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        inner_.bounds(xl, xu, gl, gu);
    }
    double eval_f(hven::ConstVecRef x) const override {
        check(x);
        return inner_.eval_f(x);
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        check(x);
        inner_.eval_grad_f(x, g);
    }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        check(x);
        inner_.eval_g(x, g);
    }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        inner_.jacobian_structure(r, c);
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        inner_.hessian_structure(r, c);
    }
    void eval_jacobian(hven::ConstVecRef x, hven::VecRef v) const override {
        check(x);
        inner_.eval_jacobian(x, v);
    }
    void eval_hessian(hven::ConstVecRef x, double obj_factor, hven::ConstVecRef lambda,
                      hven::VecRef v) const override {
        check(x);
        inner_.eval_hessian(x, obj_factor, lambda, v);
    }
    std::string name() const override { return "TrialRefusingHs071"; }

  private:
    void check(hven::ConstVecRef x) const {
        if (refusing && (safe.size() != x.size() || x != safe)) {
            throw hven::EvaluationError("trial point refused");
        }
    }
    hven_drivers_tests::Hs071Problem inner_;
};

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
            EXPECT_EQ(run.stop_reason, hven::StopReason::kNone);
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
    EXPECT_EQ(run.stop_reason, hven::StopReason::kNone);
    EXPECT_NE(
        r.message.find("stopped at an acceptable iterate: the KKT factorization at iteration "),
        std::string::npos)
        << r.message;
    EXPECT_NE(r.message.find(" left no numerics"), std::string::npos) << r.message;
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
    EXPECT_EQ(run.stop_reason, hven::StopReason::kNone);
    EXPECT_TRUE(r.returned_best_iterate);
    ASSERT_EQ(r.x.size(), 4);
    EXPECT_TRUE(r.x.allFinite());
}

// Restoration configured: the numeric refusal at the first iteration enters it, with no step and
// no throw. The injectors stay armed, so every later factorization -- restoration's own -- leaves
// no numerics too. Under the nested strategy the next refusal re-centres the elastic pairs (its
// one-shot budget) and the one after finds nothing left; under the proximal one the first refusal
// inside restoration finds nothing left. Either way the solve ends kNumericalError: entry, (re-
// centre,) abort, one iteration each, counted in the recovery-depth histogram's restoration and
// unresolved buckets.
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
    EXPECT_EQ(run.stop_reason, hven::StopReason::kNone);
    EXPECT_NE(r.message.find("(numerical issue)"), std::string::npos) << r.message;
    EXPECT_GT(run.event_x.size(), 1U) << "the solve continued past the iteration that entered it";
    EXPECT_TRUE(r.x.allFinite());
    const bool nested = GetParam() == hven::IpmRestorationMode::kL1Nested;
    // The entry (and the nested re-centre) resolved into the restoration bucket; the abort did not.
    using hven::detail::kRecoveryDepthRestoration;
    using hven::detail::kRecoveryDepthUnresolved;
    EXPECT_EQ(r.recovery_depth_histogram[kRecoveryDepthRestoration], nested ? 2 : 1);
    EXPECT_EQ(r.recovery_depth_histogram[kRecoveryDepthUnresolved], 1);
    // The entry at iteration 0, the nested re-centre at 1, the abort at the next.
    const int abort_iteration = nested ? 2 : 1;
    EXPECT_EQ(named_iteration(r.message), abort_iteration) << r.message;
    EXPECT_EQ(run.rows.size(), static_cast<std::size_t>(abort_iteration + 1));
}

// The order: at an acceptable-tier iterate outside restoration the refusal stops there --
// kAcceptable, the reason in the message, x unmoved -- and restoration, configured and
// entry-permitted (the start is far from feasible), is never entered. The acceptable tolerances
// are opened up so that the start itself is at the tier; converge_check's own acceptable verdict
// needs `acceptable_iterations` such rows, so nothing else stops the solve at iteration 0.
TEST_P(IpmNoNumericsRestorationSeam, AnAcceptableIterateStopsBeforeRestorationIsEntered) {
    hven::IpmOptions o = hs071_options(hven::IpmBarrierMode::kLoqo);
    o.advanced.restoration_mode = GetParam();
    o.termination.acceptable_kkt_tol = 1e6;
    o.termination.acceptable_equality_tol = 1e6;
    o.termination.acceptable_inequality_tol = 1e6;
    o.termination.acceptable_barrier_tol = 1e6;
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = solve_hs071_dense(o));
    }
    const hven::IpmResult &r = run.result;
    ASSERT_FALSE(run.rows.empty());
    EXPECT_GT(run.rows.front().econ_inf, o.common.feasibility_tol)
        << "the start must be one restoration would admit";
    EXPECT_EQ(r.status, hven::SolveStatus::kAcceptable) << r.message;
    EXPECT_EQ(run.stop_reason, hven::StopReason::kNone);
    EXPECT_NE(r.message.find("stopped at an acceptable iterate: the KKT factorization at iteration "
                             "0 left no numerics"),
              std::string::npos)
        << r.message;
    EXPECT_EQ(r.last_feasibility_restoration_entries.value_or(0), 0);
    EXPECT_EQ(r.recovery_depth_histogram[hven::detail::kRecoveryDepthRestoration], 0);
    EXPECT_EQ(run.rows.size(), 1U);
    expect_x_unmoved(run);
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
    EXPECT_EQ(run.stop_reason, hven::StopReason::kNone);
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

// Inside restoration a row's residuals are the restoration subproblem's (here the nested phase's
// condensed elastic residual), so a row at the acceptable tier must never become an acceptable
// stop. The injectors stay armed through restoration; the acceptable tolerances are set so that
// the start is NOT at the tier (its inequality residual is 1.05e-3) while the restoration rows ARE
// (1.03e-3) -- asserted below, so the fixture fails loudly if a platform moves either row. The
// solve must end as ANumericRefusalEntersRestoration's does.
TEST(IpmNoNumericsSeam, AnAcceptableRestorationRowIsNeverAnAcceptableStop) {
    hven::IpmOptions o = hs071_options(hven::IpmBarrierMode::kLoqo);
    o.advanced.restoration_mode = hven::IpmRestorationMode::kL1Nested;
    o.termination.acceptable_kkt_tol = 1e6;
    o.termination.acceptable_equality_tol = 1e6;
    o.termination.acceptable_inequality_tol = 1.04e-3;
    o.termination.acceptable_barrier_tol = 1e6;
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = solve_hs071_dense(o));
    }
    const hven::IpmResult &r = run.result;
    ASSERT_GE(run.rows.size(), 2U) << r.message;
    ASSERT_FALSE(row_acceptable(run.rows.front(), o)) << "icon " << run.rows.front().icon_inf;
    ASSERT_TRUE(row_acceptable(run.rows.back(), o)) << "icon " << run.rows.back().icon_inf;
    EXPECT_EQ(r.last_feasibility_restoration_entries.value_or(0), 1) << r.message;
    EXPECT_EQ(r.status, hven::SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(run.stop_reason, hven::StopReason::kNone);
    EXPECT_NE(r.message.find("(numerical issue)"), std::string::npos) << r.message;
}

// The same hole in the un-evaluable dispatch (pre-existing): a model that refuses every trial
// point once restoration has run for a while. The refusal at iteration 0 enters nested
// restoration (the injectors are disarmed from the next event on, so every later factorization is
// a real one); from iteration 2 every trial point is refused. Outside restoration an un-evaluable
// iteration enters restoration from a row short of the tier; inside it the first re-centres the
// elastics and the next finds nothing left. The row the solve ends on is at the acceptable tier
// under the tolerance chosen here (stationarity 2.2 against the start's 14.8 -- asserted, as
// above), and the solve must still end kNumericalError (kEvaluationFailure), never kAcceptable.
TEST(IpmNoNumericsSeam, AnUnevaluableStepInsideRestorationIsNeverAnAcceptableStop) {
    hven::IpmOptions o = hs071_options(hven::IpmBarrierMode::kLoqo);
    o.advanced.restoration_mode = hven::IpmRestorationMode::kL1Nested;
    o.termination.acceptable_kkt_tol = 5.0;
    o.termination.acceptable_equality_tol = 1e6;
    o.termination.acceptable_inequality_tol = 1e6;
    o.termination.acceptable_barrier_tol = 1e6;
    auto nlp = std::make_shared<TrialRefusingHs071>();
    TrialRefusingHs071 *model = nlp.get();
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = record_solve(o, nlp, hven_drivers_tests::hs071_start(),
                                           [model](const hven::IterationEvent &e) {
                                               if (e.iteration >= 1) {
                                                   disarm();
                                               }
                                               if (e.iteration >= 2) {
                                                   model->safe = e.x;
                                                   model->refusing = true;
                                               }
                                           }));
    }
    const hven::IpmResult &r = run.result;
    ASSERT_GE(run.rows.size(), 2U) << r.message;
    ASSERT_FALSE(row_acceptable(run.rows.front(), o)) << "kkt " << run.rows.front().kkt_inf;
    ASSERT_TRUE(row_acceptable(run.rows.back(), o)) << "kkt " << run.rows.back().kkt_inf;
    EXPECT_GE(r.last_feasibility_restoration_entries.value_or(0), 1) << r.message;
    EXPECT_EQ(r.status, hven::SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(run.stop_reason, hven::StopReason::kEvaluationFailure);
    EXPECT_NE(r.message.find("could not be evaluated at any trial step"), std::string::npos)
        << r.message;
    EXPECT_NE(r.message.find("already active"), std::string::npos) << r.message;
}

// The guard route on a CONTINUING solve: restoration (proximal) configured, so the guard's failed
// re-run enters restoration instead of ending the solve. The step the guard rejected must be
// discarded there too: x at the next iteration is the x the entering iteration started from. The
// route is located as AFailedGuardReRunEndsTheSolveWithoutItsStep locates it (restoration off);
// the proximal strategy is dead until entered, so the same pass reaches the same re-run with it
// configured (asserted: the clean solves factorize alike). The injectors stay armed, so the solve
// then ends inside restoration (kNumericalError).
TEST(IpmDenseGuardSeam, AFailedGuardReRunUnderRestorationDiscardsItsStep) {
    const std::optional<GuardRoute> route = find_guard_route();
    ASSERT_TRUE(route.has_value()) << "no member of the family reached the guard's failed re-run";
    ASSERT_GE(route->iteration, 0);
    SCOPED_TRACE(fmt::format("scale {:.2f}, pass {}, iteration {}", route->scale, route->pass,
                             route->iteration));
    hven::IpmOptions o = dense_options();
    o.advanced.restoration_mode = hven::IpmRestorationMode::kProximalSwitch;
    Eigen::VectorXd x0(2);
    x0 << 3.0, -1.0;
    const RecordedSolve clean_off = solve_dense(route->scale);
    const RecordedSolve clean_on =
        record_solve(o, std::make_shared<NearDuplicateRows>(route->scale), x0);
    ASSERT_EQ(clean_on.result.kkt_factor_counters.dense_factorize_count,
              clean_off.result.kkt_factor_counters.dense_factorize_count);
    ASSERT_EQ(clean_on.result.last_feasibility_restoration_entries.value_or(0), 0);
    RecordedSolve run;
    {
        const Armed armed(route->pass);
        ASSERT_NO_THROW(run =
                            record_solve(o, std::make_shared<NearDuplicateRows>(route->scale), x0));
    }
    const hven::IpmResult &r = run.result;
    const auto i = static_cast<std::size_t>(route->iteration);
    EXPECT_GE(r.dense_residual_ratio_triggers, 1);
    EXPECT_EQ(r.last_feasibility_restoration_entries.value_or(0), 1) << r.message;
    EXPECT_EQ(r.recovery_depth_histogram[hven::detail::kRecoveryDepthRestoration], 1);
    ASSERT_GT(run.event_x.size(), i + 1)
        << "the solve did not continue past the entering iteration";
    EXPECT_EQ(run.event_x[i + 1], run.event_x[i])
        << "a step was taken from the iterate the entering iteration started from";
    EXPECT_EQ(r.status, hven::SolveStatus::kNumericalError) << r.message;
    EXPECT_TRUE(r.x.allFinite());
}

// THE PRODUCTION POLICY (M8 DN-3b): under DenseSingularPolicy::kReport a dense attempt read exactly
// singular is reported (kDenseSingular, a numeric refusal, no numerics) and never handed to the
// platform session; the inertia-correction ladder answers it, refactoring on the dense arm at every
// rung. The same two injectors are armed as above, but the platform session is never asked, so the
// platform injector cannot fire: every refusal below is the dense arm's own report.
namespace {

RecordedSolve solve_hs071_report(const hven::IpmOptions &o, const EventHook &on_event = {}) {
    return record_solve(o, std::make_shared<hven_drivers_tests::Hs071Problem>(),
                        hven_drivers_tests::hs071_start(), on_event, SingularPolicy::kReport);
}

void expect_no_platform_factorization(const hven::IpmResult &r) {
    EXPECT_EQ(r.kkt_factor_counters.dense_fallback_count, 0);
    EXPECT_EQ(r.kkt_factor_counters.dense_retake_count, 0);
    EXPECT_EQ(r.kkt_factor_counters.dense_shift_refactor_count, 0);
    EXPECT_EQ(r.kkt_factor_counters.factorize_count,
              r.kkt_factor_counters.dense_factorize_count +
                  r.kkt_factor_counters.dense_singular_count);
}

} // namespace

// From the first iteration's ladder on every dense attempt reads singular: the ladder climbs
// through every rung, each one a dense refactorization reported singular, and ends without
// numerics; restoration is off, so the solve ends kNumericalError at the committed iterate. No
// throw, and no factorization on the platform session.
TEST(IpmDenseReportSeam, ALadderOfSingularReportsEndsInAStatusNeverAThrowNorTheSparsePath) {
    const hven::IpmOptions o = hs071_options(hven::IpmBarrierMode::kLoqo);
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = solve_hs071_report(o));
    }
    const hven::IpmResult &r = run.result;
    EXPECT_EQ(r.status, hven::SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(run.stop_reason, hven::StopReason::kNone);
    EXPECT_NE(r.message.find("ended on a failed KKT factorization (numerical issue)"),
              std::string::npos)
        << r.message;
    EXPECT_NE(r.message.find("neither an elastic re-centre nor feasibility restoration was "
                             "available"),
              std::string::npos)
        << r.message;
    EXPECT_EQ(r.last_kkt_info, Eigen::NumericalIssue);
    EXPECT_EQ(r.kkt_factor_counters.dense_factorize_count, 1);
    // Every rung was refactored on the dense arm and reported.
    EXPECT_GE(r.kkt_factor_counters.dense_singular_count,
              o.advanced.max_inertia_correction_attempts);
    expect_no_platform_factorization(r);
    expect_x_unmoved(run);
}

// Restoration configured, the dense arm recovered once restoration has begun: the exhausted
// ladder's singular reports enter restoration (the exhausted-inertia dispatch), which carries the
// solve on to success, every factorization on the dense arm.
class IpmDenseReportRestorationSeam : public ::testing::TestWithParam<hven::IpmRestorationMode> {};

TEST_P(IpmDenseReportRestorationSeam,
       AnExhaustedLadderOfSingularReportsIsRecoveredThroughRestoration) {
    hven::IpmOptions o = hs071_options(hven::IpmBarrierMode::kLoqo);
    o.advanced.restoration_mode = GetParam();
    const RecordedSolve clean = solve_hs071_report(o);
    ASSERT_TRUE(success(clean.result.status)) << clean.result.message;
    ASSERT_EQ(clean.result.last_feasibility_restoration_entries.value_or(0), 0);
    ASSERT_EQ(clean.result.kkt_factor_counters.dense_singular_count, 0);
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = solve_hs071_report(o, [](const hven::IterationEvent &e) {
                            if (e.iteration >= 1) {
                                disarm();
                            }
                        }));
    }
    const hven::IpmResult &r = run.result;
    EXPECT_GE(r.last_feasibility_restoration_entries.value_or(0), 1) << r.message;
    EXPECT_TRUE(success(r.status)) << r.message;
    EXPECT_TRUE(r.x.allFinite());
    EXPECT_GE(r.kkt_factor_counters.dense_singular_count,
              o.advanced.max_inertia_correction_attempts);
    expect_no_platform_factorization(r);
}

INSTANTIATE_TEST_SUITE_P(BothStrategies, IpmDenseReportRestorationSeam,
                         ::testing::Values(hven::IpmRestorationMode::kProximalSwitch,
                                           hven::IpmRestorationMode::kL1Nested),
                         [](const ::testing::TestParamInfo<hven::IpmRestorationMode> &info) {
                             return info.param == hven::IpmRestorationMode::kProximalSwitch
                                        ? std::string("ProximalSwitch")
                                        : std::string("L1Nested");
                         });
