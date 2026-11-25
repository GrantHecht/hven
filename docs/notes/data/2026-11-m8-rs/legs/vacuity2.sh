#!/usr/bin/env bash
# RS-2 non-vacuity proofs for the re-pointed directory definitions (design 5.2 item 6): temporarily hide the directory a path-coupled test reads and show the
# tests FAIL (so they cannot pass vacuously), then restore.  usage: flock /tmp/box-build.lock setsid -w bash vacuity2.sh <out-file>
set -o pipefail
source "$(dirname "$0")/env.sh"
box_check
OUT=$(realpath -m "${1:?out}"); : > "$OUT"
cd "$WT" || exit 1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo "tracked tree not clean; abort"; exit 2; }
hide() { mv "$1" "$1.hidden"; }
unhide() { mv "$1.hidden" "$1"; }
trap 'for d in tests/trace/golden tests/model/compile_fail tests/qp/fixtures; do [ -d "$d.hidden" ] && mv "$d.hidden" "$d"; done' EXIT
run() { # name, dir, regex
  echo "== $1: hide $2; ctest -R '$3'" | tee -a "$OUT"
  hide "$2"
  ( cd build && setsid -w ctest -j4 -R "$3" > "$OUT.ctest.tmp" 2>&1 ); rc=$?
  grep -E "tests passed|tests failed" "$OUT.ctest.tmp" | tee -a "$OUT"; echo "ctest rc=$rc (nonzero expected)" | tee -a "$OUT"
  grep -E "\*\*\*Failed|Failed " "$OUT.ctest.tmp" | head -4 | tee -a "$OUT"
  unhide "$2"
  ( cd build && setsid -w ctest -j4 -R "$3" > "$OUT.ctest.tmp" 2>&1 ); rc2=$?
  grep -E "tests passed|tests failed" "$OUT.ctest.tmp" | tee -a "$OUT"; echo "restored: ctest rc=$rc2 (0 expected)" | tee -a "$OUT"
  [ $rc -ne 0 ] && [ $rc2 -eq 0 ]
}
v=0
run goldens   tests/trace/golden        '^(HumanTraceGoldens|TraceV1Fixtures)\.' || v=1
run compile_fail tests/model/compile_fail '^hven_compile_fail_'                   || v=1
rm -f "$OUT.ctest.tmp"
[ -z "$(git status --porcelain --untracked-files=no)" ] && echo "tree restored clean" | tee -a "$OUT"
echo "VACUITY_PROOF rc=$v" | tee -a "$OUT"; exit $v
