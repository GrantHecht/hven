// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_sqp_qpc_fold_seams.cpp — the SQP driver's FOLD of the walk's certificate_gate_refusals
// (M8 QPC), through the data half's verdict observer
// (hven/detail/qp/walk/certificate_seam.h, QpCertificateGateSeam; docs/testing.md "The data
// half's presentations and its verdict observer (M8 QPC)"). Compiled only into
// hven_sqp_seam_tests.
//
// THE INVARIANT. The observer counts every data-half refusal of every walk solve on this thread,
// whichever driver site ran the solve; the driver folds each solve's QpCounters into
// SqpCounters at that site. So after one SqpSolver::solve,
//
//     result.counters.certificate_gate_refusals == QpCertificateGateSeam::gate_refusals,
//
// and a fold dropped at a site that ran a refused solve breaks it. The observer FORCES the
// refusals (one ordinal at a time over an unarmed run's reads, then every read), so each fixture
// drives refusals into a chosen site without depending on a natural refusal.
//
// THE PREMISE IS ASSERTED, NOT ASSUMED. A trace sink attributes every refusal to the site that
// ran it: each walk solve announces itself on `qp.mode` with its site right after it runs, so the
// observer's refusal count moves only between two announcements, and the delta belongs to the
// solve just announced. The restoration sub-driver shares the sink (one nesting deeper, told apart
// by its own sqp.solve begin/end pair), so a refusal at depth 2 is one the restoration phase's fold
// carries. Each fixture asserts its site took at least one refusal; the fold-site map is
//
//     main subproblem   kDispatch     src/sqp/sqp_solver.cpp, mj.qs fold
//     elastic rung      kElasticRung  run_elastic_ladder, qs_e fold
//     restoration       depth 2       fold_phase, phase fold
//     cap retry         kCapRetry     the iteration-limit retry, `retried` fold
//     SOC re-solve      kSocResolve   the second-order correction, mj.qs_soc fold

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/detail/model/nlp_model.h>
#include <hven/detail/model/problem_access.h>
#include <hven/detail/qp/walk/walk_trace.h> // opens certificate_seam.h under HVEN_TESTING
#include <hven/detail/sqp/sqp_solver_impl.h>
#include <hven/sqp/sqp_solver_types.h>
#include <hven/trace/trace.h>

namespace hven {
namespace {

using detail::NlpModel;
using Gate = detail::testing::QpCertificateGateSeam;

constexpr double kInfBound = 1e20;
constexpr Index kEveryRead = std::numeric_limits<Index>::max();
constexpr std::size_t kSites = 6; // QpTraceModeSite's enumerators

/// Who took the refusals: the top-level driver's sites by `qp.mode` site, and the restoration
/// sub-driver (depth >= 2) as one bucket.
struct SiteTally {
    std::array<Index, kSites> solves{};
    std::array<Index, kSites> refused{};
    Index sub_solves = 0;
    Index sub_refused = 0;
    Index unattributed = 0; // refusals after the last announcement (must stay 0)

    Index at(QpTraceModeSite s) const { return refused[static_cast<std::size_t>(s)]; }
    void add(const SiteTally &o) {
        for (std::size_t i = 0; i < kSites; ++i) {
            solves[i] += o.solves[i];
            refused[i] += o.refused[i];
        }
        sub_solves += o.sub_solves;
        sub_refused += o.sub_refused;
        unattributed += o.unattributed;
    }
    std::string str() const {
        return fmt::format("dispatch {}/{} elastic {}/{} soc {}/{} retry {}/{} restoration {}/{} "
                           "(refused/solves) unattributed {}",
                           at(QpTraceModeSite::kDispatch),
                           solves[static_cast<std::size_t>(QpTraceModeSite::kDispatch)],
                           at(QpTraceModeSite::kElasticRung),
                           solves[static_cast<std::size_t>(QpTraceModeSite::kElasticRung)],
                           at(QpTraceModeSite::kSocResolve),
                           solves[static_cast<std::size_t>(QpTraceModeSite::kSocResolve)],
                           at(QpTraceModeSite::kCapRetry),
                           solves[static_cast<std::size_t>(QpTraceModeSite::kCapRetry)],
                           sub_refused, sub_solves, unattributed);
    }
};

class SiteSink final : public TraceSink {
  public:
    void on_solve_begin(const SolveBeginTraceEvent &) override { ++depth_; }
    void on_solve_end(const SolveEndTraceEvent &) override { --depth_; }
    void on_solve_abort() noexcept override { --depth_; }
    void on_qp_mode(const QpModeTraceEvent &e) override {
        const Index now = Gate::gate_refusals;
        const Index delta = now - seen_;
        seen_ = now;
        if (depth_ >= 2) {
            ++tally_.sub_solves;
            tally_.sub_refused += delta;
            return;
        }
        const auto i = static_cast<std::size_t>(e.site);
        ++tally_.solves[i];
        tally_.refused[i] += delta;
    }
    SiteTally finish() {
        tally_.unattributed = Gate::gate_refusals - seen_;
        return tally_;
    }

