# U0 delta report — the flag-unification mass re-derivation, characterized

**Event:** phase-C U0 (`docs/notes/2026-08-15-m3-phase-c-plan.md` §6; design
note `docs/notes/2026-08-16-m3-u0-design.md`). One unit: the flags commit
`bf8e897` (flags only, zero code motion), the suite-pin re-derivation
`cc97315`, the baseline re-derivation `39a27c9`, and this report.
Old-vs-new is **purely flag-attributable**: no code moved anywhere in the
unit.

**The event in four numbers.** Across the 57-cell walk census and the 17-cell
SSN bench set — 74 solved cells in total, re-derived twice each — **zero status
flips, zero counter movement, zero verdict movement.** Every old-vs-new
difference on either arm is a `kkt_residual` last-digit change. The suite moved
nine pins, of which one is a zero-pin (a reviewed finding, §2.2), one is a
knife-edge counter row (§2.1 row 7), and three are test-expression artifacts
rather than engine deltas. Performance is neutral-to-slightly-better: pooled
median wall **−0.97 %**, worst single cell **+1.21 %**.

**Acceptance state: PENDING — this report is the package the execution
reviewer reviews and the owner accepts BEFORE the new pins become the bar
(plan §6 U0(b)6). Nothing here is self-accepted.**

## 1. Provenance

| | |
|---|---|
| Old world | `8e2cbd0` (m3 pre-U0 HEAD), tests/sqp + bench on plain per-config flags |
| New world | `cc97315` (flags `bf8e897` + pins) for the suite and census; `39a27c9` (+ the baseline repoint) for P-BENCH and the final suite runs — everything on `${COMPILE_FLAGS}` |
| Host | fedora, AMD Ryzen 7 5800X3D (8C/16T, AVX2/FMA, no AVX-512), 31 GiB, Linux 7.1.5-201.fc44, governor powersave |
| Toolchain | clang 22.1.8 (Fedora), Ninja, CCACHE_DISABLE=1 clean configures |
| Backend | Intel oneAPI MKL, LP64, `libmkl_intel_thread` + `libiomp5` |
| MKL_NUM_THREADS | 1 in every asserting/measuring process |
| Flag delta (Release, the whole delta) | `-march=native -mtune=native -ffast-math -fno-finite-math-only -fomit-frame-pointer -fno-stack-protector -fno-asynchronous-unwind-tables -mllvm -inline-threshold=225 -pthread -fopenmp=libiomp5` added to tests/sqp + bench TUs |
| Flag delta (Debug) | `-g -ggdb3 -pthread -mllvm -inline-threshold=225` only — **no FP-relevant flag**, arithmetic provably unmoved (suite: 0 failures pre-edit) |

## 2. Suite re-derivation (P-SUITE, both configs, both configuration shapes)

Release at the flags commit before any pin edit: **9 failures / 1153**.
Debug at the same commit: **0 failures** — the empirical proof that every
pre-U0 pin remains the correct Debug pin, which is why every re-pin below is
config-split (`#ifdef NDEBUG`), never widened.

Post-re-derivation verification (all at `cc97315`):

| Run | Result |
|---|---|
| Release full suite, fresh run 1 (values lifted) | 9 known failures, values captured |
| Release full suite, fresh run 2 (post-edit) | **1153/1153, 0 fail** |
| Debug full suite, fresh runs 1 and 2 | **1153/1153, 0 fail** both |
| ScaleF7Slow (per-commit-excluded arm), Release | **6/6 pass — zero moved pins** |
| ScaleF7Slow, Debug | **6/6 pass** |
| no-SNOPT configuration shape (Release) | **1149 run / 0 fail** (= 1153 − the 4 SNOPT gate tests; count arithmetic exact) |
| Registered count | 1155 registered / 1153 run / 2 skipped + 2 disabled — **+0 against the phase entry state** |

Re-verified again at the final tree (`39a27c9`, i.e. after the baseline
repoint and the frozen-vs-frozen conversions):

| Run at `39a27c9` | Result |
|---|---|
| Release full suite, runs 1 and 2 | **1153/1153, 0 fail** both |
| Debug full suite | **1153/1153, 0 fail** |
| no-SNOPT shape (Release) | **1149/1149, 0 fail** |
| Registered count | still 1155 / 1153 run / 2 skipped + 2 disabled — the two continuity tests were **converted, not retired**, so no count arithmetic is owed |

