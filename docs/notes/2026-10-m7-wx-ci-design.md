# M7 WX — the PR #15 CI failures: diagnosis, mechanisms, batch plan

Status: **DESIGN for settler ruling.** Date: 2026-10-29. Base: m7 `aa19f364` (branch `wx`).
Authority: `docs/notes/2026-09-m7-brief.md` §1 WX (`:320–335`); the lane licence
`docs/ci.md:498–509`; CLAUDE.md §6 (UNOBSERVED) and §7 (residual near-ulp rule); the
settler's direction C145 (A/B/C/D below). Nothing in `tests/` or `src/` is changed by this
round. Every Linux measurement below was taken on the box (Ryzen 7 5800X3D, clang 22.1.8,
Release, `HVEN_FP_MODE=SAFER_FAST`, `HVEN_SIMD_ARCH=x86-64-v3`, `MKL_NUM_THREADS=1`, under
the box lock); no Apple or Windows value below was produced here — each one is quoted from a
named CI run's log.

## 0. Inputs

| Run | Commit | macOS (Accelerate) | Windows (MKL) | Linux (MKL) |
|---|---|---|---|---|
| 40861914857 | m7 `1ff37db2` | 67 failed / 3134 (job 123407040491) | 3 (job 123407040529) | install-smoke GCC 13 compile break → `aa19f364` |
| 40868557422 | m7 `aa19f364` | **68** failed (job 123429421516) | the same 3 (job 123429421347) | green |

Triage: `.superpowers/m7-wx-triage-2026-10-29.md` (read-only). Known-13 baseline:
`.superpowers/m7-macos-ci-failures.md` (main `0339749f`, run 34911916381).

## 1. Findings that shape the design

### 1.1 The extra macOS failure is a lane flake, and the lane is not run-to-run deterministic

The 68th failure at `aa19f364` is **`QpW1AscentGuard.BritgasElasticQp1Refactorize`**
(`tests/qp/test_qp_ascent_guard.cpp:198`, `ordinary_rose` 3 vs 0, worst rise 4.387e-03).
It **passed** in run 40861914857.

**Classification: candidate flake (lane nondeterminism), not a real change.** Evidence:

- The only source difference between the two runs is `src/qp/qp_engine.cpp`'s `kInf`
  becoming `static constexpr`. Recompiled here at both commits with the lane's flags, the
  object's disassembly is **identical** (106 386 lines, `cmp` equal). AppleClang was not
  observed, but a constant-folded `constexpr double` has no codegen surface on either.
- Diffing every failing test's printed output between the two macOS runs (timestamps and
  `(N ms)` stripped): **19 of the 68 differ run-to-run** on the same code —
  `CrossoverAdapter.TwoLaidPartitions…`, `QpBpsBorderCertificate.Acopr118Qp26Border`,
  five `QpE2Fix9StandstillRetreat` cells, `QpLatchReason.Britgas…`, both `QpNc4` cells, both
  `QpRdvRankRepair.Sawpath` cells, two `QpStagnationStop` cells, both `QpW1AscentGuard`
  cells, both `SqpElasticWitnessStart.Sawpath…` cells, `WarmStartBattery.Corpus` (timing
  column only — its counters agree). Examples: `Acopr118Qp4Border` minors 172 → 142 and
  `f` 900.63107343 → 900.631276102; `SawpathQp1Border` minors 46 → 33, `f` −375.4632 →
  −375.0042; `QpW1AscentGuard…Border` minors 1754 → 1757; `CrossoverAdapter` iterations
  21/26 → 26/24 *within each run's own two solves*.
- Cause (consistent with, not proven by, the record): the macOS lane never pins Accelerate's
  threading (`ci.yml:406–416` leaves it an open question; startup calls
  `ensure_accelerate_initialized(HVEN_DEFAULT_QP_THREADS)`, i.e. `VECLIB_MAXIMUM_THREADS=8`,
  `src/drivers/solver_init.cpp:24`, `CMakeLists.txt:494`), and the divergence register already states that nothing
  pins the reduction order on that lane (`2026-08-14-accelerate-divergence-register.md:70–74`).

Consequence for the licence: the register's premise that "counters, states and lists carry no
machine, thread or configuration context" is **false for route counters on knife-edge
fixtures** — a route counter inherits the float nondeterminism that chose the route. The M3-4
LATER DISPOSITION (register `:217–250`) already met this once within MKL and retired its arms.
So: **no Accelerate counter arm on any fixture in §2's route class**, and a lane-observed
counter arm elsewhere needs **three** agreeing lane runs, not two (WX-R6).

### 1.2 Windows: what differs is the kernel path, not the MKL version

- Windows lane: oneAPI **2026.1.0.191** (`HVEN_ONEAPI_CACHE_KEY`), clang-cl **20.1.8**,
  `mkl_intel_thread`, `MKL_NUM_THREADS=1`, UCRT libm. Linux lane: apt
  `intel-oneapi-mkl-*-2026.2 (2026.2.0-881)`, clang **18.1.3**, glibc. Box: MKL 2026.2,
  clang 22. Windows is deterministic across the two runs (all three cells byte-identical).
