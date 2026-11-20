// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 9: watching a solve, and stopping it early.
//
//     min  100*(y - x^2)^2 + (1 - x)^2      s.t.  x^2 + y^2 <= 1
//
// Rosenbrock cut by the unit disc: the answer sits on the circle near (0.786, 0.618).
//
// Three things a GUI or a plotting front end needs, all on both engines, all through
// the same two types:
//
//   1. A live feed. set_iteration_callback() hands the callback one IterationEvent per
//      iteration: the point, the multipliers, the diagnostics. The event's vectors are
//      borrowed for the call only; snapshot(event) is the owning copy to keep or to
//      pass to another thread.
//   2. An early stop on a user criterion. The callback returns CallbackAction::kStop; the
//      solve ends kInterrupted at the event's point, and the result carries that point.
//      kStopReturnBest asks for the best iterate seen so far instead.
//   3. An external stop from another thread. A StopSource (core/stop.h) is the one thing
//      here that is safe to touch from any thread; its token rides in SolveBudget::stop.
//
// The payload mask tells the engine what the callback reads, so it prepares nothing else.

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

#include <hven/hven.h>

namespace {

using namespace hven;

struct RosenbrockDisc : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 2, .m = 1, .jacobian_nonzeros = 2, .hessian_nonzeros = 3};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
        gl << -kInfinity;
        gu << 1.0; // x^2 + y^2 <= 1
    }
    double eval_f(ConstVecRef x) const override {
        return 100.0 * std::pow(x[1] - x[0] * x[0], 2) + std::pow(1.0 - x[0], 2);
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g << -400.0 * x[0] * (x[1] - x[0] * x[0]) - 2.0 * (1.0 - x[0]),
            200.0 * (x[1] - x[0] * x[0]);
    }
    void eval_g(ConstVecRef x, VecRef g) const override { g << x.squaredNorm(); }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override { v = 2.0 * x; }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 1; // lower triangle: (0,0), (1,0), (1,1)
        c << 0, 0, 1;
    }
    void eval_hessian(ConstVecRef x, double obj_factor, ConstVecRef lambda,
                      VecRef v) const override {
        v << obj_factor * (1200.0 * x[0] * x[0] - 400.0 * x[1] + 2.0) + 2.0 * lambda[0],
            obj_factor * -400.0 * x[0], obj_factor * 200.0 + 2.0 * lambda[0];
    }
};

constexpr double kFStar = 0.0456748; // the constrained optimum

std::string show(const Eigen::Ref<const Vec> &v) {
    std::string s = "(";
    for (Eigen::Index i = 0; i < v.size(); i++) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%s%+.4f", i ? ", " : "", v[i]);
        s += buf;
    }
    return s + ")";
}

// 1. The live feed. Works unchanged on either engine: the event is the same type.
CallbackAction print_row(const IterationEvent &e) {
    // kConstraints adds `violation`, one entry per row (0 when the row is satisfied).
    const double violation = e.violation.size() ? e.violation.maxCoeff() : 0.0;
    std::printf("  iter %2lld  f=%+.6f  x=%s  lambda=%s  violation=%.1e\n",
                static_cast<long long>(e.iteration), e.f, show(e.x).c_str(), show(e.lambda).c_str(),
                violation);
    return CallbackAction::kContinue;
}

constexpr IterationPayload kFeedPayload =
    IterationPayload::kVectors | IterationPayload::kDiagnostics | IterationPayload::kConstraints;

} // namespace

