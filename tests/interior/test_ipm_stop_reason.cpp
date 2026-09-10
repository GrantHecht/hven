// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The LIVE pins on InteriorPointSolver::last_stop_reason(): the two abnormal
// NOTCONVERGED doors and the stall-beats-cap tie. The mapping onto SolveStatus
// is pinned in tests/drivers/test_solve_status.cpp; the iteration-cap pin is in
// test_nlp_solver.cpp, on HS071.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <hven/drivers/interior_point_solver.h>
#include <hven/drivers/solve_result.h>
#include <hven/drivers/solve_status.h>
#include <hven/drivers/trace.h>
#include <hven/drivers/trace_writer.h>
#include <hven/model/nlp_problem.h>
#include <hven/model/nlp_solver.h>

#include "../common_support/console_capture.h" // NOLINT(build/include_subdir)

namespace stop_reason_test {
namespace {

using hven::solvers::NLPSolver;
using hven::solvers::RestorationModes;

constexpr double kStopReasonInf = std::numeric_limits<double>::infinity();

// c(x) = 1 + eps*x0 + x0^10 = 0 has no real root, so the feasibility phase can
// never succeed; the near-flat Jacobian at the start throws the unlinesearched
// Newton step out to |x0| ~ 1/eps and the pure power then walks it back by a
// tenth per iteration. That long walk back is the SUSTAINED WORSENING the stall
// detector certifies -- a violation a full window of iterations above the
// phase's own best (detail/globalization/feasibility_stall.h).
struct PowerSpikeProblem final : hven::solvers::NLPProblem {
    static constexpr double kEps = 1.0e-4;
    static constexpr int kPower = 10;

    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kStopReasonInf, -kStopReasonInf;
        xu << kStopReasonInf, kStopReasonInf;
        gl << 0.0;
        gu << 0.0;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = x[1]; }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd>,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g << 0.0, 1.0;
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 1.0 + kEps * x[0] + std::pow(x[0], kPower);
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v << kEps + kPower * std::pow(x[0], kPower - 1), 0.0;
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd> x, double,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v << lambda[0] * kPower * (kPower - 1) * std::pow(x[0], kPower - 2), 0.0;
    }
    std::string name() const override { return "PowerSpikeProblem"; }
};

// c(x) = x0^2 + x1^2 + 1 = 0 has no real solution and its violation has a
// STRICT stationary point at the origin: the shape a restoration phase converges
// to and then declares locally infeasible.
struct LocallyInfeasibleProblem final : hven::solvers::NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kStopReasonInf, -kStopReasonInf;
        xu << kStopReasonInf, kStopReasonInf;
        gl << 0.0;
        gu << 0.0;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] + x[1];
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd>,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g << 1.0, 1.0;
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[0] + x[1] * x[1] + 1.0;
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 2.0 * x[0], 2.0 * x[1];
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd>, double,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 2.0 * lambda[0], 2.0 * lambda[0];
    }
    std::string name() const override { return "LocallyInfeasibleProblem"; }
};

Eigen::VectorXd two_var_start(double a, double b) {
    Eigen::VectorXd x0(2);
    x0 << a, b;
    return x0;
}

