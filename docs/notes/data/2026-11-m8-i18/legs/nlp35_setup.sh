#!/usr/bin/env bash
# I18 nlp-35 setup (not a leg): the decoded cutest-nlp-35 instances of nlp35-cells.csv, copied from the local hven-bench checkout's
# .scratch/h3/decoded into the private lane clone's (train_setup.sh copies only the small-dense set). usage: nlp35_setup.sh
set -o pipefail
source "$(dirname "$0")/env.sh"
SRC=/home/ghecht/Projects/hven-bench/.scratch/h3/decoded; DST=$WT/.scratch/hb/.scratch/h3/decoded
for p in $(cut -d, -f2 "$L/nlp35-cells.csv" | sort -u); do
  [ -d "$DST/$p" ] && continue
  [ -d "$SRC/$p" ] || { echo "NO_SOURCE $p"; exit 1; }
  cp -r "$SRC/$p" "$DST/$p" || exit 1
  echo "copied $p"
done
echo "decoded: $(ls "$DST" | wc -l)"
