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

# The escape hatch for the psiopt pin below, and nothing else. Its default is
# OFF, which is the only setting a derivation run may ever use: derived
# expected tables are only meaningful against the exact tree the pin names, so
# a run that produces them must refuse to proceed on an unverifiable or
# drifted checkout rather than warn and continue. This variable exists for the
# other case -- iterating against a moving tycho checkout without deriving new
# tables -- where refusing to configure at all would be a worse failure mode
# than a loud warning.
option(HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM
    "Downgrade the interior-point old-seam pin/dirty/verifiability checks below from \
FATAL_ERROR to WARNING. NEVER set this for a run that derives psiopt-old expected tables -- \
only for iterating against a moving tycho checkout." OFF)

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

# The interior-point old seam is pinned the same way in spirit as the SQP
# seam above, and -- as of the FATAL enforcement below -- the same way in
# strength too: HVEN_RIG_PSIOPT_SEAM names a live tycho checkout (this
# project's main development sibling) rather than a checkout frozen at a
# released tag, so unlike HVEN_RIG_SQP_SEAM there is no tag to re-resolve --
# only a commit and a tree to check. But "expected to keep moving as tycho
# develops" describes when the pin needs updating, not whether an unverified
# or drifted checkout is safe to derive expectations from: it is not, so a
# mismatch, a dirty consumed tree, or an unverifiable checkout (no git, or no
# .git directory) is a configure-time FATAL_ERROR by default, same as the SQP
# seam's checks. HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM=ON (above) downgrades
# that to the previous WARNING-and-proceed behavior for the one case that
# legitimately wants it -- iterating against a moving tycho checkout without
# deriving new tables -- and re-deriving the psiopt-old expected tables is the
# actual remedy when the drift touches Pardiso/Accelerate configuration or
# linear-algebra behavior (psiopt/include/tycho/detail/solvers/linear/).
#
#   commit : the value below, checked at configure time when the checkout
#            is a git repository; both the commit AND the cleanliness of the
#            consumed linear/ tree are checked -- see the verification block
#            below for why a HEAD match alone is not enough.
#
#   Re-pinned 2026-08-13 (hven M2.5, Task 2) from 060ad2138a373b... to tycho
#   main's current HEAD below. This was NOT a blind bump: before moving the
#   pin, `git diff 060ad2138a373bacd9d1ba2daabd6b503a9a6fb1..main --stat --
#   psiopt/` was run against the tycho checkout and returned EMPTY -- the
#   frozen psiopt/ tree (including the consumed linear/ headers this pin's
#   expected tables were derived against) is byte-identical between the old
#   pin and this new one, so no re-derivation of the psiopt-old expected
#   tables was needed. Re-pinning past a commit range with a non-empty diff
#   there would require re-deriving those tables first.
set(HVEN_RIG_PSIOPT_SEAM_COMMIT "5b5dc736c238b285915507c3677d7952227c0d1b")

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
        # hven's own root, reached relatively -- see the note on HVEN_SRC_DIR
        # in tests/CMakeLists.txt for why not ${CMAKE_SOURCE_DIR}. Here a
        # wrong root fails quietly rather than loudly: a parent-project build
        # would stamp every report row with the PARENT's commit.
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
        OUTPUT_VARIABLE HVEN_RIG_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(HVEN_RIG_COMMIT STREQUAL "")
        set(HVEN_RIG_COMMIT "unknown")
    endif()
endif()

# The build configuration this rig was configured at, stamped the same way
# and for the same reason as HVEN_RIG_COMMIT above: it goes into every
# observation's provenance and, for a float row, into the context match the
# expected-table reader enforces before asserting one (a float row is pinned
# to the build configuration(s) it was proven invariant across at derivation
# -- see docs/testing.md and each expected/<trace>.csv's own "build-config"
# metadata line). All of this project's presets are single-config Ninja
# generators, so CMAKE_BUILD_TYPE is always meaningfully set here; the
# "unknown" fallback only matters for a configure that skips the preset
# entirely.
if(CMAKE_BUILD_TYPE)
    set(HVEN_RIG_BUILD_CONFIG "${CMAKE_BUILD_TYPE}")
else()
    set(HVEN_RIG_BUILD_CONFIG "unknown")
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
target_include_directories(hven_golden_rig_support PUBLIC ${INCLUDE_DIRS})
target_link_libraries(hven_golden_rig_support PUBLIC
    Eigen3::Eigen fmt::fmt-header-only Threads::Threads GTest::gtest
)
target_compile_options(hven_golden_rig_support PRIVATE ${COMPILE_FLAGS})
target_compile_definitions(hven_golden_rig_support PUBLIC
    HVEN_RIG_EXPECTED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/expected"
    HVEN_RIG_COMMIT="${HVEN_RIG_COMMIT}"
    HVEN_RIG_BUILD_CONFIG="${HVEN_RIG_BUILD_CONFIG}"
)

