# M8 walk cap -- the walk's footprint excuse capped, and its two driver layers (DECLARATION)

Committed before the first leg; never edited afterwards. Criteria are never changed after data. A failed or void leg
is answered by `DECLARATION-r1.md`, with this file and every output kept.

## 0. What this is

A declared re-pin event that changes the QP walk's classification (CLAUDE.md section 7). The DN-3 flip (dense default,
`docs/notes/data/2026-11-m8-dn3/`) exposed an SQP engine defect: F9's inconsistent pair ran to kIterationLimit on the
dense arm, so the flip pinned F9's two tests to kNever. The diagnosis (local branch `diag-dn3-flip-findings`, commit
`adc9fea4a`, env-gated probes, not landed) traced it to three layers; the orchestrator's ruling fixes all three:

1. **The walk** (`src/qp/qp_engine.cpp`, `violation_is_structural`). Condition (b) excused a row violation below
   `0.1 * dual_mu * |lambda|` with no cap, while `row_tolerance`'s footprint term is capped at
   `kInfeasibilityAbsorbTol * row_scale`. On an elastic rung `|lambda| ~ rho`; past `rho ~ 5e5` (`dual_mu 1e-8`) a 5e-4
   violation was excused and the walk returned a false kOptimal at `s = 0` violating both elastic rows. Capped like
   `row_tolerance`, (b) is implied by condition (a) (`v > 10 * row_tolerance >= 10 * min(dual_mu |lambda|, cap)`), so
   the cap is (b)'s removal: the predicate is the margin test alone, and the formerly excused violations take the
   existing terminal path (verdict-site refinement, `refine_terminal_mu_free`, the verified Farkas certificate;
   kNumericalError without one). `kStructuralResidualFrac` is removed.
2. **The driver** (`run_major`). A zero step from an elastic solve while `h_old > feasibility_tol` is exhaustion
   (verdict kRestore, restoration with no candidate), not KLV's unjudged `||d|| = 0` accept.
3. **The ladder** (`run_elastic_ladder`). `closed` and `reduced` read `slack_l1 + rung_residual_l1` (the rung's own row
   residual), an upper bound on the step's linearized violation.

Two `SqpLadderSeam` knobs (`off_rows_rungs`, `report_reads_slacks_only`; `docs/testing.md`, the sixth seam) pin layers 3
and 2, which the cap leaves unreachable without a seam.

## 1. The code and the binaries

- **Code commit `b98a19841aba62e72886d1bae200d2dbeb1b8f41`** (branch `m8-walk-cap`, one commit on `origin/m8`
  `030bae675`, the DN-3 r5 record). `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the leg
  HEAD must equal it (checked by `legs/orchestrate.sh`).
- Trees `build-wc/rel` (Release) and `build-wc/dbg` (Debug), clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST`,
  `HVEN_BUILD_BENCH=ON HVEN_BUILD_EXAMPLES=ON` (`legs/build.sh`), brought to the code commit by `legs/prebuild.sh`
  (bench stamp `check_bench_stamp: OK (b98a19841aba)`).
- Corpus binary (I1, I3, I2), frozen: `build-wc/bin/hven_sqp_corpus`, sha256
  `99ed4f2ba2af8fcb7cfb769616ade98865f6bc13d02d25d60e7ff18821a5d5de`.
- Lane binary (I4), frozen: `build-wc/bin/hven_bench_cell`, sha256
  `09d8dbd0cd083627aa03a8861d0cb70884a4f8a4abb7b538d47fc3a3973c39b2`, built by `legs/train_setup.sh` in the LOCAL
  hven-bench clone `/home/ghecht/Projects/hven-bench-dn1` (never pushed): branch `m8-walk-cap-legs` (commit `c0bc947`)
  = DN-3's flip lane commit `dn3-flip-legs` `adcb7d2` (patches g, h, i, j) + pin -> the code commit, nothing else. Pin
  install `.hven/install-b98a19841aba` (`HVEN_PIN_SHA b98a19841aba...`), libhven.a sha256
  `66ec3f2d508f4b402d482b0cd74cbd127942d367c93294027234c9e3f7ced036`. The clone's HEAD was put back on `dn1b-legs`.

Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, Release unless stated, `MKL_NUM_THREADS=1`. Apple/
Accelerate and Windows: UNOBSERVED (nothing pushed).

