#The test - seam convention

hven has two sparse backends(MKL Pardiso, Apple Accelerate),
    and both have at least one fault path a real backend cannot be made to take from any fixture
    this repository can build : MKL will not genuinely refuse a symmetric indefinite `factorize()` (
                                    static pivot perturbation gets through everything tried),
    and Accelerate's inertia query has no known input that provokes a failure independent of a
        successful factorization.Both paths matter — the frozen
        contract's fabrication fixes (`docs / dev / plans / 2026 - 08 - 08 - hven - m1 - linear -
                                      and-rig - spec.md` A.5) exist
        specifically to make these paths honest — so they need executable coverage,
    not just inspection.This page is the ONE place the convention that covers them is designed; every backend/component that needs a fault-injection seam
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
- `PardisoIparmObserver` (MKL) — `last_ordering_iparm`,
  `last_weighted_matching_iparm`. NOT a fault injector — see "A read-only
  variant" below.

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
- does * *
    not **link `hven::hven`
              .

          Not linking `hven::hven` is not just tidiness — it is
              required.Both this target 's freshly-compiled adapter object and `libhven.a`' s own
                  define `hven::linear::SymmetricFactor`'s and `Factorization`' s methods; linking
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
  **inspection-only**, exactly as the in-code factorize-failure disclosure already
  disclosed; the code comment at `FactorizeFaultInjector`'s declaration
  states this scope limit explicitly so a future editor does not assume more
  coverage than exists.

## A read-only variant: observing internal state instead of injecting a fault

Not every gap this seam closes is a fault path. `PardisoIparmObserver`
(added for the M1 ordering/weighted_matching amendment,
the write-nothing-by-default review finding) covers a different kind of untestable claim: a
guarded write inside the MPL-derived session file
(`FactorSession::analyze`'s `if (cfg_.ordering.has_value()) iparm_[1] = …`
and its `weighted_matching` twin) that behaves identically whether the
guard is present or entirely deleted, for every observable a normal
plumbing test can reach — a solve is correct and counters read 1/1/1 either
way. The guard's own correctness was previously guaranteed by inspection
only, exactly like `FactorizeFaultInjector`'s deeper scenario above.

The fix is structurally the same seam, used for observation rather than
injection: `SymmetricFactor::analyze()` (MKL adapter), under
`#ifdef HVEN_TESTING`, records what `FactorSession::ordering_iparm()` /
`weighted_matching_iparm()` actually returned right after a REAL,
unmodified `analyze()` call — those two accessors are themselves ordinary,
unconditional, non-test-gated `FactorSession` API (like `num_pos_eigs()`
and friends), so adding them is production surface, not a test hook; only
the act of recording their result into `PardisoIparmObserver` is
`HVEN_TESTING`-gated. `tests/linear/test_fault_injection.cpp`'s
`PardisoIparmObservation` suite then asserts:

