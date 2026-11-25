#!/usr/bin/env bash
# M8 DN-1 (../DECLARATION-dn1legs.txt): decode the CUTEst problems the S and T legs need in the local clone, with the
# harness's own scripts/decode_cutest.sh (SIFDecode, external): SET=small-dense (the 97 cutest-small-dense TRAIN
# problems, DN-0's; SIF files from hven-bench-sd) and the 23 cutest-nlp-35 TRAIN problems (named explicitly; the script's holdout guard runs on them).
# No HOLDOUT problem is decoded. Output: the clone's .scratch/h3/decoded (git-ignored).
# Run as: flock /tmp/box-build.lock bash hb_decode.sh
set -o pipefail
HB=/home/ghecht/Projects/hven-bench-dn1
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$HB" || exit 1
SET=small-dense bash scripts/decode_cutest.sh > .scratch/dn1-decode-sd.log 2>&1; echo "small-dense rc=$?"; tail -2 .scratch/dn1-decode-sd.log
mapfile -t TRAIN < <(python3 -c '
import json, sys
print("\n".join(p["problem"] for p in json.load(open(sys.argv[1]))["problems"] if p["split"] == "TRAIN"))' sets/cutest-nlp.json)
echo "nlp-35 TRAIN: ${#TRAIN[@]}"
# hven-bench-sd fetched only the small-dense SIF files; the 23 TRAIN instances are copied (read only) from the
# hven-bench main checkout's fetched set -- TRAIN names only, never a HOLDOUT file.
for p in "${TRAIN[@]}"; do
  [ -f "sets/cutest/sif/$p.SIF" ] || cp -p "/home/ghecht/Projects/hven-bench/sets/cutest/sif/$p.SIF" sets/cutest/sif/ || exit 1
done
bash scripts/decode_cutest.sh "${TRAIN[@]}" > .scratch/dn1-decode-nlp.log 2>&1; echo "nlp TRAIN rc=$?"; tail -2 .scratch/dn1-decode-nlp.log
ls .scratch/h3/decoded | wc -l
git status --porcelain
echo "FOREGROUND_END $(date -Is)"