## 2. Disclosures: what ran before this file (none is a leg)

- `dep/eigen` and `dep/fmt` are copies of the main checkout's submodule working trees at the pinned commits
  (`bc3b39870`, `407c905e4`, read from the main checkout's `.git/modules/*/HEAD`): `git submodule update` was refused
  over the `file` transport.
- The diagnosis's probe outputs (another agent, branch `diag-dn3-flip-findings`): with the cap alone (`DIAG_FIXA`) F9
  ends kInfeasible in 2 majors and 1 restoration in all three QP modes on both arms; T6b's high rung reaches
  (2.5, 2.5); the two objective-inflated cells read kInfeasible; HsSweepCorpus minors 3347 -> 3355.
- Dev suites on this worktree (scratch logs, not kept):
  - d1 (the three fixes, no test edited but the degeneracy arm's, which no longer compiled without
    `kStructuralResidualFrac`): Release and Debug, 4 failed of 3633 each -- exactly the diagnosis's set:
    `HsSweepCorpus.Corpus` (Release 3355 vs 3347, Debug 3346 vs 3338; the Release table differs from the base table
    only on HS15 cold, 1062 -> 1070 / 1067 -> 1075 minors in its four KD/SOC cells), the two
    `QpEngine.*ObjectiveInflated*` tests (option-off a = 1e-3 cell kInfeasible, was kNumericalError), and
    `SqpDriverElasticSeed.T6bTheBandsHIGHRungsReachTheOptimum` (x = (2.5, 2.5), residual 8.9e-16, `dep_unlocks` 1).
    Layers 2 and 3 move no test of the suite: every test moved by d1 is the cap's.
  - d2 (re-pins written, F9's two tests on both arms): Release/Debug 2 failed (an over-strong assertion of mine,
    `farkas.present` on the terminal path, which that path does not export; removed).
  - d3 = the code commit's tree: **Release and Debug "100% tests passed, 0 tests failed out of 3635"** (3633 + the two
    new seam fixtures).
  - Counterfactual (scratch `cf2.out`): with layer 2 disabled by hand, `SqpLadderSeam.WalkCapAZeroElasticStepAt
    AnInfeasibleIterateIsExhaustion` fails both arms (no restoration, kIterationLimit at 100 majors -- the defect's
    loop, reproduced); restored, it passes. Layer 3's fixture discriminates by construction (its
    `report_reads_slacks_only` arm reads the defect's `closed`/`usable`).
- Dev runs on the frozen binaries (`legs/dev.sh`, outputs committed under `dev/`, 18:04-18:15):
  - replay, once per mode (`dev/replay/`): **0 differences** against the base reference in walk, ssn and ipm.
  - TRAIN enumeration, 238 cells x {D, K} (`dev/train/train_check.txt`): **21 movers, all NEUTRAL** (section 3);
    0 missing or capped; 0 in-gate residual differences.
- **No census was taken** on any binary of this fix (one census is ~6 h, serial-tiered). Its criterion (I2) is
  therefore directional, stated here before data, not a mover list.
- `legs/train_check.py` was self-tested: DN-3 r5's rows against themselves (0 movers, I4_PASS); the dev rows against
  themselves with the 21 declared (I4_PASS), with one dropped (UNDECLARED_MOVER, I4_FAIL), against r5 as the "dev"
  reference (21 DEV_ROW_DIFFERS). `legs/census_walkcap_check.py`: r5's census (CENSUS_WALKCAP_PASS, 0 movers), and r5's
  census with one Optimal row turned NumericalError and one counter bumped (WORSE + NEUTRAL, CENSUS_WALKCAP_FAIL).

## 3. The expected movers (declared)

**Test suite** (each re-pinned in its test, with the reason; commit `b98a19841`):

