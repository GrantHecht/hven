# Declaration adoption + frozen thread modes — wall-leg record (M4, hven side)

The change under measurement is `6d6288e` (`feat(model): lay a problem from a
declaration, and freeze the thread modes`). Two standing legs were run, each
against its own base, and they are reported separately rather than pooled
because they answer different questions and their bases differ.

| leg | base | head | what it observes |
|---|---|---|---|
| layout leg (`layout_time.cpp`) | `e832675` (the parent of the change) | `6d6288e` | what a TRANSCRIPTION costs — the arm `non_linear_program.cpp` and the piece types are hot in |
| IPM leg (`ipm_time.cpp`) | `6d7433f` (the standing watch base) | `6d6288e` | whole solves through `NLPSolver` — **NOT YET RUN**, see below |

Both legs live in `../m4-ipm-wall-leg/`; their protocol, their two estimators
and the reading rules are stated there. Deltas are read off the MINIMUM
estimator. Log: `../m4-ipm-wall-leg/runs/2026-08-23-adopt-declaration-layout.log`.

**The IPM leg is OWED.** Both its arms are built (`ipm_base` at `6d7433f`,
`ipm_head` at `6d6288e`, beside the leg scripts) and the run is the standing
`./ipm_wall_leg.sh 9 15`; it wants a quiet box, which was not available in this
window. Its row and the whole-solve reading — reported against the watch base
and NOT pooled with the layout leg — land when it runs.

Both base arms were built the way `../m4-ipm-wall-leg/PROVENANCE.md` records:
`git archive <commit> | tar -x` into a scratch tree with `dep/eigen` and
`dep/fmt` symlinked to the working tree's submodules, configured with
`/usr/bin/clang++`, `Release`, `HVEN_FP_MODE=SAFER_FAST`. The head arm is the
working tree and its `build/`. The probe sources compiled into both arms are
`../m4-ipm-wall-leg/`'s own copies; each log's build header carries the probe
sha256 and both binaries' sha256 and mtime.

Conditions: Fedora, AMD Ryzen 7 5800X3D 8C/16T, 31 GiB, Linux
7.1.9-200.fc44, `/usr/bin/clang++` 22.1.8, MKL LP64 static, Release,
`HVEN_FP_MODE=SAFER_FAST`, `-DHVEN_DEFAULT_QP_THREADS=8`. Both runs held
`flock /tmp/box-build.lock` and started with the one-minute load average
below 0.6, with no foreign compile on the box.

## The layout leg — the cell that carries the bar

The asserted cell is `transcribe+decl`: the re-lay plus one `declaration()`
read, which is what every evaluating consumer pays once per lay. Minimum
estimator, base `e832675` -> head `6d6288e`:

| arm | n | `transcribe+decl` | `transcribe` | `construct` | base rep-spread (decl) |
|---|---|---|---|---|---|
| serial | 64 | +0.000% | +0.000% | +0.000% | 7.69% |
| serial | 256 | **-1.818%** | +0.000% | +0.000% | 3.51% |
| serial | 1024 | +0.000% | +0.526% | +0.826% | 3.54% |
| threaded | 64 | +0.000% | +0.000% | +0.000% | 7.69% |
| threaded | 256 | +0.000% | +1.887% | +1.515% | 5.36% |
| threaded | 1024 | +0.515% | +1.058% | +0.826% | 2.04% |

**Every `transcribe+decl` cell is inside the +/-1% bar, and the one cell that
clears it clears it on the FAST side.** The two `transcribe` / `construct`
cells that read above +1% (threaded n=256 at +1.887% / +1.515%) sit against
base rep-spreads of 3.6% and 7.5% on the same cells and are not separated from
their own arm's noise; their serial twins read +0.000%.

**Identity.** Every layout cell's structural key digest and claim count are
EQUAL between the two arms at all three sizes, on every rep. `solve1` — the
leg's bit-identity gate, one partition and one factorization thread — has one
objective per size on both arms, identical. `solve`'s objective moves in its
low bits within each arm, as the leg's README documents for that cell; its
gate is its iteration count and flag, which are equal across arms (17 / 14 / 0
flag).

## Mechanism (`nm`)

`nm_filtered_layout.txt` and `nm_filtered_ipm.txt` are the symbol-size dumps
of the layout and interior-point probe images, filtered to the model, provider, piece and engine
families. Unfiltered dumps of all four images are kept out of tree; their md5s
are in `nm_full_md5.txt`.

Three groups of symbols moved, and none of them is on an evaluation path:

- **The new entries.** `NonLinearProgram::adopt_declaration` (0x743) and
  `SolverFunctionBase<...>::set_thread_mode` (0x150 each) are new. Neither is
  called by any evaluation.
- **Lay-time routines.** `capture_laid_dimensions` 0x250 -> 0x2d7 (the freeze
  loop over the three master lists), `materialize_declaration_pieces`
  0x49 -> 0xdb and `declaration()` 0x5d -> 0xf5 (the thaw loop over the copies),
  `AggregateDeclaration::validate` 0x38b -> 0x419 (the new conjunct),
  `analyze_partitioning` 0x61c -> 0x68b. All once per lay.
- **Piece construction and copying**, +7 to +16 bytes each across the
  construct/copy/move/vector-growth family, from the one added `bool`.

**The piece types did not change size**: `sizeof(ConstraintFunction)` and
`sizeof(ObjectiveFunction)` are 352 bytes on BOTH arms — the laid marker lands
in existing padding beside the thread mode. The partition vectors an
evaluation walks therefore have byte-identical layout, which is the reason to
expect the whole-solve leg to read at parity — a prediction the owed IPM leg
tests rather than one this record asserts.
