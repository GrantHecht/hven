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
//   - WITH NO USABLE FACTOR the test does not run at all, and an un-run test is
//     not a verdict of dependence (E2 fix7, settler E4-37(b)): the integer
//     screen is the only statement available and it is exact, so a POSITIVE
//     screen is kDependent and a NEGATIVE one is kIndependent -- the pre-E2
//     admission (settler S17), DepTest::unrun true, rho left at its NaN
//     default and solves at 0 so nothing reads a measurement that was never
//     taken. It is counted QpCounters::dep_unrun_tests, and R-A (the exact
//     structural dependency, which needs no factor) still overrules it at
//     every growth site. Before fix7 this case returned kUndecidable, which
//     DepTest::dependent() reads as dependent: a blocker was then charged the
//     deferred-add path (qp_engine.h, Degeneracy) on the strength of a rho
//     that had never been computed;
//   - a STALLED refinement with rho >= kDepUndecidable is ADMITTED as
//     independent AND COUNTED (DepTest::stall_admit, QpCounters::
//     dep_stall_admits): not "stall => dependent";
//   - otherwise (converged, rho > kDepTol) INDEPENDENT -- including
//     kDepTol < rho < kDepUndecidable, where the walk's caller re-reads it
//     (R-E, E2 fix6: QpEngine::test_candidate runs certificate_coefficients and
//     certifies a dependent when rho' + its rounding floor <= kDepTol; see
//     kDepRereadKappa).
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
#include <functional>
#include <limits>
#include <utility>
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
// THE GREY RE-READ'S ROUNDING FLOOR (R-E, E2 fix6; S25''s exact_tol style). The re-read rho' is
// computed as ||fl(a - A_W^T v)_free|| / ||a||. Standard model, fl(a op b) = (a op b)(1 + d), |d|
// <= eps: each entry of a - A_W^T v sums at most m + 1 products, so its computed error is <= (m +
// 1) eps (1 + ||v||_1) max_k ||a_k|| (a's own unit coefficient in the 1, max_k over a and the rows
// with v_k != 0, m = me + |W|; pins absorb the pinned columns exactly and add nothing); v is the
// mu-free refined solution of the (n + m)-dimensional system, whose backward error is O((n + m)
// eps) relative to the same product. The floor is
//   kDepRereadKappa * (n + m) * eps * (1 + ||v||_1) * max_k ||a_k|| / ||a||,
// kappa = 10 the safety factor (kCertExactKappa's). A re-read turns dependent only when
// rho' + floor <= kDepTol: then the EXACT rho(v) is <= kDepTol, and so is the true relative
// distance (the theorem at QpEngine::test_candidate) -- the flip is certified at the contract's own
// tolerance, not at kDepTol plus rounding.
constexpr double kDepRereadKappa = 10.0;

/// THE WORKING ROWS OF A ROW-MAJOR A, GATHERED ONCE (M7 W2 C13). The dependency
/// test and the Farkas verification form A_i u and A_i^T lam with lam supported on
/// the working rows; over the full matrix each costs O(nnz(A_i)) whatever |W| is,
/// which on a problem with mi >> n (TAX: mi = 32 221 / 570 781, n = 360 / 1 512) is
/// the walk's dominant cost at a degenerate vertex, where every passed-over blocker
/// pays several of them. Here each costs O(nnz(A_W)).
///
/// SAME VALUES. `times` runs Eigen's own row-major kernel on the gathered rows, the
/// computation the full product does for each of them (each row is reduced
/// independently). `add_t` runs Eigen's own transposed kernel over the same rows in
/// the same ascending order; the full product differs only by the rows it skips,
/// whose lam is zero, so their contributions are +-0: they can change nothing but
/// the sign of an exact zero, which the build's -ffast-math (-fno-signed-zeros)
/// already declares insignificant. Measured: every TRAIN cell field-identical.
class RowSubset {
  public:
    /// @param rows ascending row indices of A.
    RowSubset(const SpMatRM &A, std::vector<Index> rows) : rows_(std::move(rows)) {
        std::vector<int> outer;
        std::vector<int> inner;
        std::vector<double> vals;
        outer.reserve(rows_.size() + 1);
        outer.push_back(0);
        for (const Index r : rows_) {
            for (SpMatRM::InnerIterator it(A, r); it; ++it) {
                inner.push_back(static_cast<int>(it.col()));
                vals.push_back(it.value());
            }
            outer.push_back(static_cast<int>(inner.size()));
        }
        sub_ = Eigen::Map<const SpMatRM>(static_cast<Index>(rows_.size()), A.cols(),
                                         static_cast<Index>(inner.size()), outer.data(),
                                         inner.data(), vals.data());
    }
    const std::vector<Index> &rows() const { return rows_; }
    /// Entry k = (A u)(rows[k]).
    Vec times(const Vec &u) const { return Vec(sub_ * u); }
    /// out += A^T lam (or -=), lam(rows[k]) = vals(k), zero elsewhere.
    void add_t(Vec &out, const Vec &vals, bool subtract) const {
        if (subtract) {
            out -= sub_.transpose() * vals;
        } else {
            out += sub_.transpose() * vals;
        }
    }

