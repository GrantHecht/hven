#!/usr/bin/env bash
# usage: replay_trace.sh LABEL OUTDIR SUFFIX [qp_replay args...] < list-of-dumps
# ONE locked leg: every listed dump is replayed with HVEN_D4_TRACE=1 (+ any HVEN_D4_FIX in the env);
# OUTDIR/<cell>_<qp>SUFFIX.trace gets the trace, OUTDIR/<cell>_<qp>SUFFIX.res the REPLAY line, .sol the solution.
set -o pipefail
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4
label="$1"; outdir="$2"; suffix="$3"; shift 3
mkdir -p "$outdir"
export LIST="$(cat)"
$D/tools/leg.sh "replay-$label" bash -c '
set -o pipefail
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4
outdir="$1"; suffix="$2"; shift 2
for f in $LIST; do
  cell=$(basename $(dirname $f)); q=$(basename $f .txt)
  base="$outdir/${cell}_${q}${suffix}"
  env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 HVEN_D4_TRACE=1 taskset -c 2 $D/replay-build/qp_replay "$f" --out "$base.sol" "$@" > "$base.res" 2> "$base.trace"
  echo "$cell $q $suffix $(head -1 $base.res)"
done
' _ "$outdir" "$suffix" "$@"
