#!/usr/bin/env bash
# M8 PF-HB, copied from the B8 riders and re-pointed (worktree, build-pfe, this directory); was: AP-2 B8 riders: arm A = cf164dcf (origin/m8: B7 landed + the C241 test-only fix), built by legs/prebuild.sh
# BEFORE ANY RIDER EDIT from its `git archive` export build-pfe/armA-src into build-pfe/armA (Release, the
# flags of legs/build.sh, CCACHE_DISABLE=1). Its corpus binary and compile database were copied to
# build-pfe/armAbin/hven_sqp_corpus and build-pfe/armA-compile_commands.json at that time. This leg only
# CHECKS them against the sha256 values in DECLARATION.txt (fail closed).
# Run as: flock /tmp/box-build.lock bash armA.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ab44eb20498de1642
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
want_bin=c9bef60132ffc9d3bcb9d81c1efd8c9e1a17e10ab5cf38bc225d8df01e085cf3
want_db=29822ff68d56b988f561d7fd9ffbacdd5ee1095153f021ec6ada027879646920
got_bin=$(sha256sum "$WT/build-pfe/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
got_db=$(sha256sum "$WT/build-pfe/armA-compile_commands.json" | cut -d' ' -f1)
echo "arm A corpus  $got_bin"; echo "arm A db      $got_db"
rc=0
[ "$got_bin" = "$want_bin" ] || { echo "ARM_A_BINARY_SHA_MISMATCH"; rc=4; }
[ "$got_db" = "$want_db" ] || { echo "ARM_A_DB_SHA_MISMATCH"; rc=4; }
grep -c '"file"' "$WT/build-pfe/armA-compile_commands.json"
[ $rc = 0 ] && echo ARM_A_OK
echo "FOREGROUND_END $(date -Is)"
exit $rc
