// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// bench/crossover_legs.h — the IPM -> SQP crossover measurement legs, and the
// dual-bind path that makes a replay-corpus cell reachable from BOTH engines
// under ONE declaration. Protocol: docs/notes/2026-08-m5-ledger.md, "W5 LEG
// PROTOCOL". Four legs per dual-bindable cell:
//
//   (a) IPM-only        the interior-point baseline, and the exporter: its
//                       export_warm_start() is what legs (c) and (d) stage.
//   (b) SQP cold        the same declaration, same x0, nothing staged.
//   (c) SQP warm core   (a)'s export with extensions_ cleared -- the neutral
//                       core alone, which is what R3 promises any engine can
//                       read.
//   (d) SQP warm polish (a)'s export exactly as the other engine handed it
//                       over, polish extension included.
//
// ASSERTED CURRENCY (CLAUDE.md §7): counters. Majors, QP minors and
// factorizations for legs b/c/d, with the c-vs-b and d-vs-b margins recorded
// per cell. Wall is recorded and is INFORMATIONAL ONLY -- never a margin,
// never a claim.
//
// Both engines reach the cell through the ONE declared NLPProblem -- the
// interior-point engine via NLPSolver's own transcription, the SQP engine via
// NlpProblemModel -- so both key the same DeclarationKey and an export stages
// across with no conversion and no re-stamp. The corpus's cells are NlpModels
// (F7CollocationChain), so the declaration is ModelAsNlpProblem below; neither
// engine sees the F7 model directly.
//
// Reading constraint: leg (b) is NOT the committed walk-corpus baseline row for
// the same cell. It is a cold solve of the same mathematics reached through the
// dual-bind conversion, so its counters may differ from
// bench/baselines/*/walk_baseline.csv. b, c and d are strictly comparable to
// one another -- one declaration, one conversion, one start point, one options
// object -- which is what the margins are taken over.
//
// Every cell's PROBLEM dual-binds; what does not always dual-bind is its START.
// The measured cells are the kNeutralCold and kPhysicsInformed ones, which
// declare a bare primal x0 that both engines take. dual_bind_refusal() below
// states, per cell, why the rest are refused; every refusal is LISTED in the
// artifact, so nothing is silently dropped.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/core/start_level.h>
#include <hven/drivers/interior_point_solver.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>
#include <hven/model/nlp_model_aggregate.h>
#include <hven/model/nlp_problem.h>
#include <hven/model/nlp_problem_model.h>
#include <hven/model/nlp_solver.h>
#include <hven/model/structure_identity.h>
#include <hven/warmstart/ipm_polish_extension.h>
#include <hven/warmstart/warm_start_data.h>

#include "corpus_cells.h"

namespace hven::solvers::crossover {

using hven::Index;
using hven::Vec;
using hven::solvers::corpus::CorpusCell;
using hven::solvers::corpus::StartTaxonomy;

// =============================================================================
// ModelAsNlpProblem — an NlpModel, stated the way NLPProblem states a problem
// =============================================================================

/// @brief One NlpModel declared as an NLPProblem, so that both engines can be
///        bound to it.
///
/// ROW LAYOUT. The declared rows are the equalities first, in the model's own
/// cE order, then the inequalities in its own cI order:
///
///   row i          in [0, me)        gl = gu = 0        -> cE_i(x) = 0
///   row me + j     in [0, mi)        gl = -inf, gu = 0  -> cI_j(x) <= 0
///
/// Reading the declaration back through NlpProblemModel reproduces the ORIGINAL
/// model's cE and cI, in the original order, with no sign flip, so NLPProblem's
/// lambda over [cE; cI] splits into the model's (lambda_e, lambda_i) by a
/// head/tail cut and nothing else.
///
/// STRUCTURE. NLPProblem queries the two sparsity patterns once and they must
/// not move afterwards, but an NlpModel decides its pattern per point. The
/// declared structure is therefore the UNION of the patterns at two points: the
/// model's own start point, and the point NLPSolver's transcription evaluates
/// at (the origin projected onto the declared box). Every later evaluation is
/// merged into the declared slots, and a nonzero arriving at a slot the union
/// did not declare is REFUSED by name: a model whose pattern depends on the
/// iterate cannot be stated as an NLPProblem.
///
/// HESSIAN TRIANGLE. NlpModel returns the UPPER triangle (row <= col);
/// NLPProblem declares the LOWER one (row >= col). Same symmetric matrix, so
/// the conversion is an index transpose on the declared structure and nothing
/// at all on the values.
class ModelAsNlpProblem final : public NLPProblem {
  public:
    /// @brief Declares @p model as an NLPProblem.
    /// @param model The model to state; retained.
    /// @param name  Diagnostic name, reported by name().
    /// @throws std::invalid_argument if @p model is null.
    ModelAsNlpProblem(std::shared_ptr<const NlpModel> model, std::string name)
        : model_(std::move(model)), name_(std::move(name)) {
        if (model_ == nullptr) {
            throw std::invalid_argument("ModelAsNlpProblem: model must not be null");
        }
        n_ = model_->n();
        me_ = model_->me();
        mi_ = model_->mi();
        build_structures();
    }