  private:
    std::vector<Index> rows_;
    SpMatRM sub_;
};

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
    // No usable factor: nothing was computed and the verdict is the integer
    // screen's alone (E2 fix7; the caller counts it, QpCounters::
    // dep_unrun_tests). `rho` is then the NaN default and `solves` is 0.
    bool unrun = false;
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

/// R-E's rounding floor (kDepRereadKappa): the bound on the rounding carried by a computed rho for
/// the coefficients `t` holds (t.v_e, t.v_w) of the candidate `a`.
double dependency_rounding_floor(const QpProblem &qp, const WorkingSet &ws, const DepTest &t,
                                 const Vec &a);

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

/// The face's inverse reduced Hessian applied to `u` (full-variable space): the variable part of
/// K^-1 [u_free; 0] on `f`, refined (at most kDepRefineSteps steps, each kept only if it lowers
/// the residual) against the factored system with dual_mu backed out, zero at pinned columns.
/// With A_W exact this is Z (Z'(H + delta I) Z)^-1 Z' u. `solves` counts the triangular solves;
/// an unusable factor or a face with no free column returns an empty vector.
Vec face_solve(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f, const Vec &u,
               const QpOptions &opts, Index &solves);

/// THE TERMINAL NULL DIRECTION (M7 W2 SDL-6): inverse iteration on `f`, in correction form, from
/// the working rows' residual r at `x` (Ae x - be; Ai x - bi on W; 0 on pins). [0; y] with
/// A_W^T y = 0 on the free columns is K_mu's eigenvector at -mu, so each step keeps r's component
/// there and contracts the rest by mu / (lambda + mu): the Farkas direction of an inconsistent
/// working set. A step is y <- y - v, K [u; v] = [A_W^T y; 0] (A_W^T y formed from A): its error
/// scales with the correction, so A_W^T y reaches rounding, which an unbounded column needs. Each
/// iterate (r itself first), unit 2-norm and oriented y'r >= 0, is offered to `accept` as
/// (y_e, y_i) (y_i size mi, zero off W, unclamped); the steps stop when the free-column null
/// residual fails to contract by kTerminalNullContraction, at most kTerminalNullSolves solves
/// (counted in `solves`). A candidate generator only -- the caller verifies. Returns whether
/// `accept` returned true; false with no usable factor.
constexpr Index kTerminalNullSolves = 32;
constexpr double kTerminalNullContraction = 0.9;
bool terminal_null_direction(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                             const Vec &x, const QpOptions &opts,
                             const std::function<bool(const Vec &, const Vec &)> &accept,
                             Index &solves);

/// Lanczos budget of face_negative_curvature (steps, one face_solve each).
constexpr Index kSaddleLanczosSteps = 24;

/// THE SADDLE EXIT's direction (qp_engine.h 4c): Lanczos with full reorthogonalisation on
/// face_solve's operator, from a fixed start, at most kSaddleLanczosSteps steps. Of the Ritz
/// vectors of its NEGATIVE Ritz values, the one with the most negative Rayleigh quotient on H
/// itself is returned (unit 2-norm) when it is VERIFIED: d'Hd <= -curvature_tol * d'd. Empty when
/// none is. Nothing here is a statement about H beyond that one verified quotient: an empty
/// answer is "no direction found", never "the face is convex". The caller also checks A_W d ~ 0.
Vec face_negative_curvature(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                            const QpOptions &opts, double curvature_tol, Index &solves);

