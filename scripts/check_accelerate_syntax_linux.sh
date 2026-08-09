#!/usr/bin/env bash
# Linux syntax-only structural check of hven's Apple Accelerate backend
# (hven/detail/linear/accelerate_session.h/.cpp,
# src/linear/symmetric_factor_accelerate.cpp) AND of the Apple-gated halves
# of the three test files that exercise it
# (tests/linear/test_fault_injection.cpp's `#if defined(__APPLE__)` block,
# tests/linear/test_symmetric_factor_evidence_invariants.cpp's one
# `#if defined(__APPLE__)` assertion,
# tests/linear/test_symmetric_factor_pardiso_only_options.cpp's
# `#if defined(__APPLE__)` block, which covers the ordering/weighted_matching
# throw path). The test-file checks were added after
# review of the Accelerate backend work: before this lane existed, the
# Accelerate half of test_fault_injection.cpp -- the SparseGetInertia-failure
# / kQueryFailed contract cell -- had never been seen by any compiler, on any
# platform; this lane is what first raised its claim ceiling from "never
# compiled" to "syntax-checked against stubs", matching the two backend TUs
# below. macOS CI now compiles and executes that same test against the real
# Accelerate framework, so this lane's syntax check is no longer the only
# Apple-test evidence for that cell -- see docs/testing.md. None of the
# three test files actually needs the Accelerate stub itself: all reach the
# Apple-gated code purely through hven/linear/symmetric_factor.h's
# backend-neutral public API, so their
# checks below need real gtest headers but not the stub Accelerate.h.
#
# hven's psiopt/tycho_sqp precedents were searched for an existing
# "Apple-TU compile lane on Linux" trick (the pattern Task-6's brief asked
# to mirror if one exists) and none was found: tycho's own Accelerate
# hygiene canary (scripts/check_accelerate_warnings.sh) is Apple-only and
# exits immediately on any other platform. So this script is the fallback
# the brief specifies instead: a `clang++ -fsyntax-only` pass against a
# minimal, hand-written stub of <Accelerate/Accelerate.h>
# (scripts/accelerate_syntax_stub/), since the real Apple SDK header does
# not exist on this machine.
#
# The three test-file checks also need `-D__APPLE__` to activate their guarded
# blocks at all, which drags in googletest's OWN `__APPLE__` platform-
# detection path (gtest-port.h / gtest-port-arch.h) -- NOT part of hven's
# Accelerate surface, but real macOS SDK headers gtest itself includes
# unconditionally whenever `__APPLE__` is defined. Two more minimal stubs,
# AvailabilityMacros.h and TargetConditionals.h (same stub directory), exist
# solely to get past that, and are documented as such in their own headers.
#
# WHAT THIS PROVES: hven's own Accelerate-backend source is internally
# self-consistent against the API surface as this stub declares it --
# correct C++ syntax, no typos in Accelerate type/function names as spelled
# here, correct member access, correct template/overload resolution given
# the stub's declared signatures, no missing #includes. It also proves the
# non-Apple-specific parts (hven's own contract logic, Eigen usage, fmt
# usage) compile cleanly when isolated from the rest of the codebase. For
# the three test files: it proves the `#if defined(__APPLE__)` blocks --
# including hven::linear::detail::testing::InertiaQueryFaultInjector, the
# fabrication-fix-2 test's Linux-side evidence, now supplemented by macOS
# CI's real execution of the same test against real Accelerate -- are
# themselves syntactically well-formed and type-check against
# symmetric_factor.h's real public API, given a real gtest and the two
# environment stubs above.
#
# WHAT THIS DOES NOT PROVE:
#   - That the stub's declarations (field names, types, function
#     signatures) actually match Apple's real <Accelerate/Accelerate.h>.
#     The stub was hand-written from reading two Mac-verified downstream
#     ports' call sites, not from Apple's header, which is unavailable
#     here.
#   - Linking. No Accelerate library exists to link against on Linux, so
#     this is a compile-only, not a build, check.
#   - Runtime behavior of any kind -- return codes, error paths, numeric
#     results, or the reportError callback's actual invocation semantics.
#     This script proves AccelerateInertiaQueryFaultInjection.* syntax-sound,
#     nothing more; macOS CI now executes that test against real Accelerate.
#   - That CMake's actual Apple configuration (cmake/FindAccelerateSparse.cmake,
#     the ACCELERATE_NEW_LAPACK / USE_ACCELERATE_SPARSE defines) produces a
#     working build; this script bypasses CMake and hven's own build flags
#     entirely, invoking the compiler directly.
#
# The claim ceiling for anything this script demonstrates is exactly:
# "syntax-checked against stubs on Linux." Nothing here may be cited as
# evidence the Accelerate backend compiles, links, or runs on Apple
# hardware -- see docs/testing.md.
#
# Usage: scripts/check_accelerate_syntax_linux.sh
# Environment overrides: CXX (default: clang++)

