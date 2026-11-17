# M8 AP-5 G-RB rider r4 -- DECLARATION-r5 (mutation leg re-run only; committed before the leg; DECLARATION-r4.md stays frozen)

## Why
The r4 chain ran every leg and passed ctest Release/Debug, replay, interior and install smoke (r4/suites, replay, interior, install-smoke). Its mutation verdict printed FAIL for an INSTRUMENT fault of mine, not a
test finding: x1's description in legs/mutations.sh contained a comma ("equality residual alone, <= ties"), which split the CSV row, so mutation_verdict.py read the wrong column ("INSTRUMENT FAULT"). The raw row
shows x1 built and was KILLED (Callback.IpmTerminalEventDescribesTheReturnedPointUnderReturnBest, drivers rc=1); o1 o2 o3 killed; the control failed nothing. The r4 mutation outputs stay under r4/mutation/ as the first record.

## What changes
Only the x1 description text (comma removed). No source, test, binary or other leg changes: the code head is the same 683897c48e28250a1ecee7a89dd2c662a243b7be (SOURCE unchanged, checked by git diff against it before the
leg); the other legs' evidence from r4 stands and is not re-run.

## Leg
`r4/legs/mutations.sh 683897c48e28250a1ecee7a89dd2c662a243b7be r4/mutation-r5` then `mutation_verdict.py` (same must-kill set: o1 o2 o3 x1; the control fails nothing), under the box protocol. Pass = MUTATION_VERDICT PASS.
Raw dumps under the ignored .scratch/r4-mutation-r5. ON FAIL: stop and report.
