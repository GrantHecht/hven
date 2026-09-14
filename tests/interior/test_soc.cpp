// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

///////////////////////////////////////////////////////////////////////////////
// Unit tests for the second-order correction (Wächter & Biegler 2006, §2.4):
// the policy predicates and the correction loop in
// globalization/soc.h, and SocRecovery's own body in
// src/drivers/ipm_solver_globalization.cpp.
//
// The file test_recovery_dispatch_gate.cpp and test_watchdog.cpp have both
// named "test_soc.cpp" as the home of the SOC policy tests since the engine
// migration; until M6 W6 T2 no such file existed, and the whole SOC path was
// cold — `soc.h` read 0/28 lines and `soc_should_trigger`'s call site
// (`ipm_solver_globalization.cpp`, inside `SocRecovery::on_step_rejected`) read
// 0 on the W6 T0 coverage read.
//
// Three layers, each driven at the cheapest faithful level:
//   - soc_should_trigger / soc_should_continue — pure predicates, truth-tabled.
//   - soc_run_loop — the correction-loop driver, run against a scripted
//     primitive (the header documents that shape), so the accept / cap /
//     stagnation exits and the soc_steps accounting are checked without any
//     KKT or NLP machinery.
//   - SocRecovery::on_step_rejected — the live link, driven with a REAL
//     NonLinearProgram (so eval_trial_constraints does a real assemble) and a
//     REAL KktFactorization holding the identity (so the correction solve is
//     exact and the corrected direction is -rhs_soc), plus a scripted
//     GlobalizationMechanism that decides each corrected trial's verdict. What
//     is asserted is the EFFECT of each branch: which Action leaves the link,
//     what is committed into DXSL/alpha, whether the diagnostic
//     fraction-to-boundary lengths are restored, how many corrections were
//     spent, and what the evaluation-error log records on each of the four
//     throw arms.
//
// UNITY RULE: the unity build defeats anonymous namespaces for ODR, so every
// file-local helper here is prefixed Soc* to stay globally unique across the
// interior suite (grep-confirmed no other "Soc" -prefixed file-local symbol
// exists in it).
///////////////////////////////////////////////////////////////////////////////

#include "solver_test_utils.h"

#include "hven/detail/globalization/acceptance_strategy.h"
#include "hven/detail/globalization/globalization_mechanism.h"
#include "hven/detail/globalization/recovery_chain.h"
#include "hven/detail/globalization/soc.h"
#include "hven/detail/interior/eval_error_log.h"
#include "hven/detail/interior/iterate_info.h"
#include "hven/detail/interior/kkt_factorization.h"
#include "hven/detail/model/nlp_adapter.h"
#include "hven/model/nlp_triplet_model.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>

