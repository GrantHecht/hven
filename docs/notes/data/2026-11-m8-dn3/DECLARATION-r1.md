# M8 DN-3 -- measured dense auto-selection (DECLARATION-r1)

This file is the successor of `DECLARATION.md` (r0, HELD, commit `cbd7b41a7`, never run, kept unedited beside this one).
It is committed before the first DN-3 leg and never edited afterwards. Criteria are never changed after data. A failed
or void leg is answered by `DECLARATION-r2.md`, this file and every output kept. Every path below is absolute or
relative to this directory; every invocation uses absolute paths.

## 0. Why a successor, and what changed from r0

r0 was written for the design's proposals and HELD until the settler ruled. The rulings (relayed to this round) differ
from r0's premises in the places below, so r0's criteria are re-derived here, on the final code head, before any data:

| ruling | r0's premise | this declaration |
|---|---|---|
| Q1 | P-A proposed | P-A ruled: T_SQP = 105, T_IPM = 167 (criterion T is confirmed by L1, below) |
| Q2 | per engine proposed | per engine ruled (A5 unchanged) |
| Q3 | IPM `kSparseFallback` + guard | settled by DN-3b: the IPM's kAuto inherits `kReport` (`set_qp_params` sets it whenever the policy is not kNever). No IPM routing work in DN-3 |
| Q4 | the pin to be implemented in DN-3 (R1) | landed in DN-3a (MKL `MklThreadScope(kDenseArmThreads)`; Accelerate's dense threading UNOBSERVED). Not re-proved here |
| Q5 | C283 + detector | landed in DN-3a (SQP per-solve dense checks) |
| Q6 | counter proposed | `dense_factorizations` approved on `SqpCounters` and on `IpmResult` (flat): implemented; R-c is reported |
| Q7 | opt-in proposed | kAuto ships accepted whatever A5 says; the default stays kNever unless A5 adopts and the I set passes |
| Q8 | a census mover from a small side factor was a STOP to be decided later | **ruled**: side factors are selected by their own n. Before any adoption leg an ENUMERATION leg (E) lists every roster cell, per engine, with any analyzed pattern of order <= T; those are the declared candidates and go through the adoption test; every other cell must be byte-identical to kNever |

Consequence of Q8 for criterion A (stated before data): a non-candidate cell is a tie by identity (its kAuto row equals
its kNever row in every printed key, E3) and is not measured against the 10 % bar; the instruction record leg (P) runs on
the candidates only. The stat leg (S) still runs on all 238 cells, and its non-candidate rows are reported. Everything
else in r0 §3 (the counter, the arms, the bar, the families, A5) is unchanged. r0's N set is replaced by the DN-3b-style
chain below (P1-P6), which is what the ruling for this round names; r0's N1, N3, N7 and N9 are NOT RUN (proportionate,
§7).

## 1. The code head and the arms

- Base `09718f3dd4f44997b956319b7e39f2d1025b8688` (origin/m8). Branch `m8-dn3`.
- **Code head `96ac1a27d085853ccc84326dfb36783fb63dab2e`** (one commit on the base). Evidence commits after it touch no
  source (`legs/chain.sh` refuses to start otherwise).
- Arm A: the base, rebuilt by `legs/prebuild.sh` from its archive export: `build-dn3/armAbin/hven_sqp_corpus` sha256
  `e53c29d22745fa6a3eb8b49e8b9199ad893957b9ab5ee69ce07ba5301d77a952`.
- Arm B: the code head, `build-dn3/rel` (Release, every target, `legs/build.sh`), frozen by `runtime/run_all.sh` to
  `build-dn3/armBbin/hven_sqp_corpus`, sha256 `3cb0aafba25eb7c310ef85863834819fc5d66459db342fa695c9e41ec0ba0c3f`. `build-dn3/dbg` (Debug, every target).
