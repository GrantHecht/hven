# The test-seam convention

hven has two sparse backends (MKL Pardiso, Apple Accelerate), and both
have at least one fault path a real backend cannot be made to take from
any fixture this repository can build: MKL will not genuinely refuse a
symmetric indefinite `factorize()` (static pivot perturbation gets
through everything tried), and Accelerate's inertia query has no known
input that provokes a failure independent of a successful factorization.
Both paths matter — the per-backend evidence contract exists specifically
to make these paths honest — so they need executable coverage, not just
inspection. This page is the ONE place the convention that covers them is
designed; every backend/component that needs a fault-injection seam
should use it rather than inventing another.

## Where instrumentation goes: a preference, with an escape

Both backend session implementations
(`hven/detail/linear/pardiso_session.h`/`.cpp`,
`hven/detail/linear/accelerate_session.h`/`.cpp`) are MPL-2.0-derived files
(from Eigen's PardisoSupport and AccelerateSupport modules respectively —
see the notice at the top of each). The two backend ADAPTER files that own the
contract logic AROUND each session (`src/linear/symmetric_factor_mkl.cpp`,
`src/linear/symmetric_factor_accelerate.cpp`) are ordinary Apache-2.0 hven
files.

**Prefer boundary instrumentation. Deviate when the work needs it, and say
why.** The default is the adapter boundary, for two reasons that are about
engineering rather than licensing: an observation taken there is one a
consumer could in principle have made too, and a derived file stays a clean
diff against its upstream. A hook inside a session file is a deviation that
has to earn itself — but it is a deviation, not a violation. Where the fact
being observed leaves no trace outside the function that produces it, the
boundary has nothing to watch and refusing to look would trade a real
coverage gap for a tidy file.

The rule for deviating is the same rule as for everything else here: do it
where the work needs it, keep it as small as it can be, prove it costs the
production build nothing, and write down why the boundary could not carry it.

### The sanctioned deviations, in full

**One, today.** `PardisoIparmObserver`'s did-the-write-execute fields, recorded
at the two guarded parameter-array writes inside `FactorSession::analyze`
(`src/linear/pardiso_session.cpp`).

*Why the boundary cannot carry it.* The claim under test is
don't-write-by-default: at `Options::ordering == kBackendDefault`, hven must
not touch `iparm[1]` at all. The boundary can only read the array afterwards —
and on some MKL versions `pardisoinit`'s own default for that entry equals one
of the three non-default values `Ordering` can request (3, on the version this
repository currently links). On such a version "left it alone" and "wrote
exactly that value" produce identical arrays, so no after-the-fact read can
distinguish them and the rule falls back to inspecting an `if`. The fact that
is missing is whether a statement *executed*, and a statement that did not
execute leaves its trace nowhere but where it isn't.

*What it costs.* Two `#ifdef HVEN_TESTING` lines per write site. Verified, not
asserted: compiling `pardiso_session.cpp` with the production flags before and
after the change yields a **byte-identical object file**, and `nm` finds no
observer symbol anywhere in `libhven.a`.

*What it buys.* The default-case test asserts `ordering_was_written == false`
— a version-independent claim no backend default can make vacuous — and the
non-default tests assert `true` plus the value, so the false assertion cannot
pass for want of a live observable. Mutation-checked: inverting the guard in
`pardiso_session.cpp` makes the default-case test fail on exactly that
assertion **while every value-level assertion still passes**, which is the gap
in one line.

The `notices/eigen-mpl2.txt` entry for that file records the modification.

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
- does **not** link `hven::hven`.

Not linking `hven::hven` is not just tidiness — it is required. Both this
target's freshly-compiled adapter object and `libhven.a`'s own object
define `hven::linear::SymmetricFactor`'s and `Factorization`'s methods;
linking both into one binary would be a duplicate-symbol error. The consequence is
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
(added for the ordering/weighted_matching write-nothing-by-default review
finding) covers a different kind of untestable claim: a
guarded write inside the MPL-derived session file
(`FactorSession::analyze`'s `if (cfg_.ordering.has_value()) iparm_[1] = …`
and its `weighted_matching` twin) that behaves identically whether the
guard is present or entirely deleted, for every observable a normal
plumbing test can reach — a solve is correct and counters read 1/1/1 either
way. The guard's own correctness was previously guaranteed by inspection
only, exactly like `FactorizeFaultInjector`'s deeper scenario above.

