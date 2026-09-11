// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_crossover_legs.cpp — the correctness gate for
// bench/crossover_legs.h, the M5 W5 measurement instrument.
//
// The bench binary that produces the W5 artifact
// (docs/notes/data/2026-08-m5-w5-crossover/) is not ctest-registered: a full
// sweep of the dual-bindable census runs for hours. This file gates it under
// the standing rule that a measurement binary's gate SHARES ITS
// IMPLEMENTATION -- crossover_legs.h is quote-included here, never copied.
//
// Gated: that ModelAsNlpProblem states the same problem it wraps (a dropped
// Jacobian block or a reversed row-kind mapping would still yield plausible
// counters, of a different problem); that both engines key one declaration the
// same way, which is the premise of the dual-bind path; that the
// moving-pattern guard fires; and that the dual-bind partition is the one the
// artifact documents.
//
// Names carry a `Crossover` prefix: this suite's TUs share a link unit.

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/model/nlp_adapter.h>
#include <hven/drivers/interior_point_solver.h>
#include <hven/model/nlp_model_aggregate.h>
#include <hven/model/nlp_problem_model.h>
#include <hven/model/non_linear_program.h>
#include <hven/model/structure_identity.h>
#include <hven/warmstart/ipm_polish_extension.h>
#include <hven/warmstart/warm_start_data.h>

#include "../../bench/crossover_legs.h"

#include "hven/core/compiler.h"

// by-value oracle of the in-place hot path; migration is a separate task
HVEN_SUPPRESS_DEPRECATED_BEGIN

namespace {

using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::declaration_key;
using hven::solvers::NlpModel;
using hven::solvers::NlpModelAggregate;
using hven::solvers::NlpProblemModel;
using hven::solvers::SolveStatus;
using hven::solvers::SqpDriver;
using hven::solvers::SqpOptions;
using hven::solvers::SqpSolution;
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

// The round trip, through the path the SQP leg actually takes: declared as an
// NLPProblem, read straight back as a native model. A wrong sign in the
// row-kind mapping shows up here as a cI block off by more than a tolerance.
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

