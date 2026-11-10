#!/usr/bin/env bash
# M8 DN-1c: after the S and T legs, the clone is put back as found: HEAD on dn1b-legs (DN-1b's commit C). The branch
# dn1c-legs (commit D) and .hven/install-<head12> stay as the legs' record (local only, never pushed).
# Run as: flock /tmp/box-build.lock bash hb_restore.sh
set -o pipefail
HB=/home/ghecht/Projects/hven-bench-dn1
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
echo PGREP_EMPTY
cd "$HB" || exit 1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo HB_DIRTY; exit 4; }
git checkout --quiet dn1b-legs && git log --oneline -1 && git status --short | head
