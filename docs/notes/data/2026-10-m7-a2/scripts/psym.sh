#!/bin/bash
# A2 P-SYM (design page s6.1 G3): base f424c8db vs head, both configs, ONE build directory per
# config (the precondition: same absolute build path), every build its own lock hold.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa38d0451de957235
S=$W/.scratch/a2
BD=$S/psym-build
OUT=$S/psym
BASE=f424c8db
rm -rf "$OUT"
mkdir -p "$OUT"
HEAD_SHA=$(git -C "$W" rev-parse HEAD)
BRANCH=$(git -C "$W" rev-parse --abbrev-ref HEAD)
[ -z "$(git -C "$W" status --porcelain --untracked-files=no)" ] || { echo "REFUSED: tree not clean"; exit 2; }
echo "P-SYM head=$HEAD_SHA base=$BASE branch=$BRANCH"
locked() { # NAME COMMAND...
    local name=$1
    shift
    if pgrep -a hven_bench_cell; then echo "PGREP_NONEMPTY $name"; else echo "PGREP_EMPTY $name"; fi
    echo "FOREGROUND_START $name $(date -Is)"
    flock /tmp/box-build.lock "$@"
    local rc=$?
    echo "FOREGROUND_END $name rc=$rc $(date -Is)"
    return $rc
}
restore() { git -C "$W" checkout -q "$BRANCH"; }
rc=0
for CFG in Release Debug; do
    rm -rf "$BD"
    cmake -S "$W" -B "$BD" -G Ninja -DCMAKE_BUILD_TYPE="$CFG" -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
        > "$OUT/configure-after-$CFG.log" 2>&1 || exit 91
    locked "psym-after-$CFG" env CCACHE_DISABLE=1 cmake --build "$BD" -j14 \
        > "$OUT/build-after-$CFG.log" 2>&1 || exit 91
    "$W/scripts/psym_compare.sh" capture "$OUT/after-$CFG" "$BD" > "$OUT/capture-after-$CFG.log" 2>&1
    echo "capture after $CFG rc=$?"
    ninja -C "$BD" -t deps > "$OUT/deps-after-$CFG.txt" 2>/dev/null
    trap restore EXIT
    git -C "$W" checkout -q --detach "$BASE" || exit 92
    cmake -S "$W" -B "$BD" > "$OUT/configure-before-$CFG.log" 2>&1 || { restore; exit 91; }
    ninja -C "$BD" -t cleandead > "$OUT/cleandead-$CFG.log" 2>&1 || { restore; exit 91; }
    locked "psym-before-$CFG" env CCACHE_DISABLE=1 cmake --build "$BD" -j14 \
        > "$OUT/build-before-$CFG.log" 2>&1 || { restore; exit 91; }
    "$W/scripts/psym_compare.sh" capture "$OUT/before-$CFG" "$BD" > "$OUT/capture-before-$CFG.log" 2>&1
    echo "capture before $CFG rc=$?"
    restore
    trap - EXIT
    "$W/scripts/psym_compare.sh" compare "$OUT/before-$CFG" "$OUT/after-$CFG" \
        > "$OUT/compare-$CFG.txt" 2>&1
    echo "compare $CFG rc=$? (exit 1 is EXPECTED for a commit adding a public options field; the classification is the gate)"
done
python3 -B "$S/psym_classify.py" "$OUT" "$BASE" "$HEAD_SHA" > "$OUT/classification.txt" 2>&1
echo "classify rc=$?"
tail -12 "$OUT/classification.txt"
git -C "$W" rev-parse HEAD
echo "PSYM_DONE $(date -Is)"
