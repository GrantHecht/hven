// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "hven/model/problem.h"

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "hven/detail/drivers/result_access.h"
#include "hven/detail/model/make_nlp_program.h"
#include "hven/detail/model/nlp_adapter.h"
#include "hven/detail/model/nlp_model.h"
#include "hven/detail/model/nlp_model_assembly.h"
#include "hven/detail/model/nlp_problem_model.h"
#include "hven/detail/model/non_linear_program.h"
#include "hven/detail/model/problem_access.h"
#include "hven/drivers/solve_result.h"
#include "hven/model/assembly_declaration.h"
#include "hven/model/nlp.h"

namespace hven::solvers {
namespace detail {

namespace {

// The variable census of a box. A box whose sides disagree in length with each
// other or with @p n describes no census: it is left empty here, and the solve
// refuses the box by name (the engines' own entry checks).
void count_variables(Index n, const Vec &lower, const Vec &upper, ProblemStatistics &stats) {
    stats.variables = n;
    stats.free_variables = stats.lower_bounded_variables = stats.upper_bounded_variables = 0;
    stats.boxed_variables = stats.fixed_variables = 0;
    if (lower.size() != n || upper.size() != n) {
        return;
    }
    for (Index i = 0; i < n; ++i) {
        const bool has_lower = std::isfinite(lower[i]);
        const bool has_upper = std::isfinite(upper[i]);
        if (has_lower && has_upper) {
            ++(lower[i] == upper[i] ? stats.fixed_variables : stats.boxed_variables);
        } else if (has_lower) {
            ++stats.lower_bounded_variables;
        } else if (has_upper) {
            ++stats.upper_bounded_variables;
        } else {
            ++stats.free_variables;
        }
    }
}

void count_rows(const std::vector<NLPRowKind> &kinds, ProblemStatistics &stats) {
    stats.constraints = static_cast<Index>(kinds.size());
    stats.equality_rows = stats.lower_bounded_rows = stats.upper_bounded_rows = 0;
    stats.range_rows = stats.free_rows = 0;
    for (const NLPRowKind kind : kinds) {
        switch (kind) {
        case NLPRowKind::Equality:
            ++stats.equality_rows;
            break;
        case NLPRowKind::UpperBounded:
            ++stats.upper_bounded_rows;
            break;
        case NLPRowKind::LowerBounded:
            ++stats.lower_bounded_rows;
            break;
        case NLPRowKind::Range:
            ++stats.range_rows;
            break;
        case NLPRowKind::Free:
            ++stats.free_rows;
            break;
        }
    }
}

// The split-form census: me equality rows, mi rows c_I(x) <= 0.
void count_split_rows(Index me, Index mi, ProblemStatistics &stats) {
    stats.constraints = me + mi;
    stats.equality_rows = me;
    stats.upper_bounded_rows = mi;
    stats.lower_bounded_rows = stats.range_rows = stats.free_rows = 0;
}

void count_program(const NonLinearProgram &program, ProblemStatistics &stats) {
    const std::vector<VariableBound> bounds = program.declaration().materialize_variable_bounds();
    Vec lower(static_cast<Index>(bounds.size())), upper(static_cast<Index>(bounds.size()));
    for (std::size_t i = 0; i < bounds.size(); ++i) {
        lower[static_cast<Index>(i)] = bounds[i].lower_;
        upper[static_cast<Index>(i)] = bounds[i].upper_;
    }
    count_variables(program.primal_vars_, lower, upper, stats);
    count_split_rows(program.user_equal_cons_, program.inequal_cons_, stats);
}

Vec concatenate(const Vec &head, const Vec &tail) {
    Vec out(head.size() + tail.size());
    out << head, tail;
    return out;
}

[[noreturn]] void throw_moved_from() {
    throw std::logic_error(
        "Problem: the problem was moved from; it may only be destroyed or assigned to");
}

// Whether two index columns hold the same entries; on a difference, the first
// differing slot. The four columns are one length: the caller has checked the
// nonzero counts, and a mismatch here is a defect, refused rather than read past.
Index first_difference(const Eigen::VectorXi &a_rows, const Eigen::VectorXi &a_cols,
                       const IndexVec &b_rows, const IndexVec &b_cols) {
    if (a_cols.size() != a_rows.size() || b_rows.size() != a_rows.size() ||
        b_cols.size() != a_rows.size()) {
        throw std::logic_error(
            fmt::format("Problem: structure columns of unequal length ({}, {}, {}, {})",
                        a_rows.size(), a_cols.size(), b_rows.size(), b_cols.size()));
    }
    for (Index k = 0; k < a_rows.size(); ++k) {
        if (a_rows[k] != b_rows[k] || a_cols[k] != b_cols[k]) {
            return k;
        }
    }
    return -1;
}

} // namespace

/// One Problem's state: the problem as stated, and each engine's transcription.
class ProblemState {
  public:
    enum class Kind { kNlp, kPieces, kProgram, kModel, kAssembly };

