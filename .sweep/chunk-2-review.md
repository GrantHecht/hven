# Comment-accuracy review — chunk 2 (interior) + chunk 1 fix round

Repo `/home/ghecht/Projects/hven-sweep`, branch `sweep/base`.

- **Section A** — `ea7efe8` over `3d37369`: chunk 2, 19 headers under
  `include/hven/detail/interior/` (incl. `typedefs/`, `utils/`).
- **Section B** — `fe2a396` over `ea7efe8`: chunk-1 fix round over
  `include/hven/detail/globalization/`, against `.sweep/chunk-1-review.md`
  (18 findings) and `.sweep/chunk-1-fix-brief.md`.

Rules applied: `.sweep/comment-rules.md`. Mechanical gate (comment-stripped diff
empty / zero code-token change) was already PASSED and is not re-litigated here.
Section B appears first below (it was reviewed first); Section A follows.

---

## Section B — chunk-1 fix round (fe2a396)

**Verdict: APPROVED WITH ITEMS — 2 blocking, 1 should-fix, 4 nits.**
16 of 18 findings addressed as specified; 1 legitimately deferred (11);
2 partially addressed, one of which introduced a NEW false docstring.

The two blocking findings of the original review — #1 (`basval`) and #2 (re-entry
condition) — were re-verified against the implementation and are both correctly
fixed. Nothing previously verified-true regressed.

### Disposition table

