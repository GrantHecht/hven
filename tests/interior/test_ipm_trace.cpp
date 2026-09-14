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

#include <bit>
#include <cstddef>
#include <cstdint>
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
#include "../common_support/console_capture.h"
#include "hven/core/ledger.h"
#include "hven/drivers/console_trace_sink.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/drivers/trace_writer.h"
#include "hven/model/nlp_model.h"
#include "hven/model/nlp_triplet_model.h"
#include "hven/model/non_linear_program.h"

#include "declared_route.h" // NOLINT(build/include_subdir)

namespace hven::solvers {
namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// ===========================================================================
// The fixtures
// ===========================================================================

/// @brief The canonical HS071 cell -- the same problem `test_ipm_solver_entry.cpp`
/// pins the interior-point driver's optimum on, so the stream is taken over a
/// trajectory that is already asserted elsewhere.
struct Hs071Problem : NlpTripletModel {
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
struct TwoVarProblem : NlpTripletModel {
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

/// @brief THE DETERMINISTIC FACTOR-TIME MESSAGE FIXTURE (M6 W5 T8.7b fix1,
/// astra's I2 with the lane's section 2.2 ruling).
///
/// A two-variable box-constrained problem whose objective is CONCAVE --
/// `f = -50 (x0^2 + x1^2)`, so the Hessian is `-100 I` at every point and the
/// KKT system's (1,1) block is negative definite at the start point, well
/// beyond anything the bound-barrier diagonal adds back. Its FIRST
/// factorization therefore has the wrong inertia by construction.
///
/// Paired with `max_refac = 0` -- legal, validated `>= 0` only
/// (`ipm_options.cpp:160`), and the ladder is `for (i = 0; i < max_refac; ++i)`
/// -- the wrong-inertia factorization falls straight through an EMPTY ladder to
/// the `inertia_exhausted` emit at `factor_impl`'s tail, on iteration 0, with
/// `k == 0`. No backend behaviour is being relied on to produce it, which is
/// why this is the fixture rather than a duplicated-row `rank_deficiency`
/// probe: whether Pardiso reports a duplicated equality row as an inertia
/// mismatch or as a perturbed pivot is the BACKEND's choice and would make the
/// pin MKL-only.
struct NonconvexProblem : NlpTripletModel {
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
        f = -50.0 * (x[0] * x[0] + x[1] * x[1]);
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = -100.0 * x[0];
        g[1] = -100.0 * x[1];
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
        v[0] = -100.0 * obj_factor;
        v[1] = -100.0 * obj_factor;
    }
    std::string name() const override { return "NonconvexProblem"; }
};

Eigen::VectorXd nonconvex_start() {
    Eigen::VectorXd x0(2);
    x0 << 0.5, 0.5;
    return x0;
}

/// A solver over `NonconvexProblem` with the empty ladder and no console, with
/// the program it solves (M6 W5 T8.9: declared_route.h's IpmCase).
hven_interior_tests::IpmCase nonconvex_solver() {
    hven_interior_tests::IpmCase c{
        hven::solvers::make_nlp_program(std::make_shared<NonconvexProblem>()),
        std::make_unique<hven::solvers::IpmSolver>()};
    auto o = c.engine->options();
    o.common.print_level = 10;
    o.max_refac = 0;
    c.engine->set_options(std::move(o));
    return c;
}

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

/// @brief The SQP side of pin (v): the smallest model that makes `SqpSolver`
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
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    CallbackOracle oracle;
    solver.attach_trace(&sink);
    solver.set_iteration_callback(oracle.hook());

    result = solver.solve(*program, hs071_start());
    const hven::solvers::SolveStatus flag = result.status;
    ASSERT_EQ(flag, hven::solvers::SolveStatus::kOptimal);

    const std::vector<std::string> iter_lines = lines_of_event(os.str(), "ipm.iter");
    const Index reported = result.iterations;
    ASSERT_GT(reported, 1);
    EXPECT_EQ(static_cast<Index>(iter_lines.size()), reported);
    EXPECT_EQ(oracle.seen.size(), iter_lines.size());
}

TEST(IpmTrace, EveryIterLineIsTheRecordTheCallbackSawInTheSameOrder) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    RecordOracle records(&sink);
    CallbackOracle oracle;
    solver.attach_trace(&records);
    solver.set_iteration_callback(oracle.hook());
    result = solver.solve(*program, hs071_start());
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);

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
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    CallbackOracle oracle;
    solver.attach_trace(&sink);
    solver.set_iteration_callback(oracle.hook());
    result = solver.solve(*program, hs071_start());
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);

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
    const auto classic_program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver classic;
    hven::solvers::IpmResult classic_result;
    {
        auto o = classic.options();
        o.common.print_level = 10;
        classic.set_options(std::move(o));
    }
    std::ostringstream os_classic;
    JsonLinesTraceSink sink_classic(os_classic);
    classic.attach_trace(&sink_classic);
    classic_result = classic.solve(*classic_program, hs071_start());
    ASSERT_EQ(classic_result.status, hven::solvers::SolveStatus::kOptimal);
    for (const std::string &l : lines_of_event(os_classic.str(), "ipm.iter")) {
        EXPECT_EQ(field(l, "prox_reg_primal"), "null");
        EXPECT_EQ(field(l, "prox_reg_dual"), "null");
    }

    const auto prox_program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver prox;
    hven::solvers::IpmResult prox_result;
    {
        auto o = prox.options();
        o.common.print_level = 10;
        prox.set_options(std::move(o));
    }
    {
        auto o = prox.options();
        o.inertia_mode = InertiaModes::proximal_regularization;
        prox.set_options(std::move(o));
    }
    std::ostringstream os_prox;
    JsonLinesTraceSink sink_prox(os_prox);
    prox.attach_trace(&sink_prox);
    prox.solve(*prox_program, hs071_start());
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
    const auto converged_program =
        hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver converged;
    hven::solvers::IpmResult converged_result;
    {
        auto o = converged.options();
        o.common.print_level = 10;
        converged.set_options(std::move(o));
    }
    std::ostringstream os_c;
    JsonLinesTraceSink sink_c(os_c);
    converged.attach_trace(&sink_c);
    converged_result = converged.solve(*converged_program, hs071_start());
    ASSERT_EQ(converged_result.status, hven::solvers::SolveStatus::kOptimal);
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

    const auto truncated_program =
        hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver truncated;
    hven::solvers::IpmResult truncated_result;
    {
        auto o = truncated.options();
        o.common.print_level = 10;
        o.max_iters = 3;
        truncated.set_options(std::move(o));
    }
    std::ostringstream os_t;
    JsonLinesTraceSink sink_t(os_t);
    truncated.attach_trace(&sink_t);
    truncated_result = truncated.solve(*truncated_program, hs071_start());
    ASSERT_EQ(truncated_result.status, hven::solvers::SolveStatus::kMaxIter);
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
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.attach_trace(&sink);
    result = solver.solve(*program, hs071_start());
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);

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
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.attach_trace(&sink);
    result = solver.solve(*program, hs071_start());
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);

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
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        o.max_iters = 40;
        solver.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.attach_trace(&sink);
    result = solver.solve(*program, hs071_start());
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);

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
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.attach_trace(&sink);
    {
        auto o = solver.options();
        o.phases = {IpmPhase::kSolve, IpmPhase::kOptimize}; // what solve_optimize() ran
        solver.set_options(std::move(o));
    }
    result = solver.solve(*program, hs071_start());

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
    const auto converged_program =
        hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver converged;
    hven::solvers::IpmResult converged_result;
    {
        auto o = converged.options();
        o.common.print_level = 10;
        converged.set_options(std::move(o));
    }
    std::ostringstream os_c;
    JsonLinesTraceSink sink_c(os_c);
    converged.attach_trace(&sink_c);
    converged_result = converged.solve(*converged_program, hs071_start());
    ASSERT_EQ(converged_result.status, hven::solvers::SolveStatus::kOptimal);
    const std::vector<std::string> end_c = lines_of_event(os_c.str(), "ipm.solve.end");
    ASSERT_EQ(end_c.size(), 1u);
    // THE VOCABULARY MOVED IN M6 W5 T8.4, declared: the event carries SolveStatus
    // now (the engine's own ConvergenceFlags is gone), so `converged` reads
    // `optimal` and `not_converged` reads `max_iter` -- or `stalled` at the two
    // abnormal exits the old vocabulary could not tell apart at all.
    EXPECT_EQ(field(end_c.front(), "status"), "\"optimal\"");
    EXPECT_EQ(field(end_c.front(), "iters"), std::to_string(converged_result.iterations));

    const auto truncated_program =
        hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver truncated;
    hven::solvers::IpmResult truncated_result;
    {
        auto o = truncated.options();
        o.common.print_level = 10;
        o.max_iters = 3;
        truncated.set_options(std::move(o));
    }
    std::ostringstream os_t;
    JsonLinesTraceSink sink_t(os_t);
    truncated.attach_trace(&sink_t);
    truncated_result = truncated.solve(*truncated_program, hs071_start());
    ASSERT_EQ(truncated_result.status, hven::solvers::SolveStatus::kMaxIter);
    const std::vector<std::string> end_t = lines_of_event(os_t.str(), "ipm.solve.end");
    ASSERT_EQ(end_t.size(), 1u);
    EXPECT_EQ(field(end_t.front(), "status"), "\"max_iter\"");
    EXPECT_EQ(field(end_t.front(), "iters"), "3");
}

