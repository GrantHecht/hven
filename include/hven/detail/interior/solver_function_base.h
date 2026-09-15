// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// Implements the SolverFunctionBase class which is the base class to
// ConstraintFunction and ObjectiveFunction. Holds an Constraint/ObjectiveInterface type erasure
// class and SolverIndexingData struct. Defines methods for the function to request and reserve KKT
// and RHS space from the solver, and passes relevant arguments to the underlying type erased
// function or index data structure. The two Derived classes ( Constraint/ObjectiveInterface) then
// define the rest of the interface to the type-erased functions constraints and objective methods.

#pragma once

#include <iostream>
#include <stdexcept>
#include <string>

#include <Eigen/Core>

#include <fmt/format.h>

#include "hven/detail/interior/indexing_data.h"
#include "hven/detail/interior/threading_flags.h"
#include "hven/detail/interior/typedefs/eigen_types.h"
#include "hven/detail/solvers/solver_interface_specs.h"

namespace hven::solvers {

/// The one type allowed to move a piece's lay marker: it is the thing that
/// lays layouts, and the marker records that it has.
struct NonLinearProgram;

template <class FuncType> struct SolverFunctionBase {
    using MatrixXi = Eigen::MatrixXi;
    using VectorXi = Eigen::VectorXi;

    FuncType function_;
    SolverIndexingData index_data_;

    SolverFunctionBase() {}

    void print_data() {
        using std::cout;
        using std::endl;

        cout << "Name: " << this->function_.name() << endl << endl;
        cout << "Input  Rows:" << this->function_.input_rows() << endl << endl;
        cout << "Output Rows:" << this->function_.output_rows() << endl << endl;
        cout << "Thread Policy:" << static_cast<int>(thread_mode_) << endl << endl;

        cout << "v_index_: " << endl << this->index_data_.get_v_index() << endl << endl;
        if (this->index_data_.cindex_init_) {
            cout << "c_index_: " << endl << this->index_data_.get_c_index() << endl << endl;
        }
    }

    /// @brief How many KKT slots one application of this piece claims when it is
    ///        asked for Jacobian space, Hessian space, or both.
    ///
    /// MUST BE ADDITIVE IN ITS TWO FLAGS: `num_kkt_elements(true, true)` must
    /// equal `num_kkt_elements(true, false) + num_kkt_elements(false, true)`.
    /// A layout counts its total with the both-flags call and splits that total
    /// per domain with the two single-flag calls, in one pass, so a piece that
    /// answers something else describes two different layouts depending on which
    /// question it is asked. It is REFUSED rather than trusted: the domain-
    /// contiguous claim stream a laid layout publishes is sized from the split
    /// and checked against the slots the lay actually handed out, and the two
    /// disagreeing is a named refusal
    /// (detail/model/claim_restatement.h, restate_claim_stream). Nothing else
    /// sees it, so a non-additive piece breaks the claim stream and never
    /// quietly moves a layout.
    int num_kkt_elements(bool dojac, bool dohess) {
        return this->function_.num_kkt_elements(dojac, dohess) * this->index_data_.num_appl();
    }
    int num_con_eles() const {
        return this->function_.output_rows() * this->index_data_.num_appl();
    }
    int num_grad_eles() const {
        return this->function_.input_rows() * this->index_data_.num_appl();
    }
    ThreadingFlags get_thread_mode() const { return this->thread_mode_; }

    /// @brief Sets the thread assignment policy this piece is laid out under.
    /// @param mode the policy the partitioner reads.
    /// @throws std::invalid_argument if a layout has already been laid over
    ///         this piece, naming it by name().
    ///
    /// The mode decides which work partition the piece lands in, so it decides
    /// claim order and therefore the structural key. It is declaration data:
    /// set it before the problem is laid. Changing it afterwards is a
    /// structural mutation and is reached by re-laying from a declaration that
    /// carries the new mode, never by writing the piece on hand.
    void set_thread_mode(ThreadingFlags mode) {
        if (this->thread_mode_laid_) {
            throw std::invalid_argument(fmt::format(
                "set_thread_mode: the layout on hand was laid over '{0}' with thread mode {1}, and "
                "the mode decides which partition claims its KKT slots; re-lay from a declaration "
                "carrying the new mode instead of writing the laid piece",
                this->function_.name(), static_cast<int>(this->thread_mode_)));
        }
        this->thread_mode_ = mode;
    }
    void get_kkt_space(EigenRef<VectorXi> KKTrows, EigenRef<VectorXi> KKTcols, int &freeloc,
                       int conoffset, bool dojac, bool dohess) {
        this->function_.get_kkt_space(KKTrows, KKTcols, freeloc, conoffset, dojac, dohess,
                                      this->index_data_);
    }
    void get_gradient_space(EigenRef<VectorXi> GXrows, int &freeloc) {
        this->index_data_.get_gradient_space(GXrows, freeloc);
    }
    void get_constraint_space(EigenRef<VectorXi> FXrows, int &freeloc) {
        this->index_data_.get_constraint_space(FXrows, freeloc);
    }

  private:
    /// @brief Freezes this piece's thread mode, as the lay that partitioned it
    ///        leaves it.
    ///
    /// PRIVATE, and reachable only from the type that lays layouts. A public
    /// pair of these would make the refusal advisory: two calls -- thaw, then
    /// write -- would move a laid piece's mode while the structural key, the
    /// structure epoch and the claim arrays all stood still, and the size-only
    /// guard on the declaration readers would not see it. There would be no
    /// diagnostic anywhere, and the declaration would report a mode the layout
    /// was not partitioned from.
    void mark_thread_mode_laid() { this->thread_mode_laid_ = true; }

    /// @brief Thaws this piece's thread mode, for a copy handed out as
    ///        declaration data rather than held as part of a layout. Paired
    ///        with mark_thread_mode_laid(), and private for the same reason.
    void clear_thread_mode_laid() { this->thread_mode_laid_ = false; }

    /// @brief Whether a layout has been laid over this piece.
    ///
    /// Private with its two setters, and read by the same one type: the layout
    /// uses it to tell a piece it laid from one a caller wrote into a master list
    /// afterwards, which is a distinction nothing else has any business drawing.
    bool thread_mode_is_laid() const { return this->thread_mode_laid_; }

    friend struct NonLinearProgram;

    ThreadingFlags thread_mode_ = ThreadingFlags::ByApplication;

    /// Whether a layout has been laid over this piece. Set by the lay, cleared
    /// on the copies handed out as declaration data, and what makes the mode
    /// frozen for as long as the structures that were partitioned from it
    /// stand.
    bool thread_mode_laid_ = false;
};

} // namespace hven::solvers
