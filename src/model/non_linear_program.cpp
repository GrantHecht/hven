// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "hven/detail/interior/indexing_data.h"
#include "hven/detail/interior/utils/timer.h"
#include "hven/model/non_linear_program.h"

void hven::solvers::NonLinearProgram::make_nlp(int PV, int EQ, int IQ) {
    // FIRST, before anything below mutates a master list or can throw: the
    // discard just below truncates equality_constraints_, the invariant check
    // after it throws, and the bound materialization further down throws on an
    // empty intersection, so each of those can leave this call part-way through
    // with the master lists already moved. See invalidate_laid_state's
    // definition for what that position buys.
    this->invalidate_laid_state();

    // A previous configuration's internal fixing rows describe the bounds as they
    // were then, and this call re-materializes those bounds from scratch -- so the
    // rows go first, and the next configure_variable_treatment call derives its
    // own. Dropped before equal_cons_ is set from EQ, which is the USER's own row
    // count and knows nothing about them.
    this->discard_fixed_variable_rows();

    // The one invariant that would catch an internal-row bookkeeping slip before
    // it can corrupt a scatter: after the discard nothing internal may remain, and
    // the equality row space must be back at the count the last make_nlp was
    // handed. A logic_error rather than an assert because asserts are compiled out
    // in every shipped configuration, and this is not a user-facing rejection --
    // no argument can trip it, only a defect in the install/discard pair.
    if (this->internal_fixed_cons_ != 0 || this->equal_cons_ != this->user_equal_cons_) {
        throw std::logic_error(
            fmt::format("make_nlp: internal fixing rows were not fully discarded (internal {0}, "
                        "equal_cons {1}, user_equal_cons {2})",
                        this->internal_fixed_cons_, this->equal_cons_, this->user_equal_cons_));
    }

    this->primal_vars_ = PV;
    this->user_equal_cons_ = EQ;
    this->equal_cons_ = EQ;
    this->inequal_cons_ = IQ;
    this->slack_vars_ = IQ;

    // Everything below is laid out over the full variable space. A previous
    // configuration's reduction does not survive a re-transcription: its output
    // maps are dropped and its structures are about to be rebuilt from scratch,
    // so the next configure_variable_treatment call starts from the pristine
    // state and re-derives whatever the new bounds call for.
    this->reduced_primal_vars_count_ = PV;
    this->fixed_reduction_active_ = false;
    this->fixed_treatment_valid_ = false;
    this->clear_function_output_maps();

    this->materialize_variable_bounds();

    this->count_elems();

    // Cap partitions so each has enough work to offset dispatch overhead.
    // num_user_kkt_elems_ counts Jacobian + Hessian NNZ across all functions —
    // proportional to per-partition compute in eval_kkt/eval_aug. Below the
    // per-partition threshold, dispatch overhead dominates actual work.
    // Threshold empirically chosen via solver benchmarks (bench_all);
    // re-evaluate with the bench harness if dispatch overhead changes. The same
    // cap is what negotiate_partition_count applies, which is what makes the
    // count it reports as ADOPTED the count a transcription would have got.
    if (this->num_partitions_ > 1) {
        int max_parts = std::max(1, this->num_user_kkt_elems_ / kMinKktElementsPerPartition);
        this->num_partitions_ = std::min(this->num_partitions_, max_parts);
    }

    this->analyze_partitioning();
    this->rebuild_structures();
}

void hven::solvers::NonLinearProgram::adopt_declaration(AggregateDeclaration declaration) {
    // FIRST, and before a single member of this problem is written. Every
    // refusal this entry can make is the declaration's own -- the dimensions,
    // the piece sums, the bounds, and the shape of the fixing-row tail this
    // entry is about to split -- so a refused adoption leaves the problem on
    // hand exactly as it was: master lists, layout, stored declaration and
    // epoch all untouched, with nothing owing.
    declaration.validate();

    // The declaration type's piece-sum conjunct is conditional on there being
    // pieces at all -- a provider that is not a piece collection declares none,
    // and there is then no sum for its row counts to disagree with. THIS ENTRY
    // is a piece-laying one: it takes the declared counts straight into the
    // layout, so a declaration with rows and no pieces would lay a row space
    // nothing claims, and the defect would surface much later as a structurally
    // singular factorization with nothing pointing back here. Refused before
    // anything moves.
    const bool has_pieces = !declaration.objectives_.empty() ||
                            !declaration.equality_constraints_.empty() ||
                            !declaration.inequality_constraints_.empty();
    if (!has_pieces && (declaration.equality_rows_ != 0 || declaration.inequality_rows_ != 0)) {
        throw std::invalid_argument(fmt::format(
            "NonLinearProgram::adopt_declaration: the declaration carries no pieces but declares "
            "{0} equality and {1} inequality rows; this entry lays a problem out of its pieces, so "
            "those rows would be laid with nothing to claim or evaluate them. A declaration from a "
            "provider that is not a piece collection is not adoptable here",
            declaration.equality_rows_, declaration.inequality_rows_));
    }

    const int fixing_rows = declaration.fixing_rows_;
    const int user_equality_rows = declaration.equality_rows_ - fixing_rows;

    // The split runs on the DECLARATION, which is this call's own copy, and so
    // still touches nothing of the problem's. The declared internal fixing rows
    // sit at the tail of the equality list, one piece per row, where the
    // treatment that produced them appended them -- a shape validate() has just
    // established, which is why splitting by piece count alone is sound here.
    // Lifting them off before the lay and putting them back after it is what
    // makes the adopting problem repeat the source's own history rather than
    // lay a row space nothing declared.
    std::vector<ConstraintFunction> internal_rows;
    if (fixing_rows > 0) {
        auto &declared = declaration.equality_constraints_;
        const std::size_t first = declared.size() - static_cast<std::size_t>(fixing_rows);
        internal_rows.reserve(static_cast<std::size_t>(fixing_rows));
        for (std::size_t k = first; k < declared.size(); k++) {
            // WHERE each lifted row writes, checked against the band the
            // internal rows occupy. The count itself is trusted (see the field's
            // own note), but the rows it names are not: a declaration whose
            // dimensions were edited would otherwise splice these pieces back
            // over user rows, or past the end of the row space, and scatter into
            // constraint rows that belong to something else.
            const auto &constraint_index = declared[k].index_data_.get_c_index();
            if (constraint_index.size() != 1) {
                throw std::invalid_argument(fmt::format(
                    "NonLinearProgram::adopt_declaration: declared internal fixing row {0} names "
                    "{1} constraint rows; a fixing row writes exactly one",
                    k, constraint_index.size()));
            }
            const int constraint_row = constraint_index(0, 0);
            if (constraint_row < user_equality_rows ||
                constraint_row >= declaration.equality_rows_) {
                throw std::invalid_argument(fmt::format(
                    "NonLinearProgram::adopt_declaration: declared internal fixing row {0} writes "
                    "equality row {1}, outside the [{2}, {3}) band the declaration's {4} fixing "
                    "rows occupy; a treatment appends its rows after every row a transcription "
                    "declared",
                    k, constraint_row, user_equality_rows, declaration.equality_rows_,
                    fixing_rows));
            }
            internal_rows.push_back(std::move(declared[k]));
        }
        declared.resize(first);
    }

    // Nothing below throws before make_nlp, whose own first statement is the
    // invalidation: the bound replay cannot refuse a record validate() has
    // already accepted, and the three moves and three counter writes cannot
    // refuse anything. The three lists are replaced wholesale, so whatever
    // internal rows the previous layout counted went with them; the pair is
    // cleared here so make_nlp's discard has nothing stale to truncate and its
    // bookkeeping check reads a consistent pair.
    this->objectives_ = std::move(declaration.objectives_);
    this->equality_constraints_ = std::move(declaration.equality_constraints_);
    this->inequality_constraints_ = std::move(declaration.inequality_constraints_);
    this->internal_fixed_cons_ = 0;
    this->user_equal_cons_ = 0;
    this->equal_cons_ = 0;

    // Cleared and replayed in DECLARATION ORDER, so the tightest-wins merge is
    // re-derived from the same history rather than from a merged result -- which
    // is what keeps the bound conjunct of the structural key equal across the
    // round trip.
    this->clear_variable_bounds();
    for (const VariableBound &bound : declaration.variable_bounds_) {
        this->set_variable_bound(bound.index_, bound.lower_, bound.upper_);
    }

    // A REQUEST. make_nlp's cap may bring it down, and declaration() then
    // reports the count that was adopted.
    this->num_partitions_ = declaration.partition_count_;

    this->make_nlp(declaration.primal_vars_, user_equality_rows, declaration.inequality_rows_);

    if (fixing_rows > 0) {
        // The same chain the treatment runs after its own install: the set of
        // functions changed, so the element counts and the work partitioning are
        // re-derived before the layout is laid over them.
        this->splice_fixed_variable_rows(std::move(internal_rows));
        this->refresh_function_partitions();
        this->rebuild_structures();
    }
}

void hven::solvers::NonLinearProgram::rebuild_structures() {
    // THE ONE ROUTINE THAT RE-LAYS, and therefore the one place a structural
    // event is recorded. Every path that changes the assembled claim structure
    // -- a (re)transcription, either treatment path that eliminates or restores
    // variables, a MakeConstraint row-space change, a partition renegotiation,
    // and the restore that runs when a reconfiguration is REJECTED after having
    // re-laid -- reaches the layout through here. Nothing else does, so nothing
    // else needs a bump of its own, and the one configure_variable_treatment
    // exit that does not re-lay (the identity path, which changed nothing) and
    // the one restore path that does not (a classification-stage rejection at
    // an NLP that was never reduced) correctly do not bump.
    //
    // INVALIDATION FIRST, before the eager scalars are written and before a
    // single array is touched -- see invalidate_laid_state's definition for the
    // two failure modes that ordering shuts. It is idempotent, so the make_nlp
    // path calling it once more here costs four flag writes and three clears
    // of already-empty vectors.
    this->invalidate_laid_state();

    this->capture_laid_dimensions();

    this->set_mat_dimensions();
    this->set_rhs_dimensions();

    this->get_mat_space();
    this->get_rhs_space();
    this->finalize_data();

    this->publish_location_tables();

    this->laid_partition_count_ = this->num_partitions_;

    // A re-lay resets kkt_locations_ to -1: only analyze_sparsity fills it, and
    // it has not run against this layout yet. The destination binding goes with
    // the offsets it described.
    this->analyzed_kkt_values_ = nullptr;
    this->analyzed_kkt_matrix_ = nullptr;

    // LAST, and that program order is the substance of the ordering guarantee:
    // no evaluation of these structures is reachable under the previous epoch.
    this->bump_structure_epoch();
}

void hven::solvers::NonLinearProgram::freeze_laid_thread_modes() {
    freeze_thread_modes(this->objectives_);
    freeze_thread_modes(this->equality_constraints_);
    freeze_thread_modes(this->inequality_constraints_);

    // The partition copies too, and that is the point of doing this after the
    // partitioning rather than inside it: a copy is taken from a master whose
    // marker says whatever the PREVIOUS lay left, so freezing only the masters
    // would leave the partition lists frozen on a re-lay and thawed on a first
    // lay. Every piece this layout holds carries the same answer.
    for (auto &partition : this->part_obj_) {
        freeze_thread_modes(partition);
    }
    for (auto &partition : this->part_eq_) {
        freeze_thread_modes(partition);
    }
    for (auto &partition : this->part_iq_) {
        freeze_thread_modes(partition);
    }
}

void hven::solvers::NonLinearProgram::capture_laid_dimensions() {
    this->declaration_.primal_vars_ = this->primal_vars_;
    this->declaration_.equality_rows_ = this->equal_cons_;
    this->declaration_.inequality_rows_ = this->inequal_cons_;
    this->declaration_.partition_count_ = this->num_partitions_;

    // How many of those equality rows are the internal ones a fixed-variable
    // treatment appended. Written HERE and nowhere else: a declaration that
    // carries its own fixing-row count is self-describing, so the adoption
    // entry subtracts a property of the declaration rather than of whichever
    // problem happens to receive it.
    this->declaration_.fixing_rows_ = this->equal_cons_ - this->user_equal_cons_;

    // The sizes the deferred piece copy will be checked against. Captured here,
    // with the other scalars, because here is where "as laid" is decided.
    //
    // The narrowing to int is checked rather than assumed. A piece count past
    // INT_MAX is unreachable in any problem this engine can lay, but an
    // unchecked narrowing here would wrap into a laid count that the guard then
    // compares against and silently accepts -- turning an impossible input into
    // a wrong answer instead of a refusal.
    const auto laid_count = [](const char *which, std::size_t count) {
        if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::invalid_argument(
                fmt::format("NonLinearProgram::make_nlp: the {0} list holds {1} pieces, past the "
                            "{2} this layout can index",
                            which, count, std::numeric_limits<int>::max()));
        }
        return static_cast<int>(count);
    };
    this->laid_objective_pieces_ = laid_count("objective", this->objectives_.size());
    this->laid_equality_pieces_ =
        laid_count("equality constraint", this->equality_constraints_.size());
    this->laid_inequality_pieces_ =
        laid_count("inequality constraint", this->inequality_constraints_.size());
    this->ever_laid_ = true;

    // The thread modes are frozen here, with the other "as laid" facts. The
    // partitioner has already read them, and the claim order they produced is
    // what the claim arrays and the claim digest now describe -- so a write
    // that moved a piece to another partition would describe a layout nobody
    // laid. Changing one is a re-lay from a declaration that carries the new
    // mode.
    this->freeze_laid_thread_modes();

    // The equality row count is the row space AS LAID, so it counts whatever
    // internal fixing rows the MakeConstraint treatment currently has
    // installed. Those rows are appended after every row the transcription
    // declared, so no user row is ever renumbered and every declared global row
    // identity survives -- which is the property the candidate surface and the
    // partition-invariance sentence both rest on.
}

