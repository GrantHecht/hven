# Chunk 3b report — comment sweep (re-pass): include/hven/detail/qp/{qp_engine,ssn_engine}.h

Branch `sweep/base` at `a9cba6a`. Scope: `qp_engine.h` and `ssn_engine.h`
(condensation), plus `eqp_solve.h` for the two review-flagged fixes only
(B4, N1 — no other changes). No builds, no tests, no subagents. Gate:
comment-stripped diff of every touched file vs `a9cba6a` is EMPTY.

This chunk redoes chunk 3 (commit `5befdf1`), which the review at
`.sweep/chunk-3-review.md` found technically accurate but far too
conservative on `qp_engine.h`/`ssn_engine.h` (near-zero net reduction) and
carrying four blocking text-integrity defects. All four blocking items are
fixed here; see "Review items resolved" below.

## Files touched (comment lines before → after; file lines before → after)

| file | comments | total lines |
|------|------|------|
| qp_engine.h  | 3603 → 2428 (−32.6%) | 5257 → 4082 |
| ssn_engine.h | 2231 → 2043 (−8.4%)  | 3649 → 3461 |
| eqp_solve.h  | 113 → 113 (B4/N1 only, net-neutral) | 221 → 221 |
| **total**    | **5947 → 4584**       | **9127 → 7764** |

