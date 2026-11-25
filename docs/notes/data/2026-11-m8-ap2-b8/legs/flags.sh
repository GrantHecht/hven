#!/usr/bin/env bash
# AP-2 B8: compile-flags diff, library TUs, arm A = fa727270 (this worktree's own Release build at build-b8/psym, built before any B8 edit; was: B7's fresh Release
# build, its compile database copied by armA.sh to build-b8/armA-compile_commands.json) vs the rider head
# (build-b8/rel). flags_diff.py is B7's (B5's, B3's), byte-for-byte. Arm A's database is first rewritten so
# its build-tree prefix (B7's build-b7e/rel) reads as the head's (build-b8/rel); the source tree is this
# worktree in both. Pure path spelling: no other token is touched.
# usage: flags.sh <evidence-dir>   (reads two existing compile databases; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4
EV=${1:?evidence dir}
mkdir -p "$EV"
TMP=$WT/build-b8/armA-compile_commands.rewritten.json
python3 - "$WT" "$TMP" <<'PY'
import sys
wt, out = sys.argv[1], sys.argv[2]
s = open(wt + '/build-b8/armA-compile_commands.json').read()
s = s.replace(wt + '/build-b8/' + 'psym', wt + '/build-b8/rel')
open(out, 'w').write(s)
PY
python3 "$WT/docs/notes/data/2026-11-m8-ap2-b8/legs/flags_diff.py" "$TMP" \
  "$WT/build-b8/rel/compile_commands.json" > "$EV/flags-diff.txt" 2>&1
rc=$?
tail -8 "$EV/flags-diff.txt"
echo "flags_diff rc=$rc (B8: 1 expected, the D7 define)"
python3 "$WT/docs/notes/data/2026-11-m8-ap2-b8/legs/flags_check.py" "$EV/flags-diff.txt" | tee "$EV/flags-check.txt"
rc=${PIPESTATUS[0]}
echo "rc=$rc"
exit $rc
