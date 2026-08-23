// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// nlp_adapter.h — the piece host that carries one NlpModel onto
// NonLinearProgram, the shape the interior-point engine consumes.
//
// The host owns no problem semantics. Row splitting, bound shifts, multiplier
// composition and the triplet-to-sparse conversion all belong to whatever
// produced the model -- model/nlp_problem_model.h does exactly that for the
// triplet-shaped convenience problem. What is left here is coordinates: which
// KKT slot each stored matrix entry claims, and which arena row each residual
// writes.
//
// The host relies on the precondition the model already carries rather than a
// new one: eval_hess, eval_jac_e and eval_jac_i emit an invariant sparsity
// pattern (nlp_model.h's structural-pattern-invariance note). The claim pass
// walks those three patterns once, at the model's own start point, and every
// later evaluation scatters through the slots that walk assigned. An
// evaluation presenting a coordinate its slot was not claimed at is refused by
// name rather than summed into the location laid for another coordinate.

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include <fmt/format.h>

#include "hven/core/types.h"
#include "hven/detail/interior/constraint_function.h"
#include "hven/detail/interior/indexing_data.h"
#include "hven/detail/interior/objective_function.h"
#include "hven/detail/interior/threading_flags.h"
#include "hven/detail/interior/typedefs/eigen_types.h"
#include "hven/model/nlp_model.h"
#include "hven/solver_interface_adapter.h"

namespace hven::solvers {

struct NonLinearProgram;

/// One stored matrix entry's coordinates, in the order the model presents them.
struct NLPCoordinate {
    int row_;
    int col_;
};

/// @brief Refuses a matrix whose stored coordinates are not the ones the claim
///        pass recorded for it, in the same order.
/// @param matrix The freshly evaluated matrix.
/// @param recorded The coordinates recorded at claim time, in claim order.
/// @param what The model method that produced @p matrix, for diagnostics.
/// @param name Names the model, for diagnostics.
/// @throws std::invalid_argument if the entry count or any coordinate differs
///         from what was claimed.
///
/// Called once per evaluated matrix, before any of its values are read. Every
/// consumer then pairs stored value k with recorded coordinate k by index,
/// which is what this check makes meaningful -- and, because it runs first,
/// nothing has been summed anywhere when it refuses.
void nlp_require_claimed_pattern(const Eigen::SparseMatrix<double, Eigen::RowMajor> &matrix,
                                 const std::vector<NLPCoordinate> &recorded, const char *what,
                                 const std::string &name);

/// Shared state behind the adapter pieces: the model, the coordinates its three
/// matrices were claimed at, the staged variable bounds, the per-iterate
/// evaluation cache, and the per-assembly multiplier records. Shared by every
/// piece copy via shared_ptr; safe without locking because the adapter always
/// runs single-partition, so every access is serial.
struct NLPAdapterCore {
    std::shared_ptr<NlpModel> model_;
    std::string name_; ///< names the model in diagnostics and in the piece names
    int n_ = 0, num_eq_ = 0, num_iq_ = 0;

    Eigen::VectorXd x_lower_, x_upper_; ///< variable bounds, as the model reports them

    std::vector<NLPCoordinate> eq_jac_; ///< equality Jacobian, claim order
    std::vector<NLPCoordinate> iq_jac_; ///< inequality Jacobian, claim order
    std::vector<NLPCoordinate> hess_;   ///< Lagrangian Hessian, upper triangle, claim order

    /// Which piece scatters the (monolithic) Lagrangian Hessian: the last one
    /// the evaluation order reaches, decided once here.
    enum class HessOwner { Objective, EqPiece, IqPiece };
    HessOwner hess_owner_ = HessOwner::Objective;

    // --- Per-iterate cache. The model's callbacks are pure, so it keys on the
    // iterate value. x_cache_ is also the plain vector every model call is
    // handed, so one assembly copies the solver's iterate once. ---
    Eigen::VectorXd x_cache_;
    bool gradient_valid_ = false;
    bool residuals_valid_ = false;
    bool jacobians_valid_ = false;

