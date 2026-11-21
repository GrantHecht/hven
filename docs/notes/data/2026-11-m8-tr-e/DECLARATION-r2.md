# M8 TR-E -- DECLARATION-r2 (committed before the re-run; DECLARATION.md is unedited)

r1 of the TRAIN leg aborted after its first run: `runners/run_serial.py` exits 1 when any cell trips its reference check (`hs/hs033/hven-sqp/default` does, at BOTH pins: arm A
reported it too), and `legs/train.sh` treated every non-zero exit as a failed leg, so the cutest and traced runs never ran. No verdict was read from r1 (its hs outputs were
deleted unread-for-verdict). This is an instrument fault, not a result.

CHANGED, and only this: `legs/train.sh` now continues past a runner exit of 0 or 1 when `results.csv` was written, and aborts on any other exit or a missing file. Everything else in
DECLARATION.md stands unchanged: code heads, arms, cells, maps, the pass criteria (TR (a)-(c), P1, ST, M74) and the not-run list. The previous r1 outputs under `.scratch/train/` were
removed before the re-run, so r2 starts from nothing. P1 Release (3850 tests, 100% passed, run before this note) is a leg of the r1 chain and is not repeated; its log is kept and
cited in VERDICTS.txt; P1 Debug, ST and M74 have not run yet at this point.