  private:
    int depth_ = 0;
    Index seen_ = 0;
    SiteTally tally_;
};

struct FoldRun {
    SqpResult result;
    Index reads = 0;
    Index refusals = 0;
    SiteTally tally;
};

/// One solve with the observer reset, armed on [from, to) when `refuse` is engaged.
FoldRun solve_once(const NlpModel &model, const SqpOptions &opts, const Vec &x0,
                   std::optional<std::pair<Index, Index>> refuse) {
    Gate::reset();
    if (refuse.has_value()) {
        Gate::armed = true;
        Gate::refuse_from = refuse->first;
        Gate::refuse_to = refuse->second;
    }
    SqpSolver driver(opts);
    SiteSink sink;
    driver.attach_trace(&sink);
    FoldRun out;
    out.result = driver.solve(detail::problem_ref(model), x0);
    out.reads = Gate::gate_reads;
    out.refusals = Gate::gate_refusals;
    out.tally = sink.finish();
    Gate::reset();
    return out;
}

/// THE INVARIANT on one run: every refusal was folded exactly once, and every one is attributed.
void expect_folded_once(const FoldRun &r, const std::string &what) {
    EXPECT_GT(r.refusals, 0) << what << ": the run refused nothing, so it tests no fold";
    EXPECT_EQ(r.result.counters.certificate_gate_refusals, r.refusals)
        << what << ": SqpCounters::certificate_gate_refusals is not the walk's refusal count ("
        << r.tally.str() << ")";
    EXPECT_EQ(r.tally.unattributed, 0) << what;
}

/// The sweep: learn the unarmed run's N reads, then refuse each ordinal alone, then every read.
/// Returns the per-site refusals summed over the armed runs, so a fixture asserts its premise.
SiteTally sweep(const char *name, const NlpModel &model, const SqpOptions &opts, const Vec &x0) {
    const FoldRun base = solve_once(model, opts, x0, std::nullopt);
    EXPECT_EQ(base.result.counters.certificate_gate_refusals, base.refusals)
        << name << " unarmed (natural refusals " << base.refusals << ")";
    EXPECT_GT(base.reads, 0) << name << ": the unarmed run read no certificate";
    SiteTally total;
    for (Index k = 0; k < base.reads; ++k) {
        const FoldRun r = solve_once(model, opts, x0, std::make_pair(k, k + 1));
        expect_folded_once(r, fmt::format("{} refuse read {} of {}", name, k, base.reads));
        total.add(r.tally);
    }
    SqpOptions all_opts = opts;
    all_opts.common.max_iterations = std::min<Index>(opts.common.max_iterations, 30);
    const FoldRun all = solve_once(model, all_opts, x0, std::make_pair(Index{0}, kEveryRead));
    expect_folded_once(all, fmt::format("{} refuse every read", name));
    total.add(all.tally);
    std::printf("[ qpc-fold ] %s: unarmed reads %lld status %d | armed runs %lld | %s\n", name,
                static_cast<long long>(base.reads), static_cast<int>(base.result.status),
                static_cast<long long>(base.reads + 1), total.str().c_str());
    return total;
}

SqpOptions silent() {
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    return o;
}

// ---------------------------------------------------------------------------------------------
// Models, copied so this file stands alone.
// ---------------------------------------------------------------------------------------------

// test_qp_cap_levers.cpp's BoxPullModel: six variables pulled past their unit box, one binding
// row sum(x) <= 3.5. A healthy solve in a few majors; capped one minor short with the cap retry
// on, its first subproblem is retried.
constexpr Index kBoxN = 6;
class BoxPullModel : public NlpModel {
  public:
    Index n() const override { return kBoxN; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }
    double eval_f(const Vec &x) const override {
        double f = 0.0;
        for (Index i = 0; i < kBoxN; ++i) {
            const double d = x(i) - target(i);
            f += 0.5 * weight(i) * d * d;
        }
        return f;
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(kBoxN);
        for (Index i = 0; i < kBoxN; ++i) {
            g(i) = weight(i) * (x(i) - target(i));
        }
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = x.sum() - 3.5;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(kBoxN, kBoxN);
        for (Index i = 0; i < kBoxN; ++i) {
            h.insert(i, i) = obj_scale * weight(i);
        }
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override { return SpMatRM(0, kBoxN); }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, kBoxN);
        for (Index i = 0; i < kBoxN; ++i) {
            j.insert(0, i) = 1.0;
        }
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(kBoxN); }

