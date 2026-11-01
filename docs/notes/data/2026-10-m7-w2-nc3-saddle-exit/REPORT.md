> Placed by the settler from the NC3 implementer's hand-back text (the harness refused the subagent's write to `.superpowers/`); body verbatim. Worktree `/home/ghecht/Projects/hven/.claude/worktrees/agent-a386d80a989ac109c`, branch `nc3`, base `baddb728`; commits `bd3155e7` (fixtures first) and `170ce2c9` (the mechanism); evidence in that worktree's `.scratch/nc3/` (STATE.md the running log).

# M7 W2 NC3: the saddle exit, report

**Summary.** The mechanism should ship.
- It converts the three elastic-QP saddles of STEENBRB, STEENBRD and STEENBRE. Each now ends kOptimal at a lower objective, and the reduced Hessian on the final face is positive definite (oracle plus an in-test dense check). Results are identical on both algebras. In the corpus it also converts Hj_STEENBRD qp1 and qp9.
- No optimal TRAIN cell is lost, U0 is exact, HS and the census are unmoved, and the option-off path is unchanged.
- There is no TRAIN conversion. STEENBRB/C/D/E now leave the saddle and reach feasible points, but none certifies.
- The brief's premise was partly wrong. BRITGAS's and STEENBRB's restoration "saddles" are LPs (H ≡ 0) already sitting at their optimum, with a false kWrong. There is no negative curvature to ride, so they stay failures.
- One false certificate was found during development and closed before commit (§3b).

## 1. Oracle on the 13 C10 dumps that exited at site 1335
The oracle is `qp_oracle.py` (scratchpad venv, never vendored); its outputs are in `.scratch/nc3/oracle/`.

| dump | stage | feasible | final face Z'HZ | verdict |
|---|---|---|---|---|
| STEENBRB qp2 | elastic, major 1 | yes | dim 36, min -12.3 | true saddle |
| STEENBRD qp1 | elastic, major 0 | yes | dim 35, min -250 | true saddle |
| STEENBRE qp1 | elastic, major 0 | yes | dim 35, min -250 | true saddle |
| STEENBRE qp2 | main phase, TR 0.1 | yes | min -249.9 = -4e-14 of ‖H‖ = 6.2e15 | below the engine's curvature resolution |
| STEENBRB qp3–qp8 | restoration | yes | H ≡ 0 | LP stopped at its Clarabel optimum (gap ≤ 3e-12); false kWrong |
| BRITGAS qp2–qp4 | restoration | yes | H ≡ 0 | LP at its optimum (gap ≤ 3.5e-10); false kWrong |

The saddle fixtures are in `tests/qp/test_qp_saddle_exit.cpp` (walk seam, both algebras). Each takes its premise from the dump's own result line (`status=3 site=1335`) and asserts:
- kOptimal strictly below the saddle's objective;
- the KKT battery;
- a positive-definite reduced Hessian on the final working set (dense QR of the free block, in the test);
- `saddle_exits ≥ 1`;
- the descent invariant on every saddle pass.

No objective is pinned: these QPs are nonconvex and have no global oracle.

| dump | saddle f | NC3 f | reduced-Hessian min | exits |
|---|---|---|---|---|
| STEENBRB qp2 | 1962909.61465 | 1962888.12239 | +2994.4 | 4 |
| STEENBRD qp1 | 1973701.4143 | 1973581.3643 | +15550.8 | 1 |
| STEENBRE qp1 | 4172746.15834 | 4172626.10834 | +24694.8 | 1 |

Two further fixtures assert that the exit does not ride (`saddle_exits == 0`, misses ≥ 1, never kOptimal): STEENBRB qp3 (the LP) and STEENBRE qp2 (curvature below resolution). A synthetic saddle reached without a drop (min -x0² + x1² - 0.1x0, rows |x0| ≤ 1, no bounds) still ends kNumericalError at (-0.05, 0) with the option off, and reaches a local minimizer with it on. In Debug every one of these tests is under 600 s (worst 268 s).

## 2. The four driver fixtures (`bd3155e7`) and the other tests that moved
`SqpDriverQpFailure` ×3 and `SqpDriverSoc.SocIsSkippedOnRoutedFailures` now take their premise from the option-off walk through `twin_saddle_premise(opts)`. That walk has no saddle exit, so the TwinSaddle subproblem still genuinely fails there. `RepeatedQpMaxIterStillPropagates` already pins its premise to the option-off walk the same way.

This is not a re-pin. On the base install, a scratch probe shows the ON and OFF walks giving identical driver trajectories row for row (`logs/probe-base.txt`), for 2 and 3 copies, failure budgets 1 and 3, and both algebras. So no pinned value changes. A premise built on the SuspectStall model was measured and rejected: it keeps failing down to radius 3 and never converges in 40 majors.