// The stall lever set. The stall branch lives in the feasibility phase, so the
// entry is solve(); restoration must be constructed and its entry budget must
// run out with the stage no better off than where its one episode left it. The
// loosened tolerances keep that episode's exit the near-feasible one rather than
// a local-infeasibility declaration, and the divergence thresholds are lifted
// past the spike this fixture is built around.
//
// lift_div_tols is the one lever the pair of pins below differ in: with it the
// stage runs long enough to stall, without it the same fixture is DIVERGING in a
// handful of iterations. See WithDefaultDivergenceThresholdsTheSameFixtureDiverges.
NLPSolver make_stall_solver(int max_iters, bool lift_div_tols = true) {
    NLPSolver solver(std::make_shared<PowerSpikeProblem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        o.common.threads = 1;
        o.max_iters = max_iters;
        o.kkt_tol = 1.0e-8;
        o.econ_tol = 0.02;
        o.icon_tol = 0.02;
        o.bar_tol = 1.0e-8;
        o.acc_kkt_tol = 1.0e-6;
        o.acc_econ_tol = 0.2;
        o.acc_icon_tol = 0.2;
        o.acc_bar_tol = 1.0e-6;
        solver.optimizer_->set_options(std::move(o));
    }
    if (lift_div_tols) {
        auto o = solver.optimizer_->options();
        o.div_kkt_tol = 1.0e300;
        o.div_econ_tol = 1.0e300;
        o.div_icon_tol = 1.0e300;
        o.div_bar_tol = 1.0e300;
        solver.optimizer_->set_options(std::move(o));
    }
    {
        auto o = solver.optimizer_->options();
        o.restoration_mode = RestorationModes::l1_nested;
        solver.optimizer_->set_options(std::move(o));
    }
    {
        auto o = solver.optimizer_->options();
        o.max_feas_rest = 1;
        solver.optimizer_->set_options(std::move(o));
    }
    return solver;
}

// The TERMINAL LOOP INDEX of the last phase that ran, recorded through the late
// callback. IterateInfo::iter_ IS alg_impl's own loop variable, while
// result().iterations is iters.size() -- and the restoration transitions pop
// history entries, so the two counts differ by the number of transitions. Only
// the loop index answers "was the last iteration the cap iteration?".
//
// Installing the callback moves no trajectory: both invocation sites discard its
// return value, and it is handed read-only views.
struct TerminalIter {
    int last = -1;
};

void record_terminal_iter(NLPSolver &solver, TerminalIter &rec) {
    // M6 W5 T8.6: the shared iteration callback in place of the late one. The
    // TERMINAL row's index is the same number either way -- one event per
    // `ipm.iter` row, and the last event is the last row.
    solver.optimizer_->set_iteration_callback([&rec](const hven::solvers::IterationEvent &ev) {
        rec.last = static_cast<int>(ev.iteration);
        return hven::solvers::CallbackAction::kContinue;
    });
}

// The restoration-locally-infeasible lever set, on the other fixture and the
// OPTIMIZE entry: the optimality phase's own switch enters restoration and the
// subproblem converges at a still-infeasible point. Default tolerances.
NLPSolver make_locally_infeasible_solver(int max_iters) {
    NLPSolver solver(std::make_shared<LocallyInfeasibleProblem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        o.common.threads = 1;
        o.max_iters = max_iters;
        solver.optimizer_->set_options(std::move(o));
    }
    {
        auto o = solver.optimizer_->options();
        o.restoration_mode = RestorationModes::l1_nested;
        solver.optimizer_->set_options(std::move(o));
    }
    {
        auto o = solver.optimizer_->options();
        o.max_feas_rest = 1;
        solver.optimizer_->set_options(std::move(o));
    }
    return solver;
}

// A sink that counts `ipm.iter` lines and nothing else (M6 W5 T8.6 fix1).
class IterCountingSink : public hven::solvers::TraceSink {
  public:
    hven::Index rows = 0;
    void on_ipm_iter(const hven::solvers::IpmIterTraceEvent &) override { ++rows; }
    void on_ipqp_iter(const hven::solvers::IpqpTraceIterEvent &) override {}
    void on_ipqp_reg(const hven::solvers::IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const hven::solvers::IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const hven::solvers::IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const hven::solvers::IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const hven::solvers::IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const hven::solvers::QpModeTraceEvent &) override {}
    void on_fallback_verdict(const hven::solvers::SqpFallbackVerdictTraceEvent &) override {}
};

/// Splits a JSON-lines stream and reads one raw key off a line (M6 W5 T8.7b).
/// The same two helpers `TheDoorsMarkerFollowsItsOwnIterLineOnALiveSolve`
/// builds inline; hoisted so the T8.7b pins below can share them.
std::vector<std::string> stream_lines(const std::string &stream) {
    std::vector<std::string> lines;
    std::size_t pos = 0;
    while (pos < stream.size()) {
        const std::size_t eol = stream.find('\n', pos);
        const std::size_t end = (eol == std::string::npos) ? stream.size() : eol;
        lines.push_back(stream.substr(pos, end - pos));
        if (eol == std::string::npos) {
            break;
        }
        pos = eol + 1;
    }
    return lines;
}

