#include "support/fake_aggregate.h"

#include <fmt/format.h>

#include "hven/core/pattern_hash.h"
#include "hven/model/structure_identity.h"

namespace hven::model_tests {

namespace {

/// A request named this arena, so its view must be present. An arena the
/// request does NOT name may legally be empty; one it DOES name may not.
void require_arena(const hven::solvers::RhsArenaView &view, const char *arena) {
    if (view.empty()) {
        throw std::invalid_argument(fmt::format(
            "FakeAggregate::assemble: the request names the {0} arena, but its view is empty",
            arena));
    }
}

void fill_arena(const hven::solvers::RhsArenaView &view) {
    for (int slot = 0; slot < view.locations_->size(); ++slot) {
        const int row = view.locations_->location(slot);
        if (row >= 0) {
            view.values_[row] += kFillMarker;
        }
    }
}

void fill_kkt(const KktScatterView &kkt) {
    if (kkt.empty()) {
        throw std::invalid_argument("FakeAggregate::assemble: the request names a KKT arena, but "
                                    "its scatter view is empty");
    }
    for (int slot = 0; slot < kkt.locations_->size(); ++slot) {
        kkt.values_[kkt.locations_->location(slot)] += kFillMarker;
    }
}

} // namespace

void FakeAggregate::rekey() {
    hven::Fnv1a hash;
    hven::solvers::feed_dimensions(hash, declaration_.primal_vars_, declaration_.equality_rows_,
                                   declaration_.inequality_rows_);
    for (const auto &claim : claims_) {
        hven::solvers::feed_claim(hash, claim.first, claim.second);
    }
    key_.claim_digest_ = hash.value();
    key_.partition_count_ = adopted_partitions_;
    key_.bound_digest_ = hven::solvers::materialized_bound_digest(declaration_);
}

void FakeAggregate::assemble(const CandidatePoint &point, EvalRequest request, KktScatterView kkt,
                             RhsScatterView rhs) {
    // Both entry checks, in the order the contract states them.
    hven::solvers::validate_eval_request(request);
    if (hven::solvers::request_consumes_multipliers(request)) {
        hven::solvers::validate_full_multipliers(point, kEqualityRows, kInequalityRows);
    }
    this->record_evaluation();

    if (hven::solvers::has_request(request, EvalRequest::kObjectiveValue)) {
        if (rhs.objective_ == nullptr) {
            throw std::invalid_argument("FakeAggregate::assemble: the request names the objective "
                                        "value, but no out slot was supplied");
        }
        *rhs.objective_ += kFillMarker;
    }
    if (hven::solvers::has_request(request, EvalRequest::kObjectiveGradient)) {
        require_arena(rhs.objective_gradient_, "objective-gradient");
        fill_arena(rhs.objective_gradient_);
    }
    if (hven::solvers::has_request(request, EvalRequest::kConstraintValues)) {
        require_arena(rhs.equality_residuals_, "equality-residual");
        require_arena(rhs.inequality_residuals_, "inequality-residual");
        fill_arena(rhs.equality_residuals_);
        fill_arena(rhs.inequality_residuals_);
    }
    if (hven::solvers::has_request(request, EvalRequest::kConstraintAdjointGradient)) {
        require_arena(rhs.constraint_adjoint_gradient_, "constraint-adjoint-gradient");
        fill_arena(rhs.constraint_adjoint_gradient_);
    }
    constexpr EvalRequest kKktBearing = EvalRequest::kObjectiveHessian |
                                        EvalRequest::kConstraintJacobian |
                                        EvalRequest::kConstraintAdjointHessian;
    if ((request & kKktBearing) != EvalRequest::kNone) {
        fill_kkt(kkt);
    }
}

void FakeAggregate::evaluate_candidate_values(const CandidatePoint &point, CandidateValues out) {
    hven::solvers::validate_candidate_values(out, kEqualityRows, kInequalityRows);
    this->record_evaluation();
    values_calls_++;
    out.objective_ = static_cast<double>(point.x_.sum());
    out.equality_residuals_.setConstant(kFillMarker);
    out.inequality_residuals_.setConstant(kFillMarker);
}

void FakeAggregate::evaluate_candidate_first_order(const CandidatePoint &point,
                                                   CandidateFirstOrder out) {
    hven::solvers::validate_candidate_first_order(out, kPrimalVars, kEqualityRows, kInequalityRows);
    // A first-order evaluation always produces the constraint adjoint gradient,
    // so it always reads the multipliers.
    hven::solvers::validate_full_multipliers(point, kEqualityRows, kInequalityRows);
    this->evaluate_candidate_values(point, out.values_);
    out.objective_gradient_.setConstant(kFillMarker);
    out.constraint_adjoint_gradient_.setConstant(kFillMarker);
}

IdentityProbe FakeAggregate::probe_identity(ConstVecRef x) {
    double objective = 0.0;
    Vec equality(kEqualityRows);
    Vec inequality(kInequalityRows);
    CandidateValues values{objective, equality, inequality};
    const CandidatePoint point{x, empty_multipliers_, empty_multipliers_};
    this->evaluate_candidate_values(point, values);
    return IdentityProbe{this->structure_epoch(), hven::solvers::candidate_value_digest(values)};
}

} // namespace hven::model_tests
