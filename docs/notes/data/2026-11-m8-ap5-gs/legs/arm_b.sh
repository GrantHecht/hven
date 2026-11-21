#!/usr/bin/env bash
# M8 AP-5 G-S: freezes arm B (the release tree's hven_sqp_corpus at the code head) as build-gs/armBbin and prints its sha256.
# usage: arm_b.sh     Run as: flock /tmp/box-build.lock bash arm_b.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g-rb
rm -rf "$WT/build-gs/armBbin"; mkdir -p "$WT/build-gs/armBbin"
cp -p "$WT/build-gs/rel/bench/hven_sqp_corpus" "$WT/build-gs/armBbin/hven_sqp_corpus" || exit 1
sha256sum "$WT/build-gs/armBbin/hven_sqp_corpus"
