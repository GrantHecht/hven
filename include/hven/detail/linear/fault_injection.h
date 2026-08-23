// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The hven test-seam convention -- see docs/testing.md for the full design
// rationale (why this shape, and why the two narrower alternatives were
// rejected). Summary:
//
// Both sparse backends have at least one fault path that a real backend
// cannot be made to take from this repository's own fixtures (MKL: a
// symmetric-indefinite factorize() that genuinely fails -- static pivot
// perturbation gets through everything tried;
// Accelerate: an inertia query that fails independently of a successful
// factorization -- untestable at all without a fault-injection point, since
// there is no known input that provokes it).
//
// WHERE INSTRUMENTATION GOES. The preference is the ADAPTER BOUNDARY:
// symmetric_factor_mkl.cpp and symmetric_factor_accelerate.cpp own the
// contract logic around each backend session, they are ordinary Apache-2.0
// hven files, and an observation taken there is one a consumer could in
// principle have made too. The two session implementations
// (pardiso_session.h/.cpp, accelerate_session.h/.cpp) are MPL-derived, so
// reaching into them is a deviation that has to earn itself. It is a
// preference with an escape, not a prohibition: where the fact being observed
// leaves no trace outside the function that produces it, the boundary cannot
// carry it and the hook goes where the fact is -- with the reason written
// down. Two such deviations exist today (PardisoIparmObserver's
// did-the-write-execute fields, recorded inside FactorSession::analyze; and
// its post_pardisoinit_* fields, recorded at a different line in the same
// function for a different reason); both are argued in full in
// docs/testing.md.
//
// This header is the ONE place all of it is declared -- both adapters' fault
// injectors plus the read-only OBSERVER that rides the identical seam for a
// different reason (see PardisoIparmObserver below) -- so the convention is
// documented once rather than invented per-case. It compiles to NOTHING
// unless HVEN_TESTING is defined, so #include-ing it from a normal (non-test)
// build of any TU is provably inert: there is nothing here for the
// preprocessor to keep, and the production `hven` library target's compiled
// objects are byte-for-byte what they would be if this header did not exist.
// HVEN_TESTING is defined ONLY target-wide on the standalone
// hven_fault_injection_tests executable (tests/CMakeLists.txt), which
// recompiles the platform's session and adapter sources a second time (their
// own objects, never reused from libhven.a) alongside its own test sources
// and does NOT link hven::hven -- so the normal hven_tests executable, which
// links the real hven::hven static library, never sees HVEN_TESTING either,
// and its pre-existing assertions are exercised against the exact same object
// code as a release build.

#ifdef HVEN_TESTING

