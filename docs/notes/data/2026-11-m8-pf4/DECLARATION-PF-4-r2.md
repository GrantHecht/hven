# M8 PF-4 r2 -- the Schur border batch on origin/m8 331b31837, after the settler's accuracy question (DECLARATION)

Committed on `pf4-r2` before the first r2 leg; never edited afterwards. `DECLARATION-PF-4.md`, `DECLARATION-PF-4-r1.md` and
every run-1 / r1 output are kept: they were re-committed from `pf4` without the r1 runtime leg's per-process perf tables.
A failed or void r2 leg is answered by `DECLARATION-PF-4-r3.md`. Everything r2 adds lives under `r2/`.

## 0. Why r2

The settler did not ship r1. Batched columns differ from single solves by up to 1.1e-3 relative (STEENBRC), far beyond
rounding. The trajectory movers were read as an accuracy gap in one path: ACOPP118 x2.04 minors; STEENBRB and STEENBRD at
worse local minima; CRESC100.D 40 -> 211 iterations; MESH.K kInfeasible -> kNumericalError; CRESC50.K kNumericalError ->
kIterationLimit. r2 does four things:
- it measures the question (section 2) and finds no accuracy gap, so it builds no accuracy remedy (section 3);
- it rebases the unchanged code onto `origin/m8` `331b31837` (RS-6b, I17);
- it adds the retake-arm killers that r1 lacked;
- it re-derives every mover against the new arm A (they are r1's, exactly).

## 1. The code and the binaries

- Branch `pf4-r2` = `origin/m8` `331b3183747b5c1849651706f648de2c337ddf75` (arm A) plus these cherry-picks from `pf4`:
  - the code: `8aa9d23da` (= `d0c228d85`), `6cf5400f2` (= `204c4444a`), `5a40a5e7a` (= `ea71502aa`);
  - the docs: `144a760e1`, `c80d60cce`, `248e15a45`, and `d526c803a` (= `4f7edc027` + `b174de584` squashed, so the 66 MB of
    tables never enter this branch's history);
  - one new test commit, `4a256c7a30c4d1953cc3f84fed0fac7b497436c3`.
  The cherry-picks had three conflicts: the source-count pin (60 -> 61, I17 added `qp/qp_dump_writer.cpp`, also recorded
  in `docs/build.md`), `tests/sqp/CMakeLists.txt`'s list, and the `sqp_complete.jsonl` golden. The golden was merged by
  script: m8's lines plus the two PF-4 keys after `sparse_retake_refusals`, 2 lines changed. No `src/` or `include/` line
  differs from `pf4`'s code except where m8 itself moved underneath.
- **Code commit `4a256c7a30c4d1953cc3f84fed0fac7b497436c3`.** The code paths `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the
  leg HEAD must equal it (`r2/legs/orchestrate.sh`). Its one new commit, `test(kkt)`, adds
  `Schur.BatchOnRetakeNumericsLandsThePerAddColumns` (MKL only). For each of kScaled and kMatched (`retake_in_place`) it
  checks three things against the per-add stack:
  - the batch's one solve runs on the tier's session, with 0 solves on the platform factor;
  - the bordered solve's bits equal the per-add stack's;
  - the answer solves the dense bordered system to 1e-9 relative.
- Trees: `.scratch/build-pf4/{rel,dbg}` of the `pf4-r2` worktree, and arm A's corpus tree `.scratch/build-pf4/armA` from
  `git archive 331b31837`. clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON`.
- Corpus binaries (frozen, `.scratch/bin/{A,B}/hven_sqp_corpus`): A `b63d68e8356291dc8db9f7d0c5ab5a06cb29551b019ee33fbdf6eab31111bb8e`, B `b55a15090390697dbca09d9a53b931da042ed4450c82aaa93667532231d164d7`.
  B was built at `0c7465445`, whose tree differs from the code commit only in `tests/kkt/test_schur.cpp`.
- Lane binaries (frozen, `.scratch/bin/{A,B}/hven_bench_cell`; `r2/legs/lane_build.sh`): the same private hven-bench clone
  as r1 (main `e03cbad` + `never-<base>` + the pin, never pushed). A (pin `331b31837`) `9d766ea786fdbffb938120547b629177c5ff8d4309efa144194a5044246c9481`; B (pin = the code commit)
  `def8b04c10f70ccd5c0f3c6d4bc882409b3e103be77b6dd8a1309168011805ef`.

Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, `MKL_NUM_THREADS=1`. Apple/Accelerate and Windows: CI only.

## 2. The measurement (what ran before this file; none is a leg)

**Probe 3** (`r2/probe/`; scratch branch `pf4-probe3`, never pushed, diff `r2/probe/pf4-probe3.diff`). It runs on the r1
code. Inside every k >= 2 batch it also solves each column singly, through the session the numerics use (plain, scaled or
matched). It then computes each column's backward error against K0, for both answers:
eta = ||K0 x - b||inf / (||K0||inf ||x||inf + ||b||inf). It also reads the backend's refinement steps (iparm[6]) and the eta
after one more refinement step through `solve_mat`.

`PF4P3_LAND=batch|single` chooses which answer lands, so one binary runs either trajectory. It reproduces lane B and
lane A's rows exactly on every cell. Re-solving the batch reproduced its columns bit for bit on every column.

Cells: DALLASM, STEENBRC, ACOPP118 (nlp-35, `default`) and SPANHYD (`default` and `never-default`), on both trajectories:
5.55 million columns in 73 367 batches. Every batch of these cells ran on plain numerics: no retake numerics occurred.
Full table: `r2/probe/backward_error.txt` (tool `r2/tools/probe3_summary.py`, CSVs of the three small cells in
`r2/probe/csv/`). On the batch trajectory:

| cell | perturbed pivots | columns | eta batch median / p99 / max | eta single median / p99 / max | mean log10(eta_b / eta_s) | refinement steps |
|---|---|---:|---|---|---:|---|
| DALLASM | 0 | 6 187 | 2.1e-20 / 1.3e-17 / 7.5e-17 | 2.1e-20 / 1.3e-17 / 7.5e-17 | +0.003 | 2 / 2 |
| STEENBRC | 0 | 26 102 | 1.6e-17 / 1.2e-11 / 2.3e-10 | 1.6e-17 / 8.8e-12 / 1.6e-10 | +0.031 | 2 / 2 |
| STEENBRC | > 0 | 23 803 | 5.1e-11 / 6.9e-10 / 4.8e-09 | 4.8e-11 / 5.6e-10 / 4.0e-09 | +0.042 | 2 / 2 (12 single columns stopped at 1) |
| ACOPP118 | 0 | 51 493 | 2.9e-11 / 1.1e-10 / 5.2e-10 | 2.9e-11 / 1.1e-10 / 4.8e-10 | +0.023 | 2 / 2 |
| SPANHYD D | 0 | 1 303 247 | 8.8e-19 / 2.6e-07 / 1.0e-06 | 8.6e-19 / 2.6e-07 / 1.7e-06 | +0.006 | 2 / 2 |
| SPANHYD D | > 0 | 104 821 | 6.6e-18 / 2.8e-17 / 3.1e-17 | 6.3e-18 / 2.3e-17 / 2.8e-17 | -0.003 | 2 / 2 |
| SPANHYD K | 0 | 1 227 120 | 4.9e-19 / 2.7e-07 / 1.9e-06 | 5.3e-19 / 2.5e-07 / 1.1e-06 | +0.012 | 2 / 2 |

The single-trajectory rows say the same (`backward_error.txt`).

**Reading.**
- **The two solves are equally backward-stable.** They have the same median, the same p99 and the same tail. The
  geometric mean of the batch's eta exceeds the singles' by 0.7-10 % (log10 +0.003 to +0.042): a few hundredths of a
  digit, against a per-column spread of about +/-0.5 decades both ways.
- **Iterative refinement is not the difference.** Both report 2 steps (iparm[6]) on every column, with and without
  perturbed pivots. Where single solves stopped after 1 step (12 STEENBRC columns), the batch ran 2: its one count
  covers all columns, so a batch never refines less than a single solve would.
- **The forward differences (1.1e-3 STEENBRC, 1e-4 SPANHYD, 3.5e-6 ACOPP118) are conditioning.** Two answers that are
  both backward-stable at about 1e-11 can differ by cond(K0) x 1e-11.
- **The cause** is rounding order. The multi-RHS solve's blocked forward/back substitution sums in a different order
  than k single substitutions, a last-ulp difference per operation. That difference is amplified by cond(K0), and
  further by the stalled refinement on perturbed-pivot factors. Neither path is more accurate.
- **No iparm, workspace or layout difference is involved.** The same phase-33 call carries `nrhs`, and the scaled and
  matched arms of `solve_mat` form the same elementwise products as `solve_vec`. The scaled and matched arms are now
  pinned by the new fixture.
- **A finding in both arms, unrelated to PF-4.** SPANHYD's unperturbed columns keep a backward error of up to 1.9e-6
  after Pardiso's two refinement steps. One more step through the same numerics takes them to 1e-10. This is registered
  in section 7.

**Probe 3j: trajectory sensitivity** (`r2/probe/jitter.txt`, `probe3j.sh`). This is the same probe, with every landed
k >= 2 column perturbed by x += K0^-1 delta. Here delta is dense, with entries 2^-52 ||b||inf u, u uniform in [-1, 1) from
a seeded generator: a backward perturbation at the unit roundoff, below both solves' own eta. Seeds 1-3, on both landings.
Seed 0 is the unperturbed arm.

| cell | arm A (single) seeds 0 / 1 / 2 / 3 | arm B (batch) seeds 0 / 1 / 2 / 3 |
|---|---|---|
| ACOPP118 minors | 10361 / 5739 / 5639 / 2546 (all optimal) | 33922 / 954 / 954 / 24491 (numerical_error) |
| CRESC100.D iterations | 40 / 44 / 44 / 44 | 211 / 40 / 38 / 40 |
| STEENBRB objective | 9086.19 / 9119.97 / 9444.38 (numerical_error) / 9119.97 | 9203.80 / 9075.86 / 9075.86 / 9159.69 |
| STEENBRD iterations (objective) | 85 (9030.08) x 4 | 107 (9074.19) / 73 / 50 / 62 (9030.08) |
| MESH.K | infeasible 5 / NE 114 / infeasible 9 / NE 15 | NE 93 / NE 19 / NE 105 / max_iter 242 |
| CRESC50.K | NE 138 / 252 / 208 / 122 | max_iter 279 / 259 / 277 / NE 225 |

Every r1 cost mover sits inside its own cell's unit-roundoff spread:
- ACOPP118's x3.3 minors is one draw from a spread of 954-33922 minors, which also contains a failure. Arm A's draws span
  2546-10361.
- CRESC100.D's 211 iterations becomes 38-40 under any jitter.
- STEENBRB's arm A itself reaches kNumericalError at one seed.
- STEENBRD's arm B reaches arm A's 9030.08 at every non-zero seed.
- MESH.K's arm A alternates kInfeasible and kNumericalError.

The movers are the sensitivity of these trajectories to perturbations below the solves' own error, not an accuracy gap.

**Probe 3g: a guard, not built** (`r2/probe/probe3g.log`). The batch columns with eta > 1e-12 get one refinement step
through `solve_mat` (one extra multi-RHS solve on the flagged subset), each kept iff its eta falls.
- Offline, on probe 3's columns, this makes the batch's mean eta lower than the singles' on every cell (-0.025 to -0.8
  decades), at the cost of the extra solve on 49 % (STEENBRC), 82 % (ACOPP118) or 14 % (SPANHYD) of the columns.
- In the solver, on the movers, seeds 0-2: ACOPP118 6133 / 11120 / 6229 minors; MESH.K kInfeasible x3; STEENBRB/D equal
  to the batch arm.
- **CRESC100.D goes to kIterationLimit at two of three seeds**: a status worse than both arms' every draw.
- Higher accuracy in the K0 columns does not buy a better trajectory, and it would cost roughly half of the saving on
  the ill-conditioned cells. Not built.

**Dev runs at `0c7465445`** (the code commit's tree but for the R2-killer's two count assertions; raw under
`.scratch/dev/`):
- ctest Release and Debug: "100% tests passed, 0 tests failed out of 3965".
- Replay vs the new arm A: walk 3, ssn 0, ipm 0. The 3 are r1's three residual-class columns of `f7_n800_path_warm`, to
  the digit.
- TRAIN 238 x D/K: 476 rows, 0 missing or capped. The movers are exactly `movers-train.txt` (33; 1 BETTER, 32 NEUTRAL,
  0 WORSE), with every mover's base and new status, iterations, objective and minors identical to r1's. hven-sqp B/A
  0.591184 (D 0.5302, K 0.6544), hven-ipm 1.000001. Against r1's rows, 40 rows of each arm differ, in I17's harness ledger
  `side.*` columns only (I17's declared "40 ledger movers").
- nlp-35: the movers are exactly `movers-nlp.txt` (14) with r1's values. Total B/A 0.998925; ACOPP118 2.0359, STEENBRB
  1.0519, STEENBRD 0.8862.
- Mutation pre-flight (`.scratch/dev/mut/`, every id run as must-kill): control clean; B1-B7, T2-T6 and R1 KILLED; R2
  SURVIVED. On this fixture, the platform factor's solve of the matched tier's columns gives the matched session's bits.
  The fixture then gained the two solve-count assertions (the code commit). Re-run on the code commit, R1 and R2 alone
  (`.scratch/dev/mut-r2/`): control clean; R1 KILLED, R2 KILLED, each by `Schur.BatchOnRetakeNumericsLandsThePerAddColumns` (MUTATIONS_AS_DECLARED).
- The private clone's lane B was rebuilt at the code commit (`def8b04c10f70ccd5c0f3c6d4bc882409b3e103be77b6dd8a1309168011805ef`). Rows skip `hven_pin_sha` (train_check's rule), so
  P3 compares the gate's rows against the dev rows of the `0c7465445` lane.

**Box protocol (owner's rule, 2026-11-20, mid-round).** The box now has two 4-core lanes (`~/.local/bin/box-lane`).
r2's copied legs:
- each check `BOX_LANE` instead of a box-wide `pgrep` guard;
- take `-j$BOX_LANE_JOBS`;
- pin THREADS=1 cells and the runtime leg to the lane's first physical core. Instruction counts do not depend on the core.

`r2/legs/orchestrate.sh` takes one box lane per compute leg. No leg asserts or quotes wall-clock, so none needs
`--exclusive`. The dev runs above ran under the legacy lock (`.scratch/r2s1.sh`); the R1/R2 re-run and probe 3g ran in a
lane.

## 3. The remedy: none

Batch is not less accurate than singles (section 2), and singles are not less accurate than batch. No accuracy check is
built, so there is no accuracy-check mutation. No iparm is touched.

The movers stand as r1 declared them. They are a property of these trajectories' sensitivity, and the settler's ship
question is judged against probe 3j's spreads.

## 3a. The expected movers (declared)

- Every mover list of r1 holds unchanged, against the new arm A: `r2/movers-train.txt`, `r2/movers-nlp.txt`,
  `r2/movers-replay.txt` and `r2/movers-runtime.txt` are byte copies of r1's. Directions and values are those of
  `DECLARATION-PF-4.md` section 3 (re-measured identical, section 2).
- Test suite: 3965 tests on both trees. This is m8's count plus PF-4's (r1's) plus the new fixture.

## 4. The legs and their criteria (fixed here, before data); outputs under `r2/gate/`

| leg | instrument (`r2/legs/`) | criterion |
|---|---|---|
| P1 | `ctest.sh` rel, dbg | each log reads "100% tests passed, 0 tests failed out of 3965" |
| P2 | `replay.sh` x3 | each run and mode: vs arm A's dev rows exactly walk 3, ssn 0, ipm 0; vs arm B's dev rows 0 (`scripts/compare_replay.py`, exact) |
| P3 | `train_enum.sh` (MODE stat) + `train_check.py` + `instr_compare.py` | `I4_PASS`: every one of the 476 rows equals its dev B row; the rows differing from arm A's dev rows are exactly `r2/movers-train.txt`; no row missing or capped; no WORSE mover. `TOTAL hven-ipm` B/A in [0.995, 1.005] and `TOTAL hven-sqp` B/A <= 1.005 |
| P4a | `../runtime/run_walk_r1.sh` (r2 copy) | the corpus runtime gate, walk mode, 3 rounds (A->B, B->A, A->B), in one box lane: THROTTLE 0; `analyze.py`'s stop condition (A vs B equal except the declared residual-class columns of `r2/movers-runtime.txt`; each arm's rounds equal); filtered B/A <= 1.005 |
| P4b | `nlp_instr.sh` + `nlp_check.py` | `NLP_AS_DECLARED`: A and B rows equal their dev rows; the movers are exactly `r2/movers-nlp.txt`, none WORSE; instruction totals reported, no threshold |
| P5 | `mutations.sh` | control clean; B1-B7, T2-T6, **R1, R2** each KILLED (`MUTATIONS_AS_DECLARED`); the tree restored |
| P6 | `install_smoke.sh` | install smoke and export contract rc 0 on GCC and clang++ |

The mutations are r1's (`r2/legs/mutate.py`):
- R1: `solve_mat`'s scaled-retake unscaling dropped. Must-kill, by the fixture's bit and bordered-system checks.
- R2: the matched arm solving on the platform factor. Must-kill, by the fixture's solve counts.

Protocols:
- Every compute leg runs in one `box-lane` acquisition, with `set -o pipefail`, FOREGROUND_START/END markers,
  `MKL_NUM_THREADS=1`, `TMPDIR` in the worktree, and binary sha256 checked first.
- P1: `ctest -j4`.
- P2, P3, P4b: serial, one process at a time. P3 and P4b run through `cell.sh` with THREADS=1, the lane's first core,
  CAP 300 s.

## 5. Order, STOP and ON FAIL

A pre-gate build (not a leg): `build.sh` rel and dbg at the code commit, each in a box lane. Then, outside any lock:

```
bash docs/notes/data/2026-11-m8-pf4/r2/legs/orchestrate.sh 4a256c7a30c4d1953cc3f84fed0fac7b497436c3 b63d68e8356291dc8db9f7d0c5ab5a06cb29551b019ee33fbdf6eab31111bb8e b55a15090390697dbca09d9a53b931da042ed4450c82aaa93667532231d164d7 9d766ea786fdbffb938120547b629177c5ff8d4309efa144194a5044246c9481 def8b04c10f70ccd5c0f3c6d4bc882409b3e103be77b6dd8a1309168011805ef 3965 > docs/notes/data/2026-11-m8-pf4/r2/gate/orchestrate.out 2>&1
```

Legs in order: P1 (rel, dbg), P2, P3, P4a, P4b, P5, P6. The chain stops at the first failed leg. Then the evidence is
committed and reported, with no re-run and no criterion change after data. `DECLARATION-PF-4-r3.md` follows a fix.

## 6. NOT RUN

- HOLDOUT (never).
- The census.
- Any wall-clock leg.
- Probe 3j's ensemble on the 238 TRAIN cells. It ran on six mover cells, seeds 0-3; a wider ensemble is the settler's to
  ask for.
- Apple/Accelerate and Windows: CI only. The batch's multi-RHS rounding against single solves on real Mac hardware is
  UNOBSERVED.

## 7. Registered (for the settler)

- **Ship or not.** On accuracy: equal (section 2). On cost: TRAIN hven-sqp instructions -41 %, nlp-35 -0.1 % in total
  (-11 to -35 % on 12 of 15 cells), and r1's movers unchanged. Every cost mover lies inside its cell's unit-roundoff
  spread (probe 3j). A rule of "no material cost regression on any solved cell" cannot be met by any numerics event on
  these cells; arm A's own draws break it.
- **SPANHYD K0 column accuracy (both arms, not PF-4).** Unperturbed columns stay at eta up to 1.9e-6 after Pardiso's two
  refinement steps; a third step reaches 1e-10. Whether that relates to SPANHYD's kNumericalError in both arms is
  unmeasured.
- Symbolic reuse, the trace reader's two keys, and Accelerate's rounding: as r1 registered them.
