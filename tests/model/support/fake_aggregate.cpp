#include "support/fake_aggregate.h"

#include "hven/core/pattern_hash.h"
#include "hven/model/structure_identity.h"

namespace hven::model_tests {

namespace {

/// Neither filler tests its view for presence, and that is the contract rather
/// than an omission: the non-virtual entry has already refused a request that
/// names a destination it was given nowhere to put. A hook only ever sees views
/// that are there for everything its request names.
void fill_arena(const hven::solvers::RhsArenaView &view) {
    for (int slot = 0; slot < view.locations_->size(); ++slot) {
        const int row = view.locations_->location(slot);
        if (row >= 0) {
            view.values_[row] += kFillMarker;
        }
    }
}

void fill_kkt(const KktScatterView &kkt) {
    for (int slot = 0; slot < kkt.locations_->size(); ++slot) {
        kkt.values_[kkt.locations_->location(slot)] += kFillMarker;
    }
}

} // namespace

void FakeAggregate::rekey() {
    // Both conjuncts through their one public builder each; the third is the
    // adopted partition count.
    key_.claim_digest_ = hven::solvers::claim_stream_digest(declaration_, claim_rows_, claim_cols_);
    key_.partition_count_ = adopted_partitions_;
    key_.bound_digest_ = hven::solvers::materialized_bound_digest(declaration_);
}

void FakeAggregate::assemble_impl(const CandidatePoint &point, EvalRequest request,
                                  KktScatterView kkt, RhsScatterView rhs) {
    // Not one check of its own, deliberately: the public entry has already
    // refused an unmapped request, a short-blocked point and a request naming a
    // destination it was handed nowhere to put -- and this fake is where "an
    // implementation cannot skip that" is under test.
    (void)point;
    this->record_evaluation();

    if (hven::solvers::has_request(request, EvalRequest::kObjectiveValue)) {
        *rhs.objective_ += kFillMarker;
    }
    if (hven::solvers::has_request(request, EvalRequest::kObjectiveGradient)) {
        fill_arena(rhs.objective_gradient_);
    }
    if (hven::solvers::has_request(request, EvalRequest::kConstraintValues)) {
        fill_arena(rhs.equality_residuals_);
        fill_arena(rhs.inequality_residuals_);
    }
    if (hven::solvers::has_request(request, EvalRequest::kConstraintAdjointGradient)) {
        fill_arena(rhs.constraint_adjoint_gradient_);
    }
    constexpr EvalRequest kKktBearing = EvalRequest::kObjectiveHessian |
                                        EvalRequest::kConstraintJacobian |
                                        EvalRequest::kConstraintAdjointHessian;
    if ((request & kKktBearing) != EvalRequest::kNone) {
        fill_kkt(kkt);
    }
}

void FakeAggregate::evaluate_candidate_values_impl(const CandidatePoint &point,
                                                   CandidateValues out) {
    this->record_evaluation();
    values_calls_++;
    out.objective_ = static_cast<double>(point.x_.sum());
    out.equality_residuals_.setConstant(kFillMarker);
    out.inequality_residuals_.setConstant(kFillMarker);
}

void FakeAggregate::evaluate_candidate_first_order_impl(const CandidatePoint &point,
                                                        CandidateFirstOrder out) {
    this->evaluate_candidate_values_impl(point, out.values_);
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
