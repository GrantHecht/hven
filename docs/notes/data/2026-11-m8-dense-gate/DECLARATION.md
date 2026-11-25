# M8 dense gate -- DECLARATION (a declared event changing dense-arm flag/refuse behaviour)

Committed before the first leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is
answered by `DECLARATION-r1.md` (then -r2, ...), with this file and every output kept.

## 0. The change

Code head **`53fc92c93433b1652e94751e0318b83a597bd0e9`** (branch `m8-dense-gate`, one commit on `origin/m8`
`030bae675`, the DN-3 r5 record). Design and argument: `docs/notes/2026-11-m8-dense-gate-design.md`.

1. The kReport detector re-judges an unshifted factorization whose raw dsycon_rook estimate is below 2^-53 on its
   Ruiz-equilibrated matrix (Higham's 1-norm estimator through dsytrs_rook on the factors in hand; numerics
   untouched); flagged iff that rcond is below 2^-53 too.
2. Every dense solve gate (shifted, bordered shifted-K0, unshifted SSN, IPQP step) reads the normwise backward error
   eta = ||r|| / (||K|| ||y|| + ||b||) at 1e-8 instead of ||r|| / ||b|| (the IPQP's refusal at 1e-5 becomes 1e-8).

`src/` / `include/` move only on the dense arm (kReport detector, gates reached only through dense numerics). kNever
paths are untouched by construction: the detector runs under kReport only, which is refused under kNever; every gate
is conditioned on dense numerics (`dense_shifted`, `FactorArm::kDense`).

Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, `-DHVEN_FP_MODE=SAFER_FAST`, `MKL_NUM_THREADS=1`.
Apple/Accelerate and Windows: UNOBSERVED (nothing pushed; the Accelerate adapter's two-line counter change and the
estimator's dsytrs_rook call through `lapacke_shim.h` are compiled by no lane here).

## 1. Binaries (frozen by `legs/prebuild.sh`, `dev/prebuild.out`)

- Trees `build-dg/rel` (Release), `build-dg/dbg` (Debug), `HVEN_BUILD_BENCH=ON HVEN_BUILD_EXAMPLES=ON`; bench stamp
  `check_bench_stamp: OK (53fc92c93433)`.
- Corpus binary `build-dg/dgbin/hven_sqp_corpus` sha256
  `927cfc3d8c7af0660719179b9db1e5aaf8d5fe3aa3cd695f0a4f2276b4c14a7b`.
- Lane binary `build-dg/dgbin/hven_bench_cell` sha256
  `e17dc5f396b71d0032fabde833730843d2fcb2ca7655b1a814cfd40c18ed032e`, built by `sl/lane_setup.sh` in the LOCAL
  hven-bench clone `/home/ghecht/Projects/hven-bench-dn1` (never pushed): branch `dense-gate-legs` (commit `c6e4d88`) =
  DN-3's flip lane `dn3-flip-legs` (`adcb7d2`: DN-3's lane + `never-` prefix) with only the pin moved to the code head;
  pin install `.hven/install-53fc92c93433`, libhven.a sha256
  `db63cb78adc44359c132d1e2fda2b21e2bede0ea848d94f4d3b19af7a486f5f5`. The clone's HEAD was put back on `dn1b-legs`.
- The clone's decoded CUTEst instances (`.scratch/h3/decoded/`, deleted with the earlier build trees) were re-made by
  the harness's own `scripts/decode_cutest.sh` with `SET=small-dense` (`sl/decode.sh`, `dev/decode.out`: 97 decoded,
  0 failed; GNU Fortran 16.2.1). SIFDecode is the external install (owner ruling 2026-09-15).

## 2. Disclosures: what ran before this file (none is a leg)

- `dep/eigen`, `dep/fmt`: copies of the main checkout's submodule work trees at the pinned commits (`bc3b39870`,
  `407c905e4`, read from `.git/modules/*/HEAD`).
- The diagnosis's probes (branch `diag-dn3-flip-findings`, outside this tree) and, in this tree, an env-gated stderr
  probe (removed before the code head) on the elastic fixture: raw rcond 5.0e-17, equilibrated 5.0e-17 (still
  flagged); 8 shifted answers at eta 4.2e-11 against rho 1.0. Targeted unit runs with the same probe.
- Dev suites (scratch `build-dg/out/`): the first full Release run after the change read 19 failures, every one a unit
  fixture written around the two old measurements (section 3); after the re-derivations Release and Debug read
  "100% tests passed, 0 tests failed out of 3638" on the code head's tree.