    int num_vars() const override { return static_cast<int>(n_); }
    int num_cons() const override { return static_cast<int>(me_ + mi_); }
    int num_jac_nonzeros() const override { return static_cast<int>(jac_pattern_.nonZeros()); }
    int num_hess_nonzeros() const override { return static_cast<int>(hess_pattern_.nonZeros()); }

    void bounds(Eigen::Ref<Eigen::VectorXd> x_lower, Eigen::Ref<Eigen::VectorXd> x_upper,
                Eigen::Ref<Eigen::VectorXd> g_lower,
                Eigen::Ref<Eigen::VectorXd> g_upper) const override {
        x_lower = model_->lower();
        x_upper = model_->upper();
        // Equalities at zero; inequalities upper-bounded at zero, free below.
        g_lower.head(me_).setZero();
        g_upper.head(me_).setZero();
        g_lower.tail(mi_).setConstant(-std::numeric_limits<double>::infinity());
        g_upper.tail(mi_).setZero();
    }

    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = model_->eval_f(x);
    }

    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> grad) const override {
        grad = model_->eval_grad(x);
    }

    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        if (me_ > 0) {
            g.head(me_) = model_->eval_ce(x);
        }
        if (mi_ > 0) {
            g.tail(mi_) = model_->eval_ci(x);
        }
    }

    void jac_structure(Eigen::Ref<Eigen::VectorXi> rows,
                       Eigen::Ref<Eigen::VectorXi> cols) const override {
        rows = jac_rows_;
        cols = jac_cols_;
    }

    void hess_structure(Eigen::Ref<Eigen::VectorXi> rows,
                        Eigen::Ref<Eigen::VectorXi> cols) const override {
        rows = hess_rows_;
        cols = hess_cols_;
    }

    void eval_jac(ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> vals) const override {
        const SpRM stacked = stacked_jacobian(x);
        merge_into_slots(jac_pattern_, stacked, "Jacobian", vals);
    }

    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> vals) const override {
        // g = [cE; cI], so lambda splits into the model's pair by a head/tail cut.
        const Vec lambda_e = me_ > 0 ? Vec(lambda.head(me_)) : Vec(0);
        const Vec lambda_i = mi_ > 0 ? Vec(lambda.tail(mi_)) : Vec(0);
        const SpRM upper = model_->eval_hess(Vec(x), obj_factor, lambda_e, lambda_i);
        merge_into_slots(hess_pattern_, upper, "Hessian", vals);
    }

    std::string name() const override { return name_; }

    /// @brief The model this was built over, for callers that need its own
    ///        surface (start point, box) beside the declaration.
    const NlpModel &model() const { return *model_; }

  private:
    using SpRM = Eigen::SparseMatrix<double, Eigen::RowMajor>;

    // The point NLPSolver's transcription evaluates at: the origin projected
    // onto the declared box. The pattern union has to cover it -- that call
    // happens before any solve iterate exists, and its pattern is what the
    // solver keeps.
    Vec projected_origin() const {
        const Vec &lower = model_->lower();
        const Vec &upper = model_->upper();
        Vec x = Vec::Zero(n_);
        for (Index i = 0; i < n_; ++i) {
            x(i) = std::min(std::max(0.0, lower(i)), upper(i));
        }
        return x;
    }

    SpRM stacked_jacobian(const Vec &x) const {
        const SpRM je = me_ > 0 ? model_->eval_jac_e(x) : SpRM(0, n_);
        const SpRM ji = mi_ > 0 ? model_->eval_jac_i(x) : SpRM(0, n_);
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(static_cast<std::size_t>(je.nonZeros() + ji.nonZeros()));
        for (Index r = 0; r < je.outerSize(); ++r) {
            for (SpRM::InnerIterator it(je, r); it; ++it) {
                t.emplace_back(static_cast<int>(it.row()), static_cast<int>(it.col()), it.value());
            }
        }
        for (Index r = 0; r < ji.outerSize(); ++r) {
            for (SpRM::InnerIterator it(ji, r); it; ++it) {
                t.emplace_back(static_cast<int>(it.row() + me_), static_cast<int>(it.col()),
                               it.value());
            }
        }
        SpRM out(me_ + mi_, n_);
        out.setFromTriplets(t.begin(), t.end());
        out.makeCompressed();
        return out;
    }

    // The union of two patterns, as a compressed matrix whose stored order IS
    // the declared slot order.
    static SpRM pattern_union(const SpRM &a, const SpRM &b) {
        SpRM u = a;
        u += b; // Eigen's sparse sum is a pattern union; the values are junk.
        u.makeCompressed();
        return u;
    }

    // Walks `pattern` and `values` in tandem -- both row-major, both
    // compressed, both with sorted inner indices -- and writes each declared
    // slot's value in declared order. A stored entry of `values` that the
    // pattern never declared is a MOVING PATTERN and is refused by name.
    static void merge_into_slots(const SpRM &pattern, const SpRM &values, const char *what,
                                 Eigen::Ref<Eigen::VectorXd> out) {
        // The tandem walk below reads outerIndexPtr/innerIndexPtr/valuePtr
        // directly, so compressed storage is a precondition. The engine's own
        // route enforces NlpModel's contract at nlp_require_claimed_pattern;
        // this adapter does not pass through there, so it checks by name rather
        // than reading an uncompressed return as silent garbage.
        if (!values.isCompressed()) {
            throw std::runtime_error(fmt::format(
                "ModelAsNlpProblem: the model returned an UNCOMPRESSED {}. NlpModel requires "
                "every matrix return to be compressed with sorted, duplicate-free inner "
                "indices (see model/nlp_model.h); pairing a stored value with a pattern slot "
                "reads a different element in uncompressed storage. Call makeCompressed(), or "
                "build the return with setFromTriplets(), which leaves it compressed.",
                what));
        }
        out.setZero();
        const int *p_outer = pattern.outerIndexPtr();
        const int *p_inner = pattern.innerIndexPtr();
        const int *v_outer = values.outerIndexPtr();
        const int *v_inner = values.innerIndexPtr();
        const double *v_val = values.valuePtr();
        for (Index r = 0; r < pattern.outerSize(); ++r) {
            int pk = p_outer[r];
            const int p_end = p_outer[r + 1];
            int vk = v_outer[r];
            const int v_end = v_outer[r + 1];
            while (vk < v_end) {
                while (pk < p_end && p_inner[pk] < v_inner[vk]) {
                    ++pk;
                }
                if (pk >= p_end || p_inner[pk] != v_inner[vk]) {
                    throw std::runtime_error(fmt::format(
                        "ModelAsNlpProblem: the model's {} pattern moved -- an entry at "
                        "(row {}, col {}) is not in the structure declared at setup. The "
                        "declared structure is the union of the patterns at the model's start "
                        "point and at the projected origin; a model whose pattern depends on "
                        "the iterate cannot be stated as an NLPProblem, whose structures are "
                        "queried once and must not change.",
                        what, r, v_inner[vk]));
                }
                out(pk) = v_val[vk];
                ++pk;
                ++vk;
            }
        }
    }

    void build_structures() {
        const Vec x_start = model_->start_point();
        const Vec x_origin = projected_origin();

        jac_pattern_ = pattern_union(stacked_jacobian(x_start), stacked_jacobian(x_origin));
        jac_rows_.resize(jac_pattern_.nonZeros());
        jac_cols_.resize(jac_pattern_.nonZeros());
        {
            int slot = 0;
            for (Index r = 0; r < jac_pattern_.outerSize(); ++r) {
                for (SpRM::InnerIterator it(jac_pattern_, r); it; ++it, ++slot) {
                    jac_rows_(slot) = static_cast<int>(it.row());
                    jac_cols_(slot) = static_cast<int>(it.col());
                }
            }
        }

        // A Lagrangian Hessian pattern can depend on which multipliers are
        // nonzero, so the union is taken with the ones the two evaluations
        // below actually use: all-ones, which activates every constraint's
        // contribution at once.
        const Vec ones_e = Vec::Ones(me_);
        const Vec ones_i = Vec::Ones(mi_);
        hess_pattern_ = pattern_union(model_->eval_hess(x_start, 1.0, ones_e, ones_i),
                                      model_->eval_hess(x_origin, 1.0, ones_e, ones_i));
        hess_rows_.resize(hess_pattern_.nonZeros());
        hess_cols_.resize(hess_pattern_.nonZeros());
        {
            int slot = 0;
            for (Index r = 0; r < hess_pattern_.outerSize(); ++r) {
                for (SpRM::InnerIterator it(hess_pattern_, r); it; ++it, ++slot) {
                    // Upper (row, col) declared as lower (col, row): same
                    // symmetric entry, the triangle NLPProblem asks for.
                    hess_rows_(slot) = static_cast<int>(it.col());
                    hess_cols_(slot) = static_cast<int>(it.row());
                }
            }
        }
    }

    std::shared_ptr<const NlpModel> model_;
    std::string name_;
    Index n_ = 0, me_ = 0, mi_ = 0;
    SpRM jac_pattern_, hess_pattern_;
    Eigen::VectorXi jac_rows_, jac_cols_, hess_rows_, hess_cols_;
};

