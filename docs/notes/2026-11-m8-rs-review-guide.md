# RS review guide: the code-organization restructure (M8 close gate)

For Grant. Everything here was read from `origin/m8` at `42f17996b`; no code was run for this guide, so every
"passed" below is a claim quoted from an evidence file, with its path. Paths are repo-relative. Evidence root:
`docs/notes/data/2026-11-m8-rs/` (written `$E/` below). Each gate directory `$E/gates/<step>/` has a `RESULT.txt`
(the verdict, one line per leg), `VERDICTS.txt` (the runner's log) and a `CI.txt` or CI record.

## 1. What RS set out to do, and its rules

Before RS, the IPM was spread over six directories and the SQP over five. `drivers/` held both solvers, the shared
surface and the trace sinks. `globalization/` held IPM-only code plus the SQP's quasi-Newton files. `interior/`
held generic utilities. Two headers were both named `warm_start.h`. Tests were laid out by origin project, and
`hven_sqp_tests` was UNIX-only. RS made the tree answer "where is the IPM / the SQP / a QP engine / the linear
algebra?" with one directory each, and put executables, fixtures and layering rules on the same footing.

Design (frozen, read-only copy): `$E/RS-design-page-v2.md`. Complete file-by-file map: `$E/RS-move-map-v2-amended.txt`
(the amended one is current; `-v2.txt` is the pre-ruling version). Its section 2.2 holds the placement rules; section
2.3 holds the layer order; section 5.2 holds the per-commit gate. The rules, in short:

1. **Organise by engine, over shared layers.** Five engines: `ipm`, `sqp`, and the QP engines `walk`, `ssn`,
   `ipqp` (under `qp/`). A shared layer exists when at least two engines include a file, or when the file is a named
   bridge (model, warmstart, solver, trace). Generic infrastructure (core, util, linear) is owned by concept.
2. **One layer order for every file** (public, detail and src alike):
   `core < util < barrier < linear < model < qp-data < kkt < qp-engines < warmstart < solver < {ipm, sqp} public < trace < engine implementations`.
   `ipm` and `sqp` may not include each other. Any upward include must be on a debt-annotated file-to-file allow-list.
3. **Placement per concept.** A file lives with the component that owns its concept. A fixture lives with the
   component whose objects it builds. A test lives with the component whose contract it pins, whatever drives it.
4. **Pure moves first.** RS-1 and RS-2 are `git mv` plus include and CMake path rewrites and nothing else. Moves,
   comment re-pointing, include re-spelling/formatting and new tests are separate commits with separate gates.
   (CLAUDE.md section 5: "migrate first, then rationalize".)
5. **No TU is split or merged.** The one exception is whole-TU deletion in RS-1a. So the section 5 split protocol
   (flags diff / counter identity / benchmark) was never triggered. What was proven instead is symbol-level identity
   of the compiled objects (below).
6. **Public paths: a clean break, no forwarding headers** (M8 is the break window, 0.2.0). Only 16 public headers
   changed path, only in their directory part; `hven.h` consumers are unaffected.

The ordering and the debt list are enforced by code now (RS-5), not by this page: `tests/core/test_layering.cpp`.

## 2. The tree, before and after

Counts after RS are at the RS-R1 / RS-6b close, from `git ls-tree origin/m8`.

| before | after | note |
|---|---|---|
| `include/hven/drivers/` (16 public headers: both engines, shared surface, trace) | `hven/ipm/` (6), `hven/sqp/` (3), `hven/solver/` (4), `hven/trace/` (3) | the 16 public path changes |
| `include/hven/{globalization,interior,kkt}/` (empty `.gitkeep` dirs); `core/detail/` | removed; `core/detail/*` moved to `detail/core/` | export contract now forbids unknown dirs |
| `detail/drivers/` (20) | `detail/ipm/`, `detail/sqp/`, `detail/solver/` | |
| `detail/globalization/` (23, IPM-only) | `detail/ipm/globalization/` (18); `inertia_regularization.h` to `detail/kkt/` | three barrier-parameter governors went to `detail/ipm/` |
| `detail/globalization/sqp/` (9, with 4 quasi-Newton files) | `detail/sqp/globalization/` (funnel, trust_region, soc, elastic, restoration) and `detail/sqp/quasi_newton/` (4) | `globalization.h` renamed `funnel.h` |
| `detail/interior/` (+`typedefs/`, `utils/`) | `detail/core/` (eigen_types, super_scalar_traits, finite_screen), `detail/util/`, `detail/barrier/` (barrier_math, bound_set), `detail/ipm/`, `detail/kkt/kkt_factorization.h`, `detail/model/fixed_variable_row.h` | dead utilities deleted, see leftovers |
| `detail/qp/` (14, flat, walk files spelled `qp_*`) | `detail/qp/` data layer (qp_problem, working_set) + `walk/` (8), `ssn/` (1), `ipqp/` (4) | `bordered_eqp.h`, `eqp_solve.h` moved from kkt to `walk/` |
| `detail/kkt/kkt_calls.h` | `detail/kkt/kkt_factor.h` (+ `.cpp`) | name only; two factor families now documented |
| `detail/warmstart/{continuation,predictor}.h` | `detail/sqp/` | they drive `SqpSolverImpl` |
| `detail/warmstart/warm_start.h` (crossover; same basename as the public currency header) | `detail/warmstart/crossover.h` (+ `src/warmstart/crossover.cpp`) | |
| `src/drivers/` (22), `src/globalization/sqp/` (6), `src/interior/` (+`utils/`) | `src/ipm/` (6), `src/sqp/` (8 + `globalization/` 2 + `quasi_newton/` 4), `src/solver/` (7), `src/trace/` (3 + private `trace_names.h`), `src/util/` (3), `src/kkt/` (2), `src/qp/{walk,ssn,ipqp}/` | `src/sqp/globalization/` vs flat `src/ipm/` is deliberate (design 2.2 "Placing TUs") |
| `tests/{drivers,interior,sqp,common_support,...}` by origin project | `tests/{core,linear,model,kkt,qp,warmstart,solver,trace,ipm,sqp,bench}/` each with `support/`; `tests/support/` for unowned fixtures; goldens at `tests/trace/golden/` | `golden_rig/` and `install_smoke/` unchanged |
| test executables: `hven_tests`, `hven_interior_tests`, `hven_drivers_tests`, `hven_sqp_tests`, 4 ad-hoc seam executables | `hven_<dir>_tests` (11), `hven_<dir>_seam_tests` (6: linear, kkt, qp, solver, ipm, sqp), `hven_linear_alloc_tests` | seam sources in `tests/<dir>/seam/` |

**TUs that are new** (the only change to the compiled set):
- `src/qp/ipqp/ipqp_options.cpp` (RS-R1: `validate(const IpqpOptions &)` split out of the engine).
- `src/trace/json_lines_trace_sink.cpp` is a rename of `trace_writer.cpp`, with an unused include dropped.
- Other library TUs are renamed or moved only (`kkt_calls`->`kkt_factor`, `qp_engine`->`walk_engine`,
  `solve_status`->`ipm_phase_status`, `warm_start`->`crossover`). RS-1a deleted `color_text.cpp` and `fmtlib.*`.
- New public header: `include/hven/qp/qp_options.h`. New detail header: `include/hven/detail/sqp/nlp_eval.h`.
- New test machinery: `hven_testing_library` (one shared HVEN_TESTING object library), `Layering.*`,
  `HeaderSelfContained`, and the configure guards in `tests/CMakeLists.txt`.

Expected library TU count: `src/CMakeLists.txt:269` (`_hven_expected_source_count 65`).

## 3. Step by step

Every step carries a declaration committed before its first gate leg (`$E/DECLARATION-*.md`, never edited; a failure
gets an `-rN` successor). Hashes are the commits on `origin/m8` after rebases. A common core of evidence is repeated
at each step, so it is stated once:

- **P-SYM** (`scripts/psym_compare.sh`): per-symbol instruction identity of every compiled object, before vs after,
  both arms built at the same path with `__FILE__` neutralised, an object map generated from the move map, an empty
  exceptions file unless declared. Verdicts: byte-identical, NOISE-ONLY (a `__LINE__` immediate or the three
  pad-noise bench objects `bench_corpus`, `bench_crossover`, `ipqp_e1_arm`), or UNCLASSIFIED (a failure). Every step
  closed with 0 UNCLASSIFIED and every library object byte-identical (see RS-R1 and RS-6a r6 for the declared movers).
- **Flags**: `compile_commands.json` diff, token-identical modulo declared classes.
- **PCH neutrality** (`scripts/check_pch_neutrality.sh`) where an include set was touched.
- **ctest Release and Debug**, with the sorted `--gtest_list_tests` of every executable and `ctest -N` identical to
  the baseline (the byte-dump test names, 133 of them, differ per process and are compared as a class).
- **Install smoke + export contract**, and the installed file listing equal to the baseline with the move map applied.
- **27-cell replay** x (walk, ssn, ipm) vs the pre-step replay: 0 differences, 75 columns each (counters, statuses,
  residuals). Serial, under the box lock. This is the behaviour-identity proof; no wall-clock is asserted anywhere.
- **Mutation proofs** where a step adds a check: plant the violation, see the named check fail, restore, see it pass.
- **CI**: four lanes (Linux, macOS, Windows, one more) on the pushed head, recorded in `CI.txt`.

### RS-0: instruments and baseline
- Commits: design copy `956e7a09d`; instruments `229fb713a`, `830ad67b0`; declaration `5b54e8596`; baseline at `5b54e8596`.
- Changed: no compiled code. P-SYM learned derived object directories and an object-map generator (self-tested);
  static-initialiser check (`nm` for `_GLOBAL__sub_I_*`) on every TU to be renamed; baselines of gtest lists, ctest -N,
  install listing, trace-site count (130), core-layering count.
- Evidence: `$E/baseline/SUMMARY.txt` (P-SYM capture of 199 objects, ctest 3869/3869, install listing 962 files).

### RS-1a..1e: the library moves (five commits, pushed together)
- Commits: 1a `a6bcaf94e` (foundations: detail/core, detail/util, src/util; dead utilities, fmtlib, color_text
  deleted), 1b `26d748090` (barrier, kkt, the qp split into walk/ssn/ipqp), 1c `f9727bf28` (IPM), 1d `543589f2d` (SQP),
  1e `48a4ecd5b` (solver, trace; crossover rename; `.gitkeep` removal). Declarations `5b54e8596`, `a24f0cabd`.
- Changed: `git mv` + include rewrites + CMake path strings + `hven_pch.h` + the umbrella + install-smoke includes +
  re-pointing path-coupled tests. Sentinel in `cmake/hvenConfig.cmake.in` re-pointed to `hven/hven.h` in 1c.
- Neutrality: each commit gated, final run cumulative against RS-0. P-SYM 198 objects, 186 identical, 3 noise, 0
  unclassified (1e). `warm_start`->`crossover` etc. compared per symbol through the symbol map. Flags: 363 TUs
  identical (the only delta: `-I src/interior/utils` removed). PCH neutral on 1a/1b/1c. Replay 0 differences x3.
  Trace-off-path site count asserted EQUAL to 130.
- Evidence: `$E/gates/RS-1a/` .. `RS-1e/RESULT.txt` (run 1 of 1a kept in `RS-1a-run1/`); CI `3edf5ede4`.
- Disclosed: the old-seam golden-rig adapters (the two CLAUDE.md section 1 exceptions) did not compile against
  `linear::InertiaEvidence` before RS and still do not (`RS-1e/RESULT.txt` G8). Not an RS finding.

### RS-2: test re-layout
- Commits: declaration `22820a51d`; move `43f5573f9`; gate `7c280a3a6`; CI `aa1330cad`. Declarations -r2, -r3 are
  instrument corrections (unity directory move, `__FILE__`-neutral arms).
- Changed: tests moved by component; `interior`->`ipm`, `drivers`->`solver`; fixtures into `support/`; goldens to
  `tests/trace/golden/` (bytes unchanged, `.gitattributes` re-pointed, so hven-bench's sha256 copies still match);
  `tests/` becomes an include root; configure-time duplicate-fixture-basename check.
- Neutrality: P-SYM 198 objects, 188 identical, 3 noise, 0 unclassified, EXCEPTIONS none (7 renamed test objects
  compared per symbol). Flags identical modulo the declared include-root swap. Gtest lists raw-identical (20
  executables). Non-vacuity proof: hiding `tests/trace/golden` or `tests/model/compile_fail` makes those tests fail
  (`$E/gates/RS-2/vacuity.txt`). Replay 0 differences.
- Evidence: `$E/gates/RS-2/RESULT.txt`.

### RS-3: prose and literals
- Commits: declaration `04e271258`; `54996885e` (comments, banners, `#error` texts, recipe strings; origin names swept
  from build-file comments); `cbcc9fa07` (CLAUDE.md sections 1-2 rewritten, the migration section, `.git-blame-ignore-revs`);
  `4915418c0`, `c4c680576` (evidence); CI `4064ff97a`.
- Changed: comments only. Declared string-literal changes: golden-rig recipe description strings, one `static_assert` message.
- Neutrality: reduced gate (no moves). P-SYM 198 objects, 195 identical, 3 noise, 0 unclassified. A comment-stripped
  token compare of every changed C++ file shows only the declared literals differ.
- Evidence: `$E/gates/RS-3/RESULT.txt`.

### RS-4: include spelling and formatting
- Commits: declaration `f416d1ef0` (base moved to GL-0, `b86848e30`); `d18dfb455` (every hven include as `<hven/...>`;
  `nlp_model.h` no longer includes `qp_types.h`); `6422c6213` (`.clang-format` sorts includes); `6d788afb0` (pure
  formatter output over 260 files; listed in `.git-blame-ignore-revs` by `3261ff8c9`); `392c0c301` (three order-dependence fixes).
- Neutrality: P-SYM 198 objects, 141 identical, 57 noise-only (54 are `__LINE__` shifts from the reformat), 0 unclassified;
  symbol set unchanged in all 198. 795 compiler-local label families matched. Flags identical. PCH neutral. Replay
  0 differences. Every header compiled alone (header sweep).
- Evidence: `$E/gates/RS-4/RESULT.txt`, CI record `bc8ed0e12`. Run 1 (`RS-4-run1/`) stopped at G1 on three
  order-dependent includes; the fix is `392c0c301`.

### RS-5: layering test and export contract
- Commits: declaration `c9a4253cc`; `319b4c1c6` (NlpEval extracted into `detail/sqp/nlp_eval.h`); `62c454091`
  (`tests/core/test_layering.cpp` + `HeaderSelfContained`); `8cb620aaa` (export-contract extension); `2bbb1de9e`
  (macOS-only exclusion found by CI); evidence `44f74446f`.
- Changed: no library behaviour. `NlpEval` is a pure struct move; every library object byte-identical.
- Neutrality: P-SYM with one declared exception (the renamed, rewritten test object). The allow-list was re-derived
  from the tree before any edit and equals the design's 12 pairs. Mutation proofs: planted layering violations,
  root header, unknown dir, non-self-contained header: every plant fails, every restore passes
  (`$E/gates/RS-5/mutation/MUTATION-VERDICTS.txt`).
- Evidence: `$E/gates/RS-5/RESULT.txt`.

### RS-6a: one test executable per directory
- Commits: declaration `7d2359700`; `8f0ab646a` (executables, seams, membership and seam guards);
  `518796224` (the shared HVEN_TESTING library); r6 evidence `da1e129f5`; CI `b02e88eb4`. Declarations r2..r6 are
  instrument and measurement corrections; r6 is the final run.
- Changed: tests only. 11 ordinary + 6 seam + 1 alloc executables replace 12; `hven_testing_library` replaces
  per-seam library recompiles (CMake only: `tests/CMakeLists.txt:401-440`).
- Neutrality: every library and bench object byte-identical; membership equals the declared membership (188 sources);
  3404 names equal the base's; replay 0 differences; mutations 7/7 killed. Unity-class test objects are NOT
  instruction-identical (4136 bodies identical, 169 differ beyond numbers); the evidence for that class is G5
  (behaviour), declared in `DECLARATION-RS6a-r6.md`.
- Cost measured: seam build 75.7 s -> 63.7 s, CPU 1070 s -> 899 s, clean tree 203 s -> 194 s (wall-clock, informational).
- Evidence: `$E/gates/RS-6a-r6/RESULT.txt`; the pre-Q9 state is kept in `$E/gates/RS-6a/`.

### RS-R1: whole-RS review rider
- Commits: declaration `2bab66552`; `05a7d2c78` (code: `qp/qp_options.h`, debts 2/3/12 paid, `trace_writer`
  rename, `src/CMakeLists.txt` regrouped); `e5032c84f` (test renames and namespaces, seam dirs, uniform test flags);
  `463fcd78a`, `22576316e` (stale references); `243cb6660` (`docs/testing.md` restructure); `1a8f7a2d7` (umbrella
  includes `qp_options.h`). Final declaration -r4; evidence `83dbf6b95`; CI `64e384e2a`.
- Neutrality: P-SYM was declared to allow movers (`ipqp_engine`, `sqp_options`, the renamed JSON sink, three bench
  objects), each with defined-symbol-set equality (modulo assembler-local labels). Library flags token-identical modulo the commit stamp.
  Gtest union equals the base's plus exactly two new `Layering` tests, modulo one rename rule (`SqpDriver`->`SqpSolver`).
  Replay 0 differences. Mutations 8/8 killed. Test TU count 427 -> 452 is reported, not asserted.
- Evidence: `$E/gates/RS-R1/RESULT.txt`, `declared-movers.txt`.

### RS-6b: Windows build gates lifted
- Commits: code `85b8307ce`; declarations `6a11c1dba`, `487d11f7e`, `aad276876`, `3043341cc`, `c8d6f6d1f` (r1..r5);
  evidence `a2fa7fda7`, `44313bae4`.
- Changed: `tests/` CMake and three test sources only. `hven_sources_not_on_windows()` is deleted; the bench
  runner-driving trio stays off Windows with its reason. One compile fix (`M_PI` -> `std::numbers::pi`). Three tests
  were moved to an `#if HVEN_TEST_ROUTE_REFERENCE` arm because Windows MKL reads a different value (`DECLARATION-RS-6b.md`).
- Neutrality: no library file touched; 64 non-test objects byte-identical, three bench objects identical after
  the commit-stamp literal is normalised. Windows ctest 3772/3772 (3782 with examples). Windows-lane mutations killed in their named suites.
- Evidence: `$E/gates/RS-6b/RESULT.txt`, `$E/gates/RS-6b-r5/RESULT.txt` (r5 is the re-take on the rebased head).

## 4. Where to look first

Ordered by how much a human's judgement can add. The evidence says the compiled library is byte-identical; these are
the places where a tool cannot tell you whether the decision was right.

1. **The allow-list and the layer order** (`tests/core/test_layering.cpp:541-575`, the matcher near `:579`, the
   stale-entry checks near `:709-725`). Nine pairs, each with a debt and a named remover. Questions: is each stated
   remover real (debt 1 names "T8 section 6 group", debts 4-5 "model-contract unification")? Is the list the right
   size to ship? The forward-declaration check (`:759`, `:893`) is a grep, not a parse; its known-upward list is the
   least rigorous part of the enforcement.
2. **`solver_counters.h` sits at the bottom of the order and holds engine counters** (`include/hven/core/solver_counters.h`:
   `QpCounters` at 34, `SsnCounters` 685, `IpqpCounters` 942, `SqpCounters` 1554; the IPM block follows). RS recorded it
   as debt rather than moving it. This is the largest piece of engine code left in `core/`.
3. **The PCH and include-order lockstep** (`src/hven_pch.h:31-50`, `src/ipm/ipm_solver.cpp:9-30`). Both carry
   `// clang-format off` so the PCH's include list stays byte-for-byte equal to that TU's (the PCH-neutrality
   property). RS-4 re-spelled both in lockstep, and the ipm list is intentionally not alphabetical. If anyone formats these, the build still works and the property goes.
   `scripts/check_pch_neutrality.sh` is the only guard.
4. **Order-dependent includes RS-4 found and RS-5 fixed** (`include/hven/detail/sqp/nlp_eval.h`; its consumers
   `assembly_eval_seam.h`, `sqp/globalization/soc.h`, `sqp/globalization/restoration.h`; `include/hven/detail/sqp/sqp_solver_impl.h:43`).
   RS-4 left comment-separated include blocks as a workaround; `319b4c1c6` removed them. Check that no consumer still
   relies on a header being included first, and that `eval_nlp` staying in `sqp_solver_impl.h` while `NlpEval` lives
   elsewhere is the right split.
5. **The new public header** (`include/hven/qp/qp_options.h`, whole file; included from `include/hven/sqp/sqp_options.h:31`
   and the umbrella). This is the one RS public-API addition, beyond the 16 path moves. The types moved verbatim, but it
   changes the include story for anyone who named `QpMode` or the `Ssn*Rule` enums through `sqp_options.h`
   (they still get them). Also read `src/qp/ipqp/ipqp_options.cpp:34` (`validate(const IpqpOptions &)`): the IPQP engine
   no longer builds a `SqpOptions` to validate, which is the one place RS-R1 changed code in a library TU beyond moves
   (declared, symbol-set-equal, replay-neutral).
6. **The two kkt factor families** (`include/hven/detail/kkt/kkt_factor.h:9` and `kkt_factorization.h`). RS renamed
   one and documented that they are distinct on purpose. Whether to unify is RS-7. Decide whether the file names
   (`kkt_factor` vs `kkt_factorization`) are close enough to mislead.
7. **Where `bordered_eqp.h` and `eqp_solve.h` landed** (`include/hven/detail/qp/walk/`). Design rule 1 moved them out of
   `kkt/` because the walk is their only library includer. `src/qp/walk/` is a thin pair of TUs over a lot of header
   code. Confirm that "walk-private" matches how you think about them.
8. **`counters_dump.cpp` and `result_message.cpp` each print both engines** (`src/solver/counters_dump.cpp`,
   `src/solver/result_message.cpp`; debts 7-11). These are the TUs that make `src/solver/` include `ipm/` and
   `sqp/` public types. They are the clearest case of a seam that RS left awkward on purpose.
9. **Test executable structure and the shared HVEN_TESTING library** (`tests/CMakeLists.txt:238-330` guards,
   `:401-440` `_hven_testing_library`, `:500-550` seam checks). This is the only place RS changed what
   gets compiled with `HVEN_TESTING`. CLAUDE.md section 8 requires that production objects never see it; the guards that
   enforce this are `hven_check_seam_targets()` and the 7+8 mutation proofs. The unity-class test objects are not
   instruction-identical (RS-6a r6 G2); only their behaviour is proven.
10. **Uniform test flags** (RS-R1, `e5032c84f`; the one-rule-per-directory block in `tests/CMakeLists.txt`). Owner-ruled:
    no per-file `-O1`, no unity batching. This changes test compile cost and test object code and is outside
    P-SYM. The evidence is gtest-name equality, not code equality.
11. **Public path break with no forwarding headers** (`$E/hven-bench-breaks.txt`; `docs/migration-0.2.md:736`). One live
    hven-bench include (`lanes/external/declared_point.h:87`, `<hven/detail/drivers/declared_diagnostics.h>`) breaks on
    the RS head; a hven pin bump needs the consumer sed. The migration section states "no symbol, namespace, option
    or behaviour moved". That is true of RS-1..6 but RS-R1 moved the IPQP/SSN option types to `qp_options.h` and renamed the
    `SqpDriver*` gtest suites to `SqpSolver*`, which breaks any CI filter or script keyed on the old name (listed in the
    RS-R1 hand-back, not edited out of repo).
12. **Windows off-reference test arms** (`DECLARATION-RS-6b.md`: three tests; `tests/qp`, `tests/sqp` routes
    pinned under `#if HVEN_TEST_ROUTE_REFERENCE`). These weaken what a Windows run asserts; the argument is that
    Windows MKL dispatch differs and the safety half of each assertion stays unconditional. Read the three and decide
    whether you agree.
13. **Dead-code deletion in RS-1a** (`a6bcaf94e`: five util headers with zero includers, `fmtlib.{h,cpp}`,
    `color_text.{h,cpp}` with `enable_color_console`). The design made it conditional on owner confirmation; the check
    was "all executables link and P-SYM shows only removed symbols". Confirm you accepted that deletion; `memory_management`
    was kept as a separate decision.
14. **Export contract and install tree** (`scripts/check_export_contract.sh:162-199`; `cmake/hvenConfig.cmake.in:110`).
    The sentinel is now `hven/hven.h`; the contract now rejects unknown directories under `include/hven` and
    `include/hven/detail`. A new component needs two list edits. Check this is the friction you want.
15. **The trace-site count guard** (`tests/trace/test_trace_off_path.cpp:223-227`, asserts 130). It scans
    `{ipm, sqp, solver, qp, warmstart}`; a legitimate new emit site breaks it by design. Check that is acceptable.

## 5. Known leftovers and registered follow-ups

- **RS-7 (registered, not done)**: symbol renames (`QpEngine`->`WalkEngine` and the arms, which still carry old names
  under new file names), unifying the two kkt factor families, per-engine split of `counters_dump` and `result_message`
  (pays debts 7-11), splitting `test_hs_sweeps`, renaming remaining test files, a review of `bench/`'s layout (untouched
  but for forced edits). Source: `$E/RS-design-page-v2.md` section 5.1 and the `RS-7` mentions in the debt notes.
- **Debts still on the allow-list**: 1 (model <- ipm enum; model-contract unification), 4-5 (`problem.cpp` reads
  `SolveResult`), 6 (IPQP engine includes `trace/trace.h`; trace upgrade), 7-11 (RS-7 split).
  Engine counters in `core/solver_counters.h` are noted as further debt (consistent-solver-interface track).
- **Registered but outside RS**: 17 detail headers included only by src TUs but installed (design 1.12);
  src-private headers; the `memory_management` decision.
- **Stale spots seen while reading** (not RS bugs, worth a rider): `tests/CMakeLists.txt:235` still names
  `hven_sources_not_on_windows()`, which RS-6b deleted. Design page v2 still lists the removed 13th allow-list pair
  and the old `qp_engine` names; it is a frozen evidence copy and is not edited. `docs/migration-0.2.md:736` is the
  RS include-path table; commit `42f17996b` (DC-1) says "drop the migration guide", so confirm the file's fate before you point a consumer at it.
- **Windows**: the bench runner trio (`test_bench_dump`, `test_corpus_cells`, `test_crossover_legs`) and the SNOPT
  bridge gate remain off Windows (POSIX process control); the old-seam golden-rig adapters do not compile and are
  slated for deletion when the engine migrations close.
- **Platform evidence**: macOS and Windows evidence is for pushed heads only (CI lanes), by design (RS-1 was one push).
  Apple values are never fabricated; anything not run on Mac is UNOBSERVED.
- **hven-bench and tycho** are not updated by RS; the catch-up list carries the path and suite renames.

## 6. Suggested reading order and time

About 3 hours for the judgement spots; add 1-2 hours if you want to spot-check evidence. The restructure is large
(about 360 moves in RS-1/2) but the review burden is small because most of it is machine-verified.

1. (10 min) Section 1 above, then `RS-design-page-v2.md` change log and sections 2.1-2.3.
2. (10 min) `git show --stat --diff-filter=R -M a6bcaf94e 26d748090 f9727bf28 543589f2d 48a4ecd5b | less`: scan moves, not contents. Pure renames show 100% similarity; open any below 100%.
3. (30 min) Items 1, 2, 8 in section 4 (the layer order, counters, mixed-engine TUs); read `tests/core/test_layering.cpp` top to `:575` first.
4. (30 min) Items 3, 4, 5, 6, 7 (headers and include structure).
5. (30 min) Items 9, 10, 12 (`tests/CMakeLists.txt`, test flags, Windows arms).
6. (20 min) Items 11, 13, 14, 15 and section 5.
7. (optional, 60-90 min) Read three `RESULT.txt` files in full, in this order: `RS-1e` (cumulative moves), `RS-6a-r6` (the executable restructure), `RS-R1` (the declared movers). Pick one gate and replay one leg from its `VERDICTS.txt` yourself.

A quick spot-check that needs no build: `git grep -n "hven/drivers\|hven/interior\|detail/globalization" origin/m8 -- include src tests bench examples scripts cmake CMakeLists.txt`
finds two comment-only hits at `origin/m8`: `include/hven/detail/sqp/globalization/funnel.h:100` (a stale
`detail/globalization/` reference) and `tests/core/test_layering.cpp:24` (a historical comment). Both are stale prose RS-3 and
RS-R1 missed; no include or CMake path uses an old directory.
