# Chunk 5 — include/hven/core and include/hven/drivers

Branch `sweep/base`. Pull first.

Scope: every `.h` under `include/hven/core/` (7 files) and `include/hven/drivers/` (4 files:
`interior_point_solver.h`, `sqp_driver.h`, `sqp_types.h`, `optimization_problem_base.h` — ~6.9k
lines, the library's PUBLIC surface). Nothing outside.

Apply `.sweep/comment-rules.md` (re-read it; the Accuracy section has grown). Two things
specific to this chunk:
- These are public headers. Every public declaration keeps (or gains) a Doxygen docstring in the
  structured form: `/// @brief` one line, then `@param`/`@tparam`/`@return`/`@throws` as the
  signature warrants, terse contract text. Options structs: one `///<` per field stating meaning,
  unit and default/domain — that is the documentation users read; do not delete field docs,
  condense them.
- `sqp_driver.h` and `sqp_types.h` carry long design notes (cost accounting, hand-off
  invariants, SOC/elastic rationale). Condense each to its contract + invariant; the argument
  chain goes to the report's rationale candidates. Counter/cost statements must keep EVERY case
  (a prior chunk lost a "one extra minor_iter" case by merging two sentences — never merge
  cases). `@throws` must state the full domain the body checks (NaN, negative, range), read off
  the code.

Commit subject `docs: comment sweep — include/hven/core and include/hven/drivers`. Report
`.sweep/chunk-5-report.md` (same sections). Before pushing, `git pull --rebase origin sweep/base`
(other directories may have moved). Push and report the hash.
