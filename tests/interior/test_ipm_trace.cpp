// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_ipm_trace.cpp -- M6 W4 task 4: the interior-point driver's schema-v0
// events (`ipm.iter`, `ipm.solve.begin`/`.end`) as the DRIVER produces them.
//
// The serializer's own golden lines live with every other golden line, in
// tests/sqp/test_trace_writer.cpp; what is pinned HERE is the driver side:
//
//   (i)   THE CALLBACK IS THE ORACLE. Both are attached at once, and the two
//         are emitted at the same point, so any divergence between the stream
//         and what the late callback saw is a bug in one of them.
//
//   (ii)  BOTH EMIT SITES. `alg_impl` has ONE loop with TWO late-callback
//         sites; a converged solve leaves through the converge-check branch
//         (site 1), a max-iters solve runs out of the loop and never does.
//
//         Site 1 fires on a record `fill_iter_info` never touched, which is
//         what tells them apart -- see `looks_like_the_early_exit_site`.
//
//   (iii) `ipm.solve` on two exit statuses, its census, and its single exit.
//
//   (iv)  THE NULL SINK CHANGES NOTHING -- the same result, iteration count and
//         verdict with and without a sink attached.
//
//   (v)   ONE SINK, TWO ENGINES: `seq` stays contiguous across an SQP solve
//         followed by an IPM solve, and `depth` reads 0 throughout (the
//         `ipm.solve` pair moves no depth -- this driver nests no driver).

#include <cstddef>
#include <cstdio>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <gtest/gtest.h>

// The portable stdout capture both console suites share (M6 W5 T8.7 fix1,
// astra's I2): this file used to carry its own copy, which included <unistd.h>
// and called the POSIX descriptor functions unconditionally -- source that
// cannot be compiled on Windows, where this target is also built.
#include "../support/console_capture.h"
#include "hven/core/ledger.h"
#include "hven/drivers/console_trace_sink.h"
#include "hven/drivers/interior_point_solver.h"
#include "hven/drivers/sqp_driver.h"
#include "hven/drivers/trace_writer.h"
#include "hven/model/nlp_model.h"
#include "hven/model/nlp_problem.h"
#include "hven/model/nlp_solver.h"

namespace hven::solvers {
namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// ===========================================================================
// The fixtures
// ===========================================================================

/// @brief The canonical HS071 cell -- the same problem `test_nlp_solver.cpp`
/// pins the interior-point driver's optimum on, so the stream is taken over a
/// trajectory that is already asserted elsewhere.
struct Hs071Problem : NLPProblem {
    int num_vars() const override { return 4; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 8; }
    int num_hess_nonzeros() const override { return 10; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 1.0, 1.0, 1.0, 1.0;
        xu << 5.0, 5.0, 5.0, 5.0;
        gl << 25.0, 40.0;
        gu << kInfinity, 40.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[3] * (2.0 * x[0] + x[1] + x[2]);
        g[1] = x[0] * x[3];
        g[2] = x[0] * x[3] + 1.0;
        g[3] = x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[1] * x[2] * x[3];
        g[1] = x[0] * x[0] + x[1] * x[1] + x[2] * x[2] + x[3] * x[3];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 0, 0, 1, 1, 1, 1;
        c << 0, 1, 2, 3, 0, 1, 2, 3;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3;
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = x[1] * x[2] * x[3];
        v[1] = x[0] * x[2] * x[3];
        v[2] = x[0] * x[1] * x[3];
        v[3] = x[0] * x[1] * x[2];
        v[4] = 2.0 * x[0];
        v[5] = 2.0 * x[1];
        v[6] = 2.0 * x[2];
        v[7] = 2.0 * x[3];
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        const double x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];
        v[0] = obj_factor * 2 * x3 + lambda[1] * 2;
        v[1] = obj_factor * x3 + lambda[0] * x2 * x3;
        v[2] = lambda[1] * 2;
        v[3] = obj_factor * x3 + lambda[0] * x1 * x3;
        v[4] = lambda[0] * x0 * x3;
        v[5] = lambda[1] * 2;
        v[6] = obj_factor * (2 * x0 + x1 + x2) + lambda[0] * x1 * x2;
        v[7] = obj_factor * x0 + lambda[0] * x0 * x2;
        v[8] = obj_factor * x0 + lambda[0] * x0 * x1;
        v[9] = lambda[1] * 2;
    }
    std::string name() const override { return "Hs071Problem"; }
};

/// @brief A second program with a DIFFERENT structure: two variables, one
/// inequality row, a diagonal Hessian. Nothing about its KKT pattern can be
/// HS071's, so a solver that has just solved HS071 must lay a fresh analysis to
/// solve this one -- which is the situation M6 W5 T8.7 fix1's L3 pins the
/// per-call factorization delta across.
struct TwoVarProblem : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -10.0, -10.0;
        xu << 10.0, 10.0;
        gl << 1.0;
        gu << kInfinity;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = (x[0] - 2.0) * (x[0] - 2.0) + (x[1] - 3.0) * (x[1] - 3.0);
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0 * (x[0] - 2.0);
        g[1] = 2.0 * (x[1] - 3.0);
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] + x[1];
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
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 1.0;
        v[1] = 1.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 2.0 * obj_factor;
        v[1] = 2.0 * obj_factor;
    }
    std::string name() const override { return "TwoVarProblem"; }
};

Eigen::VectorXd two_var_start() {
    Eigen::VectorXd x0(2);
    x0 << 0.0, 0.0;
    return x0;
}

Eigen::VectorXd hs071_start() {
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    return x0;
}

/// @brief The SQP side of pin (v): the smallest model that makes `SqpDriver`
/// write a `sqp.solve` pair -- min (x0-2)^2 + (x1+1)^2 s.t. x0 + x1 = 1,
/// 0 <= x <= 4. Nothing about it is asserted except that its stream exists.
class TinySqpModel : public NlpModel {
  public:
    TinySqpModel() : lower_(Vec::Zero(2)), upper_(Vec::Constant(2, 4.0)) {}

    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        return (x(0) - 2.0) * (x(0) - 2.0) + (x(1) + 1.0) * (x(1) + 1.0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 2.0 * (x(0) - 2.0), 2.0 * (x(1) + 1.0);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(0) + x(1) - 1.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        std::vector<Eigen::Triplet<double>> t{{0, 0, 2.0 * obj_scale}, {1, 1, 2.0 * obj_scale}};
        h.setFromTriplets(t.begin(), t.end());
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 2);
        std::vector<Eigen::Triplet<double>> t{{0, 0, 1.0}, {0, 1, 1.0}};
        j.setFromTriplets(t.begin(), t.end());
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(0, 2);
        j.makeCompressed();
        return j;
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Constant(2, 0.5); }

  private:
    Vec lower_;
    Vec upper_;
};

// ===========================================================================
// Stream helpers -- the writer's layout is ASSUMED, exactly as the T1 pins do
// ===========================================================================

std::vector<std::string> split_lines(const std::string &s) {
    std::vector<std::string> out;
    std::size_t begin = 0;
    while (begin < s.size()) {
        const std::size_t nl = s.find('\n', begin);
        if (nl == std::string::npos) {
            out.push_back(s.substr(begin));
            break;
        }
        out.push_back(s.substr(begin, nl - begin));
        begin = nl + 1;
    }
    return out;
}

/// @brief The token a key maps to, or an empty string when the key is absent.
/// Flat events only, which is every event this file reads.
std::string field(const std::string &line, const std::string &k) {
    const std::string pat = "\"" + k + "\":";
    const std::size_t p = line.find(pat);
    if (p == std::string::npos) {
        return {};
    }
    const std::size_t begin = p + pat.size();
    std::size_t e = begin;
    bool in_str = false;
    bool esc = false;
    for (; e < line.size(); ++e) {
        const char c = line[e];
        if (in_str) {
            if (esc) {
                esc = false;
            } else if (c == '\\') {
                esc = true;
            } else if (c == '"') {
                in_str = false;
            }
            continue;
        }
        if (c == '"') {
            in_str = true;
        } else if (c == ',' || c == '}') {
            break;
        }
    }
    return line.substr(begin, e - begin);
}

