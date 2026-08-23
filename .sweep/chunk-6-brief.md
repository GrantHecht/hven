# Chunk 6 — src/ (core, globalization/sqp, interior, interior/utils, kkt, linear, warmstart)

Branch `sweep/base`. Pull first.

Scope: every `.cpp` and `.h` under `src/core/`, `src/globalization/sqp/`, `src/interior/`,
`src/interior/utils/`, `src/kkt/`, `src/linear/`, `src/warmstart/`, plus `src/hven_pch.h`
(22 files, ~5.3k lines). NOT in scope: `src/model/` and `src/drivers/` (those wait for a
separate grant). `get_core_count.cpp` keeps its Boost notice verbatim; the two Eigen-derived
linear sessions keep their MPL/BSD notices verbatim.

Apply `.sweep/comment-rules.md`. These are implementation files: docstrings belong on the
declarations in the headers (already swept), so in `.cpp` files the surviving comments are
concise descriptors of obscure code and important-fact callouts only — a definition should not
repeat its header's docstring. Counter/cost statements keep every case; `@throws`-style facts
in the body become one-line callouts at the throw site, read off the code.

Commit subject `docs: comment sweep — src implementation files`. Report `.sweep/chunk-6-report.md`
(same sections). `git pull --rebase origin sweep/base` before pushing; push; report the hash.
