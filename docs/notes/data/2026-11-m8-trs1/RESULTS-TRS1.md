# RESULTS-TRS1

Branch `trs1` on m8 `0cecdc6d0`; head after this file is the commit that carries it. Declaration: `DECLARATION-TRS1.md` (+ `-r2`). Evidence in `gate/`.

| leg | result |
|---|---|
| L1 ctest Release | 100% passed, 0 failed of 3987 (`gate/ctest-rel.log`) |
| L1 ctest Debug | 100% passed, 0 failed of 3987 (`gate/ctest-dbg.log`) |
| L2 replay x3 (walk, ssn, ipm; 27 cells, 75 columns) vs the pinned m8-code baseline | differences: 0 in each (`gate/replay-compare.txt`); counters, statuses and residuals all equal |
| L3 golden diff review | `gate/golden-diff.md`: human goldens header/legend only (19 files, 105 legend lines, 27 headers), v1 JSON goldens 6 fixture `iter` lines |
| L4 install smoke + export contract | rc 0, both front ends (`gate/install-smoke.out`) |
| L5 mutations (re-taken, `-r2`) | control clean; M1-M8 all killed (`gate/mutation.out`) |
| CI | see the hand-back |

Test totals: both ctest legs ran 3987 tests (PF-7's gate: 3957; m8 has landed work since).
The IPM replay of the 27-cell corpus was compared against `docs/notes/data/2026-11-m8-pf7/gate/replay/B-*.csv`, the same m8 solver code (PF-7 changes only `get_core_count`, which no counter reads); no fresh base arm was built.
