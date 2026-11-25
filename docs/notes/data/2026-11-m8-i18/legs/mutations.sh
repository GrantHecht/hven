#!/usr/bin/env bash
# I18 mutation leg: control, then each mutation of mutate_i18.py: apply, rebuild hven_qp_seam_tests in <WT>/build-mut (Release),
# run the seam suite (4 gtest shards, one per pinned physical core of $PHYS), record the failing tests (<id>.killers.txt), restore
# the touched file from HEAD. The tree must be clean at start and is restored and rebuilt at the end.
# usage: mutations.sh <out-dir> <ids...>      run under: box-lane -- bash mutations.sh ...
set -o pipefail
source "$(dirname "$0")/env.sh"
OUT=$(realpath -m "${1:?out dir}"); shift
mkdir -p "$OUT"; cd "$WT" || exit 1
echo "FOREGROUND_START $(date -Is) lane=${BOX_LANE:-none}"
[ -z "$(git status --porcelain -- src include tests)" ] || { echo TREE_NOT_CLEAN; git status --short -- src include tests; exit 3; }
python3 "$L/mutate_i18.py" "$WT" check || exit 3
B=$WT/build-mut
build() { bash "$L/build.sh" "$WT" Release build-mut hven_qp_seam_tests > "$OUT/$1.build.txt" 2>&1; }
suite() {
  local id=$1 phys=($PHYS) pids=()
  for s in 0 1 2 3; do
    GTEST_TOTAL_SHARDS=4 GTEST_SHARD_INDEX=$s taskset -c "${phys[$s]}" "$B/tests/qp/hven_qp_seam_tests" --gtest_brief=1 > "$OUT/$id.shard$s.log" 2>&1 &
    pids+=($!)
  done
  for p in "${pids[@]}"; do wait "$p"; done
  cat "$OUT/$id".shard*.log | grep '^\[  FAILED  \]' | sed 's/ (.*//; s/^\[  FAILED  \] //' | grep -v 'tests, listed below' | sort -u > "$OUT/$id.killers.txt"
}
restore() { git checkout -q -- include/hven/detail/qp/walk/eqp_solve.h include/hven/detail/qp/walk/bordered_eqp.h tests/qp/support/qp_dump.h; }
build control || { echo CONTROL_BUILD_FAIL; exit 1; }
suite control
echo "control: $(wc -l < "$OUT/control.killers.txt") failing"
[ -s "$OUT/control.killers.txt" ] && { echo CONTROL_NOT_CLEAN; exit 1; }
for id in "$@"; do
  python3 "$L/mutate_i18.py" "$WT" apply "$id" || { restore; exit 1; }
  build "$id" || { echo "$id: BUILD_FAIL"; restore; exit 1; }
  suite "$id"
  restore
  n=$(wc -l < "$OUT/$id.killers.txt")
  echo "$id: $([ "$n" -gt 0 ] && echo KILLED || echo SURVIVED) ($n) $(head -3 "$OUT/$id.killers.txt" | tr '\n' ' ')"
done
build restored || { echo RESTORE_BUILD_FAIL; exit 1; }
[ -z "$(git status --porcelain -- src include tests)" ] || { echo TREE_NOT_RESTORED; exit 1; }
echo "FOREGROUND_END $(date -Is)"