    /// Every destination an evaluation writes, owned by this host and sized
    /// once at construction. The model is asked to fill them in place, so a
    /// model that overrides those methods writes here directly and one that
    /// does not gets the base class's delegation to its by-value methods --
    /// the same values either way, and one code path for the consumers.
    Eigen::VectorXd grad_scratch_, ce_scratch_, ci_scratch_;
    SpMatRM jac_e_scratch_, jac_i_scratch_, hess_scratch_;

    // --- Per-assembly consume-once records. The objective piece records the
    // objective scale when its Hessian-bearing method runs; the equality piece
    // records its multipliers likewise; the Hessian owner consumes both. A
    // chain that skips the objective (the solver's no-objective KKT mode)
    // leaves no record, and the owner correctly uses scale 0. The multiplier
    // record is a sized member rather than an optional vector so that setting
    // it allocates nothing. ---
    std::optional<double> pending_obj_scale_;
    bool le_recorded_ = false;
    Eigen::VectorXd le_record_;

    /// Multiplier scratch, sized once at setup: the pieces hand their solver-
    /// space blocks in as references, and the model takes vectors.
    Eigen::VectorXd le_scratch_, li_scratch_;

    /// @brief Claims the model's three patterns and stages its bounds.
    /// @param model The model to host; must not be null.
    /// @param name Names the model in diagnostics and in the piece names.
    /// @throws std::invalid_argument on a null model, a non-positive variable
    ///         count, a negative row count, a bound vector of the wrong length,
    ///         a variable bound that is NaN, inverted, or fixed at infinity, or
    ///         a Hessian return carrying an entry below the diagonal.
    explicit NLPAdapterCore(std::shared_ptr<NlpModel> model, std::string name = "NLP model");

    /// @brief Copies @p x into the cache, invalidating what is keyed on it.
    /// @param x The solver's iterate.
    /// @throws std::invalid_argument if @p x is not n() elements.
    void sync_x(ConstEigenRef<Eigen::VectorXd> x);

    /// @brief Brings the objective gradient up to date at @p x.
    /// @param x The solver's iterate.
    /// @throws std::invalid_argument if the model returns the wrong length.
    void refresh_gradient(ConstEigenRef<Eigen::VectorXd> x);

    /// @brief Brings both residual blocks up to date at @p x.
    /// @param x The solver's iterate.
    /// @throws std::invalid_argument if either block is the wrong length.
    void refresh_residuals(ConstEigenRef<Eigen::VectorXd> x);

    /// @brief Brings both Jacobians up to date at @p x, checking each against
    ///        the coordinates its claims were laid over.
    /// @param x The solver's iterate.
    /// @throws std::invalid_argument on a wrong shape or a shifted pattern.
    void refresh_jacobians(ConstEigenRef<Eigen::VectorXd> x);

    /// @brief Records this chain's equality multipliers for the Hessian owner.
    /// @param L The solver's equality multiplier block; this host's rows are
    ///          its leading entries, so it may be longer but not shorter.
    /// @throws std::invalid_argument if @p L is shorter than this host's rows.
    void record_equality_multipliers(ConstEigenRef<Eigen::VectorXd> L);
    /// Throws the refusal for a multiplier block shorter than a piece's rows,
    /// naming @p site so the three places that check are told apart in the
    /// message.
    ///
    /// This is the one rule for every multiplier block the host reads: a block
    /// may be longer than the rows it is checked against -- the engine appends
    /// rows of its own after the pieces' (a fixed variable turned into an
    /// equality row) and hands the whole vector down, and this host's rows are
    /// the first ones laid, so its own block is the head -- but never shorter.
    /// An empty block is not a shape that means all-zero; it is only the right
    /// length where the host has no rows of that kind to take a head from.
    ///
    /// Never inlined: it carries the message formatting, which must not sit
    /// in the pieces' value fills.
    [[noreturn]] void refuse_short_multiplier_block(Index actual, int rows, bool inequality,
                                                    const char *site) const;