| # | finding | disposition |
|---|---------|-------------|
| 1 (blocking) | filter ceiling formula | **ADDRESSED — re-verified.** `filter_acceptance.h:94-97` now reads `> kFilterObjMaxInc + basval, where basval = log10(\|phi_ref\|) when \|phi_ref\| > 10 and 1.0 otherwise (only evaluated when phi_trial > phi_ref)`. Matches `interior_point_solver_globalization.cpp:2046-2051` term for term, including the `phi_trial > phi_current` guard. |
| 2 (blocking) | governor re-entry "no tiny step" | **ADDRESSED — re-verified.** `monitored_governor.h:73-77` now says re-entry is on the sufficient-progress test alone, with the tiny-step requirement explicitly attributed away ("The reference additionally requires … this engine tracks no tiny-step state and does not apply that extra condition"). `MonitoredBarrierGovernor::decide` (:2326-2332) re-enters free mode on `check_sufficient_progress(curr_error)` alone; `grep -i tiny src/` still returns nothing. Attribution rule satisfied. |
| 3 | "tagged below" + lost divergences | **ADDRESSED.** Clause dropped; items (4) theta_min/theta_max and (5) KLV Table 1 vs Uno defaults restored as a condensed `Deliberate deviations` block at `sqp/globalization.h:26-35`, including the beta 0.99-vs-0.9999 contraction consequence (0.995 vs 0.99995). *Nit B-a below on one word of the pointer.* |
| 4 | report: six-divergence block missing | **ADDRESSED.** `.sweep/chunk-1-report.md:78-85` now carries the full six-item summary with the pre-sweep line range. |
| 5 | report: total-lines column wrong | **ADDRESSED.** Table regenerated; spot-checked `acceptance_strategy.h 215 -> 140` (review measured 140 at f0fadf2, unchanged by the fix round) and `filter_acceptance.h 462 -> 269` (review measured 261; the fix round adds exactly 8 comment lines to that file → 269). Totals now `6555 -> 4479`. *Nit B-b below on the comments column.* |
| 6 | double `@brief` on GlobalizationStrategy | **ADDRESSED.** Generalization note demoted to `//` (`sqp/globalization.h:224-232`); a single `@brief Interface the driver holds.` remains on the class. |
| 7 | floating `///` narration | **PARTIALLY ADDRESSED — see BLOCKING B-1.** `sqp/globalization.h` fully fixed (algorithm block :48-82 and parameters line :84-85 both `//`). `inertia_regularization.h` is NOT fixed: only the first line of each paragraph was demoted, leaving an interleaved `//`/`///` block. |
| 8 | `BarrierDecision.mu` qualifier | **ADDRESSED.** `///<` restored at `monitored_governor.h:147-149` with the free-mode caveat; verified against `decide()` — `d.mu = mu_in` is returned untouched on both free-mode exits. The two added siblings (`mu_event`, `monotone`) are also true of the code. |
| 9 | elastic contamination premise | **ADDRESSED.** `sqp/elastic.h:205-206` restores "with z_j = 0 when s_j is off its bound"; the derivation follows again. |
| 10 | ownership sentences | **ADDRESSED.** Restored to `filter_acceptance.h:162-164`, `funnel_acceptance.h:97-99`, `l1_restoration.h:119-122`. |
| 11 | trailing whitespace | **LEGITIMATELY DEFERRED.** The review itself scoped this to a follow-up formatting pass. Residual count is now 13 lines (filter_acceptance 3, monitored_governor 6, sqp/globalization 3, switching_acceptance 1), down from 20 — the `///<` additions of #8/#18 consumed seven of them as a side effect. No code-token risk either way. |
| 12 | missing `@throws` | **PARTIALLY ADDRESSED + REGRESSED — see BLOCKING B-2.** Five of six sites correct and verified (filter both hooks, funnel both hooks, modern_merit both hooks, `GlobalizationStrategy::judge`). The sixth is wrong: the `logic_error` on an unknown `MeritPenaltyRules` was documented on the *constructor*, which does not throw. |
| 13 | unnamed `@param current` | **ADDRESSED.** Both `acceptance_strategy.h` hooks fold the sentence into the description; no `@param` names a parameter that does not exist. |
| 14 | l1_restoration docstring on wrong declaration | **ADDRESSED.** `ElasticSlackInit` now describes the result aggregate; the closed form + unit-testability rationale moved to `l1_elastic_slack_init`'s `@brief`. |
| 15 | kKappaResto citation | **ADDRESSED.** `acceptance_strategy.h:25-26` — Ipopt option `required_infeasibility_reduction`, shipped default 0.9; value in file is 0.9. |
| 16 | grouped constant docstrings mis-attached | **ADDRESSED.** `monitored_governor.h:19-25` and `recovery_chain.h:21-30` demoted to `//` with an explicit per-constant mapping. Both mappings verified: the six Ipopt option names match the six constants in declaration order (and `adaptive_mu_monotone_init_factor` is now spelled with its real prefix), and the depth values `Soc=0 / Extended=1 / Watchdog=2 / Unresolved=3 / Restoration=4` match the constants immediately below. |
| 17 | `barr_obj` undocumented | **ADDRESSED — verified.** `barrier_governor.h:95-97`; `BarrierGovernor::update_barrier_monotone` (:1286-1296) does write `barr_obj = psi` over the same `slacks`/`iq_lmults`/`dual_grad` segments the docstring names. |
| 18 | RejectionCause per-value mapping | **ADDRESSED.** One `///<` per enumerator at `switching_acceptance.h:29-32`, matching the mapping the review quoted from the pre-image. |

### Blocking

**B-1. `inertia_regularization.h:11-41` — finding 7 is applied to one line in
five, leaving an interleaved comment block that still mis-attaches.**

Reported APPLIED ("caveats block demoted to `//`"). What is in the tree is a
block whose paragraph-opening lines are `//` and whose continuation lines are
still `///`:

```
 11 // Proximal primal-dual KKT regularization: constants and pure-logic helpers
 13 // classic on-demand inertia ladder.
 14 ///
 15 /// Instead of first attempting an unperturbed factorization every iteration and
 ...
 23 // Numerical caveats every reader needs:
 24 // - The negative sign on the constraint block is the augmented-system
 25 ///   convention (W + delta_w*I / -delta_c*I): it keeps one negative eigenvalue
 26 ///   per constraint row, so the target inertia count is unchanged, while making
 27 //   a rank-deficient constraint Jacobian factorizable. The dual shift shrinks
 28 ///   to zero with mu and never masks non-convergence — convergence residuals
```

Two problems, both worse than the original finding. (i) The defect the finding
named is not removed: the surviving `///` runs at :14-22, :25-26, :28-29,
:31-32, :34-37 and :39-41 are still doc comments with no declaration between
them and `kProxRegFloor` (:46), so Doxygen still stacks them onto that constant
— which carries its own `@brief`. (ii) The block is now visually incoherent, with
the comment marker changing mid-sentence (:24→:25, :26→:27) — a reader will read
it as damage.