namespace {

using hven::SpMatRM;
using hven::solvers::AcceptanceStrategy;
using hven::solvers::GlobalizationMechanism;
using hven::solvers::IpmSolver;
using hven::solvers::IterateInfo;
using hven::solvers::KktFactorization;
using hven::solvers::kRecoveryDepthUnresolved;
using hven::solvers::kSocRecommendedMaxCorrections;
using hven::solvers::kSocViolationDecrease;
using hven::solvers::NonLinearProgram;
using hven::solvers::ProgressMeasures;
using hven::solvers::RecoveryChain;
using hven::solvers::soc_run_loop;
using hven::solvers::soc_should_continue;
using hven::solvers::soc_should_trigger;
using hven::solvers::SocCorrectionOutcome;
using hven::solvers::SocRecovery;
using hven::solvers::SolverContext;
using TychoTest::InertSolverContext;

using Action = RecoveryChain::Action;

constexpr double kSocInf = std::numeric_limits<double>::infinity();

///////////////////////////////////////////////////////////////////////////////
// soc_should_trigger / soc_should_continue — pure predicates.
///////////////////////////////////////////////////////////////////////////////

IterateInfo soc_rejected_at(int rejection_iter, double theta) {
    IterateInfo it;
    it.first_rejection_iter_ = rejection_iter;
    it.theta_at_first_rejection_ = theta;
    return it;
}

// The trigger fires only on a FIRST-trial rejection whose trial violation did
// not improve on the current point's. Each of the three refusals is a separate
// reason and each is checked on its own.
TEST(SocTrigger, FiresOnlyOnANonImprovingFirstTrialRejection) {
    // Fires: rejection index 0, trial violation >= current.
    EXPECT_TRUE(soc_should_trigger(soc_rejected_at(0, 4.0), 3.0));
    // The boundary is inclusive (>=): an exactly-equal violation still fires.
    EXPECT_TRUE(soc_should_trigger(soc_rejected_at(0, 3.0), 3.0));

    // Refusal 1: the first trial was ACCEPTED and a later rung was rejected,
    // so the step being corrected is not the full one SOC is written for.
    EXPECT_FALSE(soc_should_trigger(soc_rejected_at(1, 4.0), 3.0));
    // Refusal 2: no infeasibility reading (IterateInfo's -1 sentinel; the LANG
    // merit variant records none).
    EXPECT_FALSE(soc_should_trigger(soc_rejected_at(0, -1.0), 3.0));
    // Refusal 3: the trial DID cut the violation, so the rejection was not the
    // Maratos-like case a correction addresses.
    EXPECT_FALSE(soc_should_trigger(soc_rejected_at(0, 2.9), 3.0));
}

// The termination predicate: the cap first, then the required per-correction
// decrease factor.
TEST(SocContinue, StopsAtTheCapAndOnAStagnatingViolation) {
    // Under the cap and cutting the violation by more than the factor: continue.
    EXPECT_TRUE(soc_should_continue(/*trial=*/0.5, /*prev=*/1.0, /*done=*/1, /*max_soc=*/4));
    // The cap is checked BEFORE the decrease, so a perfectly good decrease at
    // the cap still stops.
    EXPECT_FALSE(soc_should_continue(/*trial=*/0.5, /*prev=*/1.0, /*done=*/4, /*max_soc=*/4));
    EXPECT_FALSE(soc_should_continue(/*trial=*/0.5, /*prev=*/1.0, /*done=*/5, /*max_soc=*/4));
    // The decrease boundary is inclusive: exactly kSocViolationDecrease * prev
    // continues, a hair above it does not.
    EXPECT_TRUE(soc_should_continue(kSocViolationDecrease * 1.0, 1.0, 1, 4));
    EXPECT_FALSE(soc_should_continue(kSocViolationDecrease * 1.0 + 1e-12, 1.0, 1, 4));
    // A non-finite violation (the guard value the correction primitive returns
    // on an unusable direction or an unevaluable accumulation point) stops.
    EXPECT_FALSE(soc_should_continue(kSocInf, 1.0, 1, 4));
}

///////////////////////////////////////////////////////////////////////////////
// soc_run_loop — the correction-loop driver, against a scripted primitive.
///////////////////////////////////////////////////////////////////////////////

// Records the (index, prev_violation) pair each call receives and returns the
// next scripted outcome, so the loop's own bookkeeping is visible.
struct SocScriptedPrimitive {
    std::vector<SocCorrectionOutcome> outcomes;
    std::vector<int> seen_index;
    std::vector<double> seen_prev;

