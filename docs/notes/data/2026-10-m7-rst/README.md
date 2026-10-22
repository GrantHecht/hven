# M7 W2 RST -- the restoration phase's curvature and entry rule: the measurement record

Commit under measurement: `80da6e46` on branch `rst-restoration` (parent: m7 `0dc2f1aa`).
Options: `SqpOptions::restoration_curvature = kModelThenRegularized`, `restoration_max_entries = 10`
(the new library defaults); the bench lane's own options are RETAKE16's (300-iteration budget,
`column_scaling = kAuto` label-wide on `default` and `sqp-qn`).

## Provenance

| | |
|---|---|
| toolchain | clang 22.1.8 (Fedora 22.1.8-4.fc44), Release, LTO off (RETAKE16's lane build flags) |
| host | AMD Ryzen 7 5800X3D, 8 physical cores |
| date | 2026-10-22 |
| bench lane | a working-tree copy of hven-bench `c214ba1` built against an install of this tree; the lane carries scratch patches that are inert without their environment variables (trace sink, option overrides) and were exercised only on the rows marked as such |
| measured binary | lane sha256 `479179236cd4...`; the binary rebuilt from the committed tree (`80da6e46`) is byte-identical |
| baseline | hven-bench `runs/2026-10-m7-w2-retake16/measured/pin/<cell>-<label>.out` (pin `0dc2f1aa`) |

Cell conditions (CLAUDE.md section 7): one solver at a time, each cell its own hold of
`/tmp/box-build.lock`, `MKL_NUM_THREADS=1`, `taskset -c 2`, a watchdog signalling only its own
child. Cell caps: 300 s on the five cells RETAKE16 capped, 450 s on the others (BIGBANK `sqp-qn`
re-run at 590 s, RETAKE16's cap for it). Scoring = `optimal` inside 300 s. Wall-clock is
informational; statuses, objectives and counters are the claims.

## Files

* `train46-vs-retake16.txt` -- all 46 TRAIN label-cells against RETAKE16, field by field (time fields excluded).
* `target-traces.txt` -- per-depth trace summaries (majors, verdicts, QP statuses, minors) for the two
  target cells at the base `0dc2f1aa` and for BRITGAS and NET3 at this head.
* `ablations-and-experiments.txt` -- the two levers apart (NET3, BRITGAS), and the experiment lanes the
  design was chosen on (environment hooks, never shipped).
* `regularized-only-arm-partial.txt` -- `kRegularized` alone (no exact phase), the 20 exact-label cells
  it reached before it was stopped: it loses STEENBRC/E/G (296 of 300 majors in one phase).
* `suites.txt` -- Release and Debug ctest, 3187/3187 each (2 tests disabled in the tree, unchanged).
* `u0-exact-vs-scl-record.txt` -- the U0 exact arm against `docs/notes/data/2026-10-m7-scl/gates/replay`: 0 / 0 / 0.
* `mutants.txt` -- 14 mutants on the new logic, all killed.

## Summary

| | RETAKE16 | this head |
|---|---|---|
| exact label (`default`) optimal inside 300 s | 15 / 23 | **16 / 23** (+NET3) |
| QN label (`sqp-qn`) | 8 / 23 | 8 / 23 |
| union | 16 | **17** |

* NET3 `default`: numerical_error/8 (401 s) -> **optimal/23 at 96 s**, f = 5859393.702, stationarity
  3.9e-9, primal 9.0e-13; SNOPT (PointCannotBeImproved) and IPOPT (Restoration_Failed) fail this cell.
  Both levers are needed: entries = 1 gives numerical_error/11, kModel with re-entry hits the 450 s cap.
* BRITGAS `default`: numerical_error/19 (403 s) -> optimal/62 but at 378 s, beyond the ceiling. Its cost
  after the change is the MAIN loop (63 majors, 57.6k walk minors, 30 QP iteration-cap exits), not
  restoration (2 phases, 30.7k minors). Either lever alone leaves it numerical_error.
* YORKNET: fail -> fail in both labels. Its restorations succeed once convex; the main loop's QPs at
  the restored near-feasible points fail.
* Every other exact-label cell is field-identical to RETAKE16 or capped in both; in the QN label 16 are
  identical, 4 capped in both, and NET3 / STEENBRC / STEENBRG move fail -> fail (the re-entry rule).
  No optimum lost, none worsened.
