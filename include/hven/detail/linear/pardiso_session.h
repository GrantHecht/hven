#pragma once

// =============================================================================
// TWO LICENSES APPLY TO THIS FILE, because its upstream carries two. Eigen's
// PardisoSupport module is MPL-2.0 as part of Eigen AND reproduces Intel's own
// BSD-3-Clause notice at the top of every one of its files, that module being
// Intel's contributed Pardiso binding. Deriving from it means retaining both.
// Both are reproduced here in full, exactly as PardisoSupport.h itself does.
// See notices/eigen-mpl2.txt.
//
// -----------------------------------------------------------------------------
// Copyright (c) 2011, Intel Corporation. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Intel Corporation nor the names of its contributors may
//   be used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// -----------------------------------------------------------------------------
//
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
// The two licenses above apply to THIS FILE only, as MPL-2.0 permits and as
// the BSD terms' source-redistribution condition requires; the remainder of
// hven is Apache-2.0. See notices/eigen-mpl2.txt.
//
// What is NOT derived: the session lifecycle, ownership, session-identity and
// epoch semantics,
// evidence reporting, and the partial-solve refinement rule around it are
// hven's own and exist to serve the frozen interface contract of
// hven/linear/symmetric_factor.h. So is the one test-only record in the .cpp
// -- two HVEN_TESTING-gated lines at the guarded parameter-array writes in
// analyze(), compiling to nothing in any normal build. See docs/testing.md for
// why that particular observation cannot be taken at the adapter boundary this
// project otherwise instruments at.
// =============================================================================

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <mkl_pardiso.h>
#include <mkl_types.h>