- **MKL version ruled out.** The same tree built here against MKL **2026.1** (static, `MKLROOT`
  swapped; `.scratch/build-mkl261`) passes all 68 cells, and every printed counter, objective
  and residual of the 68 is **bit-identical** to the 2026.2 build — only wall-clock columns
  differ (also under `MKL_CBWR=AVX2`).
- **Kernel path ruled in.** Forcing any MKL CBWR branch on the box (`COMPATIBLE`: 16 cells
  fail; `SSE4_2`/`AVX`/`AVX2`: the same 10) moves exactly the class of fixture Windows moves,
  and **all three Windows failures are inside the CBWR-10** — with values matching *neither*
  build: `QpNc4ConvexCertificate.BritgasRestorationLpQp4` border minors 805 (Windows) / 965
  (`AVX2`) / 889 (`COMPATIBLE`), each with `convex_certificates=1 rereads=3` against the pin's
  0 / 1. `QpE4ProximalEqp.TheSteenbrbRestorationQp2…` reads kOptimal on Windows, under every
  CBWR branch, and on macOS.

**Answer to C145(A)'s question: "per-backend" is really "per-build".** The pins were measured
on one build — Linux, x86-64, MKL Pardiso at its AUTO dispatch, clang, glibc libm — which the
Linux lane and the box both reproduce today. Windows is a second MKL *build* (different OS
libm, compiler and dispatch), and on these fixtures a second build is as foreign as a second
backend. §2.1 names that build and gates on it.

### 1.3 Accelerate's absent pivot count is a semantic gap in the walk, not only a missing field

`inertia_verdict` (`include/hven/detail/qp/qp_engine.h:1103–1125`) routes a factor with a
present-and-nonzero perturbed-pivot count to kSuspect. Accelerate reports the count as absent
(`src/linear/symmetric_factor_accelerate.cpp:273`), "and absent evidence … does not trigger
the rule" — so on Accelerate the walk **trusts** inertia that MKL would distrust; the
dependency test's refusal likewise "does not fire" there (`ws_independence.h:297–299`). Most
of §3's macOS route divergences (`dep_*`, `convex_certificates`, rereads, saddle exits,
latches, rank repair) run through exactly these verdicts. That is a deterministic difference in
the solver's decision tree, not a knife-edge (WX-R2). It does not change §2's test mechanism —
the reference build is still the only place the route is pinned — but it is why several class-B
cells below are "backend semantics" rather than "ulp".

### 1.4 Two stale Accelerate arms are now observed equal to MKL

`Continuation.ProbeBudgetBoundsAFailingProposal` reads **18** factorizations on the lane (the
MKL value; the Accelerate arm still asserts the pre-E2 304), and
`HsBattery.CrashBasis…` reads **800/797 minors, 275/276 factorizations** (the MKL values; the
arm asserts 861/858, 320/323). Both identical across the two runs. These arms collapse (§2.4).

## 2. Mechanisms (one per class; each reuses a mechanism the tree already has)

### 2.1 The reference build — `HVEN_TEST_ROUTE_REFERENCE` (class A, and B's pins)

One new test-support header, `tests/common_support/route_reference.h` (included as
`"../common_support/route_reference.h"`, the `console_capture.h` precedent):

```cpp
// The build every exact ROUTE pin was measured on (WX design §1.2): Linux, x86-64,
// MKL Pardiso at its AUTO dispatch, clang. A route (minors, factorizations, dep_*/
// certificate/saddle/latch counters, the status of a capped or knife-edge fixture) is
// pinned there and nowhere else; off it the cell asserts its portable contract and
// RECORDS the route. Not a backend switch: Windows runs MKL and is off-reference.
#if defined(__linux__) && defined(__x86_64__) && defined(__clang__) && \
    !defined(USE_ACCELERATE_SPARSE)
#define HVEN_TEST_ROUTE_REFERENCE 1
#else
#define HVEN_TEST_ROUTE_REFERENCE 0
#endif
namespace hven::test_support {
// RecordProperty("<key>_route", "UNOBSERVED-ROUTE off the reference build: <counters>")
void record_off_reference_route(const std::string &key, const std::string &observed);
}
```

The arm shape is the tree's existing one (`test_ipqp_acceptance.cpp:403–410`,
`test_ipqp_seams.cpp:467–474`): `#if HVEN_TEST_ROUTE_REFERENCE` → the pin **verbatim**;
`#else` → `record_off_reference_route(...)` + the portable contract; then, outside both, the
"STRUCTURAL, on every backend" assertions. Rules for implementers:

1. **The reference arm is the old assertion text, byte-for-byte.** Reviewers diff it. Nothing
   on the reference build is relaxed (C145: the Linux-MKL value stays exact).
2. **The portable contract** is what the fixture's *claim* needs on every build, and never a
   range that admits both routes:
   - status ∈ the fixture's honest set — a feasible fixture never reads kInfeasible; an
     infeasibility verdict carries its certificate; a capped fixture may read kMaxIter;
   - **if kOptimal, the full KKT battery** at the fixture's existing tolerances, and the
     objective against the **oracle** `f*` where the problem is convex (LP restoration dumps,
     convex QPs). For a **nonconvex** dump (YORKNET, BRITGAS elastic, STEENBRB qp2) a
     different local optimum is legitimate: drop the objective comparison, keep the battery;
   - finiteness (`x.allFinite()`, finite objective) where the fixture asserts it.
