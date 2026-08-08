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
// This header is the ONE place both adapters' fault injectors are declared,
// so the convention is documented once and used twice rather than invented
// per-backend. It compiles to NOTHING unless HVEN_TESTING is defined --
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

} // namespace hven::linear::detail::testing

#endif // HVEN_TESTING
