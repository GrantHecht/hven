M8 DN-1c -- S leg run 1 (DECLARATION-r2.txt S): VOID -- INVOCATION ERROR, ZERO CELLS RAN, NO VERDICT
================================================================================
Invocation  from this evidence directory: flock /tmp/box-build.lock bash sl/s_run.sh sl/out/s 80e98711...  (the
            DECLARATION's literal text), 2026-11-10T06:39:41-05:00 .. 06:39:45 (4 s), PGREP_EMPTY, lane binary sha
            verified.
Failure     the evidence directory was given RELATIVE. s_run.sh creates sl/out/s/cells relative to the caller's cwd,
            but sl/dn1_cell.sh cd's into the hven-bench clone before redirecting each cell's output to <prefix>.out,
            so every redirect failed ("No such file or directory") before the lane binary was exec'd: no solver
            process ran, no .result was written ("cells done ...: 0 results"), and s_analyze.py raised
            FileNotFoundError (analysis rc=1). s_run.sh itself exited 0. DN-1b's S leg (same text) ran with an
            absolute path.
Files       leg.out (the leg's stdout+stderr), roster.tsv (the roster copy the runner made), s-analysis.txt (the
            traceback). No cell data exist.
Successor   DECLARATION-r3.txt: the S and T legs invoked with the ABSOLUTE path of the same declared directories
            (sl/out/s, sl/out/t under this evidence directory); criteria verbatim.
