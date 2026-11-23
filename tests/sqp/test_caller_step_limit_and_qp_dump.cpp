// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_caller_step_limit_and_qp_dump.cpp -- M8 I17: SqpOptions::caller_step_limit on a synthetic
// weak-column model (a thrust-direction vector d * u(RA, dec) in degrees, hven issue 17) and over
// whole solves (convergence, SOC, the elastic ladder, an infeasible limited QP, restoration, the
// declared-box corner cases), the separate column-amplification cap, and the public QP dump
// (advanced.qp_dump_dir): round trip, policy, names unique across solvers and reruns, failures.
// Step assertions are on the visited points in caller units, never on a QP route; route facts are
// pinned on the reference build only (HVEN_TEST_ROUTE_REFERENCE).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <numbers>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include <hven/hven.h>

#include "model/support/hs071_problem.h"
#include "qp/support/qp_dump.h"
#include "support/route_reference.h"

namespace hven {
namespace {

namespace fs = std::filesystem;

constexpr double kDeg = std::numbers::pi / 180.0;

// c = L t u(a, b) + y - T = 0, f = w t + |y|^2 / 2; a, b in degrees, b in [-90, 90], t >= 0.
// The columns of (a, b) are weak (L t is small), so Ruiz's rule amplifies them.
struct Synth final : Nlp {
    double L;
    double w = 1.0;
    Vec T = Vec{{1.0, 2.0, 0.5}};
    explicit Synth(double L_) : L(L_) {}
    NlpDimensions dimensions() const override {
        return {.n = 6, .m = 3, .jacobian_nonzeros = 12, .hessian_nonzeros = 0};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
        xl[1] = -90.0;
        xu[1] = 90.0;
        xl[2] = 0.0;
        gl.setZero();
        gu.setZero();
    }
    static Vec u(double a, double b) {
        a *= kDeg;
        b *= kDeg;
        return Vec{{std::cos(b) * std::cos(a), std::cos(b) * std::sin(a), std::sin(b)}};
    }
    double eval_f(ConstVecRef x) const override { return w * x[2] + 0.5 * x.tail(3).squaredNorm(); }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g.setZero();
        g[2] = w;
        g.tail(3) = x.tail(3);
    }
    void eval_g(ConstVecRef x, VecRef g) const override {
        g = L * x[2] * u(x[0], x[1]) + x.tail(3) - T;
    }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        int k = 0;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                r[k] = i;
                c[k] = j;
                ++k;
            }
            r[k] = i;
            c[k] = 3 + i;
            ++k;
        }
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override {
        const double a = x[0] * kDeg, b = x[1] * kDeg, t = x[2];
        const double da[3] = {-std::cos(b) * std::sin(a) * kDeg, std::cos(b) * std::cos(a) * kDeg,
                              0.0};
        const double db[3] = {-std::sin(b) * std::cos(a) * kDeg, -std::sin(b) * std::sin(a) * kDeg,
                              std::cos(b) * kDeg};
        const Vec uu = u(x[0], x[1]);
        int k = 0;
        for (int i = 0; i < 3; ++i) {
            v[k++] = L * t * da[i];
            v[k++] = L * t * db[i];
            v[k++] = L * uu[i];
            v[k++] = 1.0;
        }
    }
    bool has_hessian() const override { return false; }
};

Vec synth_start(const Synth &m) {
    Vec x0(6);
    x0 << 30.0, 20.0, 0.0513, 0, 0, 0;
    Vec c(3);
    m.eval_g(x0, c);
    x0.tail(3) = -c;
    x0.tail(3) += Vec{{2e-3, -2.5e-3, 2e-3}};
    return x0;
}

SqpOptions synth_options(QpMode mode, SqpColumnScaling cs) {
    SqpOptions o;
    o.hessian_mode = SqpHessianMode::kLbfgsDamped;
    o.qp_mode = mode;
    o.row_scaling = true;
    o.column_scaling = cs;
    o.trust_region_initial = 1.0;
    o.trust_region_max = 1e10;
    o.advanced.restoration_curvature = SqpRestorationCurvature::kRegularized;
    o.common.feasibility_tol = 1e-4;
    o.common.optimality_tol = 1e-6;
    o.common.max_iterations = 1;
    o.common.verbosity = Verbosity::kSilent;
    return o;
}

// The first step's largest excess over r * (1 + |x0_j|), in caller units; <= 0 when within.
double step_excess(const Vec &x0, const Vec &x, double r) {
    double worst = -1e300;
    for (Index j = 0; j < x0.size(); ++j) {
        worst = std::max(worst, std::abs(x[j] - x0[j]) - r * (1.0 + std::abs(x0[j])));
    }
    return worst;
}

SqpResult solve_synth(double L, SqpOptions o, Vec *x0_out = nullptr) {
    auto m = std::make_shared<Synth>(L);
    const Vec x0 = synth_start(*m);
    if (x0_out != nullptr) {
        *x0_out = x0;
    }
    Problem p(m);
    SqpSolver s(o);
    return s.solve(p, x0);
}

long test_pid() {
#ifdef _WIN32
    return static_cast<long>(_getpid());
#else
    return static_cast<long>(getpid());
#endif
}

