# M8 TR-E -- DECLARATION-r3 (committed before the re-run; DECLARATION.md and DECLARATION-r2.md are unedited)

r2 produced no verdict either: two more INSTRUMENT FAULTS, found in the first seconds of the legs, before any cell result was read for a verdict.
1. TRAIN: the cutest run aborted in both arms with FileNotFoundError, because the private clones lack `sets/cutest/sif/` (gitignored, fetched data). FIXED by copying the
   main checkout's `sets/cutest/sif` (35 SIF files, ignored data) into both clones; no script change. The hs runs of r2 completed in both arms; they are re-run with the rest.
2. M74: `runtime/runtime_leg.sh` pointed at `$WT/build-tre/armbin/` (my sed lost the `.scratch/` component), so every process failed with "No such file or directory" and the
   analysis stopped at counter identity. FIXED: BIN and SCRATCH now `$WT/.scratch/build-tre/...`, the path run_m74.sh freezes into.
Nothing else changes: code heads, arms, cells, maps, pass criteria, not-run list. P1 Release and P1 Debug (both 3850/3850, 100% passed) and ST ran in r1/r2 and are not
repeated; their logs are kept and cited in VERDICTS.txt. Outputs of the aborted r2 attempts under `.scratch/train` and `.scratch/m74` were removed.