#include "hven/core/types.h"
#include "hven/detail/linear/session_id.h"

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

    // Fill-in reordering override for iparm[1]. std::nullopt means "leave
    // iparm[1] alone" -- pardisoinit's own value survives -- and is the only
    // state this field takes at SymmetricFactor::Options::Ordering::
    // kBackendDefault; a present value is written verbatim. The values
    // FactorSession::analyze is asked for today are 0 (minimum degree --
    // kMinimumDegree, a real Pardiso iparm[1] value psiopt exposes as
    // QPOrderingModes::MINDEG), 2 (nested dissection / METIS), and 3 (its
    // OpenMP-parallel variant), matching Options::Ordering exactly. An
    // earlier `int` encoding that used 0 itself as the "don't write"
    // sentinel would have collided with the real value 0 now represents --
    // std::optional side-steps that collision entirely rather than reserving
    // a magic sentinel, which is exactly what let kMinimumDegree land as a
    // pure Options-side amendment with no change to this field's wire
    // representation.
    std::optional<int> ordering;

    // Maximum weighted matching (iparm[12]). false leaves iparm[12] alone;
    // true writes iparm[12] = 1. There is no way to request writing
    // iparm[12] = 0 explicitly through this surface -- matching
    // Options::weighted_matching's don't-write-by-default rule.
    bool weighted_matching = false;

    // Matrix scaling (iparm[10]). false leaves iparm[10] alone; true writes
    // iparm[10] = 1 -- same don't-write-by-default shape as
    // weighted_matching, and see Options::matrix_scaling for why the two
    // are the same judgment call on the Accelerate side.
    bool matrix_scaling = false;

    // Pivoting strategy override for iparm[20]. std::nullopt means "leave
    // iparm[20] alone" -- the only state this field takes at
    // Options::PivotStrategy::kBackendDefault; a present value (0, 1, 4, 6,
    // 8, or 13 -- Options::PivotStrategy's own documented values) overrides
    // it verbatim.
    std::optional<int> pivot_strategy;

    // Two-level factorization algorithm override for iparm[23].
    // std::nullopt means "leave iparm[23] alone" -- the only state this
    // field takes at Options::FactorizationAlgorithm::kBackendDefault; a
    // present value (0 = classic, 1 = two-level) overrides it verbatim.
    std::optional<int> factorization_algorithm;

    // Parallel forward/backward solve (iparm[24]). false leaves iparm[24]
    // alone; true writes iparm[24] = 1 -- same don't-write-by-default shape
    // as weighted_matching and matrix_scaling.
    bool parallel_solve = false;

    // Thread count for conditional numerical reproducibility mode
    // (iparm[33]). 0 (default) leaves iparm[33] alone -- CNR mode off; a
    // positive value writes iparm[33] to that count and turns CNR mode on.
    int cnr_threads = 0;

    // Whether to request factor-size evidence (iparm[17] nonzero count,
    // iparm[18] Mflop count) during analyze(), and read it back after a
    // successful factorize(). false (default) leaves both entries alone and
    // FactorSession::factor_nonzeros()/factor_mflops() report the
    // not-collected sentinel; see Options::report_factor_evidence for why
    // this is opt-in.
    bool report_factor_evidence = false;
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
    // factorize(), never by a failed one. Unique only WITHIN this session --
    // see session_id() for the part of the identity that separates two
    // sessions carrying the same epoch.
    std::uint64_t epoch() const noexcept { return epoch_; }

    // This session's process-unique identity, fixed at construction. Re-
    // analysis builds a NEW session (a fork) whose epoch sequence continues
    // the old one's, so the epoch alone cannot tell the two branches apart;
    // this id can. See hven/detail/linear/session_id.h.
    std::uint64_t session_id() const noexcept { return session_id_; }

    // Readbacks from the most recent successful factorization. Only
    // meaningful while has_numerics() is true.
    Index num_pos_eigs() const noexcept { return static_cast<Index>(n_pos_); }
    Index num_neg_eigs() const noexcept { return static_cast<Index>(n_neg_); }
    Index num_perturbed_pivots() const noexcept { return static_cast<Index>(perturbed_pivots_); }

    // Refinement steps performed by the most recent solve.
    Index refinement_iters() const noexcept { return static_cast<Index>(refinement_iters_); }

    // True iff cfg_.report_factor_evidence requested factor-size evidence
    // -- derived straight from the config rather than a separate flag,
    // since the two can never disagree. factor_nonzeros()/factor_mflops()
    // are only meaningful while this AND has_numerics() are both true.
    bool has_factor_evidence() const noexcept { return cfg_.report_factor_evidence; }
    Index factor_nonzeros() const noexcept { return static_cast<Index>(factor_nonzeros_); }
    Index factor_mflops() const noexcept { return static_cast<Index>(factor_mflops_); }

    // Read-only access to iparm[1] (fill-in reordering) / iparm[12] (maximum
    // weighted matching) as they stand after the most recent analyze().
    // General-purpose and unconditional -- NOT a test hook -- for entries
    // this class writes on a caller's behalf but does not otherwise cache
    // into a named field the way num_pos_eigs() and friends do for the
    // entries its own contract logic consumes. Their first consumer is
    // executable coverage for the ordering/weighted_matching
    // don't-write-by-default rule (docs/testing.md, hven_fault_injection_tests);
    // nothing about either accessor is conditional on HVEN_TESTING.
    MKL_INT ordering_iparm() const noexcept { return iparm_[1]; }
    MKL_INT weighted_matching_iparm() const noexcept { return iparm_[12]; }

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
    const std::uint64_t session_id_ = next_session_id();

    MKL_INT n_pos_ = 0;
    MKL_INT n_neg_ = 0;
    MKL_INT perturbed_pivots_ = 0;
    mutable MKL_INT refinement_iters_ = 0;

    // Cached from iparm[17]/iparm[18] at factorize() time, only when
    // cfg_.report_factor_evidence is set -- see has_factor_evidence().
    MKL_INT factor_nonzeros_ = -1;
    MKL_INT factor_mflops_ = -1;
};

} // namespace hven::linear::detail
