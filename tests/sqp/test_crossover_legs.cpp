// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_crossover_legs.cpp — the correctness gate for
// bench/crossover_legs.h, the M5 W5 measurement instrument.
//
// The bench binary that produces the W5 artifact
// (docs/notes/data/2026-08-m5-w5-crossover/) is not ctest-registered: a full
// sweep of the dual-bindable census runs for hours. This file is what stands
// behind it, on the repository's standing rule that every measurement binary's
// correctness gate is a registered test SHARING ITS IMPLEMENTATION -- so
// crossover_legs.h is quote-included here, never copied.
//
// WHAT IS GATED, and why each item is the thing that could silently corrupt an
// artifact rather than fail loudly:
//
//   * ModelAsNlpProblem states the SAME PROBLEM the model states. If the
//     adapter dropped a Jacobian block, transposed the Hessian wrongly, or got
//     the row-kind mapping backwards, every leg would still run and every
//     counter would still be a number -- of a different problem. The round trip
//     is checked through NlpProblemModel, which is the path the SQP leg
//     actually takes.
//   * The two engines key the SAME DeclarationKey off it. That is the entire
//     premise of the dual-bind path: if the keys parted, leg (c)/(d) would be
//     refused at solve entry and the artifact would be all-cold rows.
//   * The moving-pattern guard fires. NLPProblem's structures are queried once;
//     a model whose pattern depends on the iterate cannot be stated as one, and
//     the adapter must say so rather than write a wrong value into a declared
//     slot.
//   * The dual-bind partition is the one the artifact documents (24 measured,
//     33 refused, with a reason on every refusal).
//
// Names carry a `Crossover` prefix: this suite's TUs share a link unit.

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <gtest/gtest.h>

#include <hven/model/nlp_model_aggregate.h>
#include <hven/model/nlp_problem_model.h>
#include <hven/model/nlp_solver.h>
#include <hven/model/structure_identity.h>
#include <hven/warmstart/ipm_polish_extension.h>
#include <hven/warmstart/warm_start_data.h>

#include "../../bench/crossover_legs.h"

namespace {

using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::declaration_key;
using hven::solvers::NlpModel;
using hven::solvers::NlpModelAggregate;
using hven::solvers::NlpProblemModel;
using hven::solvers::NLPSolver;
using hven::solvers::SqpDriver;
using hven::solvers::SqpOptions;
using hven::solvers::SqpSolution;
using hven::solvers::SqpStatus;
using hven::solvers::StartLevel;
using hven::solvers::WarmStartData;
using hven::solvers::corpus::CorpusCell;
using hven::solvers::corpus::StartTaxonomy;
using hven::solvers::crossover::ModelAsNlpProblem;
using hven::solvers::test_support::F7CollocationChain;

// A collocation chain small enough for a test to solve on both engines, at the
// corpus's own path-window parameter so the inequality block is genuinely
// active -- an empty active set would leave the row-kind mapping untested where
// it matters most.
constexpr Index kCrossoverNodes = 12;
constexpr double kCrossoverP = 0.85;

std::shared_ptr<F7CollocationChain> crossover_model() {
    return std::make_shared<F7CollocationChain>(kCrossoverNodes, /*states=*/3, /*controls=*/2,
                                                kCrossoverP, /*radius=*/1.0);
}

// A probe point that is neither the start point nor the optimum, so an
// evaluation that silently ignored x would be caught.
Vec probe_point(const F7CollocationChain &model) {
    Vec x = model.start_point();
    for (Index i = 0; i < x.size(); ++i) {
        x(i) += 0.01 * std::sin(3.0 * static_cast<double>(i));
    }
    return x;
}

void expect_sparse_near(const SpMatRM &a, const SpMatRM &b, const std::string &what) {
    ASSERT_EQ(a.rows(), b.rows()) << what;
    ASSERT_EQ(a.cols(), b.cols()) << what;
    const Eigen::MatrixXd da = Eigen::MatrixXd(a);
    const Eigen::MatrixXd db = Eigen::MatrixXd(b);
    EXPECT_LT((da - db).cwiseAbs().maxCoeff(), 1e-12) << what;
}

} // namespace

// --- The adapter states the same problem ---

