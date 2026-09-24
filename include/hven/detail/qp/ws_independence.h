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
// DEPENDENCY TEST (the landed contract, settler S23; it replaces R2). For a
// candidate row a, solve K_mu [u; v] = [a; 0], then at most kDepRefineSteps
// refinement steps toward the mu-free system (delta kept; a step is kept only
// if it lowers the residual). rho = ||(a - A_W^T v)_free|| / ||a||: scale-
// normalized, with NO row equilibration -- rho's independent bound is
// row-scale-free, and equilibration cannot be applied to the retained factor
// (it would only buy convergence speed). Verdicts, in order:
//   - DEPENDENT iff rho <= kDepTol or the integer screen me + |W| + 1 > n fires;
//   - a STALLED refinement (residual above target after the steps) with
//     rho < kDepUndecidable is UNDECIDABLE: the border path retreats to the
//     eliminated path, the eliminated path defers the add to the next pass's
//     detector (qp_engine.h, Degeneracy);
//   - a STALLED refinement with rho >= kDepUndecidable is ADMITTED as
//     independent AND COUNTED (DepTest::stall_admit, QpCounters::
//     dep_stall_admits): not "stall => dependent";
//   - otherwise (converged, rho > kDepTol) INDEPENDENT -- including
//     kDepTol < rho < kDepUndecidable.
// Pins take the residual at pinned columns as their coefficients.
//
// ENTRY DETECTOR. First structural: a member row whose nonzeros all sit at
// pinned columns is dependent on those pins (exact, no solve). Else [0; y] with A_W^T y = 0 is an
// exact eigenvector of K_mu with eigenvalue -mu. At most kEntryDetectSolves inverse-iteration
// solves from a fixed irrational constraint-space start; found iff ||A_W^T y_hat||_free <= kDepTol
// * the largest member norm (a proof, whatever the amplification). The support is every member
// whose scaled coefficient clears kDepCoeffTol of the largest; on the eliminated path a pin's
// coefficient is (A_W^T y_hat) at its column (the pins are eliminated, not in the factor), so a
// dependency an optional pin creates names that pin (E2 fix1, D2). A removable member is listed
// only when y_hat certifies it (rho_k <= kDepRemovalTol, E2 fix1b, a sufficient certificate; see
// kDepRemovalTol), else -- a heuristic fallback -- the uncertified member with the largest scaled
// coefficient alone. It can miss: -mu not separated
// from the next eigenvalue (large mu), or a border path whose C is swamped by an ill-conditioned
// K0. A miss on an over-counted set is never "independent"; the caller counts it as untested
// growth and trims by count.
//
// STRUCTURAL DEPENDENCY OF A CANDIDATE (E2 fix1, D3). A candidate row whose nonzeros all sit at
// pinned columns, or a candidate pin i that completes a member row (every nonzero of the row at a
// pinned column or at i), is dependent EXACTLY, with coefficients read off the row: rho = 0, no
// solve. It decides FIRST (R-A, E2 fix3): every growth site's candidate test (QpEngine::
// test_candidate: the add site, the repair pins, the ride add) replaces a numerical "independent"
// by it when it exists (dep_structural_first) -- the numerical test admitted exact structural
// dependencies at rho 3e-6..1e18 on DALLASM/STEENBRB (E2-D4 class F1) -- and the add site still
// uses it where the test is undecidable (dep_structural_exact).

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
// A detector support member is offered for removal only when the null vector certifies it:
// ||A_W^T y_hat|| <= kDepRemovalTol * |y_k| ||a_k|| (E2 fix1b). From A_W^T y_hat = r,
// dist(a_k, span{a_i, i != k}) <= ||r|| / |y_k|, an UPPER BOUND: the ratio is a SUFFICIENT
// numerical certificate of member k's dependence, not its distance. 1e-3 is a numerical-rank
// POLICY, not the candidate test's dependent/independent boundary (a converged candidate with
// kDepTol < rho < kDepUndecidable is independent there). With no removable member certified the
// detector falls back HEURISTICALLY to the uncertified member with the largest scaled coefficient
// (kept, settler S24: no fallback measured worse), counted QpCounters::dep_uncertified_fallbacks.
constexpr double kDepRemovalTol = kDepUndecidable;

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
    // The refinement stalled and rho >= kDepUndecidable: admitted as independent
    // (the caller counts it, QpCounters::dep_stall_admits).
    bool stall_admit = false;
    Index solves = 0;
    Vec v_e;   // per equality row
    Vec v_w;   // per ws.active_ineq() entry
    Vec v_pin; // size n; nonzero only at pinned columns

    bool dependent() const { return verdict != DepVerdict::kIndependent; }
};

/// `a` is the candidate in full-variable space (a bound is +-e_i).
DepTest dependency_test(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                        const Vec &a, const QpOptions &opts);

/// The same test with the refinement run to its end (until a step stops lowering the mu-free
/// residual, at most kDepRefineSteps): the Farkas certificate's multipliers, mu-free up to
/// rounding (settler S25'). Only the coefficients are meant to be read.
DepTest certificate_coefficients(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                                 const Vec &a, const QpOptions &opts);

struct EntryDetection {
    bool found = false;
    Index solves = 0;
    double null_residual = 0.0; // last ||A_W^T y_hat||_free / max member norm
    // Scaled coefficient >= kDepCoeffTol * max; removable members only when certified (below).
    std::vector<WsMember> support;
    // A removable member cleared kDepCoeffTol but not the removal certificate and was left out.
    bool uncertified_dropped = false;
    // No removable member was certified: the uncertified one with the largest scaled
    // coefficient alone is in the support (a heuristic, not a certificate).
    bool uncertified_fallback = false;
};

EntryDetection detect_dependency(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                                 const QpOptions &opts);

/// The exact structural dependency of a candidate (the row `idx` of Ai when `is_ineq`, else the
/// bound of column `idx`), in dependency_test's convention (v_e, v_w, v_pin; rho = 0,
/// kDependent). `found` is false when no member row makes it structurally dependent.
struct StructuralDependency {
    bool found = false;
    DepTest test;
};

StructuralDependency structural_dependency(const QpProblem &qp, const WorkingSet &ws, bool is_ineq,
                                           Index idx);

} // namespace hven::solvers::detail
