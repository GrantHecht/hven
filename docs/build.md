# Build notes

What a developer needs to know about how this library compiles, beyond
`cmake --preset … && cmake --build …`.

## The precompiled header

`src/hven_pch.h` is a precompiled header used by six of the `hven`
target's translation units. It exists because this library's build time
is dominated by header parsing, not by how much code any one file
contains.

It is enabled **only under Clang** (M7 W2 E4 fix round 4).
`src/CMakeLists.txt` guards `target_precompile_headers()` on the compiler
id, so a build with any other front end — notably the GCC that
`scripts/check_install_smoke.sh`'s default arm picks up — configures with
no precompiled header at all. Two reasons, both recorded at the guard:
GCC cannot clear bar 2 below (see the disclosed limitations), and a GCC
`.gch` is not safe to cache. The second is the one that forced the
guard; the ccache section states it.

The numbers behind that claim (Linux, clang, Release `-O3`, idle box,
ccache disabled):

- Parsing the header set in `src/hven_pch.h` costs **3.01 s**, and that
  cost is paid once per TU that includes it.
- `src/ipm/ipm_solver.cpp` is the largest TU at 3722
  lines and 7.02 s. **3.01 s of that is the headers**; only 4.01 s is
  its own body. (The per-TU table in `src/CMakeLists.txt` records 6.99 s
  for the same file. The two figures are separate measurements — this
  one taken from a full clean build, that one from the isolated
  with/without-PCH sweep — and the ~0.03 s between them is run-to-run
  noise, not a discrepancy.)
- `src/core/pattern_hash.cpp` is **39 lines** and still costs 2.98 s.

So file length barely predicts compile cost here, and the lever that
works is amortizing the shared header floor rather than moving code
between files. Building the PCH once costs 3.38 s; each participating TU
then drops 1.5–2.0 s. Measured effect on a clean `-j2` build of the
library target, median of three runs each: **28.16 s → 23.20 s
(−17.6%)**.

The same measurements are why the engine's large TUs are *not* split
into smaller ones — a split multiplies the 3.01 s floor by the number of
pieces instead of amortizing it, and (with `HVEN_LINK_TIME_OPT` off) turns
helper calls inside the interior-point hot loop into cross-TU references
the compiler can no longer inline.

### Which TUs use it, and the rule for joining

Membership is **opt-in**, listed in `src/CMakeLists.txt`. Everything not
named there is opted out, so a newly added source gets no PCH until
someone measures it.

A TU qualifies only if it clears both bars:

1. it compiles **faster** with the PCH, and
2. its object file stays **byte-identical** to the non-PCH build.

Bar 2 is the one that matters most. The translation-unit section of
`CLAUDE.md` requires runtime neutrality to be proven rather than
presumed for anything touching engine code, and a byte-identical object
is the strongest available proof: identical bytes cannot move a
golden-rig row or a counter. With the current list, every object file the
Linux build produces (28 of them: 21 after M3 phase-C T1/T2 added
`sqp/sqp_print.cpp` and `core/ledger.cpp` and brought the
previously-unmeasured `kkt/kkt_factor.cpp` into the measurement table,
then T3's `sqp/sqp_options.cpp` and `core/enum_names.cpp`, then
T4's `sqp/globalization/funnel.cpp`, then T5's `sqp/sqp_solver.cpp`,
then T6's `sqp/globalization/soc_elastic_restoration.cpp`, then
T7's `warmstart/crossover.cpp`, then T8's
`sqp/continuation.cpp`)
*and* `libhven.a` itself
are byte-identical with and without the PCH — which is what
`scripts/check_pch_neutrality.sh` re-proves on demand and in CI.

Several TUs get faster with the PCH but produce a *non*-byte-identical
object, and are excluded for that reason alone. Their code is not wrong
— per-symbol disassembly comparison shows identical instructions, with
only function layout order and local label numbering shifting. It
happens because those TUs include a different header set in a different
order (`ipm/ipm_solver_globalization.cpp`, for example, orders
its includes alphabetically and pulls in two headers outside the shared
set), so prefixing the shared block changes template instantiation order
and therefore emission order. Admitting them would buy roughly 5.4 s
more; doing so means relaxing bar 2, which is a numerics-governance
decision, not a build decision.

### If you edit `src/hven_pch.h`

The header's include list is the include block of
`src/ipm/ipm_solver.cpp`, verbatim and in the same order.
That ordering is load-bearing — it is what makes the byte-identity
property hold, which is why both lists carry a `// clang-format off`
guard (clang-format would otherwise alphabetize them). After any edit,
run `scripts/check_pch_neutrality.sh` and update the participating-TU
list to whatever still qualifies.

`src/CMakeLists.txt` also refuses to configure if the opt-in list stops
describing the build: it fails if a listed name matches no real source,
if the opt-out fails to take effect for any non-participating source, or
if the target's source count changes without the list being revisited.

## ccache

The build uses ccache automatically when it is on `PATH`. Two settings
matter on a PCH-using tree.

Put this in `~/.config/ccache/ccache.conf`:

```ini
max_size = 40G
sloppiness = pch_defines,time_macros
```

- **`max_size`** — the default (5G) is too small for a tree of this
  shape; a full build writes several GiB and the cache thrashes rather
  than helping.
- **`sloppiness = pch_defines,time_macros`** — this is what makes
  PCH-consuming compiles cacheable at all. Without it ccache refuses
  them, which on this tree means refusing most of the expensive ones.

Two clarifications, because the folklore around this is wrong often
enough to be worth stating precisely:

- **`-Xclang -fno-pch-timestamp` is a requirement, not an
  optimization.** ccache's manual is explicit that Clang embeds a
  timestamp in the PCH, and that using ccache with a Clang PCH requires
  compiling it with this flag. `src/CMakeLists.txt` passes it under
  Clang for exactly that reason. It is not a tuning knob that buys a few
  extra cache hits — without it the arrangement does not work.
- **`pch_external_checksum` is inert here, and is deliberately not in
  the config above.** It does not "make consumers hash the PCH" —
  hashing the PCH is already what ccache does by default. The setting
  tells ccache to hash a `<pch>.sum` file *instead of* the PCH itself
  when one exists, as a performance workaround for very large
  precompiled headers. Nothing in this build emits a `.sum` file, so the
  setting would do nothing. It was recommended here in an earlier
  revision of this page; that was a misreading of the manual.

- **A GCC `.gch` is not safe to cache, which is why there is no longer
  one to cache.** GCC bakes a table of (file size, content md5) pairs
  into the `.gch` and that table is the *only* thing keeping a consuming
  TU from re-reading a `#pragma once` header the PCH already holds; GCC
  never re-checks that the pairs still describe the files on disk, and
  there is no macro include guard to fall back on. ccache's
  preprocessor-mode key, meanwhile, is the preprocessed text, which a
  comment-only, line-count-preserving header edit leaves byte-identical
  — so it replays a `.gch` built before such an edit while the file on
  disk no longer matches the table inside it, and every consuming TU
  then fails with `multiple definition of 'enum class …'`. Measured on
  GCC 16.2.1 with ccache 4.12.3: such an edit to
  `include/hven/core/solver_status.h` gets a `preprocessed_cache_hit`
  and a served `.gch` byte-identical to the pre-edit one. This is not a
  hypothetical — it is what failed M7 W2 E4's install-smoke gate on
  sources that build it green. Clang is unexposed because it records
  each input's size and mtime in the AST file and refuses one whose
  inputs moved, so the same stale artifact is a loud refusal rather than
  a silent re-parse. The remedy is the compiler guard in
  `src/CMakeLists.txt`, not a ccache setting: nothing in a ccache config
  makes a GCC `.gch` validate its own inputs.

The safeguard that actually protects the verification path is none of
these: `scripts/check_pch_neutrality.sh` exports `CCACHE_DISABLE=1` for
both of its builds, so the byte-identity comparison always compiles the
sources in front of it rather than replaying whatever a cache happened
to hold.

## Disclosed limitations

Three things this setup does not prove, stated so nobody has to infer
them:

- **The membership bars are Linux-and-clang measurements.** Both the
  timing table and the byte-identity results were produced on Linux with
  clang. They are assumed, not verified, to transfer to the macOS and
  Windows lanes; those lanes assert only that the PCH is *engaged* on
  the expected six TUs, not that it is byte-neutral there. A full
  neutrality run on macOS is Mac-leg work.
- **Byte-identity holds for clang, and does not hold for GCC.** Measured
  on GCC 16.1.1, three of the six opted-in TUs
  (`ipm/ipm_options.cpp`, `non_linear_program.cpp`,
  `nlp_adapter.cpp`) emit a different object with the PCH than without
  it. Clang is the toolchain `CLAUDE.md` documents, every preset selects
  and all three CI lanes use, so this was already outside the supported
  configuration — but it is a real boundary on the claim, not a
  rounding error. `scripts/check_pch_neutrality.sh` defaults to clang
  for this reason and prints the compiler it used. As of M7 W2 E4 fix
  round 4 the boundary is closed by construction rather than by
  convention: no GCC build gets a PCH, so there is no GCC object whose
  identity is in question. Running that script with
  `-DCMAKE_CXX_COMPILER=g++` now fails its engagement assertion, and
  correctly so — under GCC there is no PCH for it to find.
- **tycho-embedded composition is build-verified, not
  identity-verified.** When tycho consumes hven via `add_subdirectory`,
  the two projects' precompiled headers are independent per-target
  objects and were confirmed to build cleanly together. That check did
  not compare object bytes in the embedded configuration. If that proof
  is ever needed, `scripts/check_pch_neutrality.sh` is runnable
  standalone against any pair of build directories.

## A note on diagnostics

CMake compiles its generated PCH wrapper with system-header semantics.
A practical consequence: warnings originating inside the shared header
set are suppressed in the six TUs that consume the PCH. This is not a
loss of coverage in practice — the test suite compiles those same
headers without the PCH — but it does mean a new warning introduced in
one of those headers will surface from the tests rather than from the
library build.

## MKL threading layer (PF-6)

`HVEN_MKL_THREADING` selects which MKL threading layer hven links on Linux and
Windows. It has no effect on Apple (Accelerate has no such choice).

| value | links | OpenMP runtime | `threads > 1` |
|---|---|---|---|
| `intel` (default) | `mkl_intel_thread` + `libiomp5` (`libiomp5md` on Windows) | linked, redistributed | multi-threaded Pardiso / LAPACK |
| `sequential` | `mkl_sequential` | none | **ignored** (runs single-threaded) |

```
cmake -S . -B build -DHVEN_MKL_THREADING=sequential
```