std::string key_value(const std::string &line, const std::string &key) {
    const std::size_t k = line.find("\"" + key + "\":");
    if (k == std::string::npos) {
        return std::string();
    }
    const std::size_t v = k + key.size() + 3;
    const std::size_t e = line.find_first_of(",}", v);
    return line.substr(v, e - v);
}

std::vector<std::string> lines_named(const std::string &stream, const std::string &ev) {
    std::vector<std::string> out;
    for (const std::string &l : stream_lines(stream)) {
        if (key_value(l, "ev") == "\"" + ev + "\"") {
            out.push_back(l);
        }
    }
    return out;
}

} // namespace
} // namespace stop_reason_test

TEST(IpmStopReason, NoSolveHasHappenedYet) {
    // kNone is not "we did not look": it is the engine saying no labelled door
    // was taken, so the verdict itself explains the exit.
    auto solver = stop_reason_test::make_locally_infeasible_solver(200);
    EXPECT_EQ(solver.optimizer_->last_stop_reason(), hven::solvers::IpmStopReason::kNone);
}

TEST(IpmStopReason, AStalledFeasibilityStageIsRecordedAsSuch) {
    auto solver = stop_reason_test::make_stall_solver(600);
    stop_reason_test::TerminalIter term;
    stop_reason_test::record_terminal_iter(solver, term);
    const hven::solvers::SolveStatus flag = solver.solve(stop_reason_test::two_var_start(0.0, 0.0));
    // THE ENGINE RESOLVES THE SPLIT ITSELF NOW (M6 W5 T8.4): the phase's raw
    // "ran out of iterations with nothing better to say" verdict is reconciled
    // against the stop reason at the phase's own exit, so the reported status IS
    // kStalled rather than a kMaxIter a caller has to reinterpret.
    EXPECT_EQ(flag, hven::solvers::SolveStatus::kStalled);
    EXPECT_EQ(solver.optimizer_->last_stop_reason(), hven::solvers::IpmStopReason::kStageStalled);
    // Well inside the iteration budget, asserted on the LOOP INDEX: the phase's
    // last iteration was not the cap iteration, so this is not the cap wearing
    // another label. result().iterations would not answer that question -- it
    // counts history entries, which the restoration transitions pop.
    EXPECT_LT(term.last, 600 - 1);
    // And the resolution is IDEMPOTENT: applying it again to an already-resolved
    // status returns it unchanged.
    EXPECT_EQ(hven::solvers::resolve_ipm_phase_status(flag, solver.optimizer_->last_stop_reason()),
              hven::solvers::SolveStatus::kStalled);
}

TEST(IpmStopReason, WithDefaultDivergenceThresholdsTheSameFixtureDiverges) {
    // The IDENTICAL lever set minus set_div_tols. At the defaults
    // (div_*_tol_ = 1e15, three consecutive iterates) the 1e40 spike this
    // fixture is built around is read as divergence within a few iterations --
    // far short of the fifty-iteration window the stall detector needs. So the
    // stall exit is demonstrated reachable ONLY with divergence detection
    // lifted, and whether any problem reaches it at the default thresholds is
    // undemonstrated. This also pins the DIVERGING row of "the reason never
    // contradicts the verdict": a verdict-carrying exit records no reason.
    auto solver = stop_reason_test::make_stall_solver(600, /*lift_div_tols=*/false);
    stop_reason_test::TerminalIter term;
    stop_reason_test::record_terminal_iter(solver, term);
    const hven::solvers::SolveStatus flag = solver.solve(stop_reason_test::two_var_start(0.0, 0.0));
    EXPECT_EQ(flag, hven::solvers::SolveStatus::kDiverging);
    EXPECT_EQ(solver.optimizer_->last_stop_reason(), hven::solvers::IpmStopReason::kNone);
    // Inside the detector's own window (kFeasStallWindow = 50), which is what
    // makes this the reason the stall is out of reach here.
    EXPECT_LT(term.last, 50);
    EXPECT_EQ(hven::solvers::resolve_ipm_phase_status(flag, solver.optimizer_->last_stop_reason()),
              hven::solvers::SolveStatus::kDiverging);
}

