################################################################################
# hven library sources -- appended to the `hven` target created in the root
# CMakeLists.txt (target_sources, not add_library: the target's include dirs, linked
# dependencies and compile options are already set up there).
#
# One block per component, in the layer order of CLAUDE.md section 2 / tests/core/test_layering.cpp:
#   core < util < linear < model < kkt < qp (walk, ssn, ipqp) < warmstart < solver < ipm, sqp
#   < trace. Within a block the sources are sorted. A TU lives in src/<component>/ named after
#   the header it implements (a TU goes in the deepest src directory that
#   contains every header it implements); where the name differs, the first line of the file
#   says which header it implements.
#
# Each file's own banner carries its section-5 argument (why it is a TU, what its boundary
# costs); the dated carve arguments that used to sit here are in docs/build.md, "History: the
# source-list banners".
################################################################################

# --- core: shared vocabulary ----------------------------------------------------------------
target_sources(hven PRIVATE
    core/enum_names.cpp         # display strings of the solver enums core/ owns
    core/ledger.cpp
    core/pattern_hash.cpp
    core/version.cpp
)

# --- util: thread pool, timer, core count ---------------------------------------------------
target_sources(hven PRIVATE
    util/get_core_count.cpp
    util/memory_management.cpp
    util/thread_pool.cpp
)

# --- linear: the sparse symmetric factor, its backends, and the dense arm -------------------
# The sparse backends are split at the source level, not inside a shared TU: MKL Pardiso's
# phase-driven session and Accelerate's factor objects share nothing below the public header,
# so each platform compiles only its own pair (a file that #if's itself away to nothing is a TU
# the build should not be compiling). The dense arm and the matching/scaling are backend
# independent.
target_sources(hven PRIVATE
    linear/dense_session.cpp
    linear/dense_symmetric_factor.cpp
    linear/matching_scaling.cpp
)
if(NOT APPLE)
    target_sources(hven PRIVATE
        linear/pardiso_session.cpp
        linear/symmetric_factor_mkl.cpp
    )
else()
    # Configured, built and tested against the real Accelerate framework on every macOS CI run;
    # scripts/check_accelerate_syntax_linux.sh is only a narrower structural cross-check.
    target_sources(hven PRIVATE
        linear/accelerate_session.cpp
        linear/symmetric_factor_accelerate.cpp
    )
endif()

# --- model: the provider-side problem contract and the engines' problem forms ---------------
target_sources(hven PRIVATE
    model/assembly_declaration.cpp
    model/nlp.cpp
    model/nlp_adapter.cpp
    model/nlp_model_assembly.cpp
    model/nlp_problem_model.cpp
    model/non_linear_program.cpp
    model/problem.cpp
    model/validation_errors.cpp  # the cold throw paths of the contract's inline checks
)

# --- kkt: KKT systems shared by the engines -------------------------------------------------
target_sources(hven PRIVATE
    kkt/kkt_factor.cpp           # detail/kkt/kkt_factor.h (QP-side KktFactor)
    kkt/kkt_factorization.cpp    # detail/kkt/kkt_factorization.h (IPM/IPQP KktFactorization)
)

# --- qp: the three QP engines (walk = active-set walk, ssn, ipqp) ----------------------------
# Each engine is a plain non-template class whose members travel into ONE TU on purpose (the
# per-row and per-entry helpers must stay visible to the inliner at the sites that had them as
# header siblings); the arguments are in the files' banners and docs/build.md.
target_sources(hven PRIVATE
    qp/ipqp/ipqp_engine.cpp
    qp/ipqp/ipqp_kkt_layout.cpp
    qp/ipqp/ipqp_options.cpp     # qp/qp_options.h: validate(const IpqpOptions &)
    qp/ssn/ssn_engine.cpp
    qp/walk/walk_engine.cpp
    qp/walk/ws_independence.cpp
)

# --- warmstart: the warm-start currency and the hand-off bridges ----------------------------
target_sources(hven PRIVATE
    warmstart/crossover.cpp
    warmstart/ipm_polish_extension.cpp
    warmstart/warm_start_data.cpp
)

# --- solver: what both solvers present ------------------------------------------------------
target_sources(hven PRIVATE
    solver/callback_rows.cpp
    solver/common_options.cpp
    solver/counters_dump.cpp
    solver/result_message.cpp
    solver/solve_result.cpp
)

# --- trace: the sink interface and its two sinks --------------------------------------------
target_sources(hven PRIVATE
    trace/console_trace_sink.cpp
    trace/json_lines_trace_sink.cpp   # trace/json_lines_trace_sink.h (schema v1 serializer)
    trace/trace.cpp
)

