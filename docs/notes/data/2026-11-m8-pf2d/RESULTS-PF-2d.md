# RESULTS-PF-2d (protocol in DECLARATION-PF-2d.md, -r1.md, -r2.md)

**Outcome in one paragraph.** L1 (first-solve reuse) is bit-identical and passes every gate; it is the shippable head (branch `pf2d`). L2 as specified
(re-read only for rho in (1e-6, 1e-4]) and L3 as specified (decide at the first solve when rho1 >= 1e-2) both fail existing tests on problems outside the eight f7
cells that SIZING-PF-2c measured, so they were withheld and run as evidence arms (F = L1+L3, G = L1+L2+L3). On the TRAIN set neither arm moves a status
(0 of 476 rows), but F moves the path on 13 rows and G on 22, and the unit-test failures are real counter-example evidence (section 3). Against the 1.15 bar the
shippable head (`newL`) meets tier A on 1 of 5 cells (n825), F on 1 of 5, G on 3 of 5 (n750 and n1000_corrupted stay above it).

Heads: `pf2d` = L1 `21ae792ce` + test + declarations/legs/evidence (final head in the hand-back); `pf2d-l3` (F, L1+L3: code `f3d69e8e9`, tip `b50e4c0c3`);
`pf2d-l2` (G, F + L2: code `336723ed1`, tip `cb54db83e`). Base pf2b `e8d2441a3` (code `ae2e6729f`). Instruments, machine, binaries: `BINARIES.txt`.

## 1. What each lever is and what it saved (backend detector solves per qp minor, designated solve; instructions in section 5)

| lever | bit-identical | detector solves/minor on tier-A neutral cells (prev -> newL / newF / newG) |
|---|---|---|
| L1 | yes (replay, TRAIN) | n1000 1.425 -> 1.223, n750 1.485 -> 1.273, n800 1.533 -> 1.330, n825 1.286 -> 1.115 (-0.17 to -0.21, as sized) |
| L3 (F) | no (13 TRAIN rows change path) | a further -0.22 to -0.28 (n1000 0.947, n750 1.013, n800 1.076, n825 0.834) |
| L2 (G) | no (22 rows) | a further -0.6 (n1000 0.346, n750 0.386, n800 0.474, n825 0.324) |

L1's accounting choice: `DepTest::solves` (hence `dep_test_solves`) still counts the reused solve, so every `QpCounters` value is identical and only the backend
tally falls; a reader comparing `dep_test_solves` with `backend_solves_detector` sees the saved solve as their difference (pinned by a test).

## 2. Gate (declaration section 3, as amended by -r1 and -r2)

| leg | result |
|---|---|
| P1 ctest, shippable head | rel 4044/4044 and dbg 4044/4044 (`gate/suites/`); the L1 commit alone 4043/4043 rel and dbg (`gate-L/suites/`) |
| P1 ctest, F (evidence) | rel and dbg 4046, **2 failed**: `QpE2WorkingSet.TheUndecidableBorderPingPongRetreatsOnceAndEndsOptimal`, `QpLatchReason.BritgasMainSubproblemHoldsItsConditioningLatch` (`gate-F/suites/`) |
| P1 ctest, G (evidence) | not run; the one failure observed is the ACOPR57 restoration-QP test of -r1 (kOptimal -> status 1) |
| P2 replay x3 (27 cells x walk/ssn/ipm, 75 columns) | **L vs A: 0 differences in all nine runs**; F vs A and G vs A: 0 differences in all nine runs each (the replay columns do not carry the dep counters; reported, not asserted) |
| P3 TRAIN 238 x D/K (476 rows, co-run four pinned cores, CAP 300 s) | **L vs A: 0 movers, 0 missing/capped, every key equal except the twelve `side.backend_solve*` keys** (instructions sqp L/A **0.999236**, D 0.999557, K 0.998992; ipm 0.999994). F vs A: **0 status movers**, 100 counter movers. G vs A: **0 status movers**, 100 counter movers |
| P4 mutations | L1 (R1-R5, ship tree): control clean, **5/5 killed**. L3 (T1-T6, tree F): 5 killed in the first run, **T3 (screen tests not excluded) survived** with the first test, killed (r1) after the screen fixture was given a mu at which rho1 clears the threshold. L2 (B1-B7, tree G): the first run's control was not clean (the saved-solve fixture read rho above the withheld band), re-run (r1) 6/6 killed and **B7 survived** (the first solve is kept above the band, behaviourally invisible), killed (r2) by a direct `has_first` assertion. Evidence under `gate/mutation-*` |
| P5 install smoke + export contract | default and clang front ends, rc 0 |
| CI | run **44918024347** on the pushed branch (head `5d2498b55`: the code and tests of the final head; later commits are docs and evidence only): **all four lanes green** (windows-clang-release, macos-clang-release, linux-clang-coverage, linux-clang-release); a re-dispatch at the final head is in the hand-back |

### TRAIN movers (F and G), cause

