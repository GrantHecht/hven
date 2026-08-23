# Chunk 7 report — model + drivers (branch sweep/model-drivers, off m4 @ d73779e)

Scope: every `.h`/`.cpp` under `include/hven/model/`, `include/hven/detail/model/`,
`include/hven/detail/drivers/`, `src/model/`, `src/drivers/`, plus `src/CMakeLists.txt`.
Rules applied per `.sweep/comment-rules.md` and the chunk-7 rubric: a header Doxygen block is
a constraint on the caller; the why lives once in the .cpp at the site or not at all; labels,
history, ledger/task citations, rulers, and commented-out-style narration removed; `@throws`
kept per-case; cost/counter statements kept per-case.

Gate: comment-stripped diff of before vs after is EMPTY across all 25 changed files
(verified programmatically; string/char literals respected). Provenance headers untouched.
No builds, no tests. One commit.

## Files touched (comment-line counts, comment-only lines)

| File | Before | After |
|---|---|---|
| include/hven/model/non_linear_program.h | 829 | 733 |
| src/CMakeLists.txt | 488 | 275 |
| include/hven/model/nlp_aggregate.h | 550 | 460 |
| include/hven/model/structure_identity.h | 164 | 110 |
| include/hven/model/nlp_model.h | 227 | 165 |
| src/drivers/sqp_driver.cpp | 1528 | 1443 |
| include/hven/model/candidate_point.h | 350 | 330 |
| include/hven/model/aggregate_declaration.h | 133 | 102 |
| include/hven/model/claim_space.h | 131 | 102 |
| include/hven/model/claim_stream_source.h | 80 | 60 |
| src/drivers/interior_point_solver_globalization.cpp | 894 | 851 |
| src/model/non_linear_program.cpp | 628 | 609 |
| include/hven/detail/drivers/interior_point_solver_fwd.h | 105 | 95 |
| include/hven/detail/drivers/solver_init.h | 9 | 9 |
| include/hven/detail/drivers/aggregate_eval_seam.h | 190 | 187 |
| include/hven/detail/model/nlp_adapter.h | 176 | 176 |
| include/hven/model/nlp_model_aggregate.h | 236 | 232 |
| include/hven/model/nlp_problem.h | 64 | 66 |
| include/hven/model/nlp_problem_model.h | 188 | 188 |
| include/hven/model/nlp_solver.h | 31 | 34 |
| src/drivers/interior_point_solver.cpp | 1666 | 1652 |
| src/drivers/interior_point_solver_settings.cpp | 113 | 105 |
| src/drivers/sqp_options.cpp | 82 | 80 |
| src/drivers/sqp_print.cpp | 26 | 22 |
| src/drivers/solver_init.cpp | 0 | 0 |
| **Total** | **8888** | **8086 (−802)** |

(+ lines on nlp_problem.h / nlp_solver.h are docstrings ADDED to public declarations that
had none, per rule 1.)

## The four narration blocks and what each condensed to

1. **Declaration-adoption entry** — `include/hven/model/nlp_model_aggregate.h`
   (`NLPModelAggregate` ctor block): dropped `History: commit 9579e08;
   docs/notes/2026-08-m4-ledger.md:790`; kept the const-handle widening rationale
   (overload pair would be ambiguous for `shared_ptr<Derived>`) and the aliasing-constructor
   use. Same treatment for the adoption contract on `NonLinearProgram::adopt_declaration`
   (already contract-form; unchanged) and `AggregateDeclaration::validate()`, whose header
   OWNERSHIP-SPLIT essay was dropped (the .cpp carries the why at its sites) while every
   refusal case, the vacuous-empty-lists rule, and the fully-unbounded-record carve-out were
   kept verbatim in meaning.

2. **Lay markers** — `include/hven/model/structure_identity.h`: StructureEpoch /
   StructureEpochCounter narration condensed to guarantees (default 0 = nothing laid,
   first layout 1; equality-only; release/acquire pairing; provider bumps inside the re-lay;
   wrap after 2^64 rests on unreachability) and override obligations; dropped the naming
   history ("why it is spelled model_structure_key"), the "six hundred years" arithmetic
   color, and enforcement rhetoric.

3. **Claim-block check** — `include/hven/model/claim_space.h`: KktLocationTable /
   RhsLocationTable constructor essays on sentinel discipline condensed to the rules
   (-1 is the only legal negative and why; every published mark is either that sentinel or
   an in-range index), plus the unchecked-accessor/laid-width split; dropped the weaker-vs-
   stronger protocol comparison essay and the one-type-because-shared-keying rationale.