  private:
    static double weight(Index i) { return 1.0 + 0.25 * static_cast<double>(i); }
    static double target(Index i) { return 4.0 + static_cast<double>(i); }
    Vec lower_ = Vec::Zero(kBoxN);
    Vec upper_ = Vec::Ones(kBoxN);
};

// test_sqp_solver.cpp's MaratosModel: the unit circle, f = 2(|x|^2 - 1) - x0. Major 1's raw step
// is the Maratos step (f and h both rise), and the second-order correction re-solves it.
class MaratosModel : public NlpModel {
  public:
    explicit MaratosModel(double theta) : theta_(theta) {}
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override {
        return 2.0 * (x(0) * x(0) + x(1) * x(1) - 1.0) - x(0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 4.0 * x(0) - 1.0, 4.0 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(0) * x(0) + x(1) * x(1) - 1.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &le, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * 4.0 + le(0) * 2.0;
        h.insert(1, 1) = obj_scale * 4.0 + le(0) * 2.0;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << std::cos(theta_), std::sin(theta_);
        return x;
    }

  private:
    double theta_;
};

// test_sqp_solver.cpp's InconsistentBoundedModel: x0 = 5 against the box [0, 1]^2. The main QP is
// infeasible, the elastic tier climbs its rungs, restoration is entered, and the solve ends
// kInfeasible.
class InconsistentBoundedModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return x; }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(0) - 5.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 1.0;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override {
        static const Vec l = Vec::Zero(2);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Ones(2);
        return u;
    }
    Vec start_point() const override { return Vec::Constant(2, 0.5); }
};

// test_sqp_solver.cpp's InconsistentLinearizationModel: two inequalities whose linearization at
// the start is inconsistent although the NLP is feasible.
class InconsistentLinearizationModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 2; }
    double eval_f(const Vec &x) const override {
        const double a = x(0) - 2.0;
        return a * a + x(1) * x(1);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 2.0 * (x(0) - 2.0), 2.0 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(2);
        c << x(1) - x(0) * x(0), 1.0 - x(1) - x(0) * x(0);
        return c;
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &li) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * obj_scale - 2.0 * (li(0) + li(1));
        h.insert(1, 1) = 2.0 * obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override { return SpMatRM(0, 2); }
    SpMatRM eval_jac_i(const Vec &x) const override {
        SpMatRM j(2, 2);
        j.insert(0, 0) = -2.0 * x(0);
        j.insert(0, 1) = 1.0;
        j.insert(1, 0) = -2.0 * x(0);
        j.insert(1, 1) = -1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -5.0);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 5.0);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << 0.0, 0.5;
        return x;
    }
};

} // namespace

// ---------------------------------------------------------------------------------------------
// One fixture per fold site. Each sweeps its model and asserts the site took refusals.
// ---------------------------------------------------------------------------------------------

// THE MAIN SUBPROBLEM's fold (the mj.qs fold of the major's own QP).
TEST(SqpQpcFold, MainSubproblemRefusalsAreFoldedOnce) {
    BoxPullModel model;
    const SqpOptions o = silent();
    const FoldRun healthy = solve_once(model, o, model.start_point(), std::nullopt);
    ASSERT_EQ(healthy.result.status, SolveStatus::kOptimal);
    const SiteTally t = sweep("BoxPull", model, o, model.start_point());
    EXPECT_GT(t.at(QpTraceModeSite::kDispatch), 0)
        << "premise: no main-subproblem solve took a refusal (" << t.str() << ")";
}

