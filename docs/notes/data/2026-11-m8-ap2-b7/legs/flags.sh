#!/usr/bin/env bash
# AP-2 B7: compile-flags diff, library TUs, arm A = the B7 base (31c73879: build-b7e/armA, configured
# by armA_config.sh with build.sh's flags from the detached source tree build-b7e/armA-src) vs the B7
# head (build-b7e/rel). flags_diff.py is B5's (B3's), byte-for-byte. As in B5, arm A's compile_commands.json
# is first rewritten so its two path prefixes (source tree build-b7e/armA-src, build tree build-b7e/armA) read
# as the head's (the worktree root, build-b7e/rel). Pure path spelling: no other token is touched.
# usage: flags.sh <evidence-dir>   (reads two existing compile databases; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a00603e2048c1c37b
EV=${1:?evidence dir}
mkdir -p "$EV"
TMP=$WT/build-b7e/armA-compile_commands.rewritten.json
python3 - "$WT" "$TMP" <<'PY'
import sys
wt, out = sys.argv[1], sys.argv[2]
s = open(wt + '/build-b7e/armA/compile_commands.json').read()
s = s.replace(wt + '/build-b7e/armA-src', wt).replace(wt + '/build-b7e/armA', wt + '/build-b7e/rel')
open(out, 'w').write(s)
PY
python3 "$WT/docs/notes/data/2026-11-m8-ap2-b7/legs/flags_diff.py" "$TMP" \
  "$WT/build-b7e/rel/compile_commands.json" > "$EV/flags-diff.txt" 2>&1
rc=$?
tail -8 "$EV/flags-diff.txt"
echo "rc=$rc"
exit $rc
