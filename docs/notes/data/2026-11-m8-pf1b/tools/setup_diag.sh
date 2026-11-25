#!/usr/bin/env bash
# PF-1b scratch: diag source = git archive of the worktree HEAD + deps + scratch patches (never committed).
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf1b
P=/tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/pf1b
SP=/tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad
D=$WT/build-pf1b/diag-src
rm -rf "$D"; mkdir -p "$D"
cd "$WT" && git archive HEAD | tar -x -C "$D" || exit 1
rm -rf "$D/dep/eigen" "$D/dep/fmt"; cp -a dep/eigen "$D/dep/eigen" && cp -a dep/fmt "$D/dep/fmt" || exit 1
python3 "$SP/patch_dump.py" "$D" || exit 1
python3 "$P/patch_pf1b.py" "$D" || exit 1
cp "$P/pf1b_replay.cpp" "$D/tests/qp/pf1b_replay.cpp"
cat "$P/pf1b_target.cmake" >> "$D/tests/CMakeLists.txt"
echo SETUP_OK