    explicit ProblemState(std::shared_ptr<Nlp> nlp) : kind_(Kind::kNlp), nlp_(std::move(nlp)) {
        if (nlp_ == nullptr) {
            throw std::invalid_argument("Problem: the Nlp pointer is null");
        }
        // The transcription the SQP solves, built now: its constructor reads and
        // validates the whole declaration (sizes, bounds, row bounds, both
        // structures) once, and this state reads it from there.
        nlp_model_ = std::make_shared<NlpProblemModel>(nlp_);
        dims_ = nlp_model_->dimensions();
        count_variables(dims_.n, nlp_model_->lower(), nlp_model_->upper(), statistics_);
        count_rows(nlp_model_->rows().kinds_, statistics_);
    }

    explicit ProblemState(AssemblyDeclaration pieces) : kind_(Kind::kPieces) {
        auto program = std::make_shared<NonLinearProgram>(pieces.partition_count_);
        program->adopt_declaration(std::move(pieces));
        count_program(*program, statistics_);
        program_ = std::move(program);
    }

    ProblemState(std::shared_ptr<NonLinearProgram> program, Kind kind)
        : kind_(kind), program_(std::move(program)) {
        if (program_ == nullptr) {
            throw std::invalid_argument("problem_from_program: the program pointer is null");
        }
        count_program(*program_, statistics_);
    }

    explicit ProblemState(std::shared_ptr<const NlpModel> model)
        : kind_(Kind::kModel), model_(std::move(model)) {
        if (model_ == nullptr) {
            throw std::invalid_argument("problem_from_model: the model pointer is null");
        }
        count_variables(model_->n(), model_->lower(), model_->upper(), statistics_);
        count_split_rows(model_->me(), model_->mi(), statistics_);
    }

    explicit ProblemState(NlpModelAssembly &bridge) : kind_(Kind::kAssembly), bridge_(&bridge) {
        count_variables(bridge.model().n(), bridge.model().lower(), bridge.model().upper(),
                        statistics_);
        count_split_rows(bridge.model().me(), bridge.model().mi(), statistics_);
    }

    Index num_variables() const noexcept {
        switch (kind_) {
        case Kind::kNlp:
            return dims_.n;
        case Kind::kPieces:
        case Kind::kProgram:
            return program_->primal_vars_;
        case Kind::kModel:
            return model_->n();
        case Kind::kAssembly:
            return bridge_->model().n();
        }
        return 0;
    }

    Index num_constraints() const noexcept {
        switch (kind_) {
        case Kind::kNlp:
            return dims_.m;
        case Kind::kPieces:
        case Kind::kProgram:
            return static_cast<Index>(program_->user_equal_cons_) + program_->inequal_cons_;
        case Kind::kModel:
            return model_->me() + model_->mi();
        case Kind::kAssembly:
            return bridge_->model().me() + bridge_->model().mi();
        }
        return 0;
    }

    const ProblemStatistics &statistics() const noexcept { return statistics_; }

    Index partitions() const noexcept {
        return program_ != nullptr ? static_cast<Index>(program_->num_partitions_) : 1;
    }

    NonLinearProgram &ipm_program() {
        switch (kind_) {
        case Kind::kNlp:
            if (!nlp_->has_hessian()) {
                throw std::invalid_argument(fmt::format(
                    "IpmSolver: the problem '{}' declares has_hessian() == false; the "
                    "interior-point engine needs the Lagrangian Hessian. Provide "
                    "hessian_structure() and eval_hessian(), or solve it with an SqpSolver "
                    "in quasi-Newton mode (SqpHessianMode::kLbfgsDamped)",
                    nlp_->name()));
            }
            refresh_nlp();
            if (program_ == nullptr) {
                auto core = std::make_shared<NLPAdapterCore>(nlp_model_, nlp_->name());
                program_ = make_nlp_program(core);
                core_ = std::move(core);
            }
            return *program_;
        case Kind::kModel:
            if (program_ == nullptr) {
                auto core =
                    std::make_shared<NLPAdapterCore>(std::const_pointer_cast<NlpModel>(model_));
                program_ = make_nlp_program(core);
                core_ = std::move(core);
            } else {
                core_->invalidate_cache();
            }
            return *program_;
        case Kind::kPieces:
        case Kind::kProgram:
            return *program_;
        case Kind::kAssembly:
            throw std::invalid_argument(
                "IpmSolver: this Problem wraps an SQP claim-stream bridge (NlpModelAssembly), "
                "which the interior-point engine does not solve");
        }
        throw std::logic_error("Problem: unrecognised problem kind");
    }