### 2.1 The nine moved sites, dispositioned

Two-run bar: every re-derived value was observed in one fresh run and held in
a second fresh full-suite run (plus the intermediate targeted run); Debug
values are the pre-U0 pins, re-proven green twice.

| # | Site | Class | Disposition |
|---|---|---|---|
| 1 | `SqpDriverContract.BuildSubproblemLinearizesTheModel` | test-expression artifact | The TEST's nested difference `(qp.upper − (m.upper() − x))` was reassociated by fast-math into a form that resurrects the 0.5 absorbed by an infinite bound. Rewritten as arithmetic-free entrywise `EXPECT_EQ`. **No engine delta.** |
| 2 | `SqpDriverQpFailure.RepeatedQpMaxIterStillPropagates` | knife-edge fixture chain | MKL Release now walks the 4-row retry-succeeds-once chain the **Accelerate arm already documented (D5)** — the fixture's "needs >3 minors at every radius" premise was codegen-level, exactly as D5 diagnosed. Terminal **status pin unmoved** (`kNumericalError`). `rejected_steps` 1→2 (Release). Debug keeps the 2-row chain. |
| 3 | `SqpDriverRadius.FloorRaisesTheRestorationRequest` | fixture mechanism isolation | Under fast-math the elastic tier's exhaustion (restoration route 2) fires on this fixture at Δ≈2^−18, pre-empting a 1e−10 floor — the test was measuring the wrong trigger in Release. Floors moved {1e−10,1e−4}→{1e−5,1e−2} so route 3 (the floor) is the operative trigger in BOTH configs; every claim of the test re-verified. Solve status unchanged (`kInfeasible`, certified). |
| 4 | `B1Gate.EqualityOnlyWarmSolvesAreBitIdenticalAcrossTheRepair` | trajectory constants | 3 of 8 MKL constants moved last-bits: HS7 x0 `3.4794942110750977e-10`→`3.4794942110708625e-10` (= the Accelerate arm's value — a near-zero reading), HS26 f `2.282201458793485e-12`→`2.2822014587976076e-12`, HS77 f `0.24150512879002811`→`0.24150512879002839` (28 ulp-ish; the origin flagged HS77 as the 4-ulp-margin trap). Counters, statuses, and the in-process re-solve identity **unmoved** — the control's purpose intact, only its encoding moved, exactly as it did between backends. |
| 5 | `FunnelFType.SwitchingAndArmijoBoundariesAreInclusive` | boundary construction | δ·h·h at h=1e−3 rounds association-dependently; the test's boundary landed one ulp below the strategy's. h moved to 2^−10 (power-of-two scaling is exact under every association). Semantics asserted unchanged; config-independent. |
| 6 | `Predictor.PredictorHandlesBoundCrossing` | **ZERO-PIN GONE NONZERO — reviewed finding** | see §2.2 |
| 7 | `WarmStartBattery.Corpus` (kPins) | knife-edge counters | **One row of 30 moved**: F3stress/warm+pred, majors 7→8, minors 18→20, full_step_majors 2→3, border_refine 18→20; factorizations (6), steps, level histogram, predictor calls unchanged. Plausibility: this exact row moved under BOTH prior engine perturbations (Phase-5 Tasks 0 and 6) — it is the corpus's knife-edge by record. F3n1000/warm is unmoved → `bench_scale` `kF3n1000WarmPin` duplicate untouched, `--self-check` green old and new. |
| 8 | `ScaleF7Contract.EvalValuesMatchesEvalFCeCiBitForBit` | **contract finding** | see §2.3 |
| 9 | `CorpusTask6bPhaseB.TheShippedKSsnConfigurationIsUnmovedByTheFourLevers` | frozen-artifact float encodings | The five float residual columns moved in their 8th–10th significant digits on both probe cells; **every integer column, the status, and the per-QP shape reproduce the frozen artifact exactly** — the levers-move-nothing subject survived the flag change with zero counter movement. Release pins the U0 re-derivation inline; Debug still byte-compares the artifact; the frozen artifact is untouched. |

### 2.2 The zero-pin review (plan §6(c)2: reviewed finding, never auto-pin)

`Predictor.PredictorHandlesBoundCrossing` pinned `pred.z(1) == 0.0` exactly on
MKL: a released bound's multiplier, canceled *through the linear solve*. The
Accelerate arm of this same assertion already records (D18, quoted in the
test) that the bit-exact zero is a codegen coincidence, with an observed
Apple residue of −7.7e−34 and a 1e−12 bound argued from the O(‖dp‖)
magnitude this assertion actually guards. Under `-ffast-math -march=native`
MKL Release now shows the same phenomenon: residue **−5.4805061420557189e-35**
(two fresh reproductions), thirty orders below the guarded term. Review
verdict: the MKL arm adopts the D18 arm's own bound (1e−12) with the residue
recorded in place. This is the only zero-pin the event moved; no escape
count, refusal count, or `suspect_escalations` fixture moved anywhere.

### 2.2a Per-pin plausibility table — the knife-edge and zero pins (plan §6(c)2)

Every pin in the classes §6(c)2 names — zero-pins, escape counts, refusal
counts, `suspect_escalations` fixtures, the battery's exact minors — with the
eyeball verdict. **Status and verdict pins are listed too, because the rule is
that they must not move at all.**

| Pin class | Site | Old | New | Verdict |
|---|---|---|---|---|
| Zero-pin (exact `== 0.0`) | `Predictor.PredictorHandlesBoundCrossing`, `pred.z(1)` | `0.0` exactly | `−5.4805061420557189e-35` | **MOVED — reviewed finding, §2.2.** Adopts the Accelerate arm's own already-argued 1e−12 bound; residue is 30 orders below the guarded term. NOT auto-pinned |
| Escape counts | census `escapes` column, all 57 cells | — | — | **unmoved on every cell** (see `census-delta-old-vs-new.csv`) |
| Escape counts | P-BENCH SSN arm `escapes`, all 17 cells | — | — | **unmoved on every cell** (`pbench-old-vs-new.csv`) |
| Refusal / tier-3 counts | `CorpusTask6bPhaseB` per-QP shape + the battery scorers' `0 accepted / 0 refused` | — | — | **unmoved** |
| `suspect_escalations` fixtures | every fixture asserting it | — | — | **unmoved — no fixture in this class moved anywhere in the event** |
| Battery exact minors | `WarmStartBattery.Corpus` kPins, 30 rows | 29 rows | 29 rows | **unmoved** |
| Battery exact minors | same, F3stress/warm+pred row | maj 7 / min 18 / full_step 2 / border_refine 18 | 8 / 20 / 3 / 20 | **MOVED — plausible.** The one knife-edge row of the 30; it moved under BOTH prior engine perturbations (Phase-5 Tasks 0 and 6), so it is the corpus's knife-edge by record. Factorizations (6), steps, level histogram and predictor calls all unmoved — the row took one extra minor iteration, it did not change character |
| Battery exact minors | `bench_scale --self-check` (`kF3n1000WarmPin` duplicate) | 13 values | same 13 | **unmoved — byte-identical output old arm and new arm** |
| **Status pins** | every solve-status assertion in the suite; all 57 census statuses; all 17 P-BENCH statuses | — | — | **ZERO moved.** Includes the two whose surrounding fixture changed (§2.1 rows 2 and 3): `kNumericalError` and `kInfeasible` both held |
| **Verdict pins** | the four G-gates on the walk baseline and on the kSsn battery; the phase-7 verdict | all fail | all fail | **ZERO moved — no figure moved either** (§2.4) |

The only two movements in the whole table are the zero-pin and the single
knife-edge battery row, both dispositioned above and neither auto-pinned.

### 2.3 Findings surfaced for the reviewer/owner (not silently dispositioned)

1. **`nlp_model.h`'s `eval_values` contract says "bit-identical to calling
   eval_f/ce/ci directly."** Under fast-math, two separately-compiled loops
   over the same arithmetic are not bit-identical by construction, and F7's
   merged-loop override measurably isn't (f still matches bit-for-bit; cE/cI
   differ in last bits). The test's Release arm now holds a 1e−14 relative
   gate (the transcription-slip guard it exists for is intact — a slipped
   term is O(1) relative); Debug keeps bit-for-bit. **The contract sentence
   itself needs an owner ruling** — options: re-word to "identical under
   value-preserving compilation; agreeing to reassociation residue under the
   library's flag regime," or require overrides to share loop bodies with
   the split evals. The library header was deliberately NOT edited by this
   event.
