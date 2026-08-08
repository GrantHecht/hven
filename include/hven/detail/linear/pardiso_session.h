#pragma once

// =============================================================================
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// This file (and its .cpp) is DERIVED from the PardisoSupport module of the
// Eigen linear algebra library (MPL-2.0) -- specifically its Pardiso call
// discipline: the argument block handed to ::pardiso, the phase sequence
// (11 symbolic / 22 numeric / 33 solve / -1 release), the parameter-array
// value set, and the treatment of Pardiso's error codes. That module reached
// hven by way of the interior-point engine's own Eigen-derived Pardiso
// interface, which carries the same notice.
//
// MPL-2.0 applies to THIS FILE only, as the license permits; the remainder of
// hven is Apache-2.0. See notices/eigen-mpl2.txt.
//
// What is NOT derived: the session lifecycle, ownership and epoch semantics,
// evidence reporting, and the partial-solve refinement rule around it are
// hven's own and exist to serve the frozen interface contract of
// hven/linear/symmetric_factor.h.
// =============================================================================

#include <array>
#include <cstdint>
#include <vector>

#include <mkl_pardiso.h>
#include <mkl_types.h>

#include "hven/core/types.h"

namespace hven::linear::detail {

// Pardiso's internal handle and parameter array are both fixed at 64 entries.
inline constexpr int kPardisoSlots = 64;

// hven hands Eigen's CSR index arrays straight to Pardiso with no reindexing
// pass, which is only sound while the two integer widths agree. Under MKL's
// LP64 interface (what hven links) MKL_INT is `int`, matching Eigen's default
// sparse StorageIndex. An ILP64 MKL would break the assumption silently, so
// it breaks the build here instead.
static_assert(sizeof(MKL_INT) == sizeof(SpMatRM::StorageIndex),
              "hven requires an LP64 MKL: MKL_INT must match Eigen's sparse StorageIndex width");

// The backend knobs one session is created with. Fixed for the session's
// lifetime: they are baked into the factorization it produces, so an engine
// that adopts a session inherits them rather than reinterpreting them.
struct PardisoConfig {
    int mtype = -2;               // real symmetric indefinite
    int num_threads = 0;          // 0 = leave MKL's own default alone
    int pivot_perturb_exp = 8;    // static pivot perturbation, 10^-k
    int max_refinement_iters = 0; // full-solve iterative refinement cap

    // Fill-in reordering override for iparm[1]. 0 means "leave iparm[1]
    // alone" -- pardisoinit's own value survives -- and is the only value
    // this field takes at SymmetricFactor::Options::Ordering::
    // kBackendDefault; the only other values FactorSession::analyze ever
    // sees here are 2 (nested dissection / METIS) and 3 (its OpenMP-parallel
    // variant), matching Options::Ordering exactly.
    int ordering = 0;

    // Maximum weighted matching (iparm[12]). false leaves iparm[12] alone;
    // true writes iparm[12] = 1. There is no way to request writing
    // iparm[12] = 0 explicitly through this surface -- matching
    // Options::weighted_matching's don't-write-by-default rule.
    bool weighted_matching = false;
};

// One Pardiso factorization session: the `pt` handle, the parameter array,
// the permutation vector, and the CSR copy Pardiso reads on every phase.
//
// The session is the unit of SHARING. It is held by std::shared_ptr, and an
// engine plus any handles emitted from it co-own exactly one of these; the
// last owner releases Pardiso's internal memory. Everything that describes
// "which numerics are in the backend right now" -- the numeric epoch, the
// inertia readbacks -- therefore lives here rather than in any one owner.
//
// Errors are surfaced two different ways on purpose: phases with nowhere to
// report a numeric outcome (symbolic, solve) throw, while the numeric
// factorization RETURNS Pardiso's error code for the caller to fold into its
// own outcome type.
//
// Not internally synchronized -- see the thread-safety note on the public
// class.
//
// Named `FactorSession`, not `PardisoSession`: hven/linear/symmetric_factor.h
// (the backend-neutral public header) forward-declares and holds this type by
// that name, so the ONE session type actually compiled into a given build --
// this one on MKL platforms, its Accelerate counterpart
// (hven/detail/linear/accelerate_session.h) on Apple -- must share it. No
// polymorphism is involved: src/CMakeLists.txt compiles exactly one of the
// two implementations per platform (mirroring how the dense component splits
// platforms), so there is only ever one definition of
// hven::linear::detail::FactorSession in a linked binary. Everything else
// about this class -- its Pardiso-specific fields, methods, and call
// discipline -- is unchanged from its origin as PardisoSession.
class FactorSession {
  public:
    // `initial_epoch` seeds this session's numeric epoch so that an engine
    // re-analyzing into a fresh session continues its epoch sequence instead
    // of restarting it, which would let one epoch value name two different
    // sets of numerics.
    FactorSession(const PardisoConfig &cfg, std::uint64_t initial_epoch);
    ~FactorSession();

