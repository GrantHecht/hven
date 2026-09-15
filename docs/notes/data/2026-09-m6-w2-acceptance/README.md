# M6 W2 — feasibility-mode acceptance evidence (T8)

Produced 2026-09-04 on branch `m6` at `4877bec` (code head `10a3dac`).
**Read `PROVENANCE.txt` first** — it records the commits, the toolchain, the
hardware, the declared serial protocol and the cpu10 idle snapshots, and it
governs every number below. Every value here was measured in this task; none is
carried over from an earlier task's report.

Everything is **MKL on Linux**. Apple/Accelerate values are **UNOBSERVED** and
none is inferred.

```
docs/notes/data/2026-09-m6-w2-acceptance/
  PROVENANCE.txt              the stamp, the serial rule, the cpu10 snapshots
  suites.txt                  the four ctest legs, both configs
  refusal-pins.txt            the refusal contract's pins, by name, both configs
  a4-gate-t8.csv              the A4 gate's 29x66 rows, this run
  a4-gate-verdict.txt         its printed verdict and exit status
  a4-comparison.txt           vs the committed W1 gate CSV
  kipm-counters-release.csv   the kIpm counter leg, Release
  kipm-counters-debug.csv     the kIpm counter leg, Debug
  coverage-summary.txt        llvm-cov's per-file table (instrument tree)
  coverage-areas.txt          the same, aggregated by area
  replay/base-{walk,ssn,ipm}.csv   U0 27 cells at 0dda3db
  replay/head-{walk,ssn,ipm}.csv   U0 27 cells at 4877bec
  replay/comparisons.txt           the five 0-difference lines
  replay/u0-reachability.csv       the counters the 76-column schema cannot carry
  probes/compare_replay.py         the comparator (recomputes comparisons.txt)
  probes/kipm-counter-probe.cpp    the kIpm leg's own source
  probes/u0-reachability-probe.cpp the reachability probe's own source
```

The 27-cell list is **not duplicated here**: it is
`docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt`, unchanged, and
both replay arms were driven from that file.

---

## 1. Suites — both configs, split, Debug first

`suites.txt`. Fresh reconfigure of both trees first; the stamp check is in
`PROVENANCE.txt`.

| config | leg | result | wall (informational) |
|---|---|---|---|
| Debug | `ctest -j4 -LE a4_gate -E ProbeBudgetBoundsAFailingProposal` | 100% passed, **0 failed out of 2251** | 357.69 s |
| Debug | `ctest -R Continuation.ProbeBudgetBoundsAFailingProposal` (solo) | 100% passed, 0 failed out of 1 | 673.71 s |
| Release | `ctest -j4 -LE a4_gate -E ProbeBudgetBoundsAFailingProposal` | 100% passed, **0 failed out of 2251** | 45.69 s |
| Release | `ctest -R Continuation.ProbeBudgetBoundsAFailingProposal` (solo) | 100% passed, 0 failed out of 1 | 12.87 s |

2254 tests are registered in each tree. Three do not run in the split leg, in
both configs, and are named rather than netted out: `EqpRefinementAb.FootprintRuleProbe`
and `EqpRefinementAb.FullBattery` are **Disabled** (the A/B probe battery), and
`FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas`
**Skips** (no OLD-SEAM adapter in this build). `-LE a4_gate` selects nothing
away: `HVEN_A4_GATE=OFF`, so that label registers zero tests here and the gate
is run as the arm binary (section 2).

## 2. The A4 gate — run in full, not argued

`a4-gate-verdict.txt`, `a4-gate-t8.csv`, `a4-comparison.txt`. Release, solo,
`taskset -c 2`, `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`.

```
A4 GATE (Amendment F), 29 cells:
  [RED] every cell converges               28 / 29
  [RED] iterations < 40                    28 / 29
  [RED] tier-contract recovery             13 / 27
  [RED] end-to-end recovery                12 / 27
  [RED] exact recovery (E1 Rule A)          9 / 27
  [PASS] no blow-up across active fraction
  factorizations: tier 718, tier+polish 746 (PIQP: 9-18 iters/cell)
  RED CELL: e1_f7_n20000_af30_m1e-6  (the ONE named exception)
A4 VERDICT: GREEN but for the named exception     (exit status 0)

=== committed a4 gate CSV (W1 acceptance, mu=1e-2) vs T8 re-run ===
    cells compared: 29  columns compared: 65  differences: 0
```