2. **The B1Gate/PhaseB/D5-class per-backend constant arms now also encode a
   per-flag-regime dependence.** Three sites' MKL Release constants are U0
   values while Debug holds pre-U0 values; if a future flag event moves them
   again, the config-split precedent set here applies.
3. **macOS lane (Accelerate) re-observations are still owed** — see §6.
4. **Two frozen-continuity tests are now frozen-vs-frozen, so they no longer
   watch the live engine.**
   `CorpusBaseline.TheReSweptWalkArmIsCounterIdenticalToTheFrozenPreU0Baseline`
   (renamed) and `CorpusTask6bRepair.TheWalkArmIsCounterIdenticalAcrossTheD0Repair`
   were converted rather than retired: they still assert their historical claim
   about the origin-era artifacts, but the live half of that claim ended at the
   flag unification, exactly as plan §6 U0(b)5 anticipated. **What replaces the
   live watch is the census itself** — the 57-cell comparison against the new
   baseline of record is the standing counter-identity instrument from here on,
   and it is strictly stronger (it re-solves; the converted tests only diff
   files). Converting kept the registered count at 1155, so no count arithmetic
   is owed. Recorded here so the reviewer sees the coverage change explicitly
   rather than inferring it from a rename.

### 2.4 Documented-verdict constants (plan §6(c)3)

