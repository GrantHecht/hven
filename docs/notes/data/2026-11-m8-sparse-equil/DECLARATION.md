# M8 sparse-equil -- the sparse equilibrated response (DECLARATION)

Committed before the first leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is answered by
`DECLARATION-r2.md`, with this file and every output kept.

## 0. What this is

A declared behaviour change of the SQP's KKT factors on MKL (CLAUDE.md section 7: a moved pinned value is declared and re-derived).
Ruling (settler): when a sparse (platform-session) KKT factorization reports `inertia.perturbed_pivots > 0`, refactor D K D, D =
`equilibration_scale(K)`, sigma = 0, on the same session, solve as b <- D (D K D)^-1 D b until the next factorization, and gate the
answers in the D metric. No iparm change (`sqp_kkt_options` is untouched: Pardiso runs unscaled and unmatched, `iparm[10]` =
`iparm[12]` = 0 by Intel's documented defaults -- the docs' reading, not observed). Cause, measured in the diagnosis: on the coupled
fixture K = S K0 S the unit block's pivots are perturbed against an unscaled ||K|| ~ s^2, and the raw relative error was 7.8e-9 at
s = 1e4, 4.9e-5 at 1e6, 0.106 at 1e8, 5.8e18 at 1e10, 2.2e25 at 1e12 (raw backward error 2.3e-10 at s = 1e8: vacuous; 8.4e-3 in the
Ruiz-D metric). Accelerate equilibrates unconditionally, is exact on the fixture and reports no perturbation count: untouched.

The change (rationale in `docs/migration-0.2.md`, "Sparse equilibrated response"):

1. `detail::factorize_checked`, both overloads: an `arm == kSparse`, `kOk` outcome with `perturbed_pivots` present and > 0 is
   refactored as D K D (`factorize(KD, kAssumeAnalyzed)`); the outcome returned is the refactor's. The dense arm's outcomes (whose
   shifted refactor reports perturbed_pivots = dim) and the platform retake of a dense-selected pattern are not touched.
2. `KktFactor::sparse_equilibrated`, `sparse_equilibrated_refactors`, `gated()`; `solve_vec` applies D; `clear_shift_marker`
   retires the marker on the next factorization. `gated()` (= dense_shifted || sparse_equilibrated) now guards what `dense_shifted`
   guarded for the D-metric gate and keep-while-falling refinement (eqp_solve, the border gate, the SSN merit-test exemption, the
   residual-refusal terminations); the K0 rebuild skip stays `dense_shifted` only.
3. **Deviation from the ruling's wording: no `SymmetricFactor` API is added.** The pattern is unchanged, so `factorize()` of the
   congruent matrix on the already analyzed platform session IS the sparse-arm refactor; a new method would only repeat it and force
   the byte-identical mirror edit in both adapters. The Apache-2.0 adapters, the MPL-derived session files, `notices/` and every
   iparm are untouched.
4. New counter `sparse_equilibrated_refactors` (QpCounters, SqpCounters, SsnResult; IpqpResult 0; trace key after
   `payload_ignored`); golden trace lines, arity pins and the two size pins re-derived once.

## 1. The code and the binaries

