// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// This file implements the struct SolverIndexingData which holds all meta data
// necessary for an asset vector function to be used as a constraint or objective inside of hven's
// interior-point engine. It is coupled with a function by the interface classes ConstraintFunction
// and ObjectiveFunction.

#pragma once
#include <vector>

#include <Eigen/Core>

#include "hven/model/parsed_io_flags.h"

namespace hven {

namespace detail {
// The cold throw paths of SolverIndexingData's checks (src/model/validation_errors.cpp):
// each check stays inline below, and only the message and the throw are out of line.
[[noreturn]] void throw_output_v_index_shape(Eigen::Index rows, Eigen::Index cols,
                                             Eigen::Index got_rows, Eigen::Index got_cols);
[[noreturn]] void throw_thread_split_count(int threads);
[[noreturn]] void throw_v_index_rows(int expected, Eigen::Index got);
[[noreturn]] void throw_c_index_rows(int expected, Eigen::Index got);
} // namespace detail

/// @brief Canonical KKT lock column for the physical slot coupling global indices
/// @p a and @p b: the smaller of the two.
///
/// The KKT sparsity routine (NonLinearProgram::analyze_sparsity) canonicalizes every
/// element to the lower triangle (col <= row) and stores its value offset under the
/// SMALLER endpoint, so both mirror orderings of a symmetric Hessian pair collapse to
/// one physical double filed under min(a, b). Any two writers of that double are only
/// serialized if they take the same mutex, so every KKT scatter site
/// (DenseFunctionBase::kkt_fill_all / kkt_fill_hess) keys its per-element lock on this
/// function, and NonLinearProgram::get_mat_space marks contested columns (and sizes
/// kkt_locks_) with this same function. Because all claimants of a slot derive their
/// lock column from this single shared keying, cross-partition agreement is structural
/// -- there is no per-site convention that can drift. Do NOT introduce a second keying
/// convention at any of those sites.
inline constexpr int kkt_canonical_lock_col(int a, int b) { return (a < b) ? a : b; }

struct SolverIndexingData {
    using MatrixXi = Eigen::MatrixXi;
    using VectorXi = Eigen::VectorXi;

    int input_size = 0;
    int output_size = 0;
    int num_funcappl = 0;

    bool vindex_init = false;
    bool cindex_init = false;

    /// Whether this copy's rows are written by its own partition only. Set by
    /// the layout on every partition copy (NonLinearProgram::
    /// mark_shared_constraint_rows): false when another partition also names one
    /// of its rows, and then the Jacobian scatter locks a contested column
    /// (docs/assembly-protocol.md §5). True on one partition.
    bool unique_constraints = true;

    /// @brief Matrix whose columns contain the ordered indices of the
    /// variables forwarded to the constraint or objective function.
    MatrixXi v_index;

    /// @brief Matrix whose columns contain the ordered constraint-output
    /// indices corresponding to each column of v_index. Empty for objective
    /// functions.
    MatrixXi c_index;

    /// @brief Where this function's outputs go in the SOLVER's variable space,
    /// when that differs from the problem's own variable space.
    ///
    /// v_index above is the function's INPUT map: which entries of the primal
    /// vector its arguments are gathered from. It always addresses the full
    /// problem space and is never rewritten -- a function always reads the same
    /// variables it was declared over.
    ///
    /// This is the OUTPUT map: which KKT column, and which gradient row, each of
    /// those arguments corresponds to once the solver has eliminated the
    /// variables whose bounds fix them. Entry (i, V) is the solver-space index
    /// of the same variable v_index(i, V) names, or -1 when that variable has
    /// been eliminated and the corresponding outputs have nowhere to go.
    ///
    /// Empty on the identity path: with nothing eliminated, the solver's space
    /// IS the problem's space and v_index serves as both maps, so no second
    /// table is built and no copy is made. Derived state -- always regenerated
    /// from v_index, never edited in place, so repeated configuration cannot
    /// compound.
    MatrixXi v_out_index;

    /// True while v_out_index is live, i.e. the output map differs from the
    /// input map. THE flag the KKT scatters hoist their branch on: false selects
    /// the untouched original loops, and it is false for every function on every
    /// problem with no bound-fixed variables. Setup-time emitters read
    /// v_scatter_loc(), which dispatches on the same flag.
    bool v_out_reduced = false;

    /// @brief Per column of v_index: whether its indices are sorted and
    /// contiguous (e.g. 10,11,12,...).
    std::vector<ParsedIoFlags> v_index_continuity;

    /// @brief Per column of c_index: whether its indices are sorted and
    /// contiguous (e.g. 10,11,12,...).
    std::vector<ParsedIoFlags> c_index_continuity;

    /// @brief Start of the memory region where the engine sums the constraint
    /// output of the i-th application of this function.
    VectorXi inner_constraint_starts;

    /// @brief Start of the memory region where the engine sums the gradient
    /// output of the i-th application of this function.
    VectorXi inner_gradient_starts;

    /// @brief Start of the memory region holding the locations where the
    /// derivatives of the i-th application are summed into the global KKT
    /// matrix.
    VectorXi inner_kkt_starts;

    SolverIndexingData() {}
    SolverIndexingData(int irr, int orr, const MatrixXi &vindex, const MatrixXi &cindex)
        : input_size(irr), output_size(orr) {
        this->set_v_index_c_index(vindex, cindex);
    }

    SolverIndexingData(int irr, const MatrixXi &vindex) : input_size(irr), output_size(1) {
        this->set_v_index(vindex);
    }

