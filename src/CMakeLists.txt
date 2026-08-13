################################################################################
# hven library sources — appended to the `hven` target created in the root
# CMakeLists.txt (target_sources, not add_library: the target's include dirs,
# linked dependencies, and compile options are already set up there).
################################################################################

target_sources(hven PRIVATE
    core/version.cpp
    core/pattern_hash.cpp
    linear/dense_symmetric_factor.cpp
)

# The sparse symmetric factor's two backends are split at the source level,
# not inside a shared TU. This is the opposite choice from
# linear/dense_symmetric_factor.cpp, and for a reason: there both platforms
# run the same LAPACK calls and differ only in which LAPACKE declarations
# they include, so one TU with a two-line #if is the whole difference. The
# sparse engines share no code at all — MKL Pardiso's phase-driven session
# and Accelerate's factor objects have nothing in common below the public
# header — so each backend gets its own TU and only the platform's own is
# ever compiled. A file that would #if itself away to nothing is a TU the
# build should not be compiling in the first place.
if(NOT APPLE)
    target_sources(hven PRIVATE
        linear/pardiso_session.cpp
        linear/symmetric_factor_mkl.cpp
    )
else()
    # This branch is configured, built, and tested against the real Accelerate
    # framework on every macOS CI run. The Linux syntax-only pass against a
    # stub Accelerate.h (scripts/check_accelerate_syntax_linux.sh) remains a
    # narrower structural cross-check; docs/testing.md states its claim ceiling.
    target_sources(hven PRIVATE
        linear/accelerate_session.cpp
        linear/symmetric_factor_accelerate.cpp
    )
endif()

################################################################################
# Interior-point engine (ex-PSIOPT) — migrated as-is from the standalone
# psiopt/ project the origin project previously hosted (hven M2
# milestone, engine-move task). The split below —
# drivers/ (the top-level InteriorPointSolver algorithm + NonLinearProgram),
# model/ (the NLPProblem/NLPSolver contract + its adapter), and
# interior/ + globalization/ under include/hven/detail/ (barrier /
# interior-point machinery) — mirrors psiopt's own internal grouping: a
# path map onto hven's directory conventions, not a reorganization. See
# docs/dev notes for the migration's file-by-file record.
################################################################################

target_sources(hven PRIVATE
    drivers/interior_point_solver.cpp
    drivers/interior_point_solver_globalization.cpp
    drivers/interior_point_solver_print.cpp
    drivers/interior_point_solver_settings.cpp
    drivers/solver_init.cpp
    drivers/non_linear_program.cpp
    model/nlp_adapter.cpp
    model/nlp_solver.cpp
    interior/kkt_factorization.cpp
    interior/utils/color_text.cpp
    interior/utils/get_core_count.cpp
    interior/utils/memory_management.cpp
    interior/utils/thread_pool.cpp
)
# interior/utils/fmtlib.cpp is NOT compiled here: it is a bare
# `#include "fmtlib.h"` with no definitions of its own, and psiopt's own
# utility library target (psiopt/CMakeLists.txt) never compiled it either
# -- restoring that omission here restores original target membership.
# fmtlib.h/.cpp still moved with the rest of src/utils/ (see
# src/interior/utils/); only its compilation into hven is skipped.

# color_text.h / fmtlib.h are private headers the driver TUs above include
# unqualified (#include "color_text.h"), exactly as psiopt/CMakeLists.txt's
# own PRIVATE include-dir setup (src/ + src/utils/) made possible there.
target_include_directories(hven PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/interior/utils
)

