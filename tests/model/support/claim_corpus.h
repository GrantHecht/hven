// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// claim_corpus.h — laid NonLinearProgram fixtures whose claim stream is worth
// reading, and an INDEPENDENT reference restatement to read it against.
//
// THE REFERENCE IS THE POINT OF THIS FILE. NonLinearProgram publishes its claim
// stream by classifying each laid slot with the intra-partition cursor marks its
// own lay recorded. The reference below classifies the same slots a different
// way -- by the ROW BAND of the assembled space, plus the per-slot partition ids
// the layout publishes anyway -- and restates them straight from the convention
// stated in model/claim_stream_source.h: constraint rows drop the slack offset,
// Hessian pairs are ordered into the upper triangle, and the domain runs come
// out in partition-index order. Two derivations, no shared code, one answer.
//
// The fixtures are laid and never evaluated, so every piece's evaluation surface
// below is a stub: what is under test is the layout, and a numeric surface would
// only be a second thing to keep true.

#include <algorithm>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include "hven/detail/interior/constraint_function.h"
#include "hven/detail/interior/indexing_data.h"
#include "hven/detail/interior/objective_function.h"
#include "hven/model/non_linear_program.h"
#include "hven/solver_interface_adapter.h"

namespace hven::model_tests {

// ---------------------------------------------------------------------------
// The pieces
// ---------------------------------------------------------------------------

/// @brief A constraint piece over two variables per application: one Jacobian
///        entry per variable, and -- when it owns the Hessian -- the coupling of
///        the two plus one diagonal.
///
/// The coupling is claimed with its LARGER endpoint first, deliberately. A
/// Hessian claim is left in the walk order the piece chose, and the restatement
/// is what puts it in the upper triangle; a fixture that only ever claimed
/// already-ordered pairs would let a restatement that forgot to order them pass.
/// NO std::string MEMBER, deliberately. TypeStorage's inline buffer relocates
/// what it holds with memcpy (detail/interior/utils/type_storage.h), so a stored
/// piece must be trivially relocatable and an SSO string is self-referential.
/// The name is composed on demand from a literal and an index instead.
struct CorpusConstraintPiece {
    bool owns_hessian_ = false;
    const char *kind_ = "corpus_constraint";
    int index_ = 0;

    std::string name() const { return std::string(kind_) + std::to_string(index_); }
    int input_rows() const { return 2; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }

    int num_kkt_elements(bool dojac, bool dohess) const {
        return (dojac ? 2 : 0) + ((dohess && owns_hessian_) ? 2 : 0);
    }

    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> rows, Eigen::Ref<Eigen::VectorXi> cols,
                       int &freeloc, int conoffset, bool dojac, bool dohess,
                       hven::solvers::SolverIndexingData &data) {
        data.inner_kkt_starts_.resize(data.num_appl());
        for (int appl = 0; appl < data.num_appl(); appl++) {
            data.inner_kkt_starts_[appl] = freeloc;
            const int first = data.v_scatter_loc(0, appl);
            const int second = data.v_scatter_loc(1, appl);
            const int row = data.c_loc(0, appl) + conoffset;
            if (dojac) {
                rows[freeloc] = (first < 0) ? -1 : row;
                cols[freeloc] = first;
                freeloc++;
                rows[freeloc] = (second < 0) ? -1 : row;
                cols[freeloc] = second;
                freeloc++;
            }
            if (dohess && owns_hessian_) {
                const bool live = (first >= 0 && second >= 0);
                rows[freeloc] = live ? std::max(first, second) : -1;
                cols[freeloc] = live ? std::min(first, second) : -1;
                freeloc++;
                rows[freeloc] = first;
                cols[freeloc] = first;
                freeloc++;
            }
        }
    }

