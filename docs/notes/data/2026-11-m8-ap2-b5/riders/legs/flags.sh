#!/usr/bin/env bash
# AP-2 B5 riders: compile-flags diff, library TUs, the B5 code head (2142af0a: build-b5/riA, configured
# by riA_configure.sh with build.sh's flags from the detached source tree build-b5/riA-src) vs the riders
# head (build-b5/rel). flags_diff.py is B5's (B3's), byte-for-byte. As in B5, arm A's compile_commands.json
# is first rewritten so its two path prefixes (source tree build-b5/riA-src, build tree build-b5/riA) read
# as the head's (the worktree root, build-b5/rel). Pure path spelling: no other token is touched.
# usage: flags.sh <evidence-dir>   (reads two existing compile databases; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a17ce5e51d9c90f96
EV=${1:?evidence dir}
mkdir -p "$EV"
TMP=$WT/build-b5/riA-compile_commands.rewritten.json
python3 - "$WT" "$TMP" <<'PY'
import sys
wt, out = sys.argv[1], sys.argv[2]
s = open(wt + '/build-b5/riA/compile_commands.json').read()
s = s.replace(wt + '/build-b5/riA-src', wt).replace(wt + '/build-b5/riA', wt + '/build-b5/rel')
open(out, 'w').write(s)
PY
python3 "$WT/docs/notes/data/2026-11-m8-ap2-b5/riders/legs/flags_diff.py" "$TMP" \
  "$WT/build-b5/rel/compile_commands.json" > "$EV/flags-diff.txt" 2>&1
rc=$?
tail -8 "$EV/flags-diff.txt"
echo "rc=$rc"
exit $rc
