# WX-R1: the SQP driver's false kOptimal (NLP complementarity not gated at convergence)

Origin: `docs/notes/2026-10-m7-wx-ci-design.md` §4.6 / register WX-R1 (HIGH), settler ruling C149 (M7 correctness fix).
Branch `wx-r1` off `m7` `0483a46f`. Code commit `ae151b87`. Box: Linux, MKL 2026.2 (the 2026-10-27 toolchain), clang 22.1.8.
Accelerate was NOT run. Every statement below about the macOS lane is the lane log's (run 40868557422, job 123429421516) or is marked UNOBSERVED.

## 1. Mechanism, with citations (at `0483a46f`)

- **The convergence test** (`src/drivers/sqp_solver.cpp:7003-7005`):
  `stationarity <= kkt_tol && feasibility <= feas_tol && (!(duals_ingested || duals_carried) || complementarity <= kkt_tol)`.
  The complementarity conjunct was armed only while the multipliers were the ingested ones (`:4573`) or carried across a failed subproblem's own step (`:8325`); every QP or SOC commit disarmed it (`:8300-8301`, `:8333-8334`), and so did restoration (`:6022-6023`, `:6088-6089`).
- **Where complementarity is measured**: `evaluate_kkt_over_impl`, `max_j |lambda_i(j) cI_j(x)|` (`:716`), recorded in every history row and in `SqpResult::sqp_complementarity`, read by nothing but the conjunct above.
- **The argument for not gating it** (archived header prose, `docs/notes/2026-09-header-prose-archive.md:268-280`): the subproblem's own complementarity gives `|lambda_i(j) cI_j| = O(||lambda|| ||p||)`. That presumes the QP certifies `lambda_i(j) (Ji p + cI)_j = 0`.
- **Why that presumption fails (the QP-level half)**: the walk's KKT-point test (`src/qp/qp_engine.cpp:1237-1240`, `stationary_proper`) reads free-block stationarity plus `worst_structural_violation` (`:3217-3228`), which counts an inequality row only when it is VIOLATED (`v > 0.0`). A working inequality row strictly SLACK at the iterate passes, so the walk can certify a point off its own face: zero step, a positive price on a slack row. The terminal polish (`:4380-4475`) is the only thing that could re-solve that face and its acceptance can refuse.
- **B-1 before** (Phase-5 Task 7/7b, `tests/sqp/test_hs_sweeps.cpp:17-80`, `tests/sqp/test_b1_gate.cpp`): the same false-kOptimal signature from STALE INGESTED multipliers; repaired by clearing ingested prices on geometrically slack rows (`sqp_solver.cpp:4486-4500`) plus the ingest-scoped gate (W1). E4 round 0 re-opened the family through the regularized QP answer (`dual_mu * lambda^2`) and E4 fix1 closed it with the terminal polish (`docs/notes/data/2026-09-m7-w2-e4-proximal-eqp/REPORT-fix1.md` §0). Neither repair gated the term at convergence for QP-priced multipliers.

## 2. MKL reaches it (measured, not inferred)

`evidence/diag_wx_r1.cpp` runs the six-problem HS sweep corpus as warm chains over {walk, ssn, ipm} x warm_full_step x adaptive_mu x dp scaled by {1, .5, .25, 2, .3, .7} (4280 certified points) and self-checks every kOptimal point (`nlp_kkt_check.h`). At `0483a46f`, **6 false kOptimal points, all HS10 kWalk**:

| arm | p | majors | f returned | f* = -sqrt(1+p) | lambda_i | cI | complementarity |
|---|---|---|---|---|---|---|---|
| dp = 0.25 (fs on/off, adaptive_mu on/off: 4 arms) | 2.25 | 1 | -1.732050808 (the p = 2 optimum) | -1.802775638 | 0.288675 | -0.25 | 0.0722 |
| dp = 0.125 (fs on/off: 2 arms) | 2.375 | 1 | -1.802775638 | -1.837117 | | | 0.0347 |

The p = 2.25 trajectory (`evidence/diag_wx_r1_hist.cpp`): major 0 at the p = 2 optimum with the ingested price CLEARED (the B-1 repair works: stationarity 1, complementarity 0); its QP (linear objective, cleared price, so H = 0) returns kOptimal in 1 minor with a ZERO step and lambda_i = 1/(2 sqrt 3) on the row the parameter released by 0.25 (terminal polish entered and rejected, 1/0/1); the zero step is accepted; major 1 reads stationarity 3.6e-10, feasibility 0 and, with the conjunct disarmed by the re-price, returns kOptimal. The macOS lane's HS10 p = 3 point (dp = 0.5, complementarity 0.1336, objective gap 0.0646) is the same signature on the corpus's own grid; its trajectory is UNOBSERVED.

## 3. The fix (`ae151b87`)

- The complementarity conjunct is armed on every major, tolerance `kkt_tol` absolute (the W1 gate's own tolerance; stationarity's class).
- `duals_ingested` / `duals_carried` and their watchdog and rollback snapshot copies are removed: the conjunct was their only reader.
- The full-step watchdog ranks by `max(stationarity, feasibility, complementarity)`, the quantities the test gates (its own stated rule). `SqpKkt::residual()`, `kkt_residual` and every exported column are unchanged.
- Not changed: the QP engine (the slack-working-row certificate, §1, is registered as WX-R1b, not fixed here).

