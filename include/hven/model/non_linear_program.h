// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// This file defines the default composite non-linear program class
// for interfacing with InteriorPointSolver. This class is responsible for combining many different
// dense or sparse objective or constraints into a single optimization problem and
// manages all memory allocation, sparsity pattern computation, work partitioning, and function
// evaluation.

#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include <fmt/format.h>

#include "hven/detail/interior/bound_set.h"
#include "hven/detail/interior/constraint_function.h"
#include "hven/detail/interior/fixed_variable_row.h"
#include "hven/detail/interior/objective_function.h"
#include "hven/detail/interior/typedefs/eigen_types.h"
#include "hven/detail/interior/utils/thread_pool.h"
#include "hven/model/nlp_aggregate.h"

namespace hven::solvers {

/// How a primal variable whose declared lower and upper bounds are equal is
/// handed to the solver. All three are implemented, and all three reach the same
/// solution on a well-posed problem; they differ in the size of the system the
/// solver factorizes and in how exactly the variable sits at its value.
///
/// MakeParameter (the default) removes the variable from the optimization
/// entirely: it is pinned at its bound value for every evaluation and the Newton
/// system the solver factorizes is the system of the REMAINING variables, one
/// row and column narrower per fixed variable. Its value in the returned
/// solution is exact.
///
/// MakeConstraint keeps the variable free and adds one internal equality row
/// x_i - c = 0 per fixed variable, appended AFTER every row the transcription
/// declared, so the solved system is one row and one column WIDER per fixed
/// variable than MakeParameter's and every user row keeps its own index. The
/// variable reaches its value to equality-constraint tolerance rather than
/// exactly.
///
/// RelaxBounds keeps the variable as an ordinary two-sided bounded variable with
/// its bounds pushed apart by the relax factor, so it is held near its value by
/// the barrier, within the relaxation, and the system is the same size as the
/// declared problem's.
enum class FixedVariableTreatments { MakeParameter, MakeConstraint, RelaxBounds };

/// Human-readable name for a treatment, for diagnostics and error messages.
inline const char *fixed_variable_treatment_name(FixedVariableTreatments treatment) {
    switch (treatment) {
    case FixedVariableTreatments::MakeParameter:
        return "make_parameter";
    case FixedVariableTreatments::MakeConstraint:
        return "make_constraint";
    case FixedVariableTreatments::RelaxBounds:
        return "relax_bounds";
    }
    return "unknown";
}

/// Default widening applied to every finite, non-fixing variable bound before
/// it is recorded in the BoundSet: b is moved outward by this factor times
/// max(1, |b|). Matches Ipopt's bound_relax_factor default.
inline constexpr double kDefaultBoundRelaxFactor = 1.0e-8;

/// Largest widening the solver's settings accept. The relaxed box is the one
/// every barrier term actually divides by, so a large factor does not merely
/// loosen a bound -- it solves a measurably different problem than the one
/// declared, without saying so. A hundredth of max(1, |b|) is already far past
/// any tolerance a caller would set.
inline constexpr double kMaxBoundRelaxFactor = 1.0e-2;

/// Smallest per-partition KKT element count worth dispatching for. Below it the
/// thread-dispatch overhead dominates the work, so the partition count is capped
/// at num_user_kkt_elems_ / this.
inline constexpr int kMinKktElementsPerPartition = 1000;

/// @brief The partitioned evaluation engine, and a Level 2 provider.
///
/// assemble() reaches the same per-shape passes the eval_ entry points do,
/// call for call, over the same machinery; the only step moved out to the
/// consumer is the one the mapping table transfers (the solver-coefficient
/// scatter, which assemble deliberately does not do and the eval_ entries
/// still do).
///
/// THE FILL PATH, both halves, because the capability declaration turns on
/// the difference:
///
///   * The KKT fill is DIRECT. Each piece writes the consumer's value array in
///     place, at offsets its claim recorded (kkt_locations_), under the
///     canonical-column lock protocol. There is no provider-owned matrix and no
///     copy: this is the per-minor cost center and it is not paying for one.
///   * The RIGHT-HAND-SIDE fill goes through a provider-owned intermediate --
///     each piece accumulates into its own claim slots in rhs_coeffs_, and
///     fill_pgx/fill_agx/fill_fxe/fill_fxi then fold those slots into the
///     consumer's vectors through rhs_coeff_rows_.
///
/// THAT INTERMEDIATE IS REQUIRED, not merely tolerated, and the reason is
/// determinism rather than convenience. Several pieces claim rows of one
/// gradient, so an in-place scatter would have to lock per row, and the order
/// in which contending threads won those locks would decide the order the
/// floating-point additions happened in -- making the assembled right-hand
/// side depend on scheduling, and therefore on the evaluation-thread count.
/// Claim slots are contention-free by construction (one piece owns each), and
/// the fold that follows walks them in claim order, so the accumulation order
/// is a property of the layout alone: the same problem produces bit-identical
/// right-hand sides at any thread count. This library's pins rest on that
/// stability, and ON THE DETERMINISTIC PATH -- the default, and the path
/// every pin and measurement runs on -- it outranks the no-copy property:
/// removing the intermediate there would be a regression, not an
/// optimization. Accumulation-VALUE determinism is a property of this path,
/// not a library absolute: a future user-selectable max-performance fill may
/// relax it, exactly as threaded MKL already does, behind an explicit mode
/// choice -- never silently, and never as this path's default. What stays hard
/// everywhere, on every path and for every provider, is LAYOUT determinism:
/// claim order, structural keys, and location tables are untouched by that
/// option; keys, byte-stable pins, and warm-start identity rest on them, and
/// only the floating-point summation order is ever mode-dependent.
struct NonLinearProgram : public NlpAggregate {
    using VectorXi = Eigen::VectorXi;
    using VectorXd = Eigen::VectorXd;
    using MatrixXi = Eigen::MatrixXi;

    int num_partitions_ = 1;

    /// Objective functions that will be partitioned across work partitions
    /// (part_obj_).
    std::vector<ObjectiveFunction> objectives_;

    /// Equality constraint functions that will be partitioned across work
    /// partitions (part_eq_).
    std::vector<ConstraintFunction> equality_constraints_;

    /// Inequality constraint functions that will be partitioned across work
    /// partitions (part_iq_).
    std::vector<ConstraintFunction> inequality_constraints_;

    /// Per partition: the ObjectiveFunctions assigned to it.
    std::vector<std::vector<ObjectiveFunction>> part_obj_;

    /// Per partition: the equality_constraints_ assigned to it.
    std::vector<std::vector<ConstraintFunction>> part_eq_;

    /// Per partition: the inequality_constraints_ assigned to it.
    std::vector<std::vector<ConstraintFunction>> part_iq_;

    int primal_vars_ = 0; // Number of design variables
    int slack_vars_ = 0;  // Number of slack variables appended to problem. One for every inequalcon
    int equal_cons_ = 0;  // Number of equality constraints,
    int inequal_cons_ = 0; // Number of inequality constraints
    int kkt_dim_ = 0; // Edge dimension of KKT matrix: = primal_vars_ + slack_vars_ + equal_cons_ +
                      // inequal_cons_

    VectorXi kkt_coeff_rows_; // matched row indices
    VectorXi kkt_coeff_cols_; // matched col indices
    VectorXi kkt_coeff_part_ids_;
    VectorXi kkt_locations_;

    int num_user_kkt_elems_ = 0;
    int num_solver_kkt_elems_ = 0;
    int num_kkt_elems_ = 0;

    VectorXd solver_coeffs_;
    /// Start offset of the solver-supplied slack Jacobian data (usually ones).
    int slack_jac_data_start_;
    /// Start offset of the solver-supplied diagonal elements for inertia
    /// modification or least-norm solving.
    int primal_diags_data_start_;
    /// Start offset of the solver-supplied diagonals for the slack elements in
    /// the Hessian -- used by interior-point methods, zeros for SQP.
    int slack_diag_data_start_;
    /// Start offset of the solver-supplied equality pivots.
    int e_pivot_data_start_;
    /// Start offset of the solver-supplied inequality pivots.
    int i_pivot_data_start_;

    std::vector<std::mutex> kkt_locks_;
    int num_kkt_clashes_ = 0;

