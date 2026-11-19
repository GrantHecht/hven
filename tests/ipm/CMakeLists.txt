################################################################################
# ipm/ tests: hven_ipm_tests: the interior-point engine's own contract (acceptance, restoration,
# SOC, watchdog, the entry points, the trace, the warm start, the counters and the stop paths).
# Flags: RELEASE_O1 (the directory rule in tests/CMakeLists.txt); no unity batching, so every
# test file includes what it uses.
################################################################################

set(_hven_ipm_sources
    test_acceptance_diagnostics.cpp
    test_acceptance_retraction.cpp
    test_classic_merit_restoration.cpp
    test_filter_acceptance.cpp
    test_funnel_acceptance.cpp
    test_ipm_counters.cpp
    test_ipm_phase_status.cpp
    test_ipm_solver_entry.cpp
    test_ipm_solver_presets.cpp
    test_ipm_stop_reason.cpp
    test_ipm_trace.cpp
    test_ipm_warm_start.cpp
    test_l1_restoration.cpp
    test_merit_rules.cpp
    test_monitored_governor.cpp
    test_nlp_multiplier_seeding.cpp
    test_objective_scale_reporting.cpp
    test_proximal_restoration.cpp
    test_recovery_dispatch_gate.cpp
    test_soc.cpp
    test_stop_recovery.cpp
    test_structure_epoch_gating.cpp
    test_switching_acceptance.cpp
    test_watchdog.cpp
)

hven_add_test_executable(hven_ipm_tests
    SOURCES ${_hven_ipm_sources}
    RELEASE_O1)

################################################################################
# The IPM dense guard's failed re-run, on the dense-attempt and platform-factorize injectors
# (docs/testing.md, "Seam tests"). The seam is in the IPM solver TU, whose transitive closure is the
# whole library, so the executable takes the library from hven_testing_library (shared with the
# qp, solver and sqp seam executables).
################################################################################

hven_add_seam_executable(hven_ipm_seam_tests
    WHOLE_LIBRARY
    SOURCES seam/test_ipm_dense_guard_seams.cpp)