TEST(IpmStopReason, ARestorationLocalInfeasibilityIsRecordedAsSuch) {
    auto solver = stop_reason_test::make_locally_infeasible_solver(200);
    const hven::solvers::SolveStatus flag =
        solver.optimize(stop_reason_test::two_var_start(1.0, 1.0));
    // Resolved by the engine at the phase's exit (M6 W5 T8.4), like the stall
    // above: the locally-infeasible restoration return is the OTHER exit the
    // old vocabulary could not tell from the cap.
    EXPECT_EQ(flag, hven::solvers::SolveStatus::kStalled);
    EXPECT_EQ(solver.optimizer_->last_stop_reason(),
              hven::solvers::IpmStopReason::kRestorationLocallyInfeasible);
    EXPECT_LT(solver.result().iterations, 200);
    // And the resolution is IDEMPOTENT: applying it again to an already-resolved
    // status returns it unchanged.
    EXPECT_EQ(hven::solvers::resolve_ipm_phase_status(flag, solver.optimizer_->last_stop_reason()),
              hven::solvers::SolveStatus::kStalled);
}

// THE LOCALLY-INFEASIBLE DOOR IS ON BOTH STREAMS (M6 W5 T8.6 fix1, the SQP
// lane's I2). The twin of `Callback.IpmTerminalRowStillFires`, on the exit that
// pin's HS071 fixture never takes.
//
// This door KEEPS the record pushed at the top of its iteration -- it neither
// pops it nor continues -- so `result.iterations` counts it. Until fix1 it
// emitted no `ipm.iter` line for that row and fired no iteration event, so the
// trace was one row short of the iterations the result reported and a caller
// could not observe, let alone stop at, the point the phase actually returns.
// The leg's own counting instrument is what found it:
// `infeas2_stationary/MakeParameter/resto_infeasible` read `events=24` beside
// `iter_num=25`, alone among the 41 rows.
TEST(IpmStopReason, TheLocallyInfeasibleDoorFiresItsRowOnBothStreams) {
    auto solver = stop_reason_test::make_locally_infeasible_solver(200);

    stop_reason_test::IterCountingSink sink;
    solver.optimizer_->attach_trace(&sink);
    std::vector<Eigen::VectorXd> points;
    hven::Index events = 0;
    solver.optimizer_->set_iteration_callback([&](const hven::solvers::IterationEvent &ev) {
        ++events;
        points.emplace_back(ev.x);
        return hven::solvers::CallbackAction::kContinue;
    });

    const hven::solvers::SolveStatus flag =
        solver.optimize(stop_reason_test::two_var_start(1.0, 1.0));
    ASSERT_EQ(flag, hven::solvers::SolveStatus::kStalled);
    ASSERT_EQ(solver.optimizer_->last_stop_reason(),
              hven::solvers::IpmStopReason::kRestorationLocallyInfeasible)
        << "fixture premise: the solve must leave through the locally-infeasible door";

    const hven::solvers::IpmResult &r = solver.result();
    ASSERT_GT(r.iterations, 1);
    // ONE EVENT PER `ipm.iter` ROW PER COUNTED ITERATION, all three equal.
    EXPECT_EQ(events, sink.rows);
    EXPECT_EQ(events, r.iterations);

    // AND THE LAST EVENT DESCRIBES THE POINT THE PHASE RETURNS, bit for bit --
    // the door fires its terminal dispatch below the return_best substitution,
    // so this holds whichever iterate the solve chose to hand back.
    ASSERT_FALSE(points.empty());
    ASSERT_EQ(points.back().size(), r.x.size());
    for (Eigen::Index i = 0; i < r.x.size(); ++i) {
        EXPECT_EQ(points.back()[i], r.x[i]) << "at coordinate " << i;
    }
}

