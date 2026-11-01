#!/usr/bin/env bash
# P-SYM, M7 W2 M1 FIX ROUND 1: HEAD vs $PSYM_BEFORE (the PRE-FIX head), ONE build directory at ONE
# absolute path on both arms, CCACHE_DISABLE=1, both configs.
#
# THE FIX ROUND'S GATE IS STRICTER THAN THE FIRST TAKE'S. There, exit 1 was the
# expected status and the gate was the CLASSIFICATION of every differing object,
# because the task added a PUBLIC field to SqpOptions and to SqpCounters and
# `sizeof` moved in every TU that includes either header. Here the two edits are
# COMMENT-ONLY, so the expected result is exit 0 and ZERO differing objects; any
# differing object at all is a STOP.
set -uo pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2m1
BD=$S/psym-build-fix1b
OUT=$S/psym-fix1b
CFG="${PSYM_CONFIG:-Release}"
PSYM_BEFORE="${PSYM_BEFORE:?PSYM_BEFORE is required}"
mkdir -p "$OUT"
HEAD_SHA=$(git -C "$R" rev-parse HEAD)
BRANCH=$(git -C "$R" rev-parse --abbrev-ref HEAD)
echo "PSYM config=$CFG  after=$HEAD_SHA ($BRANCH)  before=$PSYM_BEFORE"
[ -z "$(git -C "$R" status --porcelain)" ] || { echo "REFUSED: the tree is not clean"; exit 2; }

configure() {
  cmake -S "$R" -B "$BD" -G Ninja -DCMAKE_BUILD_TYPE="$CFG" \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON > "$OUT/configure-$CFG.log" 2>&1
}
set -e
rm -rf "$BD"
configure
CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-after-$CFG.log" 2>&1
"$R/scripts/psym_compare.sh" capture "$OUT/after-$CFG" "$BD" > "$OUT/capture-after-$CFG.log" 2>&1

# THE BEFORE ARM, IN THE SAME DIRECTORY AND FROM THE SAME SOURCE PATH -- the
# script's own precondition. The worktree is detached to $PSYM_BEFORE and restored;
# the trap restores it on any exit.
restore() { git -C "$R" checkout -q "$BRANCH" 2>/dev/null || git -C "$R" checkout -q "$HEAD_SHA"; }
trap restore EXIT
git -C "$R" checkout -q --detach "$PSYM_BEFORE"
CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-before-$CFG.log" 2>&1
"$R/scripts/psym_compare.sh" capture "$OUT/before-$CFG" "$BD" > "$OUT/capture-before-$CFG.log" 2>&1
git -C "$R" checkout -q "$BRANCH"
trap - EXIT
echo "tree restored to $(git -C "$R" rev-parse --abbrev-ref HEAD) $(git -C "$R" rev-parse --short HEAD)"

set +e
"$R/scripts/psym_compare.sh" compare "$OUT/before-$CFG" "$OUT/after-$CFG" > "$OUT/compare-$CFG.txt" 2>&1
rc=$?
echo "PSYM_COMPARE_RC=$rc  (0 is EXPECTED here: the edits are comment-only)"
tail -30 "$OUT/compare-$CFG.txt"
exit 0
