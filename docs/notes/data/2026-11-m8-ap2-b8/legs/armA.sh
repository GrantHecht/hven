#!/usr/bin/env bash
# AP-2 B8: arm A = fa727270 (B7 landed), built by this worktree BEFORE ANY B8 EDIT into build-b8/psym
# (the P-SYM before-arm build: Release, the flags of legs/build.sh, CCACHE_DISABLE=1). Its corpus binary
# and compile database were copied to build-b8/armAbin/hven_sqp_corpus and build-b8/armA-compile_commands.json
# at that time. This leg only CHECKS them against the sha256 values in DECLARATION.txt (fail closed).
# Run as: flock /tmp/box-build.lock bash armA.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
want_bin=273e09c6c777493788316661ffc54a998b4e2531606061fdb438ff4f1b658e98
want_db=b9fd684dd75560162b9721d487556296fc6579f66991083fbf6e611f6da62258
got_bin=$(sha256sum "$WT/build-b8/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
got_db=$(sha256sum "$WT/build-b8/armA-compile_commands.json" | cut -d' ' -f1)
echo "arm A corpus  $got_bin"; echo "arm A db      $got_db"
rc=0
[ "$got_bin" = "$want_bin" ] || { echo "ARM_A_BINARY_SHA_MISMATCH"; rc=4; }
[ "$got_db" = "$want_db" ] || { echo "ARM_A_DB_SHA_MISMATCH"; rc=4; }
grep -c '"file"' "$WT/build-b8/armA-compile_commands.json"
[ $rc = 0 ] && echo ARM_A_OK
echo "FOREGROUND_END $(date -Is)"
exit $rc
