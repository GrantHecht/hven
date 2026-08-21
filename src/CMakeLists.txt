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
    # out of a drivers/ one -- see the file's own banner (M3 phase-C T3, closing
    # the T1 review's finding F1).
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
# Interior-point engine (ex-PSIOPT) — migrated as-is from the standalone
# psiopt/ project the origin project previously hosted (hven M2
# milestone, engine-move task). The split below —
# drivers/ (the top-level InteriorPointSolver algorithm),
# model/ (the NLPProblem/NLPSolver contract, its adapter, and the
# partitioned-evaluation NonLinearProgram engine), and
# interior/ + globalization/ under include/hven/detail/ (barrier /
# interior-point machinery) — mirrors psiopt's own internal grouping: a
# path map onto hven's directory conventions, not a reorganization. See
# docs/dev notes for the migration's file-by-file record. NonLinearProgram
# later moved from drivers/ into model/ as an as-is relocation.
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
# SQP engine
#
# The SQP engine itself has not been migrated into this repository yet — it
# arrives with a later milestone — so today its code lives entirely in
# include/hven/drivers/ and include/hven/detail/. This is the first piece of it
# to acquire a translation unit, carved out by M3 phase-C task T1 under
# CLAUDE.md §5's rule that printing belongs in a .cpp TU. The file's own banner
# carries the argument; more of these appear as the T-series lands, and they
# collect here rather than in the interior-point block above, which is a record
# of a migration this file is not part of.
#
# T3 adds the second: drivers/sqp_options.cpp, the SqpOptions boundary
# validation SqpDriver's constructor runs (CLAUDE.md §5 names options code
# alongside printing). Its banner also carries the ONE flag premise in this
# tree that a comparison's spelling depends on -- `-fno-finite-math-only`,
# without which `!(x > 0.0)` may be rewritten to `x <= 0.0` and stop rejecting
# NaN -- and T3's battery pins that premise by disassembling this TU.
#
# T4 adds the third, and the first that COMPUTES with doubles:
# globalization/sqp/funnel.cpp, FunnelStrategy's acceptance state machine. It
# mirrors include/hven/detail/globalization/sqp/ (the `sqp/` level is what keeps
# it apart from the interior-point engine's own globalization code, which lives
# in drivers/interior_point_solver_globalization.cpp). Because the arithmetic
# it moves is re-associable under -ffast-math, its proof is bit-identity of
# every asserted counter across the boundary -- which the ONE uniform flag
# regime U0 established is what makes it available at all.
#
# T5 adds the fourth, and it is the big one: drivers/sqp_driver.cpp, holding
# SqpDriver::solve_impl -- the major loop itself -- together with everything the
# loop orchestrates (the trust-region update logic, the ledger tail, the exit
# helpers, the SSN tier's accessors) and the public solve() overloads. Every
# member function of the class except its two constructors is now defined there;
# the class, its data members and its documentation stay in the header. At
# ~2 330 moved lines it is the largest body this series relocates, and the one
# the plan's build-time case rests on. Its banner carries the object-file
# evidence that the loop's per-element calls were ALREADY out-of-line at -O3, so
# the boundary removes no inlining, and the S3 ruling under which the TR update
# functions travel with the loop while the TR policy constants stay in
# globalization.
#
# T6 adds the fifth: globalization/sqp/soc_elastic_restoration.cpp, holding the
# three step-recovery tiers the driver reaches for when a plain subproblem does
# not produce a usable step -- the second-order correction, the elastic l1
# exact-penalty tier, and the restoration phase's feasibility-model wrapper.
# ONE TU for the three headers, because soc.h and restoration.h both declare
# themselves not self-contained (they need NlpEval, which only
# drivers/sqp_driver.h defines), so each implementation TU would have to parse
# that header; and because the three tiers are entered from one place in
# solve_impl and are one recovery path. Its banner carries the object-file
# evidence that every definition it moves was ALREADY called through a direct
# relocation at -O3, and the one function the plan named that measurement kept
# in the header instead (set_elastic_penalty, inlined at every use).
#
# T7 adds the sixth, and the first under warmstart/: warmstart/warm_start.cpp,
# holding from_interior_point -- the interior-point crossover's ingest, which
# runs at most once per solve. Its header keeps ONE of the three `template`
# declarations in the SQP tree, detail::ip_activity_threshold, and that stays
# in the header for a reason stronger than the template exemption: it takes its
# residual as a template parameter so the ingest can pass an Eigen expression
# rather than materialise two n-sized temporaries at n = 1e6. Because this TU
# includes the header, that template is still inlined into the body exactly
# where it was.
#
# T8 adds the seventh and closes the T-series: warmstart/continuation.cpp,
# holding run_continuation and the input validation it runs first. One sweep
# per call and one whole SqpDriver::solve() per step, so the call into it is
# arithmetically the cheapest thing in any program that makes it. Three symbols
# move rather than two: run_continuation, continuation_detail::validate (its
# only caller called it exactly once and never inlined it), and the out-of-line
# operator() of run_continuation's own `record` closure, which lived in its own
# COMDAT section and is defined inside the moved body.
################################################################################