// THE SOC RE-SOLVE's fold (mj.qs_soc). The Maratos fixture's major 1 is rejected and corrected.
TEST(SqpQpcFold, SocResolveRefusalsAreFoldedOnce) {
    MaratosModel model(0.1);
    SqpOptions o = silent();
    o.common.optimality_tol = 1e-8;
    o.common.feasibility_tol = 1e-8;
    o.advanced.second_order_correction = true;
    const FoldRun healthy = solve_once(model, o, model.start_point(), std::nullopt);
    ASSERT_EQ(healthy.result.status, SolveStatus::kOptimal);
    ASSERT_GT(healthy.result.counters.soc_steps, 0) << "premise: the unarmed solve ran no SOC";
    const SiteTally t = sweep("Maratos", model, o, model.start_point());
    EXPECT_GT(t.at(QpTraceModeSite::kSocResolve), 0)
        << "premise: no SOC re-solve took a refusal (" << t.str() << ")";
}

// THE CAP RETRY's fold (`retried`). The subproblem is capped one minor short of what it needs and
// the doubled cap reaches the certificate.
TEST(SqpQpcFold, CapRetryRefusalsAreFoldedOnce) {
    BoxPullModel model;
    const FoldRun uncapped = solve_once(model, silent(), model.start_point(), std::nullopt);
    ASSERT_FALSE(uncapped.result.history.empty());
    const Index minors =
        uncapped.result.history.front().sqp->qp_minor_iterations.value_or(Index{0});
    ASSERT_GE(minors, 2) << "the fixture cannot be capped one minor short";

    SqpOptions o = silent();
    o.advanced.qp_failure_step = false;
    o.advanced.qp_iteration_limit_retry = true;
    o.qp.max_iterations = minors - 1;
    const FoldRun healthy = solve_once(model, o, model.start_point(), std::nullopt);
    ASSERT_GT(healthy.result.counters.qp_cap_retries, 0) << "premise: the retry did not run";
    const SiteTally t = sweep("BoxPullCapped", model, o, model.start_point());
    EXPECT_GT(t.at(QpTraceModeSite::kCapRetry), 0)
        << "premise: no cap retry took a refusal (" << t.str() << ")";
}

// THE ELASTIC RUNG's fold (qs_e). The main QP is infeasible and the ladder's rungs are walk solves.
TEST(SqpQpcFold, ElasticRungRefusalsAreFoldedOnce) {
    InconsistentBoundedModel model;
    const SqpOptions o = silent();
    const FoldRun healthy = solve_once(model, o, model.start_point(), std::nullopt);
    ASSERT_GT(healthy.result.counters.elastic_activations, 0)
        << "premise: the elastic tier did not open";
    const SiteTally t = sweep("InconsistentBounded", model, o, model.start_point());
    EXPECT_GT(t.at(QpTraceModeSite::kElasticRung), 0)
        << "premise: no elastic rung took a refusal (" << t.str() << ")";
}

// THE RESTORATION PHASE's fold (phase): every walk refusal inside the sub-driver reaches the caller
// only through it.
TEST(SqpQpcFold, RestorationPhaseRefusalsAreFoldedOnce) {
    SiteTally t;
    Index entries = 0;
    {
        InconsistentBoundedModel model;
        const SqpOptions o = silent();
        const FoldRun healthy = solve_once(model, o, model.start_point(), std::nullopt);
        entries += healthy.result.counters.restoration_entries;
        t.add(sweep("InconsistentBounded", model, o, model.start_point()));
    }
    {
        InconsistentLinearizationModel model;
        const SqpOptions o = silent();
        const FoldRun healthy = solve_once(model, o, model.start_point(), std::nullopt);
        entries += healthy.result.counters.restoration_entries;
        t.add(sweep("InconsistentLinearization", model, o, model.start_point()));
    }
    ASSERT_GT(entries, 0) << "premise: neither unarmed solve entered restoration";
    EXPECT_GT(t.sub_refused, 0) << "premise: no restoration sub-solve took a refusal (" << t.str()
                                << ")";
}

} // namespace hven
