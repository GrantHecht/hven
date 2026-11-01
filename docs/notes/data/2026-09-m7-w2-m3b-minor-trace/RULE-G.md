# RULE-G — the CAUSE classifier for the M3b re-trace

**Pre-registered. Committed before any leg of the re-trace ran, identity legs
included.** Nothing in this file may be edited after a leg. If a predicate turns
out to be badly posed, that is reported as a defect *of this rule*, in the
report, with the outcome it produced left standing — exactly as M3b's `RULE.md`
requires of itself and as F4 required of `RULE.md`'s own `P_imm` derivation.

`RULE.md` beside this file **stands unchanged**. It classified WHAT the walk
does and returned `CYCLING_PROPER` on both cells. This file classifies WHY, over
the same two first QPs, using the fields fix round 1 added to
`include/hven/detail/qp/qp_walk_trace.h`. The two rules are independent: nothing
here can change `RULE.md`'s outcome, and `CYCLING_PROPER` has precedence over
everything below as the statement of what the walk is doing.

**No outcome of this rule licenses a behaviour change, a default change, or a
code change of any kind.** The remedy mapping in §8 is a recommendation, ranked,
and explicitly not a decision.

---

## 0. What is recorded, what is ABSENT, and why

The review's §6 asked for eight groups. Fix round 1 records six of them and part
of two more. The rule may read ONLY what is in the left column.

| review §6 item | recorded? | the fields |
|---|---|---|
| 1. main inertia verdict, triple, expected inertia, fresh/updated/rebuild | **yes** | `inertia_verdict`, `inertia_state`, `inertia_pos/neg/zero`, `inertia_zero_derived`, `inertia_perturbed`, `inertia_expected_pos/neg`, `k0_rebuilt`, `border_needs_refactorization`, `border_latched` |
| 2. curvature test fired, `pᵀHp`, `pᵀp`, the quotient | **yes** | `curvature_fired`, `p_hp`, `p_sq`, `curvature`, `curvature_tol` |
| 3. granular ride outcome | **yes**, all six branches | `ride_outcome` ∈ {`kNotAttempted`, `kNullspaceVeto`, `kSlopeFlatVeto`, `kSignDisagreementVeto`, `kUnbounded`, `kStepped`} |
| 4. post-drop: released identity, `p[released]`, ride sign, dropped directional, `gᵀp` | **yes** | `released_block/index/side`, `p_released`, `ride_sign_chosen`, `released_directional`, `ride_slope` (= `gᵀp` with `g = Hx + g`), `ride_slope_tol`, `ride_leaves`, `ride_leaves_tol` |
| 5. unregularized stationarity and active-row residuals, effective regularization | **partly** | `stat_free_inf`, `stat_scale`, `active_res_inf`, `primal_delta`, `dual_mu`. **ABSENT: the before/after-refinement split** — one residual is taken, at the pass's starting iterate. |
| 6. per-pass refinement count, path, residual before/after refinement | **partly** | `eqp_path`, `border_attempted`, `border_dim`, `eqp_call_refine_steps`, `pass_factorizations`, `pass_eqp_refine_steps`, `pass_border_refine_steps`, `pass_verdict_refine_steps`, `pass_schur_updates`. **ABSENT: the refinement residual before and after.** |
| 7. rank / independence evidence for the released constraint | **ABSENT** | — |
| 8. top drop candidates with violations; predicted post-drop inertia per candidate | **partly** | `candidates[]` (up to 8, largest violation first: block, index, side, violation), `candidates_recorded`, `drop_candidates`, `drop_tied`, `best_multiplier`, `decision_multiplier`. **ABSENT: the predicted post-drop inertia of the candidates not picked.** |

### The two facts deliberately NOT measured

1. **Rank / independence evidence for the released constraint** (§6.7).
2. **The predicted post-drop inertia of the drop candidates not chosen** (§6.8).

Each would need **an extra factorization per candidate per pass**, and the
observer is record-only: it performs no factorization and no linear solve, and
will not be extended to. They are marked ABSENT here so that no reading of the
re-trace can quietly supply them.

**Binding consequence.** `NUMERICAL_DIRECTION`'s rank-flavoured sub-reason
(§4.4, `RANK_OR_ZERO`) rests on the **observed inertia triple alone** — a
reported zero eigenvalue or a negative-count deficit — and never on rank
evidence for the released bound, which does not exist in this trace. Any
statement about the released constraint's independence, and any statement that
some *other* candidate would have given a usable post-drop subspace, is outside
this rule and must be reported as unmeasured. The review's still-plausible cause
(1), Dantzig selection without regard to post-drop usability, is therefore
**described** by this rule (§7.8 reports the candidate set and the chosen one's
rank by violation) and **not adjudicated** by it.

