// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

// The NlpModel bridge: its claim pass, its evaluation hooks, and the boundary
// checks a bridge owes on a model implementation it did not write.

#include "hven/model/nlp_model_aggregate.h"

#include <limits>
#include <stdexcept>
#include <utility>

#include <fmt/format.h>

#include "hven/model/claim_space.h"
#include "hven/model/structure_identity.h"

namespace hven::solvers {

namespace {

/// Narrows one of the model's dimensions to the int the declaration carries.
int to_declared_count(Index value, const char *what) {
    if (value < 0 || value > static_cast<Index>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument(
            fmt::format("NlpModelAggregate: the model reports {0} = {1}, which is not a count this "
                        "declaration can carry (0 to {2})",
                        what, value, std::numeric_limits<int>::max()));
    }
    return static_cast<int>(value);
}

/// Claims one KKT slot per stored element of @p matrix, at the space's current
/// row offset.
void claim_matrix(const SpMatRM &matrix, KktClaimSpace &space) {
    for (int outer = 0; outer < static_cast<int>(matrix.outerSize()); ++outer) {
        for (SpMatRM::InnerIterator it(matrix, outer); it; ++it) {
            space.rows_[space.next_free_] =
                space.constraint_row_offset_ + static_cast<int>(it.row());
            space.cols_[space.next_free_] = static_cast<int>(it.col());
            space.next_free_++;
        }
    }
}

/// Rejects a Hessian return that is not the upper triangle the model's own
/// contract promises.
///
/// One comparison per stored element, at claim time only. The claims record the
/// triangle verbatim and the storage state is resolved once from it, so an entry
/// below the diagonal would put a claim in a triangle the assembled matrix does
/// not hold -- checked here rather than trusted.
void require_upper_triangle(const SpMatRM &hessian) {
    for (int outer = 0; outer < static_cast<int>(hessian.outerSize()); ++outer) {
        for (SpMatRM::InnerIterator it(hessian, outer); it; ++it) {
            if (it.row() > it.col()) {
                throw std::invalid_argument(fmt::format(
                    "NlpModelAggregate: eval_hess stored an entry at (row {0}, column {1}), below "
                    "the diagonal. nlp_model.h states the Hessian return as the upper triangle "
                    "only",
                    it.row(), it.col()));
            }
        }
    }
}

/// Claims one slot per row of an arena, in row order.
void claim_arena_rows(RhsClaimSpace &space, int rows) {
    for (int row = 0; row < rows; ++row) {
        space.rows_[space.next_free_] = row;
        space.next_free_++;
    }
}

/// Rejects an arena view that does not describe the claims this provider laid.
///
/// The implementation half of the two-boundary split: an entry validates
/// contract-visible facts, and an implementation validates laid-width facts. Two
/// comparisons, the storage and the published table, both against the claim
/// count this bridge laid into that arena -- the fill walks the table and writes
/// target[row] per claim, so a view of any other length is addressed off its
/// end.
///
/// An arena with no claims is skipped: a model with no rows of one kind is
/// handed a legally default view for that block.
void require_arena(const RhsArenaView &view, int claims, const char *arena) {
    if (claims == 0) {
        return;
    }
    if (view.size_ != claims) {
        throw std::invalid_argument(
            fmt::format("assemble: the {0} arena's view is {1} rows, but this provider laid its "
                        "{0} over {2} rows",
                        arena, view.size_, claims));
    }
    if (view.locations_->size() != claims) {
        throw std::invalid_argument(
            fmt::format("assemble: the {0} arena's location table has {1} claim slots, but this "
                        "provider laid {2} claims into that arena",
                        arena, view.locations_->size(), claims));
    }
}

/// Rejects a KKT view whose table does not cover this provider's claim stream.
void require_kkt_table(const KktScatterView &kkt, int claims) {
    if (kkt.locations_->size() != claims) {
        throw std::invalid_argument(
            fmt::format("assemble: the KKT location table has {0} claim slots, but this provider "
                        "laid {1} claims",
                        kkt.locations_->size(), claims));
    }
}

/// Rejects a sparse return whose nonzero count contradicts the claim pass.
///
/// A model owes an invariant sparsity pattern (nlp_model.h). The full pattern is
/// not re-derived per evaluation -- that would spend the fill's own budget on
/// re-checking a precondition -- so this one comparison is scoped accordingly:
/// it catches a pattern that collapsed or grew between the claim pass and this
/// call, and it does not catch a same-count permutation of the same entries.
/// That residual is the storage-order stability the model's own invariance
/// precondition already binds, not a gap this check leaves open.
void require_claimed_nonzeros(const SpMatRM &matrix, int claims, const char *what) {
    if (static_cast<int>(matrix.nonZeros()) != claims) {
        throw std::invalid_argument(fmt::format(
            "NlpModelAggregate: the model returned {0} with {1} stored elements, but the claim "
            "pass laid {2} slots for it. This model's sparsity pattern is not invariant, which "
            "nlp_model.h requires of every implementer",
            what, matrix.nonZeros(), claims));
    }
}

/// Rejects a value block the model sized differently from what it declares.
void require_block_size(Eigen::Index actual, int declared, const char *what) {
    if (actual != declared) {
        throw std::invalid_argument(
            fmt::format("NlpModelAggregate: the model returned {0} with {1} rows, but it declares "
                        "{2} of them",
                        what, actual, declared));
    }
}

/// Sums a matrix's stored values into the KKT destination through the claim
/// block laid for it, in the same storage order the claim pass walked.
void scatter_matrix(const SpMatRM &matrix, const ClaimBlock &block, const KktScatterView &kkt) {
    int slot = block.start_;
    for (int outer = 0; outer < static_cast<int>(matrix.outerSize()); ++outer) {
        for (SpMatRM::InnerIterator it(matrix, outer); it; ++it, ++slot) {
            kkt.values_[kkt.locations_->location(slot)] += it.value();
        }
    }
}

/// Sums a vector into an arena through its published table. Claim slot k carries
/// row k, so the value for a slot is the vector's own entry at that index.
void scatter_arena(const RhsArenaView &view, const Vec &values) {
    for (int slot = 0; slot < view.locations_->size(); ++slot) {
        const int row = view.locations_->location(slot);
        if (row >= 0) {
            view.values_[row] += values[slot];
        }
    }
}

/// Adds a Jacobian's adjoint contribution, J^T lambda, into @p out.
void accumulate_adjoint(const SpMatRM &jacobian, const Vec &multipliers, Vec &out) {
    for (int outer = 0; outer < static_cast<int>(jacobian.outerSize()); ++outer) {
        for (SpMatRM::InnerIterator it(jacobian, outer); it; ++it) {
            out[it.col()] += it.value() * multipliers[it.row()];
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Construction and layout
// ---------------------------------------------------------------------------

NlpModelAggregate::NlpModelAggregate(std::shared_ptr<NlpModel> model) : model_(std::move(model)) {
    if (model_ == nullptr) {
        throw std::invalid_argument("NlpModelAggregate: the model is null");
    }

    primal_vars_ = to_declared_count(model_->n(), "n()");
    equality_rows_ = to_declared_count(model_->me(), "me()");
    inequality_rows_ = to_declared_count(model_->mi(), "mi()");
    if (primal_vars_ < 1) {
        throw std::invalid_argument(fmt::format(
            "NlpModelAggregate: the model reports n() = {0}; an aggregate with no primal variables "
            "is not a problem",
            primal_vars_));
    }

    this->relay(1);
}

NlpModelAggregate::LaidStructures NlpModelAggregate::lay(int partition_count) const {
    LaidStructures laid;

    laid.declaration_.primal_vars_ = primal_vars_;
    laid.declaration_.equality_rows_ = equality_rows_;
    laid.declaration_.inequality_rows_ = inequality_rows_;
    laid.declaration_.partition_count_ = partition_count;

    // The model's bounds, one record per variable, verbatim. The model's own
    // "effectively infinite" spelling (nlp_model.h names +/-1e20) is a numeric
    // convention of the model's, and reinterpreting it here would put a
    // threshold of this bridge's invention into a declared structure.
    const Vec &lower = model_->lower();
    const Vec &upper = model_->upper();
    require_block_size(lower.size(), primal_vars_, "lower()");
    require_block_size(upper.size(), primal_vars_, "upper()");
    laid.declaration_.variable_bounds_.reserve(static_cast<std::size_t>(primal_vars_));
    for (int index = 0; index < primal_vars_; ++index) {
        laid.declaration_.variable_bounds_.push_back(
            VariableBound{index, lower[index], upper[index]});
    }

    // Validated at the contract's own boundary before anything is laid over it,
    // exactly as every provider does. The bridge declares no pieces, so the
    // piece-sum conjunct is vacuous here and every other check applies.
    laid.declaration_.validate();

    // The patterns, walked once at the model's own start point. Which point is
    // immaterial by the model's invariance precondition; the start point is the
    // one point every model is required to be able to produce.
    const Vec x = model_->start_point();
    require_block_size(x.size(), primal_vars_, "start_point()");
    const Vec zero_equality = Vec::Zero(equality_rows_);
    const Vec zero_inequality = Vec::Zero(inequality_rows_);

    const SpMatRM hessian = model_->eval_hess(x, 1.0, zero_equality, zero_inequality);
    const SpMatRM equality_jacobian =
        equality_rows_ > 0 ? model_->eval_jac_e(x) : SpMatRM(0, primal_vars_);
    const SpMatRM inequality_jacobian =
        inequality_rows_ > 0 ? model_->eval_jac_i(x) : SpMatRM(0, primal_vars_);

    const int hessian_claims = static_cast<int>(hessian.nonZeros());
    const int equality_claims = static_cast<int>(equality_jacobian.nonZeros());
    const int inequality_claims = static_cast<int>(inequality_jacobian.nonZeros());
    const int total_claims = hessian_claims + equality_claims + inequality_claims;

    laid.kkt_dimension_ = primal_vars_ + equality_rows_ + inequality_rows_;
    laid.kkt_claim_rows_.setZero(total_claims);
    laid.kkt_claim_cols_.setZero(total_claims);

    // The claim seam in the contract's own spelling: one cursor, one row base
    // per kind, the domains being claimed, and the storage state the assembled
    // matrix is in. The model returns its Hessian as an upper triangle, so that
    // is the state resolved here and never branched on again.
    KktClaimSpace space{laid.kkt_claim_rows_, laid.kkt_claim_cols_,      0, 0,
                        ClaimDomainSet(),     KktStorage::kUpperTriangle};

    space.domains_ = ClaimDomain::kHessian;
    space.constraint_row_offset_ = 0;
    laid.hessian_ = ClaimBlock{space.next_free_, hessian_claims};
    require_upper_triangle(hessian);
    claim_matrix(hessian, space);

    space.domains_ = ClaimDomain::kEqualityJacobian;
    space.constraint_row_offset_ = primal_vars_;
    laid.equality_jacobian_ = ClaimBlock{space.next_free_, equality_claims};
    claim_matrix(equality_jacobian, space);

    space.domains_ = ClaimDomain::kInequalityJacobian;
    space.constraint_row_offset_ = primal_vars_ + equality_rows_;
    laid.inequality_jacobian_ = ClaimBlock{space.next_free_, inequality_claims};
    claim_matrix(inequality_jacobian, space);

    // The four right-hand-side arenas. One serial piece claims each arena whole,
    // in row order, so a claim slot and its row are the same number -- there is
    // no elimination here and no partition order to interleave.
    laid.objective_gradient_rows_.setZero(primal_vars_);
    laid.adjoint_gradient_rows_.setZero(primal_vars_);
    laid.equality_residual_rows_.setZero(equality_rows_);
    laid.inequality_residual_rows_.setZero(inequality_rows_);

    RhsClaimSpace objective_gradient{laid.objective_gradient_rows_, 0,
                                     RhsArena::kObjectiveGradient};
    claim_arena_rows(objective_gradient, primal_vars_);
    RhsClaimSpace adjoint_gradient{laid.adjoint_gradient_rows_, 0,
                                   RhsArena::kConstraintAdjointGradient};
    claim_arena_rows(adjoint_gradient, primal_vars_);
    RhsClaimSpace equality_residuals{laid.equality_residual_rows_, 0, RhsArena::kEqualityResiduals};
    claim_arena_rows(equality_residuals, equality_rows_);
    RhsClaimSpace inequality_residuals{laid.inequality_residual_rows_, 0,
                                       RhsArena::kInequalityResiduals};
    claim_arena_rows(inequality_residuals, inequality_rows_);

    // Each conjunct through its one public builder. materialize_variable_bounds
    // runs inside the bound digest, so a bound history that does not describe a
    // problem fails here, before anything is committed.
    laid.key_.claim_digest_ =
        claim_stream_digest(laid.declaration_, laid.kkt_claim_rows_, laid.kkt_claim_cols_);
    laid.key_.partition_count_ = partition_count;
    laid.key_.bound_digest_ = materialized_bound_digest(laid.declaration_);

    return laid;
}

void NlpModelAggregate::relay(int partition_count) {
    // Built whole, then committed. A lay that throws -- a model that refuses to
    // evaluate, a bound history that intersects to nothing -- leaves the
    // structures on hand exactly as they were, so nothing was re-laid and there
    // is no structural event to report.
    LaidStructures built = this->lay(partition_count);
    laid_ = std::move(built);
    this->bump_structure_epoch();
}

int NlpModelAggregate::negotiate_partition_count(int requested) {
    if (requested < 1) {
        throw std::invalid_argument(fmt::format(
            "NlpModelAggregate: a partition count must be at least 1 (got {0})", requested));
    }
    // One serial piece is one partition's worth of work. The cap is reported
    // through the return value; the re-lay is unconditional because the contract
    // makes a renegotiation a structural event whether or not it moves a claim.
    const int adopted = 1;
    this->relay(adopted);
    return adopted;
}

void NlpModelAggregate::set_evaluation_threads(int n) {
    if (n < 1) {
        throw std::invalid_argument(fmt::format(
            "NlpModelAggregate: an evaluation thread count must be at least 1 (got {0})", n));
    }
}

// ---------------------------------------------------------------------------
// Staging and the model calls
// ---------------------------------------------------------------------------

const Vec &NlpModelAggregate::stage_point(const CandidatePoint &point, bool with_multipliers) {
    x_scratch_ = point.x_;
    if (with_multipliers) {
        equality_multiplier_scratch_ = point.equality_multipliers_;
        inequality_multiplier_scratch_ = point.inequality_multipliers_;
    }
    return x_scratch_;
}

void NlpModelAggregate::evaluate_values(const Vec &x, double &objective) {
    model_->eval_values(x, objective, equality_residual_scratch_, inequality_residual_scratch_);
    require_block_size(equality_residual_scratch_.size(), equality_rows_, "eval_values' cE block");
    require_block_size(inequality_residual_scratch_.size(), inequality_rows_,
                       "eval_values' cI block");
}

void NlpModelAggregate::evaluate_constraint_values(const Vec &x) {
    // The same skip eval_values applies: a block the model declares no rows for
    // is not evaluated.
    equality_residual_scratch_ = equality_rows_ > 0 ? model_->eval_ce(x) : Vec(0);
    inequality_residual_scratch_ = inequality_rows_ > 0 ? model_->eval_ci(x) : Vec(0);
    require_block_size(equality_residual_scratch_.size(), equality_rows_, "eval_ce");
    require_block_size(inequality_residual_scratch_.size(), inequality_rows_, "eval_ci");
}

void NlpModelAggregate::evaluate_jacobians(const Vec &x) {
    // Gated on the declared row counts, never on the claim counts. A constraint
    // block that has rows but whose Jacobian is all structural zeros -- a
    // constant constraint -- claims nothing, and gating on claims would silently
    // skip a callback the request named. The block is evaluated because it has
    // rows; the fill then writes nothing because it claimed nothing.
    if (equality_rows_ > 0) {
        equality_jacobian_scratch_ = model_->eval_jac_e(x);
        require_claimed_nonzeros(equality_jacobian_scratch_, laid_.equality_jacobian_.count_,
                                 "eval_jac_e");
    }
    if (inequality_rows_ > 0) {
        inequality_jacobian_scratch_ = model_->eval_jac_i(x);
        require_claimed_nonzeros(inequality_jacobian_scratch_, laid_.inequality_jacobian_.count_,
                                 "eval_jac_i");
    }
}

void NlpModelAggregate::compose_adjoint_gradient() {
    adjoint_scratch_.setZero(primal_vars_);
    if (equality_rows_ > 0) {
        accumulate_adjoint(equality_jacobian_scratch_, equality_multiplier_scratch_,
                           adjoint_scratch_);
    }
    if (inequality_rows_ > 0) {
        accumulate_adjoint(inequality_jacobian_scratch_, inequality_multiplier_scratch_,
                           adjoint_scratch_);
    }
}

// ---------------------------------------------------------------------------
// The contract's evaluation hooks
// ---------------------------------------------------------------------------

void NlpModelAggregate::assemble_impl(const CandidatePoint &point, EvalRequest request,
                                      KktScatterView kkt, RhsScatterView rhs) {
    const bool want_objective = has_request(request, EvalRequest::kObjectiveValue);
    const bool want_gradient = has_request(request, EvalRequest::kObjectiveGradient);
    const bool want_constraints = has_request(request, EvalRequest::kConstraintValues);
    const bool want_adjoint_gradient =
        has_request(request, EvalRequest::kConstraintAdjointGradient);
    const bool want_jacobian = has_request(request, EvalRequest::kConstraintJacobian);
    const bool want_objective_hessian = has_request(request, EvalRequest::kObjectiveHessian);
    constexpr EvalRequest kHessianBearing =
        EvalRequest::kObjectiveHessian | EvalRequest::kConstraintAdjointHessian;
    const bool want_hessian = (request & kHessianBearing) != EvalRequest::kNone;

    // The laid-width half of the destination checks, plus the table each named
    // arena is addressed through. The entry established presence and, where the
    // contract knows a length, that length; what it cannot see is whether a
    // table describes this provider's claims.
    if (want_gradient) {
        require_arena(rhs.objective_gradient_, primal_vars_, "objective gradient");
    }
    if (want_adjoint_gradient) {
        require_arena(rhs.constraint_adjoint_gradient_, primal_vars_,
                      "constraint adjoint gradient");
    }
    if (want_constraints) {
        require_arena(rhs.equality_residuals_, equality_rows_, "equality residual");
        require_arena(rhs.inequality_residuals_, inequality_rows_, "inequality residual");
    }
    if (want_jacobian || want_hessian) {
        require_kkt_table(kkt, static_cast<int>(laid_.kkt_claim_rows_.size()));
    }

    const Vec &x = this->stage_point(point, want_adjoint_gradient || want_hessian);
    const double scale = point.objective_scale_;

    // The model evaluators this request needs, and no others. The values half
    // splits three ways: both value kinds together go through eval_values, which
    // is the model's own one-call path for exactly that pair; either alone goes
    // through its own evaluator, because eval_values would compute the block the
    // request did not name.
    double objective = 0.0;
    if (want_objective && want_constraints) {
        this->evaluate_values(x, objective);
    } else if (want_objective) {
        objective = model_->eval_f(x);
    } else if (want_constraints) {
        this->evaluate_constraint_values(x);
    }
    if (want_gradient) {
        gradient_scratch_ = model_->eval_grad(x);
        require_block_size(gradient_scratch_.size(), primal_vars_, "eval_grad");
    }
    if (want_jacobian || want_adjoint_gradient) {
        this->evaluate_jacobians(x);
    }
    if (want_hessian) {
        // One call serves both Hessian flags: the model's surface is the exact
        // Lagrangian Hessian, objective and constraint blocks composed. A shape
        // naming only the adjoint half asks for it at obj_scale 0, which
        // nlp_model.h defines as the objective block dropped to a structural
        // zero.
        hessian_scratch_ =
            model_->eval_hess(x, want_objective_hessian ? scale : 0.0, equality_multiplier_scratch_,
                              inequality_multiplier_scratch_);
        require_claimed_nonzeros(hessian_scratch_, laid_.hessian_.count_, "eval_hess");
    }

    // Accumulated into, never assigned: the consumer owns its storage and its
    // initial state.
    if (want_objective) {
        *rhs.objective_ += scale * objective;
    }
    if (want_gradient) {
        gradient_scratch_ *= scale;
        scatter_arena(rhs.objective_gradient_, gradient_scratch_);
    }
    if (want_constraints) {
        if (equality_rows_ > 0) {
            scatter_arena(rhs.equality_residuals_, equality_residual_scratch_);
        }
        if (inequality_rows_ > 0) {
            scatter_arena(rhs.inequality_residuals_, inequality_residual_scratch_);
        }
    }
    if (want_adjoint_gradient) {
        this->compose_adjoint_gradient();
        scatter_arena(rhs.constraint_adjoint_gradient_, adjoint_scratch_);
    }
    if (want_jacobian) {
        // Row-gated for the same reason the evaluation is: a block with rows and
        // no claims scatters nothing, and a scatter over its empty pattern is
        // the loop that writes nothing.
        if (equality_rows_ > 0) {
            scatter_matrix(equality_jacobian_scratch_, laid_.equality_jacobian_, kkt);
        }
        if (inequality_rows_ > 0) {
            scatter_matrix(inequality_jacobian_scratch_, laid_.inequality_jacobian_, kkt);
        }
    }
    if (want_hessian) {
        scatter_matrix(hessian_scratch_, laid_.hessian_, kkt);
    }
}

void NlpModelAggregate::evaluate_candidate_values_impl(const CandidatePoint &point,
                                                       CandidateValues out) {
    double objective = 0.0;
    this->evaluate_values(this->stage_point(point, false), objective);

    // Assigned, per the candidate surface's own discipline: a scorer holds a
    // scratch buffer and wants the values at a point, not a running sum.
    out.objective_ = point.objective_scale_ * objective;
    out.equality_residuals_ = equality_residual_scratch_;
    out.inequality_residuals_ = inequality_residual_scratch_;
}

void NlpModelAggregate::evaluate_candidate_first_order_impl(const CandidatePoint &point,
                                                            CandidateFirstOrder out) {
    const Vec &x = this->stage_point(point, true);

    double objective = 0.0;
    this->evaluate_values(x, objective);
    out.values_.objective_ = point.objective_scale_ * objective;
    out.values_.equality_residuals_ = equality_residual_scratch_;
    out.values_.inequality_residuals_ = inequality_residual_scratch_;

    gradient_scratch_ = model_->eval_grad(x);
    require_block_size(gradient_scratch_.size(), primal_vars_, "eval_grad");
    out.objective_gradient_ = point.objective_scale_ * gradient_scratch_;

    this->evaluate_jacobians(x);
    this->compose_adjoint_gradient();
    out.constraint_adjoint_gradient_ = adjoint_scratch_;
}

IdentityProbe NlpModelAggregate::probe_identity(ConstVecRef x) {
    probe_equality_scratch_.setZero(equality_rows_);
    probe_inequality_scratch_.setZero(inequality_rows_);

    double objective = 0.0;
    CandidateValues values{objective, probe_equality_scratch_, probe_inequality_scratch_};
    const Vec no_multipliers;
    this->evaluate_candidate_values(CandidatePoint{x, no_multipliers, no_multipliers}, values);

    return IdentityProbe{this->structure_epoch(), candidate_value_digest(values)};
}

} // namespace hven::solvers