# --- the interior-point old-seam arm ------------------------------------------
if(HVEN_RIG_PSIOPT_SEAM)
    set(_psiopt_seam_include "${HVEN_RIG_PSIOPT_SEAM}/psiopt/include")
    if(NOT EXISTS "${_psiopt_seam_include}/tycho/detail/solvers/linear/pardiso_interface.h")
        message(FATAL_ERROR
            "HVEN_RIG_PSIOPT_SEAM=${HVEN_RIG_PSIOPT_SEAM} does not look like a tycho checkout: "
            "expected ${_psiopt_seam_include}/tycho/detail/solvers/linear/pardiso_interface.h")
    endif()

    # Commit pin, mirroring the SQP arm's tag+commit verification above --
    # now in severity too, not just mechanism. Three ways this can fail to
    # verify, each collected into _psiopt_pin_problem rather than emitted
    # immediately, so the single FATAL_ERROR/WARNING decision below (gated on
    # HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM) is the only place severity is
    # decided:
    #   1. the checkout is not a git repository this rig can query at all
    #      (no git, or no .git directory) -- unverifiable is not the same as
    #      acceptable, so this is a problem, not a silent "recorded, not
    #      checked".
    #   2. HEAD is not the pinned commit.
    #   3. HEAD IS the pinned commit, but the tree the adapter actually
    #      compiles against (psiopt/include/tycho/detail/solvers/linear) has
    #      uncommitted changes -- a dirty tree at the right commit is still
    #      not the tree the expected tables were derived against.
    set(HVEN_RIG_PSIOPT_SEAM_VERIFIED "unverified")
    unset(_psiopt_pin_problem)
    if(Git_FOUND AND EXISTS "${HVEN_RIG_PSIOPT_SEAM}/.git")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
            WORKING_DIRECTORY "${HVEN_RIG_PSIOPT_SEAM}"
            OUTPUT_VARIABLE _psiopt_head_commit
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _psiopt_head_rc
            ERROR_QUIET)
        if(NOT _psiopt_head_rc EQUAL 0)
            string(CONCAT _psiopt_pin_problem
                "HVEN_RIG_PSIOPT_SEAM=${HVEN_RIG_PSIOPT_SEAM} has a .git directory but its HEAD "
                "commit could not be resolved, so the pin to ${HVEN_RIG_PSIOPT_SEAM_COMMIT} COULD "
                "NOT BE VERIFIED.")
        elseif(NOT _psiopt_head_commit STREQUAL HVEN_RIG_PSIOPT_SEAM_COMMIT)
            string(CONCAT _psiopt_pin_problem
                "HVEN_RIG_PSIOPT_SEAM's checkout HEAD is ${_psiopt_head_commit}, not the commit "
                "(${HVEN_RIG_PSIOPT_SEAM_COMMIT}) this rig's psiopt-old expected tables were "
                "derived against. Re-derive the psiopt-old rows in tests/golden_rig/expected/ if "
                "the drift touches Pardiso/Accelerate configuration or linear-algebra behavior "
                "(psiopt/include/tycho/detail/solvers/linear/), then update "
                "HVEN_RIG_PSIOPT_SEAM_COMMIT here to match.")
        else()
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" status --porcelain --
                        "psiopt/include/tycho/detail/solvers/linear"
                WORKING_DIRECTORY "${HVEN_RIG_PSIOPT_SEAM}"
                OUTPUT_VARIABLE _psiopt_dirty
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE _psiopt_status_rc
                ERROR_QUIET)
            if(NOT _psiopt_status_rc EQUAL 0)
                string(CONCAT _psiopt_pin_problem
                    "HVEN_RIG_PSIOPT_SEAM=${HVEN_RIG_PSIOPT_SEAM}: 'git status' over the consumed "
                    "psiopt/include/tycho/detail/solvers/linear tree could not be run, so its "
                    "cleanliness COULD NOT BE VERIFIED.")
            elseif(NOT _psiopt_dirty STREQUAL "")
                string(CONCAT _psiopt_pin_problem
                    "HVEN_RIG_PSIOPT_SEAM=${HVEN_RIG_PSIOPT_SEAM} has uncommitted changes under "
                    "psiopt/include/tycho/detail/solvers/linear, the tree seam_psiopt.cpp actually "
                    "compiles against:\n${_psiopt_dirty}\nHEAD matches the pin "
                    "(${HVEN_RIG_PSIOPT_SEAM_COMMIT}) but the checked-out linear/ headers are not "
                    "the ones that commit contains, so this checkout is HEAD-pinned but not "
                    "tree-pinned.")
            else()
                set(HVEN_RIG_PSIOPT_SEAM_VERIFIED "verified")
                message(STATUS
                    "golden rig: interior-point old-seam arm pinned to "
                    "${HVEN_RIG_PSIOPT_SEAM_COMMIT}, HEAD matches, consumed linear/ tree clean")
            endif()
        endif()
    else()
        string(CONCAT _psiopt_pin_problem
            "HVEN_RIG_PSIOPT_SEAM=${HVEN_RIG_PSIOPT_SEAM} is not a git checkout this rig can "
            "verify (git not found, or no .git directory), so its commit cannot be checked "
            "against the pin (${HVEN_RIG_PSIOPT_SEAM_COMMIT}) this rig's psiopt-old expected "
            "tables were derived against.")
    endif()

    if(_psiopt_pin_problem)
        if(HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM)
            message(WARNING
                "golden rig: ${_psiopt_pin_problem}\nProceeding anyway because "
                "HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM=ON was set explicitly. NEVER set this for a "
                "run that derives new psiopt-old expected tables.")
        else()
            message(FATAL_ERROR
                "golden rig: ${_psiopt_pin_problem}\nSet HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM=ON "
                "to downgrade this to a warning and configure anyway -- but never for a run that "
                "derives new expected tables; an unverified checkout produces expected tables "
                "with an unverifiable provenance.")
        endif()
    endif()

    target_sources(hven_golden_rig_support PRIVATE seam_psiopt.cpp)
    target_include_directories(hven_golden_rig_support SYSTEM PUBLIC "${_psiopt_seam_include}")
    # HVEN_RIG_PSIOPT_SEAM_VERIFIED carries the outcome of the verification
    # block above ("verified" or "unverified") into the report generator, so
    # a report row's provenance says whether the checkout it ran against was
    # actually checked rather than just recording the commit it claims.
    target_compile_definitions(hven_golden_rig_support PUBLIC
        HVEN_RIG_HAVE_PSIOPT_SEAM=1
        HVEN_RIG_PSIOPT_SEAM_COMMIT="${HVEN_RIG_PSIOPT_SEAM_COMMIT}"
        HVEN_RIG_PSIOPT_SEAM_VERIFIED="${HVEN_RIG_PSIOPT_SEAM_VERIFIED}"
    )
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
    # HVEN_RIG_SQP_SEAM_VERIFIED mirrors the psiopt arm's own verified-state
    # tracking so the report generator can carry both old seams' provenance
    # symmetrically (see the target_compile_definitions call below and
    # report_main.cpp).
    set(HVEN_RIG_SQP_SEAM_VERIFIED "unverified")
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
        set(HVEN_RIG_SQP_SEAM_VERIFIED "verified")
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
    # HVEN_RIG_SQP_SEAM_TAG and HVEN_RIG_SQP_SEAM_VERIFIED reach the report
    # generator the same way the psiopt arm's do above -- this seam already
    # verifies at configure time (the FATAL_ERROR calls above), but until now
    # that identity never reached report output; report_main.cpp prints both
    # old seams' provenance symmetrically.
    target_compile_definitions(hven_golden_rig_support PUBLIC
        HVEN_RIG_HAVE_SQP_SEAM=1
        HVEN_RIG_SQP_SEAM_TAG="${HVEN_RIG_SQP_SEAM_TAG}"
        HVEN_RIG_SQP_SEAM_COMMIT="${HVEN_RIG_SQP_SEAM_COMMIT}"
        HVEN_RIG_SQP_SEAM_VERIFIED="${HVEN_RIG_SQP_SEAM_VERIFIED}"
    )
