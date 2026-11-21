# RESULTS-PF-2b (protocol in DECLARATION-PF-2b.md and -r1.md)

Code: `a1bce3866` + `ae2e6729f` (the counters, fixtures, bench sidecar) on `origin/m8` `0ee37c2ca`; r1 changes one test file. Head: see
the hand-back. Raw: `measure/` (72 runs, `analysis.txt`), `gate/` (first chain), `gate-r1/`. Instruments as RESULTS-PF-12 (perf stat
`instructions:u`, one process per cell, three rounds, median; every arm/cell's three rounds identical on status, factorizations,
qp_minors and on all twelve tally values, instruction spread 0.000 %; U0 rows equal the U0 baseline row on all eight cells).
Wall is informational and not quoted.

## 1. The counters (step 1)

Twelve `SqpCounters` appended after `backend_symbolic_repeats` (trace keys, `dump_counters`, goldens, arity pins re-derived):
`backend_solves_{ordinary,detector,polish,border_refine}`, `backend_solve_rhs_*`, `backend_solve_refine_*` -- backend solve calls,
RHS columns and backend-reported refinement steps, split by the component that issued them (`hven::linear::SolveTally` +
`SolveContextScope`, recorded in the MKL/Accelerate/dense adapters). Placement is `SqpCounters`-only on PF-4's precedent, not
`QpCounters` (declaration section 1). Contexts: detector = everything issued inside `ws_independence.cpp`; polish =
`polish_eqp_core` + `polish_bordered_unregularized`; border_refine = `refine_bordered_solve_iterated` (first mandatory step
included); ordinary = the rest (EQP first solves and residual loops, eliminated-path model refinement, verdict replays' first
solve, SSN/IPQP/restoration).

Gate: P1 ctest rel and dbg 4042/4042; P2 replay x3 (27 cells x walk/ssn/ipm) A vs B **0 differences, all nine runs**; P3 TRAIN
238 x D/K, 476 rows, **0 movers, 0 missing/capped** (every key but the twelve new `side.*` equal), `TOTAL hven-sqp` instructions B/A
**1.000106**; P4 **26 of 26 mutations killed**, control clean; P5 install smoke and export contract pass (default and clang). First
CI (run 44880880598): linux-clang-release, linux-clang-coverage, windows-clang-release green; macos red on the new fixture only
(Accelerate reports 0 refinement steps -- the first observed Accelerate reading of the counters); fixed in r1 (test-only).
R1/CI: see section 5.

## 2. The PF-2 table now (instructions:u / qp_minors; U0 = M6 19f7674f6, new = ae2e6729f + PF-12 levers, off = same binary, independent_working_set off)

| cell | U0 | new | **new/U0** | RESULTS-PF-12 | off/U0 |
|---|---|---|---|---|---|
| f7_n1000_path_neutral (A) | 1.088e8 | 1.394e8 | **1.281** | 1.291 | 1.055 |
| f7_n800_path_neutral (A) | 8.610e7 | 1.122e8 | **1.304** | 1.302 | 1.128 |
| f7_n825_path_neutral_control (A) | 9.548e7 | 1.110e8 | **1.163** | 1.161 | 0.979 |
| f7_n750_path_neutral_control (A) | 7.607e7 | 1.043e8 | **1.371** | 1.376 | 1.094 |
| f7_n1000_path_corrupted (A, setup hop incl.) | 3.730e8 | 4.877e8 | **1.308** | 1.313 | 1.056 |
| f7_n800_path_corrupted (B) | 4.514e8 | 5.878e8 | 1.302 | 1.303 | 1.057 |
| f7_n2000_path_activity (B) | 1.865e8 | 3.785e8 | 2.029 | 2.033 | 1.575 |
| f7_n2000_path_physics (B) | 1.993e8 | 4.790e8 | 2.404 | 2.407 | 1.562 |

