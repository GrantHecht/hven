#!/usr/bin/env bash
R=/home/ghecht/Projects/hven/.claude/worktrees/pf2
S=$R/docs/notes/data/2026-11-m8-pf12/scripts
for t in m6-19f7674f m8-331b31837; do
  echo "== $t $(date -u +%FT%TZ)"
  $S/build.sh $R/.scratch/$t $R/.scratch/build-$t
done
