# Chunk 7 — model + drivers (cut from branch m4, NOT sweep/base)

Branch `sweep/model-drivers` (off hven `m4` @ d73779e). `git fetch origin && git switch
sweep/model-drivers` — do NOT use sweep/base for this chunk: these files were rewritten on m4
and differ from main.

Scope: every `.h`/`.cpp` under `include/hven/model/`, `include/hven/detail/model/`,
`include/hven/detail/drivers/`, `src/model/`, `src/drivers/`; plus `src/CMakeLists.txt` (see
below). Nothing outside.

Apply `.sweep/comment-rules.md`. Rubric for this chunk (settler's, binding): a Doxygen block is
a CONSTRAINT ON THE CALLER — what it takes, returns, throws, and the invariant it keeps — never
the record of WHY. The "why" lives once, in the .cpp at the site, or not at all; the four header
blocks that carry settlement/review narration (the declaration-adoption entry, the lay markers,
the claim-block check, the aggregate contract) condense to their contract. `@throws` states the
exact condition the body checks, read off the code — the model/drivers refusals are numerous
and site-named; do not merge cases. Cost/counter statements keep every case.

`src/CMakeLists.txt`: the "M3 PHASE-C T1…T8 ADDENDUM" blocks (~400 lines of TU-carve
measurement provenance, roughly lines 263–440) MOVE VERBATIM — not edited — into a new file
`docs/notes/2026-08-m3-phase-c-tu-carve-addenda.md` (one heading, then the text as-is), leaving
a one-line pointer comment in CMakeLists.txt. Other CMake comments: labels stripped, facts kept.

Commit subject `docs: comment sweep — model and driver headers and sources`, body one
paragraph, last line `Counts: unchanged`. Report `.sweep/chunk-7-report.md` (same sections;
list the four narration blocks and what each condensed to). Push `sweep/model-drivers` and
report the hash. No builds.
