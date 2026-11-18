// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

// A ClaimStreamSource whose claim stream is written by the test rather than
// derived from a model.
//
// The seam's boundary checks are about streams no in-tree provider can produce
// -- a coordinate named twice inside one domain, blocks that overlap, a
// location past the end of the destination it addresses. A provider that lays
// its own stream correctly cannot exercise any of them, so the pins need a
// source whose stream is settable per test. This double computes nothing: it
// accepts every evaluation and writes recognizable markers, because what is
// under test is the lay, not the arithmetic.

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
#include "hven/model/claim_stream_source.h"
#include "hven/model/constraint_function.h"
#include "hven/model/objective_function.h"
#include "hven/model/structure_identity.h"

namespace hven::sqp_tests {

/// The value every filled slot receives, distinct from the seam's own arena
/// seed so "this slot was written" is observable.
inline constexpr double kClaimStreamDoubleMarker = 3.25;

/// @brief A claim-stream provider whose stream a test sets directly.
class SettableClaimStreamSource final : public hven::ClaimStreamSource {
  public:
    /// @brief Declares the dimensions; the stream starts empty.
    ///
    /// @param primal_vars     declared primal variable count.
    /// @param equality_rows   declared equality row count.
    /// @param inequality_rows declared inequality row count.
    SettableClaimStreamSource(int primal_vars, int equality_rows, int inequality_rows) {
        declaration_.primal_vars = primal_vars;
        declaration_.equality_rows = equality_rows;
        declaration_.inequality_rows = inequality_rows;
        declaration_.partition_count = 1;
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
                        hven::ClaimBlock hessian, hven::ClaimBlock equality,
                        hven::ClaimBlock inequality) {
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
        declaration_.equality_rows = equality_rows;
        declaration_.inequality_rows = inequality_rows;
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

    const hven::AssemblyDeclaration &declaration() const override { return declaration_; }

    int negotiate_partition_count(int requested) override {
        if (requested < 1) {
            throw std::invalid_argument("SettableClaimStreamSource: partition count below 1");
        }
        declaration_.partition_count = 1;
        this->bump_structure_epoch();
        return 1;
    }

    int evaluation_threads() const override { return 1; }

    void set_evaluation_threads(int n) override {
        if (n < 1) {
            throw std::invalid_argument("SettableClaimStreamSource: thread count below 1");
        }
    }

    hven::ModelStructureKey model_structure_key() const override {
        hven::ModelStructureKey key;
        key.claim_digest =
            hven::claim_stream_digest(declaration_, kkt_claim_rows_, kkt_claim_cols_);
        key.partition_count = declaration_.partition_count;
        key.bound_digest = hven::materialized_bound_digest(declaration_);
        return key;
    }

    hven::IdentityProbe probe_identity(hven::ConstVecRef x) override {
        double objective = 0.0;
        hven::Vec equality(declaration_.equality_rows);
        hven::Vec inequality(declaration_.inequality_rows);
        hven::CandidateValues values{objective, equality, inequality};
        const hven::Vec no_multipliers;
        this->evaluate_candidate_values(
            hven::CandidatePoint{x, no_multipliers, no_multipliers, 1.0}, values);
        return hven::IdentityProbe{this->structure_epoch(), hven::candidate_value_digest(values)};
    }

    // ClaimStreamSource overrides; the aggregate entries above forward to the
    // base's non-virtual validation, so only the hooks and accessors remain.
    Eigen::Ref<const Eigen::VectorXi> kkt_claim_rows() const override { return kkt_claim_rows_; }
    Eigen::Ref<const Eigen::VectorXi> kkt_claim_cols() const override { return kkt_claim_cols_; }
    hven::ClaimBlock hessian_claims() const override { return hessian_; }
    hven::ClaimBlock equality_jacobian_claims() const override { return equality_jacobian_; }
    hven::ClaimBlock inequality_jacobian_claims() const override { return inequality_jacobian_; }
    Eigen::Ref<const Eigen::VectorXi> objective_gradient_claim_rows() const override {
        return objective_gradient_rows_;
    }

  protected:
    void assemble_impl(const hven::CandidatePoint &point, hven::EvalRequest request,
                       hven::KktScatterView kkt, hven::RhsScatterView rhs) override {
        (void)point;
        if (hven::has_request(request, hven::EvalRequest::kObjectiveValue)) {
            *rhs.objective += kClaimStreamDoubleMarker;
        }
        if (hven::has_request(request, hven::EvalRequest::kObjectiveGradient)) {
            fill_arena(rhs.objective_gradient);
        }
        if (hven::has_request(request, hven::EvalRequest::kConstraintValues)) {
            fill_arena(rhs.equality_residuals);
            fill_arena(rhs.inequality_residuals);
        }
        if (hven::has_request(request, hven::EvalRequest::kConstraintAdjointGradient)) {
            fill_arena(rhs.constraint_adjoint_gradient);
        }
        constexpr hven::EvalRequest kKktBearing = hven::EvalRequest::kObjectiveHessian |
                                                  hven::EvalRequest::kConstraintJacobian |
                                                  hven::EvalRequest::kConstraintAdjointHessian;
        if ((request & kKktBearing) != hven::EvalRequest::kNone) {
            for (int slot = 0; slot < kkt.locations->size(); ++slot) {
                kkt.values[kkt.locations->location(slot)] += kClaimStreamDoubleMarker;
            }
        }
    }

    void evaluate_candidate_values_impl(const hven::CandidatePoint &point,
                                        hven::CandidateValues out) override {
        out.objective = point.x.sum();
        out.equality_residuals.setConstant(kClaimStreamDoubleMarker);
        out.inequality_residuals.setConstant(kClaimStreamDoubleMarker);
    }

    void evaluate_candidate_first_order_impl(const hven::CandidatePoint &point,
                                             hven::CandidateFirstOrder out) override {
        this->evaluate_candidate_values_impl(point, out.values);
        out.objective_gradient.setConstant(kClaimStreamDoubleMarker);
        out.constraint_adjoint_gradient.setConstant(kClaimStreamDoubleMarker);
    }

  private:
    static void fill_arena(const hven::RhsArenaView &view) {
        if (view.locations == nullptr) {
            return;
        }
        for (int slot = 0; slot < view.locations->size(); ++slot) {
            const int row = view.locations->location(slot);
            if (row >= 0) {
                view.values[row] += kClaimStreamDoubleMarker;
            }
        }
    }

    hven::AssemblyDeclaration declaration_;
    Eigen::VectorXi kkt_claim_rows_;
    Eigen::VectorXi kkt_claim_cols_;
    Eigen::VectorXi objective_gradient_rows_;
    hven::ClaimBlock hessian_;
    hven::ClaimBlock equality_jacobian_;
    hven::ClaimBlock inequality_jacobian_;
};

} // namespace hven::sqp_tests