TEST(IpmTrace, ARefusedCallWritesNoLineAtAll) {
    // The `begin` emit sits AFTER every argument refusal, so a call that never
    // ran leaves no opening line dangling in the artifact.
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.attach_trace(&sink);
    Eigen::VectorXd wrong(3);
    wrong << 1.0, 1.0, 1.0;
    EXPECT_THROW((void)solver.solve(*program, wrong), std::invalid_argument);
    EXPECT_EQ(os.str(), "");
    EXPECT_EQ(sink.lines_written(), 0);
    EXPECT_EQ(sink.depth(), 0);
}

// ===========================================================================
// (iv) THE NULL SINK CHANGES NOTHING
// ===========================================================================

TEST(IpmTrace, AttachingASinkMovesNoResultField) {
    const auto bare_program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver bare;
    hven::solvers::IpmResult bare_result;
    {
        auto o = bare.options();
        o.common.print_level = 10;
        bare.set_options(std::move(o));
    }
    bare_result = bare.solve(*bare_program, hs071_start());
    const hven::solvers::SolveStatus bare_flag = bare_result.status;
    const hven::solvers::IpmResult &b = bare_result;
    const int bare_iters = b.iterations;
    const double bare_obj = b.f;
    const double bare_kkt = b.kkt_inf;
    const double bare_barr = b.barr_inf;
    const double bare_econ = b.econ_inf;
    const double bare_icon = b.icon_inf;
    const Eigen::VectorXd bare_x = bare_result.x;

    const auto traced_program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver traced;
    hven::solvers::IpmResult traced_result;
    {
        auto o = traced.options();
        o.common.print_level = 10;
        traced.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    traced.attach_trace(&sink);
    traced_result = traced.solve(*traced_program, hs071_start());
    const hven::solvers::SolveStatus traced_flag = traced_result.status;
    const hven::solvers::IpmResult &t = traced_result;

    EXPECT_EQ(traced_flag, bare_flag);
    EXPECT_EQ(t.iterations, bare_iters);
    EXPECT_EQ(t.f, bare_obj);
    EXPECT_EQ(t.kkt_inf, bare_kkt);
    EXPECT_EQ(t.barr_inf, bare_barr);
    EXPECT_EQ(t.econ_inf, bare_econ);
    EXPECT_EQ(t.icon_inf, bare_icon);
    EXPECT_EQ(traced_result.x, bare_x);
    // NON-VACUITY: the sink really did run.
    EXPECT_GT(sink.lines_written(), 2);
}

TEST(IpmTrace, DetachingMidLifetimeStopsTheStreamAndChangesNothingElse) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.attach_trace(&sink);
    result = solver.solve(*program, hs071_start());
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    const Index first = sink.lines_written();
    ASSERT_GT(first, 0);

    solver.attach_trace(nullptr);
    result = solver.solve(*program, hs071_start());
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
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
    SqpSolver driver(opts);
    driver.attach_trace(&sink);
    TinySqpModel model;
    const SqpResult sqp_out = driver.solve(model, model.start_point());
    ASSERT_GT(sink.lines_written(), 0);
    EXPECT_EQ(sqp_out.status, SolveStatus::kOptimal);
    const Index after_sqp = sink.lines_written();

    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    solver.attach_trace(&sink);
    result = solver.solve(*program, hs071_start());
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
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
/// (tests/common_support/console_capture.h) is that redirection, in one portable
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

/// Replaces every WALL-CLOCK value in a JSON-lines stream with a fixed token
/// (M6 W5 T8.7b).
///
/// WHY A MASK RATHER THAN A DROPPED LINE. Two runs of one solve differ in
/// every `_s` field by construction -- CLAUDE.md section 7 makes those
/// informational and never asserted -- so a byte comparison of two streams has
/// to do something about them. Dropping the lines that carry one would throw
/// away every other key on those lines; masking keeps them. The list is the
/// whole of the schema's wall-clock vocabulary: `ipm.solve.end`'s seven,
/// `ipm.kkt_analysis`'s one, and `ipm.phase.exit`'s five (its own four plus the
/// embedded report's `phase_seconds`).
std::string mask_wall_clock(const std::string &stream) {
    static const std::vector<std::string> kTimeKeys = {
        "total_time_s", "pre_time_s",      "func_time_s",
        "kkt_time_s",   "print_time_s",    "solver_init_time_s",
        "misc_time_s",  "analysis_time_s", "total_s",
        "func_s",       "kkt_s",           "print_s",
        "phase_seconds"};
    std::string out = stream;
    for (const std::string &k : kTimeKeys) {
        const std::string needle = "\"" + k + "\":";
        std::size_t pos = 0;
        while ((pos = out.find(needle, pos)) != std::string::npos) {
            const std::size_t vstart = pos + needle.size();
            std::size_t vend = vstart;
            while (vend < out.size() && out[vend] != ',' && out[vend] != '}') {
                ++vend;
            }
            out.replace(vstart, vend - vstart, "<t>");
            pos = vstart + 3;
        }
    }
    return out;
}

/// A silent solver on HS071 -- `print_level` 10 -- so a ledger pin does not
/// also print a table into the test log.
hven_interior_tests::IpmCase silent_hs071() {
    hven_interior_tests::IpmCase c{
        hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>()),
        std::make_unique<hven::solvers::IpmSolver>()};
    auto o = c.engine->options();
    o.common.print_level = 10;
    c.engine->set_options(std::move(o));
    return c;
}

} // namespace

TEST(IpmLedger, OneRecordPerSolveCarryingThatCallsOwnCounters) {
    auto solver = silent_hs071();
    Ledger ledger;
    solver.engine->attach_ledger(&ledger, "ipm");
    // EACH RECORD AGAINST ITS OWN CALL'S RESULT (M6 W5 T8.7 fix1, astra's
    // Minor): each call's result is held in its own value, so record 0 is
    // compared against the call that wrote it. Comparing record 0 against the
    // SECOND result would pass on this fixture only because the two calls
    // agree, and would go on passing if the record were written from the wrong
    // call. (M6 W5 T8.9: the engine returns its result by value, so this is
    // simply the two return values.)
    const IpmResult first_result = solver.engine->solve(*solver.program, hs071_start());
    ASSERT_EQ(first_result.status, SolveStatus::kOptimal);
    const IpmResult second_result = solver.engine->solve(*solver.program, hs071_start());
    ASSERT_EQ(second_result.status, SolveStatus::kOptimal);

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
    solver.engine->attach_ledger(&ledger, "reuse");
    const IpmResult first_result = solver.engine->solve(*solver.program, hs071_start());
    ASSERT_EQ(first_result.status, SolveStatus::kOptimal);
    const Index lifetime_after_first = first_result.kkt_factor_counters.factorize_count;
    const IpmResult second_result = solver.engine->solve(*solver.program, hs071_start());
    ASSERT_EQ(second_result.status, SolveStatus::kOptimal);
    const Index lifetime_after_second = second_result.kkt_factor_counters.factorize_count;

    ASSERT_EQ(ledger.ipm_records().size(), 2u);
    EXPECT_GT(lifetime_after_second, lifetime_after_first)
        << "premise: the second solve pays factorizations of its own";
    EXPECT_EQ(ledger.ipm_records()[1].factorizations, lifetime_after_second - lifetime_after_first);
    EXPECT_LT(ledger.ipm_records()[1].factorizations, lifetime_after_second)
        << "the record reports THIS call's factorizations, not the lifetime total";
    // `analyses` is already per call, and the SECOND solve on the same program
    // reuses the analysis -- so it is the honest 0 there.
    EXPECT_EQ(ledger.ipm_records()[1].analyses, second_result.kkt_analyses_this_call);
}