    // The Hessian is the one piece the adapter transposes on the way out (the
    // model returns the upper triangle, NLPProblem the lower), so it is checked
    // at multipliers that are neither zero nor all equal -- a wrong head/tail
    // cut of NLPProblem's single lambda block would survive either.
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

// A model whose Jacobian pattern depends on the iterate: the second column of
// row 0 exists only where x(0) is positive. NLPProblem's structures are queried
// once, so such a model cannot be stated as one and the adapter must say so by
// name rather than write the entry into whatever slot comes next.
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

// One declaration, two engines, one key: the interior-point transcription and
// the SQP bridge over its conversion must produce the same DeclarationKey, so
// an exported value stages across with no re-stamp. If they part, legs (c) and
// (d) are refused at solve entry and every margin silently reads as zero saved
// rather than as a defect.
TEST(CrossoverLegs, BothEnginesKeyOneDeclarationTheSameWay) {
    const auto model = crossover_model();
    const auto declared = std::make_shared<ModelAsNlpProblem>(model, "crossover_gate");

    const auto ipm_program = hven::solvers::make_nlp_program(declared);

    const auto converted = std::make_shared<NlpProblemModel>(declared);
    NlpModelAggregate bridge(converted);

    EXPECT_TRUE(declaration_key(ipm_program->declaration()) ==
                declaration_key(bridge.declaration()))
        << "one declared problem must key the same on both engines -- the dual-bind path is "
           "exactly this fact, and the W5 artifact is meaningless without it";
}

// The composition, on a chain small enough for a test: an interior-point solve
// of the declared problem, its export, and both warm legs staging it -- that is
// run_cell_legs' own sequence at a size ctest can afford.
TEST(CrossoverLegs, TheExportStagesIntoBothWarmLegs) {
    const auto model = crossover_model();
    const auto declared = std::make_shared<ModelAsNlpProblem>(model, "crossover_gate");
    const auto converted = std::make_shared<NlpProblemModel>(declared);
    const Vec x0 = model->start_point();

    const auto ipm_program = hven::solvers::make_nlp_program(declared);
    hven::solvers::InteriorPointSolver ipm;
    {
        auto o = ipm.options();
        o.common.print_level = 10;
        ipm.set_options(std::move(o));
    }
    const hven::solvers::IpmResult ipm_result = ipm.solve(*ipm_program, x0);
    ASSERT_EQ(ipm_result.status, hven::solvers::SolveStatus::kOptimal);
    // THE EXPORT, OFF THE RESULT (M6 W5 T8.5).
    const auto snapshot = ipm_result.export_warm_start();
    ASSERT_TRUE(snapshot.has_value());
    const WarmStartData exported = *snapshot;

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

    // THE ARGUMENT FORM (M6 W5 T8.5): `stage(p); solve(b, x0)` is
    // `solve(b, x0, p)`, and the cold arm is simply the overload without one.
    const auto solve_with = [&](const WarmStartData *payload) {
        NlpModelAggregate bridge(converted);
        SqpDriver driver{opts};
        return payload != nullptr ? driver.solve(bridge, x0, *payload) : driver.solve(bridge, x0);
    };

    const SqpSolution cold = solve_with(nullptr);
    ASSERT_EQ(cold.status, SolveStatus::kOptimal);
    EXPECT_EQ(cold.counters.start_level_used, StartLevel::kCold);

    WarmStartData core = exported;
    core.extensions_.clear();
    const SqpSolution warm_core = solve_with(&core);
    const SqpSolution warm_polish = solve_with(&exported);

    // Both warm routes were actually ingested: a payload that was refused or
    // ignored comes back kCold, leaving the margins below cold against cold.
    EXPECT_EQ(warm_core.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(warm_polish.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(warm_core.status, SolveStatus::kOptimal);
    EXPECT_EQ(warm_polish.status, SolveStatus::kOptimal);

    // Margin form: the hand-off is judged by the work it saves. Neither warm leg
    // may cost more majors than cold here, and the polish route -- which infers
    // the active set from the (z_lower, z_upper) pair rather than starting from
    // an all-free working set -- must not cost more than the core-only one.
    EXPECT_LE(warm_core.counters.major_iters, cold.counters.major_iters);
    EXPECT_LE(warm_polish.counters.major_iters, cold.counters.major_iters);
    EXPECT_LE(warm_polish.counters.major_iters, warm_core.counters.major_iters);

    // All three legs answer the same question: a warm leg that certified a
    // different point would make its margin meaningless.
    EXPECT_NEAR(warm_core.f, cold.f, 1e-6 * std::max(1.0, std::abs(cold.f)));
    EXPECT_NEAR(warm_polish.f, cold.f, 1e-6 * std::max(1.0, std::abs(cold.f)));
}

// --- The partition the artifact documents ---

// 24 measured cells, 33 refused, each refusal with a reason. That count is
// quoted in the artifact README and in crossover_legs.h's banner, so it is
// pinned here: a census change that moves it must move the prose too.
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
            // Every refusal carries a reason a reader of the artifact can act
            // on.
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

// A status column reports its own leg: a leg that ran reports its outcome, a
// leg that did not is `absent`. The margin columns go absent on their own
// separate condition -- a margin needs both its legs -- which is what makes
// "absent margin" and "failed leg" different statements.
//
// Pinned because the first W5 sweep stamped one cell-level `dnf_budget` across
// all four status columns when a cell was killed at its wall deadline,
// contradicting the artifact's own per-leg CSVs on exactly the cells the
// headline finding rests on.
TEST(CrossoverLegs, AKilledCellsAggregateRowReportsEachLegsOwnOutcome) {
    const CorpusCell *cell = hven::solvers::corpus::find_cell("f7_n5000_path_neutral");
    ASSERT_NE(cell, nullptr);

    // The exact shape the deadline produced on that cell: the interior-point leg
    // converged, both warm legs solved, and the cold leg -- last, and the
    // expensive one -- never finished.
    hven::solvers::crossover::CellLegs legs;
    legs.cell = cell;
    legs.n = 25000;
    legs.me = 15000;
    legs.mi = 5000;

    legs.a.ran = true;
    legs.a.flag = hven::solvers::SolveStatus::kOptimal;
    legs.a.iters = 17;
    legs.a.export_has_polish = true;
    legs.legs_cd_identical = false;

    legs.c.ran = true;
    legs.c.status = SolveStatus::kOptimal;
    legs.c.major_iters = 1;
    legs.c.qp_minor_iters = 4435;
    legs.c.factorizations = 35;

    legs.d.ran = true;
    legs.d.status = SolveStatus::kOptimal;
    legs.d.major_iters = 1;
    legs.d.qp_minor_iters = 2;
    legs.d.factorizations = 1;

    // legs.b is left un-run: ran == false, and its `status` still holds the
    // default kNumericalError that printing it unguarded would wrongly report.

    const std::string row = hven::solvers::crossover::margins_row(legs);

    // The legs that RAN say what they did. The interior-point flag's spelling
    // moved in M6 W5 T8.4 (`CONVERGED` -> `optimal`), declared: with
    // ConvergenceFlags gone the leg reports SolveStatus's display names.
    EXPECT_NE(row.find("optimal"), std::string::npos)
        << "the interior-point leg converged and the aggregate must say so: " << row;
    EXPECT_NE(row.find("Optimal"), std::string::npos)
        << "both warm legs solved and the aggregate must say so: " << row;

    // The leg that did NOT run is absent -- never a status, and never the
    // default-constructed one in particular.
    EXPECT_EQ(row.find("NumericalError"), std::string::npos)
        << "a leg that never ran must not report the default status: " << row;
    EXPECT_EQ(row.find("numerical_error"), std::string::npos)
        << "nor under the interior-point column's new spelling: " << row;

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
    EXPECT_EQ(f[17], "optimal") << "ipm_flag: that leg DID run";
    EXPECT_EQ(f[18], "1") << "export_has_polish";
}

// A cell where nothing ran carries no outcome anywhere, and says so in every
// column rather than inventing one.
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

// Positive means saved, and an undefined margin is absent rather than zero: a
// zero would read as "the crossover saved nothing on this cell", which is a
// measurement, where absence is the truth when a leg never reached a counter.
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

HVEN_SUPPRESS_DEPRECATED_END

// ===========================================================================
// M6 W5 T8.9 -- PARTITIONS THROUGH THE ADAPTER, ON A FIXTURE BIG ENOUGH TO
// ADOPT TWO OF THEM
// ===========================================================================
//
// `make_nlp_program(problem, N)` lays N partitions, and the count is CLAMPED at
// `num_user_kkt_elems_ / kMinKktElementsPerPartition` (1000) rather than
// refused -- so a small problem silently adopts fewer, and HS071 (about a dozen
// elements) adopts 1 whatever is asked. This is the one fixture in the tree big
// enough for N = 2 to be ADOPTED, which is why the pin lives here beside the
// F7 chain rather than in the interior suite.
//
// WHAT N MEANS HERE, AND WHAT IT DOES NOT. All three adapter pieces are
// ThreadingFlags::MainThread (the shared stateful NLPAdapterCore), and
// analyze_partitioning forces every MainThread function into the LAST
// partition, run inline on the calling thread. So two partitions over an
// NLPProblem is ONE empty partition plus the whole problem evaluated serially:
// LAYOUT, not parallel evaluation. Genuine partitioned evaluation over an
// NLPProblem needs per-partition cores and is registered for the M7
// ClaimStreamSource widening.
//
// The pin is therefore: the ADOPTED count is what was asked for, and the solve
// is the SAME SOLVE -- same status, same iteration count, objective to 1e-12
// relative -- with bitwise identity REPORTED rather than assumed.
TEST(CrossoverAdapter, TwoLaidPartitionsSolveTheSameProblemAsOne) {
    // Big enough to clear 2 * kMinKktElementsPerPartition; small enough for
    // ctest. The premise below refuses the test rather than passing vacuously
    // if that ever stops being true.
    const auto model = std::make_shared<F7CollocationChain>(/*nodes=*/100, /*states=*/3,
                                                            /*controls=*/2, kCrossoverP,
                                                            /*radius=*/1.0);
    const auto declared = std::make_shared<ModelAsNlpProblem>(model, "partition_gate");
    const Vec x0 = model->start_point();

    const auto one = hven::solvers::make_nlp_program(declared, 1);
    const auto two = hven::solvers::make_nlp_program(declared, 2);
    fmt::print(stderr, "T89-PARTS user_kkt_elems={} adopted(1)={} adopted(2)={} pool={}\n",
               two->num_user_kkt_elems_, one->num_partitions_, two->num_partitions_,
               hven::utils::get_num_threads());
    ASSERT_GE(two->num_user_kkt_elems_, 2 * hven::solvers::kMinKktElementsPerPartition)
        << "fixture premise: the clamp would otherwise make the two-partition arm a "
           "one-partition arm under a two-partition name";
    EXPECT_EQ(one->num_partitions_, 1);
    EXPECT_EQ(two->num_partitions_, 2) << "the ADOPTED count, read off the program";
    EXPECT_EQ(two->declaration().partition_count_, two->num_partitions_);

    const auto solve_it = [&](hven::solvers::NonLinearProgram &program) {
        hven::solvers::InteriorPointSolver ipm;
        auto o = ipm.options();
        o.common.print_level = 10;
        o.common.threads = 1;
        ipm.set_options(std::move(o));
        return ipm.solve(program, x0);
    };
    const hven::solvers::IpmResult r1 = solve_it(*one);
    const hven::solvers::IpmResult r2 = solve_it(*two);

    ASSERT_EQ(r1.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(r2.status, r1.status);
    EXPECT_EQ(r2.iterations, r1.iterations);
    EXPECT_NEAR(r2.f, r1.f, 1e-12 * std::max(1.0, std::abs(r1.f)));

    // BITWISE IDENTITY, REPORTED. The evaluation is serial at either count and
    // nothing reorders, so identity is the expectation; a failure here would be
    // a finding about the partitioned assembly's slot order (and therefore
    // about the order the backend is handed its input), not a reason to have
    // declined the comparison. The tolerance pin above still holds either way.
    const bool bitwise = r2.f == r1.f && r2.kkt_inf == r1.kkt_inf && r2.econ_inf == r1.econ_inf &&
                         r2.icon_inf == r1.icon_inf && r2.barr_inf == r1.barr_inf;
    fmt::print(stderr, "T89-PARTS bitwise_identity={}\n", bitwise ? "YES" : "NO");
    EXPECT_TRUE(bitwise) << "reported, not merely asserted: kkt " << r1.kkt_inf << " vs "
                         << r2.kkt_inf << ", f " << r1.f << " vs " << r2.f;
}
