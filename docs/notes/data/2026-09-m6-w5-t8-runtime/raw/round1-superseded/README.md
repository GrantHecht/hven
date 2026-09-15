# Round 1's raw and perf captures — SUPERSEDED, retained

Everything under this directory is the **first** (2026-09-11, 12:08–14:06 UTC)
measurement round of W5 T8.9r. **No number in `reading.md` v2 comes from it.**

## Why it was superseded

astra's review (`FIX-ROUND`, SIGNOFF `W5-T8-9R-REVIEW-ASTRA`) found that the
solo condition the round was quoted under was not demonstrated — Important I1:

* the `pgrep` captures were not empty outside the leg's shell, and a command
  *name* cannot say whether a process ran;
* the checks were taken before ROUNDS, not between every A/B alternation;
* `box_pgrep` printed matches and continued — it implemented no pause.

That does not show a co-run happened. It shows the **evidence for solo-ness was
not there**, which under CLAUDE.md §7 is the same thing as not having a
quotable wall number. Settler ruling R2 replaced the process-name proof with a
CPU-time one, and every wall-asserting leg was re-run under it. The round-2
captures are in `raw/` and `perf/` beside this directory.

Round 1's `perf/attribution/` `--dump-qp` probe is superseded for a second and
independent reason (astra I8, settler ruling R5): it does not execute the
post-solve KKT gate or the neutral-path driver construction, so it could not
carry the claim the reading made from it. The eleven-arm attribution in
`../../attribution/` replaced it, and `reading.md` §6 cites that instead.

## What it still is

It is a real, complete, reproducible measurement — 102f729 → e51a7e0 on the
three SQP arms and b9848bf → e51a7e0 on the interior leg, three alternating
rounds each, with every raw CSV and every `perf stat` capture. It is retained
so that the two rounds can be compared, and so that nothing in the record is
deleted to make the second round look tidier than it is. Its own log files are
in `../../logs/round1-superseded/`.

The round-1 comparator that read these files is
`../../comparator-round1-superseded.py` (the version astra reproduced, sha256
`ed45f51f744b33b41ca96954c03b881722ab8a2823e32692b9ad97f9a781a6f2` on its
output). The comparator at the top of the artifact is the fix-round-1 one and
reads `raw/` and `perf/`, not this directory.