The gate is **exactly where W1 left it**: 0 differences on all 65 non-wall
columns of all 29 cells against `../2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv`.
W2 moved the tier's escape ROUTE, not the tier, and the gate is tier-direct.
The E1 proxy caveat did not retire at W1 and does not retire here.

## 3. The U0 27-cell three-arm replay — and why the window composes

`replay/`. Six legs (3 arms x {BASE, HEAD}), each solo, one at a time, under
the box lock, `taskset -c 2`, `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`.

```
provenance:  # binary: 0dda3dbcd027   (BASE, the W2 window's base = b1eb623^)
             # binary: 4877bec876ad   (HEAD)

=== base-walk vs head-walk ===                    27 cells  75 columns  0 differences
=== base-ssn  vs head-ssn  ===                    27 cells  75 columns  0 differences
=== base-ipm  vs head-ipm  ===                    27 cells  75 columns  0 differences
=== committed t10b ipm baseline vs head-ipm ===   27 cells  75 columns  0 differences
=== committed t10b ipm baseline vs base-ipm ===   27 cells  75 columns  0 differences
```

Every W2 task replayed 0-diff against its own base. This leg proves the
**window composes**: the whole of W2, against the commit W1 closed at, moves
nothing on the corpus. The fifth line is the control — it shows the BASE
binary also reproduces the committed t10b baseline, so the fourth line is a
statement about the tree and not about this run's conditions.

### 3.1 Why no U0 cell reaches W2's declared breaks — MEASURED, not assumed

The corpus schema has 76 columns and **none of them is an elastic,
restoration, seed or refinement column**, so a 0-difference replay does not by
itself say those paths were not taken. `replay/u0-reachability.csv` closes that:
it re-runs the same 27 cells in the same three arms through the corpus's own
generators, starts and options (`bench/corpus_cells.h`), and reads the counters
the row cannot carry. All 81 cells:

| counter | walk (27) | ssn (27) | ipm (27) |
|---|---|---|---|
| status | Optimal x27 | Optimal x27 | Optimal x27 |
| `major_iters` | 1–2 | 1–2 | 1–2 |
| `rejected_steps` | **0** | **0** | **0** |
| `elastic_activations` | **0** | **0** | **0** |
| `elastic_from_ipqp_escape` | **0** | **0** | **0** |
| fallback trace events (any verdict, incl. `kUnfired`) | **0** | **0** | **0** |
| `ipqp_escapes` | **0** | **0** | **0** |
| `restoration_iters` | **0** | **0** | **0** |
| `restoration_seed_used` rows | **0** | **0** | **0** |
| `verdict_refine_steps` | **0** | **0** | **0** |
| `border_refine_steps` | 2–12 (sum 68) | 0 | 0 |
| `soc_steps` | **0** | **0** | **0** |

Cell class by cell class:

* **T1 (ladder extraction), T2 (`rho_0` + working-set seed), T3 (the fallback
  body and the refusal rule), T5 (counters, placement bound, floor retry).**
  `elastic_activations == 0` and **zero fallback trace events** on all 81
  cells. Not one U0 cell enters the ladder by any route, and not one enters
  `certified_feasibility_fallback` at all — not even on pin P5's unfired
  branch, which would still emit a `kUnfired` event. The kIpm arm's own row
  agrees from the other side: `ipqp_escapes == 0` and `ipqp_to_walk == 0` on
  all 27 cells (`replay/head-ipm.csv`), so the escape branch is never reached.
* **T4 (the restoration seed, a DECLARED BREAK, mode-independent).**
  `restoration_iters == 0` and `restoration_seed_used` on **0** rows in every
  arm. Restoration has four request sites — the elastic exhausted arm, the
  judged-rejection floor, the funnel's `kRestore` sites and the routed-failure
  floor — and none can fire on a cell that reaches `Optimal` in one or two
  majors with `rejected_steps == 0`. The break is unreachable here for the
  structural reason T4 itself recorded, and this is the positive measurement of
  it rather than the inference.
