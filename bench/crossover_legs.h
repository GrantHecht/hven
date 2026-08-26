// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// bench/crossover_legs.h — M5 W5: the IPM -> SQP crossover measurement legs,
// and the dual-bind path that makes a replay-corpus cell reachable from BOTH
// engines under ONE declaration.
//
// THE PROTOCOL THIS IMPLEMENTS is declared verbatim in
// docs/notes/2026-08-m5-ledger.md's "W5 LEG PROTOCOL" entry (2026-08-25) and
// restated in the artifact's README. Four legs per dual-bindable cell:
//
//   (a) IPM-only        the interior-point baseline, and the exporter: its
//                       export_warm_start() is what legs (c) and (d) stage.
//   (b) SQP cold        the same declaration, same x0, nothing staged.
//   (c) SQP warm core   (a)'s export with extensions_ CLEARED -- the neutral
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
// =============================================================================
// THE DUAL-BIND PATH, and why it runs in this direction
// =============================================================================
//
// The W3 crossover pin (tests/sqp/test_sqp_warm_currency.cpp,
// InteriorPointExportCrossesOverIntoTheSqpEngine) established the shape: ONE
// declared NLPProblem, bound to the interior-point engine through NLPSolver
// and to the SQP engine through NlpProblemModel. Both engines then key the
// same DeclarationKey, which is the whole content of the 2026-08-25
// declaration-identity ruling, and an exported value stages across with no
// conversion and no re-stamp.
//
// The corpus's cells are NlpModels (F7CollocationChain), not NLPProblems, so
// the missing half of that path is an NlpModel stated as an NLPProblem. That
// is ModelAsNlpProblem below. It is deliberately NOT a second conversion
// alongside NlpProblemModel: the declared NLPProblem is the ONE declaration,
// and BOTH engines reach the cell through it -- the interior-point engine via
// NLPSolver's own transcription, the SQP engine via NlpProblemModel, exactly
// as the W3 pin does. Neither engine sees the F7 model directly.
//
// The consequence worth stating plainly, because it is what makes the legs
// comparable: leg (b) is NOT the committed walk-corpus baseline row for the
// same cell. It is a cold solve of the same mathematics reached through the
// dual-bind conversion, so its counters may differ from
// bench/baselines/*/walk_baseline.csv. b, c and d are strictly comparable to
// one another -- one declaration, one conversion, one start point, one
// options object -- which is what the protocol's margins are taken over.
//
// =============================================================================
// WHICH CELLS DUAL-BIND, and why the rest cannot
// =============================================================================
//
// Every cell's PROBLEM dual-binds: F7CollocationChain states f, cE, cI, the
// two Jacobians, the Lagrangian Hessian and the box, which is everything
// NLPProblem asks for. What does not always dual-bind is the cell's START.
//
//   kNeutralCold, kPhysicsInformed -- a bare primal x0. Both engines take a
//       primal start point, so the cell binds whole. THESE ARE THE 24 CELLS
//       THE ARTIFACT MEASURES.
//
//   kCorrupted, kFullWarm -- the cell's declared start is a WarmStart, the
//       SQP engine's own internal warm value, produced by a prior SQP solve
//       at p0 (kFullWarm) or that value then damaged (kCorrupted). It carries
//       a hot factorization handle and an activity encoding that have no
//       interior-point counterpart: the M5 currency's neutral core is not a
//       WarmStart, and the IPM's stage_warm_start takes a WarmStartData. Leg
//       (a) therefore cannot be run FROM THE CELL'S OWN START CONDITION, and
//       a leg (a) run from somewhere else would not be that cell's baseline.
//
//   kActivityOnly -- the cell's declared start is a SYNTHESIZED interior-point
//       iterate (corpus_cells.h's f7_ip_iterate, a hand-built central-path
//       point) pushed through from_interior_point. Leg (a) would have to be
//       the synthesis rather than a solve. This taxonomy is superseded here by
//       construction: legs (c) and (d) measure the same hand-off with a REAL
//       interior-point export in place of the synthetic one, which is the
//       measurement kActivityOnly was standing in for.
//
// Both refusals are recorded per cell by dual_bind_refusal() and are LISTED in
// the artifact. Nothing is silently dropped.

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
///        bound to it through the W3 dual-bind path.
///
/// ROW LAYOUT. The declared rows are the equalities first, in the model's own
/// cE order, then the inequalities in its own cI order:
///
///   row i          in [0, me)        gl = gu = 0        -> cE_i(x) = 0
///   row me + j     in [0, mi)        gl = -inf, gu = 0  -> cI_j(x) <= 0
///
/// which is exactly the pair of kinds NLPRowClassification calls Equality and
/// UpperBounded. Reading the declaration back through NlpProblemModel therefore
/// reproduces the ORIGINAL model's cE and cI, in the original order, with no
/// sign flip: an Equality row converts to g(x) - gl = cE, and an UpperBounded
/// row to g(x) - gu = cI. The Lagrangians agree term for term, so
/// NLPProblem's lambda over [cE; cI] splits into the model's (lambda_e,
/// lambda_i) by a head/tail cut and nothing else.
///
/// STRUCTURE. NLPProblem queries the two sparsity patterns once and they must
/// not move afterwards, but an NlpModel returns whole matrices whose pattern
/// it is free to decide per point. The declared structure here is therefore the
/// UNION of the patterns at two points: the model's own start point, and the
/// point NLPSolver's transcription evaluates at (the origin projected onto the
/// declared box -- see NLPProblem's own note, which is why that second point is
/// not optional). Every later evaluation is merged into the declared slots, and
/// a nonzero arriving at a slot the union did not declare is REFUSED by name
/// rather than silently dropped: a moving pattern would make every counter
/// below a measurement of a different problem than the one declared.
///
/// HESSIAN TRIANGLE. NlpModel returns the UPPER triangle (row <= col);
/// NLPProblem declares the LOWER one (row >= col). The two describe the same
/// symmetric matrix, so the conversion is an index transpose on the declared
/// structure and nothing at all on the values.
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
        // NLPProblem's L = obj_factor*f + lambda^T g, with g = [cE; cI]; the
        // model's is obj_scale*f + lambda_e^T cE + lambda_i^T cI. Same terms,
        // so the split is a head/tail cut.
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
    // onto the declared box. Quoted from NLPProblem's own note -- the pattern
    // union has to cover it, because those two calls happen before any solve
    // iterate exists and their patterns are what the solver keeps.
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
/// THE TWO-ARGUMENT ENTRY, deliberately, on all three legs. The four-argument
/// overload's explicit `WarmStart` argument is REFUSED against a staged value
/// -- SqpDriver::refuse_two_warm_sources names both sources rather than letting
/// one silently win, and it refuses a default-constructed argument too. So the
/// minor-iteration budget that overload carries is unavailable to legs (c) and
/// (d), and taking it on leg (b) alone would make the cold leg the only bounded
/// one, which is exactly the asymmetry the margins must not have. All three legs
/// are therefore bounded by the SAME thing: the options object's own
/// SqpOptions::max_iter and SqpOptions::qp.max_iter caps, which
/// corpus_cells.h's options_for_cell sets. The runner's wall deadline is the
/// outer guard on top of that.
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
///       2 QP minors, 1 factorization, at every size measured. It is both the
///       cheapest leg and the one that says the most.
///   (c) third -- core-only has no activity information, so its QP minors SCALE
///       with the problem and on a large cell can consume a whole budget.
///   (b) last -- the cold baseline is the most expensive of the four.
///
/// THIS ORDER WAS WRONG ONCE, AND SAYING SO IS THE POINT. The first W5 sweep
/// ran (a), (c), (d), (b), on the reasoning that only the cold leg was
/// expensive enough to starve the others. At N = 10000 on the path window that
/// is false: leg (c) alone exhausted the per-cell budget, legs (d) and (b)
/// never ran, and the cells where the polish route is most impressive are
/// exactly the ones that lost it. Putting the constant-cost leg ahead of the
/// scaling one costs nothing and cannot fail that way.
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

    // THE ONE DECLARATION. Built once, and both engines reach the cell through
    // it: the interior-point engine through NLPSolver's transcription, the SQP
    // engine through NlpProblemModel. That is what makes the DeclarationKey
    // agree and the export stage across with no re-stamp.
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
    // The tag stripped from (a)'s export, which is the R3 shape: the neutral
    // core alone, with the producer's extensions cleared. Its QP minors SCALE
    // with the problem, which is why it runs after (d) -- see the order note.
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

} // namespace hven::solvers::crossover