- **Code commit `35ab829899399d9d807982b818f15e22100d4dd5`** (branch `sparse-equil`, ONE commit over `origin/m8`
  `fa97d6bf036a1f79be362db00d103664455f99bb` = arm A). `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the
  leg HEAD must equal it (checked by `legs/orchestrate.sh`); the commits after it add only this directory.
- Trees `build-se/rel`, `build-se/dbg` (arm B), `../sparse-equil-base/build-se/rel` (arm A, detached worktree at the base), clang 22.1.8,
  `-DHVEN_FP_MODE=SAFER_FAST`, `HVEN_BUILD_BENCH=ON HVEN_BUILD_EXAMPLES=ON` (`legs/build.sh`); bench stamp B `check_bench_stamp: OK
  (35ab82989939)`. `dep/eigen`, `dep/fmt` copied from the main checkout (submodule update refused), both worktrees.
- Corpus binaries (P2, P4), frozen in `build-se/bin/{A,B}`: A `07e3e178bf9d2c84569a528eecc23928bf39f116f6a8a9892c013ef8dcd1f2e3`, B
  `d38d461819b4336eb83e87a8248b1cdf380ef1239cfc09a79fe703ee2f3d6e72`.
- Lane binaries (P3), frozen: A `d375ea46d365660dabf8c969fc278d9e90dcf14ae909270bf0c654cbeab71719`, B
  `fb99a0f55c3ea7f1aca019d413364f5a296da1734ddfbfe24941bedc266ae77a`, built by `legs/train_setup.sh` in a PRIVATE clone of the local
  hven-bench clone's `main` (`d7a55b9`, which builds against current hven; `.scratch/hb`, git-ignored, never pushed) with only
  `legs/hb_patch_se.py` applied (a `never-<base>` label prefix = `linear.dense_factorization = kNever`, DN-3 r4's semantics, and three
  SQP side columns `se_sparse_equilibrated_refactors` (B only), `se_dense_shifted_solve_refusals`, `se_dense_shifted_terminations`) and
  the pin moved to the arm's sha. libhven.a: A `c6deec7999eb...`, B `45c25d3b48ec...`. The decoded CUTEst sources are copied from
  the main clone's git-ignored `sets/cutest`.

Linux x86-64, AMD Ryzen 7 5800X3D (AVX2, no AVX-512; the reference dispatch, `HVEN_TEST_REFERENCE_DISPATCH=1`), MKL 2026.2,
`MKL_NUM_THREADS=1`. Apple/Accelerate and Windows: UNOBSERVED here; the Mac CI row is in the results file.

## 2. Disclosures: what ran before this file (none is a leg)

- Development, Release and Debug suites at intermediate heads; the first design set `dense_shifted` on the sparse factor and moved
  23 pinned fixtures (statuses included), the final one (`gated()`, `dense_shifted` untouched) 7 plus the Debug-only ones. The
  intermediate heads are not recorded. Before the final head: Release and Debug "100% tests passed" on the affected-suite subset
  (269 tests: the nine suites below, QpSparseEquil, HsSweep, the trace and counter tables) at an equal tree (a test-only line differs).
- The dev mutation run (`.scratch/dev-mut`, not committed): control clean, M1..M6 all killed (M1 by a crash in the pre-amend test,
  which was made an ASSERT; the amended head is the code commit above).
- Dev replay (arm A and arm B corpus binaries at the pre-amend head `7977abdc3`, whose `src include bench` equal the code commit's;
  libhven.a and arm A's corpus binary are byte-identical across the amend): walk / ssn / ipm, 27 cells x 75 columns, **0 differences**
  A vs B in all three. The lane pre-check: `hs014` x {default, never-default} on both lane binaries, rc 0, rows equal but the options
  digest and the B-only column; the `never-default` options dump reads `linear.dense_factorization = kNever`.
- No TRAIN, census, runtime or install-smoke run of any head before this file.

## 3. The expected movers (declared)

**Test suite** (A 3777 registered -> B 3782, +5 new: `QpSparseEquil.*`; renamed: `QpE2WorkingSet.TheDeclinedRideResolvesWithoutAnExchange
OnSteenbrbRestorationQp5` and `...Qp6`). Re-derived (route pins on the degenerate restoration dumps, Release unless stated; the trace
goldens, arity pins and size pins for the one new key):
- GOOD direction, the false kWrong this machinery was built on is not read once the factor is equilibrated: STEENBRB qp3..qp7 LPs reach
  kOptimal with 0 re-reads and no certificate (`QpNc4ConvexCertificate.SteenbrbRestorationLpQp3` 1 -> 0 certificates,
  `QpNc3SaddleExit...SteenbrbRestorationLpQp3` >= 1 -> 0); BRITGAS qp3 1 -> 0 certificates (1 re-read), BRITGAS qp4 0 -> 1 (1 -> 3
  re-reads); option-off walk minors 586 -> 582, 197 -> 86, 181 -> 184, 180 -> 183 (STEENBRB qp3..qp6); qp5 and qp6 lose their
  dependent ride blocker (re-premised, renamed).
- BAD direction, REPORTED, pinned as the new route: YORKNET restoration qp26 (`QpYrkRefusedExchange` x2) and qp29, qp30
  (`QpSdlSaddleShift` x4) end kNumericalError where they reached kOptimal; STEENBRB `R2` LP qp3 (`QpNc2FlatFace` x2) kOptimal ->
  kIterationLimit (3234 minors; E4's certification is lost); `QpLatchReason.BritgasMainSubproblemHoldsItsConditioningLatch` (first 120
  minors: factorizations 166 -> 267, conditioning latches 40 -> 92, the cost bound no longer asserted); Debug only: STEENBRB `R2` qp4
  kOptimal -> kNumericalError, BRITGAS qp4 convex-certificate route, `QpRdvRankRepair` Yorknet qp1/qp5 (no repair taken),
  `QpStagnationStop.BritgasRestorationFootprintFace` (the reading is not taken, the walk runs to the cap, 11700 minors).
- Behaviour pins: `SsnShiftedStep` (a gated sparse step is not exempt from the merit test: 0 -> 13 backtracks on the duplicated rows),
  `QpDenseArm.FactorizeCheckedShiftsAFlaggedFactor` (the kNever factor holds D iff the platform perturbed).
- HS battery (`HsSweepCorpus.*`, in P1): **none** (pins unchanged, passes at B).

**Replay (27 cells x walk / ssn / ipm):** none (dev run: 0 differences).

**TRAIN (238 cells x D/K), the mover rule, fixed here:** a row of arm B may differ from arm A's row ONLY if arm B's own row has
`side.se_sparse_equilibrated_refactors > 0` (an SQP cell that perturbed pivots at least once and was refactored; IPM rows carry no
such column and the interior-point engine never reaches `factorize_checked`, so every IPM row must be identical). Every other row
must be identical (the lane's key=value rows except `options_label`, `hven_pin_sha` and the B-only column; integers and strings
byte-equal, residual-class values byte-equal or within hven-bench `docs/corun.md`'s calibrated gate, rel 1e-12, abs 1e-13; the
`.optdump` byte-equal; the lane exit code equal). Among the perturbed rows any differing row is reported with its direction (status
rank optimal > acceptable > anything else); **0 worse statuses** is the criterion, a perturbed row may be better, neutral or equal.
No other expectation is declared; the perturbed-row count and the movers are results.

**Runtime:** counter identity on every (round, cell) of the walk corpus (no equilibrated refactor there: replay identical), so what
the gate prices is the perturbed check on the factorization path.

## 4. The legs and their criteria (fixed here, before data)

| leg | instrument (under `legs/`) | criterion |
|---|---|---|
| P1 | `ctest.sh` rel, dbg -> `suites/` (includes the HS battery, `HsSweepCorpus.*`, whose pins were derived on the base) | each log reads "100% tests passed, 0 tests failed out of 3782" |
| P2 | `replay.sh` -> `replay/` (A once, B x3) + `replay_check.py` | each B run vs arm A's row and vs the dev B row, `scripts/compare_replay.py` byte-exact, 0 differences in walk / ssn / ipm (`movers-replay.txt` is empty) |
| P3 | `train_enum.sh` x2 + `train_check.py` -> `train/` | `I4_PASS` under section 3's rule: no missing or capped row, no unperturbed row differing, 0 worse statuses |
| P4 | `../runtime/run_walk.sh` (ap5-g3's runtime gate, walk mode) | serial and solo under the lock; THROTTLE 0; counter identity on every (round, cell); B/A instructions:u within [0.995, 1.005] (`analyze_r11.py`) |
| P5 | `mutations.sh` -> `mutation/` | control (none) clean; M1 (no refactor), M2 (no D on the solve), M3 (gate removed: `gated()` ignores the marker), M4 (counter not incremented), M5 (marker not retired), M6 (the refactor factors K, not D K D) each KILLED by `QpSparseEquil.*`; tree restored |
| CI | `gh workflow run ci --ref sparse-equil`, one dispatch at a time | every lane green, the macOS (Accelerate) lane included; results in `CI.md` |

Any status worse than arm A on any leg fails the leg unless listed in section 3 (none is, outside the test pins). Protocols: every leg
`pgrep -a hven_bench_cell` empty first, under `flock /tmp/box-build.lock` (the orchestrator holds it for the chain), `set -o pipefail`,
`setsid -w`, FOREGROUND markers, `MKL_NUM_THREADS=1`, `TMPDIR=build-se/tmp`, `ctest -j4` under `setsid`; P2, P3, P4 serial, one process
at a time (P3: 476 cell processes per arm through `cell.sh`, THREADS=1, core 2, CAP 300 s). P2 and P3 assert counters and statuses only;
the wall-clock of no leg is quoted.

## 5. Order, STOP and ON FAIL

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-sparse-equil/legs/orchestrate.sh 35ab829899399d9d807982b818f15e22100d4dd5 3782 07e3e178bf9d2c84569a528eecc23928bf39f116f6a8a9892c013ef8dcd1f2e3 d38d461819b4336eb83e87a8248b1cdf380ef1239cfc09a79fe703ee2f3d6e72 d375ea46d365660dabf8c969fc278d9e90dcf14ae909270bf0c654cbeab71719 fb99a0f55c3ea7f1aca019d413364f5a296da1734ddfbfe24941bedc266ae77a > docs/notes/data/2026-11-m8-sparse-equil/orchestrate.out 2>&1
```

The chain stops at the first failed leg: commit the evidence, report; no re-run, no criterion change after data; a successor
`DECLARATION-r2.md` after a fix. Raw dumps live under the git-ignored `.scratch/` and are not committed; a MANIFEST (sha256, size, path)
and the summaries are.

## 6. NOT RUN

HOLDOUT (never). The 57-cell census, interior, install smoke, TSan (nothing here touches an install surface or a concurrent path beyond
one trailing counter). Any wall-clock measurement. The iparm leg (no Pardiso configuration moves).
