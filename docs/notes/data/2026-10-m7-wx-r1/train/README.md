# WX-R1 pre-landing TRAIN check -- results

Declaration: `DECLARATION.md` (commit cb14bfa2, written before the first cell; not edited). Pin: hven `ed412253` (library
code identical to the fix commit `ae151b87`). Harness: hven-bench `b5423f6` in a scratch clone (nothing committed or pushed
there). 46 runs (23 TRAIN cells x {`default`, `sqp-qn`}), 300-iteration lane budget, 300 s ceiling, solo under
`/tmp/box-build.lock`, `MKL_NUM_THREADS=1`, one cell per lock hold, in-lock `pgrep` empty. Chain: `logs/chain-summary.txt`
(46 legs rc=0; `logs/chain-attempt1-no-decode.out` is a first attempt in which every cell leg failed instantly because the
scratch clone lacked the decoded CUTEst problems: no solver ran; the decoded directory was then copied and the chain re-run).
Wall numbers are informational.

## Outcome against the declared STOP rule: CLEAN

- Rule 1 (a RETAKE19-optimal cell not optimal): none. 16 of 16 `default` and 8 of 8 `sqp-qn` RETAKE19-optimal cells are optimal.
- Rule 2 (a cell newly ending `max_iter` with a zero-step signature): none. Every cell's status equals its RETAKE19 status
  in both labels; no cell newly ends `max_iter`.
- Wall-clock cells: identical to RETAKE19. All cells RETAKE19 capped are capped again (`default` BRITGAS, MSS2, MSS3,
  READING8, TAX213322, TAX53322; `sqp-qn` ACOPR118, BIGBANK, MSS3, READING8, TAX213322, TAX53322, YORKNET). The comparator's
  READ list is the same as RETAKE19's: `default` BRITGAS; `sqp-qn` ACOPR118, BIGBANK, YORKNET (ended at the wall as
  declared while measured to finish solo), ACOPR57 and SAWPATH (`qn_kwrong_seen` 4 and 9). No wall-dependent deviation, so
  nothing to re-run on that account.

## What the comparator says, and why it exits 1 (two items the settler should read)