// THE DOOR'S MARKER, DRIVEN BY THE SOLVER (M6 W5 T8.7 fix1, astra's Minor).
//
// `JsonLinesTraceSink.TheRestorationExitRowIsAdjacentToItsOwnIterLine` calls
// the two writer methods BY HAND, in the order the solver is supposed to use;
// it therefore pins the writer and says nothing about the solver. Removing the
// marker emit, or moving it above its row, leaves that test green. This one
// runs the solve that actually opens the door and reads the stream it produced:
// exactly one `ipm.restoration_exit_row` line, immediately after an `ipm.iter`
// line, with the same `iter` and `phase` on both, and the two numbers that
// decided the exit on the marker.
TEST(IpmStopReason, TheDoorsMarkerFollowsItsOwnIterLineOnALiveSolve) {
    auto solver = stop_reason_test::make_locally_infeasible_solver(200);
    std::ostringstream os;
    hven::solvers::JsonLinesTraceSink sink(os);
    solver.optimizer_->attach_trace(&sink);

    const hven::solvers::SolveStatus flag =
        solver.optimize(stop_reason_test::two_var_start(1.0, 1.0));
    ASSERT_EQ(flag, hven::solvers::SolveStatus::kStalled);
    ASSERT_EQ(solver.optimizer_->last_stop_reason(),
              hven::solvers::IpmStopReason::kRestorationLocallyInfeasible)
        << "fixture premise: the solve must leave through the locally-infeasible door";

    std::vector<std::string> lines;
    {
        const std::string stream = os.str();
        std::size_t pos = 0;
        while (pos < stream.size()) {
            const std::size_t eol = stream.find('\n', pos);
            const std::size_t end = (eol == std::string::npos) ? stream.size() : eol;
            lines.push_back(stream.substr(pos, end - pos));
            if (eol == std::string::npos) {
                break;
            }
            pos = eol + 1;
        }
    }
    auto key_of = [](const std::string &line, const std::string &key) {
        const std::size_t k = line.find("\"" + key + "\":");
        if (k == std::string::npos) {
            return std::string();
        }
        const std::size_t v = k + key.size() + 3;
        const std::size_t e = line.find_first_of(",}", v);
        return line.substr(v, e - v);
    };

    std::vector<std::size_t> markers;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (key_of(lines[i], "ev") == "\"ipm.restoration_exit_row\"") {
            markers.push_back(i);
        }
    }
    ASSERT_EQ(markers.size(), 1u) << "the door is taken once, and marks its row once";
    const std::size_t m = markers.front();
    ASSERT_GT(m, 0u) << "the marker cannot be the stream's first line";
    EXPECT_EQ(key_of(lines[m - 1], "ev"), "\"ipm.iter\"")
        << "the marker must FOLLOW the row it refers to: " << lines[m - 1];
    EXPECT_EQ(key_of(lines[m], "iter"), key_of(lines[m - 1], "iter"));
    EXPECT_EQ(key_of(lines[m], "phase"), key_of(lines[m - 1], "phase"));
    // The two numbers the door decided on are real readings, not defaults.
    EXPECT_GT(std::stod(key_of(lines[m], "theta")), 0.0);
    EXPECT_GT(std::stod(key_of(lines[m], "threshold")), 0.0);
    // ... and the marker is the LAST `ipm.iter`-bearing row of the stream: the
    // door returns from the phase there.
    for (std::size_t i = m + 1; i < lines.size(); ++i) {
        EXPECT_NE(key_of(lines[i], "ev"), "\"ipm.iter\"")
            << "a row after the door's marker: " << lines[i];
    }
}

