#!/usr/bin/env bash
# M8 DN-0 IPM r1, build step 2 (after build_pin.sh). Builds, under .scratch/dn0r1/:
#   hven-cfi/      the MEASUREMENT-ONLY hven: the pinned source (.hven/src at 0a34c8a2), the pin build's
#                  exact cmake arguments, plus -DCCACHE_PROGRAM=cfi_launcher.sh: hven's CMakeLists.txt
#                  installs CCACHE_PROGRAM as CMAKE_CXX_COMPILER_LAUNCHER, so the launcher (in ccache's
#                  place) appends -fasynchronous-unwind-tables (CFI metadata only) to every compile.
#   build-cfi/hven_bench_cell   the RECORD (perf record) binary: the measured binary's own link command
#                  (build/, ninja -t commands), run from build/ on the same lane objects, with ONLY the
#                  libhven.a path swapped for hven-cfi's and the output path changed.
#   build-dump/    the DUMP build (as DN-0's build_instruments.sh: same lane sources against the pin
#                  prefix, -Wl,--wrap=pardiso + pardiso_wrap.c.o).
#   build-tools/   dn0_replay, dn0_replay_verify (against the pin install, find_package).
#   identity/      code_identity.py over libhven.a (pin vs CFI): every executable section and its
#                  relocations byte-identical; cmp of the dump build's lane objects vs the measured build's.
# Run under: flock /tmp/box-build.lock bash build_instruments.sh HARNESS_ROOT
set -euo pipefail
HB="$1"
TOOLS="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
p=$(pgrep -a hven_bench_cell || true); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "BUSY ${p}"; exit 9; }
echo "FOREGROUND_START build_instruments $(date -u +%FT%TZ)"
SHA=0a34c8a2adba9e725351b3c9e23c14471d3fecb1
prefix="${HB}/.hven/install-${SHA:0:12}"
grep -q "^HVEN_PIN_SHA ${SHA}$" "${prefix}/HVEN_PIN.txt"
[[ "$(git -C "${HB}/.hven/src" rev-parse HEAD)" == "${SHA}" ]]
[[ -z "$(git -C "${HB}/.hven/src" status --porcelain --untracked-files=all)" ]]
OUT="${HB}/.scratch/dn0r1"
mkdir -p "${OUT}"
rm -rf "${OUT}/hven-cfi" "${OUT}/build-cfi" "${OUT}/build-dump" "${OUT}/build-tools" "${OUT}/identity"

# 1. CFI hven (same cmake args as scripts/build_hven_pin.sh, plus the launcher)
cmake -S "${HB}/.hven/src" -B "${OUT}/hven-cfi/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=clang++ -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF -DHVEN_LINK_TIME_OPT=OFF \
      -DCMAKE_INSTALL_PREFIX="${OUT}/hven-cfi/install" \
      -DCCACHE_PROGRAM="${TOOLS}/cfi_launcher.sh" > "${OUT}/hven-cfi.configure.log" 2>&1
cmake --build "${OUT}/hven-cfi/build" --parallel "$(nproc)" > "${OUT}/hven-cfi.build.log" 2>&1
cmake --install "${OUT}/hven-cfi/build" > "${OUT}/hven-cfi.install.log" 2>&1
nw="$(cd "${OUT}/hven-cfi/build" && ninja -t commands | grep -c "cfi_launcher.sh" || true)"
echo "launcher-wrapped compile commands: ${nw}"
[[ "${nw}" -gt 0 ]] || { echo "the launcher wraps no compile" >&2; exit 1; }

