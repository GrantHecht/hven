#!/usr/bin/env bash
# AP-2 B7 riders: arm A = the B7 code head 8089104c, taken from B7's own frozen evidence rather than rebuilt:
#   * the corpus binary B7 froze as ITS arm B (build-b7e/armBbin/hven_sqp_corpus, sha256
#     d9d01dfaecb52b1705962a4c70649b4509827a488d7d3cc40c21a453187dd16c, B7's PROVENANCE.txt), copied to
#     build-b7r/armAbin/ and checked against that sha256 (fail closed);
#   * the compile database of B7's fresh Release build of 8089104c (build-b7e/rel/compile_commands.json,
#     configured by B7's legs/build.sh with the same flags as this directory's), copied to
#     build-b7r/armA-compile_commands.json for the flags diff (flags.sh).
# Both are read, never written, in build-b7e/. Run as: flock /tmp/box-build.lock bash armA.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a00603e2048c1c37b
SRC_BIN=$WT/build-b7e/armBbin/hven_sqp_corpus
SRC_DB=$WT/build-b7e/rel/compile_commands.json
EXPECT=d9d01dfaecb52b1705962a4c70649b4509827a488d7d3cc40c21a453187dd16c
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
mkdir -p "$WT/build-b7r/armAbin"
GOT=$(sha256sum "$SRC_BIN" | cut -d' ' -f1)
[ "$GOT" = "$EXPECT" ] || { echo "ARM_A_SOURCE_SHA_MISMATCH got=$GOT"; exit 4; }
cp -p "$SRC_BIN" "$WT/build-b7r/armAbin/hven_sqp_corpus" || exit 1
cp -p "$SRC_DB" "$WT/build-b7r/armA-compile_commands.json" || exit 1
sha256sum "$WT/build-b7r/armAbin/hven_sqp_corpus" "$WT/build-b7r/armA-compile_commands.json"
grep -c '"file"' "$WT/build-b7r/armA-compile_commands.json"
echo "FOREGROUND_END $(date -Is)"
