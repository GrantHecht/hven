# hven — Agent Guidance

## 1. Project overview

hven is a high-performance sparse NLP solver library: an interior-point
engine and an SQP engine sharing common infrastructure (sparse linear
algebra, KKT assembly, warm-start currency, globalization primitives).
Written in C++20, licensed Apache-2.0, under active development.

- Namespace: `hven::`
- Public headers: `include/hven/`
- CMake package/target: `hven::hven`

hven builds standalone with CMake and requires Intel MKL (Linux/Windows)
or Apple Accelerate (macOS) as its sparse linear algebra backend. Its
primary consumer is [tycho](https://github.com/GrantHecht/tycho), a
trajectory design and optimal control library; the interior-point engine
originated in tycho and a companion SQP engine originated in a sibling
project, but both engines' identity in this repository is `hven` — their
origin project names do not appear here.

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
- **MKL iparm changes require human review.** Any change to the Pardiso
  `iparm` surface must be flagged for explicit human review before it
  merges.
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

## 7. Measurement discipline

- Counters are the asserted currency of correctness and performance
  claims; wall-clock timings are informational only.
- Quoted wall-clock numbers are single-threaded unless stated otherwise.
- Concurrent MKL solvers spin-wait catastrophically against each other,
  so benchmark and sweep runners must serialize their suites rather than
  running solves in parallel.
- Every benchmark artifact carries a provenance stamp (toolchain,
  hardware, date, commit).
- Intentional breaks of a pinned/reproduced value are declared and
  re-derived explicitly — never silent.

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
