#!/usr/bin/env bash
# AP-2 B8 riders: arm A = cf164dcf (origin/m8: B7 landed + the C241 test-only fix), built by legs/prebuild.sh
# BEFORE ANY RIDER EDIT from its `git archive` export build-b8r/armA-src into build-b8r/armA (Release, the
# flags of legs/build.sh, CCACHE_DISABLE=1). Its corpus binary and compile database were copied to
# build-b8r/armAbin/hven_sqp_corpus and build-b8r/armA-compile_commands.json at that time. This leg only
# CHECKS them against the sha256 values in DECLARATION.txt (fail closed).
# Run as: flock /tmp/box-build.lock bash armA.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
want_bin=d2c9a99f14396554fc8753f102a316004f731b4333d55d9fc12ff5b3aeb90515
want_db=5e3264a519b8d4d55ba00657e003c1a3f612f6b3b68469300deed3ce4915af4a
got_bin=$(sha256sum "$WT/build-b8r/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
got_db=$(sha256sum "$WT/build-b8r/armA-compile_commands.json" | cut -d' ' -f1)
echo "arm A corpus  $got_bin"; echo "arm A db      $got_db"
rc=0
[ "$got_bin" = "$want_bin" ] || { echo "ARM_A_BINARY_SHA_MISMATCH"; rc=4; }
[ "$got_db" = "$want_db" ] || { echo "ARM_A_DB_SHA_MISMATCH"; rc=4; }
grep -c '"file"' "$WT/build-b8r/armA-compile_commands.json"
[ $rc = 0 ] && echo ARM_A_OK
echo "FOREGROUND_END $(date -Is)"
exit $rc
