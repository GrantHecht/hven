#!/usr/bin/env bash
# M8 AP-5 G-RB: freezes arm B (the release tree's hven_sqp_corpus at the code head) as build-grb/armBbin and prints its sha256.
# usage: arm_b.sh     Run as: flock /tmp/box-build.lock bash arm_b.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g-rb
rm -rf "$WT/build-grb/armBbin"; mkdir -p "$WT/build-grb/armBbin"
cp -p "$WT/build-grb/rel/bench/hven_sqp_corpus" "$WT/build-grb/armBbin/hven_sqp_corpus" || exit 1
sha256sum "$WT/build-grb/armBbin/hven_sqp_corpus"
