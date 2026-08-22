#!/bin/bash
# =============================================================================
# THE LAYOUT LEG.
#
# PROTOCOL. The same alternation and the same two estimators as the IPM wall
# leg beside it (read ipm_wall_leg.sh's header for both), over a different
# quantity: what a TRANSCRIPTION costs, not what a solve costs. Two arms:
#   serial    MKL_NUM_THREADS=1, pinned to one physical core. Isolates the
#             code-path cost; the layout itself is single-threaded, so this is
#             the arm a layout delta shows in cleanly.
#   threaded  default thread count, unpinned, machine otherwise idle. The arm
#             the partitioned whole-solve cell lives in.
# base/head ALTERNATED per rep so machine drift lands on both sides.
#
# WHY IT EXISTS. The IPM wall leg times whole solves of a three-piece,
# one-application problem, in which per-claim and per-piece LAYOUT work is a
# rounding error. Layout cost is what this leg observes, on a problem shaped
# like a collocation transcription: many applications over overlapping variable
# windows, spread across several pieces, more than one partition.
#
# READING IT. Four cells per size. `transcribe` is what a consumer that never
# asks about structural identity pays to lay a problem; `transcribe+key` is the
# same lay plus one model_structure_key() read, and the difference between them
# is the deferred digest cost -- INFORMATIONAL, recorded so that cost is
# visible rather than hidden. `construct` includes building the pieces.
# `solve` is one partitioned whole solve. Every cell prints an identity column
# (the structural key digest and claim count for the layout cells, the
# objective/iterations/flag for the solve cell); those must be EQUAL between
# arms, and a delta in them is a correctness finding, not a timing one.
#
# USAGE: layout_wall_leg.sh [REPS] [INNER]   (defaults 9 and 15)
# Binaries: layout_base / layout_head beside this script, built by
# build_layout_time.sh against the base and head libhven.a.
# =============================================================================
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPS=${1:-9}
INNER=${2:-15}
for arm in serial threaded; do
  echo "=== arm: $arm"
  for r in $(seq 1 "$REPS"); do
    for side in base head; do
      if [ "$arm" = serial ]; then
        out=$(MKL_NUM_THREADS=1 taskset -c 2 "$HERE/layout_$side" "$INNER")
      else
        out=$("$HERE/layout_$side" "$INNER")
      fi
      echo "$out" | sed "s/^/rep$r $side /"
    done
  done
done