    /// @brief Installs the solver-space output map. @p vout must have the same
    /// shape as v_index; -1 entries mark eliminated variables. Regenerated
    /// wholesale by the caller from v_index on every configuration, so this
    /// never has to undo a previous mapping.
    void set_output_v_index(const MatrixXi &vout) {
        if (vout.rows() != this->v_index.rows() || vout.cols() != this->v_index.cols()) {
            detail::throw_output_v_index_shape(this->v_index.rows(), this->v_index.cols(),
                                               vout.rows(), vout.cols());
        }
        this->v_out_index = vout;
        this->v_out_reduced = true;
    }

    /// Drops the output map, returning this function to the identity path where
    /// v_index is both maps.
    void clear_output_v_index() {
        this->v_out_index.resize(0, 0);
        this->v_out_reduced = false;
    }

    /// Solver-space output index, valid only while v_out_reduced is true --
    /// which is exactly when the KKT scatters take their reduced loop.
    inline int v_out_loc(int loc, int col) const { return this->v_out_index(loc, col); }

    /// Output index for setup-time emitters (KKT/gradient location tables),
    /// which run once per configuration and can afford the dispatch: the
    /// solver-space index when an output map is live, the problem-space index
    /// otherwise. Returns -1 for an eliminated variable.
    inline int v_scatter_loc(int loc, int col) const {
        return this->v_out_reduced ? this->v_out_index(loc, col) : this->v_index(loc, col);
    }

    void get_gradient_space(Eigen::Ref<VectorXi> GXrows, int &freeloc) {
        this->inner_gradient_starts.resize(this->num_appl());
        for (int V = 0; V < this->num_appl(); V++) {
            this->inner_gradient_starts[V] = freeloc;
            for (int i = 0; i < this->input_size; i++) {
                // -1 for an eliminated variable: the claim stays the same size
                // (the function still writes input_size gradient values into
                // it) but that value has no row to be summed into, and the RHS
                // fill skips it.
                GXrows[freeloc] = this->v_scatter_loc(i, V);
                freeloc++;
            }
        }
    }
    void get_constraint_space(Eigen::Ref<VectorXi> FXrows, int &freeloc) {
        this->inner_constraint_starts.resize(this->num_appl());
        for (int V = 0; V < this->num_appl(); V++) {
            this->inner_constraint_starts[V] = freeloc;
            for (int j = 0; j < this->output_size; j++) {
                FXrows[freeloc] = this->c_loc(j, V);
                freeloc++;
            }
        }
    }

    std::vector<SolverIndexingData> thread_split(int Threads) const {
        if (Threads <= 0)
            detail::throw_thread_split_count(Threads);

        std::vector<SolverIndexingData> split;
        split.reserve(Threads);

        int cols = this->num_funcappl;
        int colpThr = cols / Threads;
        int rempThr = cols % Threads;

        VectorXi perThr = VectorXi::Constant(Threads, colpThr);
        perThr.head(rempThr) += VectorXi::Constant(rempThr, 1);
        int start = 0;
        int range;
        if (colpThr > 0)
            range = Threads;
        else
            range = rempThr;
        for (int i = 0; i < range; i++) {
            if (this->cindex_init) {
                split.emplace_back(SolverIndexingData(this->input_size, this->output_size,
                                                      this->v_index.middleCols(start, perThr[i]),
                                                      this->c_index.middleCols(start, perThr[i])));
            } else {
                split.emplace_back(SolverIndexingData(this->input_size,
                                                      this->v_index.middleCols(start, perThr[i])));
            }
            split.back().unique_constraints = this->unique_constraints;
            start += perThr[i];
        }
        return split;
    }

    void set_v_index(const MatrixXi &vt) {
        if (vt.rows() != this->input_size)
            detail::throw_v_index_rows(this->input_size, vt.rows());
        this->v_index = vt;
        this->vindex_init = true;
        this->num_funcappl = this->v_index.cols();
        this->v_index_continuity.resize(this->v_index.cols());
        for (int i = 0; i < this->v_index.cols(); i++) {
            this->v_index_continuity[i] = this->check_continuity(this->v_index.col(i));
        }
    }
    void set_c_index(const MatrixXi &ct) {
        if (ct.rows() != this->output_size)
            detail::throw_c_index_rows(this->output_size, ct.rows());
        this->c_index = ct;
        this->c_index_continuity.resize(this->c_index.cols());
        this->cindex_init = true;

        for (int i = 0; i < this->c_index.cols(); i++) {
            this->c_index_continuity[i] = this->check_continuity(this->c_index.col(i));
        }
    }
    const MatrixXi &get_v_index() const { return this->v_index; }
    const MatrixXi &get_c_index() const { return this->c_index; }
    void set_v_index_c_index(const MatrixXi &vt, const MatrixXi &ct) {
        this->set_v_index(vt);
        this->set_c_index(ct);
    }
    inline int num_appl() const { return this->num_funcappl; }
    inline int c_loc(int loc, int col) const { return this->c_index(loc, col); }
    inline int v_loc(int loc, int col) const { return this->v_index(loc, col); }

    static ParsedIoFlags check_continuity(const Eigen::VectorXi &ix) {
        int s = 0;
        for (int i = 0; i < (ix.size() - 1); i++) {
            s = ix[i + 1] - ix[i] - 1;
            if (s != 0)
                return ParsedIoFlags::kNotContiguous;
        }
        return ParsedIoFlags::kContiguous;
    }
};

} // namespace hven
