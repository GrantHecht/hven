# The leg-2 `--repeat` calibration, and the leg-1 / interior timing survey

**Retained in full at fix round 1 (astra I3).** Round 1 kept only the derived
`calibration.csv` and the transcript in `logs/L3-calibration.log`; the 15
per-cell CSVs and the 8 survey CSVs stayed in scratch, so the artifact was not
self-contained. They are here.

* `hs-<mode>-N<N>.csv` / `.out` — 15 files: per-cell `median_se_pct` at
  N = 125, 250, 500, 1000, 2000 for `ipm`, `walk`, `ssn`, on the A arm
  (102f729) alone, solo, `taskset -c 2`, threads 1, warm-up 1, trace off.
  §11.3 (i) fixes `median_se_pct` — not `spread_pct` — as the column N is
  raised against.
* `survey-<mode>-<arm>.csv` / `.out` — 6 files: one full U0 27-cell run per
  mode per arm, which is what sized every later leg.
* `survey-interior-<arm>.csv` / `.out` — 2 files: one full interior run per arm.
* `calibration.csv` — the derived table the reading quotes.

**ADOPTED N: ipm 1000, walk 1000, ssn 2000** — the counts §11.3 records for the
lane, reached independently here. Worst adopted `median_se_pct`: ipm 0.354782 %,
walk 0.419009 %, ssn 0.390189 %.

**THE CALIBRATION WAS NOT RE-RUN AT FIX ROUND 1, AND THAT IS DELIBERATE.**
astra's I1 lists the wall-ASSERTING legs — leg 1's wall, leg 2, the interior
leg, and I4's per-cell perf — and the calibration asserts no wall number of its
own. What it produces is N, and N is a property of the A arm's dispersion, not
of the window it was measured in; it is identical across arms by construction
and the same three counts are used in round 2. Its own elapsed times are
recorded in `logs/round1-superseded/L3-calibration.log` and are quoted nowhere.

**AND THE EXCEEDANCES ARE NOT HIDDEN (astra §3).** `median_se_pct` at the
adopted N is the WORST PER-CELL value on the calibrating run; later runs of the
same cells at the same N do exceed 0.5 % — `ipm`/`sink` base-r2 HS5 reached
1.027244 % and `walk`/`sink` head-r1 HS3 reached 1.739174 % in round 1. That is
the population being bimodal per process, which §11.3 (ii) already records as
the reason leg 2's wall is informational and read only as the paired A/B ratio.
It is a further reason, not a new one: **N cannot be raised to fix it**, because
the term is a per-process constant that `--repeat` cannot average away at any N.
The round-2 exceedances are listed in `reading.md` §5.