The fix is structurally the same seam, used for observation rather than
injection, in two layers. At the BOUNDARY: `SymmetricFactor::analyze()` (MKL
adapter), under `#ifdef HVEN_TESTING`, records what
`FactorSession::ordering_iparm()` / `weighted_matching_iparm()` actually
returned right after a REAL, unmodified `analyze()` call — those two accessors are themselves ordinary,
unconditional, non-test-gated `FactorSession` API (like `num_pos_eigs()`
and friends), so adding them is production surface, not a test hook; only
the act of recording their result into `PardisoIparmObserver` is
`HVEN_TESTING`-gated. `tests/linear/test_fault_injection.cpp`'s
`PardisoIparmObservation` suite then asserts:

- at default `Options`, the recorded values equal a FRESH, independent
  `pardisoinit()` call made directly in the test (never a literal
  constant — see the test file's own comment on why hven's actual MKL
  version currently makes `iparm[1]`'s default coincide with one of the
  three non-default `Ordering` values, which would make a naive "assert
  != some literal" check meaningless on this box);
- at each non-default value, the recorded value equals the amendment's own
  fixed contract constant (`Ordering`: 0 / 2 / 3; `weighted_matching`: 1),
  which is a spec commitment, not a version-fragile assumption, so a literal
  is correct there.

And at the WRITE SITE, for the half the boundary cannot reach: the
`*_was_written` / `*_written_value` fields described under "the sanctioned
deviations" above. The default case asserts both flags FALSE — the write did
not execute — and each non-default case asserts its own flag TRUE with the
value, so the FALSE assertions cannot pass for want of an observable. This is
what makes the ordering half's coverage version-independent instead of
contingent on the linked MKL's `pardisoinit` default differing from the
option's value.

Mutation-checked: inverting either `if` guard in `pardiso_session.cpp` makes
the corresponding default-case `PardisoIparmObservation` assertion fail while
every other test in the suite (including the plumbing tests in
`test_symmetric_factor.cpp`) keeps passing — confirming this observer, and
only this observer, is what gives the don't-write-by-default rule executable
teeth. On the MKL currently linked, the assertion that catches it is the
`*_was_written` one and not the value comparison, which is precisely why that
field exists.

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

- **Putting the two FAULT INJECTORS inside the session files** rather than at
  the adapter boundary. Rejected — not on principle, but because the boundary
  carries them perfectly well: both injectors substitute the result of a call
  the adapter itself makes, so nothing is lost by intercepting it one frame
  out, and the derived files stay clean diffs against upstream. That reasoning
  is what makes the boundary the preference; it is also what makes the
  did-the-write-execute observable a genuine exception rather than a
  precedent-by-erosion, since there the boundary demonstrably cannot see the
  fact at all.
- **A fully virtual `FactorSession` interface**, letting tests substitute an
  entirely fake session implementing the same interface. This WOULD let the
  MKL "succeeded, then fails" scenario be tested faithfully (the fake
  controls 100% of the session's observable state, including
  `has_numerics()`/`epoch()`). Rejected because it adds permanent
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
`if(NOT APPLE)`). Linux and Windows CI compile and execute the MKL halves of
the platform-split tests; macOS CI compiles and executes their Accelerate
halves against the real framework, including the Accelerate inertia-query
fault-injection test. The Linux stub lane remains an additional syntax-only
check of the Apple halves. The file asserts properties that hold by
construction regardless of which backend answers them (a present
`perturbed_pivots` is never negative; an absent one implies
`supports_partial_solve() == false`; `zero_is_derived` matches the
per-backend semantics table). The one assertion that needs to know which
backend it is running against uses the standard `__APPLE__` predefined
macro rather than a hand-rolled build option, so there is nothing beyond
`src/CMakeLists.txt`'s own platform split to keep in sync.