4. **Aggregate contract** — `include/hven/model/nlp_aggregate.h` class-level contract:
   concurrency posture (all three overlap cases), partition invariance, claim-order
   determinism, claim exclusivity, claim-pass shape, epoch what-bumps/ordering/failure-
   restore rules, the five assemble validators, accumulation-not-assignment with the -0.0
   seed caveat, throw-prefix semantics, solver-coefficient transfer, scorer obligations —
   all kept per-case. Dropped: both `docs/notes/2026-08-m4-ledger.md` citations, "stated so
   nobody repairs it" persuasion, and redundant restatements.

## CMakeLists.txt handling

The "M3 PHASE-C T1…T8 ADDENDUM" blocks (173 comment lines, PCH measurement provenance for
the SQP-side carves) moved VERBATIM into new `docs/notes/2026-08-m3-phase-c-tu-carve-addenda.md`
under one heading; the only transformation is removal of the leading CMake `# ` comment
markers. A two-line pointer comment remains at the old location. Other CMake comments:
labels stripped (M2/T1–T9/F1/S-1/S-3/U0 references), facts kept (PCH membership bars and the
measured opt-in table stay in place; fmtlib non-compilation reason; tier-order rationale for
enum_names.cpp placement). Origin-project narration (psiopt migration record) removed from
the interior-point block per the sweep's history rule.

## Brief amendment folded in (tests/sqp/support/claim_stream_double.h)

The controller's mid-chunk brief addition was honored in this same commit: the file received
the standard hven provenance banner ("New file in hven", two content lines between ruler
rules) and the same comment sweep — two `// ---- section ----` rulers replaced with a plain
descriptor; its existing why-at-the-site comments (why the double exists, why it includes the
piece definitions itself) are contract/why for a test double and were kept. Comment lines: 30 → 29.

## Rationale candidates for docs/notes

- src/drivers/sqp_driver.cpp (old file banner, was lines 4–53): object-file relocation
  evidence (at 718eef4) that solve_impl's fourteen per-element call sites were already
  out-of-line at -O3 (eval_nlp 3/3, eval_nlp_values 3/3, evaluate_kkt 3/3, build_subproblem
  2/2, predicted_decrease 2/2, crash_basis_seed 1/1); S3 ruling separating TR policy
  constants from TR update functions; P-BENCH/P-CENSUS scope of the bit-identity proof.
- src/drivers/sqp_driver.cpp (~1800, SSN escape charge): measured justification 131 SSN iters
  vs 128 factorizations on RestorationStaysOnTheWalkUnderKSsn before the charge existed
  (invariant ssn_iters <= factorizations).
- src/drivers/sqp_options.cpp (~20): where the disassembly pin's evidence excerpt and exact
  flags line are recorded (commit + task report pointer removed with the label).
- include/hven/model/structure_identity.h (~40 region): "stream primitives internal by
  design" — single public path per conjunct; the two first-cut defects (missing dimension
  preamble keys different problems identically; hashing declared records instead of their
  intersection keys different layouts identically).
- include/hven/model/structure_identity.h ModelStructureKey: deliberate redundancy of the
  explicit partition conjunct (inspectable in diagnostics where order-sensitivity is not).
- include/hven/model/claim_stream_source.h (file block): "why it is its own interface"
  (base asks-for-a-fill vs this publishes-where-it-lands) and "what it is not" (no second
  contract).
- include/hven/model/nlp_problem.h (bounds note): concrete migration example trimmed — an
  equality row declared ±1e20 on both sides becomes an equality at 1e20 rather than a
  dropped free row; rewrite such declarations to use infinities.
- include/hven/model/nlp_model.h (pattern-invariance block): the named dependents of the
  wider clause (warm-start ingest/emission probes built at zero multipliers; restoration
  calling eval_hess with obj_scale = 0) and the HS40 worked example pointer
  (tests/sqp/support/hs_problems.h detail::make_jac).
