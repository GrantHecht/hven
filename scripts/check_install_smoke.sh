#!/usr/bin/env bash
# End-to-end check of hven's install/export rules (root CMakeLists.txt's
# install(TARGETS hven EXPORT hvenTargets ...) block and
# cmake/hvenConfig.cmake.in): configures hven standalone into a scratch build
# directory, builds it, installs it into a scratch prefix, then configures,
# builds, and runs tests/install_smoke/ against that installed prefix via
# find_package(hven) -- proving a bare `find_package(hven)` +
# `target_link_libraries(app hven::hven)` actually links and runs, not just
# configures.
#
# Wired into .github/workflows/ci.yml on all three lanes -- see the "Run
# install smoke" step in each job. That step also inspects the artifacts
# this script leaves behind (the installed hvenTargets.cmake, the hven
# configure log) via scripts/check_export_contract.sh, which is why the two
# CI-only knobs below exist: a plain local run needs neither.
#
# Requires: system clang++ (NOT a conda-toolchain compiler -- conda's
# libstdc++ rpath is not what this project's presets link against), Intel
# MKL on Linux (MKLROOT/ONEAPI_ROOT, or the FindMKL.cmake fallback paths)
# or Apple Accelerate on macOS.
#
# Optional environment:
#   HVEN_INSTALL_SMOKE_WORKDIR  Use this directory instead of a fresh
#                                mktemp one, and do NOT delete it on exit.
#                                Lets a caller (CI) inspect the installed
#                                hvenTargets.cmake and the configure log
#                                after this script returns. Local runs
#                                leave this unset and get the original
#                                scratch-dir-with-cleanup behavior.
#   HVEN_INSTALL_SMOKE_JOBS      Build parallelism (default 6).
#
# Optional arguments:
#   Any arguments given are forwarded as extra -D cache-variable overrides
#   to BOTH cmake configure invocations below (hven itself and the smoke
#   consumer). Local runs pass none and get the previous behavior exactly.
#   CI's Windows lane passes the same CMAKE_C(XX)_COMPILER/_AR/_RANLIB/
#   CMAKE_LINKER overrides windows-clang-release uses in CMakePresets.json
#   -- without them, plain `cmake -S ... -B ...` on that runner would pick
#   up whatever default MSVC toolchain a bash shell resolves instead of
#   clang-cl, and the consumer's link would not match how hven itself, or
#   MKL's import libraries, were built.

set -euo pipefail

HVEN_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXTRA_CMAKE_ARGS=("$@")

if [ -n "${HVEN_INSTALL_SMOKE_WORKDIR:-}" ]; then
    WORK_DIR="${HVEN_INSTALL_SMOKE_WORKDIR}"
    mkdir -p "${WORK_DIR}"
    HVEN_INSTALL_SMOKE_OWN_WORKDIR=0
else
    WORK_DIR="$(mktemp -d /tmp/hven-install-smoke.XXXXXX)"
    HVEN_INSTALL_SMOKE_OWN_WORKDIR=1
fi
BUILD_DIR="${WORK_DIR}/hven-build"
INSTALL_PREFIX="${WORK_DIR}/hven-install"
SMOKE_BUILD_DIR="${WORK_DIR}/smoke-build"
CONFIGURE_LOG="${WORK_DIR}/hven-configure.log"

cleanup() {
    if [ "${HVEN_INSTALL_SMOKE_OWN_WORKDIR}" = "1" ]; then
        rm -rf "${WORK_DIR}"
    fi
}
trap cleanup EXIT

echo "== hven install smoke: workdir ${WORK_DIR}"

echo "== configuring hven (Release) into ${BUILD_DIR}"
# Teed (not redirected) so the "resolved to N" line from the
# EIGEN_MAX_ALIGN_BYTES probe (root CMakeLists.txt) is both visible in this
# script's own output and preserved as a file a caller can grep afterward.
# pipefail (set above) still fails this step if cmake itself fails.
cmake -S "${HVEN_ROOT}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DHVEN_BUILD_TESTS=OFF \
    "${EXTRA_CMAKE_ARGS[@]}" | tee "${CONFIGURE_LOG}"

echo "== building hven"
cmake --build "${BUILD_DIR}" --parallel "${HVEN_INSTALL_SMOKE_JOBS:-6}"

echo "== installing hven into ${INSTALL_PREFIX}"
cmake --install "${BUILD_DIR}" --prefix "${INSTALL_PREFIX}"

echo "== configuring the smoke consumer against the installed package"
cmake -S "${HVEN_ROOT}/tests/install_smoke" -B "${SMOKE_BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
    "${EXTRA_CMAKE_ARGS[@]}"

echo "== building the smoke consumer"
cmake --build "${SMOKE_BUILD_DIR}" --parallel "${HVEN_INSTALL_SMOKE_JOBS:-6}"

echo "== running the smoke consumer"
"${SMOKE_BUILD_DIR}/hven_install_smoke"

echo "== hven install smoke: PASSED"
echo "HVEN_INSTALL_SMOKE_INSTALL_PREFIX=${INSTALL_PREFIX}"
echo "HVEN_INSTALL_SMOKE_CONFIGURE_LOG=${CONFIGURE_LOG}"
