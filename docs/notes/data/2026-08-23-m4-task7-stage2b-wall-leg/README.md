# Declaration adoption + frozen thread modes — wall-leg record (M4, hven side)

The change under measurement is the three-commit chain `6d6288e` (the entry and
the frozen thread modes) → `d6f87bc` (refuse before adopting anything) →
`4bdb24d` (the lay marker out of reach, and every laid piece frozen). Both arms
of both legs are built at the chain head, `4bdb24d`.

Two standing legs, each against its own base, reported separately rather than
pooled because they answer different questions and their bases differ.

| leg | base | head | what it observes |
|---|---|---|---|
| layout leg (`layout_time.cpp`) | `e832675` (the parent of the chain) | `4bdb24d` | what a TRANSCRIPTION costs — the arm `non_linear_program.cpp` and the piece types are hot in |
| IPM leg (`ipm_time.cpp`) | `6d7433f` (the standing watch base) | `4bdb24d` | whole solves through `NLPSolver` |

Both legs live in `../m4-ipm-wall-leg/`; their protocol, their two estimators
and the reading rules are stated there. Deltas are read off the MINIMUM
estimator. Logs: `../m4-ipm-wall-leg/runs/2026-08-23-stage2b-fixes-layout.log`
and `../m4-ipm-wall-leg/runs/2026-08-23-stage2b-fixes-ipm.log`.

Both base arms were built the way `../m4-ipm-wall-leg/PROVENANCE.md` records:
`git archive <commit> | tar -x` into a scratch tree with `dep/eigen` and
`dep/fmt` symlinked to the working tree's submodules, configured with
`/usr/bin/clang++`, `Release`, `HVEN_FP_MODE=SAFER_FAST`. The head arms are the
working tree and its `build/`. The probe sources compiled into both arms are
`../m4-ipm-wall-leg/`'s own copies; each log's build header carries the probe
sha256 and both binaries' sha256 and mtime.

Conditions: Fedora, AMD Ryzen 7 5800X3D 8C/16T, 31 GiB, Linux
7.1.9-200.fc44, `/usr/bin/clang++` 22.1.8, MKL LP64 static, Release,
`HVEN_FP_MODE=SAFER_FAST`, `-DHVEN_DEFAULT_QP_THREADS=8`. Both runs on
2026-08-23, holding `flock /tmp/box-build.lock`, started with the one-minute
load average at 0.26 and 0.20 respectively and no foreign compile or bench on
the box.

Release test suite at the head: **1491 registered / 1489 passed / 0 failed /
2 skipped / 2 disabled**, `MKL_NUM_THREADS=1`, `ctest -j2`, build `-j6`. The
two skips and two disables are the standing four.

## The layout leg — the cell that carries the bar

The asserted cell is `transcribe+decl`: the re-lay plus one `declaration()`
read, which is what every evaluating consumer pays once per lay. Minimum
estimator, base `e832675` -> head `4bdb24d`:

| arm | n | `transcribe+decl` | `transcribe` | `transcribe+key` | `construct` | base rep-spread (decl) |
|---|---|---|---|---|---|---|
| serial | 64 | +0.000% | +0.000% | +5.000% | +0.000% | 7.14% |
| serial | 256 | +0.000% | +0.000% | +0.000% | +0.000% | 3.57% |
| serial | 1024 | +0.518% | +1.058% | +0.323% | -0.412% | 6.12% |
| threaded | 64 | +0.000% | +0.000% | +0.000% | +0.000% | 7.69% |
| threaded | 256 | +0.000% | +0.000% | +0.000% | +0.000% | 37.50% |
| threaded | 1024 | +0.518% | +0.529% | +0.322% | +0.413% | 43.94% |

**Every `transcribe+decl` cell is inside the +/-1% bar**, at +0.000% in four of
six and +0.518% in the two largest. `transcribe` clears the bar once, serial
n=1024 at +1.058%, against a 6.7% base rep-spread on that cell and its own
threaded twin at +0.529%; the n=64 `transcribe+key` +5.000% is one 20 us cell
moving by one microsecond of timer resolution, against a 9.5% base spread.