    // ---- the evaluation surface, stubbed: these fixtures are laid, not run ---
    void constraints(const Eigen::Ref<const Eigen::VectorXd> &, Eigen::Ref<Eigen::VectorXd>,
                     const hven::solvers::SolverIndexingData &) const {}
    void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &,
                                     const Eigen::Ref<const Eigen::VectorXd> &,
                                     Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
                                     const hven::solvers::SolverIndexingData &) const {}
    void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &,
                              Eigen::Ref<Eigen::VectorXd>,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                              Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>,
                              std::vector<std::mutex> &,
                              const hven::solvers::SolverIndexingData &) const {}
    void constraints_jacobian_adjointgradient(
        const Eigen::Ref<const Eigen::VectorXd> &, const Eigen::Ref<const Eigen::VectorXd> &,
        Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &, Eigen::Ref<Eigen::VectorXi>,
        Eigen::Ref<Eigen::VectorXi>, std::vector<std::mutex> &,
        const hven::solvers::SolverIndexingData &) const {}
    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Eigen::VectorXd> &, const Eigen::Ref<const Eigen::VectorXd> &,
        Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &, Eigen::Ref<Eigen::VectorXi>,
        Eigen::Ref<Eigen::VectorXi>, std::vector<std::mutex> &,
        const hven::solvers::SolverIndexingData &) const {}
};

/// @brief An objective piece over two variables per application, claiming the
///        coupling (larger endpoint first, as above) and one diagonal.
///
/// Objectives are asked for Hessian space alone, so every slot this claims is a
/// Hessian slot -- which is the case the restatement classifies without reading
/// a row at all.
/// Trivially relocatable, for the reason stated on CorpusConstraintPiece.
struct CorpusObjectivePiece {
    bool owns_hessian_ = true;
    const char *kind_ = "corpus_objective";
    int index_ = 0;

    std::string name() const { return std::string(kind_) + std::to_string(index_); }
    int input_rows() const { return 2; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }

    int num_kkt_elements(bool, bool dohess) const { return (dohess && owns_hessian_) ? 2 : 0; }

    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> rows, Eigen::Ref<Eigen::VectorXi> cols,
                       int &freeloc, int, bool, bool dohess,
                       hven::solvers::SolverIndexingData &data) {
        data.inner_kkt_starts_.resize(data.num_appl());
        for (int appl = 0; appl < data.num_appl(); appl++) {
            data.inner_kkt_starts_[appl] = freeloc;
            if (!dohess || !owns_hessian_) {
                continue;
            }
            const int first = data.v_scatter_loc(0, appl);
            const int second = data.v_scatter_loc(1, appl);
            const bool live = (first >= 0 && second >= 0);
            rows[freeloc] = live ? std::max(first, second) : -1;
            cols[freeloc] = live ? std::min(first, second) : -1;
            freeloc++;
            rows[freeloc] = second;
            cols[freeloc] = second;
            freeloc++;
        }
    }

    void constraints(const Eigen::Ref<const Eigen::VectorXd> &, Eigen::Ref<Eigen::VectorXd>,
                     const hven::solvers::SolverIndexingData &) const {}
    void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &,
                                     const Eigen::Ref<const Eigen::VectorXd> &,
                                     Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
                                     const hven::solvers::SolverIndexingData &) const {}
    void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &,
                              Eigen::Ref<Eigen::VectorXd>,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                              Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>,
                              std::vector<std::mutex> &,
                              const hven::solvers::SolverIndexingData &) const {}
    void constraints_jacobian_adjointgradient(
        const Eigen::Ref<const Eigen::VectorXd> &, const Eigen::Ref<const Eigen::VectorXd> &,
        Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &, Eigen::Ref<Eigen::VectorXi>,
        Eigen::Ref<Eigen::VectorXi>, std::vector<std::mutex> &,
        const hven::solvers::SolverIndexingData &) const {}
    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Eigen::VectorXd> &, const Eigen::Ref<const Eigen::VectorXd> &,
        Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &, Eigen::Ref<Eigen::VectorXi>,
        Eigen::Ref<Eigen::VectorXi>, std::vector<std::mutex> &,
        const hven::solvers::SolverIndexingData &) const {}

    void objective(double, const Eigen::Ref<const Eigen::VectorXd> &, double &value,
                   const hven::solvers::SolverIndexingData &) const {
        value = 0.0;
    }
    void objective_gradient(double, const Eigen::Ref<const Eigen::VectorXd> &, double &value,
                            Eigen::Ref<Eigen::VectorXd>,
                            const hven::solvers::SolverIndexingData &) const {
        value = 0.0;
    }
    void objective_gradient_hessian(double, const Eigen::Ref<const Eigen::VectorXd> &,
                                    double &value, Eigen::Ref<Eigen::VectorXd>,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                                    Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>,
                                    std::vector<std::mutex> &,
                                    const hven::solvers::SolverIndexingData &) const {
        value = 0.0;
    }
};

} // namespace hven::model_tests

