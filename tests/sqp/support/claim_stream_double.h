// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

// A ClaimStreamSource whose claim stream is written by the test rather than
// derived from a model.
//
// The seam's boundary checks are about streams no in-tree provider can produce
// -- a coordinate named twice inside one domain, blocks that overlap or arrive
// out of order, a location past the end of the destination it addresses. A
// provider that lays its own stream correctly cannot exercise any of them, so
// the pins need a source whose stream is settable per test. This double
// computes nothing: it accepts every evaluation and writes recognizable
// markers, because what is under test is the lay, not the arithmetic.

#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include <Eigen/Core>

#include "hven/core/types.h"
// The declaration holds its piece lists BY VALUE, so a translation unit that
// constructs or destroys one needs the piece definitions in scope. This double
// has a declaration member, so it brings them itself rather than leaning on
// whatever its includer happens to have pulled in.
#include "hven/detail/interior/constraint_function.h"
#include "hven/detail/interior/objective_function.h"
#include "hven/model/claim_stream_source.h"
#include "hven/model/structure_identity.h"

namespace hven::sqp_tests {

/// The value every filled slot receives, distinct from the seam's own arena
/// seed so "this slot was written" is observable.
inline constexpr double kClaimStreamDoubleMarker = 3.25;

/// @brief A claim-stream provider whose stream a test sets directly.
class SettableClaimStreamSource final : public hven::solvers::ClaimStreamSource {
  public:
    /// @brief Declares the dimensions; the stream starts empty.
    ///
    /// @param primal_vars     declared primal variable count.
    /// @param equality_rows   declared equality row count.
    /// @param inequality_rows declared inequality row count.
    SettableClaimStreamSource(int primal_vars, int equality_rows, int inequality_rows) {
        declaration_.primal_vars_ = primal_vars;
        declaration_.equality_rows_ = equality_rows;
        declaration_.inequality_rows_ = inequality_rows;
        declaration_.partition_count_ = 1;
        objective_gradient_rows_.setLinSpaced(primal_vars, 0, primal_vars - 1);
        this->bump_structure_epoch();
    }

    /// @brief Installs a KKT claim stream and its three domain blocks.
    ///
    /// @param rows       assembled KKT row per claim slot.
    /// @param cols       assembled KKT column per claim slot.
    /// @param hessian    the Hessian domain's slot range.
    /// @param equality   the equality Jacobian domain's slot range.
    /// @param inequality the inequality Jacobian domain's slot range.
    void set_kkt_stream(const std::vector<int> &rows, const std::vector<int> &cols,
                        hven::solvers::ClaimBlock hessian, hven::solvers::ClaimBlock equality,
                        hven::solvers::ClaimBlock inequality) {
        kkt_claim_rows_ =
            Eigen::Map<const Eigen::VectorXi>(rows.data(), static_cast<Eigen::Index>(rows.size()));
        kkt_claim_cols_ =
            Eigen::Map<const Eigen::VectorXi>(cols.data(), static_cast<Eigen::Index>(cols.size()));
        hessian_ = hessian;
        equality_jacobian_ = equality;
        inequality_jacobian_ = inequality;
        this->bump_structure_epoch();
    }

    /// @brief Re-declares the row counts, as a re-lay a consumer must notice.
    ///
    /// @param equality_rows   the new equality row count.
    /// @param inequality_rows the new inequality row count.
    void set_row_counts(int equality_rows, int inequality_rows) {
        declaration_.equality_rows_ = equality_rows;
        declaration_.inequality_rows_ = inequality_rows;
        this->bump_structure_epoch();
    }

    /// @brief Installs the objective-gradient arena's rows.
    ///
    /// @param rows the assembled gradient row per claim slot of that arena.
    void set_objective_gradient_rows(const std::vector<int> &rows) {
        objective_gradient_rows_ =
            Eigen::Map<const Eigen::VectorXi>(rows.data(), static_cast<Eigen::Index>(rows.size()));
        this->bump_structure_epoch();
    }

    const hven::solvers::AggregateDeclaration &declaration() const override { return declaration_; }

    int negotiate_partition_count(int requested) override {
        if (requested < 1) {
            throw std::invalid_argument("SettableClaimStreamSource: partition count below 1");
        }
        declaration_.partition_count_ = 1;
        this->bump_structure_epoch();
        return 1;
    }

    int evaluation_threads() const override { return 1; }

    void set_evaluation_threads(int n) override {
        if (n < 1) {
            throw std::invalid_argument("SettableClaimStreamSource: thread count below 1");
        }
    }

    hven::solvers::ModelStructureKey model_structure_key() const override {
        hven::solvers::ModelStructureKey key;
        key.claim_digest_ =
            hven::solvers::claim_stream_digest(declaration_, kkt_claim_rows_, kkt_claim_cols_);
        key.partition_count_ = declaration_.partition_count_;
        key.bound_digest_ = hven::solvers::materialized_bound_digest(declaration_);
        return key;
    }