    /// [i] = -1 if no fill clash, [i] = mutex lock index otherwise
    VectorXi kkt_clashes_;

    VectorXd rhs_coeffs_;
    VectorXi rhs_coeff_rows_;

    int num_pgx_elems_ = 0;
    int num_agx_elems_ = 0;
    int num_icon_elems_ = 0;
    int num_econ_elems_ = 0;
    int num_rhs_elems_ = 0;

    int pgx_data_start_ = 0;
    int agx_data_start_ = 0;
    int econ_data_start_ = 0;
    int icon_data_start_ = 0;

    NonLinearProgram(int NumParts) { this->num_partitions_ = std::max(NumParts, 1); }
    NonLinearProgram(int PV, int EQ, int IQ, std::vector<ObjectiveFunction> &obj,
                     std::vector<ConstraintFunction> &eq, std::vector<ConstraintFunction> &ineq,
                     int NumParts) {
        this->num_partitions_ = std::max(NumParts, 1);

        this->objectives_ = obj;
        this->equality_constraints_ = eq;
        this->inequality_constraints_ = ineq;
        this->make_nlp(PV, EQ, IQ);
    }

    /// @brief Lays the whole problem out over PV primal variables, EQ equality
    ///        rows and IQ inequality rows, from the objective and constraint
    ///        lists as they stand.
    ///
    /// EQ is the count of equality rows the USER's own constraint functions
    /// address. The MakeConstraint fixed-variable treatment appends its internal
    /// fixing rows on top of that count, so equal_cons_ is EQ only while no such
    /// row is installed; a caller re-materializing an already configured NLP
    /// hands back user_equal_cons_ (which is EQ as passed here), not equal_cons_.
    /// This call drops any installed fixing row itself -- it re-materializes the
    /// bounds those rows were derived from, so the next configuration re-derives
    /// them.
    void make_nlp(int PV, int EQ, int IQ);

    /// @brief Adopts a declaration whose row counts are the row space AS LAID,
    ///        fixing rows included, and lays the problem out from it.
    ///
    /// equality_rows_ is the AS-LAID count, fixing rows included; this entry
    /// lifts the declared fixing rows off the equality tail, lays from the user
    /// count that leaves, and appends them again over the same chain the
    /// treatment that produced them uses.
    ///
    /// The order of operations, which is observable through the bound merge:
    ///   1. @p declaration.validate() -- nothing is touched until it passes.
    ///   2. the three piece lists move in, each piece taking its declared
    ///      thread mode.
    ///   3. the staged bounds are cleared, then the declared records are
    ///      replayed through set_variable_bound() in DECLARATION ORDER. A record
    ///      that is unbounded on both sides narrows nothing and is dropped
    ///      there, so the replayed list is the declared one less those; the
    ///      merged bounds, and therefore the key, are the same either way.
    ///   4. the partition count becomes the declared one -- a request the lay's
    ///      cap may reduce; declaration() reports the adopted count.
    ///   5. the layout runs, from the declared counts less
    ///      @p declaration.fixing_rows_.
    ///   6. the lifted fixing rows are appended again and the layout is re-laid
    ///      over them.
    ///
    /// Steps 5 and 6 are the history the source layout was laid by -- a
    /// user-row lay, then the internal rows -- which is what makes the round
    /// trip exact with fixing rows present: the claim stream, the row space and
    /// the adopting problem's own fixing-row count all come back the same.
    ///
    /// What does NOT come back is the CLASSIFICATION those rows were derived
    /// from: the adopting problem reports the fixing rows through
    /// internal_fixed_constraints() while fixed_variable_indices() is
    /// UNCHANGED -- empty on a target that never configured a treatment, and
    /// otherwise still whatever its last one recorded -- until the next
    /// configure_variable_treatment discards the rows and re-derives both from
    /// the replayed bounds.
    ///
    /// EVERY REFUSAL IS MADE BEFORE ANYTHING MOVES: a refused adoption leaves
    /// the problem on hand exactly as it was, down to its layout, its stored
    /// declaration and its structure epoch.
    ///
    /// @param declaration the pieces, thread modes, dimensions, bounds and
    ///                    requested partition count to lay from; moved out of.
    /// @throws std::invalid_argument through @p declaration.validate() -- a
    ///         non-positive partition count, negative dimensions, piece row
    ///         counts that do not sum to the declared row counts, a piece row
    ///         sum past INT_MAX, an out-of-range bound index, a NaN bound, an
    ///         inverted single record or an empty bound intersection; and a
    ///         fixing-row count the tail of the equality list does not have one
    ///         single-row piece each for.
    /// @throws std::invalid_argument from this entry itself if the declaration
    ///         carries no pieces but declares equality or inequality rows (this
    ///         entry lays a problem out of its pieces); if a declared internal
    ///         fixing row names other than exactly one constraint row; or if
    ///         one names an equality row outside the [declared equality rows
    ///         less the fixing-row count, declared equality rows) band.
    void adopt_declaration(AggregateDeclaration declaration);

    /// One staged variable-bound declaration, as handed to set_variable_bound.
    /// Recorded verbatim (no merging at declaration time) so that repeated
    /// make_nlp calls re-derive the same tightest-wins result from the same
    /// history.
    struct VariableBoundStage {
        int index_;
        double lower_;
        double upper_;
    };

    /// Staged variable-bound declarations, applied by make_nlp. Declaration
    /// may precede sizing (primal_vars_ is only known once make_nlp runs), so
    /// index-range validation and the tightest-wins merge both happen at
    /// materialization time, not here. Cleared only by clear_variable_bounds().
    std::vector<VariableBoundStage> staged_variable_bounds_;

    /// Dense per-primal-variable bounds materialized from
    /// staged_variable_bounds_ by make_nlp. Length primal_vars_ after
    /// make_nlp; -inf/+inf where no staged bound narrows the variable. Empty
    /// (size 0) before the first make_nlp call or after clear_variable_bounds().
    VectorXd x_lower_;
    VectorXd x_upper_;

    /// @brief Stages a bound declaration for primal variable global_index.
    ///
    /// Repeated declarations on the same index are intersected (tightest wins)
    /// when make_nlp materializes x_lower_/x_upper_: l = max(l_prev, l_new),
    /// u = min(u_prev, u_new). A declaration with both bounds infinite leaves
    /// the variable unbounded and is a no-op.
    ///
    /// @throws std::invalid_argument on a NaN bound, since it cannot
    ///         participate in the max/min merge.
    void set_variable_bound(int global_index, double lower, double upper) {
        if (std::isnan(lower) || std::isnan(upper)) {
            throw std::invalid_argument(
                fmt::format("set_variable_bound: bound for index {0} is NaN (lower={1}, "
                            "upper={2})",
                            global_index, lower, upper));
        }
        constexpr double kInf = std::numeric_limits<double>::infinity();
        if (lower == -kInf && upper == kInf) {
            return;
        }
        this->staged_variable_bounds_.push_back({global_index, lower, upper});
    }

    /// Drops every staged declaration and the materialized x_lower_/x_upper_
    /// vectors. The next make_nlp call starts from an unbounded problem.
    void clear_variable_bounds() {
        this->staged_variable_bounds_.clear();
        this->x_lower_.resize(0);
        this->x_upper_.resize(0);
        this->bounds_revision_++;
    }

    /// True iff any primal variable has a finite lower or upper bound after
    /// materialization. False before the first make_nlp call.
    bool has_variable_bounds() const {
        if (this->x_lower_.size() == 0) {
            return false;
        }
        constexpr double kInf = std::numeric_limits<double>::infinity();
        return (this->x_lower_.array() > -kInf).any() || (this->x_upper_.array() < kInf).any();
    }

