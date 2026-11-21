#!/usr/bin/env bash
# usage: setup_bench.sh <layer> <sha>  -- private hven-bench worktree at e03cbad whose pin file names the pf6 commit (local edit, never committed).
set -euo pipefail
LAYER=$1; SHA=$2; HB=/home/ghecht/Projects/hven-bench-pf6-$LAYER
[ -d "$HB" ] || git -C /home/ghecht/Projects/hven-bench worktree add --detach "$HB" e03cbad
cd "$HB"
sed -i "s/^set(HVEN_PIN_SHA \".*\")/set(HVEN_PIN_SHA \"$SHA\")/; s/^set(HVEN_PIN_BRANCH \".*\")/set(HVEN_PIN_BRANCH \"pf6\")/" cmake/hven_pin.cmake
mkdir -p sets/cutest; rm -rf sets/cutest/sif; ln -sfn /home/ghecht/Projects/hven-bench/sets/cutest/sif sets/cutest/sif
mkdir -p .scratch/h3; ln -sfn /home/ghecht/Projects/hven-bench/.scratch/h3/decoded .scratch/h3/decoded
ls -la sets/cutest .scratch/h3