TEST(IpmLedger, ThePerCallDeltaSurvivesAReAnalysisInsideTheCall) {
    // M6 W5 T8.7 fix1 (the lane's M3). `FactorizationsAndAnalysesArePerCallNot
    // Lifetime` pins the delta on a solver REUSED ON ONE PROGRAM, where the
    // analysis is reused too. What it does not reach is the other reuse: the
    // same solver handed a DIFFERENT program, which re-lays the analysis inside
    // the call. The delta is only honest there if the factor's lifetime counter
    // is monotone across a re-analysis, and this says so out loud rather than
    // assuming it.
    //
    // (M6 W5 T8.7b, the lane's first rider: a sentence here used to add
    // "nothing in `linear/` resets it", which the premise block below
    // CONTRADICTS -- the re-lay replaces the engine and its per-instance
    // counters restart. What is monotone is `KktFactorization`'s own
    // accumulator, which is exactly what T8.7 fix1 added and what this pins.)
    auto hs = silent_hs071();
    // The second program, laid out and ready to be BORROWED by the first
    // solver: since M6 W5 T8.4 a program is an argument of solve(), so one
    // solver may be handed two.
    const auto other_program = hven::solvers::make_nlp_program(std::make_shared<TwoVarProblem>());

    Ledger ledger;
    hs.engine->attach_ledger(&ledger, "cross");
    const IpmResult first_result = hs.engine->solve(*hs.program, hs071_start());
    ASSERT_EQ(first_result.status, SolveStatus::kOptimal);
    const IpmResult second_result = hs.engine->solve(*other_program, two_var_start());

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
    both.engine->attach_ledger(&ledger, "seq");
    {
        auto o = both.engine->options();
        o.phases = {IpmPhase::kSolve, IpmPhase::kOptimize}; // what solve_optimize() ran
        both.engine->set_options(std::move(o));
    }
    const IpmResult ran_both = both.engine->solve(*both.program, hs071_start());
    ASSERT_EQ(ran_both.status, SolveStatus::kOptimal);
    ASSERT_EQ(ran_both.phases.size(), 2u) << "premise: two phases were declared";
    EXPECT_TRUE(ran_both.phases[0].ran);
    EXPECT_TRUE(ran_both.phases[1].ran);
    ASSERT_EQ(ledger.ipm_records().size(), 1u);
    EXPECT_EQ(ledger.ipm_records()[0].phases_run, 2);

    // THE SKIPPED PHASE: the callback stops the solve inside phase 0, and the
    // sequence ends there.
    auto stopped = silent_hs071();
    Ledger stop_ledger;
    stopped.engine->attach_ledger(&stop_ledger, "stop");
    stopped.engine->set_iteration_callback(
        [](const IterationEvent &) { return CallbackAction::kStop; });
    {
        auto o = stopped.engine->options();
        o.phases = {IpmPhase::kSolve, IpmPhase::kOptimize};
        stopped.engine->set_options(std::move(o));
    }
    const IpmResult stopped_result = stopped.engine->solve(*stopped.program, hs071_start());
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
    solver.engine->attach_ledger(&ledger, "boom");
    ASSERT_EQ(solver.engine->solve(*solver.program, hs071_start()).status, SolveStatus::kOptimal);
    ASSERT_EQ(ledger.ipm_records().size(), 1u);

    Index fired = 0;
    solver.engine->set_iteration_callback([&](const IterationEvent &) -> CallbackAction {
        ++fired;
        throw std::runtime_error("from inside the callback, after work began");
    });
    EXPECT_THROW(solver.engine->solve(*solver.program, hs071_start()), std::runtime_error);
    EXPECT_GE(fired, 1) << "premise: the solve had begun iterating";
    EXPECT_EQ(ledger.ipm_records().size(), 1u) << "a throw out of a solve records nothing";

    // ... and the number it did not consume is the next one.
    solver.engine->clear_iteration_callback();
    ASSERT_EQ(solver.engine->solve(*solver.program, hs071_start()).status, SolveStatus::kOptimal);
    ASSERT_EQ(ledger.ipm_records().size(), 2u);
    EXPECT_EQ(ledger.ipm_records()[1].label, "boom_1");
}

TEST(IpmLedger, AttachResetsTheCounterAndADetachStopsRecording) {
    auto solver = silent_hs071();
    Ledger first_ledger;
    solver.engine->attach_ledger(&first_ledger, "a");
    ASSERT_EQ(solver.engine->solve(*solver.program, hs071_start()).status, SolveStatus::kOptimal);
    EXPECT_EQ(first_ledger.ipm_records().size(), 1u);

    Ledger second_ledger;
    solver.engine->attach_ledger(&second_ledger, "b");
    ASSERT_EQ(solver.engine->solve(*solver.program, hs071_start()).status, SolveStatus::kOptimal);
    ASSERT_EQ(second_ledger.ipm_records().size(), 1u);
    EXPECT_EQ(second_ledger.ipm_records()[0].label, "b_0")
        << "attach_ledger restarts the label sequence";
    EXPECT_EQ(first_ledger.ipm_records().size(), 1u) << "the old ledger stopped receiving";

    solver.engine->attach_ledger(nullptr, "");
    ASSERT_EQ(solver.engine->solve(*solver.program, hs071_start()).status, SolveStatus::kOptimal);
    EXPECT_EQ(second_ledger.ipm_records().size(), 1u);
}

TEST(IpmLedger, ARefusedCallRecordsNothingAndConsumesNoLabel) {
    auto solver = silent_hs071();
    Ledger ledger;
    solver.engine->attach_ledger(&ledger, "throw");
    // One good solve first, which is also what TRANSCRIBES the program this
    // test then hands a bad start to.
    ASSERT_EQ(solver.engine->solve(*solver.program, hs071_start()).status, SolveStatus::kOptimal);
    ASSERT_EQ(ledger.ipm_records().size(), 1u);
    EXPECT_EQ(ledger.ipm_records()[0].label, "throw_0");

    // A start vector of the wrong length is refused at the public boundary,
    // ABOVE the funnel that writes the record.
    Eigen::VectorXd bad(3);
    bad << 1.0, 1.0, 1.0;
    EXPECT_THROW(solver.engine->solve(*solver.program, bad), std::invalid_argument);
    EXPECT_EQ(ledger.ipm_records().size(), 1u) << "a refused call records nothing";

    ASSERT_EQ(solver.engine->solve(*solver.program, hs071_start()).status, SolveStatus::kOptimal);
    ASSERT_EQ(ledger.ipm_records().size(), 2u);
    EXPECT_EQ(ledger.ipm_records()[1].label, "throw_1") << "the refusal consumed no number";
}