// Two invariants rest on this running FIRST, before any master list is touched
// and before any eager scalar is written: no reader ever sees new scalar
// dimensions beside old piece lists, and a lay that throws part-way leaves the
// deferred state owing rather than leaving the previous lay's digests cached
// over half-replaced arrays. The next read re-derives it, and
// require_master_lists_unmoved() refuses that read if the master lists moved in
// the meantime.
void hven::solvers::NonLinearProgram::invalidate_laid_state() {
    this->declaration_bounds_pending_ = true;
    this->declaration_pieces_pending_ = true;
    this->claim_digest_pending_ = true;
    this->bound_digest_pending_ = true;

    // DROPPED, not merely marked. The flags alone would be enough for every
    // read that goes through an accessor, which is every read the interface
    // offers; clearing is what makes the state a consumer could still be
    // holding a reference to EMPTY rather than stale, so a retained reference
    // cannot be mistaken for a live one. clear() keeps the capacity, so the
    // next materialization refills the same buffers.
    this->declaration_.objectives_.clear();
    this->declaration_.equality_constraints_.clear();
    this->declaration_.inequality_constraints_.clear();
    this->declaration_.variable_bounds_.clear();
    this->claim_digest_ = 0;
    this->bound_digest_ = 0;
}

void hven::solvers::NonLinearProgram::materialize_declaration_bounds() const {
    if (!this->declaration_bounds_pending_) {
        return;
    }

    // From the LAID snapshot, never from the live staging state. A record staged
    // since the last materialization describes a problem these structures were
    // not laid for: folding it in here would move the structural key of a layout
    // nothing re-laid.
    this->declaration_.variable_bounds_.clear();
    this->declaration_.variable_bounds_.reserve(this->laid_variable_bounds_.size());
    for (const auto &stage : this->laid_variable_bounds_) {
        this->declaration_.variable_bounds_.push_back(
            VariableBound{stage.index_, stage.lower_, stage.upper_});
    }

    this->declaration_bounds_pending_ = false;
}

void hven::solvers::NonLinearProgram::materialize_declaration_pieces() const {
    if (!this->declaration_pieces_pending_) {
        return;
    }

    this->declaration_.objectives_ = this->objectives_;
    this->declaration_.equality_constraints_ = this->equality_constraints_;
    this->declaration_.inequality_constraints_ = this->inequality_constraints_;

    // COPIES rather than views because an AggregateDeclaration is a value over
    // its pieces -- which is what makes a layout a pure function of the
    // declaration, and what lets a consumer MOVE one in.
    //
    // The copies are DECLARATION data, and a thread mode is settable in a
    // declaration: that is the one route a consumer has to change one, by
    // re-declaring and adopting. The pieces the layout itself holds stay
    // frozen; only these copies are thawed.
    thaw_thread_modes(this->declaration_.objectives_);
    thaw_thread_modes(this->declaration_.equality_constraints_);
    thaw_thread_modes(this->declaration_.inequality_constraints_);

    this->declaration_pieces_pending_ = false;
}

void hven::solvers::NonLinearProgram::require_master_lists_unmoved() const {
    // Nothing to disagree with before the first lay: the pieces a front end has
    // pushed have not been laid over yet, and declaration() reports the empty
    // declaration there by design.
    if (!this->ever_laid_) {
        return;
    }

    // EVERY read, not only the one that still owes a copy. The lists are read
    // after the lay rather than at it, and a front end writes these three
    // public members directly, so "still the lists that were laid" is not
    // automatic: the claim arrays, the claim digest and the row counts all
    // describe the lists AS LAID, and a piece added or dropped since then
    // matches none of them. Refused by name rather than served.
    const auto check = [](const char *which, std::size_t have, int laid) {
        if (static_cast<std::size_t>(laid) != have) {
            throw std::invalid_argument(fmt::format(
                "NonLinearProgram::declaration: the {0} list holds {1} pieces but the layout was "
                "laid over {2} -- the master lists were mutated after the last lay, and the claim "
                "arrays, the claim digest and the row counts all describe the list as it was laid; "
                "re-lay (make_nlp) before reading the declaration",
                which, have, laid));
        }
    };
    check("objective", this->objectives_.size(), this->laid_objective_pieces_);
    check("equality constraint", this->equality_constraints_.size(), this->laid_equality_pieces_);
    check("inequality constraint", this->inequality_constraints_.size(),
          this->laid_inequality_pieces_);
}

std::uint64_t hven::solvers::NonLinearProgram::claim_digest() const {
    if (this->claim_digest_pending_) {
        this->claim_digest_ = claim_stream_digest(
            this->declaration_, this->kkt_coeff_rows_.head(this->num_user_kkt_elems_),
            this->kkt_coeff_cols_.head(this->num_user_kkt_elems_));
        this->claim_digest_pending_ = false;
    }
    return this->claim_digest_;
}

std::uint64_t hven::solvers::NonLinearProgram::bound_digest() const {
    if (this->bound_digest_pending_) {
        // The bound conjunct is taken over the declaration's records, so the
        // one part of the declaration it needs is materialized first. The
        // records are the laid snapshot and have already passed materialization
        // once, so nothing here can throw over a bound set that does not
        // describe a problem.
        this->materialize_declaration_bounds();
        this->bound_digest_ = materialized_bound_digest(this->declaration_);
        this->bound_digest_pending_ = false;
    }
    return this->bound_digest_;
}

void hven::solvers::NonLinearProgram::publish_location_tables() {
    // Views over the arrays every scatter already addresses. The KKT table's
    // clash marks are published over the whole kkt_clashes_ array: a slot's
    // canonical column is min(row, col), which for a Jacobian element is its
    // variable column and for a Hessian element is one of the two coupled
    // variables, so the marks a scatter reads all sit below the primal width --
    // but publishing the full array is what makes the table's own bounds check
    // cover every column it describes.
    this->kkt_table_ = KktLocationTable(
        this->kkt_locations_.data(), this->num_user_kkt_elems_, this->kkt_clashes_.data(),
        static_cast<int>(this->kkt_clashes_.size()), &this->kkt_locks_);

    const int *rhs_rows = this->rhs_coeff_rows_.data();
    this->pgx_table_ = RhsLocationTable(rhs_rows + this->pgx_data_start_, this->num_pgx_elems_);
    this->agx_table_ = RhsLocationTable(rhs_rows + this->agx_data_start_, this->num_agx_elems_);
    this->econ_table_ = RhsLocationTable(rhs_rows + this->econ_data_start_, this->num_econ_elems_);
    this->icon_table_ = RhsLocationTable(rhs_rows + this->icon_data_start_, this->num_icon_elems_);
}

int hven::solvers::NonLinearProgram::negotiate_partition_count(int requested) {
    // Refused, not clamped. Capping a request the work cannot support is this
    // method's job and is reported honestly through the return value; a request
    // for zero or fewer partitions names no partitioning at all, and quietly
    // serving one instead would hand the caller a count it never asked for --
    // which it would then key its structural key on.
    if (requested < 1) {
        throw std::invalid_argument(
            fmt::format("negotiate_partition_count: a partition count must be at least 1 (got {0})",
                        requested));
    }

    this->num_partitions_ = requested;
    if (this->num_partitions_ > 1) {
        const int max_parts = std::max(1, this->num_user_kkt_elems_ / kMinKktElementsPerPartition);
        this->num_partitions_ = std::min(this->num_partitions_, max_parts);
    }

    // The order refresh_function_partitions' own contract requires:
    // re-partitioning hands out fresh copies of the master functions and those
    // copies carry the pristine input map, so a reduction in force has to
    // reinstall its output maps before the structures are laid over them.
    this->analyze_partitioning();
    if (this->fixed_reduction_active_) {
        this->install_function_output_maps();
    } else {
        this->clear_function_output_maps();
    }
    this->rebuild_structures();

    return this->num_partitions_;
}

void hven::solvers::NonLinearProgram::refresh_function_partitions() {
    this->count_elems();
    this->analyze_partitioning();
}

void hven::solvers::NonLinearProgram::install_fixed_variable_rows() {
    const int count = static_cast<int>(this->fixed_idx_.size());

    // Built into a local first, spliced onto the master list only once every row
    // exists. All-or-nothing on purpose: a throw part-way through the
    // construction would otherwise leave rows on the master list that
    // internal_fixed_cons_ does not yet count, and a count that does not see them
    // is a count nothing can repair -- the restore's discard would skip them, the
    // element counts and the partitioning would not include them, and the next
    // successful install would append on top, giving two functions the same
    // constraint row to scatter into. There is no such throw today (a non-finite
    // fixing value is rejected during classification, before this runs, and the
    // factory's own validation is defence in depth), and this is what keeps that
    // from being load-bearing.
    std::vector<ConstraintFunction> rows;
    rows.reserve(count);
    for (int k = 0; k < count; k++) {
        // Numbered after every row the transcription declared, so the user's own
        // constraint rows keep the indices they were given and the multipliers
        // that come back are still theirs.
        rows.push_back(make_fixed_variable_row(this->fixed_idx_[k], this->fixed_vals_[k],
                                               this->user_equal_cons_ + k));
    }

    this->splice_fixed_variable_rows(std::move(rows));
}

void hven::solvers::NonLinearProgram::splice_fixed_variable_rows(
    std::vector<ConstraintFunction> rows) {
    const int count = static_cast<int>(rows.size());
    const std::size_t before = this->equality_constraints_.size();
    try {
        this->equality_constraints_.reserve(before + rows.size());
        for (auto &row : rows) {
            this->equality_constraints_.push_back(std::move(row));
        }
    } catch (...) {
        // Truncating back is a no-op when nothing landed, and cannot throw. What
        // it buys is that the list and internal_fixed_cons_ (still 0 here) agree
        // on every path out of this function.
        this->equality_constraints_.resize(before);
        throw;
    }
    this->internal_fixed_cons_ = count;
    this->equal_cons_ = this->user_equal_cons_ + count;
}

void hven::solvers::NonLinearProgram::discard_fixed_variable_rows() {
    if (this->internal_fixed_cons_ <= 0) {
        return;
    }
    const int installed = this->internal_fixed_cons_;
    const int total = static_cast<int>(this->equality_constraints_.size());
    if (total < installed) {
        throw std::logic_error(
            fmt::format("discard_fixed_variable_rows: {0} internal fixing rows are recorded but "
                        "the equality list holds only {1} functions",
                        installed, total));
    }
    this->equality_constraints_.resize(total - installed);
    this->internal_fixed_cons_ = 0;
    this->equal_cons_ = this->user_equal_cons_;
}

void hven::solvers::NonLinearProgram::clear_function_output_maps() {
    auto clear_all = [](auto &funcs) {
        for (auto &func : funcs) {
            func.index_data_.clear_output_v_index();
        }
    };
    for (int i = 0; i < static_cast<int>(this->part_obj_.size()); i++) {
        clear_all(this->part_obj_[i]);
    }
    for (int i = 0; i < static_cast<int>(this->part_eq_.size()); i++) {
        clear_all(this->part_eq_[i]);
    }
    for (int i = 0; i < static_cast<int>(this->part_iq_.size()); i++) {
        clear_all(this->part_iq_[i]);
    }
}

void hven::solvers::NonLinearProgram::install_function_output_maps() {
    // Always regenerated from the pristine input map, never edited in place, so
    // repeated configuration cannot compound a previous remapping.
    auto install = [&](auto &funcs) {
        for (auto &func : funcs) {
            const Eigen::MatrixXi &v_in = func.index_data_.get_v_index();
            Eigen::MatrixXi v_out(v_in.rows(), v_in.cols());
            for (int col = 0; col < v_in.cols(); col++) {
                for (int row = 0; row < v_in.rows(); row++) {
                    const int global = v_in(row, col);
                    if (global < 0 || global >= this->primal_vars_) {
                        throw std::logic_error(fmt::format(
                            "configure_variable_treatment: function variable index {0} is "
                            "outside [0, {1})",
                            global, this->primal_vars_));
                    }
                    v_out(row, col) = this->full_to_reduced_[global];
                }
            }
            func.index_data_.set_output_v_index(v_out);
        }
    };
    for (int i = 0; i < static_cast<int>(this->part_obj_.size()); i++) {
        install(this->part_obj_[i]);
    }
    for (int i = 0; i < static_cast<int>(this->part_eq_.size()); i++) {
        install(this->part_eq_[i]);
    }
    for (int i = 0; i < static_cast<int>(this->part_iq_.size()); i++) {
        install(this->part_iq_[i]);
    }
}

