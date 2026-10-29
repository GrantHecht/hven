#!/bin/bash
# A3 final gates at the committed head: rebuild Release + Debug, the three suite legs, the mutation
# run, then P-SYM (base 534bc87f vs head, same build path, both configs). Each its own lock holds.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-afecbe0a5611f9eff
S=$W/.scratch/a3
echo "CHAIN_FINAL_START $(date -Is) HEAD $(git -C "$W" rev-parse HEAD) DIRTY=$(git -C "$W" status --porcelain --untracked-files=no | wc -l)"
fail=0
bash "$S/build.sh" head rel || fail=1
bash "$S/build.sh" head dbg || fail=1
if [ "$fail" = 0 ]; then
    for s in rel dbg long; do
        bash "$S/suite.sh" "$s" || fail=1
        cp "$S/logs/suite-$s.log" "$S/logs/final-suite-$s.log"
        grep -E 'tests passed|tests failed' "$S/logs/suite-$s.log"
    done
fi
echo "FINAL_SUITES_DONE fail=$fail $(date -Is)"
python3 -u "$S/mutants.py" > "$S/logs/mutants.txt" 2>&1
echo "MUTANTS_DONE rc=$? $(date -Is)"
tail -14 "$S/logs/mutants.txt"
bash "$S/psym.sh" > "$S/logs/psym.txt" 2>&1
echo "PSYM_DONE rc=$? $(date -Is)"
tail -14 "$S/logs/psym.txt"
echo "CHAIN_FINAL_DONE fail=$fail HEAD $(git -C "$W" rev-parse HEAD) $(date -Is)"