std::vector<std::string> lines_of_event(const std::string &stream, const std::string &ev) {
    std::vector<std::string> out;
    for (const std::string &l : split_lines(stream)) {
        if (field(l, "ev") == "\"" + ev + "\"") {
            out.push_back(l);
        }
    }
    return out;
}

/// @brief The whole `ipm.iter` body, keys included, so a comparison against a
/// second serialization of the same record is byte-for-byte.
std::string body_after_envelope(const std::string &line) {
    const std::string pat = "\"depth\":";
    const std::size_t p = line.find(pat);
    if (p == std::string::npos) {
        return {};
    }
    const std::size_t comma = line.find(',', p);
    if (comma == std::string::npos) {
        return {};
    }
    return line.substr(comma + 1);
}

/// @brief Serializes one record through a throwaway sink, so a recorded
/// `IterateInfo` is compared through the SAME writer the stream used -- a
/// field-by-field comparison here would be a second, weaker copy of the key
/// list.
std::string serialize_iter(const IterateInfo &record, Index phase) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_iter(IpmIterTraceEvent{record, phase});
    return body_after_envelope(split_lines(os.str()).front());
}

/// @brief Records every `IterateInfo` the TRACE hands out (M6 W5 T8.6 fix1).
///
/// THE WHOLE-RECORD ORACLE, RESTORED. Until T8.6 the oracle was the LATE
/// CALLBACK, which was handed the `IterateInfo` itself, so re-serializing its
/// copy compared EVERY field of the line -- the two `null` conventions included
/// -- rather than a hand-picked subset. The shared iteration callback that
/// replaced the late one is handed a different view entirely, and T8.6 dropped
/// the byte comparison with the record it had lost. It comes back here through
/// the type's OTHER public door: `IpmIterTraceEvent::iterate` (drivers/trace.h)
/// hands out the same record, one per iterate, to any sink. So the second
/// serialization is available again, from a second sink, and both comparisons
/// stand side by side -- this one for every field of the line, the event
/// oracle's for the two quantities the event and the line share.
///
/// ONE SINK MAY BE ATTACHED AT A TIME, so this one FORWARDS: the solver sees
/// this object, and this object hands every event on to the JsonLines sink the
/// stream assertions read. The stream is byte-for-byte what it would have been
/// with the writer attached directly -- the forwarding adds nothing to it and
/// drops nothing from it.
struct RecordOracle : public TraceSink {
    TraceSink *inner = nullptr;
    std::vector<IterateInfo> seen;
    std::vector<Index> phases;

    explicit RecordOracle(TraceSink *next) : inner(next) {}

    void on_ipm_iter(const IpmIterTraceEvent &e) override {
        seen.push_back(e.iterate);
        phases.push_back(e.phase);
        inner->on_ipm_iter(e);
    }
    void on_ipm_solve_begin(const IpmSolveBeginTraceEvent &e) override {
        inner->on_ipm_solve_begin(e);
    }
    void on_ipm_solve_end(const IpmSolveEndTraceEvent &e) override { inner->on_ipm_solve_end(e); }
    void on_sqp_major(const SqpMajorTraceEvent &e) override { inner->on_sqp_major(e); }
    void on_sqp_solve_begin(const SqpSolveBeginTraceEvent &e) override {
        inner->on_sqp_solve_begin(e);
    }
    void on_sqp_solve_end(const SqpSolveEndTraceEvent &e) override { inner->on_sqp_solve_end(e); }
    void on_ipqp_iter(const IpqpTraceIterEvent &e) override { inner->on_ipqp_iter(e); }
    void on_ipqp_reg(const IpqpTraceRegEvent &e) override { inner->on_ipqp_reg(e); }
    void on_ipqp_restart(const IpqpTraceRestartEvent &e) override { inner->on_ipqp_restart(e); }
    void on_ipqp_route(const IpqpTraceRouteEvent &e) override { inner->on_ipqp_route(e); }
    void on_ipqp_certify(const IpqpTraceCertifyEvent &e) override { inner->on_ipqp_certify(e); }
    void on_ipqp_escape(const IpqpTraceEscapeEvent &e) override { inner->on_ipqp_escape(e); }
    void on_qp_mode(const QpModeTraceEvent &e) override { inner->on_qp_mode(e); }
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &e) override {
        inner->on_fallback_verdict(e);
    }
};

/// @brief Records every event the SHARED ITERATION CALLBACK is handed -- the
/// oracle (M6 W5 T8.6; it recorded `IterateInfo` from the late callback until
/// that callback was retired). Installed alongside the sink so both observe the
/// same iterations.
///
/// WHAT IT CAN STILL COMPARE, AND WHAT IT CANNOT. The old oracle re-serialized
/// the very record the stream had written, so its comparison covered every
/// field of the line. The shared event is a DIFFERENT view -- declared space,
/// caller units, four shared diagnostics in place of the engine's own -- so the
/// two overlap in the row's IDENTITY (`iter`) and its OBJECTIVE (`prim_obj`,
/// which is `f` at obj_scale 1) and in nothing else. Those two are what these
/// tests now compare, one per row and in order, plus the count identity that
/// was always the point: one event per `ipm.iter` line.
struct CallbackOracle {
    struct Seen {
        Index iteration = 0;
        Index phase = 0;
        double f = 0.0;
    };
    std::vector<Seen> seen;

    hven::solvers::IterationCallback hook() {
        return [this](const hven::solvers::IterationEvent &ev) {
            this->seen.push_back(Seen{ev.iteration, ev.phase.value_or(-1), ev.f});
            return hven::solvers::CallbackAction::kContinue;
        };
    }
};

// ===========================================================================
// (i) THE CALLBACK IS THE ORACLE
// ===========================================================================

TEST(IpmTrace, IterCountEqualsTheReportedIterationsAndTheCallbackInvocations) {
    NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        solver.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    CallbackOracle oracle;
    solver.optimizer_->attach_trace(&sink);
    solver.optimizer_->set_iteration_callback(oracle.hook());

    const hven::solvers::SolveStatus flag = solver.optimize(hs071_start());
    ASSERT_EQ(flag, hven::solvers::SolveStatus::kOptimal);

    const std::vector<std::string> iter_lines = lines_of_event(os.str(), "ipm.iter");
    const Index reported = solver.result().iterations;
    ASSERT_GT(reported, 1);
    EXPECT_EQ(static_cast<Index>(iter_lines.size()), reported);
    EXPECT_EQ(oracle.seen.size(), iter_lines.size());
}