TEST(IpmLedger, TheSummaryTableIsEmptyWithoutRecordsAndHasARowPerRecordWithThem) {
    Ledger empty;
    EXPECT_EQ(empty.ipm_summary_table(), "");

    auto solver = silent_hs071();
    Ledger ledger;
    solver.engine->attach_ledger(&ledger, "tbl");
    ASSERT_EQ(solver.engine->solve(*solver.program, hs071_start()).status, SolveStatus::kOptimal);
    ASSERT_EQ(solver.engine->solve(*solver.program, hs071_start()).status, SolveStatus::kOptimal);

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
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        o.wide_console = true;
        solver.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.attach_trace(&sink);
    result = solver.solve(*program, hs071_start());
    ASSERT_EQ(result.status, SolveStatus::kOptimal);

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

// ===========================================================================
// M6 W5 T8.7b -- THE PHASE EVENTS ON A LIVE SOLVE.
//
// The engine's last direct prints are events. These pin the SHAPE of the
// stream they make -- the per-phase bracket, the analysis count, the embedded
// report, and the line arithmetic -- where the BYTES are pinned by
// tests/drivers/test_console_sink.cpp and by the live-transcript leg.
// ===========================================================================

namespace {

/// One live IPM stream, with the events this task added counted by name.
struct StreamShape {
    std::string text;
    std::vector<std::string> order; ///< Every `ipm.*` event name, in order.
    Index begin = 0;
    Index end = 0;
    Index phase_begin = 0;
    Index phase_end = 0;
    Index phase_exit = 0;
    Index analysis = 0;
    Index iter = 0;
    Index message = 0;
    Index door = 0;
    Index lines = 0;
};

StreamShape shape_of(const std::string &stream) {
    StreamShape sh;
    sh.text = stream;
    for (const std::string &l : split_lines(stream)) {
        ++sh.lines;
        std::string ev = field(l, "ev");
        // `field` returns the RAW token, quotes included.
        if (ev.size() >= 2) {
            ev = ev.substr(1, ev.size() - 2);
        }
        sh.order.push_back(ev);
        sh.begin += (ev == "ipm.solve.begin") ? 1 : 0;
        sh.end += (ev == "ipm.solve.end") ? 1 : 0;
        sh.phase_begin += (ev == "ipm.phase.begin") ? 1 : 0;
        sh.phase_end += (ev == "ipm.phase.end") ? 1 : 0;
        sh.phase_exit += (ev == "ipm.phase.exit") ? 1 : 0;
        sh.analysis += (ev == "ipm.kkt_analysis") ? 1 : 0;
        sh.iter += (ev == "ipm.iter") ? 1 : 0;
        sh.message += (ev == "ipm.message") ? 1 : 0;
        sh.door += (ev == "ipm.restoration_exit_row") ? 1 : 0;
    }
    return sh;
}

} // namespace

TEST(IpmPhaseEvents, TheLineCountIsTheDeclaredArithmeticOnThreePhaseShapes) {
    // THE ARITHMETIC, DECLARED BEFORE THE RUN and stated here in the form the
    // CODE makes true:
    //
    //     lines = 2 + 3P + A + R + M + D
    //
    // with P the phases that RAN (each writing `phase.begin`, `phase.exit` and
    // `phase.end`), A the KKT analyses, R the `ipm.iter` rows, M the messages
    // and D the restoration-door markers.
    //
    // A IS NOT P, and that is the one place this differs from the brief's §5
    // A2, which lists `kkt_analysis` among the four things "per phase ran".
    // The brief's own §2 says "+1 per KKT analysis", which is what the engine
    // does: `init_impl` runs once before the loop and once more at the END of
    // every phase body that is not the last STEP -- ahead of the next
    // iteration's conditional-skip test. So a sequence whose second phase is
    // SKIPPED still pays for its analysis:
    //
    //     A = 1 + #{phases that ran, were not the last step, and did not break}
    //
    // On the two arms where every requested phase runs, A == P and the formula
    // reduces to A2's `2 + 4P` exactly. The third arm below is the one that
    // separates them. DECLARED, not changed silently -- see
    // `.superpowers/w5-t8-7b-progress.md` §1b.
    auto run = [](int which) {
        const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
        hven::solvers::IpmSolver solver;
        hven::solvers::IpmResult result;
        {
            auto o = solver.options();
            o.common.print_level = 10;
            solver.set_options(std::move(o));
        }
        std::ostringstream os;
        JsonLinesTraceSink sink(os);
        solver.attach_trace(&sink);
        if (which == 0) {
            result = solver.solve(*program, hs071_start());
            EXPECT_EQ(result.status, SolveStatus::kOptimal);
        } else if (which == 1) {
            auto o = solver.options();
            o.phases = {IpmPhase::kSolve, IpmPhase::kOptimize}; // what solve_optimize() ran
            solver.set_options(std::move(o));
            result = solver.solve(*program, hs071_start());
        } else {
            auto o = solver.options();
            o.phases = {IpmPhase::kOptimize, IpmPhase::kSolve}; // what optimize_solve() ran
            solver.set_options(std::move(o));
            result = solver.solve(*program, hs071_start());
            EXPECT_EQ(result.status, SolveStatus::kOptimal);
        }
        return shape_of(os.str());
    };

    // (a) COLD `optimize()`: one requested phase, one run, one analysis.
    {
        const StreamShape sh = run(0);
        EXPECT_EQ(sh.begin, 1);
        EXPECT_EQ(sh.end, 1);
        EXPECT_EQ(sh.phase_begin, 1);
        EXPECT_EQ(sh.phase_exit, 1);
        EXPECT_EQ(sh.phase_end, 1);
        EXPECT_EQ(sh.analysis, 1);
        const Index p = sh.phase_begin;
        EXPECT_EQ(sh.lines, 2 + 3 * p + sh.analysis + sh.iter + sh.message + sh.door);
        // A2's own form, which holds here because A == P.
        EXPECT_EQ(sh.analysis, p);
        EXPECT_EQ(sh.lines, 2 + 4 * p + sh.iter + sh.message + sh.door);
        // `phases` REQUESTED, read off the begin line rather than assumed equal
        // to the count that ran.
        EXPECT_EQ(field(lines_of_event(sh.text, "ipm.solve.begin").front(), "phases"), "1");
    }

    // (b) `{kSolve, kOptimize}`: the second phase is UNCONDITIONAL (a kOptimize
    // that follows a kSolve always runs), so both run and A == P == 2.
    {
        const StreamShape sh = run(1);
        EXPECT_EQ(field(lines_of_event(sh.text, "ipm.solve.begin").front(), "phases"), "2");
        EXPECT_EQ(sh.phase_begin, 2);
        EXPECT_EQ(sh.phase_exit, 2);
        EXPECT_EQ(sh.phase_end, 2);
        EXPECT_EQ(sh.analysis, 2);
        const Index p = sh.phase_begin;
        EXPECT_EQ(sh.lines, 2 + 3 * p + sh.analysis + sh.iter + sh.message + sh.door);
        EXPECT_EQ(sh.lines, 2 + 4 * p + sh.iter + sh.message + sh.door);
    }

    // (c) `{kOptimize, kSolve}`: the trailing kSolve is CONDITIONAL on the
    // optimize phase not converging. HS071 converges, so it is skipped -- and
    // the inter-phase re-initialization has ALREADY run and written its
    // analysis by then. P = 1, A = 2, and A2's `2 + 4P` is one line short.
    {
        const StreamShape sh = run(2);
        EXPECT_EQ(field(lines_of_event(sh.text, "ipm.solve.begin").front(), "phases"), "2")
            << "premise: two phases were REQUESTED";
        EXPECT_EQ(sh.phase_begin, 1) << "premise: the conditional second phase was skipped";
        EXPECT_EQ(sh.phase_exit, 1);
        EXPECT_EQ(sh.phase_end, 1);
        EXPECT_EQ(sh.analysis, 2) << "the inter-phase re-init runs before the skip test";
        const Index p = sh.phase_begin;
        EXPECT_EQ(sh.lines, 2 + 3 * p + sh.analysis + sh.iter + sh.message + sh.door);
        EXPECT_NE(sh.lines, 2 + 4 * p + sh.iter + sh.message + sh.door)
            << "and this is the arm on which the two forms differ";
        // `ran` is read off the exit line, not inferred from `phases`.
        EXPECT_EQ(field(lines_of_event(sh.text, "ipm.phase.exit").front(), "ran"), "true");
    }
}

TEST(IpmPhaseEvents, TheOrderIsAnalysisThenTheBracketWithTheExitInsideIt) {
    // THE ORDER THE ENGINE EMITS IN, pinned so a later change that moves one of
    // these has to say so. The analysis is OUTSIDE the bracket, ahead of it:
    // `init_impl` runs before the phase loop for the first phase and at the end
    // of the previous phase's body for every later one.
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.attach_trace(&sink);
    {
        auto o = solver.options();
        o.phases = {IpmPhase::kSolve, IpmPhase::kOptimize}; // what solve_optimize() ran
        solver.set_options(std::move(o));
    }
    result = solver.solve(*program, hs071_start());
    const StreamShape sh = shape_of(os.str());

    // The skeleton, with the rows and any messages removed.
    std::vector<std::string> skeleton;
    for (const std::string &ev : sh.order) {
        if (ev != "ipm.iter" && ev != "ipm.message" && ev != "ipm.restoration_exit_row") {
            skeleton.push_back(ev);
        }
    }
    const std::vector<std::string> expected = {
        "ipm.solve.begin", "ipm.kkt_analysis", "ipm.phase.begin", "ipm.phase.exit",
        "ipm.phase.end",   "ipm.kkt_analysis", "ipm.phase.begin", "ipm.phase.exit",
        "ipm.phase.end",   "ipm.solve.end"};
    EXPECT_EQ(skeleton, expected);

    // THE LABELS, and the phase indices they belong to. `solve_optimize` runs
    // the feasibility phase first.
    const std::vector<std::string> begins = lines_of_event(os.str(), "ipm.phase.begin");
    const std::vector<std::string> ends = lines_of_event(os.str(), "ipm.phase.end");
    ASSERT_EQ(begins.size(), 2u);
    ASSERT_EQ(ends.size(), 2u);
    EXPECT_EQ(field(begins[0], "label"), "\"Solve Algorithm \"");
    EXPECT_EQ(field(begins[0], "entry"), "\"solve\"");
    EXPECT_EQ(field(begins[0], "phase"), "0");
    EXPECT_EQ(field(begins[1], "label"), "\"Optimization Algorithm \"");
    EXPECT_EQ(field(begins[1], "entry"), "\"optimize\"");
    EXPECT_EQ(field(begins[1], "phase"), "1");
    // The closing line of a phase carries the same three values as its opener.
    for (std::size_t k = 0; k < 2; ++k) {
        EXPECT_EQ(field(ends[k], "label"), field(begins[k], "label"));
        EXPECT_EQ(field(ends[k], "entry"), field(begins[k], "entry"));
        EXPECT_EQ(field(ends[k], "phase"), field(begins[k], "phase"));
    }
    // THE FIRST ANALYSIS COMPUTES, the inter-phase one refactorizes.
    const std::vector<std::string> analyses = lines_of_event(os.str(), "ipm.kkt_analysis");
    ASSERT_EQ(analyses.size(), 2u);
    EXPECT_EQ(field(analyses[0], "docompute"), "true");
    EXPECT_EQ(field(analyses[1], "docompute"), "false");
    EXPECT_NE(field(analyses[0], "factor_mem"), "null");
    EXPECT_EQ(field(analyses[1], "factor_mem"), "null")
        << "a refactorization's figures are the previous analysis's, so they are absent";
}

TEST(IpmPhaseEvents, TheExitEventEmbedsTheReportTheSolveReturns) {
    // ONE SHAPE BY CONSTRUCTION. The event holds `IpmResult::phases[i]` itself,
    // so this compares the LINE against the RETURNED report field for field --
    // everything except `phase_seconds`, which is wall-clock and never asserted
    // (CLAUDE.md §7).
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    solver.attach_trace(&sink);
    {
        auto o = solver.options();
        o.phases = {IpmPhase::kSolve, IpmPhase::kOptimize}; // what solve_optimize() ran
        solver.set_options(std::move(o));
    }
    result = solver.solve(*program, hs071_start());

    const std::vector<std::string> exits = lines_of_event(os.str(), "ipm.phase.exit");
    ASSERT_EQ(exits.size(), 2u);
    ASSERT_EQ(result.phases.size(), 2u);
    for (std::size_t k = 0; k < exits.size(); ++k) {
        const IpmPhaseReport &report = result.phases[k];
        EXPECT_EQ(field(exits[k], "phase"), std::to_string(k));
        EXPECT_EQ(field(exits[k], "entry"),
                  std::string("\"") + (report.phase == IpmPhase::kOptimize ? "optimize" : "solve") +
                      "\"");
        EXPECT_EQ(field(exits[k], "status"), std::string("\"") + to_string(report.status) + "\"");
        EXPECT_EQ(field(exits[k], "iterations"), std::to_string(report.iterations));
        EXPECT_EQ(field(exits[k], "stop_reason"),
                  std::string("\"") + to_string(report.stop_reason) + "\"");
        EXPECT_EQ(field(exits[k], "ran"), report.ran ? "true" : "false");
        // AND THE ITERATION COUNT IS THE ROW COUNT of that phase: the `ms/iter`
        // divisor the console block uses is `iters.size()`, which is what the
        // report counts.
        Index rows = 0;
        for (const std::string &l : lines_of_event(os.str(), "ipm.iter")) {
            rows += (field(l, "phase") == std::to_string(k)) ? 1 : 0;
        }
        EXPECT_EQ(rows, report.iterations);
    }
}

// ===========================================================================
// THE DEFERRAL, PINNED FROM A FACTOR-TIME MESSAGE (M6 W5 T8.7b fix1)
//
// WHAT THE T8.7b PINS DID NOT ESTABLISH (astra's I2). The installing sink
// below used to install from `on_ipm_iter` AS WELL, and its premise asserted
// only that ROWS were seen -- so on HS071, where no factor-time warning is
// reached, the whole pin ran through the iteration fallback and proved nothing
// about a set made from inside a FACTORIZATION, which is the case the deferral
// exists for. It now installs from `on_ipm_message` ONLY, over a fixture whose
// first factorization raises `inertia_exhausted` by construction
// (`NonconvexProblem` + `max_refac = 0`), and asserts the KIND it saw.
//
// THE INITIALIZATION NOTICE CANNOT BE PINNED THE SAME WAY, and this is the
// place that says so. `solver_initialized` fires at most ONCE PER PROCESS --
// `ensure_solver_initialized()` is a `std::call_once` over a function-local
// `std::once_flag` (src/drivers/solver_init.cpp:19) with no reset -- and this
// file's tests share one process with every other interior test, in an order
// no test controls. No test here can be the process's first solve, so an
// assertion about a set made from that notice would be vacuous whenever it did
// not fire, which is always. What CAN be said, and is: the notice is emitted
// from `ensure_solver_initialized()`, called at `run_phase_sequence`'s
// :5535 -- INSIDE the solve guard armed at :5069 -- through the very
// `emit_message` every other kind goes through, so it takes the same
// `solve_in_flight_` branch of the setter that the `inertia_exhausted` pin
// below exercises. The branch does not read the kind.
// ===========================================================================

TEST(IpmDeferral, AHookInstalledFromInsideAFactorTimeMessageReachesTheNEXTSolve) {
    // M6 W5 T8.7b (the lane's Q5). The six setters DEFER while a solve is in
    // flight. Before this task a `set_kkt_hook` made from inside a SINK method
    // -- which since T8.7b fires from inside a factorization -- took the direct
    // branch and armed the hook mid-iteration; now it lands at the next solve's
    // entry, and THIS solve is bitwise the solve it would have been.
    struct HookInstallingSink final : TraceSink {
        IpmSolver *solver = nullptr;
        Index *hook_calls = nullptr;
        Index messages = 0;
        Index rows = 0;
        bool saw_inertia_exhausted = false;
        bool installed = false;
        // ONE ORIGIN ONLY: `on_ipm_message`. There is deliberately no install
        // from `on_ipm_iter` -- that fallback is what made this pin vacuous.
        void on_ipm_message(const IpmMessageTraceEvent &e) override {
            ++messages;
            if (e.kind == IpmMessageKind::kInertiaExhausted) {
                saw_inertia_exhausted = true;
            }
            if (installed) {
                return;
            }
            installed = true;
            solver->set_kkt_hook([this](int, double, hven::ConstEigenRef<Eigen::VectorXd>, double,
                                        hven::ConstEigenRef<Eigen::VectorXd>,
                                        hven::ConstEigenRef<Eigen::VectorXd>,
                                        Eigen::SparseMatrix<double, Eigen::RowMajor> &) {
                ++*hook_calls;
                return 0;
            });
        }
        void on_ipm_iter(const IpmIterTraceEvent &) override { ++rows; }
        void on_ipqp_iter(const IpqpTraceIterEvent &) override {}
        void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
        void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
        void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
        void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
        void on_ipqp_escape(const IpqpTraceEscapeEvent &) override {}
        void on_qp_mode(const QpModeTraceEvent &) override {}
        void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {}
    };

    auto solver = nonconvex_solver();
    Index hook_calls = 0;
    HookInstallingSink sink;
    sink.solver = solver.engine.get();
    sink.hook_calls = &hook_calls;
    solver.engine->attach_trace(&sink);
    (void)solver.engine->solve(*solver.program, nonconvex_start());
    // THE PREMISE, NON-VACUOUS: a FACTOR-TIME message really was raised, and it
    // is the kind the fixture is built to raise.
    ASSERT_GT(sink.messages, 0) << "premise: the sink saw a message to install from";
    ASSERT_TRUE(sink.saw_inertia_exhausted)
        << "premise: the message the sink installed from is the factor-time one";
    ASSERT_TRUE(sink.installed);
    EXPECT_EQ(hook_calls, 0)
        << "a hook installed from inside a sink method must not arm the solve that is running";

    // ... and the NEXT solve runs it, from its first iteration on.
    const IpmResult second = solver.engine->solve(*solver.program, nonconvex_start());
    EXPECT_GT(hook_calls, 0) << "the deferral must be applied at the next solve's entry";
    // AND THE VERIFICATION IS ARMED FOR IT: the hand-out site sets the flag
    // beside the hand-out itself, so a hook that arrives this way still forces
    // every factorization from its first hand-out on to re-derive the pattern.
    EXPECT_GT(second.kkt_factor_counters.pattern_verify_count, 0);
}

TEST(IpmDeferral, TheSolveIsBitwiseUnchangedByASinkThatSetsAHookMidSolve) {
    // THE OTHER HALF, and the one that matters for the trajectory: the stream
    // a JSON sink records is IDENTICAL whether or not a second sink beside it
    // installs a hook mid-solve -- wall-clock masked, as everywhere else.
    //
    // FROM A FACTOR-TIME MESSAGE (M6 W5 T8.7b fix1, astra's I2). This pin used
    // to install from `on_ipm_iter` on HS071, which is BETWEEN factorizations;
    // the case the deferral exists for is a set made from INSIDE one, so it
    // installs from `on_ipm_message` over the `inertia_exhausted` fixture now.
    //
    // THROUGH A FAN-OUT, because `JsonLinesTraceSink` is `final`: the recording
    // half is the shipped writer, unmodified, and the installing half is a
    // separate sink watching the same stream.
    struct Installer final : TraceSink {
        IpmSolver *solver = nullptr;
        bool arm = false;
        bool done = false;
        void on_ipm_message(const IpmMessageTraceEvent &e) override {
            if (e.kind != IpmMessageKind::kInertiaExhausted || !arm || done) {
                return;
            }
            done = true;
            solver->set_kkt_hook([](int, double, hven::ConstEigenRef<Eigen::VectorXd>, double,
                                    hven::ConstEigenRef<Eigen::VectorXd>,
                                    hven::ConstEigenRef<Eigen::VectorXd>,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &) { return 0; });
        }
        void on_ipqp_iter(const IpqpTraceIterEvent &) override {}
        void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
        void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
        void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
        void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
        void on_ipqp_escape(const IpqpTraceEscapeEvent &) override {}
        void on_qp_mode(const QpModeTraceEvent &) override {}
        void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {}
    };
    // ONE THROWAWAY SOLVE FIRST, for the reason `TheConsoleDoesNotDisplaceAUser
    // Sink` states: the process-global initialization runs on the first solve
    // of the process and writes an `ipm.message`/`solver_initialized` line when
    // it does, which would put one extra line -- and a one-off `seq` shift --
    // into whichever arm ran first.
    {
        {
            auto c = silent_hs071();
            (void)c.engine->solve(*c.program, hs071_start());
        }
    }
    auto run = [](bool install) {
        auto solver = nonconvex_solver();
        std::ostringstream os;
        JsonLinesTraceSink json(os);
        Installer inst;
        inst.solver = solver.engine.get();
        inst.arm = install;
        FanOutTraceSink fan(&json, &inst);
        solver.engine->attach_trace(&fan);
        (void)solver.engine->solve(*solver.program, nonconvex_start());
        EXPECT_EQ(inst.done, install) << "premise: the installing arm really did install";
        return os.str();
    };
    const std::string quiet = run(false);
    const std::string armed = run(true);
    ASSERT_FALSE(quiet.empty());
    EXPECT_EQ(mask_wall_clock(quiet), mask_wall_clock(armed))
        << "a sink that installed a hook mid-solve changed the solve it was watching";
}

namespace {

/// @brief Every non-timing thing a solve ANSWERS, for the retry pin below.
///
/// The stream carries no vector (schema section 2's rule), so the vectors are
/// compared here off `IpmResult` and the per-row/per-phase fields are compared
/// through the masked JSON stream beside it. Together they are the T8.6 reuse
/// enumeration: status, iterations, the objective, the four engine residuals,
/// the primal point, all three multiplier blocks, and both constraint vectors.
struct IpmAnswer {
    SolveStatus status = SolveStatus::kMaxIter;
    int iterations = -1;
    double f = 0.0;
    double kkt_inf = 0.0, barr_inf = 0.0, econ_inf = 0.0, icon_inf = 0.0;
    Eigen::VectorXd x, lambda_e, lambda_i, z, ce, ci;
};

IpmAnswer answer_of(const IpmResult &r) {
    IpmAnswer a;
    a.status = r.status;
    a.iterations = r.iterations;
    a.f = r.f;
    a.kkt_inf = r.kkt_inf;
    a.barr_inf = r.barr_inf;
    a.econ_inf = r.econ_inf;
    a.icon_inf = r.icon_inf;
    a.x = r.x;
    a.lambda_e = r.lambda_e;
    a.lambda_i = r.lambda_i;
    a.z = r.z;
    a.ce = r.ce;
    a.ci = r.ci;
    return a;
}

void expect_bits(double lhs, double rhs, const char *what) {
    EXPECT_EQ(std::bit_cast<std::uint64_t>(lhs), std::bit_cast<std::uint64_t>(rhs)) << what;
}

void expect_bits(const Eigen::VectorXd &lhs, const Eigen::VectorXd &rhs, const char *what) {
    ASSERT_EQ(lhs.size(), rhs.size()) << what;
    for (Eigen::Index i = 0; i < lhs.size(); ++i) {
        EXPECT_EQ(std::bit_cast<std::uint64_t>(lhs[i]), std::bit_cast<std::uint64_t>(rhs[i]))
            << what << " [" << i << "]";
    }
}

void expect_same_ipm_answer(const IpmAnswer &lhs, const IpmAnswer &rhs) {
    EXPECT_EQ(lhs.status, rhs.status);
    EXPECT_EQ(lhs.iterations, rhs.iterations);
    expect_bits(lhs.f, rhs.f, "f");
    expect_bits(lhs.kkt_inf, rhs.kkt_inf, "kkt_inf");
    expect_bits(lhs.barr_inf, rhs.barr_inf, "barr_inf");
    expect_bits(lhs.econ_inf, rhs.econ_inf, "econ_inf");
    expect_bits(lhs.icon_inf, rhs.icon_inf, "icon_inf");
    expect_bits(lhs.x, rhs.x, "x");
    expect_bits(lhs.lambda_e, rhs.lambda_e, "lambda_e");
    expect_bits(lhs.lambda_i, rhs.lambda_i, "lambda_i");
    expect_bits(lhs.z, rhs.z, "z");
    expect_bits(lhs.ce, rhs.ce, "ce");
    expect_bits(lhs.ci, rhs.ci, "ci");
}

/// @brief Replaces the three keys on `ipm.kkt_analysis` that REPORT the reuse.
///
/// `docompute`, `factor_mem` and `factor_flops` are the reuse itself: a solve
/// that refactorizes on an existing analysis writes `false` and two `null`s
/// where a solve that computed one writes `true` and the factor's figures. They
/// are the ONE declared difference between the retry and the fresh arm below,
/// and the test asserts their values EXPLICITLY on each side rather than
/// letting this mask hide anything: what the mask buys is that every other key
/// on that line -- `kkt_dim`, `nnz` -- stays inside the byte comparison.
std::string mask_analysis_reuse(const std::string &stream) {
    static const std::vector<std::string> kReuseKeys = {"docompute", "factor_mem", "factor_flops"};
    std::string out = stream;
    for (const std::string &k : kReuseKeys) {
        const std::string needle = "\"" + k + "\":";
        std::size_t pos = 0;
        while ((pos = out.find(needle, pos)) != std::string::npos) {
            const std::size_t vstart = pos + needle.size();
            std::size_t vend = vstart;
            while (vend < out.size() && out[vend] != ',' && out[vend] != '}') {
                ++vend;
            }
            out.replace(vstart, vend - vstart, "<reuse>");
            pos = vstart + 7;
        }
    }
    return out;
}

/// A sink that THROWS once, from the first factor-time message it is handed.
struct ThrowingMessageSink final : TraceSink {
    bool threw = false;
    Index messages = 0;
    void on_ipm_message(const IpmMessageTraceEvent &e) override {
        ++messages;
        if (threw || e.kind != IpmMessageKind::kInertiaExhausted) {
            return;
        }
        threw = true;
        throw std::runtime_error("a sink that throws from inside a factorization");
    }
    void on_ipqp_iter(const IpqpTraceIterEvent &) override {}
    void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const QpModeTraceEvent &) override {}
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {}
};

} // namespace