void hven::solvers::NonLinearProgram::materialize_variable_bounds() {
    constexpr double kInf = std::numeric_limits<double>::infinity();

    // Any re-materialization invalidates a previous classification, whether the
    // declared bounds changed or only primal_vars_ did.
    this->bounds_revision_++;

    this->x_lower_ = Eigen::VectorXd::Constant(this->primal_vars_, -kInf);
    this->x_upper_ = Eigen::VectorXd::Constant(this->primal_vars_, kInf);

    for (const auto &stage : this->staged_variable_bounds_) {
        if (stage.index_ < 0 || stage.index_ >= this->primal_vars_) {
            throw std::invalid_argument(
                fmt::format("set_variable_bound: index {0} out of range [0, {1})", stage.index_,
                            this->primal_vars_));
        }

        double &lower = this->x_lower_[stage.index_];
        double &upper = this->x_upper_[stage.index_];
        lower = std::max(lower, stage.lower_);
        upper = std::min(upper, stage.upper_);

        if (lower > upper) {
            throw std::invalid_argument(
                fmt::format("set_variable_bound: conflicting bounds for index {0}: "
                            "lower={1}, upper={2}",
                            stage.index_, lower, upper));
        }
    }

    // The records this layout is materialized from, snapshotted at the one point
    // where they have just been proved to describe a problem: every index is in
    // range and every intersection is non-empty. The declaration is refreshed
    // from this copy rather than from the live staging list, so the structural
    // key describes the bounds AS LAID and the bound digest can never be the
    // thing that throws part-way through a later re-lay. Taken only on the
    // success path, so a rejected materialization leaves the previous layout's
    // snapshot -- and the previous layout -- describing each other.
    this->laid_variable_bounds_ = this->staged_variable_bounds_;
}

void hven::solvers::NonLinearProgram::count_elems() {
    int nkkt = 0;

    int npgx = 0;
    int nagx = 0;
    int nec = 0;
    int nic = 0;

    for (auto &obj : this->objectives_) {
        nkkt += obj.num_kkt_elements(false, true);
        npgx += obj.num_grad_eles();
    }
    for (auto &eq : this->equality_constraints_) {
        nkkt += eq.num_kkt_elements(true, true);
        nagx += eq.num_grad_eles();
        nec += eq.num_con_eles();
    }
    for (auto &ineq : this->inequality_constraints_) {
        nkkt += ineq.num_kkt_elements(true, true);
        nagx += ineq.num_grad_eles();
        nic += ineq.num_con_eles();
    }

    this->num_user_kkt_elems_ = nkkt;
    this->num_pgx_elems_ = npgx;
    this->num_agx_elems_ = nagx;
    this->num_icon_elems_ = nic;
    this->num_econ_elems_ = nec;
}

void hven::solvers::NonLinearProgram::analyze_partitioning() {
    // Loops over the master list of objectives and constraints and assigns
    // them to num_partitions_ work partitions. Each partition's work is
    // dispatched as a single task to the global thread pool.
    this->part_obj_.clear();
    this->part_eq_.clear();
    this->part_iq_.clear();

    this->part_obj_.resize(this->num_partitions_);
    this->part_eq_.resize(this->num_partitions_);
    this->part_iq_.resize(this->num_partitions_);

    int rrPart = 0;

    auto analyzeOP = [&](auto &SourceFuncs, auto &TargetPartFuncs) {
        for (auto &func : SourceFuncs) {
            if (func.get_thread_mode() ==
                ThreadingFlags::MainThread) { // Force to last partition — parallel_sequence runs
                                              // the last index inline on the calling thread, so
                                              // MainThread functions stay safe.
                TargetPartFuncs.back().push_back(func);
            } else if (func.get_thread_mode() == ThreadingFlags::RoundRobin) {
                TargetPartFuncs[rrPart].push_back(func);
                rrPart++;
                if (rrPart > (this->num_partitions_ - 1))
                    rrPart = 0;
            } else if (static_cast<int>(func.get_thread_mode()) >=
                       0) { // Specific Partition Assignment
                int part =
                    std::min(static_cast<int>(func.get_thread_mode()), this->num_partitions_ - 1);
                TargetPartFuncs[part].push_back(func);
            } else { // By application
                auto TempPartFuncs = func.thread_split(this->num_partitions_);
                for (int i = 0; i < TempPartFuncs.size(); i++) {
                    TargetPartFuncs[i].push_back(TempPartFuncs[i]);
                }
            }
        }
    };

    analyzeOP(this->objectives_, this->part_obj_);
    analyzeOP(this->equality_constraints_, this->part_eq_);
    analyzeOP(this->inequality_constraints_, this->part_iq_);
}

void hven::solvers::NonLinearProgram::get_mat_space() {
    // Loops over all constraints and objectives on each partition and has each
    // claim its own portion of kkt_coeff_cols_/kkt_coeff_rows_, tagging each
    // element with the partition that will operate on it; from that, calculates
    // which columns/rows of the KKT matrix need locking when multiple
    // partitions scatter into it, and allocates kkt_locks_ accordingly.
    //
    // Canonical-column locking protocol: every KKT scatter site
    // (DenseFunctionBase::kkt_fill_all and ::kkt_fill_hess) locks each
    // element's mutex on the slot's canonical column --
    // hven::solvers::kkt_canonical_lock_col(row, col), the smaller endpoint,
    // which is the same endpoint analyze_sparsity stores the physical slot
    // under. The clash detection below marks contested columns with that SAME
    // shared keying function, so cross-partition claimants of one physical slot
    // agree on the mutex BY CONSTRUCTION -- there is no per-site convention that
    // can drift, and hence no runtime check (see kkt_canonical_lock_col's doc
    // comment for the structural argument). Writes within one partition need no
    // mutual exclusion -- each partition's scatter runs serially on a single
    // thread (parallel_sequence dispatches one task per partition), and
    // single-partition problems take no locks at all (no column can be claimed
    // by more than one partition, so kkt_clashes_ is all -1).

    // THE CLAIM SEAM, in the contract's own spelling. The cursor a piece claims
    // out of is a KktClaimSpace: the two index arrays, the next free slot, the
    // row base for the kind being claimed, the domains being claimed, and the
    // storage state the assembled matrix will be in. The domains replace the
    // dojac/dohess boolean pair -- "which structures does this piece claim
    // slots for?" is then a question with a named answer rather than one
    // inferred from two positional flags -- and the storage state is resolved
    // ONCE, here, which is what lets every later scatter write
    // values[table.location(slot)] without ever branching on which triangle
    // that offset names.
    //
    // The pieces are still handed the space's fields rather than the space
    // itself: their claim method is the type-erased piece surface, and
    // retiring its spelling is a change to the pieces, not to the layout that
    // drives them.
    KktClaimSpace space{this->kkt_coeff_rows_.head(this->num_user_kkt_elems_),
                        this->kkt_coeff_cols_.head(this->num_user_kkt_elems_),
                        0,
                        0,
                        ClaimDomainSet(),
                        KktStorage::kUpperTriangle};

    const ClaimDomainSet objective_domains = ClaimDomain::kHessian;
    const ClaimDomainSet equality_domains = ClaimDomain::kHessian | ClaimDomain::kEqualityJacobian;
    const ClaimDomainSet inequality_domains =
        ClaimDomain::kHessian | ClaimDomain::kInequalityJacobian;

    const int eqoffset = this->reduced_primal_vars_count_ + this->slack_vars_;
    const int iqoffset = this->reduced_primal_vars_count_ + this->slack_vars_ + this->equal_cons_;

    auto claim = [&](auto &pieces, ClaimDomainSet domains, int row_offset) {
        space.domains_ = domains;
        space.constraint_row_offset_ = row_offset;
        const bool jacobian = domains.contains(ClaimDomain::kEqualityJacobian) ||
                              domains.contains(ClaimDomain::kInequalityJacobian);
        const bool hessian = domains.contains(ClaimDomain::kHessian);
        for (auto &piece : pieces) {
            piece.get_kkt_space(space.rows_, space.cols_, space.next_free_,
                                space.constraint_row_offset_, jacobian, hessian);
        }
    };

    for (int i = 0; i < this->num_partitions_; i++) {
        int kkstart = space.next_free_;

        claim(this->part_obj_[i], objective_domains, 0);
        claim(this->part_eq_[i], equality_domains, eqoffset);
        claim(this->part_iq_[i], inequality_domains, iqoffset);

        int kklen = space.next_free_ - kkstart;

        this->kkt_coeff_part_ids_.segment(kkstart, kklen).setConstant(i);
    }

    // Mark a KKT column contested iff >= 2 partitions write a slot whose CANONICAL column
    // (kkt_canonical_lock_col(row, col), the smaller endpoint) is that column -- the same
    // shared keying function every scatter site locks with, so a contested slot's writers
    // all map to the mutex allocated here. (Keying on the raw recorded column
    // kkt_coeff_cols_[i] instead -- the outer-loop variable -- would mis-attribute
    // mirror-order Hessian writes and leave genuinely shared slots unlocked, which
    // is the race this keying closes.)
    Eigen::MatrixXi KKTclash(this->num_partitions_, this->kkt_dim_);
    KKTclash.setZero();
    for (int i = 0; i < this->num_user_kkt_elems_; i++) {
        // An element belonging to an eliminated variable names no matrix entry
        // (get_kkt_space recorded it as (-1, -1)) and is never written, so it
        // must not mark a column contested either. Its own column does not exist
        // in this space at all.
        if (this->kkt_coeff_rows_[i] < 0 || this->kkt_coeff_cols_[i] < 0) {
            continue;
        }
        int lockcol = kkt_canonical_lock_col(this->kkt_coeff_rows_[i], this->kkt_coeff_cols_[i]);
        int thrid = this->kkt_coeff_part_ids_[i];
        KKTclash(thrid, lockcol) = 1;
    }

    this->kkt_clashes_.resize(this->kkt_dim_);
    this->num_kkt_clashes_ = 0;

    for (int i = 0; i < this->kkt_dim_; i++) {
        if (KKTclash.col(i).sum() > 1) {
            this->kkt_clashes_[i] = num_kkt_clashes_;
            num_kkt_clashes_++;
        } else {
            this->kkt_clashes_[i] = -1;
        }
    }
    std::vector<std::mutex> kktemp(this->num_kkt_clashes_);

    this->kkt_locks_.swap(kktemp);
}

void hven::solvers::NonLinearProgram::get_rhs_space() {
    int PGXfreeloc = 0;
    int AGXfreeloc = 0;
    int FXEfreeloc = 0;
    int FXIfreeloc = 0;

    for (int i = 0; i < this->num_partitions_; i++) {
        for (auto &obj : this->part_obj_[i]) {
            obj.get_gradient_space(this->pgx_coeff_rows(), PGXfreeloc);
        }
        for (auto &eq : this->part_eq_[i]) {
            eq.get_gradient_space(this->agx_coeff_rows(), AGXfreeloc);
            eq.get_constraint_space(this->econ_coeff_rows(), FXEfreeloc);
        }
        for (auto &ineq : this->part_iq_[i]) {
            ineq.get_gradient_space(this->agx_coeff_rows(), AGXfreeloc);
            ineq.get_constraint_space(this->icon_coeff_rows(), FXIfreeloc);
        }
    }
}

void hven::solvers::NonLinearProgram::set_mat_dimensions() {
    // Sized over the space the solver actually factorizes: the full variable
    // space until a configuration eliminates bound-fixed variables from it.
    this->kkt_dim_ = this->reduced_primal_vars_count_ + this->slack_vars_ + this->equal_cons_ +
                     this->inequal_cons_;

    // Storage order of the solver data block.
    this->num_solver_kkt_elems_ = this->slack_vars_                  // solver ijac slack ones
                                  + this->reduced_primal_vars_count_ // solver primal hess diags
                                  + this->slack_vars_                // solver slack hessian diags
                                  + this->equal_cons_                // solver equal pivots
                                  + this->inequal_cons_;             // solver inequal pivots

    this->slack_jac_data_start_ = 0;
    this->primal_diags_data_start_ = this->slack_jac_data_start_ + this->slack_vars_;
    this->slack_diag_data_start_ =
        this->primal_diags_data_start_ + this->reduced_primal_vars_count_;
    this->e_pivot_data_start_ = this->slack_diag_data_start_ + this->slack_vars_;
    this->i_pivot_data_start_ = this->e_pivot_data_start_ + this->equal_cons_;

    this->solver_coeffs_ = Eigen::VectorXd::Zero(this->num_solver_kkt_elems_);

    this->num_kkt_elems_ = this->num_user_kkt_elems_ + this->num_solver_kkt_elems_;

    this->kkt_coeff_rows_ = Eigen::VectorXi::Constant(this->num_kkt_elems_, -1);
    this->kkt_coeff_cols_ = Eigen::VectorXi::Constant(this->num_kkt_elems_, -1);
    this->kkt_coeff_part_ids_ = Eigen::VectorXi::Constant(this->num_kkt_elems_, 0);
    this->kkt_locations_ = Eigen::VectorXi::Constant(this->num_kkt_elems_, -1);

    this->solver_coeffs_ = Eigen::VectorXd::Constant(this->num_solver_kkt_elems_, 0);
}

