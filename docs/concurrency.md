# The concurrency contract

hven provides **thread-safe solves**. It does not provide a parallel-solve
facility, and it will not grow one: handing N problems to N worker threads and
collecting the results is orchestration, and orchestration belongs to the
consumer. That is the owner's ruling of 2026-09-11
(`docs/notes/2026-08-m6-ledger.md:4421-4431`), which also deleted
`detail/interior/jet.h` — the header that used to carry a map of that shape —
and registered this page and its one pin as the W6 item that states the terms.
`jet.h` stays deleted.

This page is the contract. Everything on it is verified against the tree, and
where the ruling's own wording and the tree disagree the tree is what is written
here, with the disagreement named.

## What a consumer may do

**N solver instances, one per thread, each solving its own model, each with
`common.threads = 1`.** Both engines: `hven::solvers::IpmSolver` and
`hven::solvers::SqpSolver`. The results are the results of the same solves run
serially — counters and statuses identically, floating-point measures to within
last-digit drift (see "How identical" below).

```cpp
// One instance, one model, one thread. Nothing is shared.
hven::solvers::IpmOptions opts;
opts.common.threads = 1;
hven::solvers::IpmSolver solver(opts);
const auto program = hven::solvers::make_nlp_program(problem);
const hven::solvers::IpmResult r = solver.solve(*program, x0);
```

`common.threads = 1` is the whole of the thread setting a worker needs. It
applies a **thread-local** MKL override at backend-call scope and undoes it on
every exit, so an instance pinned to one backend thread does not reach out and
re-point any other MKL user in the process — see "`MklThreadScope`" below. For
the interior-point engine, `hven::solvers::ipm_worker_options(base)`
(`include/hven/drivers/ipm_solver_types.h:701-728`) is the named preset that
writes exactly `common.threads = 1` and `common.print_level = 10` and nothing
else; it is the replacement for the retired `NLPSolver::jet_initialize()`.

**The evidence** is `Concurrency.NInstancesOnNThreadsMatchSerial`
(`tests/drivers/test_concurrency.cpp`): four instances on four `std::thread`s —
two `SqpSolver` cells, two `IpmSolver` cells, each over its own model, every
options value at `common.threads = 1` — against the same four solves run
serially first in the same process.

## What a consumer may NOT do

- **Share one solver instance across threads.** A solver owns a live backend
  session and mutable workspace. One instance per thread.
