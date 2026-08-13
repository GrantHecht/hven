#pragma once

// working_set.h — the QP engine's mutable "current guess" of which
// inequality rows are active (in the working set) and which variables are
// pinned at a bound (kAtLower/kAtUpper/kFixed) versus free. Tasks 7-9
// (active-set updates, Schur-complement warm starts, the outer QP loop)
// mutate this via bound_state()/add_ineq()/drop_ineq() as the algorithm
// walks between active sets; assemble_kkt() (kkt_assembly.h) reads it to
// build the reduced KKT system for the CURRENT working set.

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

#include <tycho_sqp/types.h>

namespace tycho::sqp {

class WorkingSet {
  public:
    WorkingSet(Index n, Index mi)
        : n_(n), mi_(mi), bound_state_(static_cast<std::size_t>(n), BoundState::kFree) {}

    std::vector<BoundState> &bound_state() { return bound_state_; }
    const std::vector<BoundState> &bound_state() const { return bound_state_; }

    // Sorted, unique row indices into Ai currently in the working set.
    const std::vector<Index> &active_ineq() const { return active_ineq_; }

    // Throws std::invalid_argument if `row` is out of range or already in
    // the working set.
    void add_ineq(Index row) {
        check_row(row, "add_ineq");
        auto it = std::lower_bound(active_ineq_.begin(), active_ineq_.end(), row);
        if (it != active_ineq_.end() && *it == row) {
            throw std::invalid_argument(
                fmt::format("WorkingSet::add_ineq: row {} is already in the working set", row));
        }
        active_ineq_.insert(it, row);
    }

    // Throws std::invalid_argument if `row` is out of range or not currently
    // in the working set.
    void drop_ineq(Index row) {
        check_row(row, "drop_ineq");
        auto it = std::lower_bound(active_ineq_.begin(), active_ineq_.end(), row);
        if (it == active_ineq_.end() || *it != row) {
            throw std::invalid_argument(
                fmt::format("WorkingSet::drop_ineq: row {} is not in the working set", row));
        }
        active_ineq_.erase(it);
    }

    // Number of variables currently marked BoundState::kFree.
    Index num_free() const {
        return static_cast<Index>(
            std::count(bound_state_.begin(), bound_state_.end(), BoundState::kFree));
    }

    Index n() const { return n_; }
    Index mi() const { return mi_; }

  private:
    void check_row(Index row, const char *who) const {
        if (row < 0 || row >= mi_) {
            throw std::invalid_argument(
                fmt::format("WorkingSet::{}: row {} out of range [0, {})", who, row, mi_));
        }
    }

    Index n_;
    Index mi_;
    std::vector<BoundState> bound_state_;
    std::vector<Index> active_ineq_; // sorted, unique
};

} // namespace tycho::sqp
