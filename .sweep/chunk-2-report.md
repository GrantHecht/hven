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
- eval_error_log.h:9-13 (pre-sweep) — why the log got its own header (eight
  globalization consumers reach it through interior_point_solver.h).
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