TEST(IpmStopReason, AStallOnTheCapIterationKeepsTheStallLabel) {
    // The tie's cap is derived, not hard-coded: raise the cap one at a time from
    // the reported iteration count until the phase reaches the stall again. That
    // first cap is the one whose last loop iteration is BOTH the stall iteration
    // and the cap iteration, so both disjuncts of the terminal conjunction hold.
    //
    // The search is bounded: 21 candidate caps, so at most 22 solves counting
    // the uncapped pilot. The window is safe because result().iterations is the
    // history size and the stall iteration's LOOP INDEX is that plus one entry
    // per restoration transition (at most two, with max_feas_rest = 1).
    //
    // And the tie is pinned on the LOOP INDEX, not merely on the label: an
    // implementation that let the cap win the tie would report kIterationCap at
    // the real tie cap, and the search would then find kStageStalled one cap
    // LATER, where the stall fires strictly before the cap. The
    // terminal-index == cap - 1 assertion is what refuses that: it requires both
    // disjuncts to hold on ONE iteration.
    auto unbounded = stop_reason_test::make_stall_solver(600);
    (void)unbounded.solve(stop_reason_test::two_var_start(0.0, 0.0));
    ASSERT_EQ(unbounded.optimizer_->last_stop_reason(),
              hven::solvers::IpmStopReason::kStageStalled);
    const int reported = unbounded.result().iterations;

    int tie_cap = -1;
    int tie_terminal_iter = -1;
    for (int cap = reported; cap <= reported + 20 && tie_cap < 0; ++cap) {
        auto solver = stop_reason_test::make_stall_solver(cap);
        stop_reason_test::TerminalIter term;
        stop_reason_test::record_terminal_iter(solver, term);
        (void)solver.solve(stop_reason_test::two_var_start(0.0, 0.0));
        const hven::solvers::IpmStopReason reason = solver.optimizer_->last_stop_reason();
        if (reason == hven::solvers::IpmStopReason::kStageStalled) {
            tie_cap = cap;
            tie_terminal_iter = term.last;
        } else {
            EXPECT_EQ(reason, hven::solvers::IpmStopReason::kIterationCap) << "cap " << cap;
        }
    }
    ASSERT_GT(tie_cap, 0) << "no cap in [" << reported << ", " << reported + 20
                          << "] made the stall iteration the cap iteration";
    EXPECT_EQ(tie_terminal_iter, tie_cap - 1)
        << "the stall was labelled at cap " << tie_cap << ", but its last loop iteration was "
        << tie_terminal_iter << ", not the cap iteration " << tie_cap - 1;
}

// ===========================================================================
// M6 W5 T8.7b -- THE THREE DOORS' VERDICT LINE, AND THEIR MESSAGES.
//
// The exit VERDICT the console prints used to key on `alg_impl`'s RAW exit
// code, taken before `resolve_ipm_phase_status` ran; `ipm.phase.exit` carries
// the RESOLVED status instead. Resolution only ever rewrites kMaxIter -- into
// kStalled or kInterrupted -- and the console prints `No Solution Found` for
// all three, so the bytes do not move. These pin that on the doors where the
// two keys actually differ, which is the only place the change is observable.
// ===========================================================================