    SocCorrectionOutcome operator()(int index, double prev) {
        seen_index.push_back(index);
        seen_prev.push_back(prev);
        return outcomes.at(static_cast<std::size_t>(index));
    }
};

// An accepted correction leaves immediately with kRetry, and the remaining
// budget is NOT spent.
TEST(SocRunLoop, AnAcceptedCorrectionRetriesAndStopsEarly) {
    SocScriptedPrimitive primitive{{{true, 0.0}, {true, 0.0}}, {}, {}};
    int soc_steps = 0;

    const Action action =
        soc_run_loop(/*first_trial_violation=*/1.0, /*max_soc=*/4, soc_steps, primitive);

    EXPECT_EQ(action, Action::kRetry);
    EXPECT_EQ(soc_steps, 1); // one attempt, counted once
    ASSERT_EQ(primitive.seen_index.size(), 1u);
    EXPECT_EQ(primitive.seen_index[0], 0);
    EXPECT_DOUBLE_EQ(primitive.seen_prev[0], 1.0); // seeded from the first trial
}

// Rejections that keep cutting the violation run until the cap, and the loop
// carries the PREVIOUS correction's violation forward as the next comparison
// point.
TEST(SocRunLoop, RejectionsRunToTheCapCarryingTheViolationForward) {
    SocScriptedPrimitive primitive{
        {{false, 0.5}, {false, 0.25}, {false, 0.125}, {false, 0.0625}}, {}, {}};
    int soc_steps = 0;

    const Action action =
        soc_run_loop(/*first_trial_violation=*/1.0, /*max_soc=*/4, soc_steps, primitive);

    EXPECT_EQ(action, Action::kAcceptAsIs); // the rejected step is taken as-is
    EXPECT_EQ(soc_steps, 4);                // one back-substitution per attempt
    ASSERT_EQ(primitive.seen_prev.size(), 4u);
    EXPECT_DOUBLE_EQ(primitive.seen_prev[0], 1.0);
    EXPECT_DOUBLE_EQ(primitive.seen_prev[1], 0.5);
    EXPECT_DOUBLE_EQ(primitive.seen_prev[2], 0.25);
    EXPECT_DOUBLE_EQ(primitive.seen_prev[3], 0.125);
}

// A stagnating violation stops the loop with budget still on the table: the
// first correction always runs, the second is what soc_should_continue gates.
TEST(SocRunLoop, AStagnatingViolationStopsBelowTheCap) {
    SocScriptedPrimitive primitive{{{false, 1.0}, {false, 0.1}}, {}, {}};
    int soc_steps = 0;

    const Action action =
        soc_run_loop(/*first_trial_violation=*/1.0, /*max_soc=*/4, soc_steps, primitive);

    EXPECT_EQ(action, Action::kAcceptAsIs);
    EXPECT_EQ(soc_steps, 1);
    EXPECT_EQ(primitive.seen_index.size(), 1u) << "the second correction must not have been run";
}

///////////////////////////////////////////////////////////////////////////////
// SocRecovery::on_step_rejected — the live link.
///////////////////////////////////////////////////////////////////////////////

// The smallest model that gives the correction something real to evaluate: two
// primals, one equality row and one inequality row, no variable bounds (so the
// bound branches stay provably dead and ctx.bounds_ stays null). eval_g can be
// told to throw on its Nth call, which is how the four trial-evaluation catch
// arms are reached: call 1 is the correction SEED, call 2 the accumulation
// point after the first rejected correction.
struct SocTrialProblem : hven::solvers::NlpTripletModel {
    /// 1-based call index of eval_g that must throw; 0 disables the fault.
    int throw_on_call = 0;
    /// When true the fault is a non-std::exception, taking the `catch (...)`
    /// arm rather than the `catch (const std::exception &)` one.
    bool throw_unknown = false;
    mutable int eval_g_calls = 0;

    int num_vars() const override { return 2; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 4; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kSocInf, -kSocInf;
        xu << kSocInf, kSocInf;
        gl << 1.0, 0.0; // row 0 is an equality (gl == gu), row 1 a >= row
        gu << 1.0, kSocInf;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * (x[0] * x[0] + x[1] * x[1]);
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0];
        g[1] = x[1];
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        ++eval_g_calls;
        if (throw_on_call != 0 && eval_g_calls == throw_on_call) {
            if (throw_unknown) {
                throw 42; // NOLINT(hicpp-exception-baseclass) -- the `catch (...)` arm
            }
            throw std::runtime_error("SocTrialProblem: constraints refused at this point");
        }
        g[0] = x[0] + x[1];
        g[1] = x[0] - x[1];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 1, 1;
        c << 0, 1, 0, 1;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd>,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 1.0, 1.0, 1.0, -1.0;
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   hven::ConstEigenRef<Eigen::VectorXd>,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v << obj_factor, obj_factor;
    }
    std::string name() const override { return "SocTrialProblem"; }
};