namespace hven::solvers {
template <>
struct SolverInterfaceAdapter<hven::model_tests::CorpusConstraintPiece>
    : DirectFunctionModel<hven::model_tests::CorpusConstraintPiece> {};
template <>
struct SolverInterfaceAdapter<hven::model_tests::CorpusObjectivePiece>
    : DirectFunctionModel<hven::model_tests::CorpusObjectivePiece> {};
} // namespace hven::solvers

namespace hven::model_tests {

// ---------------------------------------------------------------------------
// The fixtures
// ---------------------------------------------------------------------------

/// @brief One corpus model, as a recipe. The lay is derived from it, never
///        stored, so a case can be re-laid and compared against itself.
struct CorpusCase {
    std::string name_;
    int objective_pieces_ = 1;
    int equality_pieces_ = 1;
    int inequality_pieces_ = 1;
    /// Applications per piece. The KKT element count has to clear
    /// kMinKktElementsPerPartition per partition or the lay caps the count.
    int applications_ = 8;
    int requested_partitions_ = 1;
    /// Whether the constraint pieces own Hessian claims, which is what puts
    /// Hessian slots INSIDE the two mixed segments rather than only in the
    /// objective one.
    bool constraint_hessians_ = true;
    hven::solvers::ThreadingFlags equality_mode_ = hven::solvers::ThreadingFlags::ByApplication;
    hven::solvers::ThreadingFlags inequality_mode_ = hven::solvers::ThreadingFlags::ByApplication;
    hven::solvers::ThreadingFlags objective_mode_ = hven::solvers::ThreadingFlags::ByApplication;
    /// Variables narrowed to a point, which a fixed-variable treatment turns
    /// into an elimination or into an internal fixing row.
    std::vector<int> fixed_variables_;
};

/// Variables the case is laid over: two per application per constraint piece,
/// so no two applications of one piece share a column and the Jacobian claim of
/// each row names its own pair.
inline int corpus_primal_vars(const CorpusCase &c) {
    const int pieces = std::max(1, c.equality_pieces_ + c.inequality_pieces_ + c.objective_pieces_);
    return 2 * c.applications_ * pieces;
}

inline hven::solvers::ConstraintFunction corpus_constraint(const CorpusCase &c, int first_variable,
                                                           int first_row, bool owns_hessian,
                                                           const char *kind, int index) {
    Eigen::MatrixXi v_index(2, c.applications_);
    Eigen::MatrixXi c_index(1, c.applications_);
    for (int appl = 0; appl < c.applications_; appl++) {
        v_index(0, appl) = first_variable + 2 * appl;
        v_index(1, appl) = first_variable + 2 * appl + 1;
        c_index(0, appl) = first_row + appl;
    }
    CorpusConstraintPiece piece;
    piece.owns_hessian_ = owns_hessian;
    piece.kind_ = kind;
    piece.index_ = index;
    return hven::solvers::ConstraintFunction(hven::solvers::ConstraintInterface(std::move(piece)),
                                             v_index, c_index);
}

inline hven::solvers::ObjectiveFunction corpus_objective(const CorpusCase &c, int first_variable,
                                                         int index) {
    Eigen::MatrixXi v_index(2, c.applications_);
    for (int appl = 0; appl < c.applications_; appl++) {
        v_index(0, appl) = first_variable + 2 * appl;
        v_index(1, appl) = first_variable + 2 * appl + 1;
    }
    CorpusObjectivePiece piece;
    piece.index_ = index;
    return hven::solvers::ObjectiveFunction(hven::solvers::ObjectiveInterface(std::move(piece)),
                                            v_index);
}

/// @brief Lays one corpus case and returns the program.
inline std::shared_ptr<hven::solvers::NonLinearProgram> build_corpus(const CorpusCase &c) {
    auto nlp = std::make_shared<hven::solvers::NonLinearProgram>(c.requested_partitions_);

    const int primal = corpus_primal_vars(c);
    int next_variable = 0;

    for (int p = 0; p < c.objective_pieces_; p++) {
        auto piece = corpus_objective(c, next_variable, p);
        piece.set_thread_mode(c.objective_mode_);
        nlp->objectives_.push_back(std::move(piece));
        next_variable += 2 * c.applications_;
    }
    for (int p = 0; p < c.equality_pieces_; p++) {
        auto piece = corpus_constraint(c, next_variable, p * c.applications_,
                                       c.constraint_hessians_, "corpus_equality", p);
        piece.set_thread_mode(c.equality_mode_);
        nlp->equality_constraints_.push_back(std::move(piece));
        next_variable += 2 * c.applications_;
    }
    for (int p = 0; p < c.inequality_pieces_; p++) {
        auto piece = corpus_constraint(c, next_variable, p * c.applications_,
                                       c.constraint_hessians_, "corpus_inequality", p);
        piece.set_thread_mode(c.inequality_mode_);
        nlp->inequality_constraints_.push_back(std::move(piece));
        next_variable += 2 * c.applications_;
    }

    for (int i = 0; i < primal; i++) {
        nlp->set_variable_bound(i, -3.0, 4.0);
    }
    for (int fixed : c.fixed_variables_) {
        nlp->set_variable_bound(fixed, 0.5, 0.5);
    }

    nlp->make_nlp(primal, c.equality_pieces_ * c.applications_,
                  c.inequality_pieces_ * c.applications_);
    return nlp;
}

/// @brief The corpus: the shapes a restatement has to be right on.
inline std::vector<CorpusCase> corpus_cases() {
    using hven::solvers::ThreadingFlags;
    std::vector<CorpusCase> cases;

    CorpusCase smallest;
    smallest.name_ = "smallest: one piece per list, one application";
    smallest.applications_ = 1;
    cases.push_back(smallest);

    CorpusCase all_three;
    all_three.name_ = "all three lists, one partition";
    all_three.objective_pieces_ = 2;
    all_three.equality_pieces_ = 3;
    all_three.inequality_pieces_ = 2;
    all_three.applications_ = 11;
    cases.push_back(all_three);

    CorpusCase linear_constraints;
    linear_constraints.name_ = "constraint pieces own no Hessian: pure domain segments";
    linear_constraints.equality_pieces_ = 2;
    linear_constraints.inequality_pieces_ = 2;
    linear_constraints.applications_ = 9;
    linear_constraints.constraint_hessians_ = false;
    cases.push_back(linear_constraints);

    CorpusCase objective_only;
    objective_only.name_ = "objective only: two empty domains";
    objective_only.equality_pieces_ = 0;
    objective_only.inequality_pieces_ = 0;
    objective_only.applications_ = 7;
    cases.push_back(objective_only);

    CorpusCase equalities_only;
    equalities_only.name_ = "no objective, no inequalities: an empty gradient block";
    equalities_only.objective_pieces_ = 0;
    equalities_only.equality_pieces_ = 2;
    equalities_only.inequality_pieces_ = 0;
    equalities_only.applications_ = 5;
    cases.push_back(equalities_only);

    CorpusCase inequalities_only;
    inequalities_only.name_ = "objective and inequalities, no equality rows";
    inequalities_only.equality_pieces_ = 0;
    inequalities_only.inequality_pieces_ = 3;
    inequalities_only.applications_ = 6;
    cases.push_back(inequalities_only);

    CorpusCase round_robin;
    round_robin.name_ = "four partitions, round-robin constraint pieces";
    round_robin.objective_pieces_ = 2;
    round_robin.equality_pieces_ = 5;
    round_robin.inequality_pieces_ = 4;
    round_robin.applications_ = 320;
    round_robin.requested_partitions_ = 4;
    round_robin.equality_mode_ = ThreadingFlags::RoundRobin;
    round_robin.inequality_mode_ = ThreadingFlags::RoundRobin;
    round_robin.objective_mode_ = ThreadingFlags::MainThread;
    cases.push_back(round_robin);

    CorpusCase by_application;
    by_application.name_ = "three partitions, every piece split by application";
    by_application.objective_pieces_ = 1;
    by_application.equality_pieces_ = 2;
    by_application.inequality_pieces_ = 2;
    by_application.applications_ = 700;
    by_application.requested_partitions_ = 3;
    cases.push_back(by_application);

    CorpusCase main_thread_only;
    main_thread_only.name_ = "three partitions, every piece pinned to the last one";
    main_thread_only.objective_pieces_ = 1;
    main_thread_only.equality_pieces_ = 2;
    main_thread_only.inequality_pieces_ = 2;
    main_thread_only.applications_ = 600;
    main_thread_only.requested_partitions_ = 3;
    main_thread_only.equality_mode_ = ThreadingFlags::MainThread;
    main_thread_only.inequality_mode_ = ThreadingFlags::MainThread;
    main_thread_only.objective_mode_ = ThreadingFlags::MainThread;
    cases.push_back(main_thread_only);

    CorpusCase lopsided;
    lopsided.name_ = "three partitions, inequalities all in one: empty runs in the others";
    lopsided.objective_pieces_ = 1;
    lopsided.equality_pieces_ = 3;
    lopsided.inequality_pieces_ = 1;
    lopsided.applications_ = 500;
    lopsided.requested_partitions_ = 3;
    lopsided.equality_mode_ = ThreadingFlags::RoundRobin;
    lopsided.inequality_mode_ = ThreadingFlags::Thread0;
    lopsided.objective_mode_ = ThreadingFlags::MainThread;
    cases.push_back(lopsided);

    CorpusCase objective_split;
    objective_split.name_ = "three partitions, three OBJECTIVE pieces split by application";
    objective_split.objective_pieces_ = 3;
    objective_split.equality_pieces_ = 1;
    objective_split.inequality_pieces_ = 1;
    objective_split.applications_ = 700;
    objective_split.requested_partitions_ = 3;
    // The objective list is the one whose claims are ALL Hessian, and the one
    // the restatement classifies without reading a row. Splitting it by
    // application puts an objective-Hessian run in EVERY partition, ahead of two
    // mixed segments that also contribute Hessian claims -- which is the case
    // where the Hessian domain's partition offsets have to interleave three
    // sources correctly rather than trivially.
    objective_split.objective_mode_ = ThreadingFlags::ByApplication;
    objective_split.equality_mode_ = ThreadingFlags::Thread1;
    objective_split.inequality_mode_ = ThreadingFlags::MainThread;
    cases.push_back(objective_split);

    CorpusCase with_fixed;
    with_fixed.name_ = "all three lists with two point-bounded variables";
    with_fixed.objective_pieces_ = 1;
    with_fixed.equality_pieces_ = 2;
    with_fixed.inequality_pieces_ = 1;
    with_fixed.applications_ = 13;
    with_fixed.fixed_variables_ = {3, 20};
    cases.push_back(with_fixed);

    return cases;
}

// ---------------------------------------------------------------------------
// The independent reference
// ---------------------------------------------------------------------------

/// @brief A whole restated claim stream, derived from the convention alone.
struct ReferenceStream {
    std::vector<int> rows_;
    std::vector<int> cols_;
    /// The partition that claimed each PUBLISHED slot, in published order.
    std::vector<int> partitions_;
    std::vector<int> gradient_rows_;
    hven::solvers::ClaimBlock hessian_;
    hven::solvers::ClaimBlock equality_jacobian_;
    hven::solvers::ClaimBlock inequality_jacobian_;
};

/// @brief Restates one laid, unreduced layout's claim slots straight from the
///        claim convention.
///
/// The classification here reads the ROW BAND of the assembled space and
/// NOTHING ELSE: below the primal width is a Hessian claim, the slack band is
/// refused (the claim space has no slack block), the equality row band is an
/// equality Jacobian claim, and the rest is an inequality Jacobian claim. That
/// is a different question from the one the layout's own restatement asks --
/// which piece list claimed this slot -- and the two agreeing is the whole
/// assertion.
///
/// Sort-free by construction: the three domain runs are collected in a single
/// forward walk of the emission order and then concatenated, so partition-major
/// order within each run follows from the partition ids being monotone in
/// emission order rather than from any ordering step here.
inline ReferenceStream reference_restatement(const hven::solvers::NonLinearProgram &nlp) {
    const int primal = nlp.primal_vars_;
    const int slack = nlp.slack_vars_;
    const int equality_end = primal + slack + nlp.equal_cons_;

    ReferenceStream hessian;
    ReferenceStream equality;
    ReferenceStream inequality;

    const auto push = [](ReferenceStream &into, int row, int col, int partition) {
        into.rows_.push_back(row);
        into.cols_.push_back(col);
        into.partitions_.push_back(partition);
    };

    for (int slot = 0; slot < nlp.num_user_kkt_elems_; slot++) {
        const int row = nlp.kkt_coeff_rows_[slot];
        const int col = nlp.kkt_coeff_cols_[slot];
        const int partition = nlp.kkt_coeff_part_ids_[slot];
        if (row < primal) {
            push(hessian, std::min(row, col), std::max(row, col), partition);
        } else if (row < primal + slack) {
            // The claim convention's space has no slack block, so this is not a
            // row a sound layout claims. A HARD STOP, and it really is one: a
            // sentinel pushed into a domain run would be compared against the
            // published stream and reported as an ordinary mismatch, which says
            // nothing about the layout having claimed a row that cannot exist.
            throw std::logic_error("reference_restatement: claim slot " + std::to_string(slot) +
                                   " names KKT row " + std::to_string(row) +
                                   ", a slack row; the claim convention's space has no slack "
                                   "block, so no sound layout claims one");
        } else if (row < equality_end) {
            push(equality, row - slack, col, partition);
        } else {
            push(inequality, row - slack, col, partition);
        }
    }

    ReferenceStream out;
    const auto append = [&out](const ReferenceStream &domain) {
        hven::solvers::ClaimBlock block{static_cast<int>(out.rows_.size()),
                                        static_cast<int>(domain.rows_.size())};
        out.rows_.insert(out.rows_.end(), domain.rows_.begin(), domain.rows_.end());
        out.cols_.insert(out.cols_.end(), domain.cols_.begin(), domain.cols_.end());
        out.partitions_.insert(out.partitions_.end(), domain.partitions_.begin(),
                               domain.partitions_.end());
        return block;
    };
    out.hessian_ = append(hessian);
    out.equality_jacobian_ = append(equality);
    out.inequality_jacobian_ = append(inequality);

    out.gradient_rows_.reserve(static_cast<std::size_t>(nlp.num_pgx_elems_));
    for (int slot = 0; slot < nlp.num_pgx_elems_; slot++) {
        out.gradient_rows_.push_back(nlp.rhs_coeff_rows_[nlp.pgx_data_start_ + slot]);
    }
    return out;
}

/// @brief The published partition-offset tables, expanded back to one partition
///        id per published slot -- the form the reference carries, and the form
///        a consumer would have to reconstruct.
inline std::vector<int> expand_partition_offsets(const hven::solvers::NonLinearProgram &nlp) {
    std::vector<int> per_slot(static_cast<std::size_t>(nlp.kkt_claim_rows().size()), -1);
    const auto expand = [&per_slot](const hven::solvers::ClaimBlock &block,
                                    Eigen::Ref<const Eigen::VectorXi> offsets) {
        for (int partition = 0; partition + 1 < offsets.size(); partition++) {
            for (int at = offsets[partition]; at < offsets[partition + 1]; at++) {
                per_slot[static_cast<std::size_t>(block.start_ + at)] = partition;
            }
        }
    };
    expand(nlp.hessian_claims(), nlp.hessian_claim_partition_offsets());
    expand(nlp.equality_jacobian_claims(), nlp.equality_jacobian_claim_partition_offsets());
    expand(nlp.inequality_jacobian_claims(), nlp.inequality_jacobian_claim_partition_offsets());
    return per_slot;
}

} // namespace hven::model_tests