// =============================================================================
// Which cells dual-bind
// =============================================================================

/// @brief Why @p cell cannot be measured through the dual-bind path, or an
///        empty string when it can. See this header's banner for the reasoning.
inline std::string dual_bind_refusal(const CorpusCell &cell) {
    switch (cell.start) {
    case StartTaxonomy::kNeutralCold:
    case StartTaxonomy::kPhysicsInformed:
        return {};
    case StartTaxonomy::kCorrupted:
        return "start is a damaged SQP WarmStart from a prior solve at p0; the interior-point "
               "engine accepts no such value, so leg (a) cannot run from this cell's own start";
    case StartTaxonomy::kFullWarm:
        return "start is an SQP WarmStart carried from a prior solve at p0 (hot handle and "
               "activity encoding, no interior-point counterpart), so leg (a) cannot run from "
               "this cell's own start";
    case StartTaxonomy::kActivityOnly:
        return "start is a SYNTHESIZED interior-point iterate pushed through from_interior_point; "
               "superseded here by legs (c)/(d), which measure the same hand-off with a real "
               "interior-point export";
    }
    return "unknown start taxonomy";
}

inline bool cell_dual_binds(const CorpusCell &cell) { return dual_bind_refusal(cell).empty(); }

/// @brief The cells this measurement package covers, in `all_cells()` order.
inline std::vector<const CorpusCell *> dual_bindable_cells() {
    std::vector<const CorpusCell *> out;
    for (const CorpusCell &cell : corpus::all_cells()) {
        if (cell_dual_binds(cell)) {
            out.push_back(&cell);
        }
    }
    return out;
}