struct TempDir {
    fs::path path;
    explicit TempDir(const std::string &tag) {
        const char *base = std::getenv("TMPDIR");
        path = fs::path(base != nullptr ? base : fs::temp_directory_path().string()) /
               ("hven_i17_" + std::to_string(test_pid()) + "_" + tag + "_" +
                ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::remove_all(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::vector<fs::path> dump_files(const fs::path &dir) {
    std::vector<fs::path> out;
    if (!fs::exists(dir)) {
        return out;
    }
    for (const auto &e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ".txt") {
            out.push_back(e.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

TEST(CallerStepLimit, TheDefaultIsOffAndTheOptionValidates) {
    EXPECT_EQ(SqpOptions{}.caller_step_limit, 0.0);
    SqpOptions o;
    o.caller_step_limit = -1.0;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.caller_step_limit = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.caller_step_limit = std::numeric_limits<double>::infinity();
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.caller_step_limit = 0.25;
    EXPECT_NO_THROW(validate(o));
    std::ostringstream os;
    dump_options(o, os);
    EXPECT_NE(os.str().find("caller_step_limit = 0.25"), std::string::npos) << os.str();
}

TEST(CallerStepLimit, AlwaysScalingMovesAnAngleFarWithoutItAndWithinTheBoundWithIt) {
    constexpr double kLimit = 0.05;
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
        SCOPED_TRACE(static_cast<int>(mode));
        Vec x0;
        SqpOptions off = synth_options(mode, SqpColumnScaling::kAlways);
        const SqpResult r_off = solve_synth(4.0, off, &x0);
        ASSERT_TRUE(r_off.scaling.columns_active) << "premise: Always scales the columns";
        ASSERT_GT(r_off.scaling.column_spread, 1e3) << "premise: the weak columns are amplified";
        // The premise the limit answers (the walk's route; the other kernels' unlimited steps are
        // not asserted): the unlimited step breaks the caller-unit bound.
#if HVEN_TEST_ROUTE_REFERENCE
        if (mode == QpMode::kWalk) {
            EXPECT_GT(step_excess(x0, r_off.x, kLimit), 0.0);
        }
#endif

        SqpOptions on = off;
        on.caller_step_limit = kLimit;
        const SqpResult r_on = solve_synth(4.0, on);
        ASSERT_EQ(r_on.x.size(), x0.size());
        EXPECT_LE(step_excess(x0, r_on.x, kLimit), 1e-9 * (1.0 + x0.cwiseAbs().maxCoeff()))
            << "a component of the first step left r * (1 + |x_j|)";
        EXPECT_LE(r_on.counters.major_iters, 1);
        EXPECT_TRUE(r_on.x.allFinite());
        EXPECT_GE(r_on.x[1], -90.0);
        EXPECT_LE(r_on.x[1], 90.0);
    }
}

TEST(CallerStepLimit, OffIsTheShippedSolveExactly) {
    SqpOptions a = synth_options(QpMode::kWalk, SqpColumnScaling::kAlways);
    SqpOptions b = a;
    b.caller_step_limit = 0.0;
    const SqpResult ra = solve_synth(4.0, a);
    const SqpResult rb = solve_synth(4.0, b);
    EXPECT_EQ(ra.status, rb.status);
    EXPECT_EQ((ra.x - rb.x).lpNorm<Eigen::Infinity>(), 0.0);
    EXPECT_EQ(ra.counters.qp_minor_iters, rb.counters.qp_minor_iters);
    EXPECT_EQ(ra.counters.factorizations, rb.counters.factorizations);
}

TEST(CallerStepLimit, TheLimitHoldsWithoutColumnScalingToo) {
    constexpr double kLimit = 0.05;
    SqpOptions o = synth_options(QpMode::kWalk, SqpColumnScaling::kOff);
    o.caller_step_limit = kLimit;
    Vec x0;
    const SqpResult r = solve_synth(4.0, o, &x0);
    EXPECT_FALSE(r.scaling.columns_active);
    EXPECT_LE(step_excess(x0, r.x, kLimit), 1e-9 * (1.0 + x0.cwiseAbs().maxCoeff()));
}

TEST(ColumnScalingFactorMax, ClampsTheColumnFactorsAndLeavesTheRowCapAlone) {
    EXPECT_EQ(SqpOptions{}.column_scaling_factor_max, 1024.0);
    SqpOptions o;
    o.column_scaling_factor_max = 0.5;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.column_scaling_factor_max = std::numeric_limits<double>::infinity();
    EXPECT_THROW(validate(o), std::invalid_argument);

    SqpOptions wide = synth_options(QpMode::kWalk, SqpColumnScaling::kAlways);
    wide.column_scaling_factor_max = 1e12;
    const SqpResult r_wide = solve_synth(4.0, wide);
    ASSERT_TRUE(r_wide.scaling.columns_active);
    ASSERT_GT(r_wide.scaling.column_max, 4.0) << "premise: a factor above the cap tried below";

    SqpOptions capped = wide;
    capped.column_scaling_factor_max = 4.0;
    const SqpResult r_cap = solve_synth(4.0, capped);
    EXPECT_LE(r_cap.scaling.column_max, 4.0);
    EXPECT_GE(r_cap.scaling.column_min, 0.25);

    // The row cap does not touch the column factors: tightening it leaves them as they were.
    SqpOptions rowcap = wide;
    rowcap.scaling_factor_max = 2.0;
    const SqpResult r_row = solve_synth(4.0, rowcap);
    EXPECT_EQ(r_row.scaling.column_max, r_wide.scaling.column_max);
    EXPECT_EQ(r_row.scaling.column_min, r_wide.scaling.column_min);

    // kAuto is untouched: with the gate forced open (threshold 1) its column factors are still
    // clamped by scaling_factor_max, and the new option is not read.
    SqpOptions auto_o = synth_options(QpMode::kWalk, SqpColumnScaling::kAuto);
    auto_o.column_scaling_threshold = 1.0;
    const SqpResult a0 = solve_synth(4.0, auto_o);
    ASSERT_TRUE(a0.scaling.columns_active);
    ASSERT_GT(a0.scaling.column_max, 4.0) << "premise: the factors reach beyond the tried cap";
    SqpOptions auto_cap = auto_o;
    auto_cap.column_scaling_factor_max = 4.0;
    const SqpResult a1 = solve_synth(4.0, auto_cap);
    EXPECT_EQ(a1.scaling.column_max, a0.scaling.column_max);
    EXPECT_EQ((a1.x - a0.x).lpNorm<Eigen::Infinity>(), 0.0);
    SqpOptions auto_row = auto_o;
    auto_row.scaling_factor_max = 4.0;
    EXPECT_LE(solve_synth(4.0, auto_row).scaling.column_max, 4.0);
}

TEST(CallerStepLimit, TheQpBoxIsTheCallerBoundOverTheColumnFactorAndAnInactiveLimitChangesNothing) {
    constexpr double kLimit = 0.05;
    TempDir dir("box");
    SqpOptions o = synth_options(QpMode::kWalk, SqpColumnScaling::kAlways);
    o.caller_step_limit = kLimit;
    o.advanced.qp_dump_dir = dir.path.string();
    o.advanced.qp_dump_policy = QpDumpPolicy::kAll;
    Vec x0;
    const SqpResult r = solve_synth(4.0, o, &x0);
    ASSERT_TRUE(r.scaling.columns_active);
    const std::vector<fs::path> files = dump_files(dir.path);
    ASSERT_FALSE(files.empty());
    const test_support::QpDump d = test_support::load_qp_dump(files.front().string());
    ASSERT_EQ(d.qp.n(), 6);
    bool some_scaled = false;
    // Variables 0, 3, 4, 5 have no declared bound: the box is the limit alone, in engine units,
    // r * (1 + |x_j|) / d_j with d_j a power of two -- so the ratio to the caller bound is 2^-k.
    for (const Index j : {0, 3, 4, 5}) {
        const double caller = kLimit * (1.0 + std::abs(x0[j]));
        const double ratio = d.qp.upper(j) / caller;
        EXPECT_EQ(d.qp.upper(j), -d.qp.lower(j)) << "j = " << j;
        int e = 0;
        EXPECT_NEAR(std::frexp(ratio, &e), 0.5, 1e-14)
            << "j = " << j << ": ratio " << ratio << " is not a power of two";
        some_scaled = some_scaled || e != 1;
    }
    EXPECT_TRUE(some_scaled) << "premise: a column factor other than 1";
    // A declared bound tighter than the limit stays: t >= 0 gives p_2 >= -t, below the limit
    // r * (1 + t) here, so the lower bound is -t / d_2 (a power-of-two multiple of t).
    ASSERT_LT(x0[2], kLimit * (1.0 + x0[2])) << "premise: the declared bound is the tighter one";
    int e2 = 0;
    EXPECT_NEAR(std::frexp(-d.qp.lower(2) / x0[2], &e2), 0.5, 1e-14) << "the box kept t >= 0";

    // A limit far above any step is inactive: the solve is the unlimited one.
    SqpOptions off = synth_options(QpMode::kWalk, SqpColumnScaling::kAlways);
    SqpOptions huge = off;
    huge.caller_step_limit = 1e9;
    const SqpResult a = solve_synth(4.0, off);
    const SqpResult b = solve_synth(4.0, huge);
    EXPECT_EQ((a.x - b.x).lpNorm<Eigen::Infinity>(), 0.0);
    EXPECT_EQ(a.counters.qp_minor_iters, b.counters.qp_minor_iters);
}

TEST(QpDump, RestorationQpsAreRecordedInTheAttachedLedger) {
    struct Inf final : Nlp {
        NlpDimensions dimensions() const override { return {2, 1, 2, 2}; }
        void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
            xl.setConstant(-kInfinity);
            xu.setConstant(kInfinity);
            gl << 0.0;
            gu << 0.0;
        }
        double eval_f(ConstVecRef x) const override { return x[0] + x[1]; }
        void eval_grad_f(ConstVecRef, VecRef g) const override { g << 1.0, 1.0; }
        void eval_g(ConstVecRef x, VecRef g) const override {
            g[0] = x[0] * x[0] + x[1] * x[1] + 1.0;
        }
        void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
            r << 0, 0;
            c << 0, 1;
        }
        void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
            r << 0, 1;
            c << 0, 1;
        }
        void eval_jacobian(ConstVecRef x, VecRef v) const override { v << 2.0 * x[0], 2.0 * x[1]; }
        void eval_hessian(ConstVecRef, double, ConstVecRef l, VecRef v) const override {
            v << 2.0 * l[0], 2.0 * l[0];
        }
    };
    Problem p(std::make_shared<Inf>());
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    SqpSolver s(o);
    Ledger led;
    s.attach_ledger(&led, "i17");
    const SqpResult r = s.solve(p, Vec::Constant(2, 1.0));
    ASSERT_GE(r.counters.restoration_entries, 1) << "premise";
    Index resto_qps = 0;
    Index primary_qps = 0;
    for (const auto &q : led.records()) {
        resto_qps += q.label.find("_resto") != std::string::npos ? 1 : 0;
        primary_qps += q.label.rfind("i17_qp_", 0) == 0 ? 1 : 0;
    }
    EXPECT_GE(resto_qps, 1) << "the restoration phase's QPs are ledgered";
    EXPECT_GE(primary_qps, 1);
    // A SECOND SOLVE on the same driver enters restoration again at the same entry number: every
    // label of both solves is still unique (the prefix carries the driver's lifetime run count).
    const std::size_t first = led.records().size();
    (void)s.solve(p, Vec::Constant(2, 1.0));
    ASSERT_GT(led.records().size(), first);
    std::set<std::string> labels;
    for (const auto &q : led.records()) {
        EXPECT_TRUE(labels.insert(q.label).second) << "duplicate ledger label " << q.label;
    }
}

// --- the QP dump -----------------------------------------------------------------------------

int dumped_status(const test_support::QpDump &d) {
    const auto at = d.result.find("status=");
    return at == std::string::npos ? -1 : std::stoi(d.result.substr(at + 7));
}

SqpOptions hs071_dump_options(const fs::path &dir, QpDumpPolicy policy) {
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.advanced.qp_dump_dir = dir.string();
    o.advanced.qp_dump_policy = policy;
    return o;
}

TEST(QpDump, EveryWalkSolveRoundTripsThroughTheLoaderToItsStatusAndObjective) {
    TempDir dir("all");
    Problem p(std::make_shared<hven_model_tests::Hs071Problem>());
    SqpSolver s(hs071_dump_options(dir.path, QpDumpPolicy::kAll));
    const SqpResult r = s.solve(p, hven_model_tests::hs071_start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    const std::vector<fs::path> files = dump_files(dir.path);
    ASSERT_GE(static_cast<Index>(files.size()), r.counters.major_iters)
        << "one file per walk solve, the major's QP at least";
    const std::string prefix = "qp_p" + std::to_string(test_pid()) + "_s";
    Index replay_mismatches = 0;
    for (const fs::path &f : files) {
        SCOPED_TRACE(f.string());
        const std::string name = f.filename().string();
        EXPECT_EQ(name.rfind(prefix, 0), 0u) << "process token";
        EXPECT_NE(name.find("_d0_m"), std::string::npos) << "major tag";
        const test_support::QpDump d = test_support::load_qp_dump(f.string());
        EXPECT_EQ(d.qp.infinite_bound, std::numeric_limits<double>::infinity())
            << "the SQP subproblem's absent-bound cutoff is recorded";
        const detail::QpSolution again = test_support::solve_qp_dump(d, d.opts);
        const double fa = test_support::qp_dump_objective(d, again.x);
        const double fd = test_support::qp_dump_objective(d, d.x_final);
        // WHICH SITES the solve visits and whether a fresh engine replays each solve to its
        // status and objective are the reference build's (a hot start is not recorded).
#if HVEN_TEST_ROUTE_REFERENCE
        EXPECT_NE(name.find("_main.txt"), std::string::npos) << "site tag";
        EXPECT_EQ(static_cast<int>(again.status), dumped_status(d));
        EXPECT_NEAR(fa, fd, 1e-8 * (1.0 + std::abs(fd)));
#else
        replay_mismatches += (static_cast<int>(again.status) != dumped_status(d) ||
                              std::abs(fa - fd) > 1e-8 * (1.0 + std::abs(fd)))
                                 ? 1
                                 : 0;
#endif
    }
#if !HVEN_TEST_ROUTE_REFERENCE
    hven::test_support::record_off_reference_route(
        "QpDump.round_trip", "files=" + std::to_string(files.size()) +
                                 " replay_mismatches=" + std::to_string(replay_mismatches));
#endif
    (void)replay_mismatches;
}

TEST(QpDump, OffWritesNothingAndFailedWritesOnlyFailedSolves) {
    {
        TempDir dir("off");
        Problem p(std::make_shared<hven_model_tests::Hs071Problem>());
        SqpSolver s(hs071_dump_options(dir.path, QpDumpPolicy::kOff));
        // kOff with a directory named is valid and inert.
        (void)s.solve(p, hven_model_tests::hs071_start());
        EXPECT_FALSE(fs::exists(dir.path));
    }
    {
        // An infeasible subproblem sequence: the main QP fails and the ladder's rungs run.
        struct Inf final : Nlp {
            NlpDimensions dimensions() const override { return {2, 1, 2, 2}; }
            void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
                xl.setConstant(-kInfinity);
                xu.setConstant(kInfinity);
                gl << 0.0;
                gu << 0.0;
            }
            double eval_f(ConstVecRef x) const override { return x[0] + x[1]; }
            void eval_grad_f(ConstVecRef, VecRef g) const override { g << 1.0, 1.0; }
            void eval_g(ConstVecRef x, VecRef g) const override {
                g[0] = x[0] * x[0] + x[1] * x[1] + 1.0;
            }
            void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
                r << 0, 0;
                c << 0, 1;
            }
            void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
                r << 0, 1;
                c << 0, 1;
            }
            void eval_jacobian(ConstVecRef x, VecRef v) const override {
                v << 2.0 * x[0], 2.0 * x[1];
            }
            void eval_hessian(ConstVecRef, double, ConstVecRef l, VecRef v) const override {
                v << 2.0 * l[0], 2.0 * l[0];
            }
        };
        TempDir all("inf_all");
        TempDir failed("inf_failed");
        const Vec x0 = Vec::Constant(2, 1.0);
        {
            Problem p(std::make_shared<Inf>());
            SqpSolver s(hs071_dump_options(all.path, QpDumpPolicy::kAll));
            (void)s.solve(p, x0);
        }
        {
            Problem p(std::make_shared<Inf>());
            SqpSolver s(hs071_dump_options(failed.path, QpDumpPolicy::kFailed));
            (void)s.solve(p, x0);
        }
        const std::vector<fs::path> fa = dump_files(all.path);
        const std::vector<fs::path> ff = dump_files(failed.path);
        ASSERT_FALSE(fa.empty());
        EXPECT_LE(ff.size(), fa.size());
        for (const fs::path &f : ff) {
            EXPECT_NE(dumped_status(test_support::load_qp_dump(f.string())), 0) << f;
        }
        bool saw_rung = false;
        for (const fs::path &f : fa) {
            saw_rung = saw_rung || f.filename().string().find("_rung") != std::string::npos;
        }
        // WHICH ROUTE the infeasible model's majors take (a ladder at all) is the reference
        // build's.
#if HVEN_TEST_ROUTE_REFERENCE
        EXPECT_TRUE(saw_rung) << "the ladder's rungs are dumped, tagged by rung";
#else
        hven::test_support::record_off_reference_route(
            "QpDump.infeasible_ladder", saw_rung ? "rung files written" : "no rung files");
#endif
    }
}

TEST(QpDump, AnUnwritableDirectoryThrowsAndAnOnPolicyNeedsADirectory) {
    TempDir dir("blocked");
    fs::create_directories(dir.path);
    const fs::path blocker = dir.path / "a_file";
    std::ofstream(blocker) << "x";
    SqpOptions o = hs071_dump_options(blocker / "sub", QpDumpPolicy::kAll);
    SqpSolver s(o);
    Problem p(std::make_shared<hven_model_tests::Hs071Problem>());
    EXPECT_THROW((void)s.solve(p, hven_model_tests::hs071_start()), std::runtime_error);

    SqpOptions bad;
    bad.advanced.qp_dump_policy = QpDumpPolicy::kFailed;
    EXPECT_THROW(validate(bad), std::invalid_argument);
}

// --- I17 rider: the step limit over whole solves -------------------------------------------

// Every point the solve visits (depth 0 and restoration's depth 1, caller units), in order.
struct PathRecorder {
    std::vector<Vec> x;
    std::vector<Index> depth;
    void attach(SqpSolver &s) {
        s.set_iteration_callback(
            [this](const IterationEvent &e) {
                x.emplace_back(e.x);
                depth.push_back(e.sqp ? e.sqp->depth : 0);
                return CallbackAction::kContinue;
            },
            kDefaultIterationPayload, CallbackScope::kIncludeRestoration);
    }
};

// The largest excess of a move over r * (1 + |x_prev_j|) on the first `n` entries, over every
// consecutive pair of visited points; a return to a point visited before (a watchdog restore) is
// not a step. <= 0 when every step is within the limit.
double path_excess(const PathRecorder &rec, Index n, double r) {
    double worst = -1e300;
    for (std::size_t k = 1; k < rec.x.size(); ++k) {
        const Vec &a = rec.x[k - 1];
        const Vec &b = rec.x[k];
        if (a.size() < n || b.size() < n) {
            continue;
        }
        bool revisit = false;
        for (std::size_t i = 0; i + 1 < k && !revisit; ++i) {
            revisit =
                rec.x[i].size() >= n && (rec.x[i].head(n) - b.head(n)).cwiseAbs().maxCoeff() == 0.0;
        }
        if (revisit) {
            continue;
        }
        for (Index j = 0; j < n; ++j) {
            const double tol = 1e-9 * (1.0 + std::abs(a[j]));
            worst = std::max(worst, std::abs(b[j] - a[j]) - r * (1.0 + std::abs(a[j])) - tol);
        }
    }
    return worst;
}

// A small model: f = sum_j w_j (x_j - c_j)^2, linear equalities A x = b, box per variable.
struct SmallQpLike final : Nlp {
    Vec w, c, xl, xu;
    std::vector<std::vector<double>> A;
    std::vector<double> b;
    NlpDimensions dimensions() const override {
        const Index n = w.size();
        return {.n = n,
                .m = static_cast<Index>(A.size()),
                .jacobian_nonzeros = static_cast<Index>(A.size()) * n,
                .hessian_nonzeros = n};
    }
    void bounds(VecRef l, VecRef u, VecRef gl, VecRef gu) const override {
        l = xl;
        u = xu;
        for (std::size_t i = 0; i < b.size(); ++i) {
            gl[static_cast<Index>(i)] = b[i];
            gu[static_cast<Index>(i)] = b[i];
        }
    }
    double eval_f(ConstVecRef x) const override {
        return (w.array() * (x - c).array().square()).sum();
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g = 2.0 * (w.array() * (x - c).array()).matrix();
    }
    void eval_g(ConstVecRef x, VecRef g) const override {
        for (std::size_t i = 0; i < A.size(); ++i) {
            double v = 0.0;
            for (Index j = 0; j < x.size(); ++j) {
                v += A[i][static_cast<std::size_t>(j)] * x[j];
            }
            g[static_cast<Index>(i)] = v;
        }
    }
    void jacobian_structure(IndexVecRef r, IndexVecRef cc) const override {
        int k = 0;
        for (std::size_t i = 0; i < A.size(); ++i) {
            for (Index j = 0; j < w.size(); ++j) {
                r[k] = static_cast<int>(i);
                cc[k] = static_cast<int>(j);
                ++k;
            }
        }
    }
    void eval_jacobian(ConstVecRef, VecRef v) const override {
        int k = 0;
        for (const auto &row : A) {
            for (const double a : row) {
                v[k++] = a;
            }
        }
    }
    void hessian_structure(IndexVecRef r, IndexVecRef cc) const override {
        for (Index j = 0; j < w.size(); ++j) {
            r[j] = static_cast<int>(j);
            cc[j] = static_cast<int>(j);
        }
    }
    void eval_hessian(ConstVecRef, double s, ConstVecRef, VecRef v) const override {
        v = 2.0 * s * w;
    }
};

std::shared_ptr<SmallQpLike> small_model(Vec w, Vec c, std::vector<std::vector<double>> A,
                                         std::vector<double> b) {
    auto m = std::make_shared<SmallQpLike>();
    m->w = std::move(w);
    m->c = std::move(c);
    m->A = std::move(A);
    m->b = std::move(b);
    m->xl = Vec::Constant(m->w.size(), -kInfinity);
    m->xu = Vec::Constant(m->w.size(), kInfinity);
    return m;
}

SqpOptions limit_options(double r) {
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.common.max_iterations = 400;
    o.caller_step_limit = r;
    return o;
}

TEST(CallerStepLimitSolve, AConvergingSolveKeepsEveryStepWithinTheLimitAndReachesTheSameOptimum) {
    constexpr double kLimit = 0.1;
    Problem p(std::make_shared<hven_model_tests::Hs071Problem>());
    SqpOptions off = limit_options(0.0);
    const SqpResult r_off = SqpSolver(off).solve(p, hven_model_tests::hs071_start());
    ASSERT_EQ(r_off.status, SolveStatus::kOptimal);
    SqpSolver s(limit_options(kLimit));
    PathRecorder rec;
    rec.attach(s);
    const SqpResult r = s.solve(p, hven_model_tests::hs071_start());
    EXPECT_EQ(r.status, SolveStatus::kOptimal);
    EXPECT_NEAR(r.f, r_off.f, 1e-6 * (1.0 + std::abs(r_off.f)));
    ASSERT_GE(rec.x.size(), 3u) << "premise: several majors";
    EXPECT_LE(path_excess(rec, 4, kLimit), 0.0);
}

TEST(CallerStepLimitSolve, ALimitPinIsReportedFreeAndTheStepRegionLimited) {
    // Free variables pulled far from the start: the first step stops at the limit on both. The
    // trust region is far wider, so the binding can only come from the limit (I19-1: reported as
    // `cap_binding`, not `tr_binding`).
    auto m = small_model(Vec{{1.0, 1.0}}, Vec{{10.0, 10.0}}, {{1.0, -1.0}}, {0.0});
    Problem p(m);
    SqpOptions o = limit_options(0.1);
    o.trust_region_initial = 1e3;
    o.trust_region_max = 1e4;
    o.common.max_iterations = 1;
    const SqpResult r = SqpSolver(o).solve(p, Vec::Zero(2));
    ASSERT_GE(r.history.size(), 1u);
    const IterationRecord &h = r.history.front();
    ASSERT_TRUE(h.sqp && h.sqp->qp_solved);
    EXPECT_TRUE(h.sqp->cap_binding) << "the limit-limited step is cap-bound";
    EXPECT_FALSE(h.sqp->tr_binding) << "the radius is far wider: only the limit binds";
    EXPECT_EQ(h.sqp->active_lower_sides, 0) << "no declared bound exists";
    EXPECT_EQ(h.sqp->active_upper_sides, 0) << "the limit side is not bound activity";
    EXPECT_NEAR(r.x[0], 0.1, 1e-12);
    EXPECT_NEAR(r.x[1], 0.1, 1e-12);
    // The warm currency carries no bound multiplier for a variable with no bound.
    ASSERT_FALSE(r.warm_start.empty());
    EXPECT_EQ(r.warm_start.z().cwiseAbs().maxCoeff(), 0.0);
}

TEST(CallerStepLimitSolve, ALimitThatMakesTheMainQpInfeasibleIsCarriedByTheElasticLadder) {
    // x0 = 10 from x0 = 0 under a limit of 0.1 (1 + |x0|): no limited step satisfies the
    // linearization, so the main QP is infeasible and the ladder's slacks carry the row.
    auto m = small_model(Vec{{1.0, 1.0}}, Vec{{0.0, 0.0}}, {{1.0, 0.0}}, {10.0});
    Problem p(m);
    SqpSolver s(limit_options(0.1));
    PathRecorder rec;
    rec.attach(s);
    const SqpResult r = s.solve(p, Vec::Zero(2));
    EXPECT_LE(path_excess(rec, 2, 0.1), 0.0);
    EXPECT_EQ(r.status, SolveStatus::kOptimal);
    EXPECT_NEAR(r.x[0], 10.0, 1e-6);
    ASSERT_FALSE(r.history.empty());
    ASSERT_TRUE(r.history.front().sqp);
#if HVEN_TEST_ROUTE_REFERENCE
    ASSERT_TRUE(r.history.front().sqp->ladder.has_value());
    EXPECT_EQ(r.history.front().sqp->ladder->entry, ElasticEntry::kVerifiedInfeasible);
    EXPECT_FALSE(r.history.front().sqp->ladder->exhausted);
#else
    hven::test_support::record_off_reference_route(
        "CallerStepLimit.infeasible_main_qp",
        r.history.front().sqp->ladder ? "ladder on major 0" : "no ladder on major 0");
#endif
}

TEST(CallerStepLimitSolve, AVariableBeyondItsBoxByMoreThanTheLimitKeepsItsDeclaredBox) {
    // x0 <= 1 with x0 = 10 at the (unprojected) start, limit 0.01 (1 + 10) = 0.11: the limit and
    // the declared step box [-inf, -9] do not intersect, so x0's box stays declared; x1's is the
    // limit's.
    TempDir dir("beyond");
    auto m = small_model(Vec{{1.0, 1.0}}, Vec{{0.5, 0.0}}, {{0.0, 1.0}}, {0.0});
    m->xu[0] = 1.0;
    Problem p(m);
    SqpOptions o = limit_options(0.01);
    o.project_start_point = false;
    o.column_scaling = SqpColumnScaling::kOff;
    o.common.max_iterations = 1;
    o.advanced.qp_dump_dir = dir.path.string();
    o.advanced.qp_dump_policy = QpDumpPolicy::kAll;
    const SqpResult r = SqpSolver(o).solve(p, Vec{{10.0, 0.0}});
    const std::vector<fs::path> files = dump_files(dir.path);
    ASSERT_FALSE(files.empty());
    const test_support::QpDump d = test_support::load_qp_dump(files.front().string());
    EXPECT_EQ(d.qp.upper(0), -9.0);
    EXPECT_EQ(d.qp.lower(0), -kInfinity);
    EXPECT_EQ(d.qp.upper(1), 0.01);
    EXPECT_EQ(d.qp.lower(1), -0.01);
    EXPECT_LE(r.x[0], 1.0 + 1e-9) << "the step back into the box is not limited";
}

TEST(CallerStepLimitSolve, AFixedVariableStaysFixedUnderTheLimit) {
    auto m = small_model(Vec{{1.0, 1.0}}, Vec{{3.0, 0.0}}, {{1.0, 1.0}}, {4.0});
    m->xl[1] = 2.0;
    m->xu[1] = 2.0;
    Problem p(m);
    SqpSolver s(limit_options(0.1));
    PathRecorder rec;
    rec.attach(s);
    const SqpResult r = s.solve(p, Vec{{0.0, 2.0}});
    EXPECT_EQ(r.status, SolveStatus::kOptimal);
    EXPECT_EQ(r.x[1], 2.0);
    EXPECT_NEAR(r.x[0], 2.0, 1e-6);
    EXPECT_LE(path_excess(rec, 2, 0.1), 0.0);
}

TEST(CallerStepLimitSolve, TheEqualityBasisReducedSubproblemKeepsTheLimit) {
    // Two dependent equality rows: the equality basis drops one, and the reduced subproblems still
    // carry the limit box.
    auto m = small_model(Vec{{1.0, 1.0}}, Vec{{3.0, 0.0}}, {{1.0, 1.0}, {2.0, 2.0}}, {1.0, 2.0});
    Problem p(m);
    SqpSolver s(limit_options(0.05));
    PathRecorder rec;
    rec.attach(s);
    const SqpResult r = s.solve(p, Vec::Zero(2));
    ASSERT_GE(r.equality_basis.dropped_peak, 1) << "premise: the basis reduced the subproblem";
    EXPECT_EQ(r.status, SolveStatus::kOptimal);
    EXPECT_NEAR(r.x[0] + r.x[1], 1.0, 1e-6);
    EXPECT_LE(path_excess(rec, 2, 0.05), 0.0);
}

// x0^2 + x1^2 = 1 with a linear objective whose full steps cross the circle: the second-order
// correction fires on a rejected trial.
struct Circle final : Nlp {
    NlpDimensions dimensions() const override { return {2, 1, 2, 2}; }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
        gl << 1.0;
        gu << 1.0;
    }
    double eval_f(ConstVecRef x) const override {
        return 2.0 * (x[0] * x[0] + x[1] * x[1] - 1.0) - x[0];
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override { g << 4.0 * x[0] - 1.0, 4.0 * x[1]; }
    void eval_g(ConstVecRef x, VecRef g) const override { g[0] = x[0] * x[0] + x[1] * x[1]; }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override { v << 2.0 * x[0], 2.0 * x[1]; }
    void eval_hessian(ConstVecRef, double s, ConstVecRef l, VecRef v) const override {
        v << 4.0 * s + 2.0 * l[0], 4.0 * s + 2.0 * l[0];
    }
};

TEST(CallerStepLimitSolve, TheSecondOrderCorrectionStepStaysWithinTheLimit) {
    constexpr double kLimit = 0.5;
    Problem p(std::make_shared<Circle>());
    SqpOptions o = limit_options(kLimit);
    SqpSolver s(o);
    PathRecorder rec;
    rec.attach(s);
    const SqpResult r = s.solve(p, Vec{{std::cos(2.5), std::sin(2.5)}});
    EXPECT_LE(path_excess(rec, 2, kLimit), 0.0);
    EXPECT_EQ(r.status, SolveStatus::kOptimal);
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_GE(r.counters.soc_steps, 1) << "premise: a correction was tried";
#else
    hven::test_support::record_off_reference_route(
        "CallerStepLimit.soc", "soc_steps=" + std::to_string(r.counters.soc_steps));
#endif
}

// h(x) = s x0^2 + x1^2 + 1 = 0 has no solution: the elastic ladder is exhausted and restoration
// minimizes the violation. `s` makes the two columns' scales differ.
struct InfeasibleBowl final : Nlp {
    double s;
    explicit InfeasibleBowl(double s_) : s(s_) {}
    NlpDimensions dimensions() const override { return {2, 1, 2, 2}; }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
        gl << 0.0;
        gu << 0.0;
    }
    double eval_f(ConstVecRef x) const override { return x[0] + x[1]; }
    void eval_grad_f(ConstVecRef, VecRef g) const override { g << 1.0, 1.0; }
    void eval_g(ConstVecRef x, VecRef g) const override {
        g[0] = s * x[0] * x[0] + x[1] * x[1] + 1.0;
    }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override { v << 2.0 * s * x[0], 2.0 * x[1]; }
    void eval_hessian(ConstVecRef, double, ConstVecRef l, VecRef v) const override {
        v << 2.0 * s * l[0], 2.0 * l[0];
    }
};

