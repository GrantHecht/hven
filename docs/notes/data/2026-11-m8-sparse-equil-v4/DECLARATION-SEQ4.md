# M8 sparse-equil v4 -- the platform-solve response of the SQP's sparse KKT factors (DECLARATION-SEQ4)

Committed before the first leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is answered by
`DECLARATION-SEQ4-r2.md` (and so on), with this file and every output kept.

## 0. What this is

MKL Pardiso factors the SQP's KKT matrices without matching or scaling; a pivot it cannot take inside a supernode is replaced by a
static perturbation and the solve leans on refinement. Rounds v2 and v3 (`../2026-11-m8-sparse-equil-v2/diag/`) measured two failure
classes -- the SCALE class (F3 at s >= 1e8, ALLINITC, STEENBRB's 1.1e-2 solve) and the PAIRING class (YORKNET qp29/30, CRESC100) --
and showed that a sticky response exposes a walk cycling defect on YORKNET qp26 (a separate task). Round v4's escalation experiment
(`diag/escalation.txt`) chose the tiered, non-sticky rule built here. The change (code commit, section 1; rationale in
`docs/migration-0.2.md`, last section, and `include/hven/detail/kkt/kkt_factor.h`, THE PLATFORM-SOLVE RESPONSE):

1. **Matching** (`include/hven/detail/linear/matching_scaling.h`, `src/linear/matching_scaling.cpp`): maximum-product weighted
   bipartite matching by Dijkstra shortest augmenting paths on reduced costs (Duff & Koster 2001), and the symmetric scaling
   d = sqrt(r s) read off its optimal duals (Duff & Pralet 2005). Papers only.
2. **Trigger** (`solve_eqp`, `solve_bordered_eqp`): on platform numerics that reported perturbed pivots (Accelerate, with no count:
   every platform factorization), the refined answer's equilibrated normwise backward error > `kPlatformSolveFailureTol` = 1e-5
   requests the SCALED retake (D K D on a plain session) and the EQP is re-solved.
3. **Escalation**: scaled numerics that still report perturbed pivots and fail again -- backward error, OR the EQP-site residual
   check's refusal -- request the MATCHED retake (Pardiso `iparm[12] = 1` through `SymmetricFactor::Options::weighted_matching`,
   no `iparm[10]`). IPARM-SURFACE: this is the only Pardiso configuration that moves, on a second session only; the evidence is
   the rounds' diagnosis data and this declaration's legs.
4. **Per factorization**: a request is consumed by the next factorization; a failed retake disarms the check.
5. Three counters (`sparse_scaled_retakes`, `sparse_matched_retakes`, `sparse_retake_refusals`) after GL-0's; pins and goldens
   re-derived.

## 1. The code and the binaries

