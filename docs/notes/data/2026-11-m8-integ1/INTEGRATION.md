# integ1: PF-4 + TR-S1 + AP-4 + AP-4b stacked on m8 (81052a7bb)

Order: pf4-r2 (62e370ed2, already on base), trs1 (717fd7c8b), ap4 (7682d90e0), ap4b (447f13a9e). Commits are cherry-picked
unchanged except where noted; authorship preserved.

## Conflicts and re-derivations
| Where | Conflict | Resolution |
|---|---|---|
| CHANGELOG.md (trs1) | TR-S1 entry vs I18 entry | both kept |
| tests/trace/golden/trace_v1/sqp_complete.jsonl (trs1) | PF-4 added counter keys on the solve.end rows; TR-S1 changed the iter rows | re-derived: TR-S1's iter rows (scaled stat/feas/comp + `_caller` keys) with PF-4's solve.end rows |
| src/CMakeLists.txt (ap4, twice) | `_hven_expected_source_count` | re-derived from the tree: 61 (PF-4 head) + 3 (G1) + 1 (G2) = 65; ap4b adds no src file; configure's tripwire passes |
| docs/build.md | AP-4 never recorded its counts | history lines 62-65 appended (commit "docs(build): record source counts") |
| tests/solver/CMakeLists.txt (ap4b) | test_options_reference.cpp vs test_solve_overloads.cpp | both listed |
No other textual conflicts (docs/migration-0.2.md, trace/counter/options/layering pins merged cleanly and pass).
docs/options-reference.md regenerates byte-identically (no change needed). No `.violation` user from TR-S1 needed G7's rename.

## Proof (head: see hand-back)
| Check | Result |
|---|---|
| Release build + ctest | 4036 tests, 0 failures |
| Debug build + ctest | 4036 tests, 0 failures |
| Replay 27 cells x walk/ssn/ipm, stack vs PF-4-only build at 62e370ed2 | 0 differences x3 (75 columns each); `gate/replay-compare.txt` |
| check_install_smoke.sh, default and clang front ends | pass |
| check_export_contract.sh, both | OK (linux, align 32) |
| OptionsReference tests / `options-reference` target | pass; regenerated file identical |
Legs: `legs/`. CSVs: `gate/replay/`. The 3-branch intermediate (before ap4b) was also green on all of the above (ctest 4019).
