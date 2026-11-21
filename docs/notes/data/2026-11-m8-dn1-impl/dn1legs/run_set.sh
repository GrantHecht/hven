#!/usr/bin/env bash
# M8 DN-1 set driver (../DECLARATION-dn1legs.txt): the four legs in order, each under its own flock.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
D=$WT/docs/notes/data/2026-11-m8-dn1-impl/dn1legs
S_SHA=552a772eaae1d7c0e2d37a1ce4afe2bc0748f48ba18765f0b42a2b9af69964a2
leg() { local name=$1; shift
  pgrep -a hven_bench_cell && { echo "PGREP_BUSY before $name"; return 3; }
  mkdir -p "$D/out"; flock /tmp/box-build.lock "$@" > "$D/out/$name.log" 2>&1
  echo "LEG $name rc=$? $(date -Is)"; }
leg a bash "$D/a_run.sh" "$D/out/a" c20dce440d6bd8f889237b278edb0f22a9b978dbc9b0495e2fe748a906ce60a2
leg t bash "$D/t_run.sh" "$D/out/t"
leg s bash "$D/s_run.sh" "$D/out/s" "$S_SHA"
leg m bash "$D/m_run.sh" "$D/out/m" "$S_SHA"
echo SET_DONE
