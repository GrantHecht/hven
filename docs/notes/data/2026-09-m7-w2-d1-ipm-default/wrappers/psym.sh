#!/usr/bin/env bash
# P-SYM, M7 W2 D1: HEAD vs 33f68b1e (the PRE-EVENT HEAD), ONE build directory at
# ONE absolute path on both arms, CCACHE_DISABLE=1, both configs.
#
# EXIT 1 IS THE EXPECTED STATUS and the gate is the CLASSIFICATION of every
# differing object, not byte-identity. This task changes three DEFAULT INITIALISERS
# on IpmOptions, so every TU that constructs one emits different immediates, and
# it edits one library .cpp, one bench header, two bench .cpp and six test TUs.
set -uo pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2d1
BD=$S/psym-build
OUT=$S/psym
CFG="${PSYM_CONFIG:-Release}"
mkdir -p "$OUT"
HEAD_SHA=$(git -C "$R" rev-parse HEAD)
BRANCH=$(git -C "$R" rev-parse --abbrev-ref HEAD)
echo "PSYM config=$CFG  after=$HEAD_SHA ($BRANCH)  before=33f68b1e"
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
# script's own precondition. The worktree is detached to 33f68b1e and restored;
# the trap restores it on any exit.
restore() { git -C "$R" checkout -q "$BRANCH" 2>/dev/null || git -C "$R" checkout -q "$HEAD_SHA"; }
trap restore EXIT
git -C "$R" checkout -q --detach 33f68b1e
CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-before-$CFG.log" 2>&1
"$R/scripts/psym_compare.sh" capture "$OUT/before-$CFG" "$BD" > "$OUT/capture-before-$CFG.log" 2>&1
git -C "$R" checkout -q "$BRANCH"
trap - EXIT
echo "tree restored to $(git -C "$R" rev-parse --abbrev-ref HEAD) $(git -C "$R" rev-parse --short HEAD)"

set +e
"$R/scripts/psym_compare.sh" compare "$OUT/before-$CFG" "$OUT/after-$CFG" > "$OUT/compare-$CFG.txt" 2>&1
rc=$?
echo "PSYM_COMPARE_RC=$rc  (1 is EXPECTED here; the gate is the classification)"
tail -30 "$OUT/compare-$CFG.txt"
exit 0
