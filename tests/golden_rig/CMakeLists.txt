################################################################################
# The golden-numerics rig.
#
# Three seams, one set of traces. The native arm is always built; the two
# old-seam arms are built only when their sibling checkout is named, so the
# default build -- and CI -- needs neither of them and still exercises the whole
# harness on the native arms. The two old-seam adapters are TEMPORARY: they are
# deleted once both engine migrations close, at which point the traces stay as
# regression tests against the native arm.
#
# Targets:
#   hven_golden_rig          the ctest suite (assert mode)
#   hven_golden_rig_report   the same traces in report mode, dumping
#                            observed-vs-expected in the expected tables' own
#                            CSV format
#   hven_golden_rig_audit    the consumed-surface audit's runtime instrument
#                            and its pre-registered coverage test
################################################################################

set(HVEN_RIG_PSIOPT_SEAM "" CACHE PATH
    "Path to a tycho checkout, enabling the rig's interior-point old-seam arm (TEMPORARY: \
deleted after that engine's migration)")
set(HVEN_RIG_SQP_SEAM "" CACHE PATH
    "Path to a tycho_sqp checkout at the pinned tag, enabling the rig's SQP old-seam arm \
(TEMPORARY: deleted after that engine's migration)")

# The SQP old seam is pinned to a TAG, not to whatever that checkout happens to
# have on its branch: the expected tables derived against it are only meaningful
# against one tree state. Both the tag's commit and the tree state of the
# headers the adapter compiles against are verified below.
#
# The tag is ANNOTATED, so it has two identities and they are different objects.
# The commit is what the pin is about (it names the tree); the tag object's own
# id is recorded beside it because re-cutting the tag onto the same commit
# changes that id and nothing else, which is worth being able to notice.
#
#   tag object : d3bc25315b19237f8d666be82b8768a0e8693e25  (informational)
#   commit     : the value below, verified at configure time
set(HVEN_RIG_SQP_SEAM_TAG "phase-7-close")
set(HVEN_RIG_SQP_SEAM_COMMIT "4faa1df116da53c9dc68f36635c118f52d39d2b9")

find_package(Git QUIET)

# The commit this rig was configured at, stamped into every report row's
# provenance column. Configure-time rather than build-time on purpose: a report
# is read together with the configure that produced it, and a build-time stamp
# would need a reconfigure-on-every-commit rule that costs more than it is
# worth here. Re-run cmake to refresh it.
set(HVEN_RIG_COMMIT "unknown")
if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE HVEN_RIG_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(HVEN_RIG_COMMIT STREQUAL "")
        set(HVEN_RIG_COMMIT "unknown")
    endif()
endif()

################################################################################
# Support objects: the seam abstraction and its adapters, the recipes, the
# expected-table reader and comparison engine, and the shared trace scaffolding.
################################################################################

add_library(hven_golden_rig_support OBJECT
    seam_native.cpp
    seam_registry.cpp
    recipes.cpp
    expected_table.cpp
    trace_support.cpp
)