- at default `Options`, the recorded values equal a FRESH, independent
  `pardisoinit()` call made directly in the test (never a literal
  constant — see the test file's own comment on why hven's actual MKL
  version currently makes `iparm[1]`'s default coincide with one of the
  two non-default `Ordering` values, which would make a naive "assert
  != some literal" check meaningless on this box);
- at each non-default value, the recorded value equals the amendment's own
  fixed contract constant (2 / 3 / 1), which is a spec commitment, not a
  version-fragile assumption, so a literal is correct there.

Manually verified once, not asserted by CI: deleting either `if` guard in
`pardiso_session.cpp` makes the corresponding default-case
`PardisoIparmObservation` test fail while every other test in the suite
(including the plumbing tests in `test_symmetric_factor.cpp`) keeps
passing — confirming this observer, and only this observer, is what gives
the don't-write-by-default rule executable teeth.

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
BUILT to compile and run on both platforms (unlike
`test_symmetric_factor.cpp`, which is MKL-specific and gated
`if(NOT APPLE)`). Execution status: on the only pass that has run —
Linux/MKL — only the non-Apple half of each platform-split test file
compiled and executed; the Accelerate halves (including the Accelerate
inertia-query fault-injection test) are syntax-checked against stubs only
and first EXECUTE on the Mac hardware leg. The file asserts properties that
hold by construction regardless of which backend answers them (a present
`perturbed_pivots` is never negative; an absent one implies
`supports_partial_solve() == false`; `zero_is_derived` matches the
per-backend semantics table). The one assertion that needs to know which
backend it is running against uses the standard `__APPLE__` predefined
macro rather than a hand-rolled build option, so there is nothing beyond
`src/CMakeLists.txt`'s own platform split to keep in sync.

A second unconditionally-compiled, internally platform-split file follows
the same convention:
`tests/linear/test_symmetric_factor_pardiso_only_options.cpp` asserts the
Accelerate throw path for `Options::ordering` / `Options::weighted_matching`
(non-default values THROW `std::invalid_argument` at construction, per the
M1 amendment's Accelerate semantics) under `#if defined(__APPLE__)`, and the
inverse guard — the exact same non-default values do NOT throw on the MKL
platform, since there they are real Pardiso options — under `#else`. Like
`test_symmetric_factor_evidence_invariants.cpp`, its Apple half rides the
Accelerate syntax-check lane (`scripts/check_accelerate_syntax_linux.sh`)
rather than executing on this Linux-only development pass.

## The golden-numerics rig

`tests/golden_rig/` is the instrument that gates both engine migrations. It
runs a fixed set of traces — each pinning one clause of the frozen linear
interface contract — against three *seams*: `hven::linear` itself, and the two
seams it replaces, each driven through one abstract `SeamUnderTest` surface.
Its purpose is to prove old-vs-new reproduction on the numerics the engines
already trust, so the numbers come from observing the trusted thing rather
than from asserting about the new one.

### Running it

The **native arms need nothing but this repository** and run in `ctest` on
every build, so the harness, the recipes, the comparison engine and the audit
tooling are all exercised continuously:

```bash
cmake --preset linux-clang-release
cd build && ninja && ctest --output-on-failure
```

The **three-seam run is a local/derivation activity**. Both old-seam adapters
are behind CMake path options, and the SQP one is pinned to a tag whose commit
is verified at configure time:

```bash
cmake -S . -B build-3seam -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DHVEN_RIG_PSIOPT_SEAM=/path/to/tycho \
    -DHVEN_RIG_SQP_SEAM=/path/to/tycho_sqp
cmake --build build-3seam
MKL_NUM_THREADS=1 ctest --test-dir build-3seam --output-on-failure
```

`MKL_NUM_THREADS=1` matters and is not decoration. The comparison policy
requires every asserted run to pin threads to one by the mechanism the seam
under test possesses; the native arms do that per-instance, but neither old
seam has any thread control at all, so the rig pins the process for them (an
in-process guard, which the environment variable backs up for the window
before the backend first reads it). Every recorded row carries the mechanism
and the value it pinned to.

### The report target

`hven_golden_rig_report` runs the same traces in report mode and dumps
observed-vs-expected. Its "observations" block emits each observation as a row
in the committed tables' own CSV format, provenance columns already filled from
the run, so deriving a table is a copy rather than a transcription:

```bash
HVEN_RIG_MACHINE="<a stable name for this machine>" \
HVEN_RIG_REPORT_OUT=rig-report.txt \
MKL_NUM_THREADS=1 ./build-3seam/tests/golden_rig/hven_golden_rig_report
```

Setting `HVEN_RIG_MACHINE` is optional (the run reads the host's own name
otherwise) but worth doing for a derivation run, since that string is what
every derived row's provenance carries.

### Expected tables

One committed CSV per trace, at `tests/golden_rig/expected/<trace>.csv`:
metadata comment lines, then a fixed header row, then one row per (arm,
quantity). The reader enforces the comparison policy rather than leaving each
trace to remember it:

- a row carrying an observed value must carry its full provenance **including
  the thread-pin mechanism and value** — a row without a thread pin is refused
  and the file fails to load;
- a bitwise / 0-ULP kind is **refused in a table**, because bitwise equality
  holds only between two observations of one pinned-thread process run; a trace
  needing that property asserts it against its own second observation instead;
- a float row must state a tolerance;
- the literal `UNOBSERVED` is a legal value meaning nobody has run this arm
  yet. It never compares equal to anything. Every slot for a backend arm this
  project has no hardware for ships as `UNOBSERVED` and is filled only from a
  hardware run.

Trace matrices are **regenerated from recipes** (`tests/golden_rig/recipes.h`),
never copied from either sibling checkout and never read from a file. Each
recipe's provenance string says what it does and does not reproduce from the
fixture its authority names, and the report prints that string beside every
observation.

### Capabilities, and why a trace skips

An old seam does not have everything the unified surface has — no phase-split
solve on one of them, no co-owning handle or epoch on either. Those gaps are
declared through `Capabilities`, and a trace needing a capability an arm lacks
**skips on that arm, naming what is missing**. Nothing is emulated: emulating a
missing operation would make the rig prove a property of the rig.

### The consumed-surface audit

Two halves, in `tests/golden_rig/audit/`:

- **Static.** `static_scan.sh <root> [...]` emits one CSV row per
  backend touchpoint (parameter reads and writes, phase-entry calls, Accelerate
  calls, dense LAPACK calls, thread-control calls) found in a source tree. Its
  self-test runs the scanner over a committed sample carrying one line of every
  touchpoint class plus two negative controls, so it checks the scanner on a
  machine with neither sibling checkout, and is registered with ctest.
- **Runtime.** `hven_golden_rig_audit` links a *link-level interposer* on the
  backend's phase entry point (via the linker's `--wrap`), so every call from
  hven's own session and from either old seam alike is recorded with the
  parameter array as it stood at that moment. It forwards to the real symbol
  unchanged; it only watches.

The runtime half exists because a static pass reports what the source
*mentions*, not what a run *executes*. Its **pre-registered coverage test** is
the guard on the audit's method: the instrument must find, *without being told
which entry to watch*, the correctness rule that forces the
iterative-refinement cap to zero around a phase-split solve and restores it
afterwards. That rule is not an option and no scan of an option surface would
produce it. The detector reports which parameter indices changed value between
calls, and the test asserts the refinement cap is among them — once against
this library's own seam (so it runs in ordinary CI) and once against the old
seam the rule was originally read out of (when that checkout is configured in).
If the instrument ever stops finding it, the audit has a demonstrated coverage
gap and its method is fixed before its output is trusted.

### Temporary by construction

`seam_psiopt.cpp` and `seam_sqp.cpp` are test-only and are **deleted when the
two engine migrations close**. The traces stay, as permanent regression tests
against the native arm.