Recomputed from the **new** baseline by re-scoring it offline through the same
evaluator (`--from-csv … --score-gates`), which is what the pinned test does on
every `ctest`. Every figure is unchanged, and therefore so is every verdict:

| Figure | Pre-U0 | Post-U0 |
|---|---|---|
| rows | 57 | 57 |
| DNF rows | 10 | 10 |
| ENGINE-ERROR rows | 0 | 0 |
| KKT-gated / WRONG-ANSWER rows | 43 / 0 | 43 / 0 |
| G1/G2 population | 8 cells | 8 cells |
| G1 median factorizations per QP | 1000000.000 fail | 1000000.000 fail |
| G2 p95 factorizations per QP | 1000000.000 fail | 1000000.000 fail |
| G3 median growth 5000→20000 | 1000000.000 fail | 1000000.000 fail |
| G4 escape rate per QP | 0.6369 fail | 0.6369 fail |
| G1/G2 with kCorrupted admitted | 12 cells, both fail | 12 cells, both fail |

**No G-gate verdict flipped and no G-gate figure moved** — there is nothing
Phase-8-relevant to surface loudly here. The mechanism is the reason: the gates
read counters and statuses, and the event moved neither. The `--score-gates`
output of census run 1 and census run 2 is identical apart from the output path.
The kSsn battery's documented verdict is unaffected by construction (a frozen
artifact, re-scored — verified, not assumed; §5).

## 3. Census re-derivation (two full-57 runs, P-CENSUS × 2)

Protocol: `scripts/run_walk_census.sh`, parallel-tiered v1 (T1 6-wide pinned
physical cores / T2 solo / T3 5-wide full budget), `MKL_NUM_THREADS=1` per
process, SMT siblings idle, both runs tiered from the OLD baseline of record
so the two runs share one fixed schedule; each run ALONE on the machine, the
two runs back-to-back. The co-run protocol declaration is stamped in each
merged CSV's provenance header. Wall-clock from these runs is informational
only.

Run 1: launched 2026-08-16T08:25:53−04:00, completed 14:24:35, rc=0, 57/57
rows, merge clean (binary stamp `cc973157cea8`, no `-dirty`). Run 2: launched
14:26:22 (back-to-back), completed 20:25:21, rc=0, 57/57 rows, merge clean,
same binary stamp, same schedule.

**Run 1 vs run 2 — the two-run bar, met on more than it had to be.** The two
runs **byte-agree on every column of the schema-37 row except `wall_s`**, on
all 57 cells: not just the 13 asserted columns but the whole KKT-residual
breakdown and the whole escape census. Zero disagreements means **no cell owes
a serial-confirm re-run** — §7's re-run-alone-before-calling-it-real rule has
nothing to fire on. The new baseline of record therefore carries run 1's rows
verbatim, and every column it ships is two-run reproduced. `wall_s` is the one
column that differs (47 of 57 cells), which is expected and is exactly why it
asserts nothing: it is co-run scheduling-dependent by construction, is never
quoted as a timing, and is read only to tier a later census. Tiering computed
from the new baseline reproduces the old assignment exactly (T1=44 / T2=3 /
T3=10), so the schedule is stable across the repoint.