- Lane: the LOCAL hven-bench clone `/home/ghecht/Projects/hven-bench-dn1` (never pushed). `sl/adopt_setup.sh` built,
  before this declaration, branch `dn3-legs` from DN-1d r3's commit E `f8c54bb` + `sl/hb_patch_g.py` (DN-3a r2's SQP
  column, byte copy) + `sl/hb_patch_h.py` (DN-3b's five IPM columns, byte copy) + `sl/hb_patch_i.py` (new: the `auto-`
  label prefix = kAuto on both engines; `side.dn3_dense_factorizations` on both lanes), pinned to the code head:
  - STAT binary `build-dn3/adoptbin/hven_bench_cell` sha256 `3199459cbc87fe3438ec28d05945ad47a21c49fab09bf85f1ee4d30d8b3ac268`;
  - RECORD binary `build-dn3/adoptbin/hven_bench_cell.cfi` sha256 `20200e3fd610fa64c7a08baf6b121f773747df4626f8404e00635d499b44367f`: the stat binary's own link command with
    only `libhven.a` swapped for the CFI build's (`-fasynchronous-unwind-tables` appended by `sl/cfi_launcher.sh`, DN-0
    r1's); `sl/code_identity.py` CODE_IDENTITY_OK, 57 members, 6895 executable sections (7,069,355 bytes) compared, 0 differing; only `.eh_frame` / `.rela.eh_frame` differ; the two installs' header trees identical (`sl/setup/libhven-code-identity.txt`, `sl/setup/link.diff`).
  The clone's HEAD is put back on `dn1b-legs` (as found); `dn3-legs` stays as the record.
- Toolchain: clang 22.1.8, `HVEN_FP_MODE=SAFER_FAST`, Eigen 5.0.1, MKL 2026.2 (intel_thread), AMD Ryzen 7 5800X3D,
  Linux 7.2.12-200.fc44, perf 7.2.12. Apple/Accelerate and Windows: UNOBSERVED.

## 2. THE CHANGE (code head; lines at the head)

C1 `include/hven/detail/kkt/dense_selection.h` (new): `kSqpDenseSelectionThreshold = 105`,
   `kIpmDenseSelectionThreshold = 167`, `dense_selection_threshold(policy, t)` (t under kAuto, empty otherwise). No public
   name. Applied at the three places each engine configures its factors: `sqp_kkt_options()` (every walk and SSN
   `KktFactor`, the rank repair's Gram), `IpqpEngine`'s reconfigure, `IpmSolverImpl::set_qp_params()`.
C2 `validate()` (both engines) accepts kAuto (`enum_in_range` up to kAuto); `linear.max_dense_dimension` stays refused
   (reserved; kAuto uses the library's threshold). Doc comments: `IpmLinearOptions`, `SqpLinearOptions`, `DensePolicy`.
C3 `QpEngine::engine_fingerprint`: kAuto's threshold mixed into the hot-handle key (kNever's and kAlways' keys unchanged,
   pinned by `DenseAuto.TheWalkKeySeparatesThePolicies`).
C4 `BorderState`, `SsnEngine`, `QpEngine`: no default for `threads` or the policy (design R4); every test and bench
   construction now states `(0, kNever)`.
C5 `dense_factorizations` (Q6): `SqpCounters` (appended last, trace key appended; `docs/trace-schema-v0.md` row; both
   golden lines re-derived), `QpCounters`, `SsnResult`, `IpqpResult`, `RankRepair`, `IpmResult` (flat). Each is the factor
   layer's `dense_factorize_count + dense_singular_count` delta (factorizations whose arm was dense; shifted refactors
   included). `DenseTallyScope` carries it; the SQP folds it beside `dense_flagged_factorizations` at every site.
C6 Tests: `tests/drivers/test_dense_auto.cpp` (12), `WsRankRepair.TheGramFactorCountsItsDenseFactorization`; the two
   validation tests and `Limits.*` updated; the `SqpCounters` size pins (test_threads, test_callback) and the trace field
   count (92 -> 93).

## 3. Declared differences (everything not listed is EXACT IDENTITY at the defaults, DensePolicy::kNever)

D0 THE CHANGE. At kNever no dense session exists and every new branch reads empty.
D1 ENGINES AT kNever: NO MOVERS -- replay P2, interior P3 (unmoved and promoted), runtime counter identity P5, the
   enumeration leg's N rows (E1). ANY kNever MOVER IS A STOP.
D2 API: no new public option; new public field `SqpCounters::dense_factorizations` (appended) and
   `IpmResult::dense_factorizations`; kAuto accepted. TESTS: `3634` registered, `3632` run (the two DISABLED
   EqpRefinementAb tests excluded), Release and Debug.
