# Chunk 2 report — comment sweep: include/hven/detail/interior (incl. typedefs/, utils/)

Branch `sweep/base` at `3d37369`. Scope: every `.h` under
`include/hven/detail/interior/` and its `typedefs/` and `utils/` subdirectories —
31 files (the brief's "~7.7k lines" estimate; actual total 4886). No builds, no
tests. Gate: comment-stripped diff of every touched file vs HEAD is EMPTY.

## Files touched (19)

| file | change summary |
|------|----------------|
| barrier_math.h            | dangling pass-0 history-bullet fragment + ruler removed; extraction-history sentences dropped; kernel contracts kept, converted to /// docstrings; ULP/damping/mu-form-vs-z-form callouts kept |
| bound_set.h               | stray ruler removed; `<summary>` wrappers -> @brief style |
| constraint_function.h     | `/* */` blocks -> /// @brief docstrings |
| eval_error_log.h          | extraction-history paragraph dropped; struct block reshaped to /// |
| fixed_variable_row.h      | VectorFunction-expression history condensed to a descriptor; rulers removed; `<summary>` -> @brief |
| flat_map.h                | duplicate top-of-file paragraph (class docstring already says it) + ruler removed |
| indexing_data.h           | `<summary>` wrappers -> @brief; wording normalized in rewritten comments only |
| iterate_info.h            | member blocks -> /// docstrings; version label dropped; norm-convention callout kept |
| jet.h                     | rulers removed; map() overloads gained @brief docstrings |
| kkt_factorization.h       | top WHY-blocks condensed to descriptors keeping the projection-fidelity contract |
| kkt_vector.h              | dangling pass-0 fragment + rulers removed; class block reshaped to /// |
| objective_function.h      | `/* */` blocks -> /// @brief docstrings |
| parsed_io_flags.h         | relocation history dropped (enum docstring already carries the semantics) |
| sizing_specs.h            | rulers + ASSET scaffolding comments removed; spec descriptor kept as @brief |
| super_scalar_traits.h     | trailing ruler removed (all Eigen-5 contract notes kept verbatim) |
| thread_pool.h             | banner frames/rulers stripped; every rationale/measured-decision note kept verbatim |
| threading_flags.h         | relocation history dropped; ownership one-liner kept |
| tuple_iterator.h          | ruler removed |
| type_storage.h            | "replaces rubber_types" history dropped; clone_into convention retained |

## Files touched (comment lines before -> after; file lines before -> after)

Added at the fix round (finding A-13); counts regenerated programmatically
(`3d37369` -> current HEAD of this fix round) with the same line-based
convention as chunk 1's fix-round table (`.sweep/chunk-1-report.md`): a line
counts as a comment line if, after stripping leading whitespace, it opens
with `//` or falls inside a `/* ... */` span (`///` docstring lines count).
This is an independent count, not a copy of chunk 1's; a small
counting-convention disagreement with a by-hand tally is expected (see
`.sweep/chunk-1-review.md` nit B-b) and is not a factual error.

| file | comments | total lines |
|------|----------|-------------|
| barrier_math.h            | 122 ->  86 | 280 -> 243 |
| bound_set.h               |  80 ->  74 | 125 -> 119 |
| constraint_function.h     |  40 ->  25 | 127 -> 112 |
| eval_error_log.h          |  16 ->  10 |  40 ->  33 |
| fixed_variable_row.h      |  81 ->  64 | 241 -> 224 |
| flat_map.h                |  39 ->  31 | 115 -> 106 |
| indexing_data.h           |  98 ->  82 | 303 -> 287 |
| iterate_info.h            |  75 ->  44 | 121 ->  90 |
| jet.h                     |  26 ->  33 | 276 -> 283 |
| kkt_factorization.h       |  93 ->  79 | 163 -> 149 |
| kkt_vector.h              |  52 ->  38 | 107 ->  93 |
| objective_function.h      |  29 ->  20 |  86 ->  77 |
| parsed_io_flags.h         |  17 ->   9 |  31 ->  22 |
| sizing_specs.h            |  13 ->  12 |  58 ->  57 |
| super_scalar_traits.h     |  64 ->  63 | 200 -> 199 |
| thread_pool.h             | 288 -> 281 | 805 -> 798 |
| threading_flags.h         |  20 ->  15 |  63 ->  58 |
| tuple_iterator.h          |  66 ->  65 | 219 -> 217 |
| type_storage.h            |  70 ->  64 | 201 -> 195 |
| **total**                 | **1289 -> 1095** | **3561 -> 3362** |

(`jet.h`'s comment count rises: the A-12 fix round correction and the
original @brief expand the map() docstring by a net two lines.)

## Files in scope, already compliant (12 — untouched)

aggregate_views.h, accelerate_threads.h, eigen_types.h, function_return_type.h,
get_core_count.h, math_functions.h, memory_management.h, sizing_helpers.h,
solver_function_base.h, std_extensions.h, timer.h, type_name.h.
(type_name.h keeps its two-line external-source citation — attribution.)

## Rationale candidates for docs/notes

- barrier_math.h:9-11 (pre-sweep) — pass-0 leftover bullet fragment ("had left
  one verbatim copy per component / Added the primal variable-bound barrier
  kernels") — pure change-history, no content lost.
- kkt_vector.h:9-10 (pre-sweep) — same class of pass-0 leftover ("was a private
  nested class that the globalization components could not name").
- fixed_variable_row.h:7-15 (pre-sweep) — why the row is solver-side rather
  than a VectorFunction expression (dependency-weight argument).
- eval_error_log.h:4-7 (pre-sweep, `3d37369`) — why the log got its own header
  (eight globalization consumers reach it through interior_point_solver.h).
- threading_flags.h / parsed_io_flags.h relocation histories (vf re-export
  mechanism).

## Looked like a defect, reported not fixed

None found in this chunk.

## Docstrings written under uncertainty (reviewer first)

- jet.h — the three map() overloads' new @brief lines summarize behavior read
  from the bodies (thread-pool vs sequential path, per-job BLAS pin); verify
  wording matches intent.
- iterate_info.h — prox_reg_* field note condensed; sentinel semantics (-1 =
  mode off) restated from the original without re-derivation.

## Fix round (per .sweep/chunk-2-fix-brief.md; review at .sweep/chunk-2-review.md)

Section A (`include/hven/detail/interior`) and Section B residuals
(`include/hven/detail/globalization`) both covered; Section B's own table
extends `.sweep/chunk-1-report.md` directly (see that file) rather than being
duplicated here.

### Section A — interior

| finding | disposition |
|---------|-------------|
| A-1 (blocking) barrier_math.h duplicate bound-kernel paragraph | APPLIED — :107-115 (the dangling second copy) deleted outright; :97-106 is now the sole @brief block |
| A-2 (blocking) iterate_info.h norm-selection actor | APPLIED — re-verified against `SocRecovery::on_step_rejected` (`interior_point_solver_globalization.cpp:1392-1394`) and `soc_should_trigger` (`globalization/soc.h:49-56`); wording now names the caller, not `soc_should_trigger()`, as the selector |
| A-3 (blocking) constraint_function.h / objective_function.h invented per-row locks | APPLIED — restored the pre-image's neutral wording ("passing through the solver's arguments and the indexing data") on both flagged sites; re-verified the lock is per clash COLUMN and conditional on `!unique_constraints_ && VarClashes[active] != -1` at `fixed_variable_row.h:172-178` |
| A-4 apply_reset_slacks docstring | APPLIED — corrected to match the loop: the clamp lands in a local `si` used only for the sum; `S[i]` itself is untouched on that branch |
| A-5 barrier_math.h group prose sharing a block with @brief | APPLIED — :90-95 demoted to `//` file-section prose (folded into the same edit as A-1) |
| A-6 barrier_math.h anonymized Ipopt attribution | APPLIED — "The reference implementation" restored to "Ipopt" at the mu-form-gradient docstring |
| A-7 kkt_factorization.h floating file-narration promoted to @brief | APPLIED — demoted the whole block back to `//` |
| A-8 indexing_data.h orphaned `</summary>` | APPLIED — line deleted |
| A-9 thread_pool.h banner strip incomplete | APPLIED — all three remaining ruler pairs stripped; the `:320-326` duplicate-of-@brief body deleted, EXCEPT the cache-line/false-sharing sentence, which the class @brief does NOT restate (verified: no other mention of `m_tasks_pending` padding in the file) — kept as a one-line `//` note on the `m_tasks_pending` declaration instead of being deleted; review's "delete the body as redundant" is not fully accurate for that one clause |
| A-10 iterate_info.h grouped member blocks mis-attach | APPLIED — `prox_reg_primal_`/`prox_reg_dual_` and `accepted_`/`first_rejection_iter_`/`theta_at_first_rejection_` groups: shared narrative demoted to `//`, each member given its own short `///<` |
| A-11 parsed_io_flags.h lost consumer sentence | APPLIED — restored (folded into the enum's `@brief`); verified the tagging site (`indexing_data.h:113-118,246-263`, `v_index_continuity_`/`c_index_continuity_` populated via `check_continuity`) — the consuming "gather routines" are outside this repo (tycho-side, per the hven/tycho boundary), so I restored the sentence's content without re-verifying that half beyond what the deleted pre-image already asserted |
| A-12 jet.h BLAS-pin lifetime claim | APPLIED — re-verified: MKL path is scope-guarded (`MklLocalPinGuard`), Accelerate path is set-and-leak healed at solve entry (`accelerate_set_num_threads(1)` at jet.h:166, no restore, healed by `interior_point_solver`'s `qp_threads_` re-apply) — docstring now states both, drops "for its duration" |
| A-13 report missing before/after comment-line counts | APPLIED — table added above |
| A-a (nit) report rationale line-range off | APPLIED — `eval_error_log.h:9-13` corrected to `eval_error_log.h:4-7` at `3d37369` |
| A-b (nit) jet.h missing @return/@throws, identity overload's brief conditional | NOT APPLIED — not a one-line change (adds new @return/@throws content across two docstrings and a qualifying clause); left for a follow-up docstring pass |
| A-c (nit) iterate_info.h vestigial "so adding them" antecedent | APPLIED — resolved as part of the A-10 edit ("Not printed" reworded to "not printed (the iteration table formats an explicit field list)") |
| A-d (nit) sizing_specs.h:47 trailing space | NOT APPLIED — same class as chunk-1 finding 11 (trailing whitespace), which that review explicitly deferred to a follow-up formatting pass; kept consistent with that precedent |
| A-e (nit) type_storage.h implementer sentence allegedly lost | NOT APPLIED — REVIEW ERROR: re-verified against `3d37369` and current HEAD, `s.emplace<Model>(data_)` is present at the class docstring (:26-27) in BOTH revisions; the implementer sentence was never lost, only its (redundant) top-of-file duplicate was removed. No action needed. |
| A-f (nit) asymmetric scatter/lock embellishment across 4 sibling methods | APPLIED — resolved as a side effect of A-3: reverting the two flagged methods to the pre-image's neutral wording makes all four sibling methods (both files) textually consistent again |

### Section B — globalization residuals

See the extended fix-round table in `.sweep/chunk-1-report.md` for B-1, B-2,
B-3 (report-row correction). Nits:

| finding | disposition |
|---------|-------------|
| B-a (nit) sqp/globalization.h "recorded at the constants below" | APPLIED — reworded to "recorded in the deviations note below" |
| B-b (nit) chunk-1-report.md comments-column convention note | APPLIED — caption note added to that table (see `.sweep/chunk-1-report.md`) |
| B-c (nit) surviving rulers in monitored_governor.h / restoration.h | NOT APPLIED — review itself notes this is carry-forward from the original chunk-1 sweep, not a fix-round regression; multi-line (2 rulers x 2 files), deferred to a follow-up formatting pass |
| B-d (nit) cosmetic reflow artifacts (sqp/elastic.h orphan line, monitored_governor.h `///<` indent) | NOT APPLIED — not one-line changes (a paragraph reflow and a multi-line indent fix); review itself flags these as pure cosmetics costing nothing against the gate, deferred to a follow-up formatting pass |

Section A: 3/3 blocking applied, 10/10 should-fix applied, 3/6 nits applied
(A-a, A-c, A-f) and 3/6 not applied (A-b, A-d, A-e — A-e is a review error,
no fix was needed). Section B residuals: 2/2 blocking applied (B-1, B-2),
1/1 should-fix applied (B-3, disposition recorded in `.sweep/chunk-1-report.md`),
2/4 nits applied (B-a, B-b) and 2/4 not applied (B-c, B-d).

Totals across both sections: 26 findings, 21 applied, 5 not applied.

Gate re-run after this fix round: comment-stripped diff of every touched file
(`include/hven/detail/interior/**`, `include/hven/detail/globalization/**`)
vs the fix round's parent commit is EMPTY.