// SocRecovery reads drives_classic_path() and hands the strategy on to the
// mechanism; nothing else on this interface is its business, so every other
// member is a wiring bug if it is reached.
class SocUnusedAcceptance : public AcceptanceStrategy {
  public:
    bool drives_classic_path() const override { return true; }
    bool is_iterate_acceptable(const ProgressMeasures &, const ProgressMeasures &,
                               const ProgressMeasures &, double, double) override {
        ADD_FAILURE() << "SocRecovery must reach acceptance only through the mechanism";
        return false;
    }
    bool is_infeasibility_sufficiently_reduced(const ProgressMeasures &,
                                               const ProgressMeasures &) const override {
        return false;
    }
    void reset() override {}
};

/// One scripted verdict for one corrected trial.
struct SocScriptedVerdict {
    bool accepted;
    double alpha_soc;
    /// The corrected trial's own infeasibility, stamped onto the fresh
    /// IterateInfo the link hands the mechanism. Only read when !accepted; a
    /// negative value is the "no reading available" sentinel.
    double theta;
};

// Scripted mechanism: records the corrected direction it is handed, stamps the
// scripted verdict onto the trial IterateInfo, and reports the scripted step
// length. max_primal_dual_step writes the fraction-to-boundary lengths the
// correction is supposed to overwrite, so the restore-on-decline behaviour is
// observable.
class SocScriptedMechanism : public GlobalizationMechanism {
  public:
    explicit SocScriptedMechanism(std::vector<SocScriptedVerdict> verdicts)
        : verdicts_(std::move(verdicts)) {}

    double compute_step(IpmSolver::LineSearchModes, double, double, double, double,
                        Eigen::VectorXd &, Eigen::VectorXd &, Eigen::VectorXd &, Eigen::VectorXd &,
                        Eigen::VectorXd &, AcceptanceStrategy &, double &, double &, IterateInfo &,
                        const std::vector<IterateInfo> &, SolverContext &) override {
        ADD_FAILURE() << "SocRecovery drives run_acceptance_backtrack, never compute_step";
        return 1.0;
    }

    void max_primal_dual_step(Eigen::VectorXd &, Eigen::VectorXd &, double, double &alphap,
                              double &alphad, const SolverContext &) override {
        ++scalings_;
        alphap = kScaledAlphaP;
        alphad = kScaledAlphaD;
    }

    double run_acceptance_backtrack(IpmSolver::LineSearchModes, double, double, double, double,
                                    Eigen::VectorXd &, Eigen::VectorXd &DXSL, Eigen::VectorXd &,
                                    Eigen::VectorXd &, Eigen::VectorXd &, AcceptanceStrategy &,
                                    IterateInfo &Citer, const std::vector<IterateInfo> &,
                                    SolverContext &) override {
        recorded_dxsl_.push_back(DXSL);
        const SocScriptedVerdict verdict = verdicts_.at(static_cast<std::size_t>(calls_));
        ++calls_;
        Citer.accepted_ = verdict.accepted;
        if (verdict.accepted) {
            Citer.ls_iters_ = kAcceptedLsIters;
            Citer.merit_val_ = kAcceptedMerit;
        } else {
            Citer.first_rejection_iter_ = 0;
            Citer.theta_at_first_rejection_ = verdict.theta;
        }
        return verdict.alpha_soc;
    }

    void reset() override {}

    static constexpr double kScaledAlphaP = 0.25;
    static constexpr double kScaledAlphaD = 0.125;
    static constexpr int kAcceptedLsIters = 3;
    static constexpr double kAcceptedMerit = 11.0;

    int calls_ = 0;
    int scalings_ = 0;
    std::vector<Eigen::VectorXd> recorded_dxsl_;

  private:
    std::vector<SocScriptedVerdict> verdicts_;
};

// Everything one on_step_rejected call needs, owned so the SolverContext's
// references stay valid for the whole test (solver_test_utils.h's lifetime
// rule). The KKT factor holds the IDENTITY, so the correction solve returns
// rhs_soc unchanged and the corrected direction is exactly -rhs_soc — which is
// what lets the committed direction be asserted by value.
struct SocDrive {
    std::shared_ptr<SocTrialProblem> problem = std::make_shared<SocTrialProblem>();
    std::shared_ptr<NonLinearProgram> program;
    SpMatRM analyzed_pattern;
    InertSolverContext inert;
    hven::solvers::EvalErrorLog errors;

