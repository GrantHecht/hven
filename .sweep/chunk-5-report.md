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

---

## Fix round — accuracy review response

Applied against `.sweep/chunk-5-review.md`. Every fix re-verified at the
implementation (`src/drivers/interior_point_solver.cpp`,
`interior_point_solver_settings.cpp`, `sqp_options.cpp`, `sqp_driver.cpp`).
Comment-only: the strip-compare gate reports 0 violations against the fix
round's start commit.

| Finding | File:decl | What changed | Verified at |
|---|---|---|---|
| B1 | `interior_point_solver.h` `Settings::cnr_mode_` | Was "colorize console output". Now: MKL Conditional Numerical Reproducibility — true pins the sparse backend to `qp_threads_` CNR threads; read once at `set_qp_params()` transcribe time | `interior_point_solver.cpp:230`; `detail/interior/kkt_factorization.h`'s refresh-cadence note |
| B2 | `interior_point_solver.h` `Settings::max_acc_iters_` | Was "acceptable-iterate iteration cap". Now: the count of consecutive trailing iterates that must ALL be acceptable before ACCEPTABLE fires; raising it makes ACCEPTABLE harder | `interior_point_solver.cpp:1598-1607`, `:74-76` |
| B3 | `interior_point_solver.h` `bound_push_` / `bound_interval_push_` | Block split: paragraph 1 (formula + INIT slack) moved onto `bound_push_`, paragraph 2 stays on `bound_interval_push_`; each now carries its own domain | `interior_point_solver.cpp:427-447`, `:3279-3287`; `interior_point_solver_settings.cpp:657,664` |
| B4 | `interior_point_solver.h` `force_qp_analysis_` | Was "each factorization". Now: defeats the once-per-solver-instance `claim_kkt_analysis()` latch, i.e. forces a fresh SYMBOLIC analysis every solve; numeric refactorizations never consult it | `interior_point_solver.cpp:253-260`, `:3570` |
| B5 | `interior_point_solver.h` `fast_factor_alg_` | Was "fast factorization algorithm toggle" (collided with `qp_alg_`). Now: the zero-perturbation-attempt cycling heuristic (past iter 6, 3 of 4 iterations, skip when the last four all perturbed) | `interior_point_solver.cpp:2451-2467` |
| B6 | `interior_point_solver.h` `optimize_solve` / `solve_optimize_solve`; same two on `optimization_problem_base.h` | Trailing phase documented as conditional — skipped when the preceding phase reported CONVERGED | `interior_point_solver.cpp:3763-3779`, `:3600` |
| B7 | `core/start_level.h:46` | Subject restored: "an object with no hash can never reach kHot" | parent text `bbbe4c8` |
| B8 | `optimization_problem_base.h` `JetJobModes::DoNothing` | Was "run nothing". Now: parsed by `strto_jet_job_mode` but dispatched by nothing — `jet_run()` and `run_nlp_solver()` both throw `std::invalid_argument` | `optimization_problem_base.h:141-167, 249` |
| B9 | `optimization_problem_base.h` `solve()` / `optimize()` | Rewritten in hven terms: SOE-mode / OPT-mode phase sequences. Tycho's "phase" vocabulary removed | `interior_point_solver.cpp:3746-3757` |
| N1 | `interior_point_solver.h:69, :76` | Dropped the two citations of test files absent from this checkout (`test_soc_generic_acceptance.cpp`, `test_inertia_regularization.cpp`). Friend declarations untouched — they are code | `tests/` listing |
| N2 | 5 sites | Dropped 5 dangling `docs/` references (2× `2026-08-07-ssn-safeguards.md`, `2026-07-29-eqp-refinement-ab.md`, `2026-08-02-controller-retry-economics.md`, `2026-08-03-crash-basis.md`) and the tycho-only `docs/dev/analysis/2026-07-pr9-pardiso-options.md`. Surrounding facts kept | `ls docs/notes/` |
| N3 | 16 sites | `tests/test_*.cpp` → `tests/sqp/test_*.cpp` across `solver_counters.h`, `solver_status.h`, `sqp_driver.h`, `sqp_types.h`. All 5 filenames exist there | `ls tests/sqp/` |
| N4 | `optimization_problem_base.h:82` | Dropped the `MIN_NNZ_PER_PARTITION` sentence — the identifier exists nowhere and `make_nlp` does no partition capping | `model/non_linear_program.h:267` |
| N5 | `interior_point_solver.h` `Settings::validate()` | `@throws` domain widened with the two combination guards (funnel/filter + classic_adaptive + !never_monotone; never_monotone + monitored) | `interior_point_solver_settings.cpp:581-598` |
| N6 | `sqp_types.h` `validate_sqp_options` | `@throws` now states the `+inf tr_init` exemption from the `tr_max >= tr_init` check | `sqp_options.cpp:106` |
| N7 | `sqp_driver.h` `SqpDriver(const SqpOptions &)` | `@throws` widened to the full `validate_sqp_options` domain (tr_max/tr_min cases added) | `sqp_options.cpp:76-132` |
| N8 | `interior_point_solver.h` `SolveResult::reset_accumulators()` | "only the … counters and the convergence flag" corrected: also `last_kkt_info_`, SOC/watchdog/recovery counters, all `last_*` diagnostics, `last_eval_exception_` | body at the declaration |
| N9 | `sqp_driver.h:39` ONE TRIAL step 2 | max_iter now stated as bounding `major_iters + restoration_iters`, agreeing with `SqpOptions::max_iter` | `sqp_driver.cpp:1184, 1260, 1336` |
| N11 | 13 sites | Labels stripped, facts kept: `B-1` → "the geometric complementarity clear" (6 sites); `R1/R2/R4/R5` → descriptive headings (6 sites); `sqp_types.h:603` sweep self-reference → "the remaining 21 rows are an un-committed probe" | `sqp_driver.cpp:774` (label origin) |
| N12 | `sqp_driver.h` 4-arg `solve(model, x0, warm, minor_budget)` | Given `@brief`/`@param`×4/`@return`; its `@throws` no longer duplicates the 2-arg overload's 20-line enumeration verbatim, it cites it and keeps the ingest-order NOTE | the sibling overload at `:2875` |

