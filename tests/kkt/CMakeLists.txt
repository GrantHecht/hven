################################################################################
# kkt/ tests: hven_kkt_tests (RS-6a), plus the directory's seam executable.
# test_kkt_factorization was unity-batched in the retired interior target; alone in this executable it is the plain translation unit (a unity batch of one), so the executable is built non-unity.
################################################################################

set(_hven_kkt_sources
    test_kkt_factorization.cpp
)

# The former hven_sqp_tests sources: UNIX-only (its subprocess wall-deadline tests drive the
# corpus runner, whose deadline enforcement is fork/exec orchestration, and the suite has never
# been built or derived on Windows; RS-6b is the stage that lifts the gate).
set(_hven_kkt_unix_sources
    test_border_ops.cpp
    test_kkt_assembly.cpp
    test_kkt_calls.cpp
    test_kkt_inertia_probe.cpp
    test_kkt_partial_solve.cpp
    test_schur.cpp
)
if(NOT WIN32)
    list(APPEND _hven_kkt_sources ${_hven_kkt_unix_sources})
else()
    hven_test_sources_absent("Windows" ${_hven_kkt_unix_sources})
endif()

# Compiled at -O1 in Release, as the retired interior/drivers/model targets compiled them
# (compile time; the sources of the former hven_tests and hven_sqp_tests were not).
set(_hven_kkt_o1_sources
    test_kkt_factorization.cpp
)

hven_add_test_executable(hven_kkt_tests
    SOURCES ${_hven_kkt_sources}
    O1_SOURCES ${_hven_kkt_o1_sources})

################################################################################
# The interior-point engine's KKT factorization rides the backend fault seam (linear/): its evidence projection on a failed factorization, and its record-the-status-and-continue behavior on a failed
# symbolic analysis, are both backend-specific and both unreachable from any fixture. The TU depends on nothing but the linear surface's public header, so it compiles here as cheaply as it does into the library.
# A standalone HVEN_TESTING executable on the same terms as hven_linear_seam_tests; it does not link hven::hven.
################################################################################

hven_add_seam_executable(hven_kkt_seam_tests
    SOURCES test_kkt_factorization_faults.cpp
    LIBRARY_SOURCES
        ${HVEN_SRC_DIR}/core/pattern_hash.cpp
        ${HVEN_SRC_DIR}/kkt/kkt_factorization.cpp)
