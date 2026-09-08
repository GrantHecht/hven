################################################################################
# The SQP engine test suite — migrated whole at M3 (phase A) from the engine's
# pre-migration sandbox at its archived tag. Same test names, same GoogleTest
# fixtures; the identity collapse (namespace, include paths, macro prefixes)
# is the only rewrite. A separate executable, not folded into hven_tests, for
# the same reasons hven_interior_tests is one: it mirrors the origin suite's
# own separately-buildable binary and keeps the migration diff reviewable on
# its own. googletest is already fetched by the parent tests/CMakeLists.txt.
#
# COMPILE FLAGS: ${COMPILE_FLAGS}, like every other target in this repository
# (the owner's uniform-flags ruling, CLAUDE.md §7, executed at phase-C U0).
# The origin-regime exception that used to live here — plain per-config
# defaults, matching the flags every pre-U0 pinned value was derived under —
# was retired by U0's declared mass re-derivation event; the pins this suite
# asserts are derived under the unified regime and the pre-U0 artifacts are
# frozen history. See docs/notes/2026-08-16-m3-u0-design.md.
################################################################################

# Sorted, like every other source list in this tree (tests/interior,
# tests/model, and src/'s own SQP group). The migration order this list once
# carried gave a new entry no defined home, and the last one was appended for
# want of one.
set(HVEN_SQP_TEST_SOURCES
    test_accelerate_probes.cpp
    test_activity_telemetry.cpp
    test_aggregate_eval_seam.cpp
    test_b1_gate.cpp
    test_bench_dump.cpp
    test_border_ops.cpp
    test_continuation.cpp
    test_corpus_cells.cpp
    test_crossover_legs.cpp
    test_dense_oracle.cpp
    test_eqp_refine_ab.cpp
    test_eqp_solve.cpp
    test_funnel.cpp
    test_hs_battery.cpp
    test_hs_sweeps.cpp
    test_ipqp_acceptance.cpp
    test_ipqp_certification.cpp
    test_ipqp_counters.cpp
    test_ipqp_dispatch.cpp
    test_ipqp_engine.cpp
    test_ipqp_kkt_layout.cpp
    test_ipqp_math.cpp
    test_ipqp_options.cpp
    test_ipqp_trace.cpp
    test_ipqp_warm_restart.cpp
    test_kkt_assembly.cpp
    test_kkt_calls.cpp
    test_kkt_inertia_probe.cpp
    test_kkt_partial_solve.cpp
    test_ledger.cpp
    test_mesh_transfer.cpp
    test_nlp_kkt_check.cpp
    test_nlp_model.cpp
    test_parametric_families.cpp
    test_predictor.cpp
    test_problem_scaling.cpp
    test_qp_engine.cpp
    test_qp_engine_border.cpp
    test_qp_engine_indefinite.cpp
    test_qp_engine_tr.cpp
    test_qp_mode_sites.cpp
    test_qp_warm_start.cpp
    test_scaffold.cpp
    test_scale_problems.cpp
    test_scale_smoke.cpp
    test_schur.cpp
    test_sqp_driver.cpp
    test_sqp_restoration.cpp
    test_sqp_warm_currency.cpp
    test_ssn_engine.cpp
    test_trace_writer.cpp
    test_types.cpp
    test_warm_start.cpp
    test_warm_start_battery.cpp
)

add_executable(hven_sqp_tests ${HVEN_SQP_TEST_SOURCES})
target_link_libraries(hven_sqp_tests PRIVATE hven::hven GTest::gtest_main)
target_compile_options(hven_sqp_tests PRIVATE ${COMPILE_FLAGS})

