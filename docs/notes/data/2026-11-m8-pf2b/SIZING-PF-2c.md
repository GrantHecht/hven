# SIZING-PF-2c -- sizing the detector and polish remedies (measurement only; nothing implemented in the library)

Base: pf2b `e8d2441a3` (code `ae2e6729f`). Counts only; no timing claim. Cells and path as RESULTS-PF-2b (new arm = `measB`
= ae2e6729f + PF-12 levers, `HVEN_PF12_QP_FAILURE_BUDGET=1`, independent_working_set on, `--engine walk`, `MKL_NUM_THREADS=1`, one
box-lane lane, four pinned workers). Instrument: a SCRATCH copy of that tree (`.scratch/measC`, env-gated event log
`HVEN_SIZING_LOG`; patch in `sizing-pf2c/sizing-instrumentation.patch` + `sizing_log.h`; not applied to the library).
Identity check: for all 8 cells the instrumented run equals the PF-2b `new` row on status, factorizations, qp_minors and all twelve
tally values (so the path is unchanged and PF-2b's instruction numbers apply). Raw logs (12 MB) are in `.scratch/sz/` (git-ignored);
`summary.json`, `analysis1.txt`, `analysis2.txt`, `model.txt` are the derived outputs, `legs/` the scripts.
The corrupted cells' logs include the setup hop (log minors 17110 / 11873 vs designated 4762 / 2310); their per-minor numbers below
use designated minors as the denominator and the setup hop's solves in the numerator, matching how PF-2b's instruction ratio is built.

## 1. What the detector bucket actually is (tier A neutral/control cells; solves = backend calls)

| cell | minors | detector solves | = tests | + cert re-reads | + other (entry detector, probes, repair) | tests/minor | re-reads/minor |
|---|---|---|---|---|---|---|---|
| n1000_neutral | 12593 | 17949 | 6892 (3720 tests) | 8968 (2242 x 4) | 2089 | 0.295 | 0.178 |
| n800_neutral | 10822 | 16591 | 6983 (3477) | 7840 (1960 x 4) | 1768 | 0.321 | 0.181 |
| n825_control | 10589 | 13613 | 5804 (3120) | 7224 (1806 x 4) | 585 | 0.295 | 0.171 |
| n750_control | 10298 | 15295 | 5631 (3121) | 7340 (1835 x 4) | 2324 | 0.303 | 0.178 |

"Re-read" = the grey-band certificate re-read in `QpEngine::test_candidate` (R-E: a converged "independent" with rho < kDepUndecidable
(1e-3) is re-solved to the end of the refinement via `certificate_coefficients`). It is HALF the detector bucket (47-54 %), and it was
not named in RESULTS-PF-2b's recommendations.

Measured facts:
- **Candidates per minor: at most 1 on every cell** (histogram 0:...,1:..., max 1, 0 % of minors with >= 2; tier B likewise). 0.25-0.38
  tests per minor. **Batching candidates into one multi-RHS solve has nothing to batch** (rec. 2 is dead as stated).
- **The re-read's first solve is bit-identical to the test's own first solve** (same factor, same rhs; rho1 and res1 equal in
  2242/2242, 1960/1960, 1806/1806, 1835/1835 pairs): 1 of its 4 solves is a duplicate.
- **The re-read almost never changes the verdict.** Flips to dependent (rho_cert <= 1e-6, an upper bound on real flips because the
  rounding-floor test can only veto): n1000 2 of 2242; n800 19 of 1960; n825 1 of 1806; n750 23 of 1835 (corrupted n1000 1 of 2304,
  n800_corrupted 10 of 1638; tier B 0). **Every flip has a final test rho <= 1.6e-5** (max by cell: 3.1e-6, 1.55e-5, 2.0e-6, 1.08e-5; n800_corrupted 5.8e-6).
  Typical re-read inputs sit at rho 1e-4..1e-3 (89 % of re-reads are rho in [3e-4,1e-3), regularization-level rho ~ sqrt(delta)=1e-4 scale).
  The first-solve rho1 is NOT a safe screen for this (flips occur with rho1 up to 8.4e-3: refinement collapses rho); the test's final
  rho is (it is already known when the re-read decision is made).
- Test refinement (before the re-read): 2638/3720 (n1000), 2185/3477, 2193/3120, 2227/3121 tests need only the first solve. The rest
  (1082, 1292, 927, 894) run up to 3 refinement steps (4 solves for 1037, 999, 861, 793), costing 3172, 3506, 2684, 2510 extra solves.
  Of those refined tests 938/970/807/695 have rho1 in [0.1, 1) and ~100 more in [1, ...): clearly independent, never converged
  (stall admits). Excluding screen-positive tests (verdict forced dependent; their coefficients are used), refinement for rho1 >= 1e-2
  costs 3097, 2382, 2598, 2306 solves with **0 verdict changes** on all 6 cells that have such tests (rho1 >= 3e-3: 2-14 changes per cell, so 1e-2 is the edge).
  Counter-only side effect: 7-10 of those tests would have converged under full refinement (stall_admit flag flips) on tier A, 203/62 on the corrupted cells.
  Tier B: no such tests (136 / 617 tests, every one decided in 1 solve), and the tier-B detector bucket is 78-97 % "other"
  (entry detector, probes, repair): not touched by any lever here.

### Proposed band (re-read) and savings
**Re-read only when the test's converged rho is in (1e-6, 1e-4]; for rho > 1e-4 the first solve's "independent" stands.**
Margin: the largest flipping rho in 8 cells is 1.55e-5 (6x below 1e-4); flips lost with UB = 3e-5 / 1e-4 / 3e-4: 0 / 0 / 0 on every cell.
Re-reads left at UB = 1e-4: 23 (n1000), 29, 6, 31 per cell (vs 2242 / 1960 / 1806 / 1835). Optionally reuse the first solve in the
remaining re-reads (rho identical): 1 solve each.
Solves saved by cell (backend calls; L2 = band only; L1+L2 = band + first-solve reuse on the re-reads that remain; L3 = skip test
refinement for non-screen tests with rho1 >= 1e-2):

| cell | det/minor now | L2 (/minor) | L1+L2 | L3 | detector after L1+L2+L3 |
|---|---|---|---|---|---|
| n1000_neutral | 1.425 | 8876 (0.705) | 8899 (0.707) | 3097 (0.246) | 0.47 |
| n800_neutral | 1.533 | 7724 (0.714) | 7753 (0.716) | 2382 (0.220) | 0.60 |
| n825_control | 1.286 | 7200 (0.680) | 7206 (0.680) | 2598 (0.245) | 0.36 |
| n750_control | 1.485 | 7216 (0.701) | 7247 (0.704) | 2306 (0.224) | 0.56 |
(Remaining detector solves are the "other" column: 0.17, 0.16, 0.06, 0.23 per minor.)
L1 alone (no band) saves 0.17-0.18 per minor with no behaviour change at all (bit-identical duplicate solve).

## 2. E4 polish (instrumented: res0, |rhs|, |y|, |delta|, step outcome per polish step)

| cell | calls | first step kept | rejected by STEP CAP | rejected by strict decrease | kept rate of polish solves | kept steps |
|---|---|---|---|---|---|---|
| n1000_neutral | 12593 | 291 (2.3 %) | 12302 (97.7 %) | 0 | 4.5 % | 582 |
| n800_neutral | 10822 | 698 (6.4 %) | 10124 (93.6 %) | 0 | 12.1 % | 1391 |
| n825_control | 10589 | 341 (3.2 %) | 10248 (96.8 %) | 0 | 6.2 % | 682 |
| n750_control | 10298 | 668 (6.5 %) | 9630 (93.5 %) | 0 | 11.6 % | 1275 |
| n1000_corrupted* | 17110 | 3371 (19.7 %) | 13509 | 230 | 28.7 % | 5878 |
| n800_corrupted* | 11875 | 2047 (17.2 %) | 9497 | 331 | 25.9 % | 3605 |
| n2000_activity | 1883 | 1172 (62 %) | 708 | 3 | 67.4 % | 2060 |
| n2000_physics | 1642 | 1387 (84 %) | 254 | 1 | 86.1 % | 2609 |
(*log includes the setup hop.) Logged polish solves are fewer than the tally's polish calls on bordered faces: a bordered polish solve costs
about 2 backend calls (n1000: 12884 logged vs 14102 tally; tier B ~2.0x).

- **The first step is rejected by the kStepCap (|delta| > 1e-6 max(1,|y|)), not by "residual did not fall"**: strict-decrease rejections
  are 0 on tier A. The wasted solve is 94-98 % of first solves on tier A (0.94-0.98 polish solves per minor).
- **"Exact-system residual already within tolerance" does not predict a wasted solve.** res0/max(1,|rhs|) is 1e-14..1e-4 for rejected
  steps and 1e-16..1e-9 for kept steps; the two overlap. Skipping the first solve when res0/max(1,|rhs|) <= tau loses kept steps
  roughly in proportion: tau 1e-11 skips 35 % of calls and loses 248/291 (n1000) kept first steps; no tau separates them.
  The residual is NOT observable without code (the loop computes it, nothing records it); it was observed here only by the scratch patch.
  Minimal test-only instrumentation: an `HVEN_TESTING`-style gate logging `res_inf`, `|rhs0|`, `|y|`, `|delta|`, step outcome inside the two
  loops (`polish_eqp_core`, `polish_bordered_unregularized`), as in `sizing-instrumentation.patch`.
- Rules that DO work (sequence/amplification based):
  - free rule: skip the polish when res0 > 0.01 x cap (cap = 1e-6 max(1,|y|)): kept lost 0 of 291/698/341/668 (and 0 on all tier B);
    skips 11.6 % / 7.7 % / 9.8 % / 9.4 % of calls (0.13 / 0.09 / 0.10 / 0.11 solves/minor after the bordered factor). Small.
  - amplification predictor: remember the last observed delta/res0 (amp); skip when amp x res0 > 100 x cap. Skips 84 % / 74 % / 82 % / 76 %
    of first solves on tier A (0.92 / 0.82 / 0.83 / 0.86 backend solves/minor) and loses 11/291 (3.8 %), 29/698 (4.2 %), 13/341 (3.8 %),
    60/668 (9.0 %) of the kept first steps. With margin 10: loses 63/71/61/465 (n750 much worse). Tier B: loses 0 on activity/physics,
    213/3371 and 20/2047 on the corrupted cells.
  - consecutive-cap back-off (K=20, S=50): skips 68 / 63 / 67 / 64 %, loses 19/291, 18/698, 27/341, 46/668.
  Kept steps are real polish work (ruling E4-11): any rule that loses some changes answers by <= 1e-6 relative per step and may move the
  trajectory; it has to pass the counters gate (qp_minors, status) before it counts.
- Caveat on the polish figure: U0 had no polish, so removing it moves the cost below `off`, not just to it.

## 3. Instruction-ratio estimates (new/U0 per minor; Delta ratio = saved solves/minor x price per solve / U0 instr/minor)

Price per solve is not directly observed per bucket. Two bounds: implied = PF-2b's (new - off) instr / extra solves where the off arm took the
same path (n1000 1.66e7, n750 1.29e7; for n800/n825 the off arm's path differs, so the n750 clean value 1.29e7 is used), and average = new
instr/minor / total solves/minor (all instruction attributed to solves; a ceiling on the saving). Each estimate is "average-price
(optimistic) .. implied-price (pessimistic)". Cases: A = L2 band only; B = A + first-solve reuse; C = B + L3 (non-screen refinement skip at
rho1 >= 1e-2); D = C + free polish rule; E = C + amplification polish predictor (loses 4-9 % of kept first steps).

| cell | new/U0 | A | B | C | D | E |
|---|---|---|---|---|---|---|
| n1000_neutral (A) | 1.281 | 1.095..1.174 | 1.094..1.173 | 1.029..1.136 | 0.995..1.116 | 0.785..0.995 |
| n800_neutral (A) | 1.303 | 1.120..1.196 | 1.119..1.196 | 1.063..1.163 | 1.041..1.150 | 0.854..1.040 |
| n825_control (A) | 1.163 | 0.982..1.071 | 0.982..1.070 | 0.917..1.037 | 0.890..1.024 | 0.695..0.924 |
| n750_control (A) | 1.371 | 1.186..1.252 | 1.185..1.252 | 1.126..1.214 | 1.098..1.195 | 0.898..1.067 |
| n1000_corrupted (A) | 1.308 | 1.164..1.225 | 1.162..1.224 | 1.104..1.190 | 1.079..1.176 | 0.914..1.081 |
(price for n1000_corrupted taken from n1000_neutral; the setup hop makes this cell rough.) Tier B: not estimated (detector bucket there is mostly
entry-detector/probe solves, which no lever here touches; BPS border refinement dominates n800_corrupted).
E goes far below 1 at the optimistic price: that is the ceiling arithmetic (solves = all instruction), not a prediction; read the
pessimistic end.

Against the 1.15 bar (pessimistic end = PF-2b's own price method):
- n825: met by A alone (band only), in the whole price range.
- n1000_neutral: A 1.17 at the pessimistic price (not met), 1.095 central implied; C 1.136 met at the pessimistic price.
- n800_neutral: C 1.163 (misses by 0.013 pessimistic; met at the implied-of-n750 midpoint); D 1.150 borderline.
- n750: C 1.214, D 1.195 not met; only E (polish predictor) crosses, 1.067 pessimistic.
- n1000_corrupted: C 1.190, D 1.176 not met; E 1.081 met.
Untouched by any lever: the "other" detector solves (0.06-0.23/minor) and the BPS border refinement (+0.05-0.23 solves/minor on tier A).

## 4. Recommendation
1. Do first, behaviour-neutral: reuse the test's first solve inside the certificate re-read (L1): -0.17..0.18 solves/minor, 0 risk of verdict change.
2. Narrow the grey re-read to rho in (1e-6, 1e-4] (L2): the biggest lever (-0.68..0.71 solves/minor, ~half the detector bucket); flips on these 8 cells all
   sit at rho <= 1.6e-5; evidence base is 8 f7 cells only, so it must pass the TRAIN gate (0 movers) before shipping; other problems UNOBSERVED.
3. Skip test refinement for non-screen tests with rho1 >= 1e-2 (L3): another -0.22..0.25; 0 verdict changes at 1e-2; a stall_admit counter shifts on ~1 % of those tests.
4. After 1-3: n825 and n1000 clear the bar at the central price, n800 is borderline, n750 and n1000_corrupted do not (1.13-1.21). To close n750 the
   polish predictor (E) or an accepted-price decision is needed: a polish skip that loses 4-9 % of kept first steps is a behaviour change, not a free saving.
5. Batching the detector's candidates (rec. 2 of RESULTS-PF-2b) is dropped: 0 minors test more than one candidate.
6. A residual-tolerance skip for the polish (rec. 3 as worded) is dropped: the residual does not separate cap-rejected from kept steps (cap is the rejector, 94-98 %).
Not observed: Accelerate/Windows readings; any problem outside these 8 cells; wall-clock; effect of any lever on trajectory (all estimates assume an unchanged path,
which L3 and the polish rules may not preserve).