endif()

################################################################################
# The trace suite, and the same traces in report mode.
################################################################################

set(HVEN_RIG_TRACE_SOURCES
    traces_sqp.cpp
    traces_interior_point.cpp
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
# HVEN_GTEST_DISCOVERY_MODE: POST_BUILD everywhere except Windows, where it is
# PRE_TEST -- see the ../CMakeLists.txt comment above this variable's
# definition for why.
gtest_discover_tests(hven_golden_rig DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})

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
# off the trace suite.
#
# PLATFORM SCOPE: Linux only (UNIX AND NOT APPLE), for two independent
# reasons, not one:
#
#   1. --wrap is a GNU ld / LLVM lld ELF-format feature; it has no COFF
#      equivalent. lld-link (the linker this project's Windows preset uses)
#      does not implement it -- confirmed the hard way: passing
#      -Wl,--wrap=pardiso through clang-cl on Windows produces "lld-link:
#      warning: ignoring unknown argument '-Wl,--wrap=pardiso'" followed by
#      an undefined-symbol error on __real_pardiso, because nothing actually
#      wired __wrap_pardiso to the real symbol. It has no Mach-O equivalent
#      either -- ld64 has never implemented --wrap or a comparable
#      link-time symbol-aliasing mechanism (Mach-O's nearest analogue,
#      dyld's __DATA,__interpose section, is a runtime interposition table,
#      not a linker flag, and would need a materially different
#      implementation here, not a flag swap) -- which is the deeper reason
#      "Apple has no such entry point" was never actually tested against a
#      linker that could have proven or disproven it.
#   2. Even where the toolchain could support it, this target has no
#      Windows (or Apple) MISSION to prove: the audit exists to observe the
#      OLD seams' real pardiso touchpoints during three-seam derivation
#      (HVEN_RIG_PSIOPT_SEAM / HVEN_RIG_SQP_SEAM), and both old-seam
#      adapters only ever build against sibling Linux/MKL checkouts (see
#      seam_sqp.cpp's unconditional <mkl_service.h> include and
#      seam_registry.cpp's #if !defined(__APPLE__) arm gates) -- there is no
#      derivation run this target could ever observe on Windows or macOS,
#      with or without a working --wrap. Porting the wrap mechanism to a
#      platform with nothing for it to watch would be effort spent widening
#      toolchain coverage for a target with no reason to run there, not
#      closing a real gap.
#
# GoldenRigAudit.StaticScanSelfTest (below, outside this if() block) is
# UNSCOPED on purpose: it is the audit's separate static-scanner half, run as
# a plain shell script against a committed sample file, with no linker
# involvement and no old-seam dependency at all -- it runs on every platform
# this repo builds on, including macOS and Windows CI, and confirms the
# scanner itself works on a machine that has neither sibling checkout. (Its
# own COMMAND still needs an explicit interpreter on Windows -- see the
# add_test() call below -- CreateProcess() cannot execute a bare .sh file the
# way Unix's shebang-aware exec() can; that is a process-launch mechanism
# question, unrelated to what makes hven_golden_rig_audit above Linux-only.)
################################################################################

