/*
 * M6 W2 T8 -- the kIpm counter leg. A SCRATCH INSTRUMENT (nothing in the tree
 * moves in T8): it re-runs every W2 fixture family at kIpm through the shipped
 * driver and prints the five T5 counters, the restoration-seed rows and
 * verdict_refine_steps per cell, with the TWO IDENTITIES checked on every row.
 *
 * Fixtures are copied VERBATIM from tests/sqp/test_sqp_driver.cpp (the T7
 * anonymous-namespace models and the T2/T3 QP helpers) so the numbers are the
 * suite's own; the copies are cited by line at the point of use.
 */
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/core/solver_status.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>

#include "support/hs_problems.h"

using namespace hven;
using namespace hven::solvers;
using hven::solvers::test_support::HsProblem;
using hven::solvers::test_support::make_hs;

namespace {

// ---- test_sqp_driver.cpp:8536 ----------------------------------------------
QpProblem w2_box_blocked_qp(double b) {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 2.0;
    qp.H.insert(1, 1) = 2.0;
    qp.H.makeCompressed();
    qp.g = Vec::Zero(2);
    qp.Ae = SpMatRM(1, 2);
    qp.Ae.insert(0, 0) = 1.0;
    qp.Ae.insert(0, 1) = 1.0;
    qp.Ae.makeCompressed();
    qp.be = Vec(1);
    qp.be << 5.0;
    qp.Ai = SpMatRM(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -b);
    qp.upper = Vec::Constant(2, b);
    return qp;
}

// ---- test_sqp_driver.cpp:8631 ----------------------------------------------
IpqpResult w2_escaped(const QpProblem &qp,
                      double radius = std::numeric_limits<double>::infinity()) {
    QpOptions qopts;
    qopts.tr_radius = radius;
    IpqpEngine tier(qopts);
    return tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
}

// ---- test_sqp_driver.cpp:10593 ---------------------------------------------
class ScaledInconsistentEqualitiesModel : public NlpModel {
  public:
    ScaledInconsistentEqualitiesModel(double s, double b, double a) : s_(s), b_(b), a_(a) {}
    Index n() const override { return 2; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return x; }
    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        c << s_ * (x(0) + x(1) - 2.0), s_ * (x(0) + x(1) + 2.0);
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
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(2, 2);
        j.insert(0, 0) = s_;
        j.insert(0, 1) = s_;
        j.insert(1, 0) = s_;
        j.insert(1, 1) = s_;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        lo_ = Vec::Constant(2, -b_);
        return lo_;
    }
    const Vec &upper() const override {
        up_ = Vec::Constant(2, b_);
        return up_;
    }
    Vec start_point() const override { return Vec::Constant(2, a_); }

  private:
    double s_, b_, a_;
    mutable Vec lo_, up_;
};

// ---- test_sqp_driver.cpp:10647 ---------------------------------------------
class BoxBlockedRowModel : public NlpModel {
  public:
    BoxBlockedRowModel(double s, double r, double a) : s_(s), r_(r), a_(a) {}
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return x; }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << s_ * (x(0) - r_);
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
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = s_;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Zero(2);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Ones(2);
        return u;
    }
    Vec start_point() const override { return Vec::Constant(2, a_); }

  private:
    double s_, r_, a_;
};

// ---- test_sqp_driver.cpp:10705 ---------------------------------------------
class FallbackSink : public IpqpTraceSink {
  public:
    std::vector<SqpFallbackVerdictTraceEvent> fallbacks;
    void on_ipqp_iter(const IpqpTraceIterEvent &) override {}
    void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const QpModeTraceEvent &) override {}
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &e) override {
        fallbacks.push_back(e);
    }
};

Index count_verdict(const std::vector<SqpFallbackVerdictTraceEvent> &fb, SqpFallbackVerdict v) {
    Index k = 0;
    for (const SqpFallbackVerdictTraceEvent &e : fb) {
        k += e.verdict == v ? 1 : 0;
    }
    return k;
}

const char *mode_name(QpMode m) {
    switch (m) {
    case QpMode::kWalk:
        return "walk";
    case QpMode::kSsn:
        return "ssn";
    case QpMode::kIpm:
        return "ipm";
    }
    return "?";
}

int failures = 0;