3. **Mechanism premises** (`EXPECT_GE(mech_counter, 1)` — "the rule under test fired on this
   fixture") are reference-only: the mechanism's coverage is the reference lane's, and a build
   whose route never reaches the rule has nothing to say about it.
4. **No Accelerate or Windows counter arm in this class**, even from a lane log (§1.1).
5. **Dispatch-only blocks are excluded**: CBWR (`MKL_CBWR` set) puts the box off-reference in
   fact while the macro still says 1. A one-test sentinel,
   `RouteReference.TheReferenceBuildRunsAtMklsAutoDispatch`, fails on a reference build that
   has `MKL_CBWR` set, naming the pins it invalidates — so a local CBWR run fails one
   readable test instead of ten opaque ones. It also `RecordProperty`s the MKL version.

### 2.2 Lane-observed counter arm — `#ifdef USE_ACCELERATE_SPARSE` (class A/K, non-route)

Only for a count that is not a knife-edge route and that **three** lane runs agree on (WX-R6),
written exactly as the `test_continuation.cpp:1219–1236` precedent: the Apple value with its
run ids, the MKL arm untouched, never a range. Today this qualifies:
`IpqpAcceptanceA1` `uncertain` 2 and 11 (runs 34911916381, 40861914857, 40868557422 — three,
across commits, identical) and `WarmStartBattery.Corpus` F3n50 cold / cold@pred
`border_refine` 57 (two runs; the first WX batch push supplies the third).

### 2.3 UNOBSERVED / honest-absence arm (class D)

The tree's existing shapes: `#ifndef USE_ACCELERATE_SPARSE` around an MKL-only fact
(`test_ipqp_trace.cpp:281–283`), and `GTEST_SKIP() << "… UNOBSERVED"` /
`RecordProperty(…, "UNOBSERVED …")` where nothing can be asserted. Where Accelerate's state is
a **documented contract** (`perturbed_pivots` is `std::nullopt` by construction,
`symmetric_factor_accelerate.cpp:273`), the Accelerate arm asserts that absence — asserting
the backend's own declared honest state is not inventing a value.

### 2.4 Collapse (an Accelerate arm the lane now shows equal to MKL)

Delete the `#ifdef USE_ACCELERATE_SPARSE` half, keep the MKL assertion unconditional, and
append a LATER DISPOSITION to the register entry that carried the arm (M3-1 for HsBattery,
M3-2 for Continuation), quoting both run ids. Append-only; earlier text is never edited.

### 2.5 Floats (class C) — never a per-backend float arm

Either the float leaves the off-reference arm (reported via `RecordProperty`, the M3-3
"reported, not asserted" shape), or it is replaced by a **structural** fact with a tolerance
derived from the problem — e.g. "the iterate lies on the face `x0 + x1 = 2`" at `1e-12` for an
O(1) linear face. No Apple float is committed (CLAUDE.md §6; `ci.md:498–509`).

### 2.6 The registers

Each batch appends its rows: `2026-08-14-accelerate-divergence-register.md` gets **M3-8**
(the lane's run-to-run nondeterminism, §1.1, with the 19-cell list and both run ids) and
**M3-9** (the route class moved to the reference build, citing this page); the Linux register
`2026-08-15-linux-runner-divergence-register.md` gets **W-1** under a new "Windows lane"
heading (§1.2's version/CBWR evidence). `docs/testing.md` "Per-backend arms, and the divergence
register" gains one paragraph on the reference build and the three-run counter bar.

## 3. The decision table (68 macOS + 3 Windows → 68 distinct tests)

Columns: class (A route pin, B status/stop-reason, C float, D known field/default, K = known-13
member), fix (§2.x), evidence the fix rests on. "mac R1/R2" = runs 40861914857 / 40868557422;
"diff" = the cell's output differs between them (§1.1); "CBWR" = moves on the box under a CBWR
branch; "win" = also fails on Windows.

### 3.1 `tests/qp/` hard-fixture dumps (`hven_qp_walk_seam_tests`) — 39 tests

| Test | Cls | Fix | Evidence |
|---|---|---|---|
| `QpE2Fix9StandstillRetreat.Acopp118Qp0Border`, `Qp1Border`, `Qp3Border` | A | 2.1: retreat count + `status==kOptimal` reference-only; portable: status ∈ {kOptimal, kMaxIter} (the dump's premise is in-cell kMaxIter), the existing kOptimal ⇒ battery + `f_reference` block unchanged | mac R2 status=1 retreats=0; Qp3 diff |
| `…Acopr118Qp3Border`, `Qp4Border`, `Qp5Border`, `Qp6Border` | A | same | mac: Qp3/5/6 kOptimal with retreats 0, Qp4 kMaxIter; Qp3–6 diff; Qp4 CBWR |
| `QpBpsBorderCertificate.Acopr118Qp26Border` | A | 2.1: `border_uncertified_fallbacks >= 1` reference-only | diff |
| `QpE2WorkingSet.ADeclinedRideIsExchangedOnSteenbrbRestorationQp4`, `Qp5`, `Qp6` | A | 2.1: off-arm minors and ride/exchange premises reference-only; portable: on-arm status + battery | Qp5 CBWR |
| `QpE2WorkingSet.TheDeclinedRideResolvesWithoutAnExchangeOnSteenbrbRestorationQp3` | A | same | CBWR |
| `QpE2WorkingSet.AFullyPinnedRowDecidesAnUndecidablePinExactly` | A | 2.1: `dep_repair_restarts`, `minor_iters == 2`, `dep_structural_first` reference-only | mac |
| `QpE2WorkingSet.TheUndecidableBorderPingPongRetreatsOnceAndEndsOptimal` | A | 2.1: `dep_undecidable`, `dep_border_retreats` reference-only; "ends optimal" stays portable if the lane shows it holds (it does — only 1712/1713 fail) | mac, CBWR |
| `QpE2WorkingSet.AMuFreeInertiaReReadCertifiesTheSingularPsdFaceOfAcopp57` | A | 2.1: `dep_inertia_reread_flips >= 1` reference-only | mac (WX-R2 mechanism) |
| `QpE2WorkingSet.AStructuralDependencyDecidesFirstOnTheSteenbrbElasticQp` | A | 2.1: `dep_structural_first >= 1` reference-only | mac, **win**, CBWR |
| `QpE4ProximalEqp.TheSteenbrbRestorationQp2ExitsFiniteAndSaysWhy` | **B** | 2.1: `kNumericalError` reference-only; portable: status ∈ {kOptimal, kNumericalError}, the finite/eq/refusal/minors asserts unchanged (they hold on both lanes) | §4.2 |
| `QpNc3SaddleExit.SteenbrbElasticQp2Border`, `…Refactorize` | A | 2.1: `saddle_exits >= 1` reference-only | mac: kOptimal, saddle_exits 0 |
| `QpNc3SaddleExit.AZeroCurvatureDeadEndIsNotRiddenSteenbrbRestorationLpQp3` | A | 2.1: `convex_certificates >= 1` reference-only | mac, CBWR |
| `QpNc4ConvexCertificate.BritgasRestorationLpQp4`, `SteenbrbRestorationLpQp3` | A | 2.1 via the file's own contract mode: off-reference pass `route_certificates = -1` (the "a kWrong read ⇒ the certificate" implication, `test_qp_convex_certificate.cpp:55–112`) | mac (diff), **win** (Qp4), CBWR (both) — **pilot cell for B0** |
| `QpRdvRankRepair.SawpathQp1Border`, `…Refactorize` | A (status-bearing) | 2.1: `dep_certificate_rejections == 0` and `status == kOptimal` reference-only; portable: status ≠ kInfeasible, kOptimal ⇒ battery | mac: kNumericalError, certrej 1, diff; WX-R2 |
| `QpSdlSaddleShift.YorknetRestorationQp29Border`, `Qp29Refactorize`, `Qp30Border`, `Qp30Refactorize` | A | 2.1: `saddle_shifted_directions`, `saddle_exits` premises reference-only | mac: kOptimal, shifts 0 |
| `QpStagnationStop.BritgasRestorationFootprintFace` | A | 2.1: stagnation premise and `minor_iters < 3000` reference-only; portable: status ∈ {kOptimal, kMaxIter}, finite | mac 11 700 minors to the cap, CBWR |
| `QpStagnationStop.BritgasRestorationUncertifiedStationaryPointBorder`, `…Refactorize` | A | 2.1: `stagnation_stops == 1`, `terminal_polish_solves == 0` reference-only | mac kOptimal 717/719 minors, diff |
| `QpW1AscentGuard.BritgasElasticQp1Border`, `…Refactorize` | A | 2.1: `ordinary_rose == 0` reference-only; record rose/worst_rise; portable: status + battery | mac rose 3 (Refactorize **passed R1**: §1.1's extra failure) |
| `QpYrkRefusedExchange.YorknetRestorationQp25Border`, `Qp25Refactorize`, `Qp26Border`, `Qp26Refactorize` | A | 2.1: refusal/miss/exchange premises and the `f_release` objective comparison reference-only (nonconvex: mac f = −5.7996 vs CBWR −1398.63, both kOptimal) | mac, Qp25 CBWR |
| `QpLatchReason.BritgasMainSubproblemHoldsItsConditioningLatch` | A | 2.1: the capped iterate's objective (0.24292 — a route float at a 120-minor cap, not an optimum) and the four latch counters reference-only | mac 0.21328 (diff in the 9th digit), CBWR |
| `QpCertificateSign.Net3QnSubproblemInconsistentFaceIsNotInfeasible` | **B** | 2.1: `kNumericalError` and `certificate_row_refusals == 1` reference-only; portable: the claim itself — `status != kInfeasible`, `terminal_farkas_certificates == 0` (already asserted per algebra, and holding on the lane) | §4.1 |

### 3.2 SQP driver fixtures (`hven_sqp_tests`, `hven_sqp_seam_tests`) — 19 tests

| Test | Cls | Fix | Evidence |
|---|---|---|---|
| `SqpDriverCertifiedFallback.T3TheTierSolvedRungReachesTheSameVerdictOnTheSameLinearization` | A | 2.1: analyses+verifies == majors (3 vs 4) reference-only; portable: the verdict equality the test is named for | mac |
| `SqpDriverCertifiedFallback.T7F1TheSameInconsistentLinearizationInAllThreeModes` | A | 2.1: per-major counts (3 vs 4) reference-only | mac |
| `SqpDriverCertifiedFallback.T7TheEscapeBranchTernaryCONSUMESAnAttachedReportAtTheDriver` | A, K | 2.1: `fallbacks.size() == 3` reference-only; portable: ≥ 1 entry and the last one exhausted | mac 2 vs 3, and known-13 |
| `SqpDriverRestorationEntries.ASecondRequestAfterProgressIsServicedOnlyAboveACapOfOne` | A (status-bearing) | 2.1: resumes 1 and `kNumericalError` reference-only; portable: the cap-of-one claim (entries ≤ cap) | mac: 2 resumes and **kOptimal** — the better outcome |
| `SqpDriverRestorationExcursion.TheSubStatusTellsACutOffSubSolveFromAConvergedOne`, `…DiscriminatesAtOneParentStatus` | A | 2.1: the fixture premise `restoration_entries == 1` gates the sub-status asserts → off-reference, skip the sub-status block with `record_off_reference_route` when the phase did not run | mac entries 0; CBWR (both) |
| `SqpDriverRestorationProximity.TheSeedGuardFiresAndTheStartObjectiveIsHPlusP` | A | 2.1: `seeded_rows == 0` reference-only (a margin comparison: "the candidate measures better again") | mac 1 |
| `SqpDriverRestorationSeed.TheExhaustedLadderSiteOffersItsOwnStepAndTheGuardRefusesIt`, `TheOffDiagonalStartOffersTheLadderStepAndTheGuardRefusesIt`, `TheOfferedCandidateNoLongerMeasuresBetterAndIsNotTaken`, `TheSeedOfferIsNotGatedOnTheQpModeAndIsRefusedInEveryOne` | A | same | mac 1 in each |
| `SqpDriverRestorationSeed.TheSeedsEvaluationIsCountedExactlyOnce` | A, K | 2.1: `evals_values == 36` reference-only; portable: the "exactly once" identity the name claims (evals with seed − evals without = 1), if the test can express it without the route; else record only | mac 17; known-13 read 24/28 at M6 |
| `SqpElasticWitnessStart.SawpathMajor0RungSolvesFromItsWitness`, `…LadderNeedsTheLastOptimalRung` | A (status-bearing) | 2.1: replay statuses and ladder counters reference-only; portable: no rung reports kInfeasible without its certificate | mac diff, CBWR (Rung…); WX-R2 |
| `SqpDriverRadius.TheElasticExhaustionPreemptsTheRadiusFloorOnThisModel` | **B** | 2.1: route (2) vs (3) asserts (last verdict, radius never shrank, tr_min inert) reference-only; portable: kInfeasible, certified, certificate h / stationarity / selector (all hold on the lane) | §4.4 |
| `SqpEqualityBasis.F9ZeroStepAtAnInconsistencyHandsOffToTheLadder` | **B** | **hold — WX-R3** (§4.5); interim: Accelerate arm records UNOBSERVED-ROUTE and asserts the hand-off fields that hold | §4.5 |
| `SqpEqualityBasisSeam.T6DisabledHandOffRunsF9ToMaxIter` | **B** | the control half follows F9; the negative control (seam on ⇒ kMaxIter at the cap) is portable and holds | §4.5 |
| `QpLiftOracle.LiftedSolutionMatchesTheDenseBOracleOnEveryEngineAndRadius` | **B** | 2.1 + symmetric with the existing rule (an SSN cell whose dense oracle did not converge is counted, not compared): off-reference, an SSN cell whose **lifted** arm did not converge is counted and recorded; reference asserts that count == 0 | §4.3 |
| `SqpDriverRestoration.AlgebraModeDivergenceIsANonUniqueSubproblemOptimum` | **C** | 2.5: **delete the D14 Accelerate float arm** (a per-backend float arm the licence forbids); off-reference assert the structural reading — `lambda_e(0)` is exactly 0 or the phase ran; each iterate lies on `x0 + x1 = 2` or on the diagonal mid-restoration, at 1e-12; part (b) (the perturbed-start fragility, portable today) unchanged | mac: both modes on the face at (1.875, 0.125), 2.3e-10 apart |
| `CrossoverAdapter.TwoLaidPartitionsSolveTheSameProblemAsOne` | **C**, K | 2.5: bitwise identity and iteration equality reference-only (MKL's claim); off-reference, both solves kOptimal and `|Δf| ≤ 10·max(kkt₁, kkt₂)·max(1,|f|)` — the bound the two KKT residuals actually license | mac: 21/26 then 26/24 iterations, Δf 1.5e-11 then 1.3e-12 — not deterministic even within a run |

### 3.3 Batteries, corpora, IPQP, threads — 10 tests

| Test | Cls | Fix | Evidence |
|---|---|---|---|
| `Continuation.ProbeBudgetBoundsAFailingProposal` | A→collapse | 2.4: drop the 304 arm; 18 unconditional; M3-2 LATER DISPOSITION | mac 18 both runs |
| `HsBattery.CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions` | A→collapse | 2.4: drop the 861/858 and 320/323 arms; M3-1 LATER DISPOSITION | mac 800/797, 275/276 both runs |
| `WarmStartBattery.Corpus` | A | 2.2: F3n50 cold / cold@pred `border_refine` 57 on Accelerate once a third run agrees | mac 57 both runs (counters identical; only `secs` differs) |
| `HsSweepCorpus.Corpus` | **C→B (HIGH)** | **hold — WX-R1, ruling R-2** (§4.6); the HS10 pins (26/40, 23/4, 3717) follow whatever R-2 rules | mac: HS10 warm at p = 3 kOptimal with complementarity 0.1336 |
| `IpqpAcceptanceA1.TheWindowAtF7sOwnJunctionsIsRecoveredExactly`, `…TwoBlocksOneAtEachJunctionAreRecoveredExactly` | K | 2.2: `uncertain` 2 and 11 on Accelerate (three runs) | known-13 + R1 + R2 |
| `IpqpSeamTest.WithNothingInjectedTheTierSolvesExactlyAsItDoesInProduction` | D, K | 2.3: Accelerate asserts `!perturbed_pivots.has_value()` (its declared state); MKL arm unchanged | `symmetric_factor_accelerate.cpp:273` |
| `IpqpTrace.IterEventsCarryTheDriverSetMajorAndCrossCheckAgainstTheResult` | D, K | 2.3: the same, in the `:281` arm that already gates `zero_derived` | same |
| `Threads.ZeroLeavesEveryFactorAtTheBackendDefault` | D, K | 2.3: the restoration leg's K0 read is UNOBSERVED on Accelerate (the count is stored and applied to nothing, `test_threads.cpp:58`; and this knife-edge model's route ends with no hot handle there, as it does on MKL Debug, `:668–669`): `RecordProperty` + skip that leg under `HVEN_USE_ACCELERATE_LAPACK`; `max_threads_now() == before` stays | mac: hot null, −1 |

## 4. Class B — each diagnosed before any pin moves

**4.1 `QpCertificateSign.Net3…` — fixture knife-edge on the walk's dead end; the claim holds;
an older weakness underneath.** On the lane the border algebra runs 4185 minors to kMaxIter
(row_res 0.108) where MKL dead-ends at once in kNumericalError; refactorize still reads
kNumericalError in 19. The test's subject — *an inconsistent face is not declared infeasible
without a verified Farkas certificate* — holds on both (`!= kInfeasible`, zero Farkas
certificates, asserted per algebra, passing). Not CBWR-sensitive on MKL, so the difference is
the WX-R2 semantics (Accelerate cannot mark the rank-deficient working set perturbed), not an
ulp. The weakness is pre-existing and backend-independent: NET3 qp98 is **feasible** (HiGHS
phase 1 at 0, Clarabel f* = −683.26) and the walk solves it on neither backend. **Register
WX-R4 (low).** Pin moves per §3.1.

**4.2 `QpE4ProximalEqp.TheSteenbrbRestorationQp2…` — fixture knife-edge; the reference build
is the outlier.** kOptimal on Windows, on macOS, and on the box under every CBWR branch; only
Linux-MKL-AUTO reads the declared kNumericalError. The fixture's own comment places it at the
polish step cap's edge (a cap of 1e-5 converts it). The honest-exit asserts (finite, rows met
to 1e-12, `eqp_residual_refusals >= 1`, < 1000 minors) hold everywhere. **Not a weakness**;
nothing to register beyond M3-9.

**4.3 `QpLiftOracle…` (SSN seed 6, r = 0.3, x0) — backend semantics; worth registering.**
The lifted arm reads kMaxIter where the dense oracle converges, and `clean_inertia` is false on
a lifted system that is **positive definite by construction** (the test's own section 4b/4c
note). On MKL the cell is stable under every CBWR branch. So Accelerate's inertia read of a PD
lifted KKT is not kOk — the same evidence gap as WX-R2, on the quasi-Newton lifting the
first-order mode will lean on. **Register WX-R5 (medium; target the first-order mode's
window).** The fix in §3.2 is symmetric with the rule the test already applies to the dense
arm; it is not a widening on the reference build.

**4.4 `SqpDriverRadius.TheElasticExhaustion…` — not a status flip: same verdict, different
trigger.** On the lane both floors are reached (route (3)) instead of the elastic tier's
exhaustion (route (2)); the solve still ends kInfeasible, certified, at the same certificate
(`h = 2 − √2`, stationarity, selector — all passing). The test pins *which trigger fired*,
which is a route. Origin D14 (quoted at `test_sqp_restoration.cpp:1361–1381`) already observed
that on Accelerate this model's first elastic subproblem exits kMaxIter with a zero step, which
is what keeps route (2) from firing. Same root as 4.5.

**4.5 `SqpEqualityBasis.F9…` and `SqpEqualityBasisSeam.T6…` — a real robustness gap
(register, and hold the pin).** A two-variable QP-as-NLP with an inconsistent duplicate pair
(`x1 = 1`, `x1 = 1.001`) runs to `max_iter = 100` in **all three QP modes** on Accelerate
instead of finishing kInfeasible; the equality basis hands off correctly (those asserts pass);
what fails is the ladder after the hand-off. Not CBWR-sensitive on MKL. Same family as 4.4 and
D14: an exactly dependent row pair is decided by Pardiso's pivot perturbation on MKL and by
nothing equivalent on Accelerate. A solver that cannot call the smallest possible inconsistent
system infeasible on a supported backend has a gap, not a knife-edge — and unlike 4.4 there is
no backstop: the radius does not shrink on a zero step. **Register WX-R3 (HIGH).**
Recommendation (ruling R-3): do not move the pin to a portable "kMaxIter is acceptable"
reading; give the Accelerate arm a `RecordProperty` UNOBSERVED-ROUTE and assert only the
hand-off fields until WX-R3 lands, so the lane goes green without the test ever blessing
kMaxIter.

**4.6 `HsSweepCorpus.Corpus` (triaged C) — not a tolerance: a false kOptimal.** At HS10,
p = 3, the warm arm returns **kOptimal** at a point whose own KKT check (the test's
`nlp_kkt_check.h`) passes stationarity (< 1e-5), primal (< 1e-5) and dual sign, and fails
complementarity at **0.1336**; the warm/cold objective gap is 0.0646. HS10's objective is
linear, so a stationary point with a strictly slack constraint and a nonzero price is not an
optimum — it is exactly the B-1 defect signature this section was written to catch
("pre-repair this read 1.50 (HS10)"). Mechanism, from the source: NLP complementarity is
**measured but not gated** at convergence (header archive
`docs/notes/2026-09-header-prose-archive.md:268–280`), on the argument that
`|λ_i cI| = O(‖λ‖‖p‖)` vanishes with the step; the gate is armed only while duals are
ingested or carried (`src/drivers/sqp_solver.cpp:3799–3803`, `:4562–4573`). The lane shows a
trajectory on which that argument fails and nothing catches it. Identical on both runs. Not
reproducible on MKL (passes under every CBWR branch), so it cannot be diagnosed further from
this box without Apple observation. **Register WX-R1 (HIGH): gate NLP complementarity at
convergence (or prove the route that bypasses the step argument), and treat HS10 p = 3 on the
lane as its fixture.** The test must not be relaxed: ruling R-2.

## 5. The batch plan (Sonnet implementers; one batch = one commit range, one push, one lane run)

Every batch: Debug + Release on the box for the touched binaries; the touched tests run alone
at `MKL_NUM_THREADS=1`, then `ctest -j4` (setsid) for the full suite; **the reference arm's
text diffed against `aa19f364`** (reviewer check: zero changed lines inside
`#if HVEN_TEST_ROUTE_REFERENCE`); push to `wx`; read all three lanes
(`gh run view --job … --log-failed`); the batch is closed when its own cells pass on Linux,
Windows and macOS. A cell that passes once on macOS is closed only if it carries no
Accelerate counter arm; an arm needs its third agreeing run (WX-R6). Each batch appends its
register rows (§2.6) and its `RecordProperty` keys are checked in the macOS gtest-XML artifact.

| Batch | Scope | Cells | Risk |
|---|---|---|---|
| **B0** | `route_reference.h` + the sentinel test + `docs/testing.md` paragraph + register M3-8/M3-9/W-1 skeletons; **pilot**: `QpNc4ConvexCertificate` ×2 (closes 1 of the 3 Windows cells) | 2 (+1 new) | low; establishes the pattern every later batch copies |
| **B1** | Class D + collapse + lane counter arms: `IpqpSeamTest`, `IpqpTrace`, `Threads`, `Continuation`, `HsBattery`, `IpqpAcceptanceA1` ×2, `WarmStartBattery` | 8 | low |
| **B2** | `tests/qp` mechanical: standstill ×7, BPS ×1, NC3 ×3, SDL ×4, YRK ×4 | 19 | low–medium (portable contract per fixture) |
| **B3** | `tests/qp` remainder: `QpE2WorkingSet` ×8, `QpE4ProximalEqp` (B), stagnation ×3, ascent guard ×2, latch ×1, RDV ×2, `QpCertificateSign.Net3` (B) — closes the last 2 Windows cells | 18 | medium (two class-B cells) |
| **B4** | SQP driver routes: certified fallback ×3, restoration entries/excursion/proximity/seed ×9, elastic witness ×2 | 14 | medium (premise-gated blocks) |
| **B5** | SQP class B/C: `SqpDriverRadius`, `QpLiftOracle`, `AlgebraModeDivergence`, `CrossoverAdapter`, and F9/T6 per ruling R-3 | 6 | medium |
| **B6** | `HsSweepCorpus.Corpus` per ruling R-2 | 1 | depends on R-2 |
| **close** | two consecutive all-green lane runs on the same head (the second is the flake check §1.1 demands); `.superpowers/m7-macos-ci-failures.md` closing note (settler's file) | — | — |

B2–B4 are independent of each other once B0 lands and may run in sequence on one branch with a
push each; they must not share a build tree concurrently (CLAUDE.md §8).

## 6. Known-13 dispositions (`.superpowers/m7-macos-ci-failures.md`)

| # | Known-13 test | Today | Disposition |
|---|---|---|---|
| 1 | `IpqpSeamTest.WithNothingInjected…` | fails | B1, §2.3 |
| 2 | `IpmStopReason.TheProximalArmLeavesRestoration…` | **passes** R1 + R2 | closed by lane observation; no change |
| 3 | `Threads.ZeroLeavesEveryFactorAtTheBackendDefault` | fails | B1, §2.3 |
| 4 | `CrossoverAdapter.TwoLaidPartitions…` | fails (nondeterministic) | B5, §2.5 |
| 5 | `IpqpAcceptanceA1.TheWindowAtF7sOwnJunctions…` | fails, 2 (3 runs) | B1, §2.2 |
| 6 | `IpqpAcceptanceA1.TwoBlocksOneAtEachJunction…` | fails, 11 (3 runs) | B1, §2.2 |
| 7 | `IpqpTrace.IterEventsCarry…` | fails | B1, §2.3 |
| 8 | `SqpDriverCertifiedFallback.T7TheEscapeBranchTernary…` | fails, 2 vs 3 | B4, §2.1 |
| 9 | `SqpDriverCertifiedFallback.T7TheVerdictRefineCost…` | **passes** | closed by lane observation |
| 10 | `SqpDriverCertifiedFallback.T7TheAttachedReportsRestorationOffer…` | **passes** | closed by lane observation |
| 11 | `SqpDriverRestorationSeed.TheExhaustedLadderSiteSeedsFromItsOwnStep` | renamed (R2 re-premise, `test_sqp_restoration.cpp:2260`) → `…OffersItsOwnStepAndTheGuardRefusesIt`, which fails | B4, §2.1 |
| 12 | `SqpDriverRestorationSeed.AJacobianPoisonedCandidateIsRefusedNotTaken` | **passes** | closed by lane observation |
| 13 | `SqpDriverRestorationSeed.TheSeedsEvaluationIsCountedExactlyOnce` | fails (17 vs 36) | B4, §2.1 |
| + | `HsSweepCorpus.Corpus` (the 2026-09-18 UNOBSERVED addendum: "any value other than 3735 is a finding") | 3332, and the HS10 false optimal | **a finding**, as the addendum said it would be: §4.6, B6 per R-2 |

## 7. Register (WX-R…)

| Id | Item | Priority | Where it lives |
|---|---|---|---|
| **WX-R1** | HS10 p = 3 warm arm: kOptimal at complementarity 0.134 on the macOS lane — NLP complementarity is not gated at convergence | **HIGH** (correctness) | solver; candidate for the window that owns the convergence test |
| **WX-R2** | Accelerate's absent perturbed-pivot count makes `inertia_verdict` and the dependency test strictly more trusting than on MKL (§1.3); explains most of the lane's walk divergences | medium | design note; candidate remedy families: an Accelerate-side perturbation proxy (pivot magnitude after LDLᵀ-TPP), or kSuspect on absent evidence near singularity |
| **WX-R3** | F9 (and D14 / §4.4): an exactly dependent inconsistent pair is not called infeasible on Accelerate — kMaxIter at 100 in all three QP modes | **HIGH** (robustness) | solver |
| **WX-R4** | NET3 QN qp98: a feasible QP the walk solves on neither backend (MKL kNumericalError; Accelerate burns 4185 minors to the cap) | low | walk backlog |
| **WX-R5** | Accelerate reads a PD-by-construction lifted quasi-Newton KKT as not kOk (`QpLiftOracle` SSN seed 6) | medium | first-order / QN window |
| **WX-R6** | The macOS lane is not run-to-run deterministic (19 of 68 failing cells differ between two runs of codegen-identical code): the counter-arm bar becomes three agreeing runs, and the lane's Accelerate threading pin (`ci.yml:406`'s open question) should be decided as a re-observation event | medium | CI / register M3-8 |
| **WX-R7** | The Linux lane installs whatever MKL apt calls latest; the reference build should name its MKL. Pin `intel-oneapi-mkl-devel-2026.2` in `ci.yml` (2026.1 and 2026.2 are bit-identical on all 68 cells, so the pin costs nothing today) | low | CI |
| **WX-R8** | Route-pinned fixtures whose route is CBWR-sensitive on MKL itself (the 10/16 here) are knife-edge on the reference lane too; the L-1 Linux-runner flake class is the same phenomenon. Candidate: re-derive route pins under `MKL_CBWR=AVX2` as one declared event, which would make them CPU-independent across x86 runners (it would not bring Windows in) | low | declared-event candidate |

## 8. Rulings needed

- **R-1 — the reference build.** Adopt §2.1: exact route pins are owned by the Linux-x86-64-MKL
  clang build; Windows and Accelerate assert the portable contract and record the route, with
  no counter arm from either lane on these fixtures. (C145(A) asked whether "per-backend" should
  be "the Linux MKL build the pin was measured on": the evidence says yes.)
- **R-2 — HsSweepCorpus / WX-R1.** Recommend: WX-R1 gets a diagnosis task before B6 (it is a
  correctness defect on a supported backend, and B-1's test exists precisely to fail on it).
  If the lane must be green first, the alternative is a B-1-style **defect pin** on Accelerate
  only — assert the defect exists (HS10 p = 3 complementarity > 1e-6, structural, no float
  pinned) with a register row, so the repair trips it — never a relaxed tolerance.
- **R-3 — F9/T6 / WX-R3.** Recommend the interim in §4.5 (record, assert the hand-off, never
  bless kMaxIter) and WX-R3 as a solver task.
- **R-4 — WX-R6's lane threading pin and WX-R7's MKL pin.** Both change what a lane observes;
  recommend deciding them before the close's two-run check, so the check measures the lane
  that will be kept.