    const NonLinearProgram *built_ipm_program() const noexcept { return program_.get(); }

    SqpTarget sqp_target(bool exact_hessian) {
        switch (kind_) {
        case Kind::kNlp:
            if (exact_hessian && !nlp_->has_hessian()) {
                throw std::invalid_argument(fmt::format(
                    "SqpSolver: the problem '{}' declares has_hessian() == false, and this "
                    "solve reads the exact Lagrangian Hessian (SqpHessianMode::kExact). Use "
                    "SqpHessianMode::kLbfgsDamped, or provide hessian_structure() and "
                    "eval_hessian()",
                    nlp_->name()));
            }
            refresh_nlp();
            return SqpTarget{nlp_model_.get(), nullptr};
        case Kind::kModel:
            return SqpTarget{model_.get(), nullptr};
        case Kind::kAssembly:
            return SqpTarget{nullptr, bridge_};
        case Kind::kPieces:
        case Kind::kProgram:
            throw std::invalid_argument(
                "SqpSolver: this Problem is built from pieces (an AssemblyDeclaration), which "
                "the SQP engine does not solve; solve it with IpmSolver");
        }
        throw std::logic_error("Problem: unrecognised problem kind");
    }

    // The result in the problem's own row space, mapped from the engine's own row
    // blocks. NO CALLBACK IS MADE HERE: every callback count after a solve is the
    // engine's.
    void report(SolveResult &result) const {
        const auto &rows = engine_rows(result);
        const Index m = num_constraints();
        if (kind_ == Kind::kNlp) {
            const NlpProblemModel &model = *nlp_model_;
            if (rows.lambda_e.size() == model.me() && rows.lambda_i.size() == model.mi()) {
                result.lambda = model.compose_user_multipliers(rows.lambda_e, rows.lambda_i);
            } else {
                result.lambda.resize(0);
            }
            map_row_values(model, rows.ce, rows.ci, result.g);
            return;
        }
        if (rows.lambda_e.size() + rows.lambda_i.size() == m) {
            result.lambda = concatenate(rows.lambda_e, rows.lambda_i);
        } else {
            result.lambda.resize(0);
        }
        if (rows.ce.size() + rows.ci.size() == m) {
            result.g = concatenate(rows.ce, rows.ci);
        } else {
            result.g.resize(0);
        }
    }

    const NlpProblemModel *nlp_model() const noexcept { return nlp_model_.get(); }

  private:
    // g in the problem's row space from the engine's measured residuals, each
    // row through its kind's shift: equality g = c_E + g_lower; upper-bounded
    // g = c_I + g_upper; lower-bounded g = g_lower - c_I; range g = c_I(upper
    // part) + g_upper. One rounding of that shift apart from g(x) itself.
    //
    // THE RULE FOR AN UNMEASURED ROW. A free row is passed to no engine, so no
    // engine measures it: its entry is NaN (unmeasured) and is never evaluated.
    // The engine's blocks are empty where it could not measure its returned
    // point, and then g is empty. Otherwise g is m long.
    static void map_row_values(const NlpProblemModel &model, const Vec &ce, const Vec &ci, Vec &g) {
        const NLPRowClassification &rows = model.rows();
        const int m = model.num_declared_rows();
        if (ce.size() != model.me() || ci.size() != model.mi()) {
            g.resize(0);
            return;
        }
        const Vec &gl = model.row_lower();
        const Vec &gu = model.row_upper();
        g.resize(m);
        for (int r = 0; r < m; ++r) {
            switch (rows.kinds_[static_cast<std::size_t>(r)]) {
            case NLPRowKind::Equality:
                g[r] = ce[rows.eq_row_[r]] + gl[r];
                break;
            case NLPRowKind::UpperBounded:
            case NLPRowKind::Range:
                g[r] = ci[rows.iq_upper_row_[r]] + gu[r];
                break;
            case NLPRowKind::LowerBounded:
                g[r] = gl[r] - ci[rows.iq_lower_row_[r]];
                break;
            case NLPRowKind::Free:
                g[r] = std::numeric_limits<double>::quiet_NaN();
                break;
            }
        }
    }