namespace hven::linear::detail::testing {

// Fault injection for SymmetricFactor::factorize()'s call into the backend
// session (BOTH backends -- see the use sites in symmetric_factor_mkl.cpp and
// symmetric_factor_accelerate.cpp for the exact scope, which is identical on
// each). MKL is the backend that cannot be made to fail from a fixture at all;
// Accelerate's numeric refusal is reachable in principle on real hardware but
// not from any input this repository's fixtures produce, and its consumer --
// the interior-point engine's zero-filling evidence projection on a failed
// factorization -- needs the failure to be provoked deterministically and by
// specific status code, which only injection gives.
//
// When active, the real detail::FactorSession::factorize() call is
// SKIPPED entirely and `injected_backend_code` is used in its place -- the
// session's own internal state is therefore left completely untouched by
// this injector, which is what keeps the injected scenario faithful: it is
// valid ONLY when engaged on a session that has never previously factorized
// successfully (a session that has never touched has_numerics_ = true has
// nothing for this injector to leave inconsistent). Testing the deeper
// "successful factorize, then a later one fails" scenario would require
// either touching the MPL session file or a fully virtual session interface
// -- both rejected in docs/testing.md; that deeper scenario stays
// inspection-only, exactly as the factorize-failure disclosure in
// symmetric_factor_mkl.cpp already states.
struct FactorizeFaultInjector {
    static inline bool active = false;
    static inline int injected_backend_code = -99;
};

// Fault injection for SymmetricFactor::analyze()'s call into the backend
// session's symbolic phase (BOTH backends -- see the use sites in
// symmetric_factor_mkl.cpp and symmetric_factor_accelerate.cpp). When active,
// the real session's analyze() is SKIPPED and a failure is raised in its
// place, exactly as a backend symbolic failure would be: the freshly built
// session is discarded before it is committed, so this engine keeps whatever
// state it had, which is the same guarantee analyze()'s own contract makes on
// a real failure. Faithful in every scenario for that reason -- nothing about
// an existing session is touched, because the failure happens before any
// existing session is replaced.
//
// It exists because no matrix reaching this surface makes either backend's
// symbolic phase fail: the adapters validate the input convention themselves
// (compressed, square, non-empty, upper triangle, structural diagonal) and
// reject a violation as a caller error before the backend sees it, so what is
// left for the backend to fail on is reordering and sizing, which no fixture
// can provoke. The consumer of this seam is the interior-point engine's
// record-the-status-and-continue behavior on a symbolic failure, which is
// otherwise unreachable.
struct AnalyzeFaultInjector {
    static inline bool active = false;
    static inline int injected_backend_code = -3;
};

// Fault injection for SymmetricFactor::inertia()'s SparseGetInertia call
// (Accelerate only -- see its use site in symmetric_factor_accelerate.cpp).
// SparseGetInertia is a side-effect-free query against an already-successful
// factorization, so unlike the injector above, this one is faithful in every
// scenario: overriding its return code changes nothing about the session's
// own state, only what the query is reported to have found.
struct InertiaQueryFaultInjector {
    static inline bool active = false;
    static inline int injected_rc = -1;
};

// NOT a fault injector -- a pure OBSERVER (MKL only), riding the same
// target-wide HVEN_TESTING seam because it needs the same thing the two
// structs above need: a read that only the ADAPTER
// (symmetric_factor_mkl.cpp) can reach, since the MPL-derived session file
// may carry no test-only hooks (see the file-level comment above and
// docs/testing.md). It never changes behavior -- SymmetricFactor::analyze()
// (MKL) records what iparm[1] / iparm[12] actually held right after a real,
// unmodified analyze() call, via FactorSession's own unconditional,
// non-test-gated ordering_iparm()/weighted_matching_iparm() accessors
// (pardiso_session.h). `int`, not MKL_INT, keeps this header (shared by both
// backends' adapter TUs) free of any MKL type dependency, matching the
// style of the two structs above. Exists solely so a test can assert the
// ordering/weighted_matching don't-write-by-default rule EXECUTES, rather
// than trusting the guarded `if` in FactorSession::analyze by inspection
// alone -- see tests/linear/test_fault_injection.cpp's Pardiso*Iparm* tests
// and docs/testing.md.
//
// The post_pardisoinit_* fields below serve a related but DISTINCT purpose
// and come from a DIFFERENT point in FactorSession::analyze -- not the
// adapter boundary at all. They pin the linked MKL's pardisoinit defaults
// for iparm[10]/iparm[33]/iparm[18], for iparm[7]/iparm[9], and for
// iparm[17], for the canaries design decisions elsewhere rely on (see
// BackendDefaultPremise.MklPardisoinitLeavesScalingAndCnrAtZero,
// BackendDefaultPremise.MklPardisoinitDefaultsTheRefinementCapAndPivotPerturbExponent
// and BackendDefaultPremise.MklPardisoinitAlreadyRequestsTheFactorNonzeroCount
// in test_fault_injection.cpp). They cannot be adapter-boundary reads the way
// last_ordering_iparm/last_weighted_matching_iparm are: this session's own
// phase-11 (symbolic analysis) backend call was observed, empirically, to
// overwrite iparm[33] (and iparm[18]) with its own output before
// FactorSession::analyze returns, so a read taken after that call answers
// "what phase 11 left there," not "what pardisoinit defaulted this to."
// Recorded instead at the one line inside the session file where the fact
// is still true -- see that file's own comment at the capture site for the
// full argument.
struct PardisoIparmObserver {
    // reset() before arming an observation keeps a later test whose
    // expected value happens to be 0 from silently passing on stale
    // state left by an earlier analyze().
    static void reset() {
        last_ordering_iparm = 0;
        last_weighted_matching_iparm = 0;
        post_pardisoinit_recorded = false;
        post_pardisoinit_matrix_scaling_iparm = 0;
        post_pardisoinit_cnr_iparm = 0;
        post_pardisoinit_factor_mflops_request_iparm = 0;
        post_pardisoinit_refinement_cap_iparm = 0;
        post_pardisoinit_pivot_perturb_iparm = 0;
        post_pardisoinit_factor_nnz_request_iparm = 0;
        recorded = false;
        ordering_was_written = false;
        ordering_written_value = 0;
        weighted_matching_was_written = false;
        weighted_matching_written_value = 0;
        matrix_scaling_was_written = false;
        matrix_scaling_written_value = 0;
        pivot_strategy_was_written = false;
        pivot_strategy_written_value = 0;
        factorization_algorithm_was_written = false;
        factorization_algorithm_written_value = 0;
        solve_parallelism_was_written = false;
        solve_parallelism_written_value = 0;
        cnr_was_written = false;
        cnr_written_value = 0;
        factor_mflops_was_written = false;
        factor_mflops_written_value = 0;
        max_refinement_was_written = false;
        max_refinement_written_value = 0;
        pivot_perturb_was_written = false;
        pivot_perturb_written_value = 0;
    }
    static inline bool recorded = false;
    static inline int last_ordering_iparm = 0;
    static inline int last_weighted_matching_iparm = 0;

