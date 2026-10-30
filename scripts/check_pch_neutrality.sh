#!/usr/bin/env bash
# Proves the precompiled header is numerically inert.
#
# The PCH (src/hven_pch.h, wired in src/CMakeLists.txt) exists purely to cut
# build time. The property that lets it near engine code is that it changes
# nothing the compiler emits: every object file, and the archive itself, must
# be byte-for-byte identical whether or not the PCH was used. Byte-identical
# objects cannot move a golden-rig row or a counter, which is how this change
# satisfies CLAUDE.md's "runtime neutrality is proven, not presumed".
#
# That property held when the PCH was introduced, but a property proven once
# and enforced nowhere decays silently: reordering the include list in
# hven_pch.h, reformatting the include block of
# src/drivers/ipm_solver.cpp (clang-format would alphabetize both),
# or adding a TU to the opt-in list without measuring it all break it without
# breaking the build. This script re-proves it on demand and in CI.
#
# What it does:
#   1. Configures and builds hven twice into fresh scratch build directories
#      from identical sources -- once normally, once with
#      -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON (CMake's own documented switch
#      for turning target_precompile_headers() into a no-op).
#   2. Compares every object file under the hven target by relative path, plus
#      libhven.a, and fails on any difference.
#   3. In the PCH-enabled build, asserts the PCH artifact was actually produced
#      and that exactly the expected number of compile commands consume it.
#      That second assertion doubles as the platform-engagement check: it is
#      what catches "the PCH silently stopped applying on this platform", which
#      a byte-identity comparison alone would report as a pass.
#
# Both directions matter. Comparison alone passes trivially if the PCH
# disengaged; engagement alone passes even if the PCH changed the output.
#
# ccache is disabled for both builds (CCACHE_DISABLE=1). This is a
# verification safeguard, not a performance choice: a cache hit would replay a
# previously stored object rather than compiling the source in front of us,
# which is precisely the thing under test.
#
# Requires: a C++ compiler CMake can find (system clang++ by default -- NOT a
# conda-toolchain compiler), and Intel MKL on Linux/Windows or Apple
# Accelerate on macOS, same as any hven build.
#
# CLANG ONLY, and it will say so by failing (M7 W2 E4 fix round 4).
# src/CMakeLists.txt now guards target_precompile_headers() on the compiler id,
# so a caller who passes -DCMAKE_CXX_COMPILER=g++ gets a build with no PCH in
# it and this script's engagement assertion (step 3) fails on a missing PCH
# artifact. That is the correct answer, not a bug in either place: under GCC
# there is no PCH, so there is no byte-identity question to settle. The reason
# the guard exists -- a GCC .gch carries no input validation and is therefore
# unsafe to cache -- is at the guard and in docs/build.md.
#
# Optional environment:
#   HVEN_PCH_NEUTRALITY_JOBS      Build parallelism (default 2, which suits
#                                   the memory-constrained boxes this is
#                                   usually run on; CI may raise it).
#   HVEN_PCH_NEUTRALITY_WORKDIR   Use this directory instead of a fresh
#                                   mktemp one, and do NOT delete it on exit.
#                                   For inspecting the two build trees after a
#                                   failure.
#   HVEN_PCH_EXPECTED_TU_COUNT    Number of compile commands expected to
#                                   consume the PCH. NORMALLY UNSET: the
#                                   default is DERIVED from _hven_pch_sources
#                                   in src/CMakeLists.txt (below), which is the
#                                   list the build actually applies, so the two
#                                   cannot drift apart. Set it only to assert a
#                                   count deliberately different from that list;
#                                   it is read BEFORE the derivation, skips it
#                                   entirely, and the run banner then says the
#                                   count was overridden rather than derived.
#
# Optional arguments:
#   Any arguments given are forwarded as extra -D cache-variable overrides to
#   BOTH cmake configure invocations, so the two builds cannot differ in
#   anything but the PCH switch. CI's Windows lane uses this to pass the same
#   clang-cl toolchain overrides its preset does.

set -euo pipefail

HVEN_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="${HVEN_PCH_NEUTRALITY_JOBS:-2}"

