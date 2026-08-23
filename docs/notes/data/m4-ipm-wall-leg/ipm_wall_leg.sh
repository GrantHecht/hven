#!/bin/bash
# =============================================================================
# THE STANDING IPM WALL LEG.
#
# PROTOCOL. Whole solves of a dense-Jacobian NLP through NLPSolver, which is the
# driver the interior-point evaluation path sits under. Two arms:
#   serial    MKL_NUM_THREADS=1, pinned to one physical core. The standing
#             protocol; isolates the code-path cost.
#   threaded  default thread count, unpinned, machine otherwise idle. The arm in
#             which any change to the evaluation path's own parallel structure
#             would show.
# base/head ALTERNATED per rep so machine drift lands on both sides. Each rep
# reports the median and the minimum of $INNER whole solves at each size.
#
# WHY IT EXISTS. The standing SQP bench (hven_sqp_bench) and the ssn corpus
# drive SqpDriver, which never reaches InteriorPointSolver, so neither can
# observe a change to the interior-point consumption path. This is the leg that
# does.
#
# READING IT. aggregate.py prints two estimators per cell. The MEDIAN estimator
# carries this instrument's rep-to-rep noise, which on these problem sizes is
# 0.5-0.8% of the measurement (and larger on the smallest threaded cell) --
# comparable to the effects being looked for, so a median delta under ~1% is not
# by itself a signal. The MINIMUM estimator is the low-noise one and is what a
# per-task delta should be read off. Both are printed, alongside the base arm's
# rep-spread, so a reader can see whether a delta clears the noise.
#
# USAGE: ipm_wall_leg.sh [REPS] [INNER]   (defaults 9 and 15)
# Binaries: ipm_base / ipm_head beside this script, built by build_ipm_time.sh
# against the base and head libhven.a.
# =============================================================================
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPS=${1:-9}
INNER=${2:-15}

# BUILD HEADER. Which binaries produced the rows below, byte for byte, and
# which source they were built from. A log without this cannot be told apart
# from one produced by a stale probe -- which is exactly how a probe that
# printed the convergence flag under the name `iters` outlived the source
# beside it.
echo "# leg: ipm wall leg"
echo "# date: $(date -Is)   host: $(uname -sr)"
echo "# probe source: $HERE/ipm_time.cpp  sha256=$(sha256sum "$HERE/ipm_time.cpp" | cut -d' ' -f1)"
for side in base head; do
  b="$HERE/ipm_$side"
  if [ -f "$b" ]; then
    echo "# binary $side: $b  sha256=$(sha256sum "$b" | cut -d' ' -f1)  mtime=$(date -Ir -r "$b")"
  else
    echo "# binary $side: $b  MISSING"
  fi
done
echo "# reps=$REPS inner=$INNER  loadavg=$(cut -d' ' -f1-3 /proc/loadavg)"
for arm in serial threaded; do
  echo "=== arm: $arm"
  for r in $(seq 1 "$REPS"); do
    for side in base head; do
      if [ "$arm" = serial ]; then
        out=$(MKL_NUM_THREADS=1 taskset -c 2 "$HERE/ipm_$side" "$INNER")
      else
        out=$("$HERE/ipm_$side" "$INNER")
      fi
      echo "$out" | sed "s/^/rep$r $side /"
    done
  done
done