* **T6b (the border verdict-site refinement) and T7 (its eliminated twin, and
  the provenance-keyed dispatch).** `verdict_refine_steps == 0` on all 81
  cells. Both twins are entered only when the classifier has already said
  `kInfeasible` at a dead end; no U0 subproblem is ever certified infeasible
  (`kkt_verdict` is `ok` and `status` `Optimal` on every row, every arm). The
  reading is **not vacuous through "no bordered solve happens"**: the walk arm
  charges 68 `border_refine_steps` across its 27 cells, so the bordered path is
  genuinely exercised — it simply never reaches a verdict site. T7's dispatch
  key (`eqp.refine_steps > 0`) is therefore never consulted on this corpus.
* **T7's corpus leg.** Independently negative and already recorded:
  `docs/notes/data/2026-09-m6-w2-t7-corpus-sweep/` swept 1200 F7 cells and
  found no configuration that reaches the elastic tier in any mode.

## 4. The kIpm counter leg — the W2 counters, with both identities

`kipm-counters-release.csv`, `kipm-counters-debug.csv`; source at
`probes/kipm-counter-probe.cpp` (fixtures copied verbatim from
`tests/sqp/test_sqp_driver.cpp`, cited there by line). Solo, pinned,
single-threaded, both configs. Cells with no fallback and no elastic activity
are in the CSVs and omitted here.

**Release** (`major_iters` and the ladder columns move with the trajectory;
see the Debug note below):

| cell | mode | status | majors | fb entries | fired | disp | relax | exh | rungB | unfired | `elastic_activations` | `elastic_escalations` | `elastic_from_ipqp_escape` | `ipqp_suspicion_disproved` | `ipqp_fallback_rung_b` | `elastic_rho0_ceiling_hits` | `elastic_floor_retries` | seed rows | ceiling rows | `verdict_refine_steps` | identities |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| hs10 | ipm | Optimal | 14 | 3 | 3 | 0 | 3 | 0 | 0 | 0 | 8 | 44 | 3 | 0 | 0 | 1 | 0 | 0 | 1 | 0 | OK |
| hs11 | ipm | Optimal | 7 | 2 | 2 | 0 | 2 | 0 | 0 | 0 | 2 | 6 | 2 | 0 | 0 | 1 | 0 | 0 | 1 | 0 | OK |
| hs15 | ipm | Optimal | 9 | 2 | 1 | 0 | 1 | 0 | 0 | 1 | 2 | 9 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | OK |
| hs27 | ipm | Optimal | 4 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 6 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | OK |
| hs38 | ipm | Optimal | 49 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | OK |
| F-1 `f1_scaled_inconsistent` | walk | Infeasible | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2 | 12 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | OK |
| F-1 `f1_scaled_inconsistent` | ssn | Infeasible | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2 | 12 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | OK |
| F-1 `f1_scaled_inconsistent` | ipm | Infeasible | 2 | 2 | 2 | 0 | 1 | 1 | 0 | 0 | 2 | 11 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | OK |
| `t7_exhaustion_attached_report` | ipm | Infeasible | 3 | 3 | 2 | 0 | 1 | 1 | 0 | 1 | 2 | 4 | 2 | 0 | 0 | 2 | 0 | **1** | 2 | 0 | OK |
| `t3a_refusal_then_walk` | ipm | Infeasible | 2 | 2 | 2 | 0 | 0 | 1 | 1 | 0 | 3 | 12 | 2 | 0 | 1 | 0 | 0 | 0 | 0 | **15** | OK |
| P4 `p4_refusal_unit` (direct call) | — | kInfeasible | — | 1 | 1 | 0 | 0 | 0 | 1 | 0 | 1 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | OK |

The last row is **not a driver run**: it is a DIRECT call to
`certified_feasibility_fallback` with a floor-placed evidence block on an
engine whose radius declines rung A (the P4 fixture,
`tests/sqp/test_sqp_driver.cpp:9443`), so it has no majors and its "status" is
the returned `QpSolution`'s — rung B's own `kInfeasible`, passed through
verbatim, never synthesized.

**The two identities were checked on every row of both configs; 0 failures.**