- **Code commit `16efab6248d81a28a033d520ca1b5909083f1ff7`** (branch `sparse-equil-v4`; base = arm A = `origin/m8`
  `66cc44ccc9ce4b62efe7b6e1a1de3d67628a2c71`). `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the leg
  HEAD must equal it (checked by `legs/orchestrate.sh`). Commits after it add only evidence and runners.
- Trees: `build-k/rel`, `build-k/dbg` (arm B), `../seq4-base/build-k/rel` (arm A, detached worktree at the base), clang 22.1.8,
  `-DHVEN_FP_MODE=SAFER_FAST`, `HVEN_BUILD_BENCH=ON HVEN_BUILD_EXAMPLES=ON` (`legs/build.sh`).
- Corpus binaries (P2, P4), frozen in `build-k/bin/{A,B}`: A `51fd5c0bb644a26f4becf9d51cd2c897b5e5bd0b8ef2d65871a6629b409721dd`,
  B `e5d6650214be3c673c9bdf14d1ff22ccfca4f9e6f8762bfac16a60f6f142ac62`.
- Lane binaries (P3), frozen: A `000c90854b3c84b81ddd7ea2b96886165e154fbf821961057ee3f716799b26ba`, B
  `0181247dd047faa881c1b8d1216ae026f362ccef658dcd94615d68d32aab4e76`, built by `legs/train_setup.sh` in a PRIVATE clone
  (`.scratch/hb`, git-ignored, never pushed) of the local hven-bench clone at `main` 6d1a8a0 plus `legs/hb_patch_never.py`, pin moved
  to the arm's hven sha (A: the base; B: the code commit). The clone's evidence commits read "kopt-1x arm A/B" (a label left over
  from the copied script; fixed in `legs/train_setup.sh` since, not re-run).

Linux x86-64, AMD Ryzen 7 5800X3D, MKL 2026.2, `MKL_NUM_THREADS=1`. Apple/Accelerate: UNOBSERVED here (macOS CI is the
correctness check; the Accelerate arm of the response is the check on every factorization, scaled retake only). Windows: CI.

## 2. Disclosures: what ran before this file (none is a leg)

- The escalation experiment (dev instrument on branch `sparse-equil-v2`, `diag/escalation.txt`, `diag/MANIFEST-escalation.txt`).
- `dep/eigen`, `dep/fmt` copied from worktree seq2 (same submodule commits as the base); both worktrees.
- The branch was first cut from `origin/m8` ca3588735 and moved to 66cc44ccc (the Windows CI fix: `ci.yml`, `CMakePresets.json`, a
  WIN32-only block of `tests/CMakeLists.txt`) before the code commit; nothing in the Linux build differs between the two bases.
- Dev runs on the code commit (outputs under `dev/`, raw cells in `.scratch/`): ctest Release and Debug "100% tests passed, 0 tests
  failed out of 3897" (`dev/dev-ctest.txt`); replay once per arm on the frozen corpus binaries, 27 cells x walk / ssn / ipm:
  `.scratch/dev4-replay` (the P2 reference rows; an earlier dev replay on pre-commit binaries also read 0 differences); TRAIN once per
  arm on the frozen lane binaries, 476 rows, 0 missing or capped, **9 movers: 1 BETTER, 8 NEUTRAL, 0 WORSE**
  (`dev/train_check-dev.txt`); the runtime gate, walk mode, once: counter identity on every (round, cell), filtered B/A = 1.000002
  (`dev/runtime-analysis-dev.txt`; a first attempt was VOID on a missing copied analyzer, `analyze_r5.py`, copied since).
  Replay dev comparison: `dev/replay-compare-dev.txt` (0 differences, three modes).
- Pre-commit dev states: the Debug ctest of an earlier state showed BRITGAS qp4's and qp26's routes unchanged under Debug rounding,
  so their route pins are Release-only (in the code commit).
- `legs/train_check.py` treats the three new `side.sparse_*` keys as ADDITIVE (absent from arm A's rows; compared exactly between
  B's leg rows and B's dev rows).

## 3. The expected movers (declared)

**Test suite** (arm B vs arm A): +8 matching tests (`MaxProductMatching.*`, `SymmetricMatchingScaling.*`), +4 response tests
(`QpPlatformRetake.*`); BRITGAS qp4's Release route pin 0 certificates / 1 re-read -> 1 / 3 (Debug unchanged 0 / 1); the counter
arity / size pins (106 table fields; `SqpCounters` 110 * 8 + nested) and the trace goldens (three keys) re-derived.

**Replay (27 cells x walk / ssn / ipm):** none.

**TRAIN (238 cells x D/K), `legs/movers-train.txt`:** 9 rows (A -> B):
- **ALLINITC.K BETTER**: numerical_error 78 iterations -> optimal 28 (20 scaled retakes; the scale class).
- HS101.K NEUTRAL: optimal both; iterations 28 -> 26, factorizations 542 -> 1075, qp minors 806 -> 1039 (a cost increase on a solved
  cell; 7 scaled retakes reroute it).
- MESH.K NEUTRAL: infeasible both; factorizations 707 -> 782, minors 653 -> 776.
- LAKES.D and LAKES.K NEUTRAL: numerical_error both; factorizations 5375 -> 5380, minors 6384 -> 6388 (10 scaled, 3 matched).
- LEWISPOL.K NEUTRAL: infeasible both; minors 83 -> 78.
- HS90.K NEUTRAL (optimal), HS99EXP.K NEUTRAL (max_iter), MSS1.K NEUTRAL (numerical_error): counter-only moves (eqp polish/refine
  counts, inertia verdicts), status and iterations unchanged.
Retakes without a row move (only the additive keys differ): CRESC100.D/.K, CRESC4.K, CRESC50.D/.K, OPTPRLOC.K. CRESC100.K stays
infeasible (the pairing class is not won by this rule; `diag/escalation.txt`).

## 4. The legs and their criteria (fixed here, before data)

| leg | instrument (under `legs/`) | criterion |
|---|---|---|
| P1 | `ctest.sh` rel, dbg -> `suites/` | each log reads "100% tests passed, 0 tests failed out of 3897" |
| P2 | `replay.sh` -> `replay/`, 3 repetitions x walk / ssn / ipm | each B run vs arm A's dev rows and vs B's dev rows (`.scratch/dev4-replay`), `scripts/compare_replay.py`, 0 differences |
| P3 | `train_enum.sh` + `train_check.py` -> `train/` | `I4_PASS`: every row equals B's dev row; the set of rows differing from arm A's dev row is exactly `movers-train.txt`; none missing or capped. THE SHIP RULE: no WORSE mover, no lost certified (optimal) cell |
| P4 | `runtime/run_walk.sh` (the ap5-g3 runtime gate, walk mode) | serial and solo under the lock; THROTTLE 0; counter identity on every (round, cell); B/A instructions:u within [0.995, 1.005] (the declared cost bound) |
| P5 | `mutations.sh` -> `mutation/` | control clean; each of Mt1 Mt2 Mt3 (trigger), Me1 Me2 (escalation), Mm1 Mm2 Mm3 Mm4 (matching and the scaled solve), `mutate.py`, KILLED by at least one test; tree restored |
| P6 | `install_smoke.sh` -> `install-smoke/` | both front ends and both export-contract checks rc 0 |

Any status worse than arm A on any leg fails the leg. Protocols: every leg `pgrep -a hven_bench_cell` empty first, under
`flock /tmp/box-build.lock` (the orchestrator holds it for the chain), `set -o pipefail`, `setsid -w`, FOREGROUND markers,
`MKL_NUM_THREADS=1`, `ctest -j4` under `setsid`; P2, P3, P4 serial, one process at a time (P3: 476 cell processes through `cell.sh`,
THREADS=1, core 2, CAP 300 s). P2 and P3 assert counters and statuses only; the wall-clock of no leg is quoted. Other agents share the
lock; waiting on it is expected.

## 5. Order, STOP and ON FAIL

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-sparse-equil-v4/legs/orchestrate.sh 16efab6248d81a28a033d520ca1b5909083f1ff7 51fd5c0bb644a26f4becf9d51cd2c897b5e5bd0b8ef2d65871a6629b409721dd e5d6650214be3c673c9bdf14d1ff22ccfca4f9e6f8762bfac16a60f6f142ac62 000c90854b3c84b81ddd7ea2b96886165e154fbf821961057ee3f716799b26ba 0181247dd047faa881c1b8d1216ae026f362ccef658dcd94615d68d32aab4e76 3897 > docs/notes/data/2026-11-m8-sparse-equil-v4/orchestrate.out 2>&1
```

The chain stops at the first failed leg: commit the evidence, report; no re-run, no criterion change after data; a successor
`DECLARATION-SEQ4-r2.md` after a fix. Then CI on all four lanes (`gh workflow run ci.yml --ref sparse-equil-v4`).

## 6. NOT RUN

HOLDOUT (never). The 57-cell census, the interior leg (the interior-point engine reaches neither the walk nor the SQP's KKT factors),
TSan. Any wall-clock measurement. The CRESC100.K pairing case (open; `diag/escalation.txt` rule B is an option for a later round).
