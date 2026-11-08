#!/usr/bin/env bash
# M8 DN-1 impl: copied from R11 (2026-11-m8-r11-block-decode) and re-pointed: worktree dn1, build-dn1e, this directory, arm A 02065dac.
# M8 R11: copied from F1 (2026-11-m8-f1-init; runtime from its runtime-r2, period 400000; census legs from PF-1) and re-pointed: worktree r11, build-r11e, this directory, arm A a8c540cf.
# M8 F1 (copied from PF-1, re-pointed: worktree f1, build-f1, this directory, arm A 2340b95d). Was: M8 PF-1, copied from PF-HB (which copied the B8 riders) and re-pointed (worktree, build-f1, this directory); was: AP-2 B8 riders: compile-flags diff, library TUs, arm A = cf164dcf (legs/prebuild.sh's Release build of the
# `git archive` export build-r11e/armA-src at build-r11e/armA, its compile database copied to
# build-r11e/armA-compile_commands.json) vs the rider head (build-r11e/rel). flags_diff.py is B7's (B5's,
# B3's), byte-for-byte. Arm A's database is first rewritten so its build-tree prefix (build-r11e/armA) reads
# as the head's (build-r11e/rel) and its source tree (build-r11e/armA-src) as this worktree. Pure path
# spelling: no other token is touched.
# usage: flags.sh <evidence-dir>   (reads two existing compile databases; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
EV=${1:?evidence dir}
mkdir -p "$EV"
TMP=$WT/build-dn1e/armA-compile_commands.rewritten.json
python3 - "$WT" "$TMP" <<'PY'
import sys
wt, out = sys.argv[1], sys.argv[2]
s = open(wt + '/build-dn1e/armA-compile_commands.json').read()
s = s.replace(wt + '/build-dn1e/armA' + '-src', wt).replace(wt + '/build-dn1e/arm' + 'A/', wt + '/build-dn1e/rel/')
s = s.replace(wt + '/build-dn1e/arm' + 'A"', wt + '/build-dn1e/rel"')
open(out, 'w').write(s)
PY
python3 "$WT/docs/notes/data/2026-11-m8-dn1-impl/legs/flags_diff.py" "$TMP" \
  "$WT/build-dn1e/rel/compile_commands.json" > "$EV/flags-diff.txt" 2>&1
rc=$?
tail -8 "$EV/flags-diff.txt"
echo "flags_diff rc=$rc (DN-1: no flag difference; one added TU)"
python3 "$WT/docs/notes/data/2026-11-m8-dn1-impl/legs/flags_check.py" "$EV/flags-diff.txt" | tee "$EV/flags-check.txt"
rc=${PIPESTATUS[0]}
echo "rc=$rc"
exit $rc
