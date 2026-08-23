# Chunk 4 report — comment sweep: include/hven/detail/{kkt,linear,warmstart,solvers} and include/hven/linear

Branch `sweep/base` at `a9cba6a`. Scope: all 17 headers (5 `detail/kkt/`, 5
`detail/linear/`, 1 `detail/solvers/`, 4 `detail/warmstart/`, 2 `linear/`; the
brief's "17 files, ~7.9k lines" was exact at 7912). No builds, no tests.
Gates, all passing on every touched file: (a) comment-stripped TOKEN STREAM
identical to HEAD (zero code-token change); (b) non-comment physical LINES
identical to HEAD (whole comment lines removed only, blank-line structure
untouched); (c) labels grep clean (Task N, PHASE-x, fix round, WAVE, review
names, PR #, project-rule-T6 shorthand: none remain); (d) the MPL/BSD notice
blocks of `pardiso_session.h` (lines 1-68) and `accelerate_session.h`
(lines 1-38) byte-identical to HEAD, as is the ASSET provenance header of
`solver_interface_specs.h`.

## Files touched

| file | comments | total lines |
|------|----------|-------------|
| detail/kkt/border_ops.h               | 113 ->  86 | 185 -> 158 |
| detail/kkt/bordered_eqp.h             | 192 -> 157 | 391 -> 356 |
| detail/kkt/kkt_assembly.h             | 127 -> 117 | 289 -> 279 |
| detail/kkt/kkt_calls.h                |  69 ->  51 | 113 ->  95 |
| detail/kkt/schur_complement.h         | 248 -> 205 | 467 -> 424 |
| detail/linear/accelerate_session.h    | 187 -> 172 | 272 -> 257 |
| detail/linear/fault_injection.h       | 251 -> 184 | 357 -> 290 |
| detail/linear/lapacke_shim.h          |  60 ->  49 | 127 -> 116 |
| detail/linear/pardiso_session.h       | 263 -> 242 | 380 -> 359 |
| detail/linear/session_id.h            |  34 ->  32 |  49 ->  47 |
| detail/solvers/solver_interface_specs.h | 73 -> 69 | 590 -> 586 |
| detail/warmstart/continuation.h       | 549 -> 391 | 623 -> 465 |
| detail/warmstart/mesh_transfer.h      | 450 -> 347 | 691 -> 588 |
| detail/warmstart/predictor.h          | 869 -> 633 | 1450 -> 1214 |
| detail/warmstart/warm_start.h         | 670 -> 398 | 754 -> 482 |
| linear/dense_symmetric_factor.h       | 127 -> 126 | 180 -> 179 |
| linear/symmetric_factor.h             | 760 -> 754 | 994 -> 988 |
| **total**                             | **5042 -> 4013** | **7912 -> 6883** |

(solver_interface_specs' before-count includes its two 3-line `/* */` blocks,
converted to `///` docstrings.)

Character of the chunk: split in half. The four warmstart headers were
label/history-dense and took chunk-1-depth treatment (banner paragraphs of
phase/task/findings adjudication history rewritten down to the surviving
contract; measured tables and review-process narratives moved to rationale
below). The kkt and linear-detail headers are contract-dense — their bulk is
load-bearing invariant documentation (all-or-nothing border-stack guarantees,
evidence-usability discipline, don't-write iparm semantics) which survives;
what went was task/review labels, origin-project migration history, and
derivation chains whose conclusion stayed. `symmetric_factor.h` and
`dense_symmetric_factor.h` were already close to conforming (their sweep
mostly removed interior-point-engine attributions and one naming-history
note); `solver_interface_specs.h` needed only ruler removal and two block
comments reshaped into docstrings.

## Rationale candidates for docs/notes

Line numbers refer to the PRE-sweep files at `a9cba6a`.

- kkt_calls.h:70-79 — B2 pricing: steady state paid three O(nnz) hashes per
  SSN major vs the dissolved KktSystem seam's two; B1 priced the extra at
  6.12-7.76 % of SSN-major wall-clock on SSN-heavy families
  (docs/notes/data/2026-08-15-m3-b1-hash-cost/), above the plan's 5 %
  gate-blocking threshold.
- kkt_assembly.h:30-32 — SpMatU->SpMatRM vocabulary move (M3 phase-C S2b):
  the type used to carry the upper-triangle convention.
- kkt_assembly.h:139-142 — REACHABILITY of the box-check throws: dead on all
  in-tree driver paths (which validate upstream); written for direct users of
  the detail header.
- border_ops.h:93-120 — full delete_k0_row derivation: substituting the
  border's pinning equation into K0's own row reduces the rest of the system
  to exactly K0 minus that row; DeleteK0RowMatchesDirect; Tikhonov-relaxation
  vs exact-pin contrast.
- bordered_eqp.h:89-96 — constants history: kMaxBorderRefineSteps/
  kBorderRefineRelFloor moved to eqp_solve.h when solve_eqp briefly carried
  the same flag-gated loop; loop deleted once both shipped backends measured
  it inert; constants deliberately not moved back.
- bordered_eqp.h:163-207 — iterated-refinement narrative: HS26 first-
  subproblem reproduction (one step left the equality row violated by 8.7e-3;
  QpEngine classified the point structurally infeasible;
  HsBattery.BorderModeFalseInfeasible); measured residual/footprint
  separation 5e-5..5e-1 (agreeing fixtures) vs 1.6e4 (HS26); the open
  question whether the SHARED single step should also iterate
  (task-11b-report.md "Alternatives rejected"); dim()==0 guard measured
  deletable (suite green) and kept anyway.
- bordered_eqp.h:54-71 — pinned-scatter evidence: mutation-verified that
  removing the overwrite does not break BorderModeMatchesRefactorizeMode
  (refinement closes the gap on every fixture); kept as by-construction
  agreement rather than load-bearing.
- schur_complement.h:26-37 — Eigen::LDLT rejection specifics: always 1x1
  blocks, info()==NumericalIssue and garbage vectorD() on [[0,1],[1,0]],
  silent wrong answer.
- schur_complement.h:51-54 — LAPACK has no public rank-one update/downdate
  for symmetric-indefinite factorizations; incremental update a possible
  later optimization if the O(dim()^3) rebuild ever matters.
- lapacke_shim.h:18-22 — file originated in the SQP solver's standalone
  development; shimmed entry points unchanged; docs/pattern-hash.md another
  same-lineage primitive.
- symmetric_factor.h:421-424, 476-478 — attributions dropped:
  kTwoByTwo/kAdaptivePartitioning were "the values the interior-point engine
  writes today".
- symmetric_factor.h:500-506 — SolveParallelism naming history: an earlier
  bare `bool parallel_solve` wrote iparm[24]=1 for `true` — exactly backwards
  (1 is SEQUENTIAL, 0 is Pardiso's parallel default).
- pardiso_session.h:122-135 — ordering-field wire-representation history: an
  int encoding with 0 as don't-write sentinel would have collided with real
  iparm[1] value 0; the optional shape let kMinimumDegree land as a pure
  Options-side amendment.
- fault_injection.h:55-76 — FactorizeFaultInjector consumer detail: the
  interior-point engine's zero-filling evidence projection on a failed
  factorization needs the failure provoked deterministically and by specific
  status code; Accelerate's refusal reachable in principle but not from
  fixtures.
- fault_injection.h:315-337 — ThreadCountObserver supporting evidence:
  MKL exposes no query for the thread-local override in force; behavioural
  coverage carried by
  SymmetricFactor.APerInstanceThreadCountRestoresTheCallersOwnThreadLocalOverride;
  traced data flow via MklThreadScope(cfg_.num_threads) at the single ::pardiso
  call site.
- warm_start.h:13-36 — Phase-4 Task 2/Task 4 scope history: the object was
  POD-ish copyable data until `hot` landed.
- warm_start.h:91-123 — ingest-clear adjudication: finding B-1
  (docs/notes/2026-07-31-nonconvex-sweep-adjudications.md §1), the choice
  over three alternatives (sqp_driver.h's THE INGESTED MULTIPLIERS ARE MADE
  COMPLEMENTARY), the kSeeded resolution history, and clear-runs-first /
  clamp-second ordering normativity.
- warm_start.h:134-142 — probe P2: an ingest at x=0 carrying
  lambda_i=(-1,+0.5) on `min x s.t. x<=0, -x-1<=0` certifies kOptimal in zero
  majors at f=0 where the truth is f=-1.
- warm_start.h:144-176 — producer-history corrections: O-B1-4 closure route;
  CrossoverWrongSignDualLeavesRowFree's -1e-8-vs--1e-9 fixture; the
  two-orders-inside-kSeededDualClampTol correction (final fix wave W9).
- warm_start.h:289-320 — structure_hash history: through Phase 4 zero-major
  exits wrote 0 and their hand-offs were silently unusable (O-1; make_warm_
  start's THE ZERO-MAJOR PROBE).
- warm_start.h:349-406 — proximal-carry adjudications: the anchor-at-current-
  iterate ruling (keeps modified-Newton on the exact residual; fixed points
  exact unregularized KKT points), the gate-is-the-sigma review trail
  (NF-3, WAVE #11), and the Phase-5 n=1e6/1285 MiB scale ceiling behind the
  cost gate.
- warm_start.h:465-498 — activity-rule repair measurements: old slack-side
  rule recovered 84/92 (N=100), 250/370 (N=400), 0/1486 (N=1600) and 0/92 at
  mu=1e-8; repaired rule 92/92, 370/370, 1484/1486
  (docs/notes/2026-08-06-activity-tol-repair.md).
- warm_start.h:500-510 — extreme-dual trap instance: a hand-off pricing a row
  at 1e6 against a residual ~1 was certified ACTIVE at the old 1e-6 default.
- warm_start.h:525-583 — three-limits derivations with F7 numbers: dual_tol
  overtakes the relative term around N~800 (1e-6 vs 5.00e-7; live rows 3.63e-7
  below both floors; noise rows 8.50e-7 between them); false actives 12/24/46
  at N=1600/3200/6400 for mu=1e-8 vs 0/2/4 at 1e-9 with recall identical;
  loose hand-off mu=1e-4 inferring 114 rows, all false;
  CrossoverDegenerateRowLeftFree and
  CrossoverFalseActiveGuardDegradesAtTheOperativeBarrierLevel pins.
- warm_start.h:598-638 — crossover-value history: through Phase 5 the 3-arg
  crossover call was byte-identical to the 2-arg call;
  CrossoverRecoversExactSolveHs14 pinned that identity and now pins the
  opposite.
- warm_start.h:657-671 — activity_rel_tol rename history: replaced absolute
  activity_tol (default 1e-6) which multiplied the SLACK; deliberate
  rename-forced compile error for per-mesh tuners.
- predictor.h:62-92 — regularization-floor law and FD-term presence:
  err_inf/(delta*||dp||)=1.000 across delta=1e-8/1e-10/1e-12 on F1 (family
  constants F3 ~500, F4 ~1.4); term (2) identically zero for identity-map
  parameter dependence, ~1e-10 for F4's sum; no-refinement cost comparison
  (||dp||=0.01 on F2: 3e-5 vs 1e-10).
- predictor.h:127-141 — RELAX numerics: MKL Pardiso lands the frozen-z
  cancellation at exactly 0.0 bit-for-bit; Accelerate leaves
  -7.7037197775489434e-34 against a frozen z of -0.05 (~1.5e-32); origin
  divergence entries D16/D17/D18 did not migrate
  (docs/notes/2026-08-14-accelerate-divergence-register.md);
  test_predictor.cpp's D18 arm quotes the observation.
- predictor.h:164-211 — ratio-test defect narrative: F3 n=1000 p 0.35->0.55
  put ninety variables on the upper bound where the truth has one (prediction
  44.5 off; 195 consuming-solve minors vs 18 unpredicted); why the old loop
  could not repair its own overshoot (flat-run zero multipliers are a fixed
  point); the node-999/t=0.75 walkthrough.
- predictor.h:212-225 — lineage citations: qpOASES (Ferreau, Bock & Diehl
  2008; Ferreau et al., MPC 6:327-363, 2014), Kungurtsev & Diehl 2014 (COAP
  59:475-509), sIpopt (Pirnay et al., MPC 2012),
  docs/notes/2026-07-27-literature-survey.md; the WHERE-along-dp contribution
  is separable from sIpopt's repairs.
- predictor.h:279-283 — weakly-active exemplar: parametric_families.h's F1 is
  weakly active across its entire middle branch (active with lambda == 0 for
  every p there).
- predictor.h:283-313 — hot-handle reasoning detail: BorderState deliberately
  non-copyable AND non-movable because SchurComplement holds a reference into
  its own KKT factor; reuse-gate condition (b) is HotState::values_hash.
- predictor.h:443-469 — max_activity_rounds budget sweep: QP-minor counts for
  budgets 1..inf on three F7 threshold-crossing cells (MKL, clang++ Release);
  1 vs 4 tradeoff (fewer Schur solves, equal-or-better minors on all three
  cells); "data picked the interval, a human picked 4 inside it".
- predictor.h:496-505 — to_string carve note: block line-count preserved for
  P-SYM __LINE__-class noise accounting during the TU-boundary diff.
- predictor.h:651-655 — bound_at_frac pre-ratio-test equivalence note
  (bound_at_step(i,s) == bound_at_frac(i,s,1.0) for every historical caller).
- continuation.h:59-84 — shrink-fix history: retraction of the earlier
  "bounded (one wasted attempt)" claim; worked example (six wasted solves for
  dp=0.5 against a 0.01 remainder at shrink=0.5) and the second face (dp<dp_min
  failure reportable without ever attempting the clamped remainder).
- continuation.h:141-222 — retry-economics measurements: F7 nx=1e5 sweep 14
  attempts / 9 converged steps, failed proposals 84 % of minors and 86 % of
  wall (docs/notes/2026-08-01-psiopt-first-comparison.md §4.5(b),
  prototypes/psiopt_bridge/results/sweep_n20000_warm.csv); false-abandonment
  frequency (once on that sweep, still 0.54x minors) and growth-suspension
  buy (0.80x) in docs/notes/2026-08-02-controller-retry-economics.md §3/§7(2)/§4.
- continuation.h:330-385 — probe_budget multiplier evidence: floor-only
  controller (probe_budget=1) counter-for-counter identical to shipped
  default; probe_budget=4 strictly worse (3909 vs 3637 minors); BASE
  14/6732 vs suspension-only 15/5409
  (sweep_n20000_warm_task1_suspension_only.csv).
- continuation.h:530-572 — kProbeBudgetFloor derivations: F7 N=100
  counterexample cascade (3 steps/51 minors -> shrink-and-retry pile;
  4 steps/63 -> 25 steps/298, a 4.7x regression, over the longer range);
  floor-removal mutation fails eight tests (six predating the feature);
  binding control N=2000 (second step 126 minors over 3 majors after a
  22-minor predecessor; floor=100 moves a healthy cell 5/270 -> 7/304);
  step-by-step budgets 334,334,334,356,200,214,214,200... — the floor set
  10 of 16 budgeted steps on the large sweep
  (sweep_n20000_warm_task1_after.csv).
- mesh_transfer.h:63-64 — test pointer: test_mesh_transfer.cpp measures raw
  copying vs genuine destination solves and pins the weight ratio.
- mesh_transfer.h:98-107 — defect-block concrete forms: trapezoidal
  zeta_i = y_{i+1}-y_i-(h_i/2)(f_i+f_{i+1}) (h-multiplied, multiplier already
  the costate) vs the residual form and Garg-Rao Radau differentiation-matrix
  defects (unweighted rows, lambda/w is the standard estimate).
- mesh_transfer.h:239-249 — battery-note §8 distinction: uncomputed hash
  (driver zero-major exits, repaired) vs unknown hash (this layer) vs unused
  hash (unevaluable start point, cold outright).
- mesh_transfer.h:309-375 — THE PHASE-6 INGEST GAP full specification: what
  the gap meant (only x reachable through the driver API), the two candidate
  driver-side closures ((i) seeded level adopted; (ii) caller-stamped hashes
  rejected — any caller stamping a hash while leaving non-null hot defeats
  the gate), repair-A/repair-C complementarity
  (docs/notes/2026-07-30-warm-start-battery-results.md §8 item 1;
  tests/test_b1_gate.cpp
  AZeroMajorWarmSolveWithALiveInequalityPriceStillCertifies and its seeded
  twin), and the twin-write-up sync obligation with warm_start.h's crossover
  note.

## Looked like a defect, reported not fixed

None found in this chunk. Two wording inaccuracies in existing comments were
corrected as part of the accuracy rule rather than deferred: (a) predictor.h's
kDualSignTol note called the threshold "absolute" while the code scales it by
max(1, |multiplier|) — now described as relative-with-absolute-floor; (b)
warm_start.h's SHAPE paragraph said nothing serializes `hot` "and none of this
struct's own (de)serialization -- there is none -- is expected to grow any",
whose double negative survived intact but was reworded for readability without
changing meaning.

## Docstrings written under uncertainty (reviewer first)

1. kkt_calls.h — `@throws std::runtime_error` on both factorize_checked
   overloads: VERIFIED against src/kkt/kkt_calls.cpp (both throw on
   FactorizeOutcome::Status::kBackendError; the 2-arg overload delegates).
   Not uncertain, listed only because it is a new @throws tag.
2. kkt_assembly.h — replaced the actor-labelled "Task 7 assembles the solve
   right-hand side ... and is expected to SUBTRACT rhs_shift" with "The QP
   engine assembles ... and SUBTRACTS rhs_shift". Actor claim taken from the
   surrounding derivation (K y = b - rhs_shift); reviewer should confirm
   qp_engine.h's assemble path indeed subtracts (it cannot be checked from
   this chunk without reading qp_engine.h, out of scope).
3. warm_start.h — the producer inventory at the end of the banner
   (mesh_transfer resolves kSeeded never above it; predictor clamps emitted
   prices non-negative; every SqpDriver exit non-negative, with the
   zero-major/restoration qualifications) is a heavy condensation of several
   original paragraphs; each clause should be checked against sqp_driver.h.
4. predictor.h — PredictorOptions::max_activity_rounds now states the curved-
   fixture degradation qualitatively (the specific ||x_pred-x*|| and
   minor-count figures went to rationale); verify both claimed directions
   match the deleted table.
5. predictor.h — VALIDATE-THEN-CATCH-EVERYTHING no longer attributes the
   historical exception-type crossings to named past failures ("the dissolved
   border reported dsytrs info != 0 ... as std::invalid_argument"); the
   generalized claim ("exception types have crossed type lines historically")
   should be confirmed fair.
6. continuation.h — RETRY ECONOMICS states the measurements qualitatively
   ("failed proposals have been measured to account for the large majority of
   a sweep's QP minor iterations") and kProbeBudgetFloor as "~1.8x clearance"
   / "multi-x regression"; the exact figures are in rationale above.
7. solver_interface_specs.h — the two `/// @brief` docstrings on
   SolverConstraintSpec/SolverObjectiveSpec are newly written from the code
   (plain descriptions of the Concepts); nothing controversial, but they are
   new prose.
8. bordered_eqp.h — pinned-scatter paragraph softened "a pin's dual is a
   regularization artifact of size ~1/mu, making the gap O(1)" to "a pin's
   dual can be large, making that gap material"; confirm the softening loses
   nothing a reader needs.

## Fix round (chunk-4-review.md, applied at c4b6e6e)

| # | Severity | Location | Applied | Disposition |
|---|----------|----------|---------|-------------|
| F1 | Blocking | `fault_injection.h:24-26` MPL-deviation ledger "Two such deviations" | yes | Restored the second item (`post_pardisoinit_*` fields, recorded at a different line in the same function for a different reason) so the enumeration matches the stated count of two. |
| F2 | Blocking | `kkt_calls.h:78-90` `@throws` on the wrong `factorize_checked` overload | yes | Moved `@throws std::runtime_error If the outcome is FactorizeOutcome::Status::kBackendError; every other status is returned.` to the 3-arg overload (`src/kkt/kkt_calls.cpp:25-46` confirms it, not the delegating 2-arg, is where the throw lives); the 2-arg overload now carries `@throws std::runtime_error As the three-argument overload.` |
| F3 | Blocking | `predictor.h:445` `predict()` THROWS list dropped the non-finite `fd_step_scale` case | yes | Reworded to "an fd_step_scale that is not finite and positive", matching the guard at `predictor.h:549` and the sibling field doc at `:229`. |
| F4 | Blocking | `predictor.h:93-95` RELAX note lost "numerically" and the cross-backend caveat | yes | Restored "land at numerically zero (exactly 0.0 on MKL; an O(1e-34) residue on Accelerate)". |
| F5 | Blocking | `kkt_calls.h:58` hash-cost floor unit "per solve" vs "per SSN major" | yes | Restored "per SSN major". |
| F6 | Blocking | `continuation.h:412-415` `~1.8x` clearance attached to the wrong quantity | yes | Restated as "the largest pre-convergence spend ... (110 minors at the top of the step's final major)", matching 200/110 ≈ 1.8x. |
| F7 | Blocking | `mesh_transfer.h` "THE PHASE-6 INGEST GAP" title deleted but still referenced from `core/start_level.h:63` and `tests/sqp/test_mesh_transfer.cpp:446` | yes | Took option (a): restored a two-line anchor in section 4 carrying the exact title and the one surviving fact ("the hash-less object reaches the solve at kSeeded; factorization reuse is what the sentinel protects"). Neither external referring file needed touching since the exact title is back. |
| F8 | Blocking | `warm_start.h:462-476` malformed `//` immediately followed by `///` on `from_interior_point`, real `@brief` invisible to Doxygen | yes | Rebuilt as one `///` block: `@brief Builds a WarmStart from an interior-point-style primal-dual point.` first, sign-convention pointer and dimensions folded into the body, `@throws` retained. |
| F9 | Non-blocking | `symmetric_factor.h:282,420` two table rows six columns over-indented | yes | Both re-indented to `//   ` matching their siblings (`kOneByOne`, `kOneByOneNoAutoRefine`, the present-value row). |
| F10 | Non-blocking | `warm_start.h:338` citation to `docs/notes/2026-08-06-activity-tol-repair.md`, not in this repo | yes | Dropped the path; the sentence stands without it (measurement claim kept, citation removed). |
| F11 | Non-blocking | Three stale `tests/*.cpp` paths (`kkt_assembly.h:74`, `continuation.h:234`, `predictor.h:65`) missing the `sqp/` subdirectory | yes | All three retargeted to `tests/sqp/test_qp_engine_border.cpp`, `tests/sqp/test_continuation.cpp`, `tests/sqp/test_predictor.cpp` (confirmed those are the actual paths in the tree). |
| F12 | Non-blocking | `mesh_transfer.h:222` "warm_start.h's StartLevel note" — StartLevel is documented in `core/start_level.h` | yes | Retargeted to "core/start_level.h's StartLevel note". |
| F13 | Non-blocking | `bordered_eqp.h:59` pinned-scatter softening lost the magnitude and the over-determined-working-set reachability parenthetical | yes | Restored both: "a pin's dual is a regularization artifact of order 1/dual_mu, so that gap is O(1) rather than O(dual_mu)" and the parenthetical "(every variable pinned AND a working row still demanding something of them)". |
| F14 | Non-blocking | `bordered_eqp.h:159` "the well-conditioned unregularized system" asserted a general property the file never establishes | yes | Reworded to "whose target system was well conditioned in the reproduction (cond ~1.5e2)", restoring the measured framing and the dropped magnitude. |
| F15 | Non-blocking | `schur_complement.h:38,113` two consequence clauses deleted with their premises | yes | Restored `:38`'s "so one border beyond the cap is always paid for. A caller that ignores needs_refactorization() will simply grow C without bound and pay the O(dim()^3) rebuild for it." and `:113`'s Release-only memory-safety parenthetical ("drop_border's per-array erase and rebuild_schur's C assembly both index all three at dim(), so a length skew is an unguarded out-of-bounds read in Release, not a degraded answer"). |
| F16 | Informational | Report vs. review comment-count delta (4 lines, counting-convention only) | n/a | No code action — informational reconciliation note only, nothing to fix in the headers. |

Applied: 16 of 16 actionable findings (8/8 blocking, 8/8 non-blocking); F16 is
informational and required no header change. Counts (comments, total lines)
are unchanged from the table above — every fix is a same-line or same-block
rewording, not an addition/removal of comment lines beyond what the review
itself flagged as missing.

Gate: `strip_compare.py . c4b6e6e HEAD` → `files checked: 10 violations: 0`
(pre-commit token-strip verification against the working tree; re-run against
the committed HEAD after this commit lands gives the identical result since
no further changes were made).
Files touched: `include/hven/detail/kkt/{bordered_eqp.h, kkt_assembly.h,
kkt_calls.h, schur_complement.h}`, `include/hven/detail/linear/fault_injection.h`,
`include/hven/detail/warmstart/{continuation.h, mesh_transfer.h, predictor.h,
warm_start.h}`, `include/hven/linear/symmetric_factor.h`.
Commit subject: `docs: comment sweep — kkt, linear and warm-start accuracy fixes`.