    Eigen::VectorXd XSL, DXSL, XSL2, RHS, RHS2;
    IterateInfo citer;
    std::vector<IterateInfo> iters;

    double alpha = 1.0;
    double alphap = kEntryAlphaP;
    double alphad = kEntryAlphaD;
    int soc_steps = 0;
    int resolved_depth = kRecoveryDepthUnresolved;
    int watchdog_activations = 0;

    static constexpr double kEntryAlphaP = 0.9;
    static constexpr double kEntryAlphaD = 0.8;

    explicit SocDrive(int max_soc) {
        program = hven::solvers::make_nlp_program(problem);

        // The engine lays the KKT pattern before it evaluates; do the same so
        // the residual assemble below runs against an analysed program.
        analyzed_pattern.resize(program->kkt_dim_, program->kkt_dim_);
        program->analyze_sparsity(analyzed_pattern);

        inert.nlp_ = program.get();
        inert.primal_vars_ = program->primal_vars_;
        inert.slack_vars_ = program->slack_vars_;
        inert.equal_cons_ = program->equal_cons_;
        inert.inequal_cons_ = program->inequal_cons_;
        inert.kkt_dim_ = program->kkt_dim_;
        inert.eval_errors_ = &errors;
        inert.opts_.max_soc = max_soc;

        // The correction's own factor: the identity at the KKT dimension, so
        // solve() is exact and the corrected direction is -rhs_soc.
        KktFactorization::Options factor_opts;
        factor_opts.kind = hven::linear::FactorKind::kLDLT;
        factor_opts.num_threads = 1;
        inert.kkt_solver_.reconfigure(factor_opts);
        inert.kkt_solver_.matrix() = soc_identity(program->kkt_dim_);
        inert.kkt_solver_.compute();

        const int n = program->kkt_dim_;
        XSL = Eigen::VectorXd::Constant(n, 0.5);
        DXSL = Eigen::VectorXd::Constant(n, 0.1);
        XSL2 = Eigen::VectorXd::Zero(n);
        RHS = Eigen::VectorXd::Constant(n, 0.2);
        RHS2 = Eigen::VectorXd::Zero(n);

        // A first-trial rejection whose violation did not improve on the
        // current point's: the trigger's one firing case. RHS's constraint
        // block is the current measure (squared L2 on the classic path).
        citer.first_rejection_iter_ = 0;
        citer.theta_at_first_rejection_ = 10.0;
    }

    static SpMatRM soc_identity(int n) {
        SpMatRM m(n, n);
        std::vector<Eigen::Triplet<double>> entries;
        entries.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            entries.emplace_back(i, i, 1.0);
        }
        m.setFromTriplets(entries.begin(), entries.end());
        m.makeCompressed();
        return m;
    }

    Action run(SocRecovery &soc, AcceptanceStrategy &acceptance,
               GlobalizationMechanism &mechanism) {
        SolverContext ctx = inert.ctx();
        return soc.on_step_rejected(citer, iters, ctx, acceptance, mechanism,
                                    IpmSolver::LineSearchModes::kAugLang, /*obj_scale=*/1.0,
                                    /*mu=*/1e-3, /*prim_obj=*/0.0, /*barr_obj=*/0.0, XSL, DXSL,
                                    XSL2, RHS, RHS2, alpha, alphap, alphad, soc_steps,
                                    resolved_depth, watchdog_activations);
    }
};

// max_soc == 0: the defensive guard. SocRecovery is only built when SOC is on,
// so this is the belt-and-braces early return — it must touch nothing.
TEST(SocRecovery, DisabledDeclinesWithoutEvaluatingAnything) {
    SocDrive drive(/*max_soc=*/0);
    SocUnusedAcceptance acceptance;
    SocScriptedMechanism mechanism({});
    SocRecovery soc;

    EXPECT_EQ(drive.run(soc, acceptance, mechanism), Action::kAcceptAsIs);
    EXPECT_EQ(drive.soc_steps, 0);
    EXPECT_EQ(drive.problem->eval_g_calls, 0) << "no trial constraint evaluation may be spent";
    EXPECT_EQ(mechanism.calls_, 0);
    EXPECT_DOUBLE_EQ(drive.alphap, SocDrive::kEntryAlphaP);
    EXPECT_DOUBLE_EQ(drive.alphad, SocDrive::kEntryAlphaD);
}