The unmodified RETAKE19 comparator (`compare-*-vs-retake19.txt`, run with `HVEN_RETAKE19_RUN` at RETAKE19's run directory)
exits 1 in both labels. Neither item is a declared-rule STOP, but both are reported plainly.

1. **options_digest differs on every row (both labels).** The declaration expected the options dump to be unchanged. It is
   not, and the cause is NOT the fix: the pinned library carries three new `IpqpOptions` dump lines added by the earlier m7
   landings (A2 `ipqp_mu0_rule`, `ipqp_mu_residual_floor`; A3 `ipqp_ladder_skip_after`), all at inert defaults
   (`options-dump-diff-default.txt`, `options-dump-diff-sqp-qn.txt`: the only differences are those three appended lines;
   all 23 dumps of a label are byte-identical to each other). The fix itself changed no dumped option. The pin is the m7
   head `0483a46f` plus the fix, and `6a2a58c7..0483a46f` has 307 lines of library change; the declaration's premise
   that the pin sits on RETAKE19's library was wrong in this respect.
2. **One moved row, `default` ACOPR118** (below). Counters and status of every other finished row are identical to RETAKE19
   (exact: status, majors, factorizations, and every compared counter and results column).

## Moved rows (status / majors / factorizations vs RETAKE19)

| label | cell | RETAKE19 | this run | cause |
|---|---|---|---|---|
| `default` | ACOPR118 | optimal / 25 maj / 14802 fact | optimal / 61 maj / 19560 fact (still optimal, f = 129660.68370269195 vs ...192) | the new complementarity gate. See below. |

No other row moved: 22 of 23 `default` cells and 23 of 23 `sqp-qn` cells are identical in status, majors and factorizations
(the capped ones are capped in both).

**ACOPR118 (`default`) -- cause read from the runs.**

- RETAKE19's row certified optimal at 25 majors with `kkt_complementarity` = 2.986e-06 (results.csv) and
  `sqp_complementarity` = 2.986e-06 (counters): ABOVE `kkt_tol` = 1e-6. The old convergence test did not gate that term
  for QP-priced multipliers, so it passed; the new third conjunct (`complementarity <= kkt_tol`) refuses that point, so the
  solve continues (61 majors, 43 rejected steps vs 8, `terminal_polish_adopted` 38 vs 5) and returns at
  `sqp_complementarity` = 6.4e-14, `sqp_stationarity` = 1.3e-09, `sqp_feasibility` = 7.1e-14.
- Attribution control: the same cell at the PARENT `0483a46f` (pre-fix, same harness, same toolchain) reproduces
  RETAKE19 exactly (optimal / 25 / 14802, objective 129660.68370269192): `control-0483a46f-ACOPR118/`. So the move is the fix,
  not the other m7 landings in the pin.
- Co-run check (coordinator's notice that a WX agent's test leg may have overlapped a `default` cell): the deviating
  cell was re-run ALONE, `PGREP_NO_UNCOVERED_CELL` and `IN_LOCK_PGREP_EMPTY` both logged, same lock protocol
  (`rerun-alone-ACOPR118/`, `logs/rerun-alone-ACOPR118.out`): optimal / 61 / 19560, identical to the chain run. Both runs
  are recorded. Not a co-run artifact.
- This is the scenario of review MEDIUM-1 (an absolute `kkt_tol` on the complementarity product) in its benign form: a
  point the old test certified at complementarity 3e-6 is refused, and the solve reaches a point at 6e-14 in more work (+36
  majors, +4758 factorizations, solo wall 90 s vs 69 s). It neither ends non-optimal nor spins.
- One observation, not explained here: the harness's base-result columns `kkt_stationarity` and `kkt_complementarity` read
  `nan` for this row (results.csv), while the engine's own `sqp_stationarity` / `sqp_complementarity` counters are finite
  (above). RETAKE19's row carried finite values in those columns. `sqp_solver_types.h` documents that the shared-base
  residual fields are left NaN when the terminal sweep moved a price after the measurement; this row presumably takes
  that path (the terminal polish adopted 38 times) -- not verified.

## Counts, recomputed from the run files

SNOPT's 15 TRAIN optimal cells are recomputed from `runs/2026-09-m7-h4-external/run-A/cutest/results.csv` (snopt lane,
status `Optimal`; run-B gives the same 15): ACOPP118, ACOPP57, ACOPR118, ACOPR57, BRITGAS, DALLASM, READING6, READING8,
SAWPATH, SMBANK, STEENBRB, STEENBRD, STEENBRE, YORKNET, ZAMB2-8.

| set | optimal | overlap with SNOPT | hven-only | SNOPT-only |
|---|---:|---:|---|---|
| `default` (hven-sqp) | 16 | 12 | BIGBANK, NET3, STEENBRC, STEENBRG | BRITGAS, READING8, YORKNET |
| `sqp-qn` (hven-sqp-qn) | 8 | 8 | none | ACOPR118, READING8, SAWPATH, STEENBRB, STEENBRD, STEENBRE, YORKNET |
| union | 17 | 13 | BIGBANK, NET3, STEENBRC, STEENBRG | READING8, YORKNET |

Identical to RETAKE19 (16 / 8 / union 17).

## Files

`run-A-<label>/{results.csv,counters-hven-sqp.csv}` (merged by RETAKE19's `w2_retake19_merge.py`), `compare-<label>-vs-retake19.txt`,
`options-dump-diff-*.txt`, `control-0483a46f-ACOPR118/`, `rerun-alone-ACOPR118/`, `logs/` (`chain.sh.txt` is the driver, adapted
from RETAKE19's chain with the pre-chain/pre-flight legs removed and the close done by hand).