    // The raw array values for iparm[10] (matrix scaling), iparm[33] (CNR
    // thread count), iparm[18] (Mflop-report request code), iparm[7]
    // (full-solve refinement cap), iparm[9] (pivot perturbation
    // exponent), and iparm[17] (factor-nonzero-count request) EXACTLY as
    // pardisoinit left them -- captured inside
    // FactorSession::analyze, before that function's own phase-11 call
    // runs, per this struct's own doc comment above. iparm[7]/iparm[9]
    // extend the same deviation for the same reason: they are the entries
    // the max_refinement_iters / pivot_perturb_exp don't-write states
    // inherit, and the load-bearing BackendDefaultPremise canary pinning
    // their pardisoinit defaults (2 / 8 on the audited MKL) can only be fed
    // from this capture point. iparm[17] extends it once more for the
    // mirror-image reason: that entry is written UNCONDITIONALLY by
    // analyze() a few lines later, and the §11 ledger's delta-free
    // argument for that write rests entirely on pardisoinit having already
    // put the same value (-1) there -- a premise only a read taken before
    // the write can check. `post_pardisoinit_recorded` is a separate
    // flag from `recorded` above: the two are set at different lines (this
    // one right after pardisoinit(), the other at the very end of
    // analyze()), so a test using only one of the two pairs still gets an
    // honest "did this actually run" signal for the half it uses.
    static inline bool post_pardisoinit_recorded = false;
    static inline int post_pardisoinit_matrix_scaling_iparm = 0;
    static inline int post_pardisoinit_cnr_iparm = 0;
    static inline int post_pardisoinit_factor_mflops_request_iparm = 0;
    static inline int post_pardisoinit_refinement_cap_iparm = 0;
    static inline int post_pardisoinit_pivot_perturb_iparm = 0;
    static inline int post_pardisoinit_factor_nnz_request_iparm = 0;

    // --- the DID-THE-WRITE-EXECUTE observable ---
    //
    // The value-level fields above cannot always settle the
    // don't-write-by-default claim, and the reason is a coincidence rather
    // than a design flaw: on some MKL versions pardisoinit's own iparm[1]
    // default equals one of the three non-default values Ordering can
    // request, so "left it alone" and "wrote exactly that value" produce
    // identical arrays and no after-the-fact read can tell them apart. On
    // such a version the rule falls back to inspecting a guard -- exactly
    // the kind of claim this seam exists to make executable.
    //
    // These four fields close it outright and version-independently. They are
    // set at the two guarded write sites inside FactorSession::analyze
    // (pardiso_session.cpp) and nowhere else, so the DEFAULT case can assert
    // `ordering_was_written == false`: a claim about whether an assignment
    // executed, which no backend default can make vacuous. The non-default
    // cases assert `true` plus the value written, which is what keeps that
    // false assertion from passing for want of a live observable.
    //
    // These are the ONE place this project instruments INSIDE an MPL-derived
    // file rather than at the adapter boundary beside it. That is a sanctioned
    // deviation from the boundary-first preference, not an oversight: the
    // boundary genuinely cannot carry this one, because the fact being
    // observed is the execution of a statement that leaves no trace anywhere
    // outside the function containing it. The preference, this deviation and
    // its reasoning are written out in docs/testing.md.
    static inline bool ordering_was_written = false;
    static inline int ordering_written_value = 0;
    static inline bool weighted_matching_was_written = false;
    static inline int weighted_matching_written_value = 0;

