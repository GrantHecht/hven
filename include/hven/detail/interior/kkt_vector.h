// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// The solver's compound KKT vectors are plain Eigen::VectorXd of length
// primal_vars + slack_vars + equal_cons + inequal_cons. KKTVector is the
// non-owning view giving those four blocks names, and ConstKKTVector is its
// read-only twin for storage the holder may not write. Deliberately a standalone
// header with no InteriorPointSolver dependency: the solver and every
// globalization component build views from the SAME two types, so ONE layout
// contract governs every compound KKT vector in the engine. The two views
// duplicate the short accessor expressions that spell that contract out -- see
// ConstKKTVector's own note for why it is a second class and not a template
// instantiation -- and tests/interior/test_structure_epoch_gating.cpp pins the
// duplication accessor for accessor, since nothing else holds them in step.
// Each component supplies its own kkt_view() factory, since dimensions come
// from different places.
//
// Not every multiplier lives here: the native bound multipliers (z_L, z_U) are
// held separately in the solver-owned BoundDualState (bound_set.h) because the
// bound rows are condensed into the primal diagonal rather than enlarging the
// KKT system — which is precisely why this layout is unchanged by that feature.
// A reader looking for "all the duals" needs both.

#pragma once

#include <cassert>
#include <utility>

#include <Eigen/Core>

namespace hven::solvers {

/// @brief Lightweight non-owning view over the compound KKT layout
/// [primals | slacks | eq_lmults | iq_lmults], used both as the iterate
/// vector (x, s, lambda_e, lambda_i) and as the RHS/gradient vector (grad_x,
/// grad_s, c_eq, c_iq); the two accessor groups name each interpretation.
///
/// const overloads use std::as_const(data_) to force Eigen's segment accessors
/// to return immutable expressions. Lifetime: must not outlive the referenced
/// VectorXd.
class KKTVector {
  public:
    KKTVector(Eigen::VectorXd &data, int pv, int sv, int ec, int ic)
        : data_(data), pv_(pv), sv_(sv), ec_(ec), ic_(ic) {
        assert(pv >= 0 && sv >= 0 && ec >= 0 && ic >= 0);
        assert(data.size() >= pv + sv + ec + ic);
    }

    // --- Primal/slack segments ---
    auto primals() { return data_.head(pv_); }
    auto primals() const { return std::as_const(data_).head(pv_); }
    auto slacks() { return data_.segment(pv_, sv_); }
    auto slacks() const { return std::as_const(data_).segment(pv_, sv_); }
    auto primals_slacks() { return data_.head(pv_ + sv_); }
    auto primals_slacks() const { return std::as_const(data_).head(pv_ + sv_); }

    // --- Multiplier segments ---
    auto eq_lmults() { return data_.segment(pv_ + sv_, ec_); }
    auto eq_lmults() const { return std::as_const(data_).segment(pv_ + sv_, ec_); }
    auto iq_lmults() { return data_.tail(ic_); }
    auto iq_lmults() const { return std::as_const(data_).tail(ic_); }
    auto lmults() { return data_.tail(ec_ + ic_); }
    auto lmults() const { return std::as_const(data_).tail(ec_ + ic_); }

    // --- Gradient/constraint segments (intentional aliases) ---
    // Same memory layout as the primal/multiplier accessors above, but with
    // names matching the RHS/gradient interpretation: the primal block holds
    // the objective gradient, the slack block holds the dual gradient, and
    // the multiplier blocks hold constraint values.
    // These are intentional aliases: prim_grad() == primals(),
    // dual_grad() == slacks(), eq_cons() == eq_lmults(), iq_cons() == iq_lmults().
    auto prim_grad() { return data_.head(pv_); }
    auto prim_grad() const { return std::as_const(data_).head(pv_); }
    auto dual_grad() { return data_.segment(pv_, sv_); }
    auto dual_grad() const { return std::as_const(data_).segment(pv_, sv_); }
    auto prim_dual_grad() { return data_.head(pv_ + sv_); }
    auto prim_dual_grad() const { return std::as_const(data_).head(pv_ + sv_); }
    auto eq_cons() { return data_.segment(pv_ + sv_, ec_); }
    auto eq_cons() const { return std::as_const(data_).segment(pv_ + sv_, ec_); }
    auto iq_cons() { return data_.tail(ic_); }
    auto iq_cons() const { return std::as_const(data_).tail(ic_); }
    auto all_cons() { return data_.tail(ec_ + ic_); }
    auto all_cons() const { return std::as_const(data_).tail(ec_ + ic_); }

