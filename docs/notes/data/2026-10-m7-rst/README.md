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
  Its BIGBANK `sqp-qn` row reads `row<->dnf` because that cell ran at a 450 s cap; see the riders section.
* `target-traces.txt` -- per-depth trace summaries (majors, verdicts, QP statuses, minors) for the two
  target cells at the base `0dc2f1aa` and for BRITGAS and NET3 at this head.
* `ablations-and-experiments.txt` -- the two levers apart (NET3, BRITGAS), each row labelled with the lever
  it changed, and the experiment lanes the design was chosen on (environment hooks, never shipped, labelled
  `[hook]`).
* `regularized-only-arm-partial.txt` -- `kRegularized` alone (no exact phase), the 20 exact-label cells
  it reached before it was stopped: it loses STEENBRC/E/G (296 of 300 majors in one phase).
* `suites.txt` -- Release and Debug ctest, 3187/3187 each (2 tests disabled in the tree, unchanged).
* `u0-exact-vs-scl-record.txt` -- the U0 exact arm against `docs/notes/data/2026-10-m7-scl/gates/replay`: 0 / 0 / 0.
* `mutants.txt` -- 18 mutants on the new logic (14 of the implementation, 4 of the riders), all killed,
  each with the tests that killed it and its exact substitution.

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
  No optimum lost, none worsened. The sixteenth identical QN cell is BIGBANK `sqp-qn`, whose 450 s cell
  capped: its 590 s re-run (RETAKE16's cap for it; last row of the lever table in
  `ablations-and-experiments.txt`) reads max_iter/300, f = -4150620.403, every non-time field equal to
  RETAKE16's row -- and the riders' run below puts it through the comparator, `IDENTICAL`.

## Riders (ruling C110, the Fable review): commit `a5c29790`

One behaviour change and contract text. The behaviour change (review M1): inside a restoration
sub-solve under `kModelThenRegularized`, an exhausted elastic tier raises the nested restoration
request, which a phase never services; that refusal is now the switch's second trigger (counted in
`restoration_curvature_switches`), so the phase restarts on the regularized model instead of ending
`numerical_error`. The elastic tier keeps its place first. The ruling's bar -- land only with 0 lost
cells and 0 worse minima on the 46 cells -- is met, so it ships. M5: the regularized wrapper's own reads
of h charge `evals_values` once per point, not once per row block. Everything else is text and tests.

Same lane conditions as above; the lane binary is built from `a5c29790` with no uncommitted diff
(sha256 `a15b5f090663...`). BIGBANK `sqp-qn` ran at 590 s, every other cell at the caps above.

* `riders-train46-vs-retake16.txt` -- the 46 cells against RETAKE16 through the same comparator: exact
  label 16 / 23, QN label 8 / 23, the same as the implementation commit. 0 lost, 0 worse minima, 0
  counter-only moves on optimal cells. BIGBANK `sqp-qn` is `IDENTICAL` (max_iter/300 at 480 s).
* `riders-train46-vs-head.txt` -- the same 46 cells against the implementation commit's own rows, every
  non-time field: 45 identical or capped in both. The one mover is declared: NET3 `default`'s
  `evals_values` 45 -> 44, the per-point charge (M5; NET3 has 394 equality and 127 inequality rows, and
  one point missed both memo blocks). M1 moves no cell: none of the 46 reaches an exhausted elastic tier
  inside a phase (`riders-target-traces.txt`: depth-1 `elastic 0` on YORKNET, BRITGAS and NET3).
* `riders-target-traces.txt` -- YORKNET, BRITGAS, NET3 (exact label) traced at the rider head: YORKNET
  numerical_error/40 (one switch, first trigger; its end is a main-loop walk failure, as before),
  BRITGAS optimal/62 at 375 s (still beyond the 300 s ceiling), NET3 optimal/23 at 96 s. Their QN-label
  rows are in the 46-cell table: BRITGAS optimal/16, YORKNET numerical_error/1, NET3 max_iter/7 --
  all identical to the implementation commit.
* `riders-suites.txt` -- Release and Debug ctest, 3192 / 3192 each (3194 listed, the 2 tests disabled in
  the tree unchanged; 5 tests added by the riders), the long test included.
* `riders-u0-exact-vs-scl-record.txt` -- the U0 exact arm against `docs/notes/data/2026-10-m7-scl/gates/replay`:
  walk 0, SSN 0, IPM 0 differences.
