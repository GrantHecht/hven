# M7 W2 E4 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed *before* the chain ran, nothing in it
adjusted after): `psym-derivation-e4.txt`. Chain: pid 3170920, detached
`setsid`, started 2026-09-22T17:13:47-04:00 at HEAD `2d77a672`
(`chain.txt:2`). Runner transcript: `gates-e4.txt` (copied verbatim from
`.scratch/m7w2e4gates/logs/gates-e4.log`). Chain wrapper transcript:
`chain.txt` (copied from `.scratch/m7w2e4gates/logs/chain.log`). Full test
and build transcripts: `suite-debug.txt`, `suite-release.txt`,
`suite-debug-long.txt`, `install-smoke.txt`, `export-contract.txt`,
`fold-proofs.txt`, `psym-*.txt`, `replay/`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README (not the derivation, not a
summary of the derivation).

## G1 — Debug suite

| | DECLARED (`psym-derivation-e4.txt` §3) | MEASURED |
|---|---|---|
| tests | 2733 | 2733 — `gates-e4.txt:28` |
| failed | 0 | 0 — `gates-e4.txt:27` |

Long-running test excluded from this leg, run separately as G2
(`gates-e4.txt:21-24`; skipped: `FailByDesignControl...` and the two
disabled `EqpRefinementAb` fixtures, none of which are E4's).

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e4.txt:41` |
| failed | 0 | 0 — `gates-e4.txt:40` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, 673.63 sec
(`gates-e4.txt:33`), run alone under the >600s rule.

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2734 | 2734 — `gates-e4.txt:53` |
| failed | 0 | 0 — `gates-e4.txt:52` |

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences | 27 × 75, 0 differences, all 3 engines — `replay/comparison.txt` (walk/ssn/ipm lines); `gates-e4.txt:59,61,63`; asserted `gates-e4.txt:67-68` |

No cell moved. There is no DECLARED FINDING to record from this gate: the
predicate did not flip any currently-solved cell's status. (The two findings
this task *does* carry — the false-kOptimal fixture and the ScaleF7Slow
stale pin — come from the fixtures and from re-running the pre-existing
benchmark at base, not from this replay; see the task README.)

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 1 | 1 — `gates-e4.txt:93` | 1 — `gates-e4.txt:133` |
| class A (source changed) | 1 | 1 — `gates-e4.txt:94` (class A + class B = 1, class B = 0 per `gates-e4.txt:88,128`) | 1 — `gates-e4.txt:134` |
| class B (header includer, untouched source) | 0 | 0 — `gates-e4.txt:88` | 0 — `gates-e4.txt:128` |
| negative control | ≥ 1 | 73 — `gates-e4.txt:96` | 73 — `gates-e4.txt:136` |
| mover identity | `CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o` | same — `gates-e4.txt:101` | same — `gates-e4.txt:141` |

Cross-configuration checks: class B sets equal (both empty), both
configurations' unexplained sets empty, both class A sets are exactly
`{qp_engine.cpp.o}` and identical to each other — `gates-e4.txt:151-156`.

## G5b — sections (object-section diff under the mover)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | `{qp_engine.cpp.o}` | `{qp_engine.cpp.o}` — `gates-e4.txt:108` | `{qp_engine.cpp.o}` — `gates-e4.txt:148` |
| objects compared | — | 42 — `gates-e4.txt:102` | 42 — `gates-e4.txt:142` |
| byte-identical | 41 of 42 | 41 — `gates-e4.txt:103` | 41 — `gates-e4.txt:143` |
| DEBUG_ONLY | 0 | 0 — `gates-e4.txt:104,110` | 0 — `gates-e4.txt:144,150` |
| REFUSED | 0 | 0 — `gates-e4.txt:106` | 0 — `gates-e4.txt:146` |

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc=0 — `gates-e4.txt:163-165` |
| export contract | OK, align 32 | OK, align 32 — `gates-e4.txt:168,170,173` |
| clang-format | rc 0 on the 3 changed C++ files | rc 0 — `gates-e4.txt:175` |
| `git diff --check` | rc 0 | rc 0 — `gates-e4.txt:176` |
| dirty files | 0 | 0 — `gates-e4.txt:177` |

## Fold proofs

| | DECLARED (§6) | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain.txt:24` |

The fold proofs' own wrapper-identity check (worktree digest == digest of
the same path at HEAD) passed before the folds ran (`chain.txt` — see the
`fold-proofs.txt` header carried into the chain's foreground run).

## Aggregate

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` — `gates-e4.txt:181`.
`CHAIN_SUMMARY gates=0 fold_proofs=0` — `chain.txt:27`.

Every declared expectation in `psym-derivation-e4.txt` matches its measured
transcript line exactly; nothing here was re-derived after the chain
reported.