TEST(CallerStepLimitSolve, EveryElasticRungsBoxCarriesTheLimit) {
    constexpr double kLimit = 0.05;
    TempDir dir("rungs");
    Problem p(std::make_shared<InfeasibleBowl>(1.0));
    SqpOptions o = limit_options(kLimit);
    o.trust_region_initial = 1e6;
    o.trust_region_max = 1e7;
    o.common.max_iterations = 3;
    o.advanced.qp_dump_dir = dir.path.string();
    o.advanced.qp_dump_policy = QpDumpPolicy::kAll;
    (void)SqpSolver(o).solve(p, Vec::Constant(2, 1.0));
    Index rungs = 0;
    for (const fs::path &f : dump_files(dir.path)) {
        const std::string name = f.filename().string();
        if (name.find("_d0_m0_rung") == std::string::npos) {
            continue;
        }
        ++rungs;
        const test_support::QpDump d = test_support::load_qp_dump(f.string());
        // At x = (1, 1) the limit is 0.05 * 2 on both original variables (the slack columns
        // follow them and are not limited).
        for (Index j = 0; j < 2; ++j) {
            EXPECT_LE(d.qp.upper(j), 2.0 * kLimit * (1.0 + 1e-12)) << name << " j = " << j;
            EXPECT_GE(d.qp.lower(j), -2.0 * kLimit * (1.0 + 1e-12)) << name << " j = " << j;
        }
    }
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_GE(rungs, 1) << "premise: major 0's ladder ran";
#else
    hven::test_support::record_off_reference_route("CallerStepLimit.rungs",
                                                   "rungs=" + std::to_string(rungs));
#endif
}

