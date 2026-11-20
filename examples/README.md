# hven examples

One self-contained program per problem. Read them in order; each adds one idea to the last.
They are the reference for the public surface (`#include <hven/hven.h>`), and each finishes
with `OK` and exit status 0, or `FAILED` and 1.

| # | File | What it shows |
| --- | --- | --- |
| 1 | `01_hs071_both_engines.cpp` | One `Nlp`, one `Problem`, both engines; the shared `SolveResult` readout. |
| 2 | `02_bounded_rosenbrock.cpp` | No constraint rows (`m == 0`); a solution on a bound and its multiplier `z`. |
| 3 | `03_equality_constrained.cpp` | An equality row as equal bounds; the sign convention of `lambda`. |
| 4 | `04_partitioned_chain.cpp` | The pieces route: small block functions over index maps, partitioned (interior-point only). |
| 5 | `05_warm_continuation.cpp` | `WarmStart` along a parameter, the `Nlp` mutated in place between solves. |
| 6 | `06_ipm_to_sqp_polish.cpp` | The explicit interior-point to SQP composition and `to_sqp_warm_start`. |
| 7 | `07_sqp_quasi_newton.cpp` | `SqpHessianMode::kLbfgsDamped` against the exact Hessian; `has_hessian() == false`. |
| 8 | `08_results_trace_options.cpp` | Result fields, engine-specific extras, a trace sink, `dump_options`. |
| 9 | `09_callbacks_and_stop.cpp` | The iteration callback: a live feed, an early stop that returns the iterate, a thread-safe external stop (`StopSource`). |
| 10 | `10_first_derivatives_only.cpp` | HS071 with first derivatives only, configured as `docs/guides/snopt-like-configuration.md` describes. |

## Building and running

In tree they are part of the normal build (`HVEN_BUILD_EXAMPLES` is ON when hven is the
top-level project) and run with `ctest` as `hven_example_<name>`:

```
cmake --preset linux-clang-release
cmake --build build
ctest --test-dir build -R hven_example
```

Against an installed hven, configure this directory as its own project:

```
cmake -S examples -B ex-build -DCMAKE_PREFIX_PATH=<prefix>
```