    void read_structures(IndexVec &jac_rows, IndexVec &jac_cols, IndexVec &hess_rows,
                         IndexVec &hess_cols) const {
        jac_rows.resize(dims_.jacobian_nonzeros);
        jac_cols.resize(dims_.jacobian_nonzeros);
        if (dims_.jacobian_nonzeros > 0) {
            nlp_->jacobian_structure(jac_rows, jac_cols);
        }
        hess_rows.resize(dims_.hessian_nonzeros);
        hess_cols.resize(dims_.hessian_nonzeros);
        if (dims_.hessian_nonzeros > 0) {
            nlp_->hessian_structure(hess_rows, hess_cols);
        }
    }

    // THE PER-SOLVE RE-READ. Values and bounds may change between solves; the
    // dimensions, the structures and the row kinds may not. Every refusal is made
    // before anything is committed, so a refused solve leaves the Problem as it was.
    void refresh_nlp() {
        const NlpDimensions dims = nlp_->dimensions();
        if (dims.n != dims_.n || dims.m != dims_.m ||
            dims.jacobian_nonzeros != dims_.jacobian_nonzeros ||
            dims.hessian_nonzeros != dims_.hessian_nonzeros) {
            throw std::invalid_argument(fmt::format(
                "Problem '{}': the dimensions changed between solves (n {} -> {}, m {} -> {}, "
                "jacobian_nonzeros {} -> {}, hessian_nonzeros {} -> {}). Dimensions and "
                "structures are fixed for a Problem's lifetime; build a new Problem",
                nlp_->name(), dims_.n, dims.n, dims_.m, dims.m, dims_.jacobian_nonzeros,
                dims.jacobian_nonzeros, dims_.hessian_nonzeros, dims.hessian_nonzeros));
        }
        read_structures(scratch_jac_rows_, scratch_jac_cols_, scratch_hess_rows_,
                        scratch_hess_cols_);
        refuse_structure_change("Jacobian", nlp_model_->jacobian_rows(),
                                nlp_model_->jacobian_cols(), scratch_jac_rows_, scratch_jac_cols_);
        refuse_structure_change("Hessian", nlp_model_->hessian_rows(), nlp_model_->hessian_cols(),
                                scratch_hess_rows_, scratch_hess_cols_);

        scratch_x_lower_.resize(dims_.n);
        scratch_x_upper_.resize(dims_.n);
        scratch_g_lower_.resize(dims_.m);
        scratch_g_upper_.resize(dims_.m);
        nlp_->bounds(scratch_x_lower_, scratch_x_upper_, scratch_g_lower_, scratch_g_upper_);
        const bool unchanged = scratch_x_lower_ == nlp_model_->lower() &&
                               scratch_x_upper_ == nlp_model_->upper() &&
                               scratch_g_lower_ == nlp_model_->row_lower() &&
                               scratch_g_upper_ == nlp_model_->row_upper();
        if (!unchanged) {
            adopt_bounds();
        }
        nlp_model_->invalidate_cache();
        if (core_ != nullptr) {
            core_->invalidate_cache();
        }
    }

    void refuse_structure_change(const char *which, const Eigen::VectorXi &rows,
                                 const Eigen::VectorXi &cols, const IndexVec &new_rows,
                                 const IndexVec &new_cols) const {
        const Index k = first_difference(rows, cols, new_rows, new_cols);
        if (k >= 0) {
            throw std::invalid_argument(fmt::format(
                "Problem '{}': the {} structure changed between solves (slot {}: ({}, {}) -> "
                "({}, {})). Dimensions and structures are fixed for a Problem's lifetime; "
                "build a new Problem",
                nlp_->name(), which, k, rows[k], cols[k], new_rows[k], new_cols[k]));
        }
    }

    // A changed bound set. The SQP's model adopts the new values in place: its row
    // layout and multiplier map depend on each row's kind and not on a value, and
    // it refuses a kind flip (by name) before writing anything. The
    // interior-point program then takes one of two paths:
    //   - the variable box's STRUCTURE is unchanged (which sides are finite, which
    //     variables are fixed): the program adopts the values in place, with no
    //     re-lay and no re-analysis;
    //   - it changed: the program is dropped and re-laid on that engine's next
    //     solve, which re-analyses (counted).
    void adopt_bounds() {
        const bool box_changed = nlp_model_->update_bounds(scratch_x_lower_, scratch_x_upper_,
                                                           scratch_g_lower_, scratch_g_upper_);
        if (program_ != nullptr) {
            if (box_changed) {
                program_.reset();
                core_.reset();
            } else {
                core_->x_lower_ = nlp_model_->lower();
                core_->x_upper_ = nlp_model_->upper();
                program_->update_variable_bound_values(core_->x_lower_, core_->x_upper_);
            }
        }
        count_variables(dims_.n, nlp_model_->lower(), nlp_model_->upper(), statistics_);
    }

