# Chunk 6 report — src implementation files

Commit applies `.sweep/comment-rules.md` to every `.cpp`/`.h` under `src/core/`,
`src/globalization/sqp/`, `src/interior/` (incl. `utils/`), `src/kkt/`,
`src/linear/`, `src/warmstart/`, plus `src/hven_pch.h`. `src/model/` and
`src/drivers/` untouched (not in scope). Boost notice (`get_core_count.cpp`)
and both Eigen-derived MPL/BSD notice blocks (`pardiso_session.cpp`,
`accelerate_session.cpp`) verbatim; all ASSET-derived provenance headers
verbatim.

Gate: a comment-stripped before/after diff over all 23 files is EMPTY — zero
code-token change, includes/macros/string literals byte-identical. No builds,
no tests, per the rules.

## Files touched (comment-line counts, before -> after)

| File | before | after |
|---|---|---|
| src/hven_pch.h | 62 | 53 |
| src/core/enum_names.cpp | 37 | 7 |
| src/core/ledger.cpp | 23 | 7 |
| src/core/pattern_hash.cpp | 9 | 7 |
| src/core/version.cpp | 7 | 7 |
| src/globalization/sqp/funnel.cpp | 113 | 48 |
| src/globalization/sqp/soc_elastic_restoration.cpp | 120 | 39 |
| src/interior/kkt_factorization.cpp | 78 | 65 |
| src/interior/utils/color_text.cpp | 15 | 14 |
| src/interior/utils/get_core_count.cpp | 26 | 21 |
| src/interior/utils/thread_pool.cpp | 34 | 31 |
| src/linear/accelerate_session.cpp | 113 | 91 |
| src/linear/pardiso_session.cpp | 220 | 208 |
| src/linear/symmetric_factor_accelerate.cpp | 271 | 236 |
| src/linear/symmetric_factor_mkl.cpp | 232 | 214 |
| src/warmstart/continuation.cpp | 186 | 114 |
| src/warmstart/warm_start.cpp | 99 | 35 |

Total: 1598 -> 1150 comment lines (the mechanical gate's count; the per-file
table above counts under a broader rule, most likely including licence and
provenance header lines the gate excludes -- the 448-line delta is identical
before and after, so this is a counting-rule difference, not a miscount).

## Files reviewed, deliberately unchanged

- `src/kkt/kkt_calls.cpp` (10 = 10) — both existing comments conform.
- `src/linear/dense_symmetric_factor.cpp` (61 = 61) — all comments conform.
- `src/interior/utils/color_text.h`, `fmtlib.h`, `fmtlib.cpp`,
  `memory_management.cpp` — provenance headers only.

## Rationale candidates for docs/notes (pre-sweep numbering @ 21c38cd)

- `src/core/enum_names.cpp`:8-33 — T3/T1 history: the core->drivers link-time
  layering inversion, why `test_core_layering.cpp` could not see it, and why
  the other three printers legitimately stay in `drivers/`.
- `src/core/ledger.cpp`:7-22 — phase-C T2: why the three reporting functions
  are out-of-lined while `record()`/accessors stay inline in the header.
- `src/globalization/sqp/funnel.cpp`:8-63 — T4 carve proof: vtable-only call
  sites for reset/judge, devirtualized-but-CALL site for
  resume_from_restoration, four fully-inlined members that stay put; FP
  boundary risk and the bit-identity falsifier protocol.
- `src/globalization/sqp/soc_elastic_restoration.cpp`:10-86 — T6: why one TU
  implements three headers (NlpEval lives only in sqp_driver.h; forced
  layering inversion), relocation-census methodology including the section-set
  rule for lambdas, RestorationModel key-function consolidation, and why
  `set_elastic_penalty` stays inline in elastic.h.
- `src/warmstart/warm_start.cpp`:8-66 — T7: `ip_activity_threshold` must stay
  a header template (Eigen-expression residuals avoid n-sized temporaries);
  object-file census; StartLevel resolution already moved by T5; bit-identity
  falsifier.
- `src/warmstart/continuation.cpp`:8-57 — T8: once-per-sweep entry point;
  validate's single-call-site count; the `record` closure's COMDAT operator()
  and the .text-undercount caveat for relocation censuses; bit-identity
  falsifier.
- `src/hven_pch.h`:10-19 — the specific measured PCH numbers (3.01 s parse
  floor, 3.38 s PCH build, ~1.5-2.0 s saved per participating TU,
  pattern_hash.cpp at 2.98 s). Informational timings tied to a specific tree
  state; docs/build.md carries the durable comparison, so only the invariant
  (byte-identity, ordering, clang-format-off, opt-in membership) stayed.
- `src/core/pattern_hash.cpp`:20-25 — the "one recipe, one implementation"
  framing and the note that the pinned digest's test reference was derived
  without calling `Fnv1a`.
- `src/linear/symmetric_factor_mkl.cpp`:597-599 (num_threads) — why this
  read-through helper lives in the adapter rather than the session header
  (contract logic vs backend fact); restates the §6 instrumentation
  preference.