    // Bound-fixed variable treatment
    //
    // A variable declared with lower == upper carries no degree of freedom.
    // Under the MakeParameter treatment it is ELIMINATED: it does not appear in
    // the solver's variable space at all, and the KKT system the solver
    // factorizes is the system of the remaining variables -- narrower by exactly
    // one row and column per eliminated variable. The elimination splits each
    // function's index map by role: the INPUT map is left alone -- a function
    // still reads exactly the variables it was declared over, out of a
    // full-space buffer built from the reduced iterate plus the pinned values --
    // and only the OUTPUT map is rewritten, at configuration time, from the
    // pristine input map: retained variables renumbered into the reduced space,
    // eliminated ones marked -1, and the KKT/RHS location tables, sparsity
    // pattern, clash marks and solver-coefficient ranges rebuilt over it.
    //
    // Element CLAIMS stay exactly as they were -- same count, same contiguous
    // per-application ranges -- because the scatters walk their claims in
    // lockstep with the function's own loop bounds; a -1 element keeps its claim
    // and simply names no matrix entry.
    //
    // Identity fast path. With no fixed variables nothing is rewritten at all:
    // no output map is installed and no expansion buffer is built. The other two
    // treatments take that path throughout -- neither eliminates anything -- and
    // differ only in what classification records: MakeConstraint records no
    // bound for the variable (it goes to the solver free) and appends one
    // internal equality row per fixed variable (re-laying the layout over the
    // widened row space), RelaxBounds records an ordinary relaxed bound pair
    // (changing nothing structural).

    /// @brief Classifies every primal variable against the materialized
    ///        x_lower_/x_upper_ (free / lower-only / upper-only / two-sided /
    ///        fixed), records the finite bounds of everything the treatment
    ///        leaves bounded in variable_bound_set_, and then applies the
    ///        treatment to the fixed variables: under MakeParameter rewriting
    ///        every function's output map into the reduced space and rebuilding
    ///        the KKT/RHS structures over it, under MakeConstraint appending one
    ///        internal equality row per fixed variable and re-laying the layout
    ///        over the widened row space, and under RelaxBounds nothing
    ///        structural at all.
    ///
    /// Called once at solve setup. Idempotent and re-entrant: a call that
    /// repeats the same treatment, the same relax factor, and the same bound
    /// state returns immediately having changed nothing; a call that changes the
    /// treatment, the factor, or arrives after the bounds were re-materialized
    /// (make_nlp / clear_variable_bounds) reclassifies from scratch. Every
    /// rewritten output map is regenerated from the pristine input map, which is
    /// never edited, and every internal fixing row is dropped before the new
    /// classification derives its own, so repeated configuration cannot compound.
    ///
    /// @return true iff this call rebuilt the KKT/RHS structures, in which case
    ///         the caller must re-read the dimensions and recompute the sparsity
    ///         pattern (InteriorPointSolver does both at its solve entry). false
    ///         means nothing changed.
    ///
    /// @throws std::invalid_argument for an unrecognized treatment, for a
    ///         negative or non-finite relax factor, for an infinite fixing
    ///         value, for a zero relax factor under RelaxBounds while a variable
    ///         is fixed (the pair would be pushed nowhere and the barrier would
    ///         divide by zero), and, under MakeParameter only, for a problem all
    ///         of whose variables are fixed.
    bool configure_variable_treatment(FixedVariableTreatments treatment, double bound_relax_factor);

    /// Width of the primal block of the KKT system the solver factorizes:
    /// primal_vars_ minus the eliminated variables. This is the size of every
    /// primal vector the solver works with, and the size gather_reduced_x
    /// compacts to and scatter_full_x expands from. Equal to primal_vars_ on the
    /// identity path.
    int reduced_primal_vars() const { return this->reduced_primal_vars_count_; }

    /// True iff at least one variable was eliminated by the configured
    /// treatment. The guard for every piece of reduction work outside the
    /// scatters (which carry their own, per-function): when it is false the
    /// solver runs exactly the code it ran before the treatment existed.
    bool is_reduced() const { return this->fixed_reduction_active_; }

    /// The finite bounds that survived classification (variables the treatment
    /// eliminated or handed to an internal equality row excluded), in REDUCED
    /// indices -- the same space every primal vector the solver holds is in --
    /// and relaxed by the configured factor. Empty before
    /// configure_variable_treatment has run.
    const BoundSet &variable_bound_set() const { return this->variable_bound_set_; }

    /// Indices of the variables the treatment acted on -- the ones whose declared
    /// bounds are equal -- in the FULL problem space, ascending. Empty when
    /// nothing is fixed, whatever the treatment.
    const VectorXi &fixed_variable_indices() const { return this->fixed_idx_; }

    /// Values those variables are fixed at, parallel to
    /// fixed_variable_indices(). Under MakeParameter they are pinned there
    /// exactly; under the other two treatments they are what the internal
    /// equality row, or the relaxed bound pair, holds the variable to.
    const VectorXd &fixed_variable_values() const { return this->fixed_vals_; }

    /// Number of internal equality rows the MakeConstraint treatment currently
    /// has installed: one per fixed variable, at the tail of
    /// equality_constraints_ and of the equality row space. Zero under every
    /// other treatment, and zero whenever nothing is fixed.
    int internal_fixed_constraints() const { return this->internal_fixed_cons_; }

    /// Full-space index -> reduced index, -1 for an eliminated variable. Empty
    /// on the identity path (where the map is the identity).
    const VectorXi &full_to_reduced() const { return this->full_to_reduced_; }

    /// Reduced index -> full-space index. Empty on the identity path.
    const VectorXi &reduced_to_full() const { return this->reduced_to_full_; }

    /// @brief Compacts a full-space primal vector into reduced_primal_vars()
    ///        entries, dropping the eliminated coordinates. Used at the solve
    ///        entry to turn a caller's initial guess into the solver's iterate.
    ///        Pass-through copy on the identity path.
    /// @throws std::invalid_argument on a size mismatch.
    void gather_reduced_x(ConstEigenRef<VectorXd> x_full, EigenRef<VectorXd> x_reduced) const;

    /// @brief Expands a reduced primal vector back to primal_vars_ entries,
    ///        writing each eliminated coordinate's pinned value.
    ///
    /// Two callers, and they are the whole reinsertion story: every evaluation
    /// builds the full-space buffer the functions read with it, and the solver
    /// uses it once at its return boundary to hand the solution back in the
    /// caller's own variable space. Pass-through copy on the identity path.
    ///
    /// @throws std::invalid_argument on a size mismatch.
    void scatter_full_x(ConstEigenRef<VectorXd> x_reduced, EigenRef<VectorXd> x_full) const;

    /// @brief The full-space primal vector the objective and constraint
    ///        functions must read, given the solver's reduced iterate.
    ///
    /// Returns a view of @p x itself on the identity path -- no buffer, no copy.
    /// Otherwise returns a view of an internal buffer, valid until the next
    /// call. Like vals_scratch_, that buffer is safe to share because a single
    /// NLP is inside at most one evaluation at a time; the parallelism inside an
    /// evaluation only reads it.
    Eigen::Ref<const VectorXd> primal_view(ConstEigenRef<VectorXd> x) {
        if (!this->fixed_reduction_active_) {
            return x;
        }
        this->full_x_scratch_.resize(this->primal_vars_);
        this->scatter_full_x(x, this->full_x_scratch_);
        return this->full_x_scratch_;
    }

    /// Selected treatment, as last configured.
    FixedVariableTreatments variable_treatment_ = FixedVariableTreatments::MakeParameter;
    /// Relax factor, as last configured.
    double bound_relax_factor_ = 0.0;

    /// Bumped whenever x_lower_/x_upper_ are (re)materialized or dropped, so
    /// configure_variable_treatment can tell a repeat call from a real change.
    /// Staging a declaration does NOT bump it: staged declarations only reach
    /// x_lower_/x_upper_ through make_nlp, which does.
    long bounds_revision_ = 0;

    /// True once a classification pass has produced structures consistent with
    /// the current bounds. make_nlp clears it: it rebuilds the whole layout from
    /// the pristine full space, so whatever the previous configuration derived
    /// is gone.
    bool fixed_treatment_valid_ = false;
    long configured_bounds_revision_ = -1;

    /// Cached is_reduced() answer -- the bool the evaluation-path guards outside
    /// the scatters read.
    bool fixed_reduction_active_ = false;
    int reduced_primal_vars_count_ = 0;

    VectorXi full_to_reduced_; ///< primal_vars_ entries, -1 where eliminated.
    VectorXi reduced_to_full_; ///< reduced_primal_vars_count_ entries.
    VectorXi fixed_idx_;       ///< Fixed variable indices, ascending.
    VectorXd fixed_vals_;      ///< The values they are fixed at.

