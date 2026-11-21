#!/usr/bin/env bash
# AP-2 B6: compile-flags diff, library TUs, the B6 base (1c94174e, B5 landed: build-b6/armA, configured
# by armA_build.sh with build.sh's flags from the detached source tree build-b6/armA-src) vs the B6
# head (build-b6/rel). flags_diff.py is B5's (B3's), byte-for-byte. As in B5, arm A's compile_commands.json
# is first rewritten so its two path prefixes (source tree build-b6/armA-src, build tree build-b6/armA) read
# as the head's (the worktree root, build-b6/rel). Pure path spelling: no other token is touched.
# usage: flags.sh <evidence-dir>   (reads two existing compile databases; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a79f97c30e3369bfe
EV=${1:?evidence dir}
mkdir -p "$EV"
TMP=$WT/build-b6/armA-compile_commands.rewritten.json
python3 - "$WT" "$TMP" <<'PY'
import sys
wt, out = sys.argv[1], sys.argv[2]
s = open(wt + '/build-b6/armA/compile_commands.json').read()
s = s.replace(wt + '/build-b6/armA-src', wt).replace(wt + '/build-b6/armA', wt + '/build-b6/rel')
open(out, 'w').write(s)
PY
python3 "$WT/docs/notes/data/2026-11-m8-ap2-b6/legs/flags_diff.py" "$TMP" \
  "$WT/build-b6/rel/compile_commands.json" > "$EV/flags-diff.txt" 2>&1
rc=$?
tail -8 "$EV/flags-diff.txt"
echo "rc=$rc"
exit $rc
