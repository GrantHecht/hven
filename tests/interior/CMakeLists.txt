################################################################################
# Interior-point engine (ex-PSIOPT) test suite — migrated as-is from
# psiopt/tests/ (hven M2 milestone, engine-move task). Same test names,
# same GoogleTest fixtures; only the include paths were mechanically
# retargeted onto include/hven/.
#
# A separate executable, not folded into hven_tests: it mirrors psiopt's own
# separately-buildable test binary, keeps this migration's diff reviewable on
# its own, and does not disturb hven_tests' existing HVEN_TEST_SOURCES list or
# discovery-mode logic. googletest is already fetched by the parent
# tests/CMakeLists.txt (FetchContent_MakeAvailable(googletest) runs before
# add_subdirectory(interior) below), so this file links GTest::gtest_main
# directly rather than re-declaring the fetch.
################################################################################

set(HVEN_INTERIOR_TEST_SOURCES
    test_acceptance_diagnostics.cpp
    test_assembly_allocation.cpp
    test_classic_merit_restoration.cpp
    test_filter_acceptance.cpp
    test_funnel_acceptance.cpp
    test_jet_mkl_guard.cpp
    test_kkt_factorization.cpp
    test_l1_restoration.cpp
    test_merit_rules.cpp
    test_monitored_governor.cpp
    test_nlp_adapter.cpp
    test_nlp_aggregate_contract.cpp
    test_nlp_multiplier_seeding.cpp
    test_nlp_problem_model.cpp
    test_nlp_solver.cpp
    test_proximal_restoration.cpp
    test_recovery_dispatch_gate.cpp
    test_solver_interface_adapter.cpp
    test_switching_acceptance.cpp
    test_watchdog.cpp
)

add_executable(hven_interior_tests ${HVEN_INTERIOR_TEST_SOURCES})
target_include_directories(hven_interior_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hven_interior_tests PRIVATE hven::hven GTest::gtest_main)
target_compile_options(hven_interior_tests PRIVATE ${COMPILE_FLAGS})

# Unity build -- merge test TUs, same pattern (and batch size) as
# psiopt/tests/CMakeLists.txt's psiopt_tests. This is load-bearing, not just
# a compile-time optimization: test_nlp_adapter.cpp (and others) only
# #include nlp_adapter.h, which forward-declares NonLinearProgram, and rely
# on another test TU in the same unity batch to have already pulled in the
# complete definition (non_linear_program.h) -- exactly as they did in
# psiopt/tests. Splitting this into separate TUs is a source-level fix
# (adding the missing include) that AS-IS migration does not make; carried
# over unchanged, see the migration report.
set_target_properties(hven_interior_tests PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 8
)

# Reduce optimization level for tests to improve compile time, matching
# psiopt/tests/CMakeLists.txt and hven's own test targets.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(hven_interior_tests PRIVATE $<$<CONFIG:Release>:-O1>)
endif()

gtest_discover_tests(hven_interior_tests DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})

