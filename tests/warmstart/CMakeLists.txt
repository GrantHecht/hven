################################################################################
# warmstart/ tests: hven_warmstart_tests: the warm-start currency (value types, byte form, round
# trip and refusals), the IPM polish extension, the mesh transfer, the crossover protocol and the
# SQP warm currency. ScaleF7Slow suites are excluded from ctest REGISTRATION (see
# test_warm_start_battery.cpp's banner); they are ordinary gtests, run directly with
#   ./tests/warmstart/hven_warmstart_tests --gtest_filter='ScaleF7Slow.*'
# in the phase-gate Debug sweep and in Release CI runs of the M3 gate.
# Flags: the project's level.
################################################################################

set(_hven_warmstart_sources
    test_ipm_polish_extension.cpp
    test_warm_protocol.cpp
    test_warm_start_data.cpp
)
list(APPEND _hven_warmstart_sources
    test_mesh_transfer.cpp
    test_sqp_warm_currency.cpp
    test_warm_start.cpp
    test_warm_start_battery.cpp
)

hven_add_test_executable(hven_warmstart_tests
    SOURCES ${_hven_warmstart_sources}
    TEST_FILTER "-ScaleF7Slow.*")
