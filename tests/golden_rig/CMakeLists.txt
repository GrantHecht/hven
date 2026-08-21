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
# strength too, but by a different IDENTITY. HVEN_RIG_PSIOPT_SEAM names a
# live tycho checkout (this project's main development sibling) rather than a
# checkout frozen at a released tag, so unlike HVEN_RIG_SQP_SEAM there is no
# tag to re-resolve. Pinning that checkout's HEAD COMMIT made every advance
# of it -- including the great majority that cannot touch these tables at all
# -- a FATAL to be cleared by hand, while verifying only a proxy for the tree
# the expected tables actually depend on. So the pin is the CONTENT of the
# consumed subtree: the rig verifies the git tree hash of
# HVEN_RIG_PSIOPT_SEAM_SCOPE at the checkout's HEAD against
# HVEN_RIG_PSIOPT_SEAM_TREE below. An advance that does not touch the
# consumed subtree is a non-event; one that does still FATALs. (Ruled by the
# rig-pin ruling, 2026-08-19, reviewer-side
# `2026-08-19-rig-pin-ruling-and-nine-set-confirmation.md`, SIGNOFF
# RIG-PIN-RULING-FINAL.)
#
# The scope is closed, and self-guarding: seam_psiopt.cpp includes exactly
# two headers from the checkout (pardiso_interface.h on the MKL branch,
# accelerate_interface.h on the Apple branch), whose include closure inside
# the checkout reaches only accelerate_utils.h beside them -- Eigen and fmt
# come from hven's own dep/ trees and MKL/Accelerate from the system, and the
# checkout is on the include path only via ${HVEN_RIG_PSIOPT_SEAM}/psiopt/
# include. Any future edit that widened that closure would necessarily edit a
# file inside the scope directory, changing the tree hash and FATALing here.
# The one residual coupling is seam_psiopt.cpp's own include set: if that
# changes, the scope has to be re-ruled in the same review.
#
# "Expected to keep moving" describes when the pin needs updating, not
# whether an unverified or drifted checkout is safe to derive expectations
# from: it is not, so a consumed tree that hashes differently, a dirty
# consumed tree, or an unverifiable checkout (no git, or no .git directory)
# is a configure-time FATAL_ERROR by default, same as the SQP seam's checks.
# HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM=ON (above) downgrades that to the
# WARNING-and-proceed behavior for the one case that legitimately wants it --
# iterating against a moving tycho checkout without deriving new tables --
# and re-deriving the psiopt-old expected tables is the actual remedy when
# the drift touches Pardiso/Accelerate configuration or linear-algebra
# behavior in the scope directory.
#
#   tree   : the value below, checked at configure time when the checkout is
#            a git repository; both the HEAD tree hash AND the cleanliness of
#            the consumed linear/ tree are checked -- see the verification
#            block below for why a HEAD-tree match alone is not enough.
#   HEAD   : observed, never pinned. Recorded in the report rows' provenance
#            beside the verified tree hash, so provenance still names a
#            human-findable commit.
#   anchor : 5b5dc736c238b285915507c3677d7952227c0d1b -- the commit the
#            psiopt-old expected tables were derived at, and the previous
#            HEAD-commit pin. The consumed tree hashes to the value below
#            there, which is why converting this pin to a tree hash owes no
#            re-derivation. (That commit was itself re-pinned 2026-08-13
#            (hven M2.5, Task 2) from 060ad2138a373b... only after a `git
#            diff ..main --stat -- psiopt/` over the range came back EMPTY;
#            the tree check below now IS that argument, made by the build
#            rather than by hand.)
#
# M6 forward note: M6 deletes psiopt/ from the checkout, at which point this
# check FATALs whatever its shape -- converting this arm to the SQP arm's
# tag+commit form against a tag cut on the last psiopt-bearing commit, or
# retiring the arm with the milestone, is decided at M6 planning, not here
# (same ruling as above).
set(HVEN_RIG_PSIOPT_SEAM_SCOPE "psiopt/include/tycho/detail/solvers/linear")
set(HVEN_RIG_PSIOPT_SEAM_TREE "bfb7d30c5cc821069b7af3f9efa077f29474de5e")

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

    # Tree-hash pin, mirroring the SQP arm's tag+commit verification above in
    # severity and in structure, though not in identity (see the long note on
    # HVEN_RIG_PSIOPT_SEAM_TREE above for why this arm pins content rather
    # than a commit). Four ways this can fail to verify, each collected into
    # _psiopt_pin_problem rather than emitted immediately, so the single
    # FATAL_ERROR/WARNING decision below (gated on
    # HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM) is the only place severity is
    # decided:
    #   1. the checkout is not a git repository this rig can query at all
    #      (no git, or no .git directory) -- unverifiable is not the same as
    #      acceptable, so this is a problem, not a silent "recorded, not
    #      checked".
    #   2. the checkout can be queried but HEAD, or the consumed subtree at
    #      HEAD, does not resolve.
    #   3. the consumed subtree resolves but hashes to something other than
    #      the pinned tree.
    #   4. the consumed subtree at HEAD matches the pin, but the tree the
    #      adapter actually compiles against has uncommitted changes -- a
    #      HEAD tree hash cannot see working-tree state, so this check stays
    #      load-bearing exactly as it was under the commit pin.
    #
    # HVEN_RIG_PSIOPT_SEAM_TREE_OBSERVED is what the checkout ACTUALLY hashed
    # to, as distinct from HVEN_RIG_PSIOPT_SEAM_TREE, which is what it was
    # pinned to. The two are equal on a verified configure and that is the
    # point of the pin -- but under the escape hatch they can differ, and the
    # report's provenance names the OBSERVED hash as the tree the adapter
    # compiled against, so no row can assert a content identity the compiled
    # headers do not have. It defaults to "unknown" rather than to the pin
    # because failure paths 1 and 2 never compute a hash at all: there is no
    # observation to report on them, and reporting the pin instead would be
    # exactly the false assertion this variable exists to prevent.
    set(HVEN_RIG_PSIOPT_SEAM_VERIFIED "unverified")
    set(HVEN_RIG_PSIOPT_SEAM_HEAD "unknown")
    set(HVEN_RIG_PSIOPT_SEAM_TREE_OBSERVED "unknown")
    unset(_psiopt_pin_problem)
    if(Git_FOUND AND EXISTS "${HVEN_RIG_PSIOPT_SEAM}/.git")
        # HEAD is read for PROVENANCE, not for the pin: the report rows name
        # the commit the verified tree was observed at, so a reader has
        # something human-findable beside the content hash.
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
                "could not be resolved, so the consumed ${HVEN_RIG_PSIOPT_SEAM_SCOPE} tree COULD "
                "NOT BE VERIFIED against the pinned tree ${HVEN_RIG_PSIOPT_SEAM_TREE}.")
        else()
            set(HVEN_RIG_PSIOPT_SEAM_HEAD "${_psiopt_head_commit}")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse "HEAD:${HVEN_RIG_PSIOPT_SEAM_SCOPE}"
                WORKING_DIRECTORY "${HVEN_RIG_PSIOPT_SEAM}"
                OUTPUT_VARIABLE _psiopt_tree_hash
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE _psiopt_tree_rc
                ERROR_QUIET)
            # Recorded the moment it exists, on EVERY arm below -- matched,
            # drifted, or matched-but-dirty. The provenance row reports what
            # was seen; whether what was seen is acceptable is the pin check's
            # separate verdict, carried by HVEN_RIG_PSIOPT_SEAM_VERIFIED.
            if(_psiopt_tree_rc EQUAL 0)
                set(HVEN_RIG_PSIOPT_SEAM_TREE_OBSERVED "${_psiopt_tree_hash}")
            endif()
            if(NOT _psiopt_tree_rc EQUAL 0)
                string(CONCAT _psiopt_pin_problem
                    "HVEN_RIG_PSIOPT_SEAM=${HVEN_RIG_PSIOPT_SEAM}: the consumed "
                    "${HVEN_RIG_PSIOPT_SEAM_SCOPE} tree does not resolve at HEAD "
                    "(${_psiopt_head_commit}), so it COULD NOT BE VERIFIED against the pinned "
                    "tree ${HVEN_RIG_PSIOPT_SEAM_TREE}.")
            elseif(NOT _psiopt_tree_hash STREQUAL HVEN_RIG_PSIOPT_SEAM_TREE)
                string(CONCAT _psiopt_pin_problem
                    "HVEN_RIG_PSIOPT_SEAM's consumed ${HVEN_RIG_PSIOPT_SEAM_SCOPE} tree hashes to "
                    "${_psiopt_tree_hash} at HEAD ${_psiopt_head_commit}, not the tree "
                    "(${HVEN_RIG_PSIOPT_SEAM_TREE}) this rig's psiopt-old expected tables were "
                    "derived against. The headers seam_psiopt.cpp compiles against have changed "
                    "content. Re-derive the psiopt-old rows in tests/golden_rig/expected/ if the "
                    "drift touches Pardiso/Accelerate configuration or linear-algebra behavior, "
                    "then update HVEN_RIG_PSIOPT_SEAM_TREE here to match.")
            else()
                execute_process(
                    COMMAND "${GIT_EXECUTABLE}" status --porcelain --
                            "${HVEN_RIG_PSIOPT_SEAM_SCOPE}"
                    WORKING_DIRECTORY "${HVEN_RIG_PSIOPT_SEAM}"
                    OUTPUT_VARIABLE _psiopt_dirty
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE _psiopt_status_rc
                    ERROR_QUIET)
                if(NOT _psiopt_status_rc EQUAL 0)
                    string(CONCAT _psiopt_pin_problem
                        "HVEN_RIG_PSIOPT_SEAM=${HVEN_RIG_PSIOPT_SEAM}: 'git status' over the "
                        "consumed ${HVEN_RIG_PSIOPT_SEAM_SCOPE} tree could not be run, so its "
                        "cleanliness COULD NOT BE VERIFIED.")
                elseif(NOT _psiopt_dirty STREQUAL "")
                    string(CONCAT _psiopt_pin_problem
                        "HVEN_RIG_PSIOPT_SEAM=${HVEN_RIG_PSIOPT_SEAM} has uncommitted changes "
                        "under ${HVEN_RIG_PSIOPT_SEAM_SCOPE}, the tree seam_psiopt.cpp actually "
                        "compiles against:\n${_psiopt_dirty}\nThe tree at HEAD matches the pin "
                        "(${HVEN_RIG_PSIOPT_SEAM_TREE}) but the checked-out linear/ headers are "
                        "not the ones that tree contains, so this checkout is HEAD-pinned but not "
                        "working-tree-pinned.")
                else()
                    set(HVEN_RIG_PSIOPT_SEAM_VERIFIED "verified")
                    message(STATUS
                        "golden rig: interior-point old-seam arm pinned to consumed tree "
                        "${HVEN_RIG_PSIOPT_SEAM_TREE} (${HVEN_RIG_PSIOPT_SEAM_SCOPE}), matched at "
                        "HEAD ${_psiopt_head_commit}, consumed linear/ tree clean")
                endif()
            endif()
        endif()
    else()
        string(CONCAT _psiopt_pin_problem
            "HVEN_RIG_PSIOPT_SEAM=${HVEN_RIG_PSIOPT_SEAM} is not a git checkout this rig can "
            "verify (git not found, or no .git directory), so its consumed "
            "${HVEN_RIG_PSIOPT_SEAM_SCOPE} tree cannot be checked against the pin "
            "(${HVEN_RIG_PSIOPT_SEAM_TREE}) this rig's psiopt-old expected tables were derived "
            "against.")
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
    # Four facts reach the report generator, and they are four because a
    # provenance row must separate what was SEEN from what was DEMANDED:
    #   TREE          -- the pin, the tree the expected tables were derived
    #                    against. A constant of this file.
    #   TREE_OBSERVED -- what this configure actually hashed the consumed
    #                    subtree to, or "unknown" if it never got that far.
    #                    This, not TREE, is the tree the adapter compiled
    #                    against, so this is what the row names as the
    #                    consumed identity.
    #   HEAD          -- the OBSERVED commit that tree was seen at ("unknown"
    #                    when the checkout could not be queried) -- the pin is
    #                    the tree, the commit is how a reader finds it again.
    #   VERIFIED      -- the outcome of the block above, so a row says whether
    #                    the checkout was actually checked rather than just
    #                    recording the tree it claims.
    # TREE_OBSERVED is a HEAD-tree observation and so cannot see working-tree
    # state; on the matched-but-dirty arm it equals the pin while the compiled
    # headers differ from both. That case is exactly why VERIFIED stays
    # load-bearing beside it and is not made redundant by the observed hash.
    target_compile_definitions(hven_golden_rig_support PUBLIC
        HVEN_RIG_HAVE_PSIOPT_SEAM=1
        HVEN_RIG_PSIOPT_SEAM_TREE="${HVEN_RIG_PSIOPT_SEAM_TREE}"
        HVEN_RIG_PSIOPT_SEAM_TREE_OBSERVED="${HVEN_RIG_PSIOPT_SEAM_TREE_OBSERVED}"
        HVEN_RIG_PSIOPT_SEAM_HEAD="${HVEN_RIG_PSIOPT_SEAM_HEAD}"
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
