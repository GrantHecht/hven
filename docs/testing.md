# The test-seam convention

hven has two sparse backends (MKL Pardiso, Apple Accelerate), and both have
at least one fault path a real backend cannot be made to take from any
fixture this repository can build: MKL will not genuinely refuse a symmetric
indefinite `factorize()` (static pivot perturbation gets through everything
tried — task-5-report.md's I1 disclosure), and Accelerate's inertia query has
no known input that provokes a failure independent of a successful
factorization. Both paths matter — the frozen contract's fabrication fixes
(`docs/dev/plans/2026-08-08-hven-m1-linear-and-rig-spec.md` A.5) exist
specifically to make these paths honest — so they need executable coverage,
not just inspection. This page is the ONE place the convention that covers
them is designed; every backend/component that needs a fault-injection seam
should use it rather than inventing another.

## The constraint that shapes the design

Both backend session implementations
(`hven/detail/linear/pardiso_session.h`/`.cpp`,
`hven/detail/linear/accelerate_session.h`/`.cpp`) are MPL-2.0-derived files
(from Eigen's PardisoSupport and AccelerateSupport modules respectively —
see the notice at the top of each). This repository's governance rule
(CLAUDE.md §6, carried from Task 5's own precedent) is that MPL-derived files
carry no test-only hooks. A fault-injection seam that adds an
`#ifdef HVEN_TESTING` branch inside either session file would violate that,
however narrow the branch.

The two backend ADAPTER files that own the contract logic AROUND each
session (`src/linear/symmetric_factor_mkl.cpp`,
`src/linear/symmetric_factor_accelerate.cpp`) are ordinary Apache-2.0 hven
files with no such restriction — they are where the seam lives.

## The shape

`hven/detail/linear/fault_injection.h` declares, for each backend, a small
`static inline` struct under `hven::linear::detail::testing`, entirely
guarded by `#ifdef HVEN_TESTING`:

- `FactorizeFaultInjector` (MKL) — `active`, `injected_backend_code`.
- `InertiaQueryFaultInjector` (Accelerate) — `active`, `injected_rc`.

The header compiles to **nothing** unless `HVEN_TESTING` is defined, so
`#include`-ing it from a normal build is provably inert — there is nothing
left for the preprocessor to keep. Each adapter's one call site checks its
injector with an `#ifdef HVEN_TESTING` guard:

```cpp
int backend_code;
#ifdef HVEN_TESTING
if (detail::testing::FactorizeFaultInjector::active) {
    backend_code = detail::testing::FactorizeFaultInjector::injected_backend_code;
} else
#endif
{
    backend_code = session_->factorize(A);
}
```

`HVEN_TESTING` is defined **target-wide, on exactly one CMake target**:
`hven_fault_injection_tests` (`tests/CMakeLists.txt`), a standalone
executable that:

- recompiles the platform's own session TU (`pardiso_session.cpp` /
  `accelerate_session.cpp`) completely UNTOUCHED — no macro, no branch, the
  identical source the production `hven` library compiles;
- recompiles the platform's own adapter TU (`symmetric_factor_mkl.cpp` /
  `symmetric_factor_accelerate.cpp`) a **second time**, as its own object,
  with `HVEN_TESTING` defined;
- does **not** link `hven::hven`.

Not linking `hven::hven` is not just tidiness — it is required. Both this
target's freshly-compiled adapter object and `libhven.a`'s own define
`hven::linear::SymmetricFactor`'s and `Factorization`'s methods; linking
both into one binary would be a duplicate-symbol error. The consequence is
also the point: the production `hven` library target, and `hven_tests`
(which links it), never see `HVEN_TESTING` and are compiled exactly as they
would be if `fault_injection.h` and `hven_fault_injection_tests` did not
exist. `hven_fault_injection_tests` is registered with `gtest_discover_tests`
like `hven_tests` — it is a normal, separate ctest executable, not a
special-cased build step.

## Why two different injection points, not one mechanism copy-pasted

The two injectors are NOT interchangeable, and the difference is why each
lives where it does:

- **`InertiaQueryFaultInjector` (Accelerate) is faithful in every scenario.**
  `SparseGetInertia` is a side-effect-free query against an
  ALREADY-successful factorization. Overriding its return code changes
  nothing about the session's own state — only what the query is reported
  to have found. This is why `evidence_of()` in
  `symmetric_factor_accelerate.cpp` calls `SparseGetInertia` directly
  against `session.native_factorization()` (a getter `FactorSession`
  exposes for exactly this reason) rather than caching the result inside the
  MPL-derived session at `factorize()` time: keeping the call site in the
  adapter is what makes it interceptable at all without touching that file.

