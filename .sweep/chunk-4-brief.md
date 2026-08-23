# Chunk 4 — include/hven/detail/{kkt,linear,warmstart,solvers} and include/hven/linear

Branch `sweep/base`. Pull first.

Scope: every `.h` under `include/hven/detail/kkt/`, `include/hven/detail/linear/`,
`include/hven/detail/warmstart/`, `include/hven/detail/solvers/` and `include/hven/linear/`
(17 files, ~7.9k lines). Nothing outside. The four Eigen-derived linear-session files keep
their MPL/BSD notice blocks verbatim (headers are not in scope for this pass at all).

Apply `.sweep/comment-rules.md` (read it again — it has grown an Accuracy section) to every
comment. Calibration from chunks 1–3: chunk 1 halved its comments and was the right depth;
chunk 3 left derivation-dense files nearly untouched and was too conservative. The rule is
not "keep derivations" — it is: a derivation condenses to the conclusion a reader needs plus
the invariant it protects; the argument chain goes to the report's rationale candidates.
Labels (Task N, T6, G1, M3, FIX ROUND, WAVE, review names, PR #) are removed everywhere.

Commit subject `docs: comment sweep — include/hven/detail/kkt, linear, warmstart, solvers`.
Report `.sweep/chunk-4-report.md` (same sections as chunk 3). Push `sweep/base`, report the hash.