// =============================================================================
// The legs
// =============================================================================

/// The interior-point leg's recorded outcome. Counters are the asserted
/// currency; `wall_s` is informational.
struct IpmLegRow {
    /// False until this leg actually finished; a leg that never ran reports
    /// `absent`, never a default value -- see margins_row.
    bool ran = false;
    hven::ConvergenceFlags flag = hven::ConvergenceFlags::NOTCONVERGED;
    int iters = -1;
    Index analyses = -1;
    Index factorizations = -1;
    Index solves = -1;
    double f = std::numeric_limits<double>::quiet_NaN();
    double kkt_inf = std::numeric_limits<double>::quiet_NaN();
    double econ_inf = std::numeric_limits<double>::quiet_NaN();
    double icon_inf = std::numeric_limits<double>::quiet_NaN();
    double barr_inf = std::numeric_limits<double>::quiet_NaN();
    bool export_has_polish = false;
    double wall_s = 0.0;
};

/// One SQP leg's recorded outcome (b, c or d). Same shape for all three so the
/// margins are a subtraction and nothing else.
struct SqpLegRow {
    /// False until this leg actually finished; see IpmLegRow::ran.
    bool ran = false;
    SqpStatus status = SqpStatus::kNumericalError;
    StartLevel start_level = StartLevel::kCold;
    Index major_iters = -1;
    Index qp_minor_iters = -1;
    Index factorizations = -1;
    Index symbolic_analyses = -1;
    Index ip_activity_inferred = -1;
    Index seeded_clamped = -1;
    double f = std::numeric_limits<double>::quiet_NaN();
    double kkt_residual = std::numeric_limits<double>::quiet_NaN();
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    double feasibility = std::numeric_limits<double>::quiet_NaN();
    double complementarity = std::numeric_limits<double>::quiet_NaN();
    double wall_s = 0.0;
};

