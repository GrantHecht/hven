# M8 DN-3 -- the flip and its I set (DECLARATION-r4)

Successor of `DECLARATION-r3.md` (commit `c438d56e1`), which stays frozen beside this file with every r3 output and
`RESULTS.txt`. Committed before r4's first leg; never edited afterwards. Criteria are never changed after data. A failed
or void leg is answered by `DECLARATION-r5.md`, with this file's record kept.

## 0. Why r4

r3's verdict (`RESULTS.txt`): A5 adopts for both engines (IPM HS 22/0/0, SD 64/11/22; SQP HS 21/0/1, SD 81/0/16), but
STOP S-A4 fired, so no default flipped and the settler ruled. **Owner ruling 2026-11-13**: the tallies are applied and
the S-A4 records are accepted as immaterial, so `IpmLinearOptions::dense_factorization` and
`SqpLinearOptions::dense_factorization` default to `kAuto`; no public `max_dense_dimension` (not now). r1 §7 (the flip
clause) and r0 §2 / §4 L4 (the I set, I1-I4) then require the flip commit and its identity checks in a successor
declaration committed before its first leg, on the flip commit. This is that declaration. L5 (the macOS and Windows CI
lanes on the flip commit) needs a push this round does not make: it is the settler's, as r1 §7 says.

## 1. The code and the binaries

