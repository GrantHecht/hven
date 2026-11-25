# M8 sparse-equil v4 r2 -- the review rider (DECLARATION-SEQ4-r2)

Successor of `DECLARATION-SEQ4.md` (kept unedited, with every output of its chain). Committed before the r2 code commit and
before any r2 leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is answered by
`DECLARATION-SEQ4-r3.md`, with this file and every output kept.

## 0. What this is

v4's gate passed (`RESULTS.md`); an independent review said "ship after fixes". r2 applies them (findings I1-I4, M1-M10, the
task brief) on the branch rebased onto `origin/m8` 44f74446f (RS-4: includes spelled `<hven/...>`, clang-format include
sorting; RS-5: NlpEval extracted, the layering and header-self-containment tests). The RS-4 rebase was mechanical: v4's three
commits replayed with `-X theirs`, then `rs4_respell.py` and clang-format over the code commit's C++ files; RS-5 replayed
cleanly. The pre-rebase head is kept as branch `sparse-equil-v4-prerebase` (v4 code `16efab624`).

The r2 code (one commit after this file):
- **I1** `take_retake` catches `hven::BackendError` (the retake session's analysis) and `std::invalid_argument` (the scaling,
  the session's input checks): a failed retake, the platform numerics kept, the check disarmed.
- **I2** a K0 request never crosses a QP solve: cleared at `run()` entry (before the reuse gate) and exit and at every border
  wipe; a pending request makes `rebuild_would_be_noop()` false (no spurious conditioning latch); a request is taken off the
  factor when a factorization STARTS (a throw cannot leave it pending). It applies to the NEXT K0, documented
  (`kkt_factor.h`, THE BORDER PATH).
- **I3** one verdict function, `platform_retake_verdict(k, missed, engine_refused)`, called by `platform_solve_response`
  (the eliminated site) and by `solve_bordered_eqp`.
- **I4** the refusal clause: KEPT (section 2, the I4 experiment: rows move), pinned by the integration must-kill
  `QpPlatformRetake.LakesQp0EscalatesOnTheEngineRefusal` on a new fixture, LAKES's first walk solve that took a refusal-only
  escalation (`tests/qp/fixtures/e2-d4/SEQ4_LAKES_qp0.txt`, dumped by `r2/diag/dump_patch.py`); the qp26 test's comment is
  corrected (its escalation fires on the backward error).
- **M1** the eliminated path retakes IN PLACE (`retake_in_place`, a bounded solve-round loop in `solve_eqp`, no recursion, no
  re-assembly, no platform refactorization); every round's residual-check solves are counted in `residual_refine_iters` /
  `eqp_refine_iters`. Counters: retake factorizations stay OUT of `factorizations` (the active-set search's accounting
  contract, which budgets and the hot-handle pins read) and are counted EXACTLY (run, taken or not) in
  `sparse_scaled_retakes` / `sparse_matched_retakes`; their analyses are documented (matched: one each; scaled: only on a
  pattern change).
- **M3** the scaled session re-analyzes only when K's pattern moves; the matched session analyzes every time.
- **M4** retake sessions are built lazily and freed at the end of every walk solve except the one K0's numerics live in.
- **M5** failed matching searches are pruned (their columns dead; dual feasibility restored on dead columns at the end); the
  heap's storage is reused; no deadline check (argued in `matching_scaling.cpp`).
- **M6** explicit size check in the scaled `solve_vec`, compressed-storage check before `K.coeffs()`.
- **M7** the verdict-site refinement's `fresh` factor is factorized THROUGH `solve_eqp` (the incumbent's procedure, the
  in-place retake included).
- **M8** the session-selection tests assert the perturbed-pivot report differs between sessions; a failed-retake/disarm test.
- **M9** Release-only route pins keyed on `HVEN_TEST_RELEASE_BUILD` (`$<CONFIG:Release>`), not NDEBUG.
- **M10** `matching_scaling.h` states exactly where the bound holds.
- Tests: +6 seam tests (`QpPlatformRetake.*`); `QpEngineTestAccess` moved to `tests/qp/support/qp_engine_test_access.h`.