/// Everything one cell contributes to the artifact.
struct CellLegs {
    const CorpusCell *cell = nullptr;
    Index n = 0, me = 0, mi = 0;
    IpmLegRow a;
    SqpLegRow b, c, d;
    /// True when leg (a)'s export carried no "hven.ipm.polish.v1" extension, in
    /// which case legs (c) and (d) stage the SAME value and their margins are
    /// necessarily equal. Recorded rather than hidden.
    bool legs_cd_identical = false;
};

/// Knobs the runner exposes. Every default is the shipped default or the
/// corpus's own committed choice, so a run that passes nothing is the
/// configuration the artifact declares.
struct LegOptions {
    /// Interior-point iteration cap per phase.
    int ipm_max_iters = 200;
    /// Interior-point console verbosity; 3 and above is silent.
    int ipm_print_level = 10;
};

namespace detail {

inline double seconds_since(const std::chrono::steady_clock::time_point &t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

/// The start point the cell declares, for the two taxonomies that declare one
/// as a bare primal. Shared with the corpus's own recipe rather than
/// re-derived: corpus_cells.h is the authority on what a taxonomy means.
inline Vec start_point_for(const CorpusCell &cell, const test_support::F7CollocationChain &model) {
    switch (cell.start) {
    case StartTaxonomy::kNeutralCold:
        return model.start_point();
    case StartTaxonomy::kPhysicsInformed:
        return corpus::detail::physics_informed_start(model, cell.p);
    default:
        throw std::invalid_argument(
            fmt::format("start_point_for: cell '{}' does not declare a bare primal start; "
                        "dual_bind_refusal() explains why it is not measured here",
                        cell.id));
    }
}

inline SqpLegRow record_sqp(const SqpSolution &sol, double wall_s) {
    SqpLegRow row;
    row.ran = true;
    row.status = sol.status;
    row.start_level = sol.counters.start_level_used;
    row.major_iters = sol.counters.major_iters;
    row.qp_minor_iters = sol.counters.qp_minor_iters;
    row.factorizations = sol.counters.factorizations;
    row.symbolic_analyses = sol.counters.symbolic_analyses;
    row.ip_activity_inferred = sol.counters.ip_activity_inferred;
    row.seeded_clamped = sol.counters.seeded_clamped;
    row.f = sol.f;
    row.kkt_residual = sol.kkt_residual;
    row.stationarity = sol.stationarity;
    row.feasibility = sol.feasibility;
    row.complementarity = sol.complementarity;
    row.wall_s = wall_s;
    return row;
}

/// One SQP leg: a fresh driver, a fresh bridge over the SAME declared problem,
/// the same x0, and whatever `stage` chooses to stage before the solve.
///
/// The TWO-argument entry on all three legs. The four-argument overload's
/// explicit `WarmStart` is refused against a staged value, so the
/// minor-iteration budget it carries is unavailable to legs (c) and (d), and
/// taking it on leg (b) alone would leave the cold leg the only bounded one.
/// All three legs are therefore bounded by the SAME thing: the options object's
/// SqpOptions::max_iter and SqpOptions::qp.max_iter caps, which
/// corpus_cells.h's options_for_cell sets, with the runner's wall deadline as
/// the outer guard.
template <typename StageFn>
SqpLegRow run_sqp_leg(const std::shared_ptr<NlpProblemModel> &model, const Vec &x0,
                      const SqpOptions &opts, StageFn &&stage) {
    NlpModelAggregate bridge(model);
    SqpDriver driver{opts};
    stage(driver, bridge);
    const auto t0 = std::chrono::steady_clock::now();
    const SqpSolution sol = driver.solve(bridge, x0);
    return record_sqp(sol, seconds_since(t0));
}

} // namespace detail

/// Which leg has just finished, in the order run_cell_legs runs them.
enum class LegStage { kIpm, kWarmCore, kWarmPolish, kCold, kMargins };

/// Called after each leg with the partially-filled result, so a runner can
/// write a row the moment it exists rather than at the end.
using LegSink = std::function<void(const CellLegs &, LegStage)>;

/// @brief Runs all four legs of one dual-bindable cell.
///
/// EXECUTION ORDER IS (a), (d), (c), (b) -- CHEAPEST-AND-MOST-INFORMATIVE
/// FIRST, because a runner may be enforcing a wall deadline. Each leg is an
/// independent solve on a fresh driver over a fresh bridge, so the order moves
/// no counter; what it decides is WHICH LEGS EXIST when a cell runs out of
/// budget.
///
///   (a) first, unconditionally -- it is the exporter the two warm legs stage.
///   (d) second, because it is CONSTANT COST. The polish extension carries the
///       active set, so this leg builds one subproblem and certifies: 1 major,
///       2 QP minors, 1 factorization, at every size measured.
///   (c) third -- core-only has no activity information, so its QP minors SCALE
///       with the problem and on a large cell can consume a whole budget.
///   (b) last -- the cold baseline is the most expensive of the four.
///
/// @param cell The cell; must satisfy cell_dual_binds().
/// @param opts The runner's knobs.
/// @param sink Optional per-leg callback; see LegSink.
/// @throws std::invalid_argument if @p cell does not dual-bind.
inline CellLegs run_cell_legs(const CorpusCell &cell, const LegOptions &opts = {},
                              const LegSink &sink = {}) {
    if (!cell_dual_binds(cell)) {
        throw std::invalid_argument(fmt::format("run_cell_legs: cell '{}' does not dual-bind: {}",
                                                cell.id, dual_bind_refusal(cell)));
    }

    // THE ONE DECLARATION, built once; both engines reach the cell through it.
    const auto f7 =
        std::make_shared<test_support::F7CollocationChain>(corpus::detail::make_model(cell));
    const Vec x0 = detail::start_point_for(cell, *f7);
    const auto declared = std::make_shared<ModelAsNlpProblem>(f7, std::string(cell.id));
    const auto model = std::make_shared<NlpProblemModel>(declared);

    CellLegs legs;
    legs.cell = &cell;
    legs.n = model->n();
    legs.me = model->me();
    legs.mi = model->mi();

    // --- leg (a): the interior-point baseline, and the exporter ---
    WarmStartData exported;
    {
        NLPSolver ipm(declared);
        ipm.optimizer_->set_print_level(opts.ipm_print_level);
        ipm.optimizer_->set_max_iters(opts.ipm_max_iters);
        ipm.optimizer_->set_tols(corpus::detail::kKktTol, corpus::detail::kFeasTol,
                                 corpus::detail::kFeasTol, corpus::detail::kKktTol);
        ipm.transcribe();
        const auto t0 = std::chrono::steady_clock::now();
        legs.a.flag = ipm.optimize(x0);
        legs.a.wall_s = detail::seconds_since(t0);

        const auto &result = ipm.optimizer_->result();
        legs.a.iters = result.iter_num_;
        legs.a.f = result.obj_val_;
        legs.a.kkt_inf = result.kkt_inf_;
        legs.a.econ_inf = result.econ_inf_;
        legs.a.icon_inf = result.icon_inf_;
        legs.a.barr_inf = result.barr_inf_;
        legs.a.analyses = ipm.optimizer_->kkt_analysis_count();
        legs.a.factorizations = ipm.optimizer_->kkt_factor_counters().factorize_count;
        legs.a.solves = ipm.optimizer_->kkt_factor_counters().solve_count;

        exported = ipm.optimizer_->export_warm_start();
        legs.a.export_has_polish = find_ipm_polish(exported) != nullptr;
        legs.a.ran = true;
    }
    legs.legs_cd_identical = !legs.a.export_has_polish;
    const auto emit = [&sink, &legs](LegStage stage) {
        if (sink) {
            sink(legs, stage);
        }
    };
    emit(LegStage::kIpm);

    const SqpOptions sqp_opts = corpus::detail::options_for_cell(cell);

    // --- leg (d): SQP warm, with polish -- CONSTANT COST, so it runs first ---
    legs.d = detail::run_sqp_leg(
        model, x0, sqp_opts,
        [&exported](SqpDriver &driver, NlpModelAggregate &) { driver.stage_warm_start(exported); });
    emit(LegStage::kWarmPolish);

    // --- leg (c): SQP warm, core only ---
    // (a)'s export with the producer's extensions cleared, which is the R3
    // shape. Runs after (d) -- see the execution-order note above.
    {
        WarmStartData core = exported;
        core.extensions_.clear();
        legs.c = detail::run_sqp_leg(
            model, x0, sqp_opts,
            [&core](SqpDriver &driver, NlpModelAggregate &) { driver.stage_warm_start(core); });
    }
    emit(LegStage::kWarmCore);

    // --- leg (b): SQP cold, last -- see the execution-order note above ---
    legs.b = detail::run_sqp_leg(model, x0, sqp_opts, [](SqpDriver &, NlpModelAggregate &) {});
    emit(LegStage::kCold);
    emit(LegStage::kMargins);

    return legs;
}

// =============================================================================
// Margins
// =============================================================================

/// One leg's margin against leg (b), in the three asserted counters. POSITIVE
/// MEANS SAVED: b minus the warm leg, so 3 reads "three fewer majors than
/// cold". A margin against a leg that did not reach a counter (-1) is not
/// defined and is reported as absent.
struct CounterMargin {
    bool defined = false;
    Index majors = 0;
    Index qp_minors = 0;
    Index factorizations = 0;
};

inline CounterMargin margin_against_cold(const SqpLegRow &cold, const SqpLegRow &warm) {
    CounterMargin m;
    if (cold.major_iters < 0 || warm.major_iters < 0) {
        return m;
    }
    m.defined = true;
    m.majors = cold.major_iters - warm.major_iters;
    m.qp_minors = cold.qp_minor_iters - warm.qp_minor_iters;
    m.factorizations = cold.factorizations - warm.factorizations;
    return m;
}

// =============================================================================
// The aggregate row
// =============================================================================
//
// The margins row lives here, not in the runner's CLI glue, so that the gate
// test shares it rather than copying it.

/// @brief One cell's identifying prefix: id, node count, window, taxonomy.
inline std::string cell_prefix(const CorpusCell &cell) {
    return fmt::format("{},{},{},{}", cell.id, cell.n_nodes, corpus::to_string(cell.ctag),
                       corpus::to_string(cell.start));
}

/// @brief The interior-point convergence flag as text.
inline const char *flag_string(hven::ConvergenceFlags flag) {
    switch (flag) {
    case hven::ConvergenceFlags::CONVERGED:
        return "CONVERGED";
    case hven::ConvergenceFlags::ACCEPTABLE:
        return "ACCEPTABLE";
    case hven::ConvergenceFlags::NOTCONVERGED:
        return "NOTCONVERGED";
    case hven::ConvergenceFlags::DIVERGING:
        return "DIVERGING";
    case hven::ConvergenceFlags::SINGULAR_KKT:
        return "SINGULAR_KKT";
    }
    return "UNKNOWN";
}

/// The token every column of this artifact uses for a value that was never
/// measured. Never `0` and never a status: a status would claim an outcome the
/// leg never reached.
inline constexpr const char *kAbsent = "absent";

/// @brief A leg's own status, or `absent` when that leg never ran.
inline std::string leg_status(const SqpLegRow &row) {
    return row.ran ? std::string(hven::solvers::to_string(row.status)) : std::string(kAbsent);
}

/// @brief The interior-point leg's own flag, or `absent` when it never ran.
inline std::string ipm_status(const IpmLegRow &row) {
    return row.ran ? std::string(flag_string(row.flag)) : std::string(kAbsent);
}

/// @brief One counter as text, or `absent` when its leg never ran.
inline std::string leg_counter(const SqpLegRow &row, Index value) {
    return row.ran ? fmt::format("{}", value) : std::string(kAbsent);
}

/// @brief A margin's three counters, or three `absent`s when it is undefined.
inline std::string margin_cell(const CounterMargin &m) {
    if (!m.defined) {
        return fmt::format("{},{},{}", kAbsent, kAbsent, kAbsent);
    }
    return fmt::format("{},{},{}", m.majors, m.qp_minors, m.factorizations);
}

/// @brief The aggregate row for one cell.
///
/// EVERY STATUS COLUMN REPORTS ITS OWN LEG, never a cell-level outcome: on a
/// cell killed at its wall deadline, a shared status would say the legs that
/// HAD finished failed. A leg that did not run is `absent` in its status column
/// and in its counter columns. The MARGIN columns go absent on their own
/// separate condition -- a margin needs both its legs, which is what
/// CounterMargin::defined carries.
inline std::string margins_row(const CellLegs &legs) {
    const CounterMargin c = margin_against_cold(legs.b, legs.c);
    const CounterMargin d = margin_against_cold(legs.b, legs.d);
    return fmt::format("{},{},{},{},{},{},{},{},{},{},{},{},{}\n", cell_prefix(*legs.cell), legs.n,
                       leg_status(legs.b), leg_counter(legs.b, legs.b.major_iters),
                       leg_counter(legs.b, legs.b.qp_minor_iters),
                       leg_counter(legs.b, legs.b.factorizations), leg_status(legs.c),
                       leg_status(legs.d), margin_cell(c), margin_cell(d), ipm_status(legs.a),
                       legs.a.ran ? (legs.a.export_has_polish ? "1" : "0") : kAbsent,
                       legs.a.ran ? (legs.legs_cd_identical ? "1" : "0") : kAbsent);
}

} // namespace hven::solvers::crossover