Added with the mechanism: `SqpDriverQpFailure.TheSaddleExitLeavesTheTwinSaddleWithoutAFailure`. On the default walk, with 2 and 3 copies and both algebras, it asserts kOptimal at f*, a first subproblem that solves, and zero QP failures.

The other 7 tests NC2 moved do not move here. NC2 also opened a ride after drops and at kSuspect exits; NC3 acts only at the trusted-kWrong dead end. Before the redesign, NC3 moved 7 Release tests: the 4 above plus three improvements.
- **`QpEngineIndefinite.PostProbeRestartIsOneShotPerSolve`:** the second dead end is now ridden off, so both minimizers are reached (f = -0.75). With the option on the test asserts kOptimal and `saddle_exits == 1`, which shows the second dead end was not a second restart. The old one-shot assertions are kept verbatim on the option-off walk.
- **`QpE2Fix8Budget` ×2:** the repair still stops on its budget and the pass is still spent (both still asserted). The walk then rides off the indefinite face to a vertex and certifies it (kOptimal, every coordinate at a bound). Before, it ended kNumericalError, which the fixture itself pinned as the budget's declared cost.

Not moved, unmodified and passing: HsBattery.*, HsSweepCorpus, IpqpDispatch, SqpDriverSsnMode, the T7 census.

