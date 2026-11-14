# M8 DN-3 -- measured dense auto-selection (DECLARATION-r3)

Successor of `DECLARATION-r2.md` (commit `bfaa9b033`), which stays frozen beside this file with every r2 output.
Committed before r3's first leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is
answered by `DECLARATION-r4.md`.

## 0. What happened under r2 (the record, kept)

- **Chain PASSED** (`chain-r2/chain.out`, 22:51-23:32): P1 "100% tests passed, 0 tests failed out of 3632" in Release and
  Debug (`suites-r2/`); P2 walk / ssn / ipm 27 cells x 75 columns, 0 differences each (`replay-r2/`); P3 interior unmoved
  (43 cells x 30 columns) and promoted, 0 differences each (`interior-r2/`); P4 FAILPROOF_AS_DECLARED, head 56 pass, the
  ten mutations each failing exactly its WANT set (`failproof-r2/`); P6 install smoke and export contract rc 0 on both
  front ends (`install-smoke-r2/`). r1's P5 (PASS) stands.
- **E PASSED** (`enum/`, 23:32-23:37): L1 THRESHOLD_AS_DECLARED (`results/threshold.txt`); ENUM_PASS, E1-E3 none; 227
  candidates, `enum/candidates.tsv` byte-identical to the dev probe's (r1 §6's expectation).
- **S completed** (`stat/`, 23:37-23:57): 238 cells x 6 rounds, every `.stat` carries instructions.
- **P stopped the orchestration with rc 5** (`legs-logs-r2/P.log`; it waited 00:00-01:03 on the box lock held by another
  lane's serial run, then ran 01:03-02:15): all 454 (candidate, arm) records were taken and attributed, and the
  instrument flagged two SAMPLING_DEFECTs, each a single lost event in one repeat:
  - `sd-hven-ipm-SPANHYD` arm N, repeat k=9 of 31 (period 425209): LOST=1 LOST_SAMPLES=1 THROTTLE=0;
  - `sd-hven-sqp-HS92` arm N, repeat k=2 of 6 (period 405609): LOST=1 LOST_SAMPLES=1 THROTTLE=0.
  No THROTTLE anywhere. T, G, the verdict and the restore did not run. No verdict has been computed or read.

## 1. What r3 does (nothing else)

- r0 S-A2's remedy (a sampling defect voids that cell's record, which is re-run once alone; a second defect is a STOP)
  is applied to these two LOST defects, exactly as r0 states it for a throttle: the two (cell, arm) records -- their
  `.runs` and `.attr.json` -- are moved to `record/void-r2/` (kept), and `sl/adopt_run.sh P` re-takes exactly those two
  (it skips every (cell, arm) whose attribution exists), alone on the box under the lock. Every other record stands.
- A defect in either re-taken record is a STOP (S-A2: second defect).
- Then T, G, `sl/adopt_verdict.py` -> `results/verdict.txt`, and `sl/hb_restore.sh`, as r1 §10.
- Instrument: `legs/orchestrate-r3.sh` (r2's orchestrator with the chain and E/S removed, the void step added; TMPDIR
  as r2). Outputs: `legs-logs-r3/`.

Everything else is r1's as amended by r2, unchanged: the code head `96ac1a27d`, every binary and sha256, criteria E and A,
the R-items, the flip clause, the NOT RUN list, the disclosures and the STOP table.

## 2. Order

```
bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3/legs/orchestrate-r3.sh
```

ON FAIL: stop, commit the evidence, report; no re-run, no criterion change after data.