Every one of the 100 counter-moving rows moves `dep_test_solves` (the lever's purpose) and 93 move `dep_stall_admits` (L3: a skipped refinement reports the first
solve's stall, so tests that would have converged now count as admitted; e.g. HS003.K 0 -> 3). **13 rows (F) and 22 rows (G) move the path** (`factorizations`,
`qp_minor_iters`, `ws_exchanges`...); all keep their status. Notable path rows: HS99EXP.K (max_iter both; objective -4.2e7 -> -7.8e4, iterations 50 -> 1), MESH.K
(numerical_error both; objective -56.7 -> -5.8e5), MSS1.K (31 -> 9 iterations, objective -15.0 -> -3.0 under F), DEMBO7 D/K (iterations 15/17 -> 23/20, same optimum to
1e-9), NET1.K (13 -> 14 iterations, qp minors 784 -> 1116), SPANHYD D/K under G (numerical_error both, objective 9.5e9 -> 1.3e10), CRESC100.K (+26 minors). Full per-row
keys: `gate/train/movers-{F,G}.txt`. No row is capped or missing in any arm.

## 3. Why L2 and L3 are withheld (the counter-examples)

- **L2 vs the ACOPR57 restoration QP.** With the old rule logged, 68 of 72 re-reads certify dependent, at test rho up to **4.5e-4** (rho 1.05e-4, 1.31e-4, 1.51e-4,
  2.0e-4, 2.3e-4, 2.6e-4, 2.7e-4, 3.1e-4, 4.0e-4, 4.5e-4 all flip; log `diag/acopr57-rereads-old-rule.txt`). The (1e-6, 1e-4] band drops them and the
  walk ends status 1 (objective -16.36 vs -18.395, KKT stationarity 0.12). SIZING-PF-2c's 1.55e-5 is the f7 cells' bound only.
- **L3 vs a constructed border candidate.** `TheUndecidableBorderPingPong...`: first-solve rho1 = 0.064, refinement stalls at rho 1.2e-6 (UNDECIDABLE); L3 admits it
  at the first solve (`dep_undecidable` 1 -> 0). A scratch diagnostic (L1 + a log of rho1 against the refined verdict for every refining test; not committed) over
  the whole qp and qp-seam suites (15 296 refining tests): the only verdict change with rho1 >= 1e-2 is this one (the other 979 verdict changes, 814 to dependent and 165 to undecidable, all have rho1 < 1e-2, which L3 never
  touches; raw logs `diag/`, the scratch patch `diag/diag-ws_independence.patch`).
- **L3 vs BRITGAS qp0** (objective 0.24429 vs 0.24292, 167 vs 166 factorizations): the diagnostic shows no verdict change among its 102 skipped tests (35 would have
  converged: `stall_admit` flips); **the mechanism by which the path moves is UNOBSERVED.**

## 4. The PF-2 table (instructions:u per qp minor / U0; three rounds each, spread 0.000 %, identical status, factorizations, qp_minors and all twelve tally values per arm and cell; u0 rows equal the U0 baseline row)

| cell | prev (pf2b) | **newL (ship head)** | newF (L1+L3) | newG (+L2) | newL/prev |
|---|---|---|---|---|---|
| f7_n1000_path_neutral (A) | 1.281 | **1.253** | 1.211 | 1.123 | 0.978 |
| f7_n800_path_neutral (A) | 1.304 | **1.274** | 1.236 | 1.146 | 0.977 |
| f7_n825_path_neutral_control (A) | 1.163 | **1.140 (met)** | 1.100 | 1.031 | 0.981 |
| f7_n750_path_neutral_control (A) | 1.371 | **1.338** | 1.296 | 1.195 | 0.976 |
| f7_n1000_path_corrupted (A, setup hop incl.) | 1.308 | **1.288** | 1.249 | 1.192 | 0.985 |
| f7_n800_path_corrupted (B) | 1.302 | 1.281 | 1.244 | 1.182 | 0.984 |
| f7_n2000_path_activity (B) | 2.029 | 2.003 | 2.003 | 1.987 | 0.987 |
| f7_n2000_path_physics (B) | 2.404 | 2.275 | 2.275 | 1.919 | 0.946 |

Bar (<= 1.15 on tier A): newL meets 1 of 5, newF 1 of 5, newG 3 of 5 (n1000_neutral 1.123, n800 1.146, n825 1.031; n750 1.195 and n1000_corrupted 1.192 not). The
ratios compare per-minor cost on identical paths (minors equal in all four arms on every cell). The per-bucket solve table is in `measure/analysis.txt`; the detector
bucket is the only one that moves between arms, as designed; the polish (1.05-1.2 solves/minor on tier A) and the BPS border refinement (tier B) are untouched.

## 5. Open items

- Settler decision: L2 and L3 as specified do not hold outside the f7 cells. G reaches the bar on 3 of 5 tier-A cells and cuts TRAIN sqp instructions 14.7 % (F: 6.95 %, K-arm
  11.6 % and 22.7 %), with 0 status movers on TRAIN but path moves on 13-22 rows and existing tests failing (F: the two above; G: those plus the ACOPR57 test, G's ctest not run). A rule keyed on more than rho (the L2 band
  needs top >= 5e-4 on ACOPR57, which keeps most f7 re-reads) is the thing to design; none was tried here.
- BRITGAS qp0's path change under L3: mechanism UNOBSERVED.
- n750 and n1000_corrupted stay above 1.15 even in G; SIZING-PF-2c's remaining lever there is the polish predictor (a behaviour change, not built).
- `dep_stall_admits` under any first-solve decision counts a skip as a stall (declared); `dep_test_solves` under L1 is logical.
- Accelerate and Windows: CI only. HOLDOUT not run. `f7_n10000_path_physics` not run. The U0 solve tally is unobserved (no tally in the M6 tree).
- Scratch trees `.scratch/{armL,armF,armG,diag,measL,measD,measG}` and builds remain (git-ignored); `pf2d-l3` and `pf2d-l2` are local-or-pushed side branches.