// THE ROUND TRIP, through the path the SQP leg actually takes. The model is
// declared as an NLPProblem and read straight back as a native model; every
// piece of the contract must survive both directions. A single wrong sign in
// the row-kind mapping shows up here as a cI block that differs by more than a
// tolerance, which is the failure that would otherwise be invisible in a CSV
// full of plausible counters.
TEST(CrossoverAdapter, StatesTheSameProblemAsTheModelItWraps) {
    const auto model = crossover_model();
    const auto declared = std::make_shared<ModelAsNlpProblem>(model, "crossover_gate");
    const NlpProblemModel converted(declared);

    ASSERT_EQ(converted.n(), model->n());
    ASSERT_EQ(converted.me(), model->me());
    ASSERT_EQ(converted.mi(), model->mi());
    EXPECT_EQ(converted.lower(), model->lower());
    EXPECT_EQ(converted.upper(), model->upper());

    const Vec x = probe_point(*model);
    EXPECT_DOUBLE_EQ(converted.eval_f(x), model->eval_f(x));
    EXPECT_LT((converted.eval_grad(x) - model->eval_grad(x)).cwiseAbs().maxCoeff(), 1e-12);
    EXPECT_LT((converted.eval_ce(x) - model->eval_ce(x)).cwiseAbs().maxCoeff(), 1e-12);
    EXPECT_LT((converted.eval_ci(x) - model->eval_ci(x)).cwiseAbs().maxCoeff(), 1e-12);

    expect_sparse_near(converted.eval_jac_e(x), model->eval_jac_e(x), "equality Jacobian");
    expect_sparse_near(converted.eval_jac_i(x), model->eval_jac_i(x), "inequality Jacobian");

    // The Hessian is the one piece the adapter TRANSPOSES on the way out (the
    // model returns the upper triangle, NLPProblem declares the lower one), so
    // it is checked at multipliers that are neither zero nor all equal -- a
    // wrong head/tail cut of NLPProblem's single lambda block would survive
    // either of those.
    Vec lambda_e(model->me());
    for (Index i = 0; i < lambda_e.size(); ++i) {
        lambda_e(i) = 0.5 + 0.1 * static_cast<double>(i % 7);
    }
    Vec lambda_i(model->mi());
    for (Index i = 0; i < lambda_i.size(); ++i) {
        lambda_i(i) = 0.25 + 0.05 * static_cast<double>(i % 5);
    }
    expect_sparse_near(converted.eval_hess(x, 1.0, lambda_e, lambda_i),
                       model->eval_hess(x, 1.0, lambda_e, lambda_i), "Lagrangian Hessian");
    expect_sparse_near(converted.eval_hess(x, 0.0, lambda_e, lambda_i),
                       model->eval_hess(x, 0.0, lambda_e, lambda_i),
                       "constraint-only Hessian (obj_factor = 0)");
}

// --- The moving-pattern guard ---

namespace {

// A model whose Jacobian pattern DEPENDS ON THE ITERATE: the second column of
// row 0 exists only where x(0) is positive. NLPProblem's structures are queried
// once and must not change, so such a model cannot be stated as one -- and the
// adapter must say that by name rather than write the entry into whatever slot
// happens to be next.
class CrossoverMovingPatternModel final : public NlpModel {
  public:
    CrossoverMovingPatternModel() : lower_(Vec::Constant(2, -1.0)), upper_(Vec::Constant(2, 1.0)) {}

    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return x; }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, x(0) + x(1)); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }

    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 1.0;
        if (x(0) > 0.5) {
            j.insert(0, 1) = 1.0; // the entry that is not always there
        }
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    // Start point and projected origin both have x(0) = 0, so the declared
    // union genuinely lacks the entry the probe below produces.
    Vec start_point() const override { return Vec::Zero(2); }

  private:
    Vec lower_, upper_;
};

} // namespace

TEST(CrossoverAdapter, RefusesAModelWhosePatternMoves) {
    const auto model = std::make_shared<CrossoverMovingPatternModel>();
    ModelAsNlpProblem declared(model, "moving");

    Eigen::VectorXd vals(declared.num_jac_nonzeros());
    Eigen::VectorXd x(2);
    x << 0.9, 0.0; // past the threshold, so the extra entry appears

    try {
        declared.eval_jac(x, vals);
        FAIL() << "a Jacobian entry outside the declared structure must be refused";
    } catch (const std::runtime_error &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("pattern moved"), std::string::npos) << message;
        EXPECT_NE(message.find("Jacobian"), std::string::npos) << message;
    }

    // And the same model IS usable where its pattern holds -- the guard is a
    // guard, not a blanket refusal of the model.
    Eigen::VectorXd inside(2);
    inside << 0.1, 0.0;
    EXPECT_NO_THROW(declared.eval_jac(inside, vals));
}

// --- The premise of the dual-bind path ---

