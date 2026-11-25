#!/usr/bin/env bash
# AP-2 B8 riders: compile-flags diff, library TUs, arm A = cf164dcf (legs/prebuild.sh's Release build of the
# `git archive` export build-b8r/armA-src at build-b8r/armA, its compile database copied to
# build-b8r/armA-compile_commands.json) vs the rider head (build-b8r/rel). flags_diff.py is B7's (B5's,
# B3's), byte-for-byte. Arm A's database is first rewritten so its build-tree prefix (build-b8r/armA) reads
# as the head's (build-b8r/rel) and its source tree (build-b8r/armA-src) as this worktree. Pure path
# spelling: no other token is touched.
# usage: flags.sh <evidence-dir>   (reads two existing compile databases; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4
EV=${1:?evidence dir}
mkdir -p "$EV"
TMP=$WT/build-b8r/armA-compile_commands.rewritten.json
python3 - "$WT" "$TMP" <<'PY'
import sys
wt, out = sys.argv[1], sys.argv[2]
s = open(wt + '/build-b8r/armA-compile_commands.json').read()
s = s.replace(wt + '/build-b8r/armA' + '-src', wt).replace(wt + '/build-b8r/arm' + 'A/', wt + '/build-b8r/rel/')
s = s.replace(wt + '/build-b8r/arm' + 'A"', wt + '/build-b8r/rel"')
open(out, 'w').write(s)
PY
python3 "$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/legs/flags_diff.py" "$TMP" \
  "$WT/build-b8r/rel/compile_commands.json" > "$EV/flags-diff.txt" 2>&1
rc=$?
tail -8 "$EV/flags-diff.txt"
echo "flags_diff rc=$rc (riders: 1 expected, the two declared defines)"
python3 "$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/legs/flags_check.py" "$EV/flags-diff.txt" | tee "$EV/flags-check.txt"
rc=${PIPESTATUS[0]}
echo "rc=$rc"
exit $rc