set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

CXX="${CXX:-clang++}"

echo "==== hven Accelerate backend: Linux syntax-only structural check ===="
echo "CXX   : ${CXX} ($(${CXX} --version | head -1))"
echo "Claim : syntax-checked against stubs on Linux -- see this script's own"
echo "        header comment for exactly what that does and does not prove."
echo "======================================================================="

STATUS=0

check_one() {
    local src="$1"
    shift
    echo
    echo "---- ${src} $* ----"
    if "${CXX}" -std=c++20 -fsyntax-only \
        -DEIGEN_INITIALIZE_MATRICES_BY_ZERO \
        "$@" \
        -I "${SCRIPT_DIR}/accelerate_syntax_stub" \
        -I "${REPO_ROOT}/include" \
        -isystem "${REPO_ROOT}/dep/eigen" \
        -isystem "${REPO_ROOT}/dep/fmt/include" \
        "${src}"
    then
        echo "OK (syntax-only, stubbed Accelerate)"
    else
        echo "FAILED"
        STATUS=1
    fi
}

check_one "${REPO_ROOT}/src/linear/accelerate_session.cpp"
check_one "${REPO_ROOT}/src/linear/symmetric_factor_accelerate.cpp"
# Also with HVEN_TESTING defined, the same way hven_fault_injection_tests
# compiles this file's own copy (tests/CMakeLists.txt) -- covers the
# fault-injection call site's #ifdef branch, which the check above never
# instantiates.
check_one "${REPO_ROOT}/src/linear/symmetric_factor_accelerate.cpp" -DHVEN_TESTING=1

# ---- Apple-gated test-file halves ------------------------------------------
#
# All three files below reach their `#if defined(__APPLE__)` code purely through
# hven/linear/symmetric_factor.h's backend-neutral public API, so they need
# real gtest headers (not the Accelerate stub) plus -D__APPLE__ to activate
# the guarded block. gtest is a CMake FetchContent dependency, not vendored
# in dep/, so it is only present once this repo has been configured at least
# once; this check degrades to a clearly-labeled SKIP (not a failure) rather
# than forcing a build as a side effect of a syntax-check script.
GTEST_INCLUDE=""
for candidate in \
    "${REPO_ROOT}/build/_deps/googletest-src/googletest/include" \
    "${REPO_ROOT}/build-debug/_deps/googletest-src/googletest/include"
do
    if [[ -f "${candidate}/gtest/gtest.h" ]]; then
        GTEST_INCLUDE="${candidate}"
        break
    fi
done

TEST_HALVES_CHECKED=0
if [[ -z "${GTEST_INCLUDE}" ]]; then
    echo
    echo "---- Apple-gated test-file halves ----"
    echo "SKIP: no fetched googletest include dir found under build*/_deps/"
    echo "      googletest-src/googletest/include -- configure the project"
    echo "      at least once (cmake --preset linux-clang-release) so"
    echo "      FetchContent has pulled it, then re-run this script."
else
    TEST_HALVES_CHECKED=1
    check_one "${REPO_ROOT}/tests/linear/test_fault_injection.cpp" \
        -D__APPLE__ -DHVEN_TESTING=1 -isystem "${GTEST_INCLUDE}"
    check_one "${REPO_ROOT}/tests/linear/test_symmetric_factor_evidence_invariants.cpp" \
        -D__APPLE__ -isystem "${GTEST_INCLUDE}"
    check_one "${REPO_ROOT}/tests/linear/test_symmetric_factor_pardiso_only_options.cpp" \
        -D__APPLE__ -isystem "${GTEST_INCLUDE}"
fi

echo
# In CI a SKIP means the step checked less than it claims to gate: fail hard.
if [[ "${TEST_HALVES_CHECKED}" -eq 0 && -n "${CI:-}" ]]; then
    echo "FAIL (CI): the Apple-gated test halves were SKIPPED (no fetched gtest);"
    echo "           this step must not go green having checked nothing."
    STATUS=1
fi

if [[ "${STATUS}" -eq 0 ]]; then
    if [[ "${TEST_HALVES_CHECKED}" -eq 1 ]]; then
        echo "==== PASS: Accelerate-backend TUs and Apple-gated test halves are syntax-clean ===="
    else
        echo "==== PASS (partial): Accelerate-backend TUs are syntax-clean;"
        echo "     Apple-gated test halves were SKIPPED (no fetched gtest) and are NOT claimed clean ===="
    fi
else
    echo "==== FAIL: see above ===="
fi
exit "${STATUS}"