A second unconditionally-compiled, internally platform-split file follows
the same convention:
`tests/linear/test_symmetric_factor_pardiso_only_options.cpp` asserts the
construction-time behavior of the two options this file is named for, which
now diverges between them per the backend-neutral ordering mapping
(`hven/linear/symmetric_factor.h`'s own doc comment on `Options::ordering`):

- `Options::weighted_matching`: still Pardiso-only. A non-default value
  (`true`) THROWS `std::invalid_argument` at construction on Accelerate,
  which has no matching analogue, under `#if defined(__APPLE__)`; the
  inverse guard — the same value does NOT throw on the MKL platform, where
  it is a real Pardiso option — runs under `#else`.
- `Options::ordering`: NO LONGER Pardiso-only. Every `Ordering` value now
  maps onto a real Accelerate order method (`kBackendDefault` ->
  `SparseOrderDefault`, `kMinimumDegree` -> `SparseOrderAMD`,
  `kNestedDissection` -> `SparseOrderMetis`, `kParallelNestedDissection` ->
  `SparseOrderMTMetis` with an OS-availability downgrade to
  `SparseOrderMetis` on a host that lacks it), so the Apple half now asserts
  ALL FOUR values are ACCEPTED (never throw) at construction — the mapping
  itself is resolved later, inside `accelerate_ordering_code()`
  (`src/linear/symmetric_factor_accelerate.cpp`) at `analyze()` time, not at
  construction. The MKL-side `#else` half already accepted every non-default
  value (real Pardiso options), and now additionally covers
  `kMinimumDegree`.

Like `test_symmetric_factor_evidence_invariants.cpp`, its Apple half rides
the Accelerate syntax-check lane (`scripts/check_accelerate_syntax_linux.sh`)
and also compiles and executes against the real framework in macOS CI.

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

`MKL_NUM_THREADS=1` matters and is not decoration. Every asserted run pins
threads to one by the mechanism the seam under test possesses; the native arms
do that per-instance **on MKL**, but neither old seam has any thread control at
all, so the rig pins the process for them (an in-process guard, which the
environment variable backs up for the window before the backend first reads
it). Every recorded row carries the mechanism and the value it pinned to.

**On Accelerate the native arm pins nothing, and records that.** hven's
Accelerate session stores `Options::num_threads` so `adopt()` can round-trip
the options faithfully and applies it to no backend call — the surface calls
per-instance thread control best-effort-absent on this backend rather than
fabricating one. The arm therefore reports mechanism `absent`, value `0`, and
every `native@accelerate` row in `expected/` carries that pair. The reader
still compares a float row only against a run whose own mechanism matches, so
an unpinned row never stands in for a pinned one; what replaces the pin's
protection against thread-count nondeterminism is the requirement that
Accelerate rows reproduce byte-identically across two separate CI runs before
being committed (see the `accelerate-derivation` banner in each table).

One leg deliberately escapes that pin: T8's unasserted smoke leg, whose whole
subject is what happens at more than one thread. It requests an explicit count
above one, which overrides the environment setting either way — **on the native
arms through the seam's own per-instance control (MKL; on Accelerate the count
is stored and applied to nothing, as above); on the old-seam arms through
the rig's process pin, since those seams have no control of their own** — so
the trace measures something real under the invocation above rather than
comparing a run against itself. No other leg does this, and none of T8's
asserted rows come from it.

The three quantities that leg produces (the deviation, the bitwise flag, the
count it used) are emitted with kind `record-only`: each describes a comparison
BETWEEN two thread settings, so no single thread pin describes it, and the
expected-table reader **refuses that kind outright**. Stamped with the asserted
leg's pin they would have read as ordinary pinned rows, and a derivation
following the copy-the-report-rows workflow would have committed a
run-to-run-nondeterministic number at a tight tolerance with the pin-refusal
unable to catch it. They are printed in their own clearly-labelled block of the
report, outside the paste-me section, and carry the smoke leg's own mechanism
and count so the artifact says what was actually measured.

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
  hardware run;
- a table carrying a committed (non-`UNOBSERVED`) float row must declare the
  build configuration(s) that row is pinned to, in a `# build-config:` metadata
  line — a table with a committed float row and no such line is **refused at
  load**, the same way a row without a thread pin is.

**A float expectation is context-pinned three ways, and is asserted only when
all three match: machine, build configuration, and thread pin.** These are
exactly the row's own provenance columns (machine and thread pin, per row) plus
the table's `# build-config:` declaration (build configuration, per table,
since every row in one committed table comes from one derivation run under one
configuration). On a mismatch of any of the three, the row is **not asserted**:
it is reported as `CONTEXT-MISMATCH`, with both the pinned and the observed
context named, and counted in the report's summary — never silently dropped
and never treated as a pass. Counters, states, presence and bool rows carry
none of this risk (an integer count or a named state does not drift with the
machine, compiler, or thread count underneath it) and are asserted
unconditionally, on every machine, in every build configuration, exactly as
before. In practice this means: a CI run on a machine other than the one a
table was derived on — the common case for every lane except the one that did
the deriving — still asserts every counter, state, presence, and bool row
exactly as strictly as ever, and reports (without asserting) the float rows,
which is the correct outcome for a value this project has never claimed is
portable across microarchitectures in the first place.

