# RF-1 -- SQP failure handling: honest statuses, a retry ladder that changes something, a bounded restoration

Status: DESIGN, committed before any code of RF-1. Base: `pf2d` `84a6e3373`. Lands on `m8` before the M8 API lock: the
status, stop-reason and option changes below are API surface.

Evidence base (not re-derived here): the TU-1 pilot diagnosis of every SQP `numerical_error` row (scratch and per-QP
`DIAGQP` reproductions under `.scratch/tu1-diag/`; pilot store `hven-bench-tu1/.scratch/tu1-pilot/hven-sqp/`).
Every SQP `numerical_error` the pilot produced is one of three driver exits, and under each sits a walk QP that could not
finish:

| driver exit | site (`src/sqp/sqp_solver.cpp`, pf2d) | QP underneath |
|---|---|---|
| `subproblem_failure` | retry ladder ~9173-9264 | zero-motion exhaustion (`walk_engine.cpp` ~1742), dependent-blocker termination, stagnation stop (~1445-1500, 3236), bound/row certificate refusal (~1936), true minor cap, saddle-exit miss (~1663) |
| `restoration_refused` (`entry_rule` or `nested`) | `enter_restoration` ~6160 | as above, inside the main loop or inside the phase |
| `restoration_failed` | phase status switch ~7232 | as above, inside the phase |

`map_status` (~10087) folds `QpStatus::kIterationLimit` and `kNumericalError` alike into `SolveStatus::kNumericalError`,
and `core/solver_status.h` states that the SQP reaches none of `kAcceptable`, `kStalled`, `kDiverging`.

The three problem classes, by cell:

- **(A) The ladder replays.** In 9 of 13 reproduced cells the retry's counters equal the failed attempt's: the shrink
  did not cut the failed walk's reach, the seed is the failed working set, so the retry IS the failure. YORKNET (default):
  a stagnation-stop iterate stands off its box, so `qp_failure_is_retryable` refuses it and the first failure ends the
  solve. MSS2 (`sqp-qn`): a certificate ROW refusal at h <= feas_tol goes to the shrink ladder; bound refusals already go
  to elastic.
- **(B2) Restoration is fragile.** It has no work budget of its own (BRITGAS under `kRefactorize`: 84k factorizations at
  restoration h = 1.06e-5; capped QPs re-solved up to three more times at the full cap each); an elastic exhaustion
  inside the phase is a nested request and always refused; baseline BRITGAS survives only because it reached h = 9.97e-7
  against the 1e-6 resume gate. Whether SOC re-solves are capped separately from the main QP is UNOBSERVED.
- **(C) The status lies.** SMBANK `sqp-qn`, every arm: feasible to 5.8e-11, stationarity 1.5e-6..5e-6 against 1e-6,
  f = -7.1e6. Every rejection is an f-Armijo failure at the roundoff floor; the radius reaches its floor, a no-op
  restoration resets it, it collapses again, and the second request is refused by the entry rule. Reported
  `numerical_error`.

The work is three commits of behaviour, in this order, each with its own declared gate: **C** (reporting only), then
**A + B2** (path-moving). This note fixes the design of all three; each gate's declaration is a separate file committed
before its legs.

## 1. Statuses and stop reasons (one shape on both engines)

### 1.1 What the IPM already means

- `kAcceptable`: the iterate sits inside the acceptable tier (`termination.acceptable_{stationarity,equality,
  inequality,complementarity}_tol`, defaults 1e-2 / 1e-3 / 1e-3 / 1e-3) when the engine stops short of the convergence
  tier. The IPM arrives there two ways: `acceptable_iterations` consecutive acceptable iterates, or a stall at an
  acceptable iterate (`resolve_ipm_phase_status`: a stronger verdict outranks the stop reason). Stop reason `kNone`:
  "a converged verdict names its own cause" (`to_stop_reason`).
- `kStalled`: a stage stopped making progress (`StopReason::kStageStalled`).
- `return_best_applies` treats `kAcceptable`, `kStalled`, `kNumericalError` identically (only under `kOnAnyNonOptimal`),
  so moving an exit among these three never changes which point is returned.

### 1.2 What the SQP will mean (owner ruling: one shape across both solvers)