    /// Equality-row count of the user's own constraint functions, as handed to
    /// make_nlp. equal_cons_ counts the internal fixing rows the MakeConstraint
    /// treatment installs on top of it, and this is the count the row space
    /// returns to when they are dropped.
    int user_equal_cons_ = 0;

    /// Internal fixing rows currently installed -- see
    /// internal_fixed_constraints(). They occupy the tail of
    /// equality_constraints_ and the tail of the equality row space, so dropping
    /// them is a truncation at both ends and no user row is ever renumbered.
    int internal_fixed_cons_ = 0;

    /// Full-space buffer the objective and constraint functions read while the
    /// solver iterates in the reduced space -- reduced values in their own
    /// coordinates plus the pinned ones. Empty and untouched on the identity
    /// path. See primal_view().
    VectorXd full_x_scratch_;

    BoundSet variable_bound_set_;

    void print_data() {
        for (int i = 0; i < this->num_partitions_; i++) {
            std::cout << "Partition: " << i << std::endl << std::endl;
            std::cout << "---------------objectives_---------------" << std::endl << std::endl;

            for (auto &obj : this->part_obj_[i]) {
                obj.print_data();
            }

            std::cout << "---------------Equalities---------------" << std::endl << std::endl;

            for (auto &eq : this->part_eq_[i]) {
                eq.print_data();
            }
            std::cout << "--------------Inequalities--------------" << std::endl << std::endl;

            for (auto &ineq : this->part_iq_[i]) {
                ineq.print_data();
            }
        }
    }

    void count_elems();

    void materialize_variable_bounds();

    void analyze_partitioning();

    void get_mat_space();

    void get_rhs_space();

    void set_mat_dimensions();

    void set_rhs_dimensions();

    void finalize_data();

    /// The set_mat_dimensions -> finalize_data chain: everything whose layout
    /// depends on the width of the solver's primal block. Run once by make_nlp
    /// over the full space, and again by configure_variable_treatment whenever
    /// that width changes.
    void rebuild_structures();

    /// count_elems -> analyze_partitioning: the part of make_nlp's tail that
    /// depends on WHICH functions the problem has rather than on how wide its
    /// primal block is. Run whenever that set changes -- which is whenever the
    /// MakeConstraint treatment installs or drops its internal fixing rows -- and
    /// always followed by a map install-or-clear and rebuild_structures(), in
    /// that order, because re-partitioning hands out fresh copies of the master
    /// functions and those copies carry the pristine input map.
    ///
    /// make_nlp's partition CAP is deliberately not repeated: the only thing that
    /// reaches here adds work rather than removing it, so re-capping could only
    /// ever widen the partitioning, and holding it where the transcription left it
    /// keeps a treatment switch from silently re-partitioning the problem.
    void refresh_function_partitions();

    /// Appends one internal equality row per entry of fixed_idx_/fixed_vals_ and
    /// grows equal_cons_ by that many. The layout is NOT re-laid here; the caller
    /// owns that (see refresh_function_partitions).
    void install_fixed_variable_rows();

    /// @brief Splices already-built internal fixing rows onto the tail of the
    ///        equality list and records them.
    /// @param rows the rows to append, moved out of; one row each.
    ///
    /// The ONE place the internal_fixed_cons_ / equal_cons_ pair is written by
    /// an install, so the two callers that install rows -- the treatment, which
    /// builds its rows from the classification, and the adoption entry, which
    /// carries the declared ones across its lay -- cannot drift apart on the
    /// bookkeeping the make_nlp invariant checks. All or nothing: a throw
    /// part-way through leaves the list and the count agreeing.
    ///
    /// The layout is NOT re-laid here; the caller owns that.
    void splice_fixed_variable_rows(std::vector<ConstraintFunction> rows);

    /// Truncates the internal fixing rows off equality_constraints_ and returns
    /// equal_cons_ to user_equal_cons_. A no-op when none are installed. The
    /// layout is NOT re-laid here either: every caller either re-lays it or is
    /// about to overwrite the whole thing.
    void discard_fixed_variable_rows();

    /// Rewrites every partitioned function's output map into the reduced space
    /// from its pristine input map (-1 where the variable is eliminated).
    void install_function_output_maps();

    /// Puts every partitioned function back on its pristine input map.
    void clear_function_output_maps();

    void analyze_sparsity(Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);
    void make_compressed() {
        // The claim digest is taken over the two arrays this drops, and it is
        // deferred, so it is digested HERE before its source goes away -- the
        // one place a laid layout's claim stream stops being readable without
        // a re-lay having replaced it.
        (void)this->claim_digest();
        this->kkt_coeff_part_ids_.resize(0);
        this->kkt_coeff_rows_.resize(0);
        this->kkt_coeff_cols_.resize(0);
    }

    /// Views into the solver-coefficient block of solver_coeffs_, and the
    /// matching column/row index views into kkt_coeff_cols_/kkt_coeff_rows_.
    EigenRef<VectorXd> slack_coeffs() {
        return this->solver_coeffs_.segment(this->slack_jac_data_start_, this->slack_vars_);
    }
    EigenRef<VectorXd> primal_diag_coeffs() {
        return this->solver_coeffs_.segment(this->primal_diags_data_start_,
                                            this->reduced_primal_vars_count_);
    }
    EigenRef<VectorXd> slack_diag_coeffs() {
        return this->solver_coeffs_.segment(this->slack_diag_data_start_, this->slack_vars_);
    }
    EigenRef<VectorXd> e_pivot_coeffs() {
        return this->solver_coeffs_.segment(this->e_pivot_data_start_, this->equal_cons_);
    }
    EigenRef<VectorXd> i_pivot_coeffs() {
        return this->solver_coeffs_.segment(this->i_pivot_data_start_, this->inequal_cons_);
    }

    EigenRef<VectorXi> slack_coeff_cols() {
        return this->kkt_coeff_cols_.segment(
            this->slack_jac_data_start_ + this->num_user_kkt_elems_, this->slack_vars_);
    }
    EigenRef<VectorXi> primal_diag_coeff_cols() {
        return this->kkt_coeff_cols_.segment(this->primal_diags_data_start_ +
                                                 this->num_user_kkt_elems_,
                                             this->reduced_primal_vars_count_);
    }
    EigenRef<VectorXi> slack_diag_coeff_cols() {
        return this->kkt_coeff_cols_.segment(
            this->slack_diag_data_start_ + this->num_user_kkt_elems_, this->slack_vars_);
    }
    EigenRef<VectorXi> e_pivot_coeff_cols() {
        return this->kkt_coeff_cols_.segment(this->e_pivot_data_start_ + this->num_user_kkt_elems_,
                                             this->equal_cons_);
    }
    EigenRef<VectorXi> i_pivot_coeff_cols() {
        return this->kkt_coeff_cols_.segment(this->i_pivot_data_start_ + this->num_user_kkt_elems_,
                                             this->inequal_cons_);
    }

    EigenRef<VectorXi> slack_coeff_rows() {
        return this->kkt_coeff_rows_.segment(
            this->slack_jac_data_start_ + this->num_user_kkt_elems_, this->slack_vars_);
    }
    EigenRef<VectorXi> primal_diag_coeff_rows() {
        return this->kkt_coeff_rows_.segment(this->primal_diags_data_start_ +
                                                 this->num_user_kkt_elems_,
                                             this->reduced_primal_vars_count_);
    }
    EigenRef<VectorXi> slack_diag_coeff_rows() {
        return this->kkt_coeff_rows_.segment(
            this->slack_diag_data_start_ + this->num_user_kkt_elems_, this->slack_vars_);
    }
    EigenRef<VectorXi> e_pivot_coeff_rows() {
        return this->kkt_coeff_rows_.segment(this->e_pivot_data_start_ + this->num_user_kkt_elems_,
                                             this->equal_cons_);
    }
    EigenRef<VectorXi> i_pivot_coeff_rows() {
        return this->kkt_coeff_rows_.segment(this->i_pivot_data_start_ + this->num_user_kkt_elems_,
                                             this->inequal_cons_);
    }