Correct text: demote **every** line of :11-41 to `//` (including the bare `///`
separators at :14 and :22), exactly as `sqp/globalization.h:48-85` was done.

**B-2. `modern_merit.h:105-106` — the fix round attached a `@throws` to a
constructor that cannot throw, and left the declaration that does throw
undocumented.**

New text:

```
    /// @brief Constructs with the penalty rule and a fresh state.
    /// @throws std::logic_error On an unknown MeritPenaltyRules value.
    explicit ModernMeritAcceptance(MeritPenaltyRules rule) : rule_(rule) { reset(); }
```

The constructor stores `rule_` and calls `reset()`.
`ModernMeritAcceptance::reset()` (`interior_point_solver_globalization.cpp:547-567`)
assigns constants and branches only on `in_feasibility_phase_` — it never
inspects `rule_` and contains no throw. The `logic_error("… unknown
MeritPenaltyRule")` lives at :666, in the `default:` arm of the `switch (rule_)`
inside `is_iterate_acceptable` — which is precisely the declaration the review
cited (`modern_merit.h:111` at ea7efe8) and which still has no `@throws`.

As written this is a new violation of the accuracy rule the fix round exists to
enforce: it promises an eagerly-validating constructor the code does not have,
and it hides that an unknown rule surfaces only on the first acceptance test.

Correct text: drop the `@throws` from the constructor (keep the one-line
`@brief`), and add to `is_iterate_acceptable` (`modern_merit.h:113`):
`@throws std::logic_error On an unknown MeritPenaltyRules value.`

### Should-fix

**B-3. `.sweep/chunk-1-report.md` fix-round table overstates two dispositions.**
Rows 7 and 12 are recorded as flat `APPLIED`. Given B-1 and B-2 they are partial
(7) and partial-plus-incorrect (12). Since the controller reads this table as the
record of what the review bought, correct both rows when the two blocking items
are fixed.

### Nits

**B-a. `sqp/globalization.h:18-20`** — "the two deliberate deviations … are
recorded **at the constants** below". They are recorded in the header comment
block immediately below (:26-35), not at the constants. Say "recorded below" or
"in the deviations note below".

**B-b. `.sweep/chunk-1-report.md` comments column** — the *total lines* column is
now correct (finding 5 discharged), but the *comments* column still disagrees
with the review's independent count on several rows (`filter_acceptance.h` 353 vs
357, `acceptance_strategy.h` 150 vs 151, `sqp/globalization.h` 647 vs 656 on the
before-values). A counting-convention difference (block-comment interior lines,
most likely), not a factual error about the sweep; note the convention in the
table caption rather than re-deriving.

**B-c. Rulers survived the original chunk-1 sweep in two files** —
`monitored_governor.h:145` and `restoration.h` (2 each,
`// ------------------------------------------------------------------------`).
Rule 1's "rulers (`// ====`)" clause deletes these; the original review did not
catch them and the fix round did not touch them. Carry-forward, not a fix-round
regression.

**B-d. Two cosmetic artifacts of the fix edits.** `sqp/elastic.h:206` is a
45-character orphan line left by the #9 insertion ("the contamination spreads to
rows that") mid-paragraph; `monitored_governor.h:147-149` has its `///<`
continuation lines indented one column past the opener. Both are comment-only
reflows, so fixing them costs nothing against the gate.

### Not regressed

Spot-checked the review's "verified true" list across all 12 fix-round files: the
filter two-condition margin and entry/exit/stash contract, the funnel Eq. (9)/(2a)/(2b)
text, the WMNO/pi_u/pi_l update rules, monitored-governor items (1)-(4) and (6)
including the `floor = min(bar_tol,kkt_tol)/(kBarrierTolFactor+1)` mapping, and the
`sqp/globalization.h` kRestore conjuncts are all textually untouched by `fe2a396`
except where a finding required it. No new history, narration, or plan/task labels
introduced.

---

