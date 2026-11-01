#!/bin/bash
# Second mutation pass: the unmutated suite of the reordered F-4 test, then the survivors and the two
# after-walk gate mutants.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a05b3c395d8efca12
S=$W/.scratch/a7
cd "$W" || exit 2
L=$S/logs/chain-mut3.txt
echo "CHAIN_MUT3 START $(date -Is) pid=$$" > "$L"
bash "$S/build.sh" rel hven_sqp_tests >> "$L" 2>&1
bash "$S/leg.sh" unmutated3 "$S/build-rel/tests/sqp/hven_sqp_tests" --gtest_filter=WsAlgebraRule* >> "$L" 2>&1
echo "UNMUTATED rc=$?" >> "$L"
python3 "$S/mutants.py" M6c M15c >> "$L" 2>&1
echo "CHAIN_MUT3 END $(date -Is)" >> "$L"