**The old-vs-new delta (run 1 vs the baseline of record):**

- **Status flips: ZERO.** Every one of the 57 cells carries the same status
  old and new — 43 Optimal, 4 NumericalError, 6 dnf_budget, 4 dnf_setup.
  The statuses-must-not-regress hard bar holds with nothing to adjudicate;
  the gate-B-amended physics cell (`f7_n10000_path_physics`) reproduces its
  amended Optimal row.
- **Counter movement: ZERO.** No cell moved on factorizations, qp_minors,
  escapes, qp_subproblems, or the per-QP factorization shape. The whole
  asserted-column delta is 41 cells whose `kkt_residual` moved in its 6th–
  10th significant digit (the string column re-printed at `{:.9e}`); the
  other 16 cells (the 10 DNF sentinel rows and 6 solved cells) are
  byte-identical on all 13 asserted columns.
- Per-cell listing for all 57 cells, including the run-1-vs-run-2 agreement
  column: **`census-delta-old-vs-new.csv`**, committed beside this report.

## 4. KKT-residual distribution, old vs new

Over the 47 cells with positive residuals in both arms, log10(new/old) is
0.000 to three decimals at min, p25, median, p75, and max — the largest
relative residual movement in the corpus is a few parts in 10^6, no cell is
even 0.1% worse, and no cell changed order of magnitude in either direction.
There is no systematic degradation; the unified flags left the convergence
quality of every solved cell where it was.

Concretely: 41 of 57 cells re-print a different `kkt_residual` string, all of
them in the 6th–10th significant digit of a `{:.9e}` field; the other 16 (the
10 DNF sentinel rows and 6 solved cells) are byte-identical. The largest single
movement in the corpus is `f7_n800_path_corrupted` at 1.8e−5 relative
(8.379326530e−12 → 8.379476637e−12), and it is a *residual near 1e−11* — the
absolute movement is 1.5e−16. Direction is symmetric (some cells better, some
worse, none by a margin any gate reads), which is the signature of
reassociation noise rather than of a systematically worse solve. The
independent corroboration is the SSN arm: §7's 17 bench cells show the same
picture — every counter identical, only `kkt_residual` last digits moving —
on a different engine tier and a different cell mix.

**Flag-plausibility judgment: the observed spread is what `-ffast-math`
reassociation on a residual norm should produce, and nothing more.** A residual
is a sum of squares reduced over O(n) terms; changing the reduction order moves
the last digits by O(n·ulp) and cannot move a counter, because no counter is a
function of those digits at any tolerance the engine tests against (the nearest
threshold is nine orders away). The prediction that follows from that mechanism
— counters unmoved, statuses unmoved, residuals moving in their tail — is
exactly what all 74 re-derived cells show.

## 5. Battery deltas (P-WSB × 2)

Suite-side battery: §2.1 rows 7 (kPins) and 9 (PhaseB) carry the complete
battery-counter delta — one kPins row, zero PhaseB counter movement.
ScaleF7Slow: zero moved pins, both configs, run twice (fresh binaries, runs
on 2026-08-16). The four `--from-csv` frozen-artifact re-scores and both
documented-verdict scorings passed unchanged under the new flags in every
suite run — **verified at the event, not assumed**. `bench_scale
--self-check`: green on the old arm and the new arm binaries.

**P-WSB run twice explicitly at the FINAL tree** (`39a27c9`, Release, after the
baseline repoint and the frozen-vs-frozen conversions), each component as its
own targeted invocation rather than only inside the suite:

| Component | Rep 1 | Rep 2 |
|---|---|---|
| `WarmStartBattery.*` (the kPins battery) | 3/3 pass | 3/3 pass |
| `ScaleF7Slow.*` (the per-commit-excluded arm, run explicitly) | 6/6 pass, zero moved pins | 6/6 pass |
| The four `--from-csv` frozen-artifact re-scores + the budget-table-hash check + both converted continuity tests | 6/6 pass | 6/6 pass |
| `bench_scale --self-check` | PASSED | PASSED |

The four `--from-csv` re-scores are named, because plan §6 U0(b)5 requires them
verified rather than assumed: the walk baseline (now the U0 artifact), the kSsn
gate battery, and the two Task-6b post-repair artifacts (`ssn_resweep.csv`,
`walk_resweep.csv`). All four re-score to their documented figures under the new
flags. `bench_scale --self-check`'s 13-value output is **byte-identical between
the old-arm binary (`8e2cbd0`) and the new-arm binary (`39a27c9`)** — the
strongest single statement in this section, since it is a live solve compared
across the whole flag change.

