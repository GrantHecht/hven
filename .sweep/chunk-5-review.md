# Chunk 5 accuracy review — `include/hven/core` (7) + `include/hven/drivers` (4)

Target: `hven-sweep` @ `2b8beb7` ("docs: comment sweep — include/hven/core and
include/hven/drivers"), parent `bbbe4c8`. Read-only; no builds, no edits.
Method: full read of the eleven post-commit files, `git diff bbbe4c8 2b8beb7`
for every deletion, and verification of every load-bearing claim against the
implementation TUs (`src/drivers/interior_point_solver.cpp`,
`interior_point_solver_settings.cpp`, `sqp_options.cpp`, `sqp_driver.cpp`,
`src/core/pattern_hash.cpp`, `src/core/ledger.cpp`, `include/hven/detail/qp/*`).

## Verdict

**CHANGES REQUESTED — one fix round before this lands.**

The rewrite is, on the whole, careful and materially better than what it
replaced: history and milestone narrative are gone, the big
`sqp_driver.h` banner condensed without losing the convergence-test formulas,
the dual-sign ("B-1") clear, the zero-major `eval_hess` accounting, the
one-shot-per-failure-chain bound, the WarmStart ingest levels or the
thread-safety statements; the two `SsnCounters` stale-history recasts the
report flags are correct against the shipped code; and every "Default X"
claim in the four drivers headers matches its actual initializer (checked
mechanically, zero mismatches). `pattern_hash.h`, `solver_status.h`,
`start_level.h` (bar one garble), `ledger.h` and `types.h` are accurate
end-to-end.

What blocks it is concentrated in one place: the `InteriorPointSolver::Settings`
fields the report itself flagged as "name/group-derived one-liners". Five of
those are wrong about what the field does, not merely thin — and one of them
(`cnr_mode_`) is wrong in a way that would send a reader looking for a console
option when the field pins MKL's reproducibility thread count. Two public entry
points lost a case. One condensation in `start_level.h` dropped a sentence's
subject and inverted its reading.

- Blocking findings: **9**
- Non-blocking findings: **13**
- Public declarations with no docstring after this commit, four drivers headers:
  **116** (`interior_point_solver.h` 72, `sqp_driver.h` 37, `sqp_types.h` 7,
  `optimization_problem_base.h` 0)
- Doxygen form: **324** `///` blocks total — **5** with an explicit `@brief`,
  **319** relying on autobrief

---

## A. Blocking findings

### B1 — `cnr_mode_` is documented as a console-colour switch; it is MKL Conditional Numerical Reproducibility
`include/hven/drivers/interior_point_solver.h:477-478`

Written:
```
/// Colorize console output. Default false.
bool cnr_mode_ = false;
```
Actual (`src/drivers/interior_point_solver.cpp:230`):
```cpp
opts.cnr_threads = settings_.cnr_mode_ ? settings_.qp_threads_ : 0;
```
and `include/hven/detail/interior/kkt_factorization.h:64-77`, which discusses
`cnr_threads` explicitly as "Reproducibility-mode threading", baked into the
backend session's parameter array at transcribe time and not refreshable live.

The field has nothing to do with console output. The report marked this
one **uncertain**; it is wrong. Correct text, roughly:

> Conditional Numerical Reproducibility mode. When true the sparse backend is
> pinned to `qp_threads_` CNR threads (`opts.cnr_threads`), which makes the
> factorization bit-reproducible across thread counts; false leaves
> `cnr_threads` at 0 (off). Read once, at `set_qp_params()` transcribe time —
> a later `qp_threads_` change does not move it (see
> `KktFactorization::set_num_threads`'s asymmetry note). Default false.

### B2 — `max_acc_iters_` is documented as a cap; it is a required consecutive-run length
`include/hven/drivers/interior_point_solver.h:210-211`

Written: `/// Acceptable-iterate iteration cap. Default 50.`

Actual (`src/drivers/interior_point_solver.cpp:1598-1607`): `converge_check()`
walks the trailing `max_acc_iters_` iterates and declares
`ConvergenceFlags::ACCEPTABLE` only if **all** of them are acceptable. The
in-repo prose at `interior_point_solver.cpp:74-76` says so: "converge_check()
applies it over a trailing window of `max_acc_iters_` iterates to declare
ConvergenceFlags::ACCEPTABLE".

"Cap" inverts the knob: raising it makes ACCEPTABLE *harder* to reach, whereas
a reader will read "cap" as "how many iterations the acceptable tier is allowed
to run for". Correct text: "Number of consecutive trailing iterates that must
ALL sit inside the acceptable tolerances before `converge_check()` reports
ACCEPTABLE. Default 50. Must be > 0."

### B3 — `bound_interval_push_`'s docstring opens by describing `bound_push_`, and `bound_push_` lost its formula
`include/hven/drivers/interior_point_solver.h:358-371`

Pre-sweep this was a floating `//` block sitting between the two fields
(`bbbe4c8:.../interior_point_solver.h:325-334`), documenting both. The sweep
converted it to `///`, which formally attaches the *whole* block — first
paragraph included — to `bound_interval_push_`:

```
/// Absolute component of the interior push applied to a bounded primal
/// variable at solve entry: the push away from a bound is at least
/// bound_push_ * max(1, |bound|) (Ipopt's bound_push). It also caps the
/// initial slack the INIT multiplier pass hands an inequality row.
///
/// Relative component of that same push, ...
double bound_interval_push_ = 1.0e-2;
```
`bound_interval_push_` is not "the absolute component" and does not carry
`bound_push_ * max(1,|bound|)`. Meanwhile `bound_push_` (line 358-359) was
given a fresh one-liner — "Absolute interior-push coefficient. Default 1e-3." —
that drops both the formula and the INIT-slack cap the old block carried.

Fix: split the block. Paragraph 1 (with the formula and the INIT-slack
sentence) belongs on `bound_push_`; paragraph 2 stays on
`bound_interval_push_`. This is the one place in the chunk where turning `//`
into `///` changed what a docstring asserts.

### B4 — `force_qp_analysis_`: wrong cadence
`include/hven/drivers/interior_point_solver.h:481-482`

Written: `/// Force a fresh sparsity analysis each factorization. Default false.`

Actual: `claim_kkt_analysis()` (`interior_point_solver.cpp:253-260`) is called
**once per solve**, at `run_phase_sequence()` line 3570, and latches
`qp_analyzed_`. The flag defeats that latch, i.e. it forces a fresh symbolic
analysis on **every solve** rather than reusing the first one on this solver
instance. Nothing here is per-factorization — factorization re-runs go through
`factor_impl`, which never consults this field.

### B5 — `fast_factor_alg_`: describes a factorization-algorithm selector; it is the zero-perturbation cycling heuristic
`include/hven/drivers/interior_point_solver.h:479-480`

Written: `/// Fast factorization algorithm toggle. Default true.`

Actual (`interior_point_solver.cpp:2451-2467`): when on, and past iteration 6,
on 3 of every 4 iterations, it **skips the unperturbed factorization attempt**
if the last four iterations all needed Hessian perturbation — a cycling
heuristic that saves a wasted factorization on a persistently near-singular
problem, and periodically re-probes.

Beyond being wrong, the current wording collides head-on with `qp_alg_` five
lines away, documented as "QP factorization algorithm variant. Default Classic."
A reader cannot tell the two apart.

### B6 — `optimize_solve()` / `solve_optimize_solve()` drop the conditional trailing phase
`include/hven/drivers/interior_point_solver.h:808-811`; mirrored at
`include/hven/drivers/optimization_problem_base.h:75-78`

Written: `/// Runs OPTIMIZE then SOLVE from `x`.` and
`/// Runs SOLVE, OPTIMIZE, then SOLVE from `x`.`

Actual (`interior_point_solver.cpp:3763-3779`): in both sequences the trailing
SOLVE phase is constructed with `/*conditional_=*/true`, and
`run_phase_sequence()` at line 3600 skips a conditional step whenever the
preceding phase reported `ConvergenceFlags::CONVERGED`
(`interior_point_solver.h:1245`: `bool conditional_ = false; // skip if
converge_flag_ == CONVERGED`). `solve_optimize()` has no conditional step and
its docstring is correct; these two lose the case. Same omission propagates
into `OptimizationProblemBase::solve_optimize_solve()` /
`::optimize_solve()`'s one-liners.

### B7 — `start_level.h` condensation dropped a sentence's subject and inverted its reading
`include/hven/core/start_level.h:45-47`

Written:
```
///     * THE FACTORIZATION (`hot`): the hash exists precisely to protect
///       factorization reuse, so no hash may never reach kHot
///       (kSeeded < kWarm < kHot).
```
Parent (`bbbe4c8:.../start_level.h:87-89`): "an object that cannot produce a
hash may **never** reach kHot". The condensation deleted the subject, leaving a
double negative that reads as "no hash is barred from kHot" — the opposite of
the rule. Fix: "an object with no hash can never reach kHot".

### B8 — `JetJobModes::DoNothing` documented as "run nothing"; both dispatchers throw on it
`include/hven/drivers/optimization_problem_base.h:36-38`

Written: `/// No solve; run nothing (accepted by strto_jet_job_mode only).`

Actual: `jet_run()`'s switch (lines 141-167) has no `DoNothing` case, so it
falls to `default:` and throws `std::invalid_argument("Unrecognized
jet_job_mode")` — after `jet_initialize()` has already run and before
`jet_release()` does. `run_nlp_solver()` (line 249) throws likewise. The
parenthetical hedge does not rescue "run nothing": setting this mode and
calling `jet_run()` is a throw, not a no-op. Either say that, or drop the
sentence.

*(Adjacent code observation, reported not fixed: `jet_run()` calls
`jet_release()` only on the success path. Every throw inside the switch — and
anything the dispatched mode throws — leaks whatever `jet_initialize()`
acquired. The docstring's "Runs the configured job mode between
jet_initialize()/jet_release()" promises a bracket the code does not keep.)*

### B9 — `OptimizationProblemBase::solve()` / `::optimize()` one-liners are wrong, and `optimize()` imports a foreign concept
`include/hven/drivers/optimization_problem_base.h:69-72`

Written:
```
/// Solve the NLP's dynamics/constraints once at the current variables.
virtual hven::ConvergenceFlags solve() = 0;
/// Optimize the current phase's cost over the current variables.
virtual hven::ConvergenceFlags optimize() = 0;
```
Two problems. (a) "once at the current variables" — the corresponding
`InteriorPointSolver::solve(x)` runs a whole `soe_mode_` phase sequence to
convergence; nothing about it is a single evaluation, and both take/return a
variable vector rather than acting on ambient state. (b) "the current
**phase**'s cost" — hven has no phase concept at the NLP level; `Phase` is
tycho's. The rules forbid letting another codebase's vocabulary become an hven
claim, and this is that in the opposite direction. Both were on the report's
own "written under uncertainty" list. Suggested: "Runs the feasibility
(SOE-mode) phase sequence for the derived problem's NLP." / "Runs the
optimality (OPT-mode) phase sequence for the derived problem's NLP."

---

## B. Non-blocking findings

### N1 — Eight befriended test fixtures, and the two test files cited for them, do not exist in this repo
`include/hven/drivers/interior_point_solver.h:41-83`, `:975-993`

Present and verified: the four `RecoveryDispatchGate_*` fixtures
(`tests/interior/test_recovery_dispatch_gate.cpp`).

Not found anywhere under `tests/` or `src/`:
`FeasibilitySwitch_ProximalSwitchConstructsRestorationAndWrapsRecovery_Test`,
`FeasibilitySwitch_OffModeConstructsNoRestoration_Test`,
`FeasibilitySwitch_FilterSeedsRestorationConstraintTol_Test`,
`NestedSeamHarness`, `NestedSeamIneqHarness`, `DivergencePersistenceHarness`,
`SocGenericHarness`, `NativeBoundsHarness`, and all three
`InertiaRegularizationSolve_*`. (`NestedLifecycleHarness` is declared in
`detail/globalization/feasibility_switch_recovery.h` but has no definition.)
The cited files `test_inertia_regularization.cpp` (line 76) and
`test_soc_generic_acceptance.cpp` (line 69) do not exist.

The sweep reshaped these blocks without checking their actors — precisely the
class the rules say to drop when unverifiable. Either the fixtures live
somewhere this checkout does not see, or the comments (and the friend
declarations) are stale.

### N2 — Four dangling document cross-references
- `include/hven/drivers/sqp_driver.h:2972` → `docs/notes/2026-08-02-controller-retry-economics.md` — absent
- `include/hven/drivers/sqp_types.h:~590` → `docs/notes/2026-08-03-crash-basis.md` — absent
- `include/hven/core/solver_counters.h` and `sqp_driver.h` → `docs/notes/2026-08-07-ssn-safeguards.md` — absent
- `include/hven/drivers/interior_point_solver.h:446` → `docs/dev/analysis/2026-07-pr9-pardiso-options.md` — that file exists in **tycho**, not hven; the path is wrong for this repo

(`docs/notes/2026-07-29-eqp-refinement-ab.md` in `solver_counters.h` likewise
does not resolve.) All 111 other file/path references in the eleven files
resolve.

### N3 — `tests/test_*.cpp` references systematically omit the subdirectory
Every `tests/test_sqp_driver.cpp`, `tests/test_warm_start.cpp`,
`tests/test_sqp_restoration.cpp`, `tests/test_hs_battery.cpp`,
`tests/test_b1_gate.cpp` in `sqp_types.h` / `sqp_driver.h` / `solver_status.h`
is really `tests/sqp/…`. Pre-existing and repo-wide; a mechanical fix, worth
folding in while the files are open.

### N4 — `MIN_NNZ_PER_PARTITION` names nothing
`include/hven/drivers/optimization_problem_base.h:82-83`: "make_nlp() further
caps this via MIN_NNZ_PER_PARTITION on small problems." The identifier appears
nowhere in hven **or** in tycho, and hven's `NonLinearProgram::make_nlp(int,
int, int)` does no partition capping. Pre-existing; survives the sweep.

### N5 — `Settings::validate()`'s `@throws` domain omits the two combination guards
`include/hven/drivers/interior_point_solver.h:489-496`. The docstring lists
"per-field conditions … plus cross-field invariants (min_mu <= init_mu <=
max_mu, convergence tols <= acceptable <= divergence)". `validate()` also
rejects (a) `acceptance_strategy_ ∈ {funnel, filter}` with
`barrier_governor_ == classic_adaptive` and `!never_monotone_`, and (b)
`never_monotone_ && barrier_governor_ == monitored`
(`interior_point_solver_settings.cpp:581-598`) — the two failures a user is
most likely to hit, and both referenced by name elsewhere in this very header
("validate() rejects that combination", lines 313-324). Pre-existing text; the
sweep added the `@throws` line without widening the enumeration.

### N6 — `validate_sqp_options`'s `@throws` drops the `+inf` exemption
`include/hven/drivers/sqp_types.h:686-691` says "…a tr_max below tr_init…".
`src/drivers/sqp_options.cpp:106` guards that check with
`!std::isinf(opts.tr_init)`, so a `+inf` `tr_init` is exempt. The
`SqpOptions::tr_max` field docstring states the exemption correctly; the
function-level summary does not.

### N7 — `SqpDriver`'s constructor `@throws` drops two cases
`include/hven/drivers/sqp_driver.h:2772-2777`: "(non-positive tolerance,
negative max_iter, non-positive/NaN radius)". The constructor calls
`validate_sqp_options`, which also rejects `tr_max < tr_init` and a `tr_min`
outside `(0, min(tr_init, tr_max)]`. Faithful to the parent text; still an
incomplete domain on a public constructor.

### N8 — `SolveResult::reset_accumulators()` claims less than it does
`include/hven/drivers/interior_point_solver.h:700-701`: "Resets **only** the
accumulated timing/iteration counters and the convergence flag." The body
(707-730) also clears `last_kkt_info_`, `soc_steps_taken_`,
`watchdog_activations_`, `recovery_depth_histogram_`, all eight `last_*`
diagnostics and `last_eval_exception_`. Pre-existing wording, carried across
verbatim into the `///` form.

### N9 — `ONE TRIAL` step 2 contradicts the shared restoration budget
`include/hven/drivers/sqp_driver.h:39-40`: "opts.max_iter bounds SUBPROBLEMS
SOLVED …, i.e. exactly counters.major_iters." `SqpOptions::max_iter`'s own
docstring (`sqp_types.h:257-260`) and the restoration note both state the
bounded quantity is `major_iters + restoration_iters`. Pre-existing and
unchanged by the sweep, but it now sits two lines from a `///`-quality
contract and reads as authoritative.

### N10 — Report overclaims an `@throws` on `upgrade_to_full`
The report states "`@throws` on validate_sqp_options / pattern_hash /
set_num_partitions / eval_nlp / upgrade_to_full / strto_* were each checked
against the throwing body." `pattern_hash`, `set_num_partitions`, `eval_nlp`,
`validate_sqp_options` and the four `strto_*` all do carry a verified `@throws`
(each checked here against its body — all correct). `upgrade_to_full`
(`sqp_driver.h:1769`) carries **no `///` block at all**; its documentation is
still `//` prose and it has no `@throws`, despite three
`throw std::invalid_argument` sites. Same for `eval_nlp_values`
(`sqp_driver.h:1717`) and `evaluate_kkt`'s three entries.

### N11 — Residual labels (three classes, 13 sites)
1. **`B-1`** — a plan label (`src/drivers/sqp_driver.cpp:774` shows its origin:
   "B-1 REPAIR (Phase-5 Task 7b)"). Nothing in the swept headers defines it;
   the section it names is titled "THE INGESTED MULTIPLIERS ARE MADE
   COMPLEMENTARY". Sites: `sqp_driver.h:533, 2480, 2689, 2696, 2697`;
   `sqp_types.h:514`.
2. **`R1` / `R2` / `R4` / `R5`** — spec requirement labels used as section
   headings and as a field docstring's opening token, defined nowhere in the
   repo. Sites: `sqp_types.h:51, 74, 89, 630, 663`; `sqp_driver.h:2536`.
   Suggested: keep the descriptive heading, drop the label.
3. **`sqp_types.h:603`** — "the remaining 21 are a probe tabulated in **the
   chunk report that introduced the lever**" — a reference to the sweep/plan
   process itself.

("Pardiso phase-11", "ssn_engine.h section 7b", "l1_restoration.h disclosure
(a)/(f)", `[KLV]`/`[KD]` are *not* residual labels — the first is MKL's own
term and the rest resolve to real sections in the cited headers. `A/B lever` is
methodology vocabulary, not a plan label; leaving it is defensible.)

### N12 — A `///` block with no `@brief`
`include/hven/drivers/sqp_driver.h:3015-3039`: the 4-argument
`solve(model, x0, warm, minor_budget)` gets a `///` block that opens directly
with `@throws`. The whole ingest contract, probe-budget contract and parameter
list for that overload sit above it in `//` prose (2903-3014), invisible to
Doxygen. Its bridge-taking sibling at 3047-3059 does have `@brief`/`@param`/
`@return`/`@throws` — the two should match. Same shape at
`sqp_driver.h:2806-2817` (`solve(const NlpModel&)`: `//` only, no `@throws`
despite routing through the same validation the 2-arg overload documents).

### N13 — Two removed rationale chains are not in the report's candidate list
`sqp_driver.h` parent `:600-605` (why every `||lambda_i||`-relative
complementarity threshold was rejected, including the measured IPOPT-style
capped form) and parent `:1104-1115` (the filterSQP magnitude-gate candidate
for SOC). Both are genuine argument chains, both deleted, neither appears among
the report's eight "rationale candidates for docs/notes". Nothing false
survives in their place — the deletions are clean — but the controller should
capture them.

---

## C. What was verified correct (no finding)

Recorded so the fix round does not re-litigate these:

- `evaluate_kkt`'s stationarity/feasibility/complementarity formulas,
  `at_lower`/`at_upper` tests and the NaN-yields-NaN contract, against
  `detail::evaluate_kkt_over` — exact, including the fixed-variable arm.
- The zero-major `eval_hess` accounting, the `2 + 2*[me>0] + 2*[mi>0]` call
  count, and the "a rejected trial costs no eval_hess" claim.
- `degenerate_run_max`'s reset set (ordinary non-degenerate step / ride /
  start repair reset; drop and zero-multiplier probe do not) against
  `qp_engine.h:2231, 2458, 2479-2482`.
- The `SsnCounters` recasts: `kFull` is the shipped default
  (`ssn_engine.h:1051`), the three safeguard counters are `guarded`-gated, and
  `SqpCounters::ssn` is zero at `qp_mode == kWalk`. Both stale-history claims
  the report flagged were correctly repaired.
- `SsnCounters` has exactly six core fields; `SqpSolveRecord`'s "three of six"
  split and every "Duplicate of SqpCounters::X" name resolves to a real field.
- `Ledger::summary_table()`'s column list against `src/core/ledger.cpp:60-69`.
- `pattern_hash`'s `@throws std::invalid_argument if !A.isCompressed()`,
  the "pattern_hash(A) is exactly Fnv1a h; feed_pattern(h,A); h.value()"
  identity, and the ingredient order — all exact.
- All four `strto_*` accepted-spelling lists, including the `MTMETIS`→PARMETIS
  alias and the `ECon`/`ICon`/`Prim Obj` variants.
- `OptimizationProblemBase::set_num_partitions`'s `@throws`,
  `default_num_partitions`'s `@return`, and `get_core_count()` being physical
  cores.
- Every "Default X" claim in the four drivers headers against its initializer:
  **zero mismatches** (mechanical sweep over all `///` blocks).
- No `@param` names a non-existent parameter; no `@return` on a `void`; no
  orphaned/floating `///` block anywhere in the eleven files.
- Attributions preserved rather than absorbed: Ipopt (`bound_push`,
  `bound_frac`, `bound_relax_factor`, `n_filter_resets_`, never-monotone,
  "ships no divergence abort"), Wächter & Biegler 2006, Chamberlain–Powell–
  Lemaréchal–Pedersen 1982, Conn–Gould–Toint BTR, `[KLV]`, `[KD]`,
  Byrd–Curtis–Nocedal, Gould 1985, "Uno omits SOC". No other solver's
  behaviour was turned into an hven claim anywhere in the diff.
- Thread-safety callouts intact (`sqp_driver.h:2960-2961, 3245`;
  `optimization_problem_base.h:127-132`'s Jet worker-thread rule).
- The header-ruler fragment left over at the top of `interior_point_solver.h`
  was correctly removed.

---

## D. Public-surface coverage — declarations with NO docstring after this commit

Input for the fix round, not a finding in itself.

**`interior_point_solver.h` — 72**

| What | Lines | Count |
|---|---|---|
| Validated setters (`set_max_iters` … `set_accel_zero_tolerance`) | 814-886 | **58** |
| `apply_preset` (has a `//` block, no `///`) | 902 | 1 |
| `InteriorPointSolver()`, `InteriorPointSolver(shared_ptr<NonLinearProgram>)`, `~InteriorPointSolver()` | 766-768 | 3 |
| Deleted copy/move ctors and assignments | 777-780 | 4 |
| `struct Settings`, `struct SolveResult` (banner `//` only) | 204, 502 | 2 |
| `using VectorXd` | 733 | 1 |
| `last_prox_reg_dual_`, `staged_iq_mults_`, `mults_staged_` (share a preceding field's block) | 681, 953, 954 | 3 |

The 58 setters are the headline: they are the documented-configuration surface
a Python/tycho caller reaches first, every one of them validates and throws,
and none carries a `@throws`, a unit, or a range.

**`sqp_driver.h` — 37**

`struct NlpEval` + its `f` / `grad,ce,ci` / `Je,Ji` field lines (1614-1617, 4);
`eval_nlp_values` (1717); `upgrade_to_full` (1769); `constraint_violation_l1`
(1822); `struct SqpKkt` + `stationarity`/`feasibility`/`complementarity`/
`grad_lag`/`z`/`residual()` (1844-1856, 7); `evaluate_kkt` ×3 (1970, 1979,
2182); `build_subproblem` ×2 (2017, 2040); `predicted_decrease` (2066);
`crash_basis_seed` (2120); `qp_failure_is_retryable` (2218);
`kSsnTrViolationFactor` (2385); `ssn_exit_is_a_usable_step` (2400);
`ssn_result_to_qp_solution` (2442); `ssn_start_from_qp_seed` (2485);
`ssn_fb_tol_for` (2511); `charge_ssn_subproblem_cost` (2531);
`charge_refused_face_refinement` (2566); `accumulate_ssn_counters` (2590);
`kAdaptiveMuKappa` + `kAdaptiveMuMin` (2679-2680, 2); `kSeededDualClampTol`
(2751); `class SqpDriver` (2768); `SqpDriver::solve(const NlpModel&)` (2817);
`to_string(StepVerdict)` (3295); `format_iteration_table` (3315).

Context: this file has **136** `///` lines against **2374** `//` lines. Every
one of the declarations above is preceded by substantial and largely accurate
`//` prose — the information is there, it is simply not in a form Doxygen will
attach. This is the single largest gap in the chunk, and the cheapest to close
(convert the immediately-preceding `//` block, no rewriting).

**`sqp_types.h` — 7**

`ssn_hint_rule` (669), `ssn_infeasibility_rule` (670) — both share
`ssn_sigma_rule`'s block; `SqpSolution::status` (934),
`x, lambda_e, lambda_i, z` (935), `f` (936), `counters` (937), `history` (938).

**`optimization_problem_base.h` — 0.** Fully covered (its problems are
accuracy, B8/B9/N4, not coverage). One gap of a different kind:
`run_nlp_solver` (189) has a `///` block but no `@throws`, and its `default:`
arm throws.

**Core files, for completeness (not requested but same input):**
`solver_counters.h` **27** — `QpCounters`' five leading fields (46-50),
`SsnCounters`' six escape-census fields (467-472), `SqpCounters`' twelve leading
fields (659-670), `border_refine_steps` (684), `evals_full`/`evals_values`
(852-853), `crash_seeded_bounds` (920). All are covered by a preceding
struct-level or group `///` block that Doxygen attaches only to the struct or
to the group's first member.
`types.h` **3** (`VecRef`, `ConstMatRef`, `MatRef` share `ConstVecRef`'s block);
`ledger.h` **1** public (`Ledger()`); `start_level.h`, `solver_status.h`,
`version.h`, `pattern_hash.h` **0**.

Systematic, across both directories: **no enum carries per-enumerator `///`
docs** (`QpStatus` ×4, `SqpStatus` ×5, `StartLevel` ×4, `QpMode` ×2,
`SsnSigmaRule` ×3, `SsnHintRule` ×2, `SsnInfeasibilityRule` ×3, `StepVerdict`,
`BarrierModes`, `LineSearchModes`, `AlgorithmModes`, `QPAlgModes`,
`QPOrderingModes`, `BestCriteriaModes`, `QPPivotModes`, `PDStepStrategies`).
Each is documented in the enum's own block instead — legible, but the
enumerator pages render empty. Two exceptions the sweep did do per-enumerator:
`JetJobModes` (`optimization_problem_base.h:36-48`) and `SsnSafeguards`
(`ssn_engine.h`, out of chunk). Worth a controller ruling on which convention
wins. The report itself flags `QPAlgModes`, `QPPivotModes`, `PDStepStrategies`
and `AlgorithmModes` as having no stated semantics anywhere in the header —
that is confirmed; their enumerator names are the only documentation that
exists.

---

## E. Doxygen form counts

Per file: `///` blocks / with explicit `@brief` / autobrief.

| File | Blocks | `@brief` | autobrief |
|---|---|---|---|
| core/types.h | 5 | 0 | 5 |
| core/version.h | 1 | 0 | 1 |
| core/solver_status.h | 3 | 0 | 3 |
| core/start_level.h | 8 | 0 | 8 |
| core/pattern_hash.h | 9 | 0 | 9 |
| core/ledger.h | 29 | 0 | 29 |
| core/solver_counters.h | 39 | 0 | 39 |
| drivers/optimization_problem_base.h | 33 | 1 | 32 |
| drivers/sqp_types.h | 50 | 0 | 50 |
| drivers/interior_point_solver.h | 137 | 2 | 135 |
| drivers/sqp_driver.h | 10 | 2 | 8 |
| **Total** | **324** | **5** | **319** |

The rubric in `.sweep/comment-rules.md` asks for `/// @brief …` then
`@tparam`/`@param`/`@return`/`@throws` as applicable. 98% of the blocks are
autobrief. Note the two shapes are not equivalent for multi-paragraph blocks:
under autobrief the brief is only the first *sentence*, and most of these
blocks run 5-40 lines, so the rendered brief is frequently a fragment
("Duplicate of SqpCounters::soc_steps (which IS the attempts count).",
"Solves that resolved to kCold.", "R1/R2/R4."). Recommendation for the
controller: normalise to explicit `@brief` for any block longer than one
sentence, leave true one-liners on autobrief. Mechanical, and it does not touch
any of the accuracy findings above.

---

## F. Recommended fix-round scope

1. **Must**: B1-B9. Five are single-field rewrites in `interior_point_solver.h`
   Settings, two are one-clause additions to entry-point docstrings, one is a
   four-word repair in `start_level.h`, one is `optimization_problem_base.h`'s
   three virtuals + `DoNothing`.
2. **Should**: N11 (strip `B-1`, `R1/R2/R4/R5`, the chunk-report reference);
   N2/N3 (fix or drop the dangling paths); N10/N12 (give
   `upgrade_to_full`/`eval_nlp_values`/`evaluate_kkt` and the 4-arg `solve`
   real `///` blocks with `@brief` + `@throws`); N5-N9 (widen the four
   incomplete `@throws`/reset domains).
3. **Should, mechanical**: the `sqp_driver.h` 37 and the
   `interior_point_solver.h` 58 setters — convert existing `//` prose to `///`,
   and give the setters `@param`/`@throws` from `interior_point_solver_settings.cpp`'s
   `pos_int`/`pos_finite`/`in_open_unit`/`greater_than` helpers, which already
   carry the exact predicate and message per field.
4. **Controller ruling wanted**: N1 (do the missing test fixtures exist
   elsewhere, or should the comments and the `friend` declarations go?); the
   per-enumerator convention in §D; and whether `@brief` normalisation (§E) is
   in scope for this chunk or a follow-up sweep.
