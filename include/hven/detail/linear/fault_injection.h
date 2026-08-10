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
// down. Exactly one such deviation exists today (PardisoIparmObserver's
// did-the-write-execute fields, recorded inside FactorSession::analyze); it is
// argued in full in docs/testing.md.
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

// Fault injection for SymmetricFactor::factorize()'s call into the MKL
// session (see its use site in symmetric_factor_mkl.cpp for the exact
// scope). When active, the real detail::FactorSession::factorize() call is
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
struct PardisoIparmObserver {
    // reset() before arming an observation keeps a later test whose
    // expected value happens to be 0 from silently passing on stale
    // state left by an earlier analyze().
    static void reset() {
        last_ordering_iparm = 0;
        last_weighted_matching_iparm = 0;
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
    }
    static inline bool recorded = false;
    static inline int last_ordering_iparm = 0;
    static inline int last_weighted_matching_iparm = 0;

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
    static inline bool factor_mflops_was_written = false;
};

} // namespace hven::linear::detail::testing

#endif // HVEN_TESTING
