# DECLARATION-AP4 -- groups G1 (options registry) and G2 (generated options reference)

Committed before the first gate leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is answered by a `DECLARATION-AP4-r2.md`.
hven branch `ap4` at m8 `0cecdc6d0` (base). Each group is its own commit (or small series) and is verified separately; the sibling groups G3-G7 (solve overload, dense helpers, `event_index`, the `violation` rename, docs-only items) are in another worktree and are not part of this declaration.
Design: `.superpowers/api/AP4-design-2026-11-20.md`, sections "Registry (G1/G2)" and "Groups".

## 1. What changes

G1: one X-macro options table per options struct, in declaration order, with the range and a one-line doc per field; `dump_options` (both engines) re-implemented as a walk over the tables; the file-local `kv_*` helpers and the two duplicated `enum_name` overload sets deleted (one exhaustive-switch name table per option enum, per layer); the ten hand-kept field-count constants of `tests/solver/test_options_dump.cpp` replaced by a compile-time arity-and-order check beside the tables.
G2: public `include/hven/solver/options_reference.h` (`OptionTier`, `OptionInfo`, `ipm_option_info()`, `sqp_option_info()`, `write_options_reference()`); the test-side generator `hven_options_reference` and the `options-reference` target; the checked-in `docs/options-reference.md`; its byte-comparison test, the range/enum probe tests and the header-default rider; the `.gitattributes` entry; a migration entry.
Layering: the tables and name tables sit at the layer of the struct they list (core vocabulary, solver `common.`, qp, ipm, sqp), and the two engine row builders live in the engine option TUs, so no `tests/core/test_layering.cpp` allow-list entry is added (the design's single `src/solver/option_enum_names.cpp` holding the ipm and sqp enums would have needed three).

## 2. Criteria (fixed here)

All legs under `box-lane --`, `MKL_NUM_THREADS=1`; wall-clock is not asserted. Arm A = `git archive 0cecdc6d0` + `dep/` (`.scratch/base-src`, `build-A-rel`); arm B = this worktree, Release (`build-rel`) and Debug (`build-debug`), bench + examples ON, SAFER_FAST, clang++ (`legs/build.sh`).

| leg | instrument | criterion |
|---|---|---|
| D0 | `.scratch/capture` (the pre-change dump), run against arm A before any dump code changes | fixtures `tests/solver/fixtures/options_dump/*.txt` = `IpmOptions{}`, the five `ipm_preset`s, `SqpOptions{}`, one all-fields-perturbed value of each (`tests/solver/support/options_perturbed.h`); committed with G1 |
| G1-1 | ctest Release and Debug on arm B at the G1 commit | 100% pass; `OptionsDumpFixtures.*` present and green (the new dump is byte-identical to D0's) |
| G1-2 | `legs/replay.sh` arm A and arm B at the G1 commit | 27 cells x walk, ssn, ipm; `scripts/compare_replay.py` A vs B reads `differences: 0` in each of the three |
| G1-3 | `legs/install_smoke.sh` at the G1 commit | smoke + export contract rc 0 on both front ends (default, clang) |
| G2-1..3 | the same three legs at the G2 commit | the same criteria; ctest adds `OptionsReference.*` green (`CheckedInFileIsCurrent` byte-equal, range probes, rider) |
| G-diff | `git diff` of `src/` between arm A and each group | the dump change touches no solver numerics (read, not a leg: the diff of G1 is the two options TUs, the new tables and name TUs, CMake, tests) |
| CI | push `ap4`, one `gh workflow run ci.yml --ref ap4`, one `gh run watch` | all four lanes green |

## 3. Notes

The replay compares counters and statuses only (the residual-class columns are the calibrated near-ulp gate of `scripts/compare_replay.py`); arm A is built once and serves both groups.
HOLDOUT is never run. The registry changes no solver arithmetic: no evaluation, factorization or iteration code is touched, so an identical replay is the expected outcome and a difference is a defect, not a finding.
