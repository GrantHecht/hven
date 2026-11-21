# RS-1a..1e (code-organisation restructure, pure moves) -- DECLARATION
Committed before the first gated build; never edited afterwards (a failed gate is DECLARATION-r2.md).
Base origin/m8 abfa51b04. Design: RS-design-page-v2.md (section 5.2 gate). Map: RS-move-map-v2-amended.txt (9 lines differ from v2, all owner
rulings of 2026-11-17, ledger C392: the 7 S5 deletions confirmed, color_text.{h,cpp} deleted). Groups: groups/RS-1{a..e}.map.

## What changes, per group (everything else must not change)
- 1a: detail/core (+eigen_types, super_scalar_traits, finite_screen), detail/util, accelerate_threads -> detail/ipm; src/interior/utils -> src/util
  (get_core_count, memory_management, thread_pool); DELETE the 5 zero-includer util headers, fmtlib.{h,cpp}, color_text.{h,cpp}. Private include dir
  removed, _hven_expected_source_count 58 -> 57. notices/boost-bsl1.txt: path text of get_core_count.cpp only (owner authorisation, C392).
  test_core_layering scans include/hven/core AND include/hven/detail/core, asserting 11 + 6 files.
- 1b: barrier, kkt (kkt_calls -> kkt_factor rename), qp/walk|ssn|ipqp split with the walk file renames, fixed_variable_row -> detail/model.
- 1c: IPM engine (public ipm/, detail/ipm, detail/ipm/globalization, src/ipm; solve_status -> ipm_phase_status); hvenConfig.cmake.in sentinel -> hven/hven.h.
- 1d: SQP engine (public sqp/, detail/sqp incl. globalization/ + quasi_newton/, continuation+predictor, src/sqp; globalization.h -> funnel.h);
  HVEN_SQP_SOLVER_SOURCE re-pointed.
- 1e: solver/ and trace/ public + detail + src; warm_start.h/.cpp -> crossover; the 7 .gitkeep deleted; trace-off-path scan re-pointed.
Content edits are include lines, CMake path strings, hven_pch.h, the umbrella, install-smoke include TUs, path-coupled tests. clang-format --sort-includes=false.
target_sources order preserved. No symbol, namespace or behaviour change.

## Pass criteria, for EACH group (run and recorded per group; Linux)
G1  clean configure, CCACHE_DISABLE=1, linux-clang-release preset flags + HVEN_BUILD_BENCH=ON + HVEN_BUILD_EXAMPLES=ON; library + tests + bench build.
G2  P-SYM against the RS-0 baseline capture with the generated --object-map (+ RS-0 --symbol-map): 0 UNCLASSIFIED, empty exceptions file.
    1a only: the removed objects (color_text) appear as removed objects/symbols only, nothing else. Section hashes (.text*/.rodata*/.data*) per object informational.
G3  compile-flags diff (library, tests, bench TUs; source/-o paths normalised through the map): identical. Only expected delta: the removal of the PRIVATE
    -I src/interior/utils (1a).
G4  scripts/check_pch_neutrality.sh PASS on 1a, 1b, 1c (PCH include set touched).
G5  Debug and Release ctest all pass; sorted gtest list per executable and ctest -N identical to the RS-0 baseline (deleted code has no tests: confirmed by identity).
G6  path-coupled tests re-pointed and non-vacuous: trace-off-path site count EQUALS the RS-0 count; core-layering file counts asserted; message-sites and
    HVEN_SQP_SOLVER_SOURCE read the moved files.
G7  check_install_smoke.sh on both front ends (default and clang++) + check_export_contract.sh; installed listing == baseline listing with the map applied;
    old dirs absent; public header count 55; hvenConfig sentinel resolves.
G8  golden rig (native arm in ctest; three-seam arms attempted once at baseline and at the 1e head) passes, expected tables unchanged.
G9  27-cell replay x (walk, ssn, ipm) byte-exact (compare_replay.py, 0 differences) against the RS-0 baseline run on the base commit; the M7 WX2 pin reported alongside.
    Counters only; MKL_NUM_THREADS=1; protocol: serial, solo, under the box lock (no co-run).
G10 (head only) CI on origin rs-1: Linux, macOS, Windows lanes green.
Wall-clock is informational and never asserted.
