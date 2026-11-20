#!/usr/bin/env bash
# I18 corpus leg: the 146 E2-D4 dumps (tests/qp/fixtures/e2-d4 minus FORMAT.txt and I18_*, plus the m7 corpus dumps) and the issue dump
# at {border, refactorize} x {1e-9, 1e-10}, re-solved by corpus_replay built against <arm worktree>'s <build dir> installed into
# .scratch/inst-<label>; the loader is THIS worktree's (both arms read hkc= alike). Counters only: co-run, one process per pinned
# physical core ($PHYS), MKL_NUM_THREADS=1, 300 s per dump (a dump at the cap is a TIMEOUT row and fails the check).
# usage: corpus.sh <arm worktree> <build dir> <label> <out dir>      run under: box-lane -- bash corpus.sh ...
set -o pipefail
source "$(dirname "$0")/env.sh"
AWT=${1:?arm worktree}; BDIR=$AWT/${2:?build dir}; LABEL=${3:?label}; OUT=$(realpath -m "${4:?out dir}")
mkdir -p "$OUT"
echo "FOREGROUND_START $(date -Is) lane=${BOX_LANE:-none} phys=$PHYS"
INST=$WT/.scratch/inst-$LABEL; CB=$WT/.scratch/corpus-build-$LABEL
rm -rf "$INST" "$CB"
cmake --install "$BDIR" --prefix "$INST" > "$OUT/$LABEL-install.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
cmake -S "$L" -B "$CB" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$INST" \
  -DI18_TESTS_ROOT="$WT/tests" > "$OUT/$LABEL-configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$OUT/$LABEL-configure.log"; exit 1; }
ninja -C "$CB" > "$OUT/$LABEL-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$OUT/$LABEL-build.log"; exit 1; }
BIN=$CB/corpus_replay
echo "libhven sha256 $(sha256sum $INST/lib*/libhven.a | cut -d' ' -f1) replay sha256 $(sha256sum $BIN | cut -d' ' -f1)"
LIST=$( { ls "$WT"/tests/qp/fixtures/e2-d4/*.txt | grep -v -e FORMAT -e I18_; find "$WT/docs/notes/data/2026-09-m7-w2-e2-d4-feasible-qps/dumps" -name 'qp*.txt'; } | sort)
[ "$(echo "$LIST" | wc -l)" -eq 146 ] || { echo CORPUS_NOT_146; exit 3; }
ISSUE=$WT/tests/qp/fixtures/e2-d4/I18_qp_m137.txt
JOBF=$OUT/$LABEL-jobs.txt
{ for f in $LIST; do echo "$f"; done; for a in border refactorize; do for t in 1e-9 1e-10; do echo "$ISSUE --algebra $a --feas $t"; done; done; } > "$JOBF"
one() {  # one job line -> one row; the pinned cpu is the xargs slot's physical core
  local phys=($PHYS); local cpu=${phys[$SLOT]}
  local f=$1; shift
  local name; name=$(echo "$f" | sed "s|$WT/||; s|/|__|g")
  [ $# -gt 0 ] && name="$name@$2@$4"
  local r; r=$(timeout 300 taskset -c "$cpu" "$BIN" "$f" "$@" 2>/dev/null); local rc=$?
  echo "$name ${r:-TIMEOUT_OR_FAIL rc=$rc}"
}
export -f one; export BIN WT PHYS
xargs -P "$JOBS" --process-slot-var=SLOT -L1 bash -c 'one "$@"' _ < "$JOBF" | sort > "$OUT/$LABEL-rows.txt"
echo "rows $(wc -l < "$OUT/$LABEL-rows.txt") (expect 150); $(awk '{print $2}' "$OUT/$LABEL-rows.txt" | sort | uniq -c | tr '\n' ' ')"
echo "FOREGROUND_END $(date -Is)"
[ "$(wc -l < "$OUT/$LABEL-rows.txt")" -eq 150 ]
