// =============================================================================
// New file in Tycho, carried into hven (Copyright 2026-present Grant R. Hecht,
//   Apache 2.0 — see LICENSE.txt)
// =============================================================================

#include "hven/detail/model/nlp_adapter.h"

#include <cmath>
#include <limits>

#include <fmt/format.h>

#include "hven/model/non_linear_program.h"

namespace hven::solvers {

namespace {

/// Narrows one of the model's dimensions to the int this host indexes with.
int to_host_count(Index value, const char *what, const std::string &name) {
    if (value < 0 || value > static_cast<Index>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument(
            fmt::format("{}: the model reports {} = {}, which is not a count this host can carry "
                        "(0 to {})",
                        name, what, value, std::numeric_limits<int>::max()));
    }
    return static_cast<int>(value);
}

/// Rejects a return whose shape is not the one the model declared.
void require_dimensions(const SpMatRM &matrix, int rows, int cols, const char *what,
                        const std::string &name) {
    if (matrix.rows() != rows || matrix.cols() != cols) {
        throw std::invalid_argument(
            fmt::format("{}: {} returned a {}x{} matrix, but the model declares it {}x{}", name,
                        what, matrix.rows(), matrix.cols(), rows, cols));
    }
}

/// Rejects a vector return whose length is not the one the model declared.
void require_length(Index actual, int expected, const char *what, const std::string &name) {
    if (actual != expected) {
        throw std::invalid_argument(fmt::format(
            "{}: {} returned {} rows, but the model declares {}", name, what, actual, expected));
    }
}

/// Records a matrix's stored coordinates, in storage order, as the claim order
/// every later evaluation of that matrix is checked against.
std::vector<NLPCoordinate> record_coordinates(const SpMatRM &matrix) {
    std::vector<NLPCoordinate> recorded;
    recorded.reserve(static_cast<std::size_t>(matrix.nonZeros()));
    for (int outer = 0; outer < static_cast<int>(matrix.outerSize()); outer++) {
        for (SpMatRM::InnerIterator it(matrix, outer); it; ++it) {
            recorded.push_back(
                NLPCoordinate{static_cast<int>(it.row()), static_cast<int>(it.col())});
        }
    }
    return recorded;
}

/// Rejects a return whose storage is not compressed. Every consumer of these
/// matrices pairs stored value k with the kth coordinate the claim pass
/// recorded, and uncompressed storage leaves gaps in the value array, so a
/// matrix naming every claimed coordinate would still hand back the wrong
/// number for each. Checked at the claim walk and again at every evaluation.
void require_compressed(const SpMatRM &matrix, const char *what, const std::string &name) {
    if (!matrix.isCompressed()) {
        throw std::invalid_argument(
            fmt::format("{}: {} returned an uncompressed matrix. nlp_model.h states the three "
                        "matrix returns as compressed: one contiguous value array in canonical "
                        "order, which is what pairs a stored value with its claimed coordinate",
                        name, what));
    }
}

/// Rejects a Hessian return that is not the upper triangle the model's own
/// contract promises. The claims record the triangle verbatim, so an entry
/// below the diagonal would claim a slot in a triangle the assembled matrix
/// does not hold.
void require_upper_triangle(const SpMatRM &hessian, const std::string &name) {
    for (int outer = 0; outer < static_cast<int>(hessian.outerSize()); outer++) {
        for (SpMatRM::InnerIterator it(hessian, outer); it; ++it) {
            if (it.row() > it.col()) {
                throw std::invalid_argument(fmt::format(
                    "{}: eval_hess stored an entry at (row {}, column {}), below the diagonal. "
                    "nlp_model.h states the Hessian return as the upper triangle only",
                    name, it.row(), it.col()));
            }
        }
    }
}

} // namespace

void nlp_require_claimed_pattern(const SpMatRM &matrix, const std::vector<NLPCoordinate> &recorded,
                                 const char *what, const std::string &name) {
    // Compressed first, because the pairing this function certifies is between
    // stored value k and recorded coordinate k.
    require_compressed(matrix, what, name);
    if (static_cast<std::size_t>(matrix.nonZeros()) != recorded.size()) {
        throw std::invalid_argument(fmt::format(
            "{}: {} returned {} stored entries, but {} slots were claimed for it. The model's "
            "sparsity pattern must not vary between evaluations",
            name, what, matrix.nonZeros(), recorded.size()));
    }
    std::size_t slot = 0;
    for (int outer = 0; outer < static_cast<int>(matrix.outerSize()); outer++) {
        for (SpMatRM::InnerIterator it(matrix, outer); it; ++it) {
            const int row = static_cast<int>(it.row());
            const int col = static_cast<int>(it.col());
            if (recorded[slot].row_ != row || recorded[slot].col_ != col) {
                throw std::invalid_argument(fmt::format(
                    "{}: {} presented ({}, {}) at slot {}, which was claimed for ({}, {}). The "
                    "model's sparsity pattern must not vary between evaluations",
                    name, what, row, col, slot, recorded[slot].row_, recorded[slot].col_));
            }
            slot++;
        }
    }
}

NLPAdapterCore::NLPAdapterCore(std::shared_ptr<NlpModel> model, std::string name)
    : model_(std::move(model)), name_(std::move(name)) {
    if (!model_) {
        throw std::invalid_argument("NLPAdapterCore: the model pointer is null");
    }
    n_ = to_host_count(model_->n(), "n()", name_);
    num_eq_ = to_host_count(model_->me(), "me()", name_);
    num_iq_ = to_host_count(model_->mi(), "mi()", name_);
    if (n_ <= 0) {
        throw std::invalid_argument(
            fmt::format("{}: n() must be positive, got {}", name_, model_->n()));
    }

    x_lower_ = model_->lower();
    x_upper_ = model_->upper();
    require_length(x_lower_.size(), n_, "lower()", name_);
    require_length(x_upper_.size(), n_, "upper()", name_);
    for (int i = 0; i < n_; i++) {
        if (std::isnan(x_lower_[i]) || std::isnan(x_upper_[i])) {
            throw std::invalid_argument(fmt::format("{}: variable {} bound is NaN", name_, i));
        }
        if (x_lower_[i] > x_upper_[i]) {
            throw std::invalid_argument(
                fmt::format("{}: variable {} lower bound {} exceeds upper bound {}", name_, i,
                            x_lower_[i], x_upper_[i]));
        }
        // A variable fixed at infinity is rejected. The two preceding checks
        // both pass it: inf is not NaN, and inf > inf is false. The install
        // loop in make_nlp_program then asks `isfinite` before installing
        // either side, so such a variable ends up with no bound at all -- a
        // "fixed" variable silently free, which is a wrong answer rather than
        // a missing diagnostic. Equality at a finite value is untouched and
        // remains the ordinary way to fix a variable.
        if (x_lower_[i] == x_upper_[i] && !std::isfinite(x_lower_[i])) {
            throw std::invalid_argument(
                fmt::format("{}: variable {}: both bounds are {} — a variable fixed at infinity",
                            name_, i, x_lower_[i]));
        }
    }

    // The three patterns, walked once at the model's own start point. Which
    // point is immaterial by the model's invariance precondition; the start
    // point is the one point every model is required to be able to produce.
    const Eigen::VectorXd x0 = model_->start_point();
    require_length(x0.size(), n_, "start_point()", name_);
    le_scratch_ = Eigen::VectorXd::Zero(num_eq_);
    li_scratch_ = Eigen::VectorXd::Zero(num_iq_);
    le_record_ = Eigen::VectorXd::Zero(num_eq_);

    // Everything a later evaluation writes into, sized here so that nothing
    // after this point resizes.
    grad_scratch_ = Eigen::VectorXd::Zero(n_);
    ce_scratch_ = Eigen::VectorXd::Zero(num_eq_);
    ci_scratch_ = Eigen::VectorXd::Zero(num_iq_);
    jac_e_scratch_ = SpMatRM(num_eq_, n_);
    jac_i_scratch_ = SpMatRM(num_iq_, n_);
    hess_scratch_ = SpMatRM(n_, n_);

    model_->eval_hess_in_place(x0, 1.0, le_scratch_, li_scratch_, hess_scratch_);
    require_dimensions(this->hessian(), n_, n_, "eval_hess", name_);
    require_compressed(this->hessian(), "eval_hess", name_);
    require_upper_triangle(this->hessian(), name_);
    hess_ = record_coordinates(this->hessian());

    if (num_eq_ > 0) {
        model_->eval_jac_e_in_place(x0, jac_e_scratch_);
    }
    require_dimensions(this->equality_jacobian(), num_eq_, n_, "eval_jac_e", name_);
    require_compressed(this->equality_jacobian(), "eval_jac_e", name_);
    eq_jac_ = record_coordinates(this->equality_jacobian());

    if (num_iq_ > 0) {
        model_->eval_jac_i_in_place(x0, jac_i_scratch_);
    }
    require_dimensions(this->inequality_jacobian(), num_iq_, n_, "eval_jac_i", name_);
    require_compressed(this->inequality_jacobian(), "eval_jac_i", name_);
    iq_jac_ = record_coordinates(this->inequality_jacobian());

    hess_owner_ = (num_iq_ > 0)   ? HessOwner::IqPiece
                  : (num_eq_ > 0) ? HessOwner::EqPiece
                                  : HessOwner::Objective;
}

void NLPAdapterCore::sync_x(ConstEigenRef<Eigen::VectorXd> x) {
    if (x.size() != n_) {
        throw std::invalid_argument(
            fmt::format("{}: the solver handed a {}-element iterate to a {}-variable model", name_,
                        x.size(), n_));
    }
    if (x_cache_.size() != n_ || x_cache_ != x) {
        x_cache_ = x;
        gradient_valid_ = false;
        residuals_valid_ = false;
        jacobians_valid_ = false;
    }
}

void NLPAdapterCore::refresh_gradient(ConstEigenRef<Eigen::VectorXd> x) {
    this->sync_x(x);
    if (gradient_valid_) {
        return;
    }
    model_->eval_grad_in_place(x_cache_, grad_scratch_);
    require_length(this->gradient().size(), n_, "eval_grad", name_);
    gradient_valid_ = true;
}

void NLPAdapterCore::refresh_residuals(ConstEigenRef<Eigen::VectorXd> x) {
    this->sync_x(x);
    if (residuals_valid_) {
        return;
    }
    if (num_eq_ > 0) {
        model_->eval_ce_in_place(x_cache_, ce_scratch_);
        require_length(this->equality_residuals().size(), num_eq_, "eval_ce", name_);
    }
    if (num_iq_ > 0) {
        model_->eval_ci_in_place(x_cache_, ci_scratch_);
        require_length(this->inequality_residuals().size(), num_iq_, "eval_ci", name_);
    }
    residuals_valid_ = true;
}

void NLPAdapterCore::refresh_jacobians(ConstEigenRef<Eigen::VectorXd> x) {
    this->sync_x(x);
    if (jacobians_valid_) {
        return;
    }
    if (num_eq_ > 0) {
        model_->eval_jac_e_in_place(x_cache_, jac_e_scratch_);
        require_dimensions(this->equality_jacobian(), num_eq_, n_, "eval_jac_e", name_);
        nlp_require_claimed_pattern(this->equality_jacobian(), eq_jac_, "eval_jac_e", name_);
    }
    if (num_iq_ > 0) {
        model_->eval_jac_i_in_place(x_cache_, jac_i_scratch_);
        require_dimensions(this->inequality_jacobian(), num_iq_, n_, "eval_jac_i", name_);
        nlp_require_claimed_pattern(this->inequality_jacobian(), iq_jac_, "eval_jac_i", name_);
    }
    jacobians_valid_ = true;
}

void NLPAdapterCore::refuse_short_multiplier_block(Index actual, int rows, bool inequality,
                                                   const char *site) const {
    const char *kind = inequality ? "inequality" : "equality";
    throw std::invalid_argument(fmt::format(
        "{}: {} {} multipliers reached the {} piece, which hosts {} {} rows (refused {})", name_,
        actual, kind, kind, rows, kind, site));
}

void NLPAdapterCore::record_equality_multipliers(ConstEigenRef<Eigen::VectorXd> L) {
    // Checked before the slice, not after it, under the block rule stated on
    // refuse_short_multiplier_block: the head is what this host records, and a
    // block shorter than its rows has none to take.
    if (L.size() < num_eq_) {
        refuse_short_multiplier_block(L.size(), num_eq_, false, "at the record");
    }
    le_record_ = L.head(num_eq_);
    le_recorded_ = true;
}

void NLPAdapterCore::eval_hessian_values(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                                         ConstEigenRef<Eigen::VectorXd> le,
                                         ConstEigenRef<Eigen::VectorXd> li) {
    this->sync_x(x);
    // The same block rule the pieces read under, stated on
    // refuse_short_multiplier_block: this host's rows are the first ones laid,
    // so its own block is the head of what the engine hands down, and anything
    // shorter than its rows -- an empty block included -- is a defect in the
    // chain rather than a shape to interpret. A host with no rows of a kind
    // has no head to take, so an empty block of that kind is the right length
    // and the head below is empty.
    if (le.size() < num_eq_) {
        refuse_short_multiplier_block(le.size(), num_eq_, false, "at the Hessian owner");
    }
    if (li.size() < num_iq_) {
        refuse_short_multiplier_block(li.size(), num_iq_, true, "at the Hessian owner");
    }
    le_scratch_ = le.head(num_eq_);
    li_scratch_ = li.head(num_iq_);
    model_->eval_hess_in_place(x_cache_, obj_factor, le_scratch_, li_scratch_, hess_scratch_);
    require_dimensions(this->hessian(), n_, n_, "eval_hess", name_);
    nlp_require_claimed_pattern(this->hessian(), hess_, "eval_hess", name_);
}

std::shared_ptr<NonLinearProgram> make_nlp_program(const std::shared_ptr<NLPAdapterCore> &core) {
    const int n = core->n_;
    Eigen::MatrixXi vindex(n, 1);
    for (int i = 0; i < n; i++) {
        vindex(i, 0) = i;
    }

    auto nlp = std::make_shared<NonLinearProgram>(1);

    ObjectiveFunction obj(ObjectiveInterface(NLPObjectivePiece(core)), vindex);
    obj.set_thread_mode(ThreadingFlags::MainThread);
    nlp->objectives_.push_back(obj);

    if (core->num_eq_ > 0) {
        Eigen::MatrixXi cindex(core->num_eq_, 1);
        for (int k = 0; k < core->num_eq_; k++) {
            cindex(k, 0) = k;
        }
        ConstraintFunction eq(ConstraintInterface(NLPConstraintPiece(core, false)), vindex, cindex);
        eq.set_thread_mode(ThreadingFlags::MainThread);
        nlp->equality_constraints_.push_back(eq);
    }
    if (core->num_iq_ > 0) {
        Eigen::MatrixXi cindex(core->num_iq_, 1);
        for (int k = 0; k < core->num_iq_; k++) {
            cindex(k, 0) = k;
        }
        ConstraintFunction iq(ConstraintInterface(NLPConstraintPiece(core, true)), vindex, cindex);
        iq.set_thread_mode(ThreadingFlags::MainThread);
        nlp->inequality_constraints_.push_back(iq);
    }

    for (int i = 0; i < n; i++) {
        if (std::isfinite(core->x_lower_[i]) || std::isfinite(core->x_upper_[i])) {
            nlp->set_variable_bound(i, core->x_lower_[i], core->x_upper_[i]);
        }
    }

    nlp->make_nlp(n, core->num_eq_, core->num_iq_);
    return nlp;
}

} // namespace hven::solvers
