#pragma once

// The hven test-seam convention -- see docs/testing.md for the full design
// rationale (why this shape, and why the two narrower alternatives were
// rejected). Summary:
//
// Both sparse backends have at least one fault path that a real backend
// cannot be made to take from this repository's own fixtures (MKL: a
// symmetric-indefinite factorize() that genuinely fails -- static pivot
// perturbation gets through everything tried, task-5-report.md I1;
// Accelerate: an inertia query that fails independently of a successful
// factorization -- untestable at all without a fault-injection point, since
// there is no known input that provokes it). Both backend session
// implementations (pardiso_session.h/.cpp, accelerate_session.h/.cpp) are
// MPL-derived files under the same governance Task 5 already established for
// the Pardiso one: no test-only hooks may be added to them. The two
// ADAPTER files that own the contract logic around each session
// (symmetric_factor_mkl.cpp, symmetric_factor_accelerate.cpp) are ordinary
// Apache-2.0 hven files with no such restriction.
//
// This header is the ONE place both adapters' fault injectors are declared
// (plus one read-only OBSERVER that rides the identical seam for a different
// reason -- see PardisoIparmObserver below), so the convention is documented
// once and used across all three rather than invented per-case. It compiles
// to NOTHING unless HVEN_TESTING is defined --
// #include-ing it from a normal (non-test) build of either adapter TU is
// therefore provably inert: there is nothing here for the preprocessor to
// keep once HVEN_TESTING is undefined, so the production `hven` library
// target's compiled objects are byte-for-byte what they would be if this
// header did not exist. HVEN_TESTING is defined ONLY target-wide on the
// standalone hven_fault_injection_tests executable (tests/CMakeLists.txt),
// which recompiles the platform's adapter source file a second time (its own
// object, never reused from libhven.a) alongside its own test sources and
// does NOT link hven::hven -- so the normal hven_tests executable, which
// links the real hven::hven static library, never sees HVEN_TESTING either,
// and its 47+ pre-existing assertions are exercised against the exact same
// object code as a release build.

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
// inspection-only, exactly as task-5-report.md's I1 already disclosed.
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
    static inline int last_ordering_iparm = 0;
    static inline int last_weighted_matching_iparm = 0;
};

} // namespace hven::linear::detail::testing

#endif // HVEN_TESTING