TEST(IpmTrace, EveryIterLineIsTheRecordTheCallbackSawInTheSameOrder) {
    NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        solver.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    RecordOracle records(&sink);
    CallbackOracle oracle;
    solver.optimizer_->attach_trace(&records);
    solver.optimizer_->set_iteration_callback(oracle.hook());
    ASSERT_EQ(solver.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);

    const std::vector<std::string> iter_lines = lines_of_event(os.str(), "ipm.iter");
    ASSERT_EQ(iter_lines.size(), oracle.seen.size());
    ASSERT_EQ(iter_lines.size(), records.seen.size());
    ASSERT_FALSE(iter_lines.empty());

    // (a) EVERY FIELD OF EVERY LINE, byte for byte (M6 W5 T8.6 fix1, astra item
    //     6). The recorded record is re-serialized through the SAME writer, so
    //     this covers the whole key list -- the two `null` conventions included
    //     -- and not a subset chosen by hand.
    for (std::size_t k = 0; k < iter_lines.size(); ++k) {
        EXPECT_EQ(body_after_envelope(iter_lines[k]), serialize_iter(records.seen[k], 0))
            << "at ipm.iter line " << k;
    }
    // FALSIFIABILITY: the comparison above must be able to fail. A record whose
    // iteration index is bumped serializes differently.
    IterateInfo mutated = records.seen.back();
    mutated.iter_ += 1;
    EXPECT_NE(body_after_envelope(iter_lines.back()), serialize_iter(mutated, 0));

    // (b) AND THE EVENT ORACLE, on the two quantities the event and the line
    //     share. Two instruments, two claims: (a) says the LINE is the record,
    //     (b) says the EVENT is the same iterate as the line.
    for (std::size_t k = 0; k < iter_lines.size(); ++k) {
        // ONE EVENT PER LINE, IN ORDER, describing the SAME iterate: the row's
        // own index and the objective at it. `{:.17g}` round-trips exactly, so
        // the objective comparison is an equality and not a tolerance -- this
        // solve runs at obj_scale 1, where the event's caller-unit `f` and the
        // line's engine-unit `prim_obj` are one number.
        EXPECT_EQ(std::stoll(field(iter_lines[k], "iter")), oracle.seen[k].iteration)
            << "at ipm.iter line " << k;
        EXPECT_EQ(std::stod(field(iter_lines[k], "prim_obj")), oracle.seen[k].f)
            << "at ipm.iter line " << k;
        EXPECT_EQ(std::stoll(field(iter_lines[k], "phase")), oracle.seen[k].phase)
            << "at ipm.iter line " << k;
    }
    // FALSIFIABILITY: the comparison above must be able to fail. Shifted by one
    // row it does -- consecutive iterates of this solve carry distinct indices
    // and distinct objectives.
    ASSERT_GT(iter_lines.size(), 1u);
    EXPECT_NE(std::stoll(field(iter_lines.back(), "iter")),
              oracle.seen[oracle.seen.size() - 2].iteration);
}

TEST(IpmTrace, TheLastIterLineEqualsTheLastRecordTheCallbackSaw) {
    NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        solver.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    CallbackOracle oracle;
    solver.optimizer_->attach_trace(&sink);
    solver.optimizer_->set_iteration_callback(oracle.hook());
    ASSERT_EQ(solver.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);

    const std::vector<std::string> iter_lines = lines_of_event(os.str(), "ipm.iter");
    ASSERT_FALSE(iter_lines.empty());
    ASSERT_FALSE(oracle.seen.empty());
    // THE TERMINAL ROW FIRES TOO (M6 W5 T8.6's `IpmTerminalRowStillFires`, read
    // from the trace's side): the last event describes the last line.
    EXPECT_EQ(std::stoll(field(iter_lines.back(), "iter")), oracle.seen.back().iteration);
    EXPECT_EQ(std::stod(field(iter_lines.back(), "prim_obj")), oracle.seen.back().f);
}