    /// @brief The one eval_hess call of an assembly, checked against the
    ///        coordinates the Hessian claims were laid over.
    /// @param x The solver's iterate.
    /// @param obj_factor The objective scale this chain carried.
    /// @param le Equality multipliers, under the block rule stated on
    ///           refuse_short_multiplier_block: longer than this host's rows
    ///           means its block is the head, shorter is refused.
    /// @param li Inequality multipliers, same rule.
    /// @throws std::invalid_argument on a multiplier block shorter than this
    ///         host's rows, a wrong shape, or a shifted pattern.
    void eval_hessian_values(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                             ConstEigenRef<Eigen::VectorXd> le, ConstEigenRef<Eigen::VectorXd> li);

    /// @brief The objective gradient from the last refresh_gradient.
    const Eigen::VectorXd &gradient() const { return grad_scratch_; }
    /// @brief The equality residuals from the last refresh_residuals.
    const Eigen::VectorXd &equality_residuals() const { return ce_scratch_; }
    /// @brief The inequality residuals from the last refresh_residuals.
    const Eigen::VectorXd &inequality_residuals() const { return ci_scratch_; }
    /// @brief The equality Jacobian from the last refresh_jacobians.
    const SpMatRM &equality_jacobian() const { return jac_e_scratch_; }
    /// @brief The inequality Jacobian from the last refresh_jacobians.
    const SpMatRM &inequality_jacobian() const { return jac_i_scratch_; }
    /// @brief The Lagrangian Hessian from the last eval_hessian_values.
    const SpMatRM &hessian() const { return hess_scratch_; }
};

/// Claims the Lagrangian-Hessian block: one KKT slot per stored Hessian entry,
/// at the (reduced) variable coordinates. The reduced renumbering is monotone,
/// so an upper-triangle pattern stays upper-triangle.
inline void nlp_claim_hessian_block(const NLPAdapterCore &core, EigenRef<Eigen::VectorXi> KKTrows,
                                    EigenRef<Eigen::VectorXi> KKTcols, int &freeloc,
                                    const SolverIndexingData &data) {
    for (const NLPCoordinate &entry : core.hess_) {
        const int r = data.v_scatter_loc(entry.row_, 0);
        const int c = data.v_scatter_loc(entry.col_, 0);
        if (r < 0 || c < 0) {
            KKTrows[freeloc] = -1;
            KKTcols[freeloc] = -1;
        } else {
            KKTrows[freeloc] = r;
            KKTcols[freeloc] = c;
        }
        freeloc++;
    }
}

/// Sums the freshly evaluated Hessian values into the KKT matrix through the
/// claims recorded at @p claim_start. Lock keying follows the shared protocol
/// (kkt_canonical_lock_col); with the adapter's single partition no slot is
/// ever contested, but the discipline is kept so the code stays correct if
/// that ever changes.
inline void nlp_scatter_hessian_block(const NLPAdapterCore &core, int claim_start,
                                      Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                                      EigenRef<Eigen::VectorXi> KKTLocs,
                                      EigenRef<Eigen::VectorXi> VarClashes,
                                      std::vector<std::mutex> &ClashLocks,
                                      const SolverIndexingData &data) {
    if (claim_start < 0) {
        throw std::logic_error("NLP adapter: Hessian scatter before its KKT space was claimed");
    }
    // Stored value k is recorded coordinate k: the pattern was checked against
    // the claim order when the matrix was evaluated, so this indexes rather
    // than walks.
    const double *values = core.hessian().valuePtr();
    int cursor = claim_start;
    for (std::size_t k = 0; k < core.hess_.size(); k++, cursor++) {
        const int r = data.v_scatter_loc(core.hess_[k].row_, 0);
        const int c = data.v_scatter_loc(core.hess_[k].col_, 0);
        if (r < 0 || c < 0) {
            continue;
        }
        const int lockcol = kkt_canonical_lock_col(r, c);
        const bool lock_column = (VarClashes[lockcol] != -1);
        if (lock_column) {
            ClashLocks[VarClashes[lockcol]].lock();
        }
        KKTmat.valuePtr()[KKTLocs.data()[cursor]] += values[k];
        if (lock_column) {
            ClashLocks[VarClashes[lockcol]].unlock();
        }
    }
}

/// The model's objective, as a solver objective. Also the Hessian owner when
/// the model has no constraint rows.
struct NLPObjectivePiece {
    std::shared_ptr<NLPAdapterCore> core_;
    int hess_claim_start_ = -1; ///< recorded per partition copy during get_kkt_space