# test_corpus_cells.cpp includes "../../bench/corpus_cells.h" by relative path
# (one implementation shared by the measurement binary and the assertion that
# guards it, never a second copy) — a quoted include resolves relative to the
# INCLUDING FILE, so that one needs no search path at all. corpus_cells.h
# itself then quote-includes "support/scale_problems.h" relative to ITS OWN
# directory (bench/), which has no support/ subdirectory — so this target
# needs tests/sqp/ itself on the include path, and ONLY that.
target_include_directories(hven_sqp_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# The driver TU test_qp_mode_sites.cpp SCANS (M6 W4 T5). It reads the source
# text -- it does not parse C++ -- and requires every kernel invocation in it to
# be covered by a nearby `qp.mode` emit or to carry an explicit `// trace:`
# marker, so the schema's per-site list cannot go stale as the driver grows call
# sites. Rooted at THIS directory for the same reason HVEN_CORE_INCLUDE_DIR is,
# and absolute so the test does not depend on ctest's working directory; the
# test asserts the file exists and contains the invocations it expects, so a
# wrong path fails loudly rather than scanning nothing.
target_compile_definitions(hven_sqp_tests
    PRIVATE HVEN_SQP_DRIVER_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/../../src/drivers/sqp_driver.cpp")

# The committed walk baseline, so test_corpus_cells.cpp can re-score THE
# ARTIFACT ITSELF through the offline --from-csv path and pin the verdict.
# A path, not a copy: the test reads the file the repository ships.
#
# REPOINTED AT U0 (phase-C flag unification, 2026-08-16, declared — plan §6
# U0(b)3, docs/notes/2026-08-16-m3-u0-design.md): the baseline of record is
# the U0 re-derivation (two full-57 runs, byte-agreed on every asserted
# column). The pre-U0 baseline stays frozen at its old path as historical
# evidence, still cited by the gate-A/gate-B records and still read by the
# frozen-vs-frozen continuity tests through the PRE_U0 define below.
target_compile_definitions(hven_sqp_tests PRIVATE
    HVEN_SQP_CORPUS_BASELINE_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv"
    HVEN_SQP_PRE_U0_WALK_BASELINE_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../bench/baselines/2026-08-06-corpus/walk_baseline.csv")

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

# M6 W1 T9 fix round 1: the DECLARED schema-76 kIpm baseline over the 27-cell
# U0 replay set — the artifact the two path_warm acceptance rows are pinned in,
# and the one the replay's ipm arm compares all 76 columns against.
# T10b RE-POINTS IT at the measured `ipqp_init_mu`'s own dated dir; the T9 one
# stays on disk, unrewritten, as the 0.1 placeholder's record.
target_compile_definitions(hven_sqp_tests PRIVATE
    HVEN_SQP_IPM_BASELINE_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../bench/baselines/2026-09-02-t10b-ipm/ipm_baseline.csv")

# M6 W5 T8.1: the TOP-LEVEL interior-point leg's baseline (--engine interior,
# 19 columns, one row per cell per fixed-variable treatment). Pinned here for
# the reason every baseline above is: the artifact and the gate that reads it
# hold each other in place. The corpus's own --from-csv reader cannot read this
# schema, so the gate reads the file directly.
target_compile_definitions(hven_sqp_tests PRIVATE
    HVEN_SQP_INTERIOR_BASELINE_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv")

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
# The A4 record test reads the COMMITTED gate CSV rather than re-running the
# 40-minute sweep, so it needs the artifact's path at compile time (the same
# device HVEN_SQP_CORPUS_BINARY uses for the corpus subprocess tests).
# T10b adds the SECOND record: the same 29 cells at the measured `ipqp_init_mu`.
# The shipped-defaults CSV above is NOT regenerated -- it is the placeholder's
# record and a pin is never rewritten (CLAUDE.md section 7).
target_compile_definitions(hven_sqp_tests PRIVATE
    HVEN_A4_GATE_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-shipped-defaults.csv"
    HVEN_A4_GATE_MU1E2_CSV="${CMAKE_CURRENT_SOURCE_DIR}/../../docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv")

gtest_discover_tests(hven_sqp_tests
    TEST_FILTER "-ScaleF7Slow.*"
    DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})

# Continuation.ProbeBudgetBoundsAFailingProposal drives F7CollocationChain(100,
# ...) through four arms with a cap-pinned QP (1002 minors on the failing
# proposal) and takes ~780 s in Debug (R6 report, task-c-r6-report.md §4). The
# default ctest timeout (600 s, the CI Release lane's value) is too small for
# this one Debug test and produces a spurious Timeout. gtest_discover_tests()
# tests do not exist at CMake-configure time (they are added by a generated
# script included via the directory's TEST_INCLUDE_FILES property at ctest
# time), so a plain set_tests_properties() call here would fail to find the
# test. Append a second include file, appended after gtest_discover_tests()'s
# own, that raises just this one test's TIMEOUT once it has been defined.
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/continuation_probe_budget_timeout.cmake")
