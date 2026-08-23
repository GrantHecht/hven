# Chunk 3 report — comment sweep: include/hven/detail/qp and include/hven/qp

Branch `sweep/base` at `bcc27eb`. Scope: all 6 headers (4 under
`include/hven/detail/qp/`, 1 more under `include/hven/qp/`; the brief's "~9.4k
lines" was accurate at 9696). No builds, no tests. Gate: comment-stripped diff
of every touched file vs HEAD is EMPTY (all 6 files touched).

## Files touched

| file | comments | total lines |
|------|----------|-------------|
| detail/qp/eqp_solve.h   |  143 ->  113 | 251 -> 221 |
| detail/qp/qp_engine.h   | 3652 -> 3606 | 5303 -> 5257 |
| detail/qp/qp_problem.h  |   47 ->   48 | 127 -> 128 |
| detail/qp/ssn_engine.h  | 2234 -> 2231 | 3652 -> 3649 |
| detail/qp/working_set.h |   15 ->   15 |  83 ->  83 |
| qp/qp_types.h           |  220 ->  129 | 280 -> 190 |
| **total**               | **6311 -> 6142** | **9696 -> 9528** |

Character of the chunk: these are derivation-dense engine headers whose comments
are overwhelmingly load-bearing contracts. The sweep therefore DELETED very
little text; what went was (a) task/phase/milestone/review-attribution labels
("Task 6b", "Phase-3 Task 2", "M3 phase B", "FIX ROUND n", "WAVE #n", "Fable
review", "docket D0", "Grant-ruled", "PR #5"), each removed only where the
sentence stayed true without it; (b) qp_types.h's two pure-history blocks (the
alias-retirement narrative and the Index-outlived-the-others record — the Apple
long/long long fact itself is retained inside the static_assert message, which
is the load-bearing half); (c) eqp_solve.h's measured-and-removed iterated-loop
paragraph and doc-note citations.

## Rationale candidates for docs/notes

- qp_types.h:18-47 (pre-sweep) — the full alias-retirement history: S1's
  width-pin premise false on Apple (`std::int64_t` = `long long` vs
  `std::ptrdiff_t` = `long`, CI run 31902660573), S2c's owner-ruled resolution
  redefining `hven::Index` onto `Eigen::Index`.
- qp_types.h:153-187 (pre-sweep) — max_iter default-change history (fixed 500 ->
  size-derived): F7 cold-grid failure counts (850–7341 minor demand), the
  identification-stall cost law citation, the recovery note.
- qp_engine.h:300 (pre-sweep) — reuse condition (e)'s provenance ("fix round 1,
  retargeted onto the factor identity").
- qp_engine.h:2092-2102, 2249-2258 (pre-sweep) — BorderState::generation's
  deletion history (what it covered, what replaced it).
- qp_engine.h:2137-2185 (pre-sweep) — the fix-round-1/fix-round-2 adjudication
  narrative of the detach-vs-generation mechanisms (the TECHNICAL content is
  kept in condensed form; the review-process history went).
- qp_engine.h:1365-1380 (pre-sweep) — "PHASE-3 DESIGN FRICTION" origin of
  SolveOverrides.
- qp_engine.h:3620 (pre-sweep) — export-invariant enforcement-site ruling
  provenance ("option 3, Grant-ruled").
- ssn_engine.h:54, 464 (pre-sweep) — the original defect reports (first
  implementation certifying a saddle seeded on it) as review findings C1/M7.
- ssn_engine.h:1024-1025 (pre-sweep) — "Task 4 owns the two escapes declared
  unreachable in Task 3" banner-discharge history.
- ssn_engine.h:1115-1136 (pre-sweep) — proximal-centre ruling history (which
  brief asked what, which task measured what).
- eqp_solve.h:69-76, 93-95 (pre-sweep) — the flag-gated refinement loop that
  ran for one phase and never fired: 27 HS problems x 2 regimes x 2 algebra
  modes + 13 adversarial probes, bit-identical everywhere, measured
  independently on MKL/Pardiso and Accelerate; disposition in
  docs/notes/2026-07-29-eqp-refinement-ab.md, audit in
  docs/notes/2026-07-29-accelerate-audit-results.md.

## Looked like a defect, reported not fixed

None found in this chunk. (The closest look: qp_types.h documents that
SolveOverrides cannot express "tr_radius = +inf overriding a finite opts_
default" — a disclosed single-sentinel tradeoff, not a bug.)

## Docstrings written under uncertainty (reviewer first)

Condensed from longer originals; reviewer should verify against code:

1. qp_engine.h — reuse condition (e) ("THE FACTOR-IDENTITY CONJUNCT") and the
   HotState ownership block's two-mechanism summary (detach = load-bearing,
   identity pair = defense-in-depth); the original argument chains are long and
   the condensation could have dropped a qualifier.
2. ssn_engine.h — section 7b's Accelerate corollary (perturbed-pivot semantics
   differing between backends and the certify-the-saddle consequence) kept
   verbatim in substance but re-flowed; check no clause was lost.
3. qp_types.h — max_iter's sentinel/precedence/honest-failure contract was
   heavily condensed; verify the three claims survive (explicit value wins;
   only sentinel-and-cap-reaching solves change; kMaxIter stays an honest
   failure).