// THE DEAD-END RANK REPAIR (M7 W2 RDV; qp_engine.h, Degeneracy). The working set's rows -- the
// equality rows and the working inequality rows, each divided by its 2-norm -- restricted to the
// free columns, M_F, and their numerical LEFT NULL SPACE {y : ||M_F^T y|| <= kRankNullTol *
// ||M_F||}, read WITHOUT the walk's regularized KKT factor (whose delta- and mu-sized pivots are
// what let the deficiency in: an independence test that stalls or a coefficient polluted by
// residual / sigma_min). A null vector y of the rows extends to a dependency of the whole set,
// pins included: the pin at column i carries -(M^T y)_i. Its CONSISTENCY is gamma = y' r, r the
// members' right-hand sides (rows' b / ||a||, a pin's bound): gamma != 0 means NO point meets every
// member -- the face the walk is heading for does not exist, and a dead end follows.
//
// LINEAR ALGEBRA (production through hven::linear). G = M_F M_F^T + t I, t = kRankGramShift *
// ||M_F M_F^T||_inf, is symmetric positive definite with every pivot >= t, so the backend factors
// it with the KKT factors' own configuration (sqp_kkt_options) and no pivot is perturbed (a
// perturbed one refuses -- on MKL; Accelerate reports no perturbed-pivot count (UNOBSERVED,
// absent), so there the refusal does not fire and the SPD shift alone carries the guarantee). Block
// inverse iteration on G (kRankIterations steps, deterministic start) from a block of
// kRankBlockStart columns, doubled until the block's largest Ritz value clears kRankBlockEdge * t
// (the block then holds every direction G's shift cannot separate), and the Ritz pairs read from
// the b x b B'B, B = M_F^T X formed from M_F itself (not from the shifted factor): a singular-value
// resolution of about sqrt(eps) ||M_F||, beneath kRankNullTol * ||M_F||. A null direction is one
// whose Ritz singular value is <= kRankNullTol * sqrt(||G||_inf) (an upper bound on ||M_F||_2).
// Dense Eigen acts only on the thin blocks -- except that a set of at most kRankDenseRows rows
// takes G's own dense eigenpairs instead (the same squared resolution, one O(rows^3) eigensolve;
// MEASURED: SAWPATH's graded equality block fills the block to every row, where the iteration's
// multi-right-hand-side solves cost 2..7 s a repair). A block that would exceed kRankBlockCap
// entries refuses. MEASURED (RDV battery, dense-SVD scratch twin): every outcome is the same for
// any null tolerance in [1e-12, 3e-7]; the SHIPPED Gram path resolves singular values only down to
// about sqrt(eps) ||M_F||, so its window is about [3e-8, 3e-7] *
// ||M_F|| (the twin's lower end is not reachable); at 1e-6 the SAWPATH equality block's
// graded spectrum (1e-11 .. 1e-5) reads as inconsistent and its dead end stands.
//
// THE RELEASE SET. With Z the null space extended to every member (rows, then pins), a member is
// RELEASABLE when it is a working inequality row that no shift drives or a pin that is not kFixed;
// its orientation is +1 for a row (a'x <= b) and an upper pin, -1 for a lower pin. Repeated until Z
// is empty: c = Z' r; gamma = ||c||. If gamma > kRankConsistencyFactor * feas_tol * max(1,
// sum_i |y*_i||r_i|) (y* = Z c / gamma, so the scale reads only the members that participate in
// the direction; the factor is the classifier's kInfeasibilityMarginFactor) the face is
// INCONSISTENT along y* = Z c / gamma, and the released member j maximizes orient_j * y*_j >
// kRankReleaseTol: on the face without j, c_j' x = d_j - gamma / y*_j, strictly inside its own
// constraint (the released member does not block the step onto the new face). Otherwise the
// direction is consistent, the member with the largest participation ||Z_j|| is released, and the
// face does not change (the member is implied by the others on it). Z is then restricted to y_j =
// 0. A consistent remainder carried by MANDATORY members only (the equality rows' own dependence,
// which the dual regularization absorbs) ends the set; an inconsistent direction with no releasable
// member of the right orientation is STUCK (the set is discarded: nothing is released). Ties: the
// first member in (rows ascending, pins ascending) order.
constexpr double kRankNullTol = 1e-7;
constexpr double kRankGramShift = 1e-6;
constexpr double kRankBlockEdge = 16.0;
constexpr Index kRankBlockStart = 8;
constexpr int kRankIterations = 6;
constexpr double kRankBlockCap = 2.5e7;
constexpr Index kRankDenseRows = 1000;
constexpr double kRankReleaseTol = 1e-8;
constexpr double kRankConsistencyFactor = 10.0;

struct RankRepair {
    enum class Outcome : std::uint8_t {
        kFullRank, // no optional dependency (none, or the mandatory rows' own consistent one)
        kRelease,  // `release` restores full rank
        kStuck,    // an inconsistent direction with no releasable member of the right orientation
        kRefused,  // the linear algebra declined (backend error, perturbed pivot, block cap)
    };
    Outcome outcome = Outcome::kFullRank;
    std::vector<WsMember> release; // kRow / kPin members, in the order chosen
    // Parallel to `release`: 1 when the member was released on a CONSISTENT direction (implied by
    // the others on its face), 0 on an inconsistent one.
    std::vector<char> release_consistent;
    Index deficiency = 0;         // null directions found (mandatory ones included)
    Index inconsistent_steps = 0; // releases chosen by the inconsistency's orientation
};

/// The dead-end rank repair's analysis of `ws` (above); reads `opts.feas_tol` only. `threads` is
/// the owning engine's (sqp_kkt_options). `block_cap` is the block-entry cap above which the
/// analysis refuses (production: kRankBlockCap; a parameter so a test can reach the refusal
/// without a multi-gigabyte block). Writes nothing.
RankRepair rank_repair(const QpProblem &qp, const WorkingSet &ws, const Vec &shift,
                       const QpOptions &opts, int threads, double block_cap = kRankBlockCap);

} // namespace hven::solvers::detail