    void set_primal_diags(const Eigen::VectorXd &pdiags) { this->primal_diag_coeffs() = pdiags; }
    void set_primal_diags(double val) { this->primal_diag_coeffs().setConstant(val); }
    void set_slack_diags(const Eigen::VectorXd &sdiags) { this->slack_diag_coeffs() = sdiags; }
    void set_slack_diags(double val) { this->slack_diag_coeffs().setConstant(val); }
    void set_e_pivots(const Eigen::VectorXd &epivs) { this->e_pivot_coeffs() = epivs; }
    void set_e_pivots(double val) { this->e_pivot_coeffs().setConstant(val); }
    void set_i_pivots(const Eigen::VectorXd &ipivs) { this->i_pivot_coeffs() = ipivs; }
    void set_i_pivots(double val) { this->i_pivot_coeffs().setConstant(val); }
    void set_slacks_ones() { this->slack_coeffs().setConstant(1.0); }

    void fill_solver_coeffs(Eigen::SparseMatrix<double, Eigen::RowMajor> &mat) {
        auto FillOp = [&](int start, int stop) {
            for (int i = start; i < stop; i++) {
                mat.valuePtr()[this->kkt_locations_.tail(this->num_solver_kkt_elems_)[i]] +=
                    this->solver_coeffs_[i];
            }
        };

        hven::utils::parallel_blocks(this->num_solver_kkt_elems_, FillOp, this->num_partitions_);
    }

    void assign_kkt_slack_hessian(const Eigen::Ref<const Eigen::VectorXd> &slhs,
                                  Eigen::SparseMatrix<double, Eigen::RowMajor> &mat) {
        int ofs = this->slack_diag_data_start_ + this->num_user_kkt_elems_;
        for (int i = 0; i < this->slack_vars_; i++) {
            mat.valuePtr()[this->kkt_locations_[ofs + i]] = slhs[i];
        }
    }
    void perturb_kkt_p_diags(double pert, Eigen::SparseMatrix<double, Eigen::RowMajor> &mat) {
        int ofs = this->primal_diags_data_start_ + this->num_user_kkt_elems_;
        for (int i = 0; i < this->reduced_primal_vars_count_; i++) {
            mat.valuePtr()[this->kkt_locations_[ofs + i]] += pert;
        }
    }
    // Post-assembly `+=` onto every constraint-row diagonal slot (the equality
    // and inequality pivot ranges), the mirror of perturb_kkt_p_diags over the
    // constraint block. The proximal primal-dual regularization mode applies
    // the dual shift (−δ_c) here as part of the base matrix, after the KKT
    // assembly and before the first factorization; the classic (default) mode
    // calls it on demand, at most once per phase, when a factorization reports
    // the singularity signal (see InteriorPointSolver::factor_impl). Until either happens
    // these slots hold 0.0.
    void perturb_kkt_c_diags(double pert, Eigen::SparseMatrix<double, Eigen::RowMajor> &mat) {
        int eofs = this->e_pivot_data_start_ + this->num_user_kkt_elems_;
        for (int i = 0; i < this->equal_cons_; i++) {
            mat.valuePtr()[this->kkt_locations_[eofs + i]] += pert;
        }
        int iofs = this->i_pivot_data_start_ + this->num_user_kkt_elems_;
        for (int i = 0; i < this->inequal_cons_; i++) {
            mat.valuePtr()[this->kkt_locations_[iofs + i]] += pert;
        }
    }

    // Views into the right-hand-side coefficient arena of rhs_coeffs_.
    EigenRef<VectorXd> pgx_coeffs() {
        return this->rhs_coeffs_.segment(this->pgx_data_start_, this->num_pgx_elems_);
    }
    EigenRef<VectorXd> agx_coeffs() {
        return this->rhs_coeffs_.segment(this->agx_data_start_, this->num_agx_elems_);
    }
    EigenRef<VectorXd> econ_coeffs() {
        return this->rhs_coeffs_.segment(this->econ_data_start_, this->num_econ_elems_);
    }
    EigenRef<VectorXd> icon_coeffs() {
        return this->rhs_coeffs_.segment(this->icon_data_start_, this->num_icon_elems_);
    }

    EigenRef<VectorXi> pgx_coeff_rows() {
        return this->rhs_coeff_rows_.segment(this->pgx_data_start_, this->num_pgx_elems_);
    }
    EigenRef<VectorXi> agx_coeff_rows() {
        return this->rhs_coeff_rows_.segment(this->agx_data_start_, this->num_agx_elems_);
    }
    EigenRef<VectorXi> econ_coeff_rows() {
        return this->rhs_coeff_rows_.segment(this->econ_data_start_, this->num_econ_elems_);
    }
    EigenRef<VectorXi> icon_coeff_rows() {
        return this->rhs_coeff_rows_.segment(this->icon_data_start_, this->num_icon_elems_);
    }

    EigenRef<VectorXi> get_kkt_locations() {
        return this->kkt_locations_.head(this->num_user_kkt_elems_);
    }
    EigenRef<VectorXi> get_kkt_clashes() {
        return this->kkt_clashes_.head(this->reduced_primal_vars_count_);
    }

    void set_con_coeffs_zero() {
        this->econ_coeffs().setZero();
        this->icon_coeffs().setZero();
    }
    void set_pgx_coeffs_zero() { this->pgx_coeffs().setZero(); }
    void set_agx_coeffs_zero() { this->agx_coeffs().setZero(); }
    void set_rhs_coeffs_zero() {
        this->set_pgx_coeffs_zero();
        this->set_agx_coeffs_zero();
        this->set_con_coeffs_zero();
    }

    // Gradient rows for eliminated variables are marked -1 by get_gradient_space
    // (the claim keeps its size -- the function still writes a gradient value
    // per argument -- but that value has no row to be summed into). The skip is
    // hoisted to the call, so the unreduced fill loop is the original one and
    // the whole cost of the feature here is one branch per fill.
    void fill_pgx(EigenRef<VectorXd> PGX) {
        if (this->fixed_reduction_active_) {
            rhs_fill_op_reduced(PGX, this->pgx_coeffs(), this->pgx_coeff_rows());
        } else {
            rhs_fill_op(PGX, this->pgx_coeffs(), this->pgx_coeff_rows());
        }
    }
    void fill_agx(EigenRef<VectorXd> AGX) {
        if (this->fixed_reduction_active_) {
            rhs_fill_op_reduced(AGX, this->agx_coeffs(), this->agx_coeff_rows());
        } else {
            rhs_fill_op(AGX, this->agx_coeffs(), this->agx_coeff_rows());
        }
    }
    void fill_fxe(EigenRef<VectorXd> FXE) {
        this->rhs_fill_op(FXE, this->econ_coeffs(), this->econ_coeff_rows());
    }
    void fill_fxi(EigenRef<VectorXd> FXI) {
        this->rhs_fill_op(FXI, this->icon_coeffs(), this->icon_coeff_rows());
    }
    void fill_rhs(EigenRef<VectorXd> PGX, EigenRef<VectorXd> AGX, EigenRef<VectorXd> FXE,
                  EigenRef<VectorXd> FXI) {
        this->fill_pgx(PGX);
        this->fill_agx(AGX);
        this->fill_fxe(FXE);
        this->fill_fxi(FXI);
    }

    static void rhs_fill_op(EigenRef<VectorXd> target, EigenRef<VectorXd> source,
                            EigenRef<VectorXi> sourcelocs) {
        for (int i = 0; i < source.size(); i++) {
            target[sourcelocs[i]] += source[i];
        }
    }

    /// rhs_fill_op over a location table that may carry -1 for the gradient rows
    /// of eliminated variables. Those rows are dropped: an eliminated variable's
    /// stationarity row is not part of the reduced problem's residual, and its
    /// value at a solution is the bound multiplier that holds the variable at
    /// its bound. Reached only under is_reduced().
    static void rhs_fill_op_reduced(EigenRef<VectorXd> target, EigenRef<VectorXd> source,
                                    EigenRef<VectorXi> sourcelocs) {
        for (int i = 0; i < source.size(); i++) {
            const int row = sourcelocs[i];
            if (row >= 0) {
                target[row] += source[i];
            }
        }
    }

