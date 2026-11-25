# RS-5 (layering test, export contract, self-contained headers) -- DECLARATION
Committed before any RS-5 edit; never edited afterwards (a failed gate is DECLARATION-RS5-r2.md, etc.).
Base: origin/m8 bc8ed0e12 (the RS-4 head plus its CI record). The "before" arm is this commit, built and measured by legs/baseline2.sh into the ignored .scratch/.
Design: RS-design-page-v2.md section 2.3, RS-5 group, section 5.2. Carries DECLARATION.md + -r2 + -r3, DECLARATION-RS2*.md, DECLARATION-RS3*.md and DECLARATION-RS4*.md
(instrument rules; `__FILE__`-neutralised P-SYM arms, one capture-tool stamp).

## What changes (everything else must not change)
1. Layering test. tests/core/test_core_layering.cpp is renamed to tests/core/test_layering.cpp (git mv; its two CoreLayering gtests and their file-count anchors are kept verbatim).
   New gtests in the same file enforce section 2.3's single order over include/hven (public and detail) and src/: core < util < barrier < linear < model < qp-data < kkt < qp-engines
   < warmstart < solver < {ipm, sqp} public < trace < engine implementations < hven.h; ipm and sqp never include each other. Explicit file-to-file allow-list, a debt note per pair.
   The allow-list is RE-DERIVED from the current tree (a separate scan, not copied from the design). Measured before any edit: exactly the design's 12 pairs (the 13th, nlp_model.h -> qp_types.h,
   went in RS-4); no pair the design lists has gone, no new pair has appeared. A stale allow-list entry fails the test (so debt cannot silently outlive its pair).
   Added gtests (hven_tests): Layering.TheScanReadsTheWholeTree, Layering.NoIncludeGoesUpTheLayerOrder, Layering.EveryAllowListedPairStillOccurs, Layering.PublicHeadersDoNotIncludeDetail. +4.
2. Export contract. scripts/check_export_contract.sh asserts: nothing at the installed include/hven root except hven.h; every directory directly under include/hven (and under include/hven/detail) is a known component.
3. Self-contained headers. NlpEval (the struct only) is extracted from detail/sqp/sqp_solver_impl.h into its own header detail/sqp/nlp_eval.h; assembly_eval_seam.h, globalization/restoration.h and soc.h
   include it directly and lose their NOT-SELF-CONTAINED-BY-DESIGN notes. The comment-separated include-block workarounds RS-4 added (src/sqp/assembly_eval_seam.cpp, tests/solver/test_infinite_bound.cpp,
   tests/sqp/test_assembly_eval_seam.cpp) are removed and the files formatted normally. A CTest test (HeaderSelfContained, +1) builds an EXCLUDE_FROM_ALL syntax-only object library with one generated TU per header
   under include/hven (Apple-only headers accelerate_threads.h, accelerate_session.h, lapacke_shim.h excluded off Apple by a documented list).
4. CLAUDE.md section 2's layering sentence is updated (layering is enforced for the whole order by tests/core/test_layering.cpp). Nothing else in CLAUDE.md.
5. Mutation proof: a planted layering violation, a planted root header and a planted non-self-contained header each fail the corresponding check; restoring each passes. Recorded in gates/RS-5/mutation/.
No identifier, logic or other file changes. Moving NlpEval is symbol-neutral by construction (a struct definition moves between headers; its inline members, if any, keep their mangled names); any symbol change is explained.

## Pass criteria (Linux)
G1  clean configure, CCACHE_DISABLE=1, linux-clang-release preset flags + bench + examples; library + tests + bench + examples build, rc 0.
G2  P-SYM, both arms `__FILE__`-neutralised (neutral_flags.py), same build path, no object map, no symbol map, EMPTY exceptions: 0 UNCLASSIFIED, 0 missing, 0 only-before; the only-after objects/symbols allowed
    are those of test_layering (the renamed object, new test code) -- if the renamed test object shows as missing+only-after it is reported, explained, and accepted by a successor declaration, not waved through.
    Non-identical objects are explained per symbol (`__LINE__` immediates, emission order, NlpEval-move effects). The symbol SET of every library and non-test object must be unchanged.
G3  compile_commands.json flag diff: identical for all pre-existing TUs; the only delta is the new hven_header_selfcheck TUs (only-after, all under that target), reported per target.
G4  scripts/check_pch_neutrality.sh passes if the PCH include set is touched (it is not expected to be; run regardless).
G5  ctest Release and Debug 100%. Sorted gtest lists RAW-identical to the base's per executable except hven_tests (+4 Layering.* names; CoreLayering.* kept); ctest -N differs from base by exactly +5
    (the 4 Layering gtests, HeaderSelfContained) and nothing removed.
G6  check_install_smoke.sh on both front ends + the extended check_export_contract.sh; the installed listing equals the RS-4 listing plus detail/sqp/nlp_eval.h (+1 file, 958 total).
G7  27-cell replay x (walk, ssn, ipm), compare_replay.py against the base replay: 0 differences, serial, solo, under the box lock.
G8  Mutation proofs (item 5) recorded and reproducible; header sweep over all headers: 0 FAIL except the three Apple-only headers.
G9  CI on origin rs-5 (workflow_dispatch): all four lanes green.
Wall-clock is informational and never asserted. Box rules (CLAUDE.md 7-8) apply: one solver process at a time, under flock /tmp/box-build.lock.