TEST(IpmTrace, TheTwoProximalShiftsAreNullOnTheClassicPathAndNumbersUnderProximalMode) {
    // Rule 5, the FIRST of the record's two -1 conventions: "proximal mode off".
    // The classic path writes -1 on every iteration, which is not a shift of -1.
    NLPSolver classic(std::make_shared<Hs071Problem>());
    {
        auto o = classic.optimizer_->options();
        o.common.print_level = 10;
        classic.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os_classic;
    JsonLinesTraceSink sink_classic(os_classic);
    classic.optimizer_->attach_trace(&sink_classic);
    ASSERT_EQ(classic.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);
    for (const std::string &l : lines_of_event(os_classic.str(), "ipm.iter")) {
        EXPECT_EQ(field(l, "prox_reg_primal"), "null");
        EXPECT_EQ(field(l, "prox_reg_dual"), "null");
    }

    NLPSolver prox(std::make_shared<Hs071Problem>());
    {
        auto o = prox.optimizer_->options();
        o.common.print_level = 10;
        prox.optimizer_->set_options(std::move(o));
    }
    {
        auto o = prox.optimizer_->options();
        o.inertia_mode = InertiaModes::proximal_regularization;
        prox.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os_prox;
    JsonLinesTraceSink sink_prox(os_prox);
    prox.optimizer_->attach_trace(&sink_prox);
    prox.optimize(hs071_start());
    const std::vector<std::string> prox_lines = lines_of_event(os_prox.str(), "ipm.iter");
    ASSERT_FALSE(prox_lines.empty());
    // NOT EVERY line: the converge-check exit fires before any factorization
    // and carries the fresh -1 defaults. At least one factorized iteration must
    // report a real pair, or "null" would mean nothing.
    int numeric = 0;
    for (const std::string &l : prox_lines) {
        if (field(l, "prox_reg_primal") != "null") {
            ++numeric;
            EXPECT_NE(field(l, "prox_reg_dual"), "");
        }
    }
    EXPECT_GT(numeric, 0);
}

// ===========================================================================
// (ii) BOTH EMIT SITES
// ===========================================================================

/// @brief Does this line come from SITE 1, the converge-check early exit?
///
/// THAT SITE EMITS A RECORD `fill_iter_info` NEVER TOUCHED. The record was
/// filled by `fill_residual_info` at the top of the iteration and the branch
/// leaves before any factorization, line search or barrier evaluation, so
/// `barr_obj_`, `merit_val_` and `ls_iters_` are still IterateInfo's fresh
/// per-iteration 0 and the three alphas are still its fresh 1.0 -- the set the
/// driver's own comment at that branch enumerates. Site 2's record has all of
/// them written from the iteration that just ran.
///
/// NOT `h_facs`: it counts INERTIA-PERTURBATION ladder steps, not
/// factorizations, and reads 0 at both sites on a cell that never needs one.
bool looks_like_the_early_exit_site(const std::string &line) {
    return field(line, "barr_obj") == "0" && field(line, "merit_val") == "0" &&
           field(line, "ls_iters") == "0" && field(line, "alpha_p") == "1" &&
           field(line, "alpha_d") == "1" && field(line, "alpha_t") == "1";
}

TEST(IpmTrace, AConvergedSolveLeavesThroughTheConvergeCheckSiteAndAMaxItersSolveDoesNot) {
    NLPSolver converged(std::make_shared<Hs071Problem>());
    {
        auto o = converged.optimizer_->options();
        o.common.print_level = 10;
        converged.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os_c;
    JsonLinesTraceSink sink_c(os_c);
    converged.optimizer_->attach_trace(&sink_c);
    ASSERT_EQ(converged.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);
    const std::vector<std::string> c_lines = lines_of_event(os_c.str(), "ipm.iter");
    ASSERT_GT(c_lines.size(), 1u);

    // EXACTLY ONE line comes from site 1, and it is the LAST: the branch that
    // fires it also leaves the loop.
    int early = 0;
    for (const std::string &l : c_lines) {
        early += looks_like_the_early_exit_site(l) ? 1 : 0;
    }
    EXPECT_EQ(early, 1);
    EXPECT_TRUE(looks_like_the_early_exit_site(c_lines.back()));
    // Every other line is site 2's, so BOTH sites emitted on this one solve.
    EXPECT_FALSE(looks_like_the_early_exit_site(c_lines.front()));

    NLPSolver truncated(std::make_shared<Hs071Problem>());
    {
        auto o = truncated.optimizer_->options();
        o.common.print_level = 10;
        o.max_iters = 3;
        truncated.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os_t;
    JsonLinesTraceSink sink_t(os_t);
    truncated.optimizer_->attach_trace(&sink_t);
    ASSERT_EQ(truncated.optimize(hs071_start()), hven::solvers::SolveStatus::kMaxIter);
    const std::vector<std::string> t_lines = lines_of_event(os_t.str(), "ipm.iter");
    // THE CONTROL that makes the signature above mean something: a solve that
    // runs out of iterations never reaches site 1, so no line matches -- the
    // predicate is not simply true of every line.
    ASSERT_EQ(t_lines.size(), 3u);
    for (const std::string &l : t_lines) {
        EXPECT_FALSE(looks_like_the_early_exit_site(l)) << l;
    }
}

TEST(IpmTrace, ThePerturbedPivotCountIsTheBackendsOwnAndNeverAFabricatedZero) {
    // R2 / CLAUDE.md section 6. `KktFactorization::ppivs()` projects an ABSENT
    // backend count to the integer 0 (`.value_or(0)`), so the stream must read
    // the factorization's own optional instead of repeating that 0.
    //
    // BOTH ARMS ARE COMPILED FROM ONE SOURCE and the backend picks which runs,
    // so the macOS lane executes the Accelerate arm without an edit here.
    NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        solver.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.optimizer_->attach_trace(&sink);
    ASSERT_EQ(solver.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);

    const std::vector<std::string> iter_lines = lines_of_event(os.str(), "ipm.iter");
    ASSERT_GT(iter_lines.size(), 1u);
    int factorized = 0;
    for (const std::string &l : iter_lines) {
        if (looks_like_the_early_exit_site(l)) {
            // NEVER FACTORIZED, so there is no pivot count to report on either
            // backend -- `null`, not the record's untouched 0 default.
            EXPECT_EQ(field(l, "p_pivots"), "null") << l;
            continue;
        }
        ++factorized;
#if defined(USE_ACCELERATE_SPARSE)
        // Accelerate reports NO perturbed-pivot count at all, so every
        // factorized line is `null` too. UNOBSERVED here; the macOS lane runs
        // this arm.
        EXPECT_EQ(field(l, "p_pivots"), "null") << l;
#else
        // MKL Pardiso does count them, so a factorized line carries a NUMBER --
        // which is what makes the `null`s above a reading rather than a blanket.
        const std::string v = field(l, "p_pivots");
        EXPECT_NE(v, "null") << l;
        EXPECT_GE(std::stoll(v), 0) << l;
#endif
    }
    EXPECT_GT(factorized, 0) << "the cell must actually factorize";
}

// ===========================================================================
// (iii) `ipm.solve`
// ===========================================================================

TEST(IpmTrace, SolveWritesExactlyOnePairPerEntryPointAndBracketsEveryIterLine) {
    NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        solver.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.optimizer_->attach_trace(&sink);
    ASSERT_EQ(solver.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);

    const std::vector<std::string> all = split_lines(os.str());
    ASSERT_GE(all.size(), 3u);
    EXPECT_EQ(field(all.front(), "ev"), "\"ipm.solve.begin\"");
    EXPECT_EQ(field(all.back(), "ev"), "\"ipm.solve.end\"");
    EXPECT_EQ(lines_of_event(os.str(), "ipm.solve.begin").size(), 1u);
    EXPECT_EQ(lines_of_event(os.str(), "ipm.solve.end").size(), 1u);
    for (const std::string &l : all) {
        EXPECT_EQ(field(l, "depth"), "0");
    }
}

TEST(IpmTrace, SolveBeginCarriesHs071sDimensionsCensusAndSettings) {
    NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        o.max_iters = 40;
        solver.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.optimizer_->attach_trace(&sink);
    ASSERT_EQ(solver.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);

    const std::vector<std::string> begin = lines_of_event(os.str(), "ipm.solve.begin");
    ASSERT_EQ(begin.size(), 1u);
    const std::string &b = begin.front();
    EXPECT_EQ(field(b, "n"), "4");
    EXPECT_EQ(field(b, "n_reduced"), "4");
    // HS071's one two-sided row is split by the transcription into an equality
    // and an inequality; the census is the DECLARED VARIABLE box's, and all four
    // variables carry 1 <= x <= 5, so every one of them is `ranged`.
    EXPECT_EQ(field(b, "me"), "1");
    EXPECT_EQ(field(b, "mi"), "1");
    EXPECT_EQ(field(b, "vars_free"), "0");
    EXPECT_EQ(field(b, "vars_lower_only"), "0");
    EXPECT_EQ(field(b, "vars_upper_only"), "0");
    EXPECT_EQ(field(b, "vars_ranged"), "4");
    EXPECT_EQ(field(b, "vars_fixed"), "0");
    EXPECT_EQ(field(b, "phases"), "1");
    EXPECT_EQ(field(b, "max_iters"), "40");
    EXPECT_EQ(field(b, "inertia_mode"), "\"classic\"");
    EXPECT_EQ(field(b, "restoration_mode"), "\"off\"");
}

TEST(IpmTrace, SolveOptimizeReportsTwoPhasesAndNumbersItsIterLinesByPhase) {
    NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        solver.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.optimizer_->attach_trace(&sink);
    solver.solve_optimize(hs071_start());

    const std::vector<std::string> begin = lines_of_event(os.str(), "ipm.solve.begin");
    ASSERT_EQ(begin.size(), 1u);
    EXPECT_EQ(field(begin.front(), "phases"), "2");
    // ONE pair for the whole entry point, not one per phase -- which is why the
    // per-line `phase` key exists: `iter` restarts at 0 in each phase.
    EXPECT_EQ(lines_of_event(os.str(), "ipm.solve.end").size(), 1u);
    // BOTH phases must appear (fix round 1, M-3): tracking only phase 1 would
    // be satisfied by an entirely phase-1 stream, which is exactly the bug a
    // mis-set `trace_phase_` would produce.
    bool saw_phase_zero = false;
    bool saw_phase_one = false;
    for (const std::string &l : lines_of_event(os.str(), "ipm.iter")) {
        const std::string p = field(l, "phase");
        EXPECT_TRUE(p == "0" || p == "1") << p;
        saw_phase_zero = saw_phase_zero || p == "0";
        saw_phase_one = saw_phase_one || p == "1";
    }
    EXPECT_TRUE(saw_phase_zero);
    EXPECT_TRUE(saw_phase_one);
    // ... and the FIRST phase's lines precede the second's, so `phase` is
    // monotone over the stream rather than merely present in both values.
    Index last = 0;
    for (const std::string &l : lines_of_event(os.str(), "ipm.iter")) {
        const Index p = static_cast<Index>(std::stoll(field(l, "phase")));
        EXPECT_GE(p, last);
        last = p;
    }
}

TEST(IpmTrace, SolveEndReportsTheDriversOwnStatusOnTwoDifferentExits) {
    NLPSolver converged(std::make_shared<Hs071Problem>());
    {
        auto o = converged.optimizer_->options();
        o.common.print_level = 10;
        converged.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os_c;
    JsonLinesTraceSink sink_c(os_c);
    converged.optimizer_->attach_trace(&sink_c);
    ASSERT_EQ(converged.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);
    const std::vector<std::string> end_c = lines_of_event(os_c.str(), "ipm.solve.end");
    ASSERT_EQ(end_c.size(), 1u);
    // THE VOCABULARY MOVED IN M6 W5 T8.4, declared: the event carries SolveStatus
    // now (the engine's own ConvergenceFlags is gone), so `converged` reads
    // `optimal` and `not_converged` reads `max_iter` -- or `stalled` at the two
    // abnormal exits the old vocabulary could not tell apart at all.
    EXPECT_EQ(field(end_c.front(), "status"), "\"optimal\"");
    EXPECT_EQ(field(end_c.front(), "iters"), std::to_string(converged.result().iterations));

    NLPSolver truncated(std::make_shared<Hs071Problem>());
    {
        auto o = truncated.optimizer_->options();
        o.common.print_level = 10;
        o.max_iters = 3;
        truncated.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os_t;
    JsonLinesTraceSink sink_t(os_t);
    truncated.optimizer_->attach_trace(&sink_t);
    ASSERT_EQ(truncated.optimize(hs071_start()), hven::solvers::SolveStatus::kMaxIter);
    const std::vector<std::string> end_t = lines_of_event(os_t.str(), "ipm.solve.end");
    ASSERT_EQ(end_t.size(), 1u);
    EXPECT_EQ(field(end_t.front(), "status"), "\"max_iter\"");
    EXPECT_EQ(field(end_t.front(), "iters"), "3");
}

TEST(IpmTrace, ARefusedCallWritesNoLineAtAll) {
    // The `begin` emit sits AFTER every argument refusal, so a call that never
    // ran leaves no opening line dangling in the artifact.
    NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        solver.optimizer_->set_options(std::move(o));
    }
    solver.transcribe();
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.optimizer_->attach_trace(&sink);
    Eigen::VectorXd wrong(3);
    wrong << 1.0, 1.0, 1.0;
    EXPECT_THROW((void)solver.optimizer_->solve(*solver.nlp_, wrong), std::invalid_argument);
    EXPECT_EQ(os.str(), "");
    EXPECT_EQ(sink.lines_written(), 0);
    EXPECT_EQ(sink.depth(), 0);
}

// ===========================================================================
// (iv) THE NULL SINK CHANGES NOTHING
// ===========================================================================

TEST(IpmTrace, AttachingASinkMovesNoResultField) {
    NLPSolver bare(std::make_shared<Hs071Problem>());
    {
        auto o = bare.optimizer_->options();
        o.common.print_level = 10;
        bare.optimizer_->set_options(std::move(o));
    }
    const hven::solvers::SolveStatus bare_flag = bare.optimize(hs071_start());
    const hven::solvers::IpmResult &b = bare.result();
    const int bare_iters = b.iterations;
    const double bare_obj = b.f;
    const double bare_kkt = b.kkt_inf;
    const double bare_barr = b.barr_inf;
    const double bare_econ = b.econ_inf;
    const double bare_icon = b.icon_inf;
    const Eigen::VectorXd bare_x = bare.return_x();

    NLPSolver traced(std::make_shared<Hs071Problem>());
    {
        auto o = traced.optimizer_->options();
        o.common.print_level = 10;
        traced.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    traced.optimizer_->attach_trace(&sink);
    const hven::solvers::SolveStatus traced_flag = traced.optimize(hs071_start());
    const hven::solvers::IpmResult &t = traced.result();

    EXPECT_EQ(traced_flag, bare_flag);
    EXPECT_EQ(t.iterations, bare_iters);
    EXPECT_EQ(t.f, bare_obj);
    EXPECT_EQ(t.kkt_inf, bare_kkt);
    EXPECT_EQ(t.barr_inf, bare_barr);
    EXPECT_EQ(t.econ_inf, bare_econ);
    EXPECT_EQ(t.icon_inf, bare_icon);
    EXPECT_EQ(traced.return_x(), bare_x);
    // NON-VACUITY: the sink really did run.
    EXPECT_GT(sink.lines_written(), 2);
}

TEST(IpmTrace, DetachingMidLifetimeStopsTheStreamAndChangesNothingElse) {
    NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        solver.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.optimizer_->attach_trace(&sink);
    ASSERT_EQ(solver.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);
    const Index first = sink.lines_written();
    ASSERT_GT(first, 0);

    solver.optimizer_->attach_trace(nullptr);
    ASSERT_EQ(solver.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(sink.lines_written(), first);
}

// ===========================================================================
// (v) ONE SINK, TWO ENGINES
// ===========================================================================

TEST(IpmTrace, SeqIsContiguousAcrossAnSqpSolveThenAnIpmSolveOnOneSinkAtDepthZero) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);

    SqpOptions opts;
    opts.max_iter = 20;
    SqpDriver driver(opts);
    driver.attach_trace(&sink);
    TinySqpModel model;
    const SqpSolution sqp_out = driver.solve(model, model.start_point());
    ASSERT_GT(sink.lines_written(), 0);
    EXPECT_EQ(sqp_out.status, SolveStatus::kOptimal);
    const Index after_sqp = sink.lines_written();

    NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        solver.optimizer_->set_options(std::move(o));
    }
    solver.optimizer_->attach_trace(&sink);
    ASSERT_EQ(solver.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);
    EXPECT_GT(sink.lines_written(), after_sqp);

    const std::vector<std::string> all = split_lines(os.str());
    ASSERT_EQ(static_cast<Index>(all.size()), sink.lines_written());
    for (std::size_t k = 0; k < all.size(); ++k) {
        EXPECT_EQ(field(all[k], "seq"), std::to_string(k + 1)) << "at line " << k;
        EXPECT_EQ(field(all[k], "depth"), "0") << "at line " << k;
        EXPECT_EQ(field(all[k], "v"), "0");
    }
    // The two streams PARTITION the artifact: the SQP pair closes before the
    // IPM pair opens, so a reader splits on the pairs and needs nothing else.
    EXPECT_EQ(field(all.front(), "ev"), "\"sqp.solve.begin\"");
    EXPECT_EQ(field(all.back(), "ev"), "\"ipm.solve.end\"");
    const std::vector<std::string> sqp_end = lines_of_event(os.str(), "sqp.solve.end");
    const std::vector<std::string> ipm_begin = lines_of_event(os.str(), "ipm.solve.begin");
    ASSERT_EQ(sqp_end.size(), 1u);
    ASSERT_EQ(ipm_begin.size(), 1u);
    EXPECT_LT(std::stoll(field(sqp_end.front(), "seq")),
              std::stoll(field(ipm_begin.front(), "seq")));
}

// ===========================================================================
// M6 W5 T8.7 -- THE LEDGER ON THE INTERIOR-POINT ENGINE, and the console the
// engine now attaches for itself.
// ===========================================================================

namespace {

/// The live console pins below read the process's real `stdout`: the SOLVER
/// builds its own console and gives it `stdout`, and the print sites this task
/// left direct write there too. `hven::testing::StdoutCapture`
/// (tests/support/console_capture.h) is that redirection, in one portable
/// place -- this file used to carry its own copy of it (M6 W5 T8.7 fix1,
/// astra's I2).
using hven::testing::StdoutCapture;

/// Every line carrying a wall-clock reading, replaced by a marker.
///
/// The console's closing block prints seven times and a per-iteration average,
/// all of them " ms" lines; CLAUDE.md section 7 makes those informational, and
/// two runs of one solve differ there by construction. Everything else -- the
/// banner, the statistics, every row with its colours, the Beginning/Finished
/// pair and the verdict -- is compared byte for byte.
std::string strip_timing(const std::string &text) {
    std::string out;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t eol = text.find('\n', pos);
        const std::size_t end = (eol == std::string::npos) ? text.size() : eol;
        const std::string line = text.substr(pos, end - pos);
        out += (line.find(" ms") == std::string::npos) ? line : "<TIMING LINE MASKED>";
        if (eol == std::string::npos) {
            break;
        }
        out += '\n';
        pos = eol + 1;
    }
    return out;
}

/// A silent solver on HS071 -- `print_level` 10 -- so a ledger pin does not
/// also print a table into the test log.
std::unique_ptr<NLPSolver> silent_hs071() {
    auto solver = std::make_unique<NLPSolver>(std::make_shared<Hs071Problem>());
    auto o = solver->optimizer_->options();
    o.common.print_level = 10;
    solver->optimizer_->set_options(std::move(o));
    return solver;
}

} // namespace

TEST(IpmLedger, OneRecordPerSolveCarryingThatCallsOwnCounters) {
    auto solver = silent_hs071();
    Ledger ledger;
    solver->optimizer_->attach_ledger(&ledger, "ipm");
    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);
    // EACH RECORD AGAINST ITS OWN CALL'S RESULT (M6 W5 T8.7 fix1, astra's
    // Minor): `last_result_` is overwritten by the second call, so the first
    // record's fields are held here, while they still describe the call that
    // wrote them. Comparing record 0 against the SECOND result would pass on
    // this fixture only because the two calls agree, and would go on passing if
    // the record were written from the wrong call.
    const IpmResult first_result = solver->last_result_;
    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);
    const IpmResult second_result = solver->last_result_;

    ASSERT_EQ(ledger.ipm_records().size(), 2u);
    // The QP-level and SQP-level vectors are untouched: three kinds of record,
    // three vectors, no collision.
    EXPECT_TRUE(ledger.records().empty());
    EXPECT_TRUE(ledger.sqp_records().empty());

    const IpmSolveRecord &first = ledger.ipm_records()[0];
    const IpmSolveRecord &second = ledger.ipm_records()[1];
    EXPECT_EQ(first.label, "ipm_0");
    EXPECT_EQ(second.label, "ipm_1");
    EXPECT_EQ(first.status, SolveStatus::kOptimal);
    EXPECT_EQ(first.iterations, first_result.iterations);
    EXPECT_EQ(first.total_time, first_result.total_time);
    EXPECT_EQ(first.phases_run, 1);
    EXPECT_GT(first.factorizations, 0);
    EXPECT_GT(first.analyses, 0);
    EXPECT_EQ(first.soc_steps_taken, first_result.soc_steps_taken);
    EXPECT_EQ(first.watchdog_activations, first_result.watchdog_activations);
    // The second record likewise, against the SECOND call's result.
    EXPECT_EQ(second.status, second_result.status);
    EXPECT_EQ(second.iterations, second_result.iterations);
    EXPECT_EQ(second.total_time, second_result.total_time);
    EXPECT_EQ(second.soc_steps_taken, second_result.soc_steps_taken);
    EXPECT_EQ(second.watchdog_activations, second_result.watchdog_activations);
    // wall_seconds is INFORMATIONAL (CLAUDE.md section 7): populated, never a
    // magnitude.
    EXPECT_GT(first.wall_seconds, 0.0);
}

TEST(IpmLedger, FactorizationsAndAnalysesArePerCallNotLifetime) {
    // THE DEFECT THIS FIELD'S DELTA EXISTS TO AVOID: `kkt_factor_counters` is a
    // LIFETIME snapshot, so a solver reused for a second solve would charge the
    // second record for the first call's factorizations too.
    auto solver = silent_hs071();
    Ledger ledger;
    solver->optimizer_->attach_ledger(&ledger, "reuse");
    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);
    const Index lifetime_after_first = solver->last_result_.kkt_factor_counters.factorize_count;
    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);
    const Index lifetime_after_second = solver->last_result_.kkt_factor_counters.factorize_count;

    ASSERT_EQ(ledger.ipm_records().size(), 2u);
    EXPECT_GT(lifetime_after_second, lifetime_after_first)
        << "premise: the second solve pays factorizations of its own";
    EXPECT_EQ(ledger.ipm_records()[1].factorizations, lifetime_after_second - lifetime_after_first);
    EXPECT_LT(ledger.ipm_records()[1].factorizations, lifetime_after_second)
        << "the record reports THIS call's factorizations, not the lifetime total";
    // `analyses` is already per call, and the SECOND solve on the same program
    // reuses the analysis -- so it is the honest 0 there.
    EXPECT_EQ(ledger.ipm_records()[1].analyses, solver->last_result_.kkt_analyses_this_call);
}

