#!/usr/bin/env bash
# usage: replay_all.sh LABEL OUTFILE [qp_replay args...] -- replays every dump listed on stdin (one path per line) inside ONE locked leg
set -o pipefail
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4
label="$1"; out="$2"; shift 2
export LIST="$(cat)"
$D/tools/leg.sh "replay-$label" bash -c '
set -o pipefail
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4
out="$1"; shift
for f in $LIST; do
  echo "== $f $*"
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 $D/replay-build/qp_replay "$f" "$@" 2>/dev/null
done > "$out"
' _ "$out" "$@" 2>&1 | grep -E "FOREGROUND|PGREP" >> $D/logs/legs.log
