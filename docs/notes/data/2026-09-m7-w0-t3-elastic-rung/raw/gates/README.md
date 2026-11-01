# The gate transcripts — what each one is, when it ran, and on what tree

Written in fix round 2 (2026-09-16) because gtest and the gate scripts print no commit stamp
of their own: a reader of `debug-hven_tests.log` cannot tell from the file which tree produced
it. The windows below are from the box-protocol leg logs retained in this task's scratch
(`.scratch/m7t3-fix1/logs/<leg>.log`, each with its `FOREGROUND_START` / `FOREGROUND_END`
markers and exit code), and the tree states are from the commit times plus the working tree's
own mtimes. All times are 2026-09-16, local.

The two commits that bound the window: `40ef105f` (the fix-round-1 comment/doc commit) landed
at **21:33:39**; `0d358386` (the fix-round-1 evidence commit) at **21:55:55**. Every leg below
ran between them, so the CODE under test is `40ef105f`'s in every case — `0d358386` adds
artifact files only and changes no source.

| transcript | leg window | tree state at the leg | concurrency |
|---|---|---|---|
| `release-hven_sqp_tests.log`, `release-hven_drivers_tests.log` | 21:39:01–21:41:12 | **clean** at `40ef105f` | SOLO |
| `debug-hven_tests.log`, `debug-hven_drivers_tests.log`, `debug-hven_model_tests.log`, `debug-hven_interior_tests.log` | 21:41:34–21:42:21 (one leg: the Debug build then the four suites) | **clean** at `40ef105f` | SOLO |
| `install-smoke.log` | 21:43:56–21:44:28 | `40ef105f` **plus one modified tracked file**: `../../PROVENANCE.txt`, artifact prose, rewritten at 21:42:27. No source, header, test, script or CMake input was modified (their mtimes are 21:28–21:29, i.e. edits `40ef105f` already contains) | SOLO |
| `export-contract.log` | 21:44:37 | as above; it reads the install tree the smoke leg produced | SOLO |
| `clang-format.log` | HEAD arm 21:44:40–21:45:23; base arm immediately after, in the `3d3145a8` worktree | as above for the HEAD arm; the base arm ran in a separate clean worktree at `3d3145a8` | SOLO. The check enumerates `git ls-files '*.cpp' '*.h' '*.cxx' '*.hpp'` minus `dep/`, so the modified `.txt` is not in its file set either way |

"SOLO" means: alone under `flock /tmp/box-build.lock`, with the pre-leg
`pgrep -af 'ninja|ctest|hven_|run_serial'` finding nothing but its own match. The foreign
hven-bench harness lane's activity began at 21:48, after every leg in this table; the legs it
did overlap are declared in `PROVENANCE.txt`'s CONCURRENCY block and in those logs' own
headers.

## Two numbers a reader should not have to recompute

* `clang-format.log` checks **403** files per arm (line 7 and line 49), not the "700-file
  tracked set" the fix-round-1 report section §3 says — that figure was wrong when written and
  is corrected here and in the fix-round-2 report section. The gate's verdict is unaffected:
  36 violations at `3d3145a8`, 36 at the fix HEAD, **0 new**, and the two arms enumerate the
  same set with the same command.
* The Debug suites' PASSED lines are at `debug-hven_tests.log:802` (371),
  `debug-hven_drivers_tests.log:338` (134), `debug-hven_model_tests.log:654` (258),
  `debug-hven_interior_tests.log:1296` (510).