    /// Per-partition objective/value accumulator scratch, shared across
    /// eval_rhs/eval_ogc/eval_occ/eval_obj/eval_kkt/eval_aug. Each of those
    /// entry points is only ever invoked serially on this NLP instance -- a
    /// single NLP is inside at most one alg_impl call at a time (the outer
    /// control loop is single-threaded; the only concurrency is the
    /// parallel_sequence dispatch *within* one call, which writes disjoint
    /// vals_scratch_[thrnum] entries). Resized in place (assign() re-zeros
    /// without a realloc once sized to num_partitions_).
    std::vector<double> vals_scratch_;

    void eval_rhs(double ObjScale, ConstEigenRef<VectorXd> X, ConstEigenRef<VectorXd> LE,
                  ConstEigenRef<VectorXd> LI, double &val, EigenRef<VectorXd> PGX,
                  EigenRef<VectorXd> AGX, EigenRef<VectorXd> FXE, EigenRef<VectorXd> FXI);

    void eval_ogc(double ObjScale, ConstEigenRef<VectorXd> X, double &val, EigenRef<VectorXd> PGX,
                  EigenRef<VectorXd> FXE, EigenRef<VectorXd> FXI);

    void eval_occ(double ObjScale, ConstEigenRef<VectorXd> X, double &val, EigenRef<VectorXd> FXE,
                  EigenRef<VectorXd> FXI);

    void eval_obj(double ObjScale, ConstEigenRef<VectorXd> X, double &val);

    void eval_kkt(double ObjScale, ConstEigenRef<VectorXd> X, ConstEigenRef<VectorXd> LE,
                  ConstEigenRef<VectorXd> LI, double &val, EigenRef<VectorXd> PGX,
                  EigenRef<VectorXd> AGX, EigenRef<VectorXd> FXE, EigenRef<VectorXd> FXI,
                  Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);

    void eval_kkt_no(double ObjScale, ConstEigenRef<VectorXd> X, ConstEigenRef<VectorXd> LE,
                     ConstEigenRef<VectorXd> LI, double &val, EigenRef<VectorXd> PGX,
                     EigenRef<VectorXd> AGX, EigenRef<VectorXd> FXE, EigenRef<VectorXd> FXI,
                     Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);

    void eval_soe(double ObjScale, ConstEigenRef<VectorXd> X, ConstEigenRef<VectorXd> LE,
                  ConstEigenRef<VectorXd> LI, double &val, EigenRef<VectorXd> PGX,
                  EigenRef<VectorXd> AGX, EigenRef<VectorXd> FXE, EigenRef<VectorXd> FXI,
                  Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);

    void eval_aug(double ObjScale, ConstEigenRef<VectorXd> X, ConstEigenRef<VectorXd> LE,
                  ConstEigenRef<VectorXd> LI, double &val, EigenRef<VectorXd> PGX,
                  EigenRef<VectorXd> AGX, EigenRef<VectorXd> FXE, EigenRef<VectorXd> FXI,
                  Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);

    static void nlp_test(const Eigen::VectorXd &x, int n, std::shared_ptr<NonLinearProgram> nlp1,
                         std::shared_ptr<NonLinearProgram> nlp2);

    /// The declaration as of the last lay, and deliberately not as of now.
    ///
    /// Stored state, refreshed by rebuild_structures() and by nothing else, so
    /// it describes the structures actually on hand. Staging a bound
    /// declaration does not touch it -- staged declarations only reach the
    /// layout through make_nlp -- which is exactly what "unchanging except
    /// across a structural mutation" asks for, and what keeps the structural
    /// key from moving under a consumer between two evaluations of one layout.
    ///
    /// THE ROW SPACE IS THE ROW SPACE AS LAID. The MakeConstraint treatment
    /// installs one internal equality row per fixed variable, and those rows are
    /// APPENDED AFTER EVERY ROW THE TRANSCRIPTION DECLARED. No row a
    /// transcription named is ever renumbered, every declared global row
    /// identity survives a treatment change, and the row space grows only at its
    /// tail.
    ///
    /// TREATMENT CONFIGURATION IS A STRUCTURAL MUTATION and is treated as one
    /// throughout: visible through declaration(), through model_structure_key()
    /// and through structure_epoch(), exactly as any other structural mutation
    /// is. What it is NOT is visible in vector INDEXING -- nothing a consumer
    /// addresses by declared identity moves under it.
    ///
    /// MATERIALIZED ON FIRST READ AFTER A LAY, then held. The reference is to
    /// stored state and stays valid and unchanging until the next structural
    /// mutation; a re-lay leaves behind the OBLIGATION to refill the piece lists
    /// and bound records, which the first read after it discharges once. A
    /// consumer that never reads the declaration never pays for one, and no
    /// consumer pays per call.
    ///
    /// A REFERENCE DOES NOT SURVIVE A LAY: a re-lay empties this object's piece
    /// lists before it writes anything else, so a reference held across one
    /// reads as empty UNTIL the next read refills the same stored object --
    /// after which it reads the new layout. Neither state is the old
    /// declaration; emptiness is a courtesy in the window before the refill,
    /// not a guarantee that outlives it.
    ///
    /// Not reentrant against itself: the first read after a lay writes the
    /// stored state, so two threads reading a freshly laid declaration
    /// concurrently race. The contract forbids overlapping operations on one
    /// aggregate (see the threading sentence on NlpAggregate), so this asks for
    /// nothing new -- one thread drives the provider, and its fan-out is
    /// internal.
    ///
    /// @return The declaration the current layout was laid from.
    /// @throws std::invalid_argument if the master piece lists have been
    ///         mutated since the last lay -- see
    ///         require_master_lists_unmoved().
    /// @throws std::bad_alloc from the first read after a lay, which allocates:
    ///         it is the read that takes the deferred piece and bound copies.
    ///         Worth naming because assemble() calls this on every evaluation
    ///         and so inherits it, where an eager copy put the allocation in
    ///         make_nlp instead. The materializers are restartable -- each
    ///         clears its pending flag only after it has finished -- so a throw
    ///         here leaves the state owing and the next read re-derives it.
    const AggregateDeclaration &declaration() const override {
        this->require_master_lists_unmoved();
        this->materialize_declaration_bounds();
        this->materialize_declaration_pieces();
        return this->declaration_;
    }

    /// Adopts a partition count, re-lays over it, and returns what was ADOPTED.
    ///
    /// The request is capped the same way a transcription's is: below
    /// kMinKktElementsPerPartition elements per partition there is not enough
    /// work to offset dispatch, so the count comes down. A caller that assumed
    /// its request was honoured would mis-key the structural key, whose
    /// partition conjunct is this return value.
    ///
    /// A non-positive request is REFUSED, naming the value, rather than clamped
    /// to one. Capping a request the work cannot support is this method's job
    /// and is reported honestly through the return value; a request for zero or
    /// fewer partitions is not a request this can serve at all, and silently
    /// serving something else would hand the caller a count it never asked for.
    ///
    /// Re-lays unconditionally, including when the adopted count is the one
    /// already in force: re-partitioning hands the claims out in a different
    /// order even when the claim STRUCTURE is identical, so any consumer state
    /// indexed by claim slot is stale and the epoch must say so.
    int negotiate_partition_count(int requested) override;

    /// The evaluation thread budget, which in this provider is the shared pool
    /// its partition fan-out dispatches to -- and that pool is PROCESS-GLOBAL.
    /// Setting it through one aggregate sets it for every aggregate in the
    /// process. Said plainly here because an aggregate-scoped setter otherwise
    /// implies a per-aggregate budget this provider does not have.
    int evaluation_threads() const override { return hven::utils::get_num_threads(); }
    void set_evaluation_threads(int n) override { hven::utils::set_num_threads(n); }

    /// @brief The three conjuncts of the LAST LAY: the claim digest, the
    ///        adopted partition count, and the bound digest.
    /// @return The structural key of the layout on hand.
    /// @throws std::invalid_argument if the master piece lists have been
    ///         mutated since the last lay -- the same guard declaration()
    ///         carries, for the same reason.
    ///
    /// Both digests are taken on FIRST READ after a lay and then held -- see
    /// claim_digest() and bound_digest() for what each is taken over, and
    /// materialize_declaration_bounds() for the one piece of declaration state
    /// a key read needs.
    ///
    /// Deferred rather than taken during the lay because both digests are
    /// O(claims) and O(variables) work that a consumer which never asks about
    /// structural identity has no use for -- and every consumer pays a layout,
    /// while only some ask. A consumer that does ask pays once per lay.
    ModelStructureKey model_structure_key() const override {
        this->require_master_lists_unmoved();
        return ModelStructureKey{this->claim_digest(), this->laid_partition_count_,
                                 this->bound_digest()};
    }