D3 No iparm change, no IPARM-SURFACE label, no notices/ change.

## 4. Proofs and pass criteria (each its own script under `flock /tmp/box-build.lock`; `pgrep -a hven_bench_cell`
empty first, as its own command)

P1 ctest Release (`build-dn3/rel`) and Debug (`build-dn3/dbg`), -j4 under setsid: "100% tests passed, 0 tests failed
   out of 3632" in both (`legs/ctest.sh`; `suites/`).
P2 27-cell replay x (walk, ssn, ipm) at the defaults on `build-dn3/rel`: EXACT against
   `docs/notes/data/2026-10-m7-wx2-gates/gates/replay/head-*.csv` via `scripts/compare_replay.py`, 0 differences each
   (`legs/replay.sh`; `replay/`).
P3 interior: arm A vs arm B, 27 cells, EXACT 0 differences (unmoved); arm B under the promoted baseline's invocation
   against `bench/baselines/2026-11-m8-b7-ipm-leg/interior_baseline.csv` EXACT 0 differences (promoted)
   (`legs/interior.sh`; `interior/`).
P4 THE FALSIFIER (`legs/failproof.sh`; ten mutations of the code head's archive export, each site at its declared count
   or fail closed; Release; `hven_drivers_tests` 'DenseAuto.*:DenseArmEngines.*:Limits.*', `hven_sqp_tests'
   'WsRankRepair.*'; `failproof/`): FAILPROOF_AS_DECLARED -- the head runs 56 and fails none; each mutation fails
   EXACTLY its WANT set (measured on a dev run of the code head, disclosed in §8, copied into failproof.sh):
   The mutations (sites in `legs/failproof.sh`): `threshold-ignored` (dense_selection_threshold returns 2^40: kAuto
   behaves as kAlways), `ipm-sqp-threshold` / `ipqp-ipm-threshold` (each engine handed the other's threshold),
   `sqp-auto-refused` / `ipm-auto-refused` (validate() refuses kAuto again), `sqp-counter-off` (DenseTallyScope drops
   the dense delta), `ipm-counter-off` (the IPM's lifetime dense count reads 0), `ipqp-counter-off`, `gram-counter-off`,
   `fingerprint-no-threshold` (kAuto's key without its threshold).
   - `threshold-ignored` 8: DenseAuto.AKktFactorSelectsEachPatternByItsOwnOrder, DenseAuto.EachEngineHasItsOwnThreshold, DenseAuto.IpmAboveItsThresholdIsThePlatformSession, DenseAuto.SqpAboveItsThresholdIsThePlatformSession, DenseAuto.TheIpqpTierSelectsByTheSqpThreshold, DenseAuto.TheSsnTierSelectsByTheSqpThreshold, DenseAuto.TheWalkSelectsByTheSqpThreshold, WsRankRepair.TheGramFactorCountsItsDenseFactorization
   - `ipm-sqp-threshold` 1: DenseAuto.IpmBelowItsThresholdIsTheDenseArm
   - `ipqp-ipm-threshold` 1: DenseAuto.TheIpqpTierSelectsByTheSqpThreshold
   - `sqp-auto-refused` 4: DenseArmEngines.SqpValidateAcceptsEveryPolicyAndNoThreshold, DenseAuto.SqpAboveItsThresholdIsThePlatformSession, DenseAuto.SqpBelowItsThresholdIsTheDenseArm, Limits.SqpValidateRefusesTheNewCommonValues
   - `ipm-auto-refused` 6: DenseArmEngines.IpmValidateAcceptsEveryPolicyAndNoThreshold, DenseAuto.IpmAboveItsThresholdIsThePlatformSession, DenseAuto.IpmBelowItsThresholdIsTheDenseArm, DenseAuto.IpmCountsPerCall, DenseAuto.IpmCountsTheDenseArmsFactorizations, Limits.IpmValidateRefusesTheNewCommonValues
   - `sqp-counter-off` 5: DenseAuto.AKktFactorSelectsEachPatternByItsOwnOrder, DenseAuto.SqpAboveItsThresholdIsThePlatformSession, DenseAuto.SqpBelowItsThresholdIsTheDenseArm, DenseAuto.TheSsnTierSelectsByTheSqpThreshold, DenseAuto.TheWalkSelectsByTheSqpThreshold
   - `ipm-counter-off` 3: DenseAuto.IpmBelowItsThresholdIsTheDenseArm, DenseAuto.IpmCountsPerCall, DenseAuto.IpmCountsTheDenseArmsFactorizations
   - `ipqp-counter-off` 1: DenseAuto.TheIpqpTierSelectsByTheSqpThreshold
   - `gram-counter-off` 1: WsRankRepair.TheGramFactorCountsItsDenseFactorization
   - `fingerprint-no-threshold` 1: DenseAuto.TheWalkKeySeparatesThePolicies
P5 runtime gate (`runtime/`, its own `DECLARATION.txt`; arm A vs arm B at the defaults; modes interior, ipm, ssn, walk;
   27 cells; rounds 3, core 2, perf record period 400000, THROTTLE == 0 or VOID): VERDICT PASS -- counter identity in
   every mode (a difference is a STOP), filtered B/A in [0.995, 1.005] per mode. Run FIRST and alone.
P6 install smoke and export contract on both front ends (`legs/install_smoke.sh`, DN-1d r3's instrument; `install-smoke/`):
   every step rc 0.

## 5. Criterion T (L1)

T1-T4 as r0 §1, instrument `tools/threshold.py` -> `results/threshold.txt` (run inside the enumeration script before
its first cell). `matrices.csv.gz` must hash to `f204be2c2a444b8fcdda8081a53f5940e04fc8ab8cf858b4c257f6e9b53b078c` (else
S-T0); T_SQP must be 105 and T_IPM 167 (else S-T1). These values are already the code's (C1); a mismatch is a STOP, never
a code change.

## 6. Criterion E (the enumeration leg, Q8) and criterion A (adoption)

E0 Population: DN-0's 238 TRAIN cells, roster `docs/notes/data/2026-11-m8-dn0/raw/roster.tsv`, re-checked by
   `sl/roster.py` (DN-0's, the listing binary = the stat binary) against hven-bench's split-v1 and holdout guard; the
   re-check's output must equal the roster byte for byte. **HOLDOUT is never run** (S-H).
E1-E3 `sl/adopt_run.sh E`: per cell, `run` mode, `default` (N, kNever) then `auto-default` (A, kAuto), the STAT binary,
   THREADS=1 on core 2, CAP 300 s; `sl/enum_analyze.py` -> `enum/enum-analysis.txt`, `enum/candidates.tsv`.
   A cell is a CANDIDATE iff its A row reads `side.dn3_dense_factorizations > 0` (some analyzed pattern of order <= T was
   factorized on the dense arm; under kAuto a pattern at or below T takes every factorization densely, so a positive
   count is exactly "a factorized pattern of order <= T"). ENUM_PASS iff:
   - E1 every N row reads `side.dn3_dense_factorizations == 0`;
   - E2 every row on both arms carries the column and a status, none CAPPED;
   - E3 every NON-candidate's A row equals its N row in every key=value line except `options_label`.
   Expected (dev probe of the code head with these binaries, §8): **ENUM_PASS; 227 candidates -- IPM 113 of 119 (HS 22 of 22, small-dense 91 of 97; non-candidates ACOPP30, CRESC100, CRESC50, LAKES, S365, S365MOD), SQP 114 of 119 (HS 21 of 22, small-dense 93 of 97; non-candidates hs025, ACOPP30, LAKES, S365, S365MOD)**. A different candidate set is reported
   and argued (the leg is deterministic at THREADS=1); an E1 or E3 failure is a STOP.
A0-A6 as r0 §3 A0-A6, with Q8's consequence (§0): A1 arms `default` / `auto-default` of the one lane binary; A2 the counter
   `solver = raw x (1 - s_init - s_proc)`, `raw` the median of three `perf stat -x, -e instructions:u,cycles:u` rounds in
   the order N A A N N A after one untimed warm-up per arm (`sl/adopt_run.sh S`, every cell), `s_init` / `s_proc` from
   `perf record -e instructions:u -c PERIOD --call-graph dwarf,16384 --user-callchains -m 1M` on the RECORD binary
   (`sl/adopt_run.sh P`, candidates only, both arms; reader on core 4, the lane alone on core 2), PERIOD base
   `max(400009, T/8000) | 1`, `R = min(40, ceil(8000 base / T))` repeats at `(base (1000 + 7k) / 1000) | 1`, the init set
   and B_proc rule exactly r0's (`sl/adopt_attr.py`, `sl/attribute.py` and `sl/bucket_map.tsv` byte copies of DN-0's);
   A4 win / loss / tie as r0, a NON-candidate a tie by identity (E3); A5 per engine, per family (HS TRAIN, small-dense
   TRAIN): sign(wins - losses); **kAuto becomes E's default iff at least one of E's families has sign +1 and none -1**;
   A6 `sl/adopt_verdict.py` -> `results/verdict.txt`, recorded whether it adopts or not.
R-items (reported, never gating): r0's R-a, R-b, R-d (per candidate cell), and R-c (now available: the A row's
   `dn3_dense_factorizations` of `factorizations`); R-e (`sl/adopt_run.sh T`: the four median cells hs026, HS67 (IPM),
   hs043, HS84 (SQP) at the engines' default threads, `t0-` label prefix, MKL variables unset, unpinned; raw and solver
   instructions, both arms -- reported for each that is a candidate); R-f (`sl/adopt_run.sh G`: gdb entry counts of
   `MKL_Read_ThreadENV_Count_Cores` and `__kmp_serial_initialize` per process on the four median cells, both arms).

## 7. The flip (L4) and what is not run

If A5 adopts for an engine: the flip commit sets that engine's `dense_factorization` default to kAuto; then r0's I set
(I1 replay x3, I2 the 57-cell census, I3 iparm dump and interior, I4 counter identity on the 238 cells between the flip's
`default` label and the head's `auto-default`), declared in a successor (`DECLARATION-r2.md`) committed before its first
leg, on the flip commit. L5 (the macOS and Windows CI lanes on the flip commit) needs a push this round does not make:
it is the settler's. If neither engine adopts, no flip, no I set, kAuto ships opt-in (Q7).
NOT RUN (proportionate): N1 compile-flags diff (one uniform flag regime, no build-system change), N3 census at kNever
(P2, P3 and P5 prove kNever identity; the code head moves no kNever path), N7 objects diff, N9 the Accelerate syntax
sweep (no Accelerate source changed; the threshold and the counters are platform-neutral headers). The design's
pin-removal mutation is not in P4: the pin landed in DN-3a with its own falsifier.

## 8. Disclosures (dev, before this declaration; recorded so they are not repeated)

- Arm A was rebuilt (`legs/prebuild.sh`) and the lane (`sl/adopt_setup.sh`, branch `dn3-legs`) built before this file;
  `build-dn3/rel` and `build-dn3/dbg` were brought to the code head (`legs/build.sh`) before it. None is a leg.
- Dev suites at the code head less its last test (`WsRankRepair.TheGramFactorCountsItsDenseFactorization`, added after):
  Release and Debug "100% tests passed, 0 tests failed out of 3631" (scratch logs `build-dn3/dev-ctest-*.log`); the added
  test passes in a targeted run. Registered at the head: 3634 (`ctest -N`), two DISABLED.
- Dev falsifier of the code head (`legs/failproof.sh` with `TREES=build-dn3/fpdev`, evidence `build-dn3/fpdev-ev`,
  21:20-21:45): head 56 pass, 0 fail; every mutation killed; the WANT sets and HEAD_COUNT above are that run's.
- Dev enumeration probe (`sl/adopt_run.sh E` into `build-dn3/enumdev`, 21:48-21:53) with the declared STAT binary: the
  §6 expectation above is that run's. It ran L1 too (THRESHOLD_AS_DECLARED: SQP 2817 matrices, first rho >= 1 at
  n = 106, CRESC50 mat_0049 rho 1.582; IPM 2684 matrices, first at n = 168, LAKES; values known before r0). Its
  per-cell table shows both arms' status, iterations and factorizations for every cell: SEEN. No instruction count of
  any candidate was taken before this file.
- Instrument dry run (`build-dn3/instrdev`, `DEV_ROSTER`, 21:53): the S and P legs on ONE NON-candidate cell,
  hs-hven-sqp-hs025 (its two arms are byte-identical by E3, so it carries no adoption information): T = 5.17e6
  instructions, 40 repeats, 438 samples, s_init 0, s_proc 0.914, se 0.013, 0 LOST/THROTTLE. Two instrument changes
  followed, both before this file: `sl/adopt_attr.py`'s R-d now reads only samples NEITHER share claims and whose chain
  is truncated (no `main` frame, no frame naming hven or hven_bench); the first version also counted claimed
  process-start samples (0.886 on that cell), which r0's definition ("what symbol matching cannot claim") excludes. On
  that cell R-d is 0.0022 of sampled instructions.
- RISK, flagged before data, criterion unchanged: on the smallest cells s_proc dominates raw (0.91 on hs025) and its
  sampling standard error (~0.013 of raw) is then ~15 % of solver instructions, so the 10 % bar can sit inside sampling
  noise there. The verdict reads the declared counter regardless; the margin-in-SE column (R-a) shows where.
- `sl/cell.sh` differs from DN-3b's in one place: MODE record puts perf's reader on core 4 and the lane alone on core 2
  (DN-0 r1's split); `sl/roster.py` differs from DN-0's in one place: the listing binary is an argument.

## 9. STOP conditions

r0 §5's S-H, S-T0, S-T1, S-A0 to S-A5, S-I, S-P, S-P2 unchanged, plus:
| id | condition | then |
|---|---|---|
| S-N | any P1-P6 criterion fails, or any kNever mover | STOP; evidence committed; DECLARATION-r2 after a fix |
| S-E | E1 or E3 fails | STOP; settler, with the cell and the differing keys |
| S-A3' | `s_init` = 0 on every IPM candidate's N arm | VOID (the marker set matched nothing) |

## 10. Order

P5 alone first; then `legs/chain.sh` (P1, P2, P3, P4, P6); then `sl/adopt_run.sh E` (with L1 before its first cell),
`S`, `P`, `T`, `G`; then `sl/adopt_verdict.py`; then `sl/hb_restore.sh`:

```
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3/runtime/run_all.sh 3cb0aafba25eb7c310ef85863834819fc5d66459db342fa695c9e41ec0ba0c3f
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3/legs/chain.sh 96ac1a27d085853ccc84326dfb36783fb63dab2e 3cb0aafba25eb7c310ef85863834819fc5d66459db342fa695c9e41ec0ba0c3f
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3/sl/adopt_run.sh E /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3 3199459cbc87fe3438ec28d05945ad47a21c49fab09bf85f1ee4d30d8b3ac268 20200e3fd610fa64c7a08baf6b121f773747df4626f8404e00635d499b44367f
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3/sl/adopt_run.sh S /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3 3199459cbc87fe3438ec28d05945ad47a21c49fab09bf85f1ee4d30d8b3ac268 20200e3fd610fa64c7a08baf6b121f773747df4626f8404e00635d499b44367f
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3/sl/adopt_run.sh P /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3 3199459cbc87fe3438ec28d05945ad47a21c49fab09bf85f1ee4d30d8b3ac268 20200e3fd610fa64c7a08baf6b121f773747df4626f8404e00635d499b44367f
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3/sl/adopt_run.sh T /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3 3199459cbc87fe3438ec28d05945ad47a21c49fab09bf85f1ee4d30d8b3ac268 20200e3fd610fa64c7a08baf6b121f773747df4626f8404e00635d499b44367f
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3/sl/adopt_run.sh G /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3 3199459cbc87fe3438ec28d05945ad47a21c49fab09bf85f1ee4d30d8b3ac268 20200e3fd610fa64c7a08baf6b121f773747df4626f8404e00635d499b44367f
python3 /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3/sl/adopt_verdict.py /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3 > /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3/results/verdict.txt
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3/sl/hb_restore.sh
```
(`sl/adopt_run.sh` writes `roster.tsv`, `enum/`, `stat/`, `record/`, `threads/`, `gdb/` and `results/` under this
directory; perf scripts and per-repeat files stay in the git-ignored `build-dn3/adopt-scratch/`.)

ON FAIL: stop, commit the evidence, report; no re-run, no criterion change after data.