TEST(CallerStepLimitSolve, RestorationStepsStayWithinTheLimitInCallerUnits) {
    constexpr double kLimit = 0.05;
    for (const SqpColumnScaling cs : {SqpColumnScaling::kOff, SqpColumnScaling::kAlways}) {
        SCOPED_TRACE(static_cast<int>(cs));
        Problem p(std::make_shared<InfeasibleBowl>(1e4));
        SqpOptions o = limit_options(kLimit);
        o.column_scaling = cs;
        SqpSolver s(o);
        PathRecorder rec;
        rec.attach(s);
        const SqpResult r = s.solve(p, Vec::Constant(2, 1.0));
        ASSERT_GE(r.counters.restoration_iters, 1) << "premise: the phase ran";
        if (cs == SqpColumnScaling::kAlways) {
            ASSERT_TRUE(r.scaling.columns_active) << "premise";
            ASSERT_GT(r.scaling.column_spread, 2.0) << "premise: the factors differ";
        }
        Index depth1 = 0;
        for (const Index d : rec.depth) {
            depth1 += d == 1 ? 1 : 0;
        }
        ASSERT_GE(depth1, 2) << "premise: restoration rows reached the callback";
        EXPECT_LE(path_excess(rec, 2, kLimit), 0.0)
            << "a step (restoration's included) left r * (1 + |x_j|) in caller units";
    }
}