## 6. Accelerate-side values (plan §6(c)4)

Not observable from this machine; **nothing is fabricated** (CLAUDE.md §6).
The unified flags change the macOS lane's arithmetic too (`-ffast-math
-mcpu=apple-m1` per the CI posture), so the M3-4 arm pins, the
divergence-register rows, and the Accelerate constant arms in
`test_b1_gate.cpp` / `test_predictor.cpp` / `test_corpus_cells.cpp` are
expected to need lane re-observation. Plan of record: take what the U0 push's
macOS lane runs give (run IDs recorded), two-run bar for any committed
counter, register updated, remainder folded into the scheduled Mac session
(§7/O12); anything unobservable stays UNOBSERVED.

[CI-PENDING: lane run IDs and outcomes for the pushed unit.]

## 7. P-BENCH old-vs-new (the "without sacrificing performance" measurement)

Protocol: B2's committed 17-cell `--engine ssn` wall set, 4 reps per arm,
median reported; strictly serial, one solve at a time, machine otherwise
idle, `MKL_NUM_THREADS=1`; `bench_scale --self-check` green per arm. Old arm:
`8e2cbd0` clean build, measured 2026-08-16 00:08:58→00:36:47 −04:00, BEFORE
the flags commit existed. New arm: `39a27c9` clean build (`CCACHE_DISABLE=1`,
identical configure line), measured 21:03:11→21:31:19 the same day on the same
idle machine. Each rep is the same 17 cells in the same order, pinned to CPU 2.
These are the event's only quotable timings. Full four-rep data:
**`pbench-old-vs-new.csv`**.

| cell | old median (s) | new median (s) | Δ |
|---|---|---|---|
| f7_n750_path_neutral_control | 0.268 | 0.270 | +0.62 % |
| f7_n800_path_neutral | 0.315 | 0.318 | +0.68 % |
| f7_n800_path_corrupted | 0.189 | 0.191 | +0.94 % |
| f7_n800_path_warm | 0.160 | 0.161 | +0.87 % |
| **f7_n800_path_physics** | 5.138 | 5.063 | **−1.46 %** |
| **f7_n800_path_activity** | 3.869 | 3.810 | **−1.51 %** |
| f7_n825_path_neutral_control | 0.325 | 0.327 | +0.66 % |
| f7_n1000_path_neutral | 0.322 | 0.324 | +0.78 % |
| f7_n1000_path_corrupted | 0.254 | 0.257 | +0.98 % |
| f7_n1000_path_warm | 0.259 | 0.261 | +0.74 % |
| **f7_n1000_path_physics** | 8.707 | 8.527 | **−2.07 %** |
| **f7_n1000_path_activity** | 7.056 | 6.917 | **−1.98 %** |
| f7_n2000_path_corrupted | 0.694 | 0.699 | +0.72 % |
| f7_n2000_path_warm | 0.274 | 0.277 | +1.02 % |
| f7_n5000_path_activity | 1.210 | 1.220 | +0.79 % |
| f7_n10000_path_activity | 1.990 | 2.014 | +1.21 % |
| f7_n20000_path_activity | 4.393 | 4.443 | +1.15 % |
| **POOLED** | **35.423** | **35.079** | **−0.97 %** |

**Counter identity across the arms.** All 17 cells reproduce their status,
every integer counter, and the per-QP factorization shape exactly; the only
old-vs-new difference on any cell is `kkt_residual`'s last digits. Within each
arm the 13 asserted columns are identical across all four reps (`intra-arm
counter reproducibility: OK`). So this measurement compares the *same work*, not
two different solve trajectories — which is what makes the timing comparable at
all.

**Verdict on Grant's "without sacrificing performance": SATISFIED —
neutral-to-slightly-better.** Pooled median wall is **0.97 % faster** under the
unified flags. The distribution is bimodal and the split is mechanistically
legible: the four cells that got *faster* (−1.5 % to −2.1 %) are exactly the
four cells where the SSN tier actually engages (non-zero `qp_minors`, an
escape), i.e. the cells that spend their time in engine code the flags now
optimize; the thirteen that got marginally slower (+0.62 % to +1.21 %, worst
case) are the short cells where fixed per-solve overhead dominates the measured
wall. Because the heavy cells carry the pooled figure, the aggregate moves in
the favourable direction. **No cell regressed by more than 1.21 %**, and no
regression anywhere approaches a magnitude that would trade for the evidence
uniformity the unification buys. Caveat stated plainly: at this magnitude these
are small effects on a `powersave`-governor machine; they are reported as
measured (serial, alone, median of four) and are not extrapolated.

## 8. The new baseline and the consumer sweep

**The artifact.** `bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv` — a
NEW dated directory, not an in-place rewrite (plan §6 U0(b)3), so the gate-A and
gate-B records that cite `bench/baselines/2026-08-06-corpus/walk_baseline.csv`
*by content* keep citing an unmoved file. The old file is **untouched**: byte
count, mtime-irrelevant content and its origin-naming provenance header all
stand, under CLAUDE.md §1's frozen-header exception. That also closes Q6b's
origin-string question the way the plan wanted it closed — by confining it to
the frozen file. **The new header is hven-native and contains no origin naming
in any line** (checked mechanically over the commit's added lines).

The new header declares, in the artifact itself: what the file is and what it
replaces; the two-run derivation with both runs' timestamps and outcomes; the
co-run protocol reproduced *verbatim from the run that produced the rows*
(tiering rule, tier widths, physical-core pinning, `MKL_NUM_THREADS=1`, SMT
siblings idle) per CLAUDE.md §7; that `wall_s` is informational and never a
quotable timing; and the one-line old-vs-new delta with a pointer here.

**The consumer sweep, enumerated and green in the same commit as the repoint**
(`39a27c9` — the rule exists because a gate-B amendment left three consumers red
for three unwatched pushes; see `docs/testing.md`):

| # | Consumer | Class | Outcome |
|---|---|---|---|
| 1 | `CorpusBaseline.TheCommittedWalkBaselineScoresToItsDocumentedVerdict` | scorer (`--from-csv --score-gates`) | Re-derived from the new artifact; **every pinned figure unchanged** (§2.4) |
| 2 | `CorpusBaseline.TheCommittedWalkBaselineCarriesTheCommittedBudgetTableHash` | header/row-count | New file carries `budget_table_hash: 0x357aee91dee27391` and 57 data rows |
| 3 | `CorpusBaseline.TheReSweptWalkArmIsCounterIdenticalToTheFrozenPreU0Baseline` | cross-comparison | **Converted to frozen-vs-frozen** and renamed; reads the new `HVEN_SQP_PRE_U0_WALK_BASELINE_CSV` define |
| 4 | `CorpusTask6bRepair.TheWalkArmIsCounterIdenticalAcrossTheD0Repair` | cross-comparison | **Converted to frozen-vs-frozen**; same define |
| 5 | `CorpusBaseline.TheCommittedSsnBatteryScoresToItsDocumentedVerdict` | `--from-csv` re-score of a frozen artifact | Verified unmoved under the new flags |
| 6 | `CorpusTask6bRepair.ThePostRepairArtifactsCarryTheCensusAndRescoreCleanly` | `--from-csv` re-score, two frozen artifacts | Verified unmoved under the new flags |
| 7 | `scripts/run_walk_census.sh` default `--baseline` | tooling | Repointed; tiering from the new file reproduces T1=44 / T2=3 / T3=10 exactly |
| 8 | `docs/testing.md`'s amendment-sweep episode record | documentation | Post-U0 addendum added: new baseline of record named, the rename and the two conversions recorded |

Nothing else reads the define (checked by grep over the tree). Suite evidence
for the sweep is §2's `39a27c9` table: Release ×2, Debug, and the no-SNOPT shape
all green.

**Disposition record for the frozen-continuity tests (plan §6 U0(b)5).**
Converted, not retired, and argued in the commit message: both tests assert
continuity between origin-era artifacts *and*, until this commit, the live
baseline. The flag unification ends the live half of that claim, and the
single-cell adjudicated-exception mechanism plainly does not stretch to 57
cells. Converting preserves the historical claim — which is still worth
asserting, since it is what discharged the gate-B record — while the census
against the new baseline of record takes over the live counter-identity watch,
and does it better (it re-solves rather than diffing files). Registered count
unmoved at 1155, so no count arithmetic is owed. The coverage change is
surfaced as a reviewer finding in §2.3 item 4 rather than left implicit in a
rename.