- **`kAcceptable` (new on the SQP)**: a FAILURE exit -- one that today reports `kNumericalError` with stop reason
  `kSubproblemFailure`, `kRestorationRefused`, `kRestorationFailed`, `kRestorationNoClaim`, or the new `kStalled` exits of
  1.3 -- at a returned point that passes the acceptable tier. Stop reason `kNone`, as on the IPM. The SQP has no
  consecutive-iterate rule: it never stops EARLY at an acceptable point; it only names honestly a point it already
  stopped at. `kIterationLimit`, `kWorkLimit`, `kTimeLimit`, `kInterrupted`, `kInfeasible` and the non-finite exits are
  never relabelled (the status names a cause the caller acts on differently: raise a limit, read a certificate, fix the
  model).
- **`kStalled` (new on the SQP)**: the engine stopped because it could not make progress, not because arithmetic failed
  and not at an acceptable point. Reached by the QP minor-cap exit (1.3) and the trust-region floor exit (section 3.4).
- **`kNumericalError`** keeps the remaining failure exits: a QP that ended `QpStatus::kNumericalError` and was not
  rescued, a refused or failed restoration at a point outside the acceptable tier, and the non-finite exits.
- **`kDiverging`** stays unreachable on the SQP (stated, unchanged).

**The acceptable test** (one function, `acceptable_point`, read where the exit is finished, never on the iterate path):

```
kkt.finite && stash.measured && stash.duals_measured
  && kkt.stationarity    <= termination.acceptable_stationarity_tol     // the convergence test's own (engine) measure
  && kkt.complementarity <= termination.acceptable_complementarity_tol  // likewise (WX-R1 NLP complementarity)
  && stash.d.feasibility_e <= termination.acceptable_equality_tol       // the declared (caller-unit) residuals the
  && stash.d.feasibility_i <= termination.acceptable_inequality_tol     //   result reports, box inside the inequality half
```

