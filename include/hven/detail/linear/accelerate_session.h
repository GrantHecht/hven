#pragma once

// =============================================================================
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// This file (and its .cpp) is DERIVED from the AccelerateSupport module of
// the Eigen linear algebra library (MPL-2.0) -- specifically its Accelerate
// Sparse call discipline: SparseMatrixStructure/SparseSymbolicFactorOptions
// setup for the symbolic phase, SparseNumericFactorOptions for the numeric
// phase, and the CSR-upper-as-CSC-lower interpretation of a symmetric
// matrix's stored entries. That module reached hven by way of two
// downstream, Mac-hardware-verified ports: the interior-point engine's own
// Eigen-derived Accelerate interface
// (psiopt/include/tycho/detail/solvers/linear/accelerate_interface.h +
// accelerate_utils.h) and the SQP engine's audited KktSystem port
// (tycho_sqp/include/tycho_sqp/kkt_system_accelerate.h) -- this file follows
// the SECOND of those two most closely: its simpler, implicit-workspace
// SparseFactor/SparseSolve call shapes (no caller-managed aligned buffers)
// are the ones that have actually run against real Accelerate on real Mac
// hardware, per tycho_sqp/docs/notes/2026-07-29-accelerate-audit-results.md.
// This file's own compilation and execution on Apple hardware is UNOBSERVED
// -- see docs/testing.md and the Task-6 report for exactly what is and is
// not verified here.
//
// MPL-2.0 applies to THIS FILE only, as the license permits; the remainder of
// hven is Apache-2.0. See notices/eigen-mpl2.txt.
//
// What is NOT derived: the session lifecycle, ownership and epoch semantics,
// evidence reporting, the pivot_perturb_exp -> zeroTolerance mapping, and the
// partial-solve subfactor discipline around it are hven's own and exist to
// serve the frozen interface contract of hven/linear/symmetric_factor.h and
// its per-backend semantics table.
// =============================================================================

#include <cstdint>
#include <vector>

#include <Accelerate/Accelerate.h>

#include "hven/core/types.h"

namespace hven::linear::detail {

// The three partial-solve stages Accelerate can genuinely provide via
// SparseCreateSubfactor, in the order SymmetricFactor::SolvePhase names them
// (kForward, kDiagonal, kBackward). The adapter
// (symmetric_factor_accelerate.cpp's accelerate_phase_of) maps SolvePhase to
// this enum via an explicit, exhaustive switch -- not a bare cast -- so a
// future reordering of either enum fails to COMPILE there (a missing switch
// case) rather than silently swapping which subfactor a phase solves
// against. That switch is the enforcement point; no static_assert is needed
// or present.
enum class SubfactorPhase : int { kForward = 0, kDiagonal = 1, kBackward = 2 };

// The backend knobs one session is created with. Fixed for the session's
// lifetime, mirroring PardisoConfig's role for the MKL session -- baked into
// the factorization it produces, so an engine that adopts a session inherits
// them rather than reinterpreting them.
//
// num_threads and max_refinement_iters are stored here (so SymmetricFactor::
// adopt() can round-trip Options faithfully) but NOT applied to any backend
// call: Accelerate exposes no per-instance thread control -- the contract
// records that control as best-effort-absent on this backend rather than
// fabricating one -- and no
// native iterative-refinement counter this session can honestly report (a
// hand-rolled refinement loop, as tycho's AccelerateImpl implements via vDSP,
// is deliberately NOT ported here -- see the Task-6 report for why: an
// unverified numerical loop with zero ability to check it on this Linux-only
// development pass is a correctness risk this task declines to take on
// speculatively).
struct AccelerateConfig {
    int num_threads = 0;          // stored only; no per-instance backend control exists
    int pivot_perturb_exp = 8;    // -> zeroTolerance, see FactorSession::factorize
    int max_refinement_iters = 0; // stored only; Accelerate reports no refinement count
};

// One Accelerate factorization session: the symbolic/numeric factorization
// pair, the CSR-as-CSC-lower matrix view, and the column-starts buffer
// Accelerate's SparseMatrixStructure needs in `long` width.
//
// The session is the unit of SHARING -- see the identical note on the MKL
// FactorSession (hven/detail/linear/pardiso_session.h) for the full
// rationale; it applies verbatim here. Both backends define a class with
// this exact name because hven/linear/symmetric_factor.h forward-declares
// and holds it by that name; only one of the two definitions is ever
// compiled into a given build (src/CMakeLists.txt selects by platform).
//
// Errors are surfaced the same two ways as the MKL session: analyze() throws
// (nowhere to report a numeric outcome), factorize() RETURNS a nonzero status
// for the caller to fold into FactorizeOutcome.
//
// Not internally synchronized -- see the thread-safety note on the public
// class.
class FactorSession {
  public:
    FactorSession(const AccelerateConfig &cfg, std::uint64_t initial_epoch);
    ~FactorSession();