**The choice is recorded at install time.** `hvenConfig.cmake` replays it before
`find_dependency(MKL)`, so a `find_package(hven)` consumer links the layer hven
was built with; it cannot mix `mkl_sequential` with a hven built for
`libiomp5` (or the reverse). A consumer using `FindMKL.cmake` directly gets the
same choice through `MKL_USE_SEQUENTIAL`. Switching the option in an existing
build tree is safe: `FindMKL.cmake` drops its cached layer-dependent library
paths when the layer changes. Under `sequential` the `-fopenmp` compile flag is
not added either, and `libiomp5` is neither searched for, linked, nor placed on
the rpath (Windows: the OpenMP redistributable is not needed by hven's binaries).

**What `threads > 1` does under `sequential`: it is ignored, not refused.**
`SymmetricFactor::Options::threads`, `set_threads()`, and the solver options'
thread fields keep their meaning as a *request*; the backend honours it only if
it can. That is the contract the field already had ("best-effort-absent on
backends with no thread control", and Apple stores and applies nothing). Refusing
at `validate()` would make one options struct valid on one build of the library
and an error on another, which breaks the one-source-two-builds use (a consumer
or a test suite that sets `threads = 4` and is then built both ways). MKL itself
accepts `mkl_set_num_threads_local(n)` under `mkl_sequential` as a no-op, so no
code path differs and no iparm value changes. `SymmetricFactor::threads()` still
reports the stored request, not an effective width. A consumer that needs
multi-threaded Pardiso must build with `intel`.

## LTO installs (M6 W6 T3, 2026-09-14)

`HVEN_LINK_TIME_OPT` is **OFF by default** and W6 did not move that default.
The reason it is off is older than this section — "thin-LTO was measured to buy
this library nothing (M4 Task 9's bench legs)", `CMakeLists.txt:79-80` — and M6
W6 T3 did **not** re-measure it: that task asserted no wall-clock number at all
and says nothing about whether an LTO build is faster or slower. What T3 did do
is turn the option on once, end to end, and write down what turning it on does
to an **install**. The evidence is `docs/notes/data/2026-09-m6-w6-lto/`
(`README.md` sections 5 and 7 (b)-(d); `install-smoke.txt`, whose section (iii)
tables the options below).

**The constraint.** With `HVEN_LINK_TIME_OPT=ON`, CMake emits `-flto=thin` for
this clang, and the members of the installed `libhven.a` are **LLVM IR
bitcode** where an ordinary build's are ELF relocatables. On the reference box a
**clang 22.1.8** consumer that does **not** enable LTO for itself then fails to
link the installed package, because `clang++` drives `/usr/bin/ld.bfd`
(GNU ld 2.46.1) with no LLVM plugin loaded and BFD ld cannot read a bitcode
archive member:

```
/usr/bin/ld.bfd: .../libhven.a: error adding symbols: file format not recognized
```

Three of the four consumer configurations measured **pass**: GCC 16.2.1 with the
consumer's own IPO off and on (GCC's LTO members are ELF files BFD ld reads
directly), and clang with its own IPO on (which puts `-flto=thin` on the
consumer's link line and gets an LTO-capable link). `check_export_contract.sh`
passed on **all four** install prefixes, the failing one included — the library
installs correctly; only that one consumer link fails. A clang consumer linking
through `lld`, or through a `ld.bfd` that can load `LLVMgold.so`, was **not
run** and nothing is claimed for it. Windows and Apple are **UNOBSERVED**.

**Documented, not remedied.** W6 states the constraint and builds no fix, and
the default posture is unchanged either way. The registered remedy candidate —
for the next window that touches the exported target, and with tycho's consume
input, since tycho is the consumer this would reach — is a **conditional
link-only `-flto` usage requirement**: export `INTERFACE_LINK_OPTIONS` carrying
`-flto` when, and only when, the library was built with IPO. That is inside the
export contract as it is written: `scripts/check_export_contract.sh` forbids
`INTERFACE_COMPILE_OPTIONS` (`:21-24`, enforced at `:113-121`) and asserts
nothing about `INTERFACE_LINK_OPTIONS`, so no contract widening is needed —
only a compile-side usage requirement would reopen it. `-ffat-lto-objects` is
**rejected for now**: bigger archives and slower builds, with clang support
unvalidated on this toolchain, bought for a configuration no consumer has asked
for.

**What the install smoke does and does not exercise here.**
`scripts/check_install_smoke.sh` pins no compiler — both of its configures use
whatever `c++` CMake's default search finds, which on this box and in CI is GCC,
the front end whose arms pass. So the CI shape of that check does not exercise
clang's LTO at all, and would stay green even if this constraint bit a clang
consumer. A caller that needs a particular front end passes
`-DCMAKE_CXX_COMPILER=...` as an argument to the script, which forwards every
argument to both configures; that is how T3's two clang arms were taken. No CI
matrix arm was added for LTO: it would duplicate a lane to guard a switch that
is off, and it would go green at the GCC front end while the clang consume path
stayed broken. If anything is added later, it is one smoke invocation with LTO
on at the clang front end, **after** the remedy above is ruled on.


## History: the source-list banners

Moved verbatim from `src/CMakeLists.txt` at RS-R1 (2026-11-19), where the sources were listed in
migration order under per-block banners. Paths and block names in the text are as they stood
(`drivers/`, `interior/`, `globalization/` are the retired layout); the current list is one block
per component in layer order.

```cmake
################################################################################
# hven library sources — appended to the `hven` target created in the root
# CMakeLists.txt (target_sources, not add_library: the target's include dirs,
# linked dependencies, and compile options are already set up there).
################################################################################

target_sources(hven PRIVATE
    core/version.cpp
    core/pattern_hash.cpp
    core/ledger.cpp
    # The display-string switches for the two solver enums core/ owns
    # (SolveStatus, StartLevel). They are in a core/ TU, not the sqp/ printing
    # TU that carries the other three, so that no core/ object resolves a symbol
    # out of an sqp/ one -- see the file's own banner.
    core/enum_names.cpp
    linear/dense_symmetric_factor.cpp
    # The dense arm behind SymmetricFactor: both platforms run the same LAPACK
    # calls (MKL's LAPACKE, or the Accelerate shim), as the file above does.
    linear/dense_session.cpp
    # The maximum-product matching and its symmetric scaling (Duff & Koster
    # 2001, Duff & Pralet 2005): backend-independent sparse preprocessing the
    # KKT factor's platform-solve response applies before a retake.
    linear/matching_scaling.cpp
    kkt/kkt_factor.cpp
)

# The sparse symmetric factor's two backends are split at the source level,
# not inside a shared TU. This is the opposite choice from
# linear/dense_symmetric_factor.cpp, and for a reason: there both platforms
# run the same LAPACK calls and differ only in which LAPACKE declarations
# they include, so one TU with a two-line #if is the whole difference. The
# sparse engines share no code at all — MKL Pardiso's phase-driven session
# and Accelerate's factor objects have nothing in common below the public
# header — so each backend gets its own TU and only the platform's own is
# ever compiled. A file that would #if itself away to nothing is a TU the
# build should not be compiling in the first place.
if(NOT APPLE)
    target_sources(hven PRIVATE
        linear/pardiso_session.cpp
        linear/symmetric_factor_mkl.cpp
    )
else()
    # This branch is configured, built, and tested against the real Accelerate
    # framework on every macOS CI run. The Linux syntax-only pass against a
    # stub Accelerate.h (scripts/check_accelerate_syntax_linux.sh) remains a
    # narrower structural cross-check; docs/testing.md states its claim ceiling.
    target_sources(hven PRIVATE
        linear/accelerate_session.cpp
        linear/symmetric_factor_accelerate.cpp
    )
endif()

################################################################################
# The provider-side model contract (include/hven/model/). Its own block rather
# than part of the engine block below: the contract is what any provider
# implements and any engine consumes, not one engine's implementation.
#
# The declaration needs a TU because it holds its piece lists by value while its
# header only declares the piece handle types, so its special members and its
# boundary validation are defined where those types are complete. The NlpModel
# bridge needs one because it is an implementation rather than a contract
# surface, and it sits in this block rather than the engine block below because
# it is a second provider of the contract and names no engine type. The
# triplet-to-native conversion sits here on the same footing: it is a provider
# of the model contract and names no engine type either, and so do the Nlp
# callback contract's default members.
################################################################################

target_sources(hven PRIVATE
    model/nlp.cpp
    model/assembly_declaration.cpp
    model/validation_errors.cpp
    model/nlp_model_assembly.cpp
    model/nlp_problem_model.cpp
)

################################################################################
# The shared solve-outcome vocabulary (core/solver_status.h), the interior-point
# phase-status resolution (detail/ipm/ipm_phase_status.h), the shared declared
# diagnostics and the one-line stop message. Its own block because it belongs to neither engine: the status both report and the
# mappings from the two engine-native enums onto it. A TU on the same footing as
# core/enum_names.cpp -- switches over enums, not a per-element path.
################################################################################

target_sources(hven PRIVATE
    ipm/ipm_phase_status.cpp
    solver/solve_result.cpp
    solver/result_message.cpp
    solver/callback_rows.cpp
    solver/counters_dump.cpp
)

################################################################################
# Interior-point engine — the top-level IpmSolver algorithm
# (ipm/ + solver/), the Nlp contract with its adapter and the
# partitioned-evaluation NonLinearProgram engine (model/), and the barrier /
# interior-point machinery under include/hven/detail/ (barrier/ + kkt/ +
# ipm/globalization/), split along those same seams.
################################################################################

target_sources(hven PRIVATE
    ipm/ipm_solver.cpp
    ipm/ipm_solver_handle.cpp
    ipm/ipm_solver_globalization.cpp
    solver/common_options.cpp
    ipm/ipm_options.cpp
    ipm/solver_init.cpp
    model/non_linear_program.cpp
    model/nlp_adapter.cpp
    model/problem.cpp
    kkt/kkt_factorization.cpp
    util/get_core_count.cpp
    util/memory_management.cpp
    util/thread_pool.cpp
)

################################################################################
# SQP engine
#
# The SQP engine's code lives in include/hven/sqp/, include/hven/detail/sqp/,
# and these TUs. Each TU below is a carve of header-side code into a .cpp
# under CLAUDE.md §5's rule that orchestration, printing, and options belong
# outside headers; each file's own banner carries its argument:
#
#   - sqp/sqp_print.cpp: the printing surface.
#
#   - trace/json_lines_trace_sink.cpp: JsonLinesTraceSink, the machine-trace schema
#     v0 serializer (M6 W4). Printing code by any reading -- one ostream, one
#     line per event, no arithmetic on the values it formats -- so section 5
#     selects a TU for it on exactly the ground sqp_print.cpp stands on. It is
#     off by default: nothing in the library constructs one.
#
#   - sqp/sqp_options.cpp: SqpOptions boundary validation. Its banner
#     carries the ONE flag premise in this tree that a comparison's spelling
#     depends on -- `-fno-finite-math-only`, without which `!(x > 0.0)` may be
#     rewritten to `x <= 0.0` and stop rejecting NaN -- and a battery pins
#     that premise by disassembling this TU.
#
#   - sqp/assembly_eval_seam.cpp and sqp/sqp_solver.cpp: the driver's
#     consumer-side seam and the major loop itself (SqpSolver::solve_impl plus
#     everything it orchestrates), together with the free functions
#     sqp/sqp_solver.h declares alongside the class. That TU's banner
#     carries the counter-identity bar any redraw of the boundary must clear,
#     and the ruling under which the TR update functions travel with the loop
#     while the TR policy constants stay in globalization. The object-file
#     evidence that the carve removed no inlining is in the W0.1c report, not
#     in that banner.
#
#   - sqp/problem_scaling.cpp: the W0.2 problem-scaling layer's factor
#     computation. Options-shaped code that runs once per solve, so section 5
#     selects a TU for it on the same ground sqp_options.cpp stands on; it
#     carries the same `!(x > 0.0)` NaN-rejection idiom and names that file's
#     banner for the flag premise. The APPLYING of the factors is not here --
#     it stays beside the values it multiplies, in sqp/assembly_eval_seam.cpp.
#
#   - sqp/globalization/funnel.cpp: FunnelStrategy's acceptance state machine,
#     the first carve whose arithmetic is re-associable under -ffast-math; its
#     proof is bit-identity of every asserted counter across the boundary.
#
#   - sqp/globalization/soc_elastic_restoration.cpp: ONE TU for the three
#     step-recovery tiers (second-order correction, elastic l1 exact-penalty
#     tier, restoration phase's feasibility-model wrapper), because the three
#     tiers are entered from one place in solve_impl.
#
#   - warmstart/crossover.cpp and sqp/continuation.cpp: the interior-
#     point crossover's ingest and the continuation sweep. detail::
#     ip_activity_threshold stays a header template so the ingest can pass an
#     Eigen expression rather than materialise two n-sized temporaries.
#
#   - qp/walk/walk_engine.cpp: QpEngine's primal active-set walk -- every member
#     function of it except the constructor. The engine is a plain, non-
#     template class, so section 5's header/templated exception selects none of
#     it: what is here is orchestration. Every member travels into THIS ONE TU
#     deliberately -- the walk's per-element helpers (in_working,
#     row_tolerance, has_border, dropped_directional, mark_{ineq,bound}_seen)
#     are called from inside the ratio test's and sync_borders' own loops, so
#     they must stay visible to the inliner at exactly the sites that had them
#     as header siblings. The only boundary this carve introduces is
#     engine-to-caller, crossed once per QP subproblem. The `detail::` free
#     functions above QpEngine stay inline in the header: tests, bench and
#     sqp/sqp_solver.cpp name them directly.
#
#   - qp/ssn/ssn_engine.cpp: SsnEngine's safeguarded semismooth-Newton iteration --
#     every member function of it except the constructor and the two trivial
#     accessors (options(), has_deferred_certification()). The engine is a
#     plain, non-template class, so section 5's header/templated exception
#     selects none of it: what is here is orchestration -- the attempt loop,
#     the FB classification, the KKT pattern cache, the globalization helpers
#     and the exports. Every member travels into THIS ONE TU deliberately:
#     select_branch runs once per FB row from inside the attempt loop's own
#     two row loops, residual() walks every block once per attempt and once
#     per line-search trial, and for_each_entry's emit callable is invoked
#     once per stored entry of K from sync_matrix -- so all three must stay
#     visible to the inliner at exactly the sites that had them as header
#     siblings. for_each_entry is a member TEMPLATE over that callable and
#     travels WITH its only two instantiation sites (sync_matrix's rebuild
#     and refresh lambdas), which is what keeps its type set closed inside
#     this TU; leaving it behind in the header is the one split that would
#     have cost inlining. The only boundary this carve introduces is
#     engine-to-caller -- solve(), finish_deferred_certification(),
#     discard_deferred_certification() -- crossed at most twice per QP
#     subproblem. The `detail::` free functions and constants above SsnEngine
#     stay inline in the header: tests, bench, sqp/sqp_solver.cpp and
#     warmstart/warm_start.h name them directly.
################################################################################

target_sources(hven PRIVATE
    sqp/assembly_eval_seam.cpp
    trace/console_trace_sink.cpp
    sqp/equality_basis.cpp
    sqp/problem_scaling.cpp
    sqp/sqp_solver.cpp
    sqp/sqp_solver_handle.cpp
    sqp/sqp_options.cpp
    sqp/sqp_print.cpp
    trace/trace.cpp
    trace/json_lines_trace_sink.cpp
    sqp/globalization/funnel.cpp
    sqp/quasi_newton/lbfgs_damped.cpp
    sqp/quasi_newton/mixed_lift.cpp
    sqp/quasi_newton/qn_lift_bridge.cpp
    sqp/quasi_newton/qn_models.cpp
    sqp/globalization/soc_elastic_restoration.cpp
    qp/ipqp/ipqp_engine.cpp
    qp/ipqp/ipqp_kkt_layout.cpp
    qp/walk/walk_engine.cpp
    qp/ssn/ssn_engine.cpp
    qp/walk/ws_independence.cpp
    sqp/continuation.cpp
    warmstart/crossover.cpp
)

################################################################################
# The warm-start currency (include/hven/warmstart/warm_start.h). Its own
# block rather than part of the engine blocks above, because it is neither
# engine's: WarmStart is the value a consumer holds BETWEEN solves, and the
# component names no engine type and includes no engine header. Its TU carries
# the byte form -- the layout, the bounds-checked decode, and the refusals --
# which is orchestration-shaped, non-template code that CLAUDE.md section 5
# puts in a .cpp regardless of the surrounding loop.
#
# warmstart/ipm_polish_extension.cpp sits in this block rather than in the
# interior-point block above, even though the interior-point engine is what
# PRODUCES its payload. The extension is a VALUE the currency carries and a
# bridge onto the SQP crossover; it names no engine type either, and its two
# ends belong to two different engines, so filing it under one of them would
# misdescribe it. Same shape of code as its neighbour -- a byte layout, a
# bounds-checked decode, refusals -- and the same CLAUDE.md section 5 reason
# for a .cpp.
################################################################################

target_sources(hven PRIVATE
    warmstart/ipm_polish_extension.cpp
    warmstart/warm_start_data.cpp
)
```

## History: the PCH sweep table

The per-TU measurement table of the original sweep, moved verbatim from `src/CMakeLists.txt`.
Paths are as they were when the sweep ran; the rows record measurements of files some of which
no longer exist.

```
# Measured, all 18 Linux TUs as of the original sweep (no-PCH -> with-PCH,
# seconds; identical = object byte-for-byte equal). Participating TUs first:
#
#   drivers/interior_point_solver.cpp             6.99 -> 5.04  -1.95  identical
#   drivers/non_linear_program.cpp                6.54 -> 4.66  -1.88  identical
#     (this source has since moved to model/non_linear_program.cpp; the row
#     above is left at its original path, recording the sweep as it ran)
#   drivers/interior_point_solver_settings.cpp    4.00 -> 2.45  -1.55  identical
#     (renamed to ipm/ipm_options.cpp at M6 W5 T8.3, which also deleted the
#     ~50 set_*() bodies and the four strto_*() parsers this row was measured
#     over; the row is left as it ran, and the shrunken TU's own PCH benefit is
#     UNMEASURED -- registered for the next measurement pass, not re-derived
#     here)
#   drivers/interior_point_solver_print.cpp       4.09 -> 2.56  -1.53  identical
#   model/nlp_solver.cpp                          4.09 -> 2.56  -1.53  identical
#     (this source was DELETED at M6 W5 T8.9 with the NLPSolver wrapper; the
#      row is left as it ran)
#   model/nlp_adapter.cpp                         4.61 -> 3.15  -1.46  identical
#     ------ opted out, faster but NOT byte-identical ------
#   drivers/interior_point_solver_globalization.cpp 4.94 -> 3.39 -1.55  differs
#   linear/symmetric_factor_mkl.cpp               3.41 -> 2.70  -0.71  differs
#   linear/pardiso_session.cpp                    3.20 -> 2.50  -0.70  differs
#   interior/kkt_factorization.cpp                2.97 -> 2.27  -0.70  differs
#   core/pattern_hash.cpp                         2.98 -> 2.29  -0.69  differs
#   linear/dense_symmetric_factor.cpp             3.18 -> 2.53  -0.65  differs
#   interior/utils/memory_management.cpp          1.41 -> 1.04  -0.37  differs
#     ------ opted out, slower with the PCH ------
#   core/version.cpp                              0.02 -> 0.98  +0.96
#   interior/utils/get_core_count.cpp             0.70 -> 1.14  +0.44
#   interior/utils/thread_pool.cpp                0.72 -> 1.12  +0.40
#   interior/utils/color_text.cpp                 0.70 -> 0.98  +0.28
#   drivers/solver_init.cpp                       1.34 -> 1.48  +0.14
#
# The "differs" group is not a correctness problem -- disassembly comparison
# showed identical instructions per symbol, with only function layout order and
# local label numbering shifting. It differs because those TUs include a
# different header set in a different order (interior_point_solver_globalization.cpp,
# for instance, orders its includes alphabetically and pulls in two headers
# outside the shared set), so prefixing the shared block changes template
# instantiation order and hence emission order. They are excluded because
# byte-identity is the bar this project holds for engine code, not because
# their code is wrong. Relaxing bar 2 to "identical instructions" would buy
# roughly 5.4 s more; that is a numerics-governance call, not a build call.
#
```

## History: the source count

The dated notes that accompanied each increment of `_hven_expected_source_count` (29 to 58), moved
verbatim from `src/CMakeLists.txt`. The current value and the tripwire's purpose are in that file.

```
# 29 as of model/assembly_declaration.cpp. That TU stays OUT of the opt-in
# list: the list admits a source only on measured evidence (faster with the
# PCH, and byte-identical object), and this one has neither measurement. Out is
# the unmeasured default and costs nothing but its own compile time -- it is a
# small TU that includes the two piece headers and fmt, not the shared block the
# PCH prefixes.
#
# 30 as of model/nlp_model_assembly.cpp, and it stays out on the same footing:
# no measurement exists for it, and out is the unmeasured default. It includes
# the model contract, the model interface, Eigen sparse and fmt -- a different
# header set from the one src/hven_pch.h prefixes, which is the property that
# has failed bar 2 for every TU measured so far.
#
# 31 as of sqp/assembly_eval_seam.cpp, the SQP driver's consumer-side
# binding onto the provider contract. It stays OUT on the same footing as the
# two above -- no measurement exists for it, and out is the unmeasured default
# -- and on the stronger prior the SQP-side carves established: it includes
# sqp/sqp_solver.h (for NlpEval) plus the model contract, which is a
# different header set in a different order from the one src/hven_pch.h
# prefixes, and that property has failed bar 2 for all eight SQP-side TUs
# measured so far.
#
# 32 as of model/nlp_problem_model.cpp, the triplet-to-native conversion. It
# stays OUT on the same footing as the three above -- no measurement exists for
# it, and out is the unmeasured default. It includes the model contract, the
# triplet problem surface, Eigen sparse and fmt: again a different header set
# in a different order from the one src/hven_pch.h prefixes.
#
# 33 as of warmstart/warm_start_data.cpp, the warm-start currency's byte form.
# It stays OUT on the same footing as the four above -- no measurement exists
# for it, and out is the unmeasured default. It includes the currency header,
# the model contract's structure-identity header, and fmt: again a different
# header set in a different order from the one src/hven_pch.h prefixes.
#
# 34 as of warmstart/ipm_polish_extension.cpp, the interior-point polish
# extension's byte form and its crossover bridge. It stays OUT on the same
# footing as the five above -- no measurement exists for it, and out is the
# unmeasured default. It includes the currency header, the crossover header
# (which pulls the QP types and the working set), the model contract's
# structure-identity header, and fmt: again a different header set in a
# different order from the one src/hven_pch.h prefixes.
#
# 35 as of qp/walk/walk_engine.cpp, the QP walk carved out of
# include/hven/detail/qp/walk/walk_engine.h. It stays OUT on the same footing as the
# six above -- no measurement exists for it, and out is the unmeasured default
# -- and on the SQP-side prior: it includes the QP engine header, which pulls
# the KKT assembly, the border stack, the sparse factor and the QP types, a
# different header set in a different order from the one src/hven_pch.h
# prefixes, and that property has failed bar 2 for every SQP-side TU measured
# so far.
#
# 36 as of qp/ssn/ssn_engine.cpp, the semismooth-Newton iteration carved out of
# include/hven/detail/qp/ssn/ssn_engine.h. It stays OUT on the same footing as the
# seven above -- no measurement exists for it, and out is the unmeasured
# default -- and on the SQP-side prior: it includes the SSN engine header,
# which pulls the KKT call helpers, the QP problem and QP types, the sparse
# factor, the pattern hash and the SQP types, a different header set in a
# different order from the one src/hven_pch.h prefixes, and that property has
# failed bar 2 for every SQP-side TU measured so far.
#
# 37 as of sqp/problem_scaling.cpp, the M6 W0.2 problem-scaling layer's
# factor computation. It stays OUT on the same footing as the eight above --
# no measurement exists for it, and out is the unmeasured default. It is also
# the cheapest TU in the target to compile: two standard headers and one
# self-contained project header (detail/sqp/problem_scaling.h, which pulls
# only core/types.h), so it shares none of src/hven_pch.h's prefix and has
# almost nothing for a PCH to save.
#
# 38 as of qp/ipqp/ipqp_kkt_layout.cpp, the M6 W1 interior-point QP tier's KKT
# scatter plan. It stays OUT on the same footing as the nine above -- no
# measurement exists for it, and out is the unmeasured default. Its include
# set is small and shares little with src/hven_pch.h's prefix: its own header
# (which pulls only core/types.h), Eigen/SparseCore, fmt/format.h,
# core/pattern_hash.h, and three standard headers. It names no QP engine or
# QP problem type at all -- the layout is written against the three sparse
# blocks and their dimensions, not against QpProblem -- so there is even less
# for a shared PCH to save here than for the SQP-side TUs above it.
#
# 39 as of qp/ipqp/ipqp_engine.cpp, the M6 W1 interior-point QP tier's iteration:
# the clamp-centred box, the Mehrotra predictor-corrector, the monotone
# (rho, delta) ladder and the relative-KKT stopping rule. It stays OUT on the
# same footing as the ten above -- no measurement exists for it, and out is the
# unmeasured default -- and on the SQP-side prior: it includes the tier's own
# header (which pulls the QP problem, the QP types, the SQP types, the KKT
# factorization and the scatter plan), the QP engine header and the SSN engine
# header (for the InertiaVerdict perturbed-pivot policy and the ladder's
# cap-slack constants, both reused verbatim rather than re-derived), a
# different header set in a different order from the one src/hven_pch.h
# prefixes, and that property has failed bar 2 for every SQP-side TU measured
# so far.
#
# 40 as of qp/ipqp_trace.cpp (M6 W1 task 8 fix round 1, R6; the file is
# trace/trace.cpp since M6 W5 T4): one out-of-line defaulted destructor for
# TraceSink, one include. Stays OUT on the same unmeasured-default footing as
# the eleven above -- there is nothing here for a shared PCH to save.
#
# 41 as of trace/json_lines_trace_sink.cpp (M6 W4 task 1): the machine-trace schema v0
# serializer. Stays OUT on the same unmeasured-default footing as the twelve
# above. It includes its own header (core/types.h plus trace/trace.h),
# fmt/format.h and four standard headers -- a different set in a different
# order from the one src/hven_pch.h prefixes, which is the SQP-side prior that
# has failed bar 2 for every such TU measured so far.
#
# 42 as of ipm/ipm_phase_status.cpp (M6 W5 T8.2): the shared status vocabulary's
# switches. Stays OUT on the same unmeasured-default footing as the thirteen
# above. It includes its own header (core/solver_status.h plus the interior-point
# forward header), fmt/format.h and <stdexcept> -- a different set in a different
# order from the one src/hven_pch.h prefixes.
#
# 43 as of solver/solve_result.cpp (M6 W5 T8.4): the shared declared
# diagnostics' one definition. Stays OUT on the same unmeasured-default footing
# as the fourteen above. It includes its own header (core/types.h,
# warmstart/warm_start.h), fmt/format.h and
# three standard headers -- a different set in a different order from the one
# src/hven_pch.h prefixes.
#
# 44 as of trace/console_trace_sink.cpp (M6 W5 T8.7): both engines' console
# tables, rendered from the trace stream. NOT a PCH source, on the same
# unmeasured-default footing as the fifteen above -- it includes its own header
# (trace/trace.h, fmt/color.h, <cstdio>), sqp/sqp_solver.h,
# ipm/ipm_iterate.h and three standard headers, again a different
# set in a different order from src/hven_pch.h's prefix.
#
# 43 as of M6 W5 T8.7b, which DELETED drivers/interior_point_solver_print.cpp.
# That file held the interior-point console table; T8.7 moved all but three of
# its functions into the sink above, and T8.7b moved those three
# (print_beginning, print_finished, print_exit_stats) with the events that
# carry them -- ipm.phase.begin, ipm.phase.end and ipm.phase.exit. It was a PCH
# source (measured 4.09 -> 2.56 s, byte-identical, in the original sweep) and
# its entry is removed from _hven_pch_sources with it; the sweep row above is
# left as it ran, recording a measurement of a file that no longer exists.
# THIS IS THE FIRST TIME THE COUNT HAS GONE DOWN, and it goes down because the
# engine has no printing TU left at all: there is no fmt::print anywhere in
# src/ipm/ipm_solver.cpp.
#
# 43 as of sqp/equality_basis.cpp (M7 W2 E1): the SQP driver's independent
# equality basis. NOT a PCH source, on the same unmeasured-default footing as
# the others outside the list: it includes its own header, Eigen's SparseQR,
# OrderingMethods and SparseCholesky modules, fmt/format.h and four standard
# headers, a different set from src/hven_pch.h's prefix.
#
# 44 as of qp/walk/ws_independence.cpp (M7 W2 E2): the QP walk's working-set
# dependency test and entry detector. NOT a PCH source, on the same
# unmeasured-default footing.
#
# 45 as of sqp/quasi_newton/lbfgs_damped.cpp (M7 QN stage 1): the damped compact
# L-BFGS math class. NOT a PCH source, on the same unmeasured-default footing: it
# includes its own header (Eigen/Dense), fmt/format.h and standard headers, a
# different set from src/hven_pch.h's prefix.
#
# 46 as of sqp/quasi_newton/mixed_lift.cpp (M7 QN stage 2): the quasi-Newton mixed-lift QP
# adapter. NOT a PCH source, on the same unmeasured-default footing: it includes its own
# header, fmt/format.h and standard headers, a different set from src/hven_pch.h's prefix.
#
# 47 as of sqp/quasi_newton/qn_lift_bridge.cpp (M7 QN stage 3): the quasi-Newton engine-call
# decorator. NOT a PCH source, on the same unmeasured-default footing.
# 48 as of sqp/quasi_newton/qn_models.cpp (M7 QN quality): the full-memory damped BFGS and
# the nonlinear-variables-only restriction. NOT a PCH source, on the same unmeasured-default
# footing.
# 49 as of model/validation_errors.cpp (M8 AP-2 B1): the cold throw-message paths of the model
# contract's inline checks. NOT a PCH source, on the same unmeasured-default footing: it
# includes fmt and the four model headers, a different set from src/hven_pch.h's prefix.
# 50, 51 as of ipm/ipm_solver_handle.cpp and sqp/sqp_solver_handle.cpp (M8 AP-2 B1):
# the public IpmSolver / SqpSolver handles' forwarding bodies. NOT PCH sources, on the same
# footing: each includes only its implementation header.
# 52, 53 as of model/nlp.cpp and model/problem.cpp (M8 AP-2 B2): the Nlp contract's default
# members and the Problem every solve takes. NOT PCH sources, on the same unmeasured-default
# footing: nlp.cpp includes its own header and fmt; problem.cpp the model transcription
# headers in its own order, a different set from src/hven_pch.h's prefix.
# 54 as of solver/common_options.cpp (M8 AP-2 B3): the one CommonOptions validator. NOT a PCH
# source, on the same unmeasured-default footing: it includes only its own header, fmt and
# the standard library.
# 55 as of solver/result_message.cpp (M8 AP-2 B4): the one-line stop message built by the
# public handles. NOT a PCH source, on the same footing: it includes only its own header,
# the option and result headers, fmt and the standard library.
# 56 as of solver/callback_rows.cpp (M8 AP-2 B4): the row-space compose of an iteration
# event's multipliers. NOT a PCH source, on the same footing: it includes only its own header
# and the problem access header.
# 57 as of linear/dense_session.cpp (M8 DN-1): the dense arm behind SymmetricFactor. NOT a PCH
# source, on the same unmeasured-default footing: it includes its own header, fmt, the LAPACKE
# header (or the Accelerate shim) and the standard library.
# 58 as of solver/counters_dump.cpp (M8 TR-E): dump_counters for both results, written from the
# counters' X-macro tables. NOT a PCH source, on the same footing: it includes only the counter and
# result headers, fmt and the standard library.
# 57 as of RS-1a: util/color_text.cpp (enable_color_console, no caller) deleted with its private header.
# 58 as of linear/matching_scaling.cpp (M8 sparse-equil-v4): the maximum-product matching and its
# symmetric scaling. NOT a PCH source, on the same unmeasured-default footing: it includes its own
# header, fmt and the standard library, a different set from src/hven_pch.h's prefix.
# 59 as of RS-R1 (qp/ipqp/ipqp_options.cpp added; trace_writer.cpp renamed json_lines_trace_sink.cpp).
# 60 as of qp/qp_dump_writer.cpp (M8 I17; recorded here by PF-4).
# 61 as of linear/analysis_tally.cpp (M8 PF-4): the per-thread analysis tally both platform adapters
# record into. NOT a PCH source, on the same unmeasured-default footing: it includes the public factor
# header and <unordered_set> only.
```