// The trigger's refusal, through the link: a rejection at a LATER rung is not
// the case SOC corrects, so the link declines before the seed evaluation.
TEST(SocRecovery, ATriggerRefusalCostsNoEvaluation) {
    SocDrive drive(kSocRecommendedMaxCorrections);
    drive.citer.first_rejection_iter_ = 1; // not the first trial
    SocUnusedAcceptance acceptance;
    SocScriptedMechanism mechanism({});
    SocRecovery soc;

    EXPECT_EQ(drive.run(soc, acceptance, mechanism), Action::kAcceptAsIs);
    EXPECT_EQ(drive.soc_steps, 0);
    EXPECT_EQ(drive.problem->eval_g_calls, 0);
    EXPECT_EQ(mechanism.calls_, 0);
}

// An accepted correction: kRetry, the corrected direction and its step length
// committed in place of the rejected ones, the iterate stamped from the
// corrected trial, and the fraction-to-boundary lengths KEPT (they describe the
// step that is about to be taken).
TEST(SocRecovery, AnAcceptedCorrectionCommitsTheCorrectedStep) {
    SocDrive drive(kSocRecommendedMaxCorrections);
    const Eigen::VectorXd rejected_dxsl = drive.DXSL;
    SocUnusedAcceptance acceptance;
    SocScriptedMechanism mechanism({{/*accepted=*/true, /*alpha_soc=*/0.6, /*theta=*/0.0}});
    SocRecovery soc;

    EXPECT_EQ(drive.run(soc, acceptance, mechanism), Action::kRetry);

    EXPECT_EQ(drive.soc_steps, 1) << "one attempted correction == one back-substitution";
    EXPECT_EQ(mechanism.calls_, 1);
    EXPECT_EQ(mechanism.scalings_, 1) << "the corrected direction is fraction-to-boundary scaled";

    ASSERT_EQ(mechanism.recorded_dxsl_.size(), 1u);
    // The committed direction IS the corrected one the acceptance test saw,
    // not the rejected one it replaced.
    ASSERT_EQ(drive.DXSL.size(), rejected_dxsl.size());
    EXPECT_TRUE(drive.DXSL.isApprox(mechanism.recorded_dxsl_[0]));
    EXPECT_FALSE(drive.DXSL.isApprox(rejected_dxsl));
    EXPECT_DOUBLE_EQ(drive.alpha, 0.6);

    EXPECT_TRUE(drive.citer.accepted_);
    EXPECT_EQ(drive.citer.ls_iters_, SocScriptedMechanism::kAcceptedLsIters);
    EXPECT_DOUBLE_EQ(drive.citer.merit_val_, SocScriptedMechanism::kAcceptedMerit);

    // Kept, not restored: the accepted corrected step's own lengths.
    EXPECT_DOUBLE_EQ(drive.alphap, SocScriptedMechanism::kScaledAlphaP);
    EXPECT_DOUBLE_EQ(drive.alphad, SocScriptedMechanism::kScaledAlphaD);
}

