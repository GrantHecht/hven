#!/usr/bin/env bash
# Shared assertion script for hven's install-tree ABI export contract, run
# identically on all three CI lanes (linux/macos/windows -- via bash on
# every lane, see .github/workflows/ci.yml) against that lane's own
# installed hvenTargets.cmake.
#
# Verifies the exact contract documented above the platform-macro and
# EIGEN_MAX_ALIGN_BYTES blocks in the root CMakeLists.txt's
# "Platform/backend macros..." and "The alignment Eigen assumes..."
# comments:
#
#   - the platform-conditional macros are present on their platform and
#     absent everywhere else: USE_ACCELERATE_SPARSE on Apple;
#     NOMINMAX / WIN32_LEAN_AND_MEAN / _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
#     on Windows; none of the four on Linux.
#   - EIGEN_MAX_ALIGN_BYTES is exported with a value that MATCHES what the
#     SAME build's configure-time probe (cmake/probes/
#     eigen_max_align_bytes.cpp) actually computed for that machine's SIMD
#     flags -- never a hardcoded literal, since the value is toolchain- and
#     ISA-dependent (16 on Apple Silicon NEON, 32 on x86 AVX2, ...).
#   - NO INTERFACE_COMPILE_OPTIONS property is exported at all -- hven's
#     codegen flags (-march/-mtune/-ffast-math/inline-threshold) are
#     deliberately not re-exported (see the "Codegen flags... are NOT
#     re-exported here" comment in the root CMakeLists.txt).
#   - INTERFACE_INCLUDE_DIRECTORIES is set exactly once (guards against a
#     duplicate entry from an errant extra INCLUDES DESTINATION -- see the
#     "No INCLUDES DESTINATION here" comment above install(TARGETS hven...)
#     in the root CMakeLists.txt).
#
# Usage:
#   check_export_contract.sh <path-to-hvenTargets.cmake> <platform> <expected-align-bytes>
#     platform:              apple | windows | linux
#     expected-align-bytes:  the value THIS SAME BUILD's configure log
#                             reported ("... resolved to N ..." -- see
#                             scripts/check_install_smoke.sh's configure
#                             log capture). Never a literal picked ahead of
#                             time.

set -euo pipefail

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

if [ "$#" -ne 3 ]; then
    fail "usage: $0 <hvenTargets.cmake path> <apple|windows|linux> <expected-align-bytes>"
fi

TARGETS_FILE="$1"
PLATFORM="$2"
EXPECTED_ALIGN="$3"

[ -f "${TARGETS_FILE}" ] || fail "no such file: ${TARGETS_FILE}"
TARGETS_DIR="$(cd "$(dirname "${TARGETS_FILE}")" && pwd)"

case "${PLATFORM}" in
    apple | windows | linux) ;;
    *) fail "unknown platform '${PLATFORM}' (expected apple|windows|linux)" ;;
esac

[[ "${EXPECTED_ALIGN}" =~ ^[0-9]+$ ]] \
    || fail "expected-align-bytes must be numeric, got '${EXPECTED_ALIGN}'"

echo "== checking export contract: ${TARGETS_FILE} (platform=${PLATFORM})"

DEFS_LINE="$(grep -m1 'INTERFACE_COMPILE_DEFINITIONS' "${TARGETS_FILE}" || true)"
[ -n "${DEFS_LINE}" ] \
    || fail "no INTERFACE_COMPILE_DEFINITIONS property found in ${TARGETS_FILE}"

has_macro() {
    # Word-bounded match inside the definitions line: each entry is
    # delimited by semicolons/quotes (already non-word characters), so -w
    # guards against a hypothetical accidental substring match.
    grep -Fqw "$1" <<<"${DEFS_LINE}"
}

case "${PLATFORM}" in
    apple)
        has_macro "USE_ACCELERATE_SPARSE" \
            || fail "USE_ACCELERATE_SPARSE missing from exported INTERFACE_COMPILE_DEFINITIONS on Apple: ${DEFS_LINE}"
        for m in NOMINMAX WIN32_LEAN_AND_MEAN _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR; do
            has_macro "$m" \
                && fail "Windows-only macro $m leaked into the Apple export: ${DEFS_LINE}"
        done
        ;;
    windows)
        for m in NOMINMAX WIN32_LEAN_AND_MEAN _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR; do
            has_macro "$m" \
                || fail "$m missing from exported INTERFACE_COMPILE_DEFINITIONS on Windows: ${DEFS_LINE}"
        done
        has_macro "USE_ACCELERATE_SPARSE" \
            && fail "Apple-only macro USE_ACCELERATE_SPARSE leaked into the Windows export: ${DEFS_LINE}"
        ;;
    linux)
        for m in USE_ACCELERATE_SPARSE NOMINMAX WIN32_LEAN_AND_MEAN _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR; do
            has_macro "$m" \
                && fail "platform macro $m leaked into the Linux export (expected none): ${DEFS_LINE}"
        done
        ;;
esac
echo "-- platform macro set OK for ${PLATFORM}"

ALIGN_VALUE="$(grep -oE 'EIGEN_MAX_ALIGN_BYTES=[0-9]+' <<<"${DEFS_LINE}" | head -n1 | cut -d= -f2 || true)"
[ -n "${ALIGN_VALUE}" ] \
    || fail "EIGEN_MAX_ALIGN_BYTES not found (or non-numeric) in exported definitions: ${DEFS_LINE}"
[[ "${ALIGN_VALUE}" =~ ^[0-9]+$ ]] \
    || fail "EIGEN_MAX_ALIGN_BYTES value '${ALIGN_VALUE}' is not numeric"
[ "${ALIGN_VALUE}" = "${EXPECTED_ALIGN}" ] \
    || fail "exported EIGEN_MAX_ALIGN_BYTES=${ALIGN_VALUE} does not match this build's computed value ${EXPECTED_ALIGN}"
echo "-- EIGEN_MAX_ALIGN_BYTES OK: exported=${ALIGN_VALUE} computed=${EXPECTED_ALIGN}"

# INTERFACE_COMPILE_OPTIONS: checked absent across every generated export
# file in the package directory, not just the primary hvenTargets.cmake --
# hvenConfig.cmake include()s hvenTargets.cmake plus its per-config
# companions (hvenTargets-<config>.cmake) together, so all of them are
# equally part of what a consumer actually sees.
if grep -l 'INTERFACE_COMPILE_OPTIONS' "${TARGETS_DIR}"/hvenTargets*.cmake >/dev/null 2>&1; then
    fail "INTERFACE_COMPILE_OPTIONS found under ${TARGETS_DIR} (expected absent -- codegen flags are not re-exported)"
fi
echo "-- INTERFACE_COMPILE_OPTIONS OK: absent"

INCLUDE_DIRS_COUNT="$(grep -c 'INTERFACE_INCLUDE_DIRECTORIES' "${TARGETS_FILE}" || true)"
[ "${INCLUDE_DIRS_COUNT}" -eq 1 ] \
    || fail "INTERFACE_INCLUDE_DIRECTORIES appears ${INCLUDE_DIRS_COUNT} times in ${TARGETS_FILE} (expected exactly once)"
echo "-- INTERFACE_INCLUDE_DIRECTORIES OK: appears exactly once"

echo "== export contract OK for platform=${PLATFORM}, align=${ALIGN_VALUE}"