| test | was | now | direction |
|---|---|---|---|
| `SqpEqualityBasis.F9ZeroStepAtAnInconsistencyHandsOffToTheLadder` | kNever only (kAuto: kIterationLimit) | default arm and kNever, three modes, kInfeasible | better |
| `SqpEqualityBasisSeam.T6DisabledHandOffRunsF9ToMaxIter` | kNever only | default arm and kNever (control kInfeasible; negative control unchanged) | better |
| `SqpDriverElasticSeed.T6bTheBandsHIGHRungsReachTheOptimum` | false kOptimal at (10, 10), row off by 15 (disclosed) | kOptimal at (2.5, 2.5), residual 8.9e-16, unlock fires | better |
| `QpEngine.ObjectiveInflatedMultiplierContradictionIsCertifiedOnTheIndependentWorkingSet` | option-off a = 1e-3: kNumericalError | kInfeasible, verified certificate | better |
| `QpEngine.T6bObjectiveInflatedMultiplierBehavesTheSameInBothAlgebras` | the same, both algebras | the same, both algebras | better |
| `HsSweepCorpus.Corpus` total minors | Release 3347, Debug 3338 | Release 3355, Debug 3346 (HS15 cold; statuses unchanged) | neutral (+8 minors) |
| `QpDegeneracyArm.CertificateRefusesABoundViolationOutsideTheStructuralMargin` | dual_mu 1e-4: box leak in (1e-5, 5e-5) certified kOptimal | refused, kNumericalError | correct-direction tightening |

Plus two new fixtures, `SqpLadderSeam.WalkCapARungOffItsRowsIsNotClosed` and
`SqpLadderSeam.WalkCapAZeroElasticStepAtAnInfeasibleIterateIsExhaustion`.

**Replay (27 cells x walk / ssn / ipm):** none (`legs/movers-replay.txt`).

**Interior leg:** none (the interior-point engine reaches neither the walk nor the SQP driver).

**TRAIN (238 cells x D/K, `legs/movers-train.txt`; rows in `dev/train/cells/`):** 21 rows, all hven-sqp small-dense,
all NEUTRAL (status, iterations and objective byte-equal to DN-3 r5 on both sides):

| cell | arms | status | what moved |
|---|---|---|---|
| ALLINITC | K | numerical_error | `harness_qpsum_eq_drift_violations` 91 -> 92 |
| HS64 | D, K | optimal, 16 it | qp minors 178 -> 179, border refine +2, terminal polish adopted -1 |
| HS83 | D, K | optimal, 12 it | factorizations +1, qp minors +1, terminal polish adopted 2 -> 1 |
| HS88 | D, K | infeasible, 18 it | qp minors 257 -> 262, terminal polish adopted 7 -> 3 |
| HS89 | D, K | optimal, 21 it | qp minors 86 -> 88 |
| HS90 | D, K | optimal, 50 it | qp minors 251 -> 255 |
| HS91 | D, K | optimal, 53 / 57 it | qp minors +1 |
| HS92 | D, K | infeasible, 40 it | qp minors +2 |
| LAKES | D, K | numerical_error | `harness_qpsum_eq_drift_violations` 6 -> 7 |
| LEWISPOL | D, K | infeasible, 3 it | `harness_qpsum_eq_drift_violations` 6 -> 0 |
| SPANHYD | D, K | numerical_error, 22 / 23 it | factorizations 47890 -> 45324 (D), 47872 -> 40039 (K); qp minors down |

**Census (57 cells):** not measured before this file; see I2.

## 4. The legs and their criteria (fixed here, before data)

