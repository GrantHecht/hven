# TR-S1 golden diff review (base 0cecdc6d0 -> trs1 head)

## Human console goldens (`tests/trace/golden/human_trace/*.txt`, 35 files)

- files changed: 19; legend lines removed: 105; column-header lines renamed (`stat feas comp` -> `stat_scl feas_scl comp_scl`): 27; files with any OTHER difference: 0 []

- Every golden solve is UNSCALED (the human fixtures run default options), so no row value changes: the printed measures equal the tested ones there. The numbers were deliberately not regenerated (only the structural edit was applied to the base text), so the diff is header and legend only.

## v1 JSON goldens (`tests/trace/golden/trace_v1/*.jsonl`)

- `ipm_aborted.jsonl`: 1 `iter` lines re-derived (the fixture records carry distinct tested/caller values, so the core keys changed value and the three `_caller` keys carry the former ones); every other line byte-identical.

- `ipm_complete.jsonl`: 2 `iter` lines re-derived (the fixture records carry distinct tested/caller values, so the core keys changed value and the three `_caller` keys carry the former ones); every other line byte-identical.

- `sqp_complete.jsonl`: 3 `iter` lines re-derived (the fixture records carry distinct tested/caller values, so the core keys changed value and the three `_caller` keys carry the former ones); every other line byte-identical.


Reading of one fixture line (`ipm_complete.jsonl`, iteration 0): before `"stationarity":0.5,"feasibility":0.25,"complementarity":0.125`; after `"stationarity":4,"feasibility":0.25,"complementarity":1,... "stationarity_caller":0.5,"feasibility_caller":0.25,"complementarity_caller":0.125` (fixture objective scale 8: the tested measures are 8x the caller's for the two dual-scaled keys, feasibility equal).

## Inline goldens in test sources

- `tests/trace/test_console_sink.cpp`: the two iteration-rung transcripts lose the legend and take the new header; `ConsoleRows.TheColumnHeadersAreTheSpecsText` takes the new headers; `ConsoleLadder.TheLegendFlagSuppressesOnlyTheLegend` is deleted with the flag.
- `tests/trace/test_json_lines_trace_sink.cpp`: ten hand-built `iter` golden lines gain the three `_caller` keys after `factorizations` (the records set tested == caller, so the values repeat); `SqpIterIsInCallerUnitsOnAScaledSolve` becomes `SqpIterCarriesTestedAndCallerUnitsOnAScaledSolve` (HS25, scaled: core keys == `tested_*`, `_caller` keys == the caller-unit fields, and a non-vacuity check that they differ).