* **Identity 1, the activation identity** — `elastic_activations ==`
  walk-route activations `+ elastic_from_ipqp_escape`. The walk-route term has
  no counter, so it is read as the residual and asserted non-negative on every
  row. It is non-zero exactly where a walk route exists and is taken: hs10
  (8 − 3 = 5), hs15 (2 − 1 = 1) and hs27 (1 − 0 = 1), where the **unfired**
  entry takes pin P5's cold walk and the walk's own `kInfeasible` then enters
  the tier; and `t3a_refusal_then_walk` (3 − 2 = 1), which is the T3-A route's
  disclosed second ladder. It is 0 on the cells where rung A owns every
  answer.
* **Identity 2, the ENTRY partition** — `disproved + relaxed + exhausted +
  rung_b == fired entries == elastic_from_ipqp_escape − elastic_floor_retries`,
  with the unfired entries outside it (`entries − unfired == fired` on every
  row) and the two named counters agreeing with the trace stream
  (`disproved == ipqp_suspicion_disproved`, `rung_b == ipqp_fallback_rung_b`).

**The HS corpus at kIpm, aggregated over all 27 models:** 9 fallback entries,
6 fired, **0 disproved**, 6 relaxed, 0 exhausted, 0 rung-B — reproducing
`T7TheDisprovedSuspicionCalibrationOverTheHsCorpusIsZERO` exactly. Read it as
T7 recorded it: "no false fire OBSERVED on HS", not a specificity estimate.

**Debug vs Release**, stated rather than netted: the two configs agree on every
HS row, on `t3a_refusal_then_walk` and on `p4_refusal_unit`. They differ on
two cells, both already known:

* **F-1** runs 2 majors in Release and 3 in Debug (`m` is the count the T7 F-1
  pin deliberately leaves unpinned), so its activation and escalation columns
  scale with it — the ROUTE census is identical in both.
* **`t7_exhaustion_attached_report`** records `restoration_seed_used` on 1 row
  in Release and **0** in Debug. This is exactly T7 item 2's finding, seen
  again here from a different instrument: at an ATTACHED-REPORT exhaustion the
  elastic offer is at rounding (2.22e-16 against h = 4e8), so whether the guard
  takes it is config-unstable. T4's config-stable elastic-site take is the
  scaled fixture, not this one.

## 5. The refusal path, pinned by name

`refusal-pins.txt`. All seven pass in **both** configs (Debug and Release,
7/7 each):

| pin | what it holds | origin |
|---|---|---|
| `SqpDriverCertifiedFallback.P4ARefusedRungAReturnsTheColdWalkCounterForCounter` | rung A declined ⇒ rung B's walk solution verbatim, counters-identical to W1 on a fresh engine | plan §2 P4 (T3) |
| `SqpDriverCertifiedFallback.P4OnAPrimedEngineCostsTheAnalysisRungADisplaces` | on the driver's own primed engine the same refusal costs +1 `symbolic_analyses` | T3 fix1, declared |
| `SqpDriverCertifiedFallback.P4bTheCeilingDeclineIsGONEUnderThePlacementBound` | the T3 false-ceiling decline no longer exists under T5's bound; the entry DISPROVES | T3 P4b → T5 declared break → T6b flip |
| `SqpDriverCertifiedFallback.P5AnUnfiredEvidenceBlockNeverEntersRungAAtAll` | `fired == false` ⇒ W1's cold walk, no report, nothing charged | plan §2 P5 (T3) |
| `SqpDriverCertifiedFallback.ADeclinedRungAAboveTheFloorIsRETRIEDThereExactlyOnce` | a decline above the floor is retried once AT the floor, charged as a second activation | T5 (owner-backed, T3 F6) |
| `SqpDriverCertifiedFallback.T3ARefusalWhoseWalkCertifiesInfeasibleCostsTwoActivations` | the disclosed T3-A cost: refusal + walk `kInfeasible` ⇒ exactly 2 activations | T3-A, accepted with disclosure |
| `SqpDriverCertifiedFallback.T7TheT3ARouteIsREACHABLEAtTheDriverAndCostsOneExtraLadder` | the same route at the DRIVER, and the measurement that REFUTED the registered remedy | T7 item 6 |

## 6. Coverage read — informational, for the readiness view

