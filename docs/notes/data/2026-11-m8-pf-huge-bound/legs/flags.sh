#!/usr/bin/env bash
# M8 PF-HB, copied from the B8 riders and re-pointed (worktree, build-pfe, this directory); was: AP-2 B8 riders: compile-flags diff, library TUs, arm A = cf164dcf (legs/prebuild.sh's Release build of the
# `git archive` export build-pfe/armA-src at build-pfe/armA, its compile database copied to
# build-pfe/armA-compile_commands.json) vs the rider head (build-pfe/rel). flags_diff.py is B7's (B5's,
# B3's), byte-for-byte. Arm A's database is first rewritten so its build-tree prefix (build-pfe/armA) reads
# as the head's (build-pfe/rel) and its source tree (build-pfe/armA-src) as this worktree. Pure path
# spelling: no other token is touched.
# usage: flags.sh <evidence-dir>   (reads two existing compile databases; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ab44eb20498de1642
EV=${1:?evidence dir}
mkdir -p "$EV"
TMP=$WT/build-pfe/armA-compile_commands.rewritten.json
python3 - "$WT" "$TMP" <<'PY'
import sys
wt, out = sys.argv[1], sys.argv[2]
s = open(wt + '/build-pfe/armA-compile_commands.json').read()
s = s.replace(wt + '/build-pfe/armA' + '-src', wt).replace(wt + '/build-pfe/arm' + 'A/', wt + '/build-pfe/rel/')
s = s.replace(wt + '/build-pfe/arm' + 'A"', wt + '/build-pfe/rel"')
open(out, 'w').write(s)
PY
python3 "$WT/docs/notes/data/2026-11-m8-pf-huge-bound/legs/flags_diff.py" "$TMP" \
  "$WT/build-pfe/rel/compile_commands.json" > "$EV/flags-diff.txt" 2>&1
rc=$?
tail -8 "$EV/flags-diff.txt"
echo "flags_diff rc=$rc (PF-HB: no flag difference expected)"
python3 "$WT/docs/notes/data/2026-11-m8-pf-huge-bound/legs/flags_check.py" "$EV/flags-diff.txt" | tee "$EV/flags-check.txt"
rc=${PIPESTATUS[0]}
echo "rc=$rc"
exit $rc
