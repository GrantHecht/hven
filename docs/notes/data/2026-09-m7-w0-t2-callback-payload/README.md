# M7 W0 T2 — the interior attached-callback cost under a payload mask

What the `IterationPayload` mask bought, measured at one HEAD (`1133570f`)
against itself under **M6 W6 T6's own protocol**, on the leg T6 called MOVED.
Four arms, one binary, the arms differing only by a lever. Not a commit pair;
nothing here to keep or revert.

It answers two of T6's three open items
(`docs/notes/data/2026-09-m6-w6-attached-cost/reading.md` §9): the top-level
interior-point leg's **+1.46 %/+1.71 %** attached-callback cost, registered for
M7 as **D24**, and that leg's **missing sink arm**, T6's declared omission.

## Read in this order

1. **`PROVENANCE.txt`** — CLAUDE.md §7: the tree, the one build and its hash,
   the toolchain and hardware, the serial/solo terms and the idle proof, the
   alternation, the arms and their levers, the three batches and the one
   discarded one, and what this leg does not measure.
2. **`reading.md`** — the tables, the attribution leg, the verdicts and the
   disposition.
3. **`comparator.out`** — the tool's own output, verbatim, every figure
   `reading.md` quotes.

## The result in six lines

* **The mask removes 56–57 % of the measured attached cost.** A cheap callback
  at `kAll` costs **+1.376 %/+1.401 %** instructions over the whole 43-row
  interior leg (T6's finding, reproduced); the same callback at `kScalars`
  costs **+0.588 %/+0.618 %**.
* **What it removes is O(n)**, confirmed on the leg's smallest and largest cell
  populations: the saving rises **10.2×** for a **13.5×** rise in problem width.
  That is the attached-only payload preparation T6 named.
* **`a-scalars` is NOT FLAT** — it lands between 0.5 % and 1 %. Reported as the
  finding it is; nothing was tuned toward the bar.
* **The residual does not scale with the problem** (2.94e6 instructions/event at
  mean `x` = 1 088 against 2.54e6 at 14 647), so no wider mask reaches it.
  `reading.md` §5 names the one candidate it can and says what it cannot split.
* **T6's declared omission is closed and the interior sink is FREE** —
  `a-sink`, 538 events, **+0.043 %/+0.039 %**: FLAT.
* **Counters byte-identical in all 32 comparisons** (42 750 cell-comparisons,
  0 differences). A declared payload changed no trajectory.

## What is here

| path | what it is |
|---|---|
| `PROVENANCE.txt` | the §7 stamp, and the protocol in full |
| `reading.md` | the reading: tables, the attribution leg, the disposition |
| `comparator.py`, `.sha256` | regenerates every table: `python3 comparator.py --root . --out -` |
| `comparator.out` | that tool's output |
| `idle_proof.py`, `.sha256` | a **byte-identical** copy of W6 T6's own copy of T8.9r's tool, unedited |
| `IDLE-PROOF.md`, `IDLE-PIDS.txt` | its output over this leg's twelve retained batch logs |
| `raw/ipmleg/`, `raw/ipmleg2/`, `raw/ipmleg3/` | the interior leg, three complete batches, four arms × three rounds |
| `raw/ipmlegwall/` | the interior leg's one wall batch (informational, and **NON-QUOTABLE** — see `reading.md` §2) |
| `raw/attrib/{small,large}/` | the attribution leg: the same four-arm question on the leg's smallest and largest cell populations |
| `raw/sqpperf/walk/r1/` | the SQP corpus walk-mode check, 27 single-cell processes × three arms |
| `raw/sqpperf/walk/schema-header.csv` | one untimed public-leg run of the same binary, used for its header line alone (the single-cell runs write bare rows) |
| `perf/…` | one `perf stat` file per run, laid out to match `raw/` |
| `logs/` | every batch's retained log: the two pgreps, the lock markers, the snapshots, the per-run CPU brackets, the fold proof, the manifest falsification |
| `scripts/` | every leg script and wrapper as run, including the fold proof |
| `discarded/` | the SQP walk group 23–27's FIRST attempt, killed mid-window by the harness; retained unedited, read by nothing (`PROVENANCE.txt`) |

The arms are `a00` = nothing attached (the shipped shape, and the denominator of
every ratio), `a-all` = the counting callback at `IterationPayload::kAll`
(T6's arm), `a-scalars` = the same callback at `kScalars` (the remedy), and
`a-sink` = the counting trace sink alone (the arm T6 could not take).

## Regenerating

```
python3 comparator.py --root . --out -          # exits 2 on any manifest refusal,
                                                # 1 on any counter difference
python3 idle_proof.py --pids IDLE-PIDS.txt --pids-link IDLE-PIDS.txt \
        logs/2*.log logs/3*.log logs/5*.log IDLE-PROOF.md
```

The comparator enforces an EXACT MANIFEST: every comparison the leg owes is
listed in the script, a missing file is a refusal, and the run checks its own
totals (32 / 42 750). Falsified by removing two files in a scratch copy, which
gives 30 / 38 700, two named `MANIFEST REFUSAL`s and exit 2
(`logs/43-manifest-falsification.txt`). `idle_proof.py` exits 1 whenever any
batch is UNPROVEN, which on this leg it always does — see `PROVENANCE.txt`.

The counter check shells out to `scripts/compare_replay.py` in the repo; set
`HVEN_REPO` if the checkout is not at `/home/ghecht/Projects/hven`.

## Not measured

The SQP corpus in `ssn` or `ipm` mode (the walk-mode check is what the brief
scopes here); the HS leg; anything on Apple or Windows (**UNOBSERVED**); any
commit pair; an observer that does real work; and the residual's split between
its candidates, which needs an instrument this task deliberately does not build
(`reading.md` §5).
