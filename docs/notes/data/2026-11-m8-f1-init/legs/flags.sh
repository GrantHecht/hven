#!/usr/bin/env bash
# M8 F1 (copied from PF-1, re-pointed: worktree f1, build-f1, this directory, arm A 2340b95d). Was: M8 PF-1, copied from PF-HB (which copied the B8 riders) and re-pointed (worktree, build-f1, this directory); was: AP-2 B8 riders: compile-flags diff, library TUs, arm A = cf164dcf (legs/prebuild.sh's Release build of the
# `git archive` export build-f1/armA-src at build-f1/armA, its compile database copied to
# build-f1/armA-compile_commands.json) vs the rider head (build-f1/rel). flags_diff.py is B7's (B5's,
# B3's), byte-for-byte. Arm A's database is first rewritten so its build-tree prefix (build-f1/armA) reads
# as the head's (build-f1/rel) and its source tree (build-f1/armA-src) as this worktree. Pure path
# spelling: no other token is touched.
# usage: flags.sh <evidence-dir>   (reads two existing compile databases; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/f1
EV=${1:?evidence dir}
mkdir -p "$EV"
TMP=$WT/build-f1/armA-compile_commands.rewritten.json
python3 - "$WT" "$TMP" <<'PY'
import sys
wt, out = sys.argv[1], sys.argv[2]
s = open(wt + '/build-f1/armA-compile_commands.json').read()
s = s.replace(wt + '/build-f1/armA' + '-src', wt).replace(wt + '/build-f1/arm' + 'A/', wt + '/build-f1/rel/')
s = s.replace(wt + '/build-f1/arm' + 'A"', wt + '/build-f1/rel"')
open(out, 'w').write(s)
PY
python3 "$WT/docs/notes/data/2026-11-m8-f1-init/legs/flags_diff.py" "$TMP" \
  "$WT/build-f1/rel/compile_commands.json" > "$EV/flags-diff.txt" 2>&1
rc=$?
tail -8 "$EV/flags-diff.txt"
echo "flags_diff rc=$rc (PF-1: no flag difference expected)"
python3 "$WT/docs/notes/data/2026-11-m8-f1-init/legs/flags_check.py" "$EV/flags-diff.txt" | tee "$EV/flags-check.txt"
rc=${PIPESTATUS[0]}
echo "rc=$rc"
exit $rc