TEST(IpmLedger, ThePerCallDeltaSurvivesAReAnalysisInsideTheCall) {
    // M6 W5 T8.7 fix1 (the lane's M3). `FactorizationsAndAnalysesArePerCallNot
    // Lifetime` pins the delta on a solver REUSED ON ONE PROGRAM, where the
    // analysis is reused too. What it does not reach is the other reuse: the
    // same solver handed a DIFFERENT program, which re-lays the analysis inside
    // the call. The delta is only honest there if the factor's lifetime counter
    // is monotone across a re-analysis -- nothing in `linear/` resets it, and
    // this says so out loud rather than assuming it.
    auto hs = silent_hs071();
    NLPSolver other(std::make_shared<TwoVarProblem>());
    {
        auto o = other.optimizer_->options();
        o.common.print_level = 10;
        other.optimizer_->set_options(std::move(o));
    }
    // The second program, laid out and ready to be BORROWED by the first
    // solver: since M6 W5 T8.4 a program is an argument of solve(), so one
    // solver may be handed two.
    other.transcribe();

    Ledger ledger;
    hs->optimizer_->attach_ledger(&ledger, "cross");
    ASSERT_EQ(hs->optimize(hs071_start()), SolveStatus::kOptimal);
    const IpmResult first_result = hs->last_result_;
    const IpmResult second_result = hs->optimizer_->solve(*other.nlp_, two_var_start());

    ASSERT_EQ(ledger.ipm_records().size(), 2u) << "two calls, two records";
    const IpmSolveRecord &first = ledger.ipm_records()[0];
    const IpmSolveRecord &second = ledger.ipm_records()[1];

    // THE PREMISE, IN TWO PARTS. The second call really did re-lay the
    // analysis -- and, because re-transcribing REPLACES the linear engine
    // (`set_qp_params()` calls `KktFactorization::reconfigure()`), the engine
    // counters in the result went BACKWARDS across the two calls. That second
    // fact is the defect this pin was asked for: differencing THOSE counters
    // (which is what the record did until M6 W5 T8.7 fix1) reports a negative
    // number of factorizations.
    EXPECT_GT(second_result.kkt_analyses_this_call, 0)
        << "premise: a different program must re-lay the analysis";
    EXPECT_EQ(second.analyses, second_result.kkt_analyses_this_call);
    const Index engine_count_after_first = first_result.kkt_factor_counters.factorize_count;
    const Index engine_count_after_second = second_result.kkt_factor_counters.factorize_count;
    EXPECT_GT(engine_count_after_first, 0);
    EXPECT_LT(engine_count_after_second, engine_count_after_first)
        << "premise: the re-lay replaced the engine, so its per-instance counters restarted; "
           "if this ever stops holding, the defect below has changed shape and this pin must be "
           "re-derived rather than deleted";

    // AND THE RECORD IS THIS CALL'S OWN WORK, NON-NEGATIVE, ON BOTH SIDES OF
    // THE RE-LAY. The first call ran on the engine built with the solver, so
    // its record is that engine's whole count; the second ran on an engine born
    // INSIDE it, so every factorization that engine counts is the second call's.
    EXPECT_EQ(first.factorizations, engine_count_after_first);
    EXPECT_GT(second.factorizations, 0) << "a per-call count is never negative";
    EXPECT_EQ(second.factorizations, engine_count_after_second)
        << "the second call's record must be the work done after the re-lay, which is all the "
           "work the replacement engine has ever done";
    EXPECT_EQ(second.status, second_result.status);
    EXPECT_EQ(second.iterations, second_result.iterations);
}

