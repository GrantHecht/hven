# Chunk 5 report — include/hven/core and include/hven/drivers

Branch `sweep/base`. Subject: `docs: comment sweep — include/hven/core and include/hven/drivers`.

Base note: the work order said confirm HEAD == 06bdf0b. After
`git pull --rebase origin sweep/base` HEAD was **c4b6e6e** — two commits landed
after the brief was written (`d370ab4` chunk-3b QP/SSN accuracy fixes,
`c4b6e6e` chunk-3b report), both confined to `include/hven/detail/qp/` and
`.sweep/`, i.e. outside this chunk's scope. Work proceeded on c4b6e6e.

## Files touched (before -> after)

| File | Comment lines | Total lines |
|---|---|---|
| core/types.h | 59 -> 46 | 93 -> 81 |
| core/version.h | 5 -> 7 | 13 -> 15 |
| core/solver_status.h | 79 -> 48 | 103 -> 72 |
| core/start_level.h | 139 -> 86 | 162 -> 109 |
| core/pattern_hash.h | 124 -> 126 | 208 -> 210 |
| core/ledger.h | 174 -> 136 | 241 -> 204 |
| core/solver_counters.h | 984 -> 886 | 1076 -> 1008 |
| drivers/optimization_problem_base.h | 27 -> 79 | 204 -> 258 |
| drivers/sqp_types.h | 933 -> 860 | 1046 -> 984 |
| drivers/interior_point_solver.h | 958 -> 1043 | 1583 -> 1667 |
| drivers/sqp_driver.h | 3591 -> 2805 | 4104 -> 3317 |

Comment-line counts went UP on four files: that is the public-header rule doing
its job — declarations that previously carried no Doxygen (or only bare `//`
prose) gained structured `///` docstrings (@brief/@param/@return/@throws),
while narrative/history blocks around them shrank. Net across the chunk:
7073 -> 6022 comment lines, 8833 -> 7850 total lines.

Zero code-token change verified by a string-aware comment-stripped diff of
every file against its parent commit: all eleven diffs empty. Provenance
(copyright / ASSET-derived) headers untouched. No builds, no tests, per the
rules.

## What was done per area

- **core**: every declaration gained or keeps structured Doxygen (version(),
  the Index/Vec/Mat/SpMatRM/ref aliases, both status enums, StartLevel and
  StartLevelHistogram with per-field `///`, Fnv1a members, feed_pattern /
  pattern_hash (now with an explicit `@throws std::invalid_argument if !A.isCompressed()`,
  verified against src/core/pattern_hash.cpp) / combined_pattern_hash, Ledger
  methods, SolveRecord/SqpSolveRecord with per-field docs). Counter/cost cases
  in solver_counters.h were preserved case-for-case (the refinement counters'
  different baselines, the walk-counter net identity and its named
  temporary-vertex-start-repair exception, degenerate_run_max's exact reset
  set, soc trio identity, probe-budget's three non-counts, evals_full/values
  count lists, escape census partition).
- **Stale-history recasts** (accuracy, learned from reading current code):
  SsnCounters claimed backtracks/prox-updates/uncertain-peak were
  "structurally zero" and SqpCounters::ssn "identically zero because the
  driver never constructs an SsnEngine" — both were true of an earlier task
  and false now (ssn_engine.h ships kFull as default; sqp_driver.h lazily
  owns an SsnEngine and folds counters under kSsn). Recast to present-tense
  contract: zero under kBare / zero unless qp_mode == kSsn. No case lost.
- **drivers**: sqp_driver.h's ~2000-line design-note banner condensed section
  by section to contract + invariant (ONE TRIAL steps, RADIUS MANAGEMENT rules
  and conjuncts, MODEL EVALUATION cost cases, CONVERGENCE TEST formulas and
  per-mode complementarity identity, B-1 clear contract and its costs list,
  complementarity gate scope/tolerance/exposure, failure routing including the
  per-chain-not-per-iterate one-shot bound, SOC construction/signature/
  regime-cost split, elastic construction/exhaustion signature/knife-edge
  ruling, restoration decision table and resume ordering, warm-seeding rules,
  full-step engagement/watchdog/safety invariant, budgeted-mode mechanics);
  sqp_types.h option notes condensed with every A/B-lever default and measured
  outcome kept; interior_point_solver.h gained full Settings/SolveResult field
  documentation while keeping every sentinel/per-phase/counting-semantics
  case; optimization_problem_base.h fully documented (@throws added where the
  bodies throw).

## Rationale candidates for docs/notes

Argument chains removed from the headers (one-line summaries; original
locations are pre-sweep line numbers of this chunk's parent commit):

1. `sqp_driver.h` RADIUS MANAGEMENT (:142-163) — the three-part case against
   porting CGT BTR's eta_1 shrink-on-weak-accept branch (contradicts funnel
   acceptance; growth conjunct already handles weak steps; fourth unsourced
   constant), plus the scoping note that no fixture distinguishes the policies.
