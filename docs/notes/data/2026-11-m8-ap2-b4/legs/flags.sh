#!/usr/bin/env bash
# AP-2 B4: compile-flags diff, library TUs, base (f1117a86: build-b4/armA, configured by armA_build.sh with
# build.sh's flags from the detached source tree build-b4/armA-src) vs the B4 head (build-b4/rel).
# flags_diff.py is B3's, byte-for-byte. The ONE change of instrument: arm A's compile_commands.json is
# first rewritten so its two path prefixes (source tree build-b4/armA-src, build tree build-b4/armA) read
# as the head's (the worktree root, build-b4/rel); B3 compared two builds at one path and needed none.
# Pure path spelling: no other token is touched.
# usage: flags.sh <evidence-dir>   (reads two existing compile databases; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ac366926f941a970a
EV=${1:?evidence dir}
mkdir -p "$EV"
TMP=$WT/build-b4/armA-compile_commands.rewritten.json
python3 - "$WT" "$TMP" <<'PY'
import sys
wt, out = sys.argv[1], sys.argv[2]
s = open(wt + '/build-b4/armA/compile_commands.json').read()
s = s.replace(wt + '/build-b4/armA-src', wt).replace(wt + '/build-b4/armA', wt + '/build-b4/rel')
open(out, 'w').write(s)
PY
python3 "$WT/docs/notes/data/2026-11-m8-ap2-b4/legs/flags_diff.py" "$TMP" \
  "$WT/build-b4/rel/compile_commands.json" > "$EV/flags-diff.txt" 2>&1
rc=$?
tail -8 "$EV/flags-diff.txt"
echo "rc=$rc"
exit $rc
