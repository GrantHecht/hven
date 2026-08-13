################################################################################
# The SQP engine test suite — migrated whole at M3 (phase A) from the engine's
# pre-migration sandbox at its archived tag. Same test names, same GoogleTest
# fixtures; the identity collapse (namespace, include paths, macro prefixes)
# is the only rewrite. A separate executable, not folded into hven_tests, for
# the same reasons hven_interior_tests is one: it mirrors the origin suite's
# own separately-buildable binary and keeps the migration diff reviewable on
# its own. googletest is already fetched by the parent tests/CMakeLists.txt.
#
# COMPILE FLAGS — deliberately NOT ${COMPILE_FLAGS}, unlike every other test
# target in this repository. The origin sandbox compiled this suite with plain
# per-config defaults (-O3/-DNDEBUG Release, -g Debug; no -march=native, no
# inline-threshold override, no -fomit-frame-pointer), and every pinned value
# the M3 gate asserts — the measurement pins in these tests, the 57-cell walk
# census baseline, the two gate-battery artifacts — was derived under those
# flags. Phase A's marching orders are "compile flags matched", so this target
# reproduces the origin regime; the repo-wide directory-scope definitions
# (EIGEN_INITIALIZE_MATRICES_BY_ZERO, EIGEN_DONT_PARALLELIZE, FMT_USE_LOCALE=0,
# and the Apple sparse-backend trio) still apply, exactly as they already do to
# the golden rig's old-seam arm that compiles this same engine. Whether the
# suite moves onto hven's unified COMPILE_FLAGS regime is an execution-review
# question (any pin that moves under the unified flags is a declared
# re-derivation, never a silent one), revisited no later than the phase-C TU
# split, which compiles engine TUs into the library under library flags.
################################################################################

set(HVEN_SQP_TEST_SOURCES
    test_scaffold.cpp
    test_types.cpp
    test_kkt_system.cpp
    test_kkt_inertia_probe.cpp
    test_kkt_partial_solve.cpp
    test_dense_oracle.cpp
    test_kkt_assembly.cpp
    test_eqp_solve.cpp
    test_schur.cpp
    test_border_ops.cpp
    test_qp_engine.cpp
    test_qp_engine_border.cpp
    test_qp_engine_indefinite.cpp
    test_qp_engine_tr.cpp
    test_qp_warm_start.cpp
    test_ledger.cpp
    test_scale_smoke.cpp
    test_nlp_model.cpp
    test_sqp_driver.cpp
    test_sqp_restoration.cpp
    test_hs_battery.cpp
    test_hs_sweeps.cpp
    test_b1_gate.cpp
    test_eqp_refine_ab.cpp
    test_funnel.cpp
    test_warm_start.cpp
    test_parametric_families.cpp
    test_predictor.cpp
    test_continuation.cpp
    test_mesh_transfer.cpp
    test_warm_start_battery.cpp
    test_scale_problems.cpp
    test_accelerate_probes.cpp
    test_bench_dump.cpp
    test_corpus_cells.cpp
    test_ssn_engine.cpp
)

add_executable(hven_sqp_tests ${HVEN_SQP_TEST_SOURCES})
target_link_libraries(hven_sqp_tests PRIVATE hven::hven GTest::gtest_main)

# test_corpus_cells.cpp includes "../../bench/corpus_cells.h" by relative path
# (one implementation shared by the measurement binary and the assertion that
# guards it, never a second copy) — a quoted include resolves relative to the
# INCLUDING FILE, so that one needs no search path at all. corpus_cells.h
# itself then quote-includes "support/scale_problems.h" relative to ITS OWN
# directory (bench/), which has no support/ subdirectory — so this target
# needs tests/sqp/ itself on the include path, and ONLY that.
target_include_directories(hven_sqp_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# The committed walk baseline, so test_corpus_cells.cpp can re-score THE
# ARTIFACT ITSELF through the offline --from-csv path and pin the verdict.
# A path, not a copy: the test reads the file the repository ships (the five
# artifacts migrated into bench/baselines/ at M3 phase A, byte-identical to
# the archived tag's copies).
target_compile_definitions(hven_sqp_tests PRIVATE
    HVEN_SQP_CORPUS_BASELINE_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../bench/baselines/2026-08-06-corpus/walk_baseline.csv")

# The gate battery's own two artifacts, pinned the same way and for the same
# reason: the kSsn arm IS the phase-7 verdict, and a verdict that lives only
# in a note rots. `--from-csv` re-scores both offline in milliseconds, so the
# artifact and the evaluator hold each other in place — if either moves, the
# pins fail and whoever moved it has to re-sweep rather than re-label.
target_compile_definitions(hven_sqp_tests PRIVATE
    HVEN_SQP_SSN_BATTERY_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../bench/baselines/2026-08-08-gate-battery/ssn_battery.csv"
    HVEN_SQP_WALK_RESWEPT_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../bench/baselines/2026-08-08-gate-battery/walk_reswept.csv")

# The post-D0-repair re-sweep of BOTH arms, pinned for a third reason: the
# repair's whole claim is that it moves the four crashing cells and NOTHING
# ELSE, and that claim is a DIFF between two artifacts. These two are CSV
# schema 37 (they carry the escape-reason census); the three above are schema
# 14/31 and are deliberately NOT regenerated, which is what the reader's
# optional-tail contract exists for.
target_compile_definitions(hven_sqp_tests PRIVATE
    HVEN_SQP_TASK6B_SSN_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../bench/baselines/2026-08-09-task6b-resweep/ssn_resweep.csv"
    HVEN_SQP_TASK6B_WALK_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../bench/baselines/2026-08-09-task6b-resweep-walk/walk_resweep.csv")

# The SNOPT bridge's referee gate, added to the test binary ONLY when the
# root-level SNOPT detection succeeded (see the block above the tests
# subdirectory in the root CMakeLists.txt for why the detection lives there).
# hven_sqp_snopt carries both the include path and the HVEN_SQP_HAVE_SNOPT
# definition the source additionally guards itself with.
#
# The suite size is therefore a function of the machine, deliberately:
# WITHOUT SNOPT the count is unchanged, and WITH it the gate's FOUR tests are
# added on top. Nothing else in the binary changes in either configuration.
# The gate includes ../../bench/snopt_f7_driver.h by relative path — one
# implementation of the bridge, shared by the measurement binary and the
# assertion that guards it, rather than a second copy that could drift; that
# header's own "support/scale_problems.h" quoted include resolves through
# this target's tests/sqp include path above.
if(HVEN_SQP_HAVE_SNOPT)
    target_sources(hven_sqp_tests PRIVATE test_snopt_bridge_gate.cpp)
    target_link_libraries(hven_sqp_tests PRIVATE hven_sqp_snopt)
endif()

# TEST_FILTER excludes the ScaleF7Slow suite from CTest REGISTRATION, so the
# per-commit `ctest` run never executes it. The tests themselves are built and
# are ordinary gtests; run them directly with
#
#     ./tests/sqp/hven_sqp_tests --gtest_filter='ScaleF7Slow.*'
#
# WHEN THEY MUST RUN: in the phase-gate Debug sweep and in Release CI runs of
# the M3 gate — the exclusion is from the per-commit cadence, not from the
# phase gate. See the banners in test_scale_problems.cpp, test_scale_smoke.cpp
# and test_warm_start_battery.cpp for each member's measured cost and why a
# per-commit-excluded assertion is one nobody watches by default.
gtest_discover_tests(hven_sqp_tests
    TEST_FILTER "-ScaleF7Slow.*"
    DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})