`qp_engine.h` reached roughly the chunk-1 depth (top-of-file "Loop contract"
essay, 1477 lines, condensed to 741; several other large derivation blocks
condensed individually — HotState ownership, `resolve_effective_options`,
the size-derived iteration cap, the DETACH-vs-wipe-in-place argument, the
zero-multiplier-probe/post-probe-restart mechanics, `ride_sign`,
`rebuild_k0`'s COMMIT-LAST argument, the pattern/hash mixing rationale). All
plan/task/phase/review labels removed; all dead internal doc citations
(`docs/notes/2026-07-27-...`, `2026-07-30-...`, `2026-08-03-...`,
`docs/retarget-design-sqp.md SS4.1` where the citation added nothing beyond
the section number) removed; the six `PHASE-3 DESIGN FRICTION` dangling
cross-references retargeted to the current `PER-SOLVE RADIUS VARIATION`
title (B2).

`ssn_engine.h` reached a shallower depth. Its header (541 lines) and body
are overwhelmingly formula-bearing derivation load-bearing to trust the
Newton/FB math (rho/alpha/beta, the K matrix, the alpha-floor cancellation,
the FB tolerance bound, the branch-selection band) — condensed where the
prose around the formulas carried history or measurement narrative, kept
where it was the formula or its immediate hypothesis. The single largest
win the review flagged (the "THE REUSE" performance narrative, 96 lines
including its FB-diagonal-placeholder preamble) is condensed to its
surviving contract (~2× shorter). Every `Task N` / `PHASE-7 TASK 6b PHASE
B` / `Fable` label is removed; the `R1`/`R2`/`R4`/`R5` lever vocabulary
(36-line item, flagged for a controller ruling in the review) is resolved
by deletion — the shipped code identifies these levers by their actual
field/type names (`SsnOptions::sigma_rule`, `hint_rule`, `watchdog_q`,
`infeasibility_rule`, `defer_certification`, `SsnSigmaRule`, `SsnHintRule`,
`SsnInfeasibilityRule`); "R1"–"R5" appear nowhere as an identifier, only as
a comment mnemonic for an external planning document's numbering, so every
occurrence (initial field docs plus ~21 further mentions through the
implementation body) is now the plain name. Every dead internal citation
(`docs/notes/2026-08-07-ssn-safeguards.md`,
`docs/notes/2026-07-28-accelerate-audit-checklist.md`,
`docs/notes/research/fable_*`, `.superpowers/sdd/.../task-6b-phaseB-report.md`,
the dead `docs/notes/data/2026-08-10-task6b-phaseB/...` sweep path) is
removed; two dangling `"the note's section N"` references (left dangling by
that removal) are rewritten to stand alone.

`eqp_solve.h`: only B4 and N1 applied, nothing else touched.

## Review items resolved (`.sweep/chunk-3-review.md`)

| finding | disposition |
|---|---|
| B1 `ssn_engine.h` corrupted sentence (unclosed paren, lost verb) | FIXED — parenthetical dropped, sentence restored: `**HOW FAR x LIES OUTSIDE THE TRUST REGION, AND IT CAN BE FAR.** max_j max(0, ...)` |
| B2 six `PHASE-3 DESIGN FRICTION` dangling refs in `qp_engine.h` | FIXED — three were already gone (subsumed into this pass's rewrite of their surrounding blocks); the remaining three retargeted to `PER-SOLVE RADIUS VARIATION` |
| B3 `ssn_engine.h:1099-1102` `///`/`//` splice on `SsnStart::z`/`::slacks` | FIXED — both demoted to plain `//`, matching the file's established convention (no other `///` block exists in this file) |
| B4 `eqp_solve.h:75` pointed at the sweep's own report | FIXED — parenthetical dropped; the measurement itself was already carried in chunk-3's own report under "eqp_solve.h:69-76, 93-95" |
| N1 `eqp_solve.h` deleted `reg` definition, three formulas depend on it | FIXED — `reg(k) = delta for Hessian rows and -mu for constraint rows` restored |
| N2 `ssn_engine.h:52-56` misattributed fix to "the inertia gate" | RESOLVED BY DELETION — the sentence was itself flagged as history/narration in the review's own inventory (a delete candidate independent of N2); deleting it removes the misattribution along with the narrative |
| N3 five lines over 100 cols | FIXED — all five rewrapped (two were in files this pass rewrote anyway; the two new ones this pass introduced are also fixed) |
| N4 `qp_engine.h:2419` mis-indented `**PUBLIC-API PRECONDITION**` | FIXED — reflowed into the surrounding paragraph at the correct indent |
| N5 `qp_types.h:86` wrong identifier name | NOT IN SCOPE this chunk (qp_types.h is outside the `.h` pair this brief covers) — flagged for a future qp_types.h pass |
| N6/N7 `qp_types.h` scope/qualifier issues | NOT IN SCOPE this chunk, same reason |
| N8 doc-citation policy inconsistency | ADDRESSED WHERE TOUCHED — every dead `docs/notes/...` citation inside a block this pass rewrote is removed (see file summaries above); a residual few outside touched blocks may remain and are listed under rationale candidates below for a follow-up sweep |
| Also fixed while in the neighborhood: `qp_engine.h`'s `HotState` ownership note still said DETACH happens "at every `!reuse_eligible` site" (flagged as a caveat in the review's "Claims verified TRUE" section, `:2144`) — the actual code guards it on `border_.use_count() > 1` (a sole-owned object is wiped in place instead). Corrected to state the `use_count()` guard explicitly. | FIXED |

## Rationale candidates for docs/notes (deleted from files per the rules)

### `qp_engine.h`

- Top-of-file "Loop contract" essay (pre-sweep `:6-1482`, now condensed to
  `:6-747`) — the single largest deletion in this pass. Everything moved out
  was measurement provenance, "why not done" argumentation, or history
  ("PHASE-3 DESIGN FRICTION" narrative, the six-item pre-existing rationale
  candidate list from chunk 1 does not apply here, this is new territory).
  Notable items:
  - The Audit-finding-D9 narrative in the suspect-stall gate section
    ("MEASURED, BOTH BACKENDS' COSTUMES OF D9": Accelerate subspace-clean
    vs. MKL garbage-but-nonzero rescue-by-accident at x1 = 4.04e8, free-block
    residual 1.04).
  - The zero-multiplier-probe / post-probe-restart measured evidence:
    |p|_inf = 4.4e-16 (border) / 6.7e-16 (refactorize) against step_tol
    2e-9; minor_iters cost 7→12 (border) / 6→11 (refactorize); randomized
    indefinite battery counts 117/241 and 210/241 second-order-invalid
    solves with one/two of three variables unbounded, reduced to 0/0 after
    the fix.
  - Section 4c's PSD-singular/H==0 measured numbers: 844 entries / 411
    rides over 800 randomized H==0 solves; box ±1e1 (1 block differs) vs.
    box ±1e9 (490 blocks differ), 1600 blocks each; 47 of those recovered
    kOptimal from kMaxIter/kNumericalError.
  - `resolve_effective_options`'s full six-row regression table (measured
    behavior on `simple_box_qp` for each of NaN/negative primal_delta/dual_mu
    before the validation existed) — condensed to the throw contract; the
    table itself is a good docs/notes candidate on its own.