# THE EXPECTED COUNT IS READ OFF _hven_pch_sources, NOT WRITTEN DOWN TWICE
# (M7 W0 T5(f), 2026-09-14). It used to be a literal 6 here, and the literal
# went stale the moment the list shrank: M6 W5 T8.7b (3306ad75) removed
# drivers/interior_point_solver_print.cpp when that TU was retired with the
# engine's last fmt::print, and M6 W5 T8.9 (fbdebefe) removed
# model/nlp_solver.cpp with the NLPSolver wrapper -- 6 -> 5 -> 4. Neither
# removal measured anything, because there was nothing left to measure: both
# FILES were deleted, and every TU still on the list is one whose byte-identity
# was measured when it joined. Neither touched this script, so the Linux lane
# failed the engagement assertion ("4 translation units consume the PCH ...
# expected 6") from M6's merge onward, AFTER a green suite.
#
# Deriving it closes that class of drift for good: the list is the single
# source, and adding or removing a TU moves both halves in one edit.
# AN EXPLICIT OVERRIDE IS CONSULTED FIRST, AND THE LIST IS THEN NOT PARSED AT
# ALL (M7 W0 T5 fix round 1). Until fix1 the parse ran first and its failure
# message offered an override the script had already exited before reaching --
# a promise it could not keep -- and the banner below announced "from
# _hven_pch_sources" even when an override had supplied the number. Both are
# fixed by deciding the source once, here, and carrying it.
if [ -n "${HVEN_PCH_EXPECTED_TU_COUNT:-}" ]; then
    EXPECTED_TU_COUNT="${HVEN_PCH_EXPECTED_TU_COUNT}"
    EXPECTED_TU_COUNT_SOURCE="overridden by HVEN_PCH_EXPECTED_TU_COUNT"
else
    _pch_sources_count="$(sed -n '/^set(_hven_pch_sources$/,/^)$/p' \
        "${HVEN_ROOT}/src/CMakeLists.txt" | grep -c '^    [A-Za-z]' || true)"
    if [ "${_pch_sources_count}" -lt 1 ]; then
        echo "FAIL: could not read _hven_pch_sources out of ${HVEN_ROOT}/src/CMakeLists.txt."
        echo "      This script derives the expected PCH consumer count from that list, so a"
        echo "      derivation that finds nothing must stop rather than fall back to a literal"
        echo "      -- a wrong count is exactly the failure this replaced. Fix the derivation"
        echo "      (the list is 'set(_hven_pch_sources' ... ')' with one four-space-indented"
        echo "      source per line), or set HVEN_PCH_EXPECTED_TU_COUNT deliberately, which"
        echo "      is read ABOVE this parse and skips it."
        exit 1
    fi
    EXPECTED_TU_COUNT="${_pch_sources_count}"
    EXPECTED_TU_COUNT_SOURCE="from _hven_pch_sources in src/CMakeLists.txt"
fi

# Counts hven-target compile commands that consume the PCH, in an existing
# build directory. Shared by the full check below and by --engagement-only.
count_pch_consumers() {
    local compile_db="$1"
    python3 - "${compile_db}" <<'PY'
import json, sys
with open(sys.argv[1]) as fh:
    db = json.load(fh)
n = 0
for entry in db:
    command = entry.get("command") or " ".join(entry.get("arguments", []))
    if "hven.dir" not in command:
        continue
    # The wrapper CMake compiles to PRODUCE the PCH is not a consumer of it.
    if "cmake_pch" in entry.get("file", ""):
        continue
    if "cmake_pch" in command:
        n += 1
print(n)
PY
}

assert_engagement() {
    local build_dir="$1"
    local compile_db="${build_dir}/compile_commands.json"
    if [ ! -f "${compile_db}" ]; then
        echo "FAIL: ${compile_db} not found; cannot verify PCH engagement."
        echo "      The build must be configured with CMAKE_EXPORT_COMPILE_COMMANDS=ON"
        echo "      (every hven preset sets it)."
        exit 1
    fi
    local n
    n="$(count_pch_consumers "${compile_db}")"
    if [ "${n}" != "${EXPECTED_TU_COUNT}" ]; then
        echo "FAIL: ${n} translation units consume the PCH in ${build_dir}, expected ${EXPECTED_TU_COUNT}."
        echo "      Too few means the PCH quietly stopped applying on this platform -- a"
        echo "      build-time regression no test would catch. Too many means it spread to"
        echo "      TUs whose byte-identity was never measured. The expectation was"
        echo "      ${EXPECTED_TU_COUNT_SOURCE}, so a mismatch here is the build"
        echo "      disagreeing with that source -- look at what the build applied"
        echo "      before editing either."
        exit 1
    fi
    echo "PASS: PCH engaged on ${n} translation units in ${build_dir} (expected ${EXPECTED_TU_COUNT})."
}

