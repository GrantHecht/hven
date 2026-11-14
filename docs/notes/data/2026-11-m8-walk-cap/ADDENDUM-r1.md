# M8 walk cap -- ADDENDUM r1 (review riders, ruling C339), committed before its first leg

The DECLARATION and the leg files of this directory are untouched. The riders are one commit on the rebased head
(`2c50d71ae`, on 92e7c5d83 + the walk-cap commits + the rebase evidence): the ladder's in-loop shut test reads the
rung's rows as `closed` does; a zero-elastic-step exhaustion refused as a nested restoration counts a phase switch;
seam knob `off_rows_inloop_rungs` + one fixture; comment and documentation edits. These paths are unreached
unseamed, so the expected effect on every corpus row is NONE.

Disclosure (not a leg): before this file, `SqpLadderSeam.WalkCap*` ran once in the seam-test binary (Release):
the new fixture passed, a scratch phase-switch fixture failed (not committed).

## Criteria (fixed here, before data)

| leg | criterion |
|---|---|
| F1 | full suite, Release and Debug, each "100% tests passed, 0 tests failed out of 3653" (3652 + the new fixture); HsSweepCorpus pins unmoved (3355 / 3346) |
| I1 | the 27-cell replay x walk / ssn / ipm on the code commit's Release corpus binary, `compare_replay.py` byte-exact: 0 differences against `legs/rebase-92e7c5d83/{walk,ssn,ipm}.csv` (and so against `replay/k1-*.csv`) |
| I4 | TRAIN: DN-3's 238 cells x arms D (`default`) and K (`never-default`), lane binary built from the code commit through the DN-3 flip lane commit (`dn3-flip-legs`, local clone, never pushed). `legs/train_check.py` against the walk-cap I4 leg rows (`train/cells`) as DEV and `legs/movers-train.txt`: every row equal to its leg row (counters and statuses exact; residual-class values within the calibrated gate; optdump byte-equal), the mover set vs DN-3 r5 exactly the 21 declared: I4_PASS |

Any failed criterion: stop, report, nothing loosened.

## Protocols

F1: `ctest -j4` under setsid, as the DECLARATION. I1: serial, alone, as the DECLARATION. I4: CO-RUN, permitted for
counters and statuses only: 4 processes at a time, each pinned to its own PHYSICAL core (cores 2-5; SMT siblings
idle), `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, through a copy of DN-3's `sl/cell.sh` whose only change is the pinned
core (env `CELL_CORE`); CAP 300 s per cell. Wall-clock from this run is informational and quoted nowhere. A row that
deviates (including a capped row) is a candidate: it is re-run ALONE under the serial rule on core 2 and judged on
that re-run; only a deviation reproduced alone fails I4. The box lock is held for the whole chain.