    /// kValuesFastPath, and only that.
    ///
    /// The values path is genuine: evaluate_candidate_values runs the objective
    /// and constraint VALUE evaluators and touches no derivative anywhere, so a
    /// consumer pricing a probe at values cost is pricing it correctly.
    ///
    /// kDirectScatter is NOT declared: the declaration is the weakest claim over
    /// the whole provider, so the right-hand-side fill path holding an
    /// intermediate (by design -- see the determinism argument at the top of
    /// this class) settles it for all of them.
    AggregateCapability capabilities() const override {
        return AggregateCapability::kValuesFastPath;
    }

    /// The KKT value array this provider's location tables are bound to.
    ///
    /// kkt_locations_ holds offsets into ONE matrix's value array -- the one
    /// analyze_sparsity walked -- so the tables are meaningful for that array
    /// and no other. Recorded there, cleared by every re-lay (which resets
    /// kkt_locations_ to -1 and so requires a fresh analysis anyway), and
    /// checked at the assemble entry.
    ///
    /// A CAPTURED VALUE. This returns the address recorded at analysis time and
    /// touches no matrix to produce it; re-reading valuePtr() from the analysed
    /// matrix would make the check vacuous against a resize and unsafe against
    /// destruction (see validate_bound_destination in model/nlp_aggregate.h).
    const double *bound_kkt_destination() const override { return this->analyzed_kkt_values_; }

    IdentityProbe probe_identity(ConstVecRef x) override;

    /// The published claim tables: the same arrays every scatter already
    /// addresses, in the contract's own spelling. Views, not copies -- they
    /// stop being valid at the next re-lay, which republishes them.
    const KktLocationTable &kkt_location_table() const { return this->kkt_table_; }
    const RhsLocationTable &objective_gradient_table() const { return this->pgx_table_; }
    const RhsLocationTable &constraint_adjoint_gradient_table() const { return this->agx_table_; }
    const RhsLocationTable &equality_residual_table() const { return this->econ_table_; }
    const RhsLocationTable &inequality_residual_table() const { return this->icon_table_; }

  protected:
    // WHAT THIS PROVIDER'S FIRST-ORDER CANDIDATE SURFACE DOES AND DOES NOT
    // GIVE YOU. Two facts, both easy to read past and either one silently
    // corrupting a residual score:
    //
    // 1. THE GRADIENT ROWS OF ELIMINATED COORDINATES ARE NOT PARTIAL
    //    DERIVATIVES. They read ZERO. Under the MakeParameter treatment a
    //    bound-fixed variable is removed from the solved system, and its
    //    gradient row is marked -1 at claim time -- the layout's own way of
    //    saying the row is not part of the reduced problem's residual -- so no
    //    destination for it survives and this surface reports zero at that
    //    declared coordinate. Zero is not the derivative there; it is the
    //    absence of one.
    //
    // 2. THE FIRST-ORDER SURFACE DIFFERS BETWEEN THE TREATMENTS at exactly
    //    those coordinates. MakeParameter eliminates the variable and yields
    //    the zero above. MakeConstraint and RelaxBounds keep it in the solved
    //    system, so its row carries a real value -- the objective's own partial
    //    plus, under MakeConstraint, the internal fixing row's adjoint
    //    contribution. Same declaration, same point, different rows: the
    //    difference is the treatment, not the mathematics.
    //
    // THE CONSUMER-SIDE RULE that makes both facts harmless: a correct scorer
    // EXCLUDES declared-fixed coordinates -- those whose materialized bound
    // record has lower == upper -- from stationarity scoring, per
    // evaluate_candidate_first_order in model/nlp_aggregate.h. A scorer that
    // does so never reads either row and is insensitive to which treatment is
    // configured.

    void assemble_impl(const CandidatePoint &point, EvalRequest request, KktScatterView kkt,
                       RhsScatterView rhs) override;
    void evaluate_candidate_values_impl(const CandidatePoint &point, CandidateValues out) override;
    void evaluate_candidate_first_order_impl(const CandidatePoint &point,
                                             CandidateFirstOrder out) override;

  public:
    // The eight evaluation shapes, factored.
    //
    // Each eval_ entry above is a pass plus its fills; assemble() runs one of
    // these shapes and then fills DIFFERENT destinations -- omitting the fills
    // for arenas its request does not name, and omitting the solver-coefficient
    // scatter, which the contract transfers to the consumer.
    //
    // Xf is the full-space primal buffer the pieces read, computed by the
    // caller: primal_view() for an eval_ entry (whose X is the solver's reduced
    // iterate) and declaration_view() for assemble (whose point is in
    // declaration space).

    void objective_pass(double ObjScale, ConstEigenRef<VectorXd> Xf, double &val);
    void objective_constraints_pass(double ObjScale, ConstEigenRef<VectorXd> Xf, double &val);
    void objective_gradient_constraints_pass(double ObjScale, ConstEigenRef<VectorXd> Xf,
                                             double &val);
    void first_order_rhs_pass(double ObjScale, ConstEigenRef<VectorXd> Xf,
                              ConstEigenRef<VectorXd> LE, ConstEigenRef<VectorXd> LI, double &val);
    void constraint_jacobian_pass(ConstEigenRef<VectorXd> Xf,
                                  Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);
    void first_order_kkt_pass(double ObjScale, ConstEigenRef<VectorXd> Xf,
                              ConstEigenRef<VectorXd> LE, ConstEigenRef<VectorXd> LI, double &val,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);
    void constraint_kkt_pass(ConstEigenRef<VectorXd> Xf, ConstEigenRef<VectorXd> LE,
                             ConstEigenRef<VectorXd> LI,
                             Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);
    void full_kkt_pass(double ObjScale, ConstEigenRef<VectorXd> Xf, ConstEigenRef<VectorXd> LE,
                       ConstEigenRef<VectorXd> LI, double &val,
                       Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);

    /// The full-space primal buffer for a point already in DECLARATION space.
    ///
    /// The counterpart of primal_view(), which starts from the solver's reduced
    /// iterate. Here the caller's vector already has one entry per declared
    /// variable, so on the identity path it IS the buffer and no copy happens.
    /// Once variables are eliminated the eliminated coordinates are pinned at
    /// their declared values rather than trusted from the caller -- the same
    /// values scatter_full_x would have written, so the two entry paths hand
    /// the pieces identical buffers.
    Eigen::Ref<const VectorXd> declaration_view(ConstEigenRef<VectorXd> x);

    /// Republishes the five location tables over the arrays as they now stand.
    void publish_location_tables();

    /// The bound records the current layout was materialized from: a copy taken
    /// by materialize_variable_bounds() at the point where those records have
    /// just been range-checked and intersected successfully.
    ///
    /// The snapshot, rather than reading staged_variable_bounds_ when the
    /// declaration is refreshed, keeps two promises. It keeps the structural key
    /// describing the bounds AS LAID: a record staged after the layout has no
    /// business moving the key of the structures on hand, and a later re-lay for
    /// some unrelated reason must not quietly fold it in. And it keeps the bound
    /// digest from ever throwing part-way through a re-lay: the records here have
    /// already passed materialization, so the digest cannot be the thing that
    /// fails after the structures have been re-laid but before the epoch has been
    /// bumped, which would leave the old epoch standing over new tables.
    std::vector<VariableBoundStage> laid_variable_bounds_;

    /// The partition count the structures were laid at -- the ADOPTED count,
    /// captured for the same reason: num_partitions_ is a public member and a
    /// consumer writing it directly changes nothing structural until the next
    /// lay.
    int laid_partition_count_ = 0;

    /// The VALUE of the value-array address analyze_sparsity last laid the
    /// location table against. An identity token and nothing else: it is
    /// compared, never dereferenced, and never re-derived. Cleared by every
    /// re-lay. See bound_kkt_destination().
    const double *analyzed_kkt_values_ = nullptr;