- include/hven/model/aggregate_declaration.h validate(): ownership-split framing ("one rule
  at two boundaries") beyond the carve-out sentence kept in the header.
- include/hven/model/nlp_aggregate.h assemble(): original ledger citation for the -0.0-seed
  byte-identity mechanism (docs/notes/2026-08-m4-ledger.md:760-763); header now points at
  detail/drivers/aggregate_eval_seam.h only.

## Defects noticed (reported, not fixed)

- `NonLinearProgram::print_data()` (include/hven/model/non_linear_program.h:552,
  defined src/model/non_linear_program.cpp) prints via `std::cout` directly — library code
  printing diagnostics not folded into any exception; looks debug-only but is public API.
- `NonLinearProgram::nlp_test()` (include/hven/model/non_linear_program.h:~983) is a static
  test helper on the production class with no docstring and no test-only gating; purpose not
  verifiable from this chunk alone, so no docstring was invented for it.
- `include/hven/detail/drivers/solver_init.h` has no provenance banner (starts at
  `#pragma once`) unlike every sibling file — possibly a pass-0 omission.
- `docs/notes/data/2026-08-22-o12-mac-session/` exists untracked in the worktree; not part
  of this chunk and NOT staged.

## Docstrings written where behaviour was inferred (reviewer checks)

- `nlp_solver.h`: member docstrings ("null before one" for model_/core_; do_transcription_
  reset by jet_release) and the class-level "@throws invalid_argument on x0 size mismatch" —
  derived from src/model/nlp_solver.cpp (transcribe/run/jet_release verified).
- `non_linear_program.h`: group comment over the solver-coefficient accessor block
  ("views into the solver-coefficient block / rhs arena") — read off the segment math.
- `nlp_problem.h`: per-method @briefs reshaped from the former section-marker comments
  (facts carried over, including "must be 0 when num_cons() == 0", lower-triangle, purity,
  duplicate summation, 0-based).

Counts: unchanged

---

# Completion round (review response to `.sweep/chunk-7-review.md`)

Second commit on this branch, over 0ec0553. Comment-only; no CMake, no markdown
outside `.sweep/`. Gate re-run: `strip_compare.py . 0ec0553 HEAD` — 0 violations.

## Findings → disposition

| Finding | Disposition |
|---|---|
| **B1** — eight in-scope files never opened | Swept. Per-file before/after below; three examined and found already compliant. |
| **B2** — registered M5 not folded | Folded. The five real header/`.cpp` pairs in `non_linear_program.h`/`.cpp` are deduplicated; see the M5 table below. |
| **B3** — `claim_stream_double.h:10` order obligation | "or arrive out of order" deleted; the sentence now reads disjointness + bounds only, matching `aggregate_eval_seam.cpp:330-334`. Paragraph reflowed. |
| **B4** — `interior_point_solver.cpp:813` false present tense | Past tense restored ("that `set_nlp()` used to construct"), and the real actor named: "every `run_phase_sequence()` entry rather than every (re)transcription". Ragged lines in the same block reflowed. |
| **I5** — merged 147-column line at `sqp_driver.cpp:1441` | Split into two lines. No line in the tree exceeds 100 columns. |
| **I6** — `nlp_solver.h:52` over-claims atomicity | Replaced with the retriable contract: `do_transcription_` stays true and the next solve re-transcribes. |
| **I7** — `nlp_solver.h:68` `@throws` names the wrong condition | Restated as "if the problem has not been transcribed, or if the solver's multiplier block is shorter than the transcribed row count", read off `nlp_solver.cpp:143` and `model_multiplier_block`. |
| **I8** — `adopt_declaration`'s refusal contract | "EVERY REFUSAL IS THE DECLARATION'S OWN" → "EVERY REFUSAL IS MADE BEFORE ANYTHING MOVES"; a second `@throws` added for the entry's own three refusals (piece-less declaration with rows, a fixing row naming other than one constraint row, a fixing row outside the band). The `validate()` list also gained the checked-narrowing case. |
| **I9** — `nlp_problem.h` migration remedy deleted | Both sentences restored in the header (the ±1e20 equality example and "Rewrite such declarations to use the infinities"). |
| **I10** — six stale `src/solvers/` references | All six repointed at `src/drivers/` or rewritten without the provenance clause. The three "moved VERBATIM from …" file blocks in `interior_point_solver_globalization.cpp` are now present-tense descriptions; the statement/operand-order constraint survives as a live caveat, stated once in the file block and cross-referenced from the other two. |
| **I11** — dangling pointer at `sqp_driver.cpp:222` | "and for why the check moved rather than changed" dropped; paragraph reflowed. |
| **M12** — `structure_identity.h` deliberate-redundancy callout | Restored as one sentence on `ModelStructureKey`. |
| **M13** — `claim_space.h:266` deliberate negation | The "though NOT for the same reason — the reason here is the weaker protocol, not the stronger one" denial restored. |
| **M14** — `non_linear_program.h:387` framing | "records no bound for the variable (it goes to the solver free)" restored to the MakeConstraint half. |
| **M15** — `nlp_aggregate.h:355` lifetime rule | "and a reference to anything temporary would dangle" restored to `declaration()`. |
| **M16** — `get_mat_space` navigational pointers | Both restored. The scatter sites are named as `DenseFunctionBase::kkt_fill_all` / `::kkt_fill_hess` rather than by the review's `dense_function_base.h` path — that header is the consumer project's, not hven's, and `indexing_data.h:61` already uses the type-qualified spelling. |
| **M17** — `validate()` `@throws` not exhaustive | Both checked-narrowing refusals added (piece row sum past INT_MAX, equality piece count past INT_MAX). |
| **M18** — residual labels | `T6` removed at all nine sites (plus one in `interior_point_solver_print.cpp` the review did not list), each replaced by the plain statement it prefixed. `sqp_driver.cpp:210` "used to be certified" → "would be certified". **`B-1` deliberately left** — see the deviation note below. |
| **M19** — `nlp_model.h` override pointer | Restored; `tests/sqp/support/scale_problems.h`'s `F7CollocationChain` verified present. |
| **M20** — malformed `@throws Whatever` | Now `@throws std::invalid_argument whatever materialize_variable_bounds throws …`. |
| **M21** — ragged reflow in `sqp_driver.cpp` | Reflow pass over the 53 paragraphs carrying an orphan half-line (first line under 62 columns with a continuation that fits). Three `X::` / `y` hard breaks that the join would have turned into `X:: y` were rejoined by hand. Paragraphs with bullets, tables or indented content were excluded. |
| **M22** — `solver_init.cpp:29` return-contract comment | No action: the review records no information lost, and the header carries the fact under `@return`. |
| **N23** — `capture_laid_dimensions` also freezes thread modes | Folded (one clause on the `@brief`), even though the ledger routes M2 elsewhere. Flagged here so the M2 owner does not re-do it. |
| **N24** — `tests/sqp/support/` now mixed on banners | Recorded, no action: provenance headers are pass 0's. |
| **N25** — `detail/drivers/solver_init.h` has no banner | Recorded, no action, and **widened**: `src/drivers/solver_init.cpp` has none either. The pair was added by the psiopt migration commit (`b36d24a`), so the correct banner text is a pass-0 determination, not this sweep's. |

## Deviation: `B-1` was not renamed (M18, second bullet)

The review's own note ("a rename wants tree-wide coordination") is the reason.
`B-1` is not an unresolvable label in this tree: it is introduced by name at
`include/hven/drivers/sqp_driver.h:338` ("THE DEFECT THIS REPAIRS (finding
B-1, …)"), carried by `include/hven/detail/warmstart/warm_start.h`,
`include/hven/drivers/sqp_types.h` and `include/hven/core/solver_counters.h`, and
is the subject of a whole test file, `tests/sqp/test_b1_gate.cpp` (39 uses).
Renaming the five occurrences in `sqp_driver.cpp` alone would sever the link to
all of that. It belongs in a single tree-wide pass; the surrounding
"Phase-5 Task 7b" labels, which are the part this chunk owns, are already gone.

## B1 — the eight skipped files (comment-only lines)

| File | Before | After | Action |
|---|---|---|---|
| src/drivers/aggregate_eval_seam.cpp | 205 | 192 | Three section rulers dropped; the epoch commit-point, two-claims-on-one-coordinate and by-hand-sort blocks condensed; a paragraph break restored in the wide-arithmetic block. Every claim-block-scan statement left as written (the review verified them true). |
| src/model/nlp_model_aggregate.cpp | 145 | 132 | Three section rulers dropped; `scatter_matrix`'s positional-pairing essay condensed; a paragraph break restored in `evaluate_jacobians`. |
| include/hven/detail/drivers/interior_point_solver_presets.h | 107 | 93 | Campaign narration, evidence cell hashes and two dangling cross-references removed (see defects below); each preset keeps its mechanism description and its measured cost/tail figures, now attributed to the consumer project's example suite. |
| src/model/aggregate_declaration.cpp | 65 | 64 | `declared_rows` given `@param`/`@throws`; the boundary-universality and refuse-here-instead essays condensed; the equal-non-finite-bounds carve-out kept as a deliberate-deviation callout. |
| src/model/nlp_problem_model.cpp | 44 | 44 | Examined; already compliant (docstrings, index descriptors, one numerical guard). No change needed. |
| src/model/nlp_adapter.cpp | 42 | 42 | Examined; already compliant. No change needed. |
| src/model/nlp_solver.cpp | 32 | 32 | Examined; already compliant — and it is the site the corrected `nlp_solver.h` docstrings (I6/I7) were read off. No change needed. |
| src/drivers/interior_point_solver_print.cpp | 26 | 24 | `T6` and an `InteriorPointSolver 2.4` label dropped; two history clauses ("is untouched and still", "matches today's output exactly") made present-tense. Provenance banner untouched. |

## B2 — the registered M5, pair by pair

Header keeps the caller-facing constraint; the `.cpp` keeps the why once.

| Pair | Header after | `.cpp` after |
|---|---|---|
| `invalidate_laid_state` (h:1313 ↔ cpp:27, cpp:221) | Runs at the top of `make_nlp()` and `rebuild_structures()`, before any master list is touched and before any eager scalar is written; between it and the epoch bump the piece lists are EMPTY. Idempotent. | The two-failure-mode argument moved to the **definition** (`invalidate_laid_state`), stated once. `make_nlp`'s call site keeps only what is site-specific (which statements below can throw) and points at the definition; `rebuild_structures`'s call site keeps the idempotency cost and repoints likewise. |
| `require_master_lists_unmoved` (h:1292/1298 ↔ cpp:393) | The rule ("mutating the master lists after a lay, without re-laying, is a contract violation"), the every-read guarantee, the size-change-and-nothing-finer caveat and the never-laid carve-out. | The why — the lists are public members read after the lay, and the claim arrays / digest / row counts all describe them AS LAID — now stated only here. |
| `freeze_laid_thread_modes` (h:1238 ↔ cpp:257, cpp:310) | One invariant line: every piece a laid layout holds carries the same marker, master and partition copy alike. | The after-the-partitioning argument stays at the definition; `capture_laid_dimensions`'s own freeze note is unchanged. |
| `capture_laid_dimensions` (h:1248 ↔ cpp:273) | Contract only: what it writes, that it freezes the thread modes with them, `@throws` on a piece list past INT_MAX, and the ordering obligation. | Unchanged; it already carried the "here is where AS LAID is decided" why at the site. |
| `materialize_declaration_pieces` (h:1273 ↔ cpp:374) | Idempotent; the lists are COPIES rather than views; what is deferred is WHEN the copy is taken. | Gains the one-sentence why the header gave up (a declaration is a value over its pieces, which lets a consumer move one in), beside the existing thaw note. |

## Additional cross-reference repairs (found by the post-edit grep)

- `src/drivers/sqp_driver.cpp` — `tests/test_sqp_driver.cpp`,
  `tests/test_sqp_restoration.cpp` and `tests/test_warm_start.cpp` all repointed
  at `tests/sqp/…`, where those files actually live. Same class as I10.
- `src/model/non_linear_program.cpp` — three surviving section rulers removed
  (the review did not list them; they are rule-stripped everywhere else).

## Rationale candidates for docs/notes (this round)

- `include/hven/detail/drivers/interior_point_solver_presets.h` — the
  globalization campaign's evidence of record for the four non-classic presets:
  the post-fixes evidence refresh document and the per-preset cell hashes
  (`62994231856d` filter_l1, `8417a47846c1` soc_recovery_l1, `8d8397c915b2`
  soc_proximal), plus merit_l1's matched-call finding (zermelo's wrong-basin
  guess converging at iteration 40 to objective 1.7009270229362865, the
  Ipopt-agreement reference).