Bar (<= 1.15 on tier A): **NOT MET on 5 of 5** (best 1.163), unchanged from RESULTS-PF-12 to within 0.01: PF-4's batching and
I18's refinement change did not move the per-minor cost of these cells (PF-4 acts on the Schur border's K0 solves; the path cells'
excess is not there). The `off` column drops every tier-A cell to 0.98-1.13 (n1000 1.055 vs 1.067 in PF-12, n825 0.979 vs 1.008:
PF-4's saving shows in the arm without the bundle). Tier B cells carry a fixed setup cost in the numerator (context only; their
`off` arm is also above 1.5, so that part is not the bundle).
Path note: n800_neutral solves in 10.8k minors with the bundle and 31.2k without (U0: 30.2k), so its per-minor ratio compares
different paths; n825's `new` and `off` differ in minors by 1 %.

## 3. Solves per qp minor, by component (designated solve only; `off` = U0-equivalent work)

`off` is the nearest reproduction of U0's solve mix the tally can take (the U0 binary has no tally): no detector, no certificate, no
accuracy stop. It is NOT U0: the E4 polish (not gated by `independent_working_set`) runs in it, and U0 had none, so what `off`
shows as polish/ordinary is the floor the bundle sits on, not the M6 count.

| cell | arm | ordinary | detector | polish | border | total | refine steps/minor total |
|---|---|---|---|---|---|---|---|
| n1000_neutral | off | 2.038 | 0 | 1.123 | 0.195 | 3.356 | 6.71 |
| | new | 2.044 | **1.425** | 1.120 | 0.246 | 4.836 | 9.67 |
| n750_control | off | 2.052 | 0 | 1.213 | 0.299 | 3.564 | 7.13 |
| | new | 2.080 | **1.485** | 1.215 | 0.415 | 5.195 | 10.39 |
| n800_neutral | off | 2.017 | 0 | 1.115 | 0.067 | 3.198 | 6.40 |
| | new | 2.071 | **1.533** | 1.175 | 0.298 | 5.078 | 10.16 |
| n825_control | off | 2.087 | 0 | 1.233 | 0.400 | 3.720 | 7.44 |
| | new | 2.017 | **1.286** | 1.047 | 0.029 | 4.379 | 8.76 |
| n1000_corrupted | off | 2.081 | 0 | 1.798 | 0.485 | 4.364 | 8.73 |
| | new | 2.088 | **0.594** | 1.932 | 0.591 | 5.206 | 10.41 |
| n800_corrupted (B) | off | 2.394 | 0 | 2.690 | 2.000 | 7.085 | 14.17 |
| | new | 2.383 | 0.475 | 2.987 | **3.983** | 9.828 | 19.65 |