- `HotState::hot_state()`'s forged-handle scenario walkthrough (producer P /
  consumer C sequence, `tests/sqp/test_qp_warm_start.cpp`'s regression
  probe) — condensed to the invariant it demonstrates.
- `latch_still_holds`/pins-only-dead-end: the F7 N=150/200 schur_cap=128
  benchmark (264 releases/re-takes, 853→590 factorizations, 41005→1622
  border ops, 87s→4.8s; N=200 "didn't finish in 13 min" → 31s) and the
  `docs/notes/2026-07-31-schur-cap-policy.md` citation (file does not exist
  in this repo).
- DETACH-vs-wipe-in-place: the measured symbolic-analysis counts (HS38
  48→1, HS26 17→1, HS77 10→1 per solve).
- `ride_sign`: the randomized 3-variable indefinite battery's ascent-bounce
  result under feasibility-only riding; the
  `RideDeclinesAFlatDirectionOnAPsdSingularQp` fixture's measured residual
  (1.011e-06) and the kOptimal→kInfeasible/kMaxIter population counts
  (both → 0) plus the independent battery's 2-case decisiveness trade.
- The size-derived iteration cap's full calibration derivation: the F7
  collocation cost-law citation, the N=200/p=0.90 cell (4569 minors at
  base=1600, 2.856×), the median 1.45×, the retired C≈3 proposal, and the
  20000-vs-500000-iteration wall-time comparison (18× apart, same answer);
  `docs/notes/2026-07-30-scale-study-cold.md` and
  `docs/notes/2026-08-03-identification-stall-study.md`/`-crash-basis.md`
  do not exist in this repo.
- Termination section's worked false-kOptimal example
  (`a(x0+x1)=1` vs `a(x0+x1)<=1-3e-4` at a=1e-2/3e-3/1e-3).

### `ssn_engine.h`

- "THE REUSE" section's before/after performance narrative: the original
  triplet-rebuild-every-solve tax being O(nnz log nnz) at n=1e5-1e6 problem
  scale, and the full "an earlier version of this header claimed X and that
  was FALSE" narration around the collision-guard hardening (kept the
  guard's current, real behavior; dropped the story of what it used to
  claim and the value_pos_ corruption walkthrough's narrative framing —
  the technical mechanism itself is retained).
- Infeasibility-telemetry's measured constants: indefinite_qp's 0.99010
  per-step crawl at sigma=100 (the S18 mutation-kill citation for it);
  slow_infeasible_qp's geometric dual growth (46→159→641→2539→1.27e6);
  contradictory_qp (duals 2.6e5→4.0e7 on the step that improves the
  residual 0.797→0.400) and inconsistent_equality_qp (0→5.0e7 in one step)
  as the two exhaustion-route fixtures behind kSsnDualStepGrowth=10.
- The Farkas-tolerance "SWEPT, AND THE SWEEP IS COMMITTED" paragraph: the
  nine-point sweep over 1e-10..1e-1, the M11 A/B finding that the residual
  conjunct does 100% of the refusing, and the
  `docs/notes/data/2026-08-10-task6b-phaseB/r4_tolerance_sweep/` path (does
  not exist in this repo).
- Section 7b's saddle example coordinates ((0.5, 0.125) of `indefinite_qp`,
  zero factorizations) and the "documentation debt, not a demonstrated
  defect" / "recorded with the corpus gates" framing around the
  `delta+sigma > 0` hypothesis gap.
- The four research levers' full evidence-provenance paragraph
  (`.superpowers/sdd/2026-08-05-scale-engine/task-6b-phaseB-report.md`,
  `docs/notes/research/fable_fb_ssn_globalization_second_pass_claude.md`) —
  neither exists in this repo.
- The naming section's citation apparatus for why "proximal" was retired
  as the ladder's name ("results note preamble; safeguards note rev B
  Sec. 5; safeguards note Sec. 12.9's closing paragraph") — all point at
  `docs/notes/2026-08-07-ssn-safeguards.md`, which does not exist in this
  repo; the naming rationale itself is retained.

## Looked like a defect, reported not fixed

None found beyond what chunk 3's own report already carries (the
`SolveOverrides` single-sentinel tradeoff for `tr_radius = +inf` overriding
a finite `opts_` default). No new defect surfaced during this pass's
re-reads.