void hven::solvers::NonLinearProgram::set_rhs_dimensions() {
    this->num_rhs_elems_ =
        this->num_pgx_elems_ + this->num_agx_elems_ + this->num_econ_elems_ + this->num_icon_elems_;

    this->pgx_data_start_ = 0;
    this->agx_data_start_ = this->num_pgx_elems_;
    this->econ_data_start_ = this->agx_data_start_ + this->num_agx_elems_;
    this->icon_data_start_ = this->econ_data_start_ + this->num_econ_elems_;

    this->rhs_coeffs_ = Eigen::VectorXd::Zero(this->num_rhs_elems_);
    this->rhs_coeff_rows_ = Eigen::VectorXi::Constant(this->num_rhs_elems_, -1);
}

void hven::solvers::NonLinearProgram::finalize_data() {
    // Solver-owned elements sit at fixed offsets in the space being factorized,
    // so every offset here counts from the solver's primal width.
    const int pv = this->reduced_primal_vars_count_;

    for (int i = 0; i < pv; i++) {
        this->primal_diag_coeff_cols()[i] = i;
        this->primal_diag_coeff_rows()[i] = i;
    }

    for (int i = 0; i < this->equal_cons_; i++) {
        this->e_pivot_coeff_cols()[i] = pv + this->slack_vars_ + i;
        this->e_pivot_coeff_rows()[i] = pv + this->slack_vars_ + i;
    }

    for (int i = 0; i < this->inequal_cons_; i++) {
        this->slack_coeff_cols()[i] = pv + i;
        this->slack_coeff_rows()[i] = pv + this->slack_vars_ + this->equal_cons_ + i;

        this->slack_diag_coeff_cols()[i] = pv + i;
        this->slack_diag_coeff_rows()[i] = pv + i;

        this->i_pivot_coeff_cols()[i] = pv + this->slack_vars_ + this->equal_cons_ + i;
        this->i_pivot_coeff_rows()[i] = pv + this->slack_vars_ + this->equal_cons_ + i;
    }
}

void hven::solvers::NonLinearProgram::analyze_sparsity(
    Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat) {
    // InteriorPointSolver requires that only the upper triangular part of a CSR
    // matrix be filled. get_mat_space calculates the non-zeros of the lower
    // triangular part, so this routine transposes the row-column indices when
    // making the triplet vector Eigen uses to build the compressed upper-
    // triangular pattern. It then back-calculates where every element named by
    // kkt_coeff_rows_[i]/kkt_coeff_cols_[i] is summed into that matrix and
    // stores it in kkt_locations_, which every scatter reads.
    //
    // THE CLAIM ARRAYS ARE READ, NEVER REWRITTEN: each element's canonical
    // endpoint ordering -- smaller endpoint outer, larger inner -- is derived
    // here, per element, in both passes. Writing it back into the claim arrays
    // would leave the layout with no readable record of the stream the pieces
    // actually handed out (a claim whose endpoints had been swapped would be
    // indistinguishable afterwards from one that never needed swapping).
    // Deriving costs a pair of compares per element in a loop that already
    // branches on the same two values, and it lets the structural key's claim
    // conjunct be taken on demand instead of during every layout.
    KKTmat.resize(this->kkt_dim_, this->kkt_dim_);
    std::vector<Eigen::Triplet<double>> kktvec(this->num_kkt_elems_,
                                               Eigen::Triplet<double>(0, 0, 0.0));

    auto TripFillOP = [&](int start, int stop) {
        for (int i = start; i < stop; i++) {
            int row = this->kkt_coeff_rows_[i];
            int col = this->kkt_coeff_cols_[i];
            if (row < 0 || col < 0) {
                // Element of an eliminated variable. It keeps its claim so the
                // scatter's cursor still walks the claims in lockstep, but it
                // names no matrix entry: this placeholder adds 0.0 to a slot
                // that always exists (index 0's own diagonal, which finalize_data
                // lays down for every solver primal column -- and there is always
                // at least one, since configure_variable_treatment refuses a
                // configuration that would eliminate every variable) and its
                // location is left at -1, which
                // the scatter reads as "step over". This is why the eliminated
                // variable's row and column are simply absent from the pattern.
                kktvec[i] = Eigen::Triplet<double>(0, 0, 0.0);
                continue;
            }
            // The stored triplet is (smaller endpoint, larger endpoint), which
            // is the upper-triangular entry this element sums into. Derived per
            // element and NOT written back: the claim arrays keep the stream
            // the pieces handed out, in claim order, which is what the
            // structural key's claim conjunct is defined over and what makes
            // that conjunct derivable for as long as the layout stands.
            kktvec[i] = Eigen::Triplet<double>(std::min(row, col), std::max(row, col), 1.0);
        }
    };
    hven::utils::parallel_blocks(this->num_kkt_elems_, TripFillOP, this->num_partitions_);

    KKTmat.setFromTriplets(kktvec.begin(), kktvec.end());
    KKTmat.makeCompressed();

    Eigen::VectorXi innerKKTNNZ(this->kkt_dim_);

    for (int i = 0; i < this->kkt_dim_; i++) {
        innerKKTNNZ[i] = KKTmat.row(i).nonZeros();
    }

    auto FindOP = [&](int start, int stop) {
        for (int i = start; i < stop; i++) {
            int row = this->kkt_coeff_rows_(i);
            int col = this->kkt_coeff_cols_(i);
            if (row < 0 || col < 0) {
                continue; // eliminated element: its location stays -1
            }
            // The same endpoint ordering the triplet above was stored under.
            const int outer = std::min(row, col);
            const int inner = std::max(row, col);
            for (int k = 0; k < innerKKTNNZ[outer]; k++) {
                int trow = KKTmat.innerIndexPtr()[KKTmat.outerIndexPtr()[outer] + k];
                if (trow == inner) {
                    this->kkt_locations_[i] = KKTmat.outerIndexPtr()[outer] + k;
                    break;
                }
            }
        }
    };

    hven::utils::parallel_blocks(this->num_kkt_elems_, FindOP, this->num_partitions_);

    // kkt_locations_ now holds offsets into THIS matrix's value array and no
    // other's. The address of that array is captured HERE, as a VALUE, after
    // the resize and the compression above -- which are exactly what move it.
    //
    // Capturing the address rather than re-reading it from the matrix later is
    // what makes the check at the assemble entry mean anything. A consumer that
    // resizes or re-patterns THIS SAME MATRIX moves its value array and leaves
    // every offset here describing storage that is gone; a live re-read would
    // move with it and agree, which is the failure inverted. And a matrix
    // destroyed since this call cannot be read at all, while the entry's
    // accessor runs on every assemble.
    this->analyzed_kkt_values_ = KKTmat.valuePtr();
    this->analyzed_kkt_matrix_ = &KKTmat;
}

