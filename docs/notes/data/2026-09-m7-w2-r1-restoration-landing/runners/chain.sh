#!/usr/bin/env bash
# usage: chain.sh LISTFILE -- each line "TAG CELL LABEL [ENV=V ...]" runs as its OWN locked leg via run.sh, sequentially.
R=/home/ghecht/Projects/hven/.scratch/m7w2r1
while read -r tag cell label envs; do
  [ -z "$tag" ] && continue; case "$tag" in \#*) continue;; esac
  $R/tools/run.sh "$tag" "$cell" "$label" $envs
done < "$1"
echo "CHAIN_DONE $1 $(date -Is)"