    Kind kind_;

    // Kind::kNlp: the problem as stated and the engines' transcriptions (the SQP's
    // model, which holds the last-read declaration, and the interior-point program
    // over it).
    std::shared_ptr<Nlp> nlp_;
    NlpDimensions dims_;
    std::shared_ptr<NlpProblemModel> nlp_model_;
    // Scratch for the per-solve re-read, kept so a re-read allocates nothing.
    IndexVec scratch_jac_rows_, scratch_jac_cols_, scratch_hess_rows_, scratch_hess_cols_;
    Vec scratch_x_lower_, scratch_x_upper_, scratch_g_lower_, scratch_g_upper_;

    // Kind::kModel / kAssembly: the internal SQP forms.
    std::shared_ptr<const NlpModel> model_;
    NlpModelAssembly *bridge_ = nullptr;

    // The interior-point program: built on first use (kNlp, kModel) or given.
    std::shared_ptr<NLPAdapterCore> core_;
    std::shared_ptr<NonLinearProgram> program_;

    ProblemStatistics statistics_;
};

// --- the internal routes ----------------------------------------------------

Problem problem_from_program(std::shared_ptr<NonLinearProgram> program) {
    return ProblemAccess::make(
        std::make_unique<ProblemState>(std::move(program), ProblemState::Kind::kProgram));
}

Problem problem_from_program(NonLinearProgram &program) {
    // Borrowed: an aliasing pointer with no control block owns nothing.
    return problem_from_program(
        std::shared_ptr<NonLinearProgram>(std::shared_ptr<void>(), &program));
}

Problem problem_from_model(std::shared_ptr<const NlpModel> model) {
    return ProblemAccess::make(std::make_unique<ProblemState>(std::move(model)));
}

Problem problem_from_model(const NlpModel &model) {
    return problem_from_model(std::shared_ptr<const NlpModel>(std::shared_ptr<void>(), &model));
}

Problem problem_from_assembly(NlpModelAssembly &bridge) {
    return ProblemAccess::make(std::make_unique<ProblemState>(bridge));
}

// --- the engines' door ------------------------------------------------------

Problem ProblemAccess::make(std::unique_ptr<ProblemState> state) noexcept {
    return Problem(std::move(state));
}

ProblemState &ProblemAccess::state(Problem &problem) {
    if (problem.state_ == nullptr) [[unlikely]] {
        throw_moved_from();
    }
    return *problem.state_;
}

const ProblemState &ProblemAccess::state(const Problem &problem) {
    if (problem.state_ == nullptr) [[unlikely]] {
        throw_moved_from();
    }
    return *problem.state_;
}

NonLinearProgram &ProblemAccess::ipm_program(Problem &problem) {
    return ProblemAccess::state(problem).ipm_program();
}

const NonLinearProgram *ProblemAccess::built_ipm_program(const Problem &problem) noexcept {
    return problem.state_ != nullptr ? problem.state_->built_ipm_program() : nullptr;
}

SqpTarget ProblemAccess::sqp_target(Problem &problem, bool exact_hessian) {
    return ProblemAccess::state(problem).sqp_target(exact_hessian);
}

void ProblemAccess::report(const Problem &problem, SolveResult &result) {
    ProblemAccess::state(problem).report(result);
}

const NlpProblemModel *ProblemAccess::nlp_model(const Problem &problem) noexcept {
    return problem.state_ != nullptr ? problem.state_->nlp_model() : nullptr;
}

} // namespace detail

// --- Problem ------------------------------------------------------------------

Problem::Problem(std::shared_ptr<Nlp> nlp)
    : state_(std::make_unique<detail::ProblemState>(std::move(nlp))) {}

Problem::Problem(AssemblyDeclaration pieces)
    : state_(std::make_unique<detail::ProblemState>(std::move(pieces))) {}

Problem::Problem(std::unique_ptr<detail::ProblemState> state) noexcept : state_(std::move(state)) {}

Problem::~Problem() = default;
Problem::Problem(Problem &&) noexcept = default;
Problem &Problem::operator=(Problem &&) noexcept = default;

Index Problem::num_variables() const noexcept {
    return state_ != nullptr ? state_->num_variables() : 0;
}

Index Problem::num_constraints() const noexcept {
    return state_ != nullptr ? state_->num_constraints() : 0;
}

const ProblemStatistics &Problem::statistics() const noexcept {
    static const ProblemStatistics kEmpty{};
    return state_ != nullptr ? state_->statistics() : kEmpty;
}

Index Problem::partitions() const noexcept { return state_ != nullptr ? state_->partitions() : 0; }

} // namespace hven::solvers