`coverage-summary.txt` (llvm-cov's per-file table) and `coverage-areas.txt`
(aggregated). LLVM source-based coverage on the `linux-clang-coverage`
**instrument tree**, per `scripts/run_coverage.sh`'s own header: coverage flags
change codegen, so **nothing here is a pin, a baseline or a timing**. ctest
under the instrument tree exited 0. `llvm-cov` warned "683 functions have
mismatched data" (stale profile records for functions whose bodies moved); the
totals below are what it reported after that warning.

| area | files | regions | functions | lines | branches |
|---|---|---|---|---|---|
| **TOTAL (library sources)** | 122 | 85.93% | 80.54% | **82.74%** | 80.27% |
| `src/drivers/` | 10 | 77.84% | 79.85% | **77.94%** | 70.48% |
| — of which `src/drivers/sqp*.cpp` | 3 | 95.31% | 97.75% | **94.46%** | 86.26% |
| — of which `src/drivers/interior_point_solver*` | 4 | 65.09% | 71.78% | **66.26%** | 57.46% |
| `src/qp/` | 5 | 94.87% | 99.42% | **94.38%** | 87.97% |
| `include/hven/detail/globalization/` | 21 | 74.67% | 76.73% | **54.89%** | 69.09% |
| `include/hven/detail/qp/` | 8 | 96.99% | 98.57% | **94.54%** | 91.54% |

**Against M5's close** (79.2% real overall; `src/drivers` 67.8% — the M6
register's named gap): total lines **82.74%** (+3.5 points) and `src/drivers`
**77.94%** (+10.1 points). **No target is asserted** — the number is reported.

What the split says, and it is the useful part: the `src/drivers` gap is **not
in the SQP driver**. `sqp_driver.cpp` reads 94.54% lines and `sqp_options.cpp`
100%; the directory's remaining 1725 uncovered lines are almost entirely the
**IPM** driver's four TUs (66.26%), of which `interior_point_solver_print.cpp`
(15.72%) and `interior_point_solver_settings.cpp` (39.88%) are printing and
options surface. The `detail/globalization/` header figure is dominated by
three files W2 does not touch — `soc.h` (0% of 28 lines), `restoration.h`
(12.23% of 139) and `globalization_mechanism.h` (5% of 20) — which are the IPM
side's templates, not the SQP tier's (`sqp/elastic.h` and `sqp/globalization.h`
are both 100%).

---

## 7. Rebuilding the two probes

Neither probe is a CMake target: both are scratch instruments committed so the
numbers above are re-derivable. Build each against a **Release** tree's
`libhven.a` with that tree's own flags:

```
clang++ -DMKL_LP64 -m64 -O3 -DNDEBUG -std=c++20 -fPIE -Wabsolute-value -pthread \
        -mllvm -inline-threshold=225 -fomit-frame-pointer -fno-stack-protector \
        -fno-asynchronous-unwind-tables -falign-loops=32 -march=native -mtune=native \
        -ffast-math -fno-finite-math-only -fopenmp=libiomp5 \
        -DEIGEN_DONT_PARALLELIZE -DEIGEN_INITIALIZE_MATRICES_BY_ZERO \
        -DEIGEN_MAX_ALIGN_BYTES=32 -DFMT_HEADER_ONLY -DFMT_USE_LOCALE=0 \
        -DHVEN_DEFAULT_QP_THREADS=8 \
        -I<repo>/bench -I<repo>/tests/sqp -I<repo>/include \
        -I/opt/intel/oneapi/mkl/latest/include \
        -isystem <repo>/dep/eigen -isystem <repo>/dep/fmt/include \
        probes/<probe>.cpp <build>/libhven.a \
        -L/opt/intel/oneapi/compiler/latest/lib \
        -Wl,-rpath,/opt/intel/oneapi/mkl/latest/lib:/opt/intel/oneapi/compiler/latest/lib \
        -Wl,--start-group /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_lp64.a \
        /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_thread.a \
        /opt/intel/oneapi/mkl/latest/lib/libmkl_core.a \
        /opt/intel/oneapi/compiler/latest/lib/libiomp5.so -Wl,--end-group -ldl -lm \
        -o <probe>
```

`kipm-counter-probe` takes no arguments and writes `kipm-counters-release.csv`
to stdout. `u0-reachability-probe` takes the comma-separated cell list — pass
`docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt` with its `#`
lines and whitespace stripped — and writes `replay/u0-reachability.csv`.

**Both were re-run from these committed sources, solo and pinned, after the
files were written: each reproduces its CSV byte-for-byte.**