TEST(IpmMessageSink, ARetryAfterAThrowingFactorTimeSinkIsBitwiseAFreshSolve) {
    // THE RECOVERY CONTRACT, AS THE CODE HAS IT (M6 W5 T8.7b fix1, astra's I1
    // with the lane's section 2.1 ruling).
    //
    // WHAT THE DOCUMENTATION USED TO SAY: that after a message sink throws, the
    // next solve "re-analyzes rather than reusing" the factorization. It does
    // not. The solve's scope guards clear `solve_in_flight_` and the borrowed
    // model and NOTHING invalidates `qp_analyzed_` or the structure stamps, so
    // an unchanged-model retry takes `claim_kkt_analysis()`'s REUSE branch --
    // asserted below as `kkt_analyses_this_call == 0`.
    //
    // AND THE REUSE IS SOUND, which is the other half and the reason the fix is
    // the sentence and not the guard: the symbolic analysis depends only on the
    // PATTERN, no ladder step changes the pattern (the perturbations add to
    // diagonal VALUES in place), and `init_impl` reassembles every value --
    // primal diagonals, slacks, a full INIT evaluation into the KKT buffer --
    // before it factorizes. So the retry is the computation a FRESH solver
    // performs, bit for bit, and that is what is pinned: the whole masked JSON
    // stream and every vector the result carries.
    //
    // A THROWAWAY SOLVE FIRST, for the reason the console pins state: the
    // once-per-process initialization notice would otherwise land in whichever
    // arm ran first and shift its `seq`.
    {
        auto warm = nonconvex_solver();
        (void)warm.engine->solve(*warm.program, nonconvex_start());
    }

    // ARM ONE: the solve that dies inside a factorization, then the retry.
    auto reused = nonconvex_solver();
    ThrowingMessageSink thrower;
    reused.engine->attach_trace(&thrower);
    EXPECT_THROW((void)reused.engine->solve(*reused.program, nonconvex_start()),
                 std::runtime_error);
    ASSERT_TRUE(thrower.threw) << "premise: the sink threw from a FACTOR-TIME message";
    reused.engine->attach_trace(nullptr);

    std::ostringstream retry_os;
    JsonLinesTraceSink retry_sink(retry_os);
    reused.engine->attach_trace(&retry_sink);
    const IpmResult retry_result = reused.engine->solve(*reused.program, nonconvex_start());
    const IpmAnswer retry = answer_of(retry_result);
    EXPECT_EQ(retry_result.kkt_analyses_this_call, 0)
        << "the retry must take the REUSE branch -- that is the fact the contract now states";

    // ARM TWO: a solver that never saw the throw, on the same model.
    auto fresh = nonconvex_solver();
    std::ostringstream fresh_os;
    JsonLinesTraceSink fresh_sink(fresh_os);
    fresh.engine->attach_trace(&fresh_sink);
    const IpmResult fresh_result = fresh.engine->solve(*fresh.program, nonconvex_start());
    const IpmAnswer cold = answer_of(fresh_result);
    EXPECT_GT(fresh_result.kkt_analyses_this_call, 0)
        << "premise: the fresh arm really did pay the analysis the retry skipped";

    // THE PIN. The stream carries every row, every phase event and the phase
    // exit's residuals and diagnostics; the answer carries the vectors the
    // stream does not.
    ASSERT_FALSE(retry_os.str().empty());
    EXPECT_EQ(mask_analysis_reuse(mask_wall_clock(retry_os.str())),
              mask_analysis_reuse(mask_wall_clock(fresh_os.str())))
        << "the retry after a throwing factor-time sink is not the solve a fresh solver runs";
    expect_same_ipm_answer(retry, cold);

    // THE ONE DIFFERENCE, ASSERTED RATHER THAN MASKED AWAY. The two streams
    // differ on exactly one line and exactly three keys, and those three keys
    // ARE the reuse: the retry refactorized on the analysis the thrown solve
    // had laid, the fresh arm computed its own. Every other byte of both
    // streams -- the row, the residuals, the phase exit, the verdict -- is
    // identical, which is the whole claim.
    const std::vector<std::string> retry_analysis =
        lines_of_event(retry_os.str(), "ipm.kkt_analysis");
    const std::vector<std::string> fresh_analysis =
        lines_of_event(fresh_os.str(), "ipm.kkt_analysis");
    ASSERT_EQ(retry_analysis.size(), 1u);
    ASSERT_EQ(fresh_analysis.size(), 1u);
    EXPECT_EQ(field(retry_analysis[0], "docompute"), "false");
    EXPECT_EQ(field(retry_analysis[0], "factor_mem"), "null");
    EXPECT_EQ(field(retry_analysis[0], "factor_flops"), "null");
    EXPECT_EQ(field(fresh_analysis[0], "docompute"), "true");
    EXPECT_NE(field(fresh_analysis[0], "factor_mem"), "null");

    // NON-VACUOUS: the stream really does carry the factor-time messages, so
    // the arm being compared is the one that reaches the interrupted ladder.
    EXPECT_GT(lines_of_event(retry_os.str(), "ipm.message").size(), 0u);
}