# 2. RECORD binary: the measured link command with libhven.a swapped
mkdir -p "${OUT}/build-cfi"
cmd="$(cd "${HB}/build" && ninja -t commands lanes/hven/hven_bench_cell | tail -1)"
[[ "${cmd}" == *"${prefix}/lib64/libhven.a"* ]] || { echo "link command does not name the pin libhven.a" >&2; exit 1; }
new="${cmd//${prefix}\/lib64\/libhven.a/${OUT}/hven-cfi/install/lib64/libhven.a}"
new="${new// -o lanes\/hven\/hven_bench_cell / -o ${OUT}/build-cfi/hven_bench_cell }"
new="${new//--dependency-file=lanes\/hven\/CMakeFiles\/hven_bench_cell.dir\/link.d/--dependency-file=${OUT}/build-cfi/link.d}"
printf '%s\n' "${cmd}" > "${OUT}/build-cfi/link.measured.txt"
printf '%s\n' "${new}" > "${OUT}/build-cfi/link.cfi.txt"
(cd "${HB}/build" && bash -c "${new}")
diff <(tr ' ' '\n' < "${OUT}/build-cfi/link.measured.txt") <(tr ' ' '\n' < "${OUT}/build-cfi/link.cfi.txt") \
    > "${OUT}/build-cfi/link.diff" || true

# 3. tools (replay) against the pin install; 4. dump build
cmake -S "${TOOLS}" -B "${OUT}/build-tools" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="${prefix}" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "${OUT}/build-tools.configure.log"
cmake --build "${OUT}/build-tools" -j8 > "${OUT}/build-tools.build.log"
obj="$(find "${OUT}/build-tools" -name 'pardiso_wrap.c.o' | head -1)"
[[ -f "${obj}" ]]
cmake -S "${HB}" -B "${OUT}/build-dump" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="${prefix}" \
      "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=-Wl,--wrap=pardiso ${obj}" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "${OUT}/build-dump.configure.log"
cmake --build "${OUT}/build-dump" -j8 --target hven_bench_cell > "${OUT}/build-dump.build.log"
n_dump="$( (nm "${OUT}/build-dump/lanes/hven/hven_bench_cell" | grep -c ' T __wrap_pardiso$') || true)"
n_meas="$( (nm "${HB}/build/lanes/hven/hven_bench_cell" | grep -c ' T __wrap_pardiso$') || true)"
n_cfi="$( (nm "${OUT}/build-cfi/hven_bench_cell" | grep -c ' T __wrap_pardiso$') || true)"
[[ "${n_dump}" == 1 && "${n_meas}" == 0 && "${n_cfi}" == 0 ]] || { echo "wrap symbol check failed ${n_dump}/${n_meas}/${n_cfi}" >&2; exit 1; }

# 5. identity proofs
mkdir -p "${OUT}/identity"
python3 "${TOOLS}/code_identity.py" "${prefix}/lib64/libhven.a" "${OUT}/hven-cfi/install/lib64/libhven.a" \
    "${OUT}/identity/ar" > "${OUT}/identity/libhven-code-identity.txt"
tail -1 "${OUT}/identity/libhven-code-identity.txt"
# header trees of the two installs are identical (the lane objects compile against the pin's)
diff -r "${prefix}/include" "${OUT}/hven-cfi/install/include" > "${OUT}/identity/include-diff.txt" && echo "INCLUDE_TREES_IDENTICAL"
# dump build lane objects vs measured build lane objects
{
  n=0; d=0
  while IFS= read -r o; do
    rel="${o#${HB}/build/}"
    n=$((n+1))
    if cmp -s "${o}" "${OUT}/build-dump/${rel}"; then echo "SAME ${rel}"; else echo "DIFF ${rel}"; d=$((d+1)); fi
  done < <(find "${HB}/build/lanes" "${HB}/build/sets" "${HB}/build/CMakeFiles" -name '*.o' 2>/dev/null | sort)
  echo "lane objects compared: ${n}; differing: ${d}"
} > "${OUT}/identity/dump-vs-measured-objects.txt"
tail -1 "${OUT}/identity/dump-vs-measured-objects.txt"
sha256sum "${HB}/build/lanes/hven/hven_bench_cell" "${OUT}/build-cfi/hven_bench_cell" \
          "${OUT}/build-dump/lanes/hven/hven_bench_cell" "${OUT}/build-tools/dn0_replay" \
          "${OUT}/build-tools/dn0_replay_verify" "${prefix}/lib64/libhven.a" "${OUT}/hven-cfi/install/lib64/libhven.a"
echo "FOREGROUND_END build_instruments $(date -u +%FT%TZ)"
