# M6 W6 T6 — the attached-callback / sink cost leg

What an ATTACHED observer costs, measured at one HEAD (`b0a7ffaa`) against
itself. Attached vs unattached, one binary, the arms differing only by a lever.
Not a commit pair; nothing here to keep or revert.

**FIX ROUND 1, 2026-09-14** (the sol review of T6): `PROVENANCE.txt`'s trailing
`FIX ROUND 1` block is the amendment record — the STOP comparator now walks an
exact manifest (**55 comparisons / 1597 cell-comparisons / 0 differences**, was
52 / 1468), the idle proof covers all **34** batches and three wall batches are
marked **NON-QUOTABLE**, the SQP carrier is re-attributed, the measured binary's
hash evidence is committed, and the whitespace exemption is declared by path
pattern. No measured number moved and nothing under `raw/` or `perf/` was
touched. Superseded copies of what changed are in `superseded/`.

It discharges the registration at `docs/notes/2026-08-m6-ledger.md:4455–4457`
("their attached cost is UNMEASURED, registered for W6") under plan
`docs/notes/2026-09-m6-w6-plan.md` §0 J.5 as amended (A5) and §1 W6.T6.

## Read in this order

1. **`PROVENANCE.txt`** — CLAUDE.md §7: the tree, the one build, the toolchain
   and hardware, the serial/solo terms and the idle proof per batch, the
   alternation, the levers and their stamps, the declared omission, and what
   this leg does not measure.
2. **`reading.md`** — the tables and the verdicts.
3. **`comparator.out`** — the tool's own output, verbatim, every figure
   `reading.md` quotes.

## The result in four lines

* **The SQP corpus is FLAT with anything attached** — all three modes, all three
  attached arms, 0 of 27 cells outside 0.99–1.01, corpus inside ±0.15 %.
* **The top-level interior-point leg is MOVED** — instructions **+1.46 %** and
  **+1.71 %** across two independent batches. Outside the ±0.5 % bar.
  **Registered for M7.**
* **The carrier is not the dispatch** — it is the per-event **attached-only
  payload preparation** behind each engine's callback guard (the iterate
  snapshot, the engine→caller mapping, the declared diagnostics), O(n): a few
  hundred instructions per node per event. (fix1 — round 1 said
  `IterationEvent` construction; `reading.md` §4 has the corrected attribution.)
* **Counters byte-identical in all 55 comparisons** — an attached observer
  changed no trajectory.

## What is here

| path | what it is |
|---|---|
| `PROVENANCE.txt` | the §7 stamp, and the protocol in full |
| `reading.md` | the reading: tables, verdicts, the HS re-read, the disposition |
| `comparator.py`, `.sha256` | regenerates every table: `python3 comparator.py --root . --out -` |
| `comparator.out` | that tool's output, regenerated at fix1 |
| `superseded/` | byte-identical copies of the four files fix1 replaced, with their sha256s recorded in `PROVENANCE.txt`; **never edited** |
| `logs/05-binary-hashes.txt` | the measured binary's pre- and post-format sha256 (both `0ef932a8…`), and the fix-round binary's, which is a different one |
| `idle_proof.py`, `.sha256` | a **byte-identical** copy of T8.9r's own tool (`docs/notes/data/2026-09-m6-w5-t8-runtime/scripts/idle_proof.py`), unedited |
| `IDLE-PROOF.md`, `IDLE-PIDS.txt` | its output over this leg's retained batch logs |
| `raw/sqpwall/<mode>/<arm>.csv` | the 27-cell SQP corpus leg, one arm per file — the **wall** reading and the counter byte-identity population |
| `raw/sqpperf/<mode>/r<n>/<arm>-<cell>.csv`, `.stderr` | the single-cell processes the **instruction** reading is taken on; the `.stderr` carries the attached observer's own event counts |
| `raw/ipmleg/`, `raw/ipmleg2/` | the top-level interior-point leg, both batches |
| `raw/hs/` | the HS leg, both arms, three modes |
| `perf/…` | one `perf stat` file per run, laid out to match `raw/` |
| `logs/` | every batch's retained log: the two pgreps, the lock markers, the snapshots, the per-run CPU brackets, the fold proofs |
| `scripts/` | every leg script and wrapper as run, including the fold proof |

### A note on the file naming

The brief asks for `leg-ipm.csv` / `leg-sqp-<mode>.csv` per arm per round. This
directory instead follows **T8.9r's own `raw/` + `perf/` shape**, which the same
brief names as the protocol template, so that `comparator.py` can walk one tree
rather than two and so that no evidence byte is stored twice. The mapping:

* `leg-sqp-<mode>` (wall, per arm) → `raw/sqpwall/<mode>/{a00,a10,a01,a11}.csv`
* `leg-sqp-<mode>` (instructions, per arm per round per cell) →
  `raw/sqpperf/<mode>/r<n>/<arm>-<cell>.csv`
* `leg-ipm` → `raw/ipmleg/{wall,perf}-{off,on}-r<n>.csv` and `raw/ipmleg2/…`
* the HS re-read → `raw/hs/{wall,perf}-<mode>-{off,sink}-r<n>.csv`

The arms are `a00` = nothing attached (the shipped shape, and the denominator of
every ratio), `a10` = attached callback, `a01` = attached sink, `a11` = both; on
the interior leg `off`/`on` (the `HVEN_LEG_COUNT_CALLBACK` lever) and on the HS
leg `off`/`sink` (the `--hs-trace` lever).

## Regenerating

```
python3 comparator.py --root . --out comparator.out     # exits non-zero on any
                                                        # manifest refusal
python3 idle_proof.py --pids IDLE-PIDS.txt --pids-link IDLE-PIDS.txt \
        logs/1*.log logs/2*.log logs/3*.log IDLE-PROOF.md
```

Both were re-run at fix1 and their outputs here are those runs'. The comparator
now enforces an EXACT MANIFEST: every comparison the leg owes is listed in the
script, a missing file is a refusal, and the run checks its own totals (55 /
1597) — falsified by renaming one file in a scratch copy, which gives 54 / 1554,
a named `MANIFEST REFUSAL` and exit 2. `idle_proof.py` exits 1 whenever any
batch is UNPROVEN, which on this leg it always does (see `reading.md` §7).

The counter check shells out to `scripts/compare_replay.py` in the repo; set
`HVEN_REPO` if the checkout is not at `/home/ghecht/Projects/hven`.

## Not measured

The interior leg's **sink** arm (a declared omission — that leg exposes no sink
lever and this task added none; see `PROVENANCE.txt` and `reading.md` §3);
anything on Apple or Windows (**UNOBSERVED**); any commit pair; and an observer
that does real work — both observers here are the cheapest forms that can be
written, because the question is what *attaching* costs.