- **`FactorizeFaultInjector` (MKL) is faithful in ONE scenario only: a
  session that has never previously factorized successfully.** The injector
  SKIPS the real `session_->factorize(A)` call entirely rather than
  overriding its result, so the session's own `has_numerics_`/`epoch_` (both
  owned by the MPL-derived `FactorSession`, never touched by this hook) are
  left exactly as they were. On a session that has never succeeded, that
  means "still never succeeded" — indistinguishable from a real first-time
  failure for every observable the coverage test asserts (no epoch bump,
  `kUnavailable` inertia, refused solves/share). On a session that HAS
  previously succeeded, the same trick would be dishonest: the real session
  would still report `has_numerics() == true` underneath the fabricated
  failure, which does not match what a real refactorize failure does (real
  Pardiso invalidates `has_numerics_` *before* attempting phase 22,
  regardless of outcome). Testing that deeper "succeeded, then a later
  factorize fails" scenario faithfully would require either a hook inside
  the MPL session file or a fully virtual session interface — both
  considered and rejected (see "Alternatives considered" below). It stays
  **inspection-only**, exactly as task-5-report.md's I1 finding already
  disclosed; the code comment at `FactorizeFaultInjector`'s declaration
  states this scope limit explicitly so a future editor does not assume more
  coverage than exists.

## How to use it for a new fault path

1. Confirm the fault path lives in an adapter file (Apache-2.0), not a
   session file (MPL-2.0). If the failure can only be reached from inside
   the session, this convention does not apply as-is — raise it rather than
   improvising a session-side hook.
2. Add a small `static inline`-member struct to `fault_injection.h`, guarded
   by `#ifdef HVEN_TESTING`, following the two existing ones' shape.
3. Wrap the ONE call site with the same `#ifdef HVEN_TESTING` / `active`
   check pattern shown above. Keep the branch as small as the two existing
   ones — a local variable substitution, not a re-implementation of the
   surrounding logic.
4. Add the test to `tests/linear/test_fault_injection.cpp` (or a sibling
   file compiled into the same target), gated by whichever platform macro
   selects the right backend (`#if !defined(__APPLE__)` / `#if
   defined(__APPLE__)` — the same guard the two existing halves of that file
   use).
5. State explicitly, in both the injector's doc comment and the test, what
   scenario the injection is and is NOT faithful for. An injector that skips
   a real call is faithful only where the skipped call's absence is
   indistinguishable from a real failure on every observable being asserted
   — work that out per fault path, do not assume it transfers.

## Alternatives considered and rejected

- **A CMake test-only define recompiling the MPL session TU itself**, with
  `#ifdef HVEN_TESTING` hooks inside `pardiso_session.cpp` /
  `accelerate_session.cpp`. Rejected outright: adds a test hook to an
  MPL-derived file, which this repository's governance forbids regardless of
  how the recompilation is wired.
- **A fully virtual `FactorSession` interface**, letting tests substitute an
  entirely fake session implementing the same interface. This WOULD let the
  MKL "succeeded, then fails" scenario be tested faithfully (the fake
  controls 100% of the session's observable state, including
  `has_numerics()`/`epoch()`). Rejected for Task 6 because it adds permanent
  virtual-dispatch overhead to a call path that is otherwise a direct,
  non-polymorphic call in every build (production and test alike), for a
  gap that is already disclosed and bounded rather than silently wrong,
  and because it would make the two backend adapter files share
  significantly more structure than `src/CMakeLists.txt`'s "the sparse
  engines share no code at all" design currently commits to. If a future
  task needs the deeper scenario tested (or needs the virtual boundary for
  an unrelated reason — e.g. actual runtime backend selection), revisit
  this decision then rather than paying the cost speculatively now.
- **Overriding just the adapter's `backend_code` local after a real,
  successful backend call**, without skipping the call. Rejected: creates an
  inconsistent state (the outcome reports failure while the session's own
  `has_numerics()`/`epoch()` still reflect the real success underneath),
  which would make a "passing" test prove nothing about the real
  invalidation contract. This is exactly why `FactorizeFaultInjector` skips
  the call instead of overriding its result.

## Guard tests: the other half of this repo's cross-backend coverage

Fault injection covers paths no real backend takes. The complementary need —
confirming the evidence-honesty invariants hold against whichever backend a
given build actually links — is covered separately, by
`tests/linear/test_symmetric_factor_evidence_invariants.cpp`: written once,
compiled and run on BOTH platforms (unlike `test_symmetric_factor.cpp`,
which is MKL-specific and gated `if(NOT APPLE)`), asserting properties that
hold by construction regardless of which backend answers them (a present
`perturbed_pivots` is never negative; an absent one implies
`supports_partial_solve() == false`; `zero_is_derived` matches the
per-backend semantics table). The one assertion that needs to know which
backend it is running against uses the standard `__APPLE__` predefined
macro rather than a hand-rolled build option, so there is nothing beyond
`src/CMakeLists.txt`'s own platform split to keep in sync.
