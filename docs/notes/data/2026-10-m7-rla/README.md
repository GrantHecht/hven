# M7 W2 RLA -- the gates run

> Range **`0feb82aa..b79a7daf`** on branch `m7` (base `0feb82aa` is the SNX + SNR record's commit, the previous GREEN gate record).
> Contents: RLA -- the latch and its reason (`c926e044`; `detail::take_latch`, `LatchReason` kCap / kConditioning, `detail::latch_still_holds`,
> `SchurComplement::near_null_direction / dominant_direction`; `QpCounters::border_latches_cap / border_latches_conditioning / border_latch_releases /
> border_latch_holds` appended; new `tests/qp/test_qp_latch_reason.cpp`, fixture `RLA_BRITGAS_qp0.txt`), round 2 (`fe0172e5`, `0e3554c6`), its riders (`b79a7daf`);
> `6f12bec2` (ledger catch-up, docs only).

`wrappers-gates/` is the **declaration** (committed before the first leg as `025ed4b2`, never edited after); `gates/` is the **record**.
Derivations: `wrappers-gates/psym-derivation-rla.txt`; pre-flight: `wrappers-gates/preflight-rla.txt`; rehearsal: `wrappers-gates/rehearsal/`.

## Declared values (derived from the range or measured by the declaration's rehearsal)

| item | declared |
|---|---|
| changed surface | 3 headers (`core/solver_counters.h`, `detail/kkt/schur_complement.h`, `detail/qp/qp_engine.h`); 3 modified `.cpp`, all inside the P-SYM universe (library `src/qp/qp_engine.cpp`; `hven_sqp_tests` `test_hs_battery.cpp`, `test_qp_engine_border.cpp`); 1 added `.cpp` outside the universe (`tests/qp/test_qp_latch_reason.cpp`, in `hven_qp_walk_seam_tests`); 1 added fixture; one CMake line (`tests/CMakeLists.txt` +1) |
| G1 / G3 | Debug **3137** / Release **3138** = 3117 / 3118 + 20 added by name, 0 removed; skipped sets equal the baseline's (Debug 7, Release 1) |
| G2 | the long test alone, 1 / 1 |
| G4 | the 43-difference set (walk 17 / ssn 13 / ipm 13) vs the M6 pin, as a SET; exact arm vs the SNX + SNR record measured 0 / 0 / 0 (recorded) |
| G5 P-SYM | Release **43** = A 3 + B 40; Debug **43** = A 3 + B 40; the same 3 class A and the same 40 class B objects in both; UNEXPLAINED 0; non-debug movers 7; DEBUG_ONLY 0 / 8 |
| G6 | install smoke, export contract, clang-format (7 files), `git diff --check`; fold proofs 46 / 0 |
| declared moves | `HsBattery.CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions` factorizations 287/288 -> 275/276 (`c926e044`, `0e3554c6`), minors and answers unchanged, Release and Debug agree; the new counter keys `border_latches_cap`, `border_latches_conditioning`, `border_latch_releases`, `border_latch_holds`; no U0 column added |

## Gates (chain launched 2026-10-20 12:44, worktree `agent-a1a802c0c4c085738`, declaration `025ed4b2`, `gates/chain-rla.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**. **GREEN.**

| gate | result |
|---|---|
| G1 Debug suite | 3137 / 3137 (3139 started names = baseline 3119 + 20; skipped = baseline's 7) |
| G2 long test | 1 / 1 |
| G3 Release suite | 3138 / 3138 (3140 started names; skipped = baseline's 1) |
| G4 U0 replay vs the M6 pin | the declared 43-difference set (17 / 13 / 13), measured set EQUALS the declared list. Exact arm vs the SNX + SNR record: **0 / 0 / 0** differences over 27 cells x 75 columns (`gates/replay/exact-vs-snx-snr-record.txt`) |
| G5 P-SYM | Release 43 = 3 + 40, Debug 43 = 3 + 40; UNEXPLAINED 0 both; class A sets identical (`qp/qp_engine`, `tests/sqp/test_hs_battery`, `tests/sqp/test_qp_engine_border`); class B sets equal by name (40), the six library objects `core/ledger`, `drivers/sqp_solver`, `globalization/sqp/{mixed_lift,qn_lift_bridge,soc_elastic_restoration}`, `qp/ipqp_engine`; non-debug movers = declared 7 both, DEBUG_ONLY 0 / 8; manifests 160 / 160; the added outside source built in exactly one listed outside target, no outside object in either manifest |
| G6 | install smoke, export contract, clang-format (7 files), `git diff --check`, `DIRTY_FILES=0` |

Every move the range makes was declared and PASSED in both configurations inside the green suites: the 20 added `QpLatchReason` tests and the re-pinned
`HsBattery` factorization counts. The new counter keys are folded (fold proofs 46 / 0). None of the 27 U0 cells moves a single column against the previous record
(exact arm 0 / 0 / 0); nothing in G4 or G5 moved beyond the declaration.

Observations, not deviations:
- Negative control was 122 (Release) and 83 (Debug) in the chain; the declaration's rehearsal read 123 (Release). Both are controls, not gated counts; class A / class B sets and UNEXPLAINED 0 are identical.
- Leg wall (informational): G1 8.7 min, G2 2.9, G3 5.4, G4 5.5, G5 35.1, G6 1.4. Every leg printed `PGREP_EMPTY` under the lock hold.
- `gates/psym-sections-{Release,Debug}.txt` keep the section checker's summary and mover lines only.
- The chain ran to completion; the record was assembled afterwards by a second agent (the first stopped on a usage limit) from the chain's own outputs, with no leg re-run.
