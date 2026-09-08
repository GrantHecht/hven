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
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <gtest/gtest.h>

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

/// @brief Serializes one record through a throwaway sink, so the oracle's copy
/// of an `IterateInfo` is compared through the SAME writer the stream used --
/// a field-by-field comparison here would be a second, weaker copy of the key
/// list.
std::string serialize_iter(const IterateInfo &record, Index phase) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sink.on_ipm_iter(IpmIterTraceEvent{record, phase});
    return body_after_envelope(split_lines(os.str()).front());
}

/// @brief Records every `IterateInfo` the LATE CALLBACK is handed -- the
/// oracle. Installed alongside the sink so both observe the same iterations.
struct CallbackOracle {
    std::vector<IterateInfo> seen;

    InteriorPointSolver::LateCallBackType hook() {
        return [this](const IterateInfo &it, ConstEigenRef<Eigen::VectorXd>,
                      ConstEigenRef<Eigen::VectorXd>) {
            this->seen.push_back(it);
            return 0;
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
    solver.optimizer_->set_late_callback(oracle.hook());

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
    CallbackOracle oracle;
    solver.optimizer_->attach_trace(&sink);
    solver.optimizer_->set_late_callback(oracle.hook());
    ASSERT_EQ(solver.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);

    const std::vector<std::string> iter_lines = lines_of_event(os.str(), "ipm.iter");
    ASSERT_EQ(iter_lines.size(), oracle.seen.size());
    ASSERT_FALSE(iter_lines.empty());
    for (std::size_t k = 0; k < iter_lines.size(); ++k) {
        // The oracle's record is re-serialized through the same writer, so this
        // compares EVERY field of the line -- including the two `null`
        // conventions -- and not a hand-picked subset.
        EXPECT_EQ(body_after_envelope(iter_lines[k]), serialize_iter(oracle.seen[k], 0))
            << "at ipm.iter line " << k;
    }
    // FALSIFIABILITY: the comparison above must be able to fail. A record whose
    // iteration index is bumped serializes differently.
    IterateInfo mutated = oracle.seen.back();
    mutated.iter_ += 1;
    EXPECT_NE(body_after_envelope(iter_lines.back()), serialize_iter(mutated, 0));
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
    solver.optimizer_->set_late_callback(oracle.hook());
    ASSERT_EQ(solver.optimize(hs071_start()), hven::solvers::SolveStatus::kOptimal);

    const std::vector<std::string> iter_lines = lines_of_event(os.str(), "ipm.iter");
    ASSERT_FALSE(iter_lines.empty());
    ASSERT_FALSE(oracle.seen.empty());
    EXPECT_EQ(body_after_envelope(iter_lines.back()), serialize_iter(oracle.seen.back(), 0));
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
    EXPECT_EQ(sqp_out.status, SqpStatus::kOptimal);
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

} // namespace
} // namespace hven::solvers