// The corrected direction is the solve's own answer negated: on the identity
// factor that is exactly -(RHS with its constraint block replaced by c_soc), so
// the correction really did re-solve with a corrected right-hand side and left
// the objective block alone.
TEST(SocRecovery, TheCorrectedDirectionSolvesTheCorrectedRightHandSide) {
    SocDrive drive(kSocRecommendedMaxCorrections);
    SocUnusedAcceptance acceptance;
    SocScriptedMechanism mechanism({{true, 1.0, 0.0}});
    SocRecovery soc;

    ASSERT_EQ(drive.run(soc, acceptance, mechanism), Action::kRetry);
    ASSERT_EQ(mechanism.recorded_dxsl_.size(), 1u);

    const int ncons = drive.inert.equal_cons_ + drive.inert.inequal_cons_;
    const int head = drive.inert.kkt_dim_ - ncons;
    const Eigen::VectorXd &corrected = mechanism.recorded_dxsl_[0];
    // The objective block: untouched RHS, negated by the solve's sign convention.
    EXPECT_TRUE(corrected.head(head).isApprox(-drive.RHS.head(head)));
    // The constraint block: NOT the original RHS block — it carries c_soc.
    EXPECT_FALSE(corrected.tail(ncons).isApprox(-drive.RHS.tail(ncons)));
    EXPECT_TRUE(corrected.allFinite());
    EXPECT_EQ(drive.problem->eval_g_calls, 1) << "the seed evaluation, and only it";
}

// Every correction rejected, each cutting the violation enough to earn another:
// the cap bounds the spend, the originally-rejected step is taken as-is, and the
// diagnostic fraction-to-boundary lengths are RESTORED so a rejected correction
// leaves no residue.
TEST(SocRecovery, DeclinedCorrectionsRunToTheCapAndRestoreTheStepLengths) {
    SocDrive drive(/*max_soc=*/3);
    const Eigen::VectorXd rejected_dxsl = drive.DXSL;
    SocUnusedAcceptance acceptance;
    SocScriptedMechanism mechanism({{false, 0.5, 5.0}, {false, 0.5, 2.0}, {false, 0.5, 1.0}});
    SocRecovery soc;

    EXPECT_EQ(drive.run(soc, acceptance, mechanism), Action::kAcceptAsIs);

    EXPECT_EQ(drive.soc_steps, 3) << "the cap bounds the corrections attempted";
    EXPECT_EQ(mechanism.calls_, 3);
    EXPECT_TRUE(drive.DXSL.isApprox(rejected_dxsl)) << "the rejected step is what is taken";
    EXPECT_DOUBLE_EQ(drive.alpha, 1.0);
    EXPECT_DOUBLE_EQ(drive.alphap, SocDrive::kEntryAlphaP);
    EXPECT_DOUBLE_EQ(drive.alphad, SocDrive::kEntryAlphaD);
    // The seed and at least the first accumulation were really evaluated. Not
    // one per correction: on this fixture the corrected directions share a
    // primal block (the identity factor negates the RHS, whose objective block
    // the correction never touches), so the later accumulation points repeat and
    // NlpProblemModel's per-iterate evaluation cache -- keyed on x, which its
    // header documents as sound because the callbacks are pure -- serves them
    // without calling the model again.
    EXPECT_GE(drive.problem->eval_g_calls, 2);
}

// A rejected correction whose violation did not fall far enough stops the loop
// with budget left, and the entry lengths come back.
TEST(SocRecovery, AStagnatingCorrectionStopsBelowTheCap) {
    SocDrive drive(kSocRecommendedMaxCorrections);
    SocUnusedAcceptance acceptance;
    // theta 10.0 against the seed's prev 10.0: no decrease at all.
    SocScriptedMechanism mechanism({{false, 0.5, 10.0}, {false, 0.5, 1.0}});
    SocRecovery soc;

    EXPECT_EQ(drive.run(soc, acceptance, mechanism), Action::kAcceptAsIs);
    EXPECT_EQ(drive.soc_steps, 1);
    EXPECT_EQ(mechanism.calls_, 1) << "the second correction must not have been attempted";
    EXPECT_DOUBLE_EQ(drive.alphap, SocDrive::kEntryAlphaP);
    EXPECT_DOUBLE_EQ(drive.alphad, SocDrive::kEntryAlphaD);
}

