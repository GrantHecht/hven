# M7 W2 E1 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed in `a11846b3` *before* the chain ran,
nothing in it adjusted after): `psym-derivation-e1.txt`. Chain: pid 3452015,
detached `setsid`, started 2026-09-22T22:07:45-04:00 at HEAD `a11846b3` with
0 dirty files (`chain.txt:1-2`), ended 2026-09-22T23:19:42-04:00
(`chain.txt:26`). Runner transcript: `gates-e1.txt` (copied verbatim from
`.scratch/m7w2e1gates/logs/gates-e1.log`). Chain wrapper transcript:
`chain.txt` (copied verbatim from `.scratch/m7w2e1gates/logs/chain.log`).
Full test and build transcripts, copied verbatim from
`.scratch/m7w2e1gates/gates/`: `suite-debug.txt`, `suite-release.txt`,
`suite-debug-long.txt`, `install-smoke.txt`, `export-contract.txt`,
`fold-proofs.txt`, `psym-*.txt` (from `gates/psym/`), `replay/` (the
`run-*.log` files renamed `.txt`).

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README (not the derivation, not a
summary of the derivation).

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e1.txt` §3) | MEASURED |
|---|---|---|
| tests | 2766 | 2766 — `gates-e1.txt:32`; `suite-debug.txt:5539` |
| failed | 0 | 0 — `gates-e1.txt:31`; `suite-debug.txt:5539` |

Not run (`gates-e1.txt:25-28`): `FailByDesignControl...` (skipped) and the
two disabled `EqpRefinementAb` fixtures, none of them E1's.

## G2 — the long test, alone

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e1.txt:45`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e1.txt:44` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, 677.69 sec
(`gates-e1.txt:37`), run alone.

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2767 | 2767 — `gates-e1.txt:57`; `suite-release.txt:5541` |
| failed | 0 | 0 — `gates-e1.txt:56`; `suite-release.txt:5541` |

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences | 27 × 75, 0 differences on walk, ssn and ipm — `gates-e1.txt:63,65,67` (CMP_RC=0 at `:64,66,68`); `replay/comparison.txt:1-3`; asserted `gates-e1.txt:71-72` |

No cell moved, as the offline basis check predicted (117 solves full rank at
start and at the final iterate, `../basis-check/basis-check.csv`). There is no
DECLARED FINDING from this gate.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 53 | 53 — `gates-e1.txt:92`, asserted `:103` | 53 — `gates-e1.txt:138`, asserted `:149` |
| class A (source changed) | 8 | 8 — `gates-e1.txt:93`, asserted `:104` | 8 — `gates-e1.txt:139`, asserted `:150` |
| class B (includer, source untouched) | 45 | 45 — `gates-e1.txt:94`, asserted `:105` | 45 — `gates-e1.txt:140`, asserted `:151` |
| UNEXPLAINED | 0 | 0 — `gates-e1.txt:95,101` | 0 — `gates-e1.txt:141,147` |
| negative control | ≥ 1 (rehearsal: 22 / 14) | 22 — `gates-e1.txt:96,102` | 14 — `gates-e1.txt:142,148` |
| compare rc | 1 | 1 — `gates-e1.txt:81,97` | 1 — `gates-e1.txt:127,143` |
| capture pair | after = before + 3 | 149 = 146 + 3 — `gates-e1.txt:77-79` | 149 = 146 + 3 — `gates-e1.txt:123-125` |
| ONLY-AFTER | {equality_basis, test_equality_basis, test_sqp_equality_basis}.cpp.o | same — `gates-e1.txt:82` | same — `gates-e1.txt:128` |
| library ADDED | `src/drivers/equality_basis.cpp.o` present after, absent before | same — `gates-e1.txt:106` | same — `gates-e1.txt:152` |

The mover lists are in `psym-classification-{Release,Debug}.txt`.
Cross-configuration checks (`gates-e1.txt:171-176`): class B sets equal
(45 objects), both unexplained sets empty, both class A sets all
changed-source and identical to each other. The `before` arms re-configured
and ran `ninja -t cleandead` with 0 outputs removed (`gates-e1.txt:76,122`).
The tree was restored to `m7 a11846b3` after each arm (`gates-e1.txt:80,126`).

## G5b — sections (object-section diff, library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | {sqp_options, sqp_solver} + {qp/ipqp_engine, warmstart/continuation} | exactly that — `gates-e1.txt:108-111`, compared to the declaration `:118`, asserted `:119` | exactly that — `gates-e1.txt:155,157,160,161`, compared `:168`, asserted `:169` |
| objects compared | 42 (the 43 library objects less the ADDED one) | 42 — `gates-e1.txt:112` | 42 — `gates-e1.txt:162` |
| byte-identical | — | 38 — `gates-e1.txt:113` | 34 — `gates-e1.txt:163` |
| DEBUG_ONLY | 0 / 4 | 0 — `gates-e1.txt:114,120` | 4 — `gates-e1.txt:164,170` (assembly_eval_seam `:154`, sqp_print `:156`, trace_writer `:158`, soc_elastic_restoration `:159`) |
| REFUSED | 0 | 0 — `gates-e1.txt:116` | 0 — `gates-e1.txt:166` |

`continuation.cpp.o`'s section count moved 649 → 652 in Release
(`gates-e1.txt:111`), as §4 declared from the rehearsal. In Debug it moved
7228 → 7234 (`gates-e1.txt:161`).

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e1.txt:183-185` |
| export contract | OK, align 32 | OK, align 32 — `gates-e1.txt:188,190,193` |
| clang-format | rc 0 on every changed .h/.cpp | rc 0 — `gates-e1.txt:195` (file list `:180`) |
| `git diff --check` | rc 0 | rc 0 — `gates-e1.txt:196` |
| dirty files | 0 | 0 — `gates-e1.txt:197` |

## Fold proofs

| | DECLARED (§6) | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain.txt:24`; `fold-proofs.txt` (last lines) |

## Aggregate

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` — `gates-e1.txt:201`.
`CHAIN_SUMMARY gates=0 fold_proofs=0` — `chain.txt:27`.

Every declared expectation in `psym-derivation-e1.txt` matches its measured
transcript line exactly. Nothing here was re-derived after the chain
reported.
