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
# drivers/ (the top-level PSIOPT algorithm + NonLinearProgram),
# model/ (the NLPProblem/NLPSolver contract + its adapter), and
# interior/ + globalization/ under include/hven/detail/ (barrier /
# interior-point machinery) — mirrors psiopt's own internal grouping: a
# path map onto hven's directory conventions, not a reorganization. See
# docs/dev notes for the migration's file-by-file record.
################################################################################

target_sources(hven PRIVATE
    drivers/psiopt.cpp
    drivers/psiopt_globalization.cpp
    drivers/psiopt_print.cpp
    drivers/psiopt_settings.cpp
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
