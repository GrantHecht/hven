# M7 W2 E2-D3: how the ladder declares feasible CUTEst problems infeasible (E2-F2)

Built at hven 763cda00. Its src/ and include/ differ from the re-take-2 pin 7ab88b81 only in
comments (`git diff` shows no non-comment `+`/`-` line). Each statement is marked **observed**
(from a run in this directory) or **inferred**. Line numbers refer to 763cda00. The diagnostic
implementer delivered this narrative as its hand-back (the harness refused a subagent-written
`.md`); the settler placed it here verbatim except for this preamble and the ruling at the end.

Files in this directory:
- `transcripts/`: one file per run, with the harness answer lines and the full `D3*` trace,
  plus the oracle outputs, `run-summary.txt` and `box-protocol-markers.txt`.
- `d3-instrumentation-and-experiments.patch.txt`: the scratch patch, env-gated.
- `harness-copy-xdump.patch.txt`: the scratch harness copy's x dump.
- `runners/`: the scripts that produced the runs.

## Verdict

"Infeasible" is never the verdict of restoration's own convergence test. Across the four cells,
`infeasibility_certified` = 0 and the certified exit (`sqp_solver.cpp:5261`) is never reached.

**Observed:** every false `infeasible` comes from one line. A restoration request that cannot
be serviced is spelled `SolveStatus::kInfeasible` (`sqp_solver.cpp:4648-4659`). That covers the
nested request inside the sub-driver and the once-per-solve cap in the parent. The parent passes
the spelling on unchanged (`:5270-5276`, "NO CLAIM is made") whatever the violation is. DALLASM
is declared infeasible at h = 3.6e-13 and SAWPATH at h = 0.

Behind the spelling, two things make the request unserviceable:
- **The start point lies outside its own box (new finding).** YORKNET's CUTEst x0 violates its
  bounds on 40 of 312 components (max 131.08), SAWPATH's on 2 of 583 (max 3.556).
  `project_start_point` is off by default, the restoration wrapper starts at the unclamped
  point, and h ignores the box.
- **The QP engine does not solve the elastic and restoration QPs**, which are feasible by
  construction. With E2 on they end `kNumericalError` through its terminations; with E2 off
  they end at `kMaxIter`.

The elastic-then-restoration ladder is correct in principle, and every QP certificate
re-derived here is correct.

## How it was run

- **Build:** a scratch Release of `git archive 763cda00` with the pinned submodules, in
  `.scratch/m7w2e2d3/tree`, with env-gated instrumentation: `HVEN_D3` traces; `HVEN_D3_DUMP`
  writes the point, box, c and J at restoration entry and exit; `HVEN_D3_F4` is E2-D1's F4
  routing with its exact condition; `HVEN_D3_PROJECT` forces the existing
  `project_start_point` lever; `HVEN_D3_ELASTIC_E2OFF` and `HVEN_D3_RESTO_E2OFF` put E2 off on
  the elastic rungs and on the restoration sub-driver.
- **Harness:** `hven_bench_cell` built from a `git archive` copy of hven-bench ea1aa4e. The
  copy's pin prefix points at the scratch install, plus one env-gated line that dumps the
  result x. The real harness tree and its build were untouched.
- **Faithfulness:** with no toggle set, the runs reproduce the re-take-2 default columns
  exactly: YORKNET `infeasible`, 1 major, 1309 factorizations, objective 110670.29950761795;
  DALLASM `numerical_error`, 4 majors, 8 factorizations; SAWPATH `infeasible`, 2 majors, 22344
  factorizations; STEENBRB `numerical_error`, 4 majors, 15038 factorizations,
  restoration_iters 11.
- **Oracle:** scipy 1.18 and HiGHS `linprog` in a scratchpad venv, never vendored. At a dumped
  point it computes: whether the ℓ1 violation measure (box included) has a linear descent; the
  smallest ℓ∞ radius at which the linearization is feasible; the distance to hven-ipm's final x,
  as an independent feasible point.
- **Feasibility of the problems:** committed four-lane rows show IPOPT and hven-ipm optimal on
  all four cells. SNOPT's committed rows are optimal on YORKNET, DALLASM, SAWPATH and STEENBRB.
- **Box protocol:** 25/25 legs logged PGREP_EMPTY inside `flock /tmp/box-build.lock`, with no
  timeout wrapper. One solver process per leg, `MKL_NUM_THREADS=1`, `taskset -c 2`.
- **Breach:** the `SAWPATH_proj` leg ran 12 min 20 s in the foreground, over the < 600 s rule.
  It was bounded by the failure budget and ended on its own; it was not killed.
- The main tree was never touched.