int main() {
    Problem problem(std::make_shared<RosenbrockDisc>());
    const Vec x0{{-1.2, 1.0}};
    bool ok = true;

    // ---- 1. Live feed on the SQP engine ----
    std::printf("live feed (SqpSolver)\n");
    {
        SqpSolver sqp;
        sqp.set_iteration_callback(print_row, kFeedPayload);
        const SqpResult r = sqp.solve(problem, x0);
        std::printf("  -> %s, f=%.7f\n", to_string(r.status), r.f);
        ok &= r.status == SolveStatus::kOptimal && std::abs(r.f - kFStar) < 1e-5;
    }

    // ---- 2. Early stop on a user criterion, with the iterate in hand ----
    std::printf("early stop (IpmSolver): quit once f <= 0.1 and the violation is below 1e-2\n");
    {
        const Index full_solve = IpmSolver{}.solve(problem, x0).iterations;
        IpmSolver ipm;
        IterationSnapshot at_stop; // an owning copy; outlives the event
        ipm.set_iteration_callback(
            [&](const IterationEvent &e) {
                // Diagnostics arrive as NaN when not requested or not measured (a
                // restoration row), and NaN never passes a test, so the stop is safe.
                if (e.f <= 0.1 && e.feasibility < 1e-2) {
                    at_stop = snapshot(e);
                    return CallbackAction::kStop;
                }
                return CallbackAction::kContinue;
            },
            IterationPayload::kVectors | IterationPayload::kDiagnostics);
        const IpmResult r = ipm.solve(problem, x0);
        // kInterrupted, and the result's point is the event's point.
        std::printf("  -> %s (%s) at iteration %lld of %lld, f=%.6f, x=%s\n", to_string(r.status),
                    to_string(r.stop_reason), static_cast<long long>(r.iterations),
                    static_cast<long long>(full_solve), r.f, show(r.x).c_str());
        ok &= r.status == SolveStatus::kInterrupted && r.stop_reason == StopReason::kCallbackStop &&
              at_stop.x.size() == 2 && (r.x - at_stop.x).lpNorm<Eigen::Infinity>() == 0.0 &&
              r.f <= 0.1 && r.iterations < full_solve;
    }

    std::printf("early stop (SqpSolver): kStopReturnBest after 3 iterations\n");
    {
        SqpSolver sqp;
        sqp.set_iteration_callback(
            [](const IterationEvent &e) {
                return e.iteration >= 3 ? CallbackAction::kStopReturnBest
                                        : CallbackAction::kContinue;
            },
            IterationPayload::kScalars);
        const SqpResult r = sqp.solve(problem, x0);
        // The best iterate by (violation, then objective); the result says which point it is.
        std::printf("  -> %s, returned_best_iterate=%s, f=%.6f\n", to_string(r.status),
                    r.returned_best_iterate ? "yes" : "no", r.f);
        ok &= r.status == SolveStatus::kInterrupted && r.best_iterate.has_value();
    }

    // ---- 3. External stop from another thread ----
    std::printf("external stop (SqpSolver): a worker thread plays the GUI's stop button\n");
    {
        StopSource stop; // one per solve; there is no reset
        std::atomic<bool> iterate_seen{false};

        // The button. request_stop() is one lock-free store; call it from anywhere.
        std::thread button([&] {
            while (!iterate_seen.load()) {
                std::this_thread::yield();
            }
            stop.request_stop();
        });

        SqpSolver sqp;
        sqp.set_iteration_callback(
            [&](const IterationEvent &e) {
                if (e.iteration == 2) {
                    iterate_seen = true;
                    // The demo only: wait for the press so the outcome is the same every run.
                    // A real front end has nothing here.
                    const auto give_up = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                    while (!stop.stop_requested() && std::chrono::steady_clock::now() < give_up) {
                        std::this_thread::yield();
                    }
                }
                return CallbackAction::kContinue;
            },
            IterationPayload::kScalars);

        // The token is the budget's `stop`; naming SolveBudget selects the cold-start overload.
        const SqpResult r = sqp.solve(problem, x0, SolveBudget{.stop = stop.token()});
        button.join();
        std::printf("  -> %s (%s) after %lld iterations, x=%s\n", to_string(r.status),
                    to_string(r.stop_reason), static_cast<long long>(r.iterations),
                    show(r.x).c_str());
        ok &= r.status == SolveStatus::kInterrupted && r.stop_reason == StopReason::kStopRequested;
    }

    std::printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}