## 1. Arms, code identity and binaries

- Arm A = `origin/m8` 44f74446f5e5c973de3b06b5628b41fd39449cd8 (worktree seq4-base, detached). Arm B = the r2 code commit.
- THE CODE IDENTITY: the r2 code commit's tree for `src include tests` equals the tree object ids recorded in section 6 of
  this file (computed from the dev state before this commit); `bench examples bindings CMakeLists.txt cmake scripts` equal
  44f74446f's. `r2/legs/orchestrate.sh` checks both against the leg HEAD.
- Binaries are built from the code commit by `r2/legs/build.sh` (corpus: `build-k/rel/bench/hven_sqp_corpus`, A from
  seq4-base) and `r2/legs/train_setup.sh` (lanes), frozen under `build-k/bin-r2/{A,B}`, and their sha256 are committed in
  `r2/FROZEN.txt` (a docs-only commit after the code commit, before the chain); the orchestrator checks them.
- Box: Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, `MKL_NUM_THREADS=1`, `-DHVEN_FP_MODE=SAFER_FAST`.
  Apple/Accelerate: UNOBSERVED here (macOS CI is the correctness check). Windows: CI.

## 2. Disclosures: what ran before this file (none is a leg)

All dev outputs under `.scratch/r2/` (untracked; summarized in `r2/dev/`).
- A FIRST COMMIT OF THIS FILE (33ef5b54a, kept as the local branch `seq4r2-withdrawn-decl`, never pushed) declared the r2 code
  on `origin/m8` bc8ed0e12. Before any leg ran, m8 moved to 44f74446f (RS-5, which touches `src/CMakeLists.txt` and
  `tests/CMakeLists.txt`, so the declared trees could not hold): that commit was withdrawn, the branch rebased, and the dev
  evidence the legs compare against was RE-DERIVED on the new base (below, "on 44f74446f"); the I4 experiment and the first
  dev runs stay disclosed as run on bc8ed0e12, where they were taken.
- dev1/dev2 (uncommitted r2 tree, on bc8ed0e12, before the LAKES test): Release + Debug ctest "100% tests passed, 0 tests
  failed out of 3903" (3897 + 6); replay 27 cells x walk/ssn/ipm, arm A (bc8ed0e12) vs B: 0 differences each mode.