    // --- Full vector access ---
    Eigen::VectorXd &data() { return data_; }
    const Eigen::VectorXd &data() const { return data_; }

  private:
    // The read-only twin below is built from these directly: it is the same
    // layout over the same storage, and re-deriving the widths from segment
    // sizes would be the same four numbers taken the long way round.
    friend class ConstKKTVector;

    Eigen::VectorXd &data_;
    int pv_, sv_, ec_, ic_;
};

/// @brief The READ-ONLY twin of KKTVector: the same four-block layout over
/// storage the holder may not write.
///
/// It exists because a function that only READS a compound vector should be
/// able to say so in its signature. KKTVector's constructor takes
/// `Eigen::VectorXd &`, so before this class a const `Eigen::VectorXd` could
/// not be viewed at all without a const_cast or a copy -- which is why
/// InteriorPointSolver::enter_feasibility_restoration took its RHS by mutable
/// reference while never writing it (M6 W5 T2).
///
/// DELIBERATELY NOT A TEMPLATE, and this is the whole design decision. The
/// natural C++ shape is one `BasicKKTVector<Storage>` with `KKTVector` and
/// `ConstKKTVector` as two alias instantiations. That shape is correct and it
/// was rejected: `KKTVector` names a type in 28 signatures across 11 files,
/// including the virtual interfaces of every globalization component, and an
/// alias does not preserve a mangled name -- `10KKTVector` would become the
/// template's spelling in every one of them, renaming those functions, the
/// vtables that reference them and every caller's relocation. Two small
/// non-template classes cost one duplicated set of segment expressions in one
/// header and leave every existing symbol exactly where it is.
///
/// Converts implicitly from KKTVector, so a caller holding a mutable view can
/// pass it to a read-only parameter without naming this type. Lifetime: must
/// not outlive the referenced VectorXd (the KKTVector conversion binds to that
/// vector, not to the KKTVector).
class ConstKKTVector {
  public:
    ConstKKTVector(const Eigen::VectorXd &data, int pv, int sv, int ec, int ic)
        : data_(data), pv_(pv), sv_(sv), ec_(ec), ic_(ic) {
        assert(pv >= 0 && sv >= 0 && ec >= 0 && ic >= 0);
        assert(data.size() >= pv + sv + ec + ic);
    }

    /// @brief Implicit read-only view of an existing mutable view, over the
    ///        SAME storage and with the same block widths.
    ConstKKTVector(const KKTVector &v) : ConstKKTVector(v.data_, v.pv_, v.sv_, v.ec_, v.ic_) {}

    // --- Primal/slack segments ---
    auto primals() const { return data_.head(pv_); }
    auto slacks() const { return data_.segment(pv_, sv_); }
    auto primals_slacks() const { return data_.head(pv_ + sv_); }

    // --- Multiplier segments ---
    auto eq_lmults() const { return data_.segment(pv_ + sv_, ec_); }
    auto iq_lmults() const { return data_.tail(ic_); }
    auto lmults() const { return data_.tail(ec_ + ic_); }

    // --- Gradient/constraint segments (the same intentional aliases) ---
    auto prim_grad() const { return data_.head(pv_); }
    auto dual_grad() const { return data_.segment(pv_, sv_); }
    auto prim_dual_grad() const { return data_.head(pv_ + sv_); }
    auto eq_cons() const { return data_.segment(pv_ + sv_, ec_); }
    auto iq_cons() const { return data_.tail(ic_); }
    auto all_cons() const { return data_.tail(ec_ + ic_); }

    // --- Full vector access ---
    const Eigen::VectorXd &data() const { return data_; }

  private:
    const Eigen::VectorXd &data_;
    int pv_, sv_, ec_, ic_;
};

} // namespace hven::solvers
