# M7 W2 SDL-4 + SDL-6 -- the gates run

> Range **`995439fb..df416f71`** on branch `m7` (base `995439fb` is the YRK + SDL record's commit, the previous GREEN gate record).
> Contents: SDL-4 -- the certificate's bound half never reads kInfeasible (`29a400d4`; `QpCounters::certificate_bound_refusals /
> terminal_polish_bound_reverts`, declared re-pin `QpDegeneracyArm.CertificateRefusesABoundViolationOutsideTheStructuralMargin`
> kInfeasible -> kNumericalError), the SQP driver's routing of a bound-half refusal (`915123c7`) and the riders (`99c41aee`); SDL-6 -- the
> certificate's row half reads kInfeasible only on a verified Farkas certificate (`ae0d31cc`; `QpEngine::verify_terminal_farkas`,
> `detail::terminal_null_direction`; `QpCounters::certificate_row_refusals / terminal_farkas_certificates`) and its riders (`df416f71`).

`wrappers-gates/` is the **declaration** (committed before the first leg as `8a7b08e3`, never edited after); `gates/` is the **record**.
Derivations: `wrappers-gates/psym-derivation-sdl4-sdl6.txt`; pre-flight: `wrappers-gates/preflight-sdl4-sdl6.txt`; rehearsal:
`wrappers-gates/rehearsal/`.

## Declared values (derived from the range or measured by the declaration's rehearsal)

| item | declared |
|---|---|
| changed surface | 4 headers; 8 modified `.cpp` (3 library, 2 `hven_sqp_tests` inside the P-SYM universe; 3 outside: `tests/qp/test_qp_{certificate_sign,degeneracy_arm}.cpp` in `hven_qp_walk_seam_tests`, `tests/sqp/test_sqp_ladder_seams.cpp` in `hven_sqp_seam_tests`); 0 added `.cpp`; 4 added fixtures; no CMake change |
| G1 / G3 | Debug **3105** / Release **3106** = 3088 / 3089 + 17 added by name, 0 removed; skipped sets equal the baseline's (Debug 7, Release 1) |
| G2 | the long test alone, 1 / 1 |
| G4 | the 43-difference set (walk 17 / ssn 13 / ipm 13) vs the M6 pin, as a SET; exact arm vs the YRK + SDL record measured 0 / 0 / 0 (recorded) |
| G5 P-SYM | Release **44** = A 5 + B 39; Debug **44** = A 5 + B 39; identical class A and class B sets in both configurations; UNEXPLAINED 0; non-debug movers 8; DEBUG_ONLY 0 / 6 |
| G6 | install smoke, export contract, clang-format (12 files), `git diff --check`; fold proofs 46 / 0 |

## Gates (chain pid 1204464, worktree `agent-af8f96009574734c6`, declaration `8a7b08e3`, 2026-10-18, `gates/chain-sdl4-sdl6.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**. **GREEN.**

| gate | result |
|---|---|
| G1 Debug suite | 3105 / 3105 (3107 started names = baseline 3090 + 17; skipped = baseline's 7) |
| G2 long test | 1 / 1 |
| G3 Release suite | 3106 / 3106 (3108 started names; skipped = baseline's 1) |
| G4 U0 replay vs the M6 pin | the declared 43-difference set (17 / 13 / 13), measured set EQUALS the declared list. Exact arm vs the YRK + SDL record: **0 / 0 / 0** differences (`gates/replay/exact-vs-yrk-sdl-record.txt`) |
| G5 P-SYM | Release 44 = 5 + 39, Debug 44 = 5 + 39; UNEXPLAINED 0 both; class A sets identical (`drivers/sqp_solver`, `qp/qp_engine`, `qp/ws_independence`, `tests/sqp/test_{qp_engine,sqp_solver}`); class B sets equal (39); non-debug movers = declared 8 both, DEBUG_ONLY 0 / 6; manifests 160 / 160; the three outside sources built in exactly one listed outside target each, no outside object in either manifest |
| G6 | install smoke, export contract, clang-format (12 files), `git diff --check`, `DIRTY_FILES=0` |

Declared moves, confirmed (all PASS in both configurations, inside the green suites): the 17 added tests (`QpCertificateSign` 9, `QpEngine.RowHalfInfeasibleVerdictsCarryAVerifiedCertificate`, `SqpLadderSeam` 7); the declared
re-pin `QpDegeneracyArm.CertificateRefusesABoundViolationOutsideTheStructuralMargin` (kInfeasible -> kNumericalError); the pinned DALLASM-QN qp0
kInfeasible with a certificate counter in both algebras and `T7ACertifiedInfeasibleExit` (verified certificate and its solves). The new
`QpCounters` fields are folded (fold proofs 46 / 0). The U0 exact arm is 0 / 0 / 0: none of the 27 U0 cells moves a single column against the
previous record, so no cell of the replay ends at the certificate's bound or row half; nothing in G4 or G5 moved beyond the declaration.

Observations, not deviations:
- Negative control was 120 (Release) and 85 (Debug) in the chain, equal to the rehearsal's.
- Leg wall (informational): G1 10.0 min, G2 2.9, G3 5.4, G4 5.6, G5 34.2, G6 1.4. Every leg printed `PGREP_EMPTY` under the lock hold and the lock was never contended.
- `gates/psym-sections-{Release,Debug}.txt` keep the section checker's summary and mover lines only.
- The rehearsal ran at a temporary docs-only commit `d9e9d54f` (the wrappers, above `df416f71`), reset away before the declaration was committed; its log records that hash.