bool hven::solvers::NonLinearProgram::configure_variable_treatment(
    FixedVariableTreatments treatment, double bound_relax_factor) {

    // Written as a switch with no default label, like
    // fixed_variable_treatment_name, so that adding a treatment to the enum makes
    // the compiler point here rather than letting a new value fall through as
    // "unrecognized".
    bool treatment_known = false;
    switch (treatment) {
    case FixedVariableTreatments::MakeParameter:
    case FixedVariableTreatments::MakeConstraint:
    case FixedVariableTreatments::RelaxBounds:
        treatment_known = true;
        break;
    }
    if (!treatment_known) {
        throw std::invalid_argument(fmt::format(
            "configure_variable_treatment: unrecognized fixed-variable treatment ({0}); "
            "expected one of '{1}', '{2}', '{3}'",
            static_cast<int>(treatment),
            fixed_variable_treatment_name(FixedVariableTreatments::MakeParameter),
            fixed_variable_treatment_name(FixedVariableTreatments::MakeConstraint),
            fixed_variable_treatment_name(FixedVariableTreatments::RelaxBounds)));
    }
    if (!std::isfinite(bound_relax_factor) || bound_relax_factor < 0.0) {
        throw std::invalid_argument(
            fmt::format("configure_variable_treatment: bound_relax_factor must be finite and "
                        ">= 0 (got {0})",
                        bound_relax_factor));
    }

    // Idempotence: same treatment, same relax factor, same bound state -> the
    // structures already on hand are the answer, and nothing was rebuilt.
    if (this->fixed_treatment_valid_ && this->variable_treatment_ == treatment &&
        this->bound_relax_factor_ == bound_relax_factor &&
        this->configured_bounds_revision_ == this->bounds_revision_) {
        return false;
    }

    this->variable_treatment_ = treatment;
    this->bound_relax_factor_ = bound_relax_factor;

    const int num_vars = this->primal_vars_;
    const bool was_reduced = this->fixed_reduction_active_;
    constexpr double kInf = std::numeric_limits<double>::infinity();

    // What this treatment does with a variable whose bounds are equal, decided
    // once here and read wherever the three paths differ: eliminate it from the
    // solver's variable space, hand it to an internal equality row, or keep it as
    // a two-sided bounded variable with its bounds pushed apart.
    const bool eliminate_fixed = (treatment == FixedVariableTreatments::MakeParameter);
    const bool constrain_fixed = (treatment == FixedVariableTreatments::MakeConstraint);
    const bool relax_fixed = (treatment == FixedVariableTreatments::RelaxBounds);

    // Set immediately before either of the two statements below that re-lay the
    // KKT structures, and read by the restore alongside was_reduced.
    // rebuild_structures() leaves kkt_locations_ reset to -1 (only
    // analyze_sparsity ever fills it), and the restore rethrows rather than
    // reporting a rebuild -- so a restore that calls it unconditionally hands
    // the caller an NLP whose scatter targets are all -1, with no signal that
    // the pattern has to be re-analyzed. A classification-stage rejection
    // arriving at an NLP that was NOT reduced touches no layout, so for that
    // case the rest of the restore suffices and the analyzed pattern survives.
    bool layout_touched = false;

    // Set when this call has changed the SET of functions the problem is made of
    // -- the MakeConstraint treatment installing or dropping its internal fixing
    // rows, and nothing else. The element counts and the work partitioning are
    // derived from that set, so re-laying the layout alone would not describe the
    // problem the NLP now holds; every exit that sees this flag set, the restore
    // included, re-derives all three.
    bool functions_touched = false;

    // Everything from here to the successful exits is inside the restore. The
    // first statement below already rewrites derived state, so any throw past
    // this point -- classification rejecting a bound, the all-fixed rejection,
    // a failed map install -- would otherwise leave the maps describing one
    // problem while is_reduced() and the reduced width still describe another.
    // On an NLP that was already reduced that combination reads off the end of
    // the maps, silently, since Eigen's bounds checks are compiled out.
    try {
        this->variable_bound_set_.clear();
        this->full_to_reduced_.resize(0);
        this->reduced_to_full_.resize(0);
        this->fixed_idx_.resize(0);
        this->fixed_vals_.resize(0);

        // The internal fixing rows a previous configuration installed belong to
        // the classification that produced them, so they go before this one
        // starts. Dropping them up front -- rather than at the point the new
        // treatment would install its own -- is what makes a treatment SWITCH
        // clean: every path out of here, the restore included, then starts from
        // the user's own row space and cannot leave a stale row behind.
        if (this->internal_fixed_cons_ > 0) {
            this->discard_fixed_variable_rows();
            functions_touched = true;
        }

        const bool bounds_materialized = (num_vars > 0 && this->x_lower_.size() == num_vars &&
                                          this->x_upper_.size() == num_vars);

        // --- Classification -------------------------------------------------
        int num_fixed = 0;
        if (bounds_materialized) {
            for (int i = 0; i < num_vars; i++) {
                if (this->x_lower_[i] == this->x_upper_[i]) {
                    if (!std::isfinite(this->x_lower_[i])) {
                        throw std::invalid_argument(fmt::format(
                            "configure_variable_treatment: variable {0} has equal but non-finite "
                            "bounds ({1}); a fixed variable needs a finite value",
                            i, this->x_lower_[i]));
                    }
                    num_fixed++;
                }
            }
        }

        // Under RelaxBounds the widening is the ONLY thing separating a declared
        // [c, c] pair, so a zero factor leaves a "two-sided" variable whose
        // interval has zero width: the interior push has no interior to land in
        // and the barrier takes the log of zero. Caught here rather than as a
        // non-finite objective a few evaluations later. The reference does not
        // guard this case; rejecting it is a deliberate improvement.
        if (relax_fixed && num_fixed > 0 && !(bound_relax_factor > 0.0)) {
            throw std::invalid_argument(fmt::format(
                "configure_variable_treatment: the '{0}' treatment relies on the "
                "bound_relax_factor * max(1, |bound|) widening to separate the bounds of the "
                "{1} variable(s) whose bounds are equal, and a factor of {2} separates them "
                "not at all. Pass a positive factor, or use the '{3}' treatment.",
                fixed_variable_treatment_name(FixedVariableTreatments::RelaxBounds), num_fixed,
                bound_relax_factor,
                fixed_variable_treatment_name(FixedVariableTreatments::MakeParameter)));
        }

        // The fixed variables themselves are recorded whatever the treatment:
        // MakeConstraint builds its rows out of them, and under the other two they
        // are what fixed_variable_indices()/_values() report. The index MAPS are
        // narrower than that -- they exist only where something is actually
        // eliminated, and on every other path they stay empty and every consumer
        // treats the mapping as the identity.
        if (num_fixed > 0) {
            this->fixed_idx_.resize(num_fixed);
            this->fixed_vals_.resize(num_fixed);
        }
        if (eliminate_fixed && num_fixed > 0) {
            this->full_to_reduced_ = Eigen::VectorXi::Constant(num_vars, -1);
            this->reduced_to_full_.resize(num_vars - num_fixed);
        }

        std::vector<int> lower_idx;
        std::vector<double> lower_val;
        std::vector<double> lower_damp;
        std::vector<int> upper_idx;
        std::vector<double> upper_val;
        std::vector<double> upper_damp;

        int next_reduced = 0;
        int next_fixed = 0;
        for (int i = 0; i < (bounds_materialized ? num_vars : 0); i++) {
            const double lower = this->x_lower_[i];
            const double upper = this->x_upper_[i];
            const bool is_fixed = (lower == upper);

            if (is_fixed) {
                this->fixed_idx_[next_fixed] = i;
                this->fixed_vals_[next_fixed] = lower;
                next_fixed++;

                if (eliminate_fixed) {
                    // Gone from the solver's variable space: no column, no bound,
                    // and next_reduced does not advance past it.
                    continue;
                }
                // RelaxBounds needs nothing of its own here, and that IS the
                // treatment: the variable falls through to the shared two-sided
                // path below, which moves each finite bound out by the relax factor
                // times max(1, |bound|) -- ONE such move per side, by exactly the
                // rule every other declared bound gets, so a pair declared at
                // [c, c] reaches the solver 2 * factor * max(1, |c|) wide. The
                // reference applies the widening once as well: its adapter enters a
                // relaxed variable in both bound maps at [c, c] and does not count
                // it among the fixed ones, so the adapter's own separation loop
                // (which iterates the fixed count) never runs on this path and the
                // widening comes solely from the universal relax factor.
                //
                // MakeConstraint records nothing here either, for a different
                // reason: the internal equality row installed below is what holds
                // the variable, and the variable itself reaches the solver free,
                // keeping its column and carrying no bound for the barrier.
            }

            if (eliminate_fixed && num_fixed > 0) {
                this->full_to_reduced_[i] = next_reduced;
                this->reduced_to_full_[next_reduced] = i;
            }

            // Recorded in the space the solver iterates in, and widened by the relax
            // factor so consumers never have to re-apply it. The damping
            // indicator is recorded alongside because this loop is the only place
            // that still sees both endpoints of a variable at once -- once the
            // two lists are separate, "is this bound the only one on its
            // variable" is no longer a local question.
            const bool record_bounds = !is_fixed || relax_fixed;
            if (record_bounds && lower > -kInf) {
                lower_idx.push_back(next_reduced);
                lower_val.push_back(lower - bound_relax_factor * std::max(1.0, std::abs(lower)));
                lower_damp.push_back(upper < kInf ? 0.0 : 1.0);
            }
            if (record_bounds && upper < kInf) {
                upper_idx.push_back(next_reduced);
                upper_val.push_back(upper + bound_relax_factor * std::max(1.0, std::abs(upper)));
                upper_damp.push_back(lower > -kInf ? 0.0 : 1.0);
            }
            next_reduced++;
        }

        auto fill_bound_list = [](const std::vector<int> &idx, const std::vector<double> &val,
                                  const std::vector<double> &damp, Eigen::VectorXi &idx_out,
                                  Eigen::VectorXd &val_out, Eigen::VectorXd &damp_out) {
            const int count = static_cast<int>(idx.size());
            idx_out.resize(count);
            val_out.resize(count);
            damp_out.resize(count);
            for (int i = 0; i < count; i++) {
                idx_out[i] = idx[i];
                val_out[i] = val[i];
                damp_out[i] = damp[i];
            }
        };
        fill_bound_list(lower_idx, lower_val, lower_damp, this->variable_bound_set_.lower_idx_,
                        this->variable_bound_set_.lower_val_,
                        this->variable_bound_set_.lower_damp_);
        fill_bound_list(upper_idx, upper_val, upper_damp, this->variable_bound_set_.upper_idx_,
                        this->variable_bound_set_.upper_val_,
                        this->variable_bound_set_.upper_damp_);

        // --- Treatment ------------------------------------------------------
        //
        // Commit-on-success: fixed_treatment_valid_ and configured_bounds_revision_
        // are stamped ONLY at the three successful exits below. Everything from here
        // on can throw, and a configuration that threw must not be remembered as
        // done -- otherwise the next call would take the idempotence shortcut and
        // the solve would proceed on the unreduced problem with every fixing bound
        // silently ignored. Leaving the flag clear makes the next call re-attempt,
        // and fail the same way, until the bounds are corrected.
        if (eliminate_fixed && num_fixed > 0 && num_fixed == num_vars) {
            // Nothing would be left to solve for, and a zero-width primal block is
            // not a system this solver can lay out. Rejected here rather than as a
            // degenerate factorization later. The classification above has already
            // rewritten the maps by this point, which is why this throw is inside
            // the restore.
            //
            // Only this treatment can reach it: the other two keep every variable,
            // so an all-fixed problem is a square system of internal rows under
            // MakeConstraint and a fully boxed one under RelaxBounds.
            throw std::invalid_argument(fmt::format(
                "configure_variable_treatment: all {0} primal variables are fixed by "
                "their bounds, leaving no variable to solve for. The '{1}' and '{2}' "
                "treatments keep every variable and can solve this problem.",
                num_vars, fixed_variable_treatment_name(FixedVariableTreatments::MakeConstraint),
                fixed_variable_treatment_name(FixedVariableTreatments::RelaxBounds)));
        }

        if (constrain_fixed && num_fixed > 0) {
            // One internal equality row per fixed variable, appended to the master
            // list before the counts and the partitioning below are re-derived --
            // so the rows are sized, partitioned and laid out exactly as a user's
            // own constraint would have been.
            this->install_fixed_variable_rows();
            functions_touched = true;
        }

        const bool reduce_now = (eliminate_fixed && num_fixed > 0);
        this->reduced_primal_vars_count_ = reduce_now ? num_vars - num_fixed : num_vars;
        this->fixed_reduction_active_ = reduce_now;

        if (!reduce_now) {
            if (!was_reduced && !functions_touched) {
                // Identity path, and it already was: nothing to rebuild. Both
                // treatments that keep every variable land here whenever they
                // changed no row either -- RelaxBounds always, MakeConstraint when
                // nothing is fixed.
                this->fixed_treatment_valid_ = true;
                this->configured_bounds_revision_ = this->bounds_revision_;
                return false;
            }
            // Either an elimination is no longer in force (the last fixing bound
            // was dropped, or the treatment changed to one that keeps the
            // variable), or the equality row space changed under MakeConstraint.
            // Put every function back on its pristine input map and lay the
            // structures out over the full variable space.
            layout_touched = true;
            if (functions_touched) {
                this->refresh_function_partitions();
            }
            this->clear_function_output_maps();
            this->rebuild_structures();
            this->fixed_treatment_valid_ = true;
            this->configured_bounds_revision_ = this->bounds_revision_;
            return true;
        }

        layout_touched = true;
        // Before the map install, not after: re-partitioning hands out fresh copies
        // of the master functions, and those copies carry the pristine input map.
        if (functions_touched) {
            this->refresh_function_partitions();
        }
        this->install_function_output_maps();
        this->rebuild_structures();

        if (this->kkt_dim_ != this->reduced_primal_vars_count_ + this->slack_vars_ +
                                  this->equal_cons_ + this->inequal_cons_) {
            throw std::logic_error(fmt::format(
                "configure_variable_treatment: kkt_dim ({0}) != reduced primal_vars ({1}) + "
                "slack_vars ({2}) + equal_cons ({3}) + inequal_cons ({4})",
                this->kkt_dim_, this->reduced_primal_vars_count_, this->slack_vars_,
                this->equal_cons_, this->inequal_cons_));
        }
        if (this->num_kkt_elems_ != this->num_user_kkt_elems_ + this->num_solver_kkt_elems_) {
            throw std::logic_error(fmt::format(
                "configure_variable_treatment: KKT element bookkeeping is inconsistent "
                "(num_kkt_elems {0}, user {1}, solver {2})",
                this->num_kkt_elems_, this->num_user_kkt_elems_, this->num_solver_kkt_elems_));
        }
    } catch (...) {
        // Put the whole problem back on the full variable space before letting the
        // exception out, so a caller that catches it holds a coherent (if
        // unconfigured) NLP rather than one describing two problems at once. Two
        // ways it could: a partial map install leaves some functions on a reduced
        // output map and others on the pristine one; and a throw during
        // classification leaves maps sized for a reduction that is not in effect,
        // which -- on an NLP that WAS reduced -- would be read off the end by the
        // next expansion, silently, since Eigen's bounds checks are compiled out.
        // Clearing the maps and the reduction flag closes both.
        //
        // Sequenced deliberately: the allocation-free work first, the rebuild
        // last, so even if the rebuild itself fails only the KKT layout is left
        // stale and no index space is left mixed.
        //
        // The rebuild runs when this call re-laid the layout itself, OR when the
        // NLP arrived here ALREADY REDUCED. The second condition is the one a
        // narrower guard gets wrong: the two lines below declare the problem
        // full-width again, but the structures on hand were laid out for the
        // reduced width, so skipping the rebuild leaves kkt_dim_ and every
        // block offset describing the reduced problem while the width says
        // otherwise -- exactly the two-problems-at-once state this restore
        // exists to prevent. Only a classification-stage rejection at an NLP
        // that was NOT reduced leaves the layout genuinely untouched, and that
        // is the one case worth sparing an analyzed sparsity pattern for.
        //
        // The internal fixing rows get the same treatment for the same reason. A
        // rejection reaching here with rows installed -- or, just as importantly,
        // with a previous configuration's rows already dropped -- leaves the
        // equality row space describing something other than what the counts and
        // the partitioning were derived from, so both are re-derived before the
        // layout is, and the row space itself goes back to the user's own.
        // The validity stamp goes too, and it has to: commit-on-success only ever
        // SETS it, so a rejection arriving at an NLP that a previous call had
        // configured would otherwise leave it true -- while the treatment and the
        // factor above were already overwritten with the rejected call's own, and
        // the bound revision is unchanged. The next call with those same arguments
        // would then match the idempotence shortcut exactly and report "no change"
        // on a problem this restore has just put back to unconfigured, so a solve
        // would proceed with every fixing bound silently ignored. Clearing it is
        // what makes the rejection re-attempt (and fail the same way) instead.
        this->fixed_treatment_valid_ = false;
        this->configured_bounds_revision_ = -1;
        this->reduced_primal_vars_count_ = num_vars;
        this->fixed_reduction_active_ = false;
        this->full_to_reduced_.resize(0);
        this->reduced_to_full_.resize(0);
        this->fixed_idx_.resize(0);
        this->fixed_vals_.resize(0);
        if (this->internal_fixed_cons_ > 0) {
            this->discard_fixed_variable_rows();
            functions_touched = true;
        }
        if (functions_touched)
            this->refresh_function_partitions();
        this->clear_function_output_maps();
        if (was_reduced || layout_touched || functions_touched)
            this->rebuild_structures();
        throw;
    }

    this->fixed_treatment_valid_ = true;
    this->configured_bounds_revision_ = this->bounds_revision_;
    return true;
}

