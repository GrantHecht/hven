################################################################################
# The SQP engine's bench harness — migrated whole at M3 (phase A) as a library
# asset (M0 integration plan §5: the 57-cell replay corpus, gate evaluator and
# escape-reason census are deliverables, not sandbox residue), together with
# the frozen baseline artifacts under bench/baselines/ that the test suite
# re-scores and pins.
#
# Every binary here is a measurement instrument, deliberately NOT registered
# with ctest; each one's correctness gate is a registered test that shares its
# implementation (never a copy). They are BUILT whenever the suite is, because
# an uncompiled probe rots silently — the origin project's own committed
# rationale, kept.
#
# COMPILE FLAGS: ${COMPILE_FLAGS}, same as tests/sqp/CMakeLists.txt — the
# uniform regime (owner ruling, CLAUDE.md §7), adopted at phase-C U0 together
# with the mass re-derivation of every baseline these binaries produce or
# consume. See docs/notes/2026-08-16-m3-u0-design.md.
################################################################################

# hven_sqp_bench — the bench harness and wall-time instrumentation. Its own
# correctness gate is the --self-check flag documented in bench_scale.cpp,
# which the M3 gate (and any benchmark script) runs explicitly.
add_executable(hven_sqp_bench bench_scale.cpp)
target_link_libraries(hven_sqp_bench PRIVATE hven::hven)
target_compile_options(hven_sqp_bench PRIVATE ${COMPILE_FLAGS})
# tests/sqp/support/{parametric_families,scale_problems}.h are
# test-support-only headers (no gtest dependency), included by relative path.
target_include_directories(hven_sqp_bench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")

# hven_sqp_f7_cold — the single-solve F7 cold-start probe behind the archived
# cold-scale study. Committed so those sections stay reproducible; NOT
# ctest-registered (its longest documented invocation runs well over an hour).
add_executable(hven_sqp_f7_cold bench_f7_cold.cpp)
target_link_libraries(hven_sqp_f7_cold PRIVATE hven::hven)
target_compile_options(hven_sqp_f7_cold PRIVATE ${COMPILE_FLAGS})
target_include_directories(hven_sqp_f7_cold PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")

# hven_sqp_tau_bar_sweep_probe — the hs_sweeps.h cold-arm reproduction vehicle
# for the archived elastic-tau tuning note's §4.4 sweep. The shipped build
# only ever reads the one kFunnelTauBar the library ships; SWEEPING it is a
# scratch, patched-header recompile outside this build (the probe's own banner
# documents the exact invocation), never a CMake option here.
add_executable(hven_sqp_tau_bar_sweep_probe tau_bar_sweep_probe.cpp)
target_link_libraries(hven_sqp_tau_bar_sweep_probe PRIVATE hven::hven)
target_compile_options(hven_sqp_tau_bar_sweep_probe PRIVATE ${COMPILE_FLAGS})
target_include_directories(hven_sqp_tau_bar_sweep_probe
                            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")

# hven_sqp_snopt_f7 — the SNOPT measurement binary. GUARDED: exists only when
# the root-level SNOPT detection succeeded — a machine without SNOPT keeps
# building and testing unchanged. SNOPT is commercial and is NOT vendored:
# bench/snopt_f7_driver.h documents the source firewall in full. Its
# correctness gate is tests/sqp/test_snopt_bridge_gate.cpp, which shares this
# target's SnoptF7Driver implementation and IS registered.
if(HVEN_SQP_HAVE_SNOPT)
    add_executable(hven_sqp_snopt_f7 bench_snopt_f7.cpp)
    target_link_libraries(hven_sqp_snopt_f7 PRIVATE hven::hven hven_sqp_snopt)
    target_compile_options(hven_sqp_snopt_f7 PRIVATE ${COMPILE_FLAGS})
    target_include_directories(hven_sqp_snopt_f7 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")
endif()

# hven_sqp_corpus — the replay corpus runner: cell specs, tags, the engine
# interface and the walk engine's own implementation all live in
# corpus_cells.h (a header, deliberately, so tests/sqp/test_corpus_cells.cpp
# shares exactly this implementation rather than a bench-only copy that could
# drift). bench_corpus.cpp is CLI glue only. NOT ctest-registered: its
# baseline sweep touches N = 20000 cells that run for minutes to hours. Its
# correctness gate is tests/sqp/test_corpus_cells.cpp.
# ipm_corpus_leg.cpp is the ONE non-glue TU here: the top-level interior-point
# replay leg (--engine interior). A .cpp rather than a header because it is
# ordinary non-template code with two consumers, and both COMPILE IT: this
# binary, and hven_sqp_tests (tests/sqp/CMakeLists.txt lists it explicitly).
# Until M6 W5 T8.9r it had one -- every interior gate read the committed
# artifact rather than the leg -- and the single-row mode's pin is the first
# assertion that has to call the leg's own functions to make its point.
add_executable(hven_sqp_corpus bench_corpus.cpp ipm_corpus_leg.cpp)
target_link_libraries(hven_sqp_corpus PRIVATE hven::hven)
target_compile_options(hven_sqp_corpus PRIVATE ${COMPILE_FLAGS})
target_include_directories(hven_sqp_corpus PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")

# Every CSV this runner writes carries a provenance header, and the first line
# of it is WHICH BINARY produced the rows. Resolved at CONFIGURE time (not
# build time) because that is when CMake can shell out; a tree with no git
# degrades to the literal string "unknown" rather than failing the configure.
# `--dirty` is deliberately included: a baseline produced from an uncommitted
# tree must say so.
#
# THE TRAP THIS CARRIES, and it bit once (T10b fix round 1): a tree configured
# while clean and REBUILT after an edit emits the stale, clean stamp. Re-configure
# before producing a measurement CSV -- the acceptance dir's PROVENANCE.txt.
execute_process(
    COMMAND git describe --always --dirty --abbrev=12
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE HVEN_SQP_CORPUS_GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE HVEN_SQP_CORPUS_GIT_RC)
if(NOT HVEN_SQP_CORPUS_GIT_RC EQUAL 0 OR HVEN_SQP_CORPUS_GIT_DESCRIBE STREQUAL "")
    set(HVEN_SQP_CORPUS_GIT_DESCRIBE "unknown")
endif()
target_compile_definitions(hven_sqp_corpus PRIVATE
    HVEN_SQP_CORPUS_GIT_DESCRIBE="${HVEN_SQP_CORPUS_GIT_DESCRIBE}")

# The wall-deadline tests in tests/sqp/test_corpus_cells.cpp
# (CorpusRunnerProcess.WallDeadline*) are SUBPROCESS tests — they invoke the
# actual compiled hven_sqp_corpus binary (the deadline enforcement itself is
# fork/exec orchestration in bench_corpus.cpp's main(), not something a gtest
# linked only against corpus_cells.h can reach). hven_sqp_tests already
# exists at this point (tests/ is add_subdirectory'd before bench/ in the
# root CMakeLists.txt), so the dependency and the path macro are wired HERE
# rather than in tests/sqp/CMakeLists.txt, where hven_sqp_corpus would not
# exist yet. Guarded: a tests-off, bench-on configure has no test target to
# decorate.
if(TARGET hven_sqp_tests)
    target_compile_definitions(hven_sqp_tests PRIVATE
        HVEN_SQP_CORPUS_BINARY="$<TARGET_FILE:hven_sqp_corpus>")
    add_dependencies(hven_sqp_tests hven_sqp_corpus)
endif()

# hven_sqp_ssn_safeguard_probe — the reproduction vehicle for the archived SSN
# safeguards note's search and sweep populations. Committed and built so the
# note's evidence cannot rot; NOT ctest-registered (its documented populations
# run for minutes). Its fixtures are tests/sqp/support/ssn_fixtures.h, the
# same file tests/sqp/test_ssn_engine.cpp includes, so the note and the test
# suite cannot drift apart on a fixture. The band variants it compares are
# patched-header recompiles outside this build — see the probe's banner.
add_executable(hven_sqp_ssn_safeguard_probe ssn_safeguard_probe.cpp)
target_link_libraries(hven_sqp_ssn_safeguard_probe PRIVATE hven::hven)
target_compile_options(hven_sqp_ssn_safeguard_probe PRIVATE ${COMPILE_FLAGS})
target_include_directories(hven_sqp_ssn_safeguard_probe
                            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")

# hven_sqp_crossover — the M5 W5 crossover-leg runner: the dual-bind adapter,
# the four legs and the margin arithmetic all live in bench/crossover_legs.h (a
# header, deliberately, so tests/sqp/test_crossover_legs.cpp shares exactly this
# implementation rather than a bench-only copy that could drift).
# bench_crossover.cpp is CLI glue only. NOT ctest-registered: a full sweep of
# the dual-bindable census runs for hours. Its correctness gate is
# tests/sqp/test_crossover_legs.cpp, which pins the adapter's round trip and the
# cross-engine declaration key at a size a test can afford.
add_executable(hven_sqp_crossover bench_crossover.cpp)
target_link_libraries(hven_sqp_crossover PRIVATE hven::hven)
target_compile_options(hven_sqp_crossover PRIVATE ${COMPILE_FLAGS})
target_include_directories(hven_sqp_crossover PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")
# Same configure-time git description hven_sqp_corpus stamps, and for the same
# reason: every CSV this runner writes says which binary produced its rows.
target_compile_definitions(hven_sqp_crossover PRIVATE
    HVEN_SQP_CROSSOVER_GIT_DESCRIBE="${HVEN_SQP_CORPUS_GIT_DESCRIBE}")

# hven_sqp_ipqp_e1_arm — M6 W1 T10's acceptance arm: the IPQP tier over E1's
# 29 constructed cells (A4), the Q4 `ipqp_init_mu` sweep and the A13/A14 wall
# legs. The arm lives in bench/ipqp_e1_arm.h (a header, for the same reason
# corpus_cells.h is one: tests/sqp/test_ipqp_acceptance.cpp gates exactly this
# implementation rather than a bench-only copy). Its `--gate` mode IS
# ctest-registered, below, under a label the default run excludes; the sweep
# modes are not (one A4 pass at nx = 1e5 runs for tens of minutes).
add_executable(hven_sqp_ipqp_e1_arm ipqp_e1_arm.cpp)
target_link_libraries(hven_sqp_ipqp_e1_arm PRIVATE hven::hven)
target_compile_options(hven_sqp_ipqp_e1_arm PRIVATE ${COMPILE_FLAGS})
target_include_directories(hven_sqp_ipqp_e1_arm PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sqp")
target_compile_definitions(hven_sqp_ipqp_e1_arm PRIVATE
    HVEN_SQP_E1_ARM_GIT_DESCRIBE="${HVEN_SQP_CORPUS_GIT_DESCRIBE}")

# HVEN_BENCH_E1_GENERATOR_ARITHMETIC — the arm's MATCHED-ARITHMETIC build, and
# the ONLY documented producer of regen-certificate-fpcontract-off.csv.
#
# E1's generator was compiled at plain `-O3 -DNDEBUG -std=c++20`
# (docs/notes/data/2026-08-m6-e1-acquisition/generator/build_e1.sh): baseline
# ISA, no fast-math, no FMA contraction. This repository's uniform regime
# (CLAUDE.md §7) adds `-march=native -ffast-math`, and all three differences
# matter — contraction moves the constructed `g` and the verifier's
# cancellation residual, while the ISA and reassociation flags move Eigen's
# SimplicialLDLT and hence `licq_d_min`. Turning ALL of them off for THIS
# TARGET ONLY reproduces the artifact's recorded digits exactly (225/225);
# `-ffp-contract=off` alone leaves 14 `licq_d_min` cells differing, measured.
#
# Default OFF: the shipped arm is never built this way and the uniform regime
# is not weakened. Never set it for a measurement CSV other than the
# certificate — see docs/notes/data/2026-08-m6-w1-acceptance/README.md §1.
option(HVEN_BENCH_E1_GENERATOR_ARITHMETIC
       "Build hven_sqp_ipqp_e1_arm at E1's generator arithmetic (regeneration certificate only)"
       OFF)
if(HVEN_BENCH_E1_GENERATOR_ARITHMETIC)
    target_compile_options(hven_sqp_ipqp_e1_arm PRIVATE -ffp-contract=off -fno-fast-math)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        target_compile_options(hven_sqp_ipqp_e1_arm PRIVATE -march=x86-64 -mtune=generic)
    endif()
endif()

# The A4 gate, EXECUTABLE, but registered only under -DHVEN_A4_GATE=ON
# (default OFF): it re-solves all 29 cells at both sizes, which CI's bare
# `ctest --timeout 300` (.github/workflows/ci.yml:199) has no budget for.
# T10b: it now EXITS 0 -- E1 criteria 1/2/4 hold at the measured
# `ipqp_init_mu` but for the one named cell, which the arm reports by name.
# See README.md section 2 and its dated T10b block for the verdict.
option(HVEN_A4_GATE "Register the A4Gate ctest entry (green but for one named cell)" OFF)
if(HVEN_A4_GATE)
    add_test(NAME A4Gate.TheTaxonomyAtBothSizesUnderTheMeasuredDefault
             COMMAND hven_sqp_ipqp_e1_arm --gate --out
                     "${CMAKE_CURRENT_BINARY_DIR}/a4-gate-run.csv")
    set_tests_properties(A4Gate.TheTaxonomyAtBothSizesUnderTheMeasuredDefault PROPERTIES
        LABELS "a4_gate" TIMEOUT 5400 RUN_SERIAL TRUE)
endif()