- **Flip commit `2d6d89cd076c64e6a7fa859040fe0b3269a2adf3`** (branch `m8-dn3-flip`, one commit on `origin/m8`
  `b09f1bbc0`). `src/`, `include/`, `tests/` are byte-identical between DN-3's measured code head `96ac1a27d` and
  `b09f1bbc0` (`git diff --stat 96ac1a27d b09f1bbc0 -- src include tests` is empty; the only build-side difference is
  `bench/CMakeLists.txt`'s stamp guard, M7-16), so DN-3's rows are this branch's pre-flip rows. The flip commit changes
  the two defaults and their doc comments, `conditional_reproducibility`'s doc comment, the design note (section "DN-3
  flip"), and 13 test files (section 3). No library source other than the two option headers moves.
- Trees: `build-flip/rel` (Release) and `build-flip/dbg` (Debug), clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST`,
  `HVEN_BUILD_BENCH=ON HVEN_BUILD_EXAMPLES=ON` (`r4/legs/build.sh`), brought to the flip commit by `r4/legs/prebuild.sh`
  (bench stamp `check_bench_stamp: OK (2d6d89cd076c)`).
- Corpus binary (I1, I2, I3c), frozen: `build-flip/flipbin/hven_sqp_corpus`, sha256
  `7e41142fc0aff007b301a3af905ecacd2ccc86941dcfe212896627e0442eff4e`.
- Lane binary (I4), frozen: `build-flip/flipbin/hven_bench_cell`, sha256
  `7ee7b5e1cd8567f19ff0048e4e6ff523b5beb03d16a13dfbe5de891eaf27d6ab`, built by `r4/sl/flip_setup.sh` in the LOCAL
  hven-bench clone `/home/ghecht/Projects/hven-bench-dn1` (never pushed; the main harness is not touched): branch
  `dn3-flip-legs` (commit `adcb7d2`) = DN-3's lane commit `dn3-legs` `8722b90` (patches g, h, i) + pin -> the flip
  commit + `r4/sl/hb_patch_j.py` (one label prefix, `never-<base>`, which sets `kNever` explicitly). Its pin install
  `.hven/install-2d6d89cd076c` (`HVEN_PIN_SHA 2d6d89cd076c64e6a7fa859040fe0b3269a2adf3`), libhven.a sha256
  `788302b49db4bd96cd1ddd7330b3afa0c99f3d165d3bbb942a08f832ae6e4eea`. The clone's HEAD was put back on `dn1b-legs`.

Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, Release unless stated, `MKL_NUM_THREADS=1`. Apple/
Accelerate and Windows: UNOBSERVED (nothing pushed).

## 2. Disclosures: what ran before this file (none is a leg)

- `dep/eigen` and `dep/fmt` in this worktree are copies of the main checkout's submodule working trees, at the
  commits the superproject pins (`bc3b39870`, `407c905e4`, each read from the main checkout's `.git/modules/*/HEAD`):
  the sandbox refused `git submodule update` over the `file` transport. Their content is the pinned content.
- Dev builds and dev suites of the flip tree before the flip commit (scratch logs `build-flip/dev-ctest-*.log`):
  - d1 (defaults flipped, no test edited), Release: 30 failed of 3633. 29 are the flip's movers (section 3); one,
    `BenchStampGuard.SelfTest` (added after DN-3's head by M7-16), is environmental: its "no git" case makes a
    directory under `TMPDIR`, and `TMPDIR` inside this worktree resolved to the worktree's repository. `r4/legs/ctest.sh`
    sets `GIT_CEILING_DIRECTORIES=$TMPDIR` for that reason (the test passes from d3 on).
  - d2 (first edits), Release: `HsSweepCorpus.Corpus`, `HsSweepCorpus.Hs15...` failed, `WarmStartBattery.Corpus`
    PASSED; Debug: `HsSweepCorpus.Corpus`, `WarmStartBattery.Corpus`, and two Debug-only movers
    (`SqpDriverAdaptiveMu.MuScheduleIsQuantizedAndMonotone`, `SqpDriverRestorationProximity.TheProxyWouldCertify...`).
  - A repeat probe (`build-flip/devrepeat.sh`, stand-alone, serial): `WarmStartBattery.Corpus` five times per config
    and the HS sweep tests three times per config. Release read F3n50 warm / hot `border_refine` 34 in all five
    (pinned 35; d1's ctest read 34, d2's ctest read 35); Debug 59 / 36 / 17 / 36 / 59 in all five (pinned
    57 / 35 / 16 / 35 / 57). The HS sweep readings were identical in every repeat and both ctest runs.
  - d3 (the flip commit's tree, byte-identical): **Release and Debug "100% tests passed, 0 tests failed out of 3633"**.
- One-cell instrument probe (`build-flip/devprobe.sh`) on ONE NON-candidate cell, `hs-hven-sqp-hs025` (DN-3's E3
  proved its two arms byte-identical, so it carries no flip information): `default` and `never-default` against DN-3's
  A and N rows: .out (less `options_label`, `hven_pin_sha`) and .optdump equal on both; the dumps read
  `linear.dense_factorization = kAuto` and `= kNever`. No other lane cell, no replay, census, interior or iparm run
  has been taken on the flip binaries.
- `r4/sl/flip_identity.py` was self-tested against DN-3's own rows copied into a scratch tree (D <- A, K <- N): 238/238
  both, then with one integer, one residual and one exit code perturbed: each caught (the residual as in-gate).

## 3. The re-pins (31 test cases, 13 files; each declared in its test)

Re-derived to the kAuto reading (corpus pins of the shipped default; counters):

| test | was | now |
|---|---|---|
| `OptionsDump.EnumsPrintEnumeratorNames` | `linear.dense_factorization = kNever` | `= kAuto` |
| `DenseArmEngines.IpmValidateAcceptsEveryPolicyAndNoThreshold` | CNR at the default accepted | CNR at the default refused; CNR + kNever accepted |
| `HsSweepCorpus.Corpus` HS10 warm | 26 / 40 (both configs) | Release 28 / 42, Debug 26 / 40 (config-split) |
| `HsSweepCorpus.Corpus` HS26 warm | Release 12 / 29, Debug 13 / 30 | Release 13 / 30, Debug 12 / 29 |
| `HsSweepCorpus.Corpus` total minors | Release 3340, Debug 3342 | Release 3347, Debug 3338 |
| `HsSweepCorpus.Hs15WarmKdOffWithTheFailedSubproblemStep` | Release 33 / 95 / 38 / 6 | Release 34 / 99 / 39 / 7 (Debug unchanged) |
| `WarmStartBattery.Corpus` F3n50 `border_refine` | pinned per arm | recorded, not pinned, on every F3n50 arm and platform (not stable across processes on the dense arm, section 2); every other column asserted |

`kNever` explicitly -- the platform backend on purpose (HS071 and these models are below both thresholds, so at kAuto
the platform session is never reached):

| test | kAuto reading (dev d1) |
|---|---|
| `BackendErrorSeam.AFailedInitialAnalysisIsAStatusOnTheInteriorPointEngine` | optimal, the injector never reached |
| `BackendErrorSeam.AFailedInitialFactorizationIsAStatusOnTheInteriorPointEngine` | optimal, the injector never reached |
| `BackendErrorSeam.ABackendErrorSurfacesThroughTheSqpHandleIntoLastResult` | no BackendError |
| `IpmTrace.ThePerturbedPivotCountIsTheBackendsOwnAndNeverAFabricatedZero` | `p_pivots` null (the dense arm has no such count) |
| `Options.IpmEveryBackendFieldIsAcceptedAndTakesEffectAtTheNextSolve` | `cnr_mode` refused (CNR requires kNever) |
| `SqpDriverQpFailure.SuspectEscalationsAggregateToTheDriver` | the MKL control spends 10 rungs, as Accelerate's does |

`kNever` explicitly -- scenario fixtures whose routes were derived on the sparse arm:

| test | kAuto reading (dev d1 Release unless stated) |
|---|---|
| `SqpEqualityBasisSeam.T6DisabledHandOffRunsF9ToMaxIter` | all three modes kIterationLimit at 100 (want kInfeasible) |
| `SqpEqualityBasis.F9ZeroStepAtAnInconsistencyHandsOffToTheLadder` | the same |
| `SqpDriverElastic.ElasticSurvivesALargeConstraintScale` | both algebras kNumericalError; first QP kNumericalError, no elastic activation, x = (0, 0.5) |
| `SqpDriverCertifiedFallback.T7TheVerdictRefineCostAtScaleIsFIFTEENStepsOnItsWorstFixture` | verdict_refine_steps 1 (want 0; the AVX-512 runner's off-reference reading) |
| `SqpDriverCertifiedFallback.T7F1TheSameInconsistentLinearizationInAllThreeModes` | kSsn escapes 1 of 5 majors, kIpm elastic-from-escape 3 of 5 |
| `SqpDriverCertifiedFallback.T3TheTierSolvedRungReachesTheSameVerdictOnTheSameLinearization` | IPQP analyze + verify 3 of 5 majors |
| `SqpDriverAdaptiveMu.MuScheduleIsQuantizedAndMonotone` (block 2) | Debug: a decade drop (premise moved) |
| `SqpDriverRestoration.AlgebraModeDivergenceIsANonUniqueSubproblemOptimum` | the two algebras no longer diverge (6e-9 apart) |
| `SqpDriverRestorationProximity.TheSeedGuardFiresAndTheStartObjectiveIsHPlusP` | one seeded row (want 0) |
| `SqpDriverRestorationProximity.TheProxyWouldCertifyWhereTheExactResidualRefuses` | Debug: premise 1 moved (proxy 4.56e-7 > 4.475e-7) |
| `SqpDriverRadius.TheElasticExhaustionPreemptsTheRadiusFloorOnThisModel` | the radius reaches the floor; verdict not kAcceptFeasibilityStep |
| `SqpDriverRestorationSeed.TheOfferedCandidateNoLongerMeasuresBetterAndIsNotTaken` | one seeded row |
| `SqpDriverRestorationSeed.TheSeedOfferIsNotGatedOnTheQpModeAndIsRefusedInEveryOne` | one seeded row |
| `SqpDriverRestorationSeed.TheExhaustedLadderSiteOffersItsOwnStepAndTheGuardRefusesIt` | one seeded row |
| `SqpDriverRestorationSeed.AJacobianPoisonedCandidateIsRefusedNotTaken` | the Jacobian screen reached at k = 16 |
| `SqpDriverRestorationSeed.TheOffDiagonalStartOffersTheLadderStepAndTheGuardRefusesIt` | one seeded row |
| `SqpDriverRestorationSeed.TheSeedsEvaluationIsCountedExactlyOnce` | evals_values 37 (want 36) |
| `SqpDriverRestorationExcursion.TheSubStatusTellsACutOffSubSolveFromAConvergedOne` | no restoration entry |
| `SqpDriverRestorationExcursion.TheSubStatusDiscriminatesAtOneParentStatus` | no restoration entry |
| `SqpDriverRestorationEntries.ASecondRequestAfterProgressIsServicedOnlyAboveACapOfOne` | two resumes, kOptimal (want one, kNumericalError) |

## 4. The legs and their criteria (fixed here, before data)

| leg | instrument (under `r4/`) | criterion |
|---|---|---|
| F1 | `legs/ctest.sh` rel, dbg -> `suites/` | each log reads "100% tests passed, 0 tests failed out of 3633" |
| I1 | `legs/replay.sh` -> `replay/` | the 27-cell replay x walk / ssn / ipm against `docs/notes/data/2026-10-m7-wx2-gates/gates/replay/head-<mode>.csv` (r2's P2 reference), `scripts/compare_replay.py` byte-exact (no `--residual-gate`), 0 differences in all 9 comparisons (x3) |
| I3a | `legs/iparm.sh` -> `iparm/iparm-never.txt` | byte-identical to `docs/notes/data/2026-11-m8-ap2-b8-riders/iparm/iparm-b8r.txt` (the B8-riders dumper with `kNever` set in every configuration, `r4/iparm-never/`) |
| I3b | the same leg -> `iparm/iparm-default.txt` | REPORTED, not gating. Expectation: at the flipped default (HS071 below T_IPM) 0 `pardiso()` calls in every configuration; `parallel_solve=2` REFUSED as in the pin; `cnr=on` and `cnr=on+threads=2` REFUSED (CNR requires kNever) |
| I3c | `legs/interior.sh` -> `interior/` | "unmoved": the flip's 27-cell interior run vs r2's `interior-r2/interior-B.csv` (the head 96ac1a27d), 0 differences; "promoted": the baseline invocation vs `bench/baselines/2026-11-m8-b7-ipm-leg/interior_baseline.csv`, 0 differences (r2's P3, byte-exact) |
| F3 | `legs/install_smoke.sh` -> `install-smoke/` | install smoke and export contract rc 0 on the default (GCC) and clang++ front ends |
| I4 | `sl/flip_enum.sh` + `sl/flip_identity.py` -> `identity/` | `I4_PASS`: on all 238 TRAIN cells (DN-3's `roster.tsv`, cmp-equal to DN-0's), I4a the flip's `default` row equals DN-3's `auto-default` row (`enum/cells/*.A.*`) and I4b the flip's `never-default` row equals DN-3's `default` row (`enum/cells/*.N.*`): every key of the .out except `options_label` and `hven_pin_sha`, plus the lane exit code; integer and string values byte-equal; residual-class values byte-equal or within hven-bench `docs/corun.md`'s calibrated gate (rel 1e-12, abs 1e-13; every in-gate non-byte-equal value is listed); the .optdump byte-equal; no capped or missing row |
| I2 | `legs/census.sh` -> `census/` | `CENSUS_AS_DECLARED`: DN-1d r3's `census_check.py` against `bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv` with PF-1's two declared movers, every non-deadline-bound row exact (r0 I2) |

Protocols. Every leg: `pgrep -a hven_bench_cell` empty first (PGREP_EMPTY), under `flock /tmp/box-build.lock`,
`set -o pipefail`, FOREGROUND_START/END markers, `MKL_NUM_THREADS=1`, `TMPDIR=build-flip/tmp`, binary sha256 checked
before the first cell. F1: `ctest -j4` under `setsid`. I1, I3, I4: serial, one process at a time (I4: 476 cell
processes through DN-3's unchanged `sl/cell.sh`, THREADS=1, core 2, CAP 300 s; co-run is not used: serial costs a few
minutes). I2: the census runner's own declared tiering (parallel-tiered v1 at its defaults: T1 6-wide pinned one
process per physical core, T2 solo, T3 5-wide full budget; DN-1d's protocol), counters asserted, wall-clock
informational only. No leg asserts wall-clock. The orchestrator checks that `src include tests bench examples
CMakeLists.txt cmake scripts` equal the flip commit and are clean before the first leg.

## 5. STOP and ON FAIL

The orchestrator stops at the first failed leg: stop, commit the evidence, report; no re-run, no criterion change
after data; `DECLARATION-r5.md` after a fix, with this record kept. Any I1, I2, I3a, I3c or I4 difference is r0's
S-I: the flip does not land as declared; settler, with the cell, the factor and its n. A deviation is not re-cut
into a pass.

## 6. NOT RUN

HOLDOUT (never). L5, the macOS and Windows CI lanes on the flip commit (no push this round; the settler's; every
Accelerate and Windows reading of the re-pins and of kAuto is UNOBSERVED). The runtime gate and every instruction or
wall-clock measurement (the flip claims identity, not speed; DN-3's verdict is the speed evidence). The compile-flags
diff and the objects diff (one uniform flag regime; the flip moves two default member initializers). The falsifier
(DN-3 r2's P4 covers the kAuto mechanism; the flip adds no mechanism).

## 7. Registered with the flip (not fixed here)

- **T_IPM = 167 is a tuning candidate**: the 11 IPM small-dense losses (solver A/N 1.13-2.83: BT8, DEMBO7, DNIEPER,
  HS101, HS84, HS85, HS99EXP, LAUNCH, NET1, OPTPRLOC, SPANHYD) are mid-size KKT systems where the dense arm costs more
  than Pardiso's reused analysis.
- The kAuto readings of section 3's scenario fixtures, three of them worse outcomes than the sparse arm's on
  constructed SQP models: the F9 inconsistent duplicate pair ends kIterationLimit instead of kInfeasible in all three
  QP modes (two tests); `ElasticSurvivesALargeConstraintScale` (row scale 1e8) ends kNumericalError with the first
  subproblem refused and no elastic activation. Restoration, seed and excursion fixtures mostly no longer reach their
  scenario under kAuto, so their coverage stays on the sparse arm; equivalent kAuto fixtures are not written.
- F3n50's `border_refine` is not stable across processes of one binary on the dense arm (section 2): a counter MKL's
  dense kernels make address-sensitive, which CLAUDE.md section 7 assumes of residuals only.
- `conditional_reproducibility = true` is refused at the new default unless `kNever` is set too.
- The hven-bench lanes read the library default (no lane assigns `dense_factorization`): at their next pin every hven
  label's options dump, and so its `options_digest`, moves, and every cell with a pattern at or below T moves.

## 8. Order

```
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c/docs/notes/data/2026-11-m8-dn3/r4/legs/orchestrate-r4.sh 2d6d89cd076c64e6a7fa859040fe0b3269a2adf3 7e41142fc0aff007b301a3af905ecacd2ccc86941dcfe212896627e0442eff4e 7ee7b5e1cd8567f19ff0048e4e6ff523b5beb03d16a13dfbe5de891eaf27d6ab > /home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c/docs/notes/data/2026-11-m8-dn3/r4/orchestrate.out 2>&1
```

Legs in that order: F1 (rel, dbg), I1, I3a/I3b, I3c, F3, I4, I2. ON FAIL: section 5.