// ONE DECLARATION, TWO ENGINES, ONE KEY. This is the fact the whole W5 artifact
// rests on: the interior-point engine's own transcription of the declared
// problem and the SQP bridge over its conversion must produce the SAME
// DeclarationKey, so an exported value stages across with no re-stamp. If they
// ever part, legs (c) and (d) are refused at solve entry and every margin in the
// artifact silently becomes zero -- a failure that reads as "the crossover saves
// nothing" rather than as a defect, which is precisely why it is pinned here.
TEST(CrossoverLegs, BothEnginesKeyOneDeclarationTheSameWay) {
    const auto model = crossover_model();
    const auto declared = std::make_shared<ModelAsNlpProblem>(model, "crossover_gate");

    NLPSolver ipm(declared);
    ipm.optimizer_->set_print_level(10);
    ipm.transcribe();

    const auto converted = std::make_shared<NlpProblemModel>(declared);
    NlpModelAggregate bridge(converted);

    EXPECT_TRUE(declaration_key(ipm.nlp_->declaration()) == declaration_key(bridge.declaration()))
        << "one declared problem must key the same on both engines -- the dual-bind path is "
           "exactly this fact, and the W5 artifact is meaningless without it";
}

// THE COMPOSITION, on a chain small enough for a test: an interior-point solve
// of the declared problem, its export, and both warm legs staging it. This is
// run_cell_legs' own sequence at a size ctest can afford, so the sequence the
// artifact was produced by is exercised rather than merely described.
TEST(CrossoverLegs, TheExportStagesIntoBothWarmLegs) {
    const auto model = crossover_model();
    const auto declared = std::make_shared<ModelAsNlpProblem>(model, "crossover_gate");
    const auto converted = std::make_shared<NlpProblemModel>(declared);
    const Vec x0 = model->start_point();

    NLPSolver ipm(declared);
    ipm.optimizer_->set_print_level(10);
    ipm.transcribe();
    ASSERT_EQ(ipm.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    const WarmStartData exported = ipm.optimizer_->export_warm_start();

    ASSERT_EQ(exported.primal_.size(), model->n());
    ASSERT_EQ(exported.eq_lmults_.size(), model->me());
    ASSERT_EQ(exported.iq_lmults_.size(), model->mi());
    // F7 declares a control box, so the hand-off carries the polish extension
    // beside its core -- which is what makes legs (c) and (d) different runs.
    EXPECT_NE(hven::solvers::find_ipm_polish(exported), nullptr);

    SqpOptions opts;
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    opts.adaptive_mu = false;

    const auto solve_with = [&](const WarmStartData *staged) {
        NlpModelAggregate bridge(converted);
        SqpDriver driver{opts};
        if (staged != nullptr) {
            driver.stage_warm_start(*staged);
        }
        return driver.solve(bridge, x0);
    };

    const SqpSolution cold = solve_with(nullptr);
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);
    EXPECT_EQ(cold.counters.start_level_used, StartLevel::kCold);

    WarmStartData core = exported;
    core.extensions_.clear();
    const SqpSolution warm_core = solve_with(&core);
    const SqpSolution warm_polish = solve_with(&exported);

    // BOTH warm routes were actually ingested. A staged value that was refused
    // or ignored would come back kCold, and the margins below would then be
    // measuring cold against cold.
    EXPECT_EQ(warm_core.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(warm_polish.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(warm_core.status, SqpStatus::kOptimal);
    EXPECT_EQ(warm_polish.status, SqpStatus::kOptimal);

    // MARGIN FORM (CLAUDE.md §7): the hand-off is judged by the work it saves.
    // Neither warm leg may cost MORE majors than cold on this problem, and the
    // polish route -- which infers the active set from the (z_lower, z_upper)
    // pair rather than starting from an all-free working set -- must not cost
    // more than the core-only one.
    EXPECT_LE(warm_core.counters.major_iters, cold.counters.major_iters);
    EXPECT_LE(warm_polish.counters.major_iters, cold.counters.major_iters);
    EXPECT_LE(warm_polish.counters.major_iters, warm_core.counters.major_iters);

    // All three legs answer the same question. A warm leg that certified a
    // DIFFERENT point would make its margin meaningless.
    EXPECT_NEAR(warm_core.f, cold.f, 1e-6 * std::max(1.0, std::abs(cold.f)));
    EXPECT_NEAR(warm_polish.f, cold.f, 1e-6 * std::max(1.0, std::abs(cold.f)));
}

// --- The partition the artifact documents ---

// The artifact lists 24 measured cells and 33 refused ones, each refusal with a
// reason. That count is quoted in the README and in crossover_legs.h's banner,
// so it is pinned rather than left to drift with a future cell's arrival: a
// census change that moves it should move the prose too, and this failure is
// what says so.
TEST(CrossoverLegs, TheDualBindPartitionIsTheOneTheArtifactDocuments) {
    int measured = 0;
    int refused = 0;
    for (const CorpusCell &cell : hven::solvers::corpus::all_cells()) {
        const std::string reason = hven::solvers::crossover::dual_bind_refusal(cell);
        if (reason.empty()) {
            ++measured;
            EXPECT_TRUE(cell.start == StartTaxonomy::kNeutralCold ||
                        cell.start == StartTaxonomy::kPhysicsInformed)
                << cell.id << ": only the two bare-primal taxonomies dual-bind";
        } else {
            ++refused;
            // NEVER SILENTLY DROPPED: every refusal carries a reason a reader
            // of the artifact can act on.
            EXPECT_FALSE(reason.empty()) << cell.id;
            EXPECT_GT(reason.size(), 40u) << cell.id << ": the reason must be a reason";
        }
    }
    EXPECT_EQ(measured, 24);
    EXPECT_EQ(refused, 33);
    EXPECT_EQ(hven::solvers::crossover::dual_bindable_cells().size(), 24u);
}

// A cell that does not dual-bind is refused BY NAME at the leg runner, not
// quietly run from some substitute start.
TEST(CrossoverLegs, RunningANonDualBindingCellIsRefused) {
    const CorpusCell *cell = hven::solvers::corpus::find_cell("f7_n1000_bound_warm");
    ASSERT_NE(cell, nullptr);
    ASSERT_FALSE(hven::solvers::crossover::cell_dual_binds(*cell));
    try {
        (void)hven::solvers::crossover::run_cell_legs(*cell);
        FAIL() << "a cell whose start has no interior-point counterpart must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("f7_n1000_bound_warm"), std::string::npos) << message;
        EXPECT_NE(message.find("does not dual-bind"), std::string::npos) << message;
    }
}

// --- The margin arithmetic ---

// --- The aggregate row of a cell that was killed part way through ---

// THE DEFECT THIS PINS, stated as the failure it was. The first W5 sweep wrote
// the aggregate row only after all four legs finished; when a cell was killed at
// its wall deadline the runner instead stamped ONE cell-level `dnf_budget`
// across all four status columns -- including the legs that HAD finished and
// whose rows the same loop had just written to the other four files. Twelve
// status cells in the shipped `margins.csv` then contradicted the artifact's own
// per-leg CSVs, and on exactly the cells the headline finding rests on: a reader
// of the aggregate alone would have concluded the polish route FAILED on every
// one of them, which is that finding read backwards.
//
// The invariant is one sentence: a status column reports ITS OWN LEG. A leg that
// ran reports its outcome; a leg that did not is `absent`. The margin columns go
// absent on their own separate condition -- a margin needs both its legs -- which
// is what makes "absent margin" and "failed leg" different statements.
TEST(CrossoverLegs, AKilledCellsAggregateRowReportsEachLegsOwnOutcome) {
    const CorpusCell *cell = hven::solvers::corpus::find_cell("f7_n5000_path_neutral");
    ASSERT_NE(cell, nullptr);

    // The exact shape the deadline produced on that cell: the interior-point leg
    // converged, both warm legs solved, and the COLD leg -- which runs last, and
    // is the expensive one -- never finished.
    hven::solvers::crossover::CellLegs legs;
    legs.cell = cell;
    legs.n = 25000;
    legs.me = 15000;
    legs.mi = 5000;

    legs.a.ran = true;
    legs.a.flag = hven::ConvergenceFlags::CONVERGED;
    legs.a.iters = 17;
    legs.a.export_has_polish = true;
    legs.legs_cd_identical = false;

    legs.c.ran = true;
    legs.c.status = SqpStatus::kOptimal;
    legs.c.major_iters = 1;
    legs.c.qp_minor_iters = 4435;
    legs.c.factorizations = 35;

    legs.d.ran = true;
    legs.d.status = SqpStatus::kOptimal;
    legs.d.major_iters = 1;
    legs.d.qp_minor_iters = 2;
    legs.d.factorizations = 1;

    // legs.b is left un-run: ran == false, and its `status` still holds the
    // default kNumericalError that printing it unguarded would wrongly report.

    const std::string row = hven::solvers::crossover::margins_row(legs);

    // The legs that RAN say what they did.
    EXPECT_NE(row.find("CONVERGED"), std::string::npos)
        << "the interior-point leg converged and the aggregate must say so: " << row;
    EXPECT_NE(row.find("Optimal"), std::string::npos)
        << "both warm legs solved and the aggregate must say so: " << row;

    // The leg that did NOT run is absent -- never a status, and never the
    // default-constructed one in particular.
    EXPECT_EQ(row.find("NumericalError"), std::string::npos)
        << "a leg that never ran must not report the default status: " << row;

    // Field by field, because "contains Optimal" would also pass on a row that
    // put it in the wrong column. Schema: cell_id,n_nodes,window,taxonomy,n,
    // cold_status,cold_majors,cold_qp_minors,cold_factorizations,
    // warm_core_status,warm_polish_status,<3 core margins>,<3 polish margins>,
    // ipm_flag,export_has_polish,legs_cd_identical.
    std::vector<std::string> f;
    for (std::size_t start = 0;;) {
        const std::size_t comma = row.find(',', start);
        if (comma == std::string::npos) {
            std::string last = row.substr(start);
            while (!last.empty() && (last.back() == '\n' || last.back() == '\r')) {
                last.pop_back();
            }
            f.push_back(last);
            break;
        }
        f.push_back(row.substr(start, comma - start));
        start = comma + 1;
    }
    ASSERT_EQ(f.size(), 20u) << row;
    EXPECT_EQ(f[0], "f7_n5000_path_neutral");
    EXPECT_EQ(f[5], "absent") << "cold_status: that leg never ran";
    EXPECT_EQ(f[6], "absent") << "cold_majors";
    EXPECT_EQ(f[7], "absent") << "cold_qp_minors";
    EXPECT_EQ(f[8], "absent") << "cold_factorizations";
    EXPECT_EQ(f[9], "Optimal") << "warm_core_status: that leg DID run";
    EXPECT_EQ(f[10], "Optimal") << "warm_polish_status: that leg DID run";
    // Both margins are undefined -- not because the warm legs failed, but
    // because the baseline they subtract from is missing. Different reason,
    // same token, and that is the distinction the columns have to keep.
    for (std::size_t i = 11; i <= 16; ++i) {
        EXPECT_EQ(f[i], "absent") << "margin column " << i << " needs a cold baseline";
    }
    EXPECT_EQ(f[17], "CONVERGED") << "ipm_flag: that leg DID run";
    EXPECT_EQ(f[18], "1") << "export_has_polish";
}

// A cell where NOTHING ran carries no outcome to report anywhere, and says so in
// every column rather than inventing one.
TEST(CrossoverLegs, ACellWithNoLegAtAllIsAbsentInEveryColumn) {
    const CorpusCell *cell = hven::solvers::corpus::find_cell("f7_n20000_path_neutral");
    ASSERT_NE(cell, nullptr);
    hven::solvers::crossover::CellLegs legs;
    legs.cell = cell;

    const std::string row = hven::solvers::crossover::margins_row(legs);
    EXPECT_EQ(row.find("NumericalError"), std::string::npos) << row;
    EXPECT_EQ(row.find("CONVERGED"), std::string::npos) << row;
    EXPECT_EQ(row.find("Optimal"), std::string::npos) << row;
    EXPECT_NE(row.find("absent"), std::string::npos) << row;
}

// POSITIVE MEANS SAVED, and an undefined margin is ABSENT rather than zero. A
// zero would read as "the crossover saved nothing on this cell", which is a
// measurement; absence is the truth when a leg never reached a counter.
TEST(CrossoverLegs, MarginsAreColdMinusWarmAndAbsentWhenUndefined) {
    hven::solvers::crossover::SqpLegRow cold;
    cold.major_iters = 9;
    cold.qp_minor_iters = 400;
    cold.factorizations = 11;
    hven::solvers::crossover::SqpLegRow warm;
    warm.major_iters = 2;
    warm.qp_minor_iters = 55;
    warm.factorizations = 3;

    const auto m = margin_against_cold(cold, warm);
    ASSERT_TRUE(m.defined);
    EXPECT_EQ(m.majors, 7);
    EXPECT_EQ(m.qp_minors, 345);
    EXPECT_EQ(m.factorizations, 8);

    // A leg that never ran carries -1, the project's ABSENT convention.
    const hven::solvers::crossover::SqpLegRow never;
    EXPECT_FALSE(margin_against_cold(cold, never).defined);
    EXPECT_FALSE(margin_against_cold(never, warm).defined);
}
