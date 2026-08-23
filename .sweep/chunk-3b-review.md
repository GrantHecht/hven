# Chunk 3b comment-sweep review — `qp_engine.h`, `ssn_engine.h`, `eqp_solve.h`

Repo `/home/ghecht/Projects/hven-sweep`, branch `sweep/base`, commit **aafc053** over **a9cba6a**.
Read-only review. No builds, no edits, no subagents. All line numbers are at `aafc053`.

**VERDICT: CHANGES REQUIRED** — 2 blocking, 5 should-fix, 7 nits.

The condensation is, on the whole, technically sound. Every one of the five derivations the
brief named was re-derived against the code and holds: reuse condition (e), the HotState
DETACH guard, the per-solve radius-variation note, the SSN sigma/hint/Farkas rule summaries,
and the triplet-rebuild contract. B1–B4 and N1–N4 from the prior review are all correctly
fixed, the R-lever vocabulary strip is factually clean, and every cross-reference resolves
except one. What blocks is two places where a *deleted* sentence carried a branch the
surviving text now misstates: the `@throws` domain on `resolve_effective_options`, and the
repair/ride line in the COUNTER SEMANTICS paragraph.

---

## Blocking

### 1 — `qp_engine.h:1666-1675`. `@throws` understates the throw domain; the deleted text carried the missing half.

Current text:

```
/// @throws std::invalid_argument if the SELECTED tr_radius -- the
///     override if finite and non-negative, else the stored
///     opts.tr_radius -- is NaN or negative. ... Also thrown if the
///     selected primal_delta/dual_mu is NaN (NaN is not absorbed by
///     either field's negative-means-sentinel convention). Only the
///     value actually SELECTED is validated -- a stored field a finite
///     override supersedes is never read, so it is never rejected either.
```

Two defects against the body at `qp_engine.h:1706-1720`:

* **A negative stored value throws, and the docstring says it does not.** The code is
  `if (!(overrides.primal_delta >= 0.0) && (std::isnan(opts.primal_delta) || opts.primal_delta < 0.0)) throw …`
  (same shape for `dual_mu`). With default overrides and `opts_.primal_delta = -1e-8`, the
  SELECTED value is `-1e-8` — not NaN — and the call throws. The docstring's "Also thrown …
  is NaN" is therefore false of a live branch, and the parenthetical "negative-means-sentinel
  convention" actively invites the wrong reading. The deleted text stated the domain exactly:
  *"the domain enforced here is: NOT NaN, and >= 0."*
* **"Only the value actually SELECTED is validated" is contradicted by the first check.**
  `if (std::isnan(overrides.tr_radius) || overrides.tr_radius < 0.0) throw …` fires on a NaN
  override, which by the docstring's own selection rule ("the override if finite …") is *not*
  the selected value.

Correct text: `@throws std::invalid_argument if the SELECTED primal_delta/dual_mu (the
override when it is >= 0, else the stored field) is NaN **or negative**; and if the override
tr_radius is NaN or negative, or — at the +inf sentinel — the stored opts.tr_radius is.`

### 2 — `qp_engine.h:145-147`. COUNTER SEMANTICS merges two different counter claims and gets the repair half wrong.

Current text:

```
//    Section 4b's repair and 4c's ride cost one minor_iter like any other
//    step and are otherwise invisible to these counters on a convex H (the
//    inertia gate cannot fire kWrong there).
```

The pre-image kept these apart, and they are not the same:

* The **ride** genuinely "costs one minor_iter like any other step" and is invisible to
  `factorizations`/`schur_updates` (it reuses the EQP result the iteration already computed).
* The **repair** does not. The iteration whose gate came back kWrong is counted
  (`++counters.minor_iters`, `qp_engine.h:2200`) and then `continue`s to be retried from the
  repaired vertex (`qp_engine.h:2229`), so a repaired solve spends **one extra** minor_iter;
  and each pin/release the repair probes re-runs the dispatch, costing `schur_updates` under
  `kSchurBorder` and **one factorization per probe** under `kRefactorize`. The pre-image said
  so in those words ("shows up in all three counters").