## Section A — chunk 2, `include/hven/detail/interior` (ea7efe8)

**Verdict: CHANGES REQUESTED — 3 blocking, 10 should-fix, 6 nits.**

19 touched headers read new-vs-old (`git diff 3d37369 ea7efe8 -- <file>`), every
surviving/new docstring checked against the implementing code
(`src/drivers/interior_point_solver.cpp`,
`src/drivers/interior_point_solver_globalization.cpp`, the header bodies
themselves). The three docstrings the report flagged as written-under-uncertainty
were checked first: `jet.h`'s three `map()` briefs are accurate except for the
BLAS-pin lifetime clause (finding A-12), and `iterate_info.h`'s `prox_reg_*`
sentinel semantics are TRUE of the code (verified at
`interior_point_solver.cpp:2495-2504` — written only under
`InertiaModes::proximal_regularization`, `-1.0` default = mode off).

The arithmetic content of the sweep is good: every barrier/bound formula I
re-derived from the loops matches its docstring. What fails is mechanical
follow-through — one paragraph deleted only halfway, one XML tag left behind,
one banner set stripped in three places out of six — plus two condensations that
became false, both by the same mechanism chunk 1's finding 2 identified (drop the
subject of a sentence, and a statement about *someone else* becomes a statement
about the thing being documented).

### Blocking

**A-1. `barrier_math.h:90-115` — the bound-kernel header was rewritten but the
pre-image was not deleted; the file now carries the paragraph twice, the second
copy starting mid-sentence.**

```
 90 /// Primal variable-bound barrier kernels: for the bounds recorded in b
 ...
 97 /// @brief Bound-set log-barrier objective: -mu * [sum ln(x_i-l_i) +
 ...
101 /// The damping is Ipopt's (CalcBarrierTerm adds kappa_d * mu * slack.dot(dampind)
 ...
106 /// must stay out of.
107 // one-sided damping term kappa_d * mu * sum(distance) over the entries whose
108 // variable is bounded on that side only.
109 //
110 // The damping is Ipopt's (IpIpoptCalculatedQuantities::CalcBarrierTerm adds
 ...
115 // see kKappaD's note in bound_set.h for the seams it must stay out of.
116 inline double bound_barrier_objective(...)
```

Lines 107-115 are the original `//` block minus its first line (the rewrite
consumed `// -mu * [ sum ln(x_i - l_i) + ... ] over the bound set, plus the`),
so :107 opens on a dangling sentence fragment and :110-115 restate :101-106
verbatim in substance. This is precisely the "dangling pass-0 fragment" class the
report says it removed from this very file and from `kkt_vector.h`.

Correct text: delete :107-115 outright — every fact in them survives at :97-106.

**A-2. `iterate_info.h:71-74` — the norm-selection sentence lost its subject and
now attributes to `soc_should_trigger()` a decision that function does not make.**

New text:

```
///    Never compare across strategies; compare only against another
///    reading taken under the SAME acceptance path (the live consumer,
///    soc_should_trigger(), selects the norm via
///    drives_classic_path() so its two readings always match).
```

`soc_should_trigger` (`globalization/soc.h:49-56`) is
`(citer, current_infeasibility) -> bool`; its whole body is three comparisons and
it never touches an `AcceptanceStrategy`. The selection happens one frame up, in
`SocRecovery::on_step_rejected`
(`interior_point_solver_globalization.cpp:1392-1394`):
`acceptance.drives_classic_path() ? RHS.tail(ncons).squaredNorm() :
RHS.tail(ncons).lpNorm<1>()`. The pre-image said so explicitly — "its caller
(`SocRecovery::on_step_rejected`) selects the comparison norm via
`AcceptanceStrategy::drives_classic_path()`". A reader who takes the new sentence
at face value will look for a `drives_classic_path()` call inside
`soc_should_trigger`, find none, and conclude the guard is missing.

Correct text: `… (the live consumer's caller, SocRecovery::on_step_rejected,
selects the norm via drives_classic_path(), so the two readings
soc_should_trigger() compares always match).`

**A-3. `constraint_function.h:71-73` and `objective_function.h:62-65` — a new
locking claim was invented during the rewrite, and it is false.**

