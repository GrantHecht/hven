> Copied verbatim from `.superpowers/` (the round's own report/review) on 2026-10-01; nothing below this preamble is edited.

# M7 W2 E2 — ROUND F8-1: the capped ACOPR118/ACOPP118 sub-QPs

Diagnostician/implementer report, 2026-10-01. Worktree
`/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9`, branch `e2fix8`, HEAD
`35986d1f`. The worktree is clean and nothing was committed. hven's main tree and hven-bench
were never written; hven-bench binaries were only built from `git archive` copies, with outputs
in scratch. Scratch is `.scratch/f81/`, and `STATE.md` there is current. IPARM was not touched,
no SNOPT material and no GPL/LGPL source was read, and Accelerate values are UNOBSERVED.
Every statement below is labelled **OBSERVED** (measured this round) or **INFERRED**.

## 0. The answer

- **STEP 1: ACOPR57 and READING6 stay optimal on the fix8 head.** No regression, so fix8 is not
  reopened (§2).
- **The capped QPs are not one mechanism. They fall into two classes, and both rest on the same
  root cause** (§3):
  - **Class C, a real working-set cycle** (ACOPR118 qp3, qp5). A drop is followed by a zero-step
    re-add of the same bound through the **stall-admit** path, then a moving full step. Over a
    period the objective goes up and down by the same amount. qp5 revisits an exact state
    (pass 2274 = pass 1940).
  - **Class S, a standstill that is not a cycle** (ACOPR118 qp4, qp6 and ACOPP118 qp0–qp3). The
    walk stays on one working set and takes full steps of 1e-8 to 1e-4 forever. The objective is
    constant to about 1e-12, and the iterate sits **off its own equality rows** (|Ae x − be|
    between 3e-5 and 1.4). Neither disjunct of the KKT-point test can fire.
  - **The common root:** the walk runs on **rank-deficient working sets** ([Ae; W] exactly
    deficient, smallest normalized singular value 1e-15 to 1e-17, measured by dense SVD) for most
    of each solve. On the border (Schur) path the EQP solves and the dependency tests on those
    faces return garbage: ρ between 0.3 and 9.4, which is impossible for a projection residual.
    **Stall admits** (59 of 92 deficit-raising passes), converged false "independent" verdicts,
    exchanges chosen from garbage coefficients, and two start repairs let exact dependencies in.
    That contradicts S34's corpus finding for this class.
- **Neither the zero-motion cap nor the anti-cycling arm can stop it** (§3d). Every class-C period
  contains a moving step, which calls `pt_clear`, so `pt_events` peaks at 17 (qp3) and 3 (qp5)
  against a cap of 1904. Class S
  releases nothing at all. The anti-cycling arms are `HVEN_TESTING`-only experiments, so the
  production build has none.
- **E2 off, the same eight QPs also cap, with different members** (§3e). The control arm also
  runs on rank-deficient sets on most passes.
- **All eight QPs have a solution** (§3f). Each is feasible (phase-1 LP minimum = 0) and the
  trust-region box is finite, so a minimizer exists. SLSQP gives local reference values.
- **Recommendation: candidate S2, "the standstill retreat"** (§4, §5). When the border-path walk
  stands still off its equality rows on a face whose inertia reads kWrong, it takes the existing
  BORDER RETREAT and arms the re-check. Results:

  | screen | result |
  |---|---|
  | capdumps | 7/8 kOptimal at the oracle objective |
  | ACOPR118 cell | **optimal**: 21 majors, f 129660.68370269175 |
  | other TRAIN cells (11) | field-for-field identical to base |
  | corpus | 0 of 102 rows move |
  | key battery | 39/39, HS stays 3342 |
  | U0 | byte-equal to base |
  | Release ctest | 2881/1; the one mover is a +1 factorization on an infeasible fixture, status and x unchanged |
  | ACOPP118 cell | still numerical_error |

  Seven other candidates were measured and rejected with numbers.

## 1. How it was run

- **Builds.** Scratch Release copies (`git archive` of `35986d1f` plus `eigen`/`fmt`) were
  patched by `tools/patch_f81.py`, `patch_f81_dep.py` and `patch_f81_hdr.py` into `src-T1` …
  `src-T9`. The patches add env-gated per-pass tracing (`F81_TRACE`, `F81_WSDUMP`) and the
  candidate toggles (`F81_STALL`, `F81_RECHECK`, `F81_STANDSTILL`, `F81_NULLSTEP`, `F81_ALG`).
  None of them is in the worktree. Each copy built `libhven.a` and the replay tool
  (`replay-T*/dump_replay`). Lane builds (`cell*/`) used `cellbuild*.sh`, a copy of fix8's
  recipe with the prefix named `install-ff02789b46a7`. Full tests+bench builds:
  `build-T5-full`, `build-T7-full`, `build-T9-full`.
- **Faithfulness (OBSERVED).** With every toggle unset, each patched replay reproduces fix8's
  cap replays exactly: the same minors, factorizations and f on all 8 dumps, in both arms. The
  base corpus equals fix8's H2 corpus in every column of all 102 rows. The base ACOPR118 cell
  equals fix8's `repro2` cell field-for-field, on three separate patched builds (T5, T6, T9).
  Base ctest is 2882/0.
- **Box protocol.** Every build, replay, test and cell leg ran inside
  `flock /tmp/box-build.lock` with no timeout. Each leg was preceded by a separate
  `pgrep -a hven_bench_cell` and printed PGREP_EMPTY / FOREGROUND_START / FOREGROUND_END inside
  the lock (`leg.sh`). Settings were `MKL_NUM_THREADS=1`, `taskset -c 2`, one solver at a time,
  and ctest `-j4` under `setsid`. Cells ran under `cellw.sh` with a 600 s cap on the cell's own
  pid; it never fired. Two deviations, both logged:
  - **One leg ran 698 s:** `f81-corpus-A-border`, two corpus lists in one hold inside a
    background chain. Later corpus legs were split per list.
  - **I stopped my own `chain10.sh` by its exact pid** (4054643) once its candidates were
    rejected, and let its in-flight leg finish (`logs/wallcap.log`).

  Nothing else was signalled. Gate-chain legs held the lock between mine; I waited on it and
  never broke it.
- **Independent checks** used numpy, scipy, HiGHS and Clarabel in a scratchpad venv, as an
  oracle only and never vendored (`tools/oracle2.py`, `tools/rankwalk.py`):
  - rank by dense SVD of row-normalized [Ae; Ai_W; E_pins];
  - Z'HZ on the face's null space;
  - a phase-1 LP;
  - SLSQP from the walk's point and from the phase-1 point.

## 2. STEP 1: ACOPR57 and READING6 on the fix8 head (OBSERVED)

The fix8 lane binary (`.scratch/e2fix8/bench-build`, tree `5a89c0e9` == `35986d1f`) was run as
the previous round ran it. Legs ran 23:01:33–23:01:37 on 09-30. Each cell was compared
field-by-field with its RETAKE9 `run-A-default` row (`step1/compare.txt`, `cmp_rt9.py`).

| cell | RT9 → fix8 | majors | minors | factorizations | objective | stationarity |
|---|---|---|---|---|---|---|
| ACOPR57 | optimal → **optimal** | 15 → 16 | 1890 → 2334 (+23 %) | 1335 → 1632 (+22 %) | 41737.786035180943 → …180819 (3e-15 rel) | 4.4e-8 → 1.5e-10 |
| READING6 | optimal → **optimal** | 8 → 8 | 519 → 520 | 688 → 690 | identical | identical |

- **ACOPR57:** 54 fields differ. Among them, `dep_pin_skips` goes 1144 → 0, one sub-QP is now
  certified infeasible and absorbed (`ledger_infeasible` 0 → 1), and `zero_motion_exhausted`
  goes 0 → 1.
- **READING6:** 8 fields differ, including undecidable 1 → 0 and deferred 1 → 0 (rule A).

**No status regression, so fix8 is not reopened.** ACOPR57's +22 % factorizations is a cost to
declare in the re-take.

## 3. STEP 2: the diagnosis

### 3a. Recurrence (S45): cycle or slow progress? (OBSERVED, `tools/recur.py`, `tools/table.py`)

The table covers the 8 capdumps (`.scratch/e2fix8/diag/capdump/*-pd0/`, E2 on, own cap), with
the per-pass trace from `tr3-on/`.

| QP | class | passes on a rank-deficient W | tail behaviour | exact (W, f) revisit | max `pt_events` |
|---|---|---|---|---|---|
| ACOPR118 qp3 | **C** | 4403/5300 | period ≈ 6–7: STEP (dx 8.161e-3) / DROP b222L or b218L at zero motion / zero-step re-ADD (stall admit) / STEP back; f 532.0093 ↔ 532.1904 | none (x 2-periodic to 1e-9 rel) | 17 |
| ACOPR118 qp5 | **C** | 35/5300 | DROP b220L / re-ADD b220L (stall admit, α 0) / DROP r502 / moving re-ADD r502 (α 6.9e-2) / STEP; f 77.336 ↔ 77.902 | **pass 2274 = pass 1940 (period 334)** | 3 |
| ACOPR118 qp4 | S | 117/5300 | STEP only, \|p\| ≈ 2e-7, f 27.07617 ± 1e-6 | trivial | 3 |
| ACOPR118 qp6 | S | 5174/5300 | STEP only, \|p\| ≈ 1e-7 | pass 5222 = pass 2381 | 7 |
| ACOPP118 qp0 | S | 4671/4710 | STEP, a few drops | none | 5 |
| ACOPP118 qp1 | S | 4710/4710 | STEP only, f constant to 1e-12 | pass 103 = pass 89 | 39 |
| ACOPP118 qp2 | S | 4685/4710 | STEP only, f constant | pass 142 = pass 60 | 0 |
| ACOPP118 qp3 | S | 475/4710 | slow decrease (6141 → 3922), then STEP standstill | pass 119 = pass 105 | 56 |

**S45's caution holds for six of the eight.** Class S is a standstill, not a cycle in the
working set. Only qp3 and qp5 cycle. In qp4, ACOPP qp0 and ACOPP qp3, f moves over the first
4× cap; the standstill points are not fixed points.

### 3b. The class C mechanism (OBSERVED, ACOPR118 qp3 tail, passes 5284–5299)

1. A full STEP (α = 1, no blocker, dx 8.161e-3) on W* (107 members) moves f from 532.0093 to
   532.1904.
2. At the new point the step is zero-motion (\|p\| 6e-10), so this is a "KKT point of W*". It
   **drops b222L** on a multiplier of 3.0e4.
3. The next pass's direction points back through b222. It is **re-added at α = 0**, and its
   dependency test **stalls**: ρ = 3.85e-2, kIndependent, `stall_admit` = 1. The point memory
   marks it exempt (`pt_events` 1 → 2).
4. A full STEP of the same 8.161e-3 on the same W* takes f back to 532.0093. The STEP moves
   x, so it calls `pt_clear` and `pt_events` returns to 0. The exemption is gone.
5. Zero motion again, this time with **b218L** dropped (multiplier 4.8e3). It is re-added by
   stall admit (ρ 3.909e-2), and the STEP flips back.

The two STEPs on the same W* go to two different points. Each point is zero-motion for the
EQP, and which one the walk reaches depends on the border stack's state after a drop/add pair.
INFERRED: this is consistent with W* being rank-deficient (§3c), where the EQP has no unique
answer along the deficient direction and the multipliers are r/μ-sized.

qp5 is the same pattern with b220L (stall admit, ρ 0.28) and r502 (a moving re-add, converged
ρ 1.19e-2). The r502 re-add is what produces the factorizations the cap replay saw there.

**Paths that do NOT take part (OBSERVED, action counts over all 8 runs):** the ride (0 RIDE),
drop/undo (0 UNDO), the post-probe restart, and the E2 repair (one REPAIR at pass 0 only). The
re-admitting rule is the **ordinary add with a stall-admit verdict**.

### 3c. The root: rank-deficient working sets on a broken border factor (OBSERVED)

`tools/rankwalk.py` rebuilds W at every pass by reverse-applying the trace's membership diffs
from the dumped final working set. It then takes rank([Ae; Ai_W; E_pins]) by SVD, on
row-normalized rows with tolerance 1e-10.

- **Every QP starts independent** (deficit 0, smallest singular value 1.5e-4 to 3.9e-3) and becomes
  exactly deficient (singular values 1e-15 to 1e-17) within 0 to 126 passes.
- Of the passes that **raise** the deficit, classified by the add site's own test:

  | route | passes |
  |---|---|
  | **stall admit** (kIndependent, `stall_admit`) | **59** |
  | kDependent → exchange that still raised the deficit | 17 |
  | converged false "independent" (ρ 4.2e-3, 1.1e-3 on exactly dependent pins) | 14 |
  | start REPAIR (ACOPP qp1, qp3: deficit 0 → 22 at pass 0) | 2 |

- The first deficiency comes from a **stall admit on an independent set** in qp3 (pass 15,
  b194L, ρ 2.39), qp5 (pass 55, ρ 0.32) and ACOPP qp0 (pass 39, ρ 0.84).
- **ρ > 1 is common.** Since ρ(v = 0) = ‖a_free‖/‖a‖ ≤ 1, any ρ > 1 means the solve's
  coefficients are worse than doing nothing.
- **This happens on the border path before any deficiency.** In qp3, passes 2–14, the EQP
  residual ratio is 2e-2 to 0.37 (it sits near 0.07, under the 1e-1 refusal), the equality
  residual grows from 0.03 to 0.41, and the tests on the same factor are already garbage.
  - The inertia verdict reads **kWrong on most passes** (5260/5300 in qp3; 7 of 8 QPs mostly
    kWrong).
  - The oracle's Z'HZ is **PD** on essentially every visited face: one W has an eigenvalue
    below −2δ (the start set), and the rest are ≥ 0.
  - INFERRED: the kWrong readings are the broken factor's, not the problem's.
- **The refactorize algebra solves 6 of the 8 capdumps** with production code and no toggle
  (`refac-base/`), the same six that every border-retreat candidate fixes. The two that stay
  capped there (ACOPP qp2, qp3) are an eliminated-path failure. In ACOPP qp2, converged tests
  read ρ 8e-3, 6.6e-5 and 3e-5 on exactly dependent pins.

**This contradicts S34** ("the stall class is truly independent"), for these QPs: exact
dependencies enter through stall admits, as measured by dense SVD.

### 3d. Why the zero-motion cap 2(n+mi) and the anti-cycling arm do not stop it

- **The cap (OBSERVED from traces, and from source at `qp_engine.cpp:769` and `pt_clear`).**
  `pt_events` counts releases and re-entries at one point and is cleared whenever the iterate
  moves by more than step_tol. Every class-C period contains a moving STEP of 8.161e-3 or
  2.53e-3, so `pt_events` never exceeds 3 in qp5 or 17 in qp3, against a cap of 2(344+608) =
  1904. Class S releases nothing, so `pt_events` stays at 0 and the cap is never armed.
- **The anti-cycling arm (OBSERVED, source).** EXPAND and the other M4 arms live entirely inside
  `#ifdef HVEN_TESTING` (`qp_anti_cycling_arm.h`, "nothing here ships"). Production has no
  anti-cycling rule beyond the point memory, and the moving step defeats that.

### 3e. E2 off (OBSERVED, `tr3-off/`, `logs/rankwalk-off.txt`)

All eight QPs cap with E2 off too, with **different members**:

- qp5 off is an exact period-2 zero-motion drop/re-add of b208L.
- ACOPP qp0 off is a period-2 cycle on b176L.
- The rest are STEP standstills.

The off arm also runs on rank-deficient sets on 1540 to 5285 passes per QP. It has no
independence guarantee, so it walks the same geometry.

### 3f. Do these QPs have a solution? (OBSERVED, `logs/oracle-on.txt`, `oracle-local-*.txt`, `oracle-S2.txt`)

**Every QP is feasible.** The phase-1 LP minimum l1 violation is 0 for all eight. The
trust-region box is finite (0.03125 to 2), so the feasible set is compact and **a minimizer
exists**.

The walk's final points violate their own equality rows by 5e-4 to 1.4. SLSQP, from two
starts, reaches feasible points (violation ≤ 3e-8):

| QP | SLSQP objective |
|---|---|
| ACOPP qp0 | 1155.788 |
| ACOPP qp1 | 1172.358 |
| ACOPP qp2 | 1350.088 |
| ACOPP qp3 | 1592.551 |
| ACOPR qp3–qp6 | ≈ 1e-4 (the two starts disagree in the 4th digit) |

The walk ends at 1759, 35472, 2963 and 3923 on the ACOPP QPs, and at 532, 27.1, 77.9 and 12.0
on the ACOPR QPs, all infeasible. **On the 7 QPs S2 solves, SLSQP started at hven's point
terminates successfully in 5 iterations without moving (\|Δx\| = 0).** hven's objectives
match SLSQP's two-start values to about 3e-8 relative on ACOPP, and are lower than SLSQP's on
ACOPR, where SLSQP had not converged.

## 4. STEP 3: the candidates (all scratch env toggles)

### Capdumps and cells

| arm | rule | capdumps opt/8 | ACOPR118 cell | ACOPP118 cell |
|---|---|---|---|---|
| base | production | 0 | error 15 maj | error 5 maj |
| A (= S30's retired R-D) | stalled test ⇒ kUndecidable | 6 | error 18 maj (zero_motion_exhausted 4) | **optimal** 6 maj |
| B | stalled and ρ > 1 ⇒ kUndecidable | 6 | **optimal** 26 maj | error 8 maj |
| A2 / B2 | A or B plus converged ρ > 1 ⇒ kUndecidable | 5 + qp4 → num_err | — | — |
| C | stall admit arms `recheck_pending` | 0 (qp4 → num_err) | — | — |
| refac | `ws_algebra = kRefactorize` (measurement) | 6 | error at major 1 | **optimal** |
| S | standstill off the rows ⇒ border retreat + recheck | 7 | **optimal** 21 maj | error 7 maj |
| T | a null full step (same W, Δq ≤ 1e-12 rel) counts as zero motion | 0 (ACOPP qp1/qp2 → num_err) | error = base | error 5 maj |
| S+T | both | 7 | optimal | error |
| **S2** | **S, only when the pass's inertia verdict is kWrong** | **7** | **optimal 21 maj** | error 7 maj |

### Regression screens

| arm | corpus (102 rows) | mini-TRAIN (8 fast cells) | key battery | U0 vs base | Release ctest |
|---|---|---|---|---|---|
| A | 3 opt → num per algebra (A0_YORKNET qp4, qp5, ACOPR57 qp4), ACOPR57 qp7 cap → num | **ACOPP57 opt → num** | 37/39 (HS 3342 → 3354, CrashBasis) | 0 diff | 22 failures |
| B | YORKNET qp4 opt → cap, ACOPR57 qp6 num → cap, qp7 cap → num, SAWPATH qp1 cap → num (eq 6e10) | **ACOPP57 opt → num** | 39/39 | 0 diff | 7 failures |
| S | 1 row (ACOPR57 qp1: opt → opt, 181 → 107 minors) | **ACOPP57 opt → num** | 39/39 | 0 diff | 7 failures |
| S+T | — | **ACOPP57 opt → num** | — | — | — |
| **S2** | **0 rows** | **all 8 identical field-for-field** | **39/39 (HS 3342)** | **0 diff, 0 within-gate (byte-equal)** | **2881/1** |

Notes on the screens:
- The mini-TRAIN set is ACOPP57, ACOPR57, DALLASM, READING6, SMBANK, ZAMB2-8, STEENBRB and
  STEENBRD. All of them are OBSERVED.
- STEENBRE, NET3 and STEENBRC were also run under S2. Each is field-for-field identical to base.
- `base9` ctest is 2882/0.

**Why S2 and not S (OBSERVED).**
- S retreats ACOPP57's solve 1 at pass 2. In that solve the base walk sits in a 569-pass
  full-step jitter at a 9.87e-6 equality residual, but the EQP residual ratio there is 1e-12
  and the **verdict is kOk**. The base walk escapes at pass 571 by luck: \|p\| falls below
  step_tol and a drop follows. Retreating instead sends the solve to an eliminated path that
  drifts (`eq_drift` 2687), and the cell fails.
- At every capdump trigger point the verdict is **kWrong**.
- Offline, an approximate kWrong-restricted trigger fires on 0 of 51 corpus dumps and 0 of 5
  ACOPP57 base solves. S2 then measured exactly that.

**S2's one ctest mover (OBSERVED, `logs/t7fix2-S2.log`).**
`QpEngineIndefinite.T7Fix2AFailedStartRepairDoesNotRefineThroughTheProbedFactorization`
(independent arm): factorizations go 4 → 5. Its status kInfeasible, x(0) = 0.5 and x(1) = 0
are unchanged. The fixture's rows e₀ = 0 and e₀ = 1 are inconsistent by construction, so the
walk stands still off a row on a kWrong face. S2 retreats and the eliminated pass costs one
factorization. This is a declarable counter mover, not a correctness change.

**S2's cost on ACOPR118 (OBSERVED).**

| | base | S2 |
|---|---|---|
| minors | 32929 | 16116 |
| factorizations | 15489 | 15430 |
| `dep_border_retreats` | 5 | 11 |
| wall (informational) | ≈ 198 s | ≈ 133 s |

## 5. Recommendation: S2, "THE STANDSTILL RETREAT"

**The rule, as measured.**
- **Scope:** `independent_working_set` on, `ws_algebra = kSchurBorder`, and the pass not
  already eliminated-only.
- **Trigger.** Either of these, on a pass whose main inertia verdict is **kWrong**:
  - (i) a zero-motion pass that starts with a **structural** equality-row violation. This is
    the walk's own `violation_is_structural` test in the equality-drift block.
  - (ii) two consecutive full-step passes (no blocker, α ≥ 1, no ride) that each start with
    one.
- **Action:** take **the border retreat**. Set `eliminated_only`, count it in
  `dep_border_retreats`, set `recheck_pending`, and run the rest of the solve on the
  bound-eliminated path. Once per solve by construction.
- **The option-off arm is untouched by construction:** the structural flag is computed only
  inside the independent-working-set block.

**Why it is principled (INFERRED from §3).** A walk that stands still while an equality row is
structurally violated cannot reach a KKT point on that face: the face is inconsistent or its
answer is wrong. kWrong on a face the oracle reads PD is the border factor's own failure
signature. The border retreat is the engine's existing answer to "the border path cannot be
trusted here" (E2-F1).

**What it does not fix.**
- **ACOPP118** stays numerical_error (7 majors, stationarity 25.1). Its capdump qp2 fails on the
  eliminated path too, where converged false-independents admit exact dependencies.
- **The admission defect itself:** stall admits and false independents of exact dependencies on
  these faces.
- Both are registered below.

**For the implementer, if the settler rules it in.**
1. The scratch counter for trigger (ii) is reset only by passes that reach the end of the loop
   body. Make it reset on every non-qualifying pass, then re-measure. INFERRED to change
   nothing measured: no capdump trigger had an intervening `continue`.
2. Give the trigger its own counter (for example `dep_standstill_retreats`) beside
   `dep_border_retreats`, and add it to F8-4's lane-column list.
3. Use the 8 capdumps as fixtures: 7 certify kOptimal and ACOPP qp2 stays capped.
4. Re-pin T7Fix2 4 → 5, with attribution.
5. Carry S45's riders:
   - **R9-1:** in `tests/qp/test_qp_walk_seam.cpp` near :795, add `kStartRepairRefused` to the
     `base == ws_size_out` / `Block::kNone` group.
   - **R9-2:** correct the comment in `tests/sqp/test_hs_sweeps.cpp` :1343–1346 to read
     1067 → 1069 in the SOC-on cells and 1062 → 1064 in the SOC-off cells.
6. **Not measured this round:** Debug suites, the long Debug test, the YORKNET and STEENBRG
   wall cells, the E2-off twin cells (identical by construction), and Accelerate (UNOBSERVED).

F8-2, the inertia-deficit repair budget, was not attempted.

## 6. Registered

- **F81-1 (the admission defect).** On ACOPR118 and ACOPP118 faces the border-path dependency
  test returns ρ > 1 on stalls and converged false-independents (ρ 1e-3 to 4e-3 on exactly
  dependent pins). It admits exact dependencies through four routes: stall admit, exchange from
  garbage coefficients, converged false-independent, and start repair (deficit 22 at pass 0).
  S34's "stall class truly independent" does not hold here (dense SVD). A test whose verdict is
  not amplified by ‖H‖ is a design question.
- **F81-2.** ACOPP118 and capdump ACOPP qp2: the eliminated path also admits exact dependencies.
- **F81-3.** ACOPR57 is +22 % factorizations on the fix8 head versus RT9 (status unchanged), for
  the re-take's declaration.
- **F81-4.** S30's R-D (arm A) re-measured: it fixes 6/8 capdumps but regresses ACOPP57 in-cell
  and 22 tests. Its retirement stands.

## 7. What I could not determine

- The outcome of any TRAIN cell not listed in §4 under S2: YORKNET, STEENBRG and the dnf_wall
  cells.
- Why ACOPP118's in-cell QPs still cap under S2. No in-cell dumps were taken under S2.
  INFERRED: the same eliminated-path admission seen in ACOPP qp2.
- Whether the kWrong readings come from the O(μ) eigenvalues of the dependent combinations or
  from Pardiso's perturbed pivots. The trace has no inertia triple.
- Wall times are single serial runs and informational only.

## 8. Evidence index (paths under `.scratch/f81/`)

| path | contents |
|---|---|
| `STATE.md` | running log |
| `step1/`, `cmp_rt9.py` | STEP 1 |
| `tools/patch_f81*.py`, `build.sh`, `trace.sh`, `leg.sh`, `cellw.sh`, `cellbuild*.sh` | instrumentation and runners |
| `tr-on/`, `tr2-*/`, `tr3-on/`, `tr3-off/` | per-pass traces with final ws dumps (`*_s0.ws`) |
| `tools/recur.py`, `tools/table.py`, `tools/rankwalk.py`, `logs/rankwalk-*.txt` | recurrence and rank |
| `tools/oracle2.py`, `logs/oracle-*.txt` | oracle |
| `togg-*/`, `logs/arm-*.log`, `refac-base/` | candidates on capdumps |
| `corpus/`, `tools/rowcmp.py`, `logs/rowcmp-*.txt` | corpus screen |
| `corptrace/` | corpus border traces for the separation analysis |
| `cells/`, `train/`, `diagcell/` | cells; ACOPP57 traces under base and S |
| `logs/key-*.log`, `logs/ctest-*.log`, `u0-*/`, `logs/t7fix2-*.log` | battery |
| `logs/wallcap.log` | own-pid stop of `chain10.sh` |

SIGNOFF: M7-W2-E2-F81-REPORT
