################################################################################
# The interior-point engine test suite, hven_ipm_tests (RS-6a): the engine's own contract (acceptance, restoration, SOC, watchdog, the entry points, the trace, the warm start, the counters
# and the stop paths). Same test names, same GoogleTest fixtures. A target of its own, separately buildable; googletest is already fetched by the parent tests/CMakeLists.txt.
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

# Compiled at -O1 in Release, as the retired interior/drivers/model targets compiled them
# (compile time; the sources of the former hven_tests and hven_sqp_tests were not).
set(_hven_ipm_o1_sources
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

# Unity build -- merge test TUs in batches of 8, the retired hven_interior_tests' pattern. This is load-bearing, not just a compile-time optimization: test_nlp_adapter.cpp (and others) only
# #include nlp_adapter.h, which forward-declares NonLinearProgram, and rely on another test TU in the same unity batch to have already pulled in the complete definition (non_linear_program.h) --
# exactly as they did before the migration. Splitting this into separate TUs is a source-level fix (adding the missing include) that AS-IS migration does not make. Only the sources the retired
# interior target unity-batched are batched; every other source of the executable is compiled alone.
set(_hven_ipm_unity_sources
    test_acceptance_diagnostics.cpp
    test_acceptance_retraction.cpp
    test_classic_merit_restoration.cpp
    test_filter_acceptance.cpp
    test_funnel_acceptance.cpp
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
    test_structure_epoch_gating.cpp
    test_switching_acceptance.cpp
    test_watchdog.cpp
)

hven_add_test_executable(hven_ipm_tests
    SOURCES ${_hven_ipm_sources}
    O1_SOURCES ${_hven_ipm_o1_sources}
    UNITY_SOURCES ${_hven_ipm_unity_sources})

################################################################################
# The IPM dense guard's failed re-run (DN-1b review F8), on the dense-attempt and platform-factorize injectors (docs/testing.md; HVEN_TESTING). The seam is in the IPM solver TU, whose transitive
# closure is the whole library, so this executable takes the library from hven_testing_library (every source of the hven target compiled once with HVEN_TESTING, shared by the qp, solver, ipm and sqp seam executables) and does not link hven::hven.
################################################################################

hven_add_seam_executable(hven_ipm_seam_tests
    WHOLE_LIBRARY
    SOURCES test_ipm_dense_guard_seams.cpp)
