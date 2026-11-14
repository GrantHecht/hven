#!/usr/bin/env bash
# M8 DN-0: build the instruments against the harness's pinned hven install.
#   1. tools build (dn0_replay, dn0_replay_verify, the pardiso wrap object)
#   2. the harness DUMP build: the unchanged lane sources, configured exactly as configure.sh
#      does, plus -Wl,--wrap=pardiso and the wrap object. The measured (perf) binary is the
#      harness's own build/ and is never this one.
# Run under: flock /tmp/box-build.lock bash build_instruments.sh HARNESS_ROOT
set -euo pipefail
HB="$1"
TOOLS="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
sha="$(sed -n 's/^set(HVEN_PIN_SHA "\(.*\)")$/\1/p' "${HB}/cmake/hven_pin.cmake")"
prefix="${HB}/.hven/install-${sha:0:12}"
[[ -f "${prefix}/HVEN_PIN.txt" ]] || { echo "no pin prefix ${prefix}" >&2; exit 2; }
OUT="${HB}/.scratch/dn0"
mkdir -p "${OUT}"
rm -rf "${OUT}/build-tools" "${OUT}/build-dump"
cmake -S "${TOOLS}" -B "${OUT}/build-tools" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="${prefix}" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "${OUT}/build-tools.configure.log"
cmake --build "${OUT}/build-tools" -j8 > "${OUT}/build-tools.build.log"
obj="$(find "${OUT}/build-tools" -name 'pardiso_wrap.c.o' | head -1)"
[[ -f "${obj}" ]] || { echo "no wrap object" >&2; exit 1; }
cmake -S "${HB}" -B "${OUT}/build-dump" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="${prefix}" \
      "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=-Wl,--wrap=pardiso ${obj}" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "${OUT}/build-dump.configure.log"
cmake --build "${OUT}/build-dump" -j8 --target hven_bench_cell > "${OUT}/build-dump.build.log"
n_dump="$( (nm "${OUT}/build-dump/lanes/hven/hven_bench_cell" | grep -c ' T __wrap_pardiso$') || true)"
n_meas="$( (nm "${HB}/build/lanes/hven/hven_bench_cell" | grep -c ' T __wrap_pardiso$') || true)"
[[ "${n_dump}" == 1 ]] || { echo "dump binary carries no __wrap_pardiso" >&2; exit 1; }
[[ "${n_meas}" == 0 ]] || { echo "MEASURED binary carries __wrap_pardiso" >&2; exit 1; }
sha256sum "${HB}/build/lanes/hven/hven_bench_cell" "${OUT}/build-dump/lanes/hven/hven_bench_cell" \
          "${OUT}/build-tools/dn0_replay" "${OUT}/build-tools/dn0_replay_verify" \
          "${prefix}/lib64/libhven.a"
echo BUILD_INSTRUMENTS_OK
