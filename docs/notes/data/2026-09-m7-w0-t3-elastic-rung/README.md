# 2026-09-m7-w0-t3-elastic-rung

The M7 W0 T3 measurement: the elastic ladder's rung A solved by the WALK (the shipped
default) against the interior-point TIER, on a constructed F7 family built for this task
because no natural F7 population in hven fires the rung at all.

* `PROVENANCE.txt` — the CLAUDE.md §7 stamp: tree, machine, protocol, what is asserted.
* `reading.md` — the reading and the disposition. Start here.
* `raw/both-states-3reps.log` — three replicates, both toggle states, N ∈ {100,200,400,800},
  RE-TAKEN 2026-09-16 at commit `40ef105f` on a clean tree (fix round 1). Every counter column
  is byte-identical across replicates AND to the first measurement.
* `raw/2026-09-15-both-states-3reps.log` — the first measurement, kept whole.
* `raw/debug-both-states.log` — the Debug replicate at the same commit (two sizes, both arms),
  counters identical to Release; `raw/step2a-debug.log` is step 2a's walk-only Debug log.
* `perf/both-states-perf.log` — instruction counts, one process per cell, plus the
  process-startup baseline. INFORMATIONAL. `perf/perf-raw.txt` is `perf stat`'s own output,
  byte-verbatim; `perf/2026-09-15-both-states-perf.log` is the first measurement's.

Fix round 1 (2026-09-16) added the evidence the step-2b round asserted but did not retain:

* `raw/decline-diagnostic.log` — per cell, the classification of every declined tier rung
  (3 of 5 at every size, all `kInfeasibleSuspect`), read off a sink attached to the ELASTIC
  engine alone. The patch is in `reading.md` §5 and was reverted.
* `raw/ipqp-max-iter-{default,200,600}.csv` and the three `*.diff` — the budget probe behind
  "raising `ipqp_max_iter` moves no counter". All three diffs are empty.
* `raw/post-revert-verify.log` — the same cells re-read after the diagnostic patch was
  reverted; the counters reproduce.
* `raw/psym-same-path/` — the same-path P-SYM arm, both configs, with every differing object
  classified and `ipqp_engine.cpp.o` explained (it constructs an `SqpOptions` on the stack).
* `raw/gates/` — the gate transcripts: both Release suites, the four Debug suites, the install
  smoke, the export contract, and the clang-format before/after comparison.

The cell itself is `tests/sqp/support/elastic_rung_vehicle.h`; it is a test-support fixture,
not a corpus cell, and adds no CSV column and no schema move.
