# Getting started with hven

This page takes you from nothing installed to a solved problem and a trace you
can read. It uses the programs in `examples/`, which are built and run by the
test suite, so what is quoted here is code that compiles against the current
headers. When a program is quoted in part, the file name is given.

hven solves problems of the form

```
minimize    f(x)
subject to  g_lower <= g(x) <= g_upper
            x_lower <= x <= x_upper
```

with two engines behind one problem type and one result shape: an interior-point
engine (`IpmSolver`) and an SQP engine (`SqpSolver`). Which to use is
[guides/choosing-a-solver.md](guides/choosing-a-solver.md); this page works with
both.

## 1. Install

### Requirements

- A C++20 compiler. The project is developed with clang.
- CMake 3.24 or newer.
- A sparse linear algebra backend: **Intel MKL** on Linux and Windows, **Apple
  Accelerate** on macOS. hven's CMake finds MKL through `MKLROOT` or
  `ONEAPI_ROOT`; source your oneAPI environment first (`source
  /opt/intel/oneapi/setvars.sh`) or export one of them.
- Eigen and fmt are vendored as git submodules under `dep/`; nothing else is
  downloaded to build the library.

### Build and install

```sh
git clone --recurse-submodules <hven repository>
cd hven
cmake --preset linux-clang-release          # or: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix <prefix>
```

