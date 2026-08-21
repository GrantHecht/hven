# Walk-corpus census — Phase 1 census of record, DECLARED PARTIAL

Frozen evidence for the model-contract carve. Produced by
`scripts/run_walk_census.sh` on the corpus binary stamped `0a5b6b94988d`
(schema 37, budget table `0x357aee91dee27391`), replaying the frozen
`bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv`.

## Coverage: 47 of 57 cells

| tier | cells | status |
|---|---|---|
| T1 (6-wide pinned) | 44 | complete |
| T2 (wall-sensitive, solo) | 3 | complete |
| T3 (deep-DNF, full budget) | 10 | **NOT RUN** |

Tier 3 was skipped by OWNER RULING under the strategic-points census policy.
The ten deep-DNF cells are the weakest evidence class for the change under
test -- a behaviour-preserving carve whose primary evidence is a byte-identical
solve trace -- and each costs a full budget. The sweep was terminated once T2
banked its last row; one T3 wave had launched seconds earlier and was killed,
its partial rows DISCARDED rather than recorded.

## Verdict

**0 mismatches over the 47 cells that ran**, on all 13 asserted counter/status
columns. `compare.txt` also carries 10 `MISSING CELL` lines and 0 `EXTRA CELL`:
those ten lines ARE the declared skip, named cell by cell. The comparator's
exit status of 1 is the roster gap alone, not a value disagreement.

`wall_s` is informational and excluded from the comparison by the comparator --
this is a counter replay, not a timing measurement.

## Frozen

This artifact is frozen AS A PARTIAL. It is never completed later and never
appended to. The remaining ten cells are covered by the milestone's close
census, which is a NEW artifact with its own provenance, not an extension of
this one.
