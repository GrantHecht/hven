#!/usr/bin/env bash
# RS-2 P-SYM arms (DECLARATION-RS2-r2.md): build the BEFORE arm (the RS-1e head) and the AFTER arm (this branch's HEAD) with `__FILE__` neutralised
# (tools/neutral_flags.py: the same flag list in both arms), each in the SAME build directory path (P-SYM's precondition), and capture P-SYM for each.
# usage:  cp legs/psym_arms.sh legs/env.sh tools/neutral_flags.py to .scratch/instr/ first (the checkout below removes these files from the tree),
#         then:  WT=<worktree> flock /tmp/box-build.lock setsid -w bash .scratch/instr/psym_arms.sh <out-dir>
set -o pipefail
source "$(dirname "$0")/env.sh"
box_check
OUT=$(realpath -m "${1:?out-dir}")
BASE=${PSYM_BASE:-3edf5ede4ef2cd95f1fa5d94ae19d40e018ae1d3}
cd "$WT" || exit 1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo "tracked tree not clean; abort"; exit 2; }
BR=$(git symbolic-ref --short HEAD) || exit 2
AFTER=$(git rev-parse HEAD)
FLAGS=$(python3 "$(dirname "$0")/neutral_flags.py" "$WT" "$BASE" "$AFTER") || exit 2
echo "$FLAGS" > "$OUT.flags.txt" 2>/dev/null || { mkdir -p "$(dirname "$OUT")"; echo "$FLAGS" > "$OUT.flags.txt"; }
mkdir -p "$OUT"
restore() { git -C "$WT" checkout -q -- scripts/psym_compare.sh; git -C "$WT" checkout -q "$BR"; }
trap restore EXIT
worst=0
for ARM in base after; do
  if [ $ARM = base ]; then REV=$BASE; else REV=$AFTER; fi
  git checkout -q -- scripts/psym_compare.sh
  git checkout -q --detach "$REV" || exit 2
  # RS-3 (DECLARATION-RS3-r2.md): the capture tool is stamped by its own sha, and RS-3 edits one comment line of it; the BEFORE arm is captured with the AFTER-version script (overlay,
  # removed again before the next checkout) so both arms carry one stamp. The logic of the two versions is identical (one comment line differs).
  if [ $ARM = base ]; then git show "$AFTER:scripts/psym_compare.sh" > scripts/psym_compare.sh || exit 2; fi
  echo "== arm $ARM @ $(git rev-parse --short HEAD) $(date -Is)"
  D=$WT/build-n
  rm -rf "$D"
  GIT_DIR=$(git rev-parse --absolute-git-dir) GIT_WORK_TREE=$WT \
  cmake -S "$WT" -B "$D" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE "-DCMAKE_CXX_FLAGS=$FLAGS" \
    -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/home/ghecht/Projects/hven/build/_deps/googletest-src > "$OUT/$ARM-configure.log" 2>&1 \
    || { echo "CONFIGURE_FAIL $ARM"; tail -20 "$OUT/$ARM-configure.log"; exit 1; }
  ninja -C "$D" -j16 -k 0 > "$OUT/$ARM-build.log" 2>&1; rc=$?; echo "build $ARM rc=$rc"; tail -1 "$OUT/$ARM-build.log"; [ $rc -gt $worst ] && worst=$rc
  [ $rc -eq 0 ] || exit $rc
  rm -rf "$OUT/psym-$ARM"
  scripts/psym_compare.sh capture "$OUT/psym-$ARM" build-n > "$OUT/$ARM-capture.txt" 2>&1; echo "capture $ARM rc=$?"
  cp "$D/compile_commands.json" "$OUT/cc-$ARM.json"
done
echo "worst=$worst $(date -Is)"
exit $worst