As written the sentence is either vacuous (on a convex H neither mechanism fires at all, so
"costs one minor_iter" is wrong there too) or false (unqualified, the repair is not invisible
to these counters). This paragraph is the one the file itself flags as *"relied on downstream,
e.g. warm-start assertions"*, and the deleted per-probe factorization cost is exactly what a
downstream assertion would trip on.

Correct text: split them — "4c's ride costs one minor_iter like any other step and is
otherwise invisible to these counters. 4b's repair costs one EXTRA minor_iter (the kWrong
iteration is counted and then retried) plus, per pin/release it probes, schur_updates under
kSchurBorder or one factorization under kRefactorize. Neither is reachable on a convex H."

---

## Should-fix

### 3 — `ssn_engine.h:1067`. Dangling reference left by this pass's own citation removal.

```
    // preservation this project recognises (the identification-stall note's
    // "provably inert on every one of them" standard).
```

At `a9cba6a` the file introduced that note at `:21`
(`docs/notes/2026-08-03-identification-stall-study.md Sec. 3`); this pass deleted that line, so
"the identification-stall note" now names nothing in the file, and the document does not exist
in this repo. The report claims *"two dangling `\"the note's section N\"` references … are
rewritten to stand alone"* — this third one was missed. It is the only unresolvable
cross-reference left in either file (every other `see …` anchor was checked and resolves).
Fix: drop the parenthetical, or rewrite to "the strongest form of old-behaviour preservation
this project recognises: provably inert on every one of them."

### 4 — `ssn_engine.h:735`. "far below the overflow escape" names a mechanism that does not exist, and lost the number that made the comparison mean something.

```
// measured absolutely). Four orders is far above any legitimate dual growth
// between two points that made no progress on each other and far below the
// overflow escape.
```

Pre-image: *"far below the 1e150 the iterate would need to reach an overflow escape."*
`SsnEscape` (`ssn_engine.h:940-947`) has `kBudget/kSingular/kNoContraction/kInfeasibleSuspect/
kIndefinite` — no overflow reason — so with the magnitude gone the clause is a comparison
against nothing. Restore `1e150`, or delete the clause.

### 5 — `ssn_engine.h:747`. "THREE CONJUNCTS:" mislabels the three items under it.

```
// THREE CONJUNCTS:
//
//   (i) THE WINDOW ADVANCES ON ACCEPTED STEPS, NEVER ON ATTEMPTS. …
//   (ii) THE IMPROVEMENT DEMAND IS OVER THE WHOLE WINDOW, NOT PER STEP. …
//   (iii) THE GROWTH CONJUNCT IS DIFFERENT ON THE TWO ROUTES …
```

Six lines above, the same block says *"Both halves are required"* — the test has **two**
conjuncts (stall, growth). Item (i) is a window-advance bookkeeping rule and (ii) an
aggregation choice; neither is a conjunct, and (iii) is a property of the one that is. The
pre-image heading called them *"the stall conjunct's three degrees of freedom"*, which is
correct and does not collide with "both halves". Correct text: `THREE DESIGN CHOICES IN THE
STALL TEST:` (or restore "degrees of freedom").

### 6 — Residual plan/review labels beyond the four the brief lists.

The four known are present as expected — `eqp_solve.h:47`, `:49` ("Task 9");
`ssn_engine.h:1424`, `:1791` ("tycho rule T6"). Five more survive, and the report's claim that
`ssn_engine.h`'s label sweep is complete ("zero `Task N`/`PHASE-N`/`R1-R5`/reviewer-name hits
remain") is true only of those exact token classes:

* `ssn_engine.h:1664` — `// --- convergence, and THE SECOND-ORDER VERIFICATION (C1) ----`
  (`C1` is a review-round label; the same label was correctly stripped at `qp_engine.h:2673`,
  "the C1 defect described above" → "the defect described above").