TEST(CallerStepLimitSolve, TheIssueSeventeenModelEndToEndNeverStepsBeyondTheLimit) {
    // The synthetic weak-column model under kAlways, solved to the end: every visited point,
    // restoration's included, is within the caller-unit limit of the one before.
    constexpr double kLimit = 0.05;
    auto m = std::make_shared<Synth>(4.0);
    const Vec x0 = synth_start(*m);
    Problem p(m);
    SqpOptions o = synth_options(QpMode::kWalk, SqpColumnScaling::kAlways);
    o.common.max_iterations = 300;
    o.caller_step_limit = kLimit;
    SqpSolver s(o);
    PathRecorder rec;
    rec.attach(s);
    const SqpResult r = s.solve(p, x0);
    const double excess = path_excess(rec, 6, kLimit);
    Index depth1 = 0;
    for (const Index d : rec.depth) {
        depth1 += d == 1 ? 1 : 0;
    }
    RecordProperty("solve_status", static_cast<int>(r.status));
    RecordProperty("majors", static_cast<int>(r.counters.major_iters));
    RecordProperty("restoration_entries", static_cast<int>(r.counters.restoration_entries));
    RecordProperty("restoration_iters", static_cast<int>(r.counters.restoration_iters));
    RecordProperty("restoration_rows", static_cast<int>(depth1));
    RecordProperty("elastic_activations", static_cast<int>(r.counters.elastic_activations));
    RecordProperty("path_excess", std::to_string(excess));
    EXPECT_LE(excess, 0.0);
    EXPECT_TRUE(r.x.allFinite());
}