### What a row has to survive before it is committed

Deriving a table is a copy, but not every emitted row is copyable. Two
gates, both learned from the first derivation (2026-08-09) rather than
assumed:

1. **Run to run.** Run the report twice and commit only what reproduced.
   This is the thread pin's proof of work: an unpinned float at nine
   digits is run-to-run nondeterministic, and if the pin is doing its job
   the two runs are identical. On the first derivation they were —
   byte-for-byte, all 1145 rows, including the cross-thread record-only
   block.
2. **Across build configurations.** Run the report from a Release build
   and a Debug build of the same source, at the same thread pin on the
   same machine, and record which rows move. **103 float rows do** — all
   of them solution components and residuals on the collocation-class
   fixtures. Every counter, state, boolean and presence reproduced
   exactly.
3. **Pinned to a context future runs will report.** A row's pin has to
   name the context the runs you want it to assert under will actually
   produce, or it asserts nowhere.

The second gate is the surprising one. Note that only 35 of those 103
actually exceeded their stated tolerance; the other 68 stayed inside it by
luck, which is exactly why the criterion is "did it move", not "did it
fail" — a tolerance-based rule would have committed the lucky ones as
though they were reproducible.

**How the 103 are handled: by narrowing the declared context, never by
widening a tolerance.** A table's `# build-config:` line is table-wide, so
a file containing any Release-only row declares `Release` alone, and the
reader refuses to assert ANY of its float rows under another configuration
— reported as `CONTEXT-MISMATCH`, never silently compared. Seven tables
(P1, T1, T2, T3, T4, T4b, T8) are in that position; the other nine declare
`Release, Debug` because they were shown under both, and narrowing them
too would discard real evidence for the sake of uniformity.

That trade is worth stating in numbers rather than leaving to be
discovered. Pinning those seven to Release alone costs a Debug run its
assertion of **114** float observations in those files (they are reported
instead), and buys **103** rows that previously asserted nowhere at all.
Release is where the derivation and CI both run; Debug still asserts every
counter, state, presence and bool, which is the half of each trace that
carries its structural claim.

**The third gate was learned the hard way, and it is the cheapest one to
get wrong.** The first derivation set `HVEN_RIG_MACHINE` to a hand-chosen
label. That pinned every float row to a string an ordinary `ctest` run
never produces, so the context gate refused all 198 of them on the very
machine they were derived on — the tables looked fully derived and asserted
nothing. Derive with `HVEN_RIG_MACHINE` **unset**, so the machine column
carries what the harness reports on its own. Set it to make a one-off
report easier to read; do not set it when deriving, unless the string you
choose is exactly what the runs you want to assert will report. The same
rule is what makes a CI-derived row work: pin it to the runner CLASS the
job will keep reporting, never to a hostname and never to a moving alias.

A row held back under any of these gates is listed by name in its table's
banner and left `UNOBSERVED`, so a run against it is reported as an
unfilled slot rather than passing silently. No row is held for
run-to-run or cross-run movement today.

**Five rows are held for a fourth reason, worth knowing before deriving on
a new backend: the build-configuration declaration is table-wide, so a
value can be blocked by an arm it has nothing to do with.** Every
Accelerate observation this project has comes from the macOS CI lane, and
there is no macOS Debug lane, so all of them are Release-only. In a table
pinned to Release that is fine. In a table declaring `Release, Debug` — a
claim earned by its MKL rows — an Accelerate float would inherit a Debug
reproduction nobody ran. Those five (`native@accelerate` floats in P6 and
T7) stay `UNOBSERVED`. Narrowing the two files to Release instead would
cost their MKL rows 24 Debug float assertions to gain 5, so the residue is
cheaper than the fix. What closes it properly is a per-arm build
configuration, or a Mac Debug observation — never inheriting the claim.

Trace matrices are **regenerated from recipes** (`tests/golden_rig/recipes.h`),
never copied from either sibling checkout and never read from a file. Each
recipe's provenance string says what it does and does not reproduce from the
fixture its authority names, and the report prints that string beside every
observation.

### Traces that fail by design

Two traces assert the unified surface's honesty rules against seams that are
known to break them, so a FAILURE on an old-seam arm is the finding rather than
a defect: it is what a docket entry gets written from. The adapters carry each
seam's real behaviour verbatim precisely so that this can happen — an adapter
that answered the way the new surface would answer would make the failure
silently never occur, which is the one way this rig could look healthy while
proving nothing.

