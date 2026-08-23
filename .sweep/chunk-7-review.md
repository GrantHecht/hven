# Chunk 7 review — model + drivers (hven `sweep/model-drivers`, 731bef5 over 6f03ca1)

Read-only comment-accuracy review. No builds, no edits, no subagents.
Ground truth: the code at 731bef5 plus `docs/notes/2026-08-m4-ledger.md`.
Rubric applied: `.sweep/comment-rules.md` plus the settler's binding rule — a
Doxygen block is a CONSTRAINT ON THE CALLER (takes / returns / throws /
invariant), never the record of why.

## Verdict

**CHANGES REQUESTED.** 4 Blocking, 7 Important, 11 Minor, 3 Nits.

The mechanical gate is independently confirmed (below), and the bulk of the
sweep is good, faithful work: the D5-amended claim-block contract is stated
correctly everywhere it is stated, the S6 split between `claim_stream_source.h`
(domain contiguity) and `nlp_aggregate.h` (serial issue / partition-index order
only) is intact, the invalidate-first ordering, the lazy laid state, the
`require_master_lists_unmoved()` on-every-read rule, the fixing-row lift /
re-append and its one-row-per-piece contract, and the thread-mode freeze are all
TRUE as written. What blocks is (1) a third of the chunk's declared scope was
never opened, (2) the registered M5 item the grant named as the point of the
chunk was not folded — the blocks the report names as "the four" are not the
registered pair set, (3) one D5-order falsehood survives in the one test file
the brief specifically added, and (4) one condensation turned a past-tense
history sentence into a false present-tense claim about live behaviour.

## Gate re-verification (independent)

Comment-stripped before/after comparison over all 25 changed `.h`/`.cpp` files,
string/char literals respected: **empty — zero code-token change.** Confirmed
independently of the report's own claim. No line in any changed file exceeds the
100-column limit except finding I5.

---

## Blocking

### B1 — A third of the declared scope was never opened (report §Scope)

The chunk's scope, as the grant and `.sweep/chunk-7-report.md` both state it, is
"every `.h`/`.cpp` under `include/hven/model/`, `include/hven/detail/model/`,
`include/hven/detail/drivers/`, `src/model/`, `src/drivers/`". Eight in-scope
files carrying **667 comment lines** are absent from the commit and unmentioned
in the report:

| File | comment lines at 731bef5 |
|---|---|
| `src/drivers/aggregate_eval_seam.cpp` | 205 |
| `src/model/nlp_model_aggregate.cpp` | 146 |
| `include/hven/detail/drivers/interior_point_solver_presets.h` | 107 |
| `src/model/aggregate_declaration.cpp` | 65 |
| `src/model/nlp_problem_model.cpp` | 44 |
| `src/model/nlp_adapter.cpp` | 42 |
| `src/model/nlp_solver.cpp` | 32 |
| `src/drivers/interior_point_solver_print.cpp` | 26 |

Two of them are the chunk's own subject matter: `aggregate_eval_seam.cpp` is
where the claim-block scan lives, and `aggregate_declaration.cpp` is where
`validate()`'s refusals live. The report's Files-touched table presents 25 rows
as the chunk, with no line saying these eight were read and found already
compliant. Either sweep them or record, per file, that they were examined and
needed nothing.

### B2 — The registered M5 was not folded; the report identifies the wrong four blocks

The ledger registers M5 out of Task 7 Part A as "**four header blocks of
settlement narration, each duplicated in the .cpp**", routed explicitly to "the
header-comment cleanup sweep, model/drivers chunk". The pair set that answers
that description is in `include/hven/model/non_linear_program.h` against
`src/model/non_linear_program.cpp`:

| Header block | .cpp twin |
|---|---|
| `non_linear_program.h:1313` `/// RUNS FIRST, at the top of make_nlp() and at the top of rebuild_structures()…` | `non_linear_program.cpp:27` "FIRST, before anything below mutates a master list or can throw…" **and** `:221` "INVALIDATION FIRST, before the eager scalars are written…" |
| `non_linear_program.h:1292/1298` `/// MUTATING THE MASTER LISTS AFTER A LAY… /// CALLED ON EVERY READ…` | `non_linear_program.cpp:393` "EVERY read, not only the one that still owes a copy…" |
| `non_linear_program.h:1238` `/// Both halves, and after the partitioning rather than before it, so the invariant is uniform…` | `non_linear_program.cpp:257` "The partition copies too, and that is the point of doing this after the partitioning rather than inside it…" (+ `:310`) |
| `non_linear_program.h:1273` `/// The lists are COPIES rather than views over the master lists, and that is the declaration type's own decision rather than this provider's…` | `non_linear_program.cpp:374` "The copies are DECLARATION data, and a thread mode is settable in a declaration…" |

