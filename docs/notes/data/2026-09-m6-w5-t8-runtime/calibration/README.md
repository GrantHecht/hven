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
It is a further reason, not a new one.

**AND TWO CLAIMS IN THAT PARAGRAPH ARE NARROWED AT FIX2 (astra's fix1 review,
item 8).**

*(1) The fix1 round's own exceedances are listed HERE, because they were not
listed in `reading.md` §5 as this file said they were.* On the WALL (pass A)
captures, three cells exceed 0.5 %:

| combination / arm / round | HS cell | `median_se_pct` |
|---|---|---|
| `ipm`/`sink`, head, r2 | 79 | **1.030663 %** |
| `ssn`/`off`, head, r2 | 25 | **0.808699 %** |
| `walk`/`off`, head, r2 | 30 | **0.618392 %** |

Eight more sit in the pass-B captures (`ipm`/off base-r1 HS28 0.825153 %;
`ipm`/sink head-r3 HS79 0.756542 %; `ssn`/sink base-r1 HS40 0.530673 %;
`walk`/off base-r1 HS12 0.528402 %, base-r2 HS39 0.557926 %, head-r3 HS24
0.895165 %; `walk`/sink base-r1 HS76 **1.380392 %**, head-r1 HS40 0.503930 %).
Every one is a single HS cell inside a 27-cell corpus, and none of them is quoted
as a measurement anywhere: §11.3 (ii) reads this leg's wall only as the paired
A/B ratio, and §4 gives it no instruction verdict.

*(2) "N cannot be raised to fix it" is a CONJECTURE from the between-process
evidence, not a result.* What is measured is a BETWEEN-process constant — three
runs of the same binary disagree by up to 1.4 % at the walk corpus level, and
`--repeat` inside one process cannot average across processes. **That does not
establish the cause of a WITHIN-process `median_se_pct` exceedance, and it does
not prove that a larger N could not reduce one**: `median_se_pct` is the
dispersion of the repeats INSIDE one process, which is exactly the quantity N is
supposed to shrink. No larger-N experiment was run here to test it. The claim
that stands is the narrow one — the leg's ABSOLUTE wall figure cannot resolve
the 0.5 % effect the veto turns on — and it rests on the between-process spread,
which is measured.
