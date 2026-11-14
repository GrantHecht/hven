#!/usr/bin/env bash
# M8 DN-3a (../DECLARATION.txt, FA): structure from ../../2026-11-m8-dn1d-singular/r3/sl/hb_setup_e.sh, re-pointed.
# The LOCAL hven-bench clone /home/ghecht/Projects/hven-bench-dn1 (never pushed). Two branches from DN-1d r3's commit E
# (dn1d-legs-r3, six SQP side columns):
#   dn3a-legs       pin -> the code head + hb_patch_f.py (three DN-3a tally columns; the renamed retake counter);
#   dn3a-legs-base  pin -> arm A b70e45cfb + hb_patch_f.py rename-only (no tally exists there).
# Each: scripts/build_hven_pin.sh (.hven/install-<sha12>), a configure into .scratch/<branch>-build (-Uhven_DIR), the lane
# binary frozen to build-dn3a/fabin (head) and build-dn3a/fabasebin (base). HEAD is put back on dn1b-legs afterwards
# (as found); the two branches stay as the record. Problem sources are read from the clone's working tree (unchanged
# by either commit: only cmake/hven_pin.cmake and lanes/hven/hven_bench_cell.cpp differ).
# usage: fa_setup.sh <code head sha>   Run as: flock /tmp/box-build.lock bash fa_setup.sh <sha>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-acfc8f7d568381148
D=$WT/docs/notes/data/2026-11-m8-dn3a/sl
HB=/home/ghecht/Projects/hven-bench-dn1
HEAD_SHA=${1:?code head sha}
BASE_SHA=b70e45cfb05e50a05852a1092351f5518fe268a8
COMMIT_E=f8c54bb
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
[ "$(git -C "$WT" rev-parse "$HEAD_SHA^{commit}")" = "$HEAD_SHA" ] || { echo CODE_HEAD; exit 4; }
cd "$HB" || exit 1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo HB_DIRTY; git status --porcelain; exit 4; }
[ "$(git rev-parse --short=7 dn1d-legs-r3)" = "$COMMIT_E" ] || { echo COMMIT_E; exit 4; }
mkdir -p "$WT/build-dn3a"
one() { # <branch> <pin sha> <pin branch> <patch mode> <out dir>
  local br=$1 sha=$2 pbr=$3 mode=$4 out=$5
  git rev-parse --verify -q "$br" >/dev/null && { echo "BRANCH_EXISTS $br"; exit 4; }
  git checkout --quiet -b "$br" dn1d-legs-r3 || exit 1
  sed -i -e "s/^set(HVEN_PIN_SHA \".*\")$/set(HVEN_PIN_SHA \"$sha\")/" \
         -e "s/^set(HVEN_PIN_BRANCH \".*\")$/set(HVEN_PIN_BRANCH \"$pbr\")/" \
         -e 's/^set(HVEN_PIN_DATE ".*")$/set(HVEN_PIN_DATE "2026-11-12")/' cmake/hven_pin.cmake
  python3 "$D/hb_patch_f.py" lanes/hven/hven_bench_cell.cpp $mode || exit 1
  git diff --stat
  git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit --quiet -am \
    "EVIDENCE-ONLY (local clone, never pushed): pin hven ${sha:0:9} ($pbr) + hven docs/notes/data/2026-11-m8-dn3a/sl/hb_patch_f.py ${mode:-three DN-3a tally columns} (M8 DN-3a FA leg)" || exit 1
  git log --oneline -1
  bash scripts/build_hven_pin.sh > "$WT/build-dn3a/$br-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$WT/build-dn3a/$br-pin.log"; exit 1; }
  local prefix="$HB/.hven/install-${sha:0:12}"
  rm -rf "$HB/.scratch/$br-build"
  cmake -S "$HB" -B "$HB/.scratch/$br-build" -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_PREFIX_PATH="$prefix" > "$WT/build-dn3a/$br-configure.log" 2>&1 || { echo CONFIGURE_FAIL; exit 1; }
  cmake --build "$HB/.scratch/$br-build" --parallel 16 --target hven_bench_cell > "$WT/build-dn3a/$br-build.log" 2>&1 || { echo BUILD_FAIL; tail -20 "$WT/build-dn3a/$br-build.log"; exit 1; }
  mkdir -p "$out"
  cp -p "$HB/.scratch/$br-build/lanes/hven/hven_bench_cell" "$out/hven_bench_cell" || exit 1
  sha256sum "$out/hven_bench_cell"
  grep -E 'HVEN_PIN_SHA' "$prefix/HVEN_PIN.txt"
  git checkout --quiet dn1b-legs || exit 1
}
one dn3a-legs "$HEAD_SHA" m8-dn3 "" "$WT/build-dn3a/fabin"
one dn3a-legs-base "$BASE_SHA" m8 rename-only "$WT/build-dn3a/fabasebin"
git log --oneline -1
git status --porcelain --untracked-files=no
echo "FOREGROUND_END $(date -Is)"