    NLPObjectivePiece() = default;
    explicit NLPObjectivePiece(std::shared_ptr<NLPAdapterCore> core) : core_(std::move(core)) {}

    std::string name() const { return core_->name_ + " (objective)"; }
    int input_rows() const { return core_->n_; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return false; }
    bool owns_hessian() const { return core_->hess_owner_ == NLPAdapterCore::HessOwner::Objective; }

    void objective(double ObjScale, ConstEigenRef<Eigen::VectorXd> X, double &Val,
                   const SolverIndexingData &data) const {
        (void)data;
        core_->sync_x(X);
        Val += ObjScale * core_->model_->eval_f(core_->x_cache_);
    }

    void objective_gradient(double ObjScale, ConstEigenRef<Eigen::VectorXd> X, double &Val,
                            EigenRef<Eigen::VectorXd> GX, const SolverIndexingData &data) const {
        this->objective(ObjScale, X, Val, data);
        core_->refresh_gradient(X);
        GX.segment(data.inner_gradient_starts_[0], core_->n_) = ObjScale * core_->gradient();
    }

    void objective_gradient_hessian(double ObjScale, ConstEigenRef<Eigen::VectorXd> X, double &Val,
                                    EigenRef<Eigen::VectorXd> GX,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                                    EigenRef<Eigen::VectorXi> KKTLocations,
                                    EigenRef<Eigen::VectorXi> KKTClashes,
                                    std::vector<std::mutex> &KKTLocks,
                                    const SolverIndexingData &data) const {
        // A model callback further down this same assembly (an eq/iq piece's
        // eval_ce/eval_jac_e/eval_hess) can throw after this method has already
        // recorded pending_obj_scale_. An aborted assembly must not leave that
        // record behind for a later, unrelated chain (InteriorPointSolver's restoration
        // entry runs eval_kkt_no next; a caller can also retry solve() after a
        // propagated exception) to read as if it were this chain's own -- so
        // any exception escaping this method clears both consume-once records
        // before propagating. A completed chain is unaffected: the records are
        // still set/consumed exactly as before.
        try {
            this->objective_gradient(ObjScale, X, Val, GX, data);
            if (this->owns_hessian()) {
                core_->eval_hessian_values(X, ObjScale, Eigen::VectorXd(), Eigen::VectorXd());
                nlp_scatter_hessian_block(*core_, hess_claim_start_, KKTmat, KKTLocations,
                                          KKTClashes, KKTLocks, data);
            } else {
                core_->pending_obj_scale_ = ObjScale;
            }
        } catch (...) {
            core_->pending_obj_scale_.reset();
            core_->le_recorded_ = false;
            throw;
        }
    }

    // A constraint interface is part of the objective concept because the KKT
    // sizing pass is shared; only the two structure methods are ever invoked on
    // an objective. The evaluation methods below are unreachable and say so.
    void constraints(ConstEigenRef<Eigen::VectorXd>, EigenRef<Eigen::VectorXd>,
                     const SolverIndexingData &) const {
        throw std::logic_error("NLPObjectivePiece: constraint evaluation on an objective");
    }
    void constraints_adjointgradient(ConstEigenRef<Eigen::VectorXd>, ConstEigenRef<Eigen::VectorXd>,
                                     EigenRef<Eigen::VectorXd>, EigenRef<Eigen::VectorXd>,
                                     const SolverIndexingData &) const {
        throw std::logic_error("NLPObjectivePiece: constraint evaluation on an objective");
    }
    void constraints_jacobian(ConstEigenRef<Eigen::VectorXd>, EigenRef<Eigen::VectorXd>,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                              EigenRef<Eigen::VectorXi>, EigenRef<Eigen::VectorXi>,
                              std::vector<std::mutex> &, const SolverIndexingData &) const {
        throw std::logic_error("NLPObjectivePiece: constraint evaluation on an objective");
    }
    void constraints_jacobian_adjointgradient(ConstEigenRef<Eigen::VectorXd>,
                                              ConstEigenRef<Eigen::VectorXd>,
                                              EigenRef<Eigen::VectorXd>, EigenRef<Eigen::VectorXd>,
                                              Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                                              EigenRef<Eigen::VectorXi>, EigenRef<Eigen::VectorXi>,
                                              std::vector<std::mutex> &,
                                              const SolverIndexingData &) const {
        throw std::logic_error("NLPObjectivePiece: constraint evaluation on an objective");
    }
    void constraints_jacobian_adjointgradient_adjointhessian(
        ConstEigenRef<Eigen::VectorXd>, ConstEigenRef<Eigen::VectorXd>, EigenRef<Eigen::VectorXd>,
        EigenRef<Eigen::VectorXd>, Eigen::SparseMatrix<double, Eigen::RowMajor> &,
        EigenRef<Eigen::VectorXi>, EigenRef<Eigen::VectorXi>, std::vector<std::mutex> &,
        const SolverIndexingData &) const {
        throw std::logic_error("NLPObjectivePiece: constraint evaluation on an objective");
    }