* `ssn_engine.h:1062` — "12 is **the brief's registered value**, kept rather than re-derived".
* `ssn_engine.h:1460` — "**The brief for this task** calls the first parameter's type `QpData`".
* `qp_engine.h:2346` — "fiction (**audit finding D9**)." Its source citation was removed
  elsewhere in this pass, so the label is now unresolvable — the exact N8 pattern the prior
  review flagged.
* `qp_engine.h:2218` — "(**findings-doc policy**)". Two of the three `findings-doc` references
  were removed by this pass (pre-image `:517`, `:4283`); this one was left, so the surviving
  mention points at a document nothing in the file names.

### 7 — Four ruler lines survive in `ssn_engine.h` while every other ruler in both files was removed.

`ssn_engine.h:1432`, `:1434` (around "The engine") and `:2352`, `:2354` (around "THE DEFERRED
CERTIFICATION'S TWO CLOSING MOVES"). `qp_engine.h` is now at **zero** `// ====` rulers and
`ssn_engine.h` at four. Rulers are on the rules' delete list; the inconsistency is more
conspicuous now than before the pass.

---

## Nits

### 8 — `qp_engine.h:1662-1663`. `@param overrides` "primal_delta/dual_mu disable at any negative value" is ambiguous in the one place the distinction matters.

Read in parallel with "tr_radius disables at +inf" it reads as "disables the *feature*", which
is true for tr_radius and false for these two: negative is the *sentinel* (use the stored
value); **0** is what disables the regularization. The deleted text was explicit about exactly
this asymmetry. Suggest "…primal_delta/dual_mu are overridden only by a value >= 0; a negative
override means 'use the stored field'."

### 9 — `qp_engine.h:1654`. The file's only `///` block.

Report item 2 asked for a ruling. It is rules-compliant as written (rule 1 wants declaration
docstrings in Doxygen form) and it is the right shape for this declaration, so no change is
needed on accuracy grounds — but `qp_engine.h` is otherwise 100% `//` and `ssn_engine.h` is now
100% `//` (B3 fixed correctly: no `///` remains anywhere in it). Either is defensible; flagging
so the choice is deliberate rather than incidental.

### 10 — Ragged reflow left by the label strip (report item 3 predicted this).

`ssn_engine.h:1362` ("// and its"), `:1668` ("// which"), `:1234` ("// True iff this solve
reached a certifying"), and `qp_engine.h:1896-1897`. Also `ssn_engine.h:3123`, "The
residual-driven sizing: sigma = max(the ladder's monotone state, **the residual-driven
size**)" reads redundantly. No line exceeds 100 columns in either file — N3 is fully fixed.

### 11 — `qp_engine.h:1147-1148`. `mix_values` overstates the property it buys.

"mixing the shape in here too keeps values_hash collision-resistant **on its own**". The
pre-image scoped it: "values_hash alone is no longer collision-prone **across a shape
change**". Restore the qualifier.

### 12 — Live-test evidence pointers deleted unevenly.

Some were removed (`EqualityOnlyIsOneKktSolve`, `TrustRegionRadiusIsNotPartOfTheStructureKey`,
`RideDeclinesAFlatDirectionOnAPsdSingularQp`, `ZeroMultiplierProbeIsANoOpOnStrictComplementarity`,
`GateIsANoOpOnAConvexFixture`, `StructureChangeAtConstantValueBytesForcesRefactorization`,
`tests/sqp/test_qp_warm_start.cpp`'s probe) while others were kept (`qp_engine.h:130`, `:385`,
`:562`, `:579`, `:587`). These are pointers into the live test tree, not history — the prior
review's N7 made the same observation. Either policy is fine; pick one.

### 13 — `qp_engine.h:1071`. Pre-existing brittle citation, untouched by this pass.

`tests/test_qp_engine_indefinite.cpp:1742-1748 (repro seed 777 case 1613)` — a line-number-pinned
reference that will rot on the next edit to that file. Out of scope for this chunk; noted for a
follow-up.

### 14 — `qp_engine.h:553`. Pre-existing inaccuracy in a rewritten section.

"kMaxIter once **opts.max_iter** major iterations have been spent." The loop runs to
`eff_max_iter = detail::effective_qp_max_iter(qp_in, opts_.max_iter)` (`:2196`), which is the
size-derived cap whenever `max_iter` sits at its sentinel — the very mechanism section 4's own
"SIZE-DERIVED QP ITERATION CAP" block (`:782-810`) documents. Identical in the pre-image, so not a
regression, but the surrounding text was rewritten and this was the moment to fix it.

---

## Verified TRUE against the code (the brief's named re-derivations, plus the report's uncertainty list)

* **Reuse condition (e)** — `qp_engine.h:196-205` matches `:2099-2109` conjunct for conjunct
  (`session_id() == prev_border_kkt_session_id && epoch() == prev_border_kkt_epoch &&
  inertia().state == kObserved`). The added claim "advanced by every successful `factorize()`
  inside `rebuild_k0()`, **the sole site** that reassigns or refactorizes K0" is exact:
  `factorize_checked(border.kkt, …)` appears once in the file, at `:3525` inside `rebuild_k0`.
* **HotState DETACH guard** (report item 1, the one genuinely *new* prose in the pass) — the
  corrected text at `:1273-1281` ("a refused-reuse site whose `border_.use_count() > 1` … a
  sole-owned object is still wiped in place, to keep its KktFactor's symbolic analysis
  reusable") is exactly `:2144-2151`, and the second site at `:2398-2399` uses the identical
  guarded pattern. The pre-existing "at every `!reuse_eligible` site" error is correctly
  retired.
* **PER-SOLVE RADIUS VARIATION** (`:688-700`) — "a sentinel field resolves to the corresponding
  `opts_` value" matches the three resolution ternaries at `:1722-1726`; "every read site below
  this point consults those effective values, not `opts_` directly" holds for the three fields
  it is scoped to (the only surviving `opts_.` reads inside `run()` are `ws_algebra` and
  `max_iter`, neither overridable).
* **SSN sigma rule** (`:1157-1179`) — the stated `sigma_k = max(ladder_k, clamp(c·min(r,r²),
  kSsnProxInit, kSsnProxMax))` is `:1846-1848` plus `apply_sigma`'s floor; `kResidualArmed` is
  inert until `ladder_sigma_ > 0.0` and `kResidualAlways` is not (`:1844-1845`), as documented.
  **hint rule** (`:1181-1196`) — `watchdog_q >= 1` is enforced at `:2494`, `kWatchdog` gates
  `wd_used >= sopts.watchdog_q` at `:1924`. **Farkas rule** (`:1198-1215`) — "can only make the
  engine MORE reluctant to report kInfeasible" matches the arm/fire split.
* **Triplet-rebuild contract** (`ssn_engine.h:2711-2761`) — REBUILD returns `true` / REFRESH
  returns `false` (`:2904`, `:2950`); the refresh path does bounds-check every write and does
  require `t == value_pos_.size()`, throwing `std::runtime_error` (`:2887-2903`); conjunct 2
  (`bound_rows_match_cached`, `:2863-2872`) is an exact comparison, so "no collision exposure
  at all on this half" is right; the `-1.0` FB-diagonal placeholders are at `:2790`, `:2795`.
* **`tr_violation`** (`ssn_engine.h:1356-1359`) — `max_j max(0, x_j - up_eff_j, lo_eff_j - x_j)`
  over TR-derived rows is exactly `max(tr_violation, -slack_b(rr))` over `br.from_tr` rows
  (`:2977-2979`, `export_activity`). B1's corrupted sentence is repaired verbatim as recommended.
* **`price()` is the sole producer of `z`** (export-invariant block, `:2542-2566`) — `price`
  (`:3664-3667`) writes `z(i)` only where `bound_state[i] != kFree`; the other three `z` sites
  (`:1640`, `:2539`, `:2572`) only clear it.
* **`probe_drop_made` is a per-solve sticky bit** (`:407-411`) — declared `false` at `:2172`,
  set at `:2335`, never reset; gates the restart at `:2306`.
* **eqp_solve.h N1 restoration** — `reg(k) = delta for Hessian rows and -mu for constraint
  rows` is exactly `reg.head(n_free).setConstant(opts.primal_delta)` /
  `reg.segment(n_free, me + n_working).setConstant(-opts.dual_mu)` (`:206-207`).
* **Section 2's FB algebra** (`ssn_engine.h:120-177`) — `alpha - beta = (lambda - s)/rho`, so
  "ACTIVE ⟺ alpha > beta ⟺ lambda > s" is right and matches `export_activity`'s
  `lambda_b(rr) > slack_b(rr)` (`:2980`); the alpha-floor cancellation
  `(r/alpha_f)/(beta/alpha_f + mu) = r/(beta + mu·alpha_f)` is algebraically exact; the
  uncertain row's `-(1 + mu + sigma)` diagonal follows from `alpha = beta = 1 - 1/sqrt(2)` in
  `D = beta/alpha + mu`.

## Review items B1–B4 / N1–N4 — dispositions confirmed

| item | claimed | verified |
|---|---|---|
| B1 corrupted `tr_violation` sentence | FIXED | ✅ `ssn_engine.h:1356-1359`, exactly the recommended text |
| B2 six dangling `PHASE-3 DESIGN FRICTION` refs | FIXED | ✅ zero occurrences of the old title anywhere; the three surviving sites (`:1359`, `:4027`, `:1896`) name `PER-SOLVE RADIUS VARIATION`, which exists at `:688` |
| B3 `///`/`//` splice on `SsnStart::z`/`::slacks` | FIXED | ✅ both plain `//`; zero `///` in `ssn_engine.h` |
| B4 `eqp_solve.h:75` pointed at the sweep's report | FIXED | ✅ parenthetical dropped |
| N1 `reg` definition | FIXED | ✅ restored and exactly true of `:206-207` |
| N2 misattributed saddle fix | RESOLVED BY DELETION | ✅ sentence gone; 7b's own text carries the mechanism |
| N3 five >100-col lines | FIXED | ✅ zero lines >100 cols in all three files |
| N4 mis-indented `**PUBLIC-API PRECONDITION**` | FIXED | ✅ reflowed at `qp_engine.h:1488-1494` |

## Cross-reference audit

Every `see the X note` / `section N` anchor in both files was resolved by grep. All resolve
except finding 3. Spot-checks: `4c's WHICH MECHANISM FIRES` → `:497` (inside 4c, `:442-528`);
`section 4b's SECOND-ORDER CERTIFICATION` → `:325` (inside 4b, `:258-441`); `section 4b's COST`
→ `:432`; `HOT-START REUSE INTERACTION` → `:674`; `THREAD SAFETY` → `:239`; `THE RANK
PRE-SCREEN` → `:1469`; `INVALIDATION POLICY` → `:230`; HotState `OWNERSHIP` → `:1255`.
`ssn_engine.h:406`'s "see the Accelerate note below" → `ACCELERATE COROLLARY` at `:435`;
sections 1–8 all present at `:57/108/179/203/230/259/309/386/444`. `docs/pattern-hash.md` and
`docs/retarget-design-sqp.md` both exist in the repo, so the two surviving citations
(`ssn_engine.h:868` SS4.1, `:2098` SS4.2, `qp_engine.h:1165`) are live.

## Doxygen form

One `///` block in the chunk (`qp_engine.h:1654-1675`): one `@brief`, `@param` for both
parameters, `@return`, `@throws` on the throwing declaration, no promised-but-absent tag, no
floating block. Form is correct; the `@throws` *content* is finding 1 and the `@param
overrides` wording is finding 8.