void hven::solvers::NonLinearProgram::gather_reduced_x(ConstEigenRef<VectorXd> x_full,
                                                       EigenRef<VectorXd> x_reduced) const {
    if (x_full.size() != this->primal_vars_ ||
        x_reduced.size() != this->reduced_primal_vars_count_) {
        throw std::invalid_argument(fmt::format(
            "gather_reduced_x: expected a {0}-element full vector and a "
            "{1}-element reduced vector (got {2} and {3})",
            this->primal_vars_, this->reduced_primal_vars_count_, x_full.size(), x_reduced.size()));
    }
    if (!this->fixed_reduction_active_) {
        x_reduced = x_full;
        return;
    }
    for (int i = 0; i < this->reduced_primal_vars_count_; i++) {
        x_reduced[i] = x_full[this->reduced_to_full_[i]];
    }
}

void hven::solvers::NonLinearProgram::scatter_full_x(ConstEigenRef<VectorXd> x_reduced,
                                                     EigenRef<VectorXd> x_full) const {
    if (x_full.size() != this->primal_vars_ ||
        x_reduced.size() != this->reduced_primal_vars_count_) {
        throw std::invalid_argument(fmt::format(
            "scatter_full_x: expected a {0}-element full vector and a "
            "{1}-element reduced vector (got {2} and {3})",
            this->primal_vars_, this->reduced_primal_vars_count_, x_full.size(), x_reduced.size()));
    }
    if (!this->fixed_reduction_active_) {
        x_full = x_reduced;
        return;
    }
    for (int i = 0; i < this->reduced_primal_vars_count_; i++) {
        x_full[this->reduced_to_full_[i]] = x_reduced[i];
    }
    for (int j = 0; j < this->fixed_idx_.size(); j++) {
        x_full[this->fixed_idx_[j]] = this->fixed_vals_[j];
    }
}

// THE EIGHT EVALUATION SHAPES
//
// Each shape is a PASS (the partitioned fan-out plus the internal-scratch
// zeroing it owns) and then its FILLS. The two public ways in compose them
// differently, and only in the ways the contract says they differ:
//
//   * an eval_ entry starts from the solver's reduced iterate, fills all four
//     right-hand-side blocks, and scatters the solver's own KKT coefficients;
//   * assemble() starts from a declaration-space point, fills only the arenas
//     its request names, and does NOT scatter the solver coefficients -- that
//     step transfers to the consumer.
//
// A pass evaluates nothing its entry does not.

void hven::solvers::NonLinearProgram::first_order_rhs_pass(double ObjScale,
                                                           ConstEigenRef<VectorXd> Xf,
                                                           ConstEigenRef<VectorXd> LE,
                                                           ConstEigenRef<VectorXd> LI,
                                                           double &val) {
    this->vals_scratch_.assign(this->num_partitions_, 0.0);
    this->set_rhs_coeffs_zero();

    auto RHSevalOP = [&](int thrnum) {
        double localVal = 0.0;
        for (auto &Obj : this->part_obj_[thrnum])
            Obj.objective_gradient(ObjScale, Xf, localVal, this->pgx_coeffs());
        for (auto &Con : this->part_eq_[thrnum])
            Con.constraints_adjointgradient(Xf, LE, this->econ_coeffs(), this->agx_coeffs());
        for (auto &Con : this->part_iq_[thrnum])
            Con.constraints_adjointgradient(Xf, LI, this->icon_coeffs(), this->agx_coeffs());
        this->vals_scratch_[thrnum] = localVal;
    };

    hven::utils::parallel_sequence(this->num_partitions_, RHSevalOP);
    for (int i = 0; i < this->num_partitions_; i++)
        val += this->vals_scratch_[i];
}

void hven::solvers::NonLinearProgram::objective_gradient_constraints_pass(
    double ObjScale, ConstEigenRef<VectorXd> Xf, double &val) {
    this->vals_scratch_.assign(this->num_partitions_, 0.0);
    this->set_rhs_coeffs_zero();

    auto OGCevalOP = [&](int thrnum) {
        double localVal = 0.0;
        for (auto &Obj : this->part_obj_[thrnum])
            Obj.objective_gradient(ObjScale, Xf, localVal, this->pgx_coeffs());
        for (auto &Con : this->part_eq_[thrnum])
            Con.constraints(Xf, this->econ_coeffs());
        for (auto &Con : this->part_iq_[thrnum])
            Con.constraints(Xf, this->icon_coeffs());
        this->vals_scratch_[thrnum] = localVal;
    };

    hven::utils::parallel_sequence(this->num_partitions_, OGCevalOP);
    for (int i = 0; i < this->num_partitions_; i++)
        val += this->vals_scratch_[i];
}

void hven::solvers::NonLinearProgram::objective_constraints_pass(double ObjScale,
                                                                 ConstEigenRef<VectorXd> Xf,
                                                                 double &val) {
    this->vals_scratch_.assign(this->num_partitions_, 0.0);
    this->set_con_coeffs_zero();

    auto OGCevalOP = [&](int thrnum) {
        double localVal = 0.0;
        for (auto &Obj : this->part_obj_[thrnum])
            Obj.objective(ObjScale, Xf, localVal);
        for (auto &Con : this->part_eq_[thrnum])
            Con.constraints(Xf, this->econ_coeffs());
        for (auto &Con : this->part_iq_[thrnum])
            Con.constraints(Xf, this->icon_coeffs());
        this->vals_scratch_[thrnum] = localVal;
    };

    hven::utils::parallel_sequence(this->num_partitions_, OGCevalOP);
    for (int i = 0; i < this->num_partitions_; i++)
        val += this->vals_scratch_[i];
}

void hven::solvers::NonLinearProgram::objective_pass(double ObjScale, ConstEigenRef<VectorXd> Xf,
                                                     double &val) {
    this->vals_scratch_.assign(this->num_partitions_, 0.0);

    auto OGCevalOP = [&](int thrnum) {
        double localVal = 0.0;
        for (auto &Obj : this->part_obj_[thrnum])
            Obj.objective(ObjScale, Xf, localVal);
        this->vals_scratch_[thrnum] = localVal;
    };

    hven::utils::parallel_sequence(this->num_partitions_, OGCevalOP);
    for (int i = 0; i < this->num_partitions_; i++)
        val += this->vals_scratch_[i];
}

void hven::solvers::NonLinearProgram::full_kkt_pass(
    double ObjScale, ConstEigenRef<VectorXd> Xf, ConstEigenRef<VectorXd> LE,
    ConstEigenRef<VectorXd> LI, double &val, Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat) {
    this->vals_scratch_.assign(this->num_partitions_, 0.0);

    this->set_rhs_coeffs_zero();

    auto KKTevalOP = [&](int thrnum) {
        double localVal = 0.0;
        for (auto &Obj : this->part_obj_[thrnum])
            Obj.objective_gradient_hessian(ObjScale, Xf, localVal, this->pgx_coeffs(), KKTmat,
                                           this->kkt_locations_, this->kkt_clashes_,
                                           this->kkt_locks_);
        for (auto &Con : this->part_eq_[thrnum])
            Con.constraints_jacobian_adjointgradient_adjointhessian(
                Xf, LE, this->econ_coeffs(), this->agx_coeffs(), KKTmat, this->kkt_locations_,
                this->kkt_clashes_, this->kkt_locks_);
        for (auto &Con : this->part_iq_[thrnum])
            Con.constraints_jacobian_adjointgradient_adjointhessian(
                Xf, LI, this->icon_coeffs(), this->agx_coeffs(), KKTmat, this->kkt_locations_,
                this->kkt_clashes_, this->kkt_locks_);
        this->vals_scratch_[thrnum] = localVal;
    };

    hven::utils::parallel_sequence(this->num_partitions_, KKTevalOP);
    for (int i = 0; i < this->num_partitions_; i++)
        val += this->vals_scratch_[i];
}

void hven::solvers::NonLinearProgram::constraint_kkt_pass(
    ConstEigenRef<VectorXd> Xf, ConstEigenRef<VectorXd> LE, ConstEigenRef<VectorXd> LI,
    Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat) {
    this->set_rhs_coeffs_zero();

    auto KKTevalOP = [&](int thrnum) {
        for (auto &Con : this->part_eq_[thrnum])
            Con.constraints_jacobian_adjointgradient_adjointhessian(
                Xf, LE, this->econ_coeffs(), this->agx_coeffs(), KKTmat, this->kkt_locations_,
                this->kkt_clashes_, this->kkt_locks_);
        for (auto &Con : this->part_iq_[thrnum])
            Con.constraints_jacobian_adjointgradient_adjointhessian(
                Xf, LI, this->icon_coeffs(), this->agx_coeffs(), KKTmat, this->kkt_locations_,
                this->kkt_clashes_, this->kkt_locks_);
    };

    hven::utils::parallel_sequence(this->num_partitions_, KKTevalOP);
}

void hven::solvers::NonLinearProgram::constraint_jacobian_pass(
    ConstEigenRef<VectorXd> Xf, Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat) {
    this->set_rhs_coeffs_zero();

    auto SOEevalOP = [&](int thrnum) {
        for (auto &Con : this->part_eq_[thrnum])
            Con.constraints_jacobian(Xf, this->econ_coeffs(), KKTmat, this->kkt_locations_,
                                     this->kkt_clashes_, this->kkt_locks_);
        for (auto &Con : this->part_iq_[thrnum])
            Con.constraints_jacobian(Xf, this->icon_coeffs(), KKTmat, this->kkt_locations_,
                                     this->kkt_clashes_, this->kkt_locks_);
    };

    hven::utils::parallel_sequence(this->num_partitions_, SOEevalOP);
}

void hven::solvers::NonLinearProgram::first_order_kkt_pass(
    double ObjScale, ConstEigenRef<VectorXd> Xf, ConstEigenRef<VectorXd> LE,
    ConstEigenRef<VectorXd> LI, double &val, Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat) {
    this->vals_scratch_.assign(this->num_partitions_, 0.0);
    this->set_rhs_coeffs_zero();

    auto SOEevalOP = [&](int thrnum) {
        double localVal = 0.0;
        for (auto &Obj : this->part_obj_[thrnum])
            Obj.objective_gradient(ObjScale, Xf, localVal, this->pgx_coeffs());
        for (auto &Con : this->part_eq_[thrnum])
            Con.constraints_jacobian_adjointgradient(
                Xf, LE, this->econ_coeffs(), this->agx_coeffs(), KKTmat, this->kkt_locations_,
                this->kkt_clashes_, this->kkt_locks_);
        for (auto &Con : this->part_iq_[thrnum])
            Con.constraints_jacobian_adjointgradient(
                Xf, LI, this->icon_coeffs(), this->agx_coeffs(), KKTmat, this->kkt_locations_,
                this->kkt_clashes_, this->kkt_locks_);
        this->vals_scratch_[thrnum] = localVal;
    };

    hven::utils::parallel_sequence(this->num_partitions_, SOEevalOP);
    for (int i = 0; i < this->num_partitions_; i++)
        val += this->vals_scratch_[i];
}

// The eight entry points. Each is its pass plus its fills.
//
// Every one of them starts by turning the solver's iterate into the buffer the
// functions read. primal_view hands them a buffer in the problem's own variable
// space: on the identity path a view of X itself, and once variables are
// eliminated the reduced iterate expanded back into its own coordinates with
// the pinned values in place. Nothing downstream can tell the difference, which
// is why eliminated variables' contributions to constraint values and to the
// surviving variables' derivatives need no handling of their own.


void hven::solvers::NonLinearProgram::eval_rhs(double ObjScale, ConstEigenRef<VectorXd> X,
                                               ConstEigenRef<VectorXd> LE,
                                               ConstEigenRef<VectorXd> LI, double &val,
                                               EigenRef<VectorXd> PGX, EigenRef<VectorXd> AGX,
                                               EigenRef<VectorXd> FXE, EigenRef<VectorXd> FXI) {
    this->first_order_rhs_pass(ObjScale, this->primal_view(X), LE, LI, val);
    this->fill_rhs(PGX, AGX, FXE, FXI);
}

void hven::solvers::NonLinearProgram::eval_ogc(double ObjScale, ConstEigenRef<VectorXd> X,
                                               double &val, EigenRef<VectorXd> PGX,
                                               EigenRef<VectorXd> FXE, EigenRef<VectorXd> FXI) {
    this->objective_gradient_constraints_pass(ObjScale, this->primal_view(X), val);
    this->fill_pgx(PGX);
    this->fill_fxe(FXE);
    this->fill_fxi(FXI);
}

void hven::solvers::NonLinearProgram::eval_occ(double ObjScale, ConstEigenRef<VectorXd> X,
                                               double &val, EigenRef<VectorXd> FXE,
                                               EigenRef<VectorXd> FXI) {
    this->objective_constraints_pass(ObjScale, this->primal_view(X), val);
    this->fill_fxe(FXE);
    this->fill_fxi(FXI);
}

void hven::solvers::NonLinearProgram::eval_obj(double ObjScale, ConstEigenRef<VectorXd> X,
                                               double &val) {
    this->objective_pass(ObjScale, this->primal_view(X), val);
}

