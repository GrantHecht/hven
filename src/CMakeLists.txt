################################################################################
# hven library sources — appended to the `hven` target created in the root
# CMakeLists.txt (target_sources, not add_library: the target's include dirs,
# linked dependencies, and compile options are already set up there).
################################################################################

target_sources(hven PRIVATE
    core/version.cpp
    core/pattern_hash.cpp
    core/ledger.cpp
    # The display-string switches for the two solver enums core/ owns
    # (SqpStatus, StartLevel). They are in a core/ TU, not the drivers/ printing
    # TU that carries the other three, so that no core/ object resolves a symbol
    # out of a drivers/ one -- see the file's own banner.
    core/enum_names.cpp
    linear/dense_symmetric_factor.cpp
    kkt/kkt_calls.cpp
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
# The provider-side model contract (include/hven/model/). Its own block rather
# than part of the engine block below: the contract is what any provider
# implements and any engine consumes, not one engine's implementation.
#
# The declaration needs a TU because it holds its piece lists by value while its
# header only declares the piece handle types, so its special members and its
# boundary validation are defined where those types are complete. The NlpModel
# bridge needs one because it is an implementation rather than a contract
# surface, and it sits in this block rather than the engine block below because
# it is a second provider of the contract and names no engine type. The
# triplet-to-native conversion sits here on the same footing: it is a provider
# of the model contract and names no engine type either.
################################################################################

target_sources(hven PRIVATE
    model/aggregate_declaration.cpp
    model/nlp_model_aggregate.cpp
    model/nlp_problem_model.cpp
)

################################################################################
# Interior-point engine — the top-level InteriorPointSolver algorithm
# (drivers/), the NLPProblem/NLPSolver contract with its adapter and the
# partitioned-evaluation NonLinearProgram engine (model/), and the barrier /
# interior-point machinery under include/hven/detail/ (interior/ +
# globalization/), split along those same seams.
################################################################################

target_sources(hven PRIVATE
    drivers/interior_point_solver.cpp
    drivers/interior_point_solver_globalization.cpp
    drivers/interior_point_solver_print.cpp
    drivers/interior_point_solver_settings.cpp
    drivers/solver_init.cpp
    model/non_linear_program.cpp
    model/nlp_adapter.cpp
    model/nlp_solver.cpp
    interior/kkt_factorization.cpp
    interior/utils/color_text.cpp
    interior/utils/get_core_count.cpp
    interior/utils/memory_management.cpp
    interior/utils/thread_pool.cpp
)
# interior/utils/fmtlib.cpp is NOT compiled here: it is a bare
# `#include "fmtlib.h"` with no definitions of its own, so there is nothing
# for this target to compile. The file still lives in src/interior/utils/;
# only its compilation into hven is skipped.

# color_text.h / fmtlib.h are private headers the driver TUs above include
# unqualified (#include "color_text.h"), which is what this PRIVATE
# include-dir setup makes possible.
target_include_directories(hven PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/interior/utils
)

################################################################################
# SQP engine
#
# The SQP engine's code lives in include/hven/drivers/, include/hven/detail/,
# and these TUs. Each TU below is a carve of header-side code into a .cpp
# under CLAUDE.md §5's rule that orchestration, printing, and options belong
# outside headers; each file's own banner carries its argument:
#
#   - drivers/sqp_print.cpp: the printing surface.
#
#   - drivers/sqp_options.cpp: SqpOptions boundary validation. Its banner
#     carries the ONE flag premise in this tree that a comparison's spelling
#     depends on -- `-fno-finite-math-only`, without which `!(x > 0.0)` may be
#     rewritten to `x <= 0.0` and stop rejecting NaN -- and a battery pins
#     that premise by disassembling this TU.
#
#   - drivers/aggregate_eval_seam.cpp and drivers/sqp_driver.cpp: the driver's
#     consumer-side seam and the major loop itself (SqpDriver::solve_impl plus
#     everything it orchestrates). The loop's banner carries the object-file
#     evidence that its per-element calls were already out-of-line at -O3, so
#     the boundary removes no inlining, and the ruling under which the TR
#     update functions travel with the loop while the TR policy constants stay
#     in globalization.
#
#   - globalization/sqp/funnel.cpp: FunnelStrategy's acceptance state machine,
#     the first carve whose arithmetic is re-associable under -ffast-math; its
#     proof is bit-identity of every asserted counter across the boundary.
#
#   - globalization/sqp/soc_elastic_restoration.cpp: ONE TU for the three
#     step-recovery tiers (second-order correction, elastic l1 exact-penalty
#     tier, restoration phase's feasibility-model wrapper), because soc.h and
#     restoration.h both declare themselves not self-contained (they need
#     NlpEval, which only drivers/sqp_driver.h defines) and because the three
#     tiers are entered from one place in solve_impl.
#
#   - warmstart/warm_start.cpp and warmstart/continuation.cpp: the interior-
#     point crossover's ingest and the continuation sweep. detail::
#     ip_activity_threshold stays a header template so the ingest can pass an
#     Eigen expression rather than materialise two n-sized temporaries.
################################################################################

target_sources(hven PRIVATE
    drivers/aggregate_eval_seam.cpp
    drivers/sqp_driver.cpp
    drivers/sqp_options.cpp
    drivers/sqp_print.cpp
    globalization/sqp/funnel.cpp
    globalization/sqp/soc_elastic_restoration.cpp
    warmstart/continuation.cpp
    warmstart/warm_start.cpp
)

################################################################################
# The warm-start currency (include/hven/warmstart/warm_start_data.h). Its own
# block rather than part of the engine blocks above, because it is neither
# engine's: WarmStartData is the value a consumer holds BETWEEN solves, and the
# component names no engine type and includes no engine header. Its TU carries
# the byte form -- the layout, the bounds-checked decode, and the refusals --
# which is orchestration-shaped, non-template code that CLAUDE.md section 5
# puts in a .cpp regardless of the surrounding loop.
################################################################################

target_sources(hven PRIVATE
    warmstart/warm_start_data.cpp
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
# source is explicitly opted out by the deferred block underneath, which then
# reads the opt-out back and refuses to configure if it did not stick. A
# source added later does not quietly inherit the PCH and does not quietly
# inherit the opt-out either -- it trips the source-count postcondition, so
# somebody has to decide. That is deliberate: on this library the PCH is a win
# for some TUs and a loss for others, and the losers are not guessable from
# file size.
#
# A TU qualifies only if it clears BOTH bars, measured on an idle box with
# ccache disabled (Linux, clang, Release -O3):
#   1. it gets FASTER, and
#   2. its object file is BYTE-IDENTICAL to the non-PCH build -- the
#      numerics-neutrality property CLAUDE.md states as "runtime neutrality
#      is proven, not presumed". Byte-identical objects cannot move a
#      golden-rig row.
#
# Measured, all 18 Linux TUs as of the original sweep (no-PCH -> with-PCH,
# seconds; identical = object byte-for-byte equal). Participating TUs first:
#
#   drivers/interior_point_solver.cpp             6.99 -> 5.04  -1.95  identical
#   drivers/non_linear_program.cpp                6.54 -> 4.66  -1.88  identical
#     (this source has since moved to model/non_linear_program.cpp; the row
#     above is left at its original path, recording the sweep as it ran)
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
# linear/symmetric_factor_accelerate.cpp) are unmeasured -- the table above is
# Linux -- so they are not on the opt-in list and are explicitly opted out on
# that branch, like every other unlisted source.
#
# The T1-T8 addenda (per-TU PCH measurement provenance for the SQP-side carves)
# moved verbatim to docs/notes/2026-08-m3-phase-c-tu-carve-addenda.md.
# That file also carries the 2026-08-25 re-prove of bar 2 under the Release
# flag set that added -falign-loops=32, which recompiles every object here.
set(_hven_pch_sources
    drivers/interior_point_solver.cpp
    drivers/interior_point_solver_print.cpp
    drivers/interior_point_solver_settings.cpp
    model/non_linear_program.cpp
    model/nlp_adapter.cpp
    model/nlp_solver.cpp
)

target_precompile_headers(hven PRIVATE hven_pch.h)

# Omit the embedded timestamp from the generated PCH. Under Clang this is a
# REQUIREMENT for using ccache at all, not a tuning knob: clang stamps a
# timestamp into the PCH, and ccache's manual is explicit that a clang PCH has
# to be built with this flag for caching to work. The developer-side half is
# `sloppiness = pch_defines,time_macros`, without which ccache refuses to cache
# PCH-consuming compiles outright.
#
# Note for anyone reading an older revision of this comment: ccache's
# `pch_external_checksum` is deliberately NOT part of this arrangement. It does
# not make consumers hash the PCH's content -- ccache already does that by
# default. It substitutes a `<pch>.sum` sidecar for the PCH when one exists, as
# a performance workaround for very large precompiled headers, and nothing here
# emits such a file, so setting it would be inert. docs/build.md carries the
# corrected write-up.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(hven PRIVATE -Xclang -fno-pch-timestamp)
endif()

# Total number of sources the hven target is expected to carry, per platform
# branch (both branches happen to come to the same number today: the shared
# sources plus exactly two platform-specific linear backend TUs). This is a
# tripwire, not bookkeeping -- adding a source to the target should be a
# moment where someone decides whether it belongs in the PCH, and a stale
# number here forces that decision instead of letting the new TU inherit a
# default nobody measured.
#
# 29 as of model/aggregate_declaration.cpp. That TU stays OUT of the opt-in
# list: the list admits a source only on measured evidence (faster with the
# PCH, and byte-identical object), and this one has neither measurement. Out is
# the unmeasured default and costs nothing but its own compile time -- it is a
# small TU that includes the two piece headers and fmt, not the shared block the
# PCH prefixes.
#
# 30 as of model/nlp_model_aggregate.cpp, and it stays out on the same footing:
# no measurement exists for it, and out is the unmeasured default. It includes
# the model contract, the model interface, Eigen sparse and fmt -- a different
# header set from the one src/hven_pch.h prefixes, which is the property that
# has failed bar 2 for every TU measured so far.
#
# 31 as of drivers/aggregate_eval_seam.cpp, the SQP driver's consumer-side
# binding onto the provider contract. It stays OUT on the same footing as the
# two above -- no measurement exists for it, and out is the unmeasured default
# -- and on the stronger prior the SQP-side carves established: it includes
# drivers/sqp_driver.h (for NlpEval) plus the model contract, which is a
# different header set in a different order from the one src/hven_pch.h
# prefixes, and that property has failed bar 2 for all eight SQP-side TUs
# measured so far.
#
# 32 as of model/nlp_problem_model.cpp, the triplet-to-native conversion. It
# stays OUT on the same footing as the three above -- no measurement exists for
# it, and out is the unmeasured default. It includes the model contract, the
# triplet problem surface, Eigen sparse and fmt: again a different header set
# in a different order from the one src/hven_pch.h prefixes.
#
# 33 as of warmstart/warm_start_data.cpp, the warm-start currency's byte form.
# It stays OUT on the same footing as the four above -- no measurement exists
# for it, and out is the unmeasured default. It includes the currency header,
# the model contract's structure-identity header, and fmt: again a different
# header set in a different order from the one src/hven_pch.h prefixes.
set(_hven_expected_source_count 33)

# Everything below runs DEFERRED, at the end of the directory scope that
# defines the hven target, rather than here.
#
# Here is too early. target_sources() can legitimately be called again after
# this point -- from later in this file, from another add_subdirectory(), or
# by an embedding project -- and a loop that runs now would only ever see a
# snapshot. Sources appended afterwards would silently receive the PCH,
# because target_precompile_headers() applies to the whole target while the
# opt-out is applied per source. That is the dangerous direction: a TU nobody
# measured, quietly compiled against the shared header set, with the
# byte-identity property lost and a green configure.
#
# Deferring to the end of hven's own top-level scope means the loop sees the
# final source list. PROJECT_SOURCE_DIR (not CMAKE_SOURCE_DIR) is deliberate:
# it is hven's root whether hven is the top-level project or is being consumed
# via add_subdirectory() by an embedding project, where CMAKE_SOURCE_DIR would
# point at that project's root instead.
#
# The exact scope of the guarantee, stated so it is not read as broader than it
# is: this covers every source added while hven's own directory tree is being
# processed, which is where all of hven's sources come from. It does NOT cover
# an embedding project calling target_sources(hven ...) from its own scope
# after add_subdirectory() returns -- the deferred call has already run by
# then, so such a source would receive the PCH unchecked. That is outside this
# project's contract (hven owns its own source list), and scripts/
# check_pch_neutrality.sh would still catch the consequence, but nothing here
# prevents it.

# The opt-in list, the directory it is written relative to, and the expected
# source count travel to the deferred call through GLOBAL properties rather
# than through variables or call arguments. Both alternatives fail here: a
# deferred call executes in the scope of the directory it was deferred TO
# (hven's root), not the one that scheduled it (src/), so plain variables set
# above are invisible inside; and deferred call arguments do not survive list
# expansion intact. Global properties have neither problem, and reading one
# that was never set is caught by the guards below rather than silently
# yielding an empty list.
function(_hven_apply_pch_membership)
    get_property(_list_dir GLOBAL PROPERTY HVEN_PCH_LIST_DIR)
    get_property(_expected_source_count GLOBAL PROPERTY HVEN_PCH_EXPECTED_SOURCE_COUNT)
    get_property(_optin_rel GLOBAL PROPERTY HVEN_PCH_SOURCES)
    if(NOT _list_dir OR NOT _expected_source_count)
        message(FATAL_ERROR
            "hven: the precompiled-header membership parameters did not reach the "
            "deferred check. Refusing to configure rather than applying the PCH to an "
            "unverified set of sources.")
    endif()
    if(NOT _optin_rel)
        message(FATAL_ERROR
            "hven: the precompiled-header opt-in list arrived empty. Every source "
            "would be opted out, silently discarding the PCH and its measured build-"
            "time win. Refusing to configure rather than degrading quietly.")
    endif()
    get_target_property(_all_sources hven SOURCES)
    if(NOT _all_sources)
        message(FATAL_ERROR
            "hven: cannot read the hven target's SOURCES to apply the "
            "precompiled-header opt-out. Without it every source would silently "
            "receive the PCH, including ones measured to be slower or to produce a "
            "non-byte-identical object. Refusing to configure rather than shipping "
            "that silently.")
    endif()

    # The opt-in list is written relative to the directory that declares it.
    set(_optin_abs "")
    foreach(_src IN LISTS _optin_rel)
        list(APPEND _optin_abs "${_list_dir}/${_src}")
    endforeach()

    # Target sources may come back absolute (what target_sources() stores) or
    # relative (what add_library() in the target's own directory can store).
    # Resolve relative ones against the target's directory, NOT against
    # whatever directory this deferred call happens to run in -- resolving
    # against the wrong base silently matches nothing, which fails open.
    get_target_property(_target_dir hven SOURCE_DIR)

    set(_applied 0)
    set(_total 0)
    set(_not_optin "")
    foreach(_src IN LISTS _all_sources)
        math(EXPR _total "${_total} + 1")
        if(IS_ABSOLUTE "${_src}")
            set(_abs "${_src}")
        else()
            set(_abs "${_target_dir}/${_src}")
        endif()
        if(_abs IN_LIST _optin_abs)
            math(EXPR _applied "${_applied} + 1")
        else()
            # TARGET_DIRECTORY names the scope explicitly. Source-file
            # properties are directory-scoped and `hven` is created by
            # add_library() in hven's ROOT CMakeLists.txt, not in src/; this
            # deferred call happens to already run in that root scope, so a
            # bare call would land correctly today. It is written explicitly
            # anyway, because that coincidence is a property of where the
            # DEFER points and would stop holding if this block were ever
            # moved back inline. Note that the explicitness is documentation,
            # not enforcement -- the readback postcondition below is what
            # actually catches the property failing to stick, and it was
            # verified to fire by making this call a no-op for one source.
            set_source_files_properties("${_src}"
                TARGET_DIRECTORY hven
                PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
            list(APPEND _not_optin "${_src}")
        endif()
    endforeach()

    # Postcondition 1: every name in the opt-in list matched a real source.
    # Catches a typo or a rename, which would otherwise leave a measured TU
    # quietly outside the PCH (cheap direction) or, combined with the readback
    # below, reveal a list that no longer describes the build.
    list(LENGTH _optin_rel _expected)
    if(NOT _applied EQUAL _expected)
        message(FATAL_ERROR
            "hven: the precompiled-header opt-in list matched ${_applied} of "
            "${_expected} entries. Every name in _hven_pch_sources must name a source "
            "actually compiled into the hven target (paths relative to ${_list_dir}). "
            "Fix the list rather than letting the PCH cover a different set of TUs "
            "than the measured one.")
    endif()

    # Postcondition 2: read the property back for every non-participating
    # source and require it to be set. Postcondition 1 only proves the path
    # arithmetic matched; it says nothing about whether the opt-out actually
    # took effect. If TARGET_DIRECTORY is dropped, moved, or scoped wrongly,
    # the writes land somewhere unread and all sources get the PCH -- with
    # postcondition 1 still passing. This is the check that fails in that case.
    foreach(_src IN LISTS _not_optin)
        get_source_file_property(_skip "${_src}" TARGET_DIRECTORY hven
                                 SKIP_PRECOMPILE_HEADERS)
        if(NOT _skip)
            message(FATAL_ERROR
                "hven: SKIP_PRECOMPILE_HEADERS did not stick for ${_src}. The opt-out "
                "was written but does not read back from the hven target's directory "
                "scope, which means every source in the target is about to be compiled "
                "against the precompiled header -- including TUs measured to be slower "
                "with it or to produce a non-byte-identical object. This is what a "
                "missing or misplaced TARGET_DIRECTORY looks like. Refusing to "
                "configure.")
        endif()
    endforeach()

    # Postcondition 3: the target carries the number of sources this list was
    # written against. A source added without revisiting the PCH decision trips
    # here rather than silently inheriting the unmeasured default.
    if(NOT _total EQUAL _expected_source_count)
        message(FATAL_ERROR
            "hven: the hven target carries ${_total} sources, but the "
            "precompiled-header membership list was written against "
            "${_expected_source_count}. A source was added or removed without "
            "revisiting whether it belongs in the PCH. Measure it (scripts/"
            "check_pch_neutrality.sh proves the byte-identity half), then update "
            "_hven_pch_sources and _hven_expected_source_count together.")
    endif()
endfunction()

set_property(GLOBAL PROPERTY HVEN_PCH_LIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set_property(GLOBAL PROPERTY HVEN_PCH_EXPECTED_SOURCE_COUNT "${_hven_expected_source_count}")
set_property(GLOBAL PROPERTY HVEN_PCH_SOURCES "${_hven_pch_sources}")

cmake_language(DEFER DIRECTORY "${PROJECT_SOURCE_DIR}"
               CALL _hven_apply_pch_membership)