# --engagement-only <build-dir>: the cheap half. Asserts the PCH is applied to
# exactly the expected TUs in an ALREADY-CONFIGURED build directory, without
# building anything twice. This is what the macOS and Windows CI lanes run:
# it catches "the PCH silently stopped applying on this platform" for the cost
# of reading a JSON file, while the full byte-identity comparison -- the
# expensive half -- runs on Linux only. See docs/build.md for that split and
# its claim ceiling.
if [ "${1:-}" = "--engagement-only" ]; then
    if [ -z "${2:-}" ]; then
        echo "usage: $0 --engagement-only <build-dir>" >&2
        exit 2
    fi
    assert_engagement "$2"
    exit 0
fi

EXTRA_CMAKE_ARGS=("$@")
# Expanded below as "${EXTRA_CMAKE_ARGS[@]+"${EXTRA_CMAKE_ARGS[@]}"}" rather
# than plainly: under `set -u`, bash 3.2 (what macOS ships) treats a plain
# expansion of an EMPTY array as an unbound-variable error. Same idiom, and
# same reason, as scripts/check_install_smoke.sh.

# Pick the project's documented toolchain unless the caller named one.
#
# This matters more than it looks. The byte-identity property below was
# measured, and holds, for clang -- the toolchain CLAUDE.md documents, every
# CMakePresets.json preset selects, and all three CI lanes use. It does NOT
# hold for GCC: measured on GCC 16.1.1, three of the six opted-in TUs
# (interior_point_solver_settings.cpp, non_linear_program.cpp,
# nlp_adapter.cpp) emit a different object with the PCH than without it.
# (That reading is left as it ran, with the list as it then stood: the list is
# four TUs now and the first of those three names is today's ipm_options.cpp.
# Re-measuring GCC would change the arithmetic, not the conclusion.)
#
# So a bare `cmake` here, which on many Linux distributions resolves c++ to
# GCC, would fail this check for a reason that is real but entirely outside
# the supported configuration -- and would read like a regression rather than
# like a toolchain mismatch. Defaulting to clang tests what hven actually
# ships. A caller who explicitly names a compiler gets exactly what they
# asked for, including the GCC failure, which is the honest answer to
# "is the PCH neutral under GCC?": it is not.
COMPILER_ARGS=()
_caller_named_compiler=0
for _arg in "${EXTRA_CMAKE_ARGS[@]+"${EXTRA_CMAKE_ARGS[@]}"}"; do
    case "${_arg}" in
        -DCMAKE_CXX_COMPILER=*) _caller_named_compiler=1 ;;
    esac
done
if [ "${_caller_named_compiler}" = "0" ] && [ -z "${CXX:-}" ]; then
    if command -v clang++ >/dev/null 2>&1; then
        COMPILER_ARGS=(-DCMAKE_CXX_COMPILER="$(command -v clang++)")
    fi
fi

if [ -n "${HVEN_PCH_NEUTRALITY_WORKDIR:-}" ]; then
    WORK_DIR="${HVEN_PCH_NEUTRALITY_WORKDIR}"
    mkdir -p "${WORK_DIR}"
    OWN_WORKDIR=0
else
    WORK_DIR="$(mktemp -d /tmp/hven-pch-neutrality.XXXXXX)"
    OWN_WORKDIR=1
fi

PCH_BUILD="${WORK_DIR}/build-pch-on"
NOPCH_BUILD="${WORK_DIR}/build-pch-off"

cleanup() {
    if [ "${OWN_WORKDIR}" = "1" ]; then
        rm -rf "${WORK_DIR}"
    fi
}
trap cleanup EXIT

# ccache would replay stored objects instead of compiling what is in front of
# us. Disable it for everything below.
export CCACHE_DISABLE=1