## 3. The mechanism (`170ce2c9`)
It acts at the trusted-kWrong dead end, after the μ-free inertia re-read (R-C') and after the post-probe restart, and only with `independent_working_set` on.

**(1) Direction.**
- One factorization of the current face at dual_mu = 0, on the same bound-eliminated system the re-read uses.
- Lanczos with full reorthogonalisation on the inverse reduced Hessian through that factor, at most 24 steps. Among the negative Ritz pairs it picks the one with the most negative Rayleigh quotient on H itself.
- A direction is used only if verified: d'Hd ≤ -curvature_tol·d'd and A_W d ≈ 0. An inaccurate factor can therefore only cause a miss, never a false ride.
- The fresh μ-free factor makes the result algebra-independent: border and refactorize reach the same f on every dump.

**(2) Sign (`orient_saddle_direction`).**
- If the slope resolves, take the descent sign.
- If it is flat (the usual case at a stationary point), take the sign with the larger decrease to its first blocker. Ties go to the sign that leaves the constraint just released.
- A zero-length ride back into the just-released constraint is a miss.

**(3) Ride.** This is section 4c's ride, with `ride_direction` extracted unchanged from `ride_negative_curvature`.
- An independent blocker joins the working set; a zero-length ride is a zero-motion event.
- A dependent blocker goes to the add site's rule on the exit's own factor, with nothing re-armed.
- No blocker gives kNumericalError.

**(4) Certification.** Nothing is certified by the exit. Every later exit is the ordinary walk's.

**Counters and seam.** Counters `saddle_exits`, `saddle_exit_misses`, `saddle_direction_solves`. Seam fields `saddle_exit`, `saddle_curvature`, `saddle_slope`, `saddle_slope_tol`. The section 4c "THE SADDLE EXIT" text is in the header.

**What bounds it.** One factorization plus at most 24 operator applications per dead end. A zero-length ride falls inside the zero-motion cap, and everything falls inside the minor cap.

**Descent.** A ride never climbs a slope the arithmetic can read, and the d'Hd < 0 term makes it a decrease above a rounding-sized step. This is asserted on the seam for every saddle pass.

**Option off.** Untouched: the E2-off corpus moves 0/51 and U0 is exact.

**§3b. The false certificate, found and closed.**
- The first draft oriented the direction only by g'd ≤ 0.
- On three TwinSaddle copies with the border algebra, a dead end came right after a zero-multiplier probe drop. The draft rode back into the probed bound at zero length, and the probe's exemption then let the walk certify the saddle vertex: kOptimal at f = 0 against f* = -1.125.
- The sign rule closes this. Both algebras now reach -1.125, pinned by the new driver test.
- Still registered: the probe's per-solve exemption can certify a weakly-active saddle vertex that the walk returns to by any other route. NC3 removes this route; it does not prove no other exists.

## 4. Corpus (`.scratch/nc3/corpus/cmp-F9-M2.txt`, every column vs fix 9's F9)
- **E2 on:** 5 rows move per algebra.
  - Hj_STEENBRD qp1 goes num → opt (border f -19.6365098462, refactorize -20.3867098312: two different local minimizers).
  - Hj_STEENBRD qp9 goes num → opt (f -7901.18567092).
  - ACOPR57_def qp6/qp8 and STEENBRB_def qp1 move by factorizations +1 only: a miss whose cost is the search's own factorization.
- **Optimal count:** 15/51 → 17/51 on both algebras.
- **E2 off:** 0/51 rows moved.

## 5. Battery at `170ce2c9`
- **U0:** the declared 43-difference set (walk 17, ssn 13, ipm 13), identical to fix 9's. Against fix 9's head CSVs at gate 0: 0 differences on walk, ssn and ipm.
- **Key leg** (HS / T7 / B1Gate / HsSweepCorpus): 43/43.
- **Release:** 2901/2901 (fix 9's 2891 plus 10 added, 0 removed).
- **Debug:** 2900/2900, plus 2 already-disabled tests (`EqpRefinementAb.*`).
- **Long test:** `Continuation.ProbeBudgetBoundsAFailingProposal`, run alone, detached, waited on by exact pid: passed, 628 s.
- **clang-format:** 0 warnings.

## 6. TRAIN on the scratch lane
The lane is hven `170ce2c9` installed under the re-take 10 prefix name, with hven-bench `97dc81b` archived. Cap 300 s (BRITGAS 580 s). Rows are field-compared with re-take 10 by `.scratch/nc3/tools/cellcmp.py`.

| cell | re-take 10 | NC3 | fields differing |
|---|---|---|---|
| ACOPP57, ACOPR57, ACOPR118, DALLASM, READING6, SMBANK, ZAMB2-8 | optimal | optimal, identical | 0 each |
| ACOPP118 | numerical_error | identical | 0 |
| STEENBRB | numerical_error, 2 majors, f 20674, eq 1994 | numerical_error, 68 majors, f 49029.37, eq 2.3e-13, stat 27.9 | 62 |
| STEENBRD | numerical_error, 11 majors, f 1.66e11 | numerical_error, 26 majors, f 221775.46, eq 4.5e-13, stat 351.6 | 59 |
| STEENBRC / STEENBRE | numerical_error, 220 s / 100 s | hits the 300 s wall | — |
| NET3 | numerical_error at 184 s | hits the wall at 300 s and at 580 s | — |
| BRITGAS | hit the 300 s wall | numerical_error at major 1, 496 s; identical to the C10 base row except factorizations +3, eqp_polish_steps +2 (its three misses) | 4 |

No optimal cell is lost, so the STOP condition is not met.

Informational, solo, 580 s cap:
- STEENBRE: max_iter, 86 majors, f 27875.30, feasible (eq 1e-13), stat 0.0125.
- STEENBRC: max_iter, 86 majors, f 38554.85, feasible, stat 1.48.

**What happens after the saddle is exited.** The elastic QP no longer fails. STEENBRB and STEENBRD reach feasible iterates at objectives 5–6 decades below re-take 10's landing, then end on the QP-failure budget (`qp_failures_in_a_row_peak = 3`). STEENBRC and STEENBRE walk to near-KKT feasible points and then run out of iterations. The binding constraints are now R2 (how the driver answers a QP failure) and the remaining walk failures.

## 7. Re-take prediction (inferred)
- 0 TRAIN conversions.
- The 7 optimal cells and ACOPP118: identical (measured above).
- STEENBRB/D: same status; every other field moves (READ).
- STEENBRC/E and NET3: numerical_error → dnf_wall at 300 s, a status move on non-optimal cells, to be declared.
- BRITGAS: still dnf_wall.
- Schema event: three new QpCounters. The lane doesn't export them (hven-bench untouched); adding `harness_qpsum_` columns is the settler's call.

## 8. Registered
1. **The restoration-LP false kWrong** (all 9 restoration dumps, each at its optimum). On H ≡ 0 a first-order point is optimal. Fixing it needs a certificate rule in its own round; it is what still blocks STEENBRB's restoration.
2. **The probe-exemption return** (§3b, residual).
3. **STEENBRE qp2:** curvature below `curvature_tol` on a badly scaled landing.
4. **NET3 now exceeds 580 s** (re-take 10: 184 s). Not diagnosed; the lane doesn't show the saddle counters.
5. **Process deviations, declared:**
   - Two lock legs ran over 600 s: the Debug NC3 fixture leg (795 s) and Debug third 0 (812 s). Every individual test except the long one stayed under 600 s.
   - The harness's 2 h background limit stopped two chains: the battery during a Debug leg, and TRAIN during NET3. Both were re-run in smaller legs, and a pid check found no orphan process.
   - Accelerate: UNOBSERVED.

SIGNOFF: M7-W2-NC3-REPORT
