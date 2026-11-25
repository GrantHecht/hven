#!/usr/bin/env bash
# AP-2 B7 riders: compile-flags diff, library TUs, arm A = the B7 code head 8089104c (B7's fresh Release
# build, its compile database copied by armA.sh to build-b7r/armA-compile_commands.json) vs the rider head
# (build-b7r/rel). flags_diff.py is B7's (B5's, B3's), byte-for-byte. Arm A's database is first rewritten so
# its build-tree prefix (B7's build-b7e/rel) reads as the head's (build-b7r/rel); the source tree is this
# worktree in both. Pure path spelling: no other token is touched.
# usage: flags.sh <evidence-dir>   (reads two existing compile databases; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a00603e2048c1c37b
EV=${1:?evidence dir}
mkdir -p "$EV"
TMP=$WT/build-b7r/armA-compile_commands.rewritten.json
python3 - "$WT" "$TMP" <<'PY'
import sys
wt, out = sys.argv[1], sys.argv[2]
s = open(wt + '/build-b7r/armA-compile_commands.json').read()
s = s.replace(wt + '/build-b7' + 'e/rel', wt + '/build-b7r/rel')
open(out, 'w').write(s)
PY
python3 "$WT/docs/notes/data/2026-11-m8-ap2-b7-riders/legs/flags_diff.py" "$TMP" \
  "$WT/build-b7r/rel/compile_commands.json" > "$EV/flags-diff.txt" 2>&1
rc=$?
tail -8 "$EV/flags-diff.txt"
echo "rc=$rc"
exit $rc
