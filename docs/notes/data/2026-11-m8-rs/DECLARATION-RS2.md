# RS-2 (test re-layout, pure moves) -- DECLARATION
Committed before the first gated build of the move; never edited afterwards (a failed gate is DECLARATION-RS2-r2.md).
Carries DECLARATION.md + -r2 (G2' exceptions only for design-mandated test edits, each with its clause) + -r3 (G5': ctest -N compared modulo the byte-dump class).
Base: origin/m8 3edf5ede4 = the RS-1e head (what RS-2 branches from; the "before" arm is this commit built and measured by legs/baseline2.sh into the ignored .scratch/).
Design: RS-design-page-v2.md (RS-2 group; S6, S7; M3/M4/M5). Map: groups/RS-2.map (= the 198 `tests/` lines of RS-move-map-v2-amended.txt, verified identical).
Executable membership, target names and add_executable order are UNCHANGED (per-component executables are RS-6a).

## Instruments (committed BEFORE the move commit, so no instrument changes inside the change it measures)
- tools/rs2_apply.py (mover: git mv per map + S7 include re-spelling), tools/rs2_cmake.py (CMake source-token re-pointing), tools/flags_diff2.py
  (flags_diff.py + --dir-move / --allow-removed / --allow-added; flags_diff.py unchanged), legs/gate2.sh, legs/baseline2.sh.
- legs/env.sh: the worktree path is now derived from the file's own location (was a hard-coded path of the RS-1 worktree). No measurement effect.
- Unchanged and reused as they are: scripts/psym_compare.sh, scripts/rs_object_map.py, tools/gen_exceptions.py, tools/lists_compare.py, tools/listing_apply_map.py,
  legs/{build,ctest,lists,replay,install_smoke}.sh, symbol-map-rs2.txt (prepared by RS-0).

## What changes (everything else must not change)
1. 198 test-tree entries move (git mv): fixtures to support/ directories, tests to tests/{core,linear,model,kkt,qp,warmstart,solver,trace,ipm,bench}/, goldens to
   tests/trace/golden/, compile_fail/ to tests/model/compile_fail/, the two CMakeLists.txt to tests/solver/ (from drivers/) and tests/ipm/ (from interior/).
   Renames (S6): test_solve_status -> tests/ipm/test_ipm_phase_status, test_types -> tests/qp/test_qp_types, test_scaffold -> tests/linear/test_backend_scaffold.
2. Content edits are: (a) #include lines -- every test-local include is spelled from the tests root (S7), except a bare same-directory sibling, which stays bare
   (the try_run dispatch probe has no tests include root); bench include lines likewise; (b) CMake path strings; (c) .gitattributes:7; (d) nothing else.
3. CMake: source paths in tests/CMakeLists.txt and the per-directory lists (cross-directory members are spelled `../<dir>/<file>`; list ORDER preserved);
   add_subdirectory interior->ipm, drivers->solver (positions kept); HVEN_GOLDEN_DIR / HVEN_TRACE_FIXTURE_DIR re-pointed to tests/trace/golden/...;
   the compile_fail probe directory and runner script re-pointed (the probe loops stay in tests/ipm/CMakeLists.txt until RS-6a); mkl_dispatch_probe path;
   `HVEN_TESTS_INCLUDE_ROOT` (= tests/) added as the include root of the test targets and bench targets that spell local includes from it, replacing each old
   per-directory root (tests/{drivers,interior,model,sqp}); bench/CMakeLists.txt `../tests/sqp` -> `../tests`; the configure-time duplicate-basename check across
   support/ directories (S7) -- it must pass on the real tree and was proven to fail on a planted duplicate (recorded in gates/RS-2/dupcheck.txt).
4. Prose, banners, comments and string literals are NOT touched (RS-3), including `#error` texts and bench/*.cpp literals that name old paths.

## Pass criteria (Linux), all for the single RS-2 move commit
G1  clean configure, CCACHE_DISABLE=1, linux-clang-release preset flags + HVEN_BUILD_BENCH=ON + HVEN_BUILD_EXAMPLES=ON (FetchContent googletest pinned to the same local source in both arms);
    library + tests + bench build, rc 0.
G2' P-SYM vs the RS-1e baseline capture, generated --object-map (groups/RS-2.map) + --symbol-map symbol-map-rs2.txt: 0 UNCLASSIFIED, 0 MISSING, 0 COLLISION. Expected: the exceptions
    file is EMPTY. (RS-2's design-mandated test edits are include-line spellings only; none changes code.) If a finding appears in an object whose source the design edited
    beyond include lines, it is excepted only through gen_exceptions.py with the design clause; any other finding fails the gate.
G3' compile-flags diff, flags_diff2.py with the RS-2 map, over every TU: the TU sets are identical, and every flag list is identical except exactly:
      - removed tokens fully matching -I@/tests/(drivers|interior|model|sqp) (the old per-directory include roots), and added token -I@/tests (the new root);
      - path-valued defines under tests/drivers/golden normalised through --dir-move tests/drivers/golden=tests/trace/golden (HVEN_GOLDEN_DIR, HVEN_TRACE_FIXTURE_DIR);
      - the git provenance stamps (already normalised by the tool).
    The per-target tally is printed and recorded in RESULT.txt; no target may gain -I@/tests unless some TU of it spells a root-relative local include.
G4  scripts/check_pch_neutrality.sh: NOT required (RS-2 touches neither hven_pch.h nor any library include set; design 5.2 item 4 lists RS-1a/1b/1c and RS-4 only).
G5' ctest Release and Debug all pass (3869 tests, as RS-1e); per-executable sorted gtest lists RAW-identical to the RS-1e measured lists AND to the committed RS-0 lists;
    sorted ctest -N compared modulo the byte-dump class (133 = 133, DECLARATION-r3).
G6  path-coupled tests re-pointed and non-vacuous: test_trace_off_path (site count EXACTLY 130), test_message_sites, test_qp_mode_sites (HVEN_SQP_SOLVER_SOURCE),
    test_core_layering (11 + 6), the golden-trace and trace_v1 fixture tests (they read HVEN_GOLDEN_DIR / HVEN_TRACE_FIXTURE_DIR and fail on an empty/absent directory -- verified
    by a planted-wrong-path configure that makes them fail, recorded), and the compile_fail probes (22 hven_compile_fail_* tests pass with their runner script re-pointed).
G7  check_install_smoke.sh on both front ends + check_export_contract.sh; installed listing == RS-1e listing (RS-2 touches no installed file); 55 public headers.
G8  golden rig native arm passes in ctest (three-seam arms: pre-existing non-compilation of the two old-seam adapters, reported at RS-1; not re-attempted).
G9  27-cell replay x (walk, ssn, ipm): compare_replay.py 0 differences against the replay measured on the RS-1e head by baseline2.sh; serial, solo, under the box lock.
G10 CI on origin rs-2 (workflow_dispatch): all four lanes green.
Wall-clock is informational and never asserted.

## Judgement calls the design leaves open (recorded, not invented silently)
D1  "Every test-local include is spelled from the tests root": a bare same-directory sibling include keeps its bare spelling (incl. tests/support/mkl_dispatch_probe.cpp -> mkl_dispatch_signature.h).
D2  The tests include root is added per target where needed (not as a directory-wide include), so unrelated targets' flags do not change.
D3  Cross-directory members of an unchanged executable are spelled `../<dir>/file.cpp` in the owning CMakeLists (RS-6a replaces these lists).
D4  golden_rig/ and install_smoke/ keep their layout and include directories (design: unchanged).
