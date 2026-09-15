# 2026-09-m7-w0-t3-elastic-rung

The M7 W0 T3 measurement: the elastic ladder's rung A solved by the WALK (the shipped
default) against the interior-point TIER, on a constructed F7 family built for this task
because no natural F7 population in hven fires the rung at all.

* `PROVENANCE.txt` — the CLAUDE.md §7 stamp: tree, machine, protocol, what is asserted.
* `reading.md` — the reading and the disposition. Start here.
* `raw/both-states-3reps.log` — three replicates, both toggle states, N ∈ {100,200,400,800}.
  Every counter column is byte-identical across replicates.
* `raw/step2a-debug.log` — the Debug replicate (two sizes), counters identical to Release.
* `perf/both-states-perf.log` — instruction counts, one process per cell, plus the
  process-startup baseline. INFORMATIONAL.

The cell itself is `tests/sqp/support/elastic_rung_vehicle.h`; it is a test-support fixture,
not a corpus cell, and adds no CSV column and no schema move.