- **The dev TRAIN run** (`dev/identity/`, `dev/enum.out`): `sl/enum.sh` on the frozen lane binary above, all 238
  cells x {default, never-default}, serial, before this file. A first attempt read no rows (the decoded instances
  were missing; its rows were discarded) and is not part of the record. Its tally (`dev/identity/tally.txt`): kNever
  rows 238/238 equal to DN-3 r5's; 41 default-row movers, listed with their moved keys in `train-movers.txt`. **The
  I4 criterion below binds the leg's rows to these dev rows**: the movers were seen before this file, so the leg must
  reproduce them, as DN-3 r5's I3c-A1 bound its rows to r4's.
- No replay, interior, census or install-smoke run of the code head was taken before this file.

## 3. The declared movers

**Test re-derivations** (each test states its old reading in its comment):

| file | tests | reading moved by |
|---|---|---|
| `tests/qp/test_qp_dense_arm.cpp` | FactorizeCheckedShiftsAFlaggedFactor and AThrowBeforeTheNextFactorizationKeepsTheShiftMarker (now on an intrinsically singular K; the HS90 K is a new test, TheHs90RowIsSingularOnlyByScaleAndIsNotFlagged); TheAllinitcShapeMuFreeSystemIsAnsweredOnTheDenseArm (not flagged; raw factor); TheUnscaledMonitorStallsWhereTheEquilibratedOneConverges (shift applied directly) | fix 1 (scale-only K no longer flagged) |
| same | ARankDeficientWorkingSetAtMuZero... (inconsistent half: gate passes, row check refuses); ANearlyInconsistentFace... -> ...IsAcceptedWithItsRowsMet; TheMuFreeTerminalRefinement... -> ...AdoptsAnAcceptedShiftedAnswer; TheTerminalPolish... -> ...JudgesAnAcceptedShiftedAnswerByItsOwnTest; AShiftedK0BorderAnswerMissingRho... -> AShiftedK0BorderAnswerOnANearlyInconsistentFaceIsUsed; TheBorderedShiftedRatio... -> TheBorderedShiftedBackwardErrorIsTheWholeSystemsInTheShiftsMetric; AShiftedK0AnswerWithLiveBordersMissingRhoIsRefused -> ...OnAnInconsistentSystemPassesTheGate; TheBorderBlockAloneCanRefuse... -> ATinyBorderRhsDoesNotRefuseACorrectShiftedK0Answer; TheVerdictRefinementsRefuse... -> ...TakeAShiftedReplayTheGateAccepts; TheBorderVerdictGateRefuses... -> ...PassesALiveBorderReplayWithASmallBackwardError; TheTerminalPolishRhoRejection... -> TheTerminalPolishAdoptsAnAcceptedShiftedAnswerWhereAdoptionSucceeds | fix 2 (eta ~ sigma passes) |
| `tests/drivers/test_dense_arm_engines.cpp` | IpmTrustsAFlaggedDenseFactor... -> IpmTrustsADenseFactorSingularOnlyByScale; SqpShiftsASingularKktOnTheDenseArm (new intrinsically singular fixture; the old one is SqpDoesNotFlagAKktSingularOnlyByScale) | fix 1 |
| same | TheSsnRefusesAShiftedNewtonStepThatMissesK -> TheSsnStepsOnAShiftedNewtonStepWithASmallBackwardError; TheSsnRefusesAnUnshiftedDenseNewtonStep... -> TheSsnAcceptsABackwardStableUnshiftedDenseNewtonStep | fix 2 |
| `tests/linear/test_symmetric_factor_dense_arm.cpp` | TheDetectorBracketsBothThresholds (diag(1, 0.8 * 2^-53) no longer flagged); FlagsASingularToWorkingPrecision... (+ the re-estimate count); DenseSolveCheck: AnUnflaggedIllConditionedDenseSolveIsRefused -> AnIllConditionedBackwardStableDenseSolveIsAccepted, RefinementRescues..., TheRefusalThreshold..., ANonFiniteSolve... (eta); new: RowScalingAloneNeverFlags, TheOneNormEstimatorIsALowerBoundWithinAFactorThree, ASolveThroughTheWrongNumericsIsRefused | fix 1, fix 2 |
| `tests/sqp/test_sqp_solver.cpp` | SqpDriverElastic.ElasticSurvivesALargeConstraintScale: the DN-3 flip's kNever pin removed; passes at the default | fix 2 |

**TRAIN** (`train-movers.txt`, 41 cells, each with its moved keys): 33 IPM cells move only
`side.dn3b_kkt_dense_flagged_count` (the IPM does not consume the flag; its flags over the roster fall 982 -> 24); 8 SQP cells
(ALLINITC, BT7, BURKEHAN, HS88, HS90, HS92, MESH, SPANHYD) move dense counters only -- flags 1126 -> 1099, shifted
refusals 41 -> 0, dense factorizations 72797 -> 72764 over the roster -- and ALLINITC's kkt_stationarity by 3.6e-15
(in the residual gate). No status, outcome or iteration count moves.

