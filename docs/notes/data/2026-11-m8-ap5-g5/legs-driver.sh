#!/usr/bin/env bash
# G5 legs driver (DECLARATION.md order). Stops at the first failed leg. Each leg takes the box lock itself.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/g5
E=$W/docs/notes/data/2026-11-m8-ap5-g5
ARMB=$1; ARMA=663b1138968b6b82811b730dc49d278e868b5dad56fd8ccf7b8700bcbc199a77; HEAD_SHA=$2
export GIT_CEILING_DIRECTORIES=$W TMPDIR=$W/build-g5/tmp MKL_NUM_THREADS=1
leg() { local name=$1; shift; echo "LEG_START $name $(date -Is)"
  while [ -n "$(pgrep -a hven_bench_cell)" ]; do sleep 30; done
  flock /tmp/box-build.lock setsid -w "$@"; local rc=$?
  echo "LEG_END $name rc=$rc $(date -Is)"; [ $rc -eq 0 ] || { echo "DRIVER_STOPPED_AT $name"; exit $rc; }; }
leg runtime bash $E/runtime/run_all.sh $ARMB $ARMA
leg attached bash $E/runtime/run_attached.sh $ARMB $ARMA
leg chain bash $E/legs/chain.sh $HEAD_SHA $ARMB
echo DRIVER_DONE