target_sources(hven PRIVATE
    drivers/sqp_driver.cpp
    drivers/sqp_options.cpp
    drivers/sqp_print.cpp
    globalization/sqp/funnel.cpp
    globalization/sqp/soc_elastic_restoration.cpp
    warmstart/continuation.cpp
    warmstart/warm_start.cpp
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
# M3 PHASE-C T1/T2 ADDENDUM (2026-08-17, task T9's obligation on a commit that
# adds a source). Re-measured on the post-U0 unified flag set, same method,
# same box (AMD Ryzen 7 5800X3D, clang 22.1.8, Release, CCACHE_DISABLE=1,
# minimum of three compiles per arm, the two arms being a normal configure and
# one with -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON). The absolute seconds are
# NOT comparable with the table above -- that sweep predates the flag
# unification -- so two TUs already on the opt-in list were re-measured in the
# same pass as controls, and they reproduce their verdict:
#
#   drivers/interior_point_solver.cpp             6.95 -> 4.32  -2.63  identical
#   model/nlp_solver.cpp                          4.07 -> 1.84  -2.23  identical
#     ------ the two sources this commit adds: faster, NOT byte-identical ----
#   drivers/sqp_print.cpp                         4.82 -> 2.97  -1.85  differs
#   core/ledger.cpp                               2.98 -> 1.53  -1.46  differs
#     ------ pre-existing gap, measured in the same pass -------------------
#   kkt/kkt_calls.cpp                             3.10 -> 1.55  -1.55  differs
#
# So NEITHER new source joins the opt-in list: both clear bar 1 comfortably
# and both fail bar 2, which puts them in the same "faster but not
# byte-identical" group as most of the library. The phase-C plan's written
# prediction was that sqp_print.cpp would qualify and that ledger.cpp might be
# SLOWER with the PCH; both halves are falsified here by measurement, which is
# what the plan asked for rather than the prediction being carried forward.
#
# kkt/kkt_calls.cpp is not a source this commit adds -- it was added earlier
# and never appeared in the sweep above, so the "all 18 Linux TUs" claim had
# quietly become 18 of 19. Measuring it here closes that gap without changing
# its (already correct, default) opted-out status. With this addendum the
# Linux table covers all 21 sources.
#
# M3 PHASE-C T3 ADDENDUM (2026-08-17). Same method, same box, same session
# shape as the T1/T2 addendum: two configures differing only in
# -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON, CCACHE_DISABLE=1, the TU's own compile
# command replayed three times per arm, minimum reported, objects byte-compared.
# The two opt-in controls were re-measured again and reproduce (6.96 -> 4.32 and
# 4.07 -> 1.83, both identical), so the arms are comparable with the T1/T2 rows.
#
#     ------ the source this commit adds: faster, NOT byte-identical ---------
#   drivers/sqp_options.cpp                       3.10 -> 1.60  -1.49  differs
#
# It does NOT join the opt-in list: bar 1 is cleared comfortably, bar 2 is not.
# The plan predicted this TU would resemble drivers/interior_point_solver_
# settings.cpp (opt-in, identical); measured, it lands in the large
# "faster but not byte-identical" group instead, exactly as T1's two did. The
# measurement is recorded rather than the prediction. Linux table: 22 sources.
#
# NOTE ON MEASURING AN OPTED-OUT TU. An opted-out source carries no PCH flags
# in the normal configure, so a "with-PCH" arm read straight out of that build's
# compile_commands.json would not consume the PCH and would report a delta of
# zero. The number above comes from a THROWAWAY configure with this source
# temporarily added to _hven_pch_sources, and the list here is unchanged.
#
# T3 FOLLOW-UP (same session, same method and controls). The commit that adds
# core/enum_names.cpp also SHRINKS drivers/sqp_print.cpp by two switches, so its
# existing row above is re-measured rather than assumed to still hold:
#
#     ------ the source this commit adds: faster, NOT byte-identical ---------
#   core/enum_names.cpp                           1.41 -> 0.14  -1.27  differs
#     ------ re-measured because its content shrank -------------------------
#   drivers/sqp_print.cpp                         4.80 -> 2.95  -1.85  differs
#
# enum_names.cpp does not join the opt-in list either -- same verdict, same
# reason. Its row is also the clearest statement of what this PCH actually
# buys: a 70-line TU whose own code is two switches costs 1.41 s to compile
# without the PCH and 0.14 s with it, i.e. essentially ALL of its cost is the
# shared-header parse floor. And sqp_print.cpp's row is unmoved to within the
# measurement's own spread (4.82 -> 2.97 before, 4.80 -> 2.95 after) although it
# lost two functions, which is the same fact from the other side. Linux table:
# 23 sources.
#
# M3 PHASE-C T4 ADDENDUM (2026-08-18). Same method, same box, same two controls,
# and the same throwaway-configure technique the note above describes (this
# source is opted out, so the with-PCH arm has to come from a configure that
# temporarily lists it). The controls reproduce their T3 readings to within a
# hundredth of a second (6.98 -> 4.32 and 4.09 -> 1.84, both identical), so the
# arms are comparable with every row above:
#
#     ------ the source this commit adds: faster, NOT byte-identical ---------
#   globalization/sqp/funnel.cpp                  3.00 -> 1.45  -1.55  differs
#
# It does NOT join the opt-in list: bar 1 cleared, bar 2 failed. That is the
# FOURTH consecutive TU carved out of the SQP tree to land in the "faster but
# not byte-identical" group (sqp_print.cpp, ledger.cpp, sqp_options.cpp,
# enum_names.cpp before it), and the plan's per-TU predictions have now been
# falsified every time they were made. The measurement is what is recorded.
# Linux table: 24 sources.
#
# M3 PHASE-C T5 ADDENDUM (2026-08-19). Same method, same box, same two controls,
# same throwaway-configure technique. The controls reproduce their T4 readings
# to within 0.02 s (6.96 -> 4.31 and 4.07 -> 1.84, both identical), so this row
# is comparable with every row above:
#
#     ------ the source this commit adds: faster, NOT byte-identical ---------
#   drivers/sqp_driver.cpp                       10.46 -> 8.56  -1.90  differs
#
# It does NOT join the opt-in list: bar 1 cleared, bar 2 failed. Fifth
# consecutive SQP-side TU in the "faster but not byte-identical" group.
#
# THE ROW'S SHAPE IS NEW AND IS THE POINT. Every other source in this table
# costs 1.3-7 s without the PCH, of which the ~3 s shared-header parse floor is
# most or all -- which is why their with-PCH times collapse toward zero and why
# their savings cluster at -1.5 to -2.7 s. sqp_driver.cpp costs 10.46 s, the
# most expensive TU in the library by a factor of 1.5 over the next
# (interior_point_solver.cpp at 6.96), and its saving is SMALLER in absolute
# terms than either control's. The reason is arithmetic: ~2 330 lines of driver
# body is real work no precompiled header can amortize, so only the floor part
# of its cost is removable. Reading the two facts together -- the largest TU in
# the tree, and the smallest PCH leverage per second of compile -- is the
# honest summary of what carving the major loop out costs the build, and
# T5's P-BUILD (task-c-t5-report.md) measures the whole-tree consequence.
# Linux table: 25 sources.
#
# M3 PHASE-C T6 ADDENDUM (2026-08-19). Same method, same box, same two
# controls, same throwaway-configure technique. The controls reproduce their T5
# readings to within 0.07 s (7.03 -> 4.36 and 4.10 -> 1.84, both identical), so
# this row is comparable with every row above:
#
#     ------ the source this commit adds: faster, NOT byte-identical ---------
#   globalization/sqp/soc_elastic_restoration.cpp 5.58 ->  3.70  -1.87  differs
#
# It does NOT join the opt-in list: bar 1 cleared, bar 2 failed. SIXTH
# consecutive SQP-side TU in the "faster but not byte-identical" group. Six for
# six is no longer a run of coincidences, and it is worth naming the mechanism
# the earlier notes already identified: these TUs include a different header
# set in a different order from src/hven_pch.h's, so prefixing the shared block
# changes template instantiation and therefore emission order. Every SQP-side
# carve inherits that property from the headers it must include, so the
# expectation for the remaining carves is the same -- stated as an expectation
# to be measured, not a prediction to be assumed.
#
# The row's shape is unremarkable next to T5's: 5.58 s puts it third in the
# table behind sqp_driver.cpp (10.46) and interior_point_solver.cpp (7.03), and
# its -1.87 s saving sits in the same -1.5 to -2.7 s band as everything else.
# That is what a TU costs when most of its cost IS the shared-header parse
# floor: ~370 moved lines is little enough work that the floor dominates, which
# is exactly the contrast T5's note draws.
# Linux table: 26 sources.
#
# M3 PHASE-C T7 ADDENDUM (2026-08-19). Same method, same box, same two
# controls, same throwaway-configure technique. The controls reproduce their T6
# readings to within 0.08 s (6.95 -> 4.32 and 4.08 -> 1.84, both identical), so
# this row is comparable with every row above:
#
#     ------ the source this commit adds: faster, NOT byte-identical ---------
#   warmstart/warm_start.cpp                      3.25 ->  1.75  -1.50  differs
#
# It does NOT join the opt-in list: bar 1 cleared, bar 2 failed. Seventh
# consecutive SQP-side TU in that group, and the expectation the T6 addendum
# stated is now measured rather than assumed for one more row. At 3.25 s this
# is the SMALLEST of the six SQP-side carves -- about the shared-header parse
# floor plus a hundred lines of ingest -- which is why its absolute saving is
# the smallest and why its RATIO (-46 %) is nevertheless the largest of them.
# Linux table: 27 sources.
#
# M3 PHASE-C T8 ADDENDUM (2026-08-19), closing the T-series. Same method, same
# box, same two controls, same throwaway-configure technique. The controls
# reproduce their T7 readings to within 0.01 s (6.96 -> 4.33 and 4.08 -> 1.84,
# both identical), so this row is comparable with every row above:
#
#     ------ the source this commit adds: faster, NOT byte-identical ---------
#   warmstart/continuation.cpp                    6.23 ->  4.35  -1.87  differs
#
# It does NOT join the opt-in list: bar 1 cleared, bar 2 failed. EIGHTH
# consecutive SQP-side TU in that group, and the T-series is now closed with a
# clean sweep: every one of the seven TUs T1-T8 added to this target compiles
# faster with the PCH and none produces a byte-identical object. The mechanism
# was named in the T6 addendum and has not needed revising -- these TUs include
# a different header set in a different order from src/hven_pch.h's, so
# prefixing the shared block changes template instantiation and therefore
# emission order. Anyone proposing to admit them is proposing to relax bar 2,
# which docs/build.md is explicit is a numerics-governance decision and not a
# build one. Eight measurements is now the evidence for that conversation.
# Linux table: 28 sources.
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
set(_hven_expected_source_count 28)

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
