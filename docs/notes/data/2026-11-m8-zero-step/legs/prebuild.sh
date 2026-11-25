#!/usr/bin/env bash
# Not a leg. Arm A = origin/m8 (export of ARMA, Release, hven_sqp_corpus); arm B = the code head (Release rel, Debug dbg,
# all targets). Corpus binaries frozen to build-zs/armAbin, build-zs/armBbin. usage: prebuild.sh <ARMA sha> <HEAD sha>
# Run as: flock /tmp/box-build.lock bash prebuild.sh ...
set -o pipefail
. "$(dirname "$0")/env.sh"
L=$E/legs
ARMA=${1:?arm A commit}; HEADSHA=${2:?code head}
pgrep_guard
echo "FOREGROUND_START $(date -Is)"
[ "$(git -C "$WT" rev-parse HEAD)" = "$HEADSHA" ] || { echo HEAD_NOT_CODE_HEAD; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no)" ] || { echo TREE_DIRTY; exit 2; }
mkdir -p "$B" "$TMPDIR"
rm -rf "$B/armA-src" "$B/armA" "$B/armAbin"
mkdir -p "$B/armA-src"
git -C "$WT" archive "$ARMA" | tar -x -C "$B/armA-src" || { echo ARCHIVE_FAIL; exit 1; }
MODS=/home/ghecht/Projects/hven/.git/modules/dep
[ "$(git -C "$WT" rev-parse "$ARMA:dep/eigen")" = "$(cat "$MODS/eigen/HEAD")" ] || { echo EIGEN_PIN_MISMATCH; exit 1; }
[ "$(git -C "$WT" rev-parse "$ARMA:dep/fmt")" = "$(cat "$MODS/fmt/HEAD")" ] || { echo FMT_PIN_MISMATCH; exit 1; }
rm -rf "$B/armA-src/dep/eigen" "$B/armA-src/dep/fmt"
cp -a "$WT/dep/eigen" "$B/armA-src/dep/eigen" && cp -a "$WT/dep/fmt" "$B/armA-src/dep/fmt" || exit 1
SRC=$B/armA-src bash "$L/build.sh" Release armA hven_sqp_corpus || { echo ARMA_BUILD_FAIL; exit 1; }
mkdir -p "$B/armAbin"; cp -p "$B/armA/bench/hven_sqp_corpus" "$B/armAbin/hven_sqp_corpus" || exit 1
bash "$L/build.sh" Release rel || exit 1
bash "$L/build.sh" Debug dbg || exit 1
mkdir -p "$B/armBbin"; cp -p "$B/rel/bench/hven_sqp_corpus" "$B/armBbin/hven_sqp_corpus" || exit 1
sha256sum "$B/armAbin/hven_sqp_corpus" "$B/armBbin/hven_sqp_corpus"
echo "FOREGROUND_END $(date -Is)"