- `src/linear/symmetric_factor_accelerate.cpp`:24-27 (header) — the two
  adapters deliberately kept in lock-step shape for reader navigation.
- `src/interior/kkt_factorization.cpp`:190-192 — "the honest answer is
  available at this boundary and deliberately not adopted is the same decision
  every other row of this projection makes" (cross-row consistency argument
  for the Accelerate zero-reset semantics).
- `src/linear/symmetric_factor_accelerate.cpp`:653-655 — provenance of the
  Accelerate refuses-not-perturbs observation (SQP engine's 2026-07-29
  real-hardware audit). Origin-naming sentence removed here;
  docs/testing.md is where that provenance belongs if not already recorded.

## Defects observed (reported, not fixed)

- None in code. Two comment-level slips were removed rather than fixed:
  `funnel.cpp` pre-sweep :135 had a grammatical fragment ("an iterate that
  infeasible anyway"), and `color_text.cpp` pre-sweep :34 misspelled
  "neccessary"; both lines fell entirely under the sweep rules.

## Comments rewritten where behaviour was re-derived (reviewer check first)

- `thread_pool.cpp` (set_num_threads stash block): ctor-reads-stash claim
  verified against `include/hven/detail/interior/utils/thread_pool.h`:486-495
  and `use_thread_pool()`/`pool_configuring()` at :612/:619.
- `kkt_factorization.cpp` (set_num_threads ordering note): "the factor
  validates first" verified against both backends'
  `SymmetricFactor::set_num_threads` (validate-before-mutate in
  `symmetric_factor_mkl.cpp` and `symmetric_factor_accelerate.cpp`).
- `soc_elastic_restoration.cpp` (z quarantine note): "SqpDriver never reads
  QpSolution::z -- reports the model-implied multiplier" verified against
  `sqp_driver.h`:552 (REPORTED BOUND MULTIPLIER note).
- `soc_elastic_restoration.cpp` (slack ceiling): is_runaway skip of bound-
  pinned slacks verified at `detail/qp/qp_engine.h`:2902-2907.
- `continuation.cpp`: x0-ignored-on-warm-resolve verified at `sqp_driver.h`
  :2909; probe_budget_stops driver-owned verified at `sqp_driver.h`:2967;
  predict() reads base parameters off the model verified at
  `detail/warmstart/predictor.h`:74.
- `get_core_count.cpp`: the Boost attribution was condensed to two lines
  naming boostorg/thread's pthread/thread.cpp; the full upstream URL was
  dropped as history — restore if provenance policy wants the literal URL.

## Fix round (`.sweep/chunk-6-review.md`, 2 blocking / 5 minor / 3 nits)

Comment-only corrections; counts unchanged (the strip-compare gate still
reports zero code-token change across all files).

| # | Severity | File:lines | Fix |
|---|---|---|---|
| B1 | blocking | `src/interior/kkt_factorization.cpp:179-182` | Reattributed the zeroing to this function (the write at :183-187), not "this backend"; removed the self-contradictory "reproduced here" framing. |
| B2 | blocking | `src/linear/accelerate_session.cpp:171-172` | Attributed k = 4 to Apple's own documented default and the other codebase's precedent, not an hven history ("it replaced") hven never had. |
| 3 | minor | `src/interior/kkt_factorization.cpp:22-24` | Reattributed the status categorization to hven's own mapping, not "its backend's established outcome semantics". |
| 4 | minor | `src/globalization/sqp/funnel.cpp:35-39` | Scoped the "every comparison is false" reasoning to NaN; noted infinities are also rejected by the guard, but for a different (well-defined-comparison) reason. |
| 5 | minor | `src/globalization/sqp/funnel.cpp:85-86` | Dropped the false "reordering would change the arithmetic" claim; restored "the arithmetic is the arithmetic the tests hand-derive" (IEEE addition is commutative; the cited test pins the role-swapped form bit-equal at kappa = 0.5). |
| 6 | minor | `src/warmstart/continuation.cpp:139-141` | Reworded so the invariant reads as holding generally (per `continuation.h`:357-359), with this path called out as one case, not the only one. |
| 7 | minor | `src/linear/symmetric_factor_mkl.cpp:137-139` | Pointed at what `symmetric_factor.h`'s doc comment actually carries (the citation and the enum-not-bool rationale), not a naming history it does not carry. |
| 8 | nit | `src/interior/utils/get_core_count.cpp:90-92` | `///` -> `//` for the floating Boost attribution block (attached to no declaration). |
| 9 | nit | `src/globalization/sqp/funnel.cpp:46` | "the class's THE FULL-STEP MODE note" -> "the class's FULL-STEP MODE note", matching the section title in `globalization.h:345`. |
| 10 | nit | `.sweep/chunk-6-report.md` (this file) | Corrected the file-table total to the mechanical gate's count (1598 -> 1150) and noted the 448-line delta is a counting-rule difference, not a miscount. |