// A corrected trial the acceptance test rejected WITHOUT an infeasibility
// reading (theta < 0, the LANG variant's case) gives the loop nothing to
// measure, so it stops after that one correction.
TEST(SocRecovery, ARejectionWithoutAnInfeasibilityReadingStopsTheLoop) {
    SocDrive drive(kSocRecommendedMaxCorrections);
    SocUnusedAcceptance acceptance;
    SocScriptedMechanism mechanism({{false, 0.5, -1.0}, {false, 0.5, 0.1}});
    SocRecovery soc;

    EXPECT_EQ(drive.run(soc, acceptance, mechanism), Action::kAcceptAsIs);
    EXPECT_EQ(drive.soc_steps, 1);
    EXPECT_EQ(mechanism.calls_, 1);
    // No accumulation evaluation is spent on a reading-less rejection.
    EXPECT_EQ(drive.problem->eval_g_calls, 1);
}

// The four trial-evaluation fault arms. The SEED evaluation is eval_g call 1
// and the ACCUMULATION after the first rejected correction is call 2; each
// throw site has a std::exception arm (the message is logged) and a
// `catch (...)` arm (the log records the unknown-type sentence). The effect
// asserted is the same on all four: the link declines rather than unwinding the
// solve, and the evaluation-error log carries exactly one record.
void soc_expect_eval_fault(int throw_on_call, bool unknown, int expected_soc_steps,
                           int expected_mechanism_calls) {
    SocDrive drive(kSocRecommendedMaxCorrections);
    drive.problem->throw_on_call = throw_on_call;
    drive.problem->throw_unknown = unknown;
    SocUnusedAcceptance acceptance;
    SocScriptedMechanism mechanism({{false, 0.5, 1.0}, {false, 0.5, 0.5}});
    SocRecovery soc;

    EXPECT_EQ(drive.run(soc, acceptance, mechanism), Action::kAcceptAsIs);
    EXPECT_EQ(drive.soc_steps, expected_soc_steps);
    EXPECT_EQ(mechanism.calls_, expected_mechanism_calls);
    EXPECT_EQ(drive.errors.count_, 1) << "exactly one evaluation failure was recorded";
    if (unknown) {
        EXPECT_EQ(drive.errors.last_message_,
                  "unknown exception type (not derived from std::exception)");
    } else {
        EXPECT_EQ(drive.errors.last_message_, "SocTrialProblem: constraints refused at this point");
    }
    // Either way the entry lengths come back: no correction was committed.
    EXPECT_DOUBLE_EQ(drive.alphap, SocDrive::kEntryAlphaP);
    EXPECT_DOUBLE_EQ(drive.alphad, SocDrive::kEntryAlphaD);
}

TEST(SocRecovery, AnUnevaluableSeedDeclinesAndLogsTheMessage) {
    soc_expect_eval_fault(/*throw_on_call=*/1, /*unknown=*/false, /*expected_soc_steps=*/0,
                          /*expected_mechanism_calls=*/0);
}

TEST(SocRecovery, AnUnevaluableSeedOfUnknownTypeDeclinesAndLogsTheSentinel) {
    soc_expect_eval_fault(/*throw_on_call=*/1, /*unknown=*/true, /*expected_soc_steps=*/0,
                          /*expected_mechanism_calls=*/0);
}

TEST(SocRecovery, AnUnevaluableAccumulationPointEndsTheLoopAndLogsTheMessage) {
    soc_expect_eval_fault(/*throw_on_call=*/2, /*unknown=*/false, /*expected_soc_steps=*/1,
                          /*expected_mechanism_calls=*/1);
}

TEST(SocRecovery, AnUnevaluableAccumulationPointOfUnknownTypeLogsTheSentinel) {
    soc_expect_eval_fault(/*throw_on_call=*/2, /*unknown=*/true, /*expected_soc_steps=*/1,
                          /*expected_mechanism_calls=*/1);
}

// reset() is the ownership rule's no-op: SocRecovery holds no state, so a reset
// between iterations cannot change what the next correction does.
TEST(SocRecovery, ResetIsAStatelessNoOp) {
    SocDrive drive(kSocRecommendedMaxCorrections);
    SocUnusedAcceptance acceptance;
    SocScriptedMechanism mechanism({{true, 0.6, 0.0}});
    SocRecovery soc;

    soc.reset();
    EXPECT_EQ(drive.run(soc, acceptance, mechanism), Action::kRetry);
    EXPECT_EQ(drive.soc_steps, 1);
}

} // namespace
