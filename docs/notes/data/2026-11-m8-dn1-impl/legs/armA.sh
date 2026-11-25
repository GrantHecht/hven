#!/usr/bin/env bash
# M8 DN-1 impl: copied from R11 (2026-11-m8-r11-block-decode) and re-pointed: worktree dn1, build-dn1e, this directory, arm A 02065dac.
# M8 R11: copied from F1 (2026-11-m8-f1-init; runtime from its runtime-r2, period 400000; census legs from PF-1) and re-pointed: worktree r11, build-r11e, this directory, arm A a8c540cf.
# M8 F1 (copied from PF-1, re-pointed: worktree f1, build-f1, this directory, arm A 2340b95d). Was: M8 PF-1, copied from PF-HB (which copied the B8 riders) and re-pointed (worktree, build-f1, this directory): arm A = bdd797e3 (origin/m8), built by legs/prebuild.sh
# BEFORE THE DECLARATION from its `git archive` export build-r11e/armA-src into build-r11e/armA (Release, the
# flags of legs/build.sh, CCACHE_DISABLE=1). Its corpus binary and compile database were copied to
# build-r11e/armAbin/hven_sqp_corpus and build-r11e/armA-compile_commands.json at that time. This leg only
# CHECKS them against the sha256 values in DECLARATION.txt (fail closed).
# Run as: flock /tmp/box-build.lock bash armA.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
want_bin=4ee3af3ba96395333eeb433a2f3ee1155cc01ceb443659075f16c0552741041c
want_db=d29b82f856b157addb7c4e26eb215a97998c86ae7979d80f3ad15a4e53875994
got_bin=$(sha256sum "$WT/build-dn1e/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
got_db=$(sha256sum "$WT/build-dn1e/armA-compile_commands.json" | cut -d' ' -f1)
echo "arm A corpus  $got_bin"; echo "arm A db      $got_db"
rc=0
[ "$got_bin" = "$want_bin" ] || { echo "ARM_A_BINARY_SHA_MISMATCH"; rc=4; }
[ "$got_db" = "$want_db" ] || { echo "ARM_A_DB_SHA_MISMATCH"; rc=4; }
grep -c '"file"' "$WT/build-dn1e/armA-compile_commands.json"
[ $rc = 0 ] && echo ARM_A_OK
echo "FOREGROUND_END $(date -Is)"
exit $rc