TEST(IpmStopReason, TheStalledDoorStillPrintsNoSolutionFoundAndCarriesItsMessage) {
    auto solver = stop_reason_test::make_stall_solver(200);
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 0;
        solver.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    hven::solvers::JsonLinesTraceSink sink(os);
    solver.optimizer_->attach_trace(&sink);

    std::string printed;
    {
        hven::testing::StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        const hven::solvers::SolveStatus flag =
            solver.solve(stop_reason_test::two_var_start(0.0, 0.0));
        ASSERT_EQ(flag, hven::solvers::SolveStatus::kStalled)
            << "fixture premise: this cell must leave through the stall door";
        printed = capture.text();
    }
    ASSERT_EQ(solver.optimizer_->last_stop_reason(), hven::solvers::IpmStopReason::kStageStalled);

    // THE VERDICT LINE IS UNCHANGED. The raw code at this door is kMaxIter and
    // the resolved status is kStalled; both take the `No Solution Found`
    // branch, which is what makes the key change byte-neutral.
    EXPECT_NE(printed.find("No Solution Found"), std::string::npos) << printed;
    EXPECT_EQ(printed.find("Optimal Solution Found"), std::string::npos);
    EXPECT_EQ(printed.find("Acceptable Solution Found"), std::string::npos);

    // ... and the event carries the resolved status, which is the ONE key.
    const std::vector<std::string> exits =
        stop_reason_test::lines_named(os.str(), "ipm.phase.exit");
    ASSERT_FALSE(exits.empty());
    EXPECT_EQ(stop_reason_test::key_value(exits.back(), "status"), "\"stalled\"");
    EXPECT_EQ(stop_reason_test::key_value(exits.back(), "stop_reason"), "\"stage_stalled\"");

    // THE STALL WARNING IS AN EVENT NOW, with its two infeasibilities on it,
    // and the console still prints its sentence.
    const std::vector<std::string> messages =
        stop_reason_test::lines_named(os.str(), "ipm.message");
    hven::Index stalls = 0;
    for (const std::string &m : messages) {
        if (stop_reason_test::key_value(m, "kind") == "\"feasibility_stall\"") {
            ++stalls;
            EXPECT_GT(std::stod(stop_reason_test::key_value(m, "a")), 0.0);
            EXPECT_NE(stop_reason_test::key_value(m, "b"), "null");
            // The slots this kind does not use are absences, not zeros.
            EXPECT_EQ(stop_reason_test::key_value(m, "k"), "null");
            EXPECT_EQ(stop_reason_test::key_value(m, "p"), "null");
        }
    }
    EXPECT_GT(stalls, 0) << "premise: the stall door writes its message";
    EXPECT_NE(printed.find("Feasibility phase stalled"), std::string::npos);
}

TEST(IpmStopReason, TheLocallyInfeasibleDoorStillPrintsNoSolutionFoundAndCarriesItsMessage) {
    auto solver = stop_reason_test::make_locally_infeasible_solver(200);
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 0;
        solver.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    hven::solvers::JsonLinesTraceSink sink(os);
    solver.optimizer_->attach_trace(&sink);

    std::string printed;
    {
        hven::testing::StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        const hven::solvers::SolveStatus flag =
            solver.optimize(stop_reason_test::two_var_start(1.0, 1.0));
        ASSERT_EQ(flag, hven::solvers::SolveStatus::kStalled);
        printed = capture.text();
    }
    ASSERT_EQ(solver.optimizer_->last_stop_reason(),
              hven::solvers::IpmStopReason::kRestorationLocallyInfeasible);

    EXPECT_NE(printed.find("No Solution Found"), std::string::npos) << printed;
    const std::vector<std::string> exits =
        stop_reason_test::lines_named(os.str(), "ipm.phase.exit");
    ASSERT_FALSE(exits.empty());
    EXPECT_EQ(stop_reason_test::key_value(exits.back(), "status"), "\"stalled\"");
    EXPECT_EQ(stop_reason_test::key_value(exits.back(), "stop_reason"),
              "\"restoration_locally_infeasible\"");

    // THE DOOR'S MESSAGE AND THE DOOR'S MARKER CARRY THE SAME TWO NUMBERS --
    // the marker was T8.7's, the message is T8.7b's, and a reader that has one
    // must be able to reconcile it with the other.
    const std::vector<std::string> markers =
        stop_reason_test::lines_named(os.str(), "ipm.restoration_exit_row");
    ASSERT_EQ(markers.size(), 1u);
    std::vector<std::string> door_messages;
    for (const std::string &m : stop_reason_test::lines_named(os.str(), "ipm.message")) {
        if (stop_reason_test::key_value(m, "kind") == "\"restoration_locally_infeasible\"") {
            door_messages.push_back(m);
        }
    }
    ASSERT_EQ(door_messages.size(), 1u);
    EXPECT_EQ(stop_reason_test::key_value(door_messages[0], "a"),
              stop_reason_test::key_value(markers[0], "theta"));
    EXPECT_EQ(stop_reason_test::key_value(door_messages[0], "b"),
              stop_reason_test::key_value(markers[0], "threshold"));
    EXPECT_NE(printed.find("locally infeasible"), std::string::npos);
}