2. `sqp_driver.h` B-1 clear (:511-527) — the rejected-repairs menu: gate first
   convergence test on complementarity / ingest x without multipliers /
   require major_iters >= 1, each with its disqualifying reason.
3. `sqp_driver.h` ELASTIC TIER finite-slack investigation (:1236-1292) — the
   full bound-only vs sigma-scaled sweep tables over S = 1e1..1e8 and the
   conditioning diagnosis (row (S,-1) vs (S,-S/2)); condensed in-file to the
   mechanism and one table.
4. `sqp_driver.h` stall early-exit (:1372-1489) — the corpus split behind
   shipping it off (HS15 86->63 majors reshaped, InfeasibleCircleLineModel
   60->2 via elastic route, HS10 free 6x cut), the FloorRaisesTheRestorationRequest
   redesign implication, and why bound_state cannot separate the fixture
   classes (both have a free slack column at the deciding rung).
5. `sqp_driver.h` knife-edge ruling (:1531-1591) — keep-exact-zero reasoning
   with the measured border/refactorize pred_df pair (-6.4392935428259079e-15,
   -1.1102230246251565e-15) and what a future tolerance would need to show.
6. `sqp_driver.h` SOC warm-start analysis (:928-979) — HOT-START REUSE
   conditions (a)-(d) applied to the SOC re-solve, the adaptive_mu
   convergence-tail mu-mismatch (~residual < ~2.2e-6 breaks condition (d)),
   and the caveat that SocDefeatsMaratos's "0 extra factorizations" is
   protected by attempt timing, not by (d) unconditionally.
7. `sqp_driver.h` WHERE DEFINITIONS LIVE (:3418-3442) — the TU-carve
   neutrality argument (~2350 inline lines, already-out-of-line calls at -O3);
   the pointer to src/drivers/sqp_driver.cpp's banner is retained in-file.
8. Move/layering histories dropped as history (kept nowhere else):
   `solver_status.h`/:10-15, `start_level.h`/:10-32, `ledger.h`/:217-228,
   `types.h`/:22-34 (Index retypedef story incl. the CI-lane measurement
   reference), `pattern_hash.h` none needed. If the controller wants the
   Index-retypedef provenance (why hven::Index must BE Eigen::Index, measured
   divergence on Apple arm64) preserved as a note rather than the in-file pin
   comments, the pin comments themselves still carry the essential facts.

## Defects noticed (reported, not fixed beyond comments)

1. **Wrong heading on crash_basis_seed** (`sqp_driver.h`, pre-sweep :2662): the
   comment block above `crash_basis_seed()` began "THE SECOND-ORDER CORRECTION
   SUBPROBLEM (Task 7)" while its entire body documents the crash basis seed.
   Stale heading from an earlier edit cycle. The heading was corrected during
   this sweep (comment-only change); flagged so a reviewer knows the block was
   touched for correctness, not just style.
2. **Doc-vs-code drift in solver_counters.h** (pre-sweep :342-348, :1063-1072):
   claims that SSN safeguard fields and the whole `ssn` aggregate were
   structurally/identically zero because SSN was unwired contradict the
   shipped code (kFull default, driver-owned lazy engine). Recast to accurate
   present-tense statements; the numeric contracts (inequalities, peak-by-max)
   were already correct and are untouched.
3. Nothing else looked like a code defect. No code token changed anywhere.

## Docstrings written where behaviour was inferred (reviewer: check these first)

- `optimization_problem_base.h`: the five pure-virtual mode names are
  documented from their composition ("solve_optimize_solve: solve, optimize,
  then solve") — matches run_nlp_solver's dispatch order but the phase
  semantics themselves live in derived classes; confirm the one-liners.
  JetJobModes enumerator one-liners likewise derive from strto_jet_job_mode /
  jet_run's switch.
- `interior_point_solver.h` Settings fields that had NO prior documentation and
  got name/group-derived one-liners: `max_acc_iters_` (assumed
  acceptable-iterate cap), `wide_console_`, `cnr_mode_` (**uncertain**: read as
  "colorize console output"; could be something else entirely),
  `fast_factor_alg_`, `force_qp_analysis_`, `neg_slack_reset_`, `alpha_red_`,
  `obj_scale_`, the Accelerate tolerances, and the enum one-liners for
  `QPAlgModes`, `QPPivotModes`, `PDStepStrategies`, `AlgorithmModes`
  (semantics not stated anywhere in the header; values kept verbatim, no
  invented meanings beyond "selector consumed by X").
- `interior_point_solver.h` entry-point one-liners (optimize/solve/… as phase
  sequences) follow OptimizationProblemBase's usage; confirm wording.
- Everything else (all counter semantics, all @throws domains) was written by
  reading the corresponding code in this repo or the cited sibling headers;
  @throws on validate_sqp_options/pattern_hash/set_num_partitions/eval_nlp/
  upgrade_to_full/strto_* were each checked against the throwing body.