TEST(IpmDeferral, TheLastWriteWinsWhenASinkAndACallbackBothSetInOneSolve) {
    // ONE SLOT PER SETTER (M6 W5 T8.7b fix1, the lane's M3). A sink-origin park
    // and an invocation-origin call in the SAME solve share one
    // `std::optional`: the second overwrites the first, value AND origin flag,
    // so the sink's value is LOST and the callback's lands post-invocation.
    // Last write wins is a defensible rule -- the most recent request from the
    // caller is the one that takes effect, whoever made it -- but it was
    // undocumented. It is on the setters now, and pinned here.
    //
    // THE SINK ORIGIN IS `on_ipm_iter`, ON HS071, and that is deliberate. Any
    // sink method takes the same branch: the setter reads
    // `callback_in_flight_ || kkt_hook_in_flight_` and nothing about WHICH
    // method the sink is in, so a factor-time `on_ipm_message` parks
    // identically (`AHookInstalledFromInsideAFactorTimeMessageReachesTheNEXT
    // Solve` is that case). What HS071 buys is a solve with enough iterations
    // for the sink to park FIRST and a callback invocation to overwrite it
    // AFTERWARDS, which is the order the rule is about; the `inertia_exhausted`
    // fixture exits after a single iteration and cannot express it.
    struct InstallingSink final : TraceSink {
        IpmSolver *solver = nullptr;
        Index *sink_installed_calls = nullptr;
        bool installed = false;
        void on_ipm_iter(const IpmIterTraceEvent &) override {
            if (installed) {
                return;
            }
            installed = true;
            Index *counter = sink_installed_calls;
            solver->set_iteration_callback([counter](const IterationEvent &) {
                ++*counter;
                return CallbackAction::kContinue;
            });
        }
        void on_ipqp_iter(const IpqpTraceIterEvent &) override {}
        void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
        void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
        void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
        void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
        void on_ipqp_escape(const IpqpTraceEscapeEvent &) override {}
        void on_qp_mode(const QpModeTraceEvent &) override {}
        void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {}
    };

    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Index sink_calls = 0;
    Index first_calls = 0;
    Index second_calls = 0;
    InstallingSink sink;
    sink.solver = &solver;
    sink.sink_installed_calls = &sink_calls;
    solver.attach_trace(&sink);
    solver.set_iteration_callback([&](const IterationEvent &) {
        ++first_calls;
        // On its THIRD invocation -- by which point the sink has certainly
        // parked from a row -- this callback parks OVER the sink's value.
        if (first_calls == 3) {
            solver.set_iteration_callback([&](const IterationEvent &) {
                ++second_calls;
                return CallbackAction::kContinue;
            });
        }
        return CallbackAction::kContinue;
    });
    result = solver.solve(*program, hs071_start());
    ASSERT_EQ(result.status, SolveStatus::kOptimal);

    ASSERT_TRUE(sink.installed) << "premise: the sink really did park a callback";
    ASSERT_GE(first_calls, 3) << "premise: the callback reached its third invocation";
    EXPECT_GT(second_calls, 0)
        << "the invocation-origin park must land when that invocation returns, in THIS solve";
    EXPECT_EQ(sink_calls, 0) << "the sink's parked value was overwritten: last write wins";

    // AND IT IS GONE, not queued: the next solve's entry does not resurrect it.
    const Index second_calls_after_first_solve = second_calls;
    result = solver.solve(*program, hs071_start());
    ASSERT_EQ(result.status, SolveStatus::kOptimal);
    EXPECT_EQ(sink_calls, 0) << "an overwritten park must not be applied at a later entry";
    EXPECT_GT(second_calls, second_calls_after_first_solve)
        << "the callback the second write installed is the one that survived";
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
        const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
        hven::solvers::IpmSolver solver;
        hven::solvers::IpmResult result;
        auto o = solver.options();
        o.common.print_level = 0;
        solver.set_options(std::move(o));
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        solver.solve(*program, hs071_start());
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
        EXPECT_TRUE(capture.active());
        solver.engine->solve(*solver.program, hs071_start());
        silent = capture.text();
    }
    EXPECT_EQ(silent, "");
}