Deliberate deviation from the review's suggested wording, one place: B3's
"INIT-slack cap". The INIT pass sets `slack = max(-c_i(x), bound_push_)`
(`interior_point_solver.cpp:3279-3287`), so `bound_push_` is a FLOOR on that
slack, not a cap. The restored sentence says floor.

Not in this round (coverage, not accuracy — they land in the documentation
commit): N10 (`upgrade_to_full`, `eval_nlp_values`, `evaluate_kkt` carry no
`///` block at all) and §D's 116 undocumented public declarations.

### Additional rationale candidates for docs/notes (N13)

- `sqp_driver.h` parent `bbbe4c8:600-605` — why every `||lambda_i||`-relative
  complementarity threshold was rejected, including the measured IPOPT-style
  capped form.
- `sqp_driver.h` parent `bbbe4c8:1104-1115` — the filterSQP magnitude-gate
  candidate for SOC, and why it was not taken.

---

## Documentation round — the public solver and driver surface

Second commit of the fix round. Adds a Doxygen block to every public
declaration the review's §D listed as undocumented, and normalises autobrief
one-liners on public declarations to explicit `/// @brief`. Comment-only; the
strip-compare gate reports 0 violations.

### Declarations documented: 117

| File | Count | What |
|---|---|---|
| `drivers/interior_point_solver.h` | **72** | the 58 validated setters; `apply_preset`; the two constructors and the destructor; the four deleted copy/move members; `struct Settings` and `struct SolveResult` (banner comments replaced by docstrings); `using VectorXd`; `last_prox_reg_dual_`, `staged_iq_mults_`, `mults_staged_` |
| `drivers/sqp_driver.h` | **38** | `struct NlpEval` + its three field lines; `eval_nlp_values`; `upgrade_to_full`; `constraint_violation_l1`; `struct SqpKkt` + its five fields and `residual()`; `evaluate_kkt` ×3; `build_subproblem` ×2; `predicted_decrease`; `crash_basis_seed`; `qp_failure_is_retryable`; `kSsnTrViolationFactor`; `ssn_exit_is_a_usable_step`; `ssn_result_to_qp_solution`; `ssn_start_from_qp_seed`; `ssn_fb_tol_for`; `charge_ssn_subproblem_cost`; `charge_refused_face_refinement`; `accumulate_ssn_counters`; `kAdaptiveMuKappa`/`kAdaptiveMuMin`/`kAdaptiveMuMax`; `kSeededDualClampTol`; `class SqpDriver`; `solve(const NlpModel &)`; `to_string(StepVerdict)`; `format_iteration_table` |
| `drivers/sqp_types.h` | **7** | `ssn_hint_rule`, `ssn_infeasibility_rule`; `SqpSolution`'s `status`, `x/lambda_e/lambda_i/z`, `f`, `counters`, `history` |

One more than §D's 116: `kAdaptiveMuMax` carried a trailing `//` comment
rather than none, so §D did not count it, but it sits between the two
constants that were counted and is documented with them.

Every setter's `@throws` states the exact predicate its body checks, read off
`src/drivers/interior_point_solver_settings.cpp` — `pos_int` (`< 1`),
`pos_finite` / the inline `isfinite` guards (not finite, or `<= 0`),
`in_open_unit` (`<= 0` or `>= 1`), `greater_than` (`<= bound`),
`in_open_interval` / `in_closed_interval` (negated comparisons, so a NaN is
rejected), the explicit `< 0` and `!= 0 && != 1` guards, and
`check_fixed_variable_treatment`'s enum-set check. Where a helper's form does
NOT reject a NaN (`greater_than`, `in_open_unit`), the docstring states the
comparison rather than claiming NaN rejection.

Three setters take no validation at all and are documented with no `@throws`:
the three enum-taking mode setters (`set_qp_ordering_mode`, `set_opt_bar_mode`
/ `set_soe_bar_mode`, `set_opt_ls_mode` / `set_soe_ls_mode`,
`set_best_criteria` — the enum overloads assign directly). Their
string-taking siblings throw through the `strto_*` converters and say so.

### Autobrief → `@brief` conversions: 140

Single-line `///` blocks on a declaration, converted mechanically with the
text unchanged. Multi-line blocks were left on autobrief.

| File | Count |
|---|---|
| `core/ledger.h` | 18 |
| `core/pattern_hash.h` | 3 |
| `core/solver_status.h` | 2 |
| `core/start_level.h` | 5 |
| `core/types.h` | 2 |
| `drivers/interior_point_solver.h` | 75 |
| `drivers/optimization_problem_base.h` | 17 |
| `drivers/sqp_driver.h` | 7 |
| `drivers/sqp_types.h` | 11 |
| **Total** | **140** |