- RE-DERIVED ON 44f74446f (the dev rows the legs compare against; `.scratch/r2/dev6.out`): the r2 tree (LAKES test included)
  Release + Debug ctest "100% tests passed, 0 tests failed out of 3909" (RS-5 adds tests); replay A (44f74446f, corpus binary
  built in seq4-base) vs B: 0 differences each mode (`.scratch/r2/dev-replay`, P2's reference); dev lanes A2 = 44f74446f and
  Bdev2 = the local commit `seq4r2-dev2` fb18c5704 (the r2 tree; its `src include tests` trees are section 6's); TRAIN A and
  B (`.scratch/r2/dev-train-{A,B}`, P3's reference): each row-identical to its bc8ed0e12 run (0 movers both; RS-5 is
  row-neutral), B's corpus replay identical too; B vs A = the 9 movers below.
- Dev lane binaries pinned to LOCAL, never-pushed commits: `seq4r2-dev` 7c25abe89 (the r2 tree), `seq4r2-diag` b520fae81
  (+ `r2/diag/diag_patch.py`: one stderr line per verdict, and `HVEN_SEQ_ME1` = Me1 at run time), `seq4r2-diag2` 03393384b
  (+ `r2/diag/dump_patch.py`: a D4QP dump of every walk solve that took a refusal-only escalation).
- TRAIN dev (238 x D/K, serial, THREADS=1 core 2, CAP 300 s): A(bc8ed0e12) vs v4's own arm A (66cc44ccc): 0 movers (RS-4 is
  row-neutral). r2 vs A: the same 9 movers v4 declared (1 BETTER ALLINITC.K, 8 NEUTRAL, 0 WORSE). r2 vs v4's B rows: 5 rows
  differ, in `side.eqp_refine_iters` only (HS101.K 1->2, HS90.K 0->1, HS99EXP.K 686->687, LAKES.D/K 4845->4853): M1's
  abandoned-round solves, now counted. diag (ME1 unset) vs r2: 0 movers (the instrument is row-neutral).
- **The I4 experiment** (Me1 = the refusal clause dropped):
  - qp26 (`QpPlatformRetake.YorknetRestorationQp26...`, diag source, both algebras): unchanged under Me1 (kOptimal, 23 minors,
    1 scaled, 1 matched). The escalating round's verdict line reads `refused=1` AND the verdict stays kMatched without the
    clause: in the v4/r2 code the scaled answer there MISSES the backward error too. RECONCILED: `RESULTS.md` ("the qp26
    fixture does NOT isolate the refusal clause") is right for the shipped code; `diag/escalation.txt`'s minor-15 reading
    (eta 1.07e-7, refused) came from the v2 dev instrument on branch sparse-equil-v2, which judged a different answer, and is
    not what the shipped code computes. The qp26 test's comment, which repeated the v2 reading, is corrected in the r2 code.
  - TRAIN 238 x 2 under Me1 vs r2: exactly 2 rows move, LAKES.D and LAKES.K (numerical_error in every arm; status, iterations,
    objective identical): matched retakes 3 -> 0, retake refusals 0 -> 2, factorizations 5380 -> 5379, eqp_refine_iters
    4853 -> 4843. ALLINITC.K unmoved.
  - The clause's firings over TRAIN (r2, diag lines): 3 refusal-only escalations per LAKES arm (eliminated site, dual_mu 1e-8,
    n_free 103-108 vs 78 rows: not over-determined), each answered by a matched retake the residual check ACCEPTED. At
    dual_mu = 0: ZERO refusal-only escalations (every scaled-tier verdict at mu = 0 over TRAIN reads refused=0), and no
    scaled-tier verdict on an over-determined face (nc > n_free). No spurious firing observed.
  - The LAKES dumps (`r2/diag/dump_patch.py`, lane on seq4r2-diag2, LAKES.D and .K once each): two walk solves with
    refusal-only escalations (qp0: 2, qp1: 1); D and K dump byte-identical files. qp0 replayed on the r2 tree (Release, the
    seam target): kOptimal, 131 minors, 3 scaled, 2 matched, 0 retake refusals -- the in-cell route exactly; with Me1 applied
    by hand: 2 scaled, 0 matched, 1 retake refusal (the new test and the tiered unit test fail; tree restored, rebuilt).
  - Decision: rows move, so the clause is KEPT (the brief's rule) and qp0 is its integration must-kill.

## 3. The expected movers (declared)

**Test suite** (B vs A): v4's movers (+8 matching, +4 response tests, BRITGAS qp4 Release pin 0/1 -> 1/3, counter arity/size
pins and trace goldens) plus +6 r2 tests (`QpPlatformRetake.AFailedRetakeLeavesThePlatformNumericsDisarmed`,
`TheScaledSolveRefusesAMismatchedRightHandSide`, `TheScaledSessionIsAnalyzedOncePerPatternAndReleasedWhenIdle`,
`TheBorderedAnswerCarriesTheVerdict`, `APendingK0RetakeIsNotTheNoOpLatch`, `ARequestDoesNotSurviveIntoTheNextSolve`), and the
integration must-kill `QpPlatformRetake.LakesQp0EscalatesOnTheEngineRefusal`: 3909 tests on 44f74446f (RS-5's own tests
included).

**Replay (27 cells x walk / ssn / ipm), B vs A:** none.

**TRAIN (238 x D/K), B vs A, `r2/legs/movers-train.txt`:** the same 9 rows as v4 (ALLINITC.K BETTER; HS101.K, HS90.K,
HS99EXP.K, LAKES.D, LAKES.K, LEWISPOL.K, MESH.K, MSS1.K NEUTRAL), each B row equal to the r2 dev row; compared with v4's B
rows, the 5 `eqp_refine_iters` moves of section 2 and nothing else.

## 4. The legs and their criteria (fixed here, before data)

| leg | instrument (under `r2/legs/`) | criterion |
|---|---|---|
| P0 | `orchestrate.sh` | `pgrep -a hven_bench_cell` empty; code paths equal section 6's trees and the code commit; frozen binary sha256 = `r2/FROZEN.txt` |
| P1 | `ctest.sh` rel, dbg -> `r2/suites/` | each log reads "100% tests passed, 0 tests failed out of N", N from section 6 |
| P2 | `replay.sh` -> `r2/replay/`, 3 repetitions x walk / ssn / ipm | each B run vs A's dev rows and vs B's dev rows (`.scratch/r2/dev-replay`), `scripts/compare_replay.py`, 0 differences |
| P3 | `train_enum.sh` + `train_check.py` -> `r2/train/` | `I4_PASS`: every B row equals the r2 dev row (`.scratch/r2/dev-train-B`); the set of rows differing from A's dev row (`.scratch/r2/dev-train-A`) is exactly `movers-train.txt`; none missing or capped. THE SHIP RULE: no WORSE mover, no lost certified cell |
| P4 | `../runtime/run_walk.sh` (r2 copy: the ap5-g3 runtime gate, walk mode) | serial and solo under the lock; THROTTLE 0; counter identity on every (round, cell); B/A instructions:u within [0.995, 1.005] |
| P5 | `mutations.sh` -> `r2/mutation/` | control clean; each of Mt1 Mt2 Mt3 Me1 Me2 Mm1 Mm2 Mm3 Mm4 (v4's set; Mt1 and Me1 now hit the one verdict function) and Mi1 Mr1 Mr2 Mb1 Md1 Md2 (r2: I1, I2 x2, I3 bordered, M8 disarm x2; `mutate.py`) KILLED by at least one test; Me1 must additionally be killed by the integration test of section 6 when one is named there; tree restored |
| P6 | `install_smoke.sh` -> `r2/install-smoke/` | both front ends and both export-contract checks rc 0 |

Any status worse than arm A on any leg fails the leg. Protocols as v4's section 4: every leg `pgrep -a hven_bench_cell`
empty first, the orchestrator holds `flock /tmp/box-build.lock` for the chain, `set -o pipefail`, `setsid -w`,
`MKL_NUM_THREADS=1`, `ctest -j4` under `setsid`, TMPDIR inside the worktree; P2, P3, P4 serial, one process at a time. P2 and
P3 assert counters and statuses only; the wall-clock of no leg is quoted. Then CI on all four lanes
(`gh workflow run ci.yml --ref sparse-equil-v4`).

M2 (the cost claim's scope), for RESULTS-r2: "cost within the declared bound" means P4's instruction count on the walk-mode
corpus on Pardiso, where the check is idle unless a factorization perturbs pivots; Accelerate runs the check on EVERY
platform factorization and its cost is UNOBSERVED.

## 5. Order, STOP and ON FAIL

`flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-sparse-equil-v4/r2/legs/orchestrate.sh <code sha>
<corpus A> <corpus B> <lane A> <lane B> <N> > docs/notes/data/2026-11-m8-sparse-equil-v4/r2/orchestrate.out 2>&1`, the
arguments from `r2/FROZEN.txt`. The chain stops at the first failed leg: commit the evidence, report; no re-run, no
criterion change after data; a successor `DECLARATION-SEQ4-r3.md` after a fix.

NOT RUN: HOLDOUT (never), the 57-cell census, the interior leg, TSan, any wall-clock measurement, CRESC100.K (open).

## 6. Code identity and the integration must-kill (filled before this commit)

The r2 code commit's trees (computed from the dev tree with `git write-tree` on a temporary index; `orchestrate.sh` reads
these three lines):

TREE src 6b6a689a116f45183f77753b69ccf26925aec042
TREE include d0c6754d1e86e1a758e8795b4f26a79b899c3402
TREE tests 62580e1f626b40aa1a78c4d4d0f54f50d90271b1

All three equal the dev lane's commit `seq4r2-dev2` fb18c5704 (whose TRAIN rows are P3's reference). N (P1) = 3909. The integration must-kill of Me1 (P5):
`QpPlatformRetake.LakesQp0EscalatesOnTheEngineRefusal`.
