################################################################################
# model/ tests: hven_model_tests: the provider-side contract types in include/hven/model/ that a
# consumer meets before any engine is in the picture (claim arenas and their published tables, the
# declaration and its boundary validation, the structural key, the structure epoch's ordering and
# failure-restore rules, the identity probe, the request flags), the NlpModel bridge (the
# contract's second provider, engine-free in the same sense), the function-entry seam's adapters,
# and the engine problem forms. The HS transcriptions the bridge's equivalence pins consume are in
# model/support/ (header-only test support), named from the tests root.
# Flags: RELEASE_O1 (the directory rule in tests/CMakeLists.txt); no unity batching.
################################################################################

set(_hven_model_sources
    support/fake_aggregate.cpp
    test_aggregate_contract.cpp
    test_assembly_allocation.cpp
    test_claim_stream_restatement.cpp
    test_claim_stream_state.cpp
    test_contract_flags.cpp
    test_identity_probe.cpp
    test_local_block_include.cpp
    test_model_structure_key.cpp
    test_nlp_adapter.cpp
    test_nlp_assembly_contract.cpp
    test_nlp_model_assembly.cpp
    test_nlp_problem_model.cpp
    test_pieces.cpp
    test_problem.cpp
    test_solver_interface_adapter.cpp
    test_string_piece.cpp
    test_structure_epoch.cpp
)
hven_sources_not_on_windows(_hven_model_sources
    test_nlp_model.cpp
    test_parametric_families.cpp
    test_scale_problems.cpp
)

hven_add_test_executable(hven_model_tests
    SOURCES ${_hven_model_sources}
    RELEASE_O1
    TEST_FILTER "-ScaleF7Slow.*")

################################################################################
# Compile-fail probes for the function-entry seam
# (include/hven/model/solver_interface_adapter.h).
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
    no_constraint_surface
    constraint_hook_as_objective
    local_jacobian_pattern_out_of_range
    local_hessian_pattern_upper
    local_nonconst_value
    local_constraint_hessian_signature
    local_outputs_on_objective
    local_hook_ambiguous
    local_not_copyable
    local_nonconst_name
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
        ${HVEN_TESTS_INCLUDE_ROOT})
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
set(_hven_probe_regex_no_constraint_surface   "hven constraint adapter:")
set(_hven_probe_regex_constraint_hook_as_objective "hven objective adapter:")
set(_hven_probe_regex_local_jacobian_pattern_out_of_range "hven local block:")
set(_hven_probe_regex_local_hessian_pattern_upper "hven local block:")
set(_hven_probe_regex_local_nonconst_value "hven local block: value.v, out. must be a const member")
set(_hven_probe_regex_local_constraint_hessian_signature "hven local block: a constraint piece's hessian takes the multipliers")
set(_hven_probe_regex_local_outputs_on_objective "hven local block: this piece declares kOutputs, which makes it a constraint piece")
set(_hven_probe_regex_local_hook_ambiguous "hven local block: this type is a local-block piece AND carries a")
set(_hven_probe_regex_local_not_copyable "hven local block: a piece must be copy-constructible")
set(_hven_probe_regex_local_nonconst_name "hven local block: name.. must be a const member")
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

################################################################################
# DEPRECATION PROBES (M6 W5 T3).
#
# The six by-value NlpModel evaluations carry [[deprecated]]. That attribute is
# the whole migration notice, and a warning nobody looks at is not a pin -- so
# each one gets a probe TU that calls it under -Werror=deprecated-declarations
# and must fail to build. The flag is per-target: the shared COMPILE_FLAGS must
# not carry it, because the tree still calls the by-value forms on purpose in
# nlp_model.h's own defaults and in every test that keeps a by-value oracle.
#
# Each probe TU also calls the IN-PLACE replacement, so the forbidden pattern
# below ("<name>_in_place' is deprecated") makes "the replacement is not itself
# deprecated" a tested claim rather than an assumption, and EXPECT_SINGLE_ERROR
# makes "the by-value call is the ONLY error" one too -- which is also how the
# scoped suppression inside nlp_model.h's defaults is proven to work: those
# bodies call the deprecated forms in the same TU and must stay silent.
################################################################################
set(_hven_deprecation_probes
    deprecated_eval_grad
    deprecated_eval_ce
    deprecated_eval_ci
    deprecated_eval_hess
    deprecated_eval_jac_e
    deprecated_eval_jac_i
)
set(_hven_probe_regex_deprecated_eval_grad  "'eval_grad' is deprecated")
set(_hven_probe_regex_deprecated_eval_ce    "'eval_ce' is deprecated")
set(_hven_probe_regex_deprecated_eval_ci    "'eval_ci' is deprecated")
set(_hven_probe_regex_deprecated_eval_hess  "'eval_hess' is deprecated")
set(_hven_probe_regex_deprecated_eval_jac_e "'eval_jac_e' is deprecated")
set(_hven_probe_regex_deprecated_eval_jac_i "'eval_jac_i' is deprecated")
set(_hven_probe_forbid_deprecated_eval_grad  "eval_grad_in_place' is deprecated")
set(_hven_probe_forbid_deprecated_eval_ce    "eval_ce_in_place' is deprecated")
set(_hven_probe_forbid_deprecated_eval_ci    "eval_ci_in_place' is deprecated")
set(_hven_probe_forbid_deprecated_eval_hess  "eval_hess_in_place' is deprecated")
set(_hven_probe_forbid_deprecated_eval_jac_e "eval_jac_e_in_place' is deprecated")
set(_hven_probe_forbid_deprecated_eval_jac_i "eval_jac_i_in_place' is deprecated")

foreach(_probe ${_hven_deprecation_probes})
    add_library(hven_compile_fail_${_probe} OBJECT EXCLUDE_FROM_ALL
        compile_fail/${_probe}.cpp)
    target_link_libraries(hven_compile_fail_${_probe} PRIVATE hven::hven)
    target_compile_options(hven_compile_fail_${_probe} PRIVATE
        ${COMPILE_FLAGS} -Werror=deprecated-declarations)
    add_test(NAME hven_compile_fail_${_probe}
        COMMAND ${CMAKE_COMMAND}
            -DBINARY_DIR=${CMAKE_BINARY_DIR}
            -DTARGET=hven_compile_fail_${_probe}
            -DCONFIG=$<CONFIG>
            "-DEXPECTED_REGEX=${_hven_probe_regex_${_probe}}"
            "-DFORBIDDEN_REGEX=${_hven_probe_forbid_${_probe}}"
            -DEXPECT_SINGLE_ERROR=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/run_build_must_fail.cmake)
    set_tests_properties(hven_compile_fail_${_probe} PROPERTIES
        RESOURCE_LOCK hven_compile_fail_nested_build)
endforeach()
