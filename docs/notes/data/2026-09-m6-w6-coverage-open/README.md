# M6 W6 T0 — the coverage read at HEAD (7da77b5d)

**Read `PROVENANCE.txt` first.** Everything here comes from one instrumented run
of the whole suite on the `linux-clang-coverage` tree. Coverage flags change
codegen: **nothing in this directory is a pin, a baseline or a timing**, and
nothing here is asserted except as a report of what that run measured.
Apple/Accelerate: **UNOBSERVED** — this read is MKL-on-Linux only.

| file | what it is |
|---|---|
| `PROVENANCE.txt` | commit, toolchain, hardware, how the run was made, the two departures from `scripts/run_coverage.sh`, the suite's status, the exclusion list in force (none), the mismatched-data re-count |
| `coverage-summary.txt` | `llvm-cov report`'s per-file table, verbatim |
| `coverage-areas.txt` | the per-directory rows — `src/drivers/`, `include/hven/drivers/` beside it, `src/linear/`, `src/qp/`, `src/warmstart/`, `include/hven/detail/**` by subdirectory, every remaining group, the library total — all four metrics, **lines** the asserted one |
| `zero-headers.txt` | every library file at 0.00 % lines, each with a one-line judgement and the evidence behind it |
| `parity.txt` | the parity number, the caveat that belongs beside it, the driver files below the total, the twenty biggest uncovered regions |

## 1. The headline

```
src/drivers/    lines  88.63 %  (8586 / 9687)      14 files
library total   lines  87.47 %  (22259 / 25448)   127 files
                       +1.17 points, in src/drivers/'s favour
```

**Parity holds.** Under the settler's ruling (plan §0 J.1 — the directory row
against the library row of the *same* fresh read), the mandate's "drivers
coverage 67.8 % → parity with the library" is **met at HEAD**, and the window
owes an artifact rather than work on it. The reason is W5 T8: `hven_drivers_tests`
(6648 lines, 7 TUs) did not exist when the W2 read was taken, and `src/drivers/`
itself went from ten TUs to fourteen. Details, and the files still below the
total, in `parity.txt`.

## 2. The suite under instrumentation, and the exclusion list

2544 registered cells; 2542 executed, **0 failed**; 2 disabled in the tree; ctest
exit status 0. The >600 s cell `Continuation.ProbeBudgetBoundsAFailingProposal`
ran **alone**, detached, holding the box lock: 675.72 s, passed.

The instrument-tree disposition the M5 ledger registered
(`docs/notes/2026-08-m5-ledger.md:440-461`, "decided at the next window, never by
silently editing the pins") is **taken in this task**, inside
`scripts/run_coverage.sh`: a ctest `-E` hook with the two conditions a cell must
meet before it may be named there, and **an empty list**, because none of the
three cells M5 saw fail meets the first condition here. All three still exist
under their exact M5 names after W5's renames; all three ran in this read; all
three passed. Excluding a passing cell would drop real coverage to buy nothing.
`COVERAGE_TOLERATE_FAILURES` stays as the CI escape hatch for the runner where
M5's failures were seen, which is what the ledger says they were: an
instrument-tree, foreign-microarch class. The list is **provisional** and is
re-derived at T2's close read.

## 3. The finding that matters most for T2: five of the eight 0 % headers are not gaps

llvm-cov warned **"952 functions have mismatched data"** (M5 saw 736, W2 683).
M5's diagnosis is on record — `docs/notes/2026-08-m5-ledger.md:616-617`, "the
bounded, **understating** `HVEN_TESTING` double-compile effect, not a file-drop".
This read can show the understatement directly, which M5 could not:

| header, at 0.00 % lines | a **covered** call site | its count |
|---|---|---|
| `detail/interior/utils/timer.h` | `src/drivers/solver_init.cpp:22-23` (`Timer t; t.start();`) | **177** |
| `detail/globalization/inertia_regularization.h` | `src/drivers/ipm_solver.cpp:3423` (`dual_regularization(mu)`) | **3.12 k** |
| `detail/interior/eval_error_log.h` | `src/drivers/ipm_solver.cpp:5214` (`.reset()`) | **331** |
| `drivers/ipm_solver_types.h` | `src/drivers/ipm_solver.cpp:6070` (`result_.misc_time()`) | **54** |

For `timer.h` the merged profile contains **no counter record at all** named
`hven::utils::Timer::*`, while the linked test binary defines those symbols
(`nm -C` on `hven_interior_tests`). The functions run; their counters are not in
the profile the report was built from.

So **writing tests against those rows would buy a number, not a checked path.**
`zero-headers.txt` gives the judgement for each of the eight, one at a time. The
split is: four under-read (above), one genuinely cold (`soc.h` — its call site at
`ipm_solver_globalization.cpp:1361` reads 0 too, and the 99-line `SocRecovery`
block around it is the second-largest uncovered region in `src/drivers/`), one
dead (`math_functions.h` — `factorial`/`factorial_div` have no call site
anywhere), one compile-time only (`aggregate_arity.h`), one Eigen-dispatch only
(`super_scalar_traits.h`). And `ipm_solver_fwd.h`, which plan A11 asked be judged
like any other, is **not in the report at all** — six enums, a forward
declaration and a POD emit no coverage records.

**The experiment this suggests, named here for T2, not run here.** The three
under-read headers with real library call sites (`timer.h`, `soc.h`,
`inertia_regularization.h`) are exactly the three of the eight that are in the
library's precompiled header — `src/hven_pch.h`, applied to the `hven` target
alone at `src/CMakeLists.txt:332`, and not to any test target. That is a
correlation, not a demonstrated cause, and T0 does not claim more: the cheap test
is one coverage tree configured with the PCH off, re-read the same way, with the
0 % list and the mismatched-data count compared against this one. If the count
falls and the rows fill in, the library total here is understated by a known
amount and T2 knows which rows to stop chasing; if it does not, the M5 cause
stands alone and T2 has ruled out the other candidate for the price of one build.

## 4. What did NOT change here

No source, test, bench or CMake file was touched by this task. Its only tracked
changes are the exclusion-list block inside `scripts/run_coverage.sh` and this
directory. `docs/ci.md` was left alone deliberately: it does not document the
coverage lane at all (the string "coverage" does not occur in it), so the brief's
conditional sentence about the tolerate switch had nothing to attach to.
