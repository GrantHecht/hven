# M7 W2 RDV + R2-1 -- the gates run

> Range **`bdb1a82a..a9f01851`** on branch `m7` (base `bdb1a82a` is the BPS + QC-8 record's commit, the previous GREEN gate record).
> Contents: RDV -- the dead-end rank repair (`fb41ec97`; `QpCounters::rank_repairs / rank_repair_releases / rank_repair_misses`), its
> re-premise of three `SqpElasticWitnessStart` tests (`8cda9d14`), riders (`c1bf9c33`, `c45a6cc5`); R2-1 -- the non-finite step rollback
> (`c52589ef`; `SqpCounters::nonfinite_step_rollbacks`) and its riders (`a9f01851`; `SqpIterate::rolled_back`, arity 29 -> 30).

`wrappers-gates/` is the **declaration** (committed before the first leg, never edited after); `gates/` is the **record**. Derivations:
`wrappers-gates/psym-derivation-rdv-r21.txt`; pre-flight: `wrappers-gates/preflight-rdv-r21.txt`; rehearsal: `wrappers-gates/rehearsal/`.

## Declared values (derived from the range or measured by the declaration's rehearsal)

| item | declared |
|---|---|
| changed surface | 8 headers; 15 modified `.cpp` (14 inside the P-SYM universe, 1 outside: `tests/qp/test_qp_walk_seam.cpp`); 1 added `.cpp`, outside (`tests/qp/test_qp_rank_repair.cpp`); 1 added fixture; no `src/` or `tests/sqp` CMake change |
| G1 / G3 | Debug **3070** / Release **3071** = 3045 / 3046 - 1 removed + 26 added by name; skipped sets equal the baseline's (Debug 7, Release 1) |
| G2 | the long test alone, 1 / 1 |
| G4 | the 43-difference set (walk 17 / ssn 13 / ipm 13) vs the M6 pin, as a SET; exact arm vs the BPS + QC-8 record measured 0 / 0 / 0 (recorded) |
| G5 P-SYM | Release **75** = A 14 + B 61; Debug **76** = A 14 + B 62 (one declared asymmetry: `test_solve_status.cpp.o`, Debug-only class B); UNEXPLAINED 0; non-debug movers 12; DEBUG_ONLY 0 / 4 |
| G6 | install smoke, export contract, clang-format (24 files), `git diff --check`; fold proofs 46 / 0 |

## Gates (chain pid 126556, worktree `agent-a5f4811ec7a6906b7`, declaration `7c2fa0fd`, 2026-10-17, `gates/chain-rdv-r21.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**. **GREEN.**

| gate | result |
|---|---|
| G1 Debug suite | 3070 / 3070 (3072 started names = baseline 3047 - 1 + 26; skipped = baseline's 7) |
| G2 long test | 1 / 1 |
| G3 Release suite | 3071 / 3071 (3073 started names; skipped = baseline's 1) |
| G4 U0 replay vs the M6 pin | the declared 43-difference set (17 / 13 / 13), measured set EQUALS the declared list. Exact arm vs the BPS + QC-8 record: **0 / 0 / 0** differences (no residual-digit moves either) |
| G5 P-SYM | Release 75 = 14 + 61, Debug 76 = 14 + 62; UNEXPLAINED 0 both; class A sets identical; class B asymmetry = exactly the declared `test_solve_status.cpp.o` (byte-identical in Release); sections: non-debug movers = declared set both, DEBUG_ONLY 0 / 4; manifests 160 / 160 |
| G6 | install smoke, export contract, clang-format (24 files), `git diff --check`, `DIRTY_FILES=0` |

Declared moves, confirmed: the three `SqpElasticWitnessStart` tests (and the renamed Yorknet ladder test) PASS in both configurations; the
four `sqp.major` goldens with `rolled_back` and the `SqpIterate` arity/sizeof pins pass inside the green suites; the new counters are folded.

Observations, not deviations:
- Release negative control was 87 in the chain against the rehearsal's 88 (the same kind of two-object difference earlier rounds saw; not asserted as a number).
- Leg wall (informational): G1 9.6 min, G2 2.7, G3 5.3, G4 5.5, G5 38.1, G6 1.4.
- `gates/psym-sections-{Release,Debug}.txt` keep the section checker's summary lines only.
- Every leg printed `PGREP_EMPTY` under the lock hold; no foreign cell binary was alive at any launch.