echo "== hven PCH neutrality check"
echo "   source:    ${HVEN_ROOT}"
echo "   scratch:   ${WORK_DIR}"
echo "   jobs:      ${JOBS}"
echo "   PCH TUs:   ${EXPECTED_TU_COUNT} (${EXPECTED_TU_COUNT_SOURCE})"
# Length expansion ${#arr[@]} never trips `set -u` on an empty array, in any
# bash version -- only element expansion "${arr[@]}" does (bash < 4.4), which
# is why the expansions below carry the ${arr[@]+...} guard and this one does
# not need it.
if [ "${#COMPILER_ARGS[@]}" -gt 0 ]; then
    echo "   compiler:  ${COMPILER_ARGS[0]#-DCMAKE_CXX_COMPILER=} (project default; override by passing -DCMAKE_CXX_COMPILER=...)"
else
    echo "   compiler:  caller-specified or CXX from the environment"
fi

build_one() {
    local build_dir="$1"
    local label="$2"
    shift 2
    echo
    echo "-- configuring ${label}"
    cmake -S "${HVEN_ROOT}" -B "${build_dir}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DHVEN_BUILD_TESTS=OFF \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        "${COMPILER_ARGS[@]+"${COMPILER_ARGS[@]}"}" \
        "$@" \
        "${EXTRA_CMAKE_ARGS[@]+"${EXTRA_CMAKE_ARGS[@]}"}"
    echo "-- building ${label}"
    cmake --build "${build_dir}" --target hven -- "-j${JOBS}"
}

build_one "${PCH_BUILD}"   "PCH-enabled build"
build_one "${NOPCH_BUILD}" "PCH-disabled build" -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON

################################################################################
# Engagement: the PCH must actually be in use in the first build.
################################################################################

echo
echo "-- checking the PCH was actually engaged"

# CMake names the generated wrapper cmake_pch.hxx and the compiled artifact
# cmake_pch.hxx.pch (or .gch). Find it rather than hard-coding the layout,
# which differs across generators and CMake versions.
pch_artifact_count="$(find "${PCH_BUILD}" \( -name '*.pch' -o -name '*.gch' \) | wc -l | tr -d ' ')"
if [ "${pch_artifact_count}" -lt 1 ]; then
    echo "FAIL: no compiled PCH artifact (*.pch / *.gch) found under ${PCH_BUILD}."
    echo "      The PCH-enabled build did not actually build a precompiled header,"
    echo "      so the byte-identity comparison below would pass trivially and"
    echo "      prove nothing. Check target_precompile_headers() in src/CMakeLists.txt."
    exit 1
fi
echo "   compiled PCH artifacts found: ${pch_artifact_count}"

# Count hven-target compile commands that consume the PCH. Clang and GCC use
# -include/-include-pch; clang-cl uses /Yu or -Xclang -include-pch. Match the
# generated wrapper's name, which every form references.
assert_engagement "${PCH_BUILD}"
pch_tu_count="${EXPECTED_TU_COUNT}"

################################################################################
# Neutrality: every emitted artifact must be byte-identical.
################################################################################

echo
echo "-- comparing every hven object between the two builds"

pch_objdir="${PCH_BUILD}/CMakeFiles/hven.dir"
nopch_objdir="${NOPCH_BUILD}/CMakeFiles/hven.dir"

if [ ! -d "${pch_objdir}" ] || [ ! -d "${nopch_objdir}" ]; then
    echo "FAIL: expected object directories not found:"
    echo "      ${pch_objdir}"
    echo "      ${nopch_objdir}"
    exit 1
fi

compared=0
differing=0

# Drive the comparison off the PCH-DISABLED build's object list. That build
# has no cmake_pch wrapper object of its own, so its list is exactly the real
# translation units and needs no filtering.
while IFS= read -r rel; do
    a="${nopch_objdir}/${rel}"
    b="${pch_objdir}/${rel}"
    if [ ! -f "${b}" ]; then
        echo "FAIL: ${rel} exists in the PCH-disabled build but not the PCH-enabled one."
        differing=$((differing + 1))
        continue
    fi
    compared=$((compared + 1))
    if ! cmp -s "${a}" "${b}"; then
        echo "FAIL: ${rel} differs between the two builds."
        differing=$((differing + 1))
    fi
done < <(cd "${nopch_objdir}" && find . -name '*.o' -o -name '*.obj' | sed 's|^\./||' | sort)

if [ "${compared}" -eq 0 ]; then
    echo "FAIL: compared 0 objects. The comparison found nothing to check, which is"
    echo "      a broken check rather than a passing one."
    exit 1
fi