TEST(IpmLedger, AMultiPhaseCallCountsOnlyThePhasesThatRan) {
    // M6 W5 T8.7 fix1 (astra's Minor). `phases_run` is the count of
    // `result.phases[i].ran`, which is NOT `phases.size()`: a sequence whose
    // first phase is stopped never runs the second. Both halves are pinned
    // here, on the same two-phase sequence.
    auto both = silent_hs071();
    Ledger ledger;
    both->optimizer_->attach_ledger(&ledger, "seq");
    ASSERT_EQ(both->solve_optimize(hs071_start()), SolveStatus::kOptimal);
    const IpmResult ran_both = both->last_result_;
    ASSERT_EQ(ran_both.phases.size(), 2u) << "premise: two phases were declared";
    EXPECT_TRUE(ran_both.phases[0].ran);
    EXPECT_TRUE(ran_both.phases[1].ran);
    ASSERT_EQ(ledger.ipm_records().size(), 1u);
    EXPECT_EQ(ledger.ipm_records()[0].phases_run, 2);

    // THE SKIPPED PHASE: the callback stops the solve inside phase 0, and the
    // sequence ends there.
    auto stopped = silent_hs071();
    Ledger stop_ledger;
    stopped->optimizer_->attach_ledger(&stop_ledger, "stop");
    stopped->optimizer_->set_iteration_callback(
        [](const IterationEvent &) { return CallbackAction::kStop; });
    stopped->solve_optimize(hs071_start());
    const IpmResult stopped_result = stopped->last_result_;
    ASSERT_EQ(stopped_result.phases.size(), 2u);
    ASSERT_TRUE(stopped_result.phases[0].ran);
    ASSERT_FALSE(stopped_result.phases[1].ran) << "premise: the second phase was skipped";
    ASSERT_EQ(stop_ledger.ipm_records().size(), 1u);
    EXPECT_EQ(stop_ledger.ipm_records()[0].phases_run, 1)
        << "a skipped phase must not be counted as run";
    EXPECT_EQ(stop_ledger.ipm_records()[0].status, stopped_result.status);
    EXPECT_EQ(stop_ledger.ipm_records()[0].iterations, stopped_result.iterations);
}