```
    /// @brief Calls the type-erased function's .constraints_jacobian method,
    /// scattering into the KKT matrix through the location/clash tables under
    /// per-row locks, and passing through the indexing data.
```

(`objective_function.h:62-65` carries the same clause for the Hessian.) The
pre-image said only that the method forwards its arguments; the scatter/lock
sentence is new. Two things are wrong with it:

- The locks are **per clash column, not per row**. Every scatter site takes
  `ClashLocks[VarClashes[active]]` where `active` is a *variable* (solver-space
  column) index — see `fixed_variable_row.h:166-179` and the same shape in
  `model/nlp_adapter.h:296-312, 427-444`. `indexing_data.h` names its own helper
  `kkt_canonical_lock_col`, which is the convention these sites share.
- The locking is **conditional**, not a property of the call:
  `const bool lock_column = !data.unique_constraints_ && (VarClashes[active] != -1);`
  On a problem with unique constraints nothing is locked at all.

A wrong thread-safety statement is the worst kind to ship: it is the sentence a
future reader will rely on instead of reading the scatter.

Correct text: either restore the pre-image's neutral wording ("passing through
the solver's arguments and the indexing data"), or state it accurately —
"scatters into the KKT matrix through the location/clash tables, taking the
clash-column lock only for columns marked in `KKTClashes` on a
non-`unique_constraints_` problem".

### Should-fix

**A-4. `barrier_math.h:35-37` — "repairs non-positive slacks in place" is not
what the loop does.** Body: `double si = S[i]; if (si < neg_slack_reset) si =
neg_slack_reset;` — the clamp lands in a local. `S[i]` is written only on the
`fxi < 0.0` branch; on the `else` branch the clamped `si` is merely what gets
added to `FXI[i]`, and `S[i]` keeps its sub-threshold value. The claim is
inherited from the pre-image, but the sweep re-asserted it in condensed form, so
it is in scope. Correct text: "… clamps a slack below `neg_slack_reset` to that
floor **for the purpose of the sum**; a negative residual instead zeroes the
residual and resets `S[i]` to `max(|fx|, neg_slack_reset)`." Worth a line in the
report's "looked like a defect" section too — if in-place repair was the intent,
the local clamp is a bug; the report records "none found in this chunk".

**A-5. `barrier_math.h:90-106` — group prose and an `@brief` share one comment
run.** :90-95 documents the whole four-kernel family (including its
strictly-inside-bounds invariant); :97 opens `@brief` for
`bound_barrier_objective` alone. Doxygen sees one block on one entity, so the
family invariant is filed under the objective kernel. Same defect as chunk-1
findings 6/7. Demote :90-95 to `//` (it documents a file section, not a
declaration), leaving :97-106 as the function's block.

**A-6. `barrier_math.h:137` — an Ipopt attribution was anonymized rather than
kept explicit or dropped.** New: "The reference implementation makes the same
split, damping and all." Pre-image: "Ipopt keeps the same split, damping and all:
its Newton RHS reads `curr_grad_lag_WITH_DAMPING_x` while its optimality error
reads the undamped `curr_grad_lag_x`." The comment-rules accuracy clause requires
the attribution stay explicit or the sentence go; the sibling paragraph at :101
does name Ipopt, so the file is now inconsistent with itself. Restore "Ipopt" (the
two symbol names may stay dropped).

**A-7. `kkt_factorization.h:6-20` — file-level narration was PROMOTED from `//`
to a floating `/// @brief` block.** It sits between `#pragma once` and the
includes, so Doxygen binds it to the first declaration it finds — `class
KktFactorization` (:29), which has no docstring of its own. The block describes
the *file*'s three things ("the assembly buffer … the SymmetricFactor consuming
it, and the small evidence cache"); the class it lands on is one of them. This is
the one place in the chunk where the sweep moved in the direction chunk 1's
finding 7 rules out. Demote to `//`, or split: a real `@brief` on
`KktFactorization` plus `//` prose for the rest.

