#!/usr/bin/env bash
# RS-4 header self-containment sweep: every header under include/hven (public and detail) is compiled ALONE (`#include <hven/...>` as the only line, -fsyntax-only)
# with the flag regime of the library build.  A header that only compiled because an earlier include had already pulled in what it needs fails here.
# usage: header_sweep.sh ROOT OUTFILE     (run under the box lock; prints PASS/FAIL counts; OUTFILE lists FAIL headers with the first error)
set -o pipefail
ROOT=$(realpath "${1:?root}"); OUT=$(realpath -m "${2:?out}")
FLAGS="-DEIGEN_DONT_PARALLELIZE -DEIGEN_INITIALIZE_MATRICES_BY_ZERO -DEIGEN_MAX_ALIGN_BYTES=32 -DFMT_HEADER_ONLY -DFMT_USE_LOCALE=0 -DHVEN_DEFAULT_ARENA_SIZE=256 -DHVEN_DEFAULT_IPM_THREADS=8 -DHVEN_VERSION_STRING=\"0.2.0\" -I$ROOT/include -I/opt/intel/oneapi/mkl/latest/include -isystem $ROOT/dep/eigen -isystem $ROOT/dep/fmt/include -DMKL_LP64 -m64 -std=c++20 -fsyntax-only -fopenmp=libiomp5 -march=native"
T=$(mktemp -d "${TMPDIR:-/tmp}/sweep.XXXXXX")
( cd "$ROOT/include" && find hven -name '*.h' | LC_ALL=C sort ) > "$T/list"
one() { h=$1; f="$T/$(echo "$h" | tr / _).cpp"; printf '#include <%s>\n' "$h" > "$f"
  if o=$(/usr/bin/clang++ $FLAGS -x c++ "$f" 2>&1); then echo "PASS $h"; else echo "FAIL $h :: $(echo "$o" | grep -m1 'error')"; fi; }
export -f one; export FLAGS T
xargs -P 6 -I{} bash -c 'one {}' < "$T/list" | LC_ALL=C sort > "$OUT"
echo "PASS $(grep -c '^PASS' "$OUT") FAIL $(grep -c '^FAIL' "$OUT")"; rm -rf "$T"
