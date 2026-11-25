#!/usr/bin/env bash
# M8 DN-0 IPM r1, build step 1: the pinned hven (0a34c8a2) install and the MEASURED lane binary.
#   1. keep a copy of DN-0's measured lane binary (6fd1cdce...) under .scratch/dn0r1/ (build/ is rebuilt below)
#   2. scripts/build_hven_pin.sh (the harness's own, unchanged)  -> .hven/install-0a34c8a2adba/
#   3. ./configure.sh + cmake --build build --target hven_bench_cell (the harness's own, unchanged)
# Run under: flock /tmp/box-build.lock bash build_pin.sh HARNESS_ROOT
set -euo pipefail
HB="$1"
p=$(pgrep -a hven_bench_cell || true); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "BUSY ${p}"; exit 9; }
echo "FOREGROUND_START build_pin $(date -u +%FT%TZ)"
cd "${HB}"
[[ "$(git rev-parse --short=7 HEAD)" == 38f5b4a ]] || { echo "harness HEAD is not 38f5b4a" >&2; exit 3; }
mkdir -p .scratch/dn0r1/dn0-measured-binary
if [[ ! -f .scratch/dn0r1/dn0-measured-binary/hven_bench_cell ]]; then
    cp -p build/lanes/hven/hven_bench_cell .scratch/dn0r1/dn0-measured-binary/hven_bench_cell
fi
sha256sum .scratch/dn0r1/dn0-measured-binary/hven_bench_cell
bash scripts/build_hven_pin.sh
./configure.sh > .scratch/dn0r1/configure.log 2>&1
cmake --build build -j"$(nproc)" --target hven_bench_cell > .scratch/dn0r1/build-measured.log 2>&1
grep -E "hven pin|RESOLVED" .scratch/dn0r1/configure.log
sha256sum build/lanes/hven/hven_bench_cell .hven/install-0a34c8a2adba/lib64/libhven.a
echo "FOREGROUND_END build_pin $(date -u +%FT%TZ)"
