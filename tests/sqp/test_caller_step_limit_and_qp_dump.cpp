// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_caller_step_limit_and_qp_dump.cpp -- M8 I17: SqpOptions::caller_step_limit on a synthetic
// weak-column model (a thrust-direction vector d * u(RA, dec) in degrees, hven issue 17), the
// separate column-amplification cap, and the public QP dump (advanced.qp_dump_dir).
//
//   (a) THE STEP LIMIT: under column_scaling = kAlways the unlimited first step moves an angle by
//       tens of degrees; with the limit on, every component of the first step is within
//       r * (1 + |x_j|) of the start, in kWalk, kSsn and kIpqp. Asserted on the step itself
//       (bounds), never on a QP route.
//   (b) THE COLUMN CAP: column_scaling_factor_max clamps the column factors alone.
//   (c) THE DUMP: a solve with the dump on writes D4QP files that the test loader reads and that
//       re-solve to the dumped status and objective; `kFailed` writes only failed solves; an
//       unwritable directory throws; the option's validation.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/hven.h>

#include "model/support/hs071_problem.h"
#include "qp/support/qp_dump.h"

namespace hven {
namespace {

namespace fs = std::filesystem;

constexpr double kDeg = M_PI / 180.0;

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

struct TempDir {
    fs::path path;
    explicit TempDir(const std::string &tag) {
        const char *base = std::getenv("TMPDIR");
        path = fs::path(base != nullptr ? base : fs::temp_directory_path().string()) /
               ("hven_i17_" + tag + "_" +
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
        out.push_back(e.path());
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
        if (mode == QpMode::kWalk) {
            EXPECT_GT(step_excess(x0, r_off.x, kLimit), 0.0);
        }

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
    o.advanced.qp_dump_policy = SqpQpDumpPolicy::kAll;
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
}

// --- the QP dump -----------------------------------------------------------------------------

int dumped_status(const test_support::QpDump &d) {
    const auto at = d.result.find("status=");
    return at == std::string::npos ? -1 : std::stoi(d.result.substr(at + 7));
}

SqpOptions hs071_dump_options(const fs::path &dir, SqpQpDumpPolicy policy) {
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.advanced.qp_dump_dir = dir.string();
    o.advanced.qp_dump_policy = policy;
    return o;
}

TEST(QpDump, EveryWalkSolveRoundTripsThroughTheLoaderToItsStatusAndObjective) {
    TempDir dir("all");
    Problem p(std::make_shared<hven_model_tests::Hs071Problem>());
    SqpSolver s(hs071_dump_options(dir.path, SqpQpDumpPolicy::kAll));
    const SqpResult r = s.solve(p, hven_model_tests::hs071_start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    const std::vector<fs::path> files = dump_files(dir.path);
    ASSERT_GE(static_cast<Index>(files.size()), r.counters.major_iters)
        << "one file per walk solve, the major's QP at least";
    for (const fs::path &f : files) {
        SCOPED_TRACE(f.string());
        EXPECT_NE(f.filename().string().find("_d0_m"), std::string::npos) << "major tag";
        EXPECT_NE(f.filename().string().find("_main.txt"), std::string::npos) << "site tag";
        const test_support::QpDump d = test_support::load_qp_dump(f.string());
        const detail::QpSolution again = test_support::solve_qp_dump(d, d.opts);
        EXPECT_EQ(static_cast<int>(again.status), dumped_status(d));
        const double fa = test_support::qp_dump_objective(d, again.x);
        const double fd = test_support::qp_dump_objective(d, d.x_final);
        EXPECT_NEAR(fa, fd, 1e-8 * (1.0 + std::abs(fd)));
    }
}

TEST(QpDump, OffWritesNothingAndFailedWritesOnlyFailedSolves) {
    {
        TempDir dir("off");
        Problem p(std::make_shared<hven_model_tests::Hs071Problem>());
        SqpSolver s(hs071_dump_options(dir.path, SqpQpDumpPolicy::kOff));
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
            SqpSolver s(hs071_dump_options(all.path, SqpQpDumpPolicy::kAll));
            (void)s.solve(p, x0);
        }
        {
            Problem p(std::make_shared<Inf>());
            SqpSolver s(hs071_dump_options(failed.path, SqpQpDumpPolicy::kFailed));
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
        EXPECT_TRUE(saw_rung) << "the ladder's rungs are dumped, tagged by rung";
    }
}

TEST(QpDump, AnUnwritableDirectoryThrowsAndAnOnPolicyNeedsADirectory) {
    TempDir dir("blocked");
    fs::create_directories(dir.path);
    const fs::path blocker = dir.path / "a_file";
    std::ofstream(blocker) << "x";
    SqpOptions o = hs071_dump_options(blocker / "sub", SqpQpDumpPolicy::kAll);
    SqpSolver s(o);
    Problem p(std::make_shared<hven_model_tests::Hs071Problem>());
    EXPECT_THROW((void)s.solve(p, hven_model_tests::hs071_start()), std::runtime_error);

    SqpOptions bad;
    bad.advanced.qp_dump_policy = SqpQpDumpPolicy::kFailed;
    EXPECT_THROW(validate(bad), std::invalid_argument);
}

} // namespace
} // namespace hven