---

## 1. Scope, and the legs this rule is applied to

The same two TRAIN cells and the same single QP as M3b: **STEENBRB@trace-G** and
**YORKNET@trace-G**, first QP of the first major only. HOLDOUT is not run.

The rule is applied **per cell**. There is no cross-cell aggregation and no
outcome that depends on both cells agreeing; if they disagree, both outcomes
stand and the report says so.

---

## 2. ABORT conditions — checked before any predicate

If any fires, the leg's outcome is **ABORTED**, no cause is assigned, and the
report says which fired and what was measured.

**2.1 The identity gate.** Unchanged from `RULE.md` §2.1 and enforced by the
same hardened checker: the traced solve's first QP-ledger record must equal M7
W2 M3a's committed `../2026-09-m7-w2-m3a-request-probe/rows/f/<cell>@F-trinf.txt`
record 0 on **all 20 `QpCounters` fields plus the status**, compared against the
bytes of that file rather than against any table. The checker must refuse a
missing or unparseable row file, a missing or unparseable traced record, a
comparison covering fewer than 20 fields, and a field list that does not match
the declared one. **The identity legs gate the trace legs: a failed identity
aborts before any trace leg runs.** This is what makes the re-trace a trace of
the same solve rather than a new measurement.

**2.2 Trace integrity.** `QpWalkObserver::truncated` false; `allocation_failed`
false; exactly one record per pass, i.e. `len(records) == minor_iters`; no
record with `action == kUnset`.

**2.3 Neutrality.** The attached-vs-unattached bit-identity comparison on the six
cases must be green in both configurations, and F1's concurrent fixtures green,
before any leg is read. A trace taken through an observer not proven neutral is
not evidence.

**2.4 No tail.** If §3 finds no tail meeting its size conditions, the outcome is
`ABORTED_NO_TAIL`. A cell with no tail has nothing for this rule to classify.

---

## 3. THE TAIL — how it is identified

M3b established, **outside its rule**, that each cell ends in an exact zero-motion
working-set period-2 repetition. That observation is now **pre-registered** as
this rule's object, with the definition made constructive.

Let `N = len(records)`. Let **F₀** be the field set the M3b observer recorded —
every member of `QpWalkMinorRecord` declared **above** the
`FIX-ROUND 1 … THE DISCRIMINATING FIELDS` divider in `qp_walk_trace.h`, **minus**
`solve` and `minor`, which increment by construction.

Define `R(m)` for `m ≥ 2`: record `m` agrees with record `m − 2` on **every**
field in F₀, floats compared by **exact bit pattern** with `NaN` equal to `NaN`
(so an ABSENT value matches an ABSENT value and never matches a zero).

> **Why F₀ and not the full field set.** The tail must be the SAME object M3b
> measured, so that this rule classifies the phenomenon already reported rather
> than a new one. Whether the fields fix round 1 ADDED also repeat with period 2
> is then a *finding* of the re-trace (§7.0), reported either way — not part of
> the definition, which would make it unfalsifiable.

**The tail** `T = [t₀, N)` is the suffix with `t₀` **minimal** such that `R(m)`
holds for every `m ∈ [t₀ + 2, N)`.

**Size conditions**, both required, else `ABORTED_NO_TAIL`:

* `|T| ≥ 100`. *Derivation:* the classification reads two states, so 100 passes
  is at least 50 repetitions of each; below that a homogeneity claim is a claim
  about a handful of passes. Not a tuned bar — it is a floor on evidence count.
* `|T| / N ≥ 0.50`. *Derivation:* the bar exists to refuse a leg whose tail is a
  minor feature of the trace rather than what the leg does. It is set at one
  half, **far below** the 98.74 % and 93.09 % M3b measured, precisely so that it
  cannot be a bar tuned to pass; it would refuse a tail occupying a minority of
  the trace, which neither cell's does.

**The two states.** `T` must consist of exactly two actions alternating:

* **state D** — `action == kDropDantzig`: the KKT-point pass at which the drop
  rule releases a constraint (`p` negligible, `qp_engine.cpp:743`).
* **state A** — `action == kAdd` with `post_drop == true`: the pass that follows
  it, at which §4c's gate is reached and the constraint is re-admitted.

