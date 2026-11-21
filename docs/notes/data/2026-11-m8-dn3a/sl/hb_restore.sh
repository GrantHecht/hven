#!/usr/bin/env bash
# M8 DN-3a (../DECLARATION.txt): copied from ../../2026-11-m8-dn1d-singular/r3/sl/hb_restore.sh. After the FA leg the clone
# is as found: HEAD on dn1b-legs (fa_setup.sh already returns it there; this checks). dn3a-legs and dn3a-legs-base stay
# as the record (local only, never pushed).
# Run as: flock /tmp/box-build.lock bash hb_restore.sh
set -o pipefail
HB=/home/ghecht/Projects/hven-bench-dn1
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
echo PGREP_EMPTY
cd "$HB" || exit 1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo HB_DIRTY; exit 4; }
git checkout --quiet dn1b-legs && git log --oneline -1 && git status --short --untracked-files=no | head