    hven::solvers::IdentityProbe probe_identity(hven::ConstVecRef x) override {
        double objective = 0.0;
        hven::Vec equality(declaration_.equality_rows_);
        hven::Vec inequality(declaration_.inequality_rows_);
        hven::solvers::CandidateValues values{objective, equality, inequality};
        const hven::Vec no_multipliers;
        this->evaluate_candidate_values(
            hven::solvers::CandidatePoint{x, no_multipliers, no_multipliers, 1.0}, values);
        return hven::solvers::IdentityProbe{this->structure_epoch(),
                                            hven::solvers::candidate_value_digest(values)};
    }

    // ClaimStreamSource overrides; the aggregate entries above forward to the
    // base's non-virtual validation, so only the hooks and accessors remain.
    Eigen::Ref<const Eigen::VectorXi> kkt_claim_rows() const override { return kkt_claim_rows_; }
    Eigen::Ref<const Eigen::VectorXi> kkt_claim_cols() const override { return kkt_claim_cols_; }
    hven::solvers::ClaimBlock hessian_claims() const override { return hessian_; }
    hven::solvers::ClaimBlock equality_jacobian_claims() const override {
        return equality_jacobian_;
    }
    hven::solvers::ClaimBlock inequality_jacobian_claims() const override {
        return inequality_jacobian_;
    }
    Eigen::Ref<const Eigen::VectorXi> objective_gradient_claim_rows() const override {
        return objective_gradient_rows_;
    }

  protected:
    void assemble_impl(const hven::solvers::CandidatePoint &point,
                       hven::solvers::EvalRequest request, hven::solvers::KktScatterView kkt,
                       hven::solvers::RhsScatterView rhs) override {
        (void)point;
        if (hven::solvers::has_request(request, hven::solvers::EvalRequest::kObjectiveValue)) {
            *rhs.objective_ += kClaimStreamDoubleMarker;
        }
        if (hven::solvers::has_request(request, hven::solvers::EvalRequest::kObjectiveGradient)) {
            fill_arena(rhs.objective_gradient_);
        }
        if (hven::solvers::has_request(request, hven::solvers::EvalRequest::kConstraintValues)) {
            fill_arena(rhs.equality_residuals_);
            fill_arena(rhs.inequality_residuals_);
        }
        if (hven::solvers::has_request(request,
                                       hven::solvers::EvalRequest::kConstraintAdjointGradient)) {
            fill_arena(rhs.constraint_adjoint_gradient_);
        }
        constexpr hven::solvers::EvalRequest kKktBearing =
            hven::solvers::EvalRequest::kObjectiveHessian |
            hven::solvers::EvalRequest::kConstraintJacobian |
            hven::solvers::EvalRequest::kConstraintAdjointHessian;
        if ((request & kKktBearing) != hven::solvers::EvalRequest::kNone) {
            for (int slot = 0; slot < kkt.locations_->size(); ++slot) {
                kkt.values_[kkt.locations_->location(slot)] += kClaimStreamDoubleMarker;
            }
        }
    }

    void evaluate_candidate_values_impl(const hven::solvers::CandidatePoint &point,
                                        hven::solvers::CandidateValues out) override {
        out.objective_ = point.x_.sum();
        out.equality_residuals_.setConstant(kClaimStreamDoubleMarker);
        out.inequality_residuals_.setConstant(kClaimStreamDoubleMarker);
    }

    void evaluate_candidate_first_order_impl(const hven::solvers::CandidatePoint &point,
                                             hven::solvers::CandidateFirstOrder out) override {
        this->evaluate_candidate_values_impl(point, out.values_);
        out.objective_gradient_.setConstant(kClaimStreamDoubleMarker);
        out.constraint_adjoint_gradient_.setConstant(kClaimStreamDoubleMarker);
    }

  private:
    static void fill_arena(const hven::solvers::RhsArenaView &view) {
        if (view.locations_ == nullptr) {
            return;
        }
        for (int slot = 0; slot < view.locations_->size(); ++slot) {
            const int row = view.locations_->location(slot);
            if (row >= 0) {
                view.values_[row] += kClaimStreamDoubleMarker;
            }
        }
    }

    hven::solvers::AggregateDeclaration declaration_;
    Eigen::VectorXi kkt_claim_rows_;
    Eigen::VectorXi kkt_claim_cols_;
    Eigen::VectorXi objective_gradient_rows_;
    hven::solvers::ClaimBlock hessian_;
    hven::solvers::ClaimBlock equality_jacobian_;
    hven::solvers::ClaimBlock inequality_jacobian_;
};

} // namespace hven::sqp_tests
