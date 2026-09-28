# fix round 5 evidence — what was copied and what was left out

Source: `.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4-fix5/` (read-only at copy time).
Every file below was `cmp`-verified byte-identical against its source; the diagnosis in
`../REPORT-fix5.md` is a read of these artifacts, not a re-derivation from them.

## Copied (41 files, 2.4 MB total)

| destination | source | what |
|---|---|---|
| `dumps/ACOPR118_e4/qp0.txt` .. `qp8.txt` | `dumps/ACOPR118_e4/qp{0..8}.txt` | the nine E2-D4 sub-QP dumps for the ACOPR118 cell, text form |
| `cert/qp0.oracle.txt`, `qp0.oracle.json`, `qp5.oracle.txt`, `qp5.oracle.json` | `cert/` (same names) | the oracle certificates for the two sub-QPs the diagnosis cites (qp0 the major-0 SOC re-solve, qp5 the walk-owned face) |
| `logs/*.log`, `logs/*.txt` | `logs/` (same names) | every configure/build/install log (`cfg-*`, `build-*`, `install-*`), the two replay comparison logs (`replay-cmp.log`, `replay-sens.log`), and the repro logs (`repro.log`, `repro-cmp.txt`) |
| `replay/dump_replay.cpp`, `replay/CMakeLists.txt` | `replay/` (same names) | the dump-replay tool's own source — the only non-build content under `replay/`; its build trees are excluded (see below) |
| `trace/ACOPR118-diag.out`, `.err`, `.dump` | `trace/` (same names) | the diagnostic trace run's stdout/stderr/dump, all plain ASCII text |
| `STATE.md` | `STATE.md` | the implementer's own running state note |
| `block.sh`, `build.sh`, `build_replay.sh`, `build_replay_diag.sh`, `leg.sh`, `remedy.sh`, `remedy2.sh`, `replay_cmp.sh`, `replay_sens.sh`, `patch_diag.py`, `patch_diag2.py` | top level (same names) | the fix-5 runners: the build/leg/replay drivers and the two patch scripts used to instrument the diagnosis |

## Left out, and why

| path (source) | why left out |
|---|---|
| `dumps/ACOPR118_e4/qp0.xstar.npy` | binary `.npy`; the task scope is the E2-D4 dumps' text form, explicitly skipping the `.npy` |
| `build-rel/`, `bench-build/`, `replay-build-85dc01ac9f53/`, `replay-build-ff02789b46a7/`, `replay-build-diag/` | build trees (CMake caches, ninja files, object trees, linked binaries) — excluded per scope, reproducible from the runners above plus the pinned commits they name |
| `bench/` | the bench checkout used to drive the cell; a full working tree, not evidence |
| `src/` | the hven checkout used for the diagnosis builds; a full working tree, not evidence |
| `repro/` (`ACOPR118-pin.out`, `.err`, `.dump`, `cell.sh`) | excluded as one of the named build/binary trees; its `.out`/`.dump` pair duplicates `trace/ACOPR118-diag.out` / `.dump` byte-for-byte (same sizes, same content), and `cell.sh` is superseded by the runners already copied |

Nothing copied is over 1 MB, so no gzip step was needed (the largest single file is
`logs/install-hven.log` and its three re-run duplicates at 186 KB each).
