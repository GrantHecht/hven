#!/usr/bin/env bash
# M8 PF-1, copied from PF-HB (which copied the B8 riders) and re-pointed (worktree, build-pf1e, this directory); was: AP-2 B8 riders: compile-flags diff, library TUs, arm A = cf164dcf (legs/prebuild.sh's Release build of the
# `git archive` export build-pf1e/armA-src at build-pf1e/armA, its compile database copied to
# build-pf1e/armA-compile_commands.json) vs the rider head (build-pf1e/rel). flags_diff.py is B7's (B5's,
# B3's), byte-for-byte. Arm A's database is first rewritten so its build-tree prefix (build-pf1e/armA) reads
# as the head's (build-pf1e/rel) and its source tree (build-pf1e/armA-src) as this worktree. Pure path
# spelling: no other token is touched.
# usage: flags.sh <evidence-dir>   (reads two existing compile databases; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa2c5c3a798557d0e
EV=${1:?evidence dir}
mkdir -p "$EV"
TMP=$WT/build-pf1e/armA-compile_commands.rewritten.json
python3 - "$WT" "$TMP" <<'PY'
import sys
wt, out = sys.argv[1], sys.argv[2]
s = open(wt + '/build-pf1e/armA-compile_commands.json').read()
s = s.replace(wt + '/build-pf1e/armA' + '-src', wt).replace(wt + '/build-pf1e/arm' + 'A/', wt + '/build-pf1e/rel/')
s = s.replace(wt + '/build-pf1e/arm' + 'A"', wt + '/build-pf1e/rel"')
open(out, 'w').write(s)
PY
python3 "$WT/docs/notes/data/2026-11-m8-pf1/legs/flags_diff.py" "$TMP" \
  "$WT/build-pf1e/rel/compile_commands.json" > "$EV/flags-diff.txt" 2>&1
rc=$?
tail -8 "$EV/flags-diff.txt"
echo "flags_diff rc=$rc (PF-1: no flag difference expected)"
python3 "$WT/docs/notes/data/2026-11-m8-pf1/legs/flags_check.py" "$EV/flags-diff.txt" | tee "$EV/flags-check.txt"
rc=${PIPESTATUS[0]}
echo "rc=$rc"
exit $rc
