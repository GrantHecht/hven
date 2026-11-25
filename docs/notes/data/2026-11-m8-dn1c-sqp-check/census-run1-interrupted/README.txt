M8 DN-1c -- census run 1 (DECLARATION.txt P5, legs/census.sh via legs/chain1.sh): INTERRUPTED -- VOID, NO VERDICT
================================================================================
Cause       an external harness restart (about 00:35 local, 2026-11-10) killed the implementing agent's session and,
            with it, the census process tree. Not a criterion failure: no census_check.py ran, no verdict was produced,
            and nothing in these files is read as one.
Started     2026-11-09T23:49:17-05:00 (04:49:17Z), under /tmp/box-build.lock, PGREP_EMPTY, the frozen arm B binary
            build-dn1ce/armBbin/hven_sqp_corpus sha256 65040221dc68c6df1eda7f2d4a4319c5f7aa2643156f49f60c3dc077193ae823
            (verified by the leg, ../chain/census.out).
Reached     tier 1 complete (42 of 42 cells, 05:22:00Z); tier 2 started 05:22:00Z, its first cell
            f7_n10000_path_physics in flight (rows-in-flight.txt: a .part row, never merged); tier 3 never started.
            No merged walk_census.csv, no compare.txt, no census_check.txt.
Files       census-run.log  the runner's stdout as the leg captured it (moved here from ../census/, which held only it)
            run.log         the runner's own log (build-dn1ce/census-run/run.log, copied)
            rows-completed.csv  a compact summary (cell_id, status, factorizations, qp_minors, escapes, kkt_residual,
                            wall_s) of the 42 completed per-cell rows (build-dn1ce/census-run/rows/*.csv), informational
            rows-in-flight.txt  the .part row present at the interruption
Successor   DECLARATION-r2.txt (instrument only): the census leg re-run from scratch into a fresh run directory with
            the same binary and P5's criterion verbatim; the S and T legs carried verbatim. This record is kept.