If `T` contains any other action, or the two do not strictly alternate, the tail
is **not the two-state shape**; §4 then runs in its FREQUENCY form (§5).

---

## 4. THE OUTCOMES — mutually exclusive, in precedence order

Every predicate below is evaluated **on the tail's two states**, not on
frequencies, whenever §3's two-state shape holds and §5's homogeneity holds.
That is the honest form here: M3b showed the tail is a bit-exact period-2
repetition, so a frequency over the tail is 1 or 0 and reporting it as a
proportion would dress a single observation as a statistic.

### 4.0 The inertia evidence, evaluated once, on state A

```
observed(A)      :=  inertia_state == kObserved
as_expected(A)   :=  observed(A) and inertia_pos == inertia_expected_pos
                                 and inertia_neg == inertia_expected_neg
neg_excess(A)    :=  observed(A) and inertia_neg  >  inertia_expected_neg
adverse_other(A) :=  observed(A) and not as_expected(A) and not neg_excess(A)
unobserved(A)    :=  not observed(A)
```

*Derivation, and why there is no bar to place.* For the EQP KKT matrix
`K = [[H, Aᵀ], [A, 0]]` with `A` of full row rank `m` and `H` positive definite
on `null(A)`, Sylvester's law of inertia gives the inertia **exactly**
`(n, m, 0)`, which is what `inertia_expected_pos/neg` carry. The quantity is an
**integer count**, the pure-mechanism value is that exact integer, and any
deviation is a whole eigenvalue. There is no continuum between "as expected" and
"one more negative direction", so there is nothing for a bar to sit strictly
inside; a bar here would be an invention. The null that a *fluctuating backend*
produces a spurious excess predicts an excess on some tail passes and not others
— which §5's homogeneity condition tests directly and which, if it held, sends
the leg to the frequency form rather than to an outcome.

`inertia_zero` is read **only** when `inertia_zero_derived` is false. A derived
zero (`dim − pos − neg`, which is what MKL forces) is arithmetic, not an
observation, and a derived `0` is not evidence that there are no zero
eigenvalues. This is recorded and honoured, not worked around.

### 4.1 `INDEFINITE_IGNORED` — the machinery never looked

Holds iff, on state A:

* `neg_excess(A)`, **and**
* `p_hp` is **ABSENT** (`NaN`) while `p_sq` is present — the signature of
  §4c's block at `qp_engine.cpp:911` not having executed at all, since `p_sq` is
  recorded on every pass and `p_hp` only inside that block — **and**
* `ride_outcome == kNotAttempted`, **and**
* no pass in `T` has `action ∈ {kSuspectEscalation, kStartRepair,
  kPostProbeRestart}`, i.e. the main verdict was never acted on anywhere in the
  tail either.

The reduced system carries a negative direction the backend reported, and
neither §4b's verdict nor §4c's gate was consulted on the path the loop took.

### 4.2 `CURVATURE_GATE_SILENT` — the direction is not the negative-curvature one

Holds iff, on state A: `neg_excess(A)` **and** `curvature` is present **and**
`curvature_fired == false`.

*Derivation, and why no bar on `p_hp`.* `curvature_tol = kCurvatureTolFactor ·
hessian_scale(qp)` with `kCurvatureTolFactor = 1e-12`
(`qp_engine.h:582`) and `hessian_scale(qp) > 0`, so **`curvature_tol > 0`**; the
gate fires on `curvature <= curvature_tol` (`qp_engine.cpp:929`), and `p_sq > 0`
is guaranteed at that line because the negligible-`p` case returned earlier.
Therefore `curvature_fired == false` **implies** `p_hp > curvature_tol · p_sq >
0` as an arithmetic identity, not as a measurement. The review's condition
"`pᵀHp ≥ 0` although inertia says indefinite" is thus *entailed* by the gate
having stayed silent, and a separate threshold on `p_hp` would be a second test
of the same fact. `p_hp`, `p_sq`, `curvature` and `curvature_tol` are all
reported (§7.2) so a reader can check the entailment rather than take it.

### 4.3 `RIDE_VETOED` — the gate fired and the ride refused

Holds iff, on state A: `neg_excess(A)` **and** `curvature_fired == true`
**and** `ride_outcome ∈ {kNullspaceVeto, kSlopeFlatVeto, kSignDisagreementVeto}`.

**The dominant veto is named** — on a homogeneous tail it is the single value
`ride_outcome` takes; in the frequency form (§5) it is the modal value, which
must carry share ≥ 0.90 (bar derived at §5).