TEST(IpmLedger, ACallbackThatThrowsAfterWorkBeganWritesNoRecord) {
    // M6 W5 T8.7 fix1 (astra's Minor). `ARefusedCallRecordsNothingAndConsumes
    // NoLabel` covers the throw that happens ABOVE the funnel, at the argument
    // check. This is the other one: a call that got as far as iterating and
    // then left by an exception raised in CALLER code. The record is written
    // after `wall.stop()` on the normal path only, so an exception carries the
    // call past it -- no record, and no label number consumed.
    auto solver = silent_hs071();
    Ledger ledger;
    solver->optimizer_->attach_ledger(&ledger, "boom");
    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);
    ASSERT_EQ(ledger.ipm_records().size(), 1u);

    Index fired = 0;
    solver->optimizer_->set_iteration_callback([&](const IterationEvent &) -> CallbackAction {
        ++fired;
        throw std::runtime_error("from inside the callback, after work began");
    });
    EXPECT_THROW(solver->optimize(hs071_start()), std::runtime_error);
    EXPECT_GE(fired, 1) << "premise: the solve had begun iterating";
    EXPECT_EQ(ledger.ipm_records().size(), 1u) << "a throw out of a solve records nothing";

    // ... and the number it did not consume is the next one.
    solver->optimizer_->clear_iteration_callback();
    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);
    ASSERT_EQ(ledger.ipm_records().size(), 2u);
    EXPECT_EQ(ledger.ipm_records()[1].label, "boom_1");
}

TEST(IpmLedger, AttachResetsTheCounterAndADetachStopsRecording) {
    auto solver = silent_hs071();
    Ledger first_ledger;
    solver->optimizer_->attach_ledger(&first_ledger, "a");
    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);
    EXPECT_EQ(first_ledger.ipm_records().size(), 1u);

    Ledger second_ledger;
    solver->optimizer_->attach_ledger(&second_ledger, "b");
    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);
    ASSERT_EQ(second_ledger.ipm_records().size(), 1u);
    EXPECT_EQ(second_ledger.ipm_records()[0].label, "b_0")
        << "attach_ledger restarts the label sequence";
    EXPECT_EQ(first_ledger.ipm_records().size(), 1u) << "the old ledger stopped receiving";

    solver->optimizer_->attach_ledger(nullptr, "");
    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);
    EXPECT_EQ(second_ledger.ipm_records().size(), 1u);
}

TEST(IpmLedger, ARefusedCallRecordsNothingAndConsumesNoLabel) {
    auto solver = silent_hs071();
    Ledger ledger;
    solver->optimizer_->attach_ledger(&ledger, "throw");
    // One good solve first, which is also what TRANSCRIBES the program this
    // test then hands a bad start to.
    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);
    ASSERT_EQ(ledger.ipm_records().size(), 1u);
    EXPECT_EQ(ledger.ipm_records()[0].label, "throw_0");

    // A start vector of the wrong length is refused at the public boundary,
    // ABOVE the funnel that writes the record.
    Eigen::VectorXd bad(3);
    bad << 1.0, 1.0, 1.0;
    EXPECT_THROW(solver->optimizer_->solve(*solver->nlp_, bad), std::invalid_argument);
    EXPECT_EQ(ledger.ipm_records().size(), 1u) << "a refused call records nothing";

    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);
    ASSERT_EQ(ledger.ipm_records().size(), 2u);
    EXPECT_EQ(ledger.ipm_records()[1].label, "throw_1") << "the refusal consumed no number";
}

TEST(IpmLedger, TheSummaryTableIsEmptyWithoutRecordsAndHasARowPerRecordWithThem) {
    Ledger empty;
    EXPECT_EQ(empty.ipm_summary_table(), "");

    auto solver = silent_hs071();
    Ledger ledger;
    solver->optimizer_->attach_ledger(&ledger, "tbl");
    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);
    ASSERT_EQ(solver->optimize(hs071_start()), SolveStatus::kOptimal);

    const std::string table = ledger.ipm_summary_table();
    EXPECT_NE(table.find("Label"), std::string::npos);
    EXPECT_NE(table.find("Factorizations"), std::string::npos);
    EXPECT_NE(table.find("tbl_0"), std::string::npos);
    EXPECT_NE(table.find("tbl_1"), std::string::npos);
    // Header, rule, two rows.
    EXPECT_EQ(std::count(table.begin(), table.end(), '\n'), 4);
    // NO TIMING COLUMN: both time fields are informational, and a table is
    // where an informational number gets quoted as a measurement.
    EXPECT_EQ(table.find("Time"), std::string::npos);
    EXPECT_EQ(table.find("Wall"), std::string::npos);
}

TEST(IpmTrace, SolveBeginCarriesTheEightFieldsTheConsoleTableNeeds) {
    // M6 W5 T8.7's addition, read off a REAL solve rather than a scripted
    // event: the four acceptable tolerances, the layout width, and the three
    // `print_stats` inputs.
    NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        o.wide_console = true;
        solver.optimizer_->set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.optimizer_->attach_trace(&sink);
    ASSERT_EQ(solver.optimize(hs071_start()), SolveStatus::kOptimal);

    const std::vector<std::string> begin = lines_of_event(os.str(), "ipm.solve.begin");
    ASSERT_EQ(begin.size(), 1u);
    const std::string &b = begin.front();
    EXPECT_EQ(field(b, "wide_console"), "true");
    // HS071 declares no fixed variable, so the MakeConstraint treatment
    // installs no internal row -- a reading, not a missing value.
    EXPECT_EQ(field(b, "internal_fixed_rows"), "0");
    // The KKT system is real: a positive dimension and a positive fill.
    EXPECT_GT(std::stoll(field(b, "kkt_dim")), 0);
    EXPECT_GT(std::stoll(field(b, "kkt_nnz")), 0);
    // The four acceptable tolerances are the shipped defaults, and each is
    // LOOSER than its convergence counterpart -- which is the ordering
    // `validate()` enforces and the colouring depends on.
    EXPECT_GT(std::stod(field(b, "acc_kkt_tol")), std::stod(field(b, "kkt_tol")));
    EXPECT_GT(std::stod(field(b, "acc_econ_tol")), std::stod(field(b, "econ_tol")));
    EXPECT_GT(std::stod(field(b, "acc_icon_tol")), std::stod(field(b, "icon_tol")));
    EXPECT_GT(std::stod(field(b, "acc_bar_tol")), std::stod(field(b, "bar_tol")));
}

TEST(IpmConsole, PrintLevelZeroWritesTheTableAndTenWritesNothing) {
    // The console the SOLVER attaches for itself, on a live solve. What is
    // pinned here is the SHAPE -- the banner, the statistics block, the row
    // header, the Beginning/Finished pair and the timing summary -- and that a
    // silent level writes not one byte. The BYTES are pinned twice over: the
    // rows against the old printer in tests/drivers/test_console_sink.cpp, and
    // the whole transcript against a BASE capture in this task's leg.
    std::string printed;
    {
        NLPSolver solver(std::make_shared<Hs071Problem>());
        auto o = solver.optimizer_->options();
        o.common.print_level = 0;
        solver.optimizer_->set_options(std::move(o));
        StdoutCapture capture;
        solver.optimize(hs071_start());
        printed = capture.text();
    }
    EXPECT_NE(printed.find("hven Interior-Point Solver"), std::string::npos);
    EXPECT_NE(printed.find("Problem Statistics"), std::string::npos);
    EXPECT_NE(printed.find("KKT-Matrix NNZ%"), std::string::npos);
    EXPECT_NE(printed.find("|Iter| mu Val"), std::string::npos);
    EXPECT_NE(printed.find("Beginning"), std::string::npos);
    EXPECT_NE(printed.find("Total Solve Time"), std::string::npos);
    // The NARROW layout, which is the default: the wide header's own columns
    // are absent.
    EXPECT_EQ(printed.find("Max EMult"), std::string::npos);

    std::string silent;
    {
        auto solver = silent_hs071();
        StdoutCapture capture;
        solver->optimize(hs071_start());
        silent = capture.text();
    }
    EXPECT_EQ(silent, "");
}