The configure step prints which backend it found. The install puts
`libhven.a`, the headers (hven's own, plus the vendored Eigen and fmt it
needs) and the CMake package files under `<prefix>/lib64/cmake/hven` (or
`lib/cmake/hven`, depending on your platform's `GNUInstallDirs`).

A build in tree also builds `examples/` and the test suite; pass
`-DHVEN_BUILD_TESTS=OFF` to skip the latter (the tests fetch Google Test over
the network at configure time; when hven is added with `add_subdirectory` the
test suite defaults to OFF). Developing hven itself: prefer a
Debug build, which keeps Eigen's runtime asserts on, and verify in Release.

### Use it from your CMake project

```cmake
cmake_minimum_required(VERSION 3.24)
project(app LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(hven REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE hven::hven)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=<prefix>
```

`find_package(hven)` re-finds MKL (or Accelerate) for you, and `hven::hven`
carries everything a consumer must share with the library, including the
compile definitions below. Your source includes one header:

```cpp
#include <hven/hven.h>
```

Or add hven as a subdirectory (`add_subdirectory(hven)`) and link `hven::hven`
the same way.

### The alignment trap for hand-compiled consumers

hven's layout depends on `EIGEN_MAX_ALIGN_BYTES`, the alignment Eigen assumes
for its heap allocations and its fixed-size vectorized types. A mismatch
between the library and a consumer's translation unit is a heap corruption
across the archive boundary, not a diagnostic.

If you link `hven::hven`, the target exports the value the library was built
with and you do nothing. The trap is for consumers that **compile against the
installed headers without going through the target** (a hand-written compile
line, an IDE project, another build system). Such a compile line must pass the
same define the installed target carries:

```sh
c++ -std=c++20 -DEIGEN_MAX_ALIGN_BYTES=32 -I<prefix>/include main.cpp ...
```

The value is the one printed at configure time (`hven: EIGEN_MAX_ALIGN_BYTES
resolved to 32 ...`). You can also read it from the installed headers:
`<hven/core/build_config.h>` (included by `<hven/hven.h>`) defines
`hven::kBuiltEigenMaxAlignBytes`, the value the library was built with, and
`hven::eigen_alignment_matches()`, which compares it with your translation
unit's own `EIGEN_MAX_ALIGN_BYTES`. Put `HVEN_STATIC_ASSERT_EIGEN_ALIGNMENT();`
at namespace scope in a translation unit of yours and a mismatch becomes a
compile error instead of a heap corruption. The same value is recorded in
`hvenTargets.cmake` under `INTERFACE_COMPILE_DEFINITIONS`. The
other definitions in that list (`FMT_HEADER_ONLY`, `EIGEN_DONT_PARALLELIZE`, ...)
belong in a hand-written compile line for the same reason.

### Threads

hven's MKL layer is the Intel-threaded one by default. `common.threads` sets the
backend thread count for one solver's factorizations; `0` leaves the process's
`MKL_NUM_THREADS` in charge. Concurrency rules are in
[concurrency.md](concurrency.md).

### Check the install

`examples/` configures as its own project against an install tree, the way a
stranger would:

```sh
cmake -S examples -B ex-build -DCMAKE_PREFIX_PATH=<prefix>
cmake --build ex-build
ctest --test-dir ex-build
```

Each example prints `OK` and exits 0, or prints `FAILED` and exits 1.

## 2. Your first solve

The first example is Hock-Schittkowski problem 71:

```
min   x0*x3*(x0 + x1 + x2) + x2
s.t.  x0*x1*x2*x3            >= 25
      x0^2 + x1^2 + x2^2 + x3^2 = 40
      1 <= x <= 5
```

You state a problem once, by deriving from `hven::Nlp`, wrapping it in a
`hven::Problem`, and handing that to either engine. The shape is Ipopt's
`TNLP`: callbacks for values plus the sparsity of the Jacobian and the Hessian
as coordinate triplets.

```cpp
struct Hs071 : hven::Nlp {
    hven::NlpDimensions dimensions() const override {
        return {.n = 4, .m = 2,
                .jacobian_nonzeros = hven::dense_jacobian_nonzeros(2, 4),
                .hessian_nonzeros  = hven::dense_lower_hessian_nonzeros(4)};
    }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        xl.setConstant(1.0);
        xu.setConstant(5.0);
        gl << 25.0, 40.0;                 // row 0: product >= 25 ; row 1: sum of squares == 40
        gu << hven::kInfinity, 40.0;
    }
    double eval_f(hven::ConstVecRef x) const override { /* ... */ }
    // eval_grad_f, eval_g, jacobian_structure, eval_jacobian,
    // hessian_structure, eval_hessian: see examples/01_hs071_both_engines.cpp
};
```

Things to know before reading the full file:

- **Rows.** All constraint rows live in one `g`, with `bounds()` giving each
  row's lower and upper bound. An equality row has equal bounds. A one-sided
  row has an infinite side. A bound with magnitude at or beyond
  `common.infinite_bound` (default 1e20) is absent; write `hven::kInfinity`.
- **Hessian.** Provide the **lower triangle** of the Lagrangian Hessian
  `obj_factor * grad2 f + sum lambda_i * grad2 g_i`, as triplets. The
  `dense_*` helpers produce a fully dense pattern for small problems.
  `has_hessian()` returning `false` is legal only for the quasi-Newton SQP
  (section 7 and `examples/07_sqp_quasi_newton.cpp`).
- **Everything is re-read every solve.** `dimensions()` and `bounds()` are
  queried at each `solve`, so you may change a parameter of your `Nlp` between
  solves (section 6). The dimensions, the sparsity structures and each row's
  kind (equality, one-sided, range, free) must not change.
- **`Vec{3}` is uninitialized.** For a vector holding the two values a and b,
  write `Vec{{a, b}}`.

Now solve it, on either engine:

```cpp
hven::Problem problem(std::make_shared<Hs071>());
const hven::Vec x0{{1.0, 5.0, 5.0, 1.0}};

hven::IpmSolver ipm;
hven::IpmResult r = ipm.solve(problem, x0);

hven::SqpSolver sqp;
hven::SqpResult s = sqp.solve(problem, x0);
```

`examples/01_hs071_both_engines.cpp` is this program in full; it runs both and
checks the known optimum f* = 17.0140173:

```
IpmSolver    status=optimal   iterations= 10  f=17.0140173  x=(1.0000, 4.7430, 3.8211, 1.3794)  lambda=(-0.5523, 0.1615)  OK
SqpSolver    status=optimal   iterations=  5  f=17.0140173  x=(1.0000, 4.7430, 3.8211, 1.3794)  lambda=(-0.5523, 0.1615)  OK
```

What came back, for either engine (`IpmResult` and `SqpResult` both derive from
`SolveResult`):

| field | meaning |
| --- | --- |
| `status` | `SolveStatus`: `kOptimal`, `kAcceptable`, `kIterationLimit`, `kInfeasible`, `kStalled`, `kDiverging`, `kNumericalError`, `kInterrupted`, `kWorkLimit`, `kTimeLimit`. Always test it; a solve that did not finish still returns a point |
| `message` | one line saying why the solve stopped |
| `x`, `f` | the point and its objective |
| `lambda`, `g` | one entry per row of your `Nlp`, in your order. Sign convention: Ipopt's, `L = f + lambda' g` |
| `z` | bound multipliers, one per variable |
| `stationarity`, `feasibility`, `complementarity` | the three measures at the returned point, in your problem's units |
| `iterations`, `work` | the iteration count; evaluation and factorization counts (`work.factorizations`) |
| `stop_reason` | why a stop that is not a convergence happened (`kNone` when the status already names the cause) |
| `warm_start` | the value to hand to the next solve (section 6) |

Both engines are **silent by default**. The next section turns the console on.

## 3. Turning the console on

One option line does it. Verbosity is a rung ladder, and each rung prints what
the one below prints:

| `common.verbosity` | prints |
| --- | --- |
| `kSilent` (default) | nothing |
| `kWarnings` | warnings, and the footer's `exit` line |
| `kSummary` | adds the four-line header, the interior-point phase lines (multi-phase solves only) and the full footer |
| `kIterations` | adds the column header, one row per iteration (restoration rows included) and notices |
| `kDetail` | adds one condensed line per QP-tier event on the SQP, and the interior-point KKT-analysis and phase-exit lines |

```cpp
hven::IpmOptions options;
options.common.verbosity = hven::Verbosity::kIterations;
hven::IpmSolver ipm(options);                 // validate() runs here: a bad value throws now
hven::IpmResult r = ipm.solve(problem, x0);
```

The same field exists on `SqpOptions`. Note the defaults differ: `common.verbosity` is `kSilent`, but a `ConsoleTraceSink` you build by hand defaults its `Format::verbosity` to `kIterations`. The options structs are plain
aggregates; every field, with its default and range, is in
[options-reference.md](options-reference.md), and
`hven::dump_options(options, std::cout)` prints the values in force.

## 4. Reading the human trace

This is the interior-point engine on HS071 at `kIterations`. It is real
output (clang, Release, `MKL_NUM_THREADS=1`), reproduced by
the program above plus the verbosity line:

```
hven 0.2.0 -- interior-point engine
problem    variables 4 (boxed 4) · constraints 2 (equality 1, lower 1) · absent bounds 0
engine     KKT dim 7, nnz 22 · inertia classic · restoration off · phases 1
tolerances optimality 1e-06 · feasibility 1e-06 · max iterations 1000
      - solver initialized (1.8 ms)
 iter t      objective stat_scl feas_scl comp_scl     step  lgmu alpha_pr alpha_du ls lg(rg) codes
    0 B  1.6007993e+01 1.48e+01 1.19e+01 1.00e-03 1.04e-01 -12.0 1.65e-01 7.76e-03  0      -
    1 B  1.6166757e+01 1.24e+01 9.96e+00 1.00e-03 7.01e-01  -3.2 1.00e+00 1.45e-01  0      -
    2 B  1.6777318e+01 8.89e+00 6.56e-01 1.94e-03 2.10e-01  -4.0 1.00e+00 9.51e-01  0      -
    3 B  1.7025134e+01 9.00e-01 1.28e-01 4.72e-04 7.85e-02  -4.1 2.12e-01 5.93e-01  0      -
    4 B  1.6916125e+01 5.06e-01 1.89e-01 2.91e-04 3.29e-03  -4.1 2.72e-02 1.00e+00  0      -
    5 B  1.6917772e+01 3.13e-01 1.84e-01 1.62e-04 1.21e-01  -5.4 1.00e+00 9.27e-01  0      -
    6 B  1.6999190e+01 1.53e-02 2.30e-02 3.89e-05 3.36e-03  -6.8 1.00e+00 9.92e-01  0      -
    7 B  1.7013992e+01 2.08e-04 4.11e-05 3.89e-07 8.11e-06 -11.8 9.90e-01 9.90e-01  0      -
    8 B  1.7014017e+01 2.09e-06 4.13e-07 3.89e-09 8.15e-08 -12.0 9.91e-01 9.90e-01  0      -
    9 B  1.7014017e+01 2.03e-08 3.84e-09 3.89e-11        - -12.0        -        -  -      -
exit        Optimal (none) -- converged: optimality and feasibility tolerances met
iterations  10 · restoration entries 0 · factorizations 10
objective   1.70140173e+01
diagnostics stationarity 2.0e-08 · feasibility 1.0e-08 · complementarity 1.1e-08   (declared)
counters    soc 0 · watchdog 0 · inertia corrections 0
time        0.002 s (informational)
```

The four header lines say what the solver was asked to do before it did it:

| line | read it for |
| --- | --- |
| `hven 0.2.0 -- interior-point engine` | the version and the engine |
| `problem` | the census of the problem it saw: variables and how they are bounded (`boxed 4` = four variables with both bounds), constraint rows by kind (`equality 1, lower 1` = one equality row and one lower-bounded row), and `absent bounds 0` = how many declared bounds it read as infinite. A row count that disagrees with what you meant is the first thing to check |
| `engine` | the engine's own settings: the KKT matrix size (`KKT dim 7, nnz 22`), the inertia and restoration modes, the phase count |
| `tolerances` | the tolerances the convergence test uses and the iteration cap |

The `- solver initialized` line is a notice (`-` marks a notice, `!` a
warning). Its time is informational.

### The rows

Each row is one iterate. The columns that matter to every solve are the same on
both engines:

| column | meaning |
| --- | --- |
| `iter` | iteration index (the interior-point engine restarts it at 0 in each phase) |
| `t` | which tier produced the step: `B` barrier, `R` restoration (SQP: `W` walk, `I` IPQP, `S` SSN, `E` elastic, `R` restoration) |
| `objective` | the objective at this point, in your units |
| `stat_scl` | stationarity: the size of the Lagrangian gradient, **in the scaled problem the solver works in**. Compared with `optimality_tol` |
| `feas_scl` | feasibility: the largest constraint or bound violation, scaled. Compared with `feasibility_tol` |
| `comp_scl` | complementarity: the largest `s_i * lambda_i` product, scaled. Compared with the complementarity tolerance (default `optimality_tol`) |
| `step` | the size of the step **leaving** this point. `-` where no step leaves (the final row) |

`stat_scl`, `feas_scl` and `comp_scl` are exactly the numbers the convergence
test compares with the tolerances, so you can see why a solve is still going: a
row whose three numbers are not all under their tolerances is not finished. The
`_scl` suffix says they are in the solver's own scaled units; on an unscaled
solve (the HS071 case) those are your units. When scaling is on, the same
measures in your units are the `*_caller` keys of the machine trace and the
`diagnostics` line of the footer.

Reading the HS071 rows: the objective is at 17.014 by iteration 7; the real
work after that is the three measures falling together, from order 10 at the
start to under 1e-7 at the end. `lgmu` (interior-point only) is log10 of the
barrier parameter the step was formed at. `alpha_pr` and `alpha_du` are the
primal and dual step lengths; a row with `alpha_pr` near 1 took essentially the
full Newton step. `ls` counts line-search backtracks, and
`lg(rg)` is log10 of the regularization added to the KKT matrix (`-` when none
was needed). The one-letter `codes` column flags what happened on that row
(second-order correction `s`, watchdog armed `w`, rollback `r`, inertia fix `i`,
evaluation error `x`, reject `m`); a clean solve prints none.

Every code, every SQP-only column and the verdict letters are in
[trace-schema-v1.md](trace-schema-v1.md) section 8.1.

### The SQP engine on the same problem

```
hven 0.2.0 -- SQP engine
problem    variables 4 (boxed 4) · constraints 2 (equality 1, lower 1) · absent bounds 0
engine     QP mode walk (schur_border) · Hessian exact · row scaling off · column scaling auto
tolerances optimality 1e-06 · feasibility 1e-06 · max iterations 100
 iter t      objective stat_scl feas_scl comp_scl     step   radius    h(l1) qp_it qp_fa v codes
    0 W  1.6000000e+01 2.00e+00 1.20e+01 0.00e+00 1.00e+00 1.00e+00 1.20e+01     6   15 H b
    1 W  1.6500000e+01 2.80e+00 1.25e+00 5.73e-01 2.94e-01 1.00e+00 2.38e+00     5    1 H
    2 W  1.6179839e+01 4.10e-01 1.46e+00 7.67e-01 9.98e-02 1.00e+00 1.68e+00     2    1 H
    3 W  1.7003778e+01 1.79e-02 2.18e-02 6.75e-03 7.55e-03 1.00e+00 3.41e-02     2    1 H
    4 W  1.7013955e+01 5.85e-05 9.21e-05 5.09e-05 4.39e-05 1.00e+00 1.65e-04     2    1 H
    5 W  1.7014017e+01 9.71e-10 3.83e-09 1.41e-09        - 1.00e+00 6.39e-09     -    - -
exit        Optimal (none) -- converged: optimality and feasibility tolerances met
iterations  5 · restoration entries 0 · factorizations 19
objective   1.70140173e+01
diagnostics stationarity 9.7e-10 · feasibility 3.8e-09 · complementarity 1.4e-09   (declared)
counters    soc 0 · elastic 0 · qp minors 17 · start level cold · scaling off
time        0.003 s (informational)
```

The SQP's rows have different step columns: `radius` is the trust region,
`h(l1)` the L1 constraint violation the globalization works on (not the same
quantity as `feas_scl`), `qp_it` and `qp_fa` the QP minor iterations and
factorizations spent on that row's subproblem, and `v` the verdict on the step:
`F` accepted as an objective step, `H` accepted as a feasibility step, `J`
rejected, `Q` restoration. The final row prints `-` in those columns: no step
leaves it. The `b` in `codes` on row 0 is a trust-region bound that limited the
first step. Row 0's `comp_scl` is exactly zero because the multipliers start
at zero.

## 5. The solve summary

Both traces end with the footer. It is the part you read after every solve:

```
exit        Optimal (none) -- converged: optimality and feasibility tolerances met
iterations  10 · restoration entries 0 · factorizations 10
objective   1.70140173e+01
diagnostics stationarity 2.0e-08 · feasibility 1.0e-08 · complementarity 1.1e-08   (declared)
counters    soc 0 · watchdog 0 · inertia corrections 0
time        0.002 s (informational)
```

| line | meaning |
| --- | --- |
| `exit` | `<Status> (<stop reason>) -- <message>`. The status is `SolveStatus`; the parenthesis is `StopReason`, `none` whenever the status already names the cause (an optimal solve, an iteration limit); the text is `result.message` |
| `iterations` | iterations taken, restoration entries (a retreat to recover feasibility), and KKT factorizations, the unit of cost this project counts |
| `objective` | the final objective |
| `diagnostics` | the three measures at the returned point **in your units** (`declared`), as `result.stationarity`, `result.feasibility`, `result.complementarity`. On a scaled solve these differ from the last row's `_scl` columns |
| `counters` | engine counters. IPM: second-order corrections, watchdog activations, inertia corrections. SQP: second-order corrections, elastic-mode entries, QP minor iterations, the warm-start level actually used, and which scaling was on |
| `time` | wall-clock seconds. Informational only; do not compare it between runs |

Wall-clock is deliberately the least important line. Counts of factorizations
and evaluations are what this project asserts and compares.

When a solve does not finish, the `exit` line is where to look, and
`kWarnings` is enough to get it. An iteration-limited run:

```
exit        Max iter (none) -- iteration limit reached (max_iterations = 3)
status max_iter | iteration limit reached (max_iterations = 3)
```

(`SolveStatus::kIterationLimit` prints as `max_iter` / `Max iter`: a frozen spelling.) The first line is what `kWarnings` prints (both engines print the same
line here). The second is the program's own `to_string(result.status)` and
`result.message`. The status is `SolveStatus::kIterationLimit`: the enumerator
was renamed in the 0.2.0 break, and the strings (`Max iter` on the console,
`max_iter` from `to_string`) were not. The table of statuses, and what to try next, is in
[guides/choosing-a-solver.md](guides/choosing-a-solver.md) section 5.

### Keeping a machine-readable trace

The console is one sink. The other is JSON lines, one record per event, for
tools:

```cpp
std::ostringstream out;                    // or any std::ostream you keep alive
hven::JsonLinesTraceSink sink(out);
ipm.attach_trace(&sink);                   // borrowed: you own the sink and its stream
ipm.solve(problem, x0);
sink.flush();
```

`examples/08_results_trace_options.cpp` does exactly this and tallies the
records by event. The schema is [trace-schema-v1.md](trace-schema-v1.md); a
sink is attached for one solve at a time on one thread. To write your own
(draw a plot, feed a GUI), derive from `hven::TraceSink`: every event handler
has a no-op default, so you override only the events you read.

## 6. Warm starts

Every solve returns `result.warm_start`, a value that holds the primal and dual
point and the structural identity of the problem it was taken under. Hand it to
the next solve as the third argument:

```cpp
hven::WarmStart warm;                                  // empty: means "no warm start"
for (double p : {0.2, 0.4, 0.6, 0.8, 1.0}) {
    disc->p = p;                                       // change the Nlp in place
    const hven::SolveResult r = ipm.solve(problem, x0, warm);
    warm = r.warm_start;                               // the next solve starts from here
}
```

`examples/05_warm_continuation.cpp` is this loop, run cold and warm on both
engines along a parameter that crosses a constraint switching on:

```
IPM    cold : 3 3 9 10 11   total=36  OK
IPM    warm : 3 2 8 5 4   total=22  OK
SQP    cold : 1 1 4 5 6   total=17  OK
SQP    warm : 1 1 3 3 3   total=11  OK
```

(Each number is one solve's iteration count along the sweep. The warm sweeps
cost fewer iterations in total on both engines, and the example asserts that.)

Rules worth knowing:

- The `Nlp` may change between solves in value: parameters, bounds. It may
  **not** change in structure: dimensions, sparsity patterns, row kinds. A
  warm start taken before such a change is refused as an identity mismatch.
- `common.start_level` caps how much of an offered warm start a solve trusts
  (`kCold`, `kSeeded`, `kWarm`, `kHot`; default `kWarm`). On the SQP, a
  `WarmStart` (a result's `warm_start`, or `WarmStart::seed`) reaches at most
  `kSeeded`; `kWarm` and `kHot` are reachable only through the SQP-only native
  route (`SqpWarmStart`, below). The interior-point engine takes the whole
  value at `kWarm` and has no separate `kHot`. It is a ceiling,
  never a floor.
- A warm start from the interior-point engine is accepted by the SQP. The common use is a
  loose interior-point solve handed to a tight SQP solve, which
  `examples/06_ipm_to_sqp_polish.cpp` runs three ways:

```
IPM rough          status=optimal  iterations= 8  f=17.0139920332 (tol 1e-3)
SQP polish staged  status=optimal  iterations= 2  f=17.0140172892  stationarity=4.4e-16  start level=Seeded
SQP polish payload status=optimal  iterations= 2  f=17.0140172892  stationarity=4.4e-16  start level=Seeded
SQP cold           status=optimal  iterations= 6  f=17.0140172892  stationarity=1.8e-15  start level=Cold
```

  The staged route (`to_sqp_warm_start(warm, problem)`) lets you inspect the
  crossover's active set before solving from it; the direct route passes the
  `WarmStart` itself. Both reached the same answer in two iterations here, against
  six cold. That is one problem; measure on yours.
- To start from your own point and multipliers, build a warm start with
  `hven::WarmStart::seed(problem, x, lambda, z)`, with `lambda` in your row
  space. An empty `x` seeds multipliers only.
- The SQP's own state can be reused with `SqpResult::sqp_warm_start`
  (`SqpWarmStart`, opaque), through the labelled SQP-only overload of `solve`.
  Use it for continuation on a fixed structure; the `WarmStart` is the portable
  currency.

## 7. Callbacks, early stop and stopping from another thread

Both engines accept one iteration callback. It sees every iterate and returns
whether to go on. This is the hook for a progress display, a plot, a user
stopping criterion or a GUI.

```cpp
hven::SqpSolver sqp;
sqp.set_iteration_callback(
    [](const hven::IterationEvent &e) {
        std::printf("  iter %lld  f=%.6f\n", static_cast<long long>(e.iteration), e.f);
        return hven::CallbackAction::kContinue;
    },
    hven::IterationPayload::kVectors | hven::IterationPayload::kDiagnostics);
hven::SqpResult r = sqp.solve(problem, x0);
```

- **The payload mask** says what the callback reads, so the engine prepares
  nothing else. Scalars (`iteration`, `f`) are always delivered, so `kScalars` is the value 0: "scalars only", not a bit you can test. `kVectors` adds
  `x`, `lambda`, `z`; `kDiagnostics` adds the three measures in your units;
  `kConstraints` adds `g` and `row_violation`; `kBoundMultipliers` adds
  `z_lower` and `z_upper`; `kAll` is all of them. The default is
  `kVectors | kDiagnostics`; a field you did not ask for arrives empty or NaN.
- **The event's vectors are borrowed** for the duration of the call.
  `hven::snapshot(event)` returns an owning `IterationSnapshot` to keep or to
  hand to another thread.
- **`CallbackScope`** (the third argument): `kTopLevel`, the default, delivers
  your own iterations; `kIncludeRestoration` also delivers the SQP's restoration
  sub-solve rows.
- **Stopping.** Return `CallbackAction::kStop` and the solve ends
  `kInterrupted` with `stop_reason == kCallbackStop`, and the result holds the
  event's own point. `kStopReturnBest` asks for the best iterate seen so far
  instead (`result.returned_best_iterate` says so).
- **Stopping from another thread**, such as a GUI's stop button: make a
  `hven::StopSource`, pass its token in the budget, and call
  `request_stop()` from any thread. The solve ends at its next iteration
  boundary, `kInterrupted` with `kStopRequested`.

  ```cpp
  hven::StopSource stop;
  std::thread button([&] { /* ... */ stop.request_stop(); });
  hven::SqpResult r = sqp.solve(problem, x0, hven::SolveBudget{.stop = stop.token()});
  ```

  A `StopSource` is per solve: there is no reset.
- **Exceptions.** A callback may throw; the exception propagates out of
  `solve()`. After a throw (or any exception that is not an evaluation failure),
  `solver.last_result()` holds the partial result of the solve it interrupted.

`examples/09_callbacks_and_stop.cpp` runs all of this on the same problem and
asserts each outcome. Its output:

```
live feed (SqpSolver)
  iter  0  f=+24.200000  x=(-1.2000, +1.0000)  lambda=(+0.0000)  violation=1.4e+00
  iter  1  f=+6.180518  x=(-0.8969, +0.6437)  lambda=(+6.8823)  violation=2.2e-01
  iter  2  f=+3.248943  x=(-0.7950, +0.6157)  lambda=(+1.2312)  violation=1.1e-02
  iter  3  f=+2.674601  x=(-0.5821, +0.2975)  lambda=(+0.0000)  violation=0.0e+00
  iter  4  f=+2.077266  x=(-0.4117, +0.1404)  lambda=(+0.0000)  violation=0.0e+00
  iter  5  f=+1.635066  x=(-0.2045, -0.0011)  lambda=(+0.0000)  violation=0.0e+00
  iter  6  f=+1.188770  x=(-0.0788, -0.0096)  lambda=(+0.0000)  violation=0.0e+00
  iter  7  f=+1.125048  x=(+0.1807, -0.0347)  lambda=(+0.0000)  violation=0.0e+00
  iter  8  f=+0.582687  x=(+0.2373, +0.0531)  lambda=(+0.0000)  violation=0.0e+00
  iter  9  f=+0.582687  x=(+0.2373, +0.0531)  lambda=(+0.0000)  violation=0.0e+00
  iter 10  f=+0.582687  x=(+0.2373, +0.0531)  lambda=(+0.0000)  violation=0.0e+00
  iter 11  f=+0.582687  x=(+0.2373, +0.0531)  lambda=(+0.0000)  violation=0.0e+00
  iter 12  f=+0.431033  x=(+0.3623, +0.1157)  lambda=(+0.0000)  violation=0.0e+00
  iter 13  f=+0.290473  x=(+0.5169, +0.2433)  lambda=(+0.0000)  violation=0.0e+00
  iter 14  f=+0.164477  x=(+0.6005, +0.3536)  lambda=(+0.0000)  violation=0.0e+00
  iter 15  f=+0.131330  x=(+0.7671, +0.5607)  lambda=(+0.0000)  violation=0.0e+00
  iter 16  f=+0.045208  x=(+0.7875, +0.6194)  lambda=(+0.0611)  violation=3.9e-03
  iter 17  f=+0.045674  x=(+0.7864, +0.6177)  lambda=(+0.1214)  violation=4.2e-06
  iter 18  f=+0.045675  x=(+0.7864, +0.6177)  lambda=(+0.1215)  violation=4.3e-12
  -> optimal, f=0.0456748
early stop (IpmSolver): quit once f <= 0.1 and the violation is below 1e-2
  -> interrupted (callback_stop) at iteration 16 of 18, f=0.045395, x=(+0.7884, +0.6191)
early stop (SqpSolver): kStopReturnBest after 3 iterations
  -> interrupted, returned_best_iterate=yes, f=2.674601
external stop (SqpSolver): a worker thread plays the GUI's stop button
  -> interrupted (stop_requested) after 3 iterations, x=(-0.7950, +0.6157)
```

## 8. When the model cannot evaluate

If your `eval_f`, `eval_g` or a derivative cannot produce a value at a trial
point, throw `hven::EvaluationError` (or return a non-finite value): both engines
treat the trial as rejected and back off. A throw at the **start** point
propagates. Any other exception type propagates unchanged, from either engine, so
a model that throws `std::runtime_error` for "cannot evaluate here" aborts the
solve; throw `EvaluationError` instead.

Argument validation (a wrong-length start point, a refused option) throws
`std::invalid_argument` with a message naming the problem, for example:

```
hven interior-point solver: initial guess has 3 elements, expected 4 primal variables
```

## 9. Limits and budgets

| to limit | set | status when hit |
| --- | --- | --- |
| iterations | `common.max_iterations` (IPM: per phase; SQP: subproblems solved) | `kIterationLimit` |
| factorizations (deterministic) | `common.max_factorizations` (unset = none) | `kWorkLimit` |
| wall-clock | `common.time_limit_seconds` (unset = none; not reproducible) | `kTimeLimit` |

A per-call `SolveBudget{.max_iterations = ...}` can tighten the option's cap for
one solve, never raise it. Prefer the factorization limit when you need a bound
that is the same on every run.

## 10. If your model has no second derivatives

A problem that cannot supply `eval_hessian` declares `has_hessian() == false`
and no Hessian structure, and is solved by the SQP in its quasi-Newton mode:

```cpp
hven::SqpOptions options;
options.hessian_mode = hven::SqpHessianMode::kLbfgsDamped;
hven::SqpSolver sqp(options);
```

The interior-point engine refuses such a problem by name.
`examples/07_sqp_quasi_newton.cpp` and `examples/10_first_derivatives_only.cpp`
(the latter configured as [guides/snopt-like-configuration.md](guides/snopt-like-configuration.md)
describes) are the references, and
[guides/choosing-a-solver.md](guides/choosing-a-solver.md) section 3 says what to
expect from the mode.

## 11. Where next

| you want | read |
| --- | --- |
| to choose between the engines | [guides/choosing-a-solver.md](guides/choosing-a-solver.md) |
| every option | [options-reference.md](options-reference.md) |
| every trace record and column | [trace-schema-v1.md](trace-schema-v1.md) |
| to build a problem out of small pieces | `examples/04_partitioned_chain.cpp`, [assembly-protocol.md](assembly-protocol.md) |
| to run solvers on several threads | [concurrency.md](concurrency.md) |
| to move code written against an older hven | [migration-0.2.md](migration-0.2.md) and `CHANGELOG.md` |
