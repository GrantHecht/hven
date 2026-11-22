# RESULTS-PF-2e (protocol in DECLARATION-PF-2e.md and -r1.md)

**Outcome in one paragraph.** The state that separates "re-read/refinement needed" from "decided" is the refinement's CONTRACTION,
not rho: a rho that is regularization error falls geometrically under the mu-free refinement, a rho that is distance does not.
The rule built on it (the contraction probe, arm E2: stop a stagnant refinement once even contraction at 0.5 per remaining step could
not reach the verdict-changing reading, on the eliminated path only; plus L1b, the bit-identical resume from the test's final iterate)
passes every gate: ctest 4050/4050 rel and dbg, TRAIN 0 status movers, **0 path movers and 0 lost flips** on 476 rows, 9/9 mutations
killed, the control re-take of PF-2d's ship head reproduced exactly. It recovers **43-59 % of G's per-minor saving** on the tier-A
cells (F recovered 29-41 % and moved 13 TRAIN paths; G moved 22), but meets the 1.15 bar on 1 of 5 tier-A cells (n825, 1.076), the
same count as the ship head. On TRAIN its sqp instruction total falls 0.21 %. G's "14.7 %" was almost entirely path movement on four
failing rows (SPANHYD.K/D, MSS1.K, HS99EXP.K), not detector savings: G on its own path-stable rows saves 2.75 %.
The first form of the rule (both paths, arm E) lost four certified flips on NET1's border path and was withdrawn (-r1).

Heads: `pf2e` = PF-2d ship head `84a6e3373` + rule `334085b67` + tests `13d4c541d` + r1 rule `65a7856ac` + r1 tests `268a18f4c` +
docs/evidence. Base L = pf2d (code `21ae792ce`). Binaries: `BINARIES.txt`.

## 1. Diagnosis (scratch event log; `diag/`)

| case | what the log shows | the state that decides |
|---|---|---|
| ACOPR57 restoration QP (L2's counter-example) | 68 of 72 re-reads flip, test rho up to 4.5e-4; every flip contracts q = rho_{j+1}/rho_j = 0.001-0.08 per step (4.5e-4 -> 3e-6 -> 3.6e-8 -> 5.3e-10); the 4 non-flips read q >= 1.1 (`diag/greys-acopr57.txt`). Cancellation max_k v_k a_k / a is 10..8500 there, but not a mechanism (below) | contraction |
| ping-pong border candidate (L3's counter-example) | rho1 0.064 -> 1.1e-3 -> 4.6e-5 -> 1.25e-6, q 0.017-0.04, residual stalls at 1.1e-6 > target: UNDECIDABLE (`diag/pingpong-tests.log`) | contraction (and it is a border test: E2 never probes it) |
| BRITGAS qp0 (L3's UNOBSERVED mechanism, now observed) | a test with rho1 = 0.0137 contracts to 1.53e-6 in ONE step (q 1.1e-4), converges, and the grey re-read certifies it dependent (rho' 1.7e-12). L3 decided it independent at its first solve, so it never reached the re-read: a dependent candidate entered the set and the walk moved (objective 0.24429 vs 0.24292, 167 vs 166 factorizations). PF-2d's diagnostic compared the pre-re-read verdict and so missed it | contraction |
| NET1 (TRAIN; found by leg P2 on arm E) | border path: rho 0.50 -> 3.35e-6 (test converged) -> 3.40e-6 (q = 1.02: stagnant, residual 5.0e-9 -> 4.5e-9 stagnant too) -> 4.6e-11 (collapse); 4 certified flips per arm lost under E (`diag/net1-border-collapse.txt`) | the Schur-complement path's refinement is not a steady contraction: excluded in -r1 |

Rejected states (`diag/zero-solve-features.txt`, k = 0 grey re-reads, 420 flips vs 16 037 non-flips over suites, replay, f7, TRAIN-free logs):
rho (flips up to 9.6e-4, overlapping), cancellation level (flips from 0.99: a dependent candidate needs no cancellation, a = a_k
reads 1), ||v||_1, the converged solve's mu-free residual. No zero-solve feature separates; restoration/elastic context was not needed
(the ACOPR57 restoration flips separate by contraction like every other). So a k = 0 re-read (the test converged at its first solve)
still costs one probe solve; that is the remaining gap to G.

Scratch simulation of the final rule (E2) on the old-rule logs (`diag/sim-train-and-r1.txt`; TRAIN by a scratch lane binary, pin
`adf11f94c`, never pushed): TRAIN eliminated path 1 332 flips, 0 lost, 6 232 test cuts, 0 verdict changes; suites + replay + f7 +
fixtures likewise 0 / 0. Closest case (`diag/margin-r1.txt`): LAKES flips contracting steadily at q 0.33-0.34 (below the 0.5
threshold; a STEADY q >= 0.5 cannot reach kDepTol in the budget, which is what the reach encodes, so only acceleration could defeat it).

## 2. The rule (E2) and what it costs

- L1b (bit-identical): the grey re-read resumes from the test's final iterate (solve + k steps), not its first solve.
- Probe, eliminated path, non-screen: after a kept refinement step with q >= 0.5, with `left` budget steps remaining, stop when
  rho * 0.5^left > kDepTol (grey re-read; the "independent" stands) or >= kDepUndecidable (test; a stall admission). A test with k >= 1
  steps whose last step was stagnant needs no re-read solve at all. Bordered tests/re-reads, screen tests and the Farkas multipliers
  (certificate_coefficients) are untouched.
- Declared counter effects (observed on TRAIN: 42 counter-only movers): dep_test_solves falls, dep_stall_admits rises where a cut test
  would have converged, the backend detector tally falls. Two declared test-pin moves (section 2 of the declaration).

## 3. Gate

| leg | result |
|---|---|
| P1 ctest at E2 | rel **4050/4050**, dbg **4050/4050** (`gate/suites/`; 22 skips as pf2d) |
| P2 TRAIN on E (first rule) | 0 status movers, 0 missing (criterion met) but NET1 D/K path movers (same optimum, objective and stationarity byte-equal; qp minors 853 -> 787, 784 -> 841) with `dep_grey_reread_dependent` 4 -> 0: withdrawn (-r1) (`gate/train/`) |
| P2 TRAIN on E2 | **0 status movers, 0 path movers, 42 counter-only movers, 0 missing/capped, NO_LOST_FLIP**; L re-take = pf2d's L row for row (0 movers) (`gate/train-r1/`) |
| P3 PF-2 re-measure | determinism: 56 arm x cell rows identical over three rounds; CONTROL_OK (newL re-take = PF-2d newL on counters and tally, instructions within 0.0004 %); table below (`measure/analysis.txt`) |
| P4 mutations (rel tree) | control clean, **9/9 killed**: P1 (q threshold 0.01) by the ACOPR57 seam test; P2 (2.0) by 4; P3, P4, P5, P6, P7, P8, P9 each by named tests (`gate/mutation-E2.out`) |
| CI | run **44961522205** at `2e5ae778d` (code and tests of the final head; later commits are docs/evidence only): **all four lanes green** (windows-clang-release, macos-clang-release, linux-clang-coverage, linux-clang-release) |

### TRAIN instruction ratios (hven-sqp, 238 rows x D/K; co-run, instructions only)

| arm vs L | path movers | all rows | path-stable rows only |
|---|---|---|---|
| F (pf2d, L1+L3) | 13 | 0.9312 | 0.9846 |
| G (pf2d, +L2) | 22 | 0.8535 | 0.9725 |
| E (first rule) | 2 (NET1) | 0.9894 | 0.9893 |
| **E2 (ship candidate)** | **0** | **0.9979** | 0.9979 |

Path-mover quality reads (G; F's 13 are a subset of the same rows): 11 rows the same local solution (both optimal, objective equal to
1e-6 rel: ACOPP30, BT7, CRESC100.K, DEMBO7, DNIEPER, HS111LNP, NET1); 5 failing rows with the same objective (LAKES, MESH.D, MSS1.D);
5 failing rows that move to a different non-solution: HS99EXP.K (max_iter, f -4.2e7 -> -7.8e4), MESH.K (numerical_error, f -56.7 ->
-5.8e5, feasibility worse), MSS1.K (f -15.0 -> -0.88, feasibility better), SPANHYD.D (feasibility 8e-7 -> 1e-2, worse), SPANHYD.K
(f 8.4e9 -> 3.7e9). None of the 22 is a better or worse local SOLUTION; G's instruction saving lives on the five failing rows.

## 4. The PF-2 table (instructions:u per qp minor / U0; minors identical in every arm on every cell; u0, prev, newL, newF, newG carried from PF-2d)

| cell | prev | newL (ship head) | newF | newG | **newE2** | newE2/newL | share of G's saving |
|---|---|---|---|---|---|---|---|
| f7_n1000_path_neutral (A) | 1.281 | 1.253 | 1.211 | 1.123 | **1.191** | 0.950 | 48 % |
| f7_n800_path_neutral (A) | 1.304 | 1.274 | 1.236 | 1.146 | **1.213** | 0.952 | 48 % |
| f7_n825_path_neutral_control (A) | 1.163 | 1.140 | 1.100 | 1.031 | **1.076 (met)** | 0.944 | 59 % |
| f7_n750_path_neutral_control (A) | 1.371 | 1.338 | 1.296 | 1.195 | **1.277** | 0.954 | 43 % |
| f7_n1000_path_corrupted (A) | 1.308 | 1.288 | 1.249 | 1.192 | **1.234** | 0.958 | 57 % |
| f7_n800_path_corrupted (B) | 1.302 | 1.281 | 1.244 | 1.182 | 1.281 | 1.000 | 0 % (detector solves bordered) |
| f7_n2000_path_activity (B) | 2.029 | 2.003 | 2.003 | 1.987 | 2.003 | 1.000 | 0 % (bordered) |
| f7_n2000_path_physics (B) | 2.404 | 2.275 | 2.275 | 1.919 | 2.275 | 1.000 | 0 % (bordered) |

Detector solves per minor (tier A, newL -> newE2 -> newG): n1000 1.223 -> 0.771 -> 0.346; n800 1.330 -> 0.889 -> 0.474; n825 1.115 ->
0.633 -> 0.324; n750 1.273 -> 0.859 -> 0.386; n1000_corrupted 0.552 -> 0.445 -> 0.285. The gap to G is the one probe solve per k = 0
grey re-read plus the bordered detector solves.

## 5. Judgement

**Ship E2** (as a safe, path-neutral improvement on the L1 head), **and accept the remaining per-minor cost on tier A** for the
1.15 bar: no rule beyond rho reaches it safely on this evidence.

- What is safe: on TRAIN E2 moves no status, no path and no certified flip; it is bit-identical to L wherever its probe does not
  cut, and its cuts are verified outcome-neutral on 1 332 + 371 eliminated-path flips and ~14 000 test cuts. It is an evidence rule,
  not a certificate (the premise "a stagnant refinement stays stagnant" is measured, and it is FALSE on the border path, which is
  why E2 excludes that path); its edges are pinned by tests and mutations.
- What it buys: about half of G's per-minor saving on tier A (-0.054 to -0.064 in the ratio), 1 of 5 cells under the bar (unchanged
  count), 0.2 % of TRAIN sqp instructions. That is a real but modest gain for a small rule plus two test-pin moves.
- What it cannot buy: the rest of G's saving needs either a zero-solve decision for k = 0 grey re-reads (no feature separates; the
  dependent candidates include un-cancelled ones) or probing the border path (refuted by NET1). G's TRAIN 14.7 % is not a cost
  saving to recover: it is path change on five failing rows.
- If the settler prefers not to carry an evidence rule in the detector for a 4-6 % per-minor gain, the alternative is to keep L
  (L1 only) and accept the per-minor cost; the remaining lever named by SIZING-PF-2c for n750 and n1000_corrupted is the polish
  predictor, outside this task.

## 6. Open items / not run

- Accelerate and Windows: CI only (green, run 44961522205); no Accelerate value is quoted here. HOLDOUT not run. Replay not a leg here. `f7_n10000_path_physics` not run.
- The border path's stagnant-then-collapse refinement (NET1) is observed, not explained beyond "the Schur-complement solve's
  refinement is not a steady contraction"; its mechanism inside the Schur solve is UNOBSERVED.
- Scratch (git-ignored, not committed): `.scratch/diag` (diag tree), `.scratch/dlog` (36 MB raw event logs), the scratch lane binary
  `.scratch/bin/diag` (pin `adf11f94c`, its local branch deleted after use, never pushed), private hven-bench clone `.scratch/hb`.