- **Call one instance's `solve()` concurrently with itself**, from any thread,
  including re-entering it from an iteration callback. The per-component notes
  say the same thing one layer down: `include/hven/linear/symmetric_factor.h:238-240`
  ("solves are not internally synchronized … concurrent solves across co-owners
  must be serialized by the caller"), `include/hven/detail/qp/qp_engine.h:122-128`
  (two different engines sharing one `BorderState` is undefined **concurrently**
  and safe **sequentially**), `include/hven/model/nlp_model_assembly.h:76-78`.
- **Share one `Ledger` or one `TraceSink` between concurrently-solving
  instances.** Both are attached by raw pointer to caller-owned objects
  (`IpmSolver::attach_ledger`/`attach_trace`, `SqpSolver`'s same pair) and
  neither is synchronized: `Ledger::record` is a bare `push_back`
  (`include/hven/core/ledger.h:212-218`) and `JsonLinesTraceSink` says so itself
  — "one sink serves one solve at a time on one thread"
  (`include/hven/drivers/trace_writer.h:57-59`). One ledger and one sink per
  instance; merge afterwards.
- **Expect a parallel-solve facility.** There is none, by ruling. No API here
  starts a thread, takes a thread count for solves-in-flight, or collects
  results from a fan-out.

## How identical: the pin's terms, and what it found

The ruling's word is "bit-identical". The pin asserts the form that is
defensible everywhere and **records** the literal form as a finding, because the
two are not the same claim:

- **Counters and statuses: EXACT.** No floating-point arithmetic produces them,
  so a single-bit move in one is a real regression and is refused.
- **Floating-point measures: the 1e-5 relative gate with a 1e-13 absolute
  floor**, the rule `tests/sqp/test_corpus_cells.cpp:908-944` states and `:3558`
  floors. MKL's kernels are address-sensitive — the property CNR mode exists for
  — and a heap touched by four threads hands the same solve different addresses,
  so a measure may differ in its last digits with no counter moving. CLAUDE.md §7
  carries the same caveat for the cross-process case.

**On the reference box the literal bit-identity HELD**, and the pin records it
every time it runs: on the W6 T4 landing read, all 56 counter/status columns and
all 42 measure columns were byte-equal between the concurrent and the serial run
(Linux, clang 22.1.8, MKL-linked Release and Debug alike). Byte equality is the
observed common outcome; the gate is what the pin *asserts*, and it engages only
on digits that provably vary.

## The process-global exceptions

The ruling names three. Each is verified against the tree below, one is narrower
than the ruling's phrasing and says so, and the audit that looked for a fourth
is described after them. **State outside these is per instance or per thread**:
an instance's options, workspace, factor, session, ledger pointer and sink
pointer are its own.

### 1. The interior-point engine's evaluation pool — process-global, confirmed

`hven::utils::thread_pool()` is a function-local `static ThreadPool`
(`src/interior/utils/thread_pool.cpp:29`) and its width is a namespace-scope
`std::atomic<int> g_num_threads` (`:10`) that **defaults to
`std::thread::hardware_concurrency()`, not to 1**. `hven::utils::set_num_threads`
(`:33-68`) resizes the one pool for the whole process, and the model layer says
so at the setter a consumer actually sees:
`NonLinearProgram::set_evaluation_threads` routes straight to it and its comment
names the consequence — "Setting it through one aggregate sets it for every
aggregate in the process" (`include/hven/model/non_linear_program.h:1002-1008`).
`ThreadPool::requested_thread_count()` (`include/hven/detail/interior/utils/thread_pool.h:486-487`)
is a second process-global atomic in the same mechanism: the size stash the
pool's constructor reads.

What this means for the N×1 shape: **the pool is not per instance and cannot be
made per instance by any option.** Two instances that both fan out are
dispatching into one pool. A consumer running N workers should either set the
evaluation width once, before the workers start, or arrange for its programs not
to fan out at all.

**That recipe is UNPINNED (W6 T4 close, 2026-09-14).** N instances each fanning out
into the one process-global pool — the shape tycho's Jet map runs — rests on the
pool's own design statement (per-dispatch latches make concurrent dispatches from
separate threads inherently safe; `thread_pool.h`, the "Per-dispatch latches"
paragraph) and is NOT exercised by the pin on this page, which asserts the pool is
never reached. A pin running two partitioned programs from two threads against
their serial runs is REGISTERED (W7/M7); until it lands, the recipe is design text.

Not to fan out is the easy case and it is the common one:
`make_nlp_program(problem)` defaults to **one partition**
(`include/hven/detail/model/nlp_adapter.h:605-606`, `:629-630`), the count is
capped down further on small problems
(`src/model/non_linear_program.cpp:415-418`, threshold
`kMinKktElementsPerPartition` = 1000 KKT elements per partition), and
`parallel_sequence` runs **inline** whenever `nparts <= 1`
(`include/hven/detail/interior/utils/thread_pool.h:698-707`) — so at one
partition the pool is never dispatched to and never even constructed. The pin's
interior-point cells stay clear of this exception exactly that way, and they
assert it rather than assume it: the adopted partition count is read off the
program (`num_partitions_`) and asserted to be 1, and the process-global width is
read before and after the whole test and asserted unmoved.

One further note on the pool: `parallel_sequence` **throws** if it is entered
from a pool worker (`:704`, "nested dispatch from pool worker") or while
`set_num_threads` is resizing (`:707`). Do not call a solve from inside a pool
worker, and do not resize the pool while any solve is running.

### 2. CNR — narrower in this tree than the ruling's phrasing

MKL's conditional-numerical-reproducibility mode is the reason the measures above
are gated, and the ruling names it as a process-global exception. **Verified at
source, hven's own CNR knob is per instance, not process-global.**
`SymmetricFactor::Options::cnr_threads` is carried into that session's own
`iparm[33]` (`src/linear/pardiso_session.cpp:313-322`) and `iparm_` is a member
of the session; it is off by default (0 means "leave `iparm[33]` alone"), and on
Accelerate it is refused outright as a Pardiso-only option
(`src/linear/symmetric_factor_accelerate.cpp:466-473`).

MKL's *own* process-wide reproducibility control — the `mkl_cbwr_*` /
`MKL_CBWR` family — is **never called anywhere in this repository** (no hit in
`src/` or `include/`). So the exception a consumer has to know about is the one
that belongs to the consumer: setting MKL's CBWR mode, or leaving it unset,
changes the arithmetic every instance in the process runs, hven's included. hven
neither sets it nor depends on it, and the gate above is what stands in for it.

### 3. Accelerate's thread setting — process-global or per-thread, and UNOBSERVED

On Apple only (`USE_ACCELERATE_SPARSE`), the interior-point driver calls
`accelerate_set_num_threads(opts_.common.threads)` at two sites
(`src/drivers/ipm_solver.cpp:950` and `:5281`), and startup calls
`ensure_accelerate_initialized()` once per process
(`src/drivers/solver_init.cpp:25`). Read at source
(`include/hven/detail/interior/utils/accelerate_threads.h:99-127`):

- On macOS 15+ the call is `BLASSetThreading`, which is **per calling thread**
  and a binary toggle, not a count — and it is **never restored**, on that
  thread or any other.
- On older systems, and in `ensure_accelerate_initialized` unconditionally, it is
  `setenv("VECLIB_MAXIMUM_THREADS", …)` — **process-wide**, and read by Accelerate
  exactly once at the first BLAS call, so a later write is a no-op.

Either way the promise `MklThreadScope` keeps on MKL — apply for this call, put
back what was there — is **not kept on Apple**, which is why the SQP engine
deliberately does not mirror the driver-level call
(`include/hven/drivers/common_options.h:47-54`) and why
`SymmetricFactor::set_num_threads` on Accelerate stores a count and applies it to
nothing.

**No value on this page was measured on Apple hardware.** Everything in this
section is read off the source; the behaviour is **UNOBSERVED** until a real Mac
session runs it, and nothing here is estimated or interpolated (CLAUDE.md §6).
The pin does not run this path.

### The audit for a fourth

Every remaining piece of process-global or cross-thread state a solve can touch,
found by sweeping `src/` and `include/hven/` for non-const statics at namespace
and function scope, `std::call_once`, `thread_local`, environment reads and
writes, MKL's thread and CNR setters, and the ledger and trace-sink attachment
points. None of these adds an exception to the three above, and the reasons are
recorded so a later reader does not have to re-derive them:

| Site | What it is | Why it is not a fourth exception |
| --- | --- | --- |
| `include/hven/detail/linear/session_id.h:42-45` | `next_session_id()`, a function-local `static std::atomic<uint64_t>` | Process-global by design and stated so in its own comment: an identity, unique within one process run, never persisted. Which id a session draws depends on process-wide creation order, so it is **not** stable under concurrency and is not a result column. Nothing keyed on it crosses instances. |
| `src/drivers/ipm_solver.cpp:2113-2116` | `IpmSolver::next_owner_id()`, the same shape | The same reading, for the analysis owner id. |
| `src/drivers/solver_init.cpp:18-33` | `ensure_solver_initialized()`, a `std::call_once` over a function-local flag | One-time backend initialization, thread-safe by construction. It has one observable consequence worth naming: it returns a non-zero duration **only to the first caller in the process**, so `IpmResult::solver_init_time` and the `kSolverInitialized` trace message are a once-per-process event, not a per-solve one (the tree already records this at `tests/interior/test_ipm_trace.cpp:1789-1801`). Both are timings, which CLAUDE.md §7 keeps informational and never asserted; the pin therefore compares no timing column. Its Apple branch is part of exception 3. |
| `src/interior/utils/memory_management.cpp:13-16` | `BumpAllocator`'s two `thread_local` arenas | Per thread, not shared. No library call site: nothing in `src/` or `include/hven/` outside its own header uses `BumpAllocator` today. |
| `src/linear/accelerate_session.cpp:43` | `thread_local std::string g_last_error` | Per thread. Apple only. |
| `include/hven/detail/interior/utils/thread_pool.h:36`, `:351` | `g_is_pool_worker`, `tl_enqueue_index` | Per thread; part of the pool mechanism (exception 1). |
| `include/hven/model/structure_identity.h:326` | `StructureEpochCounter::value_`, an `std::atomic` | A **member**, so per instance. Atomic so a reader on another thread is well-defined. |
| `src/interior/utils/color_text.cpp:32` | `enable_color_console()`, a Windows console-mode change | Process-global in principle; **no caller anywhere in the library**. |
| `include/hven/detail/linear/fault_injection.h`, `include/hven/detail/qp/ipqp_fault_injection.h` | `static inline` injector and observer state | Compiles to nothing without `HVEN_TESTING`, which exactly two standalone test executables define and the production library never does (`docs/testing.md`). Not in a shipped build at all. |
| `src/core/ledger.cpp` | — | Contains **no** global sink. An earlier draft of this contract claimed one; there is no such referent. The ledger is an ordinary object whose records are its own members; the hazard is sharing one between threads, which is stated above as a consumer rule. |
| `src/interior/utils/get_core_count.cpp:57` | reads `/proc/cpuinfo` | A read, no state. |

There are **no** `getenv` reads anywhere in `src/` or `include/hven/`, and the
only `setenv` writes are the two Apple ones in exception 3.

## `MklThreadScope` is not a process-global exception

`MklThreadScope` (`include/hven/detail/linear/thread_scope.h:66`) is what
`common.threads` actually does. It is constructed immediately before a backend
call — the one `::pardiso` call in `FactorSession::run_phase`
(`src/linear/pardiso_session.cpp:132-134`), and the dense border factor's LAPACK
calls — and its whole body is two lines:

```cpp
previous_ = mkl_set_num_threads_local(num_threads);   // on the way in
mkl_set_num_threads_local(previous_);                 // on the way out
```

`mkl_set_num_threads_local` sets a **thread-local** override and returns the
override it replaced. So: the count an instance runs at is scoped to the calling
thread and to the duration of one call; the caller's own pre-existing override is
saved and put back exactly, rather than reset to a hardcoded 0; and nothing here
writes a process-wide setting or an environment variable. Two instances on two
threads at `threads = 1` do not see each other's setting, and a consumer's own
MKL work on a third thread is untouched.

Restoration on a **normal** exit is pinned by
`ThreadScope.TheConfiguredCountReachesTheCallAndTheCallersOverrideIsBackAfterIt`
and by `test_symmetric_factor.cpp`'s
`APerInstanceThreadCountRestoresTheCallersOwnThreadLocalOverride`. Restoration on
an **exceptional** exit is pinned, since M6 W6 T4, by
`ThreadScope.TheCallersOverrideIsRestoredWhenAThrowUnwindsTheScope` — a throw
raised inside a scope and caught outside it, with the thread state read before,
inside and after. It used to be argued rather than measured; `docs/testing.md`'s
dated W6 note records why the seam that was registered to measure it through a
session was declined, and what measures it instead.

On Apple this class is a no-op and stores nothing anywhere —
`thread_scope.h:60-65`, **UNOBSERVED**.

## What is NOT promised

- **Anything on Apple/Accelerate.** Exception 3 is read from the source, not
  measured; the pin does not run there; `MklThreadScope` is inert there. No Apple
  number appears on this page and none is estimated.
- **Anything on Windows.** Same standing: UNOBSERVED.
- **Wall-clock under contention.** N solves on N threads contend for memory
  bandwidth and for MKL's own internals; CLAUDE.md §7 makes a co-run's wall-clock
  informational and never quotable against a serial timing. This contract is
  about *results*, not about speed, and it promises nothing about how long N
  concurrent solves take.
- **`common.threads > 1` in this shape.** A count above 1 is the backend's own
  parallelism *inside* one solve. That is a supported setting and a different
  subject; it is outside the N×1 contract this page states, and the pin does not
  cover it. Running N instances at `threads = k > 1` oversubscribes the machine
  and the results above are not claimed for it.
- **N instances fanning out into the shared evaluation pool.** Exception 1's recipe
  for that shape is design text, not measurement — the pin never reaches the pool.
  Registered above; not promised here.
- **Bit-identity as a portable guarantee.** What is asserted is exact counters
  and statuses plus gated measures. Literal byte equality held on the reference
  box and is recorded on every run; it is a finding, not a promise.