**Identity.** Every layout cell's structural key digest and claim count are
EQUAL between the two arms at all three sizes, on every rep. `solve1` — the
leg's bit-identity gate, one partition and one factorization thread — has one
objective per size on both arms, identical. `solve`'s objective moves in its
low bits within each arm, as the leg's README documents for that cell; its
gate is its iteration count and flag, equal across arms (17 / 14 / 0).

## The IPM leg — whole solves against the watch base

Base `6d7433f` -> head `4bdb24d`, minimum estimator, 9 reps x 15 inner solves:

| arm | n | median | minimum | base rep-spread |
|---|---|---|---|---|
| serial | 60 | -0.602% | -0.204% | 1.50% |
| serial | 120 | -0.486% | +0.000% | 1.55% |
| **serial** | **240** | **+0.086%** | **+0.061%** | 2.89% |
| threaded | 60 | -0.694% | -0.109% | 36.09% |
| threaded | 120 | -0.759% | -0.263% | 48.72% |
| threaded | 240 | +0.884% | +0.079% | 6.51% |

Every cell reads within +/-0.3% on the minimum estimator, well inside the
standing one-sided +3.0% band at serial n=240.

**INFORMATIONAL rather than asserted, and the reason is the base arm's own
spread, not the head's numbers.** The rubric this leg carries asserts a run
when the n=240 base rep-spread is under about 1.3%; here the three serial
cells spread 1.50%, 1.55% and 2.89%. Every head cell sits far inside that
spread in both directions, so the run corroborates parity rather than
establishing it to the tighter standard. Nothing in the change predicts a
whole-solve effect -- see the symbol section below -- and a re-run at a
quieter moment would be the way to promote it if that is wanted.

**Identity.** All 54 base/head pairs bit-identical: `flag=0` and one `iters`
and one `xnorm2` per size on both sides -- 7 / 1.3750963021836506 (n=60),
7 / 2.7501926043856093 (n=120), 8 / 5.5003852082409646 (n=240).

## Mechanism (`nm`)

`nm_filtered_layout.txt` and `nm_filtered_ipm.txt` are the symbol-size dumps of
the two probe images at both arms, filtered to the model, provider, piece and
engine families (`nm -C --print-size --size-sort`). Unfiltered dumps of all
four images are kept out of tree; their md5s are in `nm_full_md5.txt`.

31 filtered symbols moved in the layout image and 32 in the interior-point one.
Three groups, none on an evaluation path:

- **New entries**, none of them called by any evaluation:
  `NonLinearProgram::adopt_declaration` (0x60c),
  `NonLinearProgram::freeze_laid_thread_modes` (0x146),
  `NonLinearProgram::splice_fixed_variable_rows` (0xf3),
  `SolverFunctionBase<...>::set_thread_mode` (0x150 each).
- **Lay-time routines**: `AggregateDeclaration::validate` 0x38b -> 0x56b (the
  range conjunct and the per-piece tail-shape check),
  `declaration()` 0x5d -> 0xf5 and `materialize_declaration_pieces`
  0x49 -> 0xdb (the thaw over the copies), `capture_laid_dimensions`
  0x250 -> 0x266 (the freeze moved out into its own routine),
  `analyze_partitioning` 0x61c -> 0x68b,
  `install_fixed_variable_rows` 0x4db -> 0x4e5 (its splice factored out).
  All once per lay.
- **Piece construction and copying**, +7 to +16 bytes each across the
  construct / copy / move / vector-growth family, from the one added `bool`.

**The piece types did not change size**: `sizeof(ConstraintFunction)` and
`sizeof(ObjectiveFunction)` are 352 bytes on BOTH arms -- the laid marker lands
in the trailing padding beside the `ThreadingFlags : int`. The partition vectors
an evaluation walks therefore have byte-identical layout, which is why the
whole-solve leg is expected to read at parity, and does.