TEST(IpmConsole, TheConsoleDoesNotDisplaceAUserSink) {
    // The fan-out's invariant on the interior-point side: the caller's stream
    // is byte-identical with printing on and off.
    auto run = [](int print_level) {
        NLPSolver solver(std::make_shared<Hs071Problem>());
        auto o = solver.optimizer_->options();
        o.common.print_level = print_level;
        solver.optimizer_->set_options(std::move(o));
        std::ostringstream os;
        JsonLinesTraceSink sink(os);
        solver.optimizer_->attach_trace(&sink);
        StdoutCapture capture;
        solver.optimize(hs071_start());
        return std::pair<std::string, std::string>{os.str(), capture.text()};
    };
    const auto silent = run(10);
    const auto printing = run(0);
    EXPECT_FALSE(silent.first.empty());

    // THE `ipm.solve.end` LINE IS EXCLUDED FROM THE BYTE COMPARISON, and only
    // that line: every one of its seven `_s` fields is WALL-CLOCK, which
    // CLAUDE.md section 7 makes informational and never asserted -- two runs of
    // the same solve differ there by construction, console or no console. What
    // is compared byte for byte is the whole rest of the stream (the begin line
    // and every `ipm.iter` row), and the end line is compared on its two
    // DETERMINISTIC fields.
    auto without_end = [](const std::string &stream) {
        std::vector<std::string> kept;
        for (const std::string &l : split_lines(stream)) {
            if (field(l, "ev") != "\"ipm.solve.end\"") {
                kept.push_back(l);
            }
        }
        return kept;
    };
    EXPECT_EQ(without_end(silent.first), without_end(printing.first))
        << "the console perturbed the caller's stream";
    const std::vector<std::string> silent_end = lines_of_event(silent.first, "ipm.solve.end");
    const std::vector<std::string> printing_end = lines_of_event(printing.first, "ipm.solve.end");
    ASSERT_EQ(silent_end.size(), 1u);
    ASSERT_EQ(printing_end.size(), 1u);
    EXPECT_EQ(field(silent_end[0], "status"), field(printing_end[0], "status"));
    EXPECT_EQ(field(silent_end[0], "iters"), field(printing_end[0], "iters"));

    EXPECT_EQ(silent.second, "");
    EXPECT_FALSE(printing.second.empty());
}

TEST(IpmConsole, AttachTraceDuringASolveIsRefusedAndTheConsoleRunsOnUnbroken) {
    // THE IN-FLIGHT RULE (M6 W5 T8.7 fix1, the lane's M1 / astra's I1), the
    // twin of `SqpConsole.AttachTraceDuringASolveIsRefused` on this engine.
    //
    // WHAT THE DEFECT WAS. `attach_trace` wrote `trace_` unconditionally, and
    // `trace_` is the COMPOSITION -- the fan-out over the caller's sink and the
    // console. A call made from inside an iteration callback therefore replaced
    // the composition for the remainder of the solve: the console went silent
    // in the middle of its table and every later event went to the new sink
    // alone. Refusing is what the declaration already promised ("composed at
    // solve entry ... fixed for the solve").
    //
    // CONTINUITY IS THE POINT, so it is what this pins: the callback catches
    // the refusal and lets the solve run on, and the console's output is
    // BYTE-IDENTICAL to the same solve with no such callback at all.
    // ONE THROWAWAY SOLVE FIRST, and this is why: " Solver Initialization : X
    // ms" is printed by `ensure_solver_initialized()` only when the
    // PROCESS-GLOBAL initialization actually ran and took longer than half a
    // millisecond -- so whichever arm below runs first would print a line the
    // other does not, whatever either arm does with its sink. Warming it makes
    // the two arms' line structure identical by construction rather than by
    // luck.
    {
        silent_hs071()->optimize(hs071_start());
    }

    auto printing_solve = [](bool attack) {
        NLPSolver solver(std::make_shared<Hs071Problem>());
        auto o = solver.optimizer_->options();
        o.common.print_level = 0;
        solver.optimizer_->set_options(std::move(o));
        std::ostringstream os;
        JsonLinesTraceSink usurper(os);
        Index refusals = 0;
        if (attack) {
            solver.optimizer_->set_iteration_callback([&](const IterationEvent &) {
                try {
                    solver.optimizer_->attach_trace(&usurper);
                } catch (const std::logic_error &) {
                    ++refusals;
                }
                return CallbackAction::kContinue;
            });
        }
        std::string printed;
        {
            hven::testing::StdoutCapture capture;
            EXPECT_EQ(solver.optimize(hs071_start()), SolveStatus::kOptimal);
            printed = capture.text();
        }
        // The usurper never received a line: it was never attached.
        EXPECT_EQ(os.str(), "");
        return std::pair<std::string, Index>{printed, refusals};
    };
    const auto quiet = printing_solve(false);
    const auto attacked = printing_solve(true);
    EXPECT_GT(attacked.second, 0) << "premise: the callback really did try, every iteration";
    EXPECT_EQ(quiet.second, 0);
    ASSERT_FALSE(quiet.first.empty());
    EXPECT_EQ(strip_timing(quiet.first), strip_timing(attacked.first))
        << "a refused mid-solve attach must leave the console's table untouched";

    // ... and it is legal again the moment the solve has returned.
    NLPSolver after(std::make_shared<Hs071Problem>());
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    EXPECT_NO_THROW(after.optimizer_->attach_trace(&sink));
}

TEST(IpmConsole, ASinkThatDetachesItselfInsideOnIpmIterThrowsRatherThanCrashing) {
    // THE NULL DEREFERENCE THIS CLOSES (astra's I1). The restoration door emits
    // TWICE in a row -- the row's `ipm.iter` and then the door's own marker --
    // and the second emit re-read the member. A sink that called
    // `attach_trace(nullptr)` from inside `on_ipm_iter` therefore made the very
    // next statement dereference a null pointer. Two things now stop that: the
    // in-flight refusal below, which makes the detach itself illegal, and the
    // local the emit sites read the member into once per PAIR.
    //
    // HS071 is enough to reach an `ipm.iter` -- the FIRST one -- and what is
    // pinned is that the process leaves through an exception rather than a
    // signal.
    struct SelfDetachingSink final : TraceSink {
        InteriorPointSolver *solver = nullptr;
        Index rows = 0;
        void on_ipm_iter(const IpmIterTraceEvent &) override {
            ++rows;
            solver->attach_trace(nullptr); // refused: a solve is in flight
        }
        // The six QP-tier virtuals are pure on `TraceSink`; this sink is on the
        // interior-point side and never sees one.
        void on_ipqp_iter(const IpqpTraceIterEvent &) override {}
        void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
        void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
        void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
        void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
        void on_ipqp_escape(const IpqpTraceEscapeEvent &) override {}
        void on_qp_mode(const QpModeTraceEvent &) override {}
        void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {}
    };
    NLPSolver solver(std::make_shared<Hs071Problem>());
    {
        auto o = solver.optimizer_->options();
        o.common.print_level = 10;
        solver.optimizer_->set_options(std::move(o));
    }
    SelfDetachingSink sink;
    sink.solver = solver.optimizer_.get();
    solver.optimizer_->attach_trace(&sink);
    EXPECT_THROW(solver.optimize(hs071_start()), std::logic_error);
    EXPECT_GE(sink.rows, 1) << "premise: the sink saw a row before it tried to detach";
}

TEST(IpmConsole, TheWideLayoutIsTheSolversOwnOptionAndReachesItsConsole) {
    std::string printed;
    {
        NLPSolver solver(std::make_shared<Hs071Problem>());
        auto o = solver.optimizer_->options();
        o.common.print_level = 0;
        o.wide_console = true;
        solver.optimizer_->set_options(std::move(o));
        StdoutCapture capture;
        solver.optimize(hs071_start());
        printed = capture.text();
    }
    EXPECT_NE(printed.find("Max EMult"), std::string::npos);
    EXPECT_NE(printed.find("Merit Val"), std::string::npos);
}

} // namespace
} // namespace hven::solvers