(Every solve here is one RHS column: `rhs` equals `solves` except in ordinary, where residual-loop and replay solves carry a few
more columns; Pardiso's reported refinement is 2 steps per call, so refinement steps/minor = 2 x solves/minor on these cells.)
Full per-cell tables including tier B: `measure/analysis.txt`.

## 4. Which component carries the excess

- **The E2 detector/screen carries it on the tier-A path cells.** Its solves are 1.29-1.53 per minor against 3.2-3.6 for the whole
  `off` solve mix; the other buckets move by <= 0.12 per minor except where the two arms took different paths (n825's polish and
  border fall, n800's border rises). Share of the extra solves (new minus off, sum of per-bucket deltas): detector 96 % (n1000
  neutral), 91 % (n750), 82 % (n800 neutral), 71 % (n1000 corrupted); border 3-13 %; polish ~0; ordinary ~0 (0-3 %).
- Pricing a solve at the cell's own (new - off)/(extra solves): detector = 0.22 of n1000_neutral's 0.28 excess (78 %), 0.25 of
  n750's 0.37 (68 %), 0.14 of n800_neutral's 0.30 (47 %). The remainder inside the bundle is the BPS bordered refinement (+0.05 to
  +0.23 solves/minor on tier A) and, on n825, path differences.
- **BPS bordered refinement** is the carrier on the tier-B corrupted-start cells: n800_corrupted's border bucket doubles, 2.0 -> 3.98
  solves per minor (72 % of its extra solves); n2000_activity +0.97 (40 %); its detector share there is 17 % / 68 % / 95 % (n2000
  physics: 4.5 detector solves per minor).
- **The E4 polish adds no solves relative to `off`** (it is in both arms), but it IS 1.05-1.2 solves per minor on tier A (a third of
  the whole mix): its first step's solve is issued on every EQP and rejected when the exact-system residual does not fall.
  That is the floor of the 0.05-0.13 `off`-arm excess (off/U0 1.055-1.128 on tier A) -- the part of the per-minor cost outside the
  bundle; U0 had no polish, so U0's solves per minor are not recoverable from `off`.
- Caveats: counts are backend calls; cost per call differs by cell (1.3e7 to 1.7e7 instr on n750/n1000, 8e6 on n800_neutral) and a
  detector call is a single-RHS solve on the (bordered or eliminated) factor like the others, so the solve-share is a fair
  instruction share where the paths agree; the Pardiso refinement sweeps (2/call) are inside the instruction numbers, not extra
  calls.

## 5. Recommendations (the settler decides; nothing implemented)

Targets: n825 (1.163) needs 0.013, n1000/n800 (1.28-1.31) about 0.13-0.16, n750 (1.371) 0.22.
1. **Detector: decide at the first solve (largest lever).** `dependency_test_to` refines until a step stops lowering the residual
   (at most kDepRefineSteps); on a candidate whose first-solve rho is clearly independent (>> kDepTol) or clearly dependent, stop
   after that solve; refine only inside an uncertainty band. Likewise skip the numeric probe when the integer screen is already
   decisive. Expected: detector solves/minor 1.3-1.5 -> about 0.7-1.0 if the first solve decides most candidates (counters
   `dep_*` in the lane rows give the band populations before building it): n1000/n750/n800 per-minor ratio down about 0.08-0.12 each
   (1.28 -> ~1.18, 1.37 -> ~1.26, 1.30 -> ~1.22), n825 to ~1.14 (met). Does NOT reach 1.15 on n750/n1000 alone.
2. **Detector: batch the candidates of one minor** (PF-4's precedent: one multi-RHS K0 solve costs 0.4-0.7 of k single solves)
   -- the detector's rhs equals its solves today, so each candidate is its own call; a minor testing several candidate rows
   against one factor could land them in one solve. Expected only where a minor tests k >= 2 candidates (not measured here: add the
   tests-per-minor population first); upper bound if every minor batched to one call: 1.3-1.5 -> 1 per minor.
3. **Polish: do not issue the first solve when the exact-system residual is already inside the answer's own tolerance**
   (`polish_eqp_core`/`polish_bordered_unregularized` compute `res` first and the strict-decrease rule then usually rejects the
   step, so the solve is paid for nothing on 1.05-1.2 calls/minor). Expected: saves most of 1.1 calls/minor, i.e. 0.10-0.15 of
   per-minor cost on tier A, but it is outside the bundle (also lowers `off`), changes trajectories only where polish steps are kept
   (`eqp_polish_steps`; its population on these cells was not measured here and must be before building), and is the single
   cheapest way to reach 1.15 on n1000/n800 when combined with 1.
4. **Bordered refinement: cap the certify sweeps** (tier B; also +0.05-0.23 on tier A): `refine_bordered_solve_iterated` with the
   independent working set's resolution/certificate runs two steps where the footprint rule ran one. Stop when the first step's
   backward error is already inside the certificate tolerance. Expected: n800_corrupted border 3.98 -> ~2.5-3, ~0.04-0.08 of its
   per-minor ratio; about 0.01-0.04 on tier-A neutral cells.
5. If after 1 + 3 the residual is 1.15-1.2 on n750/n1000, put it to the settler as an accepted price with these numbers: the
   bundle's per-minor cost buys the IWS conversions, and n800_neutral solves in a third of U0's minors.
Not built; every expected effect assumes the cell's path is unchanged, which 1 and 4 may not preserve (they change which steps run).

## 6. Open items
- Tests-per-minor and band populations of the detector (the `dep_*` counters are in the lane rows, not the corpus CSV; the
  corpus sidecar could carry them) -- needed to size recommendations 1 and 2; `eqp_polish_steps` kept-rate for 3.
- U0's own solve count is unobserved (the M6 tree has no tally); `off` is its proxy, and contains the E4 polish.
- Accelerate: refinement counters read 0 by construction (observed); solve counts/columns on Mac unobserved beyond the unit
  fixtures' pass. Windows: CI only.
- HOLDOUT not run; `f7_n10000_path_physics` not run; scratch worktrees `.scratch/{m6-19f7674f,armA,measB}` and builds remain
  (git-ignored).