| leg | instrument (under `legs/`) | criterion |
|---|---|---|
| F1 | `ctest.sh` rel, dbg -> `suites/` | each log reads "100% tests passed, 0 tests failed out of 3635" |
| I1 | `replay.sh` -> `replay/` | three runs x walk / ssn / ipm; each run: I1-B vs `docs/notes/data/2026-10-m7-wx2-gates/gates/replay/head-<mode>.csv` (the base reference DN-3 r5 matched), `mover_check.py` MOVERS_AS_DECLARED with no declared mover (0 differences); I1-D vs `dev/replay/dev-<mode>.csv`, 0 differences. `compare_replay.py` byte-exact |
| I3 | `interior.sh` -> `interior/` | I3-A the default 27-cell interior run vs `docs/notes/data/2026-11-m8-dn3/r5/interior/interior-default.csv`, 0 differences; I3-B the baseline invocation vs `bench/baselines/2026-11-m8-dn3-flip-ipm-leg/interior_baseline.csv`, 0 differences (byte-exact) |
| F3 | `install_smoke.sh` -> `install-smoke/` | install smoke and export contract rc 0 on the default (GCC) and clang++ front ends |
| I4 | `train_enum.sh` + `train_check.py` -> `train/` | `I4_PASS`: every one of the 476 rows equals its dev row (`dev/train/cells/`; flip_identity.py's equality, unchanged), and the set of rows differing from DN-3 r5 (`docs/notes/data/2026-11-m8-dn3/r5/identity/cells/`) is exactly `movers-train.txt`; no row missing or capped |
| I2 | `census.sh` + `census_walkcap_check.py` -> `census/` | `CENSUS_WALKCAP_PASS`: against `bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv` with PF-1's two declared rows (`docs/notes/data/2026-11-m8-dn1-impl/census-movers.csv`) as their reference; deadline-bound cells per census_check.py; every other cell exact, or a reported walk-cap mover that is not WORSE (reference Optimal, fresh not Optimal) |

Every mover of every leg is reported with its direction in `RESULTS.md`; a classification change that makes a
previously solved cell worse is called out there whether or not a criterion catches it.

Protocols. Every leg: `pgrep -a hven_bench_cell` empty first, under `flock /tmp/box-build.lock` (the orchestrator
holds it for the whole chain), `set -o pipefail`, FOREGROUND_START/END markers, `MKL_NUM_THREADS=1`,
`TMPDIR=build-wc/tmp` (F1 also `GIT_CEILING_DIRECTORIES` at it), binary sha256 checked before the first cell. F1:
`ctest -j4` under `setsid`. I1, I3, I4: serial, one process at a time (I4: 476 cell processes through DN-3's unchanged
`sl/cell.sh`, THREADS=1, core 2, CAP 300 s). I2: the census runner's own declared co-run protocol (parallel-tiered v1
at its defaults: T1 6-wide pinned one process per physical core, T2 solo, T3 5-wide full budget; DN-1d's protocol),
counters asserted, wall-clock informational only. No leg asserts wall-clock.

## 5. Order, STOP and ON FAIL

```
flock /tmp/box-build.lock bash docs/notes/data/2026-11-m8-walk-cap/legs/orchestrate.sh b98a19841aba62e72886d1bae200d2dbeb1b8f41 99ed4f2ba2af8fcb7cfb769616ade98865f6bc13d02d25d60e7ff18821a5d5de 09d8dbd0cd083627aa03a8861d0cb70884a4f8a4abb7b538d47fc3a3973c39b2 > docs/notes/data/2026-11-m8-walk-cap/orchestrate.out 2>&1
```

Legs in order: F1 (rel, dbg), I1, I3, F3, I4, I2. The orchestrator stops at the first failed leg: commit the
evidence, report; no re-run, no criterion change after data; `DECLARATION-r1.md` after a fix, with this record kept.

## 6. NOT RUN

HOLDOUT (never). The iparm dump (no `iparm` surface or MKL integration is touched). The macOS and Windows CI lanes (no
push this round; every Accelerate and Windows reading of the re-pins is UNOBSERVED -- F9's off-reference arm records
its route and asserts only an honest status, as before). Any wall-clock or runtime-neutrality measurement (the fix
adds an O(rows) residual read per elastic ladder and changes no hot loop; not measured).

## 7. Registered (for the settler's ruling, not this round's)

- The census movers, if any: unmeasured before this file by choice (cost), so I2 is directional.
- `QpFarkasCertificate` is not exported on the walk's terminal row-certificate path (`QpSolution::farkas` stays absent
  while `terminal_farkas_certificates` counts it): the option-off objective-inflated cell is kInfeasible on a verified
  certificate a caller cannot read.
- Layers 2 and 3 are unreachable without the seam once the cap holds; they are kept as defence in depth.