    FactorSession(const FactorSession &) = delete;
    FactorSession &operator=(const FactorSession &) = delete;

    // Accelerate symbolic factorization (SparseFactor with no matrix data,
    // LDLTTPP -- see the class-level notes on why TPP specifically). Throws
    // std::runtime_error carrying Accelerate's status code on failure.
    void analyze(const SpMatRM &A);

    // Numeric factorization against the existing symbolic. RETURNS
    // Accelerate's status code (0 on success): a numeric failure is the
    // caller's outcome to report, exactly like the MKL twin. On success the
    // numeric epoch advances and inertia becomes queryable via
    // native_factorization(); on failure the session is left with no usable
    // numerics.
    //
    // Requires A to match the analyzed matrix's dimensions and stored-entry
    // count. Throws std::invalid_argument if it does not.
    //
    // UNLIKE Pardiso, Accelerate is documented (and was measured on real
    // hardware by the tycho_sqp audit) to genuinely REFUSE a numeric
    // factorization on singular/indefinite-beyond-repair input
    // (SparseMatrixIsSingular / SparseFactorizationFailed) rather than
    // perturbing through it -- so this path, unlike the MKL twin's, is not
    // expected to be unreachable by ordinary fixtures. It remains untested
    // here regardless: this is a Linux-only development pass with no
    // Accelerate runtime to provoke it against. See docs/testing.md for the
    // seam that covers the reachable slice of this backend's fault paths.
    int factorize(const SpMatRM &A);

    // Full solve, one right-hand side at a time via Accelerate's
    // DenseVector_Double SparseSolve overload (the exact call shape
    // KktSystem::solve uses on real hardware). `nrhs` columns are solved in
    // a loop rather than through a batched DenseMatrix_Double call:
    // deliberately conservative, see the Task-6 report for why. `b` and `x`
    // are contiguous column-major buffers of dim() * nrhs doubles and must
    // not alias. Throws std::runtime_error on any Accelerate error.
    void solve(const double *b, double *x, Index nrhs) const;

    // Partial solve via SparseCreateSubfactor: forward = SubfactorL,
    // diagonal = SubfactorD, backward = SubfactorL with the transpose
    // attribute set -- exactly KktSystem::solve_forward/diagonal/backward's
    // call shape. Throws std::runtime_error on any Accelerate error.
    void solve_partial(SubfactorPhase phase, const double *b, double *x) const;

    const AccelerateConfig &config() const noexcept { return cfg_; }
    Index dim() const noexcept { return static_cast<Index>(n_); }

    // True iff a numeric factorization has succeeded and has not since been
    // invalidated by a failed one.
    bool has_numerics() const noexcept { return have_numeric_; }

    // The committed numeric epoch: incremented by each successful
    // factorize(), never by a failed one.
    std::uint64_t epoch() const noexcept { return epoch_; }

    // The native factorization handle, exposed so the ADAPTER
    // (symmetric_factor_accelerate.cpp, Apache-licensed, not MPL-derived)
    // can call SparseGetInertia against it directly. This is a deliberate
    // architectural choice, not a leak: unlike Pardiso's inertia counters
    // (cheap fields read back from iparm_, cached at factorize() time),
    // SparseGetInertia is a genuine, independently-fallible API call --
    // keeping the call site in the adapter is what lets the test-seam
    // convention (docs/testing.md) inject its failure without adding a hook
    // to this MPL-derived file. Only meaningful while has_numerics() is
    // true.
    const SparseOpaqueFactorization_Double &native_factorization() const noexcept {
        return numeric_;
    }

  private:
    // Builds the SparseMatrixStructure Accelerate reads: A's upper-triangle
    // CSR, interpreted directly as the CSC lower triangle of the same
    // symmetric matrix (no transpose attribute needed -- see the class-level
    // notes). Pointers refer to matrix_ and col_starts_, which this class
    // keeps alive.
    SparseMatrixStructure structure() const;

    // Release the numeric / all factorization state. Never throws -- must be
    // safe from the destructor and from re-analysis paths. Accelerate
    // requires SparseCleanup even for a FAILED factorization, so the
    // have_*_ flags are set before any status check, exactly like the
    // KktSystem precedent.
    void release_numeric() noexcept;
    void release() noexcept;

    AccelerateConfig cfg_;

    SpMatRM matrix_;               // the CSR copy this session factorizes
    std::vector<long> col_starts_; // long-typed copy of matrix_'s outer index

    SparseOpaqueSymbolicFactorization symbolic_{};
    SparseOpaqueFactorization_Double numeric_{};
    bool have_symbolic_ = false;
    bool have_numeric_ = false;

    int n_ = 0;
    std::uint64_t epoch_ = 0;
};

} // namespace hven::linear::detail