TEST(IpmConsole, TheConsoleDoesNotDisplaceAUserSink) {
    // The fan-out's invariant on the interior-point side: the caller's stream
    // is byte-identical with printing on and off.
    //
    // ONE THROWAWAY SOLVE FIRST (M6 W5 T8.7b), for the reason
    // `AttachTraceDuringASolveIsRefusedAndTheConsoleRunsOnUnbroken` states and
    // this test now shares: the process-global initialization runs on the
    // FIRST solve of the process and, since T8.7b, writes an
    // `ipm.message`/`solver_initialized` line when it does. Whichever arm below
    // ran first would carry a line the other does not, whatever either arm did
    // with its sink. Warming it makes the two arms' line structure identical by
    // construction rather than by luck.
    {
        {
            auto c = silent_hs071();
            (void)c.engine->solve(*c.program, hs071_start());
        }
    }
    auto run = [](int print_level) {
        const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
        hven::solvers::IpmSolver solver;
        hven::solvers::IpmResult result;
        auto o = solver.options();
        o.common.print_level = print_level;
        solver.set_options(std::move(o));
        std::ostringstream os;
        JsonLinesTraceSink sink(os);
        solver.attach_trace(&sink);
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        solver.solve(*program, hs071_start());
        return std::pair<std::string, std::string>{os.str(), capture.text()};
    };
    const auto silent = run(10);
    const auto printing = run(0);
    EXPECT_FALSE(silent.first.empty());

    // EVERY WALL-CLOCK VALUE IS MASKED AND EVERYTHING ELSE IS COMPARED BYTE FOR
    // BYTE. CLAUDE.md section 7 makes those fields informational and never
    // asserted, and two runs of one solve differ in them by construction --
    // console or no console.
    //
    // A MASK RATHER THAN A DROPPED LINE (M6 W5 T8.7b). Until this task only
    // `ipm.solve.end` carried a `_s` field and the test simply removed that one
    // line; `ipm.kkt_analysis` and `ipm.phase.exit` now carry them too, and
    // dropping those lines would throw away the analysis's size and fill and
    // the whole of the phase's exit -- the most informative new lines in the
    // stream. Masked, every other key on them is still pinned.
    EXPECT_EQ(mask_wall_clock(silent.first), mask_wall_clock(printing.first))
        << "the console perturbed the caller's stream";
    // NON-VACUOUS: the mask really did leave the new lines in place, with their
    // non-timing keys intact.
    EXPECT_EQ(lines_of_event(silent.first, "ipm.phase.begin").size(), 1u);
    EXPECT_EQ(lines_of_event(silent.first, "ipm.phase.exit").size(), 1u);
    EXPECT_EQ(lines_of_event(silent.first, "ipm.phase.end").size(), 1u);
    EXPECT_EQ(lines_of_event(silent.first, "ipm.kkt_analysis").size(), 1u);
    EXPECT_NE(mask_wall_clock(silent.first).find("\"last_kkt_info\":\"success\""),
              std::string::npos);
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
        {
            auto c = silent_hs071();
            (void)c.engine->solve(*c.program, hs071_start());
        }
    }

    auto printing_solve = [](bool attack) {
        const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
        hven::solvers::IpmSolver solver;
        hven::solvers::IpmResult result;
        auto o = solver.options();
        o.common.print_level = 0;
        solver.set_options(std::move(o));
        std::ostringstream os;
        JsonLinesTraceSink usurper(os);
        Index refusals = 0;
        if (attack) {
            solver.set_iteration_callback([&](const IterationEvent &) {
                try {
                    solver.attach_trace(&usurper);
                } catch (const std::logic_error &) {
                    ++refusals;
                }
                return CallbackAction::kContinue;
            });
        }
        std::string printed;
        {
            hven::testing::StdoutCapture capture;
            EXPECT_TRUE(capture.active());
            result = solver.solve(*program, hs071_start());
            EXPECT_EQ(result.status, SolveStatus::kOptimal);
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
    const auto after_program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver after;
    hven::solvers::IpmResult after_result;
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    EXPECT_NO_THROW(after.attach_trace(&sink));
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
        IpmSolver *solver = nullptr;
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
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::IpmSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    SelfDetachingSink sink;
    sink.solver = &solver;
    solver.attach_trace(&sink);
    EXPECT_THROW(solver.solve(*program, hs071_start()), std::logic_error);
    EXPECT_GE(sink.rows, 1) << "premise: the sink saw a row before it tried to detach";
}

TEST(IpmConsole, TheWideLayoutIsTheSolversOwnOptionAndReachesItsConsole) {
    std::string printed;
    {
        const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
        hven::solvers::IpmSolver solver;
        hven::solvers::IpmResult result;
        auto o = solver.options();
        o.common.print_level = 0;
        o.wide_console = true;
        solver.set_options(std::move(o));
        StdoutCapture capture;
        EXPECT_TRUE(capture.active());
        solver.solve(*program, hs071_start());
        printed = capture.text();
    }
    EXPECT_NE(printed.find("Max EMult"), std::string::npos);
    EXPECT_NE(printed.find("Merit Val"), std::string::npos);
}

namespace {

/// A sink that THROWS once, from the Nth `ipm.iter` row it is handed — the
/// multi-iteration twin of `ThrowingMessageSink`, which can only throw from a
/// factor-time message and therefore only on the iteration that produces one.
struct ThrowingRowSink final : TraceSink {
    explicit ThrowingRowSink(Index throw_on_row) : throw_on_row_(throw_on_row) {}

    bool threw = false;
    Index rows = 0;

    void on_ipm_iter(const IpmIterTraceEvent &) override {
        ++rows;
        if (threw || rows != throw_on_row_) {
            return;
        }
        threw = true;
        throw std::runtime_error("a sink that throws from an iteration row");
    }
    void on_ipqp_iter(const IpqpTraceIterEvent &) override {}
    void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const QpModeTraceEvent &) override {}
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {}

  private:
    Index throw_on_row_;
};

/// One HS071 engine and the program it solves, configured identically on both
/// arms of the pin below.
struct RetryCase {
    std::shared_ptr<hven::solvers::NonLinearProgram> program =
        hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    std::unique_ptr<hven::solvers::IpmSolver> engine = std::make_unique<hven::solvers::IpmSolver>();

    RetryCase() {
        auto o = engine->options();
        o.common.print_level = 10;
        o.common.threads = 1;
        engine->set_options(std::move(o));
    }
};

} // namespace

