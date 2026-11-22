# DECLARATION-RF-1-AB2-r1 -- leg P of gate A+B2, re-run (successor; DECLARATION-RF-1-AB2.md is kept unedited)

Committed before the re-run, never edited afterwards. Everything of `DECLARATION-RF-1-AB2.md` stands except leg P.

## Why

Leg P (`ab2_corun.sh` -> `pilot_run.sh`) ran on 2026-11-22 00:52-00:55 and is **VOID**: every one of the 62 cells of both arms
threw `cutest: cannot enter .scratch/h3/decoded/<problem>` -- the pilot harness reads its decoded CUTEst set relative to its
working directory, and `pilot_run.sh`'s run directory had none. No cell produced a row. Its outputs stay in `gate-AB2/pilot/`
as the record of the void run. Legs T (TRAIN) of the same `ab2_corun.sh` step are unaffected (the TRAIN lane reads its own
clone's decoded set) and stand as run.

## The re-run

- `legs/pilot_run_r1.sh` = `pilot_run.sh` with one change: the run directory gets `.scratch/h3/decoded`, a link to the TRAIN
  lanes' decoded set `.scratch/hb/.scratch/h3/decoded`. Checked read-only before this file: for all 8 pilot problems the
  concatenated decoded files hash identically to those of the TU-1 store (`hven-bench-tu1`, which is only read, never
  written).
- `legs/ab2_pilot_r1.sh`: arms C and AB2 co-run on two pinned physical cores of one box lane, the same binaries (`pilot C`,
  `pilot AB2` in `BINARIES.txt`, sha-checked), the same 62 cells, budget 300 iterations, 900 s kill; `pilot_report.py`.
  Evidence in `gate-AB2/pilot-r1/`.
- Bar: none (reported, as declared). The leg is void again if any cell throws for an infrastructure reason; the count of
  thrown cells is printed.
