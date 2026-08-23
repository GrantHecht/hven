# Chunk 1 — include/hven/detail/globalization (incl. sqp/)

Branch `sweep/base`. Pull first; pass 0 (header normalisation) must already be on the branch
(`git log --oneline -3` shows "docs: normalise source file headers"). If it is not, stop and
report.

Scope: every `.h` under `include/hven/detail/globalization/` and
`include/hven/detail/globalization/sqp/` (28 files, ~7.9k lines). Nothing outside.

Apply `.sweep/comment-rules.md` to every comment in those files. Commit subject
`docs: comment sweep — include/hven/detail/globalization`. Report: `.sweep/chunk-1-report.md`.
Push `sweep/base` when done and report the commit hash.