// --- the dump's names ----------------------------------------------------------------------

TEST(QpDump, TwoSolversOnTwoThreadsWriteDistinctFilesIntoOneDirectory) {
    TempDir dir("threads");
    SqpResult ra, rb;
    const auto run = [&dir](SqpResult &out) {
        Problem p(std::make_shared<hven_model_tests::Hs071Problem>());
        SqpSolver s(hs071_dump_options(dir.path, QpDumpPolicy::kAll));
        out = s.solve(p, hven_model_tests::hs071_start());
    };
    std::thread ta(run, std::ref(ra));
    std::thread tb(run, std::ref(rb));
    ta.join();
    tb.join();
    ASSERT_EQ(ra.status, SolveStatus::kOptimal);
    ASSERT_EQ(rb.status, SolveStatus::kOptimal);
    const std::vector<fs::path> files = dump_files(dir.path);
    EXPECT_GE(static_cast<Index>(files.size()), ra.counters.major_iters + rb.counters.major_iters);
    std::set<std::string> solvers;
    for (const fs::path &f : files) {
        const std::string name = f.filename().string();
        const auto s0 = name.find("_s") + 2;
        solvers.insert(name.substr(s0, name.find('_', s0) - s0));
        EXPECT_NO_THROW((void)test_support::load_qp_dump(f.string())) << name;
    }
    EXPECT_EQ(solvers.size(), 2u) << "one solver token per driver";
    for (const auto &e : fs::directory_iterator(dir.path)) {
        EXPECT_EQ(e.path().extension(), ".txt") << "no temporary file is left: " << e.path();
    }
}

