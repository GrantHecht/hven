################################################################################
# warmstart/ tests: hven_warmstart_tests (RS-6a): the warm-start currency (value types, byte form, round trip and refusals), the IPM polish extension, the mesh transfer, the crossover
# protocol and the SQP warm currency. ScaleF7Slow suites are excluded from ctest REGISTRATION (test_warm_start_battery.cpp carries one); they are ordinary gtests, run directly with
# ./tests/warmstart/hven_warmstart_tests --gtest_filter='ScaleF7Slow.*' in the phase-gate Debug sweep and in Release CI runs of the M3 gate -- see the banner in test_warm_start_battery.cpp.
################################################################################

set(_hven_warmstart_sources
    test_ipm_polish_extension.cpp
    test_warm_protocol.cpp
    test_warm_start_data.cpp
)

# The former hven_sqp_tests sources: UNIX-only (its subprocess wall-deadline tests drive the
# corpus runner, whose deadline enforcement is fork/exec orchestration, and the suite has never
# been built or derived on Windows; RS-6b is the stage that lifts the gate).
set(_hven_warmstart_unix_sources
    test_mesh_transfer.cpp
    test_sqp_warm_currency.cpp
    test_warm_start.cpp
    test_warm_start_battery.cpp
)
if(NOT WIN32)
    list(APPEND _hven_warmstart_sources ${_hven_warmstart_unix_sources})
else()
    hven_test_sources_absent("Windows" ${_hven_warmstart_unix_sources})
endif()

# Compiled at -O1 in Release, as the retired interior/drivers/model targets compiled them
# (compile time; the sources of the former hven_tests and hven_sqp_tests were not).
set(_hven_warmstart_o1_sources
    test_warm_protocol.cpp
)

hven_add_test_executable(hven_warmstart_tests
    SOURCES ${_hven_warmstart_sources}
    O1_SOURCES ${_hven_warmstart_o1_sources}
    TEST_FILTER "-ScaleF7Slow.*")