## 4. Every moved value, declared

| where | before | after | why |
|---|---|---|---|
| `B1Gate.TheWatchdogRestoreCarriesTheComplementarityGateWithTheMultipliers` (re-premised test) | kOptimal, 1 major, x = 0, f = 0 vs f* = -0.8, complementarity 0.8, no watchdog restore | kMaxIter, 60 majors, watchdog restore at major 6, complementarity 0.8 | the returned triple is the one its own major 0 refused (ingest gate); major 1 certified it because major 0's QP re-priced the SAME price on a row 8e-7 slack (inside the walk's row tolerance; the pinned 1e-7 radius cannot step past it). Not a genuine optimum under the gate's absolute tolerance. The solve now cannot move and says so. |
| `HsSweepRepair.WarmChainOnAQuarterGridCertifiesNoPointOffTheRow` (new) | fails on `0483a46f` on MKL: p = 2.25 majors 1, f = -sqrt(3), complementarity 0.0722 (`evidence/oldcode_newtests.txt`) | passes: 13/13 kOptimal, f = -sqrt(1+p) to 1e-6, complementarity < 1e-6 | the regression test |
| diag hunt (`evidence/sweep-diff.txt`) | 4280 certified, 6 false | 4280 certified, 0 false; 0 lost, 0 gained | 16 points moved: the 6 false ones now at f* (3-4 majors), and 10 downstream points on the two HS10 dp = 0.125 chains (p = 2.5..3, fs on and off) that now start from the true p = 2.375 optimum: same optima, f within 5e-8, majors 3 <-> 6 |
| `HsSweepCorpus.Corpus` and every other HS sweep pin on MKL | -- | unchanged | the corpus grid (dp = 0.5) never reached the slack-row zero step on MKL |
| suites, U0, E1 | -- | unchanged (below) | |

The diag hunt's "new" side was measured on the working tree before `clang-format` (whitespace only) and the commit.

## 5. Gates (`wrappers/run.sh` + `wrappers/legs.sh`, transcript `logs/chain.log`; flock, PGREP_EMPTY, MKL_NUM_THREADS=1, ctest -j4 under setsid, replay/E1 on `taskset -c 2`)

| leg | result |
|---|---|
| G3 Release suite (fresh build, bench ON) | **3231 / 3231 passed** (m7 3230 + the new test); skipped 1, disabled 2 |
| G1 Debug suite (long test excluded) | **3230 / 3230 passed** (m7 3229 + the new test); skipped 7 (the toolchain record's 7) |
| G2 long test alone (`Continuation.ProbeBudgetBoundsAFailingProposal`, Debug) | **1 / 1 passed** (167 s) |
| U0 replay, exact arm vs `2026-10-m7-toolchain-2026-10-27/replay/head-*.csv` | **0 / 0 / 0** differences (walk / ssn / ipm), `replay/exact-vs-toolchain-record.txt` |
| U0 replay, gated arm vs the M6 close pin (1e-5) | walk 17 / ssn 13 / ipm 13 = **43, the declared 43-set**; the transcript is text-identical to the toolchain record's `comparison-vs-m6-pin.txt` modulo the header lines |
| E1 OFF (`hven_sqp_ipqp_e1_arm --gate`) | **0 differences** vs the toolchain record's `e1/e1-off.csv` (29 cells x 68 columns) and vs `2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv` (29 x 65) |

No pinned value moved, so there is no declared break (CLAUDE.md §7) beyond the re-premised fixture in §4.

## 6. WX-R3 (scope check only)

Not the same mechanism. F9/T6 is an equality-only model (`x1 = 1`, `x1 = 1.001`, `mi = 0`), so NLP complementarity is identically 0 and the conjunct cannot matter; the failure is kMaxIter (no certificate at all), decided by how an exactly dependent row pair factors on Accelerate vs Pardiso's pivot perturbation. Adjacent, not shared: both show a ZERO STEP accepted with no response from the driver (WX-R3: the radius does not shrink; here: the re-premised B1 fixture now spins to the cap on its repeated zero step). Not fixed here.

## 7. Registered from this task

- **WX-R1b (HIGH, solver/QP)**: the walk certifies a point off its own working face -- `stationary_proper` (`qp_engine.cpp:1237-1240`) reads only the violated side of a working inequality row (`worst_structural_violation`, `:3223-3228`), so a strictly slack working row keeps a positive price with a zero step. This is the QP-level root of both MKL reproducers here (HS10 at H = 0; the B1 fixture at 8e-7 slack under a 1e-7 radius). The driver gate now refuses the certificate; the QP still produces it.
- **Zero-step spin**: a kOptimal QP with a zero step at a point the convergence test refuses is accepted every major without radius or working-set response (the re-premised B1 fixture: 60 majors at x = 0). Adjacent to WX-R3.
- **HS10 p = 3 on the macOS lane**: UNOBSERVED whether it now converges (as MKL's dp = 0.25 / 0.125 points do) or ends non-optimal; the lane run on this branch is its first observation. `HsSweepCorpus.Corpus` stays unrelaxed (ruling R-2).
- Docs: `docs/notes/2026-09-m6-w5-t6-ownership.md` and the header-prose archive still describe `duals_ingested` / `duals_carried` and the "measured but not gated" argument; historical, not edited.