target_include_directories(hven_golden_rig_support PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
target_include_directories(hven_golden_rig_support PUBLIC ${INCLUDE_DIRS})
target_link_libraries(hven_golden_rig_support PUBLIC
    Eigen3::Eigen fmt::fmt-header-only Threads::Threads GTest::gtest
)
target_compile_options(hven_golden_rig_support PRIVATE ${COMPILE_FLAGS})
target_compile_definitions(hven_golden_rig_support PUBLIC
    HVEN_RIG_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/expected"
    HVEN_RIG_COMMIT="${HVEN_RIG_COMMIT}"
)

# --- the interior-point old-seam arm ------------------------------------------
if(HVEN_RIG_PSIOPT_SEAM)
    set(_psiopt_seam_include "${HVEN_RIG_PSIOPT_SEAM}/psiopt/include")
    if(NOT EXISTS "${_psiopt_seam_include}/tycho/detail/solvers/linear/pardiso_interface.h")
        message(FATAL_ERROR
            "HVEN_RIG_PSIOPT_SEAM=${HVEN_RIG_PSIOPT_SEAM} does not look like a tycho checkout: "
            "expected ${_psiopt_seam_include}/tycho/detail/solvers/linear/pardiso_interface.h")
    endif()
    target_sources(hven_golden_rig_support PRIVATE seam_psiopt.cpp)
    target_include_directories(hven_golden_rig_support SYSTEM PUBLIC "${_psiopt_seam_include}")
    target_compile_definitions(hven_golden_rig_support PUBLIC HVEN_RIG_HAVE_PSIOPT_SEAM=1)
    message(STATUS "golden rig: interior-point old-seam arm enabled (${HVEN_RIG_PSIOPT_SEAM})")
endif()

# --- the SQP old-seam arm, pinned to its tag ----------------------------------
if(HVEN_RIG_SQP_SEAM)
    set(_sqp_seam_include "${HVEN_RIG_SQP_SEAM}/include")
    if(NOT EXISTS "${_sqp_seam_include}/tycho_sqp/kkt_system.h")
        message(FATAL_ERROR
            "HVEN_RIG_SQP_SEAM=${HVEN_RIG_SQP_SEAM} does not look like a tycho_sqp checkout: "
            "expected ${_sqp_seam_include}/tycho_sqp/kkt_system.h")
    endif()

    # Pin verification, in two steps, because they catch different mistakes.
    if(Git_FOUND)
        # 1. The tag resolves to the commit this rig was pinned against. A
        #    moved or re-cut tag is a different tree wearing the same name.
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse "${HVEN_RIG_SQP_SEAM_TAG}^{commit}"
            WORKING_DIRECTORY "${HVEN_RIG_SQP_SEAM}"
            OUTPUT_VARIABLE _sqp_tag_commit
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _sqp_tag_rc
            ERROR_QUIET)
        if(NOT _sqp_tag_rc EQUAL 0)
            message(FATAL_ERROR
                "golden rig: cannot resolve tag '${HVEN_RIG_SQP_SEAM_TAG}' in "
                "${HVEN_RIG_SQP_SEAM}. The SQP old-seam arm is pinned to that tag; without it "
                "the arm would be built against an unknown tree state.")
        endif()
        if(NOT _sqp_tag_commit STREQUAL HVEN_RIG_SQP_SEAM_COMMIT)
            message(FATAL_ERROR
                "golden rig: tag '${HVEN_RIG_SQP_SEAM_TAG}' resolves to ${_sqp_tag_commit}, but "
                "this rig is pinned to ${HVEN_RIG_SQP_SEAM_COMMIT}. Expected tables derived "
                "against the old seam are only meaningful against one tree state -- either "
                "restore the tag or re-derive against the new one and update "
                "HVEN_RIG_SQP_SEAM_COMMIT here.")
        endif()

        # 2. The headers the adapter actually compiles against are byte-
        #    identical to that tag's tree. The checkout is allowed to sit on a
        #    later commit -- read-only, we do not move it -- as long as nothing
        #    the adapter includes has changed since the tag.
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" diff --quiet "${HVEN_RIG_SQP_SEAM_TAG}" --
                    "include/tycho_sqp"
            WORKING_DIRECTORY "${HVEN_RIG_SQP_SEAM}"
            RESULT_VARIABLE _sqp_tree_rc
            ERROR_QUIET)
        if(NOT _sqp_tree_rc EQUAL 0)
            message(FATAL_ERROR
                "golden rig: ${HVEN_RIG_SQP_SEAM}/include/tycho_sqp differs from tag "
                "'${HVEN_RIG_SQP_SEAM_TAG}'. The old-seam arm must compile against the pinned "
                "tree state; check that checkout out at the tag (it is read-only to this rig, "
                "which is why this is reported rather than fixed here).")
        endif()
        message(STATUS
            "golden rig: SQP old-seam arm pinned to ${HVEN_RIG_SQP_SEAM_TAG} "
            "(${HVEN_RIG_SQP_SEAM_COMMIT}), tree state verified")
    else()
        message(WARNING
            "golden rig: git was not found, so the SQP old-seam arm's pin to tag "
            "'${HVEN_RIG_SQP_SEAM_TAG}' (${HVEN_RIG_SQP_SEAM_COMMIT}) COULD NOT BE VERIFIED. "
            "Any expected table derived from this configure carries an unverified provenance.")
    endif()

    target_sources(hven_golden_rig_support PRIVATE seam_sqp.cpp)
    target_include_directories(hven_golden_rig_support SYSTEM PUBLIC "${_sqp_seam_include}")
    target_compile_definitions(hven_golden_rig_support PUBLIC
        HVEN_RIG_HAVE_SQP_SEAM=1
        HVEN_RIG_SQP_SEAM_COMMIT="${HVEN_RIG_SQP_SEAM_COMMIT}"
    )
