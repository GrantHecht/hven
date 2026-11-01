#!/usr/bin/env bash
# M7 W2 M1 FIX ROUND 1 (review finding MEDIUM-1): the two U0 27-cell three-arm
# replay sets RE-TAKEN with the bench build directory CONFIGURED AT A CLEAN,
# COMMITTED TREE.
#
# WHY: `HVEN_SQP_CORPUS_GIT_DESCRIBE` is resolved at CONFIGURE time
# (bench/CMakeLists.txt:81-95). The first take reused a build directory that had
# been configured while the M1 edits were still uncommitted, so its CSVs carry a
# `-dirty` stamp about binaries that in fact ran at a clean tree. `rm -rf` +
# re-configure inside this wrapper is the CMake comment's own instruction.
#
# The replay itself is `replay.sh` unchanged in substance -- same invocation,
# same environment pin, same cell list, same comparison against the M6 close
# HEAD's committed CSVs -- with its output directory and its log tag taken from
# the environment so the two legs do not collide.
#
# Environment: RETAKE_SRC (tree to configure from), RETAKE_BUILD (build dir),
# RETAKE_OUT (CSV output dir), RETAKE_TAG (log tag).
set -uo pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2m1
SRC=${RETAKE_SRC:-$R}
BD=${RETAKE_BUILD:?RETAKE_BUILD is required}
OUT=${RETAKE_OUT:?RETAKE_OUT is required}
TAG=${RETAKE_TAG:?RETAKE_TAG is required}
mkdir -p "$S/logs"
echo "=== HEAD $(git -C "$SRC" rev-parse --short HEAD)  dirty-files=$(git -C "$SRC" status --porcelain | wc -l) ==="
echo "=== source tree:  $SRC"
echo "=== build dir:    $BD  (removed and re-configured by this wrapper)"
echo "=== csv out:      $OUT"
echo "=== git describe: $(git -C "$SRC/bench" describe --always --dirty --abbrev=12)  (what the configure will bake in)"
rm -rf "$BD"
cmake -S "$SRC" -B "$BD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
  > "$S/logs/retake-configure-$TAG.log" 2>&1
echo "configure rc=$?"
cmake --build "$BD" -j "$(nproc)" --target hven_sqp_corpus > "$S/logs/retake-build-$TAG.log" 2>&1
rc_build=$?
echo "build rc=$rc_build  (target hven_sqp_corpus)"
[ $rc_build -eq 0 ] || { grep -E "error:|FAILED" "$S/logs/retake-build-$TAG.log" | head -20; exit 1; }
echo "libhven.a sha256:       $(sha256sum "$BD/libhven.a" | cut -d' ' -f1)"
echo "hven_sqp_corpus sha256: $(sha256sum "$BD/bench/hven_sqp_corpus" | cut -d' ' -f1)"
echo "=== U0 27-CELL THREE-ARM REPLAY (expecting 27/75/0 x3) ==="
HVEN_M1_REPLAY_BIN="$BD/bench/hven_sqp_corpus" \
HVEN_M1_REPLAY_OUT="$OUT" \
HVEN_M1_REPLAY_TAG="$TAG" \
  "$S/replay.sh"
rc=$?
echo "RETAKE_REPLAY_RC=$rc"
echo "stamp written into the CSVs: $(sed -n '2p' "$OUT/head-walk.csv")"
exit $rc
