#!/usr/bin/env bash
# PF-1b dev experiments (not evidence). usage: exps.sh <dump> <outdir> <specfile: "label ENV=.. -- args..." per line>
set -o pipefail
R=/home/ghecht/Projects/hven/.claude/worktrees/pf1b/build-pf1b/diag/tests/pf1b_replay
DUMP=${1:?}; OUT=${2:?}; SPEC=${3:?}
mkdir -p "$OUT"
while read -r label rest; do
  [ -z "$label" ] && continue
  case "$label" in \#*) continue;; esac
  envs=${rest%%--*}; args=${rest#*--}
  # shellcheck disable=SC2086
  env $envs "$R" "$DUMP" --csv "$OUT/$label.csv" $args > "$OUT/$label.txt" 2> "$OUT/$label.err"
  echo "== $label rc=$? [$envs |$args ]"
  grep -E "^PF1[RAKES]|^PF1H" "$OUT/$label.txt"
  grep -c PF1BRUNG "$OUT/$label.err" | sed 's/^/rungs_traced=/'
done < "$SPEC"