## Q1: YORKNET at production defaults (observed; `YORKNET_def.txt`)

1. **Start.** x0 lies outside the box on 40 components (max 131.08, sum 1550.8). It is not
   projected (`:2555`). h(x0) = 48 491.38: the ℓ1 norm of the 256 equality rows, which excludes
   the box.
2. **Major 0.** The QP (Δ = 1, n = 312, me = 256) ends with a verified `kInfeasible` in 7 minors.
3. **Elastic tier (`:6344`),** rung ρ = 100. The elastic QP is feasible by construction: it is
   centred on p_ref, the box projection of the origin (`soc_elastic_restoration.cpp:68`), and
   every violated row has a slack. It ends `kNumericalError` with `dep_blocker_terminations` = 1,
   after 242 minors and 247 factorizations. `usable` = 0 (`:1297`), so the driver requests
   restoration (`:6393`).
4. **Restoration** (full rows, since YORKNET's basis does not reduce; wrapper n = 824). The
   wrapper starts at the unclamped x0 (`soc_elastic_restoration.cpp:295`). Its note at `:291`
   ("feasible for this wrapper by construction") is false for an out-of-box entry. The
   sub-driver's first QP is box-infeasible at Δ = 1; it is certified `kInfeasible` after 594
   minors, correctly. Its elastic QP ends `kNumericalError` with `blk_term` = 1. The nested
   request is refused (`:4648`, `allow_restoration_` = false) and spelled `kInfeasible`
   (`:4655`). The sub-solve ends `kInfeasible` after **1 restoration major**, with its objective
   unchanged at 48 491.38 and reported feasibility 131.08 (the box gap).
5. **Parent.** h_r = h_in = 48 491.38 and |x_r − x|∞ = 0. `h_r > feas_tol`, so `:5270` sets
   `kInfeasible` with certified = 0.

**What set the status:** the nested-refusal spelling, passed up. Not a stationarity test, not
a budget and not a certificate.

**The declared point is x0 and is not violation-stationary** (`oracle_YORKNET.txt`). The
linearized ℓ1 violation falls at 3.17e4 per unit of ∞-norm step for every ε in [1e-6, 1], with
or without the box.

**A feasible point exists.** hven-ipm is optimal (primal eq 8.2e-7) 135.30 away in ∞-norm. The
linearization first becomes feasible at radius 135.30, which matches E2-D1's 135.

**The same status before E2:** the committed 0747050c rows are `infeasible` both with and
without E2, at 52 180 factorizations. E2 shortened the path; it did not create the outcome.

**With the start projected** (the projected point still needs radius 134.98):

| Variant | Elastic QP | Restoration QPs | End |
|---|---|---|---|
| `YORKNET_proj` | `blk_term` N3 | `kMaxIter` at the 7680 cap ×3, then N3 | `numerical_error` |
| `YORKNET_proj_el0` (elastic with E2 off) | `kMaxIter`, up to ρ = 1e8 | as above | `numerical_error` |
| `YORKNET_proj_el0_r0` (restoration with E2 off too) | as above | `kMaxIter` ×4 | `numerical_error` |

## Q2: the certificate, and the driver's response (observed)

**The certificates are right:** YORKNET radius 135.30 against Δ = 1; SAWPATH radius 3.56 against
Δ = 1 and Δ = 0.1. The certificates inside restoration are right too, because the wrapper's QP
cannot reach a box 131 away.

**Elastic is the right response in principle, and hven does it first.** Shrinking Δ cannot fix
an infeasible linearization, because a smaller box is a subset. Elastic relaxes the rows and
gives the best ℓ1 decrease inside Δ. Restoration is the fallback when elastic returns no usable
step. hven's order (`kInfeasible` → elastic → restoration; `kNumericalError` → shrink ×3,
E2-D1's D4) is correct.

**Why elastic handed over to restoration:** the elastic QP, a well-posed and feasible QP, was
not solved.

| Cell | Elastic QP outcome |
|---|---|
| YORKNET | `blk_term` = 1 |
| DALLASM | `blk_term` = 1 |
| SAWPATH | `dep_certificate_rejections` = 1, then N3: E2 tried a Farkas certificate on a feasible QP, verification refused it, and the solve ended N3 |
| STEENBRB | `kNumericalError` after 908 minors, with no E2 counter |

With E2 off on the rungs they end at `kMaxIter`.

**Two ladder facts that matter for the fix:** h ignores the box (`:535`) — SAWPATH's restoration
is entered at h = 0 and resumed as "feasible" (`:5100`) at the same point, still out of the
box; and an elastic step anchored at p_ref ignores Δ — in the E2-off YORKNET restoration, all
17 elastic steps are usable but have ‖p‖∞ = 131.08 whatever Δ is; all are rejected while Δ
shrinks to 7.6e-6.

## Q3: DALLASM and STEENBRB under F4

**DALLASM is reproduced: `infeasible` in 2 majors** (observed, `DALLASM_f4.txt`).
1. **Major 0.** The QP ends `kNumericalError` with `blk_term` (64 minors). F4 routes it to
   elastic, and the elastic QP ends with `blk_term` (194 minors), so restoration runs.
2. **Restoration succeeds** on the reduced rows (me = 150 of 151, `:4776-4785`). The sub-solve
   ends `kOptimal` in 2 majors: h goes from 12.456 to 3.64e-13, |Δx|∞ = 1.53. The parent resumes
   at Δ = 0.1.
3. **Major 1, at a feasible point.** The QP ends `kNumericalError` with `blk_term`, although it
   is feasible (oracle radius 0). F4 routes it to elastic. The elastic QP has no slacks
   (`viol_l1_in` = 0), so it is the same QP and fails the same way. The once-per-solve cap
   refuses the new restoration request (`:4648`) and spells it `kInfeasible` (`:4655`).
4. **What was declared infeasible:** a point with h = 3.64e-13 against `feas_tol` = 1e-6, inside
   the box. It is feasible, so stationarity is moot. hven-ipm's optimum is 216 away.
5. **With E2 off on the elastic rungs** the rung ends `kMaxIter`, with the same result.

**STEENBRB is not reproduced as `infeasible`** (observed). At 763cda00, F4 does not change the
run, because the major-0 QP is already a verified `kInfeasible`.
1. **Major 0.** The elastic QP ends `kNumericalError` (908 minors, no E2 counter).
2. **Restoration succeeds** on the reduced rows (me = 96): 11 majors, trust radius 1 → 1024,
   h from 19 800 to 1.5e-12, |Δx|∞ = 1201, and **f from 2858.8 to 2.97e12**. Restoration's ℓ1
   objective has no f term.
3. **Resumed at Δ = 0.1.** Two QPs cap at `kMaxIter` (4680), then one ends `kNumericalError`
   (not retryable), giving `numerical_error`.
4. **Before E2:** the same pattern (0747050c, f = 2.99e12).
5. **STEENBRC and STEENBRG are inferred to match, not traced** (their rows: feasibility
   4.5e-13, f ≈ 8.4e13).

E2-D1's "restoration declares STEENBRB infeasible" was made at 4a9b237a with its experimental
stack on. It does not hold at 763cda00.

## Q4: SAWPATH at defaults (observed)

1. **Start.** x0 lies outside the box on 2 components (max 3.556); h(x0) = **0**.
2. **Major 0.** The QP (Δ = 1) is certified in 5 minors. The oracle's radius is 3.56, so the
   certificate is correct and the cause is the box.
3. **Elastic.** The elastic QP is feasible by construction but ends N3 with a certificate
   rejection.
4. **Restoration, entered at h = 0.** The wrapper starts out of the box, so its QP is
   box-infeasible (certified after 2085 minors). Its elastic QP ends `kMaxIter` after 20 350
   minors, and the nested refusal gives the sub-solve `kInfeasible`.
5. **Resumed.** The parent resumes because h_r = 0: x has not moved and is still out of the box.
6. **Major 1** (Δ = 0.1). The same certificate, elastic N3 again, and the cap refuses
   restoration: **`infeasible`**.
7. **The declared point** is x0 (h = 0, box violation 3.56). It is not stationary for the
   box-inclusive measure (rate 1). hven-ipm's point is 4.87 away.
8. **Before E2:** `dnf_wall`. E2's certificate is what made it a fast `infeasible`.
9. **Projected** (`SAWPATH_proj`): the point still needs radius 3.56, because the box gap
   moves into the rows. The QP is certified again, elastic ends N3 with a certificate
   rejection, and the restoration QPs end N3 then `kMaxIter` ×3 at the 17 425 cap:
   `numerical_error`.

## Q5: the defects, with file:line

**(f1) Start box** — the root of the false status on YORKNET and SAWPATH; new, not in E2-D1.
- **Where:** `project_start_point` defaults off (`sqp_solver.cpp:2555`). The restoration
  wrapper starts at an unclamped x (`soc_elastic_restoration.cpp:291-295`).
  `constraint_violation_l1` ignores the box (`:535`), so a box-infeasible point counts as
  feasible for resume (`:5100`).
- **Fix (≈0.5–1 round, plus a census):** turn `project_start_point` on by default (already
  proposed as E5 in `2026-09-22-sqp-degeneracy-proposal.md`) and clamp the wrapper's start and
  centre. This moves 5 TRAIN starts: NET3, STEENBRD, STEENBRE, YORKNET, SAWPATH.

**(f2) Status spelling** — the proximate cause of every false `infeasible` here.
- **Where:** an unserviceable restoration request, nested or capped, is spelled `kInfeasible`
  (`:4648-4659`) and passed on (`:5270-5276`) whatever h is.
- **Fix (≈0.5 round, statuses only, no conversion):** spell it `kNumericalError` (or a stuck
  status), never `kInfeasible` at h ≤ `feas_tol`. `kInfeasible` stays reserved for the
  certified arm (`:5261`).

**(e) The QP engine on elastic and restoration QPs** — the cause behind all of them.
- **Symptom:** QPs that are feasible by construction, or at d = 0, end `kNumericalError` under
  E2 (terminations, or a certificate rejection followed by N3) and `kMaxIter` without E2.
- **Class:** E2-D1's D1/D3/D5, in the engine, not in the ladder.
- **Fix:** E2 fix round 2 and F5 (≈2–4 rounds; the number of rounds cannot be estimated from
  here). Fixtures should include the elastic QPs from YORKNET, SAWPATH and DALLASM.

**(a) Restoration's convergence test declaring infeasibility.** Not observed: the certified
arm was never reached.

**(b) A budget passing as infeasibility.** Yes, in part: the once-per-solve cap
(`st.resto.used`, `:4648`) spelled DALLASM `infeasible`. Its fix is f2.

**(c) Elastic not re-entering the normal tier.** Not reached: no elastic step was usable at
the defaults. Secondary defect (observed with E2 off only): at an out-of-box iterate the
elastic step, anchored at p_ref, ignores Δ, and all 17 steps are rejected as Δ shrinks to
7.6e-6. Fix: f1 removes the condition.

**(d) E1 or the basis hand-off.** Not implicated. Restoration uses the reduced rows
(`:4776-4785`) and succeeds on DALLASM and STEENBRB. `kFinishInconsistentEqualities` never
fired.

**Also found:** F4 as specified routes a failure at a feasible iterate into an elastic QP with
no slacks, which is the same QP (DALLASM major 1). F4 needs an h > `feas_tol` guard.

**Before F4 is safe:** f2 must land first. Otherwise F4 turns engine failures at feasible
points into false `infeasible`, as on DALLASM. F4 should also carry the h > `feas_tol` guard.
f1 does not interact with F4, but it is what YORKNET and SAWPATH hit at the defaults.

## Q6: which of the ten cells F4 plus E2-F2 (f1 + f2) would convert

**Prediction: 0 of 10.**

| Cell | Predicted outcome | Basis |
|---|---|---|
| YORKNET | `infeasible` → `numerical_error` | Observed: projected, elastic and restoration QPs fail in both E2 modes |
| DALLASM | no conversion | Observed: restoration succeeds, then the feasible-point QP fails in both modes |
| STEENBRB | no conversion | Observed: restoration succeeds (f 2.97e12), then the QP caps |
| STEENBRC, STEENBRG | no conversion | Inferred: same pattern in their rows |
| ACOPP57, ACOPP118 | no conversion | D5, the engine (E2-D1) |
| ACOPR57, ACOPR118 | no conversion | D3 in restoration QPs (E2-D1) |
| BRITGAS | no prediction | `dnf_wall`, not diagnosed |

SAWPATH, outside the ten, goes from `infeasible` to `numerical_error`.

The fixes make the statuses honest. Conversions depend on the engine solving elastic and
restoration QPs that are feasible by construction, and a feasible QP at a feasible point.
**Inferred:** once it does, restoration itself has been seen to reach feasibility in 2 majors
on DALLASM and 11 on STEENBRB.

## Settler ruling (2026-09-24)

The ladder is not the conversion blocker; the QP ENGINE on feasible QPs is. Registered:
**E2-D4** — dump the failing feasible QPs (YORKNET/SAWPATH/DALLASM elastic QPs; the restoration
QPs; DALLASM's major-1 QP) as fixtures and diagnose the walk's failure on each (the
degenerate-vertex no-admissible-member class; D5's μ-regularized inertia at a full face; the
certificate rejection on a feasible QP) — after E2 fix round 2's gates. **f1** (project the
start point; clamp the wrapper — proposal E5, now evidenced) and **f2** (never spell an
unserviceable restoration request `kInfeasible`) are ruled as a separate ladder round with F4
(with the h > feas_tol guard) after E2-D4's engine fixes; the E2-F2 label is retired in favour
of f1/f2. Every TRAIN prediction from here says 0 conversions until the engine solves feasible
QPs; that is the honest number.
