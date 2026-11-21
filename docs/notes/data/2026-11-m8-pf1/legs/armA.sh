#!/usr/bin/env bash
# M8 PF-1, copied from PF-HB (which copied the B8 riders) and re-pointed (worktree, build-pf1e, this directory): arm A = bdd797e3 (origin/m8), built by legs/prebuild.sh
# BEFORE THE DECLARATION from its `git archive` export build-pf1e/armA-src into build-pf1e/armA (Release, the
# flags of legs/build.sh, CCACHE_DISABLE=1). Its corpus binary and compile database were copied to
# build-pf1e/armAbin/hven_sqp_corpus and build-pf1e/armA-compile_commands.json at that time. This leg only
# CHECKS them against the sha256 values in DECLARATION.txt (fail closed).
# Run as: flock /tmp/box-build.lock bash armA.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa2c5c3a798557d0e
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
want_bin=cb07803891f17091a8bbe1de280d947c99d59a16e05bfbefd49d54d1a014166b
want_db=e92f61555fe95245978369ea15a8f64d41c1a72ce89b898e78f9d6e250046d57
got_bin=$(sha256sum "$WT/build-pf1e/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
got_db=$(sha256sum "$WT/build-pf1e/armA-compile_commands.json" | cut -d' ' -f1)
echo "arm A corpus  $got_bin"; echo "arm A db      $got_db"
rc=0
[ "$got_bin" = "$want_bin" ] || { echo "ARM_A_BINARY_SHA_MISMATCH"; rc=4; }
[ "$got_db" = "$want_db" ] || { echo "ARM_A_DB_SHA_MISMATCH"; rc=4; }
grep -c '"file"' "$WT/build-pf1e/armA-compile_commands.json"
[ $rc = 0 ] && echo ARM_A_OK
echo "FOREGROUND_END $(date -Is)"
exit $rc