    // The same did-the-write-execute pair for the option set's five other
    // guarded iparm writes -- matrix_scaling (iparm[10]), pivot_strategy
    // (iparm[20]), factorization_algorithm (iparm[23]), solve_parallelism
    // (iparm[24]), cnr_threads (iparm[33]) -- plus the Mflop-evidence
    // request (iparm[18], gated on Options::collect_factor_mflops; the
    // sibling iparm[17] write is UNCONDITIONAL, so it carries no flag
    // here -- see FactorSession::factor_nonzeros()'s own doc comment), for
    // the identical reason and at the identical write sites in
    // FactorSession::analyze. Carried for every new knob for symmetry and
    // the same mutation-resistance the original two fields document, not
    // because every one of them is independently known to be ambiguous at
    // the value level on the MKL currently linked. Each entry gets its OWN
    // flag pair -- no entry shares a flag with any other, including the
    // iparm[17]/iparm[18] pair the frozen-evidence amendment originally
    // bundled under one flag before the split into an unconditional write
    // (iparm[17]) and this one remaining guarded write (iparm[18]).
    static inline bool matrix_scaling_was_written = false;
    static inline int matrix_scaling_written_value = 0;
    static inline bool pivot_strategy_was_written = false;
    static inline int pivot_strategy_written_value = 0;
    static inline bool factorization_algorithm_was_written = false;
    static inline int factorization_algorithm_written_value = 0;
    static inline bool solve_parallelism_was_written = false;
    static inline int solve_parallelism_written_value = 0;
    static inline bool cnr_was_written = false;
    static inline int cnr_written_value = 0;
    // The written VALUE, not just whether the write ran -- the same pair
    // shape as every knob above, so the claim "iparm[18] was written and
    // was written to the exact contract constant Pardiso expects (-1)" is
    // asserted directly rather than inferred from the flag alone.
    static inline bool factor_mflops_was_written = false;
    static inline int factor_mflops_written_value = 0;

    // The same did-the-write-execute pair for the two writes the
    // don't-write-state amendment turned conditional: max_refinement_iters
    // (iparm[7]) and pivot_perturb_exp (iparm[9]). Unlike every knob above,
    // these two default to WRITTEN (0 / 8 -- see their Options doc
    // comments), so the default-Options act pin asserts the flags TRUE with
    // the exact legacy values -- that is the byte-identity proof that a
    // default-constructed Options still performs the identical writes the
    // unconditional statements always performed -- and the nullopt case
    // asserts them FALSE, the don't-write act no boundary read can pin
    // (pardisoinit's own values for both entries are exactly what a
    // no-op-write would also leave there, the original ordering ambiguity
    // in its strongest form).
    static inline bool max_refinement_was_written = false;
    static inline int max_refinement_written_value = 0;
    static inline bool pivot_perturb_was_written = false;
    static inline int pivot_perturb_written_value = 0;
};

// NOT a fault injector either -- a second pure OBSERVER (MKL only), and one
// that needs no deviation at all: it is taken at the ADAPTER BOUNDARY this
// project prefers, in symmetric_factor_mkl.cpp, immediately before a solve
// hands the session's own solve() the buffers, and it reads the count
// through FactorSession's ordinary, non-test-gated config() accessor
// (pardiso_session.h).
//
// READ THE NAME LITERALLY. What is recorded is the count STORED IN THE
// SESSION CONFIG at the moment of a backend call -- not a measurement of
// what MKL then ran at. This observer would report the same values if the
// thread scope inside the session were deleted outright; it is not, and
// cannot be, evidence that the scope engages.
//
// It exists for SymmetricFactor::set_num_threads, and it pins the half of
// that setter's claim which has no public observable: that a mid-life change
// lands in the configuration a subsequent backend call reads, with no
// rebuild and no re-analysis in between. (The public API cannot show even
// that much: a thread count changes no result, MKL exposes no query for the
// thread-local override in force, and no accessor reports a live session's
// configuration.)
//
// THE REST OF THE CLAIM IS CARRIED BY TWO OTHER THINGS, deliberately, rather
// than by instrumenting the MPL-derived session file:
//   - Traced data flow: the recorded field IS the field the backend call
//     scope reads. FactorSession::run_phase constructs
//     MklThreadScope(cfg_.num_threads) immediately before the one ::pardiso
//     call in this codebase, and every phase -- symbolic, numeric, solve,
//     release -- reaches the backend through that single function. There is
//     no second path and no copy taken earlier.
//   - Behavioural coverage of the scope itself:
//     SymmetricFactor.APerInstanceThreadCountRestoresTheCallersOwnThreadLocalOverride
//     (tests/linear/test_symmetric_factor.cpp) proves the scope engages and
//     restores the caller's own thread-local override around a real backend
//     call.
// Closing the seam between those two from inside the session file would be a
// deviation bought for one already-covered line, so it is not taken.
struct ThreadCountObserver {
    static void reset() {
        recorded = false;
        last_config_num_threads = -1;
    }

    // -1 rather than 0 as the unset value: 0 is a REAL count here (it means
    // "leave the backend's own default alone"), so a zero-initialized field
    // would be indistinguishable from an observation of it.
    static inline bool recorded = false;

    // The session config's num_threads as it stood when the last observed
    // backend solve was issued. Named for what it is -- see the literal-name
    // paragraph above.
    static inline int last_config_num_threads = -1;
};

} // namespace hven::linear::detail::testing

#endif // HVEN_TESTING