TEST(QpDump, ARerunIntoTheSameDirectoryKeepsEveryEarlierFile) {
    TempDir dir("rerun");
    Problem p(std::make_shared<hven_model_tests::Hs071Problem>());
    SqpSolver s(hs071_dump_options(dir.path, QpDumpPolicy::kAll));
    (void)s.solve(p, hven_model_tests::hs071_start());
    const std::vector<fs::path> first = dump_files(dir.path);
    ASSERT_FALSE(first.empty());
    std::vector<std::string> bytes;
    for (const fs::path &f : first) {
        std::ifstream in(f, std::ios::binary);
        bytes.emplace_back(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    (void)s.solve(p, hven_model_tests::hs071_start()); // the same driver again
    SqpSolver other(hs071_dump_options(dir.path, QpDumpPolicy::kAll));
    (void)other.solve(p, hven_model_tests::hs071_start()); // a new driver
    const std::vector<fs::path> after = dump_files(dir.path);
    EXPECT_EQ(after.size(), 3 * first.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        std::ifstream in(first[i], std::ios::binary);
        const std::string now{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        EXPECT_EQ(now, bytes[i]) << first[i] << " was rewritten";
    }
}

TEST(QpDump, AnExistingFileOfTheNextNameIsNeverReplaced) {
    TempDir dir("exists");
    detail::QpProblem qp;
    qp.H = SpMatRM(1, 1);
    qp.H.insert(0, 0) = 1.0;
    qp.g = Vec::Constant(1, 1.0);
    qp.Ae = SpMatRM(0, 1);
    qp.be = Vec(0);
    qp.Ai = SpMatRM(0, 1);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(1, -1.0);
    qp.upper = Vec::Constant(1, 1.0);
    const QpOptions qopts;
    const QpSolveOverrides ov;
    const QpCounters counters;
    const Vec x = Vec::Constant(1, -1.0);
    detail::QpDumpSolve ds;
    ds.qp = &qp;
    ds.opts = &qopts;
    ds.overrides = &ov;
    ds.counters = &counters;
    ds.x_final = &x;
    detail::QpDumpConfig cfg;
    cfg.dir = dir.path.string();
    cfg.policy = QpDumpPolicy::kAll;
    cfg.solver_id = 987654321;
    detail::QpDumpTag tag;
    tag.major = 3;
    tag.site = "main";
    const fs::path first = detail::write_qp_dump(cfg, tag, ds);
    ASSERT_TRUE(fs::exists(first));
    // The name's sequence field is process-wide: the next write takes the one after it.
    const std::string name = first.filename().string();
    const std::string head = "qp_p" + std::to_string(test_pid()) + "_s987654321_";
    ASSERT_EQ(name.rfind(head, 0), 0u) << name;
    const long seq = std::stol(name.substr(head.size(), 5));
    char next_seq[16];
    std::snprintf(next_seq, sizeof next_seq, "%05ld", seq + 1);
    const fs::path blocker = dir.path / (head + next_seq + "_d0_m3_main.txt");
    std::ofstream(blocker) << "keep";
    EXPECT_THROW((void)detail::write_qp_dump(cfg, tag, ds), std::runtime_error);
    std::ifstream in(blocker);
    std::string kept;
    in >> kept;
    EXPECT_EQ(kept, "keep") << "the existing file was replaced";
    for (const auto &e : fs::directory_iterator(dir.path)) {
        EXPECT_EQ(e.path().extension(), ".txt") << "no temporary file is left: " << e.path();
    }
}

} // namespace
} // namespace hven