- `src/drivers/interior_point_solver_globalization.cpp` — the component
  extraction's provenance: which helpers moved verbatim from the interior-point
  driver, the context-plumbing renames each took, `loqo_mu` dropping its unused
  `S`/`LI` parameters, and the bit-identical CBWR iteration-count merge gate the
  moves were held to.

## Defects noticed (reported, not fixed)

- `include/hven/detail/drivers/interior_point_solver_presets.h` carried **two
  cross-references that do not resolve in hven**: the evidence document
  `docs/dev/analysis/2026-07-e2-fixes-evidence-refresh.md` (it exists in the
  consumer project, not here) and the default-drift tripwire test
  `tests/cpp/solvers/test_interior_point_solver_presets.cpp` (no preset test
  exists anywhere in this tree — `apply_preset` and `kInteriorPointSolverPresets`
  are referenced only by the header, `interior_point_solver_settings.cpp` and
  `interior_point_solver.h`). The comments were rewritten to stop naming them,
  but **the default-drift tripwire itself appears to be missing from hven**, and
  `classic` being pinned as literals is exactly the invariant it guarded.
- `src/drivers/interior_point_solver_settings.cpp:735` and the presets header
  both claimed a Python binding docstring and a Python test pin the preset-name
  list. Neither exists in hven; the duplicate claim in the header was dropped and
  the one at the call site left, since it describes the consumer.
- `src/drivers/sqp_driver.cpp:1523` reads "instance-level border_ persistence".
  Pre-existing (not introduced by the reflow); `border_` names no member — the
  counter in this tree is `border_refine_steps`.
- `include/hven/detail/drivers/solver_init.h` **and**
  `src/drivers/solver_init.cpp` both lack a provenance banner (N25, widened).

Counts: unchanged