    void get_kkt_space(EigenRef<Eigen::VectorXi> KKTrows, EigenRef<Eigen::VectorXi> KKTcols,
                       int &freeloc, int conoffset, bool dojac, bool dohess,
                       SolverIndexingData &data) {
        (void)conoffset;
        (void)dojac;
        data.inner_kkt_starts_.resize(1);
        data.inner_kkt_starts_[0] = freeloc;
        if (dohess && this->owns_hessian()) {
            hess_claim_start_ = freeloc;
            nlp_claim_hessian_block(*core_, KKTrows, KKTcols, freeloc, data);
        }
    }
    int num_kkt_elements(bool dojac, bool dohess) const {
        (void)dojac;
        return (dohess && this->owns_hessian()) ? static_cast<int>(core_->hess_.size()) : 0;
    }
};

/// One block of the model's constraint rows — the equalities or the
/// inequalities — as a solver constraint. The last-evaluated piece also owns
/// the Lagrangian Hessian.
struct NLPConstraintPiece {
    std::shared_ptr<NLPAdapterCore> core_;
    bool is_inequality_ = false;
    int hess_claim_start_ = -1;

    NLPConstraintPiece() = default;
    NLPConstraintPiece(std::shared_ptr<NLPAdapterCore> core, bool is_inequality)
        : core_(std::move(core)), is_inequality_(is_inequality) {}

    std::string name() const {
        return core_->name_ + (is_inequality_ ? " (inequalities)" : " (equalities)");
    }
    int input_rows() const { return core_->n_; }
    int output_rows() const { return is_inequality_ ? core_->num_iq_ : core_->num_eq_; }
    bool thread_safe() const { return false; }
    bool owns_hessian() const {
        return is_inequality_ ? core_->hess_owner_ == NLPAdapterCore::HessOwner::IqPiece
                              : core_->hess_owner_ == NLPAdapterCore::HessOwner::EqPiece;
    }
    /// The claim-order coordinates of this block's Jacobian.
    const std::vector<NLPCoordinate> &jac_coords() const {
        return is_inequality_ ? core_->iq_jac_ : core_->eq_jac_;
    }
    /// This block's most recently evaluated Jacobian.
    const SpMatRM &jacobian() const {
        return is_inequality_ ? core_->inequality_jacobian() : core_->equality_jacobian();
    }
    /// The model method that produces this block's Jacobian, for diagnostics.
    const char *jacobian_method() const { return is_inequality_ ? "eval_jac_i" : "eval_jac_e"; }

