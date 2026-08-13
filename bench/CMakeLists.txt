################################################################################
# The SQP engine's bench harness — migrated whole at M3 (phase A) as a library
# asset (M0 integration plan §5: the 57-cell replay corpus, gate evaluator and
# escape-reason census are deliverables, not sandbox residue), together with
# the frozen baseline artifacts under bench/baselines/ that the test suite
# re-scores and pins.
#
# Every binary here is a measurement instrument, deliberately NOT registered
# with ctest; each one's correctness gate is a registered test that shares its
# implementation (never a copy). They are BUILT whenever the suite is, because
# an uncompiled probe rots silently — the origin project's own committed
# rationale, kept.
#
# COMPILE FLAGS: same deliberate choice as tests/sqp/CMakeLists.txt — plain
# per-config defaults, matching the regime every committed baseline was
# derived under. See that file's banner for the full reasoning and the
# execution-review question it defers.
################################################################################

# hven_sqp_bench — the bench harness and wall-time instrumentation. Its own
# correctness gate is the --self-check flag documented in bench_scale.cpp,
# which the M3 gate (and any benchmark script) runs explicitly.
add_executable(hven_sqp_bench bench_scale.cpp)
target_link_libraries(hven_sqp_bench PRIVATE hven::hven)
# tests/sqp/support/{parametric_families,scale_problems}.h are
# test-support-only headers (no gtest dependency), included by relative path.
target_include_directories(hven_sqp_bench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")

# hven_sqp_f7_cold — the single-solve F7 cold-start probe behind the archived
# cold-scale study. Committed so those sections stay reproducible; NOT
# ctest-registered (its longest documented invocation runs well over an hour).
add_executable(hven_sqp_f7_cold bench_f7_cold.cpp)
target_link_libraries(hven_sqp_f7_cold PRIVATE hven::hven)
target_include_directories(hven_sqp_f7_cold PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")

# hven_sqp_tau_bar_sweep_probe — the hs_sweeps.h cold-arm reproduction vehicle
# for the archived elastic-tau tuning note's §4.4 sweep. The shipped build
# only ever reads the one kFunnelTauBar the library ships; SWEEPING it is a
# scratch, patched-header recompile outside this build (the probe's own banner
# documents the exact invocation), never a CMake option here.
add_executable(hven_sqp_tau_bar_sweep_probe tau_bar_sweep_probe.cpp)
target_link_libraries(hven_sqp_tau_bar_sweep_probe PRIVATE hven::hven)
target_include_directories(hven_sqp_tau_bar_sweep_probe
                            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")

# hven_sqp_snopt_f7 — the SNOPT measurement binary. GUARDED: exists only when
# the root-level SNOPT detection succeeded — a machine without SNOPT keeps
# building and testing unchanged. SNOPT is commercial and is NOT vendored:
# bench/snopt_f7_driver.h documents the source firewall in full. Its
# correctness gate is tests/sqp/test_snopt_bridge_gate.cpp, which shares this
# target's SnoptF7Driver implementation and IS registered.
if(HVEN_SQP_HAVE_SNOPT)
    add_executable(hven_sqp_snopt_f7 bench_snopt_f7.cpp)
    target_link_libraries(hven_sqp_snopt_f7 PRIVATE hven::hven hven_sqp_snopt)
    target_include_directories(hven_sqp_snopt_f7 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")
endif()

# hven_sqp_corpus — the replay corpus runner: cell specs, tags, the engine
# interface and the walk engine's own implementation all live in
# corpus_cells.h (a header, deliberately, so tests/sqp/test_corpus_cells.cpp
# shares exactly this implementation rather than a bench-only copy that could
# drift). bench_corpus.cpp is CLI glue only. NOT ctest-registered: its
# baseline sweep touches N = 20000 cells that run for minutes to hours. Its
# correctness gate is tests/sqp/test_corpus_cells.cpp.
add_executable(hven_sqp_corpus bench_corpus.cpp)
target_link_libraries(hven_sqp_corpus PRIVATE hven::hven)
target_include_directories(hven_sqp_corpus PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")

# Every CSV this runner writes carries a provenance header, and the first line
# of it is WHICH BINARY produced the rows. Resolved at CONFIGURE time (not
# build time) because that is when CMake can shell out; a tree with no git
# degrades to the literal string "unknown" rather than failing the configure.
# `--dirty` is deliberately included: a baseline produced from an uncommitted
# tree must say so.
execute_process(
    COMMAND git describe --always --dirty --abbrev=12
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE HVEN_SQP_CORPUS_GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE HVEN_SQP_CORPUS_GIT_RC)
if(NOT HVEN_SQP_CORPUS_GIT_RC EQUAL 0 OR HVEN_SQP_CORPUS_GIT_DESCRIBE STREQUAL "")
    set(HVEN_SQP_CORPUS_GIT_DESCRIBE "unknown")
endif()
target_compile_definitions(hven_sqp_corpus PRIVATE
    HVEN_SQP_CORPUS_GIT_DESCRIBE="${HVEN_SQP_CORPUS_GIT_DESCRIBE}")

# The wall-deadline tests in tests/sqp/test_corpus_cells.cpp
# (CorpusRunnerProcess.WallDeadline*) are SUBPROCESS tests — they invoke the
# actual compiled hven_sqp_corpus binary (the deadline enforcement itself is
# fork/exec orchestration in bench_corpus.cpp's main(), not something a gtest
# linked only against corpus_cells.h can reach). hven_sqp_tests already
# exists at this point (tests/ is add_subdirectory'd before bench/ in the
# root CMakeLists.txt), so the dependency and the path macro are wired HERE
# rather than in tests/sqp/CMakeLists.txt, where hven_sqp_corpus would not
# exist yet. Guarded: a tests-off, bench-on configure has no test target to
# decorate.
if(TARGET hven_sqp_tests)
    target_compile_definitions(hven_sqp_tests PRIVATE
        HVEN_SQP_CORPUS_BINARY="$<TARGET_FILE:hven_sqp_corpus>")
    add_dependencies(hven_sqp_tests hven_sqp_corpus)
endif()

# hven_sqp_ssn_safeguard_probe — the reproduction vehicle for the archived SSN
# safeguards note's search and sweep populations. Committed and built so the
# note's evidence cannot rot; NOT ctest-registered (its documented populations
# run for minutes). Its fixtures are tests/sqp/support/ssn_fixtures.h, the
# same file tests/sqp/test_ssn_engine.cpp includes, so the note and the test
# suite cannot drift apart on a fixture. The band variants it compares are
# patched-header recompiles outside this build — see the probe's banner.
add_executable(hven_sqp_ssn_safeguard_probe ssn_safeguard_probe.cpp)
target_link_libraries(hven_sqp_ssn_safeguard_probe PRIVATE hven::hven)
target_include_directories(hven_sqp_ssn_safeguard_probe
                            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")
