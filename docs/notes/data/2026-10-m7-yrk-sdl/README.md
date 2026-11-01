# M7 W2 YRK + SDL -- the gates run

> Range **`2282f86b..e9cb2472`** on branch `m7` (base `2282f86b` is the RDV + R2-1 record's commit, the previous GREEN gate record).
> Contents: YRK -- the refused-blocker exchange (`83a46517`; `QpCounters::dep_refused_exchanges`) and its riders (`488e8a74`); SDL -- the
> certificate's sign half and the saddle exit's shifted search (`b8da7f69`; `QpCounters::certificate_sign_refusals / saddle_shifted_searches /
> saddle_shifted_directions`), the sign half alone with the terminal-polish sign guard removed (`a0917927`), the declared re-pin of
> `SqpDriverRestorationSeed.TheGuardAndThePhaseRunOnTheCallerScale` (`97324860`) and the riders (`e9cb2472`;
> `QpCounters::terminal_polish_sign_reverts`). The range also holds a docs-only ledger commit (`bc1a245d`).

`wrappers-gates/` is the **declaration** (committed before the first leg, never edited after); `gates/` is the **record**. Derivations:
`wrappers-gates/psym-derivation-yrk-sdl.txt`; pre-flight: `wrappers-gates/preflight-yrk-sdl.txt`; rehearsal: `wrappers-gates/rehearsal/`.

## Declared values (derived from the range or measured by the declaration's rehearsal)

| item | declared |
|---|---|
| changed surface | 3 headers; 4 modified `.cpp`, all inside the P-SYM universe (none outside); 3 added `.cpp`, all outside (`tests/qp/test_qp_{refused_exchange,certificate_sign,saddle_shift}.cpp`, `hven_qp_walk_seam_tests`); 5 added fixtures; 1 comment-only tests header (`tests/sqp/support/ssn_fixtures.h`); no `src/` or `tests/sqp` CMake change |
| G1 / G3 | Debug **3088** / Release **3089** = 3070 / 3071 + 18 added by name, 0 removed; skipped sets equal the baseline's (Debug 7, Release 1) |
| G2 | the long test alone, 1 / 1 |
| G4 | the 43-difference set (walk 17 / ssn 13 / ipm 13) vs the M6 pin, as a SET; exact arm vs the RDV + R2-1 record measured 0 / 0 / 0 (recorded) |
| G5 P-SYM | Release **44** = A 4 + B 40; Debug **44** = A 4 + B 40; identical class A and class B sets in both configurations (no asymmetry); UNEXPLAINED 0; non-debug movers 7; DEBUG_ONLY 0 / 6 |
| G6 | install smoke, export contract, clang-format (11 files), `git diff --check`; fold proofs 46 / 0 |

## Gates (chain pid 684089, worktree `agent-a0943981401bc8337`, declaration `aee4be5b`, 2026-10-18, `gates/chain-yrk-sdl.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**. **GREEN.**

| gate | result |
|---|---|
| G1 Debug suite | 3088 / 3088 (3090 started names = baseline 3072 + 18; skipped = baseline's 7) |
| G2 long test | 1 / 1 |
| G3 Release suite | 3089 / 3089 (3091 started names; skipped = baseline's 1) |
| G4 U0 replay vs the M6 pin | the declared 43-difference set (17 / 13 / 13), measured set EQUALS the declared list. Exact arm vs the RDV + R2-1 record: **0 / 0 / 0** differences (`gates/replay/exact-vs-rdv-r21-record.txt`) |
| G5 P-SYM | Release 44 = 4 + 40, Debug 44 = 4 + 40; UNEXPLAINED 0 both; class A sets identical (`qp/qp_engine` + the three `tests/sqp` sources); class B sets equal (40); sections: non-debug movers = declared 7 both (class A `qp/qp_engine` + class B `core/ledger`, `drivers/sqp_solver`, `globalization/sqp/{mixed_lift,qn_lift_bridge,soc_elastic_restoration}`, `qp/ipqp_engine`), DEBUG_ONLY 0 / 6; manifests 160 / 160 |
| G6 | install smoke, export contract, clang-format (11 files), `git diff --check`, `DIRTY_FILES=0` |

Declared moves, confirmed (all PASS in both configurations, inside the green suites): the 18 added tests (`QpCertificateSign` 10,
`QpYrkRefusedExchange` 4 -- the refused-exchange Yorknet qp25/qp26 Border/Refactorize pair --, `QpSdlSaddleShift` 4; the certificate-sign suite
includes the polish-fallback tests `PolishPremiseTheTerminalPolishIsAdoptedUnarmed`, `PolishedSignFlipFallsBackToACertifiedIncumbent`,
`IncumbentFailingTheSignHalfIsNumericalErrorNotInfeasible` and the DALLASM natural test); the declared re-pins
`SqpDriverRestorationSeed.TheGuardAndThePhaseRunOnTheCallerScale`, `QpEngine.ObjectiveInflatedMultiplierContradictionIsCertifiedOnTheIndependentWorkingSet`
and `QpEngine.T6bObjectiveInflatedMultiplierBehavesTheSameInBothAlgebras` (the a = 1e-3 arm), and
`SsnEngineLocal.StallWindowGuardsKeepABrutallyScaledFeasibleQpHonest`; the five new `QpCounters` fields are folded (fold proofs 46 / 0).
The U0 exact arm is 0 / 0 / 0 as expected; nothing else moved.

Observations, not deviations:
- Release negative control was 117 in the chain against the rehearsal's 120 (the same kind of small difference earlier rounds saw; not asserted as a number).
- Leg wall (informational): G1 9.8 min, G2 2.8, G3 5.4, G4 5.5, G5 33.6, G6 1.4. The gaps before G2 (06:48-06:53) and before G6 (07:43-07:48) are waits on the shared box lock.
- `gates/psym-sections-{Release,Debug}.txt` keep the section checker's summary lines only.
- Every leg printed `PGREP_EMPTY` under the lock hold. At launch a foreign cell binary (another agent's, holding the lock) was alive; the chain waited on the lock and G1 started once it was released.
