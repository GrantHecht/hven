################################################################################
# bench/ tests: hven_bench_tests (RS-6a) -- the tests that pin a bench/*.h instrument or drive the
# measurement binary: the corpus cells and runner (test_corpus_cells), the bench dump CLI
# (test_bench_dump), the crossover legs (test_crossover_legs), the SNOPT bridge gate (only when
# the root-level SNOPT detection succeeded) and the model-surface KKT instrument
# (test_model_surface_kkt, built on every platform).
#
# bench/ipm_corpus_leg.cpp is COMPILED INTO this binary, on the same reasoning corpus_cells.h is
# quote-included into test_corpus_cells.cpp -- one implementation shared by the measurement binary
# and the assertion that guards it, never a second copy. The single-row mode's pin is a statement
# about two of the leg's OWN FUNCTIONS agreeing (the routing and the row builder), which only an
# in-process call can make. It is an EXTRA source: a path out of the tree has no place in the list
# of this directory's own files.
#
# The wall-deadline tests in test_corpus_cells.cpp (CorpusRunnerProcess.WallDeadline*) are SUBPROCESS
# tests that invoke the compiled hven_sqp_corpus binary; the dependency and the
# HVEN_SQP_CORPUS_BINARY path macro are wired in bench/CMakeLists.txt, where hven_sqp_corpus exists.
#
# The committed baselines the cells test re-scores through the offline --from-csv path are named by
# path, not copied: the artifact and the evaluator hold each other in place -- if either moves, the
# pins fail and whoever moved it has to re-sweep rather than re-label. History of the pointers:
#   * REPOINTED AT U0 (phase-C flag unification, 2026-08-16, declared): the baseline of record is the
#     U0 re-derivation; the pre-U0 baseline stays frozen at its old path, still read by the
#     frozen-vs-frozen continuity tests through the PRE_U0 define.
#   * The gate battery's two artifacts (the kSsn arm IS the phase-7 verdict) and the post-D0-repair
#     re-sweep of BOTH arms (CSV schema 37; the three above are schema 14/31 and deliberately NOT
#     regenerated, which the reader's optional-tail contract exists for).
#   * M6 W1 T9 fix round 1: the DECLARED schema-76 kIpqp baseline over the 27-cell U0 replay set,
#     re-pointed by T10b at the measured ipqp_init_mu's own dated dir.
#   * M6 W5 T8.1: the TOP-LEVEL interior-point leg's baseline (--engine interior, 19 columns); M8 B7
#     (C238 (1)) re-pointed it at the file re-derived at the absent-bound threshold's default; M8
#     AP-5 G1 re-pointed it at the file re-derived through the declared stop-reason column map; the
#     B7 and flip files are frozen.
# The SNOPT bridge's referee gate adds FOUR tests ONLY when SNOPT was detected, so the suite size is a
# function of the machine, deliberately; the gate includes ../../bench/snopt_f7_driver.h by relative
# path (one implementation of the bridge, shared by the measurement binary and the assertion).
################################################################################

set(_hven_bench_sources
    test_model_surface_kkt.cpp
)

# test_model_surface_kkt was a hven_model_tests source (built everywhere, -O1). The rest were
# hven_sqp_tests sources: UNIX-only.
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
    list(APPEND _hven_bench_extra_sources "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ipm_corpus_leg.cpp")
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
    hven_test_sources_absent("Windows" ${_hven_bench_unix_sources})
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
    O1_SOURCES test_model_surface_kkt.cpp
    EXTRA_SOURCES ${_hven_bench_extra_sources}
    DEFINITIONS ${_hven_bench_defs})
if(_hven_bench_snopt)
    target_link_libraries(hven_bench_tests PRIVATE hven_sqp_snopt)
endif()
