# The 57-cell walk census — all three tiers, the 13 M5-discharged cells INCLUDED

**What this is.** `scripts/run_walk_census.sh` at its defaults, replaying the
committed 57-cell walk corpus on the corpus binary this gate built, and comparing
the **13 asserted counter/status columns** against
`bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv` — the baseline of record
since the U0 flag-unification event, read-only and never rewritten.

**Why all 57.** M5's close discharged 13 cells (3 wall-sensitive + 10 deep-DNF)
"**for the M5 close**", with a standing trigger: "they are run if any of their
cells' surfaces is touched by a future change, or at an owner-called full census"
(`docs/notes/2026-08-m5-ledger.md:652-656`). M6 touched those surfaces — W0.2's
scaling layer, W0.3's export-boundary sign sweep, W2's elastic/fallback body and
restoration seed, W5 T6's TU cuts in `solve_impl_body`, and T8.10's 206-file
rename — so the trigger fired and the brief's §0 ruled the 13 RUN.

**The comparator is BYTE-EXACT.** `scripts/census_compare.py` at its default:
exact string equality on all 13 asserted columns, `--residual-gate` NOT passed.
That is the M5 gate's own bar (`m5-ledger:630-635` read byte identity on all 44).

**The protocol** (declared, CLAUDE.md §7 — and stamped by the runner into the
merged CSV's own provenance header): parallel-tiered v1,
T1 44 cells 6-wide pinned / T2 3 cells SOLO / T3 10 cells 5-wide at FULL budget;
tiers assigned from the FROZEN baseline, not from this run's outcomes;
`MKL_NUM_THREADS=1` in every worker; every worker `taskset`-pinned to one logical
CPU of a DISTINCT physical core, SMT siblings never both used; order T1 → T2 → T3
with tier 2 starting only after every tier-1 worker was reaped.

**`wall_s` is excluded by the comparator and is never quoted.** Nothing in this
directory is a timing measurement.

**A deviating cell is a CANDIDATE, not a regression**, until it has been re-run
ALONE and still disagrees (`run_walk_census.sh:40-44`). `serial_confirm_list.txt`
is the runner's list of such candidates.

**The schema gap, so a reader is not surprised.** The frozen baseline is
**schema 37 / 36 columns**; this run's corpus binary writes **schema 76**.
`census_compare.py` truncates every row to the first 13 fields
(`census_compare.py:152-167`), so the asserted comparison is unaffected and
nothing needed re-derivation. Both files carry the same
`budget_table_hash: 0x357aee91dee27391`.

## Files

| file | what it is |
|---|---|
| `walk_census.csv` | the merged 57-row fresh CSV, with the runner's protocol/provenance header prepended |
| `compare.txt` | `census_compare.py` output: the verdict |
| `serial_confirm_list.txt` | the runner's candidate list (empty of non-comment lines when nothing deviated) |
| `run.log` | the runner's own timestamped log: tier table, per-cell start/ok lines, tier boundaries |
| `score_gates.txt` | the merge/score step's output |
| `provenance_header.txt` | the header the runner prepended to the merged CSV |
| `rows/` | every cell's own CSV row file, as produced |
| `logs/` | every cell's stdout/stderr |
| `census-leg.log` | the box-protocol wrapper log (pgrep markers, LEG_RC, WRAPPER_EXIT) |

## The 13 cells M5 discharged, run here

Recoverable exactly from the frozen baseline's own tier table,
`bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv:95-107` (the facts file
cites `:94-105`; that is off by one at the start and stops two rows short — see
the report's errata):

*T2 — 3 wall-sensitive, SOLO:* `f7_n10000_path_physics`,
`f7_n5000_path_neutral`, `f7_n5000_path_warm`.

*T3 — 10 deep-DNF, full budget each:* `f7_n10000_path_activity`,
`f7_n10000_path_corrupted`, `f7_n10000_path_neutral`, `f7_n10000_path_warm`,
`f7_n20000_path_activity`, `f7_n20000_path_corrupted`, `f7_n20000_path_neutral`,
`f7_n20000_path_physics`, `f7_n20000_path_warm`, `f7_n5000_path_corrupted`.

Their per-cell results are in the report and in `compare.txt`.
