# M7 W2 E4 (the proximal EQP) — the implementer's evidence, copied

Source: the E4 implementer's worktree scratch,
`.claude/worktrees/agent-a69f46e11796560b9/.scratch/e4/`, across all three rounds (round 0,
fix round 1, fix round 2). The index the three reports cite is `../REPORT-fix2.md` §11;
this file says what was copied here and what was left behind.

**Copy verification.** Every one of the **264** files under this directory was produced with
`cp -p` from the path below and then `cmp`-checked against that source. Every comparison
reported no differences (`cmp` exit 0, no output), the whole set re-verified once more
immediately before the declaration commit. Nothing was rewritten, re-indented or renamed —
`runners/` is the one place where the path changed (the scratch root's own top-level files are
gathered into a subdirectory), and the bytes did not. Total 7.6 MB; no file exceeds 1 MB, so
nothing needed gzipping.

**Four sources MOVED between the first copy and the commit** and all four were RE-COPIED and
re-verified rather than left stale: `STATE.md`, which the implementer kept appending to, and the
three `u0/head-*.csv`, which the fix-3 U0 run re-wrote in place at 09:18–09:19. The table below
says so at each one. That re-verification is why the final count is 264 with zero mismatches
rather than 260 with four.

## What is here

| path | what | source |
|---|---|---|
| `corpus/` | the 51-dump corpus rows, per head × algebra × cell-set, E2 on and off. 90 files. The head prefixes are the implementer's: `g1*` = the base, `h1*` = `f2226c28` (fix round 1), `e4*`/`fix1*` = round 0 and fix 1 arms, `k1*` = the SHIPPED engine, `k1off*`/`*off*` = the E2-OFF control lane. `new15`/`old36` are the two cell sets, `.err` the run's stderr | `corpus/` |
| `logs2/` | every log of the SHIPPED tree: `ctest-release-final2.log` (0 of 2870), `ctest-debug-final2.log` (0 of 2869) + `-long.log` (the one > 600 s test alone, 622.29 s), the identical earlier pair `*-final.log`, `i51-u0.log` (U0), `i51-key.log` / `i25-key.log` (census + HS + B1Gate), `rowcmp-k1-vs-{base,h1}.txt` (the all-column comparisons), `cap1e-{6,5,4}-{wsb,key}.log` (**the cap-search table**), `rebuild-{debug,release}.log` (the byte-identical `libhven.a` across the prose fix), `final2.log` / `final3.log` (the battery runners, with `PGREP_EMPTY` / `FOREGROUND_START` / `FOREGROUND_END` per leg), and the per-fixture diagnosis logs `i*.log`. 72 files | `logs2/` |
| `cert/` | the DALLASM certificate verification: `certcheck.txt`, `rawfeas.txt`, the four `*.cert` files and the two oracle read-outs per QP (`.txt` and `.json`) | `cert/` |
| `u0/` | the U0 replay. **`comparison.txt` is the transcript the 43-entry G4 declared set was first derived from and checked against** (`../wrappers-gates/preflight.txt` transcript C). The three `head-*.csv` are copied AS THE FIX-3 RUN LEFT THEM, so `u0/` is one self-consistent replay **at the final sources `0d519a2b`** rather than a mixture. `comparison.txt` is byte-identical between the fix-2 and fix-3 runs (`cmp` rc 0) — the fix-3 report's own claim, independently checked here — so nothing about the declared set depends on which run's CSVs are present. `run-*.log` are fix round 2's and are unchanged | `u0/` |
| `tools/` | the analysis tools, `*.py` only: `certcheck.py`, `rawfeas.py`, `qp_oracle.py`, `d4load.py`, `rowcmp.py`, `fcmp.py`, `patch_diag.py`, `table.py`; plus `tools/replay/` (the dump-replay probe's four source files, `CMakeLists.txt`, `dump_replay.cpp`, its `.bak-fix2` predecessor and `probe.cpp`) | `tools/` |
| `runners/` | the leg runners and their input lists, from the scratch root: the `*.sh` (`leg.sh`, `build.sh`, `build_tools.sh`, `corpus.sh`, `corp_fix2.sh`, `capsearch.sh`, `dbgfix2.sh`, `suite2.sh`, `g4.sh`, `final3.sh`, the `i*.sh` / `diag*.sh` / `probe*.sh` one-shots), `statuscmp.py`, `msgs.py`, the `*.list` inputs (`capcells`, `cs`, `dumps`, `drift`, `drift2`, `new15`, `old36`), the three `commit-msg*.txt`, and `patch-A-terminal-polish-only.diff` | the scratch root |
| `logs3/` | **the FIX-ROUND-3 battery at the final tree `0d519a2b`**: `ctest-release-k3.log` (0 of 2870, 113.89 s), `ctest-debug-k3.log` (0 of 2869, 634.99 s) + `ctest-debug-k3-long.log` (the > 600 s test alone, 619.78 s), `k3-u0.log` (the U0 replay — 43 differences, the whole comparison block byte-identical to fix round 2's), `k3-key.log` (the T7 census + HS + B1Gate, 38/38), `fix3.log` (the detached chain runner, with `PGREP_EMPTY` / `FOREGROUND_START` / `FOREGROUND_END` per leg). 6 files | `logs3/` |
| `STATE.md`, `STATE-e4-round0.md` | the implementers' running state across the rounds. `STATE.md` is a LIVING file; the copy here is the one on disk when the declaration was committed (mtime 09:46, after the fix-3 commit) and is `cmp`-verified against it | the scratch root |

## What was left out, and why

| left out | size | why |
|---|---|---|
| `tools-build-replay/` | 217 MB | a CMake build tree. Regenerable from `tools/replay/` and the repo; build trees are never committed |
| `logs/` | 13 MB, 203 files | the INTERMEDIATE-head logs of round 0 and fix round 1. `REPORT-fix2.md` §11 cites `logs/` only for heads the round no longer ships (`ctest-release-i50.log` and the `i21`/`i23`/`i24`/`i27`/`i45`/`i47` failure lists); every log of the SHIPPED tree is in `logs2/`, which is what the gate record needs. The `logs/` transcripts remain in the implementer's worktree |
| `install/` | small | an install-smoke tree the round produced in passing. A build/install artifact, and G6 makes its own |
| `tools/__pycache__/` | small | generated bytecode |
| the `.txt` dump fixtures themselves | — | three of them are ADDED to the repo by the code commit under `tests/qp/fixtures/e2-d4/`, where they belong; the rest are the R2/A0 evidence's and are not this round's to copy |

## What is NOT here

No gate transcript. This directory holds the IMPLEMENTERS' own measurements. The gate chain's
transcripts land in a sibling `gates/` directory with its own `README.md` separating DECLARED from
MEASURED, written after the chain finishes. The declaration's own pre-chain measurements — the
P-SYM rehearsal, its offline re-classification and the offline U0 replay — are not here either:
they are in `../wrappers-gates/rehearsal/`, committed with the declaration.
