################################################################################
# kkt/ tests: hven_kkt_tests and the directory's seam executable.
# Flags: the project's level.
################################################################################

set(_hven_kkt_sources
    test_kkt_factorization.cpp
)
list(APPEND _hven_kkt_sources
    test_border_ops.cpp
    test_kkt_assembly.cpp
    test_kkt_factor.cpp
    test_kkt_inertia_probe.cpp
    test_kkt_partial_solve.cpp
    test_schur.cpp
)

hven_add_test_executable(hven_kkt_tests
    SOURCES ${_hven_kkt_sources})

################################################################################
# The interior-point KKT factorization rides the backend fault seam (linear/): its evidence
# projection on a failed factorization and its record-the-status-and-continue behaviour on a
# failed symbolic analysis are backend-specific and unreachable from any fixture. The TUs it needs
# depend on nothing but the linear surface's public header, so a narrow seam (two library TUs plus
# the platform backend) is the cheap form; same terms as hven_linear_seam_tests.
################################################################################

hven_add_seam_executable(hven_kkt_seam_tests
    SOURCES seam/test_kkt_factorization_faults.cpp
    LIBRARY_SOURCES
        ${HVEN_SRC_DIR}/core/pattern_hash.cpp
        ${HVEN_SRC_DIR}/kkt/kkt_factorization.cpp)
