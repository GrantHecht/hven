#!/usr/bin/env bash
# Linux syntax-only structural check of hven's Apple Accelerate backend
# (hven/detail/linear/accelerate_session.h/.cpp,
# src/linear/symmetric_factor_accelerate.cpp).
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
# WHAT THIS PROVES: hven's own Accelerate-backend source is internally
# self-consistent against the API surface as this stub declares it --
# correct C++ syntax, no typos in Accelerate type/function names as spelled
# here, correct member access, correct template/overload resolution given
# the stub's declared signatures, no missing #includes. It also proves the
# non-Apple-specific parts (hven's own contract logic, Eigen usage, fmt
# usage) compile cleanly when isolated from the rest of the codebase.
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
#   - That CMake's actual Apple configuration (cmake/FindAccelerateSparse.cmake,
#     the ACCELERATE_NEW_LAPACK / USE_ACCELERATE_SPARSE defines) produces a
#     working build; this script bypasses CMake and hven's own build flags
#     entirely, invoking the compiler directly.
#
# The claim ceiling for anything this script demonstrates is exactly:
# "syntax-checked against stubs on Linux." Nothing here may be cited as
# evidence the Accelerate backend compiles, links, or runs on Apple
# hardware -- see docs/testing.md and the Task-6 report.
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

echo
if [[ "${STATUS}" -eq 0 ]]; then
    echo "==== PASS: both Accelerate-backend TUs are syntax-clean against the stub ===="
else
    echo "==== FAIL: see above ===="
fi
exit "${STATUS}"
