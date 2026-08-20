# hven — Agent Guidance

## 1. Project overview

hven is a high-performance sparse NLP solver library: an interior-point
engine and an SQP engine sharing common infrastructure (sparse linear
algebra, KKT assembly, warm-start currency, globalization primitives).
Written in C++20, licensed Apache-2.0, under active development.

Built so far: the core and linear-algebra components (`src/CMakeLists.txt`
is the authority on what compiles). Neither engine is in this repository
yet — both arrive with later milestones, migrated in from the projects
they originated in — so a statement about "the engines" is a statement
about where this is going, not about code you can read here.

- Namespace: `hven::`
- Public headers: `include/hven/`
- CMake target: `hven::hven`, consumable either via `add_subdirectory`
  (tycho's mode) or as an installed CMake package: `cmake --install`
  produces libhven.a, the flattened headers (hven's own plus its vendored
  Eigen/fmt), and `hvenConfig.cmake`/`hvenTargets.cmake` under
  `<prefix>/lib/cmake/hven`, so `find_package(hven)` +
  `target_link_libraries(app hven::hven)` works against an install tree.
  See `scripts/check_install_smoke.sh` for the standalone install/consume
  round-trip.

hven builds standalone with CMake and requires Intel MKL (Linux/Windows)
or Apple Accelerate (macOS) as its sparse linear algebra backend. Its
primary consumer is [tycho](https://github.com/GrantHecht/tycho), a
trajectory design and optimal control library; the interior-point engine
originated in tycho and a companion SQP engine originated in a sibling
project, but both engines' identity in this repository is `hven` — their
origin project names do not appear here.

Two bounded exceptions.

The first, for as long as it exists: the golden-numerics rig's two
OLD-SEAM adapters (`tests/golden_rig/seam_psiopt.cpp`, `seam_sqp.cpp`) name
those projects, because they compile against those checkouts and are pinned to
them by path and by tag — a temporary test-only artifact cannot be
origin-neutral about the tree it includes. They are deleted when the two engine
migrations close.

The second, permanent by construction: **frozen baseline and evidence CSVs
retain their origin-naming provenance headers verbatim.** A provenance header
records what actually ran — the binary, the invocation, the machine — and a
pinned artifact's bytes are the pin. Rewriting one to satisfy a naming rule
would be a silent pin mutation, which §7 forbids; the exception exists precisely
to prevent a future cleanup pass from "fixing" a pin for cosmetic reasons. These
headers are historical records, not statements about this repository's identity.
Artifacts derived *after* the migrations close carry hven-native headers, so the
exception does not grow.

Outside these two, the rule holds without exception.

## 2. Repository structure

```
include/hven/
  core/            typedefs, settings & diagnostics shapes, the counters
                    contract, instrumentation/ledger, threading utils
  linear/          unified sparse-LA interface (one surface, MKL Pardiso
                    + Apple Accelerate implementations)
  model/           the problem contract + the provider-side partitioned
                    evaluation engine
  kkt/             KKT assembly and layout, including the Schur-
                    complement border stack
  interior/        barrier / interior-point machinery
  qp/              QP subproblem engines
  globalization/   funnel, line-search, trust-region, SOC, and
                    restoration primitives
  warmstart/       warm-start currency, crossover, mesh transfer,
                    continuation
  drivers/         the top-level IPM and SQP algorithms
  detail/          template implementation bodies and internals kept out
                    of the public umbrella headers
src/               non-template implementation TUs
tests/             unit and correctness test suite
bench/             benchmark harness and tracked baselines
bindings/          optional nanobind Python module (own CMake component)
cmake/             CMake helper modules (Find* modules, compile options,
                    submodule helpers)
dep/               vendored submodule dependencies (Eigen, fmt; nanobind
                    added later with bindings/)
notices/           third-party license notices
docs/              engineering documentation
```

## 3. Build

- CMake >= 3.24, C++20, clang/LLVM toolchain.
- hven is a light project: no heavy template unity builds are needed, so
  ordinary `-j` parallelism (matching core count) is fine for local
  builds. It does not carry tycho's `heavy_compile` job-pool throttling
  because it does not have tycho's multi-GB unity TUs.
- Prefer Debug builds while developing — Debug keeps Eigen's runtime
  asserts active, which catch indexing and bounds bugs that are silent
  in Release. Verify changes in Release before considering them done.

## 4. Code style

Carried verbatim from tycho's conventions:

- Types and classes: `PascalCase`
- Member functions: `snake_case`
- Member variables: `snake_case_` with a trailing underscore
- Free functions: `snake_case`
- Compile-time constants: `kPascalCase`
- Macros: `HVEN_UPPER_SNAKE`

Run clang-format with the repo's `.clang-format` config before
committing.

Error handling rules (boundary validation and exception discipline):

- Library code never calls `exit()`.
- Library code never prints a diagnostic it doesn't also fold into the
  thrown exception's message (`throw std::invalid_argument(fmt::format(...))`).
- Library code never constructs an exception without throwing it.
- Validate sizes and bounds at API boundaries. Eigen's own asserts are
  compiled out under `NDEBUG` (the Release default) and must never be
  the only guard against an out-of-bounds access.

## 5. Translation-unit structure

hven makes deliberate use of separate translation units — it is not
header-only the way pure-template libraries are, and it is less
header-heavy than a from-scratch header-only design would be — but
never at measurable runtime cost. This is decided per component, not
applied as a blanket rule:

- Concrete, non-template classes get declarations in public headers and
  implementations in `src/` `.cpp` TUs. This applies to `linear/`'s
  backend classes, the dense factor, the model contract's non-template
  layers, the evaluation engine's orchestration, and settings /
  diagnostics / ledger / printing code generally.
- Per-element hot paths that depend on inlining and templates stay
  header/templated, with explicit instantiation over the closed type
  set (double, plus the fixed sparse types) where the type set is
  closed. Orchestration, drivers, options, printing, and instrumentation
  live in `.cpp` TUs regardless of how hot the surrounding loop is —
  what matters is whether the *specific* code depends on inlining
  through a template parameter to be fast.
- Runtime neutrality is proven, not presumed. Any TU boundary drawn
  through migrated engine code must be verified by a compile-flags diff,
  a counter/corpus bit-identity check, and a benchmark comparison before
  it ships. Build-side benefit is judged on parallel wall-clock and peak
  RSS, not serial CPU-time; runtime neutrality is judged on the
  benchmark comparison. A boundary that costs measurable runtime is
  reverted or redrawn — the "never at measurable runtime cost" clause
  always outranks the "use separate TUs" clause.
- When migrating existing engine code into this structure, migrate
  first (as-is, preserving bit-identity), then rationalize TU structure
  as a separate, separately-verified step. Do not mix "move" and
  "restructure" into one unverified step.

## 6. Governance

These rules bind every agent and tool working in this repository, with
no exceptions:

- **SNOPT source firewall (absolute).** No SNOPT source, headers, or
  decompiled material may be read or incorporated into this repository,
  by any agent or tool, under any circumstance.
- **MKL iparm changes require labeling and validation evidence, not
  blocking human review** (owner ruling, reiterated 2026-08-20; the
  explicit-human-review form of this rule predated hven and the SQP
  proof of concept and is retired). Any change to the Pardiso `iparm`
  surface must carry an `IPARM-SURFACE` label on its commit, cite the
  validation evidence that justifies it, and pass the tycho-lane review
  when one is in scope. The owner spot-checks at their discretion; their
  sign-off is not a merge gate.
- **MKL redistribution terms are sensitive.** Flag any change touching
  MKL integration or redistribution for manual review.
- **Math-only license discipline for GPL/LGPL-tainted references.**
  Papers describing GPL/LGPL-licensed solvers (e.g. QPALM, Cyqlone,
  FATROP, PIQP) may be read and used as algorithmic references; their
  source may never be read or incorporated. PIQP's sparse backend
  specifically is LGPL-tainted — it may be used only as a numerical
  oracle for comparison, never vendored into this repository.
- **Never fabricate Apple/Accelerate values.** A value that has not been
  observed on real Mac hardware is recorded as `UNOBSERVED`, never
  zero-filled or interpolated. Absent evidence is reported as absent.
- **`notices/` is protected.** Never modify or delete an existing entry
  in `notices/`; only add new entries when a new dependency is vendored.
  The one exception: append-only deviation records inside a derived
  file's own entry — the sanctioned test-seam deviation recording that
  §6's instrumentation rule itself mandates — may be appended to;
  existing text and prior records may never be altered or deleted.
- **Instrument at the boundary by preference, not by prohibition.** Test
  hooks belong in the Apache-2.0 adapter file that owns the contract logic
  around a backend session, not inside the MPL-derived session file itself
  (`hven/detail/linear/pardiso_session.*`, `accelerate_session.*`, and any
  future file derived the same way): an observation taken at the boundary is
  one a consumer could have made too, and a derived file stays a clean diff
  against its upstream. **Deviate when the work needs it, and say why** — a
  fact that leaves no trace outside the function producing it cannot be seen
  from the boundary, and refusing to look would trade a real coverage gap for
  a tidy file. A deviation must be minimal, `#ifdef`-gated, proven to cost the
  production build nothing (byte-identical object, no symbol in the library),
  recorded in `notices/`, and argued in `docs/testing.md`, which lists every
  sanctioned deviation in full. There are two today.

## 7. Measurement discipline

- Counters are the asserted currency of correctness and performance
  claims; wall-clock timings are informational only.
- Quoted wall-clock numbers are single-threaded unless stated otherwise.
- Concurrent MKL solvers spin-wait catastrophically against each other, so
  any measurement that ASSERTS wall-clock — a benchmark, a timing baseline,
  a throughput or build-time comparison — must serialize its suite: one
  solve at a time, alone on the machine. Nothing produced any other way is
  a quotable timing.
- A replay asserting only deterministic per-process columns — counters,
  statuses, and residuals computed at a fixed thread count — may co-run.
  Such columns are scheduling-invariant at `MKL_NUM_THREADS=1`, and
  contention can only push a cell toward a budget/deadline outcome, never
  toward a false success or a false value. The terms are fixed: one
  process per pinned PHYSICAL core (SMT siblings left idle),
  `MKL_NUM_THREADS=1` in every process, and wall-clock from such a run
  stays informational — it is never quoted as a measurement and never
  compared against a serial timing. Wall-clock-DEPENDENT statuses —
  deadline-truncated outcomes — are not scheduling-invariant and must run
  solo, or co-run only with their full budget honored.
- A deviation observed under a co-run is a candidate, not a regression.
  Re-run the deviating cell ALONE, under the serial rule above, and
  reproduce the deviation before calling it one.
- A co-run protocol is declared in the evidence artifact it produces — the
  tiering rule, the widths, and the pinning — so a reader knows the
  conditions the counters were taken under.
- Every benchmark artifact carries a provenance stamp (toolchain,
  hardware, date, commit).
- Intentional breaks of a pinned/reproduced value are declared and
  re-derived explicitly — never silent.
- The library, its tests, and its bench compile under **one uniform flag
  regime** (owner ruling, 2026-08-15): TU boundaries are drawn for code
  quality and build efficiency, not to preserve bit-identity against an
  existing census baseline, and the runtime-neutrality bar above is
  unchanged by that. The unification and the re-derivation of every
  flag-sensitive pin, battery artifact, and census baseline it moves are
  one **declared event** — phase C's task U0 — governed by the protocol
  above, not an exception to it. See
  `docs/notes/2026-08-15-m3-phase-c-plan.md` §0 and §6 U0.

## 8. Process rules for agents

- Never run `git add -A` or `git add .` on a tree another agent may be
  concurrently mutating. Stage with explicit pathspecs.
- Builds are serialized against source edits — do not edit source files
  while a build of that source is in flight, and do not start a build
  while another agent's edits to the same tree are in progress.
- Reviewers do not modify the tree they are reviewing.
- Sweep and benchmark runners own their own compute/time budgets — no
  unbounded cells that can run indefinitely or contend with other
  concurrent work.
- Delegated implementation work carries an explicit context budget: give
  a subagent the task, the interfaces it touches, and the binding
  constraints — not the session's accumulated history — and retire an
  agent whose context has grown past usefulness rather than pushing it
  through one more round.
- **Test-seam convention for fault paths no real backend takes, and for
  internal state no boundary observation can reach**: the instrumentation
  preference and the terms on which it may be deviated from live in §6; the
  mechanism is documented in full in `docs/testing.md` — a narrow
  `HVEN_TESTING`-gated point in the Apache-2.0 adapter file that owns the
  contract logic around a backend session (e.g.
  `src/linear/symmetric_factor_mkl.cpp`), with `HVEN_TESTING` defined
  target-wide on a standalone test executable that recompiles those TUs a
  second time and does not link `hven::hven`, so the production library and
  `hven_tests` are never touched by it. Use this convention for any new need
  rather than inventing another; `docs/testing.md` also records why each
  existing injector is or is not faithful for scenarios beyond the one it was
  built for, and lists every sanctioned inside-the-session-file deviation with
  its reasoning and its cost evidence.