if(UNIX AND NOT APPLE)
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
    # HVEN_GTEST_DISCOVERY_MODE is POST_BUILD here unconditionally in
    # practice: this target only ever configures on UNIX-and-not-APPLE,
    # where the variable (../CMakeLists.txt) is never switched to PRE_TEST.
    # Spelled with the variable anyway, not the literal, so this call site
    # does not silently stop matching the other three if that variable's own
    # condition ever changes.
    gtest_discover_tests(hven_golden_rig_audit DISCOVERY_MODE ${HVEN_GTEST_DISCOVERY_MODE})
endif()

# The static half's self-test. Runs the scanner over a committed sample that
# carries one line of every touchpoint class plus two negative controls, so it
# checks the SCANNER on a machine that has neither sibling checkout -- which is
# what keeps it in ordinary CI rather than only in a derivation run.
#
# static_scan.sh is invoked directly on Linux and macOS, relying on its own
# `#!/usr/bin/env bash` shebang -- both platforms' exec() honors it, and both
# already run and pass this test. Windows' CreateProcess() has no shebang
# concept at all and cannot launch a bare .sh file as a process; ctest
# reports that as BAD_COMMAND ("Process not started ... [inappropriate file
# type or format]"), not a script failure, which is exactly what a real
# windows-clang-release run surfaced the first time it got far enough to
# reach this test. Run through an explicit bash interpreter on WIN32 instead
# -- found via find_program(... REQUIRED) rather than assumed bare "bash" on
# PATH, so a missing interpreter fails loudly at configure time instead of
# mysteriously at test time. Git for Windows (preinstalled on GitHub's
# windows-latest runner image, and already this project's Windows CI steps'
# `shell: bash`) is what resolves it there.
if(WIN32)
    find_program(HVEN_BASH_EXECUTABLE NAMES bash REQUIRED)
    add_test(NAME GoldenRigAudit.StaticScanSelfTest
             COMMAND "${HVEN_BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/audit/static_scan.sh" --self-test)
else()
    add_test(NAME GoldenRigAudit.StaticScanSelfTest
             COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/audit/static_scan.sh" --self-test)
endif()
