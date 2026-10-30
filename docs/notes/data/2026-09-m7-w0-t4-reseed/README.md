# 2026-09-m7-w0-t4-reseed

The M7 W0 T4 measurement: `SqpOptions::ipqp_reseed_after_disproval`, both states, on
the one cell in this repository that DISPROVES a tier suspicion at all — constructed
for this task, because no frozen artifact in `docs/notes/data/` moves
`ipqp_suspicion_disproved` off zero.

* `PROVENANCE.txt` — the CLAUDE.md §7 stamp: tree, machine, protocol, what is
  asserted, the per-leg concurrency, and why the replay has two arms.
* `reading.md` — the reading and the disposition. **Start here.**
* `raw/both-states-3reps.log` — three replicates, both toggle states, four
  `max_iter` budgets (Release). Every counter column byte-identical across the
  three replicates. The log's own header carries `git rev-parse HEAD`,
  `git describe --always --dirty` and an EMPTY `git status --porcelain`, so the
  tree identity is in the artifact rather than only in this README.
* `raw/debug-both-states.log` — the Debug replicate, same header. Its counter
  columns are byte-identical to the Release log's.
* `raw/no-cell-disproves.txt` — the search that justifies "constructed": every
  frozen artifact carrying an `ipqp_suspicion_disproved` column, and the U0
  replay's tier-escape totals, read mechanically rather than asserted.
* `raw/psym-same-path/` — the same-path P-SYM arm, both configs, with every
  differing object classified.
* `raw/gates/` — the gate transcripts: both suites both configs, the >600 s Debug
  test run alone, the replay (both arms) and its comparison record, the install
  smoke, the export contract, the clang-format before/after comparison, and the
  gate leg's own transcript with its box-protocol markers.
* `perf/both-states-perf.log` — instruction counts, one process per toggle state,
  plus the process-startup baseline. INFORMATIONAL.

**No wall-clock number was taken.** The measurement's CSV has no wall column, so
there is nothing here that could be misread as a timing.

The cell itself is `tests/sqp/support/disproved_entry_vehicle.h` and its pins are
`tests/sqp/test_disproved_entry.cpp`; both are test-support, not corpus cells, and
add no CSV column and no schema move.