void hven::solvers::NonLinearProgram::eval_kkt(
    double ObjScale, ConstEigenRef<VectorXd> X, ConstEigenRef<VectorXd> LE,
    ConstEigenRef<VectorXd> LI, double &val, EigenRef<VectorXd> PGX, EigenRef<VectorXd> AGX,
    EigenRef<VectorXd> FXE, EigenRef<VectorXd> FXI,
    Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat) {
    this->full_kkt_pass(ObjScale, this->primal_view(X), LE, LI, val, KKTmat);

    // NOTE: fill_solver_coeffs internally calls parallel_blocks, creating a nested
    // dispatch from the inline arm. Safe because: (1) the calling thread is the main
    // thread (not a pool worker), so the pool absorbs all tasks without deadlock, and
    // (2) fill_rhs and fill_solver_coeffs operate on disjoint data (RHS vectors vs. KKT
    // matrix entries), so concurrent execution requires no synchronization.
    hven::utils::parallel_task(
        this->num_partitions_, [&] { this->fill_rhs(PGX, AGX, FXE, FXI); },
        [&] { this->fill_solver_coeffs(KKTmat); });
}

void hven::solvers::NonLinearProgram::eval_kkt_no(
    double ObjScale, ConstEigenRef<VectorXd> X, ConstEigenRef<VectorXd> LE,
    ConstEigenRef<VectorXd> LI, double &val, EigenRef<VectorXd> PGX, EigenRef<VectorXd> AGX,
    EigenRef<VectorXd> FXE, EigenRef<VectorXd> FXI,
    Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat) {
    // No-objective mode: ObjScale and val are unused but kept in the signature
    // for API consistency with eval_kkt/eval_aug (polymorphic dispatch via evalNLP).
    (void)ObjScale;
    (void)val;

    this->constraint_kkt_pass(this->primal_view(X), LE, LI, KKTmat);

    // NOTE: nested dispatch from inline arm — see comment in eval_kkt.
    hven::utils::parallel_task(
        this->num_partitions_, [&] { this->fill_rhs(PGX, AGX, FXE, FXI); },
        [&] { this->fill_solver_coeffs(KKTmat); });
}

void hven::solvers::NonLinearProgram::eval_soe(
    double ObjScale, ConstEigenRef<VectorXd> X, ConstEigenRef<VectorXd> LE,
    ConstEigenRef<VectorXd> LI, double &val, EigenRef<VectorXd> PGX, EigenRef<VectorXd> AGX,
    EigenRef<VectorXd> FXE, EigenRef<VectorXd> FXI,
    Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat) {
    // Constraint-only mode: ObjScale and val are unused but kept in the signature
    // for API consistency with eval_kkt/eval_aug (polymorphic dispatch via evalNLP).
    (void)ObjScale;
    (void)val;

    this->constraint_jacobian_pass(this->primal_view(X), KKTmat);

    // NOTE: nested dispatch from inline arm — see comment in eval_kkt.
    hven::utils::parallel_task(
        this->num_partitions_, [&] { this->fill_rhs(PGX, AGX, FXE, FXI); },
        [&] { this->fill_solver_coeffs(KKTmat); });
}

void hven::solvers::NonLinearProgram::eval_aug(
    double ObjScale, ConstEigenRef<VectorXd> X, ConstEigenRef<VectorXd> LE,
    ConstEigenRef<VectorXd> LI, double &val, EigenRef<VectorXd> PGX, EigenRef<VectorXd> AGX,
    EigenRef<VectorXd> FXE, EigenRef<VectorXd> FXI,
    Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat) {
    this->first_order_kkt_pass(ObjScale, this->primal_view(X), LE, LI, val, KKTmat);

    // NOTE: nested dispatch from inline arm — see comment in eval_kkt.
    hven::utils::parallel_task(
        this->num_partitions_, [&] { this->fill_rhs(PGX, AGX, FXE, FXI); },
        [&] { this->fill_solver_coeffs(KKTmat); });
}

Eigen::Ref<const hven::solvers::NonLinearProgram::VectorXd>
hven::solvers::NonLinearProgram::declaration_view(ConstEigenRef<VectorXd> x) {
    if (!this->fixed_reduction_active_) {
        return x;
    }
    // Every retained coordinate straight through, every eliminated one pinned
    // at its declared value -- byte for byte what gather_reduced_x followed by
    // scatter_full_x would have produced, so both entry paths hand the pieces
    // the same buffer.
    this->full_x_scratch_ = x;
    for (int j = 0; j < this->fixed_idx_.size(); j++) {
        this->full_x_scratch_[this->fixed_idx_[j]] = this->fixed_vals_[j];
    }
    return this->full_x_scratch_;
}

namespace {

/// The consumer's arena as a vector the existing fills can write. The arena's
/// location table IS the engine's own row table for that arena, so this is the
/// same fill the eval_ entries do, into storage the consumer published instead
/// of a vector it passed.
inline Eigen::Map<Eigen::VectorXd> arena_vector(const hven::solvers::RhsArenaView &arena) {
    return Eigen::Map<Eigen::VectorXd>(arena.values_, arena.size_);
}

/// The provider's half of the destination checks, for the arenas the entry can
/// only test for presence.
///
/// The entry validates what the CONTRACT can see, and the width this engine
/// laid its gradient arenas over is not among those things: a declaration
/// reports the DECLARED variable count, while an elimination narrows the space
/// these arenas are actually addressed in. So the entry cannot tell a correctly
/// sized view from a short one -- and a short one is not a harmless mistake.
/// The fill walks the published table and writes target[row] for every claim,
/// so a view shorter than the laid width is written off its end.
///
/// One comparison per named arena, at this hook's entry, before anything is
/// evaluated.
void require_laid_width(const hven::solvers::RhsArenaView &view, int laid_width,
                        const char *arena) {
    if (view.size_ != laid_width) {
        throw std::invalid_argument(fmt::format(
            "assemble: the {0} arena's view is {1} rows, but this provider laid its {0} over {2} "
            "rows. Claims scatter through the published table by row, so a view of any other "
            "length is addressed off its end.",
            arena, view.size_, laid_width));
    }
}

} // namespace

namespace {

/// @brief The spelling of a named evaluation shape, for a refusal that has to
///        say WHICH shape it is refusing.
///
/// A refusal that reports only the flag combination makes the reader decode
/// hex against the mapping table before they know what was asked for. Every
/// shape assemble() lets through is one of the named ones -- validate_eval_request
/// has already refused everything else -- so the fallback is unreachable in
/// practice and exists only so this returns a total function.
///
/// KEEP THIS LIST IN STEP with the named shapes in model/candidate_point.h. It
/// is a second enumeration of them, and a shape added there but not here does
/// not fail to build: it falls through to the fallback, and the refusal then
/// reads "an unnamed flag combination, the flag combination 0x..." for a shape
/// that does have a name.
///
/// @param request the shape to name.
/// @return the enumerator's spelling.
const char *refused_shape_name(hven::solvers::EvalRequest request) {
    using namespace hven::solvers;
    if (request == kRequestObjectiveOnly) {
        return "kRequestObjectiveOnly";
    }
    if (request == kRequestObjectiveAndConstraints) {
        return "kRequestObjectiveAndConstraints";
    }
    if (request == kRequestObjectiveGradientAndConstraints) {
        return "kRequestObjectiveGradientAndConstraints";
    }
    if (request == kRequestFirstOrderRhs) {
        return "kRequestFirstOrderRhs";
    }
    if (request == kRequestConstraintResidualsAndJacobian) {
        return "kRequestConstraintResidualsAndJacobian";
    }
    if (request == kRequestFirstOrderKkt) {
        return "kRequestFirstOrderKkt";
    }
    if (request == kRequestConstraintKkt) {
        return "kRequestConstraintKkt";
    }
    if (request == kRequestFullKkt) {
        return "kRequestFullKkt";
    }
    if (request == kRequestLagrangianHessian) {
        return "kRequestLagrangianHessian";
    }
    if (request == kRequestGradientAndJacobians) {
        return "kRequestGradientAndJacobians";
    }
    if (request == kRequestConstraintJacobiansOnly) {
        return "kRequestConstraintJacobiansOnly";
    }
    return "an unnamed flag combination";
}

} // namespace

void hven::solvers::NonLinearProgram::assemble_impl(const CandidatePoint &point,
                                                    EvalRequest request, KktScatterView kkt,
                                                    RhsScatterView rhs) {
    // The laid-width half of the destination checks, which is this provider's
    // to make: the entry established that a named gradient arena HAS storage
    // and a table, and could establish no more than that, because the width
    // these arenas are laid over is the solver's primal block and not the
    // declared variable count.
    //
    // Every destination check the hook makes comes first, before any state of
    // this provider's is read -- which is what require_laid_width's own
    // contract says it is for.
    if (has_request(request, EvalRequest::kObjectiveGradient)) {
        require_laid_width(rhs.objective_gradient_, this->reduced_primal_vars_count_,
                           "objective gradient");
    }
    if (has_request(request, EvalRequest::kConstraintAdjointGradient)) {
        require_laid_width(rhs.constraint_adjoint_gradient_, this->reduced_primal_vars_count_,
                           "constraint adjoint gradient");
    }

    constexpr EvalRequest kKktBearing = EvalRequest::kObjectiveHessian |
                                        EvalRequest::kConstraintJacobian |
                                        EvalRequest::kConstraintAdjointHessian;
    const bool needs_kkt = (request & kKktBearing) != EvalRequest::kNone;
    if (needs_kkt && this->analyzed_kkt_matrix_ == nullptr) {
        // WHICH MEMBER ANSWERS "HAS THE ANALYSIS RUN?", and it is not the one
        // that answers "is this the right destination?". The captured ADDRESS
        // is the identity token, and a legitimately analysed matrix can carry a
        // null one: Eigen's value pointer is null for a matrix with no
        // nonzeros, so a degenerate-but-analysed problem would report itself as
        // never laid if the sentinel were read off the address. The matrix
        // POINTER cannot be null once the analysis has run, so it is the
        // sentinel; the captured address stays the comparand, and only that.
        throw std::invalid_argument(
            "assemble: this provider's KKT location table has not been laid against any "
            "destination -- run the sparsity analysis on the matrix you intend to fill before "
            "requesting KKT-bearing output");
    }
    // The only place the analysed matrix OBJECT is touched, and only on this
    // path: the piece surface takes a matrix reference, so a KKT-bearing shape
    // has to hand the pieces one. Reaching it here is safe on exactly the ground
    // the entry established a moment ago -- the caller's view names
    // analyzed_kkt_values_, so the consumer is presenting this very array right
    // now. The identity check itself deliberately never comes through here.
    Eigen::SparseMatrix<double, Eigen::RowMajor> *mat = this->analyzed_kkt_matrix_;

    const Eigen::Ref<const VectorXd> Xf = this->declaration_view(point.x_);
    const double scale = point.objective_scale_;

    // The eight interior-owned shapes, in the mapping table's order. Every arm
    // runs exactly the pass its legacy counterpart ran and fills exactly the
    // destinations the request names -- never the solver's own KKT
    // coefficients, which are the consumer's to scatter. The terminal arm
    // REFUSES a legal shape outside this provider's support (the mapping
    // table's per-provider support statement): a bare-else fallback here
    // served the full-KKT pass for any legal-but-unsupported shape -- silent
    // over-evaluation, the defect class the refusal exists to prevent.
    if (request == kRequestObjectiveOnly) {
        this->objective_pass(scale, Xf, *rhs.objective_);
    } else if (request == kRequestObjectiveAndConstraints) {
        this->objective_constraints_pass(scale, Xf, *rhs.objective_);
        this->fill_fxe(arena_vector(rhs.equality_residuals_));
        this->fill_fxi(arena_vector(rhs.inequality_residuals_));
    } else if (request == kRequestObjectiveGradientAndConstraints) {
        this->objective_gradient_constraints_pass(scale, Xf, *rhs.objective_);
        this->fill_pgx(arena_vector(rhs.objective_gradient_));
        this->fill_fxe(arena_vector(rhs.equality_residuals_));
        this->fill_fxi(arena_vector(rhs.inequality_residuals_));
    } else if (request == kRequestFirstOrderRhs) {
        this->first_order_rhs_pass(scale, Xf, point.equality_multipliers_,
                                   point.inequality_multipliers_, *rhs.objective_);
        this->fill_pgx(arena_vector(rhs.objective_gradient_));
        this->fill_agx(arena_vector(rhs.constraint_adjoint_gradient_));
        this->fill_fxe(arena_vector(rhs.equality_residuals_));
        this->fill_fxi(arena_vector(rhs.inequality_residuals_));
    } else if (request == kRequestConstraintResidualsAndJacobian) {
        this->constraint_jacobian_pass(Xf, *mat);
        // Neither gradient arena is filled. The legacy shape passed both
        // buffers in and summed an identically zero contribution into each --
        // no objective piece runs, and the Jacobian entry produces no adjoint
        // gradient at all -- so declining to write them is observationally
        // identical, which is what lets the request leave them empty.
        this->fill_fxe(arena_vector(rhs.equality_residuals_));
        this->fill_fxi(arena_vector(rhs.inequality_residuals_));
    } else if (request == kRequestFirstOrderKkt) {
        this->first_order_kkt_pass(scale, Xf, point.equality_multipliers_,
                                   point.inequality_multipliers_, *rhs.objective_, *mat);
        this->fill_pgx(arena_vector(rhs.objective_gradient_));
        this->fill_agx(arena_vector(rhs.constraint_adjoint_gradient_));
        this->fill_fxe(arena_vector(rhs.equality_residuals_));
        this->fill_fxi(arena_vector(rhs.inequality_residuals_));
    } else if (request == kRequestConstraintKkt) {
        this->constraint_kkt_pass(Xf, point.equality_multipliers_, point.inequality_multipliers_,
                                  *mat);
        // The objective gradient arena is the identically-zero one here; the
        // adjoint gradient this shape genuinely produces.
        this->fill_agx(arena_vector(rhs.constraint_adjoint_gradient_));
        this->fill_fxe(arena_vector(rhs.equality_residuals_));
        this->fill_fxi(arena_vector(rhs.inequality_residuals_));
    } else if (request == kRequestFullKkt) {
        this->full_kkt_pass(scale, Xf, point.equality_multipliers_, point.inequality_multipliers_,
                            *rhs.objective_, *mat);
        this->fill_pgx(arena_vector(rhs.objective_gradient_));
        this->fill_agx(arena_vector(rhs.constraint_adjoint_gradient_));
        this->fill_fxe(arena_vector(rhs.equality_residuals_));
        this->fill_fxi(arena_vector(rhs.inequality_residuals_));
    } else {
        // request is legal (assemble() validated it) but not one of this
        // provider's eight shapes -- rows 9-11 are the SQP driver's, served by
        // the NlpModelAggregate bridge, never by this engine. Refusing by name
        // is the point: a bare-else fallback here would silently run the
        // full-KKT pass for a shape that asked for far less, over-evaluating
        // exactly as the mapping table's per-provider support statement
        // forbids.
        throw std::invalid_argument(fmt::format(
            "assemble: {9}, the flag combination 0x{0:x}, is a legal evaluation shape, but this "
            "provider does not support it. The partitioned evaluation engine serves exactly "
            "these eight shapes: kRequestObjectiveOnly (0x{1:x}), "
            "kRequestObjectiveAndConstraints (0x{2:x}), "
            "kRequestObjectiveGradientAndConstraints (0x{3:x}), kRequestFirstOrderRhs (0x{4:x}), "
            "kRequestConstraintResidualsAndJacobian (0x{5:x}), kRequestFirstOrderKkt (0x{6:x}), "
            "kRequestConstraintKkt (0x{7:x}) and kRequestFullKkt (0x{8:x}); see the mapping "
            "table's per-provider support statement in model/candidate_point.h. Legal and "
            "supported-by-this-provider are distinct facts: this request cleared "
            "validate_eval_request but names no shape among the eight listed above.",
            static_cast<std::uint32_t>(request), static_cast<std::uint32_t>(kRequestObjectiveOnly),
            static_cast<std::uint32_t>(kRequestObjectiveAndConstraints),
            static_cast<std::uint32_t>(kRequestObjectiveGradientAndConstraints),
            static_cast<std::uint32_t>(kRequestFirstOrderRhs),
            static_cast<std::uint32_t>(kRequestConstraintResidualsAndJacobian),
            static_cast<std::uint32_t>(kRequestFirstOrderKkt),
            static_cast<std::uint32_t>(kRequestConstraintKkt),
            static_cast<std::uint32_t>(kRequestFullKkt), refused_shape_name(request)));
    }

    // size_ is not validated on the engine path; identity and non-emptiness
    // are. A wrong-size view with the right pointer is accepted and ignored:
    // the fill addresses analyzed_kkt_matrix_, never the view.
    static_cast<void>(kkt);
}