A `ride_outcome` of `kStepped` or `kUnbounded` does **not** give this outcome:
a ride that moved the iterate is inconsistent with a zero-motion tail and routes
to `MIXED`, where it is reported as the contradiction it would be.

> §4.1–§4.3 **partition** the `neg_excess(A)` branch, and the partition is a
> partition of the PASS, in the order §4c's own code reaches its stations:
> the block does not execute (4.1) / it executes and the gate stays silent
> (4.2) / the gate fires and the ride refuses (4.3) / the ride acts (`MIXED`).
> The precedence order the brief gave is that causal order, which is why it is
> also the order a reader should read them in.

### 4.4 `NUMERICAL_DIRECTION` — the direction is not trustworthy

Reached when `neg_excess(A)` is FALSE. Holds iff **at least one** of:

* **`RESIDUAL`** — on state A, `stat_free_inf / max(stat_scale, 1) > 1e-6`, or
  `active_res_inf / max(x_inf, 1) > 1e-6`.
* **`REFINEMENT_SATURATED`** — on state A, `eqp_path == kBorderSchur` **and**
  `eqp_call_refine_steps >= 10`.
* **`RANK_OR_ZERO`** — `adverse_other(A)`: the backend observed an inertia that
  is neither the expected one nor a negative excess, i.e. a negative-count
  deficit or a **natively reported** nonzero `inertia_zero`.
* **`UNOBSERVED_INERTIA`** — `unobserved(A)`: no usable verdict, so
  indefiniteness can be neither asserted nor excluded and the direction's
  provenance is all that is left.

Every sub-reason that holds is named in the verdict; the outcome is one.

#### Derivation of the residual bar, `1e-6`

* **Pure-mechanism value.** A backward-stable double-precision solve leaves a
  relative residual of order `c · ε` with `ε = 2.220446e-16` and `c` a modest
  multiple of the system dimension. With `n = 468` (STEENBRB) and a growth
  factor of order `n`, `n² ε ≈ 468² · 2.22e-16 ≈ 4.9e-11`. Take the
  pure-mechanism value as **1e-10**, an order above that.
* **Null value.** The identity that makes the convex argument work for an exact,
  unregularized, full-rank KKT system is `z_i p_i = −pᵀHp`. For the computed `p`
  to have the **wrong sign** in the released component through residual error
  alone, the residual must be comparable to the terms it is subtracted from —
  a relative residual of order **1**.
* **Bar strictly inside.** `1e-10 ≪ 1e-6 ≪ 1`: four orders above the
  pure-mechanism value, six below the null. A relative residual of `1e-6` in
  double precision is ten orders above what the mechanism produces and is
  "large" by any reading; it is well short of the order-1 residual that would be
  needed to flip a sign outright, which is why this outcome says *not
  trustworthy*, never *this is the cause*.
* **Corroboration, not derivation.** M3b's already-committed §5.4 read-out gives
  STEENBRB's drop multipliers a median magnitude of `1.17e-3`. A relative
  residual at the bar perturbs such a multiplier by a relative `1e-3`. The bar
  was fixed from `ε` and the dimension; this is a sanity check on it, and it is
  recorded as such so that it cannot be mistaken for the derivation.

#### Derivation of `REFINEMENT_SATURATED`, and why no bar

`detail::kMaxBorderRefineSteps = 10` (`eqp_solve.h:110`) is the **total**
refinement budget a bordered solve may spend, and
`refine_bordered_solve_iterated` exits early once the relative residual stops
improving past `kBorderRefineRelFloor`. So: the pure-mechanism value is *any
count below the cap* — a solve that converged; the null (refinement is not the
binding constraint) is likewise any count below the cap; and the cap itself,
`10`, is the single value that means **the loop ran out of budget rather than
converging**. The predicate is equality with a compiled-in constant. There is no
continuum and therefore no bar; a bar here would only blur the one sharp fact
the counter carries.

### 4.5 `INERTIA_AS_EXPECTED_UNEXPLAINED`

Holds iff `as_expected(A)` and none of §4.4's four sub-reasons holds. The KKT
system reports exactly the inertia it should, the residuals are at the
mechanism's own scale, refinement converged — and the tail cycles anyway. This
is a real outcome, not a failure of the rule: it would say the cause is not in
anything this trace can see, and it would be the finding.

### 4.6 `MIXED` — the residue

Anything §4.1–§4.5 do not claim. In particular: a tail that is not the two-state
shape and fails §5's frequency form; a `neg_excess(A)` tail whose state A rode or
went unbounded; a leg whose state-A and state-D evidence contradict each other in
a way §7 reports and §4 cannot resolve. `MIXED` names what it saw; it never
stands in for "the rule did not run".

