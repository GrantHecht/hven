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
# Not wired into .github/workflows/ci.yml -- this is a standalone,
# rerunnable local/CI-callable check, the same relationship
# check_accelerate_syntax_linux.sh has to that workflow (that one IS wired
# in; this one is intentionally left for a future CI-lane decision).
#
# Requires: system clang++ (NOT a conda-toolchain compiler -- conda's
# libstdc++ rpath is not what this project's presets link against), Intel
# MKL on Linux (MKLROOT/ONEAPI_ROOT, or the FindMKL.cmake fallback paths)
# or Apple Accelerate on macOS.

set -euo pipefail

HVEN_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="$(mktemp -d /tmp/hven-install-smoke.XXXXXX)"
BUILD_DIR="${WORK_DIR}/hven-build"
INSTALL_PREFIX="${WORK_DIR}/hven-install"
SMOKE_BUILD_DIR="${WORK_DIR}/smoke-build"

cleanup() {
    rm -rf "${WORK_DIR}"
}
trap cleanup EXIT

echo "== hven install smoke: workdir ${WORK_DIR}"

echo "== configuring hven (Release) into ${BUILD_DIR}"
cmake -S "${HVEN_ROOT}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DHVEN_BUILD_TESTS=OFF

echo "== building hven"
cmake --build "${BUILD_DIR}" --parallel "${HVEN_INSTALL_SMOKE_JOBS:-6}"

echo "== installing hven into ${INSTALL_PREFIX}"
cmake --install "${BUILD_DIR}" --prefix "${INSTALL_PREFIX}"

echo "== configuring the smoke consumer against the installed package"
cmake -S "${HVEN_ROOT}/tests/install_smoke" -B "${SMOKE_BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}"

echo "== building the smoke consumer"
cmake --build "${SMOKE_BUILD_DIR}" --parallel "${HVEN_INSTALL_SMOKE_JOBS:-6}"

echo "== running the smoke consumer"
"${SMOKE_BUILD_DIR}/hven_install_smoke"

echo "== hven install smoke: PASSED"