**Every one of those header blocks is byte-identical before and after.** The
only change the sweep made anywhere in that private section is the removal of
two `////…` ruler lines at `non_linear_program.h:1200`. The narration is still
in the header, still duplicated in the .cpp, and each header copy is a record of
*why the placement was chosen* — the exact shape the settler's rubric excludes
from a Doxygen block.

The report's §"The four narration blocks and what each condensed to" instead
names blocks in `nlp_model_aggregate.h`, `structure_identity.h`,
`claim_space.h` and `nlp_aggregate.h`. Three of those four headers **have no
`.cpp` at all** (`structure_identity.h`, `claim_space.h` and `nlp_aggregate.h`
are header-only), so they cannot be blocks "each duplicated in the .cpp" and
cannot be the registered set. Those four were swept, and swept well — but M5 is
a different, unaddressed item.

Correct treatment for each: keep the caller-facing constraint in the header
(e.g. for `invalidate_laid_state`: "Runs at the top of `make_nlp()` and
`rebuild_structures()`, before any master list is touched and before any eager
scalar is written; between it and the epoch bump the declaration's piece lists
are empty."), and leave the two-failure-mode argument in the .cpp only.

### B3 — `tests/sqp/support/claim_stream_double.h:10-11` still asserts an order obligation

Current text:

```
// The seam's boundary checks are about streams no in-tree provider can produce
// -- a coordinate named twice inside one domain, blocks that overlap or arrive
// out of order, a location past the end of the destination it addresses.
```

D5 was **amended** (`2026-08-m4-ledger.md:1509-1521`): "The claim-block check is
DISJOINTNESS + bounds + a partition of [0, total_claims) -- NOT an order between
domains… The order predicate is gone." The code agrees explicitly —
`src/drivers/aggregate_eval_seam.cpp:330-334`:

```
// It does NOT require them to arrive in any particular
// ORDER: nothing here derives one domain's base from another's count, so a
// provider that lays its equality Jacobian ahead of its Hessian is served
// correctly.
```

This file was swept in this very commit (the brief's mid-chunk amendment), so
the sentence was read past rather than inherited untouched. Per the review
brief, any comment claiming an order obligation is blocking.

Correct text: `-- a coordinate named twice inside one domain, blocks that
overlap, a location past the end of the destination it addresses.`

### B4 — `src/drivers/interior_point_solver.cpp:813-816`: a condensation that is now false

Before:

```
// same four concrete types (ClassicMeritAcceptance, BacktrackingLineSearch,
// ClassicAdaptiveGovernor, NoopRecovery) that set_nlp() used to construct —
// only the MOMENT of construction moves (every solve entry vs. every
// (re)transcription).
```

After (731bef5):

```
// same four concrete types (ClassicMeritAcceptance, BacktrackingLineSearch,
// ClassicAdaptiveGovernor, NoopRecovery) set_nlp() constructs —
// only the MOMENT of construction differs (every solve entry vs. every
// (re)transcription).
```

`rebuild_globalization_components()` has exactly one caller —
`interior_point_solver.cpp:3539`, inside `run_phase_sequence()` — and
`interior_point_solver.cpp:768` in the same file says so in as many words:
"…`rebuild_globalization_components()`, **NOT here**". `set_nlp()` does not
construct those four types. The tense change converts a true history statement
into a false claim about live behaviour, and it contradicts both its own
neighbouring clause and the paragraph's surviving tail ("…the old
per-transcription construction", still present two lines below).

Correct text: restore the past tense — `…that set_nlp() used to construct — only
the moment of construction moved (every solve entry rather than every
(re)transcription).` — or drop the neutrality paragraph entirely as history.

---

## Important

### I5 — `src/drivers/sqp_driver.cpp:1441`: garbled merged comment line (147 columns)

```
            // The ingested duals are gone -- these are the restoration            // sub-solve's own subgradient selectors -- so the ingest-scoped
```

Two comment lines were merged with the second line's `// ` marker left embedded
mid-line and its indentation run kept as filler. The only line in the whole
commit past the 100-column limit. Should read as two lines:

```
            // The ingested duals are gone -- these are the restoration
            // sub-solve's own subgradient selectors -- so the ingest-scoped
```

### I6 — `include/hven/model/nlp_solver.h:52`: new docstring over-claims atomicity

```
/// @brief Transcribes now: builds the model, core and program and adopts
///        the program. A failure leaves the previous state whole.
```

`src/model/nlp_solver.cpp:56-64` documents the counter-case at the site: "The
one step that is not this function's to make atomic: `set_nlp` adopts the
program and then does work of its own that can throw… A throw inside it leaves
the optimizer holding the new program while the members below still name the
old one." The recovery is a re-transcription on the next solve, not wholeness.
Correct text: "A failure leaves this solver retriable — `do_transcription_`
stays true and the next solve re-transcribes."

### I7 — `include/hven/model/nlp_solver.h:68`: `@throws` names the wrong condition

```
/// @throws std::runtime_error if no solve has run yet.
```

The guard in `src/model/nlp_solver.cpp` is `if (!this->model_)` — no
*transcription*, not no solve. After `transcribe()` (or `jet_initialize()`)
with no solve, `model_` is non-null and the call returns zeros via
`model_multiplier_block`'s empty-block branch. The same helper also throws
`std::runtime_error` on a short solver block, which the docstring does not name.
Correct text: "@throws std::runtime_error if the problem has not been
transcribed, or if the solver's multiplier block is shorter than the
transcribed row count."

### I8 — `include/hven/model/non_linear_program.h:284,290`: `adopt_declaration`'s refusal contract omits the entry's own two refusals

```
/// EVERY REFUSAL IS THE DECLARATION'S OWN, and is made before anything
/// moves…
/// @throws std::invalid_argument through @p declaration.validate() -- …
```

`adopt_declaration` makes three refusals of its own, none routed through
`validate()`:

* `src/model/non_linear_program.cpp:116` — the I1 piece-less-declaration refusal
  ("the declaration carries no pieces but declares {0} equality and {1}
  inequality rows"), which the ledger records as deliberately the **ENTRY's**
  rule and not the TYPE's;
* `:150` — a lifted fixing row naming more than one constraint row;
* `:158` — the I2 c-index band check against `[user_equal_cons_, equal_cons_)`.

Block unchanged by this commit, so not introduced here — but the review's item
(a) asks for `@throws` domains matching the site-named refusals exactly, and
these do not. "EVERY REFUSAL IS THE DECLARATION'S OWN" is now simply false.
Correct: keep the before-anything-moves guarantee, restate the attribution as
"every refusal is made before anything moves", and add the three entry-side
cases to `@throws`.

### I9 — `include/hven/model/nlp_problem.h:44-53`: the migration remedy was deleted, not condensed

Deleted:

> …and an equality row declared at +/-1e20 on both sides becomes an equality at
> 1e20 rather than a dropped free row. **Rewrite such declarations to use the
> infinities.**

The surviving text keeps the diagnosis ("arrives here as genuine bounds at
+/-1e20 rather than free") but drops the instruction telling a migrating caller
what to do. That is a caller-facing remedy on a public interface, not rationale;
routing it to `docs/notes` (report §rationale candidates) puts the fix
somewhere the reader of the header will not look. Restore the two sentences.

### I10 — Stale directory in six surviving cross-references, plus the history they carry

`src/solvers/` does not exist in hven (the tree has `src/drivers/`,
`src/model/`, `src/kkt/`, …). Six in-scope occurrences survive the sweep, each
inside a "moved VERBATIM from …" history sentence the rules strip:

* `src/drivers/interior_point_solver.cpp:294`, `:393`, `:960`
* `src/drivers/interior_point_solver_globalization.cpp:10`, `:808`, `:1069`

e.g. `interior_point_solver_globalization.cpp:808`: "max_step_to_boundary and
max_primal_dual_step are moved VERBATIM from src/solvers/
interior_point_solver.cpp". Either drop the provenance clause (history, per the
rules) or repoint it at `src/drivers/interior_point_solver.cpp`. (The same
string appears in ~9 `include/hven/detail/globalization/*.h` headers, which are
another chunk's scope — worth flagging to the controller as a follow-on.)

### I11 — `src/drivers/sqp_driver.cpp:222`: dangling cross-reference created by this commit

```
    // require_declared_box at the top of this file for why the check sits
    // there and for
    // why the check moved rather than changed: …
```

The "moved rather than changed" explanation was the paragraph the same commit
deleted from `require_declared_box` ("AT THE MODEL BOUNDARY NOW, which is a move
of one call site and not a change of rule. The check used to sit at the top of
solve_impl…"). The pointer now names something that is no longer there. Drop
"and for why the check moved rather than changed".

---

## Minor

### M12 — `include/hven/model/structure_identity.h:37-44`: a rule-3 "do not repair this" callout deleted

Deleted from `ModelStructureKey`:

> The partition count is an explicit conjunct even though the claim stream is
> itself partition-sensitive… The redundancy is deliberate: an explicit field is
> inspectable in a diagnostic where an order-sensitivity is not…

This is precisely rule 3's "a deliberate deviation a reader would otherwise
'fix'" — the next reader sees a redundant field and removes it. The report
classifies it as a rationale candidate; it belongs in the header as one
sentence: "`partition_count_` is deliberately explicit though the claim stream
is already partition-sensitive: an inspectable field where an order-sensitivity
is not."

### M13 — `include/hven/model/claim_space.h:266`: a deliberate negation dropped

Before: "Same sentinel discipline as the KKT table's clash marks, **though NOT
for the same reason -- the reason here is the weaker protocol, not the stronger
one.** A filler that tests `row >= 0` skips -1 and -3 alike…"
After: "Same sentinel discipline as the KKT table's clash marks: every published
mark is either the one sentinel (-1, dropped)… or an index genuinely in range."

The condensation is factually true but silently transfers the KKT table's
rationale onto the RHS table, which the original went out of its way to deny.
Keep the four-word denial.

### M14 — `include/hven/model/non_linear_program.h:387`: a sentence that no longer answers its own framing

```
// differ only in what classification records: MakeConstraint appends one
// internal equality row per fixed variable (re-laying the layout over the
// widened row space), RelaxBounds records an ordinary relaxed bound pair
// (changing nothing structural).
```

The framing promises "what classification records"; the deleted clause was
exactly that for MakeConstraint — "records no bound for the variable (it goes to
the solver free)". As it stands, the MakeConstraint half describes what is
appended, not what is recorded. Restore the parenthetical.

### M15 — `include/hven/model/nlp_aggregate.h:355`: lifetime rule dropped

"…and returning a reference to anything temporary would dangle" removed from
`declaration()`'s contract. Rule 3 protects lifetime rules; the surviving text
gives only the cost argument.

### M16 — `src/model/non_linear_program.cpp` `get_mat_space`: two navigational pointers dropped

"every KKT scatter site **(kkt_fill_all and kkt_fill_hess in
dense_function_base.h)** locks…" lost its site names, and "…hence no runtime
check **(… see kkt_canonical_lock_col's doc comment for the structural
argument)**" lost the pointer to where the argument actually lives. The history
half of the second parenthetical was correct to drop; the pointer was not.

### M17 — `include/hven/model/aggregate_declaration.h` `validate()` `@throws` reads exhaustive but is not

The list ("for any of: …") omits the two checked-narrowing refusals the M4 fold
added — `src/model/aggregate_declaration.cpp:51` (piece row sum past `INT_MAX`)
and `:141` (equality piece count past `INT_MAX`). Either add them or soften the
lead-in.

### M18 — Residual labels

* `T6` (a CLAUDE.md project-rule id) survives at
  `src/drivers/interior_point_solver.cpp:1638`;
  `interior_point_solver_globalization.cpp:89, 585, 612, 1920, 1945, 2139,
  2174`; `interior_point_solver_settings.cpp:736`. Replace with the plain
  statement ("library code never prints a diagnostic it does not also fold into
  the thrown message").
* `B-1` (a review-finding id) survives at `src/drivers/sqp_driver.cpp:527, 704,
  710, 734, 763` — the sweep stripped the surrounding "Phase-5 Task 7b" but left
  the label, so it now names nothing resolvable in-tree. (It also appears in
  four out-of-scope headers, so a rename wants tree-wide coordination.)
* `src/drivers/sqp_driver.cpp:210` still carries "Left unchecked it **used to
  be** certified" — history phrasing.

### M19 — `include/hven/model/nlp_model.h`: the only in-tree override pointer dropped

"See tests/sqp/support/scale_problems.h's F7CollocationChain override for the
one model in this tree that exercises the override rather than the default" was
removed from `eval_values` and does not appear in the report's rationale-candidate
list. That is a navigational fact about an opt-in contract, worth one clause.

### M20 — `include/hven/model/structure_identity.h:166`: malformed `@throws`

`@throws Whatever materialize_variable_bounds throws -- …` — Doxygen takes the
first token as the exception name, so this renders as an exception called
"Whatever". Use `@throws std::invalid_argument whatever
materialize_variable_bounds throws — …`.

### M21 — Ragged reflow left behind by label-stripping

Removing a leading label without rejoining the paragraph leaves orphan
half-lines throughout `src/drivers/sqp_driver.cpp` — e.g. `:106` ("// Timed
around solve_impl ALONE -- never around" / "// model construction, …"), `:119`,
`:126`, `:1252`, `:1400`. Cosmetic, but it is the same mechanism that produced
I5, so a reflow pass over that file is the fix for both.

### M22 — `src/drivers/solver_init.cpp:29`: return-contract comment removed

`// 0.0 on every call after the first (header contract)` deleted. No information
lost (the header carries it, and this commit put it under `@return`), noted only
because it is a rule-3-shaped deletion at a call site.

---

## Nits

* **N23** — `capture_laid_dimensions`'s docstring
  (`non_linear_program.h:1244-1259`) still says only that it writes dimensions;
  it also calls `freeze_laid_thread_modes()` (`non_linear_program.cpp:316`).
  This is the ledger's registered **M2**, routed to "the header-comment cleanup
  pass / Task 9" rather than named in this grant, so out of scope — flagged for
  continuity only.
* **N24** — the provenance banner added to
  `tests/sqp/support/claim_stream_double.h:1-4` is byte-correct against every
  "New file in hven" sibling (`aggregate_declaration.h`, `claim_space.h`,
  `nlp_aggregate.h`, …). Note that no other file in `tests/sqp/support/` carries
  one (`hs_problems.h` opens at `#pragma once`), so the directory is now mixed.
* **N25** — `include/hven/detail/drivers/solver_init.h` still opens at
  `#pragma once` with no provenance banner, unlike every sibling. The report
  flags it; recording it here so it is not lost when the report is closed.

---

## Checked and found correct (no finding)

Recorded so a re-review does not redo them.

* **Invalidate-first.** `invalidate_laid_state()` is the first statement of both
  `make_nlp` (`non_linear_program.cpp:36`) and `rebuild_structures` (`:226`);
  the header block at `:1313` and both .cpp blocks state it truthfully
  (their *duplication* is B2, not their accuracy).
* **`require_master_lists_unmoved()` on every read.** Called by `declaration()`
  (`non_linear_program.h:971`) and `model_structure_key()` (`:1022`) — the two
  public readers — before any materialization, exactly as `:1298` claims;
  the "size change and nothing finer" and "never-laid is not checked" caveats
  both match `non_linear_program.cpp:385-400`.
* **The claim-block check.** Disjointness + bounds + partition, no order
  predicate, empty blocks sit anywhere: `aggregate_eval_seam.cpp:325-397`. Every
  in-scope comment about it is true; only the test double (B3) is not.
* **S6 split.** `claim_stream_source.h:37-42` carries the per-domain contiguity
  clause as AUTHORITY; `nlp_aggregate.h:333-339` carries only serial issue /
  partition-index order and points at the interface for contiguity. Correct per
  the ledger's amendment.
* **Fixing rows.** The lift-before-`make_nlp` / re-append-after chain, the
  one-single-row-piece-per-fixing-row contract checked per piece in
  `validate()` (`aggregate_declaration.cpp:138-165`), the trusted-count
  paragraph, and the `fixed_variable_indices()` "UNCHANGED" wording (M1) are all
  accurate as written.
* **Thread-mode freeze.** Frozen on masters *and* partition copies after
  partitioning (`non_linear_program.cpp:252-268`), thawed only on the
  declaration copies (`:374-382`), post-lay `set_thread_mode` throws
  (`include/hven/detail/interior/solver_function_base.h:88-93`). Header text
  matches.
* **`kMinKktElementsPerPartition`.** The "empirically chosen; re-evaluate with
  the bench harness" callout was dropped from the constant
  (`non_linear_program.h:97-100`) but survives verbatim at the cap site
  (`non_linear_program.cpp:78-83`) — correct application of "the why lives once
  in the .cpp".
* **Cross-references that resolve.** `validate_bound_destination` in
  `nlp_aggregate.h:286` (referenced from `non_linear_program.h`'s
  `bound_kkt_destination`); `nlp_require_claimed_pattern` in
  `detail/model/nlp_adapter.h`; `claim_stream_digest` /
  `materialized_bound_digest` builders. Only I10 and I11 dangle.
* **`nlp_solver.h:31`** — "Every entry point throws `std::invalid_argument` when
  the initial guess's size does not match the transcribed problem's variable
  count" is true: all ten entries funnel through `NLPSolver::run`, which
  size-checks against `core_->n_`. Likewise the member docstrings ("null before
  one", "`jet_release()` restores it") and the constructor's `@throws`.
* **Ledger citations.** All `docs/notes/2026-08-m4-ledger.md:NNN` and
  `2026-08-21-m4-task5-design.md §N` citations are gone from the swept files, and
  each is listed in the report's rationale-candidate section. Correct.
