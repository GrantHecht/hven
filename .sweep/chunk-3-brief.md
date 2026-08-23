# Chunk 3 — include/hven/detail/qp and include/hven/qp

Branch `sweep/base`. Pull first; chunks 1 and 2 and the chunk-1 accuracy fix must already be
on the branch.

Scope: every `.h` under `include/hven/detail/qp/` (5 files, ~9.4k lines — the largest chunk;
`qp_engine.h` and `ssn_engine.h` are long) and `include/hven/qp/` (1 file). Nothing outside.

Apply `.sweep/comment-rules.md` — including its Accuracy section — to every comment in those
files. The QP/SSN headers carry derivations and numerical caveats: a derivation that a reader
needs to trust a constant or a branch is an important-fact callout and stays (condensed, true);
history of how the value was arrived at goes to the report's rationale candidates. Re-read the
code before shortening any formula.

Commit subject `docs: comment sweep — include/hven/detail/qp`. Report: `.sweep/chunk-3-report.md`
(same sections as chunk 2, including "Docstrings written under uncertainty"). Push `sweep/base`
and report the hash.