    FactorSession(const FactorSession &) = delete;
    FactorSession &operator=(const FactorSession &) = delete;

    // Pardiso phase 11. Takes the CSR copy this session will factorize.
    // Throws std::runtime_error carrying Pardiso's error code on failure.
    void analyze(const SpMatRM &A);

    // Pardiso phase 22 against the existing symbolic. RETURNS Pardiso's
    // error code (0 on success) rather than throwing: a numeric failure is
    // the caller's outcome to report. On success the numeric epoch advances
    // and the inertia readbacks below become current; on failure the session
    // is left with no usable numerics.
    //
    // Requires A to match the analyzed matrix's dimensions and stored-entry
    // count -- the caller is responsible for having verified the pattern
    // itself. Throws std::invalid_argument if it does not.
    int factorize(const SpMatRM &A);

    // Pardiso phase 33. `b` and `x` are contiguous column-major buffers of
    // dim() * nrhs doubles and must not alias. Throws std::runtime_error on
    // any Pardiso error.
    void solve(const double *b, double *x, Index nrhs) const;

    // Pardiso phases 331 / 332 / 333 (forward, diagonal, backward), one
    // right-hand side. Refinement is forced to zero for the duration of the
    // call and restored afterward -- unconditionally, whatever the session
    // was configured with: Pardiso documents that a step-by-step solve with
    // refinement enabled produces a wrong result, and it does so silently.
    // Throws std::runtime_error on any Pardiso error.
    void solve_partial(int phase, const double *b, double *x) const;

    const PardisoConfig &config() const noexcept { return cfg_; }
    Index dim() const noexcept { return static_cast<Index>(n_); }

    // True iff a numeric factorization has succeeded and has not since been
    // invalidated by a failed one.
    bool has_numerics() const noexcept { return has_numerics_; }

    // The committed numeric epoch: incremented by each successful
    // factorize(), never by a failed one.
    std::uint64_t epoch() const noexcept { return epoch_; }

    // Readbacks from the most recent successful factorization. Only
    // meaningful while has_numerics() is true.
    Index num_pos_eigs() const noexcept { return static_cast<Index>(n_pos_); }
    Index num_neg_eigs() const noexcept { return static_cast<Index>(n_neg_); }
    Index num_perturbed_pivots() const noexcept { return static_cast<Index>(perturbed_pivots_); }

    // Refinement steps performed by the most recent solve.
    Index refinement_iters() const noexcept { return static_cast<Index>(refinement_iters_); }

  private:
    // Runs one Pardiso phase and returns its error code. `use_matrix`
    // selects whether the stored CSR arrays are passed (phase -1 references
    // no matrix). Applies the thread-count override for the duration of the
    // call.
    MKL_INT run_phase(MKL_INT phase, bool use_matrix, MKL_INT nrhs, const double *b,
                      double *x) const;

    // Pardiso phase -1. Never throws: it runs from the destructor.
    void release() noexcept;

    PardisoConfig cfg_;

    // Mutable because const solves still hand Pardiso writable pointers to
    // its own handle and parameter array -- the C API takes no const.
    mutable std::array<void *, kPardisoSlots> pt_{};
    mutable std::array<MKL_INT, kPardisoSlots> iparm_{};
    mutable std::vector<MKL_INT> perm_;

    // The matrix Pardiso factorized. Owned rather than borrowed: Pardiso
    // reads the value and index arrays again on every solve, long after the
    // caller's matrix may be gone.
    SpMatRM matrix_;

    MKL_INT n_ = 0;
    bool active_ = false; // pt_ holds Pardiso-owned memory (needs phase -1)
    bool has_numerics_ = false;
    std::uint64_t epoch_ = 0;

    MKL_INT n_pos_ = 0;
    MKL_INT n_neg_ = 0;
    MKL_INT perturbed_pivots_ = 0;
    mutable MKL_INT refinement_iters_ = 0;
};

} // namespace hven::linear::detail
