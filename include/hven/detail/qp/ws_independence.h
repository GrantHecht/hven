// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The QP walk's working-set independence test and entry detector.
///
/// Both read the regularized KKT factor the pass already holds, through
/// `hven::linear`, and write nothing: no factorization, no ledger or border
/// change. Gill & Wong (2015) §3, Result 3.3 is the dependency statement.
//
// DEPENDENCY TEST. For a candidate row a, solve K_mu [u; v] = [a; 0], then at
// most kDepRefineSteps refinement steps toward the mu-free system (delta kept;
// a step is kept only if it lowers the residual). rho = ||(a - A_W^T v)_free||
// / ||a||. Dependent iff rho <= kDepTol or the integer screen
// me + |W| + 1 > n fires; an unconverged refinement with rho < kDepUndecidable
// is UNDECIDABLE, which callers treat as dependent. Pins take the residual at
// pinned columns as their coefficients.
//
// ENTRY DETECTOR. First structural: a member row whose nonzeros all sit at
// pinned columns is dependent on those pins (exact, no solve). Else [0; y] with A_W^T y = 0 is an
// exact eigenvector of K_mu with eigenvalue -mu. At most kEntryDetectSolves inverse-iteration
// solves from a fixed irrational constraint-space start; found iff ||A_W^T y_hat||_free <= kDepTol
// * the largest member norm (a proof, whatever the amplification). The support is every member
// whose scaled coefficient clears kDepCoeffTol of the largest. It can miss: -mu not separated from
// the next eigenvalue (large mu), or a border path whose C is swamped by an ill-conditioned K0. A
// miss on an over-counted set is never "independent"; the caller counts it as untested growth and
// trims by count.

#include <cstdint>
#include <limits>
#include <vector>

#include <hven/detail/kkt/border_ops.h>
#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/kkt/schur_complement.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/qp/working_set.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers::detail {

constexpr double kDepTol = 1e-6;
constexpr double kDepUndecidable = 1e-3;
constexpr Index kDepRefineSteps = 3;
constexpr double kDepCoeffTol = 1e-6;
constexpr Index kEntryDetectSolves = 4;

/// The factor a pass holds for its CURRENT working set; read-only. Exactly one
/// of the two paths is set, or neither (no usable factor).
struct DepFactorView {
    const KktFactor *eliminated = nullptr;
    const KktAssembly *k0 = nullptr;
    const std::vector<Index> *k0_rows = nullptr;
    const std::vector<BorderLedgerEntry> *ledger = nullptr;
    const SchurComplement *schur = nullptr;

    bool usable() const { return eliminated != nullptr || schur != nullptr; }
};

enum class DepVerdict : std::uint8_t { kIndependent, kDependent, kUndecidable };

struct WsMember {
    enum class Kind : std::uint8_t { kEq, kRow, kPin };
    Kind kind = Kind::kEq;
    Index idx = -1;
};

struct DepTest {
    DepVerdict verdict = DepVerdict::kUndecidable;
    double rho = std::numeric_limits<double>::quiet_NaN();
    bool screen = false;
    Index solves = 0;
    Vec v_e;   // per equality row
    Vec v_w;   // per ws.active_ineq() entry
    Vec v_pin; // size n; nonzero only at pinned columns

    bool dependent() const { return verdict != DepVerdict::kIndependent; }
};

/// `a` is the candidate in full-variable space (a bound is +-e_i).
DepTest dependency_test(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                        const Vec &a, const QpOptions &opts);

struct EntryDetection {
    bool found = false;
    Index solves = 0;
    double null_residual = 0.0;    // last ||A_W^T y_hat||_free / max member norm
    std::vector<WsMember> support; // scaled coefficient >= kDepCoeffTol * max
};

EntryDetection detect_dependency(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                                 const QpOptions &opts);

} // namespace hven::solvers::detail