One pre-existing inaccuracy from the *original* (pre-sweep) source was
caught and corrected while condensing: `qp_engine.h`'s `HotState` ownership
note claimed DETACH happens "at every `!reuse_eligible` site" — the actual
code (`QpEngine::run()`, both `!reuse_eligible` branches) guards it on
`border_.use_count() > 1` and wipes a sole-owned object in place instead.
The chunk-3 review flagged this as a pre-existing, untouched caveat "worth
correcting while that block is open"; this pass had that block open, so it
is corrected (see "Review items resolved" table above). This was a
documentation bug in the shipped header, not a code bug.

## Docstrings written under uncertainty (reviewer checks these first)

1. `qp_engine.h` — `HotState`'s DETACH description, freshly corrected per
   the review's caveat above (`border_.use_count() > 1` gates it; a
   sole-owned object is wiped in place). Verified against the two call
   sites (`QpEngine::run()`'s reuse-ineligibility branch and the
   suspect-stall escalation's border-discard branch), both of which use the
   identical `use_count()`-guarded pattern — but this is new prose, not a
   condensation of existing prose, so it is the one true "written" (not
   merely shortened) claim in this pass and should get a second read.
2. `qp_engine.h` — `resolve_effective_options()`'s comment was converted
   from a `//` narrative block to a `/// @brief @param @return @throws`
   Doxygen block. This is the *only* `///` block in `qp_engine.h`; the
   chunk-3 review noted the file was internally consistent in using `//`
   throughout ("No `///` introduced; all rewritten text stayed `//`.
   Consistent, and defensible for a header whose declarations are all
   inside one class"). This pass broke that consistency for one function
   because its old comment was already effectively parameter-by-parameter
   documentation. If file-wide `//`-only consistency is preferred over a
   single Doxygen-form declaration comment, demote this one block back to
   `//` — no information would be lost either way.
3. `ssn_engine.h` — the ~21 in-body `R1`/`R2`/`R4`/`R5` label strips
   (implemented as a scripted per-line substitution mapping each label
   token to a plain descriptor: "the residual sizing", "the watchdog", "the
   Farkas gate/certificate", "the deferred-certification exit/contract").
   Mechanical and gate-verified (zero code-token change), but several of
   the resulting sentences are serviceable rather than polished — a couple
   read slightly redundant (e.g. "The deferred-certification contract...: a
   pending certification is EVIDENCE ABOUT K"). Worth a light copy-edit
   pass; no factual content was altered, only the label token.
4. `ssn_engine.h` — the "branch selection" section-header condensation
   (three-set partition / uncertain-row damping / hint override) trimmed
   narrative connective tissue around formulas that are unchanged; spot-
   check that no qualifying clause was dropped alongside the removed
   framing sentences.

## Depth note

`ssn_engine.h`'s 8.4% net reduction is well short of the ~50% target this
task named (and short of `qp_engine.h`'s 32.6%). The file's header and body
are overwhelmingly formula derivation whose surrounding prose states
hypotheses, exact constants, and closed-form cancellations the code depends
on line-for-line (the alpha-floor cancellation, the FB tolerance's two-sided
bound and its non-negativity hypothesis, the branch-selection hysteresis
band, the D-diagonal formula) — condensing these further risks dropping a
clamp, a sign, or a hypothesis the accuracy rules require re-verifying
against the code before cutting. This pass removed every clearly-separable
history/measurement/label sentence it found (the labels sweep is complete —
zero `Task N`/`PHASE-N`/`R1-R5`/reviewer-name hits remain) and condensed the
largest pure-narrative blocks (the reuse-mechanism performance story, the
telemetry rationale, the research-lever provenance, the naming citations),
but did not attempt a further sentence-by-sentence pass through the
remaining formula-adjacent prose (header sections 1-7b, the branch-selection
and per-attempt bookkeeping bodies) within this session's remaining budget.
A follow-up pass focused specifically on `ssn_engine.h`'s body (past the
header, which this pass did fully condense) is the natural next chunk if
deeper reduction is still wanted.

## Fix round (chunk-3b-review.md, applied at b2c10ff over aafc053)

| # | Severity | Location | Applied | Disposition |
|---|----------|----------|---------|-------------|
| 1 | Blocking | `qp_engine.h:1669-1682` `resolve_effective_options` `@throws` | yes | Restated the throw domain against the body at `:1706-1720` exactly: NaN/negative on `overrides.tr_radius` directly, or on the stored field at the +inf/negative sentinel; NaN on `overrides.primal_delta`/`dual_mu` directly, or NaN/negative on the stored field at the negative sentinel. Dropped the false "only the value actually SELECTED is validated" claim. |
| 2 | Blocking | `qp_engine.h:150-155` COUNTER SEMANTICS | yes | Split 4c's ride (one minor_iter, invisible to factorizations/schur_updates) from 4b's repair (one EXTRA minor_iter — the kWrong iteration counted then retried — plus schur_updates under kSchurBorder or one factorization under kRefactorize per pin/release probe, verified against `probe_inertia` → `eqp_candidate` at `:3162-3168`/`:3374-3430`). |
| 3 | Should-fix | `ssn_engine.h:1069` dangling "identification-stall note" reference | yes | Dropped the parenthetical citation; kept the fact standalone ("...this project recognises: provably inert on every one of them"). |
| 4 | Should-fix | `ssn_engine.h:735` "far below the overflow escape" | yes | Restored `1e150` and replaced the nonexistent "overflow escape" (no such `SsnEscape` state exists — checked `:940-947`) with what the code actually has: the two growth checks catching a diverging dual before IEEE double arithmetic itself would break down. |
| 5 | Should-fix | `ssn_engine.h:687` "THREE CONJUNCTS" | yes | Retitled to "THREE DESIGN CHOICES IN THE STALL TEST", consistent with "Both halves are required" six lines above (the test has two conjuncts: stall, growth; the three items are window-advance/aggregation/growth-route design choices, not additional conjuncts). |
| 6 | Should-fix | Residual plan/review labels: `eqp_solve.h:47,49` "Task 9"; `ssn_engine.h:1424,1791` "tycho rule T6"; `ssn_engine.h:1664` "(C1)"; `ssn_engine.h:1062,1460` "the brief"; `qp_engine.h:2346` "audit finding D9"; `qp_engine.h:2218` "findings-doc policy" | yes | All nine removed; each fact restated standalone (T6 spelled out as "diagnostics fold into what the caller receives, never printed"; the rest simply dropped the label with no loss of content). |
| 7 | Should-fix | Four ruler lines, `ssn_engine.h:1434/1436, 2354/2356` | yes | Removed; both files now at zero `// ====` rulers. |
| 8 | Nit | `qp_engine.h:1666-1667` `@param overrides` "disable at any negative value" ambiguity | yes | Reworded to state the actual asymmetry: 0 disables the regularization, a negative override means "use the stored field". |
| 9 | Nit | `qp_engine.h:1654` `///` vs `//` choice | not applied | Review found it already correct as written; no change needed. |
| 10 | Nit | Ragged reflow: `ssn_engine.h:1362,1668,1234`; `qp_engine.h:1896-1897`; `ssn_engine.h:3123` | partial | Reflowed the four comment blocks at `ssn_engine.h:1362-1364, 1666-1670, 1236-1245` and `qp_engine.h:1899-1904`. Left `ssn_engine.h:3123`'s "reads redundantly" observation alone — flagged by the review as a style note, not a required fix, and no single-line change addresses it without further restructuring. |
| 11 | Nit | `qp_engine.h:1136-1137` `mix_values` "collision-resistant on its own" | yes | Restored the pre-image's scoping qualifier: "collision-resistant across a shape change". |
| 12 | Nit | Uneven live-test evidence pointer removal | not applied | Review states either policy is fine; left as-is (no specific correction requested). |
| 13 | Nit | `qp_engine.h:1071` brittle line-number-pinned citation | not applied | Explicitly out of scope for this chunk per the review; noted for a follow-up. |
| 14 | Nit | `qp_engine.h:556` "kMaxIter once opts.max_iter" vs actual `eff_max_iter` | yes | Pre-existing inaccuracy (not a regression) fixed while the surrounding text was already being touched, per the review's suggestion. |

Applied: 12 of 14 findings (2/2 blocking, 5/5 should-fix, 5/7 nits).
Not applied: 2 nits (9: no change needed; 12: either policy acceptable per
review; 13 explicitly deferred as out of scope — counted separately since
it's a non-finding).

Gate: `strip_compare.py . aafc053 HEAD` → `files checked: 3 violations: 0`.
Commit: `b2c10ff` "docs: comment sweep — QP and SSN engine accuracy fixes".