TEST(IpmStopReason, TheInterruptDoorStillPrintsNoSolutionFoundAndCarriesItsMessage) {
    auto solver = stop_reason_test::make_locally_infeasible_solver(200);
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 0;
        solver.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    hven::solvers::JsonLinesTraceSink sink(os);
    solver.optimizer_->attach_trace(&sink);
    hven::Index seen = 0;
    solver.optimizer_->set_iteration_callback([&seen](const hven::solvers::IterationEvent &) {
        ++seen;
        return (seen >= 3) ? hven::solvers::CallbackAction::kStop
                           : hven::solvers::CallbackAction::kContinue;
    });

    std::string printed;
    {
        hven::testing::StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        const hven::solvers::SolveStatus flag =
            solver.optimize(stop_reason_test::two_var_start(1.0, 1.0));
        ASSERT_EQ(flag, hven::solvers::SolveStatus::kInterrupted);
        printed = capture.text();
    }
    EXPECT_NE(printed.find("No Solution Found"), std::string::npos) << printed;
    const std::vector<std::string> exits =
        stop_reason_test::lines_named(os.str(), "ipm.phase.exit");
    ASSERT_FALSE(exits.empty());
    EXPECT_EQ(stop_reason_test::key_value(exits.back(), "status"), "\"interrupted\"");
    EXPECT_EQ(stop_reason_test::key_value(exits.back(), "stop_reason"), "\"interrupted\"");

    // The interrupt's own message, with the row it stopped on.
    std::vector<std::string> stops;
    for (const std::string &m : stop_reason_test::lines_named(os.str(), "ipm.message")) {
        if (stop_reason_test::key_value(m, "kind") == "\"interrupt_at_iteration\"") {
            stops.push_back(m);
        }
    }
    ASSERT_EQ(stops.size(), 1u);
    EXPECT_EQ(stop_reason_test::key_value(stops[0], "iter"),
              stop_reason_test::key_value(exits.back(), "iter"));
    EXPECT_EQ(stop_reason_test::key_value(stops[0], "a"), "null");
    EXPECT_NE(printed.find("Solve interrupted by the iteration callback at iteration"),
              std::string::npos);
}

TEST(IpmStopReason, OnlyResolutionEverProducesStalledOrInterrupted) {
    // THE INVERSE OF THE VERDICT ARGUMENT, and what makes it sound: the console
    // may print `No Solution Found` for kStalled and kInterrupted only because
    // `converge_check` never returns either -- resolution is their sole source,
    // and it fires only from kMaxIter with a labelled stop reason. A phase that
    // reported one of the two with `stop_reason == none` would falsify that.
    auto run = [](int which) {
        std::ostringstream os;
        hven::solvers::JsonLinesTraceSink sink(os);
        if (which == 0) {
            auto solver = stop_reason_test::make_stall_solver(200);
            solver.optimizer_->attach_trace(&sink);
            solver.solve(stop_reason_test::two_var_start(0.0, 0.0));
        } else {
            auto solver = stop_reason_test::make_locally_infeasible_solver(200);
            solver.optimizer_->attach_trace(&sink);
            solver.optimize(stop_reason_test::two_var_start(1.0, 1.0));
        }
        return os.str();
    };
    for (int which = 0; which < 2; ++which) {
        const std::string stream = run(which);
        const std::vector<std::string> exits =
            stop_reason_test::lines_named(stream, "ipm.phase.exit");
        ASSERT_FALSE(exits.empty()) << "arm " << which;
        for (const std::string &e : exits) {
            const std::string status = stop_reason_test::key_value(e, "status");
            if (status == "\"stalled\"" || status == "\"interrupted\"") {
                EXPECT_NE(stop_reason_test::key_value(e, "stop_reason"), "\"none\"")
                    << "a resolution-only status with no stop reason: " << e;
            }
        }
    }
}