// THE MULTI-ITERATION RETRY PIN (M6 W6 T2; registered at the W5 T8.7b fix1 lane
// re-check, docs/notes/2026-08-m6-ledger.md:4263).
//
// `ARetryAfterAThrowingFactorTimeSinkIsBitwiseAFreshSolve` above pins the same
// contract on a ONE-ITERATION fixture: `NonconvexProblem` with an empty ladder
// reaches `inertia_exhausted` on iteration 0, which is the only reason that pin
// can throw from a factor-time message at all. The lane's non-gating note was
// that a one-iteration fixture leaves the interesting half untested — a solve
// that has already ACCEPTED steps, moved its iterate, advanced mu and filled a
// history when the throw lands. This is that fixture: HS071, a throw from the
// third `ipm.iter` row, and the same pin.
//
// The contract is unchanged and so is its reasoning: nothing invalidates
// `qp_analyzed_` or the structure stamps, so an unchanged-model retry takes
// `claim_kkt_analysis()`'s REUSE branch, and the reuse is sound because the
// symbolic analysis depends on the PATTERN alone while `init_impl` reassembles
// every value before it factorizes. A retry therefore computes what a FRESH
// solver computes, bit for bit, however many iterations the interrupted solve
// had already taken.
TEST(IpmMessageSink, AMultiIterationRetryAfterAThrowingRowSinkIsBitwiseAFreshSolve) {
    // A throwaway solve first, for the reason the console pins state: the
    // once-per-process initialization notice would otherwise land in whichever
    // arm ran first and shift its `seq`.
    {
        RetryCase warm;
        (void)warm.engine->solve(*warm.program, hs071_start());
    }

    // ARM ONE: the solve that dies on its third row, then the retry.
    RetryCase reused;
    ThrowingRowSink thrower(/*throw_on_row=*/3);
    reused.engine->attach_trace(&thrower);
    EXPECT_THROW((void)reused.engine->solve(*reused.program, hs071_start()), std::runtime_error);
    ASSERT_TRUE(thrower.threw) << "premise: the sink threw from an iteration row";
    ASSERT_EQ(thrower.rows, 3) << "premise: the throw landed on the third row, so the interrupted "
                                  "solve had already accepted steps and moved its iterate";
    reused.engine->attach_trace(nullptr);

    std::ostringstream retry_os;
    JsonLinesTraceSink retry_sink(retry_os);
    reused.engine->attach_trace(&retry_sink);
    const IpmResult retry_result = reused.engine->solve(*reused.program, hs071_start());
    const IpmAnswer retry = answer_of(retry_result);
    EXPECT_EQ(retry_result.kkt_analyses_this_call, 0)
        << "the retry must take the REUSE branch -- the same fact the one-iteration pin states";

    // ARM TWO: a solver that never saw the throw, on the same model.
    RetryCase fresh;
    std::ostringstream fresh_os;
    JsonLinesTraceSink fresh_sink(fresh_os);
    fresh.engine->attach_trace(&fresh_sink);
    const IpmResult fresh_result = fresh.engine->solve(*fresh.program, hs071_start());
    const IpmAnswer cold = answer_of(fresh_result);
    EXPECT_GT(fresh_result.kkt_analyses_this_call, 0)
        << "premise: the fresh arm really did pay the analysis the retry skipped";

    // THE MULTI-ITERATION PREMISE, asserted rather than assumed: this fixture
    // runs many iterations, so the interrupted solve was well past its first.
    ASSERT_GT(fresh_result.iterations, 3)
        << "premise: the fixture must take more iterations than the row the throw landed on";
    EXPECT_EQ(retry_result.status, SolveStatus::kOptimal);

    // THE PIN, as on the one-iteration fixture.
    ASSERT_FALSE(retry_os.str().empty());
    EXPECT_EQ(mask_analysis_reuse(mask_wall_clock(retry_os.str())),
              mask_analysis_reuse(mask_wall_clock(fresh_os.str())))
        << "the retry after a throwing row sink is not the solve a fresh solver runs";
    expect_same_ipm_answer(retry, cold);

    // THE ONE DIFFERENCE, asserted rather than masked away.
    const std::vector<std::string> retry_analysis =
        lines_of_event(retry_os.str(), "ipm.kkt_analysis");
    const std::vector<std::string> fresh_analysis =
        lines_of_event(fresh_os.str(), "ipm.kkt_analysis");
    ASSERT_EQ(retry_analysis.size(), 1u);
    ASSERT_EQ(fresh_analysis.size(), 1u);
    EXPECT_EQ(field(retry_analysis[0], "docompute"), "false");
    EXPECT_EQ(field(retry_analysis[0], "factor_mem"), "null");
    EXPECT_EQ(field(retry_analysis[0], "factor_flops"), "null");
    EXPECT_EQ(field(fresh_analysis[0], "docompute"), "true");
    EXPECT_NE(field(fresh_analysis[0], "factor_mem"), "null");
    // The THIRD reuse key on the fresh side too, so all three keys the mask
    // replaces are asserted on BOTH arms rather than two of three: a retry that
    // silently reported the figures, or a fresh solve that silently reported
    // none, would otherwise pass on this one.
    EXPECT_NE(field(fresh_analysis[0], "factor_flops"), "null");
}

} // namespace
} // namespace hven::solvers