endif()

################################################################################
# The trace suite, and the same traces in report mode.
################################################################################

set(HVEN_RIG_TRACE_SOURCES
    traces_sqp.cpp
    traces_psiopt.cpp
    # Inverted-expectation guards on the traces that are SUPPOSED to fail on an
    # old seam. Without them, an adapter regressing to smoothing would turn a
    # fail-by-design green and lose the finding silently. Compiled always; the
    # per-seam controls inside are gated on their seam being configured in.
    fail_by_design_control.cpp
)

add_executable(hven_golden_rig ${HVEN_RIG_TRACE_SOURCES})
target_link_libraries(hven_golden_rig PRIVATE
    hven_golden_rig_support hven::hven GTest::gtest_main
)
target_compile_options(hven_golden_rig PRIVATE ${COMPILE_FLAGS})
gtest_discover_tests(hven_golden_rig)

add_executable(hven_golden_rig_report ${HVEN_RIG_TRACE_SOURCES} report_main.cpp)
target_link_libraries(hven_golden_rig_report PRIVATE
    hven_golden_rig_support hven::hven GTest::gtest
)
target_compile_options(hven_golden_rig_report PRIVATE ${COMPILE_FLAGS})

################################################################################
# The consumed-surface audit.
#
# The runtime half is a LINK-LEVEL interposer on the sparse backend's phase
# entry point: --wrap redirects every reference to it -- from hven's own
# session and from either old seam's headers alike -- through a recorder that
# forwards to the real symbol unchanged. That is what lets the audit see a
# touchpoint written inside a private method of a seam it does not own, which
# is exactly the case its pre-registered coverage test is about.
#
# Its own executable, so the --wrap flag (which applies to a whole link) stays
# off the trace suite. Apple has no such entry point, so this target is built
# where the backend is.
################################################################################

if(NOT APPLE)
    add_executable(hven_golden_rig_audit
        audit/pardiso_recorder.cpp
        audit/test_audit_shim.cpp
    )
    target_link_libraries(hven_golden_rig_audit PRIVATE
        hven_golden_rig_support hven::hven GTest::gtest_main
    )
    target_include_directories(hven_golden_rig_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/audit)
    target_compile_options(hven_golden_rig_audit PRIVATE ${COMPILE_FLAGS})
    target_link_options(hven_golden_rig_audit PRIVATE "-Wl,--wrap=pardiso")
    gtest_discover_tests(hven_golden_rig_audit)
endif()

# The static half's self-test. Runs the scanner over a committed sample that
# carries one line of every touchpoint class plus two negative controls, so it
# checks the SCANNER on a machine that has neither sibling checkout -- which is
# what keeps it in ordinary CI rather than only in a derivation run.
add_test(NAME GoldenRigAudit.StaticScanSelfTest
         COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/audit/static_scan.sh" --self-test)