    /// The matrix object analyze_sparsity last walked, kept for one purpose:
    /// the piece surface takes a matrix reference, so a KKT-bearing assemble
    /// has to hand the pieces one.
    ///
    /// Dereferenced ONLY on that path, and only after the entry has established
    /// that the caller's scatter view names analyzed_kkt_values_ -- which is to
    /// say only when the consumer is, at that moment, presenting the very array
    /// this was analysed against. The identity check above deliberately does not
    /// go through here, because the whole point of holding the address as a
    /// value is that reading it back out of this object would be both vacuous
    /// (a resize moves both sides together) and unsafe (a destroyed matrix
    /// cannot be read).
    Eigen::SparseMatrix<double, Eigen::RowMajor> *analyzed_kkt_matrix_ = nullptr;

    KktLocationTable kkt_table_;
    RhsLocationTable pgx_table_;
    RhsLocationTable agx_table_;
    RhsLocationTable econ_table_;
    RhsLocationTable icon_table_;

    /// Scratch for the candidate surface, which is off the hot path and must
    /// hand back DECLARATION-space vectors: the gradient fills land in the
    /// solver's own (possibly narrower) space first and are expanded from here.
    VectorXd candidate_gradient_scratch_;
    /// Residual scratch for probe_identity, which needs storage that outlives
    /// the CandidateValues view it builds.
    VectorXd probe_equality_scratch_;
    VectorXd probe_inequality_scratch_;

  private:
    // The deferred half of the laid state, and the only paths that read it.
    //
    // PRIVATE, and that is the point rather than tidiness: between a lay and
    // the first read of each part, this state is HALF-DERIVED -- the scalar
    // conjuncts are written, the piece lists and the digests are not -- and a
    // half-derived declaration is not a declaration. The accessors above are
    // the only read path, and each one returns whole state or throws.

    /// @brief Freezes the thread modes of every piece in @p pieces.
    /// @tparam Pieces a range of ObjectiveFunction or ConstraintFunction.
    /// @param pieces the pieces to freeze.
    ///
    /// A member of this class because the piece entry it calls is private to
    /// the piece types and reachable only from here -- which is what keeps a
    /// laid piece's mode from being moved without a re-lay.
    template <class Pieces> static void freeze_thread_modes(Pieces &pieces) {
        for (auto &piece : pieces) {
            piece.mark_thread_mode_laid();
        }
    }

    /// @brief Thaws the thread modes of every piece in @p pieces.
    /// @tparam Pieces a range of ObjectiveFunction or ConstraintFunction.
    /// @param pieces the pieces to thaw.
    ///
    /// For the copies handed out as declaration data, and for nothing else.
    template <class Pieces> static void thaw_thread_modes(Pieces &pieces) {
        for (auto &piece : pieces) {
            piece.clear_thread_mode_laid();
        }
    }

    /// @brief Freezes the thread mode of every piece THIS LAYOUT HOLDS: the
    ///        three master lists and the partition copies taken from them.
    ///
    /// Every piece a laid layout holds carries the same marker, master and
    /// partition copy alike.
    void freeze_laid_thread_modes();

    /// @brief Writes the laid DIMENSIONS into the stored declaration -- the
    ///        three declared sizes, the adopted partition count and the three
    ///        piece counts the size guard checks against -- and freezes the
    ///        laid thread modes with them.
    /// @throws std::invalid_argument if a master piece list holds more pieces
    ///         than an int can index.
    ///
    /// Eager, while the piece lists and the bound records are not: three of
    /// these scalars are read on EVERY evaluation, and the partition count has
    /// to be captured because num_partitions_ is a public member a consumer may
    /// write, which changes nothing structural until the next lay.
    ///
    /// Called by rebuild_structures(), AFTER invalidate_laid_state() and never
    /// before it.
    void capture_laid_dimensions();

    /// @brief Fills the stored declaration's bound records from the laid bound
    ///        snapshot, if a lay has left that owing.
    ///
    /// Idempotent, and a no-op once discharged. Taken from the LAID snapshot
    /// and never from the live staging state: a record staged since the last
    /// materialization describes a problem these structures were not laid for.
    void materialize_declaration_bounds() const;

    /// @brief Fills the stored declaration's three piece lists from the master
    ///        lists, if a lay has left that owing.
    ///
    /// Idempotent, and a no-op once discharged. The lists are COPIES rather
    /// than views over the master lists; what is deferred is WHEN the copy is
    /// taken, not that it is taken -- once per lay, on the first read that
    /// needs it.
    ///
    /// The mutated-master-list case is refused before this runs -- see
    /// require_master_lists_unmoved(), which both public readers call first.
    void materialize_declaration_pieces() const;

    /// @brief Refuses a read whose master piece lists are no longer the lists
    ///        the layout was laid over.
    /// @throws std::invalid_argument if any of the three master lists has a
    ///         different size from the count captured at the last lay, naming
    ///         the list, both counts and the remedy.
    ///
    /// MUTATING THE MASTER LISTS AFTER A LAY, WITHOUT RE-LAYING, IS A CONTRACT
    /// VIOLATION.
    ///
    /// CALLED ON EVERY READ of declaration() and model_structure_key(),
    /// independently of whether the deferred copy is still owing, so the answer
    /// does not depend on whether the caller has read since the lay. Three
    /// integer compares.
    ///
    /// Catches a SIZE change and nothing finer: a list mutated in place at
    /// constant length is not detectable here and remains the caller's
    /// obligation. An aggregate that has never been laid is not checked -- there
    /// is no layout for the lists to disagree with.
    void require_master_lists_unmoved() const;

    /// @brief Drops every deferred part of the laid state: the cached piece
    ///        copies and bound records, both digests, and the four flags that
    ///        say they are owing.
    ///
    /// RUNS FIRST, at the top of make_nlp() and at the top of
    /// rebuild_structures(), before any master list is touched and before any
    /// eager scalar is written; between it and the epoch bump the declaration's
    /// piece lists are EMPTY. Idempotent.
    void invalidate_laid_state();

    /// @brief The claim-structure conjunct of the structural key, digested on
    ///        first read after a lay and then held.
    /// @return The digest of the claim stream as the pieces handed it out.
    ///
    /// Taken over the two index arrays in claim order, UN-CANONICALIZED.
    /// analyze_sparsity derives the canonical endpoint ordering it needs per
    /// element as it goes and leaves these arrays alone, which is what keeps
    /// the stream readable for as long as the layout stands -- and therefore
    /// what lets this be deferred at all.
    std::uint64_t claim_digest() const;

    /// @brief The bound-structure conjunct of the structural key, digested on
    ///        the same terms.
    /// @return The digest of the materialized bound structure AS LAID.
    ///
    /// Taken over the stored declaration, whose bound records are themselves a
    /// snapshot (laid_variable_bounds_), so the digest describes the bounds the
    /// structures were LAID WITH and never the staging state as it stands now.
    std::uint64_t bound_digest() const;

    /// The declaration as of the last lay -- see declaration().
    mutable AggregateDeclaration declaration_;

    /// What the last lay left owing. Each is set by invalidate_laid_state() and
    /// cleared by the one read that discharges it; all four start FALSE, so an
    /// aggregate that has NEVER BEEN LAID reports the default-constructed
    /// declaration and the zero key rather than digesting a layout that does
    /// not exist. That is the prior behaviour and is deliberate: there is no
    /// layout to describe, and refusing would give a caller no more than the
    /// empty answer already tells it.
    mutable bool declaration_bounds_pending_ = false;
    mutable bool declaration_pieces_pending_ = false;
    mutable bool claim_digest_pending_ = false;
    mutable bool bound_digest_pending_ = false;

    mutable std::uint64_t claim_digest_ = 0;
    mutable std::uint64_t bound_digest_ = 0;

    /// The sizes of the three master piece lists as of the last lay, which is
    /// what every read is checked against. See require_master_lists_unmoved().
    int laid_objective_pieces_ = 0;
    int laid_equality_pieces_ = 0;
    int laid_inequality_pieces_ = 0;

    /// Whether a layout exists at all. False until the first lay, and what
    /// keeps the size guard off an aggregate whose pieces have been pushed but
    /// which has never been laid: there is no layout for the lists to disagree
    /// with, and declaration() correctly reports the empty declaration there.
    bool ever_laid_ = false;
};

} // namespace hven::solvers