################################################################################
# Precompiled header
#
# What it contains and why the include order is load-bearing: src/hven_pch.h.
# Short version: parsing that header set costs 3.01 s, once per TU that pays
# it, and that floor -- not file length -- is what dominates this library's
# build. The PCH is built once and amortizes it.
#
# Membership is OPT-IN. Only the TUs listed below use the PCH; every other
# source in the target is opted out by the loop underneath, so a source added
# later gets no PCH until someone measures it and adds it here. That default
# is deliberate: on this library the PCH is a win for some TUs and a loss for
# others, and the losers are not guessable from file size.
#
# A TU qualifies only if it clears BOTH bars, measured on an idle box with
# ccache disabled (Linux, clang, Release -O3):
#   1. it gets FASTER, and
#   2. its object file is BYTE-IDENTICAL to the non-PCH build -- the
#      numerics-neutrality property CLAUDE.md states as "runtime neutrality
#      is proven, not presumed". Byte-identical objects cannot move a
#      golden-rig row.
#
# Measured, all 18 Linux TUs (no-PCH -> with-PCH, seconds; identical = object
# byte-for-byte equal). Participating TUs first:
#
#   drivers/interior_point_solver.cpp             6.99 -> 5.04  -1.95  identical
#   drivers/non_linear_program.cpp                6.54 -> 4.66  -1.88  identical
#   drivers/interior_point_solver_settings.cpp    4.00 -> 2.45  -1.55  identical
#   drivers/interior_point_solver_print.cpp       4.09 -> 2.56  -1.53  identical
#   model/nlp_solver.cpp                          4.09 -> 2.56  -1.53  identical
#   model/nlp_adapter.cpp                         4.61 -> 3.15  -1.46  identical
#     ------ opted out, faster but NOT byte-identical ------
#   drivers/interior_point_solver_globalization.cpp 4.94 -> 3.39 -1.55  differs
#   linear/symmetric_factor_mkl.cpp               3.41 -> 2.70  -0.71  differs
#   linear/pardiso_session.cpp                    3.20 -> 2.50  -0.70  differs
#   interior/kkt_factorization.cpp                2.97 -> 2.27  -0.70  differs
#   core/pattern_hash.cpp                         2.98 -> 2.29  -0.69  differs
#   linear/dense_symmetric_factor.cpp             3.18 -> 2.53  -0.65  differs
#   interior/utils/memory_management.cpp          1.41 -> 1.04  -0.37  differs
#     ------ opted out, slower with the PCH ------
#   core/version.cpp                              0.02 -> 0.98  +0.96
#   interior/utils/get_core_count.cpp             0.70 -> 1.14  +0.44
#   interior/utils/thread_pool.cpp                0.72 -> 1.12  +0.40
#   interior/utils/color_text.cpp                 0.70 -> 0.98  +0.28
#   drivers/solver_init.cpp                       1.34 -> 1.48  +0.14
#
# The "differs" group is not a correctness problem -- disassembly comparison
# showed identical instructions per symbol, with only function layout order and
# local label numbering shifting. It differs because those TUs include a
# different header set in a different order (interior_point_solver_globalization.cpp,
# for instance, orders its includes alphabetically and pulls in two headers
# outside the shared set), so prefixing the shared block changes template
# instantiation order and hence emission order. They are excluded because
# byte-identity is the bar this project holds for engine code, not because
# their code is wrong. Relaxing bar 2 to "identical instructions" would buy
# roughly 5.4 s more; that is a numerics-governance call, not a build call.
#
# The macOS-only linear backends (linear/accelerate_session.cpp,
# linear/symmetric_factor_accelerate.cpp) are unmeasured on this box and so are
# opted out by the default above, like every other unlisted source.
set(_hven_pch_sources
    drivers/interior_point_solver.cpp
    drivers/interior_point_solver_print.cpp
    drivers/interior_point_solver_settings.cpp
    drivers/non_linear_program.cpp
    model/nlp_adapter.cpp
    model/nlp_solver.cpp
)

target_precompile_headers(hven PRIVATE hven_pch.h)

# Omit the embedded timestamp from the generated PCH so ccache can reuse cached
# object files across PCH regenerations. Pairs with the developer-side ccache
# settings documented in docs/build.md (sloppiness = pch_defines,time_macros
# and pch_external_checksum = true): consumers then hash the PCH *content*, so
# a regenerated-but-identical PCH costs nothing while a genuinely changed one
# still invalidates.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(hven PRIVATE -Xclang -fno-pch-timestamp)
endif()

# Opt out everything not in the list above. Driven off the target's actual
# SOURCES so it stays correct as sources are added or as the platform branch
# above selects a different linear backend. target_sources() stores absolute
# paths, so the participating list is absolutized before comparing rather than
# compared as written -- a relative-vs-absolute mismatch here would silently
# match nothing, which is why the count is asserted at the end.
set(_hven_pch_sources_abs "")
foreach(_hven_src IN LISTS _hven_pch_sources)
    list(APPEND _hven_pch_sources_abs "${CMAKE_CURRENT_SOURCE_DIR}/${_hven_src}")
endforeach()

get_target_property(_hven_all_sources hven SOURCES)
if(NOT _hven_all_sources)
    message(FATAL_ERROR
        "hven: cannot read the target's SOURCES to apply the PCH opt-out list. "
        "Without it every source would silently receive the precompiled header, "
        "including ones measured to be slower or to produce a non-byte-identical "
        "object. Refusing to configure rather than shipping that silently.")
endif()

set(_hven_pch_applied 0)
foreach(_hven_src IN LISTS _hven_all_sources)
    get_filename_component(_hven_src_abs "${_hven_src}" ABSOLUTE)
    if(_hven_src_abs IN_LIST _hven_pch_sources_abs)
        math(EXPR _hven_pch_applied "${_hven_pch_applied} + 1")
    else()
        # TARGET_DIRECTORY, not a bare call: source-file properties are scoped
        # to a directory, and `hven` is created by add_library() in the ROOT
        # CMakeLists.txt, not here. A bare set_source_files_properties() would
        # write the property into this (src/) scope, where nothing reads it,
        # and the opt-out would silently do nothing.
        set_source_files_properties("${_hven_src}"
            TARGET_DIRECTORY hven
            PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    endif()
endforeach()

# Assert the opt-in list actually matched. Without this a typo or a renamed
# source degrades silently in the expensive direction: the PCH stays ON for a
# TU nobody measured, and the byte-identity property the numerics gate leans on
# is lost with no signal at configure time.
list(LENGTH _hven_pch_sources _hven_pch_expected)
if(NOT _hven_pch_applied EQUAL _hven_pch_expected)
    message(FATAL_ERROR
        "hven: the precompiled-header opt-in list matched ${_hven_pch_applied} of "
        "${_hven_pch_expected} sources. Every name in _hven_pch_sources must name a "
        "source actually compiled into the hven target (paths relative to "
        "${CMAKE_CURRENT_SOURCE_DIR}). Fix the list rather than letting the PCH "
        "silently cover a different set of TUs than the measured one.")
endif()

unset(_hven_all_sources)
unset(_hven_pch_sources_abs)
unset(_hven_pch_applied)
unset(_hven_pch_expected)