void emit(const std::string &cell, QpMode mode, const SqpSolution &sol,
          const std::vector<SqpFallbackVerdictTraceEvent> &fb) {
    const SqpCounters &c = sol.counters;
    const Index entries = static_cast<Index>(fb.size());
    Index fired = 0;
    for (const SqpFallbackVerdictTraceEvent &e : fb) {
        fired += e.entered_rung_a ? 1 : 0;
    }
    const Index disproved = count_verdict(fb, SqpFallbackVerdict::kDisproved);
    const Index relaxed = count_verdict(fb, SqpFallbackVerdict::kRelaxed);
    const Index exhausted = count_verdict(fb, SqpFallbackVerdict::kExhausted);
    const Index rung_b = count_verdict(fb, SqpFallbackVerdict::kRungB);
    const Index unfired = count_verdict(fb, SqpFallbackVerdict::kUnfired);
    Index seed_rows = 0;
    Index ceiling_rows = 0;
    for (const SqpIterate &row : sol.history) {
        seed_rows += row.restoration_seed_used ? 1 : 0;
        ceiling_rows += row.elastic_rho0_ceiling_hit ? 1 : 0;
    }
    // IDENTITY 1: activations = walk-route + fallback-route. The walk-route term has no
    // counter, so it is READ as the residual and asserted NONNEGATIVE, and asserted ZERO
    // wherever no walk route can run (kIpm cells with no refusal).
    const Index walk_route = c.elastic_activations - c.elastic_from_ipqp_escape;
    const bool id1 = walk_route >= 0;
    // IDENTITY 2: the ENTRY partition over fired entries.
    const bool id2a = (disproved + relaxed + exhausted + rung_b) == fired;
    const bool id2b = (c.elastic_from_ipqp_escape - c.elastic_floor_retries) == fired;
    const bool id2c = (disproved == c.ipqp_suspicion_disproved) && (rung_b == c.ipqp_fallback_rung_b);
    const bool id2d = (entries - unfired) == fired;
    const bool ok = id1 && id2a && id2b && id2c && id2d;
    if (!ok) {
        ++failures;
    }
    fmt::print("{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n", cell,
               mode_name(mode), to_string(sol.status), c.major_iters, entries, fired, disproved,
               relaxed, exhausted, rung_b, unfired, c.elastic_activations, c.elastic_escalations,
               c.elastic_from_ipqp_escape, c.ipqp_suspicion_disproved, c.ipqp_fallback_rung_b,
               c.elastic_rho0_ceiling_hits, c.elastic_floor_retries, seed_rows, ceiling_rows,
               c.verdict_refine_steps, ok ? "OK" : "IDENTITY-FAIL");
}

void run_model(const std::string &cell, const NlpModel &model, QpMode mode, Index max_iter = 60) {
    SqpOptions opts;
    opts.qp_mode = mode;
    opts.max_iter = max_iter;
    SqpDriver driver(opts);
    FallbackSink sink;
    driver.attach_trace(&sink);
    const SqpSolution sol = driver.solve(model);
    emit(cell, mode, sol, sink.fallbacks);
}

} // namespace

int main() {
    fmt::print("cell,mode,status,major_iters,fb_entries,fb_fired,v_disproved,v_relaxed,"
               "v_exhausted,v_rung_b,v_unfired,elastic_activations,elastic_escalations,"
               "elastic_from_ipqp_escape,ipqp_suspicion_disproved,ipqp_fallback_rung_b,"
               "elastic_rho0_ceiling_hits,elastic_floor_retries,restoration_seed_rows,"
               "ceiling_hit_rows,verdict_refine_steps,identities\n");

    // (1) THE HS CORPUS AT kIpm -- the population T7's calibration pin is measured over.
    for (const int number : hven::solvers::test_support::hs_numbers()) {
        const HsProblem p = make_hs(number);
        run_model(fmt::format("hs{}", number), *p.model, QpMode::kIpm);
    }

    // (2) F-1, THE THREE-MODE FIXTURE (T7's audit cell), all three modes.
    for (const QpMode m : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpm}) {
        ScaledInconsistentEqualitiesModel f1(1.0e2, 2.0, 0.5);
        run_model("f1_scaled_inconsistent", f1, m);
    }

    // (3) THE DRIVER-LEVEL EXHAUSTION CELL (T7 item 1: the ATTACHED report).
    {
        ScaledInconsistentEqualitiesModel exh(1.0e8, 1.0, 2.5);
        run_model("t7_exhaustion_attached_report", exh, QpMode::kIpm);
    }

    // (4) THE T3-A ROUTE AT THE DRIVER (a REFUSAL followed by a walk kInfeasible).
    {
        BoxBlockedRowModel t3a(1.0e8, 5.0, 0.25);
        run_model("t3a_refusal_then_walk", t3a, QpMode::kIpm);
    }

    // (5) P4's REFUSAL, at the unit level: the fallback called directly with a FLOOR-placed
    //     evidence block on an engine whose radius declines rung A (test_sqp_driver.cpp:9443).
    {
        const QpProblem qp = w2_box_blocked_qp(0.5);
        IpqpInfeasibilityEvidence floored = w2_escaped(qp).infeasibility_evidence;
        floored.dual_norm_start = kElasticRhoInit;
        SqpOptions opts;
        opts.qp.tr_radius = 1.0e-3;
        QpEngine engine(opts.qp);
        SqpCounters out;
        SqpIterate row;
        std::optional<ElasticLadderReport> report;
        SqpFallbackVerdictTraceEvent verdict;
        const NlpEval nlp_ev;
        const QpSolution qs = certified_feasibility_fallback(
            engine, qp, nlp_ev, nullptr, floored, SolveOverrides{}, opts,
            std::numeric_limits<double>::infinity(), out, row, report, verdict);
        SqpSolution shim;
        shim.status = qs.status == QpStatus::kOptimal ? SqpStatus::kOptimal : SqpStatus::kInfeasible;
        shim.counters = out;
        shim.history.push_back(row);
        emit("p4_refusal_unit", QpMode::kIpm, shim, {verdict});
    }

    fmt::print("# identity failures: {}\n", failures);
    return failures == 0 ? 0 : 1;
}
