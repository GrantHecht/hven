#!/bin/bash
# I4's G6-style leg: install smoke, export contract, clang-format on the range's C++ files, diff check.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-ad5e6e56c9da5d8a5
S=$W/.scratch/a1i
cd "$W" || exit 2
if pgrep -a hven_bench_cell; then echo PGREP_NONEMPTY; else echo PGREP_EMPTY; fi
echo FOREGROUND_START "$(date -Is)"
flock /tmp/box-build.lock bash -c "
set -o pipefail
rm -rf $S/smoke-work
HVEN_INSTALL_SMOKE_WORKDIR=$S/smoke-work bash scripts/check_install_smoke.sh > $S/logs/install-smoke.log 2>&1
echo INSTALL_SMOKE_RC=\$?; tail -3 $S/logs/install-smoke.log
ALIGN=\$(grep -o 'EIGEN_MAX_ALIGN_BYTES resolved to [0-9]*' $S/smoke-work/hven-configure.log | tail -1 | awk '{print \$NF}')
echo ALIGN=\$ALIGN
bash scripts/check_export_contract.sh $S/smoke-work/hven-install/lib64/cmake/hven/hvenTargets.cmake linux \$ALIGN > $S/logs/export-contract.log 2>&1
echo EXPORT_CONTRACT_RC=\$?; tail -2 $S/logs/export-contract.log
"
rc=$?
echo FOREGROUND_END rc=$rc "$(date -Is)"
CHANGED=$(git diff --name-only f7e5324c -- '*.cpp' '*.h')
clang-format --dry-run -Werror $CHANGED
echo "CLANG_FORMAT_RC=$? files=$(echo "$CHANGED" | wc -l)"
git diff --check f7e5324c
echo "DIFF_CHECK_RC=$?"