Stationarity and complementarity are read as the convergence test reads them (the engine's measure, on the SCL argument
that a scaled solve's stationarity is the meaningful one); feasibility is read in caller units, on the SCL guard's
argument that an accepted point must be feasible for the problem the caller posed. The four read the point the exit
RETURNS (the restored point when restoration moved x, the best iterate when `return_best` substitutes it): the label
describes the point handed back. If the implementation finds the stash's feasibility is engine-unit on a scaled solve,
the caller-unit re-measurement `finish` already takes is used instead; this is recorded in the C declaration, not
decided silently.

**Options**: `SqpOptions` gains `termination` (`SqpTerminationOptions`) with exactly the four IPM fields, same names,
same defaults, same validation (finite, > 0; each acceptable tolerance >= its convergence tolerance, as the IPM's tier
rule). The IPM-only fields (`acceptable_iterations`, `complementarity_tol`, `divergence_*`) do not appear on the SQP: the
SQP has neither the consecutive rule nor a divergence test. The dumped names `termination.acceptable_*_tol` are then
the same path on both engines. `docs/options-reference.md` is regenerated (generator target `options-reference`).

**Restoration sub-solves are not relabelled.** The relabel is applied by the top-level driver only
(`!is_restoration_sub_driver_`). A sub-solve that relabelled its own exit would hand the parent's status switch a
`kAcceptable` it routes differently from `kNumericalError`, which moves the path; the C gate's "0 path movers" holds
only if the sub-driver's status is untouched. Section 4.3 revisits the parent's handling of sub-solve failures as part
of B2, where path movement is declared.

### 1.3 New stop reasons and the minor cap

| new `StopReason` | verdict | engine | meaning |
|---|---|---|---|
| `kSubproblemIterationLimit` (15) | `kStalled` (or `kAcceptable` by 1.2) | SQP | The last QP of the solve ended at its minor-iteration cap (`QpStatus::kIterationLimit`) and the retry ladder had nothing left to change. Distinct from `kSubproblemFailure`, which now means a QP that ended `QpStatus::kNumericalError` (an arithmetic or working-set termination). |
| `kTrustRegionFloor` (16) | `kStalled` (or `kAcceptable`) | SQP | The radius reached `trust_region_min` at a feasible point whose rejected steps promised less than the objective's rounding floor (section 3.4). Path-moving; lands with A. |
| `kRestorationWorkLimit` (17) | `kStalled` (or `kAcceptable`) | SQP | The restoration phase spent its own work budget (section 4.1) and handed back a point the resume gates refused. Path-moving; lands with B2. |

Spellings: `subproblem_iteration_limit`, `trust_region_floor`, `restoration_work_limit`. The minor-cap split is C (a
relabel of an existing exit); the other two are added with the code that reaches them.

`map_status` stays the mapping for the QP status of a subproblem that the driver itself re-routes (it feeds the
`best_applies` decision, unchanged); the minor-cap exit's verdict is set at the `kFinishQpFailure` arm from
`mj.qs.status`, AFTER the cap retry and the elastic routing have had their say.

**The record of a relabel.** A relabelled exit keeps a trace of where it came from: a new `MessageKind::kAcceptableExit`
(severity notice, depth 0, `s` = the spelling of the stop reason the exit would otherwise have reported). The message
table of `docs/trace-schema-v1.md` gains the row; `stop_reason` on the end event is `none`, as for any `kAcceptable`.

### 1.4 Text that moves

- `core/solver_status.h`: the reachability paragraph (the SQP now reaches `kAcceptable` and `kStalled`; never
  `kDiverging`), the three new `StopReason`s, `kSubproblemFailure`'s text (QP `kNumericalError` only),
  `kRestorationRefused`'s "Verdict kNumericalError" (or `kAcceptable`), `RestorationSubStatus`'s reachable list (unchanged
  in C).
- `src/core/enum_names.cpp` (spellings), `src/solver/result_message.cpp` (the SQP `kAcceptable` and `kStalled` messages
  name the exit: "acceptable: the solve stopped at <exit> at a point inside the acceptable tolerances"),
  `docs/trace-schema-v1.md` (stop-reason alphabet, the message row), `docs/options-reference.md` (regenerated),
  `docs/migration-0.2.md` (a short RF-1 section: what a caller branching on `kNumericalError` now sees).
- The AP-5 table line "SQP has no acceptable level" is history; it is not edited (dated note).

## 2. Gate C -- reporting only

C changes no iterate, no counter that the iteration reads, no QP. Claims, each asserted by the C declaration:

1. **0 path movers on TRAIN** (238 roster cells x {D, K}, never HOLDOUT): every row equal to the reference arm (pf2d's
   evidence arm `L`, whose code is this base's: `git diff efd14c1e6 84a6e3373 -- src include tests` is empty) on every
   key except `status`, `stop_reason`, `message` and the new message line. Iterations, factorizations, minors, f,
   residuals: byte-equal or inside the calibrated residual gate.
2. **Every status mover listed** with its old and new `(status, stop_reason)` and the four acceptable-tier residuals,
   and checked to be one of: `numerical_error -> acceptable` with all four residuals inside the tier, or
   `numerical_error -> stalled` with old QP status `kIterationLimit`. Anything else is a defect, not a finding.
3. **Pinned status expectations re-derived as a declared event**: every test, replay baseline and corpus expectation
   that pins an SQP `kNumericalError` / stop reason which C relabels is listed in the declaration before it is edited,
   and each edit is the relabel the claim-2 rule predicts.
4. Mutations (each killed by a named test): the relabel applied in the sub-driver; each of the four tier comparisons
   dropped or loosened (`<=` -> `<` at the boundary); the relabel applied to `kIterationLimit` / `kInfeasible` /
   non-finite exits; the minor-cap split reading the pre-retry status; `kAcceptable`'s stop reason left at the old
   reason; the message not emitted. Control clean.
5. ctest rel and dbg green; install smoke; four-lane CI.

## 3. A -- the retry ladder and the routing of failed QPs (path-moving)

### 3.1 Every retry changes something

Today a retryable failure (`qp_failure_is_retryable`) shrinks the radius by `kTrShrinkFactor`, seeds the next solve with
the failed working set (`seed.x` zeroed) and re-solves at the next major, up to `max_consecutive_qp_failures` (3). When
the failed walk never reached the window, the shrink changes nothing it could reach and the seed is the failure's own
working set: the retry is a replay.

The redesign replaces "shrink, up to N times" with an ordered list of RUNGS, each used at most once per iterate (the
set resets on an accepted step, exactly where `qp_failures_in_a_row` resets today), each skipped when it would not change
the QP or its start:

| rung | what changes | taken when (else skipped) |
|---|---|---|
| R1 shrink | radius x `kTrShrinkFactor`, seed = failed working set | the shrink CUTS the failed walk: the failed solve's window was active (`tr_active` non-empty) or `||qs.x||_inf > kTrShrinkFactor * delta` |
| R2 cold | no seed (the walk's own cold start); radius unchanged | the failed solve was warm-seeded with a non-empty working set |
| R3 algebra | the other working-set algebra (`kSchurBorder` <-> `kRefactorize`) on a dedicated engine, as `cap_retry_engine_` is built today | always available once |
| R4 regularization | `primal_regularization` override x 100 (capped at 1e-4) | the failed solve's QP ended through a dependent-blocker or zero-motion termination (the counters say so) |

After the rungs: an infeasible linearization (h > feas_tol, the L2 box-inclusive measure) goes to the elastic ladder as
today's E2 route does; at h <= feas_tol, the radius-floor rule of 3.4 decides; otherwise the solve ends through the
`kFinishQpFailure` arm (`kSubproblemFailure`, or `kSubproblemIterationLimit` for a minor cap, relabelled by 1.2).

**The replay guard.** Whatever rung ran, a retry whose outcome fingerprint -- (`status`, `minor_iters`,
`factorizations`, the termination counters `dep_blocker_terminations`, `zero_motion_cap_hits`, `stagnation_stops`,
`certificate_{bound,row}_refusals`, and `qs.x` bitwise) -- equals the attempt before it counts `qp_retry_replays` and
moves to the next rung without being charged as a rung. With the rung rules above this should read 0; the counter is the
evidence that it does, and a mutation that makes R1 unconditional must light it.

`max_consecutive_qp_failures` keeps its meaning (the number of routed failures per iterate) and its default 3; with four
rungs it now bounds how many of them run. Whether the default should become 4 is read off the gate (rung usage counters
`qp_retry_rung_{shrink,cold,algebra,regularization}`), not decided here.

The `qp_iteration_limit_retry` toggle (default off) and the `qp_failure_step` trial are unchanged and run before the
ladder as today.

### 3.2 Box-project a stagnation iterate before the retryable test

`qp_failure_is_retryable` refuses an iterate standing farther than `feas_tol` outside the QP box. A walk ended by the
stagnation reading can stop mid-step off its box (YORKNET), which ends the solve on the first failure. New: before the
test, an iterate whose walk ended with `stagnation_stops > 0` is projected onto `[qp.lower, qp.upper]` (the box INCLUDING
the trust-region window), and the projected iterate is what every consumer reads (the cap retry's conjunct, the
failure-step trial, the routing gate, the seed). Counted: `qp_failure_box_projections`. Non-finite iterates are never
projected (still non-retryable). Other termination kinds are not projected: an off-box iterate there is evidence of a
broken solve, which is what the test exists to refuse.

### 3.3 Row refusals go to elastic at any h

`bound_refusal_to_elastic` routes a certificate ROW refusal to elastic only when the linearization is violated or the
iterate is non-retryable. New: `certificate_row_refusals > 0` routes to elastic at any h (bound refusals keep their
rule). Risk named in advance: E2-D3 found that at h <= feas_tol the elastic QP of a working-set termination "has no
slacks and IS the failed QP". The MSS2 leg of the A gate reads whether that holds for a row refusal; if the elastic
ladder replays it there, the successor declaration routes row refusals at h <= feas_tol to the rungs instead, and this
section is answered in that declaration, not edited.

### 3.4 The trust-region floor exit

At both radius-floor sites (a judged rejection and a routed failure, `shrink_hits_floor`), before `enter_restoration`:

```
h_k <= feas_tol                                   (the convergence test's feasibility measure, box included)
  && pred_last <= kNoiseFloorFactor * eps * max(1, |f_k|)     (kNoiseFloorFactor = 100, eps = DBL_EPSILON)
```

where `pred_last` is the predicted objective decrease of the last rejected step (the model's own `pred_df`; for a routed
failure, of the failed QP's iterate). When both hold, restoration is not entered -- at a feasible point it is a no-op
that resets the radius only to collapse it again (SMBANK) -- and the solve ends: `kAcceptable` when 1.2's test passes,
else `kStalled` with `kTrustRegionFloor`. Counted: `trust_region_floor_exits`. When only `h_k <= feas_tol` holds, the
floor still requests restoration as today: a promised decrease above the noise floor is evidence the model and the
function disagree for a reason restoration's restart radius can address.

`kNoiseFloorFactor` is a named constant in `globalization/trust_region.h`; SMBANK's rejected `pred` is the calibration
point and is quoted in the A declaration's measured column, not tuned after data.

## 4. B2 -- a bounded restoration that handles its own failures (path-moving)

### 4.1 A restoration work budget

Each phase gets a factorization budget of its own: `advanced.restoration_max_factorizations` (`optional<Index>`; unset
derives `kRestorationWorkFloor + F_entry`, where `F_entry` is the parent's factorizations at entry and
`kRestorationWorkFloor = 5000`, so one phase can at most double the solve's work plus a floor). It is passed as the
sub-solve's `common.max_factorizations` (the minimum of it and what is left of the caller's own ceiling, as today). The
sub-solve's `kWorkLimit` then has two causes; the parent tells them apart by which bound was the smaller and labels the
phase-budget case `kRestorationWorkLimit` (verdict by 1.2), the caller's-ceiling case `kWorkLimit` as today. The 84k
BRITGAS phase is the case this exists for; section 3's rungs remove the three-times-full-cap re-solves inside the phase,
since the sub-driver runs the same ladder.

### 4.2 The phase returns its best-h point

The sub-solve runs with `common.return_best = kOnAnyNonOptimal`: every non-optimal sub-exit (work limit, refused nested
request, failed QP) hands back its best iterate under the shared order (`best_improves`: the wrapper's own feasibility
first -- satisfied throughout, slacks carry the violation -- then its objective, which is the penalty h). The parent's
resume tests then read the best-h point instead of wherever the phase stopped.

### 4.3 Elastic exhaustion inside the phase

Today an exhausted elastic ladder inside the phase raises a nested restoration request, which is always refused and ends
the sub-solve `kNumericalError` (`kRestorationRefused`, nested), and the parent ends `kRestorationFailed`. New, inside the
sub-driver only: an exhausted ladder is a routed failure -- the rungs of 3.1 if any is left at this iterate, else the
sub-solve ends (refused, nested, as today) and 4.2 hands back its best-h point. The curvature switch
(`restoration_curvature_switches`, rider M1) keeps its trigger: it reads the refusal, which still happens, only later.
The parent's switch arm for a failed sub-solve then applies the resume gates of 4.4 to the best-h point before it settles
on `kRestorationFailed`.

### 4.4 The resume gates

Today: resume when `kkt_r.feasibility <= feas_tol` (SNR), or -- with `restoration_resume_reduction > 0`, default 0 -- when
the phase certified nothing and cut h to that fraction of the entry h with the box held. BRITGAS's baseline resume at
h = 9.97e-7 against 1e-6 is luck. Changes:

- the reduction resume is extended from `kNumericalError` / no-claim `kOptimal` sub-exits to `kStalled` and the phase's own
  `kWorkLimit` (never the caller's ceiling: that one ends the solve);
- `restoration_resume_reduction` default 0 -> 0.1 (one order of magnitude, `kRestoreRadiusFactor`'s reading), so a
  phase that bought a real reduction hands it back instead of ending the solve;
- the feasible-resume test is unchanged.

The A+B2 gate decides whether 0.1 ships; if TRAIN shows a lost solve, the successor declaration states the alternative
(the reduction resume on the budget exits only, default stays 0).

### 4.5 SOC

Whether SOC QPs are capped separately from the main QP is UNOBSERVED. B2 adds the counter that observes it
(`soc_qp_iteration_limits`, the SOC solve's `QpStatus::kIterationLimit` exits) and changes nothing about SOC.

## 5. Gate A + B2

Declared before the legs, never edited (successors `-rN`):

1. **TRAIN** (238 x {D, K}, co-run per CLAUDE.md section 7, counters only; never HOLDOUT): every status mover listed with an
   objective-quality read (f and the four residuals against the reference arm, which is the C head). Bar: **no lost
   solve** (`optimal`/`acceptable` -> anything else) without a ruling. Reported: solved count (optimal; optimal +
   acceptable) and total factorizations, new vs C.
2. **Walk census** (the M7 census instrument at its declared terms), new vs C: reported, with any status mover listed.
3. **The pilot's failing cells** (YORKNET, MSS2, BRITGAS, ACOPR118, SMBANK, the ACOP family; every option label the pilot
   store records for them) re-run from the store's options files under a copy of the lane harness pinned to the A+B2
   head; each reported old vs new (status, stop reason, iterations, factorizations, f, residuals) with the rung and
   restoration counters that explain the change.
4. Mutations, a must-kill for every new exit and route: each rung's skip rule inverted; the replay guard disabled; the
   projection applied to every termination kind / never applied; row refusal routing reverted; the floor exit's
   `h <= feas_tol` or noise conjunct dropped; the phase budget unset (inf) and the budget labelled `kWorkLimit`; the
   sub-driver's `return_best` reverted; the elastic-exhaustion route inside the phase reverted to the immediate refusal;
   the reduction-resume extension reverted. Control clean.
5. ctest rel and dbg, install smoke, four-lane CI.

## 6. Not in RF-1

HOLDOUT (never). Wall-clock claims. The QP walk's own terminations (zero-motion, dependent blocker, stagnation, saddle
exit) are not changed: RF-1 changes what the DRIVER does with a QP that could not finish. A `QpStatus` split of the walk's
working-set terminations from arithmetic failure (a fifth QP status) is the natural next step and is registered, not done.
Accelerate and Windows readings: CI only.
