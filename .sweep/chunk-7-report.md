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
