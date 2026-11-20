# M8 I18 -- issue #18: a feasible walk QP ends kNumericalError (DECLARATION)

Committed before the first gate leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is
answered by `DECLARATION-I18-r2.md`, with this file and every output kept.

## 0. What this is

Issue #18: a 22-variable SQP subproblem (trust radius 3.8e-6, `hkc=1`) ends `kNumericalError` at feasibility 1e-9 and 1e-10 on
both working-set algebras. Root cause (diagnosis, settler brief `.superpowers/i18/diagnosis-2026-11-20.md`): the EQP refinement
stops on scale-blind thresholds (border path: the footprint rule `residual <= dual_mu * |lambda|`; eliminated path: one fixed
mu-free step, the polish refused by its mixed x/multiplier step cap), so at minor 6 the walk accepts an EQP step off by 1.2e-5,
three trust radii. Settler rulings 1-5 apply.

**The change (code commit `c9a716eac`), THE ACCURACY STOP** (`eqp_solve.h` header section; `refine_to_accuracy` in `solve_eqp`,
`refine_bordered_solve_iterated`'s trigger and keep rule): on the independent working set's walk, both algebras, when the
mandatory model step moves some free variable farther than the trust-region radius, the EQP answer is refined until its last
free-variable correction is at most `kAccuracyStopRatio = 0.5` of `max(||p_F||inf, step_tol)`, each further step kept only if the
residual falls strictly and both the residual and the correction shrink by `kAccuracyContraction = 0.1`, within
`kMaxBorderRefineSteps` total. `solve_eqp` now reports those steps in `EqpResult::refine_steps` (QpCounters::eqp_refine_steps; 0
unless the stop fired). The option-off walk and an infinite radius are unchanged. Plus (ruling 4) the test loader
`tests/qp/support/qp_dump.h` sets `hessian_known_convex` from the dump's `hkc=`; the regression fixture
`tests/qp/fixtures/e2-d4/I18_qp_m137.txt` (the issue's dump, verbatim) and `tests/qp/seam/test_qp_issue18_accuracy_stop.cpp`
(portable: kOptimal + the KKT battery at 1e-9 and 1e-10 on both algebras, the loader's `hkc`, the verdict/keep rules; route,
reference build only: 8 minors, 1/8 factorizations, eliminated-path refine steps).

## 1. The code and the binaries

- Base = arm A = `origin/m8` `331b3183747b5c1849651706f648de2c337ddf75` (detached worktree `../i18-base`, `build/`). Arm B = this
  worktree (branch `i18-fix`), `build/`, `build-debug/`.
- **Code commit `c9a716eacb5ace3db1e2c42c03f30bceaad4b5eb`.** `src include tests bench examples bindings CMakeLists.txt cmake
  scripts` of the leg HEAD must equal it (`legs/orchestrate.sh` P0); commits after it add only evidence and docs.
- clang 22, `-DHVEN_FP_MODE=SAFER_FAST`, bench + examples ON (`legs/build.sh`), MKL, AMD Ryzen 7 5800X3D, Linux x86-64.
  Apple/Accelerate and Windows: CI only.
- Lane binaries (`.scratch/bin/{A,B}/hven_bench_cell`; private clone `.scratch/hb`, never pushed, hven-bench main `6d1a8a0` +
  wa1's `hb_patch_never.py` + the RS include fix; `legs/train_setup.sh`): A
  `207c2954f43dfaff8934978abc73e4d211e3dc60802b2a6320087cb8f3574df3` (pin `331b31837`, branch `i18-A`), B
  `9267a8ef6d7e3b6a179978af9f8d6ed8fdd00611c128804ca8805c4bf8230ee6` (pin `c9a716eac`, branch `i18r2-B`). The nlp-35 decoded
  instances were copied into the clone by `legs/nlp35_setup.sh`.
- Box: every build and leg under `~/.local/bin/box-lane` (a lane per step); counter legs co-run at width 4, one process per pinned
  physical core of the lane, `MKL_NUM_THREADS=1` (CLAUDE.md section 7); no leg asserts or quotes wall-clock. HOLDOUT is never run.

## 2. Disclosures (none is a leg)

- **Measurement phase (ruling 1)**, scratch HVEN_TESTING env knobs (`meas/scratch-knobs.diff`, probe `meas/scratch-i18_probe.cpp`,
  runners `meas/{batch,job,summ,cost}.sh`, specs `meas/spec*.txt`), the 146-dump corpus in each dump's recorded algebra and the QP
  seam suite per candidate; the table is `meas/TABLE.md` (rows `meas/corpus/`, failing seam tests and the issue rows `meas/suite/`).
  Candidates: N1 (polish cap on the x part; all, EQP site only, or only when the first correction exceeds the box width / the
  radius), N1G (the border certificate gate absorbing `mu * |lambda|`), N2 (extra refinement to `step_tol`, box or radius
  trigger), the relative stop (`kappa` 0.1/0.5, box / radius / every solve), the contraction keep rule (0.05/0.1/0.2), and
  combinations. Every candidate but N1G fixes the issue's four rows. Unconditional or polish-based variants lose kOptimal
  (N1P/N1Psite 6, N1 6, Rall 1, N2box 3, Rbox05 2: ACOPR57_def qp4 cycles to the cap after a slowly contracting, 0.38 per step,
  refinement on a near-flat face); the contraction rule removes those losses. **Rtr05c01 (radius trigger, kappa 0.5,
  contraction 0.1) is the only candidate with no corpus status move and no seam failure** (both-optimal minors +0.04%).
- **The box-trigger variant (Rbox05c01, code `9e7bb7748`, branch-local)** gained one corpus optimum (SDL4_BRITGAS_QN qp5) but in
  the full Release ctest moved five pins (`dev-box/ctest-summary-box-9e7bb7748.txt`): `WarmStartBattery.Corpus` (border refine
  184 -> 180), `SqpSolverRadius...DenseArm` (majors 35 -> 36, 25 -> 26), two `SqpSolverRestorationExcursion...DenseArm` (the
  fixture's restoration premise no longer occurs), and `SqpSolverElasticSeed.T6bTheBandsHIGHRungsReachTheOptimum` (the unlock route
  gone; row residual 2.9e-8 against 1e-12, x off 1.5e-8). The radius variant moves none of them
  (`dev-box/ctest-summary-tr-variant.txt`: its one failure is the box variant's re-pinned certificate-sign test, restored in the
  code commit). Adopted: the radius trigger (ruling 1: the variant meeting the bar with no collateral).
- **Dev on the code commit** (`dev/`; `dev/orchestrate-dev.out` P0-P4, `dev/rest-dev.out` P5-P7): ctest Release and Debug "100%
  tests passed, 0 tests failed out of 3958"; replay A vs B 27 cells x walk/ssn/ipm **0 differences** x3; corpus 150 rows each,
  **14 movers** (`movers-corpus.txt`): the issue's four rows kOptimal (8 minors), STEENBRB_def qp3 and STEENBRB_sf qp3
  kNumericalError on both arms with the row residual 2e-2 -> 1e-6 / 9e-6 (sf: 857 -> 3621 minors), seven kOptimal rows re-routed;
  0 kOptimal lost; both-optimal minors +0.04%, factorizations +0.06% (`dev/corpus_check-dev.txt`). TRAIN 476 rows: **28 movers,
  all NEUTRAL**, 0 missing/capped, both-optimal QP minors -0.87% (`dev/train_check-dev.txt`, `movers-train.txt`). nlp-35 42 rows:
  **10 movers, all NEUTRAL** (`dev/nlp35_check-dev.txt`, `movers-nlp35.txt`). Mutations: control clean, A1-A5, A7, A8, L1 killed,
  A6 survived (`dev/mutation/`). Install smoke and export contract rc 0 on both front ends. Dev cell rows: `dev/cells-*.tar.xz`
  (sha256 train-A `11d91a92...`, train-B `83aac90f...`, nlp35-A `19865428...`, nlp35-B `e59b79bc...`); the gate reads the same rows
  from their extracted directories.
- **The nlp-35 cost (reported, not a criterion; settler's judgement).** Over the 14 hven-sqp nlp-35 rows optimal on both arms QP
  minors rise 125247 -> 145538 (+16.2%), all of it the STEENBR family (B, C, D, E, G: +16% to +23%) and ACOPR57 (+7%). That family
  is route-chaotic under any change to this rule: the same six cells under five codes (`dev/nlp35-cost-spread.txt`, rows
  `dev/variants/`) sum to A 109084, B 129375 (+18.6%), box trigger 111874 (+2.6%), 2x-radius trigger 146422 (+34.2%), kappa 0.1
  111792 (+2.5%), single cells moving by up to 2x between neighbouring variants, every row optimal. TRAIN, the ruled cost
  instrument, is -0.87%.
- **nlp-35 leg shape, fixed during dev**: `--budget-iters 30` on the `trunc30` (deadline-class) cells capped hven-sqp TAX213322,
  MSS2, MSS3, READING8 at 300 s on both arms, and on a second run also TAX53322; the four are excluded (they pass the cap within five
  majors) and the remaining `trunc30` cells run to 5 majors. Earlier dev attempts of P5 (missing decoded instances; relative binary
  path; the two budgets) were discarded and re-run; the leg's rows are the final run's.
- Not run as legs: the QP seam suite outside ctest (it is inside P1), the 146-dump corpus in the non-recorded algebra.
- SPANHYD.K's hven-sqp row was bimodal across processes in WA-1 (`../2026-11-m8-wa1/r6/`); a P4 difference on that row alone is
  answered by a solo re-run and a successor declaration.

## 3. The expected movers (declared)

- **ctest: 3958**, Release and Debug. New tests: `QpIssue18AccuracyStop.*` (4). No existing pin moves.
- **Replay** (27 cells x walk, ssn, ipm): none (`movers-replay.txt`).
- **Corpus** (146 dumps + the issue dump at {border, refactorize} x {1e-9, 1e-10}): exactly `movers-corpus.txt` (14 rows).
- **TRAIN** (238 cells x {D = default, K = never-default}): exactly `movers-train.txt` (28 rows), no WORSE.
- **nlp-35** (42 rows): exactly `movers-nlp35.txt` (10 rows), no WORSE.

## 4. The legs and their criteria (fixed here, before data)

`legs/orchestrate.sh gate c9a716eacb5ace3db1e2c42c03f30bceaad4b5eb 207c2954f43dfaff8934978abc73e4d211e3dc60802b2a6320087cb8f3574df3 9267a8ef6d7e3b6a179978af9f8d6ed8fdd00611c128804ca8805c4bf8230ee6 3958 > gate/orchestrate.out 2>&1`
runs them in order, each its own box-lane step, and stops at the first failure; outputs under `gate/`.

| leg | instrument | criterion |
|---|---|---|
| P0 | orchestrate.sh | code paths of HEAD == the code commit and clean; `../i18-base` == `331b31837` and clean; no compile or link edge pending in the three build trees; both lane binary sha256 match |
| P1 | `ctest.sh` Release, Debug (arm B) | each log reads "100% tests passed, 0 tests failed out of 3958" |
| P2 | `replay.sh` A, B + `replay_check.py` | per mode the cells differing A vs B are exactly `movers-replay.txt`'s (none) and B equals B's dev rows ("differences: 0") -- `I18_REPLAY_PASS` |
| P3 | `corpus.sh` A, B + `corpus_check.py` | 150 rows each, no timeout; the issue's 4 rows kOptimal on B; no kOptimal row of A non-optimal on B; movers exactly `movers-corpus.txt`; both-optimal minors and factorizations within +3% -- `I18_CORPUS_PASS` |
| P4 | `train_enum.sh` A, B + `train_check.py` | no missing or capped row; every B row equals its dev row; movers vs A exactly `movers-train.txt`; no WORSE mover; both-optimal QP minors within +3% -- `I18_TRAIN_PASS` |
| P5 | `nlp35.sh` A, B + `nlp35_check.py` | 42 rows present and uncapped on both arms; no WORSE mover; movers exactly `movers-nlp35.txt` -- `I18_NLP35_PASS` |
| P6 | `mutations.sh` + `mutation_check.py` | control clean; every must-kill id of `mutations-declared.txt` killed by at least one seam test; the tree restored and rebuilt -- `MUTATIONS_AS_DECLARED` |
| P7 | `install_smoke.sh` | install smoke and export contract rc 0 on the default and the clang++ front ends |
| CI | `gh workflow run ci.yml --ref i18-fix` | all four lanes green on the pushed head (recorded in `RESULTS.md`; not a leg) |

## 5. Mutations (`legs/mutate_i18.py`, each anchor exactly once; killed iff a `hven_qp_seam_tests` test fails)

| id | what | class | killed by (dev) |
|---|---|---|---|
| A1 | the eliminated path's accuracy stop never runs | must-kill | `TheFeasibleQpIsSolvedOnTheRefactorizeAlgebra` |
| A2 | the border trigger never fires | must-kill | `TheFeasibleQpIsSolvedOnTheBorderAlgebra` |
| A3 | the verdict always reads resolved | must-kill | both issue tests, `TheVerdictAndKeepRules` |
| A4 | the keep rule without its residual contraction | must-kill | `QpE4ProximalEqp.TheSteenbrbRestorationQp2ExitsFiniteAndSaysWhy`, `TheVerdictAndKeepRules` |
| A5 | the keep rule without its correction contraction | must-kill | `TheVerdictAndKeepRules` |
| A6 | the border path keeps further steps without the keep rule | recorded | none: no seam fixture takes a triggered bordered step that falls but contracts less than tenfold (A4/A5 kill the shared rule) |
| A7 | the eliminated trigger fires on every solve | must-kill | 12 tests |
| A8 | the border trigger fires on every bordered solve | must-kill | 2 tests |
| L1 | the loader ignores `hkc=` | must-kill | `TheLoaderKeepsTheDumpedConvexityPromise` |

## 6. Order, STOP and ON FAIL

The orchestrator stops at the first failed leg; the fix lands and the declaration is re-derived as `DECLARATION-I18-r2.md`. The
branch `i18-fix` is pushed (never `m8`) after the chain passes; CI on the pushed head must show all four lanes green, and a red lane
is a successor round. Nothing in this directory is edited after this commit except by adding the gate outputs and `RESULTS.md`.

## 7. Open items registered

- SDL4_BRITGAS_QN qp5 (feasible, convex, Clarabel f* = 46659152.05) still ends kNumericalError on both arms; the box-width trigger
  certifies it on both algebras but moves five SQP-tier pins (section 2). Registered, not taken.
- STEENBRB_def/sf qp3 (corpus, kNumericalError on both arms): the stop drives their row residual from 2e-2 to 1e-6 but not to an
  optimum; sf costs 857 -> 3621 minors.
- The nlp-35 STEENBR family's route sensitivity (section 2): a cost instrument that reads one family five times.
- Mutation A6 has no killer in the seam suite.
- The N1 polish-cap units (x part against the multipliers' scale) stay as they were: measured to lose kOptimal unless gated.
