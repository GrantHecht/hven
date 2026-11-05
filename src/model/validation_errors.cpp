// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The cold throw paths of the model contract's inline argument checks.
//
// Each check in model/claim_space.h, structure_identity.h, candidate_point.h and
// nlp_assembly.h stays inline in its header; only the construction of the
// refusal message and the throw live here. None of these functions is reached on
// a valid call, so none sits on a hot path, and the headers no longer include
// the formatting library to carry them. The message text is exactly what the
// inline throws produced.

#include <cstdint>
#include <stdexcept>
#include <string>

#include <fmt/format.h>

#include "hven/model/candidate_point.h"
#include "hven/model/claim_space.h"
#include "hven/model/local_blocks.h"
#include "hven/model/nlp_assembly.h"
#include "hven/model/solver_function_base.h"
#include "hven/model/solver_indexing_data.h"
#include "hven/model/structure_identity.h"

namespace hven::detail {

// ---------------------------------------------------------------- claim_space.h

void throw_kkt_table_negative_size(int size, int clash_size) {
    throw std::invalid_argument(
        fmt::format("KktLocationTable: negative array size (locations {0}, clash marks "
                    "{1})",
                    size, clash_size));
}

void throw_kkt_table_no_locations(int size) {
    throw std::invalid_argument(fmt::format(
        "KktLocationTable: {0} claim slots were published with no location array", size));
}

void throw_kkt_table_no_clash_marks(int clash_size) {
    throw std::invalid_argument(fmt::format(
        "KktLocationTable: {0} columns were published with no clash-mark array", clash_size));
}

void throw_kkt_table_bad_clash_mark(int column, int mark) {
    throw std::invalid_argument(
        fmt::format("KktLocationTable: column {0} carries clash mark {1}; the only negative mark "
                    "is -1, meaning uncontested",
                    column, mark));
}

void throw_kkt_table_lock_index(int column, int mark, int lock_count) {
    throw std::invalid_argument(
        fmt::format("KktLocationTable: column {0} is marked contested with lock index {1}, but "
                    "only {2} locks were published",
                    column, mark, lock_count));
}

void throw_rhs_table_negative_size(int size) {
    throw std::invalid_argument(fmt::format("RhsLocationTable: negative array size ({0})", size));
}

void throw_rhs_table_no_rows(int size) {
    throw std::invalid_argument(
        fmt::format("RhsLocationTable: {0} claim slots were published with no row array", size));
}

void throw_rhs_table_bad_row(int slot, int row) {
    throw std::invalid_argument(
        fmt::format("RhsLocationTable: claim slot {0} maps to row {1}; the only "
                    "negative row is -1, meaning dropped",
                    slot, row));
}

// ---------------------------------------------------------- structure_identity.h

void throw_claim_stream_length_mismatch(Eigen::Index rows, Eigen::Index cols) {
    throw std::invalid_argument(
        fmt::format("claim_stream_digest: the claim stream has {0} rows and {1} columns; a "
                    "claim is a (row, column) pair, so the two arrays must be the same length",
                    rows, cols));
}

// ------------------------------------------------------------ candidate_point.h

void throw_candidate_primal_size(Eigen::Index size, Eigen::Index declared) {
    throw std::invalid_argument(
        fmt::format("CandidatePoint: the primal block has {0} rows, but the aggregate "
                    "declares {1} primal variables",
                    size, declared));
}

void throw_candidate_equality_multiplier_size(Eigen::Index size, Eigen::Index rows) {
    throw std::invalid_argument(
        fmt::format("CandidatePoint: the equality-multiplier block has {0} rows; it must be "
                    "either empty or {1} rows",
                    size, rows));
}

void throw_candidate_inequality_multiplier_size(Eigen::Index size, Eigen::Index rows) {
    throw std::invalid_argument(
        fmt::format("CandidatePoint: the inequality-multiplier block has {0} rows; it must be "
                    "either empty or {1} rows",
                    size, rows));
}

void throw_candidate_equality_residual_size(Eigen::Index size, Eigen::Index rows) {
    throw std::invalid_argument(
        fmt::format("CandidateValues: the equality-residual block has {0} rows, but the "
                    "aggregate declares {1} equality rows",
                    size, rows));
}

void throw_candidate_inequality_residual_size(Eigen::Index size, Eigen::Index rows) {
    throw std::invalid_argument(
        fmt::format("CandidateValues: the inequality-residual block has {0} rows, but the "
                    "aggregate declares {1} inequality rows",
                    size, rows));
}

void throw_candidate_objective_gradient_size(Eigen::Index size, Eigen::Index declared) {
    throw std::invalid_argument(
        fmt::format("CandidateFirstOrder: the objective-gradient block has {0} rows, but the "
                    "aggregate declares {1} primal variables",
                    size, declared));
}

void throw_candidate_adjoint_gradient_size(Eigen::Index size, Eigen::Index declared) {
    throw std::invalid_argument(fmt::format(
        "CandidateFirstOrder: the constraint-adjoint-gradient block has {0} rows, but the "
        "aggregate declares {1} primal variables",
        size, declared));
}

void throw_illegal_eval_request(std::uint32_t request) {
    throw std::invalid_argument(fmt::format(
        "EvalRequest: the flag combination 0x{0:x} is not one of the evaluation shapes this "
        "contract maps. The legal sets are kRequestObjectiveOnly (0x{1:x}), "
        "kRequestObjectiveAndConstraints (0x{2:x}), "
        "kRequestObjectiveGradientAndConstraints (0x{3:x}), kRequestFirstOrderRhs (0x{4:x}), "
        "kRequestConstraintResidualsAndJacobian (0x{5:x}), kRequestFirstOrderKkt (0x{6:x}), "
        "kRequestConstraintKkt (0x{7:x}), kRequestFullKkt (0x{8:x}), "
        "kRequestLagrangianHessian (0x{9:x}), kRequestGradientAndJacobians (0x{10:x}) and "
        "kRequestConstraintJacobiansOnly (0x{11:x}); see the mapping table in "
        "model/candidate_point.h",
        request, static_cast<std::uint32_t>(kRequestObjectiveOnly),
        static_cast<std::uint32_t>(kRequestObjectiveAndConstraints),
        static_cast<std::uint32_t>(kRequestObjectiveGradientAndConstraints),
        static_cast<std::uint32_t>(kRequestFirstOrderRhs),
        static_cast<std::uint32_t>(kRequestConstraintResidualsAndJacobian),
        static_cast<std::uint32_t>(kRequestFirstOrderKkt),
        static_cast<std::uint32_t>(kRequestConstraintKkt),
        static_cast<std::uint32_t>(kRequestFullKkt),
        static_cast<std::uint32_t>(kRequestLagrangianHessian),
        static_cast<std::uint32_t>(kRequestGradientAndJacobians),
        static_cast<std::uint32_t>(kRequestConstraintJacobiansOnly)));
}

void throw_candidate_equality_multipliers_absent(Eigen::Index size, Eigen::Index rows) {
    throw std::invalid_argument(fmt::format(
        "CandidatePoint: this evaluation reads the multipliers, so the equality-multiplier "
        "block must carry all {1} rows, but it has {0}",
        size, rows));
}

void throw_candidate_inequality_multipliers_absent(Eigen::Index size, Eigen::Index rows) {
    throw std::invalid_argument(fmt::format(
        "CandidatePoint: this evaluation reads the multipliers, so the inequality-multiplier "
        "block must carry all {1} rows, but it has {0}",
        size, rows));
}

// -------------------------------------------------------------- nlp_assembly.h

void throw_arena_view_missing(const char *arena, std::uint32_t flag, const char *missing) {
    throw std::invalid_argument(
        fmt::format("assemble: the request names the {0} (flag 0x{1:x}), but that arena's "
                    "scatter view supplies no {2}. A request may leave empty only the arenas "
                    "it does NOT name; see the mapping table in model/candidate_point.h",
                    arena, flag, missing));
}

void throw_residual_arena_view_missing(const char *arena, Eigen::Index declared_rows,
                                       const char *missing) {
    throw std::invalid_argument(fmt::format(
        "assemble: the request names the {0} arena, whose declared length is {1}, but the "
        "view supplies no {2}",
        arena, declared_rows, missing));
}

void throw_residual_arena_size(const char *arena, Eigen::Index view_rows,
                               Eigen::Index declared_rows) {
    throw std::invalid_argument(
        fmt::format("assemble: the {0} arena's view is {1} rows, but the aggregate declares "
                    "{2} {0} rows",
                    arena, view_rows, declared_rows));
}

void throw_objective_slot_missing(std::uint32_t flag) {
    throw std::invalid_argument(
        fmt::format("assemble: the request names the objective value (flag 0x{0:x}), but no "
                    "out slot was supplied for it",
                    flag));
}

void throw_kkt_view_empty(std::uint32_t flags) {
    throw std::invalid_argument(fmt::format(
        "assemble: the request names KKT-bearing output (flags 0x{0:x} of the request), but "
        "the KKT scatter view is empty",
        flags));
}

void throw_bound_destination_mismatch(const double *view_values, const double *bound) {
    throw std::invalid_argument(fmt::format(
        "assemble: the KKT scatter view names value array {0}, but this provider's location "
        "tables were bound at analysis time to {1}. The destination bound at analysis no "
        "longer matches the view; re-run the analysis against the destination you intend to "
        "fill. (A consumer matrix that was resized or reallocated moves its value array, so "
        "this is a stale location table being caught rather than a rejected argument.)",
        fmt::ptr(view_values), fmt::ptr(bound)));
}

// ------------------------------------------------------ solver_indexing_data.h

void throw_output_v_index_shape(Eigen::Index rows, Eigen::Index cols, Eigen::Index got_rows,
                                Eigen::Index got_cols) {
    throw std::invalid_argument(fmt::format("SolverIndexingData::set_output_v_index: expected a "
                                            "{}x{} map to match the input map, got {}x{}",
                                            rows, cols, got_rows, got_cols));
}

void throw_thread_split_count(int threads) {
    throw std::invalid_argument(
        fmt::format("thread_split: Threads must be positive, got {}", threads));
}

void throw_v_index_rows(int expected, Eigen::Index got) {
    throw std::invalid_argument(fmt::format(
        "SolverIndexingData::set_v_index: expected {} rows (input_size_), got {}", expected, got));
}

void throw_c_index_rows(int expected, Eigen::Index got) {
    throw std::invalid_argument(fmt::format(
        "SolverIndexingData::set_c_index: expected {} rows (output_size_), got {}", expected, got));
}

// ------------------------------------------------------- solver_function_base.h

void throw_thread_mode_laid(const std::string &piece, int mode) {
    throw std::invalid_argument(fmt::format(
        "set_thread_mode: the layout on hand was laid over '{0}' with thread mode {1}, and "
        "the mode decides which partition claims its KKT slots; re-lay from a declaration "
        "carrying the new mode instead of writing the laid piece",
        piece, mode));
}

// --------------------------------------------------------------- local_blocks.h

void throw_local_block_split_claim(const std::string &piece) {
    throw std::logic_error(fmt::format(
        "local constraint block '{0}': claimed for one of its Jacobian and Hessian domains "
        "without the other; a constraint piece's Hessian slots follow its Jacobian slots, so "
        "the layout claims the two together",
        piece));
}

} // namespace hven::detail