### Precedence

```
INDEFINITE_IGNORED
CURVATURE_GATE_SILENT
RIDE_VETOED
NUMERICAL_DIRECTION
INERTIA_AS_EXPECTED_UNEXPLAINED
MIXED                              (the residue)
```

Mutual exclusion is by construction — §4.1–§4.3 partition `neg_excess(A)`, §4.4
and §4.5 partition its complement, §4.6 takes the rest — so the precedence order
resolves nothing and is stated only because the brief fixes it and because it is
the causal order along a pass. **The classifier must assert that at most one
outcome holds** and report a violation as a defect of this rule rather than
silently taking the first.

---

## 5. Homogeneity, and the FREQUENCY fallback

**Precondition, tested and reported, never assumed.** Over the tail `T`, all
state-A passes must agree **exactly** on every field §4 reads
(`inertia_state`, `inertia_pos/neg`, `inertia_expected_pos/neg`,
`inertia_zero`, `inertia_zero_derived`, `curvature_fired`, presence of `p_hp`,
`ride_outcome`, `eqp_path`, `eqp_call_refine_steps`, `stat_free_inf`,
`stat_scale`, `active_res_inf`, `x_inf`), and all state-D passes must agree on
the same. Floats by exact bit pattern, `NaN == NaN`.

If homogeneity holds, §4's predicates are evaluated on the single state-A value
and the single state-D value, and the verdict reports **that value with the
repetition count**, not a proportion.

If homogeneity fails — or §3's two-state shape fails — §4 runs in its
**FREQUENCY form**: each predicate is evaluated per state-A pass and must hold on
a share **≥ 0.90** of them.

#### Derivation of the 0.90 share bar

* **Pure-mechanism value: 1.** If the tail's cause is one station of one pass
  repeated, every state-A pass carries the same value and the share is exactly 1.
* **Null value: ≤ 0.50.** The null is *"the tail does not have a single
  cause"* — its weakest non-trivial operational form is that **no value holds a
  majority**, i.e. share ≤ 1/2. This null is deliberately stated as "no
  majority" and **not** as an equiprobable draw over the branches: an
  equiprobable null would presume the branches are exchangeable, which is
  exactly the unjustified independence assumption F4 found in `RULE.md`'s
  `P_imm` derivation, and the same mistake is not made here. Nothing in this
  file assumes Dantzig selection is random, independent, or uniform over
  candidates — §7.8 *describes* the candidate set and adjudicates nothing from
  it.
* **Bar strictly inside: 0.90.** `0.50 < 0.90 < 1`. It is placed near the
  mechanism rather than near the null because the mechanism's prediction here is
  an exact 1, so the only headroom the bar needs is for a handful of
  non-conforming passes at the tail's own boundary.

---

## 6. What the classifier must refuse

Structural non-vacuity, on M3a fix1's hardened template. The classifier exits
nonzero, with the reason named, on:

* a missing, empty or unparseable trace CSV;
* a trace whose header does not carry **every** field §4, §5 and §7 read (a rule
  that silently skips a missing column is a rule that passes on nothing);
* `truncated`, `allocation_failed`, `len(records) != minor_iters`, or any
  `action == kUnset`;
* a failed identity gate (§2.1), including a checker that compared fewer than 20
  fields;
