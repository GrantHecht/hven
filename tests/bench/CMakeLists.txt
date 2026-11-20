################################################################################
# bench/ tests: hven_bench_tests: the tests that pin a bench/*.h instrument or drive the measurement
# binary: the corpus cells and runner (test_corpus_cells), the bench dump CLI (test_bench_dump), the
# crossover legs (test_crossover_legs), the SNOPT bridge gate (only when the root-level SNOPT
# detection succeeded) and the model-surface KKT instrument (test_model_surface_kkt, built on every
# platform). bench headers are spelled "bench/<h>.h" from the repository root (INCLUDE_BENCH).
#
# bench/ipm_corpus_leg.cpp is COMPILED INTO this binary, as corpus_cells.h is included into
# test_corpus_cells.cpp: one implementation shared by the measurement binary and the assertion that
# guards it, never a second copy. It is an EXTRA source because a path out of the tree has no place
# in the list of this directory's own files.
#
# The wall-deadline tests in test_corpus_cells.cpp (CorpusRunnerProcess.WallDeadline*) are
# SUBPROCESS tests that invoke the compiled hven_sqp_corpus binary; the dependency and the
# HVEN_SQP_CORPUS_BINARY path macro are wired in bench/CMakeLists.txt, where hven_sqp_corpus exists.
# That binary's fork/exec deadline enforcement is why those tests (and this directory's other
# bench-driving tests) are not built on Windows.
#
# The committed baselines the cells test re-scores through the offline --from-csv path are named by
# path, not copied: the artifact and the evaluator hold each other in place, so if either moves the
# pins fail and whoever moved it re-sweeps rather than re-labels (docs/testing.md, "History:
# baseline pointers", has the dated list of re-pointings). The SNOPT bridge's referee gate adds four
# tests ONLY when SNOPT was detected, so the suite size is a function of the machine.
# Flags: the project's level.
################################################################################

set(_hven_bench_sources
    test_model_surface_kkt.cpp
)

set(_hven_bench_unix_sources
    test_bench_dump.cpp
    test_corpus_cells.cpp
    test_crossover_legs.cpp
)
set(_hven_bench_extra_sources "")
set(_hven_bench_defs "")
if(NOT WIN32)
    set(_hven_bench_baselines "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/baselines")
    list(APPEND _hven_bench_sources ${_hven_bench_unix_sources})
    list(APPEND _hven_bench_extra_sources
        "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ipm_corpus_leg.cpp")
    list(APPEND _hven_bench_defs
        HVEN_SQP_CORPUS_BASELINE_CSV="${_hven_bench_baselines}/2026-08-16-u0-corpus/walk_baseline.csv"
        HVEN_SQP_PRE_U0_WALK_BASELINE_CSV="${_hven_bench_baselines}/2026-08-06-corpus/walk_baseline.csv"
        HVEN_SQP_SSN_BATTERY_CSV="${_hven_bench_baselines}/2026-08-08-gate-battery/ssn_battery.csv"
        HVEN_SQP_WALK_RESWEPT_CSV="${_hven_bench_baselines}/2026-08-08-gate-battery/walk_reswept.csv"
        HVEN_SQP_TASK6B_SSN_CSV="${_hven_bench_baselines}/2026-08-09-task6b-resweep/ssn_resweep.csv"
        HVEN_SQP_TASK6B_WALK_CSV="${_hven_bench_baselines}/2026-08-09-task6b-resweep-walk/walk_resweep.csv"
        HVEN_SQP_IPM_BASELINE_CSV="${_hven_bench_baselines}/2026-09-02-t10b-ipm/ipm_baseline.csv"
        HVEN_SQP_INTERIOR_BASELINE_CSV="${_hven_bench_baselines}/2026-11-m8-ap5-gs-ipm-leg/interior_baseline.csv")
else()
    hven_test_sources_absent("Windows: the corpus/crossover runner binaries are POSIX fork/exec, not built there" ${_hven_bench_unix_sources})
endif()

# The SNOPT bridge's referee gate: the source and the hven_sqp_snopt link only when the root-level
# detection succeeded (hven_sqp_snopt carries both the include path and the HVEN_SQP_HAVE_SNOPT
# definition the source additionally guards itself with). Gated by NOT WIN32 like its siblings.
set(_hven_bench_snopt OFF)
if(HVEN_SQP_HAVE_SNOPT AND NOT WIN32)
    set(_hven_bench_snopt ON)
    list(APPEND _hven_bench_sources test_snopt_bridge_gate.cpp)
else()
    hven_test_sources_absent("no SNOPT (or Windows)" test_snopt_bridge_gate.cpp)
endif()

hven_add_test_executable(hven_bench_tests
    SOURCES ${_hven_bench_sources}
    INCLUDE_BENCH
    EXTRA_SOURCES ${_hven_bench_extra_sources}
    DEFINITIONS ${_hven_bench_defs})
if(_hven_bench_snopt)
    target_link_libraries(hven_bench_tests PRIVATE hven_sqp_snopt)
endif()