################################################################################
# Compile-fail probes for the function-entry seam
# (include/hven/solver_interface_adapter.h).
#
# The seam's whole point is that the wrong type entering an interface is a
# LOUD compile error rather than a silent second virtual dispatch, so the
# refusals are only actually tested by trying to compile them. Each probe is
# a TU that MUST FAIL; ctest passes iff `cmake --build` returns non-zero AND
# the build log carries that probe's expected diagnostic.
#
# The regexes are the authored static_assert message prefixes, which is why
# those prefixes are distinct per route ("hven constraint adapter:" vs "hven
# objective adapter:"): an unrelated build break, or the RIGHT probe failing
# for the WRONG reason, then does not pass as a fake success.
#
# Every probe ALSO forbids the raw member-lookup diagnostic. Presence-only
# checking cannot tell "the seam refused this cleanly" from "the seam refused
# this AND then instantiated the machinery anyway, so the compiler piled raw
# errors on top" -- and the second is what the seam's authored-diagnostics
# promise actually rules out. The forbidden pattern is what makes each probe
# test the promise rather than a substring of it.
#
# Built on every platform: these targets compile hven's headers only, and
# what they prove -- including that a constrained partial specialization of
# the adapter is accepted, which the positive test above covers -- has to
# hold on all three CI frontends, not just the one that happens to be
# convenient.
################################################################################
set(_hven_adapter_probes
    unregistered_constraint
    constraint_only_objective
    partial_objective_surface
    objective_unsupported_mixin
    constraint_unsupported_mixin
    adapter_missing_objective
)
foreach(_probe ${_hven_adapter_probes})
    add_library(hven_compile_fail_${_probe} OBJECT EXCLUDE_FROM_ALL
        compile_fail/${_probe}.cpp)
    target_link_libraries(hven_compile_fail_${_probe} PRIVATE hven::hven)
    # The fixture header these probes share with test_solver_interface_adapter.cpp.
    target_include_directories(hven_compile_fail_${_probe} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(hven_compile_fail_${_probe} PRIVATE ${COMPILE_FLAGS})
endforeach()

# The raw diagnostic every probe must NOT produce: an ERROR-severity member
# lookup failure. That is what instantiating a Model (or an adapter member)
# behind a failed check produces -- before the install bodies guarded their
# emplacements, the constraint-only probe emitted three of these, one per
# objective member the type was missing, right beside the authored message,
# and a presence-only check passed anyway.
#
# The severity anchor is load-bearing, not decoration. Clang also prints
# "no member named" inside NOTES when it explains which requirement of a
# concept was not satisfied ("because 'f.objective_gradient(...)' would be
# invalid: no member named ..."). Those notes are attached to the authored
# assert and explain it; they are the diagnostic working, not leaking. Only
# an `error:` carrying the phrase means something got instantiated that
# should not have.
#
# All three CI frontends are clang-family, so the phrasing holds across them.
# A frontend that worded it differently would make this check a no-op on that
# lane rather than a false failure.
set(_hven_raw_member_diagnostic "error: no member named")

# probe name -> which authored prefix pins it
set(_hven_probe_regex_unregistered_constraint  "hven constraint adapter:")
set(_hven_probe_regex_constraint_only_objective "hven objective adapter:")
set(_hven_probe_regex_partial_objective_surface "hven objective adapter:")
set(_hven_probe_regex_objective_unsupported_mixin "hven objective adapter:")
set(_hven_probe_regex_constraint_unsupported_mixin "hven constraint adapter:")
set(_hven_probe_regex_adapter_missing_objective "hven objective adapter:")

foreach(_probe ${_hven_adapter_probes})
    add_test(NAME hven_compile_fail_${_probe}
        COMMAND ${CMAKE_COMMAND}
            -DBINARY_DIR=${CMAKE_BINARY_DIR}
            -DTARGET=hven_compile_fail_${_probe}
            -DCONFIG=$<CONFIG>
            "-DEXPECTED_REGEX=${_hven_probe_regex_${_probe}}"
            "-DFORBIDDEN_REGEX=${_hven_raw_member_diagnostic}"
            -DEXPECT_SINGLE_ERROR=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/run_build_must_fail.cmake)
    # Each probe drives its own nested `cmake --build` invocation against the
    # shared build directory. Those nested builds all read/write the same
    # .ninja_log (and the same underlying object/dep files for hven::hven),
    # so under `ctest -j` they race each other -- harmless when they happen
    # to interleave cleanly, but capable of corrupting the shared build state
    # or spiking memory on a tight box when several land at once. A single
    # RESOURCE_LOCK shared across all probes in this file makes ctest run
    # them one at a time relative to each other, regardless of -j; they stay
    # free to run concurrently with unrelated (non-nested-build) tests.
    set_tests_properties(hven_compile_fail_${_probe} PROPERTIES
        RESOURCE_LOCK hven_compile_fail_nested_build)
endforeach()