**A-8. `indexing_data.h:103` — orphaned `/// </summary>`.** The opening
`<summary>` on `v_out_index_`'s block was converted to `@brief` (:84) but the
closing tag was left, so a literal `</summary>` now sits inside the doc comment
and will render. Delete the line.

**A-9. `thread_pool.h` — the banner/ruler strip is half-applied, and one surviving
banner duplicates the docstring right below it.** Three banners were converted
(:28-30, :127-140, :226-236 regions); three were not: `:320-326`
(`ThreadPool — work-stealing thread pool`), `:584-590` (`Global thread budget`),
`:630-641` (`Safe parallel dispatch helpers`). Worse, :320-326's body ("Each
worker has its own queue. Tasks are distributed round-robin on enqueue. Workers
try their own queue first, then steal from others.") is restated by the class's
own `@brief` at :328-334 — the same duplicate-paragraph removal the report claims
for `flat_map.h`. Strip the three rulers; delete the :320-326 body as redundant;
keep the :584-590 and :630-641 content (both are rule-3 material) as plain `//`.

**A-10. `iterate_info.h:39-41, 44-50, 59-77` — grouped `///` blocks over multiple
members attach to the first member only.** :44-50 documents `prox_reg_primal_`
AND `prox_reg_dual_` (lands on `prox_reg_primal_`); :59-77 documents `accepted_`,
`first_rejection_iter_` AND `theta_at_first_rejection_` (lands on `accepted_`, so
the entire norm-convention callout is filed under a `bool`). Correct as prose,
mis-attached as Doxygen — chunk-1 finding 16 verbatim. Use `///<` per member, or
`//` prose plus a short `///<` each.

**A-11. `parsed_io_flags.h` — the deleted header paragraph was not all history,
and the enum docstring does not carry what was load-bearing in it.** Deleted:
"How one parsed input/output slot maps into storage. SolverIndexingData tags every
column of its variable and constraint index maps with one of these, and the gather
routines read the tag to choose between a segment copy and an element-by-element
gather." The surviving docstring (:13) is "Classifies how a parsed input/output
slot maps into a VF's storage" — it names neither the tagger nor the consumer, nor
the segment-copy-vs-gather dispatch the flag exists to drive. Only the last
sentence of the deleted paragraph (the re-export relocation) was history. Restore
the consumer sentence as a two-line `//` note or fold it into the enum's `@brief`.
The report's justification ("enum docstring already carries the semantics") does
not hold as written.