**Expected unmoved**: the 27-cell replay (walk / ssn / ipm), the interior leg, the 57-cell census -- the cells above the
dense thresholds or reached only through code the change does not alter; and every kNever row.

## 4. The legs and their criteria (fixed here, before data)

| leg | instrument | criterion |
|---|---|---|
| F1 | `legs/ctest.sh` rel, dbg -> `legs-out/suites/` | each log reads "100% tests passed, 0 tests failed out of 3638" |
| I1 | `legs/replay.sh` -> `legs-out/replay/` | 27 cells x walk / ssn / ipm, three times, against `docs/notes/data/2026-10-m7-wx2-gates/gates/replay/head-<mode>.csv` (DN-3 r5's I1 reference), `scripts/compare_replay.py` byte-exact, 0 differences in all 9 |
| I3 | `legs/interior.sh` -> `legs-out/interior/` | I3-1: the 27-cell interior run vs DN-3 r5's `r5/interior/interior-default.csv`, 0 differences; I3-2: the baseline invocation vs `bench/baselines/2026-11-m8-dn3-flip-ipm-leg/interior_baseline.csv`, 0 differences (byte-exact) |
| F3 | `legs/install_smoke.sh` -> `legs-out/install-smoke/` | install smoke and export contract rc 0 on the default (GCC) and clang++ front ends |
| I4 | `sl/enum.sh` + `sl/tally.py` -> `legs-out/identity/` | `I4_PASS`: I4K every `never-default` row equals DN-3 r5's `K` row (238/238); I4D the default-row movers against r5 are exactly `train-movers.txt` (none undeclared, none declared-but-unmoved); I4R every default row equals the dev run's (`dev/identity/`), 238/238. Equality as DN-3 r4's I4: every .out key but `options_label` / `hven_pin_sha` plus the exit code, integers and strings byte-equal, residual-class values byte-equal or within hven-bench `docs/corun.md`'s calibrated gate (rel 1e-12, abs 1e-13); the .optdump byte-equal (I4K, I4D); no capped or missing row. The adoption tally (dense vs sparse outcome per cell, before = r5, after = this) and the dense counter totals are REPORTED, not gating |
| I2 | `legs/census.sh` -> `legs-out/census/` | `CENSUS_AS_DECLARED`: DN-1d r3's `census_check.py` against `bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv` with PF-1's two declared movers, every non-deadline-bound row exact (DN-3 r5's I2) |

Protocols. Every leg: `pgrep -a hven_bench_cell` empty first, under `flock /tmp/box-build.lock`, `set -o pipefail`,
FOREGROUND_START/END markers, `MKL_NUM_THREADS=1`, `TMPDIR=build-dg/tmp`, binary sha256 checked before the first cell.
F1: `ctest -j4` under `setsid`, `GIT_CEILING_DIRECTORIES=$TMPDIR`. I1, I3, I4: serial, one process at a time (I4: 476
cell processes through DN-3's unchanged `sl/cell.sh`, THREADS=1, core 2, CAP 300 s). I2: the census runner's own
declared co-run protocol (parallel-tiered v1 at its defaults: T1 6-wide, one process per pinned physical core, T2 solo,
T3 5-wide full budget, `MKL_NUM_THREADS=1`), counters asserted, wall-clock informational only. No leg asserts
wall-clock. `legs/orchestrate.sh` checks that `src include tests bench examples CMakeLists.txt cmake scripts` equal
the code head and are clean before the first leg.

## 5. STOP and ON FAIL

The orchestrator stops at the first failed leg: commit the evidence, report; no re-run, no criterion change after
data; a successor `DECLARATION-r1.md` after a fix, with this record kept. A deviation is not re-cut into a pass.

## 6. NOT RUN

HOLDOUT (never). The macOS and Windows CI lanes (no push; every Accelerate and Windows reading UNOBSERVED). Any
wall-clock or instruction measurement (the change claims behaviour, not speed; the re-estimate runs only on
raw-flagged factorizations). The iparm leg (no Pardiso configuration moves).

## 7. Order

```
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a44d4824b0beb0ea7/docs/notes/data/2026-11-m8-dense-gate/legs/orchestrate.sh 53fc92c93433b1652e94751e0318b83a597bd0e9 927cfc3d8c7af0660719179b9db1e5aaf8d5fe3aa3cd695f0a4f2276b4c14a7b e17dc5f396b71d0032fabde833730843d2fcb2ca7655b1a814cfd40c18ed032e > /home/ghecht/Projects/hven/.claude/worktrees/agent-a44d4824b0beb0ea7/docs/notes/data/2026-11-m8-dense-gate/legs-out/orchestrate.out 2>&1
```