    void constraints(ConstEigenRef<Eigen::VectorXd> X, EigenRef<Eigen::VectorXd> FX,
                     const SolverIndexingData &data) const {
        core_->refresh_residuals(X);
        this->write_residuals(FX, data);
    }
    void constraints_adjointgradient(ConstEigenRef<Eigen::VectorXd> X,
                                     ConstEigenRef<Eigen::VectorXd> L, EigenRef<Eigen::VectorXd> FX,
                                     EigenRef<Eigen::VectorXd> AGX,
                                     const SolverIndexingData &data) const {
        core_->refresh_residuals(X);
        core_->refresh_jacobians(X);
        this->write_residuals(FX, data);
        this->write_adjoint_gradient(L, AGX, data);
    }
    void constraints_jacobian(ConstEigenRef<Eigen::VectorXd> X, EigenRef<Eigen::VectorXd> FX,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                              EigenRef<Eigen::VectorXi> KKTLocations,
                              EigenRef<Eigen::VectorXi> KKTClashes,
                              std::vector<std::mutex> &KKTLocks,
                              const SolverIndexingData &data) const {
        core_->refresh_residuals(X);
        core_->refresh_jacobians(X);
        this->write_residuals(FX, data);
        this->scatter_jacobian(KKTmat, KKTLocations, KKTClashes, KKTLocks, data);
    }
    void constraints_jacobian_adjointgradient(
        ConstEigenRef<Eigen::VectorXd> X, ConstEigenRef<Eigen::VectorXd> L,
        EigenRef<Eigen::VectorXd> FX, EigenRef<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
        EigenRef<Eigen::VectorXi> KKTLocations, EigenRef<Eigen::VectorXi> KKTClashes,
        std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const {
        core_->refresh_residuals(X);
        core_->refresh_jacobians(X);
        this->write_residuals(FX, data);
        this->write_adjoint_gradient(L, AGX, data);
        this->scatter_jacobian(KKTmat, KKTLocations, KKTClashes, KKTLocks, data);
    }
    void constraints_jacobian_adjointgradient_adjointhessian(
        ConstEigenRef<Eigen::VectorXd> X, ConstEigenRef<Eigen::VectorXd> L,
        EigenRef<Eigen::VectorXd> FX, EigenRef<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
        EigenRef<Eigen::VectorXi> KKTLocations, EigenRef<Eigen::VectorXi> KKTClashes,
        std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const {
        // Same exception-safety concern as NLPObjectivePiece::objective_gradient_hessian
        // above: this piece's own model calls can throw either before or after
        // it has recorded its multipliers, and the owner (this piece or the one
        // after it) may never run to consume a record either piece left behind.
        // Clear both consume-once records on the way out through any exception
        // so an aborted assembly can never leak a stale objective scale or
        // equality multiplier into a later, unrelated chain.
        try {
            this->constraints_jacobian_adjointgradient(X, L, FX, AGX, KKTmat, KKTLocations,
                                                       KKTClashes, KKTLocks, data);
            if (!is_inequality_ && !this->owns_hessian()) {
                // The inequality piece runs after this one in every Hessian-bearing
                // chain; leave it these multipliers.
                core_->record_equality_multipliers(L);
            }
            if (this->owns_hessian()) {
                const double obj_factor = core_->pending_obj_scale_.value_or(0.0);
                core_->pending_obj_scale_.reset();
                if (is_inequality_) {
                    // No record means the equality piece never ran, and its
                    // multipliers are zero.
                    if (!core_->le_recorded_) {
                        core_->le_record_.setZero();
                    }
                    core_->le_recorded_ = false;
                    core_->eval_hessian_values(X, obj_factor, core_->le_record_, L);
                } else {
                    core_->eval_hessian_values(X, obj_factor, L, Eigen::VectorXd());
                }
                nlp_scatter_hessian_block(*core_, hess_claim_start_, KKTmat, KKTLocations,
                                          KKTClashes, KKTLocks, data);
            }
        } catch (...) {
            core_->pending_obj_scale_.reset();
            core_->le_recorded_ = false;
            throw;
        }
    }

    void get_kkt_space(EigenRef<Eigen::VectorXi> KKTrows, EigenRef<Eigen::VectorXi> KKTcols,
                       int &freeloc, int conoffset, bool dojac, bool dohess,
                       SolverIndexingData &data) {
        data.inner_kkt_starts_.resize(1);
        data.inner_kkt_starts_[0] = freeloc;
        if (dojac) {
            for (const NLPCoordinate &entry : this->jac_coords()) {
                const int col = data.v_scatter_loc(entry.col_, 0);
                KKTrows[freeloc] = (col < 0) ? -1 : data.c_loc(entry.row_, 0) + conoffset;
                KKTcols[freeloc] = col;
                freeloc++;
            }
        }
        if (dohess && this->owns_hessian()) {
            hess_claim_start_ = freeloc;
            nlp_claim_hessian_block(*core_, KKTrows, KKTcols, freeloc, data);
        }
    }
    int num_kkt_elements(bool dojac, bool dohess) const {
        return (dojac ? static_cast<int>(this->jac_coords().size()) : 0) +
               ((dohess && this->owns_hessian()) ? static_cast<int>(core_->hess_.size()) : 0);
    }

  private:
    void write_residuals(EigenRef<Eigen::VectorXd> FX, const SolverIndexingData &data) const {
        const Eigen::VectorXd &residuals =
            is_inequality_ ? core_->inequality_residuals() : core_->equality_residuals();
        FX.segment(data.inner_constraint_starts_[0], residuals.size()) = residuals;
    }
    // Both fills below index stored value k by recorded coordinate k. The
    // pattern was checked against the claim order once, when the Jacobian was
    // evaluated, so a chain that fills both destinations reads the matrix once
    // rather than walking it per destination.
    void write_adjoint_gradient(ConstEigenRef<Eigen::VectorXd> L, EigenRef<Eigen::VectorXd> AGX,
                                const SolverIndexingData &data) const {
        // Checked before L is read at all, not only at the slice further down
        // this file (record_equality_multipliers), under the block rule stated
        // on refuse_short_multiplier_block: a block shorter than this piece's
        // rows has no head to take, and indexing into it below would read past
        // its end. The refusal itself is out of line so this fill stays
        // inlinable into the piece methods; the formatting and the throw are
        // cold, and the site it names tells this read apart from the record.
        const int rows = this->output_rows();
        if (L.size() < rows) {
            core_->refuse_short_multiplier_block(L.size(), rows, is_inequality_,
                                                 "at the piece's first read");
        }
        const std::vector<NLPCoordinate> &coords = this->jac_coords();
        const double *values = this->jacobian().valuePtr();
        const int gstart = data.inner_gradient_starts_[0];
        for (std::size_t k = 0; k < coords.size(); k++) {
            AGX[gstart + coords[k].col_] += values[k] * L[data.c_loc(coords[k].row_, 0)];
        }
    }
    void scatter_jacobian(Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                          EigenRef<Eigen::VectorXi> KKTLocs, EigenRef<Eigen::VectorXi> VarClashes,
                          std::vector<std::mutex> &ClashLocks,
                          const SolverIndexingData &data) const {
        const std::vector<NLPCoordinate> &coords = this->jac_coords();
        const double *values = this->jacobian().valuePtr();
        int cursor = data.inner_kkt_starts_[0];
        for (std::size_t k = 0; k < coords.size(); k++, cursor++) {
            const int vcol = data.v_scatter_loc(coords[k].col_, 0);
            if (vcol < 0) {
                continue;
            }
            const bool lock_column = !data.unique_constraints_ && (VarClashes[vcol] != -1);
            if (lock_column) {
                ClashLocks[VarClashes[vcol]].lock();
            }
            KKTmat.valuePtr()[KKTLocs.data()[cursor]] += values[k];
            if (lock_column) {
                ClashLocks[VarClashes[vcol]].unlock();
            }
        }
    }
};

// Both pieces are plain value types, not erased handles, so each is stored as
// itself: one erasure, one virtual dispatch per solver call. Registered here,
// beside the definitions, per the placement rule in
// hven/solver_interface_adapter.h.
template <>
struct SolverInterfaceAdapter<NLPObjectivePiece> : DirectFunctionModel<NLPObjectivePiece> {};
template <>
struct SolverInterfaceAdapter<NLPConstraintPiece> : DirectFunctionModel<NLPConstraintPiece> {};

/// @brief Builds the single-partition NonLinearProgram for an adapter core.
/// @param core The host whose patterns and bounds the program is laid over.
/// @return The program: the objective piece, the constraint pieces the row
///         counts call for, the staged variable bounds, and the layout.
///
/// The one production path, and the one the tests assemble through.
std::shared_ptr<NonLinearProgram> make_nlp_program(const std::shared_ptr<NLPAdapterCore> &core);

} // namespace hven::solvers