**A-12. `jet.h:137-139` — the BLAS-pin lifetime claim is true on one of the two
build paths.** New: "Each job pins its thread's BLAS to single-threaded mode for
its duration". Under `#else` (MKL) that is exact — `detail::MklLocalPinGuard
mkl_pin;` is RAII. Under `USE_ACCELERATE_SPARSE` the job body calls
`accelerate_set_num_threads(1)` (:166) with no restore; the code comment three
lines above says so outright ("this would leak into reused solves on this thread
— InteriorPointSolver re-applies its `qp_threads_` setting at every solve
entry"). The docstring's second sentence preserves the healing fact, so a careful
reader can reconstruct it, but "for its duration" is the wrong contract on the
Accelerate path. Correct text: "Each job runs with its thread's BLAS pinned to
single-threaded mode (scope-guarded under MKL; set-and-leak under Accelerate,
which the solver heals by re-applying its threading at every solve entry)."

**A-13. `.sweep/chunk-2-report.md` omits the before/after comment-line counts the
rules require.** `comment-rules.md:36` — "files touched with before/after
comment-line counts". The chunk-2 table has a prose `change summary` column
instead. Chunk 1 shipped the counts (and had to correct them); chunk 2 shipped
none, so there is no quantitative record of this chunk at all.

### Nits

**A-a.** Report rationale candidate "eval_error_log.h:9-13 (pre-sweep)" — the
paragraph is at `3d37369:eval_error_log.h:4-7`; :9-13 are `#pragma once`, an
include and the namespace opener. Summary is faithful; only the range is off. The
other three spot-checked candidates are exact: `barrier_math.h:9-11` ✓,
`kkt_vector.h:9-10` ✓, `fixed_variable_row.h:7-15` ✓ (and its one-line summary,
"why the row is solver-side rather than a VectorFunction expression
(dependency-weight argument)", is a fair reading of the deleted text).

**A-b.** `jet.h` — `map()` gained an `@brief` but no `@return` (it returns the
constructed `optprobs`) and no `@throws` (it rethrows the first job's exception
after draining the remaining futures, :216-245 — a deliberate, non-obvious
lifetime guard worth a line). The identity overload's brief (:270-271, "runs … in
parallel") holds only when `use_thread_pool()` is true.

**A-c.** `iterate_info.h:50, 62` — "console output stays byte-identical" survives
with its antecedent removed (the pre-image said "*so adding them* leaves console
output byte-identical"). As it stands it is a claim with no comparand, i.e.
vestigial change-history. Either say "not printed (the iteration table formats an
explicit field list)" or drop the clause.

**A-d.** `sizing_specs.h:47` — `struct Concept { ` gained a trailing space when
its inline comment was stripped. One line, chunk-1 finding 11's class; folds into
the same follow-up formatting pass.

**A-e.** `type_storage.h` — the header note now says "built on the same
`clone_into` convention" without stating it; the required signature survives at
the class docstring (:25) but the implementer half of the pre-image ("Each
concrete `Model` implements this as `s.emplace<Model>(data_)`") is gone and is not
restated anywhere. One line at :25 would close it.

**A-f.** `constraint_function.h` / `objective_function.h` — the forwarder briefs
are asymmetric: two of them (`constraints_jacobian`,
`objective_gradient_hessian`) got the scatter/lock embellishment while
`constraints_jacobian_adjointgradient` and
`constraints_jacobian_adjointgradient_adjointhessian`, which take the identical
`KKTmat`/`KKTLocations`/`KKTClashes`/`KKTLocks` parameter pack, kept the neutral
wording. Whatever A-3 resolves to should be applied to all four or none.

### Verified true (no action)

- `barrier_math.h`: `max_step_to_boundary`'s `alpha = -bfrac*SLI/dSLI` derivation
  and its `(0,1]` range; `barrier_objective`; `barrier_gradient`
  (`AGS = LI - mu*S.cwiseInverse()`); the mu-form gradient signs including the
  `+kappa_d*mu` / `-kappa_d*mu` damping derivative per side; the z-form's
  `-zL / +zU`; `augment_bound_complementarity`'s union min-of-mins,
  max-of-maxes, count-weighted average, and the "base aggregates are NOT
  re-reduced" ULP argument (the function reconstructs `avgcomp*base_count` and
  never re-folds the base pairs).
- `iterate_info.h`: `h_pert_cum_` vs `h_pert_` (cumulative vs last); the
  `prox_reg_*` sentinel and suppression semantics; `eval_exceptions_`'s
  late-callback statement.
- `kkt_vector.h`: the four-block layout, the `std::as_const(data_)`
  const-correctness rationale, the lifetime rule, and the `BoundDualState`
  carve-out ("a reader looking for all the duals needs both").
- `bound_set.h`, `eval_error_log.h`, `fixed_variable_row.h`,
  `objective_function.h` (apart from A-3), `flat_map.h`, `tuple_iterator.h`,
  `super_scalar_traits.h`, `threading_flags.h`, `sizing_specs.h`: no false
  statements found; the `<summary>` → `@brief` conversions preserve content.
- Deletions the report justified by a surviving docstring, both checked and
  sound: `flat_map.h`'s header paragraph (the class `@brief` at :15-22 carries
  the trivially-relocatable / TypeStorage-safe invariant and the insertion-order
  and O(N)-lookup facts), `type_storage.h`'s convention block (the signature
  requirement survives at :25).
- The "already compliant, untouched" claim for the other 12 files holds on
  inspection (31 files in scope, 19 + 12). `timer.h`'s block comment is a
  third-party MIT notice — correctly untouched, as the report notes for
  `type_name.h`.
- No plan/task/milestone labels (`Task N`, `T6`, `M3`, `E2`, `G7`, `phase-C`,
  `fb3`, `dossier`, `spec §`) survive in any of the 19 files. Trailing-whitespace
  delta across the chunk is +1 line (A-d).