# The object count must also match what the build actually compiled: a partial
# build that emitted 3 objects instead of the full set would otherwise pass the
# loop above. Derive the expectation from the PCH-disabled build's own compile
# database rather than hard-coding a count that would drift with the source
# list.
expected_objects="$(python3 -c "
import json, sys
entries = json.load(open('${NOPCH_BUILD}/compile_commands.json'))
# The compile-database spec allows three shapes: an optional 'output' field
# (newer CMake), a 'command' string, or an 'arguments' array in place of
# 'command'. Cover all three -- CI runners have shipped each variant. This is
# the PCH-DISABLED build, so there is no cmake_pch wrapper entry to exclude
# either way.
want = 'CMakeFiles/hven.dir/'  # no leading slash: CI emits RELATIVE paths here, local CMake absolute
def blob(e):
    parts = [e.get('output', ''), e.get('command', '')]
    args = e.get('arguments')
    if isinstance(args, list):
        parts.append(' '.join(args))
    return ' '.join(parts)
n = sum(1 for e in entries if want in blob(e))
if n == 0 and entries:
    print('KEYS:' + ','.join(sorted(entries[0].keys())), file=sys.stderr)
print(n)
")"
if [ "${expected_objects}" -eq 0 ]; then
    echo "FAIL: could not derive the expected object count from the compile database"
    echo "      (neither 'output' nor 'command' entries mention CMakeFiles/hven.dir)."
    echo "      Fix the derivation in this script; do not remove the assertion."
    exit 1
fi
if [ "${compared}" -ne "${expected_objects}" ]; then
    echo "FAIL: compared ${compared} objects but the PCH-disabled build's compile"
    echo "      database lists ${expected_objects} hven translation units. Some objects"
    echo "      were never emitted or never compared -- a silent skip, not a pass."
    exit 1
fi
echo "   object count matches the compile database: ${compared}/${expected_objects}"

# The archive too: it is what actually ships, and it catches anything that
# differs in member ordering or metadata rather than in an object's own bytes.
# If the archive is ever relocated (ARCHIVE_OUTPUT_DIRECTORY), this loop must
# not silently degrade into comparing nothing -- hence the found counter.
archives_compared=0
for archive in libhven.a hven.lib; do
    if [ -f "${NOPCH_BUILD}/${archive}" ] && [ -f "${PCH_BUILD}/${archive}" ]; then
        archives_compared=$((archives_compared + 1))
        compared=$((compared + 1))
        if cmp -s "${NOPCH_BUILD}/${archive}" "${PCH_BUILD}/${archive}"; then
            echo "   ${archive}: identical"
        else
            echo "FAIL: ${archive} differs between the two builds."
            differing=$((differing + 1))
        fi
    fi
done
if [ "${archives_compared}" -eq 0 ]; then
    echo "FAIL: no library archive was found to compare (looked for libhven.a and"
    echo "      hven.lib at the build-directory roots). If the archive output"
    echo "      location changed, update this script -- the archive comparison is"
    echo "      part of the check, not optional."
    exit 1
fi

echo "   objects compared: ${compared}, differing: ${differing}"

if [ "${differing}" -ne 0 ]; then
    echo
    echo "FAIL: the precompiled header is NOT neutral -- ${differing} artifact(s) differ."
    echo
    echo "The PCH is only permitted near engine code because it changes nothing the"
    echo "compiler emits. Something broke that. The usual causes, in order of"
    echo "likelihood:"
    echo "  * the include ORDER in src/hven_pch.h no longer matches the include block"
    echo "    of src/drivers/ipm_solver.cpp (clang-format alphabetizes both"
    echo "    unless the // clang-format off guards are intact);"
    echo "  * a TU was added to _hven_pch_sources without measuring it -- some TUs"
    echo "    compile faster with the PCH but emit a differently-ordered object;"
    echo "  * a header in the shared set started emitting something order-dependent;"
echo "  * or this run used a toolchain the property was never established for."
echo "    Byte-identity holds for clang, not for GCC -- under GCC several of the"
echo "    opted-in TUs legitimately differ. Check the compiler printed at the"
echo "    top of this run before treating the result as a regression."
    echo
    echo "Do not relax this check to make it pass. Either restore the property or"
    echo "narrow _hven_pch_sources to the TUs that still hold it."
    exit 1
fi

echo
echo "PASS: PCH engaged on ${pch_tu_count} translation units, and all ${compared}"
echo "      emitted artifacts are byte-identical with and without it."