* a tail failing §3's size conditions (`ABORTED_NO_TAIL`);
* **more than one outcome holding** (§4's precedence block);
* a synthetic fold showing the classifier accepting an input it must refuse.

The classifier is fold-proven **before** it is run on a leg: synthetic records
for each outcome, for each refusal above, and for the boundary of each bar
(`1e-6` above and below; `eqp_call_refine_steps` at 9 and 10; share at 0.89 and
0.90). The folds are committed with the verdict.

---

## 7. Descriptive read-outs — PRE-REGISTERED, reported for every cell whatever wins

These are **not** verdicts. None can change an outcome. All are reported for
both cells and for **both tail states**, whichever outcome §4 returns.

**7.0** Whether each field fix round 1 ADDED also repeats with period 2 over
`T` — reported per field, either way. §3 defines the tail on F₀ alone precisely
so that this is a finding rather than an assumption.

**7.1 Inertia.** `(inertia_pos, inertia_neg, inertia_zero)` against
`(inertia_expected_pos, inertia_expected_neg)` on state A **and** state D, with
`inertia_state`, `inertia_verdict`, `inertia_zero_derived` and
`inertia_perturbed` beside them. A derived zero is labelled as derived.

**7.2 Curvature.** `p_hp`, `p_sq`, `curvature`, `curvature_tol`,
`curvature_fired` on state A; and the entailment check of §4.2
(`curvature_fired == false ⟹ p_hp > curvature_tol · p_sq`) reported as it
lands.

**7.3 The released constraint.** `released_block/index/side`; `p_released`
with its **sign and magnitude** stated separately; `released_directional`.

**7.4 The slope.** `ride_slope` (= `gᵀp`) and `ride_slope_tol`;
`ride_leaves` and `ride_leaves_tol`; `ride_sign_chosen`. Reported on every
post-drop pass, whether or not the gate fired — which is the fact an ordinary
zero-step add cannot establish and the reason F3(b) asked for it.

**7.5 The ride outcome in detail.** `ride_outcome` by name, with §7.4's four
numbers against their two tolerances, so a reader can see which comparison
decided it.

**7.6 The linear algebra.** `eqp_path`, `border_attempted`, `border_dim`,
`k0_rebuilt`, `border_latched`, `border_needs_refactorization`,
`eqp_call_refine_steps`, and the five per-pass counter differences
(`pass_factorizations`, `pass_eqp_refine_steps`, `pass_border_refine_steps`,
`pass_verdict_refine_steps`, `pass_schur_updates`).

**7.7 Residuals and regularization.** `stat_free_inf` with `stat_scale` and the
ratio §4.4 reads; `active_res_inf` with `x_inf` and its ratio; `primal_delta`
and `dual_mu` (the EFFECTIVE values, which §4b's ladder can move).

**7.8 The candidate set on state D.** `drop_candidates`, `drop_tied`,
`candidates_recorded`, and the recorded candidates largest-violation-first with
their identities, sides and violations; `decision_multiplier`, `best_multiplier`,
and **the rank of the chosen candidate by violation** (which the Dantzig rule
makes 1 by construction — reported so that it is checked, not assumed).
Reported **with the ABSENT marks of §0 restated on the spot**: no post-drop
inertia is predicted for any candidate, and no rank evidence exists for the
released bound, so nothing here adjudicates the review's cause (1).

**7.9 The tail itself.** `t₀`, `|T|`, `|T|/N`, the period, the action histogram
over `T`, the repeated identity, and the homogeneity result of §5 field by field.

---

## 8. The remedy mapping — a RECOMMENDATION, ranked, and not a decision

Conditional on an outcome, and **binding on nothing**. No verdict of this rule
authorises a behaviour change, a default change, or a code change; any remedy is
a separately registered and separately briefed task. Open-literature papers only.

1. **`INDEFINITE_IGNORED` or `RIDE_VETOED`, cause-level, preferred** —
   inertia-controlling working-set changes: keep a second-order-consistent
   working set and resolve a nonpositive direction explicitly before an ordinary
   Newton-type step. Gill, Murray, Saunders & Wright's inertia-controlling QP
   method; Gill & Wong's general-QP treatment.
2. **`RIDE_VETOED` or `CURVATURE_GATE_SILENT`, targeted** — repair the
   negative-curvature recovery: do **not** first "correct" the sign test, which
   already permits flipping `p`; diagnose whether the direction lies in the
   retained nullspace, then construct or project a feasible negative-curvature
   direction whose sign satisfies both descent and release feasibility.
   Fletcher; Gould.
3. **`NUMERICAL_DIRECTION`** — the border/Schur solve's accuracy and the
   regularization, at the sub-reason the verdict names; `REFINEMENT_SATURATED`
   and `RESIDUAL` point at different work and must not be merged.
4. **Any outcome** — SQP-level convexification or a modified Hessian is a
   broader cause-*avoidance* remedy that changes the exact-Hessian model and is
   a separately registered policy question, considered only after the QP-level
   failure is identified.
5. **Any outcome, stopgap only** — taboo, lexicographic or EXPAND-style
   anti-cycling. These can stop an immediate re-addition but repair neither a
   wrong-sign direction, nor a rank defect, nor a failed negative-curvature
   treatment, and an exact zero-motion two-cycle is the case an EXPAND-style
   tolerance change answers least well.

Cost is part of the recommendation and not part of the verdict: hven pays dense
Schur/border maintenance per add and per drop, so an inertia method that adds
temporary constraints needs incremental updates or batched rebuilds, while one
valid ride replacing thousands of drop/re-add pairs would be an overwhelming net
reduction.