void hven::solvers::NonLinearProgram::evaluate_candidate_values_impl(const CandidatePoint &point,
                                                                     CandidateValues out) {
    // ASSIGNS, per the candidate surface's own discipline: the caller holds a
    // scratch buffer and wants the values at a point, not a running sum. The
    // pass accumulates into the provider's own claim slots, as it always does;
    // the caller's blocks are zeroed here and then filled, so what they hold on
    // return is this evaluation and nothing else.
    double objective = 0.0;
    this->objective_constraints_pass(point.objective_scale_, this->declaration_view(point.x_),
                                     objective);
    out.objective_ = objective;

    out.equality_residuals_.setZero();
    out.inequality_residuals_.setZero();
    this->fill_fxe(out.equality_residuals_);
    this->fill_fxi(out.inequality_residuals_);
}

void hven::solvers::NonLinearProgram::evaluate_candidate_first_order_impl(
    const CandidatePoint &point, CandidateFirstOrder out) {
    double objective = 0.0;
    this->first_order_rhs_pass(point.objective_scale_, this->declaration_view(point.x_),
                               point.equality_multipliers_, point.inequality_multipliers_,
                               objective);
    out.values_.objective_ = objective;

    out.values_.equality_residuals_.setZero();
    out.values_.inequality_residuals_.setZero();
    this->fill_fxe(out.values_.equality_residuals_);
    this->fill_fxi(out.values_.inequality_residuals_);

    out.objective_gradient_.setZero();
    out.constraint_adjoint_gradient_.setZero();
    if (!this->fixed_reduction_active_) {
        // The solver's primal space IS the declared one, so the fills write the
        // caller's blocks directly.
        this->fill_pgx(out.objective_gradient_);
        this->fill_agx(out.constraint_adjoint_gradient_);
        return;
    }

    // Under an active elimination the fills address the narrower space the
    // solver iterates in, so they land in scratch and are expanded into the
    // caller's declaration-space blocks -- the identity-space principle in one
    // loop.
    //
    // AN ELIMINATED VARIABLE'S ROW COMES BACK ZERO, and that is a limit of the
    // structures rather than a choice made here: the treatment marks its
    // gradient row -1 at claim time, which is the layout's way of saying the
    // row is not part of the reduced problem's residual, so the engine keeps no
    // destination for it. Its value at a solution is the bound multiplier that
    // holds the variable at its bound, which this surface does not carry.
    const int reduced = this->reduced_primal_vars_count_;
    this->candidate_gradient_scratch_.resize(reduced);

    this->candidate_gradient_scratch_.setZero();
    this->fill_pgx(this->candidate_gradient_scratch_);
    for (int i = 0; i < reduced; i++) {
        out.objective_gradient_[this->reduced_to_full_[i]] = this->candidate_gradient_scratch_[i];
    }

    this->candidate_gradient_scratch_.setZero();
    this->fill_agx(this->candidate_gradient_scratch_);
    for (int i = 0; i < reduced; i++) {
        out.constraint_adjoint_gradient_[this->reduced_to_full_[i]] =
            this->candidate_gradient_scratch_[i];
    }
}

hven::solvers::IdentityProbe hven::solvers::NonLinearProgram::probe_identity(ConstVecRef x) {
    // A values evaluation plus a hash, routed through the public entry so it
    // inherits that entry's validation rather than repeating it.
    this->probe_equality_scratch_.setZero(this->equal_cons_);
    this->probe_inequality_scratch_.setZero(this->inequal_cons_);

    double objective = 0.0;
    CandidateValues values{objective, this->probe_equality_scratch_,
                           this->probe_inequality_scratch_};
    const Vec no_multipliers;
    this->evaluate_candidate_values(CandidatePoint{x, no_multipliers, no_multipliers}, values);

    return IdentityProbe{this->structure_epoch(), candidate_value_digest(values)};
}

void hven::solvers::NonLinearProgram::nlp_test(const Eigen::VectorXd &x, int n,
                                               std::shared_ptr<NonLinearProgram> nlp1,
                                               std::shared_ptr<NonLinearProgram> nlp2) {
    using std::cout;
    using std::endl;

    Eigen::SparseMatrix<double, Eigen::RowMajor> KKTmat1(nlp1->kkt_dim_, nlp1->kkt_dim_);
    Eigen::SparseMatrix<double, Eigen::RowMajor> KKTmat2(nlp1->kkt_dim_, nlp1->kkt_dim_);

    nlp1->analyze_sparsity(KKTmat1);
    nlp2->analyze_sparsity(KKTmat2);

    Eigen::VectorXd X = x;

    std::cout << X.size() << endl;

    Eigen::VectorXd FXE1(nlp1->equal_cons_);
    Eigen::VectorXd FXE2(nlp1->equal_cons_);
    FXE1.setZero();
    FXE2.setZero();

    Eigen::VectorXd LE(nlp1->equal_cons_);
    LE.setRandom();
    LE *= 100;

    Eigen::VectorXd FXI1(nlp1->inequal_cons_);
    Eigen::VectorXd FXI2(nlp1->inequal_cons_);
    FXI1.setZero();
    FXI2.setZero();

    Eigen::VectorXd LI(nlp1->inequal_cons_);
    LI.setRandom();
    LI *= 100;
    Eigen::VectorXd PGX1(nlp1->primal_vars_);
    Eigen::VectorXd AGX1(nlp1->primal_vars_);
    PGX1.setZero();
    AGX1.setZero();

    Eigen::VectorXd PGX2(nlp1->primal_vars_);
    Eigen::VectorXd AGX2(nlp1->primal_vars_);
    PGX2.setZero();
    AGX2.setZero();

    double v1 = 0;
    double v2 = 0;

    hven::utils::Timer t1;
    hven::utils::Timer t2;

    hven::utils::Timer t3;
    hven::utils::Timer t4;

    cout << nlp1->kkt_locations_.minCoeff() << endl;
    // nlp2->kkt_clashes_.setConstant(-1);

    for (int i = 0; i < n; i++) {
        std::fill_n(KKTmat1.valuePtr(), KKTmat1.nonZeros(), 0.0);
        std::fill_n(KKTmat2.valuePtr(), KKTmat2.nonZeros(), 0.0);

        t1.start();
        nlp1->eval_kkt(1.0, X, LE, LI, v1, PGX1, AGX1, FXE1, FXI1, KKTmat1);
        t1.stop();

        t2.start();
        nlp2->eval_kkt(1.0, X, LE, LI, v2, PGX2, AGX2, FXE2, FXI2, KKTmat2);
        t2.stop();

        if (i % 10 == 0) {
            double maxval = 0;
            double maxrow = 0;
            double maxcol = 0;
            Eigen::SparseMatrix<double, Eigen::RowMajor> mat = (KKTmat1 - KKTmat2).cwiseAbs();
            for (int k = 0; k < mat.outerSize(); ++k)
                for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(mat, k); it;
                     ++it) {
                    it.value();
                    if (it.value() > maxval) {
                        maxval = it.value();
                        maxrow = it.row();
                        maxcol = it.col();
                    }
                }

            int e_err_idx = 0;
            double FXErr = (FXE1 - FXE2).cwiseAbs().maxCoeff(&e_err_idx);
            int i_err_idx = 0;
            double FXIrr = (FXI1 - FXI2).cwiseAbs().maxCoeff(&i_err_idx);
            int gx_err_idx = 0;
            double GXIrr = (PGX1 - PGX2).cwiseAbs().maxCoeff(&gx_err_idx);
            int agx_err_idx = 0;
            double AGXIrr = (AGX1 - AGX2).cwiseAbs().maxCoeff(&agx_err_idx);

            std::cout << "KKTmat Diff:" << maxval << " row: " << maxrow << "  col:" << maxcol
                      << endl;
            std::cout << "FXE Diff:" << FXErr << " row: " << e_err_idx << endl;
            std::cout << "FXI Diff:" << FXIrr << " row: " << i_err_idx << endl;
            std::cout << "PGX Diff:" << GXIrr << " row: " << gx_err_idx << endl;
            std::cout << "AGX Diff:" << AGXIrr << " row: " << agx_err_idx << endl;
        }

        t3.start();
        nlp1->eval_occ(1.0, X, v1, FXE1, FXI1);
        t3.stop();

        t4.start();
        nlp2->eval_occ(1.0, X, v2, FXE2, FXI2);
        t4.stop();

        FXE1.setZero();
        FXI1.setZero();
        PGX1.setZero();
        AGX1.setZero();

        FXE2.setZero();
        FXI2.setZero();
        PGX2.setZero();
        AGX2.setZero();
        LI.setRandom();
        LI *= 100;
        LE.setRandom();
        LE *= 100;
    }

    double t1t = double(t1.count<std::chrono::microseconds>()) / 1000.0;
    double t2t = double(t2.count<std::chrono::microseconds>()) / 1000.0;
    double t3t = double(t3.count<std::chrono::microseconds>()) / 1000.0;
    double t4t = double(t4.count<std::chrono::microseconds>()) / 1000.0;

    cout << t1t / double(n) << " ms" << endl;
    cout << t2t / double(n) << " ms" << endl;

    cout << t3t / double(n) << " ms" << endl;
    cout << t4t / double(n) << " ms" << endl;
}
