# RS-3 (prose and literals; comments-only stage) -- DECLARATION
Committed before the edits; never edited afterwards (a failed gate is DECLARATION-RS3-r2.md).
Base: origin/m8 aa1330cad (the RS-2 head). Design: RS-design-page-v2.md (RS-3; S9; Q13, Q14; section 7). No file moves; no code behaviour changes.

## What changes (everything else must not change)
1. Comments, banners, `#error` texts and diagnostic message texts (static_assert / gtest-failure / FATAL_ERROR messages) in include/ src/ tests/ bench/ scripts/ cmake/
   ci.yml and CMake banner comments that name pre-RS paths are re-pointed through the move map (tools/rs3_comments.py: a token is rewritten only if it is not a
   suffix of a file in the tree today and is a suffix of exactly one pre-RS path of the map). Self-path banner lines are dropped (Q13). Origin-project names are swept from comments.
2. Golden-rig recipe description strings (tests/golden_rig/recipes.cpp) are re-pointed. They are the ONLY edits that may change a compiled object, and only objects of the
   golden_rig target (string-literal bytes in .rodata).
3. CLAUDE.md sections 1 ("Built so far", the "neither engine is here" paragraph) and 2 are rewritten (owner-approved); the two section-1 exceptions keep their substance.
4. docs/*.md (not docs/notes/**) stale paths fixed; "Include paths (RS)" added to docs/migration-0.2.md; `.git-blame-ignore-revs` added.

## Literal policy (recorded in the migration section, never edited)
A string literal that can reach an artifact (CSV provenance rows, bench stdout/usage output, golden bytes, trace fixtures) is left alone.
Bench literals are left alone. docs/notes/**, evidence artifacts, notices/ and frozen CSV provenance headers are never touched.

## Pass criteria (Linux)
G1  clean configure, CCACHE_DISABLE=1, linux-clang-release flags + bench + examples; library + tests + bench build rc 0.
G2  P-SYM before/after (before = aa1330cad, after = the RS-3 head), both arms with `__FILE__` neutralised (RS-2's method, same flag list in both arms, same build path):
    0 UNCLASSIFIED, 0 missing, 0 collisions, empty exceptions file, EXCEPT the golden_rig target's objects, whose literal-byte change is declared here: a difference
    confined to objects of tests/golden_rig (recipes.cpp and what it links) is classified, not a failure, provided every other object is byte-identical or noise-class.
    The object-map is the identity (nothing moves), the symbol map empty.
G3  ctest Release passes in full (same count as RS-2: 3869); sorted ctest -N identical to the RS-2 run.
G4  check_install_smoke.sh passes (default + clang++), check_export_contract.sh OK; the installed listing is identical to RS-2's (no installed file is added or removed;
    installed headers change only in comments).
G5  CI on origin rs-3 (workflow_dispatch): all lanes green.
Not re-run (comments cannot move them; declared): flags diff beyond a cc-json identity check, 27-cell replay, Debug ctest, check_pch_neutrality. Wall-clock informational.