# --- ipm: the interior-point solver ---------------------------------------------------------
target_sources(hven PRIVATE
    ipm/ipm_options.cpp
    ipm/ipm_phase_status.cpp
    ipm/ipm_solver.cpp
    ipm/ipm_solver_globalization.cpp   # detail/ipm/globalization/ + detail/ipm/ bodies
    ipm/ipm_solver_handle.cpp
    ipm/solver_init.cpp
)

# --- sqp: the SQP solver --------------------------------------------------------------------
target_sources(hven PRIVATE
    sqp/assembly_eval_seam.cpp
    sqp/continuation.cpp
    sqp/equality_basis.cpp
    sqp/globalization/funnel.cpp
    sqp/globalization/soc_elastic_restoration.cpp   # one TU for the three step-recovery tiers
    sqp/problem_scaling.cpp
    sqp/quasi_newton/lbfgs_damped.cpp
    sqp/quasi_newton/mixed_lift.cpp
    sqp/quasi_newton/qn_lift_bridge.cpp
    sqp/quasi_newton/qn_models.cpp
    sqp/sqp_options.cpp          # NaN-rejection premise: see the file's banner
    sqp/sqp_print.cpp
    sqp/sqp_solver.cpp
    sqp/sqp_solver_handle.cpp
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
# The per-TU measurement table of the original sweep (no-PCH vs PCH time, byte-identity) is a
# dated record: docs/build.md, "History: the PCH sweep table".
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
    ipm/ipm_solver.cpp
    ipm/ipm_options.cpp
    model/non_linear_program.cpp
    model/nlp_adapter.cpp
)

# THE PCH IS CLANG-ONLY (M7 W2 E4 fix round 4). Under any other front end this
# target configures with NO precompiled header, and the reason is a correctness
# one on top of the measurement one this file and docs/build.md already carry.
#
# The measurement one: the per-TU table above is a clang measurement throughout,
# and docs/build.md's disclosed limitations record that bar 2 below -- the
# object stays byte-identical to the non-PCH build -- does NOT hold under GCC,
# where three of the opted-in TUs emit a different object with the PCH than
# without it. A bar a front end cannot clear is not a bar it should be admitted
# under.
#
# The correctness one, which is what this guard was added for. A GCC .gch does
# not record its inputs for validation. What keeps a consuming TU from re-reading
# a `#pragma once` header already in the PCH is a table of (file size, content
# md5) pairs baked into the .gch when it was built, and GCC never re-checks that
# those pairs still describe the files on disk. So a .gch stale for ONE header --
# even one whose only staleness is a comment -- silently makes every consuming TU
# re-parse it, and the TU dies on "multiple definition of 'enum class ...'".
# ccache serves exactly that .gch: its preprocessor-mode key is the preprocessed
# text, which a comment-only, line-count-preserving header edit leaves
# byte-identical, so a .gch built before such an edit is replayed while the file
# on disk no longer matches the table inside it. MEASURED here on GCC 16.2.1
# with ccache 4.12.3: such an edit to include/hven/core/solver_status.h gets a
# `preprocessed_cache_hit` and a served .gch byte-identical to the pre-edit one,
# and compiling src/ipm/ipm_solver.cpp against that .gch fails on every enum
# in that header. That -- not the sources -- is what failed M7 W2 E4's install
# smoke, whose default arm is the system compiler, i.e. GCC on this box and on
# CI's Linux runner.
#
# Clang keeps the PCH and is not exposed to this: it records each input's size
# and modification time in the AST file and REFUSES one whose inputs moved
# ("file ... has been modified since the precompiled header was built"), so the
# same stale artifact is a loud refusal rather than a silent re-parse. Clang is
# also the toolchain CLAUDE.md documents, every preset selects and every CI lane
# uses -- including the install smoke's second, `-DCMAKE_CXX_COMPILER=clang++`
# arm, so the PCH path stays exercised end to end by the gate that broke.
#
# The membership machinery below is deliberately NOT guarded with this: its
# opt-out loop and its three postconditions are what stop a new TU from quietly
# joining the PCH, and they are worth running on every configure whatever the
# front end is. With no PCH in the target the opt-out is inert, not wrong.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_precompile_headers(hven PRIVATE hven_pch.h)
endif()

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
# Current count: 59 (RS-R1 added qp/ipqp/ipqp_options.cpp and renamed trace_writer.cpp to
# json_lines_trace_sink.cpp). Every source since the 29th has stayed OUT of the PCH on the
# unmeasured-default footing; the per-source history is in docs/build.md, "History: the source
# count".
set(_hven_expected_source_count 59)

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