As things stand, `P5` fails on the SQP old seam (it answers a real-looking
zero-filled inertia triple before anything is factorized) and is expected to
fail on the interior-point old seam's Apple arm for the same reason. It passes
on the interior-point seam under MKL, honestly: the counts are genuinely
indeterminate there, so its adapter reports the absence of a defined state
rather than inventing one.

**A fail-by-design needs a control, and has one.** The arrangement has its own
quiet failure mode: if an adapter regresses to smoothing, the trace stops
failing, the suite goes green, and nothing says the finding was lost.
`fail_by_design_control.cpp` asserts the OPPOSITE of what those traces assert —
that the old seam really does produce the dishonest evidence, in the documented
shape — so a regression is loud in one direction or the other and cannot pass
silently. Mutation-checked: restoring the smoothing guard in the SQP adapter
turns `P5` fully green while the control goes red.

**A trace NOT meant to be fail-by-design almost became a third one, and the
near-miss is worth keeping visible.** `T7_BackendParameterSurfaceFloor`
(`traces_sqp.cpp`) is the backend-parameter GOVERNANCE floor, not a
fail-by-design trace — but for one CI review cycle its Apple branch asserted
`perturbed_pivots` absence with no arm gate at all, which would have failed
against the psiopt old seam's Apple arm too (that seam's `evidence()`
fabricates a present `perturbed_pivots` unconditionally, on every arm that
reads it, the exact same fact P5's control below already names). Fixed by
scoping the absence assertion to the `native` arm only
(`traces_sqp.cpp`'s `GetParam().seam == SeamId::kNative` guard): T7 keeps
recording every arm's actual value, but only asserts the contract-honesty
claim about the one arm it is actually a floor for. No new
`FailByDesignControl.*` test was added for this, because none was needed
this time — the fact T7's stray branch would have (re)detected is already
named and guarded by
`FailByDesignControl.PsioptSeamStillZeroFillsItsPreFactorizationInertiaOnApple`
below; a second control asserting the identical `ppivs()` fact through a
different call path would duplicate coverage rather than add any.

**A KNOWN, UNRESOLVED gap of the same shape: `P4_PerturbationEvidencePresenceIsBackendHonest`
(`traces_psiopt.cpp`).** Unlike T7, P4 was not touched by this fix, and it
carries the identical unscoped shape T7's stray branch had: an `#if
defined(__APPLE__)` `EXPECT_FALSE(perturbed_pivots.has_value())` with no arm
gate, `RIG_REQUIRE`d only on `reports_inertia` (which the psiopt old seam
declares `true`). A real Mac three-seam run should therefore be expected to
show P4 ALSO failing on `psiopt-old@accelerate`, for the same reason P5
does — this is inferred from reading the code (see below for what is and is
not actually observed), not yet confirmed against real hardware, and it is
called out here explicitly rather than silently left for whoever runs the
three-seam configuration on a Mac to discover unwarned. Closing it (arm
scope or a third named fail-by-design entry with its own control, matching
whichever shape a maintainer picks) is future work, not part of this fix.

**Derivation checklist — the three-seam run MUST NOT be all-green.** The
expected result is:

```
99% tests passed, 1 tests failed out of 153
    Arms/PsioptTrace.P5_InertiaBeforeFactorizationIsAnExplicitState/sqp-old@mkl
```

One failure, that exact entry, with all three `FailByDesignControl.*` tests
green — ON LINUX. Independently re-verified against a real three-seam Linux
build while writing this correction
(`-DHVEN_RIG_PSIOPT_SEAM=<tycho> -DHVEN_RIG_SQP_SEAM=<tycho_sqp>`, tag
`phase-7-close`): exactly this one failure, 3 `FailByDesignControl.*` tests
green.

**The total moved from 151 to 153 when the `kMinimumDegree` ordering
amendment landed**, and the two it added
(`SymmetricFactor.MinimumDegreeOrderingFactorizesAndSolvesCorrectly`,
`PardisoIparmObservation.MinimumDegreeOrderingWritesExactly0`) are both
MKL-only, so the Mac projection below is unaffected — confirmed against a
real macOS CI run at the same commit, which is still 51. Re-derived rather
than adjusted by arithmetic: the rig's own `Arms/*` count did not change,
which is why the failure and control expectations are untouched. A total
that drifts silently would make this checklist worse than useless, since
its whole job is to say when a run has one failure too many.

**The Mac shape was never actually derived — it was guessed, and the guess
was wrong on the total, not just the failure count.** The `152` figure
(`151` from Linux, `+1` for the extra failure) implicitly assumed the Mac
three-seam run has roughly the same shape as Linux's; it does not. Recomputed
here from two pieces of REAL evidence — the actual native-only
`macos-clang-release` CI green run (`docs/ci.md`, 51 tests: `Arms` 16,
non-`Arms` 35) plus the real three-seam Linux run above (which shows exactly
what an added old-seam arm contributes: 16 `Arms/*` cases per arm, whether or
not some of them skip on it) — not from a real Mac three-seam observation,
which still does not exist:

- The psiopt old seam is the ONLY old seam that can ever exist on Apple (the
  SQP old seam's adapter `#include`s `<mkl_service.h>` unconditionally and
  cannot compile there at all), and neither parity arm exists on Apple
  either (`seam_registry.cpp`'s `#if !defined(__APPLE__)` gate) — so a Mac
  three-seam run has exactly two arms, `native` and `psiopt-old`, not five.
- `psiopt-old` contributes 16 `Arms/*` cases (same per-arm count as every
  other arm), 3 of which skip on it regardless of platform
  (`T2b_PartialSolvePredicateUnderPerturbation`, `T4_HandleOutlivesItsEmitter`,
  `T4b_AdoptRefusesStaleNumerics` — `seam_psiopt.cpp`'s
  `partial_solve_predicate`/`share_handle`/`epoch`/`adopt` capabilities are
  all unconditionally `false`, not backend-split), still counted in the
  total.
- `FailByDesignControl.PsioptSeamStillZeroFillsItsPreFactorizationInertiaOnApple`
  (currently gated `#if defined(HVEN_RIG_HAVE_PSIOPT_SEAM) && defined(__APPLE__)`,
  so absent from every count observed so far) becomes real: `+1`.
- No other file gates a test on `HVEN_RIG_HAVE_PSIOPT_SEAM` (checked by
  grep, not assumed).

`51 + 16 + 1 = 68` — not `152`. The expected shape, corrected:

- Linux three-seam run: 153 tests, exactly 1 failure
  (`P5`/`sqp-old@mkl`), 3 controls green. Independently re-verified above.
- Mac three-seam run (when the psiopt adapter's Apple arm exists): **68**
  tests (not 152), exactly 1 expected failure — `P5` on the interior-point
  old seam's Apple arm, its defined zero-fill (T7 no longer contributes one,
  per the fix above) — 2 controls green
  (`ControlsArePresentForWhicheverOldSeamsThisBuildHas` and
  `PsioptSeamStillZeroFillsItsPreFactorizationInertiaOnApple`; there is no
  Mac equivalent of the SQP-seam controls, since that seam never exists
  there). **This count does not yet account for the P4 gap named above** —
  if a real run shows P4 also failing on `psiopt-old@accelerate` before that
  gap is closed, that is expected given the code as it stands today, not a
  new problem; the count to compare against at that point is 2 failures
  (`P4` + `P5`), 68 tests, same 2 controls.

This corrected Mac shape is itself still UNOBSERVED — inferred from a real
Linux three-seam run and a real Mac native-only CI run, not from an actual
Mac three-seam execution, which has never happened on this repository's
hardware. Confirm it against a real run before trusting it over further
inference.

On either platform: all-green means a fail-by-design stopped firing and
the run is not usable for derivation until that is explained; more
failures than the platform's expected count means something else broke
as well. Check the counts and the names, not just the colour.

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
  unchanged; it only watches. **Linux-only** (`tests/golden_rig/CMakeLists.txt`:
  `if(UNIX AND NOT APPLE)`) — `--wrap` is an ELF/GNU-ld-and-lld feature with no
  COFF equivalent (confirmed on the `windows-clang-release` CI job: lld-link
  ignores `-Wl,--wrap=pardiso` outright and fails on the resulting undefined
  `__real_pardiso`) and no Mach-O one either (ld64 has never implemented
  `--wrap`; Mach-O's nearest analogue, dyld's `__DATA,__interpose` runtime
  interposition, is not a linker-flag swap). Independent of the toolchain
  question, the target has no mission on either platform anyway: both old
  seams it watches only ever build against sibling Linux/MKL checkouts, so
  there is no derivation run for it to observe there. The static half above
  has neither limitation and runs everywhere, including macOS and Windows CI.

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
